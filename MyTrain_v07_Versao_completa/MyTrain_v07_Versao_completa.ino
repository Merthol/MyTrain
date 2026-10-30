
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// MYTRAIN v0.8
// ==========================================


/*MYTRAIN v0.8 - CONFIGURACAO FISICA DO CIRCUITO
 * ============================================================
 *
 * CONTROLADOR:
 *   ESP32-C3 Super Mini
 *
 * DRIVER DOS MOTORES:
 *   TB6612FNG - Ponte H dupla
 *
 * ALIMENTACAO:
 *   ESP32: USB
 *   Motores: 2 pilhas AA NiMH recarregaveis
 *   Logica TB6612FNG: 3.3V do ESP32
 *   GND compartilhado entre os componentes
 *
 * ============================================================
 * POSICIONAMENTO NA PROTOBOARD
 * ============================================================
 *
 * ESP32-C3:
 *   Linhas 1 a 8
 *   Trilha H: 5V na linha 1 / GPIO 0 na linha 8
 *   Trilha D: GPIO 5 na linha 1 / GPIO 21 na linha 8
 *
 * TB6612FNG:
 *   Linhas 23 a 30
 *   Trilha H: VM na linha 30 / GND na linha 23
 *   Trilha D: PWMA na linha 30 / GND na linha 23
 *
 * Preferencia de conexao:
 *   Trilha J para pinos na trilha H
 *   Trilha A para pinos na trilha D
 *
 * ============================================================
 * CONEXOES - ESP32 -> TB6612FNG
 * ============================================================
 *
 * J5 (GPIO 3)  -> A28 (AIN1) E A26 (BIN1)
 * J4 (GPIO 4)  -> A29 (AIN2) E A25 (BIN2)
 * A1 (GPIO 5)  -> A30 (PWMA)
 * B1 (GPIO 5)  -> A24 (PWMB)
 * A2 (GPIO 6)  -> A27 (STBY)
 *
 * J3 (3V3)     -> J29 (VCC)
 * J2 (GND)     -> J28 (GND)
 *
 * ============================================================
 * ALIMENTACAO E MOTORES
 * ============================================================
 *
 * Pilhas (+)   -> J30 (VM)
 * Pilhas (-)   -> J23 (GND)
 *
 * Motor A      -> J27 (AO1) e J26 (AO2)
 * Motor B      -> J24 (BO1) e J25 (BO2)
 *
 * Os motores A e B compartilham o sinal PWM da GPIO 5.
 *
 * ============================================================
 * ILUMINACAO
 * ============================================================
 *
 * FAROIS CENTRAIS - BRANCOS
 *   GPIO 1 -> J7
 *
 *   Dois LEDs brancos conectados a mesma GPIO.
 *   Ambos acendem e apagam simultaneamente.
 *   Controle independente pelo navegador.
 *   Acendimento e apagamento progressivos.
 *
 *   IMPORTANTE:
 *   Cada LED deve possuir seu proprio resistor em serie.
 *
 * ILUMINACAO INTERNA - LARANJA
 *   GPIO 2 -> J6
 *
 *   Controle independente pelo navegador.
 *   Acendimento e apagamento progressivos.
 *
 * LUZ LATERAL ESQUERDA - LARANJA
 *   GPIO 7 -> A3
 *
 * LUZ LATERAL DIREITA - LARANJA
 *   GPIO 8 -> A4
 *
 *   Alternancia automatica durante o movimento.
 *   Intervalo variavel conforme a velocidade logica:
 *     Motor lento: aproximadamente 2 segundos
 *     Motor rapido: aproximadamente 1 segundo
 *   Acendimento e apagamento progressivos.
 *
 * FAROL INFERIOR - CODIGO DESATIVADO
 *   GPIO 1 agora pertence aos dois farois dianteiros.
 *   Nao inicializar um segundo canal PWM neste pino.
 *
 * LUZ TRASEIRA - VERMELHA
 *   Atualmente desconectada, SEM GPIO reservado.
 *   GPIO 8 esta ocupada pela luz lateral direita.
 *
 * ============================================================
 * OBSERVACOES
 * ============================================================
 *
 * - Todos os LEDs devem ter resistores individuais.
 * - Nao alimentar motores diretamente pelas GPIOs.
 * - O PWM dos motores e compartilhado.
 * - PWM maximo em re: 100% (configuracao atual).
 * - Rampa progressiva de aceleracao e desaceleracao.
 * - Parada automatica solicitada na perda de comunicacao.
 *
 * ============================================================
 */

// REDE WI-FI
// "ssid" e "password" identificam o roteador ao qual o ESP32 se conecta.
// ATENCAO: evite publicar este arquivo com credenciais reais.
const char* ssid = "guilherme.wifi";
const char* password = "deusa2512";

// Cria um servidor HTTP que escuta conexoes na porta TCP 80.
WebServer server(80);

// ==========================================
// PONTE H - TB6612FNG
// ==========================================

