
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ======================================
// CONFIGURAÇÕES WI-FI
// ======================================

const char* ssid = "guilherme.wifi";
const char* password = "deusa2512";

WebServer server(80);

// ======================================
// PINOS DO TB6612FNG
// ======================================

constexpr int AIN1 = 3;
constexpr int AIN2 = 4;
constexpr int PWMA = 5;
constexpr int STBY = 6;

// ======================================
// CONFIGURAÇÕES DO MOTOR
// ======================================

constexpr int PWM_FREQ = 5000;
constexpr int PWM_RES = 8;

// Velocidade inicial selecionada na página
int velocidade = 60;

// Direção:
//  0 = parado
//  1 = frente
// -1 = reverso
int direcao = 0;

// Estado de preparação do PWM
bool pwmPronto = false;

// ======================================
// CONTROLE DO MOTOR
// ======================================

// Interrompe o acionamento e deixa o motor
// em roda livre, sem aplicar potência.
void pararMotor() {

  if (pwmPronto) {
    ledcWrite(PWMA, 0);
  }

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  direcao = 0;

  Serial.println("[MOTOR] Parado");
}

// Aplica a velocidade ao motor.
void aplicarVelocidade() {

  if (!pwmPronto) {
    return;
  }

  // Se estiver parado, mantém PWM em zero.
  if (direcao == 0) {
    ledcWrite(PWMA, 0);
    return;
  }

  // Converte de 0-100% para 0-255
  int pwm = map(velocidade, 0, 100, 0, 255);

  ledcWrite(PWMA, pwm);

  Serial.print("[MOTOR] Velocidade: ");
  Serial.print(velocidade);
  Serial.println("%");
}

// Define a direção com proteção na inversão.
void definirDirecao(int novaDirecao) {

  if (!pwmPronto) {
    return;
  }

  if (novaDirecao == 0) {
    pararMotor();
    return;
  }

  // Se estiver alterando de frente para ré,
  // ou de ré para frente, para primeiro.
  if (direcao != 0 && direcao != novaDirecao) {

    pararMotor();

    Serial.println("[MOTOR] Aguardando inversao...");
    delay(500);
  }

  // Interrompe o PWM antes de mudar AIN1/AIN2
  ledcWrite(PWMA, 0);

  if (novaDirecao == 1) {

    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);

    Serial.println("[MOTOR] Direcao: FRENTE");

  } else {

    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);

    Serial.println("[MOTOR] Direcao: REVERSO");
  }

  direcao = novaDirecao;

  // Volta a aplicar a velocidade escolhida
  aplicarVelocidade();
}

// ======================================
// PAGINA WEB
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
      background: #f3f4f6;
      font-family: Arial, sans-serif;
      text-align: center;
      padding: 30px 12px;
    }

    .painel {
      background: white;
      max-width: 420px;
      margin: auto;
      padding: 25px;
      border-radius: 15px;
    }

    h1 {
      color: #2563eb;
    }

    .botoes {
      display: flex;
      gap: 8px;
      justify-content: center;
    }

    button {
      padding: 15px;
      border: 0;
      border-radius: 8px;
      background: #2563eb;
      color: white;
      font-size: 16px;
      flex: 1;
      cursor: pointer;
    }

    .parar {
      background: #dc2626;
    }

    input[type=range] {
      width: 100%;
      margin: 25px 0;
    }

    #estado {
      margin-top: 25px;
      padding: 15px;
      background: #f3f4f6;
      border-radius: 8px;
    }
  </style>
</head>

<body>

<div class="painel">

  <h1>MyTrain v0.5</h1>
  <h3>Controle do motor</h3>

  <div class="botoes">
    <button onclick="mudarDirecao(1)">
      Frente
    </button>

    <button class="parar"
            onclick="mudarDirecao(0)">
      Parar
    </button>

    <button onclick="mudarDirecao(-1)">
      Ré
    </button>
  </div>

  <h3>Velocidade</h3>

  <h2><span id="valor">%VELOCIDADE%</span>%</h2>

  <input type="range"
         min="0"
         max="100"
         value="%VELOCIDADE%"
         oninput="alterarVelocidade(this.value)">

  <div id="estado">Estado: %ESTADO%</div>

</div>

