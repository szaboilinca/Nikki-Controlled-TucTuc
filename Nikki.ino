#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// 1. CONFIGURARE RETEA SI PINI HARDWARE
// ==========================================
const char* NUME_WIFI   = "Nikki";
const char* PAROLA_WIFI = "cacamaca";

const int PIN_SERVO = 3;
const int PIN_IN1   = 10;
const int PIN_IN2   = 7;
const int PIN_IN3   = 6;
const int PIN_IN4   = 5;

// Pinii LED-urilor de pe PCB
const int PIN_LED_ROSU   = 2;
const int PIN_LED_GALBEN = 1;
const int PIN_LED_VERDE  = 0;

// Limite servo pentru +-55 grade (centru = 1500 us, +-611 us)
const int SERVO_DREAPTA_MAX = 889;   // -55 grade
const int SERVO_CENTRU      = 1500;  //   0 grade
const int SERVO_STANGA_MAX  = 2111;  // +55 grade

WebServer server(80);

// ==========================================
// 2. VARIABILE DE STARE SI SIGURANTA
// ==========================================
int stareMotor = 0;
int pozitieServo = SERVO_CENTRU;

unsigned long timpUltimaComanda = 0;
const unsigned long TIMP_OPRIRE_AUTOMATA = 2500; // 2.5 secunde failsafe

// ==========================================
// 3. FUNCTII CONTROL MOTOR, SERVO SI LED-URI
// ==========================================
void seteazaLeduri(bool rosu, bool galben, bool verde) {
  digitalWrite(PIN_LED_ROSU,   rosu   ? HIGH : LOW);
  digitalWrite(PIN_LED_GALBEN, galben ? HIGH : LOW);
  digitalWrite(PIN_LED_VERDE,  verde  ? HIGH : LOW);
}

void controleazaMotor(int directie) {
  directie = constrain(directie, -100, 100);
  stareMotor = directie;

  if (directie > 0) {
    // Mers inainte la putere maxima (255) -> LED VERDE aprins
    ledcWrite(PIN_IN1, 255);
    ledcWrite(PIN_IN3, 255);
    ledcWrite(PIN_IN2, 0);
    ledcWrite(PIN_IN4, 0);
    seteazaLeduri(false, false, true);
  } 
  else if (directie < 0) {
    // Mers inapoi la putere maxima (255) -> LED GALBEN aprins
    ledcWrite(PIN_IN1, 0);
    ledcWrite(PIN_IN3, 0);
    ledcWrite(PIN_IN2, 255);
    ledcWrite(PIN_IN4, 255);
    seteazaLeduri(false, true, false);
  } 
  else {
    // Oprire motor -> LED ROSU aprins
    ledcWrite(PIN_IN1, 0);
    ledcWrite(PIN_IN3, 0);
    ledcWrite(PIN_IN2, 0);
    ledcWrite(PIN_IN4, 0);
    seteazaLeduri(true, false, false);
  }
}

void controleazaServo(int microsecunde) {
  // Limitare la +-55 grade (889 .. 2111 us)
  microsecunde = constrain(microsecunde, SERVO_DREAPTA_MAX, SERVO_STANGA_MAX);
  pozitieServo = microsecunde;

  // Conversie din microsecunde in semnal PWM pe 14 biti (la 50 Hz)
  int valoarePWM = (microsecunde * 16383L) / 20000L;
  ledcWrite(PIN_SERVO, valoarePWM);
}

