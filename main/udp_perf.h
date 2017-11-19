/* udp_perf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#ifndef __UDP_PERF_H__
#define __UDP_PERF_H__



#ifdef __cplusplus
extern "C" {
#endif

/*AP info and tcp_server info*/
#define EXAMPLE_MAX_STA_CONN 1 //how many sta can be connected(AP mode)
#define EXAMPLE_DEFAULT_SERVER_IP "192.168.4.1"

#define TAG "udp_perf:"

/* FreeRTOS event group to signal when we are connected to WiFi and ready to start UDP test*/
extern EventGroupHandle_t udp_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define UDP_CONNCETED_SUCCESS BIT1

extern int total_data;
extern int success_pack;
extern int mysocket;
extern struct sockaddr_in remote_addr;
//using esp as softap
void wifi_init_softap();
//create a udp server socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_udp_server();
//send or recv data task
void send_recv_data(void *pvParameters);
//get socket error code. return: error code
int get_socket_error_code(int socket);
//show socket error code. return: error code
int show_socket_error_reason(int socket);
//check connected socket. return: error code
int check_connected_socket();
//close all socket
void close_socket();





#ifdef __cplusplus
}
#endif


#endif /*#ifndef __UDP_PERF_H__*/

