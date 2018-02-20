/*
 * gprs.h
 *
 *  Created on:
 */
#ifndef _GPRS_H_
#define _GPRS_H_


#include <stdint.h>
#include "FreeRTOS.h"
extern "C"
{
#include "lwip/pppapi.h"
}

#define MAKEWORD(a, b)  ((uint16_t)(((uint8_t)(((uint64_t)(a)) & 0xff)) | ((uint16_t)((uint8_t)(((uint64_t)(b)) & 0xff))) << 8))
#define LOBYTE(w)       ((uint8_t)(((uint64_t)(w)) & 0xff))
#define HIBYTE(w)       ((uint8_t)((((uint64_t)(w)) >> 8) & 0xff))

#define GSM_STATE_DISCONNECTED   0  //Disconnected from Internet
#define GSM_STATE_CONNECTED      1  //Connected to Internet
#define GSM_STATE_IDLE           89 //Disconnected from Internet, Task idle, waiting for reconnect request
#define GSM_STATE_FIRSTINIT      98 //Task started, initializing PPPoS
#define GSM_STATE_ENDED          97


#define BUF_SIZE (2048)
#define UART_BDRATE 115200
#define UART_GPIO_TX 17
#define UART_GPIO_RX 16
#define GSM_OK_Str "OK"

#define SENDER_PORT_NUM 5555
#define SENDER_IP_ADDR "200.132.24.141"

class PPPtask;

/**
 * @brief GPRS modem data acess
 */
class GPRS {
public:
GPRS();
~GPRS();
void start();
void stop();
int sent_data(const char* data, u32_t len);
int encode_data(char *dth, char *imei, char *cod_linha, char *cod_motor);
void getTime(void *buf);
void getTerminalID(void *buf);
uint8_t getPPPstatus();
private:
  void configGprsUart();
  PPPtask* ppp_task;
  friend class PPPtask;
};


#endif
