#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "wifi_prov.h"
#include "mdns.h"


static const char *TAG = "WIFI_PROV";
#define WIFI_MAXIMUM_RETRY  5
static int s_retry_num = 0;


const char* wifi_page_html =
    "<!DOCTYPE html><html><head><title>Configuracion Wi-Fi</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<style>"
    "body{font-family: Arial, sans-serif; display: flex; justify-content: center; background-color: #f0f0f0; margin-top: 50px;}"
    ".form-container{width: 300px; background-color: #fff; border: 1px solid #ccc; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}"
    "h2{text-align: center; color: #333;} label{font-weight: bold; color: #555;} input{width: 95%; padding: 8px; margin-top: 5px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 4px;}"
    "button{width: 100%; padding: 10px; background-color: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px;}"
    "button:hover{background-color: #0056b3;} a{display: block; text-align: center; margin-top: 20px; color: #007bff; text-decoration: none;} a:hover{text-decoration: underline;}"
    "</style></head>"
    "<body><div class=\"form-container\"><h2>Configurar Wi-Fi</h2>"
    "<form id=\"wifiForm\"><label for=\"ssid\">SSID Wi-Fi:</label><input type=\"text\" id=\"ssid\" name=\"ssid\"><label for=\"pass\">Password Wi-Fi:</label><input type=\"password\" id=\"pass\" name=\"pass\"><button type=\"submit\">Guardar y Reiniciar</button></form>"
    "<div id=\"config-link-container\"></div></div>" // Contenedor para el enlace dinámico
    "<script>"
    "document.getElementById('wifiForm').addEventListener('submit', function(e){ e.preventDefault(); const formData = {ssid: document.getElementById('ssid').value, pass: document.getElementById('pass').value}; fetch('/save_wifi', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(formData)}).then(res => alert('Wi-Fi Guardado! El dispositivo se reiniciara.')).catch(err => alert('Error.')); });"
    // Script que añade el enlace a /config solo si no estamos en modo AP
    "if (window.location.hostname !== '192.168.4.1') { const linkContainer = document.getElementById('config-link-container'); const configLink = document.createElement('a'); configLink.href = '/config'; configLink.innerText = 'Ir a Config. del Servidor'; linkContainer.appendChild(configLink); }"
    "</script></body></html>";

const char* influx_page_html =
    "<!DOCTYPE html><html><head><title>Configurar Broker</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<style>"
    "body{font-family: Arial, sans-serif; display: flex; justify-content: center; background-color: #f0f0f0; margin-top: 50px;}"
    ".form-container{width: 300px; background-color: #fff; border: 1px solid #ccc; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}"
    "h2{text-align: center; color: #333;} label{font-weight: bold; color: #555;} input{width: 95%; padding: 8px; margin-top: 5px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 4px;}"
    "button{width: 100%; padding: 10px; background-color: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px;}"
    "button:hover{background-color: #218838;} a{display: block; text-align: center; margin-top: 20px; color: #007bff; text-decoration: none;} a:hover{text-decoration: underline;}"
    "</style></head>"
    "<body><div class=\"form-container\"><h2>Configurar Broker MQTT</h2>"
    "<form id=\"mqttForm\">"
    "<label for=\"url\">URL del Broker:</label><input type=\"text\" id=\"url\" name=\"url\" placeholder=\"mqtts://...\">"
    "<label for=\"user\">Usuario:</label><input type=\"text\" id=\"user\" name=\"user\">"
    "<label for=\"pass\">Contrasena:</label><input type=\"password\" id=\"pass\" name=\"pass\">"
    "<label for=\"topic\">Topic de Publicacion:</label><input type=\"text\" id=\"topic\" name=\"topic\" placeholder=\"ej: esp32/sensor/data\">"
    "<button type=\"submit\">Guardar Configuracion</button></form>"
    "<a href=\"/\">Volver a Config. de Wi-Fi</a></div>"
    "<script>"
    "document.getElementById('mqttForm').addEventListener('submit', function(e){ e.preventDefault();"
    "const formData = {url: document.getElementById('url').value, user: document.getElementById('user').value, pass: document.getElementById('pass').value, topic: document.getElementById('topic').value};"
    "fetch('/save_influx', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(formData)})"
    ".then(res => alert('Configuracion del Broker guardada!')).catch(err => alert('Error.'));"
    "});</script></body></html>";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static httpd_handle_t server = NULL;
static app_config_t *prov_config;

