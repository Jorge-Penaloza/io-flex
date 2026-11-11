/*
  IO FLEX  -  Nodo de I/O remoto Modbus TCP + portal web propio (red + pines)
  Arduino Nano ESP32 (ESP32-S3)  -  Jorge Peñaloza  -  2026-11-10

  El nodo se configura A SI MISMO desde su propio portal web (puerto 80):
    Pestana RED   : nombre del nodo (mDNS), SSID, clave e IP (DHCP o estatica).
                    Guardado a prueba de fallos (valida la conexion antes de quedarse
                    con la configuracion):
                      - Desde el AP: prueba la red nueva en modo AP+STA sin perder el
                        AP; solo guarda si conecta, y te lo dice en la misma pagina.
                      - Desde la red (STA): guarda la config nueva como CANDIDATO y
                        reinicia; si conecta la promueve, si NO conecta vuelve sola a
                        la ultima configuracion que funcionaba (reversion / rollback).
                    Si nunca logra conectar, levanta el AP "IOFlex-Setup".
                    Responde como <nombre>.local (mDNS).
    Pestana PINES : direccion Modbus de inicio (la eliges tu) + por cada pin su
                    modo (off/entrada/pullup/salida/analogica), leer/escribir valor
                    y guardar los modos en NVS. La tabla muestra el REGISTRO de cada pin.

  Modbus TCP (puerto 502)  -  SOLO VALORES:
    Un bloque de NPINS holding registers desde la direccion de inicio (base):
       registro (base + i) = valor del pin i   (entrada/analogica se lee; salida se escribe)
    El MODO de cada pin se fija por la web (no por Modbus).
    Indices de pin:  0..11 = D2..D13 ; 12..19 = A0..A7

  Resumen: configuracion por web local; red validada antes de guardar; control por Modbus.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ModbusIP_ESP8266.h>
#include <Preferences.h>

// Respaldo si NVS vacio (la provision real es por el portal web del nodo).
// Editar antes de compilar, o dejarlo y configurar la red por el portal AP "IOFlex-Setup".
const char* DEF_SSID = "TU_WIFI";
const char* DEF_PASS = "TU_CLAVE_WIFI";

const char* AP_SSID = "IOFlex-Setup";
const char* AP_PASS = "configurar";   // WPA2 >=8

Preferences prefs;
WebServer web(80);
ModbusIP mb;

// Tipo de configuracion de red (arriba, para los prototipos que genera Arduino)
struct NetCfg { String ssid, pass, ip, gw, mask, host; bool stat; };

// ---------------- IO Flex ----------------
struct PinDef { uint8_t pin; bool adc; };
PinDef PINS[] = {
  {D2,false},{D3,false},{D4,false},{D5,false},{D6,false},{D7,false},
  {D8,false},{D9,false},{D10,false},{D11,false},{D12,false},{D13,false},
  {A0,true},{A1,true},{A2,true},{A3,true},{A4,true},{A5,true},{A6,true},{A7,true}
};
const int NPINS = sizeof(PINS) / sizeof(PINS[0]);
enum { M_OFF=0, M_IN=1, M_INPULL=2, M_OUT=3, M_ADC=4 };

uint8_t  cfg[NPINS];
uint8_t  lastCfg[NPINS];
uint16_t mbBase = 0;

String pinName(int i) { return (i < 12) ? ("D" + String(i + 2)) : ("A" + String(i - 12)); }

// ---------------- Red ----------------
NetCfg good;              // configuracion buena actual
bool   trying  = false;  // hay un candidato por probar
bool   apMode  = false;

void loadNet() {
  prefs.begin("net", true);
  good.ssid = prefs.getString("ssid", "");
  good.pass = prefs.getString("pass", "");
  good.stat = prefs.getBool("stat", false);
  good.ip   = prefs.getString("ip", "");
  good.gw   = prefs.getString("gw", "");
  good.mask = prefs.getString("mask", "");
  good.host = prefs.getString("mdns", "ioflex");
  trying    = prefs.getBool("trying", false);
  prefs.end();
  if (good.host.length() == 0) good.host = "ioflex";
}
void writeNet(const char* ns_dummy, const NetCfg& c, bool tryingFlag) {
  prefs.begin("net", false);
  prefs.putString("ssid", c.ssid); prefs.putString("pass", c.pass);
  prefs.putBool("stat", c.stat);
  prefs.putString("ip", c.ip); prefs.putString("gw", c.gw); prefs.putString("mask", c.mask);
  prefs.putString("mdns", c.host.length() ? c.host : "ioflex");
  prefs.putBool("trying", tryingFlag);
  prefs.end();
}
void saveGood(const NetCfg& c) { writeNet("net", c, false); good = c; }  // promueve y apaga trying

void saveCandidate(const NetCfg& c) {
  prefs.begin("net", false);
  prefs.putString("cssid", c.ssid); prefs.putString("cpass", c.pass);
  prefs.putBool("cstat", c.stat);
  prefs.putString("cip", c.ip); prefs.putString("cgw", c.gw); prefs.putString("cmask", c.mask);
  prefs.putString("chost", c.host.length() ? c.host : "ioflex");
  prefs.putBool("trying", true);
  prefs.end();
  trying = true;
}
void loadCandidate(NetCfg& c) {
  prefs.begin("net", true);
  c.ssid = prefs.getString("cssid", "");
  c.pass = prefs.getString("cpass", "");
  c.stat = prefs.getBool("cstat", false);
  c.ip   = prefs.getString("cip", "");
  c.gw   = prefs.getString("cgw", "");
  c.mask = prefs.getString("cmask", "");
  c.host = prefs.getString("chost", "ioflex");
  prefs.end();
}
void clearTrying() {
  prefs.begin("net", false);
  prefs.putBool("trying", false);
  prefs.end();
  trying = false;
}

void applyMode(int i, uint8_t mode) {
  uint8_t p = PINS[i].pin;
  switch (mode) {
    case M_IN:     pinMode(p, INPUT); break;
    case M_INPULL: pinMode(p, INPUT_PULLUP); break;
    case M_OUT:    pinMode(p, OUTPUT); break;
    default: break;
  }
}
void loadIoConfig() {
  prefs.begin("ioflex", true);
  mbBase = prefs.getUShort("base", 0);
  for (int i = 0; i < NPINS; i++) {
    char k[8]; snprintf(k, sizeof(k), "m%d", i);
    cfg[i] = (uint8_t)prefs.getUShort(k, 0);
    lastCfg[i] = 0xFF;
  }
  prefs.end();
}
void savePinModes() {
  prefs.begin("ioflex", false);
  for (int i = 0; i < NPINS; i++) {
    char k[8]; snprintf(k, sizeof(k), "m%d", i);
    prefs.putUShort(k, cfg[i]);
  }
  prefs.end();
}
void saveMbBase(uint16_t base) {
  prefs.begin("ioflex", false);
  prefs.putUShort("base", base);
  prefs.end();
}

// Intenta conectar en STA con la config dada. Devuelve true si conecta.
bool connectWith(const NetCfg& c, uint32_t timeoutMs) {
  WiFi.disconnect(false, true);
  delay(120);
  if (c.stat && c.ip.length()) {
    IPAddress a, g, m;
    a.fromString(c.ip); g.fromString(c.gw); m.fromString(c.mask);
    WiFi.config(a, g, m);
  } else {
    WiFi.config(IPAddress((uint32_t)0), IPAddress((uint32_t)0), IPAddress((uint32_t)0)); // DHCP
  }
  String ss = c.ssid.length() ? c.ssid : String(DEF_SSID);
  String pp = c.ssid.length() ? c.pass : String(DEF_PASS);
  Serial.print("Probando STA -> "); Serial.println(ss);
  WiFi.begin(ss.c_str(), pp.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) { delay(300); Serial.print("."); }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

NetCfg cfgFromArgs() {
  NetCfg c;
  c.ssid = web.arg("ssid"); c.pass = web.arg("pass");
  c.stat = web.arg("mode") == "static";
  c.ip = web.arg("ip"); c.gw = web.arg("gw"); c.mask = web.arg("mask");
  c.host = web.arg("host");
  return c;
}

// ---------------- Portal web ----------------
const char PORTAL[] PROGMEM = R"HTML(<!doctype html><html lang="es"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IO Flex</title><style>
 :root{--bg:#0f1115;--card:#1a1e26;--fg:#e6e9ef;--mut:#8b94a7;--ac:#3b82f6;--ok:#22c55e;--line:#262b35;--off:#39404d}
 *{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,Segoe UI,Roboto,sans-serif;padding:14px}
 .w{max-width:580px;margin:0 auto}h1{font-size:18px;margin:2px 0}.m{color:var(--mut);font-size:12px;margin-bottom:12px}
 .tabs{display:flex;gap:8px;margin-bottom:12px}
 .tab{flex:1;padding:10px;text-align:center;border-radius:10px;background:var(--card);color:var(--mut);font-weight:700;font-size:14px;cursor:pointer;border:1px solid var(--line)}
 .tab.sel{background:var(--ac);color:#fff;border-color:var(--ac)}
 label{display:block;margin:10px 0 4px;font-size:13px;color:var(--mut)}
 input,select{width:100%;padding:10px;border-radius:8px;border:1px solid var(--line);background:#0f1115;color:var(--fg);font-size:15px}
 button{width:100%;margin-top:14px;padding:12px;border:0;border-radius:10px;background:var(--ac);color:#fff;font-size:15px;font-weight:700}
 .box{background:var(--card);border-radius:12px;padding:16px}
 table{width:100%;border-collapse:collapse;background:var(--card);border-radius:12px;overflow:hidden;margin-top:12px}
 th,td{padding:9px 7px;text-align:left;border-bottom:1px solid var(--line);font-size:14px}
 th{color:var(--mut);font-weight:600;font-size:12px}td.pin{font-weight:700;width:42px}td.reg{color:var(--mut);font-variant-numeric:tabular-nums}td select{padding:7px}
 .val{font-weight:700;font-size:16px}
 .tgl{width:auto;margin:0;border-radius:8px;padding:7px 12px;font-weight:700;color:#fff;background:var(--off);min-width:60px}
 .tgl.on{background:var(--ok)}
 .st{font-size:12px;color:var(--mut);text-align:center;margin-top:10px}.err{color:#f87171}.hide{display:none}
</style></head><body><div class="w">
 <h1>IO Flex</h1>
 <div class="m" id="stat">...</div>
 <div class="tabs">
  <div class="tab sel" id="tR" onclick="tab('R')">Red</div>
  <div class="tab" id="tP" onclick="tab('P')">Pines</div>
 </div>
 <div id="secR" class="box"><form method="POST" action="/save">
  <label>Nombre del nodo (mDNS)</label>
  <input name="host" id="host" placeholder="ioflex"><div class="m">Se accede como &lt;nombre&gt;.local</div>
  <label>Red WiFi (SSID)</label>
  <input name="ssid" id="ssid" list="nets" placeholder="Tu WiFi"><datalist id="nets"></datalist>
  <label>Clave</label><input name="pass" type="password" placeholder="Clave WiFi">
  <label>IP</label>
  <select name="mode" id="mode" onchange="tg()">
   <option value="dhcp">DHCP (automatica)</option>
   <option value="static">Estatica (fija)</option>
  </select>
  <div id="stbox" class="hide">
   <label>IP fija</label><input name="ip" id="ip" placeholder="192.168.2.50">
   <label>Gateway</label><input name="gw" id="gw" placeholder="192.168.2.1">
   <label>Mascara</label><input name="mask" id="mask" placeholder="255.255.255.0">
  </div>
  <div class="m">Antes de guardar, el nodo prueba la conexion. Si no conecta, no se queda con esta config.</div>
  <button type="submit">Probar y guardar</button>
 </form></div>
 <div id="secP" class="hide">
  <div class="box">
   <label>Direccion Modbus de inicio (valores)</label>
   <input id="base" type="number" min="0" max="65515" placeholder="0">
   <div class="m">Cada pin usa el registro (inicio + indice). Cambiarla reinicia el nodo.</div>
   <button onclick="saveBase()">Guardar direccion y reiniciar</button>
  </div>
  <table><thead><tr><th>Pin</th><th>Modo</th><th>Valor</th><th>Reg</th></tr></thead><tbody id="rows"></tbody></table>
  <button onclick="savePins()">Guardar modos en NVS</button>
  <div class="st" id="pst"></div>
 </div>
</div><script>
 const MODES={0:"Off",1:"Entrada",2:"Entrada+pullup",3:"Salida",4:"Analogica"};
 let editing=null,timer=null;
 function tab(t){
  tR.className='tab'+(t=='R'?' sel':'');tP.className='tab'+(t=='P'?' sel':'');
  secR.className=(t=='R'?'box':'hide');secP.className=(t=='P'?'':'hide');
  clearInterval(timer);
  if(t=='P'){loadPins();timer=setInterval(()=>{if(editing===null)loadPins()},1500)}
 }
 function tg(){stbox.className=mode.value=='static'?'':'hide'}
 function opts(sel){return Object.entries(MODES).map(([k,v])=>'<option value="'+k+'" '+(k==sel?'selected':'')+'>'+v+'</option>').join('')}
 fetch('/status').then(r=>r.json()).then(d=>{stat.textContent='Modo: '+d.mode+'  |  IP: '+d.ip+(d.ssid?('  |  '+d.ssid):'')+(d.host?('  |  '+d.host+'.local'):'');host.value=d.host||''});
 fetch('/scan').then(r=>r.json()).then(a=>{nets.innerHTML=a.map(s=>'<option value="'+s+'">').join('')}).catch(()=>{});
 async function loadPins(){
  try{const d=await(await fetch('/pins',{cache:'no-store'})).json();
   if(d.ap){rows.innerHTML='<tr><td colspan=4>En modo AP no hay pines. Configura la red primero.</td></tr>';pst.textContent='';return}
   if(!d.ok)throw 0;
   if(document.activeElement!==base)base.value=d.base;
   rows.innerHTML=d.pins.map(p=>{
    let vc='&ndash;';
    if(p.mode==3){const on=p.val!=0;vc='<button class="tgl'+(on?' on':'')+'" onclick="setval('+p.i+','+(on?0:1)+')">'+(on?'ON':'OFF')+'</button>'}
    else if(p.mode==1||p.mode==2||p.mode==4){vc='<span class="val">'+p.val+'</span>'}
    return '<tr><td class="pin">'+p.name+'</td><td><select onchange="setmode('+p.i+',this.value)" onfocus="editing='+p.i+'" onblur="editing=null">'+opts(p.mode)+'</select></td><td>'+vc+'</td><td class="reg">'+p.reg+'</td></tr>'
   }).join('');
   pst.textContent='en linea '+new Date().toLocaleTimeString();pst.className='st';
  }catch(e){pst.textContent='sin datos';pst.className='st err'}
 }
 async function setmode(i,m){await fetch('/pinmode?i='+i+'&m='+m,{cache:'no-store'});loadPins()}
 async function setval(i,v){await fetch('/pinval?i='+i+'&v='+v,{cache:'no-store'});loadPins()}
 async function savePins(){pst.textContent='guardando...';await fetch('/pinsave',{cache:'no-store'});pst.textContent='modos guardados en NVS'}
 async function saveBase(){const b=parseInt(base.value||'0');if(!confirm('Guardar inicio Modbus en '+b+' y reiniciar el nodo?'))return;await fetch('/mbbase?base='+b,{cache:'no-store'});pst.textContent='guardado, reiniciando...'}
</script></body></html>)HTML";

// Pagina de resultado tras guardar. Si pollForNode, espera a que el nodo vuelva,
// muestra su estado (en linea, IP, SSID) y habilita el boton "Volver al portal".
String pageResult(const String& intro, bool pollForNode) {
  String btn = "display:block;width:100%;margin-top:18px;padding:13px;border:0;border-radius:10px;background:#3b82f6;color:#fff;font-size:15px;font-weight:700;cursor:pointer";
  String head = "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<body style='background:#0f1115;color:#e6e9ef;font-family:system-ui,sans-serif;padding:24px;max-width:480px;margin:0 auto'>"
    "<h2>IO Flex</h2><p style='font-size:16px;line-height:1.5'>" + intro + "</p>";
  if (!pollForNode) {
    return head + "<button onclick=\"location.href='/'\" style='" + btn + "'>Volver al portal</button></body>";
  }
  return head +
    "<div id=r style='margin-top:12px;color:#8b94a7'>Esperando a que el nodo vuelva a estar en linea...</div>"
    "<button id=b style='" + btn + ";display:none'>Volver al portal</button>"
    "<script>var n=0;function chk(){n++;"
    "fetch('/status',{cache:'no-store'}).then(r=>r.json()).then(d=>{"
    "document.getElementById('r').innerHTML='<b style=\"color:#22c55e\">&#10003; Nodo en linea</b><br>Modo: '+d.mode+'<br>IP: '+d.ip+'<br>SSID: '+d.ssid+'<br>Nombre: '+d.host+'.local';"
    "var b=document.getElementById('b');b.style.display='block';b.onclick=function(){location.href='/'};"
    "}).catch(e=>{if(n<40){setTimeout(chk,2000)}else{"
    "document.getElementById('r').innerHTML='<b style=\"color:#f87171\">No respondio en esta direccion.</b><br>Pudo cambiar de IP: abre <a style=\"color:#3b82f6\" href=\"http://ioflex.local/\">ioflex.local</a>, o revisa si quedo en modo AP (IOFlex-Setup).';"
    "var b=document.getElementById('b');b.style.display='block';b.onclick=function(){location.href='http://ioflex.local/'}}});}"
    "setTimeout(chk,8000);</script></body>";
}

void handleRoot()   { web.send_P(200, "text/html", PORTAL); }
void handleStatus() {
  String ip = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  String m  = apMode ? "AP (configuracion)" : "STA (conectado)";
  web.send(200, "application/json",
    "{\"mode\":\"" + m + "\",\"ip\":\"" + ip + "\",\"ssid\":\"" + (apMode?String(""):WiFi.SSID()) +
    "\",\"host\":\"" + good.host + "\"}");
}
void handleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; i++) { if (i) j += ","; j += "\"" + WiFi.SSID(i) + "\""; }
  j += "]";
  web.send(200, "application/json", j);
}

// Guardado de red a prueba de fallos
void handleSave() {
  NetCfg c = cfgFromArgs();
  if (apMode) {
    // Desde el AP: probar en AP+STA sin perder el AP; guardar solo si conecta.
    WiFi.mode(WIFI_AP_STA);
    bool ok = connectWith(c, 13000);
    if (ok) {
      saveGood(c);
      web.send(200, "text/html", pageResult("Conecto correctamente. Configuracion guardada. Reiniciando en modo normal...", true));
      delay(900); ESP.restart();
    } else {
      WiFi.disconnect(false, true);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID, AP_PASS);
      web.send(200, "text/html", pageResult("No se pudo conectar (revisa la clave o el SSID). <b>No se guardo</b>; sigues en el portal de configuracion. Vuelve a intentarlo.", false));
    }
  } else {
    // Desde la red (STA): guardar candidato y reiniciar; rollback si no conecta.
    saveCandidate(c);
    web.send(200, "text/html", pageResult("Probando la nueva red. El nodo se reinicia: si conecta, queda con la nueva config; si <b>no</b> conecta, vuelve sola a la configuracion actual.", true));
    delay(900); ESP.restart();
  }
}

// --- Pines (solo STA) ---
void handlePins() {
  if (apMode) { web.send(200, "application/json", "{\"ok\":false,\"ap\":true}"); return; }
  String j = "{\"ok\":true,\"base\":" + String(mbBase) + ",\"pins\":[";
  for (int i = 0; i < NPINS; i++) {
    if (i) j += ",";
    j += "{\"i\":" + String(i) + ",\"name\":\"" + pinName(i) + "\",\"mode\":" +
         String(cfg[i]) + ",\"val\":" + String(mb.Hreg(mbBase + i)) +
         ",\"reg\":" + String(mbBase + i) + "}";
  }
  j += "]}";
  web.send(200, "application/json", j);
}
void handlePinMode() {
  int i = web.arg("i").toInt(), m = web.arg("m").toInt();
  if (i >= 0 && i < NPINS && m >= 0 && m <= 4) { cfg[i] = (uint8_t)m; applyMode(i, cfg[i]); lastCfg[i] = cfg[i]; }
  web.send(200, "application/json", "{\"ok\":true}");
}
void handlePinVal() {
  int i = web.arg("i").toInt(), v = web.arg("v").toInt();
  if (i >= 0 && i < NPINS) mb.Hreg(mbBase + i, (uint16_t)v);
  web.send(200, "application/json", "{\"ok\":true}");
}
void handlePinSave() {
  if (!apMode) savePinModes();
  web.send(200, "application/json", "{\"ok\":true}");
}
void handleMbBase() {
  long b = web.arg("base").toInt();
  if (b < 0) b = 0; if (b > 65515) b = 65515;
  saveMbBase((uint16_t)b);
  web.send(200, "application/json", "{\"ok\":true}");
  delay(600);
  ESP.restart();
}

void startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP "); Serial.print(AP_SSID);
  Serial.print("  IP "); Serial.println(WiFi.softAPIP());
}
void startModbusMdns() {
  mb.server();
  for (int i = 0; i < NPINS; i++) mb.addHreg(mbBase + i, 0);
  for (int i = 0; i < NPINS; i++) { applyMode(i, cfg[i]); lastCfg[i] = cfg[i]; }
  Serial.print("IO Flex Modbus TCP listo (502). Valores en registros ");
  Serial.print(mbBase); Serial.print(".."); Serial.println(mbBase + NPINS - 1);
  if (MDNS.begin(good.host.c_str())) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("modbus", "tcp", 502);
    Serial.print("mDNS: "); Serial.print(good.host); Serial.println(".local");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  loadNet();
  loadIoConfig();

  WiFi.mode(WIFI_STA);
  bool connected = false;

  // 1) Si hay un candidato por probar (guardado desde STA), probarlo
  if (trying) {
    NetCfg c; loadCandidate(c);
    Serial.println("Hay candidato de red -> probando...");
    if (connectWith(c, 15000)) {
      saveGood(c);              // promueve y apaga trying
      connected = true;
      Serial.println("Candidato OK -> promovido a configuracion buena");
    } else {
      clearTrying();            // descarta candidato, conserva la buena
      Serial.println("Candidato FALLO -> revirtiendo a la configuracion anterior");
    }
  }

  // 2) Conectar con la configuracion buena
  if (!connected) {
    if (connectWith(good, 15000)) connected = true;
  }

  if (connected) {
    Serial.print("STA IP: "); Serial.println(WiFi.localIP());
    startModbusMdns();
  } else {
    startAP();
  }

  web.on("/", handleRoot);
  web.on("/status", handleStatus);
  web.on("/scan", handleScan);
  web.on("/save", HTTP_POST, handleSave);
  web.on("/pins", handlePins);
  web.on("/pinmode", handlePinMode);
  web.on("/pinval", handlePinVal);
  web.on("/pinsave", handlePinSave);
  web.on("/mbbase", handleMbBase);
  web.begin();
  Serial.println("Portal web listo (:80) - Red + Pines");
}

unsigned long lastBeat = 0;
void loop() {
  web.handleClient();

  if (!apMode) {
    mb.task();
    for (int i = 0; i < NPINS; i++) {
      if (cfg[i] != lastCfg[i]) { applyMode(i, cfg[i]); lastCfg[i] = cfg[i]; }
      uint8_t p = PINS[i].pin;
      switch (cfg[i]) {
        case M_IN:
        case M_INPULL: mb.Hreg(mbBase + i, digitalRead(p) ? 1 : 0); break;
        case M_OUT:    digitalWrite(p, mb.Hreg(mbBase + i) ? HIGH : LOW); break;
        case M_ADC:    if (PINS[i].adc) mb.Hreg(mbBase + i, analogRead(p)); break;
        default: break;
      }
    }
  }

  if (millis() - lastBeat > 5000) {
    lastBeat = millis();
    Serial.print(apMode ? "[AP] " : "[STA] ");
    Serial.println(apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
  }
  yield();
}
