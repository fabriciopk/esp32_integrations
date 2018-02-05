#ifndef _GPRS_H_
#define _GPRS_H_

#include "lwip/pppapi.h"


#define GSM_STATE_DISCONNECTED	 0
#define GSM_STATE_CONNECTED		   1
#define GSM_STATE_IDLE			     89
#define GSM_STATE_FIRSTINIT		   98


static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx);
static u32_t ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx);
static void pppos_client_task();
int gprs_init();
/*
 * Get GSM/Task status
 *
 * Result:
 * GSM_STATE_DISCONNECTED	   (0)		Disconnected from Internet
 * GSM_STATE_CONNECTED		   (1)		Connected to Internet
 * GSM_STATE_IDLE			      (89)	Disconnected from Internet, Task idle, waiting for reconnect request
 * GSM_STATE_FIRSTINIT		  (98)	Task started, initializing PPPoS
 */
//================
int get_gsm_status();

#endif
