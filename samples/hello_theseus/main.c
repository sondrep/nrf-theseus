/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <FreeRTOS.h>
#include <task.h>
#include <theseus/log.h>
#include <theseus/module.h>

void helloTask1(void *pvParameters) {
    while (1) {
        LOG("Hello from Theseus!\n");
        vTaskDelay(pdMS_TO_TICKS(475));
    }
}
void helloTask2(void *pvParameters) {
    while (1) {
        LOG("Hello from Epidaurus!\n");
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
void helloTask3(void *pvParameters) {
    while (1) {
        LOG("Hello from Isthmian!\n");
        vTaskDelay(pdMS_TO_TICKS(325));
    }
}
void helloTask4(void *pvParameters) {
    while (1) {
        LOG("Hello from Megara!\n");
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

int main(void)
{
    xTaskCreate(helloTask1, "Theseus",   configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(helloTask2, "Epidaurus", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(helloTask3, "Isthmian",  configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(helloTask4, "Megara",    configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

    /* Hand control to FreeRTOS, the tasks now run on their own */
    vTaskStartScheduler();

    /* We only get here if the scheduler couldn't start (out of heap) */
    while (1);
    return 0;
}