// ==========================================
// 4. PAGINA WEB SI RUTELE SERVERULUI
// ==========================================
String genereazaPaginaHTML() {
  String pagina = "<!DOCTYPE html><html lang='ro'><head>";
  pagina += "<meta charset='UTF-8'>";
  pagina += "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>";
  pagina += "<title>Nikki RC Cockpit</title>";

  // --- STILURI VIZUALE (CSS) ---
  pagina += "<style>";
  pagina += ":root { --bg: #0b0d14; --panel: rgba(22, 26, 38, 0.85); --border: rgba(255, 255, 255, 0.12); --accent: #00f2fe; --green: #00e676; --red: #ff3b5c; --text: #eef2f8; --muted: #8a94a6; }";
  pagina += "* { box-sizing: border-box; user-select: none; -webkit-user-select: none; touch-action: none; }";
  pagina += "body { margin: 0; background: radial-gradient(circle at top, #161b2e 0%, var(--bg) 75%); color: var(--text); font-family: system-ui, sans-serif; height: 100vh; overflow: hidden; display: flex; flex-direction: column; }";

  // Ecran initial de bun venit
  pagina += "#ecran-start { position: fixed; inset: 0; background: rgba(9, 11, 18, 0.96); backdrop-filter: blur(12px); z-index: 999; display: flex; flex-direction: column; align-items: center; justify-content: center; padding: 20px; text-align: center; }";
  pagina += ".titlu-start { font-size: 28px; font-weight: 800; letter-spacing: 1px; margin-bottom: 8px; background: linear-gradient(90deg, #00f2fe, #4facfe); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }";
  pagina += ".subtitlu-start { color: var(--muted); font-size: 15px; margin-bottom: 30px; }";
  pagina += ".carduri-mod { display: flex; gap: 20px; flex-wrap: wrap; justify-content: center; }";
  pagina += ".card-mod { background: var(--panel); border: 2px solid var(--border); border-radius: 20px; padding: 28px 34px; width: 220px; cursor: pointer; transition: 0.2s; display: flex; flex-direction: column; align-items: center; gap: 12px; }";
  pagina += ".card-mod:hover { border-color: var(--accent); transform: translateY(-4px); box-shadow: 0 12px 30px rgba(0, 242, 254, 0.2); }";

  // Bara de sus + Setari
  pagina += ".bara-sus { display: flex; justify-content: space-between; align-items: center; padding: 10px 20px; background: rgba(15, 18, 28, 0.9); border-bottom: 1px solid var(--border); z-index: 50; }";
  pagina += ".grup-telemetrie { display: flex; gap: 10px; align-items: center; }";
  pagina += ".eticheta { background: rgba(255,255,255,0.05); border: 1px solid var(--border); padding: 6px 12px; border-radius: 99px; font-size: 13px; font-weight: 600; display: flex; align-items: center; gap: 6px; }";
  pagina += ".punct { width: 8px; height: 8px; border-radius: 50%; background: var(--green); box-shadow: 0 0 8px var(--green); }";
  pagina += ".punct.mediu { background: #ffab00; box-shadow: 0 0 8px #ffab00; }";
  pagina += ".punct.slab { background: var(--red); box-shadow: 0 0 8px var(--red); }";
  pagina += ".btn-setari { background: #23293d; color: #fff; border: 1px solid rgba(255,255,255,0.18); padding: 8px 16px; border-radius: 10px; font-size: 14px; font-weight: 600; cursor: pointer; }";

  // Meniu dropdown setari
  pagina += ".meniu-setari { position: absolute; top: 58px; right: 20px; background: #161a26; border: 1px solid rgba(255,255,255,0.15); border-radius: 14px; padding: 14px; display: none; flex-direction: column; gap: 8px; z-index: 200; box-shadow: 0 16px 40px rgba(0,0,0,0.75); min-width: 260px; }";
  pagina += ".meniu-setari.deschis { display: flex; }";
  pagina += ".titlu-sectiune { font-size: 11px; text-transform: uppercase; letter-spacing: 1px; color: var(--muted); margin-top: 6px; }";
  pagina += ".optiune-meniu { background: rgba(255,255,255,0.06); color: #fff; border: 1px solid var(--border); padding: 10px 14px; border-radius: 9px; font-size: 14px; font-weight: 600; text-align: left; cursor: pointer; display: flex; justify-content: space-between; align-items: center; }";
  pagina += ".optiune-meniu.selectat { background: rgba(0, 242, 254, 0.15); border-color: var(--accent); color: var(--accent); }";

  // Interfata PC
  pagina += "#interfata-pc { flex: 1; display: none; flex-direction: column; align-items: center; justify-content: center; padding: 20px; }";
  pagina += ".grila-pc { display: grid; grid-template-columns: repeat(3, 130px); gap: 14px; margin: 16px 0; }";
  pagina += ".tasta-pc { display: flex; flex-direction: column; align-items: center; justify-content: center; height: 86px; background: var(--panel); color: #fff; border-radius: 16px; font-size: 16px; font-weight: 700; border: 2px solid var(--border); cursor: pointer; box-shadow: 0 6px 0 rgba(0,0,0,0.4); }";
  pagina += ".tasta-pc span { font-size: 11px; font-weight: 500; color: var(--muted); margin-top: 4px; }";
  pagina += ".tasta-pc.apasat { background: rgba(0, 230, 118, 0.2); border-color: var(--green); color: var(--green); transform: translateY(3px); }";
  pagina += ".tasta-stop { background: rgba(255, 59, 92, 0.15); border-color: rgba(255, 59, 92, 0.5); color: #ff6b81; }";

  // Interfata Telefon
  pagina += "#interfata-telefon { flex: 1; display: none; justify-content: space-between; align-items: flex-end; padding: 20px 32px 26px 32px; position: relative; }";
  pagina += ".avertisment-rotire { display: none; position: absolute; top: 24px; left: 50%; transform: translateX(-50%); background: rgba(255, 171, 0, 0.2); border: 1px solid #ffab00; color: #ffd54f; padding: 6px 14px; border-radius: 20px; font-size: 12px; font-weight: 600; white-space: nowrap; }";
  pagina += "@media (orientation: portrait) { .avertisment-rotire { display: block; } }";

  // Optiunea 1 Telefon: Sageti
  pagina += ".grup-sageti { display: flex; gap: 18px; }";
  pagina += ".buton-sageata { width: 98px; height: 98px; border-radius: 22px; background: var(--panel); border: 2px solid rgba(255,255,255,0.18); color: #fff; font-size: 36px; font-weight: bold; display: flex; align-items: center; justify-content: center; cursor: pointer; }";
  pagina += ".buton-sageata.apasat { background: rgba(0, 242, 254, 0.25); border-color: var(--accent); color: var(--accent); transform: scale(0.95); }";

  // Optiunea 2 Telefon: Bara cu cerc (Slider)
  pagina += "#grup-bara-cerc { display: none; flex-direction: column; align-items: center; gap: 10px; }";
  pagina += ".etichete-bara { width: 250px; display: flex; justify-content: space-between; font-size: 12px; font-weight: 700; color: var(--muted); padding: 0 6px; }";
  pagina += ".axa-directie { width: 250px; height: 68px; background: var(--panel); border: 2px solid rgba(255,255,255,0.18); border-radius: 99px; position: relative; display: flex; align-items: center; box-shadow: inset 0 4px 12px rgba(0,0,0,0.5); cursor: pointer; }";
  pagina += ".linie-centru { position: absolute; left: 50%; top: 12px; bottom: 12px; width: 2px; background: rgba(255,255,255,0.2); transform: translateX(-50%); pointer-events: none; }";
  pagina += ".cerc-directie { width: 54px; height: 54px; border-radius: 50%; background: radial-gradient(circle at 30% 30%, #4facfe, var(--accent)); border: 2px solid #fff; position: absolute; left: 50%; transform: translateX(-50%); box-shadow: 0 0 16px rgba(0, 242, 254, 0.6); pointer-events: none; transition: left 0.12s ease-out; }";
  pagina += ".cerc-directie.activ { transition: none; box-shadow: 0 0 24px var(--accent); transform: translateX(-50%) scale(1.06); }";

  // Pedale Telefon + Stop
  pagina += ".grup-pedale { display: flex; gap: 20px; align-items: flex-end; }";
  pagina += ".pedala-spate { width: 84px; height: 118px; border-radius: 16px 16px 10px 10px; background: linear-gradient(180deg, rgba(255,59,92,0.22), rgba(25,20,28,0.9)); border: 2px solid var(--red); color: #ff7675; font-weight: 800; font-size: 13px; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 6px; cursor: pointer; }";
  pagina += ".pedala-spate.apasat { background: var(--red); color: #fff; transform: scale(0.96); }";
  pagina += ".pedala-fata { width: 96px; height: 200px; border-radius: 18px 18px 10px 10px; background: linear-gradient(180deg, rgba(0,230,118,0.22), rgba(18,28,26,0.9)); border: 2px solid var(--green); color: #69f0ae; font-weight: 800; font-size: 14px; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 8px; cursor: pointer; }";
  pagina += ".pedala-fata.apasat { background: var(--green); color: #000; transform: scale(0.96); }";
  pagina += ".stop-telefon { position: absolute; bottom: 26px; left: 50%; transform: translateX(-50%); background: rgba(255, 59, 92, 0.2); color: #ff6b81; border: 2px solid var(--red); padding: 12px 26px; border-radius: 14px; font-weight: 800; font-size: 14px; cursor: pointer; }";
  pagina += "</style></head><body>";

  // --- STRUCTURA PAGINII (HTML) ---
  // 1. Ecranul initial de alegere
  pagina += "<div id='ecran-start'>";
  pagina += "  <div class='titlu-start'>NIKKI RC COCKPIT</div>";
  pagina += "  <div class='subtitlu-start'>Selectează dispozitivul de pe care controlezi robotul</div>";
  pagina += "  <div class='carduri-mod'>";
  pagina += "    <div class='card-mod' onclick=\"alegeModInitial('pc')\"><div style='font-size:44px'>💻</div><b>Mod PC / Laptop</b><span style='font-size:12px;color:var(--muted)'>Tastatură (WASD + Space)</span></div>";
  pagina += "    <div class='card-mod' onclick=\"alegeModInitial('telefon')\"><div style='font-size:44px'>📱</div><b>Mod Telefon</b><span style='font-size:12px;color:var(--muted)'>Orizontal (Pedale + Direcție)</span></div>";
  pagina += "  </div>";
  pagina += "</div>";

  // 2. Bara de sus
  pagina += "<div class='bara-sus'>";
  pagina += "  <div class='grup-telemetrie'>";
  pagina += "    <div class='eticheta'><span class='punct' id='punct-ping'></span>Ping: <b id='text-ping'>-- ms</b></div>";
  pagina += "    <div class='eticheta'>Servo: <b id='text-servo'>1500 µs</b></div>";
  pagina += "  </div>";
  pagina += "  <button class='btn-setari' onclick='comutaMeniuSetari()'>⚙️ Setări</button>";
  pagina += "</div>";

  // 3. Meniul de setari
  pagina += "<div class='meniu-setari' id='meniu-setari'>";
  pagina += "  <div class='titlu-sectiune'>Mod Interfață</div>";
  pagina += "  <button class='optiune-meniu' id='optiune-pc' onclick=\"schimbaMod('pc')\">💻 Interfață PC (WASD)</button>";
  pagina += "  <button class='optiune-meniu' id='optiune-telefon' onclick=\"schimbaMod('telefon')\">📱 Interfață Telefon</button>";
  pagina += "  <div id='sectiune-directie-tel' style='display:none; flex-direction:column; gap:8px;'>";
  pagina += "    <div class='titlu-sectiune'>Stil Direcție Telefon</div>";
  pagina += "    <button class='optiune-meniu selectat' id='opt-dir-sageti' onclick=\"schimbaStilDirectie('sageti')\">◀ ▶ Săgeți Clasice</button>";
  pagina += "    <button class='optiune-meniu' id='opt-dir-bara' onclick=\"schimbaStilDirectie('bara')\">🎚️ Bară cu Cerc (Slider)</button>";
  pagina += "  </div>";
  pagina += "  <div class='titlu-sectiune'>Ecran</div>";
  pagina += "  <button class='optiune-meniu' onclick='comutaFullscreen()'><span id='text-fullscreen'>⛶ Activează Fullscreen</span></button>";
  pagina += "</div>";

  // 4. Interfata PC (+-55 grade: Stanga = 2111, Dreapta = 889)
  pagina += "<div id='interfata-pc'>";
  pagina += "  <div class='grila-pc'>";
  pagina += "    <div></div><div id='tasta-w' class='tasta-pc' onclick='pornesteMotor(100)'>W<span>Înainte MAX</span></div><div></div>";
  pagina += "    <div id='tasta-a' class='tasta-pc' data-directie='2111'>A<span>Stânga (+55°)</span></div>";
  pagina += "    <div class='tasta-pc tasta-stop' onclick='opresteTot()'>SPACE<span>STOP</span></div>";
  pagina += "    <div id='tasta-d' class='tasta-pc' data-directie='889'>D<span>Dreapta (-55°)</span></div>";
  pagina += "    <div></div><div id='tasta-s' class='tasta-pc' onclick='pornesteMotor(-100)'>S<span>Înapoi MAX</span></div><div></div>";
  pagina += "  </div>";
  pagina += "  <p style='color:var(--muted);font-size:13px;text-align:center;margin:0'><b>W</b> = Înainte | <b>S</b> = Înapoi | <b>A / D</b> = Virează ±55° | <b>SPACE</b> = STOP</p>";
  pagina += "</div>";

  // 5. Interfata Telefon (2 optiuni stanga jos: Sageti SAU Bara cu Cerc)
  pagina += "<div id='interfata-telefon'>";
  pagina += "  <div class='avertisment-rotire'>🔄 Rotește telefonul pe orizontală (Landscape)</div>";

  // Optiunea A: Sageti
  pagina += "  <div id='grup-sageti' class='grup-sageti'>";
  pagina += "    <div id='sageata-stanga' class='buton-sageata' data-directie='2111'>◀</div>";
  pagina += "    <div id='sageata-dreapta' class='buton-sageata' data-directie='889'>▶</div>";
  pagina += "  </div>";

  // Optiunea B: Bara cu cerc (Slider)
  pagina += "  <div id='grup-bara-cerc'>";
  pagina += "    <div class='etichete-bara'><span>STÂNGA (+55°)</span><span>0°</span><span>DREAPTA (-55°)</span></div>";
  pagina += "    <div id='axa-directie' class='axa-directie'>";
  pagina += "      <div class='linie-centru'></div>";
  pagina += "      <div id='cerc-directie' class='cerc-directie'></div>";
  pagina += "    </div>";
  pagina += "  </div>";

  pagina += "  <button class='stop-telefon' onclick='opresteTot()'>STOP</button>";
  pagina += "  <div class='grup-pedale'>";
  pagina += "    <div id='pedala-spate' class='pedala-spate'><span>▼</span>SPATE</div>";
  pagina += "    <div id='pedala-fata' class='pedala-fata'><span>▲</span>FAȚĂ</div>";
  pagina += "  </div>";
  pagina += "</div>";

  // --- LOGICA DE CONTROL (JAVASCRIPT) ---
  pagina += "<script>";
  pagina += "const SERVO_STANGA = 2111;";
  pagina += "const SERVO_CENTRU = 1500;";
  pagina += "const SERVO_DREAPTA = 889;";

  pagina += "let motorCurent = 0;";
  pagina += "let servoCurent = SERVO_CENTRU;";
  pagina += "let ultimulTrimisServo = SERVO_CENTRU;";
  pagina += "let butonDirectieApasat = null;";

  // Trimitere comanda catre ESP32 + calcul Ping
  pagina += "function trimiteComanda(ruta) {";
  pagina += "  let separator = ruta.includes('?') ? '&' : '?';";
  pagina += "  let momentStart = performance.now();";
  pagina += "  fetch(ruta + separator + 'ajax=1')";
  pagina += "    .then(raspuns => {";
  pagina += "      let latenta = Math.round(performance.now() - momentStart);";
  pagina += "      document.getElementById('text-ping').textContent = latenta + ' ms';";
  pagina += "      let punct = document.getElementById('punct-ping');";
  pagina += "      if (latenta < 60) punct.className = 'punct';";
  pagina += "      else if (latenta < 150) punct.className = 'punct mediu';";
  pagina += "      else punct.className = 'punct slab';";
  pagina += "    })";
  pagina += "    .catch(() => {";
  pagina += "      document.getElementById('text-ping').textContent = 'OFFLINE';";
  pagina += "      document.getElementById('punct-ping').className = 'punct slab';";
  pagina += "    });";
  pagina += "}";

  // Semnal periodic la 750ms pentru failsafe si ping
  pagina += "setInterval(() => trimiteComanda('/'), 750);";

  pagina += "function actualizeazaAfisaj() {";
  pagina += "  document.getElementById('text-servo').textContent = servoCurent + ' µs';";
  pagina += "}";

  // Directie simpla la apasare (taste A/D sau sageti telefon)
  pagina += "function vireazaSimplu(esteApasat, valoareUnghi, idElement) {";
  pagina += "  document.querySelectorAll('[data-directie]').forEach(el => el.classList.remove('apasat'));";
  pagina += "  if (esteApasat) {";
  pagina += "    if (servoCurent === valoareUnghi && butonDirectieApasat === idElement) return;";
  pagina += "    servoCurent = valoareUnghi;";
  pagina += "    ultimulTrimisServo = valoareUnghi;";
  pagina += "    butonDirectieApasat = idElement;";
  pagina += "    trimiteComanda('/?s=' + servoCurent);";
  pagina += "    if (idElement) document.getElementById(idElement).classList.add('apasat');";
  pagina += "  } else {";
  pagina += "    if (servoCurent === SERVO_CENTRU && !butonDirectieApasat) return;";
  pagina += "    servoCurent = SERVO_CENTRU;";
  pagina += "    ultimulTrimisServo = SERVO_CENTRU;";
  pagina += "    butonDirectieApasat = null;";
  pagina += "    trimiteComanda('/?s=' + SERVO_CENTRU);";
  pagina += "  }";
  pagina += "  actualizeazaAfisaj();";
  pagina += "}";

  // Logica pentru Bara cu Cerc (Slider proportional cu revenire la centru)
  pagina += "const axa = document.getElementById('axa-directie');";
  pagina += "const cerc = document.getElementById('cerc-directie');";
  pagina += "let trageDeBara = false;";

  pagina += "function actualizeazaDinBara(clientX) {";
  pagina += "  let rect = axa.getBoundingClientRect();";
  pagina += "  let razaCerc = 27;";
  pagina += "  let xRelativ = clientX - rect.left;";
  pagina += "  if (xRelativ < razaCerc) xRelativ = razaCerc;";
  pagina += "  if (xRelativ > rect.width - razaCerc) xRelativ = rect.width - razaCerc;";

  // Pozitionare vizuala a cercului pe bara (in procente)
  pagina += "  let procentPozitie = (xRelativ / rect.width) * 100;";
  pagina += "  cerc.style.left = procentPozitie + '%';";

  // Mapare pozitie stanga (-1) -> centru (0) -> dreapta (+1)
  pagina += "  let cursaUtila = (rect.width / 2) - razaCerc;";
  pagina += "  let raport = (xRelativ - (rect.width / 2)) / cursaUtila;";
  pagina += "  let unghiNou = Math.round(SERVO_CENTRU - raport * (SERVO_STANGA - SERVO_CENTRU));";
  pagina += "  servoCurent = unghiNou;";
  pagina += "  actualizeazaAfisaj();";

  // Trimitem comanda catre ESP32 doar cand valoarea s-a schimbat cu minim 15 us
  pagina += "  if (Math.abs(unghiNou - ultimulTrimisServo) >= 15) {";
  pagina += "    ultimulTrimisServo = unghiNou;";
  pagina += "    trimiteComanda('/?s=' + unghiNou);";
  pagina += "  }";
  pagina += "}";

  pagina += "axa.addEventListener('pointerdown', e => {";
  pagina += "  e.preventDefault();";
  pagina += "  trageDeBara = true;";
  pagina += "  axa.setPointerCapture(e.pointerId);";
  pagina += "  cerc.classList.add('activ');";
  pagina += "  actualizeazaDinBara(e.clientX);";
  pagina += "});";

  pagina += "axa.addEventListener('pointermove', e => {";
  pagina += "  if (!trageDeBara) return;";
  pagina += "  e.preventDefault();";
  pagina += "  actualizeazaDinBara(e.clientX);";
  pagina += "});";

  pagina += "function elibereazaBara(e) {";
  pagina += "  if (!trageDeBara) return;";
  pagina += "  e.preventDefault();";
  pagina += "  trageDeBara = false;";
  pagina += "  cerc.classList.remove('activ');";
  pagina += "  cerc.style.left = '50%';";
  pagina += "  servoCurent = SERVO_CENTRU;";
  pagina += "  ultimulTrimisServo = SERVO_CENTRU;";
  pagina += "  trimiteComanda('/?s=' + SERVO_CENTRU);";
  pagina += "  actualizeazaAfisaj();";
  pagina += "}";
  pagina += "axa.addEventListener('pointerup', elibereazaBara);";
  pagina += "axa.addEventListener('pointercancel', elibereazaBara);";

  // Pornire si oprire motor
  pagina += "function pornesteMotor(directie) {";
  pagina += "  motorCurent = directie;";
  pagina += "  trimiteComanda('/?m=' + directie);";
  pagina += "  document.getElementById('tasta-w').classList.toggle('apasat', directie > 0);";
  pagina += "  document.getElementById('tasta-s').classList.toggle('apasat', directie < 0);";
  pagina += "}";

  pagina += "function opresteTot() {";
  pagina += "  motorCurent = 0; servoCurent = SERVO_CENTRU; ultimulTrimisServo = SERVO_CENTRU; butonDirectieApasat = null;";
  pagina += "  cerc.style.left = '50%';";
  pagina += "  trimiteComanda('/?m=0&s=' + SERVO_CENTRU);";
  pagina += "  document.querySelectorAll('.apasat').forEach(el => el.classList.remove('apasat'));";
  pagina += "  actualizeazaAfisaj();";
  pagina += "}";

  // Evenimente atingere/click pentru butoanele tip sageata / PC
  pagina += "document.querySelectorAll('[data-directie]').forEach(buton => {";
  pagina += "  let unghi = parseInt(buton.getAttribute('data-directie'));";
  pagina += "  buton.addEventListener('pointerdown', e => { e.preventDefault(); buton.setPointerCapture(e.pointerId); vireazaSimplu(true, unghi, buton.id); });";
  pagina += "  buton.addEventListener('pointerup', e => { e.preventDefault(); vireazaSimplu(false); });";
  pagina += "  buton.addEventListener('pointercancel', e => { e.preventDefault(); vireazaSimplu(false); });";
  pagina += "});";

  // Evenimente atingere pentru pedalele de telefon
  pagina += "function configureazaPedala(idPedala, viteza) {";
  pagina += "  let pedala = document.getElementById(idPedala);";
  pagina += "  pedala.addEventListener('pointerdown', e => { e.preventDefault(); pedala.setPointerCapture(e.pointerId); pedala.classList.add('apasat'); pornesteMotor(viteza); });";
  pagina += "  pedala.addEventListener('pointerup', e => { e.preventDefault(); pedala.classList.remove('apasat'); pornesteMotor(0); });";
  pagina += "  pedala.addEventListener('pointercancel', e => { e.preventDefault(); pedala.classList.remove('apasat'); pornesteMotor(0); });";
  pagina += "}";
  pagina += "configureazaPedala('pedala-fata', 100);";
  pagina += "configureazaPedala('pedala-spate', -100);";

  // Tastatura PC (WASD + Space)
  pagina += "document.addEventListener('keydown', e => {";
  pagina += "  if (e.repeat) return;";
  pagina += "  let tasta = e.key.toLowerCase();";
  pagina += "  if (tasta === 'w') pornesteMotor(100);";
  pagina += "  else if (tasta === 's') pornesteMotor(-100);";
  pagina += "  else if (tasta === 'a') vireazaSimplu(true, SERVO_STANGA, 'tasta-a');";
  pagina += "  else if (tasta === 'd') vireazaSimplu(true, SERVO_DREAPTA, 'tasta-d');";
  pagina += "  else if (tasta === ' ') { e.preventDefault(); opresteTot(); }";
  pagina += "});";
  pagina += "document.addEventListener('keyup', e => {";
  pagina += "  let tasta = e.key.toLowerCase();";
  pagina += "  if (tasta === 'a' || tasta === 'd') vireazaSimplu(false);";
  pagina += "});";

  // Functii pentru Setari, Moduri, Stil Directie Telefon si Fullscreen
  pagina += "function alegeModInitial(mod) { document.getElementById('ecran-start').style.display = 'none'; schimbaMod(mod); }";
  pagina += "function comutaMeniuSetari() { document.getElementById('meniu-setari').classList.toggle('deschis'); }";
  pagina += "function schimbaMod(mod) {";
  pagina += "  opresteTot();";
  pagina += "  document.getElementById('interfata-pc').style.display = (mod === 'pc') ? 'flex' : 'none';";
  pagina += "  document.getElementById('interfata-telefon').style.display = (mod === 'telefon') ? 'flex' : 'none';";
  pagina += "  document.getElementById('sectiune-directie-tel').style.display = (mod === 'telefon') ? 'flex' : 'none';";
  pagina += "  document.getElementById('optiune-pc').classList.toggle('selectat', mod === 'pc');";
  pagina += "  document.getElementById('optiune-telefon').classList.toggle('selectat', mod === 'telefon');";
  pagina += "  document.getElementById('meniu-setari').classList.remove('deschis');";
  pagina += "}";
  pagina += "function schimbaStilDirectie(stil) {";
  pagina += "  opresteTot();";
  pagina += "  document.getElementById('grup-sageti').style.display = (stil === 'sageti') ? 'flex' : 'none';";
  pagina += "  document.getElementById('grup-bara-cerc').style.display = (stil === 'bara') ? 'flex' : 'none';";
  pagina += "  document.getElementById('opt-dir-sageti').classList.toggle('selectat', stil === 'sageti');";
  pagina += "  document.getElementById('opt-dir-bara').classList.toggle('selectat', stil === 'bara');";
  pagina += "  document.getElementById('meniu-setari').classList.remove('deschis');";
  pagina += "}";
  pagina += "function comutaFullscreen() {";
  pagina += "  if (!document.fullscreenElement) document.documentElement.requestFullscreen().catch(() => {});";
  pagina += "  else if (document.exitFullscreen) document.exitFullscreen();";
  pagina += "  document.getElementById('meniu-setari').classList.remove('deschis');";
  pagina += "}";
  pagina += "document.addEventListener('fullscreenchange', () => {";
  pagina += "  document.getElementById('text-fullscreen').textContent = document.fullscreenElement ? '✕ Ieși din Fullscreen' : '⛶ Activează Fullscreen';";
  pagina += "});";
  pagina += "</script></body></html>";

  return pagina;
}

