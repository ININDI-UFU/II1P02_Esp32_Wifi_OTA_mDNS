#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>          // NOVO em relação ao main2: servidor web
#include "services\wserial.h"

const char *ssid = "InovaIndustria";
const char *password = "industria50";
const char *hostName = KIT_HOSTNAME;

void receivedFunc(std::string str){
  wserial.println(str.c_str()+std::string("\n"));
}

// ============================================================
// NOVO em relação ao main2: osciloscópio via navegador
// A mesma "reta" e o mesmo "seno" que vão pela wserial.plot()
// também são guardados aqui e servidos por HTTP para o browser
// desenhar, além de poderem ter frequência/amplitude ajustadas
// pela própria página.
// ============================================================
WebServer server(80);

float freqSeno = 1.0f;   // multiplica a velocidade do seno (padrão = igual ao main2)
float ampSeno  = 1.0f;   // multiplica a amplitude do seno
float freqReta = 1.0f;   // multiplica a velocidade da reta
float ampReta  = 1.0f;   // multiplica a amplitude da reta

static const int N_PONTOS = 120;   // quantidade de pontos mantidos para desenhar
float retaBuf[N_PONTOS] = {0};
float senoBuf[N_PONTOS] = {0};

void adiciona(float *buf, float valor) {
  for (int i = 0; i < N_PONTOS - 1; i++) buf[i] = buf[i + 1];
  buf[N_PONTOS - 1] = valor;
}

