/*
This file only use for ov7725 with FIFO.
*/
#include "ov7725.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include <sys/socket.h>
#include "iic_perf.h"
#include "udp_perf.h"
#include <string.h>

volatile uint8_t ov7725_vsync = 0;
static char PIC_DATA[OV_WIDTH*OV_SEND_LINE*2] = {0};
extern volatile uint8_t user_connected;
const uint8_t fHead[5]={0x55,0xaa,0x55,0xaa,0xa0};
const uint8_t fEnd[5] ={0xaa,0x55,0xaa,0x55,0xaf};

static xQueueHandle ov7725_vsync_evt_queue = NULL;
/* 寄存器参数配置 */
Reg_Info Sensor_Config[] =
{
	{REG_CLKRC,     0x00}, /*clock config*/
	{REG_COM7,      0x46}, /*QVGA RGB565 */
	{REG_HSTART,    0x3f},
	{REG_HSIZE,     0x50},
	{REG_VSTRT,     0x03},
	{REG_VSIZE,     0x78},
	{REG_HREF,      0x00},
	{REG_HOutSize,  0x50},
	{REG_VOutSize,  0x78},
	{REG_EXHCH,     0x00},
	

	/*DSP control*/
	{REG_TGT_B,     0x7f},
	{REG_FixGain,   0x09},
	{REG_AWB_Ctrl0, 0xe0},
	{REG_DSP_Ctrl1, 0xff},
	{REG_DSP_Ctrl2, 0x20},
	{REG_DSP_Ctrl3,	0x00},
	{REG_DSP_Ctrl4, 0x00},

	/*AGC AEC AWB*/
	{REG_COM8,		0xf0},
	{REG_COM4,		0x81}, /*Pll AEC CONFIG*/
	{REG_COM6,		0xc5},
	{REG_COM9,		0x21},
	{REG_BDBase,	0xFF},
	{REG_BDMStep,	0x01},
	{REG_AEW,		0x34},
	{REG_AEB,		0x3c},
	{REG_VPT,		0xa1},
	{REG_EXHCL,		0x00},
	{REG_AWBCtrl3,  0xaa},
	{REG_COM8,		0xff},
	{REG_AWBCtrl1,  0x5d},

	{REG_EDGE1,		0x0a},
	{REG_DNSOff,	0x01},
	{REG_EDGE2,		0x01},
	{REG_EDGE3,		0x01},

	{REG_MTX1,		0x5f},
	{REG_MTX2,		0x53},
	{REG_MTX3,		0x11},
	{REG_MTX4,		0x1a},
	{REG_MTX5,		0x3d},
	{REG_MTX6,		0x5a},
	{REG_MTX_Ctrl,  0x1e},

	{REG_BRIGHT,	0x00},
	{REG_CNST,		0x25},
	{REG_USAT,		0x65},
	{REG_VSAT,		0x65},
	{REG_UVADJ0,	0x81},
	//{REG_SDE,		  0x20},	//黑白
	{REG_SDE,		  0x06},	//彩色	调节SDE这个寄存器还可以实现其他效果
	
    /*GAMMA config*/
	{REG_GAM1,		0x0c},
	{REG_GAM2,		0x16},
	{REG_GAM3,		0x2a},
	{REG_GAM4,		0x4e},
	{REG_GAM5,		0x61},
	{REG_GAM6,		0x6f},
	{REG_GAM7,		0x7b},
	{REG_GAM8,		0x86},
	{REG_GAM9,		0x8e},
	{REG_GAM10,		0x97},
	{REG_GAM11,		0xa4},
	{REG_GAM12,		0xaf},
	{REG_GAM13,		0xc5},
	{REG_GAM14,		0xd7},
	{REG_GAM15,		0xe8},
	{REG_SLOP,		0x20},

	{REG_HUECOS,	0x80},
	{REG_HUESIN,	0x80},
	{REG_DSPAuto,	0xff},
	{REG_DM_LNL,	0x00},
	{REG_BDBase,	0x99},
	{REG_BDMStep,	0x03},
	{REG_LC_RADI,	0x00},
	{REG_LC_COEF,	0x13},
	{REG_LC_XC,		0x08},
	{REG_LC_COEFB,  0x14},
	{REG_LC_COEFR,  0x17},
	{REG_LC_CTR,	0x05},
	
	{REG_COM3,		0xd0},/*Horizontal mirror image*/

	/*night mode auto frame rate control*/
	//{REG_COM5,		0xf5},	 /*在夜视环境下，自动降低帧率，保证低照度画面质量*/
	{REG_COM5,		0x31},	/*夜视环境帧率不变*/
};

