#include <Arduino.h>
#include <SoftwareSerial.h>
#include <math.h>
#include <EEPROM.h>

#define BLYNK_TOKEN "3_ACPOgQsyElbRQYejOCk7gC0sQSqoOS"
const char* ssid = "ProjetoIoT"; 
const char* password = "12345678";

SoftwareSerial espSerial(2, 3);
const int pinoSensor = A0;

// --- EEPROM / Offline Buffer ---
const int EEPROM_SIZE = 1024; // 1KB (Arduino UNO typical)
const int HEADER_ADDR = 0; // 2 bytes head, 2 bytes count
const int HEADER_COUNT_ADDR = 2;
const int DATA_START = 4;
const int RECORD_SIZE = 8; // float (4) + uint32_t timestamp (4)
const int MAX_RECORDS = (EEPROM_SIZE - DATA_START) / RECORD_SIZE;

uint16_t eep_head = 0;   // next write position (0..MAX_RECORDS-1)
uint16_t eep_count = 0;  // how many valid records currently stored

unsigned long lastEepromSaveMillis = 0;
const unsigned long ONLINE_SEND_INTERVAL = 5UL * 1000UL; // 5 seconds
const unsigned long OFFLINE_SAVE_INTERVAL = 30UL * 60UL * 1000UL; // 30 minutes

// Backend Vercel/FastAPI config
const char* VERCEL_HOST = "seu-projeto.vercel.app"; // <--- ajusta após deploy
const char* VERCEL_PATH = "/update";

// Forward declarations
void eepromInit();
void eepromSaveHeader();
void salvarNaEEPROM(float valor, uint32_t timestamp);
void forEachEepromRecord(void (*cb)(float, uint32_t));
void clearEEPROMBuffer();
bool enviarParaVercel(float temp, uint32_t timestamp, const char* origem);
void sincronizarDadosEEPROM();


// --- A NOSSA ARMA SECRETA ---
// Esta função envia o comando e LÊ a frase exata que o chip responder
String enviaComando(String comando, const int timeout) {
  String resposta = "";
  espSerial.print(comando + "\r\n");
  long int tempoLimit = millis() + timeout;
  
  while (tempoLimit > millis()) {
    while (espSerial.available()) {
      char c = espSerial.read();
      resposta += c; // Monta o texto letra por letra
    }
  }
  return resposta;
}

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  delay(2000); 

  Serial.println(F("\n--- INICIANDO SISTEMA COM AUDITORIA ---"));
  
  enviaComando("AT+RST", 3000); 
  enviaComando("AT+CWQAP", 1000); 
  enviaComando("AT+CWMODE=1", 1000); 
}