const char PAGINA_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang='pt-br'>
<head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>ESP32</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:Arial,Helvetica,sans-serif;background:#101418;color:#eef3f7}
main{min-height:100vh;padding:24px;display:flex;align-items:center;justify-content:center}
.painel{width:min(920px,100%);background:#171d23;border:1px solid #2c3844;border-radius:8px;padding:22px;box-shadow:0 18px 50px #0008}
.topo{display:flex;gap:16px;align-items:flex-start;justify-content:space-between;margin-bottom:18px;flex-wrap:wrap}
h1{font-size:28px;margin:0 0 8px;color:#ffffff}
p{margin:0;color:#aebbc7;line-height:1.45}
.status{display:flex;gap:10px;align-items:center;color:#9fffc2;background:#102117;border:1px solid #245b38;border-radius:6px;padding:10px 12px;font-weight:bold}
.led{width:10px;height:10px;border-radius:50%;background:#35ff78;box-shadow:0 0 16px #35ff78}
.scope{background:#050806;border:1px solid #35533d;border-radius:8px;padding:12px}
canvas{width:100%;height:360px;display:block;background:#061008;border-radius:4px}
.controles{display:grid;grid-template-columns:repeat(2,1fr);gap:14px;margin-top:14px}
.ctrl{background:#101820;border:1px solid #293946;border-radius:6px;padding:12px}
.ctrl h3{margin:0 0 8px;font-size:14px;color:#8da1b2}
.ctrl label{display:flex;justify-content:space-between;font-size:13px;color:#aebbc7;margin-top:8px}
.ctrl input[type=range]{width:100%}
.rodape{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-top:14px}
.medida{background:#101820;border:1px solid #293946;border-radius:6px;padding:10px}
.rotulo{display:block;color:#8da1b2;font-size:12px;margin-bottom:4px}
.valor{font-size:18px;color:#f5fbff;font-weight:bold}
@media(max-width:640px){main{padding:12px}.painel{padding:16px}h1{font-size:22px}canvas{height:260px}.controles{grid-template-columns:1fr}.rodape{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<main>
<section class='painel'>
<div class='topo'>
<div>
<h1>ESP32 Osciloscopio</h1>
<p>Mesma reta e seno enviados pela wserial.plot(), agora tambem no navegador.</p>
</div>
<div class='status'><span class='led'></span>ONLINE</div>
</div>
<div class='scope'>
<canvas id='scope' width='900' height='360'></canvas>
</div>
<div class='controles'>
<div class='ctrl'>
<h3>Seno</h3>
<label>Frequencia <span id='vFreqSeno'>1.00</span></label>
<input type='range' id='freqSeno' min='0.1' max='5' step='0.1' value='1'>
<label>Amplitude <span id='vAmpSeno'>1.00</span></label>
<input type='range' id='ampSeno' min='0' max='5' step='0.1' value='1'>
</div>
<div class='ctrl'>
<h3>Reta</h3>
<label>Frequencia <span id='vFreqReta'>1.00</span></label>
<input type='range' id='freqReta' min='0' max='5' step='0.1' value='1'>
<label>Amplitude <span id='vAmpReta'>1.00</span></label>
<input type='range' id='ampReta' min='0' max='3' step='0.1' value='1'>
</div>
</div>
<div class='rodape'>
<div class='medida'><span class='rotulo'>Seno atual</span><span class='valor' id='rSeno'>0.00</span></div>
<div class='medida'><span class='rotulo'>Reta atual</span><span class='valor' id='rReta'>0.00</span></div>
</div>
</section>
</main>
<script>
const canvas=document.getElementById('scope');
const ctx=canvas.getContext('2d');
let dadosReta=[], dadosSeno=[];

function grade(){
ctx.clearRect(0,0,canvas.width,canvas.height);
ctx.fillStyle='#061008';ctx.fillRect(0,0,canvas.width,canvas.height);
ctx.strokeStyle='#16361f';ctx.lineWidth=1;
for(let x=0;x<=canvas.width;x+=45){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,canvas.height);ctx.stroke();}
for(let y=0;y<=canvas.height;y+=36){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(canvas.width,y);ctx.stroke();}
ctx.strokeStyle='#2d6b3c';ctx.lineWidth=2;
ctx.beginPath();ctx.moveTo(0,canvas.height/2);ctx.lineTo(canvas.width,canvas.height/2);ctx.stroke();
ctx.beginPath();ctx.moveTo(canvas.width/2,0);ctx.lineTo(canvas.width/2,canvas.height);ctx.stroke();
}

function desenhaCurva(dados,cor){
if(dados.length<2) return;
let minY=Math.min(...dados), maxY=Math.max(...dados);
if(maxY-minY<0.001){minY-=1;maxY+=1;}
let margem=(maxY-minY)*0.12;
minY-=margem; maxY+=margem;
ctx.strokeStyle=cor; ctx.lineWidth=2.5; ctx.shadowColor=cor; ctx.shadowBlur=8;
ctx.beginPath();
for(let i=0;i<dados.length;i++){
let x = i/(dados.length-1)*canvas.width;
let y = canvas.height - ((dados[i]-minY)/(maxY-minY))*canvas.height;
if(i==0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
}
ctx.stroke();ctx.shadowBlur=0;
}

function texto(){
ctx.fillStyle='#bfffd0';ctx.font='16px Arial';
ctx.fillText('CH1 Seno',18,24);
ctx.fillStyle='#ffe9ae';
ctx.fillText('CH2 Reta',18,44);
}

function desenha(){
grade();
desenhaCurva(dadosReta,'#ffd166');
desenhaCurva(dadosSeno,'#60ff8b');
texto();
requestAnimationFrame(desenha);
}
desenha();

async function buscaDados(){
try{
const r = await fetch('/data');
const j = await r.json();
dadosReta = j.reta;
dadosSeno = j.seno;
document.getElementById('rSeno').textContent = dadosSeno[dadosSeno.length-1].toFixed(3);
document.getElementById('rReta').textContent = dadosReta[dadosReta.length-1].toFixed(3);
}catch(e){}
setTimeout(buscaDados,150);
}
buscaDados();

function debounce(fn,espera){
let t;
return (...args)=>{clearTimeout(t);t=setTimeout(()=>fn(...args),espera);};
}

function ligaControle(id,spanId,casas,param){
const el=document.getElementById(id);
const sp=document.getElementById(spanId);
const envia=debounce((v)=>{fetch('/set?'+param+'='+v);},120);
el.addEventListener('input',()=>{
sp.textContent=parseFloat(el.value).toFixed(casas);
envia(el.value);
});
}
ligaControle('freqSeno','vFreqSeno',2,'freqSeno');
ligaControle('ampSeno','vAmpSeno',2,'ampSeno');
ligaControle('freqReta','vFreqReta',2,'freqReta');
ligaControle('ampReta','vAmpReta',2,'ampReta');
</script>
</body>
</html>
)rawliteral";

void paginaInicial() {
  server.send(200, "text/html", PAGINA_HTML);
}

void paginaDados() {
  String json = "{\"reta\":[";
  for (int i = 0; i < N_PONTOS; i++) { if (i) json += ","; json += String(retaBuf[i], 4); }
  json += "],\"seno\":[";
  for (int i = 0; i < N_PONTOS; i++) { if (i) json += ","; json += String(senoBuf[i], 4); }
  json += "]}";
  server.send(200, "application/json", json);
}

void paginaSet() {
  if (server.hasArg("freqSeno")) freqSeno = constrain(server.arg("freqSeno").toFloat(), 0.0f, 10.0f);
  if (server.hasArg("ampSeno"))  ampSeno  = constrain(server.arg("ampSeno").toFloat(), 0.0f, 10.0f);
  if (server.hasArg("freqReta")) freqReta = constrain(server.arg("freqReta").toFloat(), 0.0f, 10.0f);
  if (server.hasArg("ampReta"))  ampReta  = constrain(server.arg("ampReta").toFloat(), 0.0f, 10.0f);
  server.send(200, "text/plain", "OK");
}
// ====================== fim da parte NOVA ======================

void setup() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  WiFi.setHostname(hostName);

  // Tenta listen até conseguir
  wserial.begin(115200, 47268UL);
  wserial.onInputReceived([](std::string str){ wserial.println((str+'\n').c_str()); });
  wserial.println("[IP] is " + String(WiFi.localIP().toString()));

  if (!MDNS.begin(hostName)) wserial.println("[mDNS] begin failed");
  else wserial.println("[mDNS] begin in " + String(hostName));

  ArduinoOTA
      .onStart([]() {wserial.println("[OTA] Start");})
      .onEnd([]() {wserial.println("[OTA] End"); })
      .onProgress([](unsigned int p, unsigned int t) {wserial.println("[OTA] " + String((p*100)/t));})
      .onError([](ota_error_t e) { wserial.println("[OTA] Error " + String(e)); })
      .setHostname(hostName)
      .begin();

  // NOVO em relação ao main2: sobe o servidor web com o osciloscópio
  server.on("/", paginaInicial);
  server.on("/data", paginaDados);
  server.on("/set", paginaSet);
  server.begin();
  wserial.println("[HTTP] acesse http://" + String(hostName) + ".local/");
}

void loop() {
  ArduinoOTA.handle();
  wserial.update();
  server.handleClient();       // NOVO em relação ao main2
  
  uint32_t now = millis();

  static float t_reta = 0.0f;            // variável de tempo para a reta
  static uint32_t t1 = 0;
  if (now - t1 > 200) {
    t1 = now;
    float valorReta = ampReta * 10 * t_reta;
    wserial.plot("reta", valorReta);
    adiciona(retaBuf, valorReta);        // NOVO: guarda para o navegador
    t_reta += 0.2f * freqReta;           // incrementa o tempo (ajustável pela página)
  }

  static float t_seno = 0.0f;            // variável de tempo para o seno
  static uint32_t t2 = 0;
  if (now - t2 > 100) {
    t2 = now;
    float valorSeno = ampSeno * sin(t_seno);
    wserial.plot("seno", valorSeno);     // envia para o gráfico
    adiciona(senoBuf, valorSeno);        // NOVO: guarda para o navegador
    t_seno += 0.2f * freqSeno;           // incrementa o tempo (ajustável pela página)
    if (t_seno > 2 * M_PI) t_seno = 0;   // reinicia o ciclo a cada 2π
  }
}
