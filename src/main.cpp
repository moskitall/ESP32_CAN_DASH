#include <SPI.h>
#include <mcp_can.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "ESP32_CAN";
const char* password = "";

MCP_CAN CAN0(5);  // CS на GPIO5

AsyncWebServer server(80);

// Глобальные значения
String rpmStr = "0";
String tempStr = "--";
String boostStr = "--";

void setup() {
  Serial.begin(115200);

  // Инициализация CAN
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("Setting Baudrate Successful!");
    Serial.println("CAN init OK");
  } else {
    Serial.println("CAN init FAIL");
    while (1);
  }

  CAN0.setMode(MCP_NORMAL);

  // Wi-Fi
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("WiFi AP Started\nIP: "); Serial.println(IP);

  // Веб-интерфейс
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"rawliteral(
      <!DOCTYPE html><html><head>
      <meta charset='UTF-8'><meta name='viewport' content='width=device-width'>
      <title>ESP32 Dash</title>
      <style>
        body { background:#111; color:#eee; font-family:sans-serif; text-align:center; padding:20px; }
        h1 { color:#0af; margin: 10px 0; }
        .rpm { font-size:3em; color:#ff4444; margin:20px 0; }
        .info { font-size:1.5em; }
        .logo svg { margin-bottom: 10px; }
      </style>
      </head><body>
        <div class="logo">
          <svg width="220" height="80" viewBox="0 0 500 120" xmlns="http://www.w3.org/2000/svg">
            <!-- Кольца Audi -->
            <circle cx="50" cy="60" r="40" stroke="#eee" stroke-width="6" fill="none"/>
            <circle cx="110" cy="60" r="40" stroke="#eee" stroke-width="6" fill="none"/>
            <circle cx="170" cy="60" r="40" stroke="#eee" stroke-width="6" fill="none"/>
            <circle cx="230" cy="60" r="40" stroke="#eee" stroke-width="6" fill="none"/>
            <!-- S4 -->
            <rect x="290" y="30" width="120" height="60" rx="8" fill="#d00"/>
            <text x="310" y="73" fill="white" font-size="42" font-weight="bold" font-family="Arial, sans-serif">S4</text>
          </svg>
        </div>
        <h1>ESP32 CAN Dash</h1>
        <div class="rpm" id="rpm">RPM: --</div>
        <div class="info">
          <div>Температура: <span id="temp">--</span> °C</div>
          <div>Наддув: <span id="boost">--</span> бар</div>
        </div>
        <script>
          let dots = '';
          setInterval(() => {
            fetch('/rpm').then(r => r.text()).then(rpm => {
              const el = document.getElementById('rpm');
              if (parseInt(rpm) > 0) {
                el.innerText = 'RPM: ' + rpm;
              } else {
                dots = dots.length < 3 ? dots + '.' : '';
                el.innerText = 'RPM: --' + dots;
              }
            });

            fetch('/info').then(r => r.json()).then(data => {
              document.getElementById('temp').innerText = data.temp || '--';
              document.getElementById('boost').innerText = data.boost || '--';
            });
          }, 500);
        </script>
      </body></html>
    )rawliteral";
    request->send(200, "text/html", html);
  });

  server.on("/rpm", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", rpmStr);
  });

  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"temp\":\"" + tempStr + "\",\"boost\":\"" + boostStr + "\"}";
    request->send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  unsigned long id;
  byte ext;
  byte len = 0;
  byte buf[8];

  if (CAN0.checkReceive() == CAN_MSGAVAIL) {
    if (CAN0.readMsgBuf(&id, &ext, &len, buf) == CAN_OK) {
      Serial.print("ID: 0x"); Serial.print(id, HEX);
      Serial.print(" Data: ");
      for (byte i = 0; i < len; i++) {
        if (buf[i] < 0x10) Serial.print("0");
        Serial.print(buf[i], HEX); Serial.print(" ");
      }
      Serial.println();

      // RPM (пример: ID 0x280, байты 2-3)
      if (id == 0x280) {
        int rpm = ((buf[3] << 8) | buf[2]) / 4;
        rpmStr = String(rpm);
      }

      // Температура (пример: ID 0x420, байт 0)
      if (id == 0x420) {
        int temp = buf[0] - 40;
        tempStr = String(temp);
      }

      // Наддув (пример: ID 0x440, байт 2)
      if (id == 0x440) {
        float boost = buf[2] * 0.01;
        boostStr = String(boost, 2);
      }
    }
  }
}