static bool parse_json_value(const char *json_string, const char *key, char *result_buffer, size_t buffer_size) {
    char key_to_find[64];
    snprintf(key_to_find, sizeof(key_to_find), "\"%s\":\"", key);
    char *key_ptr = strstr(json_string, key_to_find);
    if (key_ptr == NULL) return false;
    char *value_ptr = key_ptr + strlen(key_to_find);
    char *end_quote_ptr = strchr(value_ptr, '\"');
    if (end_quote_ptr == NULL) return false;
    size_t value_len = end_quote_ptr - value_ptr;
    if (value_len < buffer_size) {
        memcpy(result_buffer, value_ptr, value_len);
        result_buffer[value_len] = '\0';
        return true;
    }
    return false;
}

static esp_err_t save_config_to_nvs(const app_config_t *config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;
    nvs_set_str(nvs_handle, "wifi_ssid", config->wifi_ssid);
    nvs_set_str(nvs_handle, "wifi_pass", config->wifi_pass);
    nvs_set_str(nvs_handle, "mqtt_url", config->mqtt_url);
    nvs_set_str(nvs_handle, "mqtt_user", config->mqtt_user);
    nvs_set_str(nvs_handle, "mqtt_pass", config->mqtt_pass);
    nvs_set_str(nvs_handle, "mqtt_topic", config->mqtt_topic);
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

static esp_err_t load_config_from_nvs(app_config_t *config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS: No se encontro el namespace 'storage'.");
        return err;
    }

    size_t required_size;
    // Usamos el operador |= para acumular cualquier error que ocurra.
    // Si una sola de estas lecturas falla, 'err' no será ESP_OK.
    err |= nvs_get_str(nvs_handle, "wifi_ssid", config->wifi_ssid, &(size_t){sizeof(config->wifi_ssid)});
    err |= nvs_get_str(nvs_handle, "wifi_pass", config->wifi_pass, &(size_t){sizeof(config->wifi_pass)});
    err |= nvs_get_str(nvs_handle, "mqtt_url", config->mqtt_url, &(size_t){sizeof(config->mqtt_url)});
    err |= nvs_get_str(nvs_handle, "mqtt_user", config->mqtt_user, &(size_t){sizeof(config->mqtt_user)});
    err |= nvs_get_str(nvs_handle, "mqtt_pass", config->mqtt_pass, &(size_t){sizeof(config->mqtt_pass)});
    err |= nvs_get_str(nvs_handle, "mqtt_topic", config->mqtt_topic, &(size_t){sizeof(config->mqtt_topic)});
    nvs_close(nvs_handle);

    nvs_close(nvs_handle);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS: Faltan una o mas credenciales en la memoria.");
    } else {
        ESP_LOGI(TAG, "NVS: Todas las credenciales fueron cargadas exitosamente.");
    }

    return err;
}




static void trigger_factory_reset(void) {
    ESP_LOGW(TAG, "Disparando reseteo de fabrica en el proximo reinicio...");
    nvs_handle_t nvs_handle;
    // No usamos "storage", usamos el namespace por defecto para esta bandera
    nvs_open("nvs", NVS_READWRITE, &nvs_handle);
    // Escribimos un 1 en la bandera 'reset_flag'
    nvs_set_u8(nvs_handle, "reset_flag", 1);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}





