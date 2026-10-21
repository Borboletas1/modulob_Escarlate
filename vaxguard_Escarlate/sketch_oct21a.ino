#include <WiFiS3.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Arduino_LED_Matrix.h>
#include <Servo.h>
#include "secrets.h"
#include "pagina.h"

// ================== Pinos ==================
#define PIN_DHT      3
#define PIN_PORTA    2
#define PIN_BUZZER   6
#define PIN_BOTAO    7
#define PIN_SERVO    8
#define PIN_LED_R    9
#define PIN_LED_G    10
#define PIN_LED_Y    11
#define PIN_LDR      A0

// ================== DHT11 ==================
#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

// ================== Tilt switch ==================
#define PORTA_ABERTA_NIVEL HIGH

// ================== Servo ==================
#define SERVO_FECHADO_DEG   0
#define SERVO_ABERTO_DEG    90

// ================== LDR ==================
#define MARGEM_LUZ           40
#define HISTERESE_LUZ        40
#define AMOSTRAS_MEDIA         8
#define AMOSTRAS_CALIBRACAO   20
#define LDR_INVERTIDO      false

// ================== Constantes ==================
#define INTERVALO_LEITURA_PADRAO 5000UL
#define LIMITE_TEMP_PADRAO       8.0f
#define HISTERESE_TEMP           1.0f
#define PORTA_ALERTA_MS          20000UL
#define PORTA_CRITICO_MS         60000UL
#define FORA_FAIXA_CRITICO_MS    30000UL
#define BIPE_INTERVALO_MS        10000UL
#define BUFFER_TAMANHO           30
#define EEPROM_ADDR_MAGIC        0
#define EEPROM_ADDR_LIMITE       4
#define EEPROM_ADDR_INTERVALO    8
#define EEPROM_MAGIC             0xA5

// ================== Estados ==================
enum Estado { NORMAL, ATENCAO, CRITICO, RECONHECIDO, FALHA_SENSOR };

// ================== Estruturas ==================
struct Leitura {
  uint32_t seq;
  unsigned long ts;
  float temp;
  float umid;
  bool porta;
  uint8_t estado;
  uint8_t estadoAnterior;
  uint16_t luz;
  bool extras[2];
};

Leitura bufferOffline[BUFFER_TAMANHO];
int bufHead = 0, bufCount = 0;
uint32_t seqGlobal = 0;

// ================== Globais ==================
ArduinoLEDMatrix matrix;
WiFiClient wifiClient;
Servo servoVentilacao;
WiFiServer servidorWeb(80);

Estado estadoAtual = NORMAL;
Estado estadoAnterior = NORMAL;
unsigned long ultimaLeituraMs = 0;
unsigned long portaAbertaDesdeMs = 0;
unsigned long foraFaixaDesdeMs = 0;
unsigned long tempoReconexaoMs = 0;
unsigned long backoffAtual = 1000;
bool wifiConectado = false;
bool portaEstavaAberta = false;
int posicaoServoAtual = SERVO_FECHADO_DEG;

float limiteTemp = LIMITE_TEMP_PADRAO;
unsigned long intervaloLeitura = INTERVALO_LEITURA_PADRAO;
int falhasDHT = 0;

// Últimos valores lidos (usados pelo servidor web)
float ultimaTemp = 0;
float ultimaUmid = 0;
bool ultimaPorta = false;
uint16_t ultimaLuz = 0;

// LDR
uint16_t luzFechada = 0;
uint16_t limiarLuz = 0;
bool estadoLuzAberta = false;
uint16_t amostrasLuz[AMOSTRAS_MEDIA];
int idxAmostraLuz = 0;
bool amostrasLuzCheias = false;

// Buzzer
bool buzzerAtivo = false;
unsigned long ultimoToggleBuzzer = 0;
bool nivelBuzzer = false;
unsigned long ultimoBipeCurtoMs = 0;
bool bipeCurtoAtivo = false;
unsigned long inicioBipeCurtoMs = 0;