// Motor A: cada entrada determina um dos lados da ponte H.
// HIGH/LOW e LOW/HIGH selecionam sentidos opostos de rotacao.
constexpr int AIN1 = 3;  // GPIO 3 -> TB6612 AIN1 (J5 -> A28)
constexpr int AIN2 = 4;  // GPIO 4 -> TB6612 AIN2 (J4 -> A29)

// GPIO 5 gera um unico PWM conectado a PWMA E PWMB.
// Assim os dois motores recebem o mesmo ciclo ativo (duty cycle).
constexpr int PWMA = 5;  // A1 -> A30; B1 -> A24

// Standby: LOW deixa o driver desabilitado; HIGH habilita suas saidas.
// Nao transporta comandos de direcao: somente habilita/desabilita o driver.
constexpr int STBY = 6;  // GPIO 6 -> STBY (A2 -> A27)

// PWMB também está conectado à GPIO 5.
// Assim, os dois motores compartilham PWM.

// AIN1/BIN1 e AIN2/BIN2 usam GPIOs compartilhadas.
// Se um motor girar ao contrario, inverta os fios desse motor no TB6612.

// ==========================================
// LEDS
// ==========================================

// Os numeros abaixo sao GPIOs, nao numeros de canais PWM.
// ATUALIZACAO FISICA: DOIS farois brancos compartilham GPIO 1;
// cada um deve ter seu resistor em serie.
constexpr int FAROL_SUPERIOR = 1;  // GPIO 1 / J7: dois farois brancos juntos
constexpr int LUZ_INTERNA = 2;     // GPIO 2 / J6: LED laranja interno

// Antigo farol inferior: codigo comentado mais abaixo, sem pino reservado. 

// Luzes laranjas da frente: alternam enquanto o trem e acionado.
constexpr int LUZ_ESQUERDA = 7; // GPIO 7 / A3
constexpr int LUZ_DIREITA = 8;  // GPIO 8 / A4

// LED traseiro desconectado; GPIO 8 agora e exclusivamente da lateral direita.
// A rotina experimental de PWM traseiro fica desativada.

// CANAIS PWM (LEDC) DO ESP32-C3
// Canal LEDC e diferente de GPIO: um canal gera o sinal PWM, que depois
// e associado a um pino por ledcAttachChannel(). Ha 6 canais disponiveis.
// CH_MOTOR e compartilhado entre os dois motores, pela fiacao na protoboard.
constexpr int CH_MOTOR = 0;
constexpr int CH_FAROL_SUP = 1;
constexpr int CH_FAROL_INF = 2;
constexpr int CH_INTERIOR = 3;
constexpr int CH_ESQUERDA = 4;
constexpr int CH_DIREITA = 5;

// ==========================================
// CONFIGURAÇÕES
// ==========================================

// Frequencias do PWM, em hertz (ciclos por segundo):
// 5000 Hz para motores; 1000 Hz para LEDs.
constexpr int PWM_FREQ_MOTOR = 5000;
constexpr int PWM_FREQ_LED = 1000;
// Resolucao de 8 bits: duty cycle inteiro entre 0 (0%) e 255 (100%).
constexpr int PWM_RES = 8;

// Compensacao para vencer o atrito de partida do N20:
// 0% = desligado; qualquer valor positivo comeca com no minimo 20% de PWM.
// Isso e percentual de acionamento, nao velocidade real medida.
constexpr int PWM_MINIMO = 20;

// Limite atual em re: 100%. Ajuste se necessario apos testes mecanicos.
constexpr int PWM_MAX_RE = 100;

// Rampa linear: varia a "velocidadeAtual" em 2 pontos percentuais
// a cada 50 ms. Percorrer 0..100% leva aproximadamente 2,5 segundos.
// O timer nao bloqueia as demais tarefas, pois utiliza millis().
constexpr int PASSO_RAMPA = 2;
constexpr unsigned long INTERVALO_RAMPA = 50;

// Depois que a rampa chega a zero, aguarda 500 ms antes de
// habilitar a direcao oposta. Nao mede se o eixo parou fisicamente.
constexpr unsigned long PAUSA_INVERSAO = 500;

// Intervalo entre atualizacoes de brilho dos LEDs (25 ms).
// Quanto menor, mais frequentes sao os passos de transicao.
constexpr unsigned long INTERVALO_LUZ = 25;

// Intervalo entre trocas dos LEDs laterais, em milissegundos.
// ATENCAO: este arquivo atualmente usa 1 s (rapido) e 2 s (lento),
// conforme os valores abaixo; nao 1,5 s e 4 s de uma sugestao anterior.
constexpr unsigned long INTERVALO_MINIMO = 1000; // Valor para motor rapido
constexpr unsigned long INTERVALO_MAXIMO = 2000; // Valor para motor lento
// Periodo da onda triangular traseira; codigo reservado, LED desconectado.
constexpr unsigned long PERIODO_TRASEIRO = 2000;

