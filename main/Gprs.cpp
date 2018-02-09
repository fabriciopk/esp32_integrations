/*
 * gprs.cpp
 *
 *  Created on:
 */

#include <string.h>
#include "Task.h"
#include "Gprs.h"

extern "C"
{
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "esp_system.h"
  #include "esp_log.h"
  #include "driver/uart.h"
  #include "driver/gpio.h"
  #include "tcpip_adapter.h"
  #include "netif/ppp/pppos.h"
  #include "netif/ppp/ppp.h"
  #include "lwip/pppapi.h"

  #include "lwip/err.h"
  #include "lwip/sockets.h"
  #include "lwip/sys.h"
  #include "lwip/netdb.h"
  #include "lwip/dns.h"
/*UART port file descriptor*/

}


static const char *TAG = "[PPPOS CLIENT]";
static const char *TAG_SOCKET = "[SOCKET]";

static uint8_t conn_ok;
static uart_port_t uart_num;

static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
        struct netif *pppif = ppp_netif(pcb);
        LWIP_UNUSED_ARG(ctx);

        switch(err_code) {
        case PPPERR_NONE: {
                ESP_LOGI(TAG,"status_cb: Connected\n");
#if PPP_IPV4_SUPPORT
                ESP_LOGI(TAG,"   ipaddr    = %s\n", ipaddr_ntoa(&pppif->ip_addr));
                ESP_LOGI(TAG,"   gateway   = %s\n", ipaddr_ntoa(&pppif->gw));
                ESP_LOGI(TAG,"   netmask   = %s\n", ipaddr_ntoa(&pppif->netmask));
#endif

#if PPP_IPV6_SUPPORT
                ESP_LOGI(TAG,"   ip6addr   = %s\n", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
#endif
                conn_ok = 1;
                break;
        }
        case PPPERR_PARAM: {
                ESP_LOGE(TAG,"status_cb: Invalid parameter\n");
                break;
        }
        case PPPERR_OPEN: {
                ESP_LOGE(TAG,"status_cb: Unable to open PPP session\n");
                break;
        }
        case PPPERR_DEVICE: {
                ESP_LOGE(TAG,"status_cb: Invalid I/O device for PPP\n");
                break;
        }
        case PPPERR_ALLOC: {
                ESP_LOGE(TAG,"status_cb: Unable to allocate resources\n");
                break;
        }
        case PPPERR_USER: {
                ESP_LOGE(TAG,"status_cb: User interrupt\n");
                conn_ok = GSM_STATE_DISCONNECTED;
                break;
        }
        case PPPERR_CONNECT: {
                ESP_LOGE(TAG,"status_cb: Connection lost\n");
                conn_ok = GSM_STATE_DISCONNECTED;
                break;
        }
        case PPPERR_AUTHFAIL: {
                ESP_LOGE(TAG,"status_cb: Failed authentication challenge\n");
                break;
        }
        case PPPERR_PROTOCOL: {
                ESP_LOGE(TAG,"status_cb: Failed to meet protocol\n");
                break;
        }
        case PPPERR_PEERDEAD: {
                ESP_LOGE(TAG,"status_cb: Connection timeout\n");
                break;
        }
        case PPPERR_IDLETIMEOUT: {
                ESP_LOGE(TAG,"status_cb: Idle Timeout\n");
                break;
        }
        case PPPERR_CONNECTTIME: {
                ESP_LOGE(TAG,"status_cb: Max connect time reached\n");
                break;
        }
        case PPPERR_LOOPBACK: {
                ESP_LOGE(TAG,"status_cb: Loopback detected\n");
                break;
        }
        default: {
                ESP_LOGE(TAG,"status_cb: Unknown error code %d\n", err_code);
                break;
        }
        }

        /*
         * This should be in the switch case, this is put outside of the switch
         * case for example readability.
         */

        if (err_code == PPPERR_NONE) {
                return;
        }

        /* ppp_close() was previously called, don't reconnect */
        if (err_code == PPPERR_USER) {
                /* ppp_free(); -- can be called here */
                return;
        }
}

/**
 * @brief Handle sending to GSM modem ***
 */