// Botão
bool botaoEstavaPressionado = false;
unsigned long ultimoBotaoMs = 0;

// ================== Matrizes de LED ==================
byte frameN[8][12] = {
  {1,1,0,0,0,0,0,0,0,0,1,1},{1,1,1,0,0,0,0,0,0,1,1,1},
  {1,1,1,1,0,0,0,0,1,1,1,1},{1,1,1,1,1,0,0,1,1,1,1,1},
  {1,1,0,1,1,1,1,1,1,0,1,1},{1,1,0,0,1,1,1,1,0,0,1,1},
  {1,1,0,0,0,1,1,0,0,0,1,1},{1,1,0,0,0,0,0,0,0,0,1,1}
};
byte frameA[8][12] = {
  {0,0,0,0,1,1,1,1,0,0,0,0},{0,0,0,1,1,1,1,1,1,0,0,0},
  {0,0,1,1,0,0,0,0,1,1,0,0},{0,1,1,0,0,0,0,0,0,1,1,0},
  {0,1,1,1,1,1,1,1,1,1,1,0},{0,1,1,0,0,0,0,0,0,1,1,0},
  {0,1,1,0,0,0,0,0,0,1,1,0},{0,1,1,0,0,0,0,0,0,1,1,0}
};
byte frameC[8][12] = {
  {0,0,1,1,1,1,1,1,0,0,0,0},{0,1,1,1,1,1,1,1,1,0,0,0},
  {1,1,1,0,0,0,0,0,1,1,0,0},{1,1,0,0,0,0,0,0,0,0,0,0},
  {1,1,0,0,0,0,0,0,0,0,0,0},{1,1,1,0,0,0,0,0,1,1,0,0},
  {0,1,1,1,1,1,1,1,1,0,0,0},{0,0,1,1,1,1,1,1,0,0,0,0}
};
byte frameR[8][12] = {
  {1,1,1,1,1,1,1,0,0,0,0,0},{1,1,0,0,0,1,1,1,0,0,0,0},
  {1,1,0,0,0,0,1,1,0,0,0,0},{1,1,0,0,0,1,1,1,0,0,0,0},
  {1,1,1,1,1,1,1,0,0,0,0,0},{1,1,0,0,1,1,0,0,0,0,0,0},
  {1,1,0,0,0,1,1,0,0,0,0,0},{1,1,0,0,0,0,1,1,0,0,0,0}
};
byte frameF[8][12] = {
  {1,1,1,1,1,1,1,1,1,0,0,0},{1,1,0,0,0,0,0,0,0,0,0,0},
  {1,1,0,0,0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,0,0,0,0,0},
  {1,1,0,0,0,0,0,0,0,0,0,0},{1,1,0,0,0,0,0,0,0,0,0,0},
  {1,1,0,0,0,0,0,0,0,0,0,0},{1,1,0,0,0,0,0,0,0,0,0,0}
};

// ================== Protótipos ==================
void carregarConfig();
void conectarWiFi(unsigned long agora);
void lerSensores(unsigned long agora);
void avaliarEstado(unsigned long agora);
void executarAtuacao(unsigned long agora);
void atualizarDisplay();
void adicionarEventoBuffer(Estado ant, Estado novo, const char* motivo);
void tentarEnviarBuffer();
bool enviarJSON(const String &json);
void resetarWatchdog();
bool tiltIndicaPortaAberta();
uint16_t lerLdrMedia();
void calibrarLdr();
bool luzIndicaPortaAberta(uint16_t luzMedia);
bool portaEstaAberta(uint16_t luzMedia);
void moverServo(int angulo);
void atualizarBuzzer(unsigned long agora);
void ligarBuzzerContinuo();
void desligarBuzzer();
void dispararBipeCurto();
void atenderNavegador();

