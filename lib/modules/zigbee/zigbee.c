
#include <nrfx_clock.h>
#include <assert.h>
#include <nrf.h>
#include <theseus/module.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <core_cm33.h>
#include <stdio.h>

static void fault_handler_(const char *file, const uint32_t line)
{
	printf("MPSL fault: %s:%lu\n", file ? file : "?", (unsigned long)line);
	assert(0);
}

static int zigbee_init(void)
{

	return 0;
}

THESEUS_MODULE_SET(zigbee) = {.init = zigbee_init, .stage = THESEUS_MODULE_STAGE_INTERMEDIARY};
