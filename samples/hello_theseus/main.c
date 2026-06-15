#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <FreeRTOS.h>
#include <task.h>
#include <theseus/log.h>


void helloTask1(void *pvParameters) {
    while (1) {
        printf("Hello from Theseus!\n");
        vTaskDelay(pdMS_TO_TICKS(475));
    }
}
void helloTask2(void *pvParameters) {
    while (1) {
        printf("Hello from Epidaurus!\n");
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
void helloTask3(void *pvParameters) {
    while (1) {
        printf("Hello from Isthmian!\n");
        vTaskDelay(pdMS_TO_TICKS(325));
    }
}
void helloTask4(void *pvParameters) {
    while (1) {
        printf("Hello from Megara!\n");
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

int main(void)
{
    theseus_console_init();

    xTaskCreate(helloTask1, "Theseus", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(helloTask2, "Epidaurus", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(helloTask3, "Isthmian", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(helloTask4, "Megara", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

    /* Hand control to FreeRTOS, the tasks now run on their own */
    vTaskStartScheduler();

    /* We only get here if the scheduler couldn't start (out of heap) */
    while (1);
    return 0;
}
