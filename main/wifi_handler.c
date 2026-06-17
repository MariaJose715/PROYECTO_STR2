/**
 * ================================================================
 * wifi_handler.c - CONEXIÓN WiFi (AP + Station)
 * ================================================================
 *
 * Este archivo maneja toda la conectividad WiFi del ESP32-C6.
 *
 * El sistema funciona en modo APSTA (Access Point + Station):
 *
 *   AP (Access Point):
 *     - Crea una red WiFi a la que el usuario se conecta
 *     - SSID por defecto: "maria_esp" (configurable)
 *     - Contraseña: "12345678" (configurable)
 *     - IP del AP: 192.168.4.1
 *     - Proporciona acceso al dashboard web
 *
 *   Station:
 *     - Se conecta a una red WiFi externa para:
 *       * Sincronizar hora vía NTP (Internet)
 *       * Permitir acceso al dashboard desde la red local
 *     - Las credenciales se guardan en NVS (memoria no volátil)
 *     - Reintenta hasta 5 veces si falla la conexión
 *
 * Almacenamiento de credenciales:
 *   - Se usa NVS (Non-Volatile Storage) para guardar SSID y password
 *   - Las credenciales persisten entre reinicios del ESP32
 *   - Namespaces: "wifi_sta" y "wifi_ap"
 *
 * Patrón de eventos:
 *   - Usa event groups de FreeRTOS para sincronización
 *   - WIFI_CONNECTED_BIT: se activa cuando Station se conecta
 *   - WIFI_FAIL_BIT: se activa cuando Station falla (tras 5 intentos)
 *   - xEventGroupWaitBits(): espera bloqueante hasta conexión o fallo
 *
 * Referencia: ejemplos oficiales station y softAP de ESP-IDF
 * ================================================================
 */

#include "wifi_handler.h"
#include "uart_handler.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

// Variable global para la dirección IP del modo Station
static char ip_sta[16] = "0.0.0.0";
static bool wifi_conectado = false;
static int s_retry_num = 0;

// Event group para sincronizar conexión Station (patrón ejemplo oficial)
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0   // Bit 0: conexión exitosa
#define WIFI_FAIL_BIT       BIT1   // Bit 1: conexión fallida

#define MAXIMUM_RETRY       5      // Número máximo de reintentos de conexión

// ================================================================
// wifi_event_handler()
// ================================================================
// Manejador de eventos WiFi (patrón de los ejemplos station y
// softAP de ESP-IDF).
//
// Eventos manejados:
//   WIFI_EVENT_STA_START         -> Iniciar conexión Station
//   WIFI_EVENT_STA_DISCONNECTED  -> Reintentar o fallar
//   IP_EVENT_STA_GOT_IP          -> Conexión exitosa (con IP)
//   WIFI_EVENT_AP_STACONNECTED    -> Cliente se conectó al AP
//   WIFI_EVENT_AP_STADISCONNECTED -> Cliente se desconectó del AP
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // El WiFi Station se inició, conectar a la red guardada
        esp_wifi_connect();
        uart_send_msg("[WiFi] Conectando a la red guardada...");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // Se perdió la conexión o falló el intento
        if (s_retry_num < MAXIMUM_RETRY)
        {
            // Reintentar
            esp_wifi_connect();
            s_retry_num++;
            uart_send_msg("[WiFi] Reintento %d/%d...", s_retry_num, MAXIMUM_RETRY);
        }
        else
        {
            // Falló definitivamente
            wifi_conectado = false;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            uart_send_msg("[WiFi] Conexión fallida después de %d intentos", MAXIMUM_RETRY);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // ¡Conectado! Obtuvimos una dirección IP
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_sta, sizeof(ip_sta));
        s_retry_num = 0;
        wifi_conectado = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        uart_send_msg("[WiFi] Conectado! IP: %s", ip_sta);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        uart_send_msg("[WiFi] Dispositivo conectado al AP, AID=%d", event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        uart_send_msg("[WiFi] Dispositivo desconectado del AP");
    }
}

// ================================================================
// INICIALIZACIÓN PRINCIPAL
// ================================================================

// ================================================================
// wifi_init()
// ================================================================
// Inicializa todo el sistema WiFi en modo APSTA.
//
// Flujo:
//   1. Inicializar NVS (para guardar credenciales)
//   2. Crear event group para sincronización
//   3. Inicializar TCP/IP y event loop
//   4. Crear interfaces de red (Station y AP)
//   5. Inicializar WiFi
//   6. Registrar manejadores de eventos
//   7. Configurar modo APSTA
//   8. Configurar Station con credenciales guardadas
//   9. Configurar AP con credenciales guardadas (o default)
//   10. Arrancar WiFi
//   11. Esperar conexión Station (con timeout de 10s)
void wifi_init(void)
{
    // Inicializar NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Si NVS está corrupto o actualizado, borrar y reinicializar
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Crear event group para sincronización
    s_wifi_event_group = xEventGroupCreate();

    // Inicializar TCP/IP y event loop (necesario para WiFi)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Crear interfaces de red (Station y AP)
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    // Inicializar WiFi con configuración por defecto
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registrar manejadores de eventos (patrón ejemplo oficial)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                        ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                        IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // Configurar modo AP + Station simultáneamente
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    // ---- Configurar Station con credenciales guardadas ----
    char sta_ssid[WIFI_SSID_MAX_LEN + 1] = "";
    char sta_pass[WIFI_PASS_MAX_LEN + 1] = "";
    bool has_sta_creds = wifi_cargar_credenciales_sta(sta_ssid, sta_pass);

    wifi_config_t wifi_sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,  // Usar WPA2
        },
    };
    if (has_sta_creds && strlen(sta_ssid) > 0)
    {
        // Copiar credenciales guardadas a la configuración
        strncpy((char *)wifi_sta_config.sta.ssid, sta_ssid, WIFI_SSID_MAX_LEN);
        strncpy((char *)wifi_sta_config.sta.password, sta_pass, WIFI_PASS_MAX_LEN);
        uart_send_msg("[WiFi] Credenciales Station: %s", sta_ssid);
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));

    // ---- Configurar AP con credenciales guardadas o por defecto ----
    char ap_ssid[WIFI_SSID_MAX_LEN + 1] = "";
    char ap_pass[WIFI_PASS_MAX_LEN + 1] = "";
    bool has_ap_creds = wifi_cargar_credenciales_ap(ap_ssid, ap_pass);

    if (!has_ap_creds || strlen(ap_ssid) == 0)
    {
        // No hay credenciales guardadas, usar valores por defecto
        strncpy(ap_ssid, AP_DEFAULT_SSID, WIFI_SSID_MAX_LEN);
        strncpy(ap_pass, AP_DEFAULT_PASS, WIFI_PASS_MAX_LEN);
    }

    wifi_config_t wifi_ap_config = {
        .ap = {
            .max_connection = AP_MAX_CONNECTIONS,  // Máx. 4 clientes
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,    // Seguridad WPA/WPA2
        },
    };
    strncpy((char *)wifi_ap_config.ap.ssid, ap_ssid, WIFI_SSID_MAX_LEN);
    strncpy((char *)wifi_ap_config.ap.password, ap_pass, WIFI_PASS_MAX_LEN);
    wifi_ap_config.ap.ssid_len = strlen(ap_ssid);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    // Arrancar WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Esperar conexión Station (no bloqueante, con timeout de 10s)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT)
        uart_send_msg("[WiFi] Station conectada. IP: %s", ip_sta);
    else if (bits & WIFI_FAIL_BIT)
        uart_send_msg("[WiFi] Station falló. AP activo: %s", ap_ssid);
    else
        uart_send_msg("[WiFi] Station timeout. AP activo: %s", ap_ssid);

    uart_send_msg("[WiFi] AP: %s | IP del AP: 192.168.4.1", ap_ssid);
}

// ================================================================
// FUNCIONES STATION
// ================================================================

// ================================================================
// wifi_guardar_credenciales_sta()
// ================================================================
// Guarda SSID y password de la red Station en NVS para que
// persistan entre reinicios.
void wifi_guardar_credenciales_sta(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    if (nvs_open("wifi_sta", NVS_READWRITE, &nvs_handle) != ESP_OK) return;

    nvs_set_str(nvs_handle, "ssid", ssid);
    nvs_set_str(nvs_handle, "password", password);
    nvs_commit(nvs_handle);    // Asegurar escritura en flash
    nvs_close(nvs_handle);
    uart_send_msg("[WiFi] Credenciales Station guardadas");
}

// ================================================================
// wifi_cargar_credenciales_sta()
// ================================================================
// Carga SSID y password de Station desde NVS.
// Retorna true si se encontraron credenciales guardadas.
bool wifi_cargar_credenciales_sta(char *ssid, char *password)
{
    nvs_handle_t nvs_handle;
    size_t len_ssid = WIFI_SSID_MAX_LEN;
    size_t len_pass = WIFI_PASS_MAX_LEN;

    if (nvs_open("wifi_sta", NVS_READONLY, &nvs_handle) != ESP_OK) return false;

    esp_err_t err = nvs_get_str(nvs_handle, "ssid", ssid, &len_ssid);
    if (err != ESP_OK) { nvs_close(nvs_handle); return false; }

    err = nvs_get_str(nvs_handle, "password", password, &len_pass);
    nvs_close(nvs_handle);
    return (err == ESP_OK);
}

// ================================================================
// wifi_conectar_sta()
// ================================================================
// Conecta el ESP32 a una red WiFi externa (Station).
// Guarda las credenciales y se conecta inmediatamente.
//
// Parámetros:
//   ssid: nombre de la red WiFi
//   password: contraseña de la red
void wifi_conectar_sta(const char *ssid, const char *password)
{
    // Guardar credenciales
    wifi_guardar_credenciales_sta(ssid, password);

    // Configurar y conectar
    wifi_config_t wifi_sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_sta_config.sta.ssid, ssid, WIFI_SSID_MAX_LEN);
    strncpy((char *)wifi_sta_config.sta.password, password, WIFI_PASS_MAX_LEN);

    s_retry_num = 0;  // Reiniciar contador de reintentos
    esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
    esp_wifi_connect();
    uart_send_msg("[WiFi] Conectando a Station: %s", ssid);
}

bool wifi_esta_conectado(void) { return wifi_conectado; }
char* wifi_obtener_ip_sta(void) { return ip_sta; }

// ================================================================
// FUNCIONES AP
// ================================================================

void wifi_guardar_credenciales_ap(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    if (nvs_open("wifi_ap", NVS_READWRITE, &nvs_handle) != ESP_OK) return;

    nvs_set_str(nvs_handle, "ssid", ssid);
    nvs_set_str(nvs_handle, "password", password);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    uart_send_msg("[WiFi] Credenciales AP guardadas");
}

bool wifi_cargar_credenciales_ap(char *ssid, char *password)
{
    nvs_handle_t nvs_handle;
    size_t len_ssid = WIFI_SSID_MAX_LEN;
    size_t len_pass = WIFI_PASS_MAX_LEN;

    if (nvs_open("wifi_ap", NVS_READONLY, &nvs_handle) != ESP_OK) return false;

    esp_err_t err = nvs_get_str(nvs_handle, "ssid", ssid, &len_ssid);
    if (err != ESP_OK) { nvs_close(nvs_handle); return false; }

    err = nvs_get_str(nvs_handle, "password", password, &len_pass);
    nvs_close(nvs_handle);
    return (err == ESP_OK);
}

// ================================================================
// wifi_reiniciar_ap()
// ================================================================
// Cambia el SSID y contraseña del AP en caliente (sin reiniciar).
//
// Parámetros:
//   ssid: nuevo nombre de la red AP
//   password: nueva contraseña (mín. 8 caracteres)
void wifi_reiniciar_ap(const char *ssid, const char *password)
{
    // Guardar nuevas credenciales
    wifi_guardar_credenciales_ap(ssid, password);

    // Reconfigurar el AP
    wifi_config_t wifi_ap_config = {
        .ap = {
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_ap_config.ap.ssid, ssid, WIFI_SSID_MAX_LEN);
    strncpy((char *)wifi_ap_config.ap.password, password, WIFI_PASS_MAX_LEN);
    wifi_ap_config.ap.ssid_len = strlen(ssid);

    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
    esp_wifi_start();
    uart_send_msg("[WiFi] AP reiniciado: %s", ssid);
}