// ================== Setup ==================
void setup() {
  Serial.begin(9600);
  delay(2000);

  pinMode(PIN_PORTA, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BOTAO, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_Y, OUTPUT);
  pinMode(PIN_LDR, INPUT);
  digitalWrite(PIN_BUZZER, LOW);

  servoVentilacao.attach(PIN_SERVO);
  servoVentilacao.write(SERVO_FECHADO_DEG);
  posicaoServoAtual = SERVO_FECHADO_DEG;

  dht.begin();
  delay(2000);
  matrix.begin();

  carregarConfig();
  calibrarLdr();
  
  // ===== CONEXÃO WI-FI (revisada) =====
  Serial.println("Limpando estado anterior do Wi-Fi...");
  WiFi.disconnect();
  WiFi.end();
  delay(1000);

  conectarWiFi(millis());

  Serial.print("Aguardando conexao Wi-Fi");
  unsigned long inicioWiFi = millis();
  
  // Aguarda até 20s por um IP VÁLIDO (não só WL_CONNECTED)
  while (millis() - inicioWiFi < 20000) {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0)) {
      break;
    }
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  // Dá mais 1.5s para o DHCP estabilizar (problema conhecido do R4 WiFi)
  delay(1500);

  servidorWeb.begin();

  Serial.println("=================================");
  Serial.println("VaxGuard Connect iniciado");
  Serial.print("Limite temp: "); Serial.println(limiteTemp);
  Serial.print("Intervalo leitura: "); Serial.println(intervaloLeitura);
  Serial.print("Luz fechada (calibrado): "); Serial.println(luzFechada);
  Serial.print("Limiar de luz: "); Serial.println(limiarLuz);
  Serial.print("Servidor Python: http://");
  Serial.print(SERVER_IP); Serial.print(":"); Serial.print(SERVER_PORT);
  Serial.println(SERVER_PATH);
  Serial.println("=================================");

  // Diagnóstico de Wi-Fi
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0)) {
    Serial.print("Wi-Fi OK. Acesse no navegador: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("!! FALHA Wi-Fi: sem IP valido.");
    Serial.print("   Status: "); Serial.println(WiFi.status());
    Serial.println("   Verifique SSID/senha no secrets.h ou use hotspot.");
  }
  Serial.println("=================================");
}

// ================== Loop principal ==================
void loop() {
  unsigned long agora = millis();

  atenderNavegador();

  if (WiFi.status() != WL_CONNECTED) {
    wifiConectado = false;
    if (agora - tempoReconexaoMs >= backoffAtual) {
      conectarWiFi(agora);
    }
  } else {
    wifiConectado = true;
    backoffAtual = 1000;
  }

  if (agora - ultimaLeituraMs >= intervaloLeitura) {
    ultimaLeituraMs = agora;
    lerSensores(agora);
    avaliarEstado(agora);
    executarAtuacao(agora);
    atualizarDisplay();
  }

  bool botaoAgora = (digitalRead(PIN_BOTAO) == LOW);
  if (botaoAgora && !botaoEstavaPressionado && (agora - ultimoBotaoMs > 200)) {
    ultimoBotaoMs = agora;
    Serial.println(">> BOTAO PRESSIONADO");
    if (estadoAtual == CRITICO) {
      Estado ant = estadoAtual;
      estadoAtual = RECONHECIDO;
      desligarBuzzer();
      adicionarEventoBuffer(ant, RECONHECIDO, "Botao pressionado");
    }
  }
  botaoEstavaPressionado = botaoAgora;

  if (wifiConectado) {
    tentarEnviarBuffer();
  }

  atualizarBuzzer(agora);

  if (estadoAtual == ATENCAO && !bipeCurtoAtivo &&
      agora - ultimoBipeCurtoMs >= BIPE_INTERVALO_MS) {
    ultimoBipeCurtoMs = agora;
    dispararBipeCurto();
  }
  if (bipeCurtoAtivo && agora - inicioBipeCurtoMs >= 100) {
    bipeCurtoAtivo = false;
    buzzerAtivo = false;
    digitalWrite(PIN_BUZZER, LOW);
  }

  static unsigned long ultimoRefreshWDT = 0;
  if (agora - ultimoRefreshWDT > 300000UL) {
    NVIC_SystemReset();
  }
  resetarWatchdog();
}