// Watchdog de comunicacao: se nenhuma solicitacao de controle/estado
// chegar por 3000 ms, o firmware solicita a parada gradual dos motores.
// Nao substitui uma parada de emergencia fisica.
constexpr unsigned long TIMEOUT_CONTROLE = 3000;


// IMPULSO DE PARTIDA (somente ao sair efetivamente de PWM zero).
// Permanece com PWM alto por 150 ms; depois sobe/desce gradualmente
// ate o alvo pedido no slider, SEM retornar ao inicio da rampa.
constexpr unsigned long DURACAO_IMPULSO = 150;
constexpr int PWM_IMPULSO_FRENTE = 80;  // 80% de duty, nao 80% de RPM
constexpr int PWM_IMPULSO_RE = 80;      // limite configuravel, maximo PWM_MAX_RE
// Passo do PWM efetivo a cada INTERVALO_RAMPA (50 ms).
constexpr int PASSO_RAMPA_PWM = 2;

// Uma partida e habilitada novamente somente depois de chegar a PWM zero.
bool partidaArmada = true;
bool impulsoAtivo = false;
unsigned long inicioImpulso = 0;


// ==========================================
// ESTADOS
// ==========================================

// Estados de direcao: 0 = parado; 1 = frente; -1 = re.
// "Solicitada" recebe o comando HTTP; "Atual" so muda quando a ponte H
// efetivamente troca de sentido, apos a desaceleracao e eventual pausa.
int direcaoSolicitada = 0;
int direcaoAtual = 0;

// Porcentagens de 0 a 100: a desejada vem do slider;
// a atual se aproxima gradualmente da desejada na rampa.
// O valor inicial 60 e apenas a posicao inicial do slider, nao liga motor.
int velocidadeDesejada = 60;
int velocidadeAtual = 0;

// PWM de 0..100% apos compensacao de partida e limite de re.
// Nao corresponde necessariamente a RPM ou a tensao exata no motor.
int pwmEfetivo = 0;

// Estado logico dos dois controles manuais da pagina.
// O brilho de cada LED ainda muda gradualmente para seguir esse estado.
bool farolLigado = false;
bool interiorLigado = false;

// Guarda o instante da ultima troca e qual lado deve ficar aceso.
// A alternancia usa millis() e intervalo calculado pela velocidade.
unsigned long ultimaAlternancia = 0;
bool ladoEsquerdoAtivo = true;

// Brilho PWM dos LEDs (escala de 8 bits, 0..255).
// Cada valor se aproxima gradualmente de um alvo definido pela logica.
// A traseira permanece sem saida ativa no loop, conforme teste anterior.
int brilhoSuperior = 0;
int brilhoInferior = 0;
int brilhoInterior = 0;
int brilhoEsquerda = 0;
int brilhoDireita = 0;
int brilhoTraseira = 0;

// Estado da espera de seguranca antes de mudar o sentido dos motores.
// inicioPausa guarda o instante em que a desaceleracao chegou a zero.
bool pausaAtiva = false;
unsigned long inicioPausa = 0;

// Marcadores de tempo em milissegundos desde o boot:
// ultimoPasso -> ultimo ajuste da rampa;
// ultimaLuz -> ultima atualizacao dos LEDs;
// ultimoContato -> ultima solicitacao HTTP de direcao/velocidade/estado.
unsigned long ultimoPasso = 0;
unsigned long ultimaLuz = 0;
unsigned long ultimoContato = 0;

// Fica true somente se a configuracao dos canais PWM tiver sucesso.
// Impede que as rotinas de motor/luz executem sem a inicializacao prevista.
bool hardwarePronto = false;

// ==========================================
// FUNÇÕES AUXILIARES
// ==========================================

// Aproxima um valor atual de um alvo,
// respeitando o passo máximo por chamada.
int aproximar(int atual, int alvo, int passo) {

  if (atual < alvo) {
    return min(atual + passo, alvo);
  }

  if (atual > alvo) {
    return max(atual - passo, alvo);
  }

  return atual;
}

// Gera uma onda triangular entre 0 e 255: sobe gradualmente e depois
// desce gradualmente dentro do periodo informado (usada na luz traseira).
// Usa modulo (%) para reiniciar o ciclo automaticamente.
int ondaTriangular(unsigned long tempo,
                  unsigned long periodo) {

  unsigned long fase = tempo % periodo;
  unsigned long metade = periodo / 2;

  if (fase < metade) {
    return (fase * 255UL) / metade;
  }

  return ((periodo - fase) * 255UL) / metade;
}

// Converte 1..100% do slider para uma faixa de PWM efetivo:
// em frente: 20..100%; em re: 20..PWM_MAX_RE. Se velocidade=0, retorna 0.
// A funcao map() utiliza aritmetica inteira, podendo arredondar valores.
int calcularPWM(int velocidade, int direcao) {

  if (velocidade <= 0 || direcao == 0) {
    return 0;
  }

  int maximo = (direcao == -1)
                 ? PWM_MAX_RE
                 : 100;

  return map(
    constrain(velocidade, 1, 100),
    1, 100,
    PWM_MINIMO, maximo
  );
}

