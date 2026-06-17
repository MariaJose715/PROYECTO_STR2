#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "system_context.h"

void web_server_start(system_context_t *ctx);
void web_server_stop(system_context_t *ctx);

float web_get_temperatura_actual(system_context_t *ctx);
void  web_set_temperatura_actual(system_context_t *ctx, float temp);
float web_get_temp_deseada(system_context_t *ctx);
float web_get_temp_maxima(system_context_t *ctx);
void  web_set_temp_deseada(system_context_t *ctx, float t);
void  web_set_temp_maxima(system_context_t *ctx, float t);
bool  web_get_fan_auto_mode(system_context_t *ctx);
void  web_set_fan_auto_mode(system_context_t *ctx, bool auto_mode);
uint8_t web_get_fan_speed_manual(system_context_t *ctx);
void    web_set_fan_speed_manual(system_context_t *ctx, uint8_t speed);
bool  web_get_curtain_auto_mode(system_context_t *ctx);
void  web_set_curtain_auto_mode(system_context_t *ctx, bool auto_mode);
uint8_t web_get_curtain_position(system_context_t *ctx);
void    web_set_curtain_position(system_context_t *ctx, uint8_t pos);
void web_get_rgb_color(system_context_t *ctx, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *brillo);
void web_set_rgb_color(system_context_t *ctx, uint8_t r, uint8_t g, uint8_t b, uint8_t brillo);
void web_get_schedules(system_context_t *ctx, curtain_schedule_t *schedules, int *count);
void web_set_schedules(system_context_t *ctx, curtain_schedule_t *schedules, int count);
int  web_get_schedule_count(system_context_t *ctx);
bool web_hora_sincronizada(system_context_t *ctx);

#endif
