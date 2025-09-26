#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include "esp_err.h"
#include "wifi_prov.h" 

esp_err_t mqtt_service_start(const app_config_t* config);
esp_err_t mqtt_service_publish_sensors(float temp, float press, float hum);

#endif // MQTT_SERVICE_H