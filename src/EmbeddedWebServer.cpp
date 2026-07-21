#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "EmbeddedWebServer.h"
#include "AppState.h"
#include "ThermostatHomePage.h"
#include "TemperatureControlSystem.h"
#include "UIContext.h"
#include "iot_configs.h"
#include "DeviceAuth.h"

extern ThermostatState appState;      // defined in main.cpp
extern ThermostatHomePage homePage;   // defined in main.cpp
extern UIContext uiCtx;               // defined in main.cpp
extern void applyMode(HvacMode mode); // defined in main.cpp
extern bool webExternalMode;          // defined in main.cpp
extern TemperatureController tempController; // defined in main.cpp

static WebServer server(80);
static bool started = false;

// Add CORS headers so an externally-hosted site can call the device API.
static void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-Auth-Token");
}

// Returns false and sends a 403 if auth is required but the token is missing/invalid.
static bool checkAuth() {
  if (!DeviceAuth::isEnabled()) return true; // no devices enrolled yet — open access
  String token = server.header("X-Auth-Token");
  if (DeviceAuth::isAuthorized(token)) return true;
  addCorsHeaders();
  server.send(403, "text/plain",
    "Unauthorized. Scan the QR code on the thermostat display to add this device.");
  return false;
}

static void handleOptions() {
  addCorsHeaders();
  server.send(204);
}