static void IRAM_ATTR ov7725_vsync_handle(void* arg)
{
	uint32_t gpio_num = (uint32_t) arg;
	if(ov7725_vsync == 0)
	{
		FIFO_WRST_L(); 	                      //拉低使FIFO写(数据from摄像头)指针复位
		FIFO_WE_H();	                        //拉高使FIFO写允许
		ov7725_vsync = 1;
		FIFO_WRST_H();                        //允许使FIFO写(数据from摄像头)指针运动
		FIFO_WE_H();                          //使FIFO写允许
	}
	else if(ov7725_vsync == 1)
	{
		FIFO_WE_L();    //FIFO写失能
		ov7725_vsync = 2;
		xQueueSendFromISR(ov7725_vsync_evt_queue, &gpio_num, NULL);
	}
}

static void ov7725_vsync_handle_task(void* arg)
{
	uint32_t io_num;
	for(;;)
	{
		if(xQueueReceive(ov7725_vsync_evt_queue, &io_num, portMAX_DELAY))
		{
			OV7725_FIFO_Read();
			ov7725_vsync = 0;
        }
    }
}

void cam_io_init(void)
{
    gpio_config_t io_conf;
    //FIFO控制引脚
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((uint64_t)1<<FIFO_OE)|((uint64_t)1<<FIFO_RRST)|\
                            ((uint64_t)1<<FIFO_RCLK)|((uint64_t)1<<FIFO_WRST)|\
                            ((uint64_t)1<<FIFO_WEN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    //FIFO数据引脚
    io_conf.pin_bit_mask = ((uint64_t)1<<FIFO_DATA_0)|((uint64_t)1<<FIFO_DATA_1)|\
                            ((uint64_t)1<<FIFO_DATA_2)|((uint64_t)1<<FIFO_DATA_3)|\
                            ((uint64_t)1<<FIFO_DATA_4)|((uint64_t)1<<FIFO_DATA_5)|\
                            ((uint64_t)1<<FIFO_DATA_6)|((uint64_t)1<<FIFO_DATA_7);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    FIFO_OE_L();	  					/*拉低使FIFO输出使能*/
    FIFO_WE_L();   						/*拉高使FIFO写允许*/
    ov7725_vsync_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(ov7725_vsync_handle_task, "tk", 8192, NULL, 20, NULL);
    //VSYNC  下降沿中断
    io_conf.pin_bit_mask = ((uint64_t)1<<CAM_VSYNC);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;//下降沿
    gpio_config(&io_conf);
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);//配置中断服务
    gpio_isr_handler_add(CAM_VSYNC, ov7725_vsync_handle, (void*) CAM_VSYNC);//配置中断服务函数
}
void OV7725_StartEnd(uint8_t m)
{
	if(m)
		sendto(mysocket,fHead, 5, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
	else
		sendto(mysocket,fEnd, 5, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
}
void OV7725_Send(uint8_t line)
{
	sendto(mysocket,PIC_DATA, OV_WIDTH*line*2, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
	//memset(PIC_DATA, 0, OV_WIDTH*line*2);
}
void config_ov7725(void)
{
	
	for(int i=0;i<sizeof(Sensor_Config)/sizeof(Sensor_Config[0]);i++)
	{
		SCCB_WriteByte(Sensor_Config[i].Address, Sensor_Config[i].Value);
	}
	/*光照模式*/
	OV7725_Light_Mode(0);//自动
	/*饱和度*/
	OV7725_Color_Saturation(0);
	/*光照度*/
	OV7725_Brightness(0);
	/*对比度*/
	OV7725_Contrast(0);
	/*特殊效果*/
    OV7725_Special_Effect(0);
    /*窗口大小*/
    OV7725_Window_Set(0,0,OV_WIDTH,OV_HEIGHT,0);
}

void OV7725_FIFO_Read(void)
{
	int i,j;
	SDELAY();
    FIFO_RRST_L();
	SDELAY();
    FIFO_RCLK_L();
	SDELAY();
    FIFO_RCLK_H();
	SDELAY();
    FIFO_RRST_H();
	SDELAY();
    FIFO_RCLK_L();
	SDELAY();
	FIFO_RCLK_H();
	OV7725_StartEnd(1);
    uint8_t line_count = 0;
    char * pData = PIC_DATA;
    for(i = 0;i < OV_HEIGHT;i++)
    {
        for(j = (OV_WIDTH * 2);j > 0 ;j--)
        {
			FIFO_RCLK_L();
			asm volatile ("nop");
            *(pData++)  = \
			((GPIO.in >> (FIFO_DATA_0 - 0)) & (0x01 << 0)) |\
			((GPIO.in1.data >> (FIFO_DATA_1-32 - 1)) & (0x01 << 1)) |\
			((GPIO.in1.data << (FIFO_DATA_2-32 + 2)) & (0x01 << 2)) |\
			((GPIO.in >> (FIFO_DATA_3 - 3)) & (0x01 << 3)) |\
			((GPIO.in >> (FIFO_DATA_4 - 4)) & (0x01 << 4)) |\
			((GPIO.in >> (FIFO_DATA_5 - 5)) & (0x01 << 5)) |\
			((GPIO.in >> (FIFO_DATA_6 - 6)) & (0x01 << 6)) |\
			((GPIO.in >> (FIFO_DATA_7 - 7)) & (0x01 << 7));
            FIFO_RCLK_H();
        }
        if(++line_count >= OV_SEND_LINE)//单次发送不能超过MTU值(最大1500)
        {
            line_count = 0;
			if(user_connected)OV7725_Send(OV_SEND_LINE);
			pData = PIC_DATA;//复位数据指针
		}
    }
    if(line_count != 0)//最后一次可能不够
    {
        if(user_connected)OV7725_Send(line_count);
	}
	OV7725_StartEnd(0);
}

/* 特殊效果设置 */
void OV7725_Special_Effect(uint8_t eff)
{
	switch(eff)
	{
		case 0://正常
			SCCB_WriteByte(0xa6, 0x06);
			SCCB_WriteByte(0x60, 0x80);
			SCCB_WriteByte(0x61, 0x80);
		break;
		
		case 1://黑白
			SCCB_WriteByte(0xa6, 0x26);
			SCCB_WriteByte(0x60, 0x80);
			SCCB_WriteByte(0x61, 0x80);
		break;	
		
		case 2://偏蓝
			SCCB_WriteByte(0xa6, 0x1e);
			SCCB_WriteByte(0x60, 0xa0);
			SCCB_WriteByte(0x61, 0x40);	
		break;	
		
		case 3://复古
			SCCB_WriteByte(0xa6, 0x1e);
			SCCB_WriteByte(0x60, 0x40);
			SCCB_WriteByte(0x61, 0xa0);	
		break;	
		
		case 4://偏红
			SCCB_WriteByte(0xa6, 0x1e);
			SCCB_WriteByte(0x60, 0x80);
			SCCB_WriteByte(0x61, 0xc0);		
		break;	
		
		case 5://偏绿
			SCCB_WriteByte(0xa6, 0x1e);
			SCCB_WriteByte(0x60, 0x60);
			SCCB_WriteByte(0x61, 0x60);		
		break;	
		
		case 6://反相
			SCCB_WriteByte(0xa6, 0x46);
		break;	
				
		default:
			OV7725_DEBUG("Special Effect error!");
			break;
	}
}
/* 光照模式 */
void OV7725_Light_Mode(uint8_t mode)
{
	switch(mode)
	{
		case 0:	//Auto，自动模式
			SCCB_WriteByte(0x13, 0xff); //AWB on 
			SCCB_WriteByte(0x0e, 0x65);
			SCCB_WriteByte(0x2d, 0x00);
			SCCB_WriteByte(0x2e, 0x00);
			break;
		case 1://sunny，晴天
			SCCB_WriteByte(0x13, 0xfd); //AWB off
			SCCB_WriteByte(0x01, 0x5a);
			SCCB_WriteByte(0x02, 0x5c);
			SCCB_WriteByte(0x0e, 0x65);
			SCCB_WriteByte(0x2d, 0x00);
			SCCB_WriteByte(0x2e, 0x00);
			break;	
		case 2://cloudy，多云
			SCCB_WriteByte(0x13, 0xfd); //AWB off
			SCCB_WriteByte(0x01, 0x58);
			SCCB_WriteByte(0x02, 0x60);
			SCCB_WriteByte(0x0e, 0x65);
			SCCB_WriteByte(0x2d, 0x00);
			SCCB_WriteByte(0x2e, 0x00);
			break;	
		case 3://office，办公室
			SCCB_WriteByte(0x13, 0xfd); //AWB off
			SCCB_WriteByte(0x01, 0x84);
			SCCB_WriteByte(0x02, 0x4c);
			SCCB_WriteByte(0x0e, 0x65);
			SCCB_WriteByte(0x2d, 0x00);
			SCCB_WriteByte(0x2e, 0x00);
			break;	
		case 4://home，家里
			SCCB_WriteByte(0x13, 0xfd); //AWB off
			SCCB_WriteByte(0x01, 0x96);
			SCCB_WriteByte(0x02, 0x40);
			SCCB_WriteByte(0x0e, 0x65);
			SCCB_WriteByte(0x2d, 0x00);
			SCCB_WriteByte(0x2e, 0x00);
			break;	
		
		case 5://night，夜晚
			SCCB_WriteByte(0x13, 0xff); //AWB on
			SCCB_WriteByte(0x0e, 0xe5);
			break;	
		
		default:
			 OV7725_DEBUG("Light Mode parameter error!"); 

			break;
	}
}		
/* 饱和度  -4--+4*/
void OV7725_Color_Saturation(int8_t sat)
{

 	if(sat >=-4 && sat<=4)
	{	
		SCCB_WriteByte(REG_USAT, (sat+4)<<4); 
		SCCB_WriteByte(REG_VSAT, (sat+4)<<4);
	}
	else
	{
		OV7725_DEBUG("Color Saturation parameter error!");
	}
}
/* 设置光照度 -4--+4*/
void OV7725_Brightness(int8_t bri)
{
	uint8_t BRIGHT_Value=0x08,SIGN_Value=0x06;	
	
	switch(bri)
	{
		case 4:
				BRIGHT_Value = 0x48;
				SIGN_Value = 0x06;
			break;
		
		case 3:
				BRIGHT_Value = 0x38;
				SIGN_Value = 0x06;		
		break;	
		
		case 2:
				BRIGHT_Value = 0x28;
				SIGN_Value = 0x06;			
		break;	
		
		case 1:
				BRIGHT_Value = 0x18;
				SIGN_Value = 0x06;			
		break;	
		
		case 0:
				BRIGHT_Value = 0x08;
				SIGN_Value = 0x06;			
		break;	
		
		case -1:
				BRIGHT_Value = 0x08;
				SIGN_Value = 0x0e;		
		break;	
		
		case -2:
				BRIGHT_Value = 0x18;
				SIGN_Value = 0x0e;		
		break;	
		
		case -3:
				BRIGHT_Value = 0x28;
				SIGN_Value = 0x0e;		
		break;	
		
		case -4:
				BRIGHT_Value = 0x38;
				SIGN_Value = 0x0e;		
		break;	
		
		default:
			OV7725_DEBUG("Brightness parameter error!");
			break;
	}

		SCCB_WriteByte(REG_BRIGHT, BRIGHT_Value); //AWB on
		SCCB_WriteByte(REG_SIGN, SIGN_Value);
}
/* 对比度 -4--+4*/
void OV7725_Contrast(int8_t cnst)
{
	if(cnst >= -4 && cnst <=4)
	{
		SCCB_WriteByte(REG_CNST, (0x30-(4-cnst)*4));
	}
	else
	{
		OV7725_DEBUG("Contrast parameter error!");
	}
}
/* 配置窗口大小 */
void OV7725_Window_Set(uint16_t sx,uint16_t sy,uint16_t width,uint16_t height,uint8_t QVGA_VGA)
{
	uint8_t reg_raw,cal_temp;

	/***********QVGA or VGA *************/
	if(QVGA_VGA == 0)
	{
		/*QVGA RGB565 */
		SCCB_WriteByte(REG_COM7,0x46); 
	}
	else
	{
			/*VGA RGB565 */
		SCCB_WriteByte(REG_COM7,0x06); 
	}

	/***************HSTART*********************/
	//读取寄存器的原内容，HStart包含偏移值，在原始偏移植的基础上加上窗口偏移	
	SCCB_ReadByte(&reg_raw,1,REG_HSTART);
	
	//sx为窗口偏移，高8位存储在HSTART，低2位在HREF
	cal_temp = (reg_raw + (sx>>2));	
	SCCB_WriteByte(REG_HSTART,cal_temp ); 
	
	/***************HSIZE*********************/
	//水平宽度，高8位存储在HSIZE，低2位存储在HREF
	SCCB_WriteByte(REG_HSIZE,width>>2);//HSIZE左移两位 
	
	
	/***************VSTART*********************/
	//读取寄存器的原内容，VStart包含偏移值，在原始偏移植的基础上加上窗口偏移	
	SCCB_ReadByte(&reg_raw,1,REG_VSTRT);	
	//sy为窗口偏移，高8位存储在HSTART，低1位在HREF
	cal_temp = (reg_raw + (sy>>1));	
	
	SCCB_WriteByte(REG_VSTRT,cal_temp);
	
	/***************VSIZE*********************/
	//垂直高度，高8位存储在VSIZE，低1位存储在HREF
	SCCB_WriteByte(REG_VSIZE,height>>1);//VSIZE左移一位
	
	/***************VSTART*********************/
	//读取寄存器的原内容	
	SCCB_ReadByte(&reg_raw,1,REG_HREF);	
	//把水平宽度的低2位、垂直高度的低1位，水平偏移的低2位，垂直偏移的低1位的配置添加到HREF
	cal_temp = (reg_raw |(width&0x03)|((height&0x01)<<2)|((sx&0x03)<<4)|((sy&0x01)<<6));	
	
	SCCB_WriteByte(REG_HREF,cal_temp);
	
	/***************HOUTSIZIE /VOUTSIZE*********************/
	SCCB_WriteByte(REG_HOutSize,width>>2);
	SCCB_WriteByte(REG_VOutSize,height>>1);
	
	//读取寄存器的原内容	
	SCCB_ReadByte(&reg_raw,1,REG_EXHCH);	
	cal_temp = (reg_raw |(width&0x03)|((height&0x01)<<2));	

	SCCB_WriteByte(REG_EXHCH,cal_temp);	
}