static u32_t ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
        // *** Handle sending to GSM modem ***
        uint32_t ret = uart_write_bytes(uart_num, (const char*)data, len);
        uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
        return ret;
}


class PPPtask : public Task {
public:
PPPtask(std::string name) : Task(name, 4096*5) {
        tcpip_adapter_init();
        configUart();
};

int get_gsm_status(){
        return conn_ok;
}

private:
/**
 * @brief Configure the communication with the gsm module
 */
void configUart() {

        uart_num = UART_NUM_1;
        gpio_set_direction((gpio_num_t)UART_GPIO_TX, GPIO_MODE_OUTPUT);
        gpio_set_direction((gpio_num_t)UART_GPIO_RX, GPIO_MODE_INPUT);
        gpio_set_pull_mode((gpio_num_t)UART_GPIO_RX, GPIO_PULLUP_ONLY);

        uart_config_t uart_config = {
                .baud_rate = UART_BDRATE,
                .data_bits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
                .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                .rx_flow_ctrl_thresh = 122
        };

        //Configure UART1 parameters
        uart_param_config(uart_num, &uart_config);
        uart_set_pin(uart_num, (gpio_num_t)UART_GPIO_TX, (gpio_num_t)UART_GPIO_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        uart_driver_install(uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
}

/**
 * @brief Perform the task handling for gprs ppp client
 */
void run(void* data) {
        struct netif ppp_netif;
        uint8_t init_ok = 1;

        char sresp[256] = {'\0'};
        char* ppp_data = (char*) malloc(BUF_SIZE);
        char PPP_ApnATReq[sizeof(CONFIG_GSM_APN)+20];

        int pass = 0;
        int GSM_MGR_InitCmdsSize = sizeof(GSM_MGR_InitCmds)/sizeof(GSM_Cmd);

        //Copy from apn from config
        sprintf(PPP_ApnATReq, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n ", CONFIG_GSM_APN);
        GSM_MGR_InitCmds[6].cmd = PPP_ApnATReq;
        GSM_MGR_InitCmds[6].cmdSize = strlen(PPP_ApnATReq);

        ESP_LOGI(TAG,"Gsm init start");
        // *** Disconnect if connected ***
        delay(1000);
        while (uart_read_bytes(uart_num, (uint8_t*)ppp_data, BUF_SIZE, 100 / portTICK_RATE_MS)) {
                delay(100);
        }
                      uart_write_bytes(uart_num, "AT+CGACT=0,1\r\n", 14);
        uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
        delay(10000);


        while(1)
        {
                init_ok = GSM_STATE_CONNECTED;
                // Init gsm
                int gsmCmdIter = 0;
                while(1)
                {
                        // ** Send command to GSM
                        memset(sresp, 0, 256);
                        for (int i=0; i<255; i++) {
                                if ((GSM_MGR_InitCmds[gsmCmdIter].cmd[i] >= 0x20) && (GSM_MGR_InitCmds[gsmCmdIter].cmd[i] < 0x80)) {
                                        sresp[i] = GSM_MGR_InitCmds[gsmCmdIter].cmd[i];
                                        sresp[i+1] = 0;
                                }
                                if (GSM_MGR_InitCmds[gsmCmdIter].cmd[i] == 0) break;
                        }
                        delay(100);
                        while (uart_read_bytes(uart_num, (uint8_t*)ppp_data, BUF_SIZE, 100 / portTICK_RATE_MS)) {
                                delay(100);
                        }
                        uart_write_bytes(uart_num, (const char*)GSM_MGR_InitCmds[gsmCmdIter].cmd,
                                         GSM_MGR_InitCmds[gsmCmdIter].cmdSize);
                        uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);

                        // ** Wait for and check the response
                        int timeoutCnt = 0;
                        memset(sresp, 0, 256);
                        int idx = 0;
                        int tot = 0;

                        while(1)
                        {
                                memset(ppp_data, 0, BUF_SIZE);
                                int len = 0;
                                len = uart_read_bytes(uart_num, (uint8_t*)ppp_data, BUF_SIZE, 10 / portTICK_RATE_MS);
                                if (len > 0) {
                                        for (int i=0; i<len; i++) {
                                                if (idx < 255) {
                                                        if ((ppp_data[i] >= 0x20) && (ppp_data[i] < 0x80)) {
                                                                sresp[idx++] = ppp_data[i];
                                                        }
                                                        else sresp[idx++] = 0x2e;
                                                        sresp[idx] = 0;
                                                }
                                        }
                                        tot += len;
                                }
                                else {
                                        if (tot > 0) {
                                                ESP_LOGI(TAG," <Resp: [%s], %d\r\n", sresp, tot);
                                                if (strstr(sresp, GSM_MGR_InitCmds[gsmCmdIter].cmdResponseOnOk) != NULL) {
                                                        break;
                                                }
                                                else {
                                                        ESP_LOGE(TAG, "Wrong response, expected [%s]\r\n", GSM_MGR_InitCmds[gsmCmdIter].cmdResponseOnOk);
                                                        init_ok = GSM_STATE_DISCONNECTED;
                                                        break;
                                                }
                                        }
                                }
                                timeoutCnt += 10;

                                if (timeoutCnt > GSM_MGR_InitCmds[gsmCmdIter].timeoutMs)
                                {
                                        ESP_LOGE(TAG,"[GSM INIT] No response, Gsm Init Error\r\n");
                                        init_ok = GSM_STATE_DISCONNECTED;
                                        break;
                                }
                        }//GSM responses

                        if (init_ok == GSM_STATE_DISCONNECTED) {
                                // No response or not as expected
                                delay(5000);
                                init_ok = 1;
                                gsmCmdIter = 0;
                                continue;
                        }
                        if (gsmCmdIter == 2) delay(500);
                        if (gsmCmdIter == 3) delay(1500);
                        gsmCmdIter++;
                        if (gsmCmdIter >= GSM_MGR_InitCmdsSize) break; // All init commands sent
                }

                ppp = pppapi_pppos_create(&ppp_netif,
                                          ppp_output_callback, ppp_status_cb, NULL);

                if (conn_ok == GSM_STATE_FIRSTINIT) {
                        if(ppp == NULL) {
                                ESP_LOGE(TAG, "Error init pppos");
                                return;
                        }
                }
                conn_ok = 99;

                pppapi_set_default(ppp);
                //pppapi_set_auth(ppp, PPPAUTHTYPE_ANY, PPP_User, PPP_Pass);
                pppapi_set_auth(ppp, PPPAUTHTYPE_NONE, PPP_User, PPP_Pass);
                pppapi_connect(ppp, 0);

                // *** Handle GSM modem responses ***
                while(1) {
                        memset(ppp_data, 0, BUF_SIZE);
                        int len = uart_read_bytes(uart_num, (uint8_t*)ppp_data, BUF_SIZE, 30 / portTICK_RATE_MS);
                        if(len > 0) {
                                pppos_input_tcpip(ppp, (uint8_t*)ppp_data, len);
                        }
                        // Check if disconnected, start AT cmd commands
                        if (conn_ok == GSM_STATE_DISCONNECTED) {
                                ESP_LOGE(TAG, "Disconnected, trying again...");
                                pppapi_close(ppp, 0);
                                gsmCmdIter = 0;
                                conn_ok = GSM_STATE_IDLE;
                                delay(1000);
                                break;
                        }
                }// *** Handle GSM modem responses ***
        }
}

ppp_pcb *ppp;
const char *PPP_User = CONFIG_GSM_INTERNET_USER;
const char *PPP_Pass = CONFIG_GSM_INTERNET_PASSWORD;

typedef struct
{
        const char *cmd;
        uint16_t cmdSize;
        const char *cmdResponseOnOk;
        uint32_t timeoutMs;
}GSM_Cmd;
/*Connection status*/
/*GSM AT COMANDS LIST*/
GSM_Cmd GSM_MGR_InitCmds[11] =
{
        {
                .cmd = "AT\r\n",
                .cmdSize = sizeof("AT\r\n")-1,
                .cmdResponseOnOk = GSM_OK_Str,
                .timeoutMs = 3000,
        },
        {
                .cmd = "ATH\r\n",
                .cmdSize = sizeof("ATH\r\n")-1,
                .cmdResponseOnOk = GSM_OK_Str,
                .timeoutMs = 3000,
        },
        {
                .cmd = "AT+CFUN=1,0\r\n",
                .cmdSize = sizeof("AT+CFUN=1,0\r\n")-1,
                .cmdResponseOnOk = GSM_OK_Str,
                .timeoutMs = 3000,
        },
        {
                .cmd = "ATE0\r\n",
                .cmdSize = sizeof("ATE0\r\n")-1,
                .cmdResponseOnOk = GSM_OK_Str,
                .timeoutMs = 3000,
        },
        {
                .cmd = "AT+CGATT=1\r\n",
                .cmdSize = sizeof("AT+CGATT=1\r\n")-1,
                .cmdResponseOnOk = GSM_OK_Str,
                .timeoutMs = 3000,
        },
        {
                .cmd = "AT+CREG?\r\n",
                .cmdSize = sizeof("AT+CREG?\r\n")-1,
                .cmdResponseOnOk = "CREG: 0,1",
                .timeoutMs = 3000,
        },
        {
                .cmd = "AT+CGDCONT=1,\"IP\",\"apn\"\r",
                .cmdSize = sizeof("AT+CGDCONT=1,\"IP\",\"apn\"\r")-1,
                .cmdResponseOnOk = GSM_OK_Str,
                .timeoutMs = 8000,
        },
        {
                .cmd = "AT+CGQREQ=1\r\n",
                .cmdSize = sizeof("AT+CGQREQ=1\r\n")-1,
                .cmdResponseOnOk = GSM_OK_Str,
                .timeoutMs = 10000,
        },
        {
                .cmd = "AT+CGQMIN=1\r\n",
                .cmdSize = sizeof("AT+CGQMIN=1\r\n")-1,
                .cmdResponseOnOk = GSM_OK_Str,
                .timeoutMs = 10000,
        },
        {
                .cmd = "AT+CGEREP=1,0\r\n",
                .cmdSize = sizeof("AT+CGEREP=1,0\r\n")-1,
                .cmdResponseOnOk = GSM_OK_Str,
                .timeoutMs = 10000,
        },
        {
                .cmd = "AT+CGDATA=\"PPP\",1\r\n",
                .cmdSize = sizeof("AT+CGDATA=\"PPP\",1\r\n")-1,
                .cmdResponseOnOk = "CONNECT",
                .timeoutMs = 30000,
        }
};
};


void GPRS::start() {
        ppp_task = new PPPtask("ppp_task");
        ppp_task->start();
}


void GPRS::sent_data(const char* data, u32_t len) {

        int socket_fd,accept_fd;
        int addr_size,sent_data; char data_buffer[80];
        struct sockaddr_in sa,ra,isa;

        while(1) {
                /*wait for the gsm connection*/
                while (ppp_task->get_gsm_status() != GSM_STATE_CONNECTED) {
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                }

                socket_fd = socket(PF_INET, SOCK_STREAM, 0);
                if ( socket_fd < 0 ) {
                        ESP_LOGE(TAG_SOCKET, "... Failed to allocate socket.");
                        close(socket_fd);
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        continue;
                }

                memset(&sa, 0, sizeof(struct sockaddr_in));
                sa.sin_family = AF_INET;
                sa.sin_addr.s_addr = inet_addr(SENDER_IP_ADDR);
                sa.sin_port = htons(SENDER_PORT_NUM);

                if(connect(socket_fd, (struct sockaddr*)&sa, sizeof(struct sockaddr)) < 0)
                {
                        ESP_LOGE(TAG_SOCKET, "connect failed \n");
                        close(socket_fd);
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        continue;
                }
                ESP_LOGI(TAG_SOCKET, "... connected");

                if (write(socket_fd, data, len) < 0) {
                        ESP_LOGE(TAG_SOCKET, "... socket send failed");
                        close(socket_fd);
                        vTaskDelay(4000 / portTICK_PERIOD_MS);
                        continue;
                }

                ESP_LOGI(TAG_SOCKET, "... socket send success");
                close(socket_fd);
                break;
        }
}
