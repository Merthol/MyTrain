
#include <WiFi.h>
#include <WebServer.h>

// ======================================
// CONFIGURAÇÕES WI-FI
// ======================================

const char* ssid = "guilherme.wifi";
const char* password = "deusa2512";

WebServer server(80);

// ======================================
// CONFIGURAÇÕES DO LED
// ======================================

// GPIO onde o LED está conectado
constexpr int LED_PIN = 0;

// Frequência PWM: 5.000 ciclos por segundo
constexpr int PWM_FREQ = 5000;

// Resolução de 8 bits: valores de 0 a 255
constexpr int PWM_RES = 8;

// Intensidade inicial do LED (0 a 100%)
int brilho = 0;

// ======================================
// CONTROLE DO BRILHO
// ======================================

void ajustarBrilho(int porcentagem) {

  // Garante que o valor esteja entre 0 e 100
  brilho = constrain(porcentagem, 0, 100);

  // Converte a porcentagem para PWM (0 a 255)
  int pwm = map(brilho, 0, 100, 0, 255);

  // Aplica o PWM na GPIO 0
  ledcWrite(LED_PIN, pwm);

  Serial.print("[LED] Brilho: ");
  Serial.print(brilho);
  Serial.println("%");
}

// ======================================
// PÁGINA WEB
// ======================================

void mostrarPagina() {

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="pt-br">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport"
          content="width=device-width, initial-scale=1">

    <title>MyTrain</title>

    <style>
      body {
        font-family: Arial, sans-serif;
        text-align: center;
        background: #f3f4f6;
        padding: 35px 15px;
      }

      .painel {
        max-width: 400px;
        margin: auto;
        background: white;
        padding: 30px;
        border-radius: 15px;
      }

      input[type=range] {
        width: 100%;
        margin: 25px 0;
      }

      h1 {
        color: #2563eb;
      }
    </style>
  </head>

  <body>
    <div class="painel">
      <h1>MyTrain</h1>

      <h3>Controle de iluminação</h3>

      <h2><span id="valor">%BRILHO%</span>%</h2>

      <input
        type="range"
        id="slider"
        min="0"
        max="100"
        value="%BRILHO%"
        oninput="alterarBrilho(this.value)"
      >
    </div>

    <script>
      let temporizador;

      function alterarBrilho(valor) {

        // Atualiza o número na página
        document.getElementById("valor").innerText = valor;

        // Evita enviar muitos comandos seguidos
        clearTimeout(temporizador);

        temporizador = setTimeout(() => {
          fetch("/brilho?valor=" + valor)
            .catch(erro => console.log(erro));
        }, 80);
      }
    </script>

  </body>
  </html>
  )rawliteral";

  // Preenche o HTML com o brilho atual
  html.replace("%BRILHO%", String(brilho));

  server.send(200, "text/html; charset=utf-8", html);
}

// ======================================
// RECEBE OS COMANDOS DO NAVEGADOR
// ======================================

void receberBrilho() {

  if (!server.hasArg("valor")) {
    server.send(400, "text/plain", "Valor ausente");
    return;
  }

  int valor = server.arg("valor").toInt();

  ajustarBrilho(valor);

  server.send(200, "text/plain", "OK");
}

// ======================================
// MONITORAMENTO WI-FI
// ======================================

void eventoWiFi(WiFiEvent_t evento, WiFiEventInfo_t info) {

  switch (evento) {

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WIFI] Conectado ao roteador");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("[WIFI] IP recebido");
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.print("[WIFI] Desconectado. Motivo: ");
      Serial.println(info.wifi_sta_disconnected.reason);
      break;

    default:
      break;
  }
}

// ======================================
// SETUP
// ======================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println("=== MYTRAIN v0.2 ===");

  // Configura o PWM na GPIO 0
  if (!ledcAttach(LED_PIN, PWM_FREQ, PWM_RES)) {
    Serial.println("[ERRO] Falha ao configurar PWM");
  }

  // Inicia o LED desligado
  ajustarBrilho(0);

  // Registra os eventos Wi-Fi
  WiFi.onEvent(eventoWiFi);

  // Conecta ao roteador
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // Mantém a potência reduzida usada nos testes
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  // Configura as rotas do servidor
  server.on("/", HTTP_GET, mostrarPagina);
  server.on("/brilho", HTTP_GET, receberBrilho);

  // Inicia o servidor web
  server.begin();

  Serial.println("[WEB] Servidor iniciado");
}

// ======================================
// LOOP
// ======================================

void loop() {

  // Processa as requisições HTTP
  server.handleClient();

  // Monitoramento a cada 5 segundos
  static unsigned long ultimoMonitoramento = 0;

  if (millis() - ultimoMonitoramento >= 5000) {

    ultimoMonitoramento = millis();

    if (WiFi.status() == WL_CONNECTED) {

      Serial.print("[WIFI] IP: ");
      Serial.print(WiFi.localIP());

      Serial.print(" | RSSI: ");
      Serial.print(WiFi.RSSI());

      Serial.println(" dBm");

    } else {
      Serial.println("[WIFI] Sem conexao");
    }
  }

  delay(10);
}
