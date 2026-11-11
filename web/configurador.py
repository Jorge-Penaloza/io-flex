# -*- coding: utf-8 -*-
# Web configuradora del nodo IO Flex (Arduino Nano ESP32) por Modbus TCP.
# Cada pin: elegir modo (off/entrada/pullup/salida/analogica), leer/escribir valor,
# y guardar la config en NVS. Jorge Peñaloza.
# Escucha en 0.0.0.0:8080 (publicada por Tailscale serve).

import socket, struct, json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

NODE_IP = "192.168.2.220"
NODE_PORT = 502
LISTEN_PORT = 8080

CMD_REG = 10
CFG_BASE = 100
VAL_BASE = 200
PIN_NAMES = ["D2","D3","D4","D5","D6","D7","D8","D9","D10","D11","D12","D13",
             "A0","A1","A2","A3","A4","A5","A6","A7"]
NPINS = len(PIN_NAMES)

def _tx(req):
    s = socket.create_connection((NODE_IP, NODE_PORT), timeout=3)
    try:
        s.sendall(req); return s.recv(300)
    finally:
        s.close()

def read_hregs(start, qty):
    r = _tx(struct.pack('>HHHBBHH', 1, 0, 6, 1, 3, start, qty))
    return [(r[9 + i*2] << 8) | r[10 + i*2] for i in range(qty)]

def write_hreg(addr, val):
    _tx(struct.pack('>HHHBBHH', 1, 0, 6, 1, 6, addr, val & 0xFFFF))

def snapshot():
    modes = read_hregs(CFG_BASE, NPINS)
    vals  = read_hregs(VAL_BASE, NPINS)
    return [{"i": i, "name": PIN_NAMES[i], "mode": modes[i], "val": vals[i]} for i in range(NPINS)]

PAGE = """<!doctype html><html lang="es"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IO Flex - Config</title>
<style>
 :root{--bg:#0f1115;--card:#1a1e26;--fg:#e6e9ef;--mut:#8b94a7;--ac:#3b82f6;--ok:#22c55e;--line:#262b35}
 *{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);
  font-family:system-ui,Segoe UI,Roboto,sans-serif;padding:14px}
 .wrap{max-width:560px;margin:0 auto}
 h1{font-size:18px;margin:2px 0}.sub{color:var(--mut);font-size:12px;margin-bottom:14px}
 table{width:100%;border-collapse:collapse;background:var(--card);border-radius:12px;overflow:hidden}
 th,td{padding:10px 8px;text-align:left;border-bottom:1px solid var(--line);font-size:14px}
 th{color:var(--mut);font-weight:600;font-size:12px}
 td.pin{font-weight:700;width:48px}
 select{background:#0f1115;color:var(--fg);border:1px solid var(--line);border-radius:8px;padding:7px;font-size:14px;width:100%}
 .val{font-weight:700;font-size:16px}
 .tgl{border:0;border-radius:8px;padding:8px 12px;font-weight:700;color:#fff;background:#39404d;min-width:64px}
 .tgl.on{background:var(--ok)}
 .bar{display:flex;gap:10px;margin:14px 0}
 .btn{flex:1;padding:13px;border:0;border-radius:10px;font-size:15px;font-weight:700;color:#fff;background:var(--ac)}
 .st{font-size:12px;color:var(--mut);text-align:center;margin-top:8px}
 .err{color:#f87171}
</style></head><body><div class="wrap">
 <h1>IO Flex &mdash; Configurador</h1>
 <div class="sub">Nano ESP32 &middot; 192.168.2.220:502 &middot; Jorge Peñaloza</div>
 <table><thead><tr><th>Pin</th><th>Modo</th><th>Valor</th></tr></thead><tbody id="rows"></tbody></table>
 <div class="bar"><button class="btn" onclick="save()">Guardar config en el nodo (NVS)</button></div>
 <div class="st" id="st">conectando...</div>
</div><script>
 const MODES={0:"Off",1:"Entrada",2:"Entrada+pullup",3:"Salida",4:"Analogica"};
 let editing=null;
 function opts(sel){return Object.entries(MODES).map(([k,v])=>`<option value="${k}" ${k==sel?'selected':''}>${v}</option>`).join('')}
 async function load(){
  try{
   const d=await (await fetch('/api',{cache:'no-store'})).json();
   if(!d.ok)throw 0;
   rows.innerHTML=d.pins.map(p=>{
    let valcell='&ndash;';
    if(p.mode==3){const on=p.val!=0;valcell=`<button class="tgl ${on?'on':''}" onclick="setval(${p.i},${on?0:1})">${on?'ON':'OFF'}</button>`}
    else if(p.mode==1||p.mode==2){valcell=`<span class="val">${p.val}</span>`}
    else if(p.mode==4){valcell=`<span class="val">${p.val}</span>`}
    return `<tr><td class="pin">${p.name}</td>
      <td><select onchange="setmode(${p.i},this.value)" onfocus="editing=${p.i}" onblur="editing=null">${opts(p.mode)}</select></td>
      <td>${valcell}</td></tr>`
   }).join('');
   st.textContent='en linea &middot; '+new Date().toLocaleTimeString();st.className='st';
  }catch(e){st.textContent='sin conexion con el nodo';st.className='st err'}
 }
 async function setmode(i,m){await fetch('/setmode?i='+i+'&m='+m,{cache:'no-store'});load()}
 async function setval(i,v){await fetch('/setval?i='+i+'&v='+v,{cache:'no-store'});load()}
 async function save(){st.textContent='guardando...';await fetch('/save',{cache:'no-store'});st.textContent='config guardada en NVS'}
 load();setInterval(()=>{if(editing===null)load()},1500);
</script></body></html>"""

class H(BaseHTTPRequestHandler):
    def _s(self, code, ctype, body):
        self.send_response(code); self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store'); self.end_headers()
        self.wfile.write(body)
    def _json(self, obj): self._s(200, 'application/json', json.dumps(obj).encode())
    def do_GET(self):
        u = urlparse(self.path); q = parse_qs(u.query)
        try:
            if u.path == '/':
                self._s(200, 'text/html; charset=utf-8', PAGE.encode('utf-8'))
            elif u.path == '/api':
                self._json({"ok": True, "pins": snapshot()})
            elif u.path == '/setmode':
                i = int(q['i'][0]); m = int(q['m'][0]); write_hreg(CFG_BASE + i, m); self._json({"ok": True})
            elif u.path == '/setval':
                i = int(q['i'][0]); v = int(q['v'][0]); write_hreg(VAL_BASE + i, v); self._json({"ok": True})
            elif u.path == '/save':
                write_hreg(CMD_REG, 1); self._json({"ok": True})
            else:
                self._s(404, 'text/plain', b'not found')
        except Exception as e:
            self._json({"ok": False, "error": str(e)})
    def log_message(self, *a): pass

if __name__ == '__main__':
    print('IO Flex configurador en http://0.0.0.0:%d' % LISTEN_PORT)
    ThreadingHTTPServer(('0.0.0.0', LISTEN_PORT), H).serve_forever()