// ================== Servidor web ==================
void atenderNavegador() {
  WiFiClient cliente = servidorWeb.available();
  if (!cliente) return;

  String requisicao = "";
  unsigned long inicio = millis();
  while (cliente.connected() && (millis() - inicio < 500)) {
    if (cliente.available()) {
      char c = cliente.read();
      requisicao += c;
      if (requisicao.endsWith("\r\n\r\n")) break;
    }
  }

  if (requisicao.indexOf("GET /dados") >= 0) {
    String json = "{";
    json += "\"temp\":" + String(ultimaTemp, 1) + ",";
    json += "\"umid\":" + String(ultimaUmid, 0) + ",";
    json += "\"porta\":" + String(ultimaPorta ? "true" : "false") + ",";
    json += "\"luz\":" + String(ultimaLuz) + ",";
    json += "\"estado\":" + String((int)estadoAtual);
    json += "}";

    cliente.println("HTTP/1.1 200 OK");
    cliente.println("Content-Type: application/json");
    cliente.println("Access-Control-Allow-Origin: *");
    cliente.println("Connection: close");
    cliente.println();
    cliente.println(json);
  } else {
    cliente.println("HTTP/1.1 200 OK");
    cliente.println("Content-Type: text/html; charset=UTF-8");
    cliente.println("Connection: close");
    cliente.println();
    cliente.println(PAGINA_HTML);
  }
  delay(1);
  cliente.stop();
}

// ================== Buzzer ==================
void ligarBuzzerContinuo() { buzzerAtivo = true; }
void desligarBuzzer() {
  buzzerAtivo = false;
  bipeCurtoAtivo = false;
  nivelBuzzer = false;
  digitalWrite(PIN_BUZZER, LOW);
}
void dispararBipeCurto() {
  bipeCurtoAtivo = true;
  buzzerAtivo = true;
  inicioBipeCurtoMs = millis();
}
void atualizarBuzzer(unsigned long agora) {
  if (buzzerAtivo) {
    if (agora - ultimoToggleBuzzer >= 1) {
      nivelBuzzer = !nivelBuzzer;
      digitalWrite(PIN_BUZZER, nivelBuzzer);
      ultimoToggleBuzzer = agora;
    }
  } else {
    digitalWrite(PIN_BUZZER, LOW);
  }
}

// ================== LDR ==================
uint16_t lerLdrMedia() {
  uint32_t soma = 0;
  for (int i = 0; i < AMOSTRAS_MEDIA; i++) {
    soma += analogRead(PIN_LDR);
    delayMicroseconds(200);
  }
  return (uint16_t)(soma / AMOSTRAS_MEDIA);
}

void calibrarLdr() {
  uint32_t soma = 0;
  for (int i = 0; i < AMOSTRAS_CALIBRACAO; i++) {
    soma += analogRead(PIN_LDR);
    delay(20);
  }
  luzFechada = (uint16_t)(soma / AMOSTRAS_CALIBRACAO);
  limiarLuz = luzFechada + MARGEM_LUZ;
  if (limiarLuz > 1023) limiarLuz = 1023;
  estadoLuzAberta = false;
  for (int i = 0; i < AMOSTRAS_MEDIA; i++) amostrasLuz[i] = luzFechada;
  idxAmostraLuz = 0;
  amostrasLuzCheias = false;
}

bool luzIndicaPortaAberta(uint16_t luzMedia) {
  bool acima, abaixo;
  if (LDR_INVERTIDO) {
    acima  = (luzMedia < (limiarLuz - HISTERESE_LUZ));
    abaixo = (luzMedia > (limiarLuz + HISTERESE_LUZ));
  } else {
    acima  = (luzMedia > (limiarLuz + HISTERESE_LUZ));
    abaixo = (luzMedia < (limiarLuz - HISTERESE_LUZ));
  }
  if (estadoLuzAberta) {
    if (abaixo) estadoLuzAberta = false;
  } else {
    if (acima) estadoLuzAberta = true;
  }
  return estadoLuzAberta;
}

