#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_prov.h"
#include "i2c_components.h"

static const char *TAG = "MAIN_APP";

static void start_and_run_sensors(void)
{
    ESP_LOGI(TAG, "Iniciando los sensores I2C...");
    EnvironmentalSensor_t *my_sensor = sensor_init();
    
    if (my_sensor == NULL) {
        ESP_LOGE(TAG, "Fallo al inicializar el componente de sensores. Deteniendo tarea.");
        return;
    }
    
    ESP_LOGI(TAG, "Sensores inicializados. Entrando en bucle de lectura.");

    // Bucle principal para leer los sensores y mostrarlos en el monitor serie.
    while (1) {
        // Obtenemos los últimos valores usando los getters.
        float temp = sensor_get_temperature(my_sensor);
        float press = sensor_get_pressure(my_sensor);
        float hum = sensor_get_humidity(my_sensor);

        // Imprimimos los valores de forma clara.
        printf("----------------------------------\n");
        printf("  🌡️ Temperatura: %.2f C\n", temp);
        printf("  💨 Presión:     %.2f hPa\n", press / 100.0);
        
        if(my_sensor->is_bme280 || my_sensor->sht3x_available){
           printf("  💧 Humedad:     %.2f %%\n", hum);
        } else {
           printf("     Humedad:     No disponible\n");
        }
        
        // Esperamos 5 segundos antes de la siguiente lectura.
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}


void app_main(void)
{
    // Esta estructura contendrá las credenciales cargadas desde la memoria NVS.
    app_config_t config = {0};

    ESP_LOGI(TAG, "--- INICIO DEL SERVIDOR AMBIENTAL ---");
    ESP_LOGI(TAG, "Paso 1: Iniciando provisioning de Wi-Fi...");
    esp_err_t wifi_status = provision_start(&config);

    if (wifi_status == ESP_OK) {
        ESP_LOGI(TAG, "Paso 1: ¡Éxito! Wi-Fi conectado a '%s'", config.wifi_ssid);
        
        // --- BLOQUE 2: LÓGICA DE SENSORES ---
        // Si el Wi-Fi está listo, llamamos a la función dedicada a los sensores.
        //ESP_LOGI(TAG, "Paso 2: Iniciando la lógica de los sensores.");
        //start_and_run_sensors();

    } else {
        ESP_LOGE(TAG, "Paso 1: ¡Fallo crítico! No se pudo configurar o conectar el Wi-Fi.");
    }

    ESP_LOGE(TAG, "La aplicación ha finalizado de forma inesperada.");
}