<script>

  let temporizador;
  let direcaoAtual = %DIRECAO%;

  // Atualiza o texto de estado na página.
  function atualizarEstado() {

    let texto = "Parado";

    if (direcaoAtual === 1) {
      texto = "Frente";
    }

    if (direcaoAtual === -1) {
      texto = "Ré";
    }

    document.getElementById("estado").innerText =
      "Estado: " + texto;
  }

  // Envia um comando de direção.
  async function mudarDirecao(novaDirecao) {

    // Evita comandos pendentes de velocidade.
    clearTimeout(temporizador);

    try {

      const resposta = await fetch(
        "/direcao?valor=" + novaDirecao,
        { method: "POST" }
      );

      if (!resposta.ok) {
        throw new Error("Falha no comando");
      }

      direcaoAtual = novaDirecao;
      atualizarEstado();

    } catch (erro) {
      console.log(erro);
      document.getElementById("estado").innerText =
        "Erro ao enviar comando!";
    }
  }

  // Envia a velocidade do slider.
  function alterarVelocidade(valor) {

    document.getElementById("valor").innerText = valor;

    clearTimeout(temporizador);

    temporizador = setTimeout(async () => {

      try {

        const resposta = await fetch(
          "/velocidade?valor=" + valor,
          { method: "POST" }
        );

        if (!resposta.ok) {
          throw new Error("Falha na velocidade");
        }

      } catch (erro) {
        console.log(erro);
        document.getElementById("estado").innerText =
          "Erro de comunicação!";
      }

    }, 100);
  }

</script>

</body>
</html>
)rawliteral";

  html.replace("%VELOCIDADE%", String(velocidade));
  html.replace("%DIRECAO%", String(direcao));

  String estado = "Parado";

  if (direcao == 1) {
    estado = "Frente";
  } else if (direcao == -1) {
    estado = "Ré";
  }

  html.replace("%ESTADO%", estado);

  server.send(200, "text/html; charset=utf-8", html);
}

// ======================================
// RECEBIMENTO DOS COMANDOS HTTP
// ======================================

// POST /direcao?valor=1
// POST /direcao?valor=0
// POST /direcao?valor=-1

void receberDirecao() {

  if (!server.hasArg("valor")) {
    server.send(400, "text/plain", "Valor ausente");
    return;
  }

  String parametro = server.arg("valor");
  int valor = parametro.toInt();

  // Valida a entrada
  if (parametro != "-1" &&
      parametro != "0" &&
      parametro != "1") {
    server.send(400, "text/plain", "Direcao invalida");
    return;
  }

  if (!pwmPronto) {
    server.send(503, "text/plain", "PWM indisponivel");
    return;
  }

  definirDirecao(valor);

  server.send(200, "text/plain", "OK");
}

// POST /velocidade?valor=60

void receberVelocidade() {

  if (!server.hasArg("valor")) {
    server.send(400, "text/plain", "Valor ausente");
    return;
  }

  String parametro = server.arg("valor");

  // Aceita apenas números de 0 a 100.
  if (parametro.length() == 0) {
    server.send(400, "text/plain", "Valor invalido");
    return;
  }

  for (unsigned int i = 0; i < parametro.length(); i++) {
    if (parametro[i] < '0' || parametro[i] > '9') {
      server.send(400, "text/plain", "Valor invalido");
      return;
    }
  }

  int valor = parametro.toInt();

  if (valor < 0 || valor > 100) {
    server.send(400, "text/plain", "Fora da faixa");
    return;
  }

  velocidade = valor;

  // Atualiza imediatamente o PWM
  aplicarVelocidade();

  server.send(200, "text/plain", "OK");
}

// ======================================
// EVENTOS WI-FI
// ======================================

void eventoWiFi(WiFiEvent_t evento,
                WiFiEventInfo_t info) {

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

  Serial.println("=== MYTRAIN v0.5 ===");

  // Configura os pinos de direção e STBY.
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(STBY, OUTPUT);

  // Começa com a ponte H desabilitada.
  digitalWrite(STBY, LOW);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  // Configura PWM para a GPIO 5.
  pwmPronto = ledcAttach(PWMA, PWM_FREQ, PWM_RES);

  if (!pwmPronto) {
    Serial.println("[ERRO] Falha ao configurar PWM");
  } else {
    ledcWrite(PWMA, 0);

    // Habilita a ponte H com o motor parado.
    digitalWrite(STBY, HIGH);
  }

  pararMotor();

  // Conecta ao Wi-Fi.
  WiFi.onEvent(eventoWiFi);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // Configuração que funcionou na Super Mini.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  // Registra as rotas HTTP.
  server.on("/", HTTP_GET, mostrarPagina);
  server.on("/direcao", HTTP_POST, receberDirecao);
  server.on("/velocidade", HTTP_POST, receberVelocidade);

  server.begin();

  Serial.println("[WEB] Servidor iniciado");
}

// ======================================
// LOOP
// ======================================

void loop() {

  // Processa as requisições HTTP.
  server.handleClient();

  // Exibe o estado do Wi-Fi a cada 5 s.
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
