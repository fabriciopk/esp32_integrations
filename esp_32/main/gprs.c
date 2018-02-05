
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "tcpip_adapter.h"
#include "netif/ppp/pppos.h"
#include "netif/ppp/ppp.h"
#include "gprs.h"


static uint8_t conn_ok = 0;
/* The examples use simple GSM configuration that you can set via
   'make menuconfig'.
 */
#define BUF_SIZE (1024)
// *** If not using hw flow control, set it to 38400
#define UART_BDRATE 115200
#define GSM_OK_Str "OK"


const char *PPP_User = CONFIG_GSM_INTERNET_USER;
const char *PPP_Pass = CONFIG_GSM_INTERNET_PASSWORD;

// UART
#define UART_GPIO_TX 17
#define UART_GPIO_RX 16

static int uart_num = UART_NUM_1;

// The PPP control block
ppp_pcb *ppp;

// The PPP IP interface
struct netif ppp_netif;

static const char *TAG = "[PPPOS CLIENT]";


typedef struct
{
	char *cmd;
	uint16_t cmdSize;
	char *cmdResponseOnOk;
	uint32_t timeoutMs;
	uint32_t delayMs;
}GSM_Cmd;

//--------------------------
GSM_Cmd GSM_MGR_InitCmds[] =
{
		{
				.cmd = "AT\r\n",
				.cmdSize = sizeof("AT\r\n")-1,
				.cmdResponseOnOk = GSM_OK_Str,
				.timeoutMs = 3000,
		},
		{
				.cmd = "ATZ\r\n",
				.cmdSize = sizeof("ATZ\r\n")-1,
				.cmdResponseOnOk = GSM_OK_Str,
				.timeoutMs = 3000,
		},
		// {
		// 		.cmd = "AT+CFUN=4\r\n",
		// 		.cmdSize = sizeof("ATCFUN=4\r\n")-1,
		// 		.cmdResponseOnOk = GSM_OK_Str,
		// 		.timeoutMs = 3000,
		// },
		// {
		// 		.cmd = "AT+CFUN=1\r\n",
		// 		.cmdSize = sizeof("ATCFUN=4,0\r\n")-1,
		// 		.cmdResponseOnOk = GSM_OK_Str,
		// 		.timeoutMs = 3000,
		// },
		{
				.cmd = "ATE0\r\n",
				.cmdSize = sizeof("ATE0\r\n")-1,
				.cmdResponseOnOk = GSM_OK_Str,
				.timeoutMs = 3000,
		},
		// {
		// 		.cmd = "AT+CPIN?\r\n",
		// 		.cmdSize = sizeof("AT+CPIN?\r\n")-1,
		// 		.cmdResponseOnOk = "CPIN: READY",
		// 		.timeoutMs = 3000,
		// },
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
				.cmd = "ATDT*99***1#\r\n",
				.cmdSize = sizeof("ATDT*99***1#\r\n")-1,
				.cmdResponseOnOk = "CONNECT",
				.timeoutMs = 30000,
		}
		// {
		// 		.cmd = "AT+CGDATA=\"PPP\",1\r\n",
		// 		.cmdSize = sizeof("AT+CGDATA=\"PPP\",1\r\n")-1,
		// 		.cmdResponseOnOk = "CONNECT",
		// 		.timeoutMs = 30000,
		// }
};

#define GSM_MGR_InitCmdsSize  (sizeof(GSM_MGR_InitCmds)/sizeof(GSM_Cmd))


// PPP status callback
//----------------------------------------------------------------
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
		break;
	}
	case PPPERR_CONNECT: {
		ESP_LOGE(TAG,"status_cb: Connection lost\n");
		conn_ok = 0;
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

//--------------------------------------------------------------------------------
static u32_t ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
	// *** Handle sending to GSM modem ***
	uint32_t ret = uart_write_bytes(uart_num, (const char*)data, len);
  uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
  return ret;
}

