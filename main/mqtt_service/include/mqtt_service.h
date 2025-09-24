#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include "esp_err.h"
#include "wifi_prov.h" // Incluimos para tener acceso a la estructura app_config_t

/**
 * @brief Inicializa y arranca el cliente MQTT con la configuración proporcionada.
 * * @param config Puntero a la estructura de configuración que contiene las credenciales de InfluxDB.
 * @return esp_err_t Resultado de la operación.
 */
esp_err_t mqtt_service_start(const app_config_t* config);

/**
 * @brief Formatea y publica las lecturas de los sensores a InfluxDB.
 * * @param temp Temperatura en grados Celsius.
 * @param press Presión en Pascales.
 * @param hum Humedad relativa en %.
 * @return esp_err_t Resultado de la operación.
 */
esp_err_t mqtt_service_publish_sensors(float temp, float press, float hum);

#endif // MQTT_SERVICE_H