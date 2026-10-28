
#include <WiFi.h>
#include <WebServer.h>

// =========================================
// CONFIGURAÇÕES
// =========================================

// Dados da rede Wi-Fi
const char* ssid = "guilherme.wifi";
const char* password = "deusa2512";

// LED conectado na GPIO 0
constexpr int LED_PIN = 0;

// Servidor web
WebServer server(80);

// Estado do LED
bool ledLigado = false;

// Controle de tempo
unsigned long ultimoMonitoramento = 0;
unsigned long ultimaTentativa = 0;

// =========================================
// EVENTOS DO WI-FI
// =========================================

void eventoWiFi(WiFiEvent_t evento, WiFiEventInfo_t info) {

  switch (evento) {

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WIFI] Conectado ao roteador!");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("[WIFI] Endereco IP recebido!");
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.print("[WIFI] Conexao perdida! Motivo: ");
      Serial.println(info.wifi_sta_disconnected.reason);
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.println("[WIFI] Endereco IP perdido!");
      break;

    default:
      break;
  }
}

// =========================================
// PAGINA WEB
// =========================================

void mostrarPagina() {

  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="pt-br">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <title>MyTrain</title>
    </head>

    <body style="text-align:center; font-family:Arial; padding:40px;">
      <h2>MyTrain</h2>
      <p>LED: %ESTADO%</p>

      <form action="/toggle" method="POST">
        <button style="font-size:22px; padding:15px 35px;">
          %BOTAO%
        </button>
      </form>
    </body>
    </html>
  )rawliteral";

  html.replace("%ESTADO%", ledLigado ? "LIGADO" : "DESLIGADO");
  html.replace("%BOTAO%", ledLigado ? "Desligar" : "Ligar");

  server.send(200, "text/html; charset=utf-8", html);
}

// =========================================
// CONTROLE DO LED
// =========================================

void alternarLED() {

  ledLigado = !ledLigado;

  digitalWrite(LED_PIN, ledLigado ? HIGH : LOW);

  Serial.println(ledLigado ? "[LED] Ligado" : "[LED] Desligado");

  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

// =========================================
// INICIALIZAÇÃO
// =========================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println("=== MYTRAIN INICIANDO ===");

  // Configura o LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Registra eventos de conexão
  WiFi.onEvent(eventoWiFi);

  // Configura o Wi-Fi
  WiFi.mode(WIFI_STA);

  // Desativa a economia de energia do Wi-Fi
  WiFi.setSleep(false);

  // Reduz a potência de transmissão
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  // Habilita reconexão automática
  WiFi.setAutoReconnect(true);

  // Inicia conexão
  WiFi.begin(ssid, password);

  Serial.println("[WIFI] Iniciando conexao...");

  // Configura servidor web
  server.on("/", HTTP_GET, mostrarPagina);
  server.on("/toggle", HTTP_POST, alternarLED);

  server.begin();

  Serial.println("[WEB] Servidor iniciado");
}

// =========================================
// EXECUÇÃO CONTÍNUA
// =========================================

void loop() {

  // Processa os comandos do navegador
  server.handleClient();

  unsigned long agora = millis();

  // Monitoramento a cada 5 segundos
  if (agora - ultimoMonitoramento >= 5000) {

    ultimoMonitoramento = agora;

    if (WiFi.status() == WL_CONNECTED) {

      Serial.println("----- STATUS WIFI -----");

      Serial.print("IP: ");
      Serial.println(WiFi.localIP());

      Serial.print("RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");

      Serial.println("Estado: CONECTADO");

    } else {

      Serial.println("[WIFI] DESCONECTADO");
    }
  }

  // Tenta reconectar em intervalos de 15 segundos
  if (WiFi.status() != WL_CONNECTED &&
      agora - ultimaTentativa >= 15000) {

    ultimaTentativa = agora;

    Serial.println("[WIFI] Solicitando reconexao...");
    WiFi.reconnect();
  }

  delay(10);
}