//-----------------------------
static void pppos_client_task()
{
	uint8_t init_ok = 1;
	int pass = 0;
	char sresp[256] = {'\0'};

  gpio_set_direction(UART_GPIO_TX, GPIO_MODE_OUTPUT);
  gpio_set_direction(UART_GPIO_RX, GPIO_MODE_INPUT);
  gpio_set_pull_mode(UART_GPIO_RX, GPIO_PULLUP_ONLY);

  char* data = (char*) malloc(BUF_SIZE);
	char PPP_ApnATReq[sizeof(CONFIG_GSM_APN)+8];

	uart_config_t uart_config = {
			.baud_rate = UART_BDRATE,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	//Configure UART1 parameters
	uart_param_config(uart_num, &uart_config);
	//Set UART1 pins(TX, RX, RTS, CTS)
	uart_set_pin(uart_num, UART_GPIO_TX, UART_GPIO_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_driver_install(uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);

	// Set APN from config
	sprintf(PPP_ApnATReq, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", CONFIG_GSM_APN);
	GSM_MGR_InitCmds[7].cmd = PPP_ApnATReq;
	GSM_MGR_InitCmds[7].cmdSize = strlen(PPP_ApnATReq);

	ESP_LOGI(TAG,"Gsm init start");
	// *** Disconnect if connected ***
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	while (uart_read_bytes(uart_num, (uint8_t*)data, BUF_SIZE, 100 / portTICK_RATE_MS)) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
	uart_write_bytes(uart_num, "+++", 3);
  uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
	vTaskDelay(1000 / portTICK_PERIOD_MS);

	conn_ok = 98;
	while(1)
	{
		init_ok = 1;
		// Init gsm
		int gsmCmdIter = 0;
		while(1)
		{
			// ** Send command to GSM
			memset(sresp, 0, 256);
			for (int i=0; i<255;i++) {
				if ((GSM_MGR_InitCmds[gsmCmdIter].cmd[i] >= 0x20) && (GSM_MGR_InitCmds[gsmCmdIter].cmd[i] < 0x80)) {
					sresp[i] = GSM_MGR_InitCmds[gsmCmdIter].cmd[i];
					sresp[i+1] = 0;
				}
				if (GSM_MGR_InitCmds[gsmCmdIter].cmd[i] == 0) break;
			}
			printf("[GSM INIT] >Cmd: [%s]\r\n", sresp);
			vTaskDelay(100 / portTICK_PERIOD_MS);
			while (uart_read_bytes(uart_num, (uint8_t*)data, BUF_SIZE, 100 / portTICK_RATE_MS)) {
				vTaskDelay(100 / portTICK_PERIOD_MS);
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
				memset(data, 0, BUF_SIZE);
				int len = 0;
				len = uart_read_bytes(uart_num, (uint8_t*)data, BUF_SIZE, 10 / portTICK_RATE_MS);
				if (len > 0) {
					for (int i=0; i<len;i++) {
						if (idx < 255) {
							if ((data[i] >= 0x20) && (data[i] < 0x80)) {
								sresp[idx++] = data[i];
							}
							else sresp[idx++] = 0x2e;
							sresp[idx] = 0;
						}
					}
					tot += len;
				}
				else {
					if (tot > 0) {
						printf("[GSM INIT] <Resp: [%s], %d\r\n", sresp, tot);
						if (strstr(sresp, GSM_MGR_InitCmds[gsmCmdIter].cmdResponseOnOk) != NULL) {
							break;
						}
						else {
							printf("           Wrong response, expected [%s]\r\n", GSM_MGR_InitCmds[gsmCmdIter].cmdResponseOnOk);
							init_ok = 0;
							break;
						}
					}
				}
				timeoutCnt += 10;

				if (timeoutCnt > GSM_MGR_InitCmds[gsmCmdIter].timeoutMs)
				{
					printf("[GSM INIT] No response, Gsm Init Error\r\n");
					init_ok = 0;
					break;
				}
			}//GSM responses


			if (init_ok == 0) {
				// No response or not as expected
				vTaskDelay(5000 / portTICK_PERIOD_MS);
				init_ok = 1;
				gsmCmdIter = 0;
				continue;
			}

			if (gsmCmdIter == 2) vTaskDelay(500 / portTICK_PERIOD_MS);
			if (gsmCmdIter == 3) vTaskDelay(1500 / portTICK_PERIOD_MS);
			gsmCmdIter++;
			if (gsmCmdIter >= GSM_MGR_InitCmdsSize) break; // All init commands sent
			if ((pass) && (gsmCmdIter == 2)) gsmCmdIter = 4;
			if (gsmCmdIter == 6) pass++;
		}

		ESP_LOGI(TAG,"Gsm init end");

		if (conn_ok == 98) {
			// After first successful initialization
			ppp = pppapi_pppos_create(&ppp_netif,
					ppp_output_callback, ppp_status_cb, NULL);

			ESP_LOGI(TAG,"After pppapi_pppos_create");

			if(ppp == NULL)	{
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
			memset(data, 0, BUF_SIZE);
			int len = uart_read_bytes(uart_num, (uint8_t*)data, BUF_SIZE, 30 / portTICK_RATE_MS);
			if(len > 0)	{
				pppos_input_tcpip(ppp, (u8_t*)data, len);
			}
			// Check if disconnected
			if (conn_ok == 0) {
				ESP_LOGE(TAG, "Disconnected, trying again...");
				pppapi_close(ppp, 0);
				gsmCmdIter = 0;
				conn_ok = 89;
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				break;
			}
		}
	}
}

int get_gsm_status() {
	int gstat = conn_ok;
	return gstat;
}

int gprs_init() {
  tcpip_adapter_init();
  xTaskCreate(&pppos_client_task, "pppos_client_task", 4096, NULL, 10, NULL);
  return conn_ok;
}
