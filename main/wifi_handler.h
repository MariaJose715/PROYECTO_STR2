#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

// ============================================================
// MANEJADOR WiFi
// Gestiona la conexión a redes WiFi (Station) y el punto de acceso (SoftAP)
// También administra el almacenamiento de credenciales en NVS
// ============================================================

// Configuración por defecto del AP (Access Point)
#define AP_DEFAULT_SSID     "maria"
#define AP_DEFAULT_PASS     "12345678"
#define AP_MAX_CONNECTIONS  4

// Máximo largo de cadenas para SSID y contraseña
#define WIFI_SSID_MAX_LEN   32
#define WIFI_PASS_MAX_LEN   64

// ============================================================
// PROTOTIPOS - Configuración de red
// ============================================================

// Inicializa el subsistema WiFi y arranca en modo AP + Station
void wifi_init(void);

// ============================================================
// PROTOTIPOS - Modo Station (Cliente WiFi)
// ============================================================

// Guarda las credenciales de la red WiFi en NVS
void wifi_guardar_credenciales_sta(const char *ssid, const char *password);

// Carga las credenciales guardadas desde NVS
bool wifi_cargar_credenciales_sta(char *ssid, char *password);

// Conecta a una red WiFi como Station con las credenciales proporcionadas
void wifi_conectar_sta(const char *ssid, const char *password);

// Obtiene el estado actual de la conexión Station
bool wifi_esta_conectado(void);

// Obtiene la dirección IP asignada al modo Station
char* wifi_obtener_ip_sta(void);

// ============================================================
// PROTOTIPOS - Modo AP (Punto de Acceso)
// ============================================================

// Guarda las credenciales del AP (SSID y password) en NVS
void wifi_guardar_credenciales_ap(const char *ssid, const char *password);

// Carga las credenciales del AP desde NVS
bool wifi_cargar_credenciales_ap(char *ssid, char *password);

// Reinicia el AP con nuevas credenciales
void wifi_reiniciar_ap(const char *ssid, const char *password);

#endif // WIFI_HANDLER_H
