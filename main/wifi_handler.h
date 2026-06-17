/**
 * wifi_handler.h - CONEXIÓN WiFi (AP + Station)
 *
 * Modo APSTA: el ESP32 funciona como punto de acceso y cliente
 * simultáneamente.
 *
 * AP por defecto: "maria_esp" / "12345678"
 * Station: credenciales guardadas en NVS (persistentes)
 *
 * Las credenciales se almacenan en NVS bajo los namespaces
 * "wifi_sta" y "wifi_ap" para persistencia entre reinicios.
 */

#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

#define AP_DEFAULT_SSID     "maria_esp"
#define AP_DEFAULT_PASS     "12345678"
#define AP_MAX_CONNECTIONS  4

#define WIFI_SSID_MAX_LEN   32
#define WIFI_PASS_MAX_LEN   64

void wifi_init(void);

void wifi_guardar_credenciales_sta(const char *ssid, const char *password);
bool wifi_cargar_credenciales_sta(char *ssid, char *password);
void wifi_conectar_sta(const char *ssid, const char *password);
bool wifi_esta_conectado(void);
char* wifi_obtener_ip_sta(void);

void wifi_guardar_credenciales_ap(const char *ssid, const char *password);
bool wifi_cargar_credenciales_ap(char *ssid, char *password);
void wifi_reiniciar_ap(const char *ssid, const char *password);

#endif
