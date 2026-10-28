
#include <WiFi.h>
#include <WebServer.h>

// GPIO onde o LED está conectado
constexpr int LED_PIN = 0;

// Dados da rede Wi-Fi criada pelo ESP32
const char* wifiName = "MyTrain";
const char* wifiPassword = "mytrain";

// Servidor HTTP na porta 80
WebServer server(80);

// Guarda o estado atual do LED
bool ledLigado = false;

// Monta a página web
void mostrarPagina() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="pt-br">
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <meta charset="UTF-8">
      <title>MyTrain</title>

      <style>
        body {
          font-family: Arial, sans-serif;
          text-align: center;
          background: #f3f4f6;
          padding: 35px 15px;
        }

        button {
          padding: 18px 40px;
          font-size: 20px;
          border: none;
          border-radius: 10px;
          background: #2563eb;
          color: white;
          cursor: pointer;
        }
      </style>
    </head>

    <body>
      <h2>MyTrain</h2>
      <p>LED: <strong id="estado">DESLIGADO</strong></p>
      <button id="botao" onclick="alternarLED()">Ligar LED</button>

      <script>
        async function alternarLED() {
          const resposta = await fetch('/toggle');
          const estado = await resposta.text();

          document.getElementById('estado').innerText =
            estado === 'ON' ? 'LIGADO' : 'DESLIGADO';

          document.getElementById('botao').innerText =
            estado === 'ON' ? 'Desligar LED' : 'Ligar LED';
        }

        async function atualizarEstado() {
          const resposta = await fetch('/estado');
          const estado = await resposta.text();

          document.getElementById('estado').innerText =
            estado === 'ON' ? 'LIGADO' : 'DESLIGADO';

          document.getElementById('botao').innerText =
            estado === 'ON' ? 'Desligar LED' : 'Ligar LED';
        }

        atualizarEstado();
      </script>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html; charset=utf-8", html);
}

// Alterna o LED entre ligado e desligado
void alternarLED() {
  ledLigado = !ledLigado;

  digitalWrite(LED_PIN, ledLigado ? HIGH : LOW);

  server.send(200, "text/plain", ledLigado ? "ON" : "OFF");

  Serial.println(ledLigado ? "LED ligado" : "LED desligado");
}

// Retorna o estado atual sem alterá-lo
void consultarEstado() {
  server.send(200, "text/plain", ledLigado ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);

  // Configura o LED como saída e inicia desligado
  digitalWrite(LED_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);

  // Cria uma rede Wi-Fi própria
  WiFi.mode(WIFI_AP);

  // Cria a rede no canal 1 e deixa seu nome visível
  bool wifiIniciado = WiFi.softAP(
    wifiName,
    wifiPassword,
    1,  // Canal Wi-Fi
    0   // 0 = rede visível
  );

  // Verifica se a criação da rede funcionou
  if (wifiIniciado) {
    Serial.println("Wi-Fi criado com sucesso!");
    Serial.print("Nome: ");
    Serial.println(WiFi.softAPSSID());
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("MAC: ");
    Serial.println(WiFi.softAPmacAddress());
  } else {
    Serial.println("ERRO: falha ao criar a rede Wi-Fi!");
  }

  // Define as rotas do servidor
  server.on("/", mostrarPagina);
  server.on("/toggle", alternarLED);
  server.on("/estado", consultarEstado);

  // Inicia o servidor
  server.begin();

  Serial.println("MyTrain iniciado!");
  Serial.print("Wi-Fi: ");
  Serial.println(wifiName);
  Serial.print("Endereco: http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  // Processa as requisições recebidas do navegador
  server.handleClient();

  // Mostra uma mensagem a cada 3 segundos
  static unsigned long ultimoTeste = 0;

  if (millis() - ultimoTeste >= 3000) {
    ultimoTeste = millis();
    Serial.println("MyTrain executando...");
  }
}
