#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "i2c_components.h"

void app_main(void)
{
    // 1. Creamos e inicializamos nuestro "objeto" sensor.
    EnvironmentalSensor_t *my_sensor = sensor_init();

    if (my_sensor == NULL) {
        ESP_LOGE("MAIN", "Fallo al inicializar el componente de sensores.");
        return;
    }

    // 2. Bucle principal para mostrar los datos.
    while (1) {
        // Obtenemos los últimos valores usando los getters.
        float temp = sensor_get_temperature(my_sensor);
        float press = sensor_get_pressure(my_sensor);
        float hum = sensor_get_humidity(my_sensor);

        // Imprimimos los valores.
        printf("--- Datos Actuales ---\n");
        printf("Temperatura: %.2f C\n", temp);
        printf("Presión:     %.2f hPa\n", press / 100.0);
        
        if(my_sensor->is_bme280 || my_sensor->sht3x_available){
           printf("Humedad:     %.2f %%\n", hum);
        } else {
           printf("Humedad:     No disponible\n");
        }
        printf("----------------------\n\n");
        
        // Esperamos 5 segundos para volver a mostrar.
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}