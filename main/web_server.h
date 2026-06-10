#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// SERVIDOR WEB HTTP
// Proporciona la interfaz de control web para todo el sistema
// Incluye la página HTML embebida con CSS y JavaScript
// ============================================================

// Estructura para registrar un evento de cortina (horario programado)
typedef struct {
    uint8_t hora;           // Hora del evento (0-23)
    uint8_t minuto;         // Minuto del evento (0-59)
    uint8_t apertura;       // Apertura de cortina en % (0-100)
    bool    activo;         // Si el registro está habilitado
} curtain_schedule_t;

// Número máximo de registros programables para cortina
#define MAX_CURTAIN_SCHEDULES 8

// ============================================================
// PROTOTIPOS
// ============================================================

// Inicializa y arranca el servidor web
void web_server_start(void);

// Detiene el servidor web
void web_server_stop(void);

// Obtiene la temperatura actual (lectura compartida)
float web_get_temperatura_actual(void);

// Establece la temperatura actual (desde la tarea de monitoreo)
void web_set_temperatura_actual(float temp);

// Obtiene/configura los parámetros de temperatura para el control del ventilador
float web_get_temp_deseada(void);
float web_get_temp_maxima(void);
void  web_set_temp_deseada(float t);
void  web_set_temp_maxima(float t);

// Obtiene/configura el modo del ventilador (true = automático, false = manual)
bool  web_get_fan_auto_mode(void);
void  web_set_fan_auto_mode(bool auto_mode);

// Obtiene/configura la velocidad manual del ventilador (0-100%)
uint8_t web_get_fan_speed_manual(void);
void    web_set_fan_speed_manual(uint8_t speed);

// Obtiene/configura el modo de cortina (true = automático, false = manual)
bool  web_get_curtain_auto_mode(void);
void  web_set_curtain_auto_mode(bool auto_mode);

// Obtiene/configura la apertura manual de cortina (0-100%)
uint8_t web_get_curtain_position(void);
void    web_set_curtain_position(uint8_t pos);

// Obtiene/configura el color RGB actual
void web_get_rgb_color(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *brillo);
void web_set_rgb_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brillo);

// Obtiene/configura los horarios de cortina
void web_get_schedules(curtain_schedule_t *schedules, int *count);
void web_set_schedules(curtain_schedule_t *schedules, int count);
int  web_get_schedule_count(void);

#endif // WEB_SERVER_H
