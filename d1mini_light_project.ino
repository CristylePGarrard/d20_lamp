#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "UpdateWithWiFiSSID";
const char* password = "UpdateWithWiFiPassword";
ESP8266WebServer server(80);

const int LED_PIN    = D2;
const int BUTTON_PIN = D5;

bool ledOn        = false;
int  brightness   = 255;

// Button debounce
bool     lastButtonState = LOW;
bool     buttonState     = LOW;
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LED Control</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, sans-serif;
      background: #1a1a2e;
      color: #eee;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
    }
    .card {
      background: #16213e;
      border-radius: 20px;
      padding: 40px 32px;
      width: 320px;
      text-align: center;
      box-shadow: 0 8px 32px rgba(0,0,0,0.4);
    }
    h1 { font-size: 22px; margin-bottom: 8px; }
    .subtitle { font-size: 13px; color: #888; margin-bottom: 36px; }

    /* LED bulb icon */
    .bulb { font-size: 64px; margin-bottom: 24px; transition: opacity 0.3s; }
    .bulb.off { opacity: 0.25; filter: grayscale(1); }

    /* Toggle switch */
    .toggle-row { display: flex; align-items: center; justify-content: space-between; margin-bottom: 32px; }
    .toggle-label { font-size: 15px; }
    .switch { position: relative; display: inline-block; width: 56px; height: 30px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider-track {
      position: absolute; cursor: pointer;
      top: 0; left: 0; right: 0; bottom: 0;
      background: #333;
      border-radius: 30px;
      transition: background 0.3s;
    }
    .slider-track:before {
      position: absolute; content: "";
      height: 22px; width: 22px;
      left: 4px; bottom: 4px;
      background: white;
      border-radius: 50%;
      transition: transform 0.3s;
    }
    input:checked + .slider-track { background: #f0a500; }
    input:checked + .slider-track:before { transform: translateX(26px); }

    /* Brightness slider */
    .brightness-label { font-size: 13px; color: #888; margin-bottom: 12px; text-align: left; }
    input[type=range] {
      -webkit-appearance: none;
      width: 100%; height: 6px;
      border-radius: 3px;
      background: #333;
      outline: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 22px; height: 22px;
      border-radius: 50%;
      background: #f0a500;
      cursor: pointer;
    }
    .brightness-value {
      font-size: 13px; color: #888;
      text-align: right; margin-top: 8px;
    }
  </style>
</head>
<body>
<div class="card">
  <h1>LED Control</h1>
  <p class="subtitle">D1 Mini · Pin D2</p>

  <div id="bulb" class="bulb off">💡</div>

  <div class="toggle-row">
    <span class="toggle-label">Power</span>
    <label class="switch">
      <input type="checkbox" id="toggle" onchange="setLed()">
      <span class="slider-track"></span>
    </label>
  </div>

  <p class="brightness-label">Brightness</p>
  <input type="range" min="0" max="255" value="255" id="bright" oninput="setBrightness(this.value)">
  <p class="brightness-value" id="pct">100%</p>
</div>

<script>
  function setLed() {
    const on = document.getElementById('toggle').checked;
    document.getElementById('bulb').className = 'bulb ' + (on ? '' : 'off');
    fetch('/led?state=' + (on ? '1' : '0'));
  }

  function setBrightness(val) {
    document.getElementById('pct').textContent = Math.round(val / 255 * 100) + '%';
    // Auto-turn on if sliding up from 0
    if (val > 0 && !document.getElementById('toggle').checked) {
      document.getElementById('toggle').checked = true;
      document.getElementById('bulb').className = 'bulb';
    }
    fetch('/brightness?val=' + val);
  }
</script>
</body>
</html>
)rawliteral";

// --- Last control source for UI display ---
String lastSource = "web";

void applyLed() {
  analogWrite(LED_PIN, ledOn ? brightness : 0);
}

void handleRoot() {
  server.send(200, "text/html", webpage);
}

void handleState() {
  String json = "{\"on\":" + String(ledOn ? "true" : "false") +
                ",\"brightness\":" + String(brightness) +
                ",\"source\":\"" + lastSource + "\"}";
  server.send(200, "application/json", json);
}

void handleLed() {
  if (server.hasArg("state")) {
    ledOn = server.arg("state") == "1";
    lastSource = "web";
    applyLed();
  }
  server.send(200, "text/plain", "OK");
}

void handleBrightness() {
  if (server.hasArg("val")) {
    brightness = server.arg("val").toInt();
    if (brightness > 0) ledOn = true;
    lastSource = "web";
    applyLed();
  }
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  analogWrite(LED_PIN, 0);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  server.on("/",          handleRoot);
  server.on("/state",     handleState);
  server.on("/led",       handleLed);
  server.on("/brightness", handleBrightness);
  server.begin();
}

void loop() {
  server.handleClient();

  // --- Potentiometer (A0) ---
  int potVal = analogRead(A0);              // 0–1023
  int newBrightness = map(potVal, 0, 1023, 0, 255);
  if (abs(newBrightness - brightness) > 4) { // deadband to avoid flicker
    brightness = newBrightness;
    if (brightness > 0) ledOn = true;
    lastSource = "pot";
    applyLed();
  }

  // --- Button (D5) with debounce ---
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) lastDebounce = millis();
  if ((millis() - lastDebounce) > DEBOUNCE_MS) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {       // button pressed
        ledOn = !ledOn;
        lastSource = "button";
        applyLed();
      }
    }
  }
  lastButtonState = reading;
}