// ==========================================
// CONTROLE FÍSICO DOS MOTORES
// ==========================================

// Atualiza ambas as pontes por AIN1/BIN1 (GPIO3) e AIN2/BIN2 (GPIO4).

void aplicarDirecaoMotor(int sentido) {

  // Desliga o PWM antes de alterar a direção dos motores.
  ledcWriteChannel(CH_MOTOR, 0);

  if (sentido == 1) {

    // Frente: ambos os motores recebem o mesmo sinal.
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);

  } else if (sentido == -1) {

    // Ré: inverte os sinais dos dois motores.
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);

  } else {

    // Parado: desativa os sinais de direção.
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
  }

  // Registra a direção aplicada.
  direcaoAtual = sentido;
}

// Aplica o percentual de PWM efetivo aos dois canais da ponte H.
// O PWM em 8 bits vai de 0 a 255; pwmEfetivo vai de 0 a 100.
void escreverPWMMotores() {
  int duty = map(constrain(pwmEfetivo, 0, 100), 0, 100, 0, 255);
  ledcWriteChannel(CH_MOTOR, duty);
}

// Atua somente durante movimento no MESMO sentido.
// Impulso ocorre uma unica vez quando pwmEfetivo estava em 0.
// Alterar o slider durante a marcha nao reinicia o impulso.
void atualizarPWMMotores(unsigned long agora) {

  int alvo = calcularPWM(velocidadeDesejada, direcaoAtual);

  // Slider em zero: desacelera normalmente e cancela qualquer impulso.
  if (alvo == 0) {
    impulsoAtivo = false;
    pwmEfetivo = aproximar(pwmEfetivo, 0, PASSO_RAMPA_PWM);
    if (pwmEfetivo == 0) partidaArmada = true;
    escreverPWMMotores();
    return;
  }

  // So comeca o impulso se os motores estavam totalmente desligados.
  if (partidaArmada && pwmEfetivo == 0) {
    int potenciaPartida = (direcaoAtual == 1)
      ? PWM_IMPULSO_FRENTE : PWM_IMPULSO_RE;

    int limiteDirecao = (direcaoAtual == -1) ? PWM_MAX_RE : 100;
    pwmEfetivo = constrain(potenciaPartida, 0, limiteDirecao);

    inicioImpulso = agora;
    impulsoAtivo = true;
    partidaArmada = false;

    Serial.print("[MOTOR] Impulso de partida: ");
    Serial.print(pwmEfetivo);
    Serial.println("%");
  }

  // Mantem o impulso pelo tempo configurado, sem queda prematura.
  if (impulsoAtivo) {
    if (agora - inicioImpulso < DURACAO_IMPULSO) {
      escreverPWMMotores();
      return;
    }
    impulsoAtivo = false;
  }

  // Depois do impulso, ajusta a partir do PWM que ja esta aplicado.
  // Ex.: 80 -> 60, ou 80 -> 100; nunca 80 -> 20 -> alvo.
  pwmEfetivo = aproximar(pwmEfetivo, alvo, PASSO_RAMPA_PWM);
  escreverPWMMotores();
}

// Maquina de estados da tracao: executada a cada INTERVALO_RAMPA.
// Numa inversao, desacelera PWM REAL e velocidade logica ate zero,
// respeita PAUSA_INVERSAO e so entao aciona a direcao oposta.
void atualizarMotores() {
  if (!hardwarePronto) return;

  unsigned long agora = millis();
  if (agora - ultimoPasso < INTERVALO_RAMPA) return;
  ultimoPasso = agora;

  // Mudanca de direcao, inclusive comando PARAR (direcao=0).
  if (direcaoAtual != direcaoSolicitada) {

    // Nunca gerar impulso enquanto reduz para parar/inverter.
    impulsoAtivo = false;

    velocidadeAtual = aproximar(velocidadeAtual, 0, PASSO_RAMPA);
    pwmEfetivo = aproximar(pwmEfetivo, 0, PASSO_RAMPA_PWM);
    escreverPWMMotores();

    // Nao troca a ponte H enquanto restar PWM, nem se o indicador
    // de velocidade logica ainda estiver acima de zero.
    if (pwmEfetivo > 0 || velocidadeAtual > 0) return;

    partidaArmada = true;

    if (direcaoAtual != 0) {
      aplicarDirecaoMotor(0);
      inicioPausa = agora;
      pausaAtiva = true;
      Serial.println("[MOTOR] Parado para inversao");
      return;
    }

    if (pausaAtiva) {
      if (agora - inicioPausa < PAUSA_INVERSAO) return;
      pausaAtiva = false;
    }

    if (direcaoSolicitada != 0) {
      aplicarDirecaoMotor(direcaoSolicitada);
      Serial.print("[MOTOR] Direcao: ");
      Serial.println(direcaoAtual);
    }
    return;
  }

  if (direcaoAtual == 0) {
    velocidadeAtual = 0;
    pwmEfetivo = 0;
    impulsoAtivo = false;
    partidaArmada = true;
    escreverPWMMotores();
    return;
  }

  // A variavel logica continua com sua rampa original (inclusive luzes).
  // O PWM real possui rampa propria, iniciada pelo impulso de partida.
  velocidadeAtual = aproximar(
    velocidadeAtual, velocidadeDesejada, PASSO_RAMPA
  );
  atualizarPWMMotores(agora);
}

