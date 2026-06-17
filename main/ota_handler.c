#include "ota_handler.h"
#include "uart_handler.h"
#include <string.h>
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * tarea_ota_actualizar()
 * Tarea FreeRTOS para la actualización OTA.
 * Incluye manejo de rollback (patrón del ejemplo advanced_https_ota).
 */
static void tarea_ota_actualizar(void *param)
{
    char *url = (char *)param;

    if (url == NULL || strlen(url) == 0)
    {
        uart_send_msg("[OTA] Error: URL vacía");
        free(url);
        vTaskDelete(NULL);
        return;
    }

    uart_send_msg("[OTA] Iniciando actualización desde: %s", url);

    // ---- 1. Verificar partición actual y manejar rollback pendiente ----
#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            // Si hay una actualización pendiente de verificar, la validamos
            esp_ota_mark_app_valid_cancel_rollback();
            uart_send_msg("[OTA] Rollback cancelado, imagen marcada como válida");
        }
    }
#endif

    // ---- 2. Configurar el cliente HTTP ----
    esp_http_client_config_t http_config = {
        .url              = url,
        .timeout_ms       = 10000,
        .keep_alive_enable = false,
    };

    // ---- 3. Configurar OTA ----
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    // ---- 4. Ejecutar la actualización ----
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK)
    {
        uart_send_msg("[OTA] Actualización exitosa. Reiniciando en 2s...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }
    else
    {
        uart_send_msg("[OTA] Error: %s", esp_err_to_name(ret));
    }

    free(url);
    vTaskDelete(NULL);
}

/**
 * ota_init()
 * Inicializa y marca la partición actual como válida.
 */
void ota_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_set_boot_partition(running);
    uart_send_msg("[OTA] Partición activa: %s", running->label);
}

/**
 * ota_actualizar_desde_url()
 * Inicia OTA desde una URL en una tarea separada.
 */
bool ota_actualizar_desde_url(const char *url)
{
    char *url_copy = strdup(url);
    if (url_copy == NULL) return false;

    BaseType_t result = xTaskCreate(
        tarea_ota_actualizar,
        "ota_update_task",
        8192,
        url_copy,
        5,
        NULL
    );

    if (result != pdPASS)
    {
        free(url_copy);
        return false;
    }

    return true;
}

const char* ota_get_cur_ota_version(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    return running->label;
}
