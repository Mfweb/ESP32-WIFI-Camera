#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "udp_perf.h"
#include "iic_perf.h"
#include "cam.h"
#include "ov7725.h"
# include <stdlib.h>

static portMUX_TYPE cam_init_slock = portMUX_INITIALIZER_UNLOCKED;
uint8_t volatile user_connected = 0;
unsigned int socklen;

static void udp_conn(void *pvParameters)
{
    uint8_t databuff[10],len;
    /*等待设备连接到本AP*/
    xEventGroupWaitBits(udp_event_group, WIFI_CONNECTED_BIT,false, true, portMAX_DELAY);
    int socket_ret;
    socklen = sizeof(remote_addr);
    //创建UDP服务器
    printf("start udp server.\r\n");
    socket_ret = create_udp_server();
    //创建失败，删除任务
    if(socket_ret == ESP_FAIL)
    {
        printf("udp server fail\r\n");
        vTaskDelete(NULL);
    }

    while (1) 
    {
        len = recvfrom(mysocket, databuff, 10, 0, (struct sockaddr *)&remote_addr, &socklen);
        if(len>0)
        {
            printf("rev!\r\n");
            if(user_connected == 0)
            {
                user_connected=1;
            }
            ov7725_vsync = 0;
        }
        vTaskDelay(100/ portTICK_RATE_MS);//every 3s
    }
    close_socket();
    vTaskDelete(NULL);
}

void app_main(void)
{
    printf("start\r\n");
    wifi_init_softap();//进入AP模式
    xTaskCreate(&udp_conn, "udp_conn", 4096, NULL, 5, NULL);//创建UDP进程
    portENTER_CRITICAL(&cam_init_slock);//进入临界区
    cam_init();
    portEXIT_CRITICAL(&cam_init_slock);//退出临界区
    /*while(1)
    {
        //vTaskDelay(3000/ portTICK_RATE_MS);//every 3s
        
        if(ov7725_vsync == FIFO_DATA_READY)
        {
            //DISABLE_OV;
            OV7725_FIFO_Read();
            ov7725_vsync = 0;
            //ENABLE_OV;
            //printf("Get F\r\n");
        }
    }*/
}
