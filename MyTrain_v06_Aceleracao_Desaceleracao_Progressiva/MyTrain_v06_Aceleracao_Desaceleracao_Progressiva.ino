
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
// CONFIGURAÇÕES DO PWM
// ======================================

constexpr int PWM_FREQ = 5000;
constexpr int PWM_RES = 8;
constexpr int PWM_MINIMO = 20;

// A cada 50 ms, o PWM muda 2 pontos percentuais.
// 0 a 100% = aproximadamente 2,5 segundos.
constexpr unsigned long INTERVALO_RAMPA = 50;
constexpr int PASSO_RAMPA = 2;

// Tempo de espera após parar antes de
// permitir a inversão de direção.
constexpr unsigned long PAUSA_INVERSAO = 500;

// ======================================
// ESTADO DO MOTOR
// ======================================

// Velocidade solicitada pelo navegador
int velocidadeConfigurada = 60;

// PWM atualmente aplicado (em porcentagem)
int velocidadeAtual = 0;

//  0 = parado
//  1 = frente
// -1 = ré
int direcaoSolicitada = 0;

// Direção efetivamente aplicada na ponte H
int direcaoAtual = 0;

// Controle dos intervalos com millis()
unsigned long ultimoPasso = 0;
unsigned long inicioPausa = 0;

// Indica que estamos aguardando uma
// pausa antes de acionar outra direção.
bool aguardandoInversao = false;

// Indica se o PWM foi configurado corretamente
bool pwmPronto = false;

// ======================================
// FUNÇÕES DE BAIXO NÍVEL
// ======================================

// Converte porcentagem para duty cycle PWM
void escreverPWM(int porcentagem) {

  if (!pwmPronto) {
    return;
  }

  // Limita a porcentagem entre 0 e 100
  porcentagem = constrain(porcentagem, 0, 100);

  int duty = 0;

  if (porcentagem > 0) {

    // Converte 1-100% do navegador
    // para 20-100% de PWM real
    int pwmReal = map(
      porcentagem,
      1, 100,
      PWM_MINIMO, 100
    );

    // Converte o percentual real
    // para resolução de 8 bits (0-255)
    duty = map(pwmReal, 0, 100, 0, 255);
  }

  // Atualiza o PWM da ponte H
  ledcWrite(PWMA, duty);
}

// Define a direção nos pinos da ponte H.
// Deve ser chamada apenas com PWM zerado
// durante uma troca de direção.
void aplicarDirecao(int novaDirecao) {

  escreverPWM(0);

  if (novaDirecao == 1) {

    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);

    Serial.println("[MOTOR] Direcao aplicada: FRENTE");

  } else if (novaDirecao == -1) {

    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);

    Serial.println("[MOTOR] Direcao aplicada: RE");

  } else {

    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);

    Serial.println("[MOTOR] Direcao aplicada: PARADO");
  }

  direcaoAtual = novaDirecao;
}

// ======================================
// RAMPA DE ACELERAÇÃO E DESACELERAÇÃO
// ======================================

