#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_service.h"
#include "esp_crt_bundle.h"

static const char *TAG = "MQTT_SERVICE";



static esp_mqtt_client_handle_t client = NULL;
static app_config_t service_config;

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
    memcpy(&service_config, config, sizeof(app_config_t));

    ESP_LOGI(TAG, "Iniciando MQTT con Broker URL: %s", service_config.mqtt_url);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = service_config.mqtt_url,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.username = service_config.mqtt_user, 
        .credentials.authentication.password = service_config.mqtt_pass
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(client);
}


// --- ¡CONFIGURACIÓN ESTÁTICA AQUÍ! ---

/*
#include "esp_crt_bundle.h"
#define HIVEMQ_BROKER_URL      "mqtts://43e80d2dd7ce45a2b580baf272f1dae7.s1.eu.hivemq.cloud:8883"
#define HIVEMQ_USERNAME        "yesid"
#define HIVEMQ_PASSWORD        "123456Ya"
#define MQTT_PUBLISH_TOPIC     "esp32/sensor/data"

// ----------------------------------------------------


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "¡CONEXION EXITOSA AL BROKER MQTT!");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Desconectado del broker MQTT");
        break;
    default:
        break;
    }
}

esp_err_t mqtt_service_start(const app_config_t* config)
{
    ESP_LOGI(TAG, "Iniciando MQTT con Broker URL: %s", HIVEMQ_BROKER_URL);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = HIVEMQ_BROKER_URL,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.username = HIVEMQ_USERNAME, 
        .credentials.authentication.password = HIVEMQ_PASSWORD
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(client);
}
*/



// Función para publicar los datos de los sensores
esp_err_t mqtt_service_publish_sensors(float temp, float press, float hum)
{
    if (client == NULL) {
        ESP_LOGE(TAG, "El cliente MQTT no ha sido inicializado.");
        return ESP_FAIL;
    }

 
    // 2. Construir el Payload en formato "Line Protocol"
    char payload[256];
    snprintf(payload, sizeof(payload), 
                "{\"temperature\":%.2f, \"pressure\":%.2f, \"humidity\":%.2f}", 
                temp, press, hum);

    ESP_LOGI(TAG, "Publicando en topic '%s': %s", service_config.mqtt_topic, payload);

    int msg_id = esp_mqtt_client_publish(client, service_config.mqtt_topic, payload, 0, 1, 0);

    if (msg_id > 0) {
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error al publicar el mensaje.");
        return ESP_FAIL;
    }
}