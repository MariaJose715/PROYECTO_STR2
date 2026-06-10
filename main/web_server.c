#include "web_server.h"
#include "uart_handler.h"
#include "pwm_handler.h"
#include "adc_handler.h"
#include "led_handler.h"
#include "wifi_handler.h"
#include "ota_handler.h"
#include "dashboard_content.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "cJSON.h"

// ============================================================
// VARIABLES DE ESTADO COMPARTIDO
// ============================================================

static float temperatura_actual = 25.0f;      // Última lectura de temperatura
static float temp_deseada = 25.0f;             // Temperatura deseada por el usuario
static float temp_maxima = 35.0f;              // Temperatura máxima permitida
static bool  fan_auto_mode = true;             // true = automático, false = manual
static uint8_t fan_speed_manual = 50;           // Velocidad manual del ventilador (0-100%)
static bool  curtain_auto_mode = true;          // true = programado, false = manual
static uint8_t curtain_position = 50;           // Posición manual de cortina (0-100%)
static uint8_t rgb_r = 255, rgb_g = 255, rgb_b = 255;  // Color RGB
static uint8_t rgb_brillo = 50;                 // Brillo (0-100%)

static curtain_schedule_t schedules[MAX_CURTAIN_SCHEDULES];  // Horarios de cortina
static int schedule_count = 0;                               // Cantidad de horarios activos

static httpd_handle_t server = NULL;  // Manejador del servidor HTTP

// ============================================================
// FUNCIONES DE ACCESO A VARIABLES COMPARTIDAS
// ============================================================

float web_get_temperatura_actual(void) { return temperatura_actual; }
void  web_set_temperatura_actual(float temp) { temperatura_actual = temp; }
float web_get_temp_deseada(void) { return temp_deseada; }
float web_get_temp_maxima(void) { return temp_maxima; }
void  web_set_temp_deseada(float t) { temp_deseada = t; }
void  web_set_temp_maxima(float t) { temp_maxima = t; }
bool  web_get_fan_auto_mode(void) { return fan_auto_mode; }
void  web_set_fan_auto_mode(bool mode) { fan_auto_mode = mode; }
uint8_t web_get_fan_speed_manual(void) { return fan_speed_manual; }
void    web_set_fan_speed_manual(uint8_t s) { fan_speed_manual = s; }
bool  web_get_curtain_auto_mode(void) { return curtain_auto_mode; }
void  web_set_curtain_auto_mode(bool mode) { curtain_auto_mode = mode; }
uint8_t web_get_curtain_position(void) { return curtain_position; }
void    web_set_curtain_position(uint8_t p) { curtain_position = p; }
void web_get_rgb_color(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *brillo)
    { *r = rgb_r; *g = rgb_g; *b = rgb_b; *brillo = rgb_brillo; }
void web_set_rgb_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brillo)
    { rgb_r = r; rgb_g = g; rgb_b = b; rgb_brillo = brillo; }
void web_get_schedules(curtain_schedule_t *s, int *count)
    { memcpy(s, schedules, sizeof(schedules)); *count = schedule_count; }
int  web_get_schedule_count(void) { return schedule_count; }

void web_set_schedules(curtain_schedule_t *s, int count)
{
    if (count > MAX_CURTAIN_SCHEDULES) count = MAX_CURTAIN_SCHEDULES;
    memcpy(schedules, s, count * sizeof(curtain_schedule_t));
    schedule_count = count;
}

// ============================================================
// MANEJADORES DE LAS RUTAS HTTP
// ============================================================

/**
 * leer_cuerpo_post()
 * Lee el cuerpo de una solicitud POST de forma segura,
 * respetando content_length. (Patrón del ejemplo http_server)
 *
 * req:     manejador de la solicitud
 * buffer:  donde almacenar el body
 * buf_size: tamaño máximo del buffer
 *
 * Retorna: número de bytes leídos, o -1 en error.
 */
static int leer_cuerpo_post(httpd_req_t *req, char *buffer, size_t buf_size)
{
    int total_len = 0;
    int ret;

    memset(buffer, 0, buf_size);
    size_t remaining = req->content_len;

    if (remaining >= buf_size) remaining = buf_size - 1;

    while (remaining > 0)
    {
        ret = httpd_req_recv(req, buffer + total_len, remaining);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return -1;  // Error real
        }
        total_len += ret;
        remaining -= ret;
    }
    buffer[total_len] = '\0';
    return total_len;
}

/**
 * Ruta: GET /
 * Sirve la página web principal embebida.
 */
static esp_err_t ruta_raiz(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_sendstr(req, index_html);
    return ESP_OK;
}

static esp_err_t ruta_estilo(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_sendstr(req, style_css);
    return ESP_OK;
}

