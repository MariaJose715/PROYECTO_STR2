#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <stdbool.h>
#include <stdint.h>
#include "system_context.h"

#define AP_DEFAULT_SSID     "maria_esp"
#define AP_DEFAULT_PASS     "12345678"
#define AP_MAX_CONNECTIONS  4
#define WIFI_SSID_MAX_LEN   32
#define WIFI_PASS_MAX_LEN   64

void wifi_init(system_context_t *ctx);
void wifi_guardar_credenciales_sta(const char *ssid, const char *password);
bool wifi_cargar_credenciales_sta(char *ssid, char *password);
void wifi_conectar_sta(system_context_t *ctx, const char *ssid, const char *password);
bool wifi_esta_conectado(system_context_t *ctx);
char* wifi_obtener_ip_sta(system_context_t *ctx);
void wifi_guardar_credenciales_ap(const char *ssid, const char *password);
bool wifi_cargar_credenciales_ap(char *ssid, char *password);
void wifi_reiniciar_ap(const char *ssid, const char *password);

#endif