void loop() {
  // 1. CHECAGEM DE REDE
  String respIP = enviaComando("AT+CIFSR", 2000);

  static bool wasConnected = false;
  bool isConnected = !(respIP.indexOf("0.0.0.0") != -1 || respIP.indexOf("ERROR") != -1);

  if (!isConnected) {
    Serial.println(F("-> Desconectado! A tentar ligar ao Hotspot..."));

    String comandoConexao = "AT+CWJAP=\"";
    comandoConexao += ssid;
    comandoConexao += "\",\"";
    comandoConexao += password;
    comandoConexao += "\"";

    // Tenta conectar e dá 10 segundos para a negociação ocorrer
    enviaComando(comandoConexao, 10000);

    // Salva na EEPROM a cada 30 minutos quando offline
    if (millis() - lastEepromSaveMillis > OFFLINE_SAVE_INTERVAL) {
      int valorADC = analogRead(pinoSensor);
      float rNTC = 10000.0 * ((1023.0 / (float)valorADC) - 1.0);
      float temp = 1.0 / ((1.0 / 298.15) + (1.0 / 3950.0) * log(rNTC / 10000.0)) - 273.15;
      uint32_t ts = (uint32_t)(millis() / 1000UL);
      salvarNaEEPROM(temp, ts);
      lastEepromSaveMillis = millis();
    }

    wasConnected = false;
    return;
  }

  // Se acabou de reconectar, sincroniza dados offline
  if (!wasConnected && isConnected) {
    Serial.println(F("-> Conexão restabelecida. Iniciando sincronização EEPROM..."));
    sincronizarDadosEEPROM();
  }
  wasConnected = true;

  // 2. Lê sensor e envia em tempo real
  int valorADC = analogRead(pinoSensor);
  float rNTC = 10000.0 * ((1023.0 / (float)valorADC) - 1.0);
  float temp = 1.0 / ((1.0 / 298.15) + (1.0 / 3950.0) * log(rNTC / 10000.0)) - 273.15;

  Serial.print(F("Wi-Fi OK | Temp: "));
  Serial.print(temp);
  Serial.print(F(" C | A ligar TCP... "));

  // 3. Envia para Blynk
  String respTCP = enviaComando("AT+CIPSTART=\"TCP\",\"ny3.blynk.cloud\",80", 4000);

  if (respTCP.indexOf("OK") != -1 || respTCP.indexOf("Linked") != -1 || respTCP.indexOf("CONNECT") != -1) {
    String url = "GET /external/api/update?token=";
    url += BLYNK_TOKEN;
    url += "&v0=";
    url += String(temp, 1);
    url += " HTTP/1.1\r\nHost: ny3.blynk.cloud\r\nConnection: close\r\n\r\n";

    String comandoCipsend = "AT+CIPSEND=";
    comandoCipsend += url.length();
    enviaComando(comandoCipsend, 1000);
    enviaComando(url, 2000);
    Serial.println(F("Sucesso Blynk!"));

    // Tenta enviar para PocketBase; se falhar, grava para retry
    uint32_t ts = (uint32_t)(millis() / 1000UL);
    bool okVercel = enviarParaVercel(temp, ts, "realtime");
    if (!okVercel) {
      salvarNaEEPROM(temp, ts);
    }
  } else {
    Serial.println(F("Falha na rota TCP (Blynk). Salvando localmente."));
    uint32_t ts = (uint32_t)(millis() / 1000UL);
    salvarNaEEPROM(temp, ts);
  }

  delay(5000);
}

// ---------------- EEPROM helpers ----------------
void eepromInit() {
  // No need to EEPROM.begin() on AVR; just sanity-load header
  uint16_t head = 0xFFFF;
  uint16_t count = 0xFFFF;
  EEPROM.get(HEADER_ADDR, head);
  EEPROM.get(HEADER_COUNT_ADDR, count);
  if (head == 0xFFFF || head >= MAX_RECORDS) head = 0;
  if (count == 0xFFFF || count > MAX_RECORDS) count = 0;
  eep_head = head;
  eep_count = count;
}

void eepromSaveHeader() {
  EEPROM.put(HEADER_ADDR, eep_head);
  EEPROM.put(HEADER_COUNT_ADDR, eep_count);
}

void salvarNaEEPROM(float valor, uint32_t timestamp) {
  int writePos = eep_head;
  int addr = DATA_START + writePos * RECORD_SIZE;
  EEPROM.put(addr, valor);           // 4 bytes
  EEPROM.put(addr + 4, timestamp);   // 4 bytes

  eep_head = (eep_head + 1) % MAX_RECORDS;
  if (eep_count < MAX_RECORDS) eep_count++;
  eepromSaveHeader();

  Serial.print(F("[EEPROM] Gravado: "));
  Serial.print(valor);
  Serial.print(F(" @ "));
  Serial.println(timestamp);
}

// Gera os registros em ordem cronológica e envia por callback
void forEachEepromRecord(void (*cb)(float, uint32_t)) {
  if (eep_count == 0) return;
  int start = (eep_head + MAX_RECORDS - eep_count) % MAX_RECORDS;
  for (int i = 0; i < eep_count; ++i) {
    int pos = (start + i) % MAX_RECORDS;
    int addr = DATA_START + pos * RECORD_SIZE;
    float v = 0.0;
    uint32_t ts = 0;
    EEPROM.get(addr, v);
    EEPROM.get(addr + 4, ts);
    cb(v, ts);
  }
}