static esp_err_t save_wifi_post_handler(httpd_req_t *req) {
    char buf[128] = {0};
    httpd_req_recv(req, buf, sizeof(buf) - 1);
    
    parse_json_value(buf, "ssid", prov_config->wifi_ssid, sizeof(prov_config->wifi_ssid));
    parse_json_value(buf, "pass", prov_config->wifi_pass, sizeof(prov_config->wifi_pass));

    // --- MEJORA: No guardar si el SSID está vacío ---
    if (strlen(prov_config->wifi_ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "El SSID no puede estar vacio");
        return ESP_FAIL;
    }

    if (save_config_to_nvs(prov_config) == ESP_OK) {
        httpd_resp_send(req, "Wi-Fi guardado. Reiniciando...", HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else { // ... (código de error igual)
    }
    return ESP_OK;
}

static esp_err_t save_influx_post_handler(httpd_req_t *req) {
    char buf[512];
    httpd_req_recv(req, buf, sizeof(buf));
    parse_json_value(buf, "url", prov_config->mqtt_url, sizeof(prov_config->mqtt_url));
    parse_json_value(buf, "user", prov_config->mqtt_user, sizeof(prov_config->mqtt_user));
    parse_json_value(buf, "pass", prov_config->mqtt_pass, sizeof(prov_config->mqtt_pass));
    parse_json_value(buf, "topic", prov_config->mqtt_topic, sizeof(prov_config->mqtt_topic));
    if (save_config_to_nvs(prov_config) == ESP_OK) {
        httpd_resp_send(req, "Configuracion del Broker guardada.", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Fallo al guardar");
    }
    return ESP_OK;
}

static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set("esp32-sensor"); // El nombre que usarás: http://esp32-sensor.local
    mdns_instance_name_set("Servidor Ambiental ESP32");
    
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS inicializado. Accede via http://esp32-sensor.local");
}

static esp_err_t wifi_get_handler(httpd_req_t *req) { httpd_resp_send(req, wifi_page_html, HTTPD_RESP_USE_STRLEN); return ESP_OK; }
static esp_err_t influx_get_handler(httpd_req_t *req) { httpd_resp_send(req, influx_page_html, HTTPD_RESP_USE_STRLEN); return ESP_OK; }

httpd_uri_t uri_wifi_get = { .uri = "/", .method = HTTP_GET, .handler = wifi_get_handler };
httpd_uri_t uri_influx_get = { .uri = "/config", .method = HTTP_GET, .handler = influx_get_handler };
httpd_uri_t uri_wifi_post = { .uri = "/save_wifi", .method = HTTP_POST, .handler = save_wifi_post_handler };
httpd_uri_t uri_influx_post = { .uri = "/save_influx", .method = HTTP_POST, .handler = save_influx_post_handler };

static void start_webserver(bool provision_only) {
    if (server == NULL) {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        if (httpd_start(&server, &config) == ESP_OK) {
            httpd_register_uri_handler(server, &uri_wifi_get);
            httpd_register_uri_handler(server, &uri_wifi_post);
            if (!provision_only) {
                httpd_register_uri_handler(server, &uri_influx_get);
                httpd_register_uri_handler(server, &uri_influx_post);
            }
        }
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Reintentando conectar al AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            ESP_LOGE(TAG, "Fallo al conectar despues de %d intentos. Reiniciando en modo AP.", WIFI_MAXIMUM_RETRY);
            trigger_factory_reset(); 
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0; 
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } 
}

static void wifi_init_sta(const app_config_t* config)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = { .sta = { .threshold.authmode = WIFI_AUTH_WPA2_PSK } };
    strcpy((char*)wifi_config.sta.ssid, config->wifi_ssid);
    strcpy((char*)wifi_config.sta.password, config->wifi_pass);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Esperar a que la conexión se establezca
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
}

static void wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config = {
        .ap = { .ssid = "ESP32-Sensor-Config", .password = "password", .max_connection = 1, .authmode = WIFI_AUTH_WPA_WPA2_PSK },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Punto de Acceso 'ESP32-Sensor-Config' iniciado. Password: 'password'. Conectate y ve a 192.168.4.1");
}

// --- Función Pública Principal (Refactorizada) ---
esp_err_t provision_start(app_config_t *config) {
    prov_config = config;

    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // --- NUEVA LÓGICA DE RESETEO ---
    // 1. Comprobar si existe la bandera de reseteo al arrancar
    nvs_handle_t nvs_handle;
    nvs_open("nvs", NVS_READONLY, &nvs_handle);
    uint8_t reset_flag = 0;
    nvs_get_u8(nvs_handle, "reset_flag", &reset_flag);
    nvs_close(nvs_handle);

    if (reset_flag == 1) {
        ESP_LOGW(TAG, "¡Bandera de reseteo detectada! Ejecutando limpieza de fabrica...");
        // Borramos el namespace "storage" donde están las credenciales
        nvs_open("storage", NVS_READWRITE, &nvs_handle);
        nvs_erase_all(nvs_handle);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        
        // Borramos la propia bandera para el siguiente arranque
        nvs_open("nvs", NVS_READWRITE, &nvs_handle);
        nvs_set_u8(nvs_handle, "reset_flag", 0);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        
        ESP_LOGW(TAG, "Limpieza completada. Reiniciando de nuevo en modo limpio...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    
    // Si no hay bandera de reseteo, el programa continúa normalmente
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    if (load_config_from_nvs(config) == ESP_OK && strlen(config->wifi_ssid) > 0)
    {
        ESP_LOGI(TAG, "Configuracion Wi-Fi encontrada. Conectando como Cliente (Station)...");
        wifi_init_sta(config);
        
                // Esperamos aquí hasta que la conexión sea exitosa o falle definitivamente
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

        // Comprobamos el resultado
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Conexión exitosa.");
            start_webserver(false); // Iniciar servidor con AMBAS páginas
            initialise_mdns();      // Iniciar mDNS
            return ESP_OK; 

        } else {
            ESP_LOGE(TAG, "Fallo al conectar con las credenciales guardadas.");
            // Las credenciales son incorrectas, disparamos un reseteo
            trigger_factory_reset(); 
            return ESP_FAIL; // El dispositivo se reiniciará
        } 
    }else {
        ESP_LOGI(TAG, "No se encontró Wi-Fi. Iniciando como Punto de Acceso (AP)...");
        wifi_init_softap();
        start_webserver(true);

        while(1) {
            vTaskDelay(portMAX_DELAY);
        }
    }
    

}