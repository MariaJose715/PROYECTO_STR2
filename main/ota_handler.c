/**
 * ================================================================
 * ota_handler.c - ACTUALIZACIÓN OTA (Over-The-Air)
 * ================================================================
 *
 * Este archivo implementa la funcionalidad de actualización de
 * firmware inalámbrica (OTA) usando HTTP/HTTPS.
 *
 * ¿Qué es OTA?
 *   OTA permite actualizar el firmware del ESP32 sin necesidad
 *   de conexión física por USB. El ESP32 descarga un nuevo
 *   firmware (.bin) desde un servidor web y lo escribe en la
 *   partición de respaldo (OTA_0 u OTA_1), luego se reinicia
 *   para ejecutar la nueva versión.
 *
 * ¿Cómo funciona?
 *   1. El usuario envía una URL del firmware por el dashboard web
 *   2. Se crea una tarea FreeRTOS "ota_update_task"
 *   3. La tarea descarga el firmware usando esp_https_ota
 *   4. Si la descarga es exitosa, se escribe en la partición
 *      de respaldo y se reinicia el ESP32
 *   5. Si falla, se muestra el error y se elimina la tarea
 *
 * Rollback:
 *   Si el bootloader tiene rollback habilitado, la nueva imagen
 *   se marca como "pending_verify" y debe confirmarse como válida
 *   en el siguiente arranque. Si no se confirma (por ejemplo,
 *   porque el nuevo firmware falla), el bootloader revierte
 *   automáticamente a la versión anterior.
 *
 * Referencia: ejemplo oficial advanced_https_ota de ESP-IDF
 * ================================================================
 */

#include "ota_handler.h"
#include "uart_handler.h"
#include <string.h>
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ================================================================
// tarea_ota_actualizar()
// ================================================================
// Tarea FreeRTOS que ejecuta la actualización OTA.
//
// Parámetros:
//   param: puntero a una cadena con la URL del firmware (debe
//          ser liberada con free() al finalizar)
//
// Flujo:
//   1. Validar URL
//   2. Si hay rollback pendiente, cancelarlo (marcar imagen
//      previa como válida)
//   3. Configurar cliente HTTP con timeout de 10s
//   4. Ejecutar esp_https_ota()
//   5. Si éxito: reiniciar después de 2s
//   6. Si error: mostrar mensaje y eliminar tarea
static void tarea_ota_actualizar(void *param)
{
    char *url = (char *)param;

    // Validar URL
    if (url == NULL || strlen(url) == 0)
    {
        uart_send_msg("[OTA] Error: URL vacía");
        free(url);
        vTaskDelete(NULL);
        return;
    }

    uart_send_msg("[OTA] Iniciando actualización desde: %s", url);

    // ---- 1. Verificar partición actual y manejar rollback pendiente ----
    //
    // Si el bootloader tiene APP_ROLLBACK_ENABLE, después de una
    // actualización OTA la nueva imagen está en estado "pending_verify".
    // Si el ESP32 se reinicia y la nueva imagen funciona, debe llamar
    // a esp_ota_mark_app_valid_cancel_rollback() para confirmarla.
    // Si no se confirma, en el siguiente reinicio el bootloader
    // revierte automáticamente a la imagen anterior.
#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            // Hay una actualización pendiente de verificar
            esp_ota_mark_app_valid_cancel_rollback();
            uart_send_msg("[OTA] Rollback cancelado, imagen marcada como válida");
        }
    }
#endif

    // ---- 2. Configurar el cliente HTTP ----
    esp_http_client_config_t http_config = {
        .url              = url,
        .timeout_ms       = 10000,           // Timeout de 10 segundos
        .keep_alive_enable = false,           // No mantener conexión abierta
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
        vTaskDelay(pdMS_TO_TICKS(2000));  // Esperar 2s para que el mensaje se vea
        esp_restart();                     // Reiniciar el ESP32
    }
    else
    {
        uart_send_msg("[OTA] Error: %s", esp_err_to_name(ret));
    }

    // Liberar memoria y eliminar tarea
    free(url);
    vTaskDelete(NULL);
}

// ================================================================
// ota_init()
// ================================================================
// Inicializa el sistema OTA: obtiene la partición en ejecución
// y la marca como partición de arranque.
//
// Esto es necesario para que el bootloader sepa qué partición
// debe ejecutar al reiniciar.
void ota_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_set_boot_partition(running);
    uart_send_msg("[OTA] Partición activa: %s", running->label);
}

// ================================================================
// ota_actualizar_desde_url()
// ================================================================
// Inicia una actualización OTA desde una URL en una tarea separada.
//
// ¿Por qué en una tarea separada?
//   - La descarga OTA puede tomar varios segundos o minutos
//   - Si se ejecutara en el manejador HTTP, bloquearía el servidor
//   - Una tarea separada permite que el servidor siga respondiendo
//     mientras se descarga el firmware
//
// Parámetros:
//   url: URL del firmware .bin a descargar
//
// Retorna:
//   true si la tarea se creó correctamente
//   false si no se pudo crear la tarea (falta de memoria)
bool ota_actualizar_desde_url(const char *url)
{
    // Hacer una copia de la URL (la tarea necesita su propia copia)
    char *url_copy = strdup(url);
    if (url_copy == NULL) return false;

    // Crear tarea FreeRTOS con stack de 8KB
    BaseType_t result = xTaskCreate(
        tarea_ota_actualizar,
        "ota_update_task",     // Nombre de la tarea (para depuración)
        8192,                  // Stack size: 8KB (suficiente para HTTPS)
        url_copy,              // Parámetro: puntero a la URL
        5,                     // Prioridad
        NULL                   // Handle (no necesario)
    );

    if (result != pdPASS)
    {
        free(url_copy);
        return false;
    }

    return true;
}

// ================================================================
// ota_get_cur_ota_version()
// ================================================================
// Retorna el nombre (etiqueta) de la partición actual en ejecución.
//
// Esto se muestra en el dashboard para que el usuario sepa
// qué versión de firmware está ejecutando.
const char* ota_get_cur_ota_version(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    return running->label;
}
