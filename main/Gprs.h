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

#define GSM_STATE_DISCONNECTED   0  //Disconnected from Internet
#define GSM_STATE_CONNECTED      1  //Connected to Internet
#define GSM_STATE_IDLE           89 //Disconnected from Internet, Task idle, waiting for reconnect request
#define GSM_STATE_FIRSTINIT      98 //Task started, initializing PPPoS
#define BUF_SIZE (1024)
#define UART_BDRATE 115200
#define UART_GPIO_TX 10
#define UART_GPIO_RX 9
#define GSM_OK_Str "OK"

#define SENDER_PORT_NUM 5555
#define SENDER_IP_ADDR "200.132.24.141"


class PPPtask;

class GPRS {
public:
void start();
void sent_data(const char* data, u32_t len);
private:
  PPPtask* ppp_task;
  friend class PPPtask;
};


#endif