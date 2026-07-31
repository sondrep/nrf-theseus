#include <theseus/log.h>
#include <zb_transceiver.h>
#include <zboss_api.h>
#include <zboss_api_buf.h>

static void task(void *arg)
{
	LOG("This is a ligma sample\n");

	zb_trans_hw_init();

	zb_trans_set_pan_id(0xCAFE);

	uint64_t long_addr = 0xCAFEBABE;
	zb_trans_set_long_addr((uint8_t *)(&long_addr));

	zb_trans_set_short_addr(0xBABE);

	zb_ret_t ok = zb_trans_set_channel(11);
	if (ok == RET_OK) {
		LOG("THIS IS OK\n");
	}

	zb_int8_t power = 0;
	zb_trans_set_tx_power(power);

	zb_trans_get_tx_power(&power);

	zb_trans_set_pan_coord(true);

	zb_trans_set_promiscuous_mode(true);

	zb_trans_transmit(ZB_MAC_TX_WAIT_NONE, 0,
			  (zb_uint8_t *)"\x7E\x00\x13\x10\x01\x00\x13\xA2\x00\x40\xDA\x9D\x23\xA6"
					"\xB9\x00\x00\x48"
					"\x65\x6C\x6C\x6F\x0C",
			  0);
	zb_trans_enter_receive();

	zb_bool_t is_not_sleeping = zb_trans_is_active();
	if (is_not_sleeping) {
		LOG("Not sleep\n");
	} else {
		LOG("Sleep\n");
	}

	while (zb_trans_is_receiving()) {
		if (zb_trans_rx_pending()) {
			zb_bufid_t buf = zb_buf_get(ZB_TRUE, 0);
			if (buf == ZB_BUF_INVALID) {
				LOG("Could not allocate buffer.\n");
			}
			zb_trans_get_next_packet(buf);
			zb_time_t time = *ZB_BUF_GET_PARAM(buf, zb_time_t);
			LOG("Frame time is %u\n", time);
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

int main(void)
{
	xTaskCreate(task, "tramsceiver task", 1024, NULL, 2, NULL);
	vTaskStartScheduler();

	return 0;
}