// ==========================================
// ILUMINAÇÃO
// ==========================================

// Cada chamada valida o intervalo de 25 ms antes de mudar os brilhos.
// Esse processo e independente dos ajustes da rampa dos motores.
void atualizarLuzes() {

  if (!hardwarePronto) return;

  unsigned long agora = millis();

  if (agora - ultimaLuz < INTERVALO_LUZ) {
    return;
  }

  ultimaLuz = agora;

  // ----------------------------------------
  // FAROL SUPERIOR
  // ----------------------------------------

  int alvoSuperior = farolLigado ? 255 : 0;

  brilhoSuperior = aproximar(
    brilhoSuperior, alvoSuperior, 5
  );

  ledcWriteChannel(
    CH_FAROL_SUP,
    brilhoSuperior
  );

  // ----------------------------------------
  // FAROL INFERIOR
  // Vinculado ao PWM real dos motores
  // ----------------------------------------

  // int alvoInferior = map(
  //   pwmEfetivo, 0, 100, 0, 255
  // );

  // brilhoInferior = aproximar(
  //   brilhoInferior, alvoInferior, 8
  // );

  // ledcWriteChannel(
  //   CH_FAROL_INF,
  //   brilhoInferior
  // );

  // ----------------------------------------
  // ILUMINAÇÃO INTERNA
  // ----------------------------------------

  int alvoInterior = interiorLigado ? 255 : 0;

  brilhoInterior = aproximar(
    brilhoInterior, alvoInterior, 5
  );

  ledcWriteChannel(
    CH_INTERIOR,
    brilhoInterior
  );

  // ----------------------------------------
  // LUZES LATERAIS ALTERNADAS
  // Velocidade de alternância variável
  // ----------------------------------------

  bool emMovimento = (
    direcaoAtual != 0 &&
    pwmEfetivo > 0
  );

  int alvoEsquerda = 0;
  int alvoDireita = 0;

  if (emMovimento) {

    // Calcula o intervalo conforme a
    // velocidade atual da locomotiva.
    // ATUAL: usa velocidadeAtual (escala do slider/rampa), nao pwmEfetivo.
    // Em re, a alternancia pode ficar rapida mesmo com PWM limitado a 50%.
    unsigned long intervalo = map(
      velocidadeAtual,
      1, 100,
      INTERVALO_MAXIMO,
      INTERVALO_MINIMO
    );

    // Verifica se chegou o momento de alternar
    if (agora - ultimaAlternancia >= intervalo) {

      ultimaAlternancia = agora;
      ladoEsquerdoAtivo = !ladoEsquerdoAtivo;
    }

    // Define qual LED deverá estar aceso
    if (ladoEsquerdoAtivo) {
      alvoEsquerda = 255;
      alvoDireita = 0;
    } else {
      alvoEsquerda = 0;
      alvoDireita = 255;
    }

  } else {

    // Reinicia o ciclo quando o motor para
    ultimaAlternancia = agora;
    ladoEsquerdoAtivo = true;
  }

  // Mantém o acendimento e apagamento gradual
  brilhoEsquerda = aproximar(
    brilhoEsquerda, alvoEsquerda, 8
  );

  brilhoDireita = aproximar(
    brilhoDireita, alvoDireita, 8
  );

  // Atualiza os LEDs por PWM
  ledcWriteChannel(CH_ESQUERDA, brilhoEsquerda);
  ledcWriteChannel(CH_DIREITA, brilhoDireita);

  // ----------------------------------------
  // LUZ TRASEIRA VERMELHA
  // Só pisca ao receber potência em ré
  // ----------------------------------------

  int alvoTraseira = 0;

  if (direcaoAtual == -1 && pwmEfetivo > 0) {

    alvoTraseira = ondaTriangular(
      agora, PERIODO_TRASEIRO
    );
  }

  brilhoTraseira = aproximar(
    brilhoTraseira,
    alvoTraseira,
    8
  );
}

