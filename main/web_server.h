/**
 * web_server.h - SERVIDOR WEB Y API REST
 *
 * Sirve el dashboard embebido (HTML/CSS/JS) y 19 endpoints
 * REST para controlar todos los periféricos del sistema.
 *
 * Las variables de estado compartido se acceden mediante
 * getters/setters para desacoplar el servidor web del
 * resto del sistema (especialmente la tarea de control
 * en main.c).
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t hora;
    uint8_t minuto;
    uint8_t apertura;   // 0-100%
    bool    activo;
} curtain_schedule_t;

#define MAX_CURTAIN_SCHEDULES 8

void web_server_start(void);
void web_server_stop(void);

float web_get_temperatura_actual(void);
void  web_set_temperatura_actual(float temp);

float web_get_temp_deseada(void);
float web_get_temp_maxima(void);
void  web_set_temp_deseada(float t);
void  web_set_temp_maxima(float t);

bool  web_get_fan_auto_mode(void);
void  web_set_fan_auto_mode(bool auto_mode);

uint8_t web_get_fan_speed_manual(void);
void    web_set_fan_speed_manual(uint8_t speed);

bool  web_get_curtain_auto_mode(void);
void  web_set_curtain_auto_mode(bool auto_mode);

uint8_t web_get_curtain_position(void);
void    web_set_curtain_position(uint8_t pos);

void web_get_rgb_color(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *brillo);
void web_set_rgb_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brillo);

void web_get_schedules(curtain_schedule_t *schedules, int *count);
void web_set_schedules(curtain_schedule_t *schedules, int count);
int  web_get_schedule_count(void);

bool web_hora_sincronizada(void);

#endif
