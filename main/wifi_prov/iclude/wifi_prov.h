#ifndef WIFI_PROV_H
#define WIFI_PROV_H


#include "esp_err.h"


// Estructura ÚNICA para guardar todas nuestras credenciales
typedef struct {
    // Credenciales de Red
    char wifi_ssid[32];
    char wifi_pass[64];
    
    // Credenciales de InfluxDB
    char influx_url[128];
    char influx_token[128];
    char influx_org[64];
    char influx_bucket[64];
} app_config_t;

/**
 * @brief Inicia el proceso de provisioning.
 * - Modo AP: Sirve la página para configurar solo el Wi-Fi.
 * - Modo Station: Se conecta al Wi-Fi y sirve ambas páginas (/ y /config).
 */
esp_err_t provision_start(app_config_t *config);

#endif // WIFI_PROV_H