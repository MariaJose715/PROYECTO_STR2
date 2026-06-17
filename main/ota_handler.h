/**
 * ota_handler.h - ACTUALIZACIÓN OTA
 *
 * Descarga un nuevo firmware .bin desde una URL HTTP/HTTPS
 * y lo escribe en la partición de respaldo.
 *
 * Soporta rollback:
 *   - Si el nuevo firmware falla, el bootloader revierte
 *     automáticamente a la versión anterior
 *   - La nueva imagen debe confirmarse llamando a
 *     esp_ota_mark_app_valid_cancel_rollback()
 */

#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <stdbool.h>

void ota_init(void);
bool ota_actualizar_desde_url(const char *url);
const char* ota_get_cur_ota_version(void);

#endif
