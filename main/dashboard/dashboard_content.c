#include "dashboard_content.h"

const char index_html[] = R"hdash(
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

const char style_css[] = R"cssd(
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

const char app_js[] = R"appjs(
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