// ================== Auxiliares ==================
bool tiltIndicaPortaAberta() {
  return digitalRead(PIN_PORTA) == PORTA_ABERTA_NIVEL;
}

bool portaEstaAberta(uint16_t luzMedia) {
  return tiltIndicaPortaAberta() || luzIndicaPortaAberta(luzMedia);
}

void moverServo(int angulo) {
  if (angulo != posicaoServoAtual) {
    servoVentilacao.write(angulo);
    posicaoServoAtual = angulo;
    Serial.print(">> Servo: "); Serial.print(angulo); Serial.println(" graus");
  }
}

void resetarWatchdog() {}

// ================== Leitura e estados ==================
void lerSensores(unsigned long agora) {
  bool wifiEstavaLigado = (WiFi.status() == WL_CONNECTED);
  if (wifiEstavaLigado) { WiFi.disconnect(); delay(10); }

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (wifiEstavaLigado) { WiFi.begin(WIFI_SSID, WIFI_PASS); }

  if (isnan(t) || isnan(h)) {
    falhasDHT++;
    Serial.print("DHT falhou ("); Serial.print(falhasDHT); Serial.println("/3)");
    if (falhasDHT >= 3 && estadoAtual != FALHA_SENSOR) {
      Estado ant = estadoAtual;
      estadoAtual = FALHA_SENSOR;
      adicionarEventoBuffer(ant, FALHA_SENSOR, "3 leituras DHT invalidas");
    }
    return;
  }
  falhasDHT = 0;

  uint16_t luz = lerLdrMedia();
  bool tiltAberta = tiltIndicaPortaAberta();
  bool luzAberta = luzIndicaPortaAberta(luz);
  bool porta = tiltAberta || luzAberta;

  ultimaTemp = t;
  ultimaUmid = h;
  ultimaPorta = porta;
  ultimaLuz = luz;

  Leitura L;
  L.seq = ++seqGlobal;
  L.ts = agora / 1000;
  L.temp = t;
  L.umid = h;
  L.porta = porta;
  L.estado = (uint8_t)estadoAtual;
  L.estadoAnterior = (uint8_t)estadoAnterior;
  L.luz = luz;
  L.extras[0] = false;
  L.extras[1] = false;

  bufferOffline[bufHead] = L;
  bufHead = (bufHead + 1) % BUFFER_TAMANHO;
  if (bufCount < BUFFER_TAMANHO) bufCount++;

  Serial.print("Temp: "); Serial.print(t);
  Serial.print(" Umid: "); Serial.print(h);
  Serial.print(" Luz: "); Serial.print(luz);
  Serial.print(" Tilt: "); Serial.print(tiltAberta ? "AB" : "FE");
  Serial.print(" LDR: "); Serial.print(luzAberta ? "AB" : "FE");
  Serial.print(" => "); Serial.println(porta ? "ABERTA" : "FECHADA");
}

