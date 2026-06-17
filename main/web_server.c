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
#include <sys/time.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "cJSON.h"

static const char index_html[] = R"hdash(
<!DOCTYPE html>
<html lang='es'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>STR 2026 - Control Ambiental</title>
<link rel='stylesheet' href='style.css'>
</head>
<body>
<div class='header'><h1>STR 2026</h1><p>Sistema de Control Ambiental Automatizado</p></div>
<div class='container'>
<div class='card' id='temp-section'>
<h2><span class='icon'>🌡️</span> Temperatura & Ventilación</h2>
<div class='temp-display'>
<span class='temp-value' id='temp-value'>--</span><span class='temp-unit'>°C</span>
<div class='temp-label'>Temperatura Ambiente</div>
<div class='temp-status' id='temp-status'>Monitoreando...</div>
</div>
<div class='mode-toggle' id='fan-mode-toggle'>
<button id='fan-auto' class='active' onclick='setFanMode(true)'>Automático</button>
<button id='fan-manual' onclick='setFanMode(false)'>Manual</button>
</div>
<div id='fan-auto-controls'>
<div class='control-row'><label>Temperatura Deseada</label><input type='number' id='temp-deseada' value='25' min='0' max='50' step='0.5'></div>
<div class='control-row'><label>Temperatura Máxima</label><input type='number' id='temp-maxima' value='35' min='0' max='60' step='0.5'></div>
<button class='btn btn-primary btn-block' onclick='saveTempConfig()'>Guardar Configuración</button>
</div>
<div id='fan-manual-controls' style='display:none'>
<div class='control-row'><label>Velocidad Ventilador</label><input type='range' id='fan-speed' min='0' max='100' value='50' oninput='updateFanSpeedLabel()'><span id='fan-speed-label'>50%</span></div>
<button class='btn btn-primary btn-block' onclick='setFanSpeed()'>Aplicar Velocidad</button>
</div>
<div class='status-msg'>Ventilador: <span id='fan-status'>0%</span></div>
</div>
<div class='card' id='curtain-section'>
<h2><span class='icon'>🪟</span> Control de Cortinas</h2>
<div class='mode-toggle' id='curtain-mode-toggle'>
<button id='curtain-auto' class='active' onclick='setCurtainMode(true)'>Programado</button>
<button id='curtain-manual' onclick='setCurtainMode(false)'>Manual</button>
</div>
<div id='curtain-manual-controls'>
<div class='control-row'><label>Apertura</label><input type='range' id='curtain-pos' min='0' max='100' value='50' oninput='updateCurtainLabel()'><span id='curtain-pos-label'>50%</span></div>
<button class='btn btn-primary btn-block' onclick='setCurtainPosition()'>Aplicar Posición</button>
</div>
<div id='curtain-auto-controls'>
<p style='color:#aab;font-size:13px;margin-bottom:10px;'>Horarios Programados (máx. 8)</p>
<table class='schedule-table'><thead><tr><th>Hora</th><th>Min</th><th>Apertura %</th><th>Activo</th><th></th></tr></thead><tbody id='schedule-rows'></tbody></table>
<button class='btn btn-success btn-small' onclick='addScheduleRow()'>+ Agregar Horario</button>
<button class='btn btn-primary btn-block' onclick='saveSchedules()'>Guardar Horarios</button>
</div>
<div class='status-msg'>Cortina: <span id='curtain-status'>50%</span></div>
</div>
<div class='card' id='rgb-section'>
<h2><span class='icon'>💡</span> Iluminación LED RGB</h2>
<div class='rgb-preview' id='rgb-preview'></div>
<div class='color-picker-row'><label>Color</label><input type='color' id='rgb-color' value='#ffffff' onchange='previewRGB()'></div>
<div class='control-row'><label>Brillo</label><input type='range' id='rgb-brillo' min='0' max='100' value='50' oninput='updateBrilloLabel();previewRGB()'><span id='rgb-brillo-label'>50%</span></div>
<button class='btn btn-primary btn-block' onclick='applyRGB()'>Aplicar Color</button>
</div>
<div class='card' id='wifi-section'>
<h2><span class='icon'>📶</span> Configuración de Red</h2>
  <div class='wifi-status'><div><span class='label'>IP Station:</span> <span class='value' id='wifi-ip'>0.0.0.0</span></div><div><span class='label'>Conectado:</span> <span class='value' id='wifi-connected'>No</span></div><div><span class='label'>Hora:</span> <span class='value' id='hora-actual'>--:--:--</span></div></div>