void proceseazaCerereWeb() {
  timpUltimaComanda = millis();

  if (server.hasArg("m")) {
    controleazaMotor(server.arg("m").toInt());
  }
  if (server.hasArg("s")) {
    controleazaServo(server.arg("s").toInt());
  }

  // Raspuns rapid pentru comenzile din fundal (fara sa retrimita toata pagina HTML)
  if (server.hasArg("ajax")) {
    server.send(200, "text/plain", "OK");
    return;
  }

  server.send(200, "text/html", genereazaPaginaHTML());
}

// ==========================================
// 5. PROGRAM PRINCIPAL (SETUP SI LOOP)
// ==========================================
void setup() {
  // Initializare pini LED-uri
  pinMode(PIN_LED_ROSU, OUTPUT);
  pinMode(PIN_LED_GALBEN, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);

  // Initializare PWM servo si motor
  ledcAttach(PIN_SERVO, 50, 14);
  ledcAttach(PIN_IN1, 20000, 8);
  ledcAttach(PIN_IN2, 20000, 8);
  ledcAttach(PIN_IN3, 20000, 8);
  ledcAttach(PIN_IN4, 20000, 8);

  // Pornire in stare oprita (aprinde automat LED-ul rosu) si servo pe centru
  controleazaMotor(0);
  controleazaServo(SERVO_CENTRU);

  WiFi.softAP(NUME_WIFI, PAROLA_WIFI);
  server.on("/", proceseazaCerereWeb);
  server.begin();
}

void loop() {
  server.handleClient();

  // Oprire automata de siguranta daca se pierde conexiunea (aprinde automat LED-ul rosu)
  if (stareMotor != 0 && (millis() - timpUltimaComanda > TIMP_OPRIRE_AUTOMATA)) {
    controleazaMotor(0);
  }
}