void atualizarMotor() {

  if (!pwmPronto) {
    return;
  }

  unsigned long agora = millis();

  // A rampa é atualizada em intervalos fixos.
  if (agora - ultimoPasso < INTERVALO_RAMPA) {
    return;
  }

  ultimoPasso = agora;

  // --------------------------------------
  // 1. VERIFICA SE A DIREÇÃO MUDOU
  // --------------------------------------

  if (direcaoAtual != direcaoSolicitada) {

    // Se ainda existe PWM aplicado,
    // desacelera primeiro até zero.
    if (velocidadeAtual > 0) {

      velocidadeAtual -= PASSO_RAMPA;

      if (velocidadeAtual < 0) {
        velocidadeAtual = 0;
      }

      escreverPWM(velocidadeAtual);
      return;
    }

    // PWM já chegou a zero.
    // Se ainda havia uma direção ativa,
    // desliga as saídas e inicia a pausa.
    if (direcaoAtual != 0) {

      aplicarDirecao(0);

      inicioPausa = agora;
      aguardandoInversao = true;

      Serial.println("[MOTOR] Aguardando parada...");
      return;
    }

    // Aguarda o intervalo de segurança.
    if (aguardandoInversao) {

      if (agora - inicioPausa < PAUSA_INVERSAO) {
        return;
      }

      aguardandoInversao = false;
    }

    // Agora podemos aplicar a nova direção.
    if (direcaoSolicitada != 0) {
      aplicarDirecao(direcaoSolicitada);
    }

    return;
  }

  // --------------------------------------
  // 2. VERIFICA SE O MOTOR ESTÁ PARADO
  // --------------------------------------

  if (direcaoAtual == 0) {
    velocidadeAtual = 0;
    escreverPWM(0);
    return;
  }

  // --------------------------------------
  // 3. AJUSTA A VELOCIDADE GRADUALMENTE
  // --------------------------------------

  if (velocidadeAtual < velocidadeConfigurada) {

    // Aceleração
    velocidadeAtual += PASSO_RAMPA;

    if (velocidadeAtual > velocidadeConfigurada) {
      velocidadeAtual = velocidadeConfigurada;
    }

  } else if (velocidadeAtual > velocidadeConfigurada) {

    // Desaceleração
    velocidadeAtual -= PASSO_RAMPA;

    if (velocidadeAtual < velocidadeConfigurada) {
      velocidadeAtual = velocidadeConfigurada;
    }
  }

  // Atualiza o sinal PWM na ponte H.
  escreverPWM(velocidadeAtual);
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

  <title>MyTrain v0.6</title>

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

    h1 { color: #2563eb; }

    .botoes {
      display: flex;
      gap: 8px;
    }

    button {
      flex: 1;
      padding: 15px 8px;
      border: 0;
      border-radius: 8px;
      background: #2563eb;
      color: white;
      font-size: 16px;
      cursor: pointer;
    }

    .parar { background: #dc2626; }

    input[type=range] {
      width: 100%;
      margin: 20px 0;
    }

    .estado {
      background: #f3f4f6;
      padding: 15px;
      border-radius: 8px;
      margin-top: 20px;
    }

    .barra {
      height: 12px;
      border-radius: 8px;
      background: #e5e7eb;
      overflow: hidden;
    }

    .progresso {
      width: 0%;
      height: 100%;
      background: #0d9488;
      transition: width 0.1s linear;
    }
  </style>
</head>

<body>
<div class="painel">

  <h1>MyTrain v0.6</h1>
  <h3>Controle de tração</h3>

  <div class="botoes">
    <button onclick="mudarDirecao(1)">Frente</button>
    <button class="parar"
            onclick="mudarDirecao(0)">Parar</button>
    <button onclick="mudarDirecao(-1)">Ré</button>
  </div>

  <h3>Velocidade desejada</h3>

  <h2><span id="valor">%VELOCIDADE%</span>%</h2>

  <input type="range"
         id="slider"
         min="0"
         max="100"
         value="%VELOCIDADE%"
         oninput="alterarVelocidade(this.value)">

  <div class="estado">
    <p>Direção: <strong id="direcao">Parado</strong></p>

    <p>Potência PWM aplicada:
       <strong id="atual">0%</strong>
    </p>

    <div class="barra">
      <div id="progresso" class="progresso"></div>
    </div>
  </div>

</div>

<script>

  let temporizador;
  let direcaoDesejada = 0;

  // Envia direção ao ESP32
  async function mudarDirecao(valor) {

    try {

      const resposta = await fetch(
        "/direcao?valor=" + valor,
        { method: "POST" }
      );

      if (!resposta.ok) {
        throw new Error("Falha ao definir direcao");
      }

      direcaoDesejada = valor;

    } catch (erro) {
      console.log(erro);
      alert("Erro ao enviar comando de direcao");
    }
  }

  // Controla o slider
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
      }

    }, 100);
  }

  // Consulta o estado real do ESP32.
  // O valor mostrado não é uma simulação:
  // vem da variável velocidadeAtual.
  async function consultarEstado() {

    try {

      const resposta = await fetch("/estado", {
        cache: "no-store"
      });

      if (!resposta.ok) {
        throw new Error("Sem resposta");
      }

      const dados = await resposta.json();

      document.getElementById("atual").innerText =
        dados.velocidadeAtual + "%";

      document.getElementById("progresso").style.width =
        dados.velocidadeAtual + "%";

      let texto = "Parado";

      if (dados.direcaoAtual === 1) {
        texto = "Frente";
      }

      if (dados.direcaoAtual === -1) {
        texto = "Ré";
      }

      if (dados.direcaoAtual === 0 &&
          dados.direcaoSolicitada !== 0) {
        texto = "Aguardando inversão";
      }

      document.getElementById("direcao").innerText =
        texto;

    } catch (erro) {
      document.getElementById("direcao").innerText =
        "Sem comunicação";
    }
  }

  // Atualiza os dados a cada 250 ms.
  setInterval(consultarEstado, 250);
  consultarEstado();

