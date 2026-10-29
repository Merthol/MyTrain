
#include <Arduino.h>

// ======================================
// PINOS DO TB6612FNG
// ======================================

constexpr int AIN1 = 3;
constexpr int AIN2 = 4;
constexpr int PWMA = 5;
constexpr int STBY = 6;

// Tempo de rotação e pausa (milissegundos)
constexpr int TEMPO_ROTACAO = 3000;
constexpr int TEMPO_PAUSA = 1500;

// ======================================
// FUNÇÕES DE CONTROLE DO MOTOR
// ======================================

// Gira no primeiro sentido
void girarFrente() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(PWMA, HIGH);

  Serial.println("[MOTOR] Girando para frente");
}

// Gira no sentido contrário
void girarReverso() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(PWMA, HIGH);

  Serial.println("[MOTOR] Girando em reverso");
}

// Para o motor (roda livre)
void pararMotor() {
  digitalWrite(PWMA, LOW);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  Serial.println("[MOTOR] Parado");
}

// ======================================
// SETUP
// ======================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println("=== MYTRAIN - TESTE DE DIRECAO  ===");

  // Inicialmente, mantém o driver desabilitado
  digitalWrite(STBY, LOW);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(PWMA, LOW);

  // Configura os pinos de controle
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(STBY, OUTPUT);

  // Mantém o motor parado inicialmente
  pararMotor();

  // Habilita a ponte H
  digitalWrite(STBY, HIGH);

  Serial.println("[SISTEMA] Ponte H habilitada");
}

// ======================================
// LOOP - SEQUÊNCIA CONTÍNUA
// ======================================

void loop() {

  // 1. Gira para frente
  girarFrente();
  delay(TEMPO_ROTACAO);

  // 2. Para
  pararMotor();
  delay(TEMPO_PAUSA);

  // 3. Gira em reverso
  girarReverso();
  delay(TEMPO_ROTACAO);

  // 4. Para novamente
  pararMotor();
  delay(TEMPO_PAUSA);

  // Retorna automaticamente ao início do loop
}
