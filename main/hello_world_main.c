/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

void app_main(void)
{
    printf("Hola mundo desde ESP-IDF!\n");
    
    // Bucle infinito que se ejecuta cada 1 segundo
    while (1) {
        printf("Reiniciando en 10 segundos...\n");
        for (int i = 10; i >= 0; i--) {
            printf("Reiniciando en %d...\n", i);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        printf("Reiniciando ahora!\n");
        fflush(stdout); // Asegura que el texto se imprima antes de reiniciar
        esp_restart();
    }
}