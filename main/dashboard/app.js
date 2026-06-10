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
      var statusEl=document.getElementById('temp-status');
      var alarmEl=document.getElementById('temp-status');
      if(d.alarma){
        statusEl.className='temp-status alarm';
        statusEl.textContent='⚠ ALARMA: Temperatura excede el máximo!';
      } else if(d.temperatura>d.temp_maxima-3){
        statusEl.className='temp-status warn';
        statusEl.textContent='⚠ Temperatura elevada';
      } else {
        statusEl.className='temp-status ok';
        statusEl.textContent='✅ Temperatura normal';
      }
      document.getElementById('fan-status').textContent=d.fan_speed+'%';
      document.getElementById('curtain-status').textContent=d.curtain_pos+'%';
      document.getElementById('wifi-ip').textContent=d.wifi_ip;
      document.getElementById('wifi-connected').textContent=d.wifi_conectado?'Sí':'No';
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