static const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Thermostat</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',system-ui,Arial,sans-serif;background:#000;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}
.card{background:#fff;color:#111;border-radius:20px;padding:28px 24px 24px;width:100%;max-width:420px}
.hdr{display:flex;align-items:center;gap:10px;margin-bottom:16px}
.hdr .ref{color:#2563eb;font-size:22px;cursor:pointer;line-height:1;flex-shrink:0;background:none;border:none;padding:0}
.hdr svg{flex-shrink:0}
.hdr h1{font-size:26px;font-weight:700;color:#111}
.tabs{display:flex;gap:0;margin-bottom:20px;border-bottom:2px solid #e5e7eb}
.tab{flex:1;padding:10px 0;font-size:14px;font-weight:700;letter-spacing:.04em;background:none;border:none;border-bottom:3px solid transparent;cursor:pointer;color:#9ca3af;transition:color .15s,border-color .15s;margin-bottom:-2px}
.tab.active{color:#2563eb;border-bottom-color:#2563eb}
.row{text-align:center;font-size:17px;color:#555;margin-bottom:8px}
.row b{color:#111;font-weight:600}
.sldwrap{margin:18px 0 22px}
input[type=range]{-webkit-appearance:none;width:100%;height:6px;border-radius:3px;cursor:pointer}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:26px;height:26px;border-radius:50%;background:#2563eb;cursor:pointer;box-shadow:0 0 0 2px #fff,0 0 0 4px #2563eb}
input[type=range]::-moz-range-thumb{width:26px;height:26px;border-radius:50%;background:#2563eb;border:none;cursor:pointer}
.mgrp{display:flex;border-radius:10px;overflow:hidden;border:2px solid #2563eb;margin-bottom:16px}
.mbtn{flex:1;padding:12px 0;font-size:15px;font-weight:700;letter-spacing:.04em;border:none;cursor:pointer;background:#fff;color:#2563eb;transition:background .15s,color .15s}
.mbtn+.mbtn{border-left:1px solid #2563eb}
.mbtn.active{background:#2563eb;color:#fff}
.mstat{text-align:center;font-size:17px;color:#555;margin-bottom:20px}
.mstat b{color:#111;font-weight:600}
.sbtn{width:100%;padding:16px;font-size:15px;font-weight:700;letter-spacing:.06em;border:none;border-radius:10px;background:#d1d5db;color:#9ca3af;cursor:not-allowed;transition:background .2s,color .2s}
.sbtn.ready{background:#2563eb;color:#fff;cursor:pointer}
.msec{margin-bottom:22px}
.msec h2{font-size:12px;font-weight:700;letter-spacing:.08em;text-transform:uppercase;color:#9ca3af;margin-bottom:8px;padding-bottom:6px;border-bottom:1px solid #f3f4f6}
.mtr{display:flex;justify-content:space-between;align-items:baseline;padding:9px 0;border-bottom:1px solid #f9fafb}
.mtr:last-child{border-bottom:none}
.mtr .lbl{font-size:15px;color:#555}
.mtr .val{font-weight:700;color:#111;font-size:16px}
.msub{font-size:12px;color:#6b7280;font-weight:400;margin-left:6px}
.srow{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid #f3f4f6;margin-bottom:12px}
.slbl{font-size:15px;font-weight:600;color:#111}
.sw{position:relative;display:inline-block;width:44px;height:24px}
.sw input{opacity:0;width:0;height:0}
.sl{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;border-radius:24px;transition:.3s}
.sl::before{position:absolute;content:'';height:18px;width:18px;left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.3s}
input:checked+.sl{background:#2563eb}
input:checked+.sl::before{transform:translateX(20px)}
.sday{margin-bottom:8px;background:#f9fafb;border-radius:8px;padding:8px 10px}
.sdayname{font-size:12px;font-weight:700;color:#374151;text-transform:uppercase;letter-spacing:.06em;margin-bottom:5px}
.speriod{display:flex;align-items:center;gap:6px;padding:2px 0}
.splbl{font-size:11px;color:#6b7280;width:54px;flex-shrink:0}
.stinp{border:1px solid #e5e7eb;border-radius:6px;padding:3px 4px;font-size:12px;width:88px}
.sspinp{border:1px solid #e5e7eb;border-radius:6px;padding:3px 4px;font-size:12px;width:46px;text-align:center}
.spunit{font-size:11px;color:#6b7280}
.smsg{text-align:center;font-size:13px;color:#16a34a;min-height:18px;margin-top:6px}
.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}:focus-visible{outline:3px solid #2563eb;outline-offset:2px}.mbtn:focus-visible{outline:3px solid #1d4ed8;outline-offset:-3px}input[type=range]:focus-visible{outline:3px solid #2563eb;outline-offset:4px;border-radius:3px}.sw input:focus-visible+.sl{outline:3px solid #2563eb;outline-offset:2px;border-radius:26px}
@media(min-width:700px){.card{max-width:900px;padding:32px 40px 36px}#ctrl{display:flex;gap:36px;align-items:start}.ctrl-l,.ctrl-r{flex:1;min-width:0}#meters{display:flex;gap:32px}#meters .msec{flex:1;min-width:0}#sdGrid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.sday{margin-bottom:0}}
</style>

</head>
<body>
<main aria-label="Thermostat control">
<div class="card">
  <div class="hdr">
    <button class="ref" id="rfBtn" title="Refresh" aria-label="Refresh">&#8635;</button>
    <svg xmlns="http://www.w3.org/2000/svg" width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="#2563eb" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/></svg>
    <h1>Thermostat</h1>
  </div>
  <div class="tabs" role="tablist" aria-label="Sections">
    <button class="tab active" id="tab-ctrl" role="tab" aria-selected="true" aria-controls="ctrl" tabindex="0" onclick="showTab('ctrl',this)">Control</button>
    <button class="tab" id="tab-sched" role="tab" aria-selected="false" aria-controls="sched" tabindex="-1" onclick="showTab('sched',this)">Schedule</button>
    <button class="tab" id="tab-meters" role="tab" aria-selected="false" aria-controls="meters" tabindex="-1" onclick="showTab('meters',this)">Hour Meters</button>
  </div>
  <div id="ctrl" role="tabpanel" aria-labelledby="tab-ctrl" tabindex="0">
    <div class="ctrl-l">
      <div class="row">Current Temperature: <b id="curT">Loading...</b></div>
      <div class="row">Target Temperature: <b id="tgtT">Loading...</b></div>
      <div class="sldwrap"><label for="sld" class="sr-only">Temperature setpoint</label><input type="range" id="sld" min="60" max="90" value="72" step="1" aria-valuenow="72" aria-valuetext="72 degrees Fahrenheit"/></div>
    </div>
    <div class="ctrl-r">
      <div class="mgrp" role="group" aria-label="Heating and cooling mode">
        <button class="mbtn" data-m="Off" aria-pressed="false">OFF</button>
        <button class="mbtn" data-m="Heat" aria-pressed="false">HEAT</button>
        <button class="mbtn" data-m="Cool" aria-pressed="false">COOL</button>
      </div>
      <div class="mstat">Current Mode: <b id="curM">Loading...</b></div>
      <div class="mgrp" role="group" aria-label="Fan mode">
        <button class="mbtn fbtn" data-f="Auto" aria-pressed="false">AUTO</button>
        <button class="mbtn fbtn" data-f="High" aria-pressed="false">HIGH</button>
        <button class="mbtn fbtn" data-f="Low" aria-pressed="false">LOW</button>
      </div>
      <div class="mstat">Fan Mode: <b id="curF">Loading...</b></div>
      <button class="sbtn" id="setBtn" disabled>SET THERMOSTAT</button>
    </div>
  </div>
  <div id="meters" role="tabpanel" aria-labelledby="tab-meters" tabindex="0" style="display:none" aria-hidden="true">
    <div class="msec">
      <h2>Runtime</h2>
      <div class="mtr"><span class="lbl">&#x1F525; Heat</span><span class="val" id="mHeat">--</span></div>
      <div class="mtr"><span class="lbl">&#x2744;&#xFE0F; Cool</span><span class="val" id="mCool">--</span></div>
    </div>
    <div class="msec">
      <h2>Avg Time to Reach Setpoint</h2>
      <div class="mtr"><span class="lbl">&#x1F525; Heat</span><span class="val" id="mHeatSp">--</span></div>
      <div class="mtr"><span class="lbl">&#x2744;&#xFE0F; Cool</span><span class="val" id="mCoolSp">--</span></div>
    </div>
  </div>
  <div id="fault" role="alert" style="display:none;margin-top:12px;padding:10px 14px;background:#fee2e2;color:#b91c1c;border-radius:8px;font-size:14px;font-weight:600;align-items:center;gap:10px"><span id="faultTxt"></span><button onclick="resetFault()" aria-label="Reset fault" style="margin-left:auto;padding:4px 10px;background:#b91c1c;color:#fff;border:none;border-radius:6px;font-size:13px;font-weight:600;cursor:pointer">Reset</button></div>
  <div id="sched" role="tabpanel" aria-labelledby="tab-sched" tabindex="0" style="display:none" aria-hidden="true">
    <div class="srow"><span class="slbl" id="sEnLbl">Enable Schedule</span><label class="sw"><input type="checkbox" id="sEn" aria-labelledby="sEnLbl"><span class="sl" aria-hidden="true"></span></label></div>
    <div id="sdGrid"></div>
    <button class="sbtn ready" id="saveSchedBtn" style="margin-top:10px">SAVE SCHEDULE</button>
    <div class="smsg" id="sMsg" role="status" aria-live="polite" aria-atomic="true"></div>
  </div>
  <div id="authNotice" role="alert" aria-live="assertive" style="display:none;margin-top:12px;padding:10px 14px;background:#fef3c7;color:#92400e;border-radius:8px;font-size:14px">Not authorized. Scan the QR code on the thermostat display to add this device.</div>
</div></main>
<script>
var pm=null,pf=null,ps=null,cs=72,cm=null,cf='Auto',curTab='ctrl';
var authToken=localStorage.getItem('hvac_auth')||'';
function ah(){return authToken?{'content-type':'application/json','X-Auth-Token':authToken}:{'content-type':'application/json'}}
function showAuthNotice(){document.getElementById('authNotice').style.display='block'}
var sld=document.getElementById('sld');
var setBtn=document.getElementById('setBtn');
var mbtns=document.querySelectorAll('.mbtn[data-m]');
var fbtns=document.querySelectorAll('.fbtn');
var DAYS=['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];
var PERIODS=['morning','day','night'];
var PLBLS=['Morning','Day','Night'];
function pad2(n){return n<10?'0'+n:''+n}
function buildSchedGrid(data){
  document.getElementById('sEn').checked=data.enabled;
  var html='';
  for(var d=0;d<7;d++){
    html+='<div class="sday"><div class="sdayname">'+DAYS[d]+'</div>';
    for(var p=0;p<3;p++){
      var pk=PERIODS[p];var per=data.days[d][pk];
      var tv=pad2(per.h)+':'+pad2(per.min);
      html+='<div class="speriod"><span class="splbl" aria-hidden="true">'+PLBLS[p]+'</span>'
        +'<input type="time" class="stinp" data-d="'+d+'" data-p="'+pk+'" value="'+tv+'" aria-label="'+DAYS[d]+' '+PLBLS[p]+' start time">'
        +'<input type="number" class="sspinp" data-d="'+d+'" data-p="'+pk+'" min="60" max="90" step="1" value="'+Math.round(per.sp)+'" aria-label="'+DAYS[d]+' '+PLBLS[p]+' setpoint in Fahrenheit">'
        +'<span class="spunit" aria-hidden="true">\xb0F</span></div>';
    }
    html+='</div>';
  }
  document.getElementById('sdGrid').innerHTML=html;
}
async function loadSched(){
  try{var r=await fetch('/api/schedule');if(!r.ok)throw r;buildSchedGrid(await r.json());}
  catch(e){console.warn('sched load',e)}
}
document.getElementById('saveSchedBtn').addEventListener('click',async function(){
  var pay={enabled:document.getElementById('sEn').checked,days:[]};
  for(var d=0;d<7;d++){
    var dayObj={};
    for(var p=0;p<3;p++){
      var pk=PERIODS[p];
      var ti=document.querySelector('input[data-d="'+d+'"][data-p="'+pk+'"][type=time]');
      var si=document.querySelector('input[data-d="'+d+'"][data-p="'+pk+'"][type=number]');
      var parts=(ti.value||'00:00').split(':');
      dayObj[pk]={h:parseInt(parts[0])||0,min:parseInt(parts[1])||0,sp:parseFloat(si.value)||70};
    }
    pay.days.push(dayObj);
  }
  try{
    var r=await fetch('/api/schedule',{method:'POST',headers:ah(),body:JSON.stringify(pay)});
    if(r.status===403){showAuthNotice();return;}
    if(!r.ok)throw r;
    var m=document.getElementById('sMsg');m.textContent='Saved!';
    setTimeout(function(){m.textContent=''},2500);
  }catch(e){console.warn('sched save',e)}
});
function track(){var p=((sld.value-sld.min)/(sld.max-sld.min))*100;sld.style.background='linear-gradient(to right,#2563eb 0%,#2563eb '+p+'%,#d1d5db '+p+'%,#d1d5db 100%)';sld.setAttribute('aria-valuenow',sld.value);sld.setAttribute('aria-valuetext',sld.value+' degrees Fahrenheit')}
function setAM(m){mbtns.forEach(function(b){var on=b.dataset.m===m;b.classList.toggle('active',on);b.setAttribute('aria-pressed',String(on))})}
function setAF(f){fbtns.forEach(function(b){var on=b.dataset.f===f;b.classList.toggle('active',on);b.setAttribute('aria-pressed',String(on))})}
function chkDirty(){var d=(ps!==null&&ps!==cs)||(pm!==null&&pm!==cm)||(pf!==null&&pf!==cf);setBtn.disabled=!d;setBtn.classList.toggle('ready',d)}
function showTab(id,btn){
  curTab=id;
  ['ctrl','meters','sched'].forEach(function(p){
    var panel=document.getElementById(p);var show=p===id;
    panel.style.display=show?'':'none';panel.setAttribute('aria-hidden',show?'false':'true');
  });
  document.querySelectorAll('.tab').forEach(function(t){
    var sel=t===btn;t.classList.toggle('active',sel);
    t.setAttribute('aria-selected',String(sel));t.setAttribute('tabindex',sel?'0':'-1');
  });
  if(id==='meters')refreshMeters();
  if(id==='sched')loadSched();
}
function fmtHours(sec){var h=Math.floor(sec/3600);var m=Math.floor((sec%3600)/60);return h+'h '+m+'min'}
function fmtMin(sec){if(!sec)return '--';if(sec<60)return sec+'s';return Math.round(sec/60)+'min'}
sld.addEventListener('input',function(){ps=Number(sld.value);document.getElementById('tgtT').textContent=sld.value+' \xb0F';track();chkDirty()});
mbtns.forEach(function(b){b.addEventListener('click',function(){pm=b.dataset.m;setAM(pm);chkDirty()})});
fbtns.forEach(function(b){b.addEventListener('click',function(){pf=b.dataset.f;setAF(pf);chkDirty()})});
document.getElementById('rfBtn').addEventListener('click',function(){if(curTab==='meters')refreshMeters();else if(curTab==='sched')loadSched();else refresh()});
setBtn.addEventListener('click',async function(){
  if(setBtn.disabled)return;
  try{
    var t=[];
    if(ps!==null&&ps!==cs)t.push(fetch('/api/setpoint',{method:'POST',headers:ah(),body:JSON.stringify({setpoint:ps})}));
    if(pm!==null&&pm!==cm)t.push(fetch('/api/mode',{method:'POST',headers:ah(),body:JSON.stringify({mode:pm})}));
    if(pf!==null&&pf!==cf)t.push(fetch('/api/fan',{method:'POST',headers:ah(),body:JSON.stringify({fanMode:pf})}));
    var results=await Promise.all(t);
    if(results.some(function(r){return r.status===403})){showAuthNotice();return;}
    ps=null;pm=null;pf=null;await refresh();
  }catch(e){console.warn(e)}
});
async function refresh(){
  try{
    var r=await fetch('/api/state');if(!r.ok)throw r;var j=await r.json();
    cs=j.setpointF;cm=j.mode;cf=j.fanMode||'Auto';
    document.getElementById('curT').textContent=j.roomTempF.toFixed(1)+' \xb0F';
    document.getElementById('tgtT').textContent=j.setpointF+' \xb0F';
    document.getElementById('curM').textContent=j.mode;
    document.getElementById('curF').textContent=cf;
    var fd=document.getElementById('fault');
    if(j.fault){document.getElementById('faultTxt').textContent='\u26a0 Fault: '+j.faultMsg;fd.style.display='flex';}else{fd.style.display='none';}
    if(ps===null){sld.value=j.setpointF;track()}
    if(pm===null)setAM(j.mode);
    if(pf===null)setAF(cf);
    chkDirty();
  }catch(e){console.warn(e)}
}
async function refreshMeters(){
  try{
    var r=await fetch('/api/meters');if(!r.ok)return;var j=await r.json();
    document.getElementById('mHeat').textContent=fmtHours(j.heatRunSec);
    document.getElementById('mCool').textContent=fmtHours(j.coolRunSec);
    var hsp=j.avgHeatSetptSec?fmtMin(j.avgHeatSetptSec)+'<span class="msub">'+j.heatCycles+' cycles</span>':'--';
    var csp=j.avgCoolSetptSec?fmtMin(j.avgCoolSetptSec)+'<span class="msub">'+j.coolCycles+' cycles</span>':'--';
    document.getElementById('mHeatSp').innerHTML=hsp;
    document.getElementById('mCoolSp').innerHTML=csp;
  }catch(e){console.warn(e)}
}
async function resetFault(){
  try{
    var r=await fetch('/api/resetfault',{method:'POST',headers:ah()});
    if(r.status===403){showAuthNotice();return;}
    await refresh();
  }catch(e){console.warn(e)}
}
track();refresh();
setInterval(function(){if(curTab==='meters')refreshMeters();else if(curTab!=='sched')refresh();},5000);
document.querySelector('[role=tablist]').addEventListener('keydown',function(e){
  var tabs=Array.from(document.querySelectorAll('[role=tab]'));
  var idx=tabs.indexOf(document.activeElement);if(idx<0)return;
  var next=-1;
  if(e.key==='ArrowRight')next=(idx+1)%tabs.length;
  else if(e.key==='ArrowLeft')next=(idx+tabs.length-1)%tabs.length;
  else if(e.key==='Home')next=0;
  else if(e.key==='End')next=tabs.length-1;
  if(next>=0){e.preventDefault();tabs[next].focus();tabs[next].click();}
});
</script>
</body>
</html>
)rawliteral";

static void handleRoot() {
  // Only redirect when a real external URL has been configured AND the toggle is ON.
  static const bool hasExternalUrl =
    strncmp(WEB_EXTERNAL_URL, "https://your-thermostat", 23) != 0 &&
    strlen(WEB_EXTERNAL_URL) > 0;
  if (webExternalMode && hasExternalUrl) {
    // Redirect to the external hosted UI in Firebase mode.
    String url = WEB_EXTERNAL_URL;
    server.sendHeader("Location", url);
    server.send(302, "text/plain", "");
    return;
  }
  server.send_P(200, "text/html", index_html);
}

static void handleState() {
  addCorsHeaders();
  JsonDocument doc;
  doc["roomTempF"] = appState.roomTempF;
  doc["setpointF"] = appState.setpointF;
  doc["mode"] = (appState.mode == HvacMode::Heat) ? "Heat" : (appState.mode == HvacMode::Cool) ? "Cool" : "Off";
  switch (appState.fanSpeed) {
    case FanSpeed::High: doc["fanMode"] = "High"; break;
    case FanSpeed::Low:  doc["fanMode"] = "Low";  break;
    case FanSpeed::Auto:
    default:             doc["fanMode"] = "Auto"; break;
  }
  doc["wifiState"] = (appState.wifiState == WifiState::Connected) ? "Connected" : "Idle";
  doc["lastUpdateMs"] = appState.weather.lastUpdateMs;
  doc["fault"] = appState.faultActive;
  doc["faultMsg"] = appState.faultActive ? appState.faultMsg : "";
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleMeters() {
  addCorsHeaders();
  JsonDocument doc;
  doc["heatRunSec"]     = (unsigned long)(tempController.getHeatRunMs() / 1000UL);
  doc["coolRunSec"]     = (unsigned long)(tempController.getCoolRunMs() / 1000UL);
  doc["avgHeatSetptSec"] = (unsigned long)(tempController.getAvgHeatSetptMs() / 1000UL);
  doc["avgCoolSetptSec"] = (unsigned long)(tempController.getAvgCoolSetptMs() / 1000UL);
  doc["heatCycles"]     = tempController.getHeatSetptCount();
  doc["coolCycles"]     = tempController.getCoolSetptCount();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleSetpoint() {
  addCorsHeaders();
  if (server.method() != HTTP_POST) { server.send(405); return; }
  if (!checkAuth()) return;
  String body = server.arg("plain");
  JsonDocument doc;
  auto err = deserializeJson(doc, body);
  if (err) { server.send(400, "text/plain", "bad json"); return; }
  if (!doc["setpoint"].is<float>()) { server.send(400, "text/plain", "no setpoint"); return; }
  float sp = doc["setpoint"].as<float>();
  appState.setpointF = sp;
  tempController.updateTemperatureSettingFromAzure((int)sp);
  homePage.markSetpointDirty();
  server.send(200, "text/plain", "ok");
}

static void handleMode() {
  addCorsHeaders();
  if (server.method() != HTTP_POST) { server.send(405); return; }
  if (!checkAuth()) return;
  String body = server.arg("plain");
  JsonDocument doc;
  auto err = deserializeJson(doc, body);
  if (err) { server.send(400, "text/plain", "bad json"); return; }
  if (!doc["mode"].is<const char*>()) { server.send(400, "text/plain", "no mode"); return; }
  String modeStr = doc["mode"].as<String>();
  HvacMode newMode;
  if (modeStr.equalsIgnoreCase("Heat"))      newMode = HvacMode::Heat;
  else if (modeStr.equalsIgnoreCase("Cool")) newMode = HvacMode::Cool;
  else                                        newMode = HvacMode::Off;
  applyMode(newMode);
  server.send(200, "text/plain", "ok");
}

static void handleFan() {
  addCorsHeaders();
  if (server.method() != HTTP_POST) { server.send(405); return; }
  if (!checkAuth()) return;
  String body = server.arg("plain");
  JsonDocument doc;
  auto err = deserializeJson(doc, body);
  if (err) { server.send(400, "text/plain", "bad json"); return; }
  if (!doc["fanMode"].is<const char*>()) { server.send(400, "text/plain", "no fanMode"); return; }

  String fanStr = doc["fanMode"].as<String>();
  FanSpeed newFan = FanSpeed::Auto;
  if (fanStr.equalsIgnoreCase("High")) newFan = FanSpeed::High;
  else if (fanStr.equalsIgnoreCase("Low")) newFan = FanSpeed::Low;

  appState.fanSpeed = newFan;
  tempController.setFanSpeed(newFan);
  homePage.markModeDirty();
  server.send(200, "text/plain", "ok");
}

static void handleResetFault() {
  addCorsHeaders();
  if (server.method() != HTTP_POST) { server.send(405); return; }
  if (!checkAuth()) return;
  tempController.clearFault();
  appState.faultActive = false;
  appState.faultMsg[0] = '\0';
  homePage.markFaultDirty();
  server.send(200, "text/plain", "ok");
}

// --- Forward-declare so handlers can call the main.cpp persist helper ---
extern void saveSchedule();

static void handleGetSchedule() {
  addCorsHeaders();
  JsonDocument doc;
  doc["enabled"] = appState.schedule.enabled;
  JsonArray days = doc["days"].to<JsonArray>();
  for (int d = 0; d < 7; d++) {
    JsonObject day = days.add<JsonObject>();
    auto addPeriod = [&](const char* key, const SchedulePeriod& p) {
      JsonObject obj = day[key].to<JsonObject>();
      obj["h"]   = p.startHour;
      obj["min"] = p.startMinute;
      obj["sp"]  = p.setpointF;
    };
    addPeriod("morning", appState.schedule.days[d].morning);
    addPeriod("day",     appState.schedule.days[d].day);
    addPeriod("night",   appState.schedule.days[d].night);
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handlePostSchedule() {
  addCorsHeaders();
  if (server.method() != HTTP_POST) { server.send(405); return; }
  if (!checkAuth()) return;
  String body = server.arg("plain");
  JsonDocument doc;
  if (deserializeJson(doc, body)) { server.send(400, "text/plain", "bad json"); return; }
  if (!doc["days"].is<JsonArray>() || doc["days"].as<JsonArray>().size() != 7) {
    server.send(400, "text/plain", "days must be array[7]"); return;
  }
  appState.schedule.enabled = doc["enabled"] | false;
  JsonArray days = doc["days"].as<JsonArray>();
  for (int d = 0; d < 7; d++) {
    auto loadPeriod = [&](const char* key, SchedulePeriod& p) {
      JsonObject obj = days[d][key];
      if (!obj.isNull()) {
        p.startHour   = (uint8_t)(obj["h"]   | (int)p.startHour);
        p.startMinute = (uint8_t)(obj["min"]  | (int)p.startMinute);
        p.setpointF   = obj["sp"] | p.setpointF;
        p.setpointF   = constrain(p.setpointF, 50.0f, 95.0f);
      }
    };
    loadPeriod("morning", appState.schedule.days[d].morning);
    loadPeriod("day",     appState.schedule.days[d].day);
    loadPeriod("night",   appState.schedule.days[d].night);
  }
  saveSchedule();
  homePage.markSetpointDirty();
  server.send(200, "text/plain", "ok");
}

static void handleEnroll() {
  String code = server.arg("code");
  String token = DeviceAuth::completeEnrollment(code.c_str());
  if (token.length() == 0) {
    server.send(400, "text/html",
      "<!doctype html><html><head><meta charset='utf-8'/>"
      "<title>Enrollment Failed</title>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
      "<style>body{font-family:system-ui,sans-serif;background:#000;color:#fff;"
      "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0}"
      ".c{background:#111;border-radius:16px;padding:32px;text-align:center;max-width:340px}"
      "h1{color:#ef4444;margin-bottom:12px}p{color:#9ca3af}</style></head>"
      "<body><div class='c'><h1>Enrollment Failed</h1>"
      "<p>The code was invalid or expired.<br>Return to the thermostat display and tap "
      "<b>Add Device</b> again to generate a new code.</p></div></body></html>");
    return;
  }
  // Build success page that stores the token in localStorage.
  String html =
    "<!doctype html><html><head><meta charset='utf-8'/>"
    "<title>Device Added</title>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
    "<style>body{font-family:system-ui,sans-serif;background:#000;color:#fff;"
    "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0}"
    ".c{background:#111;border-radius:16px;padding:32px;text-align:center;max-width:340px}"
    "h1{color:#22c55e;margin-bottom:12px}p{color:#9ca3af;margin-bottom:24px}"
    "a{display:inline-block;padding:14px 28px;background:#2563eb;color:#fff;"
    "border-radius:8px;text-decoration:none;font-weight:700}</style></head>"
    "<body><div class='c'><h1>&#10003; Device Added</h1>"
    "<p>Your device is now authorized to control the thermostat.</p>"
    "<a href='/'>Go to Thermostat</a></div>"
    "<script>localStorage.setItem('hvac_auth','" + token + "');</script>"
    "</body></html>";
  server.send(200, "text/html", html);
}

void initEmbeddedWebServer() {
  if (started) return;
  server.on("/", HTTP_GET, handleRoot);
  server.on("/enroll", HTTP_GET, handleEnroll);
  // /enroll-external: validates one-time code then redirects to the external
  // hosted UI with the SITE_TOKEN so that browser gains permanent write access.
  server.on("/enroll-ext", HTTP_GET, []() {
    String code = server.arg("code");
    // Validate the code (reuse the same DeviceAuth pool).
    String token = DeviceAuth::completeEnrollment(code.c_str());
    if (token.length() == 0) {
      server.send(400, "text/html",
        "<!doctype html><html><head><meta charset='utf-8'/>"
        "<title>Failed</title>"
        "<style>body{font-family:system-ui,sans-serif;background:#000;color:#fff;"
        "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0}"
        ".c{background:#111;border-radius:16px;padding:32px;text-align:center;max-width:340px}"
        "h1{color:#ef4444;margin-bottom:12px}p{color:#9ca3af}</style></head>"
        "<body><div class='c'><h1>Code Invalid</h1>"
        "<p>The code was invalid or expired.<br>Tap <b>Add Device</b> on the thermostat again.</p>"
        "</div></body></html>");
      return;
    }
    // Redirect to the external site with the Cloudflare token.
    // If external mode was turned off, fall back to local auth success page.
    if (!webExternalMode) {
      String html =
        "<!doctype html><html><head><meta charset='utf-8'/>"
        "<title>Device Added</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<style>body{font-family:system-ui,sans-serif;background:#000;color:#fff;"
        "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0}"
        ".c{background:#111;border-radius:16px;padding:32px;text-align:center;max-width:340px}"
        "h1{color:#22c55e;margin-bottom:12px}p{color:#9ca3af;margin-bottom:24px}"
        "a{display:inline-block;padding:14px 28px;background:#2563eb;color:#fff;"
        "border-radius:8px;text-decoration:none;font-weight:700}</style></head>"
        "<body><div class='c'><h1>&#10003; Device Added</h1>"
        "<p>Your device is now authorized to control the thermostat.</p>"
        "<a href='/'>Go to Thermostat</a></div>"
        "<script>localStorage.setItem('hvac_auth','" + token + "');</script>"
        "</body></html>";
      server.send(200, "text/html", html);
      return;
    }

    // Prefer QR-provided external target; fall back to firmware config.
    String url = server.arg("next");
    if (url.length() == 0) url = WEB_EXTERNAL_URL;
    if (url.length() == 0 || strncmp(url.c_str(), "https://your-thermostat", 23) == 0) {
      server.send(400, "text/plain", "WEB_EXTERNAL_URL is not configured");
      return;
    }
    if (url.indexOf('?') >= 0) url += "&token=";
    else {
      if (!url.endsWith("/")) url += "/";
      url += "?token=";
    }
    url += SITE_TOKEN;
    server.sendHeader("Location", url);
    server.send(302, "text/plain", "");
  });
  server.on("/api/state",      HTTP_GET,  handleState);
  server.on("/api/meters",     HTTP_GET,  handleMeters);
  server.on("/api/schedule",   HTTP_GET,  handleGetSchedule);
  server.on("/api/setpoint",   HTTP_POST, handleSetpoint);
  server.on("/api/mode",       HTTP_POST, handleMode);
  server.on("/api/fan",        HTTP_POST, handleFan);
  server.on("/api/resetfault", HTTP_POST, handleResetFault);
  server.on("/api/schedule",   HTTP_POST, handlePostSchedule);
  // Handle CORS preflight for all API routes.
  server.on("/api/state",      HTTP_OPTIONS, handleOptions);
  server.on("/api/setpoint",   HTTP_OPTIONS, handleOptions);
  server.on("/api/mode",       HTTP_OPTIONS, handleOptions);
  server.on("/api/fan",        HTTP_OPTIONS, handleOptions);
  server.on("/api/resetfault", HTTP_OPTIONS, handleOptions);
  server.on("/api/schedule",   HTTP_OPTIONS, handleOptions);
  // Collect the auth token header on every request.
  const char* headers[] = { "X-Auth-Token" };
  server.collectHeaders(headers, 1);
  server.begin();
  started = true;
}

void embeddedWebServerHandleClient() {
  if (started) server.handleClient();
}

bool embeddedWebServerStarted() { return started; }
