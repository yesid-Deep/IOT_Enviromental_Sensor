
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Nuestros componentes personalizados
#include "wifi_prov.h"
#include "i2c_components.h"
#include "mqtt_service.h"

static const char *TAG = "MAIN_APP";

// Puntero estático para el manejador de sensores, accesible por las funciones de este archivo
static EnvironmentalSensor_t *sensor_handle = NULL;

/**
 * @brief Se encarga de la lógica de inicialización del Wi-Fi.
 *
 * @param config Puntero a la estructura donde se cargarán las credenciales.
 * @return true Si el Wi-Fi se conectó con éxito.
 * @return false Si el Wi-Fi falló.
 */
static bool initialize_wifi(app_config_t *config)
{
    ESP_LOGI(TAG, "--- PASO 1: INICIANDO WI-FI ---");
    if (provision_start(config) == ESP_OK) {
        ESP_LOGI(TAG, "¡Wi-Fi conectado a '%s'!", config->wifi_ssid);
        return true;
    } else {
        ESP_LOGE(TAG, "Fallo en el provisioning de Wi-Fi. Deteniendo.");
        return false;
    }
}

/**
 * @brief Inicializa los servicios que dependen de la red (MQTT y Sensores).
 *
 * @param config Puntero a la configuración ya cargada.
 * @return true Si todos los servicios se iniciaron correctamente.
 * @return false Si algún servicio falló.
 */
static bool initialize_services(const app_config_t* config)
{
    ESP_LOGI(TAG, "--- PASO 2: INICIANDO SERVICIOS ---");

    // Revisar si tenemos configuración de InfluxDB antes de iniciar MQTT
    if (strlen(config->mqtt_url) == 0 || strlen(config->mqtt_user) == 0|| strlen(config->mqtt_pass) == 0 ) {
        ESP_LOGW(TAG, "No se encontraron credenciales de InfluxDB.");
        ESP_LOGW(TAG, "Por favor, conéctate a http://esp32-sensor.local/config para configurarlo.");
        // Decidimos continuar sin MQTT, pero podrías retornar 'false' si es crítico.
    } else {
        if (mqtt_service_start(config) != ESP_OK) {
            ESP_LOGE(TAG, "Fallo al iniciar el servicio MQTT.");
            return false; // Error crítico si no puede iniciar
        }
        ESP_LOGI(TAG, "Servicio MQTT iniciado correctamente.");
    }

    // Iniciar sensores
    sensor_handle = sensor_init();
    if (sensor_handle == NULL) {
        ESP_LOGE(TAG, "Fallo al inicializar los sensores.");
        return false;
    }
    ESP_LOGI(TAG, "Sensores inicializados correctamente.");
    
    return true;
}

/**
 * @brief Bucle principal de la aplicación: leer sensores y publicar datos.
 *
 * Esta función contiene el bucle infinito que se ejecutará mientras el dispositivo
 * esté en funcionamiento normal.
 */
static void run_application_loop(void)
{
    ESP_LOGI(TAG, "--- PASO 3: INICIANDO BUCLE PRINCIPAL ---");
    while (1) {
        // Esperamos 10 segundos entre cada publicación
        vTaskDelay(pdMS_TO_TICKS(5000));

        // Leemos los datos más recientes de los sensores
        float temp = sensor_get_temperature(sensor_handle);
        float press = sensor_get_pressure(sensor_handle);
        float hum = sensor_get_humidity(sensor_handle);

        // Mostramos la lectura en la consola local para depuración
        printf("----------------------------------\n");
        printf("  🌡️ Lectura Local: Temp: %.2f C, Pres: %.2f hPa, Hum: %.2f %%\n", temp, press / 100.0, hum);
        
        // Publicamos los datos a través del servicio MQTT
        mqtt_service_publish_sensors(temp, press, hum);
    }
}


void app_main(void)
{
    app_config_t config = {0};

    // La ejecución del programa ahora es una secuencia clara de pasos
    if (initialize_wifi(&config)) {
        if (initialize_services(&config)) {
            run_application_loop();
        }
    }

    ESP_LOGE(TAG, "La aplicación ha finalizado debido a un error en la inicialización.");
}