void clearEEPROMBuffer() {
  eep_head = 0;
  eep_count = 0;
  eepromSaveHeader();
}

// ---------------- Vercel/FastAPI HTTP via AT ----------------
bool enviarParaVercel(float temp, uint32_t timestamp, const char* origem) {
  // GET /update?temp=XX.X&timestamp=123&origem=realtime|offline_sync
  String path = VERCEL_PATH;
  path += "?temp=";
  path += String(temp, 2);
  path += "&timestamp=";
  path += String(timestamp);
  path += "&origem=";
  path += origem;

  String req = "GET ";
  req += path;
  req += " HTTP/1.1\r\nHost: ";
  req += VERCEL_HOST;
  req += "\r\nConnection: close\r\n\r\n";

  // Testa resolução antes de abrir TCP
  String pingCmd = "AT+PING=\"";
  pingCmd += VERCEL_HOST;
  pingCmd += "\"";
  String pingResp = enviaComando(pingCmd, 3000);
  Serial.print(F("[Vercel] PING resp: "));
  Serial.println(pingResp);

  String cstart = "AT+CIPSTART=\"TCP\",\"";
  cstart += VERCEL_HOST;
  cstart += "\",80";

  String resp = enviaComando(cstart, 8000);
  if (resp.indexOf("OK") == -1 && resp.indexOf("CONNECT") == -1 && resp.indexOf("Linked") == -1) {
    Serial.println(F("[Vercel] Falha na CIPSTART"));
    return false;
  }

  String cipsend = "AT+CIPSEND=";
  cipsend += req.length();
  enviaComando(cipsend, 2000);

  String prompt = "";
  unsigned long tlim = millis() + 2000UL;
  while (millis() < tlim) {
    while (espSerial.available()) {
      char c = espSerial.read();
      prompt += c;
    }
    if (prompt.indexOf(">") != -1) break;
  }

  if (prompt.indexOf(">") != -1) {
    espSerial.print(req);
    String after = enviaComando("", 5000);
    if (after.indexOf("SEND OK") != -1 || after.indexOf("200") != -1 || after.indexOf("OK") != -1) {
      Serial.println(F("[Vercel] Envio OK"));
      return true;
    }
  } else {
    String respSend = enviaComando(req, 6000);
    if (respSend.indexOf("SEND OK") != -1 || respSend.indexOf("200") != -1 || respSend.indexOf("OK") != -1) {
      Serial.println(F("[Vercel] Envio OK"));
      return true;
    }
  }

  Serial.println(F("[Vercel] Envio falhou"));
  return false;
}

// Lê todos, envia para PocketBase e limpa se tudo OK
void sincronizarDadosEEPROM() {
  if (eep_count == 0) {
    Serial.println(F("[EEPROM] Nada para sincronizar."));
    return;
  }

  bool allOK = true;
  Serial.println(F("[EEPROM] Sincronizando dados..."));

  // Simpler approach: iterate and send one-by-one; if any fails, abort and keep buffer
  int start = (eep_head + MAX_RECORDS - eep_count) % MAX_RECORDS;
  for (int i = 0; i < eep_count; ++i) {
    int pos = (start + i) % MAX_RECORDS;
    int addr = DATA_START + pos * RECORD_SIZE;
    float v = 0.0;
    uint32_t ts = 0;
    EEPROM.get(addr, v);
    EEPROM.get(addr + 4, ts);
    bool ok = enviarParaVercel(v, ts, "offline_sync");
    if (!ok) { allOK = false; break; }
  }

  if (allOK) {
    clearEEPROMBuffer();
    Serial.println(F("[EEPROM] Sincronização completa, buffer limpo."));
  } else {
    Serial.println(F("[EEPROM] Sincronização incompleta, manter dados para retry."));
  }
}