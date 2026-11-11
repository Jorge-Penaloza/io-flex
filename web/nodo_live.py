# -*- coding: utf-8 -*-
# Web live del nodo Modbus TCP (Arduino Nano ESP32)
# Proyecto: Distribucion Linux / banco de pruebas OT - Jorge Peñaloza
# Sirve una pagina (apta celular) con los valores del nodo y botones del LED.
# Escucha en 0.0.0.0:8080 (accesible por LAN y por Tailscale).

import socket, struct, json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

NODE_IP = "192.168.2.82"
NODE_PORT = 502
LISTEN_PORT = 8080

def _tx(req):
    s = socket.create_connection((NODE_IP, NODE_PORT), timeout=3)
    try:
        s.sendall(req)
        return s.recv(260)
    finally:
        s.close()

def read_hregs(start, qty):
    r = _tx(struct.pack('>HHHBBHH', 1, 0, 6, 1, 3, start, qty))
    return [(r[9 + i*2] << 8) | r[10 + i*2] for i in range(qty)]

def read_coil(addr):
    r = _tx(struct.pack('>HHHBBHH', 1, 0, 6, 1, 1, addr, 1))
    return r[9] & 1

def write_coil(addr, on):
    _tx(struct.pack('>HHHBBHH', 2, 0, 6, 1, 5, addr, 0xFF00 if on else 0x0000))

PAGE = """<!doctype html><html lang="es"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nodo Modbus TCP</title>
<style>
 :root{--bg:#0f1115;--card:#1a1e26;--fg:#e6e9ef;--mut:#8b94a7;--ac:#3b82f6;--ok:#22c55e;--off:#39404d}
 *{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);
  font-family:system-ui,Segoe UI,Roboto,sans-serif;padding:16px}
 .wrap{max-width:460px;margin:0 auto}
 h1{font-size:18px;margin:4px 0 2px}.sub{color:var(--mut);font-size:13px;margin-bottom:16px}
 .card{background:var(--card);border-radius:14px;padding:18px;margin-bottom:14px}
 .lbl{color:var(--mut);font-size:13px}.val{font-size:40px;font-weight:700;letter-spacing:.5px}
 .row{display:flex;gap:12px}.row .card{flex:1;text-align:center}
 .led{display:inline-block;width:14px;height:14px;border-radius:50%;background:var(--off);vertical-align:middle;margin-right:8px}
 .led.on{background:var(--ok);box-shadow:0 0 10px var(--ok)}
 .btns{display:flex;gap:10px;margin-top:10px}
 button{flex:1;padding:14px;border:0;border-radius:10px;font-size:15px;font-weight:600;color:#fff;background:var(--ac)}
 button.off{background:var(--off)}
 .st{font-size:12px;color:var(--mut);text-align:center;margin-top:8px}
 .err{color:#f87171}
</style></head><body><div class="wrap">
 <h1>Nodo Modbus TCP</h1>
 <div class="sub">Arduino Nano ESP32 · 192.168.2.82:502 · Jorge Peñaloza</div>
 <div class="row">
  <div class="card"><div class="lbl">Contador (Hreg0)</div><div class="val" id="cont">--</div></div>
  <div class="card"><div class="lbl">ADC A0 (Hreg1)</div><div class="val" id="adc">--</div></div>
 </div>
 <div class="card">
  <div class="lbl"><span class="led" id="dot"></span>LED (Coil0): <b id="ledtxt">--</b></div>
  <div class="btns">
   <button onclick="setLed(1)">Encender</button>
   <button class="off" onclick="setLed(0)">Apagar</button>
  </div>
 </div>
 <div class="st" id="st">conectando...</div>
</div><script>
 async function tick(){
  try{
   const r=await fetch('/api',{cache:'no-store'});const d=await r.json();
   if(!d.ok){throw new Error(d.error||'error')}
   cont.textContent=d.contador;adc.textContent=d.adc;
   const on=d.led==1;dot.className='led'+(on?' on':'');ledtxt.textContent=on?'ON':'OFF';
   st.textContent='en linea · '+new Date().toLocaleTimeString();st.className='st';
  }catch(e){st.textContent='sin conexion con el nodo';st.className='st err'}
 }
 async function setLed(v){try{await fetch('/led?v='+v,{cache:'no-store'});tick()}catch(e){}}
 tick();setInterval(tick,1000);
</script></body></html>"""

class H(BaseHTTPRequestHandler):
    def _s(self, code, ctype, body):
        self.send_response(code); self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store'); self.end_headers()
        self.wfile.write(body)
    def do_GET(self):
        u = urlparse(self.path)
        try:
            if u.path == '/':
                self._s(200, 'text/html; charset=utf-8', PAGE.encode('utf-8'))
            elif u.path == '/api':
                hr = read_hregs(0, 2); led = read_coil(0)
                self._s(200, 'application/json',
                        json.dumps({'ok': True, 'contador': hr[0], 'adc': hr[1], 'led': led}).encode())
            elif u.path == '/led':
                on = parse_qs(u.query).get('v', ['0'])[0] == '1'
                write_coil(0, on)
                self._s(200, 'application/json', json.dumps({'ok': True, 'led': 1 if on else 0}).encode())
            else:
                self._s(404, 'text/plain', b'not found')
        except Exception as e:
            self._s(200, 'application/json', json.dumps({'ok': False, 'error': str(e)}).encode())
    def log_message(self, *a):
        pass

if __name__ == '__main__':
    print('Nodo live en http://0.0.0.0:%d' % LISTEN_PORT)
    ThreadingHTTPServer(('0.0.0.0', LISTEN_PORT), H).serve_forever()
