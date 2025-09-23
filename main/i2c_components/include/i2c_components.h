#ifndef I2C_COMPONENTS_H
#define I2C_COMPONENTS_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <bmp280.h>
#include <sht3x.h> // CAMBIO: Incluimos la nueva librería

// Esta es nuestra "clase". Contiene todos los datos del sensor.
typedef struct {
    // Manejadores de los dispositivos físicos
    bmp280_t dev_bmp;
    sht3x_t dev_sht; // CAMBIO: Usamos el tipo de la nueva librería

    // Estado de los sensores
    bool is_bme280;
    bool sht3x_available; // CAMBIO: Renombramos la variable de estado
    
    // Mutex para garantizar acceso seguro a los datos
    SemaphoreHandle_t mutex;

    // Últimos valores leídos
    float temperature;
    float pressure;
    float humidity;
} EnvironmentalSensor_t;

// --- El resto de las declaraciones de funciones no cambia ---

/**
 * @brief Inicializa los sensores y la tarea de actualización. "Constructor".
 */
EnvironmentalSensor_t* sensor_init(void);

/**
 * @brief Obtiene el último valor de temperatura leído. "Getter".
 */
float sensor_get_temperature(EnvironmentalSensor_t* sensor);

/**
 * @brief Obtiene el último valor de presión leído. "Getter".
 */
float sensor_get_pressure(EnvironmentalSensor_t* sensor);

/**
 * @brief Obtiene el último valor de humedad leído. "Getter".
 */
float sensor_get_humidity(EnvironmentalSensor_t* sensor);

#endif // I2C_COMPONENTS_H