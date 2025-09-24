#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_service.h"

static const char *TAG = "MQTT_SERVICE";

// Variables estáticas para guardar el cliente y la configuración
static esp_mqtt_client_handle_t client = NULL;
static app_config_t service_config;

// Manejador de eventos para saber si nos conectamos, desconectamos, etc.
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado al broker MQTT");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Desconectado del broker MQTT");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Mensaje publicado con éxito, msg_id=%d", event->msg_id);
        break;
    default:
        break;
    }
}

// Función para iniciar el servicio
esp_err_t mqtt_service_start(const app_config_t* config)
{
    // Copiamos la configuración para tenerla disponible en la función de publicar
    memcpy(&service_config, config, sizeof(app_config_t));

    ESP_LOGI(TAG, "Iniciando MQTT con Broker URL: %s", service_config.influx_url);
    
    // Configuramos el cliente MQTT para InfluxDB
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = service_config.influx_url,
        .broker.verification.skip_cert_common_name_check = true,
        .credentials.username = "esp32-sensor", 
        // ¡IMPORTANTE! La contraseña es el Token de InfluxDB
        .credentials.authentication.password = service_config.influx_token
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(client);
}

// Función para publicar los datos de los sensores
esp_err_t mqtt_service_publish_sensors(float temp, float press, float hum)
{
    if (client == NULL) {
        ESP_LOGE(TAG, "El cliente MQTT no ha sido inicializado.");
        return ESP_FAIL;
    }

    // 1. Construir el Topic: "organizacion/bucket"
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%s", service_config.influx_org, service_config.influx_bucket);

    // 2. Construir el Payload en formato "Line Protocol"
    char payload[256];
    snprintf(payload, sizeof(payload), 
             "environment,device=esp32 temperature=%.2f,pressure=%.2f,humidity=%.2f", 
             temp, 
             press, // La presión ya está en Pascales
             hum);

    ESP_LOGI(TAG, "Publicando en topic '%s': %s", topic, payload);

    // 3. Publicar el mensaje
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);

    if (msg_id > 0) {
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error al publicar el mensaje.");
        return ESP_FAIL;
    }
}