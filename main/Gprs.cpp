/*
 * gprs.cpp
 *
 *  Created on:
 */

#include <string.h>
#include "Task.h"
#include "Gprs.h"
#include "GeneralUtils.h"

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

}

static const char *TAG = "[PPPOS CLIENT]";
static const char *TAG_SOCKET = "[SOCKET]";

static uint8_t conn_ok;
static uart_port_t uart_num;
static uint8_t disconectPPP;


static int atCmd_waitResponse(const char * cmd, const char *resp, const char * resp1, int cmdSize, int timeout, char **response, int size)
{
        char sresp[256] = {'\0'};
        char data[256] = {'\0'};
        int len, res = 1, idx = 0, tot = 0, timeoutCnt = 0;

        // ** Send command to GSM
        vTaskDelay(100 / portTICK_PERIOD_MS);
        uart_flush(uart_num);

        if (cmd != NULL) {
                if (cmdSize == -1) cmdSize = strlen(cmd);
                uart_write_bytes(uart_num, (const char*)cmd, cmdSize);
                uart_wait_tx_done(uart_num, 100 / portTICK_RATE_MS);
        }

        if (response != NULL) {
                // Read GSM response into buffer
                char *pbuf = *response;
                len = uart_read_bytes(uart_num, (uint8_t*)data, 256, timeout / portTICK_RATE_MS);
                while (len > 0) {
                        if ((tot+len) >= size) {
                                char *ptemp = (char*)realloc(pbuf, size+512);
                                if (ptemp == NULL) return 0;
                                size += 512;
                                pbuf = ptemp;
                        }
                        memcpy(pbuf+tot, data, len);
                        tot += len;
                        response[tot] = '\0';
                        len = uart_read_bytes(uart_num, (uint8_t*)data, 256, 100 / portTICK_RATE_MS);
                }
                *response = pbuf;
                return tot;
        }

        // ** Wait for and check the response
        idx = 0;
        while(1)
        {
                memset(data, 0, 256);
                len = 0;
                len = uart_read_bytes(uart_num, (uint8_t*)data, 256, 10 / portTICK_RATE_MS);
                if (len > 0) {
                        for (int i=0; i<len; i++) {
                                if (idx < 256) {
                                        if ((data[i] >= 0x20) && (data[i] < 0x80)) sresp[idx++] = data[i];
                                        else sresp[idx++] = 0x2e;
                                }
                        }
                        tot += len;
                }
                else {
                        if (tot > 0) {
                                // Check the response
                                if (strstr(sresp, resp) != NULL) {
                                        ESP_LOGI(TAG,"AT RESPONSE: [%s]", sresp);
                                        break;
                                }
                                else {
                                        if (resp1 != NULL) {
                                                if (strstr(sresp, resp1) != NULL) {
                                                        ESP_LOGI(TAG,"AT RESPONSE (1): [%s]", sresp);
                                                        res = 2;
                                                        break;
                                                }
                                        }
                                        ESP_LOGI(TAG,"AT BAD RESPONSE: [%s]", sresp);
                                        res = 0;
                                        break;
                                }
                        }
                }

                timeoutCnt += 10;
                if (timeoutCnt > timeout) {
                        ESP_LOGE(TAG,"AT: TIMEOUT");
                        res = 0;
                        break;
                }
        }

        return res;
}


class PPPtask : public Task {
public:
PPPtask(std::string name) : Task(name, 2048*2) {
        ESP_LOGI(TAG,"A new ppp task is on the way\n");
        tcpip_adapter_init();
};

int get_gsm_status(){
        return conn_ok;
}

private:

/**
 * @brief Handle PPP status events.
 * see https://github.com/yarrick/lwip/blob/master/doc/ppp.txt
 */
static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
        struct netif *pppif = ppp_netif(pcb);
        LWIP_UNUSED_ARG(ctx);

        switch(err_code) {
        case PPPERR_NONE: {
                ESP_LOGI(TAG,"status_cb: Connected\n");
                ESP_LOGI(TAG,"   ipaddr    = %s\n", ipaddr_ntoa(&pppif->ip_addr));
                ESP_LOGI(TAG,"   gateway   = %s\n", ipaddr_ntoa(&pppif->gw));
                ESP_LOGI(TAG,"   netmask   = %s\n", ipaddr_ntoa(&pppif->netmask));
                conn_ok = GSM_STATE_CONNECTED;
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
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                disconectPPP = 1;
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
}

/**
 * @brief Handle sending data to GSM modem
 */
static u32_t ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
        // *** Handle sending to GSM modem ***
        uint32_t ret = uart_write_bytes(uart_num, (const char*)data, len);
        uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
        return ret;
}
/**
 * @brief Task for the PPP connection with GSM modem
 */
void run(void* data) {

        struct netif ppp_netif;
        ppp_pcb *ppp;
        const char *PPP_User = CONFIG_GSM_INTERNET_USER;
        const char *PPP_Pass = CONFIG_GSM_INTERNET_PASSWORD;
        char* ppp_data = (char*) malloc(BUF_SIZE);
        char PPP_ApnATReq[sizeof(CONFIG_GSM_APN)+20];

        int GSM_MGR_InitCmdsSize = sizeof(GSM_MGR_InitCmds)/sizeof(GSM_Cmd);

        //Copy from apn from config
        sprintf(PPP_ApnATReq, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n ", CONFIG_GSM_APN);
        GSM_MGR_InitCmds[6].cmd = PPP_ApnATReq;
        GSM_MGR_InitCmds[6].cmdSize = strlen(PPP_ApnATReq);

        ESP_LOGI(TAG,"PPP OVER GSM START");
        // *** Try disconnect if before connected ***
        while (uart_read_bytes(uart_num, (uint8_t*)ppp_data, BUF_SIZE, 100 / portTICK_RATE_MS)) {
                delay(100);
        }
        atCmd_waitResponse("AT+CGACT=0,1\r\n", GSM_OK_Str, "NO CARRIER", 14, 5000, NULL, 0);

        disconectPPP = 0;

        while(1)
        {
                int gsmCmdIter = 0, nfail = 0;
                while(1)
                {
                        int atCmdRes = atCmd_waitResponse(GSM_MGR_InitCmds[gsmCmdIter].cmd, \
                                                          GSM_OK_Str, NULL, GSM_MGR_InitCmds[gsmCmdIter].cmdSize, 1000, NULL, 0);
                        if(atCmdRes != 1 && gsmCmdIter != GSM_MGR_InitCmdsSize -1) {
                                gsmCmdIter = 0;
                                nfail++;
                        }else if (gsmCmdIter == GSM_MGR_InitCmdsSize -1) {
                                break;
                        }
                        gsmCmdIter++;
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                }/*Send all the GSM init commands of GSM_MGR_InitCmds*/

                ppp = pppapi_pppos_create(&ppp_netif,
                                          ppp_output_callback, ppp_status_cb, NULL);


                if (ppp == NULL) {
                        ESP_LOGE(TAG, "Error init pppos");
                        return;
                }

                /*see https://github.com/yarrick/lwip/blob/master/doc/ppp.txt*/
                ppp->settings.persist = 0;
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            printf("\n\n\n\nGot here motherfucker\n\n\n\n\n");

                pppapi_set_default(ppp);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            printf("\n\n\n\nGot here motherfucker 2\n\n\n\n\n");
                pppapi_set_auth(ppp, PPPAUTHTYPE_ANY, PPP_User, PPP_Pass);
            printf("\n\n\n\nGot here motherfucker  3\n\n\n\n\n");
                //pppapi_set_auth(ppp, PPPAUTHTYPE_NONE, PPP_User, PPP_Pass);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                pppapi_connect(ppp, 0);
            printf("\n\n\n\nGot here motherfucker 4\n\n\n\n\n");

                vTaskDelay(1000 / portTICK_PERIOD_MS);
                conn_ok = 99;
            // *** Handle GSM modem responses ***
                while(1) {
//                    printf("\n\n\n\nGot here motherfucker 5\n\n\n\n\n");
                        memset(ppp_data, 0, BUF_SIZE);
//                    printf("\n\n\n\nGot here motherfucker 6\n\n\n\n\n");
                        int len = uart_read_bytes(uart_num, (uint8_t*)ppp_data, BUF_SIZE, 30 / portTICK_RATE_MS);
//
//                        printf("\n\n\n\nGot here motherfucker 111\n\n\n\n\n");
                        if(len > 0) {
//                            printf("\n\n\n\nGot here motherfucker 7 %d\n\n\n\n\n", len);
                                pppos_input_tcpip(ppp, (uint8_t*)ppp_data, len);
                        }
                        if (disconectPPP) {
//                            printf("\n\n\n\nGot here motherfucker 8\n\n\n\n\n");
                                break;
                        }
//                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }// *** Handle GSM modem responses ***

                if(disconectPPP) {
                    printf("\n\n\n\nGot here motherfucker 9\n\n\n\n\n");
                        pppapi_close(ppp, 0);
                        vTaskDelay(15000 / portTICK_PERIOD_MS); /*mysterious time to get the ppp connection close*/
                        if (data) free(data); // free data buffer
                        if (ppp) ppp_free(ppp);
                        if (ppp_data) free(ppp_data);
                        atCmd_waitResponse("AT+CGACT=0,1\r\n", GSM_OK_Str, "NO CARRIER", 14, 10000, NULL, 0);
                        break;
                }
        }
        conn_ok = GSM_STATE_ENDED;
        this->stop();
}

typedef struct
{
        const char *cmd;
        uint16_t cmdSize;
        const char *cmdResponseOnOk;
        uint32_t timeoutMs;
}GSM_Cmd;

/*GSM INIT AT COMANDS LIST, all this commands are used for
 *  start a PPP over serial gsm modem conection*/
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

}; /*END OF PPPtask Class*/


/**
 * @brief Configure the UART communication with the gsm module
 */

void GPRS::configGprsUart() {
        uart_num = UART_NUM_2;
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
/*
 * @brief GSM costructor, instantiate PPPtask and config the uart
 */
GPRS::GPRS() {
        ppp_task = new PPPtask("ppp_task");
        configGprsUart();
}

GPRS::~GPRS(){
        if(ppp_task) free(ppp_task);
        ESP_LOGI(TAG,"Our aowesome class just died");
}
/*
 * @brief Start ppp task, set status connection variable
 */
void GPRS::start() {
        conn_ok = GSM_STATE_IDLE;
        ppp_task->start();
}

void GPRS::stop() {
        disconectPPP = 1;
}

uint8_t GPRS::getPPPstatus(){
        return conn_ok;
}

/*
   Get time from gprs tracker, and clean the serial buffer
    return a list with the date and time
        [y_high, y_low, month, day, hour, minute, second ]
 */
void GPRS::getTime(void *buf) {
        memset(buf, 0, 7);
        char tmp[7];

        uart_write_bytes(uart_num, "AT+CCLK?\r\n", sizeof("AT+CCLK?\r\n"));
        uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
        vTaskDelay(1500 / portTICK_PERIOD_MS);
        //buffer for receive the serial response
        char* ser_buffer = (char*) malloc(BUF_SIZE);
        uart_read_bytes(uart_num, (uint8_t*)ser_buffer, BUF_SIZE, 10 / portTICK_RATE_MS);

        char *ptr = strstr(ser_buffer, "+CCLK");
        if( ptr != NULL) {
                //+CCLK: "18/02/11,23:44:35"
                int dt[7];
                sscanf(ptr,"+CCLK: \"%d/%d/%d,%d:%d:%d", &dt[0],&dt[1],&dt[2],&dt[3],&dt[4],&dt[5]);
                for(int i=0; i<7; i++) {
                        tmp[i] = (char)dt[i];
                }
                memcpy(buf, tmp, 7);
        }else{
                ESP_LOGE(TAG_SOCKET, "Prolem get time");
                free(ser_buffer);
                return;
        }
        free(ser_buffer);

}

/*
 * @brief Request product serial number identification(IMEI)
 * @param [out] buf return a list with 8 integer numbers
 */

void GPRS::getTerminalID(void *buf) {

        int res=0;
        memset(buf, 0, 8);
        res = atCmd_waitResponse("ATE1\r\n", GSM_OK_Str, NULL, 6, 1000, NULL, 0);
        if(res != 1) return;
        //ask for the terminal IMEI
        uart_write_bytes(uart_num, "AT+CGSN\r\n", sizeof("AT+CGSN\r\n"));
        uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
        vTaskDelay(1500 / portTICK_PERIOD_MS);
        //buffer for receive the serial response
        char* ser_buffer = (char*) malloc(BUF_SIZE);
        uart_read_bytes(uart_num, (uint8_t*)ser_buffer, BUF_SIZE, 10 / portTICK_RATE_MS);

        atCmd_waitResponse("ATE0\r\n", GSM_OK_Str, NULL, 6, 1000, NULL, 0);
        //check if we receive the echo from device
        char *ptr = strstr(ser_buffer, "AT+CGSN");
        char tmp[8];
        if( ptr != NULL) {
                int z, j =0;
                char s[2];
                for (int i = 0; i < 15; i+=2) {
                        memcpy(s, ptr + 13 + i,2);
                        z = atoi(s);
                        tmp[j] = (char)z;
                        j++;
                }
                memcpy(buf, tmp, 8);
        }else{
                ESP_LOGE(TAG_SOCKET, "Prolem get imei");
                free(ser_buffer);
                return;
        }
        free(ser_buffer);
}

/*
 * @brief Open a new socket and send data to server
 * @param [in] a list of bytes to send
 * @param [in] len len of the data
 */
int GPRS::sent_data(const char* data, u32_t len) {

        int socket_fd, n_fails = 0, nfails_send = 0;
        struct sockaddr_in sa;
        char recv_buf[len];
        ssize_t bytes_read = 0;

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
                        nfails_send++; /*TRY TO SEND N TIMES*/
                        if (nfails_send > 5) {
                          break;
                        }
                        continue;
                }

                ESP_LOGI(TAG_SOCKET, "... connected");

                if (write(socket_fd, data, len) < 0) {
                        ESP_LOGE(TAG_SOCKET, "... socket send failed");
                        close(socket_fd);
                        vTaskDelay(4000 / portTICK_PERIOD_MS);
                        continue;
                }else{
                        ESP_LOGI(TAG_SOCKET, "... socket send success");
                }

                //Receive the acknowledge from the server
                memset(&recv_buf[0], 0, sizeof(recv_buf));
                do {
                        bytes_read = recv(socket_fd, recv_buf, sizeof(recv_buf), 0);
                        if (bytes_read < 0) ESP_LOGE(TAG_SOCKET,"recv problem");
                        n_fails++;/*TRY TO Receive*/
                } while (bytes_read < 1 && n_fails < 5);
                ESP_LOGI(TAG_SOCKET, "Received from server %d\n", (int)recv_buf[0]);
                break;
        }

        close(socket_fd);
        return bytes_read;
}

/*
   0x20, 0x20, data_lenth, packet_id, data0, data1, ...., datan, check_h, check_l, 0x0D, 0x0A
 * @brief Encode sysjourney data
 * @param [in] dth
 * @param [in] imei
 * @param [in] cod_linha
 * @param [in] cod_motor
 * @return 0 if data was not sent else the byte received from server
 */

int GPRS::encode_data(char *dth, char *imei, char *cod_linha, char *cod_motor) {

        start();/*START PPP TASK*/

        uint8_t txpacket[56]    = {0};
        txpacket[0]            = 0x20;
        txpacket[1]            = 0x20;
        txpacket[2]            = 0x29;

        memcpy(txpacket+3, dth, 6);
        memcpy(txpacket+9, imei, 8);
        memcpy(txpacket+17, cod_linha, 10);
        memcpy(txpacket+27, cod_motor, 10);

        txpacket[37]            = 0x33;//checksum l
        txpacket[38]            = 0x34;//checksum h

        txpacket[39]            = 0x0D;
        txpacket[40]            = 0x0A;
        int nfails = 0, recv=0;
        do {
                if (conn_ok != GSM_STATE_CONNECTED) {
                        vTaskDelay(1000 / portTICK_RATE_MS);
                        recv = sent_data((const char *)txpacket, 41);
                        if(recv > 0){
                          stop();
                          while (conn_ok != GSM_STATE_ENDED) {
                            vTaskDelay(300 / portTICK_RATE_MS);
                          }
                          return recv;
                        }

                }
                vTaskDelay(3000 / portTICK_RATE_MS);
                nfails++;
        } while(nfails < 3);

        stop();
        while (conn_ok != GSM_STATE_ENDED) {
          vTaskDelay(300 / portTICK_RATE_MS);
        }
        return recv;
}