void avaliarEstado(unsigned long agora) {
  if (estadoAtual == FALHA_SENSOR) {
    if (falhasDHT == 0) {
      Estado ant = estadoAtual;
      estadoAtual = NORMAL;
      adicionarEventoBuffer(ant, NORMAL, "DHT recuperado");
    }
    return;
  }

  float t = dht.readTemperature();
  if (isnan(t)) return;

  uint16_t luz = lerLdrMedia();
  bool porta = portaEstaAberta(luz);
  bool foraFaixa = (t < limiteTemp - HISTERESE_TEMP) || (t > limiteTemp + HISTERESE_TEMP);
  bool proximoLimite = (abs(t - limiteTemp) < HISTERESE_TEMP);

  if (porta && !portaEstavaAberta) {
    portaAbertaDesdeMs = agora;
    Serial.println(">> Porta ABRIU");
  } else if (!porta && portaEstavaAberta) {
    portaAbertaDesdeMs = 0;
    Serial.println(">> Porta FECHOU");
  }
  portaEstavaAberta = porta;

  if (foraFaixa && foraFaixaDesdeMs == 0) {
    foraFaixaDesdeMs = agora;
  } else if (!foraFaixa) {
    foraFaixaDesdeMs = 0;
  }

  if (estadoAtual == RECONHECIDO) {
    if (!foraFaixa && !porta) {
      Estado ant = estadoAtual;
      estadoAtual = NORMAL;
      moverServo(SERVO_FECHADO_DEG);
      portaAbertaDesdeMs = 0;
      foraFaixaDesdeMs = 0;
      adicionarEventoBuffer(ant, NORMAL, "Normalizado apos reconhecimento");
    }
    return;
  }

  bool criticoTemp = foraFaixa && (agora - foraFaixaDesdeMs >= FORA_FAIXA_CRITICO_MS);
  bool criticoPorta = porta && (agora - portaAbertaDesdeMs >= PORTA_CRITICO_MS);

  if (criticoTemp || criticoPorta) {
    if (estadoAtual != CRITICO) {
      Estado ant = estadoAtual;
      estadoAtual = CRITICO;
      const char* motivo = criticoTemp ? "Temp fora da faixa >30s" : "Porta aberta >60s";
      adicionarEventoBuffer(ant, CRITICO, motivo);
    }
    return;
  }

  bool atencaoPorta = porta && (agora - portaAbertaDesdeMs >= PORTA_ALERTA_MS);
  if (proximoLimite || atencaoPorta) {
    if (estadoAtual != ATENCAO) {
      Estado ant = estadoAtual;
      estadoAtual = ATENCAO;
      const char* motivo = proximoLimite ? "Temp proximo ao limite" : "Porta aberta >20s";
      adicionarEventoBuffer(ant, ATENCAO, motivo);
    }
    return;
  }

  if (estadoAtual != NORMAL) {
    Estado ant = estadoAtual;
    estadoAtual = NORMAL;
    adicionarEventoBuffer(ant, NORMAL, "Normalizado");
  }
}

void executarAtuacao(unsigned long agora) {
  switch (estadoAtual) {
    case NORMAL:
      digitalWrite(PIN_LED_R, LOW); digitalWrite(PIN_LED_G, HIGH); digitalWrite(PIN_LED_Y, LOW);
      desligarBuzzer(); moverServo(SERVO_FECHADO_DEG); break;
    case ATENCAO:
      digitalWrite(PIN_LED_R, LOW); digitalWrite(PIN_LED_G, LOW); digitalWrite(PIN_LED_Y, HIGH);
      moverServo(SERVO_FECHADO_DEG); break;
    case CRITICO:
      digitalWrite(PIN_LED_R, HIGH); digitalWrite(PIN_LED_G, LOW); digitalWrite(PIN_LED_Y, LOW);
      ligarBuzzerContinuo(); moverServo(SERVO_ABERTO_DEG); break;
    case RECONHECIDO:
      digitalWrite(PIN_LED_R, (agora / 500) % 2); digitalWrite(PIN_LED_G, LOW); digitalWrite(PIN_LED_Y, LOW);
      desligarBuzzer(); moverServo(SERVO_ABERTO_DEG); break;
    case FALHA_SENSOR:
      digitalWrite(PIN_LED_R, (agora / 500) % 2); digitalWrite(PIN_LED_G, LOW);
      digitalWrite(PIN_LED_Y, ((agora / 500) + 1) % 2);
      desligarBuzzer(); moverServo(SERVO_FECHADO_DEG); break;
  }
}

void atualizarDisplay() {
  switch (estadoAtual) {
    case NORMAL:        matrix.renderBitmap(frameN, 8, 12); break;
    case ATENCAO:       matrix.renderBitmap(frameA, 8, 12); break;
    case CRITICO:       matrix.renderBitmap(frameC, 8, 12); break;
    case RECONHECIDO:   matrix.renderBitmap(frameR, 8, 12); break;
    case FALHA_SENSOR:  matrix.renderBitmap(frameF, 8, 12); break;
  }
}

