#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <i2cdev.h>
#include <freertos/task.h>

#include "i2c_components.h" // Incluimos nuestra propia cabecera

#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
static const char *TAG = "SENSOR_COMPONENT";
float temp=0;
float press=0;
float hum=0;
// TAREA DEDICADA: Se ejecuta en segundo plano para leer los sensores.
static void sensor_update_task(void *pvParameters)
{
    EnvironmentalSensor_t *sensor = (EnvironmentalSensor_t *)pvParameters;
    
    // Variables temporales para las lecturas
    float temp_bmp, press, hum_bme;
    float temp_sht, hum_sht;

    while (1)
    {
        // 1. Leemos del BMP/BME280
        if (bmp280_read_float(&sensor->dev_bmp, &temp_bmp, &press, &hum_bme) != ESP_OK)
        {
            ESP_LOGE(TAG, "Fallo al leer del BMP/BME280");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        // 2. Si el SHT3x está disponible, leemos de él
        if (sensor->sht3x_available)
        {
            // La función sht3x_measure obtiene temperatura y humedad en una sola llamada
            if (sht3x_measure(&sensor->dev_sht, &temp_sht, &hum_sht) != ESP_OK)
            {
                ESP_LOGE(TAG, "Fallo al leer del SHT3x");
            }
        }

        // --- SECCIÓN CRÍTICA: Actualizamos los datos compartidos ---
        if (xSemaphoreTake(sensor->mutex, portMAX_DELAY) == pdTRUE) {
            // Siempre actualizamos la presión desde el BMP280
            sensor->pressure = press;
            if (sensor->is_bme280) {
                // Si es un BME280, usamos sus propios datos
                sensor->temperature = temp_bmp;
                sensor->humidity = hum_bme;
            } 
            else if (sensor->sht3x_available) {
                // Si tenemos BMP280 + SHT3x, priorizamos los datos del SHT3x
                // ya que suele ser más preciso para la temperatura ambiente.
                sensor->temperature = temp_sht;
                sensor->humidity = hum_sht;
            } 
            else {
                // Si solo tenemos un BMP280, solo tenemos su temperatura
                sensor->temperature = temp_bmp;
                sensor->humidity = 0; // No hay dato de humedad
            }
            
            xSemaphoreGive(sensor->mutex);
        }
        // --- FIN DE LA SECCIÓN CRÍTICA ---

        vTaskDelay(pdMS_TO_TICKS(500)); // Esperamos 2 segundos
    }
}

// "CONSTRUCTOR" de nuestra clase
EnvironmentalSensor_t* sensor_init(void)
{
    ESP_ERROR_CHECK(i2cdev_init());

    EnvironmentalSensor_t *sensor = malloc(sizeof(EnvironmentalSensor_t));
    if (sensor == NULL) {
        ESP_LOGE(TAG, "Fallo al reservar memoria para el sensor");
        return NULL;
    }
    memset(sensor, 0, sizeof(EnvironmentalSensor_t));
    
    sensor->mutex = xSemaphoreCreateMutex();

    // --- Inicialización del BMP/BME280 (sin cambios) ---
    bmp280_params_t params;
    bmp280_init_default_params(&params);
    ESP_ERROR_CHECK(bmp280_init_desc(&sensor->dev_bmp, BMP280_I2C_ADDRESS_0, 0, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO));
    ESP_ERROR_CHECK(bmp280_init(&sensor->dev_bmp, &params));
    sensor->is_bme280 = (sensor->dev_bmp.id == BME280_CHIP_ID);
    ESP_LOGI(TAG, "Sensor de presión encontrado: %s", sensor->is_bme280 ? "BME280" : "BMP280");

    // --- CAMBIO: Inicialización del SHT3x ---
    ESP_ERROR_CHECK(sht3x_init_desc(&sensor->dev_sht, SHT3X_I2C_ADDR_GND, 0, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO));
    if (sht3x_init(&sensor->dev_sht) == ESP_OK) {
        sensor->sht3x_available = true;
        ESP_LOGI(TAG, "Sensor de humedad SHT3x encontrado.");
    }

    // Creamos la tarea de actualización y le pasamos el puntero al sensor
    xTaskCreate(sensor_update_task, "sensor_update_task", configMINIMAL_STACK_SIZE * 4, sensor, 5, NULL);
    
    return sensor;
}

// --- Los GETTERS no necesitan ningún cambio ---
float sensor_get_temperature(EnvironmentalSensor_t* sensor)
{
    if (xSemaphoreTake(sensor->mutex, portMAX_DELAY) == pdTRUE) {
        temp = sensor->temperature;
        xSemaphoreGive(sensor->mutex);
    }
    return temp;
}

float sensor_get_pressure(EnvironmentalSensor_t* sensor)
{
    
    if (xSemaphoreTake(sensor->mutex, portMAX_DELAY) == pdTRUE) {
        press = sensor->pressure;
        xSemaphoreGive(sensor->mutex);
    }
    return press;
}

float sensor_get_humidity(EnvironmentalSensor_t* sensor)
{
    
    if (xSemaphoreTake(sensor->mutex, portMAX_DELAY) == pdTRUE) {
        hum = sensor->humidity;
        xSemaphoreGive(sensor->mutex);
    }
    return hum;
}