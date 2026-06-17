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

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MAXIMUM_RETRY       5

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    system_context_t *ctx = (system_context_t *)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        uart_send_msg("[WiFi] Conectando a la red guardada...");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (ctx->s_retry_num < MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            ctx->s_retry_num++;
            uart_send_msg("[WiFi] Reintento %d/%d...", ctx->s_retry_num, MAXIMUM_RETRY);
        }
        else
        {
            ctx->wifi_conectado = false;
            xEventGroupSetBits(ctx->wifi_event_group, WIFI_FAIL_BIT);
            uart_send_msg("[WiFi] Conexión fallida después de %d intentos", MAXIMUM_RETRY);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, ctx->ip_sta, sizeof(ctx->ip_sta));
        ctx->s_retry_num = 0;
        ctx->wifi_conectado = true;
        xEventGroupSetBits(ctx->wifi_event_group, WIFI_CONNECTED_BIT);
        uart_send_msg("[WiFi] Conectado! IP: %s", ctx->ip_sta);
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

void wifi_init(system_context_t *ctx)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ctx->wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                        ESP_EVENT_ANY_ID, &wifi_event_handler, ctx, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                        IP_EVENT_STA_GOT_IP, &wifi_event_handler, ctx, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    char sta_ssid[WIFI_SSID_MAX_LEN + 1] = "";
    char sta_pass[WIFI_PASS_MAX_LEN + 1] = "";
    bool has_sta_creds = wifi_cargar_credenciales_sta(sta_ssid, sta_pass);

    wifi_config_t wifi_sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    if (has_sta_creds && strlen(sta_ssid) > 0)
    {
        strncpy((char *)wifi_sta_config.sta.ssid, sta_ssid, WIFI_SSID_MAX_LEN);
        strncpy((char *)wifi_sta_config.sta.password, sta_pass, WIFI_PASS_MAX_LEN);
        uart_send_msg("[WiFi] Credenciales Station: %s", sta_ssid);
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));

    char ap_ssid[WIFI_SSID_MAX_LEN + 1] = "";
    char ap_pass[WIFI_PASS_MAX_LEN + 1] = "";
    bool has_ap_creds = wifi_cargar_credenciales_ap(ap_ssid, ap_pass);

    if (!has_ap_creds || strlen(ap_ssid) == 0)
    {
        strncpy(ap_ssid, AP_DEFAULT_SSID, WIFI_SSID_MAX_LEN);
        strncpy(ap_pass, AP_DEFAULT_PASS, WIFI_PASS_MAX_LEN);
    }

    wifi_config_t wifi_ap_config = {
        .ap = {
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_ap_config.ap.ssid, ap_ssid, WIFI_SSID_MAX_LEN);
    strncpy((char *)wifi_ap_config.ap.password, ap_pass, WIFI_PASS_MAX_LEN);
    wifi_ap_config.ap.ssid_len = strlen(ap_ssid);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(ctx->wifi_event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT)
        uart_send_msg("[WiFi] Station conectada. IP: %s", ctx->ip_sta);
    else if (bits & WIFI_FAIL_BIT)
        uart_send_msg("[WiFi] Station falló. AP activo: %s", ap_ssid);
    else
        uart_send_msg("[WiFi] Station timeout. AP activo: %s", ap_ssid);

    uart_send_msg("[WiFi] AP: %s | IP del AP: 192.168.4.1", ap_ssid);
}

void wifi_guardar_credenciales_sta(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    if (nvs_open("wifi_sta", NVS_READWRITE, &nvs_handle) != ESP_OK) return;

    nvs_set_str(nvs_handle, "ssid", ssid);
    nvs_set_str(nvs_handle, "password", password);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    uart_send_msg("[WiFi] Credenciales Station guardadas");
}

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

void wifi_conectar_sta(system_context_t *ctx, const char *ssid, const char *password)
{
    wifi_guardar_credenciales_sta(ssid, password);

    wifi_config_t wifi_sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_sta_config.sta.ssid, ssid, WIFI_SSID_MAX_LEN);
    strncpy((char *)wifi_sta_config.sta.password, password, WIFI_PASS_MAX_LEN);

    ctx->s_retry_num = 0;
    esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
    esp_wifi_connect();
    uart_send_msg("[WiFi] Conectando a Station: %s", ssid);
}

bool wifi_esta_conectado(system_context_t *ctx) { return ctx->wifi_conectado; }
char* wifi_obtener_ip_sta(system_context_t *ctx) { return ctx->ip_sta; }

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

void wifi_reiniciar_ap(const char *ssid, const char *password)
{
    wifi_guardar_credenciales_ap(ssid, password);

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
