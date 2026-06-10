#include "web_server.h"
#include "uart_handler.h"
#include "pwm_handler.h"
#include "adc_handler.h"
#include "led_handler.h"
#include "wifi_handler.h"
#include "ota_handler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "cJSON.h"

// ============================================================
// VARIABLES DE ESTADO COMPARTIDO
// ============================================================

static float temperatura_actual = 25.0f;      // Última lectura de temperatura
static float temp_deseada = 25.0f;             // Temperatura deseada por el usuario
static float temp_maxima = 35.0f;              // Temperatura máxima permitida
static bool  fan_auto_mode = true;             // true = automático, false = manual
static uint8_t fan_speed_manual = 50;           // Velocidad manual del ventilador (0-100%)
static bool  curtain_auto_mode = true;          // true = programado, false = manual
static uint8_t curtain_position = 50;           // Posición manual de cortina (0-100%)
static uint8_t rgb_r = 255, rgb_g = 255, rgb_b = 255;  // Color RGB
static uint8_t rgb_brillo = 50;                 // Brillo (0-100%)

static curtain_schedule_t schedules[MAX_CURTAIN_SCHEDULES];  // Horarios de cortina
static int schedule_count = 0;                               // Cantidad de horarios activos

static httpd_handle_t server = NULL;  // Manejador del servidor HTTP

// ============================================================
// FUNCIONES DE ACCESO A VARIABLES COMPARTIDAS
// ============================================================

float web_get_temperatura_actual(void) { return temperatura_actual; }
void  web_set_temperatura_actual(float temp) { temperatura_actual = temp; }
float web_get_temp_deseada(void) { return temp_deseada; }
float web_get_temp_maxima(void) { return temp_maxima; }
void  web_set_temp_deseada(float t) { temp_deseada = t; }
void  web_set_temp_maxima(float t) { temp_maxima = t; }
bool  web_get_fan_auto_mode(void) { return fan_auto_mode; }
void  web_set_fan_auto_mode(bool mode) { fan_auto_mode = mode; }
uint8_t web_get_fan_speed_manual(void) { return fan_speed_manual; }
void    web_set_fan_speed_manual(uint8_t s) { fan_speed_manual = s; }
bool  web_get_curtain_auto_mode(void) { return curtain_auto_mode; }
void  web_set_curtain_auto_mode(bool mode) { curtain_auto_mode = mode; }
uint8_t web_get_curtain_position(void) { return curtain_position; }
void    web_set_curtain_position(uint8_t p) { curtain_position = p; }
void web_get_rgb_color(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *brillo)
    { *r = rgb_r; *g = rgb_g; *b = rgb_b; *brillo = rgb_brillo; }
void web_set_rgb_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brillo)
    { rgb_r = r; rgb_g = g; rgb_b = b; rgb_brillo = brillo; }
void web_get_schedules(curtain_schedule_t *s, int *count)
    { memcpy(s, schedules, sizeof(schedules)); *count = schedule_count; }
int  web_get_schedule_count(void) { return schedule_count; }

void web_set_schedules(curtain_schedule_t *s, int count)
{
    if (count > MAX_CURTAIN_SCHEDULES) count = MAX_CURTAIN_SCHEDULES;
    memcpy(schedules, s, count * sizeof(curtain_schedule_t));
    schedule_count = count;
}

// ============================================================
// PÁGINA WEB EMBEBIDA (HTML + CSS + JavaScript)
// ============================================================

static const char *WEB_PAGE =
"<!DOCTYPE html>"
"<html lang='es'>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>STR 2026 - Control Ambiental</title>"
"<style>"
"* { margin:0; padding:0; box-sizing:border-box; font-family:'Segoe UI',sans-serif; }"
"body { background:#1a1a2e; color:#eee; min-height:100vh; display:flex; flex-direction:column; }"
".header { background:#16213e; padding:20px; text-align:center; border-bottom:3px solid #0f3460; }"
".header h1 { color:#e94560; font-size:28px; }"
".header p { color:#889; font-size:14px; margin-top:5px; }"
".container { max-width:1200px; margin:0 auto; padding:20px; display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:20px; }"
".card { background:#16213e; border-radius:12px; padding:20px; box-shadow:0 4px 15px rgba(0,0,0,0.3); }"
".card h2 { color:#e94560; font-size:18px; margin-bottom:15px; border-bottom:1px solid #0f3460; padding-bottom:8px; display:flex; align-items:center; gap:8px; }"
".card h2 .icon { font-size:22px; }"
".temp-display { text-align:center; padding:10px; }"
".temp-value { font-size:48px; font-weight:bold; color:#fff; }"
".temp-label { color:#889; font-size:14px; margin-top:5px; }"
".temp-unit { font-size:24px; color:#889; }"
".temp-status { margin-top:10px; padding:8px 15px; border-radius:20px; display:inline-block; font-size:13px; }"
".temp-status.ok { background:#1b5e20; color:#81c784; }"
".temp-status.warn { background:#e65100; color:#ffcc80; }"
".temp-status.alarm { background:#b71c1c; color:#ef9a9a; animation:pulse 1s infinite; }"
"@keyframes pulse { 0%,100% { opacity:1; } 50% { opacity:0.5; } }"
".control-row { display:flex; align-items:center; gap:15px; margin:10px 0; flex-wrap:wrap; }"
".control-row label { min-width:120px; color:#aab; font-size:14px; }"
".control-row input[type='range'] { flex:1; min-width:150px; }"
".control-row input[type='number'] { width:80px; padding:6px; border-radius:6px; border:1px solid #0f3460; background:#1a1a2e; color:#fff; text-align:center; }"
".control-row input[type='color'] { width:60px; height:40px; border:none; border-radius:6px; cursor:pointer; background:transparent; }"
".control-row select { padding:8px; border-radius:6px; border:1px solid #0f3460; background:#1a1a2e; color:#fff; }"
".btn { padding:8px 20px; border:none; border-radius:8px; cursor:pointer; font-size:14px; font-weight:bold; transition:all 0.3s; }"
".btn-primary { background:#e94560; color:#fff; }"
".btn-primary:hover { background:#d63851; transform:translateY(-2px); }"
".btn-success { background:#2e7d32; color:#fff; }"
".btn-success:hover { background:#388e3c; }"
".btn-warning { background:#e65100; color:#fff; }"
".btn-warning:hover { background:#bf360c; }"
".btn-small { padding:5px 12px; font-size:12px; }"
".btn-block { width:100%; margin-top:10px; }"
".mode-toggle { display:flex; gap:0; margin:10px 0; }"
".mode-toggle button { flex:1; padding:8px; border:1px solid #0f3460; background:#1a1a2e; color:#889; cursor:pointer; transition:all 0.3s; }"
".mode-toggle button:first-child { border-radius:8px 0 0 8px; }"
".mode-toggle button:last-child { border-radius:0 8px 8px 0; }"
".mode-toggle button.active { background:#e94560; color:#fff; border-color:#e94560; }"
".schedule-table { width:100%; border-collapse:collapse; margin:10px 0; font-size:13px; }"
".schedule-table th { background:#0f3460; padding:8px; text-align:left; color:#aab; }"
".schedule-table td { padding:8px; border-bottom:1px solid #0f3460; }"
".schedule-table td input { width:100%; padding:4px; border-radius:4px; border:1px solid #0f3460; background:#1a1a2e; color:#fff; }"
".rgb-preview { width:60px; height:60px; border-radius:50%; border:3px solid #0f3460; margin:10px auto; transition:all 0.3s; }"
".color-picker-row { display:flex; align-items:center; gap:15px; margin:8px 0; }"
".color-picker-row label { min-width:40px; color:#aab; }"
".wifi-status { padding:10px; background:#0f3460; border-radius:8px; margin:10px 0; font-size:13px; }"
".wifi-status .label { color:#889; }"
".wifi-status .value { color:#fff; font-weight:bold; }"
".ota-row { display:flex; gap:10px; }"
".ota-row input { flex:1; padding:8px; border-radius:6px; border:1px solid #0f3460; background:#1a1a2e; color:#fff; }"
".toast { position:fixed; bottom:20px; right:20px; padding:12px 24px; border-radius:8px; font-size:14px; z-index:999; opacity:0; transition:opacity 0.5s; }"
".toast.success { background:#2e7d32; color:#fff; }"
".toast.error { background:#c62828; color:#fff; }"
".toast.show { opacity:1; }"
".status-msg { font-size:12px; color:#889; margin-top:5px; }"
"@media (max-width:600px) { .container { padding:10px; } .card { padding:15px; } }"
"</style>"
"</head>"
"<body>"
"<div class='header'><h1>STR 2026</h1><p>Sistema de Control Ambiental Automatizado</p></div>"
"<div class='container'>"

// Sección 1: Temperatura y Ventilador
"<div class='card' id='temp-section'>"
"<h2><span class='icon'>🌡️</span> Temperatura & Ventilación</h2>"
"<div class='temp-display'>"
"<span class='temp-value' id='temp-value'>--</span><span class='temp-unit'>°C</span>"
"<div class='temp-label'>Temperatura Ambiente</div>"
"<div class='temp-status' id='temp-status'>Monitoreando...</div>"
"</div>"
"<div class='mode-toggle' id='fan-mode-toggle'>"
"<button id='fan-auto' class='active' onclick='setFanMode(true)'>Automático</button>"
"<button id='fan-manual' onclick='setFanMode(false)'>Manual</button>"
"</div>"
"<div id='fan-auto-controls'>"
"<div class='control-row'><label>Temperatura Deseada</label><input type='number' id='temp-deseada' value='25' min='0' max='50' step='0.5'></div>"
"<div class='control-row'><label>Temperatura Máxima</label><input type='number' id='temp-maxima' value='35' min='0' max='60' step='0.5'></div>"
"<button class='btn btn-primary btn-block' onclick='saveTempConfig()'>Guardar Configuración</button>"
"</div>"
"<div id='fan-manual-controls' style='display:none'>"
"<div class='control-row'><label>Velocidad Ventilador</label><input type='range' id='fan-speed' min='0' max='100' value='50' oninput='updateFanSpeedLabel()'><span id='fan-speed-label'>50%</span></div>"
"<button class='btn btn-primary btn-block' onclick='setFanSpeed()'>Aplicar Velocidad</button>"
"</div>"
"<div class='status-msg'>Ventilador: <span id='fan-status'>0%</span></div>"
"</div>"

// Sección 2: Cortina
"<div class='card' id='curtain-section'>"
"<h2><span class='icon'>🪟</span> Control de Cortinas</h2>"
"<div class='mode-toggle' id='curtain-mode-toggle'>"
"<button id='curtain-auto' class='active' onclick='setCurtainMode(true)'>Programado</button>"
"<button id='curtain-manual' onclick='setCurtainMode(false)'>Manual</button>"
"</div>"
"<div id='curtain-manual-controls'>"
"<div class='control-row'><label>Apertura</label><input type='range' id='curtain-pos' min='0' max='100' value='50' oninput='updateCurtainLabel()'><span id='curtain-pos-label'>50%</span></div>"
"<button class='btn btn-primary btn-block' onclick='setCurtainPosition()'>Aplicar Posición</button>"
"</div>"
"<div id='curtain-auto-controls'>"
"<p style='color:#aab;font-size:13px;margin-bottom:10px;'>Horarios Programados (máx. 8)</p>"
"<table class='schedule-table'><thead><tr><th>Hora</th><th>Min</th><th>Apertura %</th><th>Activo</th><th></th></tr></thead><tbody id='schedule-rows'></tbody></table>"
"<button class='btn btn-success btn-small' onclick='addScheduleRow()'>+ Agregar Horario</button>"
"<button class='btn btn-primary btn-block' onclick='saveSchedules()'>Guardar Horarios</button>"
"</div>"
"<div class='status-msg'>Cortina: <span id='curtain-status'>50%</span></div>"
"</div>"

// Sección 3: LED RGB
"<div class='card' id='rgb-section'>"
"<h2><span class='icon'>💡</span> Iluminación LED RGB</h2>"
"<div class='rgb-preview' id='rgb-preview'></div>"
"<div class='color-picker-row'><label>Color</label><input type='color' id='rgb-color' value='#ffffff' onchange='previewRGB()'></div>"
"<div class='control-row'><label>Brillo</label><input type='range' id='rgb-brillo' min='0' max='100' value='50' oninput='updateBrilloLabel();previewRGB()'><span id='rgb-brillo-label'>50%</span></div>"
"<button class='btn btn-primary btn-block' onclick='applyRGB()'>Aplicar Color</button>"
"</div>"

// Sección 4: WiFi
"<div class='card' id='wifi-section'>"
"<h2><span class='icon'>📶</span> Configuración de Red</h2>"
"<div class='wifi-status'><div><span class='label'>IP Station:</span> <span class='value' id='wifi-ip'>0.0.0.0</span></div><div><span class='label'>Conectado:</span> <span class='value' id='wifi-connected'>No</span></div></div>"
"<h3 style='color:#aab;font-size:14px;margin:10px 0;'>Red WiFi (Station)</h3>"
"<div class='control-row'><label>SSID</label><input type='text' id='sta-ssid' placeholder='Nombre de la red'></div>"
"<div class='control-row'><label>Contraseña</label><input type='password' id='sta-pass' placeholder='Contraseña'></div>"
"<button class='btn btn-success btn-block' onclick='connectSTA()'>Conectar a WiFi</button>"
"<h3 style='color:#aab;font-size:14px;margin:15px 0 10px;'>Punto de Acceso (AP)</h3>"
"<div class='control-row'><label>SSID</label><input type='text' id='ap-ssid' placeholder='STR2026_Control'></div>"
"<div class='control-row'><label>Contraseña</label><input type='password' id='ap-pass' placeholder='Mín. 8 caracteres'></div>"
"<button class='btn btn-warning btn-block' onclick='configAP()'>Cambiar AP</button>"
"</div>"

// Sección 5: OTA
"<div class='card' id='ota-section'>"
"<h2><span class='icon'>🔄</span> Actualización OTA</h2>"
"<p style='color:#aab;font-size:13px;margin-bottom:10px;'>Ingrese la URL del firmware .bin para actualizar el dispositivo de forma inalámbrica.</p>"
"<div class='ota-row'><input type='url' id='ota-url' placeholder='http://ejemplo.com/firmware.bin'><button class='btn btn-warning' onclick='startOTA()'>Actualizar</button></div>"
"<div class='status-msg'>Versión: <span id='ota-version'>---</span></div>"
"</div>"

"</div>"  // fin container

"<div class='toast' id='toast'></div>"

"<script>"
"// ============================"
"// FUNCIONES DE LA INTERFAZ WEB"
"// ============================"

"// Función auxiliar para mostrar notificaciones"
"function showToast(msg, type) {"
"  var t=document.getElementById('toast');"
"  t.textContent=msg;"
"  t.className='toast '+type+' show';"
"  setTimeout(function(){t.className='toast '+type;},3000);"
"}"

"// Función genérica para enviar POST"
"function apiPost(url, data, callback) {"
"  var xhr=new XMLHttpRequest();"
"  xhr.open('POST', url, true);"
"  xhr.setRequestHeader('Content-Type','application/json');"
"  xhr.onreadystatechange=function(){"
"    if(xhr.readyState==4){"
"      if(xhr.status==200){showToast('Comando ejecutado','success');if(callback)callback(JSON.parse(xhr.responseText));}"
"      else showToast('Error: '+xhr.status,'error');"
"    }"
"  };"
"  xhr.send(JSON.stringify(data));"
"}"

"// Modo del ventilador"
"function setFanMode(auto){"
"  document.getElementById('fan-auto').className=auto?'active':'';"
"  document.getElementById('fan-manual').className=auto?'':'active';"
"  document.getElementById('fan-auto-controls').style.display=auto?'block':'none';"
"  document.getElementById('fan-manual-controls').style.display=auto?'none':'block';"
"  apiPost('/api/fan/mode',{auto_mode:auto});"
"}"

"function updateFanSpeedLabel(){"
"  var v=document.getElementById('fan-speed').value;"
"  document.getElementById('fan-speed-label').textContent=v+'%';"
"}"

"function saveTempConfig(){"
"  var td=parseFloat(document.getElementById('temp-deseada').value);"
"  var tm=parseFloat(document.getElementById('temp-maxima').value);"
"  apiPost('/api/fan/config',{temp_deseada:td,temp_maxima:tm});"
"}"

"function setFanSpeed(){"
"  var v=parseInt(document.getElementById('fan-speed').value);"
"  apiPost('/api/fan/speed',{speed:v});"
"}"

"// Modo cortina"
"function setCurtainMode(auto){"
"  document.getElementById('curtain-auto').className=auto?'active':'';"
"  document.getElementById('curtain-manual').className=auto?'':'active';"
"  document.getElementById('curtain-auto-controls').style.display=auto?'block':'none';"
"  document.getElementById('curtain-manual-controls').style.display=auto?'none':'block';"
"  apiPost('/api/curtain/mode',{auto_mode:auto});"
"}"

"function updateCurtainLabel(){"
"  var v=document.getElementById('curtain-pos').value;"
"  document.getElementById('curtain-pos-label').textContent=v+'%';"
"}"

"function setCurtainPosition(){"
"  var v=parseInt(document.getElementById('curtain-pos').value);"
"  apiPost('/api/curtain/position',{position:v});"
"}"

"// Horarios"
"var schedIdx=0;"
"function addScheduleRow(existing){"
"  var t=document.getElementById('schedule-rows');"
"  var r=document.createElement('tr');"
"  var idx=schedIdx++;"
"  r.id='sched-'+idx;"
"  var h=existing?existing.hora:'8';"
"  var m=existing?existing.minuto:'0';"
"  var a=existing?existing.apertura:'50';"
"  var act=existing?existing.activo:true;"
"  r.innerHTML='<td><input type=\"number\" id=\"sh-'+idx+'\" value=\"'+h+'\" min=\"0\" max=\"23\"></td>'"
"    +'<td><input type=\"number\" id=\"sm-'+idx+'\" value=\"'+m+'\" min=\"0\" max=\"59\"></td>'"
"    +'<td><input type=\"number\" id=\"sa-'+idx+'\" value=\"'+a+'\" min=\"0\" max=\"100\"></td>'"
"    +'<td><input type=\"checkbox\" id=\"sact-'+idx+'\" '+(act?'checked':'')+'></td>'"
"    +'<td><button class=\"btn btn-small btn-warning\" onclick=\"this.parentElement.parentElement.remove()\">X</button></td>';"
"  t.appendChild(r);"
"}"

"function saveSchedules(){"
"  var rows=document.getElementById('schedule-rows').children;"
"  var scheds=[];"
"  for(var i=0;i<rows.length;i++){"
"    var id=rows[i].id.split('-')[1];"
"    scheds.push({"
"      hora:parseInt(document.getElementById('sh-'+id).value),"
"      minuto:parseInt(document.getElementById('sm-'+id).value),"
"      apertura:parseInt(document.getElementById('sa-'+id).value),"
"      activo:document.getElementById('sact-'+id).checked"
"    });"
"  }"
"  apiPost('/api/curtain/schedule',{schedules:scheds});"
"}"

"// RGB"
"function previewRGB(){"
"  var c=document.getElementById('rgb-color').value;"
"  var b=parseInt(document.getElementById('rgb-brillo').value)/100;"
"  var p=document.getElementById('rgb-preview');"
"  p.style.backgroundColor=c;"
"  p.style.opacity=Math.max(0.1,b);"
"}"

"function updateBrilloLabel(){"
"  var v=document.getElementById('rgb-brillo').value;"
"  document.getElementById('rgb-brillo-label').textContent=v+'%';"
"}"

"function applyRGB(){"
"  var c=document.getElementById('rgb-color').value;"
"  var b=parseInt(document.getElementById('rgb-brillo').value);"
"  var r=parseInt(c.substr(1,2),16);"
"  var g=parseInt(c.substr(3,2),16);"
"  var bl=parseInt(c.substr(5,2),16);"
"  apiPost('/api/rgb',{r:r,g:g,b:bl,brillo:b});"
"}"

"// WiFi"
"function connectSTA(){"
"  var s=document.getElementById('sta-ssid').value;"
"  var p=document.getElementById('sta-pass').value;"
"  if(!s){showToast('Ingrese un SSID','error');return;}"
"  apiPost('/api/wifi/sta',{ssid:s,password:p});"
"}"

"function configAP(){"
"  var s=document.getElementById('ap-ssid').value;"
"  var p=document.getElementById('ap-pass').value;"
"  if(!s){showToast('Ingrese un SSID para el AP','error');return;}"
"  if(p.length<8){showToast('La contraseña debe tener al menos 8 caracteres','error');return;}"
"  apiPost('/api/wifi/ap',{ssid:s,password:p});"
"}"

"// OTA"
"function startOTA(){"
"  var url=document.getElementById('ota-url').value;"
"  if(!url){showToast('Ingrese una URL','error');return;}"
"  if(!confirm('¿Está seguro de que desea actualizar el firmware?')) return;"
"  apiPost('/api/ota',{url:url});"
"}"

"// Actualización periódica de datos"
"function updateData(){"
"  var xhr=new XMLHttpRequest();"
"  xhr.open('GET','/api/temp',true);"
"  xhr.onreadystatechange=function(){"
"    if(xhr.readyState==4&&xhr.status==200){"
"      var d=JSON.parse(xhr.responseText);"
"      document.getElementById('temp-value').textContent=d.temperatura.toFixed(1);"
"      var statusEl=document.getElementById('temp-status');"
"      var alarmEl=document.getElementById('temp-status');"
"      if(d.alarma){"
"        statusEl.className='temp-status alarm';"
"        statusEl.textContent='⚠ ALARMA: Temperatura excede el máximo!';"
"      } else if(d.temperatura>d.temp_maxima-3){"
"        statusEl.className='temp-status warn';"
"        statusEl.textContent='⚠ Temperatura elevada';"
"      } else {"
"        statusEl.className='temp-status ok';"
"        statusEl.textContent='✅ Temperatura normal';"
"      }"
"      document.getElementById('fan-status').textContent=d.fan_speed+'%';"
"      document.getElementById('curtain-status').textContent=d.curtain_pos+'%';"
"      document.getElementById('wifi-ip').textContent=d.wifi_ip;"
"      document.getElementById('wifi-connected').textContent=d.wifi_conectado?'Sí':'No';"
"    }"
"  };"
"  xhr.send();"
"}"

"// Inicialización"
"window.onload=function(){"
"  // Cargar horarios existentes"
"  var xhr=new XMLHttpRequest();"
"  xhr.open('GET','/api/curtain/schedule',true);"
"  xhr.onreadystatechange=function(){"
"    if(xhr.readyState==4&&xhr.status==200){"
"      var d=JSON.parse(xhr.responseText);"
"      for(var i=0;i<d.schedules.length;i++){"
"        addScheduleRow(d.schedules[i]);"
"      }"
"    }"
"  };"
"  xhr.send();"
"  // Cargar OTA version"
"  var x2=new XMLHttpRequest();"
"  x2.open('GET','/api/ota/version',true);"
"  x2.onreadystatechange=function(){"
"    if(x2.readyState==4&&x2.status==200)"
"      document.getElementById('ota-version').textContent=JSON.parse(x2.responseText).version;"
"  };"
"  x2.send();"
"  // Actualizar cada 3 segundos"
"  updateData();"
"  setInterval(updateData,3000);"
"};"
"</script>"
"</body></html>";

// ============================================================
// MANEJADORES DE LAS RUTAS HTTP
// ============================================================

/**
 * leer_cuerpo_post()
 * Lee el cuerpo de una solicitud POST de forma segura,
 * respetando content_length. (Patrón del ejemplo http_server)
 *
 * req:     manejador de la solicitud
 * buffer:  donde almacenar el body
 * buf_size: tamaño máximo del buffer
 *
 * Retorna: número de bytes leídos, o -1 en error.
 */
static int leer_cuerpo_post(httpd_req_t *req, char *buffer, size_t buf_size)
{
    int total_len = 0;
    int ret;

    memset(buffer, 0, buf_size);
    size_t remaining = req->content_len;

    if (remaining >= buf_size) remaining = buf_size - 1;

    while (remaining > 0)
    {
        ret = httpd_req_recv(req, buffer + total_len, remaining);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return -1;  // Error real
        }
        total_len += ret;
        remaining -= ret;
    }
    buffer[total_len] = '\0';
    return total_len;
}

/**
 * Ruta: GET /
 * Sirve la página web principal embebida.
 */
static esp_err_t ruta_raiz(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, WEB_PAGE, strlen(WEB_PAGE));
    return ESP_OK;
}

/**
 * Ruta: GET /api/temp
 * Devuelve la temperatura actual, configuración del ventilador y estado de alarma.
 */
static esp_err_t ruta_api_temp(httpd_req_t *req)
{
    char buffer[512];
    snprintf(buffer, sizeof(buffer),
        "{\"temperatura\":%.1f,\"temp_deseada\":%.1f,\"temp_maxima\":%.1f,"
        "\"fan_speed\":%d,\"curtain_pos\":%d,"
        "\"alarma\":%s,\"wifi_ip\":\"%s\",\"wifi_conectado\":%s}",
        temperatura_actual, temp_deseada, temp_maxima,
        fan_auto_mode ? 0 : fan_speed_manual,
        curtain_position,
        (temperatura_actual > temp_maxima) ? "true" : "false",
        wifi_obtener_ip_sta(),
        wifi_esta_conectado() ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

/**
 * Ruta: POST /api/fan/mode
 * Cambia el modo del ventilador (automático/manual).
 * Body: {"auto_mode": true/false}
 */
static esp_err_t ruta_fan_mode(httpd_req_t *req)
{
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *auto_mode = cJSON_GetObjectItem(json, "auto_mode");
        if (auto_mode)
        {
            web_set_fan_auto_mode(auto_mode->valueint);
            uart_send_msg("[WEB] Fan modo: %s", auto_mode->valueint ? "Automático" : "Manual");
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/fan/config
 * Guarda la temperatura deseada y máxima.
 * Body: {"temp_deseada": 25.0, "temp_maxima": 35.0}
 */
static esp_err_t ruta_fan_config(httpd_req_t *req)
{
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *td = cJSON_GetObjectItem(json, "temp_deseada");
        cJSON *tm = cJSON_GetObjectItem(json, "temp_maxima");
        if (td) web_set_temp_deseada((float)td->valuedouble);
        if (tm) web_set_temp_maxima((float)tm->valuedouble);
        uart_send_msg("[WEB] Temp config: Deseada=%.1f, Max=%.1f", temp_deseada, temp_maxima);
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/fan/speed
 * Establece la velocidad manual del ventilador.
 * Body: {"speed": 75}
 */
static esp_err_t ruta_fan_speed(httpd_req_t *req)
{
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *sp = cJSON_GetObjectItem(json, "speed");
        if (sp)
        {
            uint8_t speed = (uint8_t)sp->valueint;
            if (speed > 100) speed = 100;
            web_set_fan_speed_manual(speed);
            if (!fan_auto_mode) pwm_fan_set_speed(speed);
            uart_send_msg("[WEB] Fan velocidad manual: %d%%", speed);
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/curtain/mode
 * Cambia el modo de cortina (automático/manual).
 */
static esp_err_t ruta_curtain_mode(httpd_req_t *req)
{
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *auto_mode = cJSON_GetObjectItem(json, "auto_mode");
        if (auto_mode)
        {
            web_set_curtain_auto_mode(auto_mode->valueint);
            uart_send_msg("[WEB] Cortina modo: %s", auto_mode->valueint ? "Programado" : "Manual");
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/curtain/position
 * Establece la posición manual de la cortina.
 */
static esp_err_t ruta_curtain_position(httpd_req_t *req)
{
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *pos = cJSON_GetObjectItem(json, "position");
        if (pos)
        {
            uint8_t p = (uint8_t)pos->valueint;
            if (p > 100) p = 100;
            web_set_curtain_position(p);
            if (!curtain_auto_mode) pwm_servo_set_position(p);
            uart_send_msg("[WEB] Cortina posición: %d%%", p);
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/curtain/schedule
 * Guarda los horarios programados de cortina.
 * Body: {"schedules": [{"hora":8,"minuto":0,"apertura":50,"activo":true}, ...]}
 */
static esp_err_t ruta_curtain_schedule_post(httpd_req_t *req)
{
    char content[2048];
    int len = leer_cuerpo_post(req, content, sizeof(content));

    cJSON *json = cJSON_Parse(content);
    if (json)
    {
        cJSON *scheds = cJSON_GetObjectItem(json, "schedules");
        if (scheds && cJSON_IsArray(scheds))
        {
            int count = cJSON_GetArraySize(scheds);
            if (count > MAX_CURTAIN_SCHEDULES) count = MAX_CURTAIN_SCHEDULES;

            for (int i = 0; i < count; i++)
            {
                cJSON *item = cJSON_GetArrayItem(scheds, i);
                schedules[i].hora     = (uint8_t)cJSON_GetObjectItem(item, "hora")->valueint;
                schedules[i].minuto   = (uint8_t)cJSON_GetObjectItem(item, "minuto")->valueint;
                schedules[i].apertura = (uint8_t)cJSON_GetObjectItem(item, "apertura")->valueint;
                cJSON *act = cJSON_GetObjectItem(item, "activo");
                schedules[i].activo   = act ? act->valueint : true;
            }
            schedule_count = count;
            uart_send_msg("[WEB] %d horarios de cortina guardados", count);
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: GET /api/curtain/schedule
 * Devuelve los horarios guardados de cortina.
 */
static esp_err_t ruta_curtain_schedule_get(httpd_req_t *req)
{
    char buffer[1024];
    int offset = snprintf(buffer, sizeof(buffer), "{\"schedules\":[");

    for (int i = 0; i < schedule_count; i++)
    {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
            "%s{\"hora\":%d,\"minuto\":%d,\"apertura\":%d,\"activo\":%s}",
            (i > 0) ? "," : "",
            schedules[i].hora, schedules[i].minuto, schedules[i].apertura,
            schedules[i].activo ? "true" : "false");
    }

    snprintf(buffer + offset, sizeof(buffer) - offset, "]}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

/**
 * Ruta: POST /api/rgb
 * Establece el color y brillo del LED RGB.
 * Body: {"r":255,"g":100,"b":50,"brillo":80}
 */
static esp_err_t ruta_rgb(httpd_req_t *req)
{
    char content[256];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        uint8_t r = rgb_r, g = rgb_g, b = rgb_b, br = rgb_brillo;

        cJSON *item;
        if ((item = cJSON_GetObjectItem(json, "r")))      r = (uint8_t)item->valueint;
        if ((item = cJSON_GetObjectItem(json, "g")))      g = (uint8_t)item->valueint;
        if ((item = cJSON_GetObjectItem(json, "b")))      b = (uint8_t)item->valueint;
        if ((item = cJSON_GetObjectItem(json, "brillo"))) br = (uint8_t)item->valueint;

        web_set_rgb_color(r, g, b, br);

        // Aplicar al LED RGB
        rgb_color_t color = { .r = r, .g = g, .b = b, .brightness = br };
        led_rgb_set(&color);

        uart_send_msg("[WEB] RGB: R=%d G=%d B=%d Brillo=%d%%", r, g, b, br);
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/wifi/sta
 * Conecta el ESP32 a una red WiFi (Station).
 * Body: {"ssid":"MiRed","password":"clave123"}
 */
static esp_err_t ruta_wifi_sta(httpd_req_t *req)
{
    char content[512];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
        cJSON *pass = cJSON_GetObjectItem(json, "password");

        if (ssid && ssid->valuestring)
        {
            wifi_conectar_sta(
                ssid->valuestring,
                pass && pass->valuestring ? pass->valuestring : ""
            );
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/wifi/ap
 * Cambia el SSID y contraseña del AP.
 * Body: {"ssid":"NuevoAP","password":"clave1234"}
 */
static esp_err_t ruta_wifi_ap(httpd_req_t *req)
{
    char content[512];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
        cJSON *pass = cJSON_GetObjectItem(json, "password");

        if (ssid && ssid->valuestring)
        {
            wifi_reiniciar_ap(
                ssid->valuestring,
                pass && pass->valuestring ? pass->valuestring : "12345678"
            );
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/**
 * Ruta: POST /api/ota
 * Inicia una actualización OTA desde una URL.
 * Body: {"url":"http://ejemplo.com/firmware.bin"}
 */
static esp_err_t ruta_ota(httpd_req_t *req)
{
    char content[1024];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *url = cJSON_GetObjectItem(json, "url");
        if (url && url->valuestring)
        {
            ota_actualizar_desde_url(url->valuestring);
            uart_send_msg("[WEB] OTA iniciada desde: %s", url->valuestring);
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"msg\":\"Actualización iniciada\"}");
    return ESP_OK;
}

/**
 * Ruta: GET /api/ota/version
 * Devuelve la versión actual de la partición OTA.
 */
static esp_err_t ruta_ota_version(httpd_req_t *req)
{
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "{\"version\":\"%s\"}", ota_get_cur_ota_version());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

// ============================================================
// REGISTRO DE RUTAS
// ============================================================

/**
 * web_server_start()
 * Inicializa el servidor HTTP y registra todas las rutas.
 */
void web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;       // Suficientes para todas nuestras rutas
    config.lru_purge_enable = true;     // Liberar conexiones viejas si es necesario

    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Registrar rutas
        httpd_uri_t uri_raiz = { .uri = "/", .method = HTTP_GET, .handler = ruta_raiz, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_raiz);

        httpd_uri_t uri_temp = { .uri = "/api/temp", .method = HTTP_GET, .handler = ruta_api_temp, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_temp);

        httpd_uri_t uri_fan_mode = { .uri = "/api/fan/mode", .method = HTTP_POST, .handler = ruta_fan_mode, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_fan_mode);

        httpd_uri_t uri_fan_config = { .uri = "/api/fan/config", .method = HTTP_POST, .handler = ruta_fan_config, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_fan_config);

        httpd_uri_t uri_fan_speed = { .uri = "/api/fan/speed", .method = HTTP_POST, .handler = ruta_fan_speed, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_fan_speed);

        httpd_uri_t uri_curtain_mode = { .uri = "/api/curtain/mode", .method = HTTP_POST, .handler = ruta_curtain_mode, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_curtain_mode);

        httpd_uri_t uri_curtain_pos = { .uri = "/api/curtain/position", .method = HTTP_POST, .handler = ruta_curtain_position, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_curtain_pos);

        httpd_uri_t uri_curtain_sched_post = { .uri = "/api/curtain/schedule", .method = HTTP_POST, .handler = ruta_curtain_schedule_post, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_curtain_sched_post);

        httpd_uri_t uri_curtain_sched_get = { .uri = "/api/curtain/schedule", .method = HTTP_GET, .handler = ruta_curtain_schedule_get, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_curtain_sched_get);

        httpd_uri_t uri_rgb = { .uri = "/api/rgb", .method = HTTP_POST, .handler = ruta_rgb, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_rgb);

        httpd_uri_t uri_wifi_sta = { .uri = "/api/wifi/sta", .method = HTTP_POST, .handler = ruta_wifi_sta, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_wifi_sta);

        httpd_uri_t uri_wifi_ap = { .uri = "/api/wifi/ap", .method = HTTP_POST, .handler = ruta_wifi_ap, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_wifi_ap);

        httpd_uri_t uri_ota = { .uri = "/api/ota", .method = HTTP_POST, .handler = ruta_ota, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_ota);

        httpd_uri_t uri_ota_version = { .uri = "/api/ota/version", .method = HTTP_GET, .handler = ruta_ota_version, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_ota_version);

        uart_send_msg("[WEB] Servidor HTTP iniciado en puerto 80");
    }
    else
    {
        uart_send_msg("[WEB] Error al iniciar servidor HTTP");
    }
}

/**
 * web_server_stop()
 * Detiene el servidor HTTP.
 */
void web_server_stop(void)
{
    if (server)
    {
        httpd_stop(server);
        server = NULL;
        uart_send_msg("[WEB] Servidor HTTP detenido");
    }
}
