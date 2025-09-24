#ifndef WIFI_PROV_H
#define WIFI_PROV_H


#include "esp_err.h"


// Estructura ÚNICA para guardar todas nuestras credenciales
typedef struct {
    // Credenciales de Red
    char wifi_ssid[32];
    char wifi_pass[64];
    
    // Credenciales del Broker MQTT
    char mqtt_url[128];
    char mqtt_user[64];
    char mqtt_pass[64];
    char mqtt_topic[128]; 
} app_config_t;

esp_err_t provision_start(app_config_t *config);

#endif // WIFI_PROV_H