<h3 style='color:#aab;font-size:14px;margin:10px 0;'>Red WiFi (Station)</h3>
<div class='control-row'><label>SSID</label><input type='text' id='sta-ssid' placeholder='Nombre de la red'></div>
<div class='control-row'><label>Contraseña</label><input type='password' id='sta-pass' placeholder='Contraseña'></div>
<button class='btn btn-success btn-block' onclick='connectSTA()'>Conectar a WiFi</button>
<h3 style='color:#aab;font-size:14px;margin:15px 0 10px;'>Punto de Acceso (AP)</h3>
<div class='control-row'><label>SSID</label><input type='text' id='ap-ssid' placeholder='STR2026_Control'></div>
<div class='control-row'><label>Contraseña</label><input type='password' id='ap-pass' placeholder='Mín. 8 caracteres'></div>
<button class='btn btn-warning btn-block' onclick='configAP()'>Cambiar AP</button>
</div>
<div class='card' id='ota-section'>
<h2><span class='icon'>🔄</span> Actualización OTA</h2>
<p style='color:#aab;font-size:13px;margin-bottom:10px;'>Ingrese la URL del firmware .bin para actualizar el dispositivo de forma inalámbrica.</p>
<div class='ota-row'><input type='url' id='ota-url' placeholder='http://ejemplo.com/firmware.bin'><button class='btn btn-warning' onclick='startOTA()'>Actualizar</button></div>
<div class='status-msg'>Versión: <span id='ota-version'>---</span></div>
</div>
</div>
<div class='toast' id='toast'></div>
<script src='app.js'></script>
</body>
</html>
)hdash";

static const char style_css[] = R"cssd(
* { margin:0; padding:0; box-sizing:border-box; font-family:'Segoe UI',sans-serif; }
body { background:#1a1a2e; color:#eee; min-height:100vh; display:flex; flex-direction:column; }
.header { background:#16213e; padding:20px; text-align:center; border-bottom:3px solid #0f3460; }
.header h1 { color:#e94560; font-size:28px; }
.header p { color:#889; font-size:14px; margin-top:5px; }
.container { max-width:1200px; margin:0 auto; padding:20px; display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:20px; }
.card { background:#16213e; border-radius:12px; padding:20px; box-shadow:0 4px 15px rgba(0,0,0,0.3); }
.card h2 { color:#e94560; font-size:18px; margin-bottom:15px; border-bottom:1px solid #0f3460; padding-bottom:8px; display:flex; align-items:center; gap:8px; }
.card h2 .icon { font-size:22px; }
.temp-display { text-align:center; padding:10px; }
.temp-value { font-size:48px; font-weight:bold; color:#fff; }
.temp-label { color:#889; font-size:14px; margin-top:5px; }
.temp-unit { font-size:24px; color:#889; }
.temp-status { margin-top:10px; padding:8px 15px; border-radius:20px; display:inline-block; font-size:13px; }
.temp-status.ok { background:#1b5e20; color:#81c784; }
.temp-status.warn { background:#e65100; color:#ffcc80; }
.temp-status.alarm { background:#b71c1c; color:#ef9a9a; animation:pulse 1s infinite; }
@keyframes pulse { 0%,100% { opacity:1; } 50% { opacity:0.5; } }
.control-row { display:flex; align-items:center; gap:15px; margin:10px 0; flex-wrap:wrap; }
.control-row label { min-width:120px; color:#aab; font-size:14px; }
.control-row input[type='range'] { flex:1; min-width:150px; }
.control-row input[type='number'] { width:80px; padding:6px; border-radius:6px; border:1px solid #0f3460; background:#1a1a2e; color:#fff; text-align:center; }
.control-row input[type='color'] { width:60px; height:40px; border:none; border-radius:6px; cursor:pointer; background:transparent; }
.control-row select { padding:8px; border-radius:6px; border:1px solid #0f3460; background:#1a1a2e; color:#fff; }
.btn { padding:8px 20px; border:none; border-radius:8px; cursor:pointer; font-size:14px; font-weight:bold; transition:all 0.3s; }
.btn-primary { background:#e94560; color:#fff; }
.btn-primary:hover { background:#d63851; transform:translateY(-2px); }
.btn-success { background:#2e7d32; color:#fff; }
.btn-success:hover { background:#388e3c; }
.btn-warning { background:#e65100; color:#fff; }
.btn-warning:hover { background:#bf360c; }
.btn-small { padding:5px 12px; font-size:12px; }
.btn-block { width:100%; margin-top:10px; }
.mode-toggle { display:flex; gap:0; margin:10px 0; }
.mode-toggle button { flex:1; padding:8px; border:1px solid #0f3460; background:#1a1a2e; color:#889; cursor:pointer; transition:all 0.3s; }
.mode-toggle button:first-child { border-radius:8px 0 0 8px; }
.mode-toggle button:last-child { border-radius:0 8px 8px 0; }
.mode-toggle button.active { background:#e94560; color:#fff; border-color:#e94560; }
.schedule-table { width:100%; border-collapse:collapse; margin:10px 0; font-size:13px; }
.schedule-table th { background:#0f3460; padding:8px; text-align:left; color:#aab; }
.schedule-table td { padding:8px; border-bottom:1px solid #0f3460; }
.schedule-table td input { width:100%; padding:4px; border-radius:4px; border:1px solid #0f3460; background:#1a1a2e; color:#fff; }
.rgb-preview { width:60px; height:60px; border-radius:50%; border:3px solid #0f3460; margin:10px auto; transition:all 0.3s; }
.color-picker-row { display:flex; align-items:center; gap:15px; margin:8px 0; }
.color-picker-row label { min-width:40px; color:#aab; }
.wifi-status { padding:10px; background:#0f3460; border-radius:8px; margin:10px 0; font-size:13px; }
.wifi-status .label { color:#889; }
.wifi-status .value { color:#fff; font-weight:bold; }
.ota-row { display:flex; gap:10px; }
.ota-row input { flex:1; padding:8px; border-radius:6px; border:1px solid #0f3460; background:#1a1a2e; color:#fff; }
.toast { position:fixed; bottom:20px; right:20px; padding:12px 24px; border-radius:8px; font-size:14px; z-index:999; opacity:0; transition:opacity 0.5s; }
.toast.success { background:#2e7d32; color:#fff; }
.toast.error { background:#c62828; color:#fff; }
.toast.show { opacity:1; }
.status-msg { font-size:12px; color:#889; margin-top:5px; }
@media (max-width:600px) { .container { padding:10px; } .card { padding:15px; } }
)cssd";

static const char app_js[] = R"appjs(
function showToast(msg, type) {
  var t=document.getElementById('toast');
  t.textContent=msg;
  t.className='toast '+type+' show';
  setTimeout(function(){t.className='toast '+type;},3000);
}

function apiPost(url, data, callback) {
  var xhr=new XMLHttpRequest();
  xhr.open('POST', url, true);
  xhr.setRequestHeader('Content-Type','application/json');
  xhr.onreadystatechange=function(){
    if(xhr.readyState==4){
      if(xhr.status==200){showToast('Comando ejecutado','success');if(callback)callback(JSON.parse(xhr.responseText));}
      else showToast('Error: '+xhr.status,'error');
    }
  };
  xhr.send(JSON.stringify(data));
}

function setFanMode(auto){
  document.getElementById('fan-auto').className=auto?'active':'';
  document.getElementById('fan-manual').className=auto?'':'active';
  document.getElementById('fan-auto-controls').style.display=auto?'block':'none';
  document.getElementById('fan-manual-controls').style.display=auto?'none':'block';
  apiPost('/api/fan/mode',{auto_mode:auto});
}

function updateFanSpeedLabel(){
  var v=document.getElementById('fan-speed').value;
  document.getElementById('fan-speed-label').textContent=v+'%';
}

function saveTempConfig(){
  var td=parseFloat(document.getElementById('temp-deseada').value);
  var tm=parseFloat(document.getElementById('temp-maxima').value);
  apiPost('/api/fan/config',{temp_deseada:td,temp_maxima:tm});
}

function setFanSpeed(){
  var v=parseInt(document.getElementById('fan-speed').value);
  apiPost('/api/fan/speed',{speed:v});
}

function setCurtainMode(auto){
  document.getElementById('curtain-auto').className=auto?'active':'';
  document.getElementById('curtain-manual').className=auto?'':'active';
  document.getElementById('curtain-auto-controls').style.display=auto?'block':'none';
  document.getElementById('curtain-manual-controls').style.display=auto?'none':'block';
  apiPost('/api/curtain/mode',{auto_mode:auto});
}

function updateCurtainLabel(){
  var v=document.getElementById('curtain-pos').value;
  document.getElementById('curtain-pos-label').textContent=v+'%';
}

function setCurtainPosition(){
  var v=parseInt(document.getElementById('curtain-pos').value);
  apiPost('/api/curtain/position',{position:v});
}

var schedIdx=0;
function addScheduleRow(existing){
  var t=document.getElementById('schedule-rows');
  var r=document.createElement('tr');
  var idx=schedIdx++;
  r.id='sched-'+idx;
  var h=existing?existing.hora:'8';
  var m=existing?existing.minuto:'0';
  var a=existing?existing.apertura:'50';
  var act=existing?existing.activo:true;
  r.innerHTML='<td><input type="number" id="sh-'+idx+'" value="'+h+'" min="0" max="23"></td>'
    +'<td><input type="number" id="sm-'+idx+'" value="'+m+'" min="0" max="59"></td>'
    +'<td><input type="number" id="sa-'+idx+'" value="'+a+'" min="0" max="100"></td>'
    +'<td><input type="checkbox" id="sact-'+idx+'" '+(act?'checked':'')+'></td>'
    +'<td><button class="btn btn-small btn-warning" onclick="this.parentElement.parentElement.remove()">X</button></td>';
  t.appendChild(r);
}

function saveSchedules(){
  var rows=document.getElementById('schedule-rows').children;
  var scheds=[];
  for(var i=0;i<rows.length;i++){
    var id=rows[i].id.split('-')[1];
    scheds.push({
      hora:parseInt(document.getElementById('sh-'+id).value),
      minuto:parseInt(document.getElementById('sm-'+id).value),
      apertura:parseInt(document.getElementById('sa-'+id).value),
      activo:document.getElementById('sact-'+id).checked
    });
  }
  apiPost('/api/curtain/schedule',{schedules:scheds});
}

function previewRGB(){
  var c=document.getElementById('rgb-color').value;
  var b=parseInt(document.getElementById('rgb-brillo').value)/100;
  var p=document.getElementById('rgb-preview');
  p.style.backgroundColor=c;
  p.style.opacity=Math.max(0.1,b);
}

function updateBrilloLabel(){
  var v=document.getElementById('rgb-brillo').value;
  document.getElementById('rgb-brillo-label').textContent=v+'%';
}

function applyRGB(){
  var c=document.getElementById('rgb-color').value;
  var b=parseInt(document.getElementById('rgb-brillo').value);
  var r=parseInt(c.substr(1,2),16);
  var g=parseInt(c.substr(3,2),16);
  var bl=parseInt(c.substr(5,2),16);
  apiPost('/api/rgb',{r:r,g:g,b:bl,brillo:b});
}

function connectSTA(){
  var s=document.getElementById('sta-ssid').value;
  var p=document.getElementById('sta-pass').value;
  if(!s){showToast('Ingrese un SSID','error');return;}
  apiPost('/api/wifi/sta',{ssid:s,password:p});
}

function configAP(){
  var s=document.getElementById('ap-ssid').value;
  var p=document.getElementById('ap-pass').value;
  if(!s){showToast('Ingrese un SSID para el AP','error');return;}
  if(p.length<8){showToast('La contraseña debe tener al menos 8 caracteres','error');return;}
  apiPost('/api/wifi/ap',{ssid:s,password:p});
}

function startOTA(){
  var url=document.getElementById('ota-url').value;
  if(!url){showToast('Ingrese una URL','error');return;}
  if(!confirm('¿Está seguro de que desea actualizar el firmware?')) return;
  apiPost('/api/ota',{url:url});
}

function updateData(){
  var xhr=new XMLHttpRequest();
  xhr.open('GET','/api/temp',true);
  xhr.onreadystatechange=function(){
    if(xhr.readyState==4&&xhr.status==200){
      var d=JSON.parse(xhr.responseText);
      document.getElementById('temp-value').textContent=d.temperatura.toFixed(1);
      if(document.activeElement!=document.getElementById('temp-deseada'))
        document.getElementById('temp-deseada').value=d.temp_deseada;
      if(document.activeElement!=document.getElementById('temp-maxima'))
        document.getElementById('temp-maxima').value=d.temp_maxima;
      var el=document.getElementById('temp-status');
      if(d.alarma){
        el.className='temp-status alarm';
        el.textContent='\u26A0 ALARMA: Temperatura excede el m\u00E1ximo!';
      } else if(d.temperatura>d.temp_maxima-3){
        el.className='temp-status warn';
        el.textContent='\u26A0 Temperatura elevada';
      } else {
        el.className='temp-status ok';
        el.textContent='\u2705 Temperatura normal';
      }
      var fanLabel=d.fan_auto_mode?'Auto: ':'';
      document.getElementById('fan-status').textContent=fanLabel+d.fan_speed+'%';
      document.getElementById('curtain-status').textContent=d.curtain_pos+'%';
      document.getElementById('wifi-ip').textContent=d.wifi_ip;
      document.getElementById('wifi-connected').textContent=d.wifi_conectado?'S\u00ED':'No';
      document.getElementById('hora-actual').textContent=d.hora+(d.hora_sincronizada?' (NTP)':' (local)');
    }
  };
  xhr.send();
}

window.onload=function(){
  var xhr=new XMLHttpRequest();
  xhr.open('GET','/api/curtain/schedule',true);
  xhr.onreadystatechange=function(){
    if(xhr.readyState==4&&xhr.status==200){
      var d=JSON.parse(xhr.responseText);
      for(var i=0;i<d.schedules.length;i++){
        addScheduleRow(d.schedules[i]);
      }
    }
  };
  xhr.send();
  var x2=new XMLHttpRequest();
  x2.open('GET','/api/ota/version',true);
  x2.onreadystatechange=function(){
    if(x2.readyState==4&&x2.status==200)
      document.getElementById('ota-version').textContent=JSON.parse(x2.responseText).version;
  };
  x2.send();
  updateData();
  setInterval(updateData,3000);
};
)appjs";

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
            return -1;
        }
        total_len += ret;
        remaining -= ret;
    }
    buffer[total_len] = '\0';
    return total_len;
}

static esp_err_t ruta_raiz(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_sendstr(req, index_html);
    return ESP_OK;
}

static esp_err_t ruta_estilo(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_sendstr(req, style_css);
    return ESP_OK;
}

static esp_err_t ruta_script(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_sendstr(req, app_js);
    return ESP_OK;
}

static esp_err_t ruta_api_temp(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    char buffer[512];
    float temp_act = ctx->temperatura_actual;

    time_t now;
    struct tm ti;
    time(&now);
    localtime_r(&now, &ti);
    char hora_str[32];
    strftime(hora_str, sizeof(hora_str), "%H:%M:%S", &ti);

    snprintf(buffer, sizeof(buffer),
        "{\"temperatura\":%.1f,\"temp_deseada\":%.1f,\"temp_maxima\":%.1f,"
        "\"fan_speed\":%d,\"fan_auto_mode\":%s,"
        "\"curtain_pos\":%d,\"alarma\":%s,"
        "\"wifi_ip\":\"%s\",\"wifi_conectado\":%s,"
        "\"hora\":\"%s\",\"hora_sincronizada\":%s}",
        temp_act, ctx->temp_deseada, ctx->temp_maxima,
        pwm_fan_get_speed(ctx),
        ctx->fan_auto_mode ? "true" : "false",
        ctx->curtain_position,
        (temp_act > ctx->temp_maxima) ? "true" : "false",
        wifi_obtener_ip_sta(ctx),
        wifi_esta_conectado(ctx) ? "true" : "false",
        hora_str,
        web_hora_sincronizada(ctx) ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

static esp_err_t ruta_fan_mode(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    uart_send_msg("[DBG] POST /api/fan/mode");
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *auto_mode = cJSON_GetObjectItem(json, "auto_mode");
        if (auto_mode)
        {
            web_set_fan_auto_mode(ctx, auto_mode->valueint);
            uart_send_msg("[WEB] Fan modo: %s", auto_mode->valueint ? "Automático" : "Manual");
        }
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t ruta_fan_config(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    uart_send_msg("[DBG] POST /api/fan/config");
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *td = cJSON_GetObjectItem(json, "temp_deseada");
        cJSON *tm = cJSON_GetObjectItem(json, "temp_maxima");
        if (td) web_set_temp_deseada(ctx, (float)td->valuedouble);
        if (tm) web_set_temp_maxima(ctx, (float)tm->valuedouble);
        uart_send_msg("[WEB] Temp config: Deseada=%.1f, Max=%.1f", ctx->temp_deseada, ctx->temp_maxima);
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t ruta_fan_speed(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    uart_send_msg("[DBG] POST /api/fan/speed");
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
            web_set_fan_speed_manual(ctx, speed);
            if (!ctx->fan_auto_mode) pwm_fan_set_speed(ctx, speed);
            uart_send_msg("[WEB] Fan velocidad manual: %d%%", speed);
        }
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t ruta_curtain_mode(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    uart_send_msg("[DBG] POST /api/curtain/mode");
    char content[128];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *auto_mode = cJSON_GetObjectItem(json, "auto_mode");
        if (auto_mode)
        {
            web_set_curtain_auto_mode(ctx, auto_mode->valueint);
            uart_send_msg("[WEB] Cortina modo: %s", auto_mode->valueint ? "Programado" : "Manual");
        }
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t ruta_curtain_position(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    uart_send_msg("[DBG] POST /api/curtain/position");
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
            web_set_curtain_position(ctx, p);
            if (!ctx->curtain_auto_mode) pwm_servo_set_position(p);
            uart_send_msg("[WEB] Cortina posición: %d%%", p);
        }
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t ruta_curtain_schedule_post(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    char content[2048];
    leer_cuerpo_post(req, content, sizeof(content));

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
                ctx->schedules[i].hora     = (uint8_t)cJSON_GetObjectItem(item, "hora")->valueint;
                ctx->schedules[i].minuto   = (uint8_t)cJSON_GetObjectItem(item, "minuto")->valueint;
                ctx->schedules[i].apertura = (uint8_t)cJSON_GetObjectItem(item, "apertura")->valueint;
                cJSON *act = cJSON_GetObjectItem(item, "activo");
                ctx->schedules[i].activo   = act ? act->valueint : true;
            }
            ctx->schedule_count = count;
            uart_send_msg("[WEB] %d horarios de cortina guardados", count);
        }
        cJSON_Delete(json);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t ruta_curtain_schedule_get(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    char buffer[2048];
    int offset = snprintf(buffer, sizeof(buffer), "{\"schedules\":[");

    for (int i = 0; i < ctx->schedule_count; i++)
    {
        int remaining = sizeof(buffer) - offset;
        if (remaining < 64) break;
        offset += snprintf(buffer + offset, remaining,
            "%s{\"hora\":%d,\"minuto\":%d,\"apertura\":%d,\"activo\":%s}",
            (i > 0) ? "," : "",
            ctx->schedules[i].hora, ctx->schedules[i].minuto, ctx->schedules[i].apertura,
            ctx->schedules[i].activo ? "true" : "false");
    }

    snprintf(buffer + offset, sizeof(buffer) - offset, "]}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

static esp_err_t ruta_rgb(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    uart_send_msg("[DBG] POST /api/rgb");
    char content[256];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        uint8_t r = ctx->rgb_r, g = ctx->rgb_g, b = ctx->rgb_b, br = ctx->rgb_brillo;

        cJSON *item;
        if ((item = cJSON_GetObjectItem(json, "r")))      r = (uint8_t)item->valueint;
        if ((item = cJSON_GetObjectItem(json, "g")))      g = (uint8_t)item->valueint;
        if ((item = cJSON_GetObjectItem(json, "b")))      b = (uint8_t)item->valueint;
        if ((item = cJSON_GetObjectItem(json, "brillo"))) br = (uint8_t)item->valueint;

        web_set_rgb_color(ctx, r, g, b, br);

        rgb_color_t color = { .r = r, .g = g, .b = b, .brightness = br };
        led_rgb_set(&color);

        uart_send_msg("[WEB] RGB: R=%d G=%d B=%d Brillo=%d%%", r, g, b, br);
        cJSON_Delete(json);
    }
    else uart_send_msg("[DBG] JSON parse error: %s", content);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t ruta_wifi_sta(httpd_req_t *req)
{
    system_context_t *ctx = (system_context_t *)req->user_ctx;
    uart_send_msg("[DBG] POST /api/wifi/sta");
    char content[512];
    leer_cuerpo_post(req, content, sizeof(content));
    cJSON *json = cJSON_Parse(content);

    if (json)
    {
        cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
        cJSON *pass = cJSON_GetObjectItem(json, "password");

        if (ssid && ssid->valuestring)
        {
            wifi_conectar_sta(ctx,
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

static esp_err_t ruta_wifi_ap(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/wifi/ap");
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

static esp_err_t ruta_ota(httpd_req_t *req)
{
    uart_send_msg("[DBG] POST /api/ota");
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

static esp_err_t ruta_ota_version(httpd_req_t *req)
{
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "{\"version\":\"%s\"}", ota_get_cur_ota_version());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

static esp_err_t ruta_ping(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"pong\":true,\"server\":\"STR2026\"}");
    return ESP_OK;
}

static esp_err_t ruta_time_get(httpd_req_t *req)
{
    time_t now;
    struct tm ti;
    time(&now);
    localtime_r(&now, &ti);

    httpd_resp_set_type(req, "application/json");
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"hora\":%d,\"min\":%d,\"seg\":%d,\"anio\":%d,\"mes\":%d,\"dia\":%d}",
        ti.tm_hour, ti.tm_min, ti.tm_sec,
        ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t ruta_time_set(httpd_req_t *req)
{
    char content[128];
    int len = httpd_req_recv(req, content, sizeof(content) - 1);
    if (len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }
    content[len] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    int h = 8, m = 0, s = 0;
    cJSON *h_item = cJSON_GetObjectItem(json, "hora");
    cJSON *m_item = cJSON_GetObjectItem(json, "min");
    cJSON *s_item = cJSON_GetObjectItem(json, "seg");
    if (h_item) h = h_item->valueint;
    if (m_item) m = m_item->valueint;
    if (s_item) s = s_item->valueint;
    cJSON_Delete(json);

    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid time");
        return ESP_FAIL;
    }

    struct tm tm_set;
    time_t now;
    time(&now);
    localtime_r(&now, &tm_set);
    tm_set.tm_hour = h;
    tm_set.tm_min  = m;
    tm_set.tm_sec  = s;

    time_t t = mktime(&tm_set);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    uart_send_msg("[WEB] Hora cambiada a %02d:%02d:%02d", h, m, s);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// ============================================================
// FUNCIONES DE ACCESO A VARIABLES COMPARTIDAS
// ============================================================

float web_get_temperatura_actual(system_context_t *ctx) { return ctx->temperatura_actual; }
void  web_set_temperatura_actual(system_context_t *ctx, float temp) { ctx->temperatura_actual = temp; }
float web_get_temp_deseada(system_context_t *ctx) { return ctx->temp_deseada; }
float web_get_temp_maxima(system_context_t *ctx) { return ctx->temp_maxima; }
void  web_set_temp_deseada(system_context_t *ctx, float t) { ctx->temp_deseada = t; }
void  web_set_temp_maxima(system_context_t *ctx, float t) { ctx->temp_maxima = t; }
bool  web_get_fan_auto_mode(system_context_t *ctx) { return ctx->fan_auto_mode; }
void  web_set_fan_auto_mode(system_context_t *ctx, bool mode) { ctx->fan_auto_mode = mode; }
uint8_t web_get_fan_speed_manual(system_context_t *ctx) { return ctx->fan_speed_manual; }
void    web_set_fan_speed_manual(system_context_t *ctx, uint8_t s) { ctx->fan_speed_manual = s; }
bool  web_get_curtain_auto_mode(system_context_t *ctx) { return ctx->curtain_auto_mode; }
void  web_set_curtain_auto_mode(system_context_t *ctx, bool mode) { ctx->curtain_auto_mode = mode; }
uint8_t web_get_curtain_position(system_context_t *ctx) { return ctx->curtain_position; }
void    web_set_curtain_position(system_context_t *ctx, uint8_t p) { ctx->curtain_position = p; }
void web_get_rgb_color(system_context_t *ctx, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *brillo)
    { *r = ctx->rgb_r; *g = ctx->rgb_g; *b = ctx->rgb_b; *brillo = ctx->rgb_brillo; }
void web_set_rgb_color(system_context_t *ctx, uint8_t r, uint8_t g, uint8_t b, uint8_t brillo)
    { ctx->rgb_r = r; ctx->rgb_g = g; ctx->rgb_b = b; ctx->rgb_brillo = brillo; }
void web_get_schedules(system_context_t *ctx, curtain_schedule_t *s, int *count)
    { memcpy(s, ctx->schedules, sizeof(ctx->schedules)); *count = ctx->schedule_count; }
int  web_get_schedule_count(system_context_t *ctx) { return ctx->schedule_count; }

void web_set_schedules(system_context_t *ctx, curtain_schedule_t *s, int count)
{
    if (count > MAX_CURTAIN_SCHEDULES) count = MAX_CURTAIN_SCHEDULES;
    memcpy(ctx->schedules, s, count * sizeof(curtain_schedule_t));
    ctx->schedule_count = count;
}

// ============================================================
// REGISTRO DE RUTAS
// ============================================================

void web_server_start(system_context_t *ctx)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 26;
    config.lru_purge_enable = true;

    esp_err_t reg_ok = ESP_OK;

    if (httpd_start(&ctx->server, &config) == ESP_OK)
    {
#define REG_URI(u, m, h) do { \
    httpd_uri_t uri = { .uri = u, .method = m, .handler = h, .user_ctx = ctx }; \
    esp_err_t e = httpd_register_uri_handler(ctx->server, &uri); \
    if (e != ESP_OK) { uart_send_msg("[WEB] Error registrando %s: %s", u, esp_err_to_name(e)); reg_ok = e; } \
} while(0)

        REG_URI("/", HTTP_GET, ruta_raiz);
        REG_URI("/style.css", HTTP_GET, ruta_estilo);
        REG_URI("/app.js", HTTP_GET, ruta_script);
        REG_URI("/api/ping", HTTP_GET, ruta_ping);
        REG_URI("/api/temp", HTTP_GET, ruta_api_temp);
        REG_URI("/api/fan/mode", HTTP_POST, ruta_fan_mode);
        REG_URI("/api/fan/config", HTTP_POST, ruta_fan_config);
        REG_URI("/api/fan/speed", HTTP_POST, ruta_fan_speed);
        REG_URI("/api/curtain/mode", HTTP_POST, ruta_curtain_mode);
        REG_URI("/api/curtain/position", HTTP_POST, ruta_curtain_position);
        REG_URI("/api/curtain/schedule", HTTP_POST, ruta_curtain_schedule_post);
        REG_URI("/api/curtain/schedule", HTTP_GET, ruta_curtain_schedule_get);
        REG_URI("/api/rgb", HTTP_POST, ruta_rgb);
        REG_URI("/api/wifi/sta", HTTP_POST, ruta_wifi_sta);
        REG_URI("/api/wifi/ap", HTTP_POST, ruta_wifi_ap);
        REG_URI("/api/ota", HTTP_POST, ruta_ota);
        REG_URI("/api/ota/version", HTTP_GET, ruta_ota_version);
        REG_URI("/api/time", HTTP_GET, ruta_time_get);
        REG_URI("/api/time/set", HTTP_POST, ruta_time_set);

        if (reg_ok == ESP_OK)
            uart_send_msg("[WEB] Servidor HTTP iniciado en puerto 80");
        else
            uart_send_msg("[WEB] Servidor iniciado con errores en algunas rutas");
    }
    else uart_send_msg("[WEB] Error al iniciar servidor HTTP");
}

void web_server_stop(system_context_t *ctx)
{
    if (ctx->server)
    {
        httpd_stop(ctx->server);
        ctx->server = NULL;
        uart_send_msg("[WEB] Servidor HTTP detenido");
    }
}