void adicionarEventoBuffer(Estado ant, Estado novo, const char* motivo) {
  Serial.print(">> EVENTO: "); Serial.print((int)ant); Serial.print(" -> ");
  Serial.print((int)novo); Serial.print(" ("); Serial.print(motivo); Serial.println(")");

  uint16_t luz = lerLdrMedia();
  Leitura ev;
  ev.seq = ++seqGlobal;
  ev.ts = millis() / 1000;
  ev.temp = ultimaTemp;
  ev.umid = ultimaUmid;
  ev.porta = portaEstaAberta(luz);
  ev.estado = (uint8_t)novo;
  ev.estadoAnterior = (uint8_t)ant;
  ev.luz = luz;
  ev.extras[0] = false;
  ev.extras[1] = false;

  bufferOffline[bufHead] = ev;
  bufHead = (bufHead + 1) % BUFFER_TAMANHO;
  if (bufCount < BUFFER_TAMANHO) bufCount++;
}

// ================== Wi-Fi ==================
void conectarWiFi(unsigned long agora) {
  Serial.print("WiFi begin (tentativa). Backoff: "); Serial.println(backoffAtual);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  tempoReconexaoMs = agora;
  backoffAtual = min(backoffAtual * 2, 30000UL);
}

void tentarEnviarBuffer() {
  if (bufCount == 0) return;
  int idx = (bufHead - bufCount + BUFFER_TAMANHO) % BUFFER_TAMANHO;
  Leitura &L = bufferOffline[idx];

  StaticJsonDocument<512> doc;
  doc["device_id"] = DEVICE_ID;
  doc["placa"] = "UNO_R4_WIFI";
  doc["seq"] = L.seq;
  doc["ts"] = L.ts;
  doc["temp"] = L.temp;
  doc["umid"] = L.umid;
  doc["porta"] = L.porta;
  doc["estado"] = L.estado;
  doc["estado_anterior"] = L.estadoAnterior;
  doc["luz"] = L.luz;

  String json;
  serializeJson(doc, json);

  if (enviarJSON(json)) {
    bufCount--;
    Serial.print(">> Enviado ao Python (seq="); Serial.print(L.seq); Serial.println(")");
  }
}

bool enviarJSON(const String &json) {
  if (!wifiClient.connect(SERVER_IP, SERVER_PORT)) {
    return false;
  }

  wifiClient.print("POST ");
  wifiClient.print(SERVER_PATH);
  wifiClient.println(" HTTP/1.1");

  wifiClient.print("Host: ");
  wifiClient.print(SERVER_IP);
  wifiClient.print(":");
  wifiClient.println(SERVER_PORT);

  wifiClient.println("Content-Type: application/json");

  wifiClient.print("Content-Length: ");
  wifiClient.println(json.length());

  wifiClient.println("Connection: close");
  wifiClient.println();
  wifiClient.println(json);

  delay(50);
  wifiClient.stop();
  return true;
}

// ================== EEPROM ==================
void carregarConfig() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC) {
    EEPROM.get(EEPROM_ADDR_LIMITE, limiteTemp);
    EEPROM.get(EEPROM_ADDR_INTERVALO, intervaloLeitura);
    if (intervaloLeitura < 1000) intervaloLeitura = INTERVALO_LEITURA_PADRAO;
    if (limiteTemp < -40 || limiteTemp > 80) limiteTemp = LIMITE_TEMP_PADRAO;
  } else {
    limiteTemp = LIMITE_TEMP_PADRAO;
    intervaloLeitura = INTERVALO_LEITURA_PADRAO;
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.put(EEPROM_ADDR_LIMITE, limiteTemp);
    EEPROM.put(EEPROM_ADDR_INTERVALO, intervaloLeitura);
  }
}