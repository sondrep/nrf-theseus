#include <theseus/log.h>
#include <zb_time.h>

static void task(void *arg)
{
	(void)arg;
	LOG("Zigbee timer sample starting\n");

	zb_osif_timer_start();

	if (zb_osif_timer_is_on()) {
		LOG("OSIF Timer Start success\n");
	} else {
		LOG("OSIF Timer not started successfully");
	}

	while (1) {
		zb_time_t time = osif_transceiver_time_get();
		LOG("Time is %u\n", time);

		zb_uint64_t long_time = osif_transceiver_time_get_long();
		LOG("Long time is %llu\n", long_time);
		if (time > 1000000) {
			zb_osif_timer_stop();
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

int main(void)
{
	xTaskCreate(task, "timer task", 1024, NULL, 2, NULL);
	vTaskStartScheduler();

	return 0;
}
