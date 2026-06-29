
#include <FreeRTOS.h>
#include <task.h>
#include <assert.h>
#include <theseus/log.h>
#include <nrf_modem.h>
#include <string.h>
#include <nrf_modem_at.h>
#include <theseus/modem.h>

static SemaphoreHandle_t cereg_sem;

static void notif_handler(const char *notif)
{
	/* Registered, home network; registered: roaming. */
	if (!strncmp(notif, "+CEREG: 1", strlen("+CEREG: 1")) ||
	    !strncmp(notif, "+CEREG: 5", strlen("+CEREG: 5"))) {
		xSemaphoreGive(cereg_sem);
	}
}

void http_task(void *param)
{
	int err;

	err = modem_init();
	if (err != 0) {
		LOG("[APP] nrf_modem_init_failed error: %d", err);
	}

	cereg_sem = xSemaphoreCreateBinary();

	err = nrf_modem_at_notif_handler_set(notif_handler);
	if (err != 0) {
		LOG("[APP] nrf_modem_at_notif_handler_set error: %d", err);
	}

	err = nrf_modem_at_printf("AT+CEREG=1");
	if (err != 0) {
		LOG("[APP] nrf_modem_at_printf error: %d", err);
	}

	err = nrf_modem_at_printf("AT+CFUN=1");
	if (err != 0) {
		LOG("[APP] nrf_modem_at_printf error: %d", err);
	}

	xSemaphoreTake(cereg_sem, portMAX_DELAY);
}

int main(void)
{
	int ret = 0;

	LOG("This is a joe mama sample\n");

	BaseType_t ok = xTaskCreate(http_task, "http", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
	assert(ok == pdPASS);

	LOG("[APP] starting scheduler\n");
	vTaskStartScheduler();

	while (1) {
		/* Should never reach here unless the scheduler can't start (eks: no heap). */
	}

	return 0;
}
