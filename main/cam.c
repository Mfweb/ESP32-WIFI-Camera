#include "cam.h"
#include "iic_perf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "ov7725.h"

uint8_t cam_init(void)
{
    uint8_t temp_sd = 0;
    cam_io_init();
    iic_init();//初始化SSCB
    SCCB_WriteByte(0x12,0x80);//复位sensor
    vTaskDelay(pdMS_TO_TICKS(5));
    if(SCCB_ReadByte(&temp_sd,1,0x0b) != ENABLE)
    {
        printf("Read sensor ID fail!\r\n");
        return 0;
    }
    if(temp_sd != 0x21)
    {
        printf("Sensor ID must be 0x21(OV7725)! ID:0x%02x\r\n",temp_sd);
        return 0;
    }
    printf("start config ov7725\r\n");
    config_ov7725();
    printf("Sensor init finish\r\n");
    return 1;
}