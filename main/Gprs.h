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
// *** If not using hw flow control, set it to 38400
#define UART_BDRATE 115200
#define GSM_OK_Str "OK"


class PPPtask;

// UART
#define UART_GPIO_TX 10
#define UART_GPIO_RX 9

typedef struct
{
        const char *cmd;
        uint16_t cmdSize;
        const char *cmdResponseOnOk;
        uint32_t timeoutMs;
}GSM_Cmd;


class GPRS {
public:
void start();
private:
friend class GPRStask;
};


#endif
