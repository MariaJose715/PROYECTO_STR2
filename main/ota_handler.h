#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <stdbool.h>

// ============================================================
// MANEJADOR OTA (Over-The-Air)
// Permite actualizar el firmware del ESP32 de forma inalámbrica
// ============================================================

// Inicializa el servicio OTA
// Esto registra un manejador que permite recibir nuevas imágenes
void ota_init(void);

// Inicia una actualización OTA desde una URL HTTP/HTTPS
// url: dirección completa del firmware .bin
// Devuelve true si la actualización se inició correctamente
bool ota_actualizar_desde_url(const char *url);

// Obtiene información de la partición actual
const char* ota_get_cur_ota_version(void);

#endif // OTA_HANDLER_H