// Implementacao EXPERIMENTAL e DESATIVADA de PWM por software traseiro.
// Como depende das chamadas ao loop(), pode piscar irregularmente.
// Atualmente NAO e chamada no loop() e o LED esta desconectado.
/* Funcao antiga desativada: GPIO 8 foi reaproveitada pela luz lateral.
void atualizarPWMTraseiro() {
  constexpr unsigned long PERIODO_US = 5000;
  unsigned long fase = micros() % PERIODO_US;
  unsigned long tempoLigado =
    ((unsigned long)brilhoTraseira * PERIODO_US) / 255;
  digitalWrite(LUZ_TRASEIRA, fase < tempoLigado ? HIGH : LOW);
}
*/

// ==========================================
// PÁGINA WEB
// ==========================================

// Requisicao GET /: monta e envia ao navegador o HTML, CSS e JS.
// O navegador executa JavaScript e envia comandos de volta por HTTP.
void mostrarPagina() {

  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
<meta charset="UTF-8">
<meta name="viewport"
      content="width=device-width,initial-scale=1">
<title>MyTrain v0.8</title>

<style>
body {
  font-family: Arial, sans-serif;
  background: #f3f4f6;
  text-align: center;
  padding: 20px 12px;
}
.painel {
  max-width: 460px;
  margin: auto;
  padding: 24px;
  background: white;
  border-radius: 16px;
}
h1 { color: #2563eb; }
.botoes {
  display: flex;
  gap: 8px;
}
button {
  border: 0;
  border-radius: 9px;
  padding: 14px 10px;
  color: white;
  background: #2563eb;
  font-size: 15px;
  cursor: pointer;
}
.botoes button { flex: 1; }
.parar { background: #dc2626; }
input[type=range] {
  width: 100%;
  margin: 18px 0;
}
.secao {
  margin-top: 26px;
  padding-top: 15px;
  border-top: 1px solid #ddd;
}
.luz {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 0;
}
.luz button {
  min-width: 100px;
  background: #64748b;
}
.luz button.ativo {
  background: #0d9488;
}
.barra {
  height: 12px;
  background: #e5e7eb;
  border-radius: 10px;
  overflow: hidden;
}
.progresso {
  height: 100%;
  width: 0%;
  background: #0d9488;
}
.estado {
  background: #f3f4f6;
  padding: 15px;
  margin-top: 16px;
  border-radius: 9px;
}
small { color: #64748b; }
</style>
</head>

<body>
<div class="painel">

<h1>MyTrain v0.8</h1>
<h3>Controle da locomotiva F7</h3>

<div class="secao">
  <h3>Tração — 2 motores</h3>

  <div class="botoes">
    <button onclick="direcao(1)">Frente</button>
    <button class="parar"
            onclick="direcao(0)">Parar</button>
    <button onclick="direcao(-1)">Ré</button>
  </div>

  <h3>Velocidade desejada</h3>

  <h2><span id="valor">%VELOCIDADE%</span>%</h2>

  <input type="range"
         min="0" max="100"
         value="%VELOCIDADE%"
         oninput="velocidade(this.value)">

  <small>Em ré, o PWM máximo configurado é 100%.</small>

  <div class="estado">
    <p>Direção: <b id="dirAtual">Parado</b></p>
    <p>PWM aplicado: <b id="pwmAtual">0%</b></p>
    <div class="barra">
      <div class="progresso" id="barra"></div>
    </div>
  </div>
</div>

<div class="secao">
  <h3>Iluminação</h3>

  <div class="luz">
    <span>Farol superior (branco)</span>
    <button id="btnFarol"
            onclick="alternarLuz('farol')">
      Desligado
    </button>
  </div>

  <div class="luz">
    <span>Luz interna (laranja)</span>
    <button id="btnInterior"
            onclick="alternarLuz('interior')">
      Desligado
    </button>
  </div>

  <small>
    Faróis brancos: controle manual.
    Luzes laterais: alternadas em movimento.
    Luz traseira: desativada.
  </small>
</div>

<p id="comunicacao">Conectando...</p>

</div>

<script>

let timerVelocidade;
let estadoRecebido = {};
let consultando = false;

async function enviar(url) {

  const resposta = await fetch(url, {
    method: "POST"
  });

  if (!resposta.ok) {
    throw new Error("Comando nao aceito");
  }
}

async function direcao(valor) {

  try {
    await enviar("/direcao?valor=" + valor);
  } catch (erro) {
    document.getElementById("comunicacao")
      .innerText = "Erro no comando de direção";
  }
}

function velocidade(valor) {

  document.getElementById("valor")
    .innerText = valor;

  clearTimeout(timerVelocidade);

  timerVelocidade = setTimeout(async () => {
    try {
      await enviar("/velocidade?valor=" + valor);
    } catch (erro) {
      document.getElementById("comunicacao")
        .innerText = "Erro no comando de velocidade";
    }
  }, 100);
}

async function alternarLuz(tipo) {

  const atual = tipo === "farol"
    ? estadoRecebido.farol
    : estadoRecebido.interior;

  const novo = atual ? 0 : 1;

  try {
    await enviar(
      "/luz?tipo=" + tipo + "&valor=" + novo
    );
    await consultarEstado();
  } catch (erro) {
    document.getElementById("comunicacao")
      .innerText = "Erro ao controlar iluminação";
  }
}

async function consultarEstado() {

  if (consultando) return;
  consultando = true;

  try {

    const resposta = await fetch("/estado", {
      cache: "no-store"
    });

    if (!resposta.ok) {
      throw new Error("Sem resposta");
    }

    const dados = await resposta.json();

    estadoRecebido = dados;

    let texto = "Parado";

    if (dados.direcaoAtual === 1) {
      texto = "Frente";
    } else if (dados.direcaoAtual === -1) {
      texto = "Ré";
    } else if (dados.direcaoSolicitada !== 0) {
      texto = "Aguardando inversão";
    }

    document.getElementById("dirAtual")
      .innerText = texto;

    document.getElementById("pwmAtual")
      .innerText = dados.pwmEfetivo + "%";

    document.getElementById("barra")
      .style.width = dados.pwmEfetivo + "%";

    const btnFarol =
      document.getElementById("btnFarol");

    const btnInterior =
      document.getElementById("btnInterior");

    btnFarol.innerText =
      dados.farol ? "Ligado" : "Desligado";

    btnInterior.innerText =
      dados.interior ? "Ligado" : "Desligado";

    btnFarol.classList.toggle(
      "ativo", dados.farol
    );

    btnInterior.classList.toggle(
      "ativo", dados.interior
    );

    document.getElementById("comunicacao")
      .innerText = "Conectado";

  } catch (erro) {

    document.getElementById("comunicacao")
      .innerText = "Sem comunicação com o MyTrain";

  } finally {
    consultando = false;
  }
}

setInterval(consultarEstado, 300);
consultarEstado();

</script>
</body>
</html>
)rawliteral";

  html.replace(
    "%VELOCIDADE%",
    String(velocidadeDesejada)
  );

  server.send(200, "text/html; charset=utf-8", html);
}

// ==========================================
// ROTAS HTTP
// ==========================================
// O WebServer associa um endereco e um metodo HTTP a uma funcao C++.
// Ex.: POST /direcao?valor=1 chama receberDirecao().
// O parametro "valor" vem como texto e precisa ser validado.

// Valida um parametro inteiro recebido na query string (?valor=...).
// O argumento "resultado" e passado por referencia e recebe o numero.
// Retorna false se nao existir, contiver caracteres invalidos ou sair
// dos limites permitidos para o comando.
bool receberInteiro(int minimo, int maximo,
                    int &resultado) {

  if (!server.hasArg("valor")) {
    return false;
  }

  String valor = server.arg("valor");

  if (valor.length() == 0) {
    return false;
  }

  // Permite sinal negativo apenas no início
  int inicio = valor[0] == '-' ? 1 : 0;

  if (inicio == (int)valor.length()) {
    return false;
  }

  for (int i = inicio; i < (int)valor.length(); i++) {

    if (valor[i] < '0' || valor[i] > '9') {
      return false;
    }
  }

  long numero = valor.toInt();

  if (numero < minimo || numero > maximo) {
    return false;
  }

  resultado = (int)numero;
  return true;
}

// POST /direcao?valor=-1|0|1: nao altera diretamente GPIOs.
// Apenas atualiza o destino; atualizarMotores() faz a transicao segura.
void receberDirecao() {

  int valor;

  if (!receberInteiro(-1, 1, valor)) {
    server.send(400, "text/plain", "Direcao invalida");
    return;
  }

  direcaoSolicitada = valor;
  ultimoContato = millis();

  Serial.print("[HTTP] Direcao: ");
  Serial.println(valor);

  server.send(200, "text/plain", "OK");
}

// POST /velocidade?valor=0..100: registra o alvo da rampa.
// A alteracao fisica de PWM ocorre depois, dentro de atualizarMotores().
void receberVelocidade() {

  int valor;

  if (!receberInteiro(0, 100, valor)) {
    server.send(400, "text/plain", "Velocidade invalida");
    return;
  }

  velocidadeDesejada = valor;
  ultimoContato = millis();

  server.send(200, "text/plain", "OK");
}

// POST /luz?tipo=farol|interior&valor=0|1: muda o estado solicitado.
// atualizarLuzes() realiza o fade, sem mudar o brilho bruscamente.
void receberLuz() {

  if (!server.hasArg("tipo")) {
    server.send(400, "text/plain", "Tipo ausente");
    return;
  }

  int valor;

  if (!receberInteiro(0, 1, valor)) {
    server.send(400, "text/plain", "Valor invalido");
    return;
  }

  String tipo = server.arg("tipo");

  if (tipo == "farol") {

    farolLigado = (valor == 1);

  } else if (tipo == "interior") {

    interiorLigado = (valor == 1);

  } else {
    server.send(400, "text/plain", "Luz invalida");
    return;
  }

  server.send(200, "text/plain", "OK");
}

// GET /estado: devolve um JSON com as variaveis atuais do sistema.
// A pagina consulta periodicamente essa rota e atualiza os indicadores.
// Essa consulta tambem renova o temporizador de comunicacao.
void mostrarEstado() {

  ultimoContato = millis();

  String json = "{";

  json += "\"direcaoAtual\":";
  json += String(direcaoAtual);

  json += ",\"direcaoSolicitada\":";
  json += String(direcaoSolicitada);

  json += ",\"velocidadeAtual\":";
  json += String(velocidadeAtual);

  json += ",\"velocidadeDesejada\":";
  json += String(velocidadeDesejada);

  json += ",\"pwmEfetivo\":";
  json += String(pwmEfetivo);

  json += ",\"farol\":";
  json += (farolLigado ? "true" : "false");

  json += ",\"interior\":";
  json += (interiorLigado ? "true" : "false");

  json += "}";

  server.send(200, "application/json", json);
}

// ==========================================
// INICIALIZAÇÃO
// ==========================================

// Executada uma vez no boot/reset: prepara ponte H, PWM, Wi-Fi e rotas.
void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println("=== MYTRAIN v0.8 ===");

  // Primeiro mantém o driver desabilitado
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);

  // Entradas de direção
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  // GPIO 8 pertence a luz lateral direita: nao configurar LED traseiro.

  // Associa cada GPIO de iluminacao/motor a um dos 6 canais LEDC.
  // ledcAttachChannel(pino, frequencia, resolucao, canal).
  // O canal 0 e ligado fisicamente tanto a PWMA quanto a PWMB.
  // Configura os canais LEDC utilizados
  bool ok = true;

  ok &= ledcAttachChannel(
    PWMA, PWM_FREQ_MOTOR, PWM_RES, CH_MOTOR
  );

  ok &= ledcAttachChannel(
    FAROL_SUPERIOR, PWM_FREQ_LED,
    PWM_RES, CH_FAROL_SUP
  );

  // ok &= ledcAttachChannel(
  //   FAROL_INFERIOR, PWM_FREQ_LED,
  //   PWM_RES, CH_FAROL_INF
  // );

  ok &= ledcAttachChannel(
    LUZ_INTERNA, PWM_FREQ_LED,
    PWM_RES, CH_INTERIOR
  );

  ok &= ledcAttachChannel(
    LUZ_ESQUERDA, PWM_FREQ_LED,
    PWM_RES, CH_ESQUERDA
  );

  ok &= ledcAttachChannel(
    LUZ_DIREITA, PWM_FREQ_LED,
    PWM_RES, CH_DIREITA
  );

  // Apenas canais associados a GPIOs: canal 2 esta livre.
  for (int canal : {CH_MOTOR, CH_FAROL_SUP, CH_INTERIOR,
                    CH_ESQUERDA, CH_DIREITA}) {
    ledcWriteChannel(canal, 0);
  }

  if (ok) {

    hardwarePronto = true;

    // Habilita a ponte H com PWM zero
    digitalWrite(STBY, HIGH);

    Serial.println("[MOTOR] Dois canais prontos");

  } else {

    Serial.println("[ERRO] Falha ao configurar PWM");
    digitalWrite(STBY, LOW);
  }

  // WIFI_STA conecta a uma rede ja existente, sem criar ponto de acesso.
  // setSleep(false) desativa economia de energia do radio;
  // 8,5 dBm e a potencia de transmissao validada nos testes da placa.
  // Configuração Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setAutoReconnect(true);

  WiFi.begin(ssid, password);

  // Define quais funcoes o servidor chama para cada rota/metodo.
  // Rotas HTTP
  server.on("/", HTTP_GET, mostrarPagina);
  server.on("/direcao", HTTP_POST, receberDirecao);
  server.on("/velocidade", HTTP_POST, receberVelocidade);
  server.on("/luz", HTTP_POST, receberLuz);
  server.on("/estado", HTTP_GET, mostrarEstado);

  server.begin();

  Serial.println("[WEB] Servidor iniciado");

  ultimoContato = millis();
}

// ==========================================
// LOOP PRINCIPAL
// ==========================================

// Executada continuamente: atende HTTP, monitora comunicacao, atualiza
// as rampas dos dois motores e os efeitos PWM das luzes.
void loop() {

  server.handleClient();

  // Se perder o Wi-Fi, solicita parada.
  // Se a página parar de consultar /estado,
  // também solicita parada.
  if (
    WiFi.status() != WL_CONNECTED ||
    millis() - ultimoContato > TIMEOUT_CONTROLE
  ) {

    direcaoSolicitada = 0;
  }

  // Atualiza motores e luzes
  atualizarMotores();
  atualizarLuzes();
  // Luz traseira desconectada: manter desativada para evitar cintilacao.
  // atualizarPWMTraseiro();

  // Monitor serial a cada 5 segundos
  static unsigned long ultimoLog = 0;

  if (millis() - ultimoLog >= 5000) {

    ultimoLog = millis();

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
}