static esp_err_t ruta_script(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_sendstr(req, app_js);
    return ESP_OK;
}

/**
 * Ruta: GET /api/temp
 * Devuelve la temperatura actual, configuración del ventilador y estado de alarma.
 */
static esp_err_t ruta_api_temp(httpd_req_t *req)
{
    char buffer[512];
    float temp_act = temperatura_actual;

    time_t now;
    struct tm ti;
    time(&now);
    localtime_r(&now, &ti);
    char hora_str[32];
    strftime(hora_str, sizeof(hora_str), "%H:%M:%S", &ti);

    snprintf(buffer, sizeof(buffer),
        "{\"temperatura\":%.1f,\"temp_deseada\":%.1f,\"temp_maxima\":%.1f,"
        "\"fan_speed\":%d,\"fan_auto_mode\":%s,"
        "\"curtain_pos\":%d,\"alarma\":%s,"
        "\"wifi_ip\":\"%s\",\"wifi_conectado\":%s,"
        "\"hora\":\"%s\",\"hora_sincronizada\":%s}",
        temp_act, temp_deseada, temp_maxima,
        pwm_fan_get_speed(),
        fan_auto_mode ? "true" : "false",
        curtain_position,
        (temp_act > temp_maxima) ? "true" : "false",
        wifi_obtener_ip_sta(),
        wifi_esta_conectado() ? "true" : "false",
        hora_str,
        web_hora_sincronizada() ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

/**
 * Ruta: POST /api/fan/mode
 * Cambia el modo del ventilador (automático/manual).
 * Body: {"auto_mode": true/false}
 */
static esp_err_t ruta_fan_mode(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/fan/mode");
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *auto_mode = cJSON_GetObjectItem(json, "auto_mode");
        if (auto_mode)
        {
            web_set_fan_auto_mode(auto_mode->valueint);
            uart_send_msg("[WEB] Fan modo: %s", auto_mode->valueint ? "Automático" : "Manual");
        }
        cJSON_Delete(json);
    }
    else
    {
        uart_send_msg("[DBG] JSON parse error: %s", content);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/fan/config
 * Guarda la temperatura deseada y máxima.
 * Body: {"temp_deseada": 25.0, "temp_maxima": 35.0}
 */
static esp_err_t ruta_fan_config(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/fan/config");
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *td = cJSON_GetObjectItem(json, "temp_deseada");
        cJSON *tm = cJSON_GetObjectItem(json, "temp_maxima");
        if (td) web_set_temp_deseada((float)td->valuedouble);
        if (tm) web_set_temp_maxima((float)tm->valuedouble);
        uart_send_msg("[WEB] Temp config: Deseada=%.1f, Max=%.1f", temp_deseada, temp_maxima);
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/fan/speed
 * Establece la velocidad manual del ventilador.
 * Body: {"speed": 75}
 */
static esp_err_t ruta_fan_speed(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/fan/speed");
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *sp = cJSON_GetObjectItem(json, "speed");
        if (sp)
        {
            uint8_t speed = (uint8_t)sp->valueint;
            if (speed > 100) speed = 100;
            web_set_fan_speed_manual(speed);
            if (!fan_auto_mode) pwm_fan_set_speed(speed);
            uart_send_msg("[WEB] Fan velocidad manual: %d%%", speed);
        }
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/curtain/mode
 * Cambia el modo de cortina (automático/manual).
 */
static esp_err_t ruta_curtain_mode(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/curtain/mode");
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *auto_mode = cJSON_GetObjectItem(json, "auto_mode");
        if (auto_mode)
        {
            web_set_curtain_auto_mode(auto_mode->valueint);
            uart_send_msg("[WEB] Cortina modo: %s", auto_mode->valueint ? "Programado" : "Manual");
        }
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/curtain/position
 * Establece la posición manual de la cortina.
 */
static esp_err_t ruta_curtain_position(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/curtain/position");
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *pos = cJSON_GetObjectItem(json, "position");
        if (pos)
        {
            uint8_t p = (uint8_t)pos->valueint;
            if (p > 100) p = 100;
            web_set_curtain_position(p);
            if (!curtain_auto_mode) pwm_servo_set_position(p);
            uart_send_msg("[WEB] Cortina posición: %d%%", p);
        }
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/curtain/schedule
 * Guarda los horarios programados de cortina.
 * Body: {"schedules": [{"hora":8,"minuto":0,"apertura":50,"activo":true}, ...]}
 */
static esp_err_t ruta_curtain_schedule_post(httpd_req_t *req)
{
    char content[2048];
    leer_cuerpo_post(req, content, sizeof(content));

    cJSON *json = cJSON_Parse(content);
    if (json)
    {
        cJSON *scheds = cJSON_GetObjectItem(json, "schedules");
        if (scheds && cJSON_IsArray(scheds))
        {
            int count = cJSON_GetArraySize(scheds);
            if (count > MAX_CURTAIN_SCHEDULES) count = MAX_CURTAIN_SCHEDULES;

            for (int i = 0; i < count; i++)
            {
                cJSON *item = cJSON_GetArrayItem(scheds, i);
                schedules[i].hora     = (uint8_t)cJSON_GetObjectItem(item, "hora")->valueint;
                schedules[i].minuto   = (uint8_t)cJSON_GetObjectItem(item, "minuto")->valueint;
                schedules[i].apertura = (uint8_t)cJSON_GetObjectItem(item, "apertura")->valueint;
                cJSON *act = cJSON_GetObjectItem(item, "activo");
                schedules[i].activo   = act ? act->valueint : true;
            }
            schedule_count = count;
            uart_send_msg("[WEB] %d horarios de cortina guardados", count);
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: GET /api/curtain/schedule
 * Devuelve los horarios guardados de cortina.
 */
static esp_err_t ruta_curtain_schedule_get(httpd_req_t *req)
{
    char buffer[2048];
    int offset = snprintf(buffer, sizeof(buffer), "{\"schedules\":[");

    for (int i = 0; i < schedule_count; i++)
    {
        int remaining = sizeof(buffer) - offset;
        if (remaining < 64) break;
        offset += snprintf(buffer + offset, remaining,
            "%s{\"hora\":%d,\"minuto\":%d,\"apertura\":%d,\"activo\":%s}",
            (i > 0) ? "," : "",
            schedules[i].hora, schedules[i].minuto, schedules[i].apertura,
            schedules[i].activo ? "true" : "false");
    }

    snprintf(buffer + offset, sizeof(buffer) - offset, "]}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

/**
 * Ruta: POST /api/rgb
 * Establece el color y brillo del LED RGB.
 * Body: {"r":255,"g":100,"b":50,"brillo":80}
 */
static esp_err_t ruta_rgb(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/rgb");
    char content[256];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        uint8_t r = rgb_r, g = rgb_g, b = rgb_b, br = rgb_brillo;

        cJSON *item;
        if ((item = cJSON_GetObjectItem(json, "r")))      r = (uint8_t)item->valueint;
        if ((item = cJSON_GetObjectItem(json, "g")))      g = (uint8_t)item->valueint;
        if ((item = cJSON_GetObjectItem(json, "b")))      b = (uint8_t)item->valueint;
        if ((item = cJSON_GetObjectItem(json, "brillo"))) br = (uint8_t)item->valueint;

        web_set_rgb_color(r, g, b, br);

        rgb_color_t color = { .r = r, .g = g, .b = b, .brightness = br };
        led_rgb_set(&color);

        uart_send_msg("[WEB] RGB: R=%d G=%d B=%d Brillo=%d%%", r, g, b, br);
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/wifi/sta
 * Conecta el ESP32 a una red WiFi (Station).
 * Body: {"ssid":"MiRed","password":"clave123"}
 */
static esp_err_t ruta_wifi_sta(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/wifi/sta");
    char content[512];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
        cJSON *pass = cJSON_GetObjectItem(json, "password");

        if (ssid && ssid->valuestring)
        {
            wifi_conectar_sta(
                ssid->valuestring,
                pass && pass->valuestring ? pass->valuestring : ""
            );
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/wifi/ap
 * Cambia el SSID y contraseña del AP.
 * Body: {"ssid":"NuevoAP","password":"clave1234"}
 */
static esp_err_t ruta_wifi_ap(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/wifi/ap");
    char content[512];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
        cJSON *pass = cJSON_GetObjectItem(json, "password");

        if (ssid && ssid->valuestring)
        {
            wifi_reiniciar_ap(
                ssid->valuestring,
                pass && pass->valuestring ? pass->valuestring : "12345678"
            );
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/ota
 * Inicia una actualización OTA desde una URL.
 * Body: {"url":"http://ejemplo.com/firmware.bin"}
 */
static esp_err_t ruta_ota(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/ota");
    char content[1024];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *url = cJSON_GetObjectItem(json, "url");
        if (url && url->valuestring)
        {
            ota_actualizar_desde_url(url->valuestring);
            uart_send_msg("[WEB] OTA iniciada desde: %s", url->valuestring);
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"msg\":\"Actualización iniciada\"}");
    return ESP_OK;
}

/**
 * Ruta: GET /api/ota/version
 * Devuelve la versión actual de la partición OTA.
 */
static esp_err_t ruta_ota_version(httpd_req_t *req)
{
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "{\"version\":\"%s\"}", ota_get_cur_ota_version());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

/**
 * Ruta: GET /api/ping
 * Endpoint de diagnóstico para verificar que el servidor responde.
 */
static esp_err_t ruta_ping(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"pong\":true,\"server\":\"STR2026\"}");
    return ESP_OK;
}

static esp_err_t ruta_time_get(httpd_req_t *req)
{
    time_t now;
    struct tm ti;
    time(&now);
    localtime_r(&now, &ti);

    httpd_resp_set_type(req, "application/json");
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"hora\":%d,\"min\":%d,\"seg\":%d,\"anio\":%d,\"mes\":%d,\"dia\":%d}",
        ti.tm_hour, ti.tm_min, ti.tm_sec,
        ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t ruta_time_set(httpd_req_t *req)
{
    char content[128];
    int len = httpd_req_recv(req, content, sizeof(content) - 1);
    if (len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }
    content[len] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    int h = 8, m = 0, s = 0;
    cJSON *h_item = cJSON_GetObjectItem(json, "hora");
    cJSON *m_item = cJSON_GetObjectItem(json, "min");
    cJSON *s_item = cJSON_GetObjectItem(json, "seg");
    if (h_item) h = h_item->valueint;
    if (m_item) m = m_item->valueint;
    if (s_item) s = s_item->valueint;
    cJSON_Delete(json);

    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid time");
        return ESP_FAIL;
    }

    struct tm tm_set;
    time_t now;
    time(&now);
    localtime_r(&now, &tm_set);
    tm_set.tm_hour = h;
    tm_set.tm_min  = m;
    tm_set.tm_sec  = s;

    time_t t = mktime(&tm_set);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    uart_send_msg("[WEB] Hora cambiada a %02d:%02d:%02d", h, m, s);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// ============================================================
// REGISTRO DE RUTAS
// ============================================================

/**
 * web_server_start()
 * Inicializa el servidor HTTP y registra todas las rutas.
 */
void web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 26;
    config.lru_purge_enable = true;     // Liberar conexiones viejas si es necesario

    esp_err_t reg_ok = ESP_OK;

    if (httpd_start(&server, &config) == ESP_OK)
    {
#define REG_URI(u, m, h) do { \
    httpd_uri_t uri = { .uri = u, .method = m, .handler = h, .user_ctx = NULL }; \
    esp_err_t e = httpd_register_uri_handler(server, &uri); \
    if (e != ESP_OK) { uart_send_msg("[WEB] Error registrando %s: %s", u, esp_err_to_name(e)); reg_ok = e; } \
} while(0)

        REG_URI("/", HTTP_GET, ruta_raiz);
        REG_URI("/style.css", HTTP_GET, ruta_estilo);
        REG_URI("/app.js", HTTP_GET, ruta_script);
        REG_URI("/api/ping", HTTP_GET, ruta_ping);
        REG_URI("/api/temp", HTTP_GET, ruta_api_temp);
        REG_URI("/api/fan/mode", HTTP_POST, ruta_fan_mode);
        REG_URI("/api/fan/config", HTTP_POST, ruta_fan_config);
        REG_URI("/api/fan/speed", HTTP_POST, ruta_fan_speed);
        REG_URI("/api/curtain/mode", HTTP_POST, ruta_curtain_mode);
        REG_URI("/api/curtain/position", HTTP_POST, ruta_curtain_position);
        REG_URI("/api/curtain/schedule", HTTP_POST, ruta_curtain_schedule_post);
        REG_URI("/api/curtain/schedule", HTTP_GET, ruta_curtain_schedule_get);
        REG_URI("/api/rgb", HTTP_POST, ruta_rgb);
        REG_URI("/api/wifi/sta", HTTP_POST, ruta_wifi_sta);
        REG_URI("/api/wifi/ap", HTTP_POST, ruta_wifi_ap);
        REG_URI("/api/ota", HTTP_POST, ruta_ota);
        REG_URI("/api/ota/version", HTTP_GET, ruta_ota_version);
        REG_URI("/api/time", HTTP_GET, ruta_time_get);
        REG_URI("/api/time/set", HTTP_POST, ruta_time_set);

        if (reg_ok == ESP_OK)
            uart_send_msg("[WEB] Servidor HTTP iniciado en puerto 80");
        else
            uart_send_msg("[WEB] Servidor iniciado con errores en algunas rutas");
    }
    else
    {
        uart_send_msg("[WEB] Error al iniciar servidor HTTP");
    }
}

/**
 * web_server_stop()
 * Detiene el servidor HTTP.
 */
void web_server_stop(void)
{
    if (server)
    {
        httpd_stop(server);
        server = NULL;
        uart_send_msg("[WEB] Servidor HTTP detenido");
    }
}
