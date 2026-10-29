
#include <WiFi.h>
#include <WebServer.h>

// ======================================
// CONFIGURAÇÕES WI-FI
// ======================================

const char* ssid = "guilherme.wifi";
const char* password = "deusa2512";

WebServer server(80);

// ======================================
// CONFIGURAÇÕES DOS LEDs
// ======================================

// Array com as GPIOs dos três LEDs
const int LED_PINS[] = {0, 1, 2};

// Quantidade de LEDs
constexpr int NUM_LEDS = 3;

// Array que armazena o brilho de cada LED
int brilhos[NUM_LEDS] = {0, 0, 0};

// Configurações do PWM
constexpr int PWM_FREQ = 5000;
constexpr int PWM_RES = 8;

// ======================================
// CONTROLE DOS LEDs
// ======================================

void ajustarBrilho(int indice, int porcentagem) {

  // Verifica se o LED solicitado existe
  if (indice < 0 || indice >= NUM_LEDS) {
    return;
  }

  // Limita a porcentagem entre 0 e 100
  porcentagem = constrain(porcentagem, 0, 100);

  // Armazena o brilho do LED
  brilhos[indice] = porcentagem;

  // Converte porcentagem para PWM (0 a 255)
  int pwm = map(porcentagem, 0, 100, 0, 255);

  // Aplica o PWM à GPIO correspondente
  ledcWrite(LED_PINS[indice], pwm);

  Serial.print("[LED] GPIO ");
  Serial.print(LED_PINS[indice]);
  Serial.print(" | Brilho: ");
  Serial.print(porcentagem);
  Serial.println("%");
}

void desligarTodos() {

  for (int i = 0; i < NUM_LEDS; i++) {
    ajustarBrilho(i, 0);
  }

  server.send(200, "text/plain", "OK");
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
        padding: 25px 15px;
      }

      .painel {
        max-width: 420px;
        margin: auto;
        background: white;
        padding: 25px;
        border-radius: 15px;
      }

      .led {
        padding: 12px 0;
        border-bottom: 1px solid #ddd;
      }

      .verde {
        color: green;
      }

      .vermelho {
        color: red;
      }

      .azul{
        color: blue;
      }

      input[type=range] {
        width: 100%;
        margin: 15px 0;
      }

      h1 {
        color: #2563eb;
      }

      button {
        margin-top: 25px;
        padding: 12px 24px;
        background: #dc2626;
        color: white;
        border: none;
        border-radius: 8px;
        font-size: 16px;
        cursor: pointer;
      }
    </style>
  </head>

  <body>

    <div class="painel">

      <h1>MyTrain v0.3</h1>
      <h3>Controle de iluminação</h3>

      <div class="led verde">
        <h3>LED Verde - GPIO 0</h3>
        <h2><span id="valor0">%BRILHO0%</span>%</h2>

        <input type="range"
               min="0" max="100"
               value="%BRILHO0%"
               oninput="alterarBrilho(0, this.value)">
      </div>

      <div class="led vermelho">
        <h3>LED Vermelho - GPIO 1</h3>
        <h2><span id="valor1">%BRILHO1%</span>%</h2>

        <input type="range"
               min="0" max="100"
               value="%BRILHO1%"
               oninput="alterarBrilho(1, this.value)">
      </div>

      <div class="led azul">
        <h3>LED Azul - GPIO 2</h3>
        <h2><span id="valor2">%BRILHO2%</span>%</h2>

        <input type="range"
               min="0" max="100"
               value="%BRILHO2%"
               oninput="alterarBrilho(2, this.value)">
      </div>

      <button onclick="desligarTodos()">
        Desligar todos
      </button>

    </div>

    <script>
      const temporizadores = {};

      function alterarBrilho(led, valor) {

        // Atualiza o valor mostrado na tela
        document.getElementById("valor" + led).innerText = valor;

        // Cancela o temporizador anterior desse LED
        clearTimeout(temporizadores[led]);

        // Envia o comando após 80 ms
        temporizadores[led] = setTimeout(() => {
          fetch("/brilho?led=" + led + "&valor=" + valor)
            .catch(erro => console.log(erro));
        }, 80);
      }

      function desligarTodos() {

        // Cancela comandos pendentes
        Object.keys(temporizadores).forEach(led => {
          clearTimeout(temporizadores[led]);
        });

        fetch("/desligar", {method: "POST"})
          .then(resposta => {
            if (!resposta.ok) {
              throw new Error("Falha ao desligar LEDs");
            }

            // Atualiza a interface
            for (let i = 0; i < 3; i++) {
              document.getElementById("valor" + i).innerText = 0;

              document.querySelectorAll('input[type=range]')[i].value = 0;
            }
          })
          .catch(erro => console.log(erro));
      }
    </script>

  </body>
  </html>
  )rawliteral";

  // Substitui os marcadores pelos valores atuais
  for (int i = 0; i < NUM_LEDS; i++) {
    html.replace(
      "%BRILHO" + String(i) + "%",
      String(brilhos[i])
    );
  }

  server.send(200, "text/html; charset=utf-8", html);
}

// ======================================
// RECEBE OS COMANDOS HTTP
// ======================================

void receberBrilho() {

  if (!server.hasArg("led") || !server.hasArg("valor")) {
    server.send(400, "text/plain", "Parametros ausentes");
    return;
  }

  int indice = server.arg("led").toInt();
  int valor = server.arg("valor").toInt();

  // Valida o índice do LED
  if (indice < 0 || indice >= NUM_LEDS) {
    server.send(400, "text/plain", "LED invalido");
    return;
  }

  ajustarBrilho(indice, valor);

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

  Serial.println("=== MYTRAIN v0.3 ===");

  // Inicializa cada LED com PWM independente
  for (int i = 0; i < NUM_LEDS; i++) {

    // Configura PWM com canal automático
    if (!ledcAttach(LED_PINS[i], PWM_FREQ, PWM_RES)) {
      Serial.print("[ERRO] Falha no PWM da GPIO ");
      Serial.println(LED_PINS[i]);
      continue;
    }

    // Inicia o LED apagado
    ajustarBrilho(i, 0);
  }

  // Eventos de conexão
  WiFi.onEvent(eventoWiFi);

  // Wi-Fi em modo estação
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  // Rotas HTTP
  server.on("/", HTTP_GET, mostrarPagina);
  server.on("/brilho", HTTP_GET, receberBrilho);
  server.on("/desligar", HTTP_POST, desligarTodos);

  // Inicia o servidor
  server.begin();

  Serial.println("[WEB] Servidor iniciado");
}

// ======================================
// LOOP
// ======================================

void loop() {

  // Processa as requisições HTTP
  server.handleClient();

  // Mostra o estado do Wi-Fi a cada 5 segundos
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