</script>
</body>
</html>
)rawliteral";

  html.replace(
    "%VELOCIDADE%",
    String(velocidadeConfigurada)
  );

  server.send(200, "text/html; charset=utf-8", html);
}

// ======================================
// ROTAS HTTP
// ======================================

void receberDirecao() {

  if (!server.hasArg("valor")) {
    server.send(400, "text/plain", "Valor ausente");
    return;
  }

  String parametro = server.arg("valor");

  if (parametro != "1" &&
      parametro != "0" &&
      parametro != "-1") {

    server.send(400, "text/plain", "Direcao invalida");
    return;
  }

  if (!pwmPronto) {
    server.send(503, "text/plain", "PWM indisponivel");
    return;
  }

  direcaoSolicitada = parametro.toInt();

  Serial.print("[HTTP] Direcao solicitada: ");
  Serial.println(direcaoSolicitada);

  server.send(200, "text/plain", "OK");
}

void receberVelocidade() {

  if (!server.hasArg("valor")) {
    server.send(400, "text/plain", "Valor ausente");
    return;
  }

  String parametro = server.arg("valor");

  if (parametro.isEmpty()) {
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

  velocidadeConfigurada = valor;

  Serial.print("[HTTP] Velocidade solicitada: ");
  Serial.print(valor);
  Serial.println("%");

  server.send(200, "text/plain", "OK");
}

void mostrarEstado() {

  String json = "{";

  json += "\"velocidadeAtual\":";
  json += String(velocidadeAtual);

  json += ",\"velocidadeConfigurada\":";
  json += String(velocidadeConfigurada);

  json += ",\"direcaoAtual\":";
  json += String(direcaoAtual);

  json += ",\"direcaoSolicitada\":";
  json += String(direcaoSolicitada);

  json += "}";

  server.send(200, "application/json", json);
}

// ======================================
// WI-FI
// ======================================

void eventoWiFi(WiFiEvent_t evento,
                WiFiEventInfo_t info) {

  switch (evento) {

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WIFI] Conectado");
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

  Serial.println("=== MYTRAIN v0.6 ===");

  // Configura as GPIOs
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(STBY, OUTPUT);

  // Motor desligado no início
  digitalWrite(STBY, LOW);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  // Configura PWM
  pwmPronto = ledcAttach(
    PWMA,
    PWM_FREQ,
    PWM_RES
  );

  if (pwmPronto) {

    escreverPWM(0);

    // Habilita a ponte H
    digitalWrite(STBY, HIGH);

    Serial.println("[MOTOR] Ponte H pronta");

  } else {

    Serial.println("[ERRO] Falha no PWM");
  }

  // Configura Wi-Fi
  WiFi.onEvent(eventoWiFi);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setAutoReconnect(true);

  WiFi.begin(ssid, password);

  // Configura rotas HTTP
  server.on("/", HTTP_GET, mostrarPagina);
  server.on("/direcao", HTTP_POST, receberDirecao);
  server.on("/velocidade", HTTP_POST, receberVelocidade);
  server.on("/estado", HTTP_GET, mostrarEstado);

  server.begin();

  Serial.println("[WEB] Servidor iniciado");
}

// ======================================
// LOOP
// ======================================

void loop() {

  // Processa requisições do navegador
  server.handleClient();

  // Atualiza a rampa do motor sem delay()
  atualizarMotor();

  // Monitoramento Wi-Fi
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

      // Solicita desaceleração quando
      // o ESP32 perde a rede Wi-Fi.
      direcaoSolicitada = 0;
    }
  }

  delay(1);
}
