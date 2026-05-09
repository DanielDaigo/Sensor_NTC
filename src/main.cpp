#include <Arduino.h>
#include <SoftwareSerial.h>
#include <math.h>
#include <EEPROM.h>
#include "secrets.h"

SoftwareSerial espSerial(2, 3);

// --- CONFIGURAÇÕES GERAIS ---
const char* ssid = SECRET_SSID; 
const char* password = SECRET_PASS;

const String host = "api.thingspeak.com";
const String apiKey = SECRET_TS_API_KEY; 

const int pinoSensor = A0;
const float resistorFixo = 10000.0;

// --- ESTRUTURA E CONTROLE DA EEPROM ---
struct Registro {
  float temperatura;
  unsigned long tempoSalvo; 
};

const int ENDERECO_ASSINATURA = 0; 
const int ENDERECO_CONTADOR = 1;   
const int ENDERECO_DADOS = 2;      
const int MAX_REGISTROS = 100;     
const byte ASSINATURA_FORMATACAO = 0x42; 

// --- CONTROLE DE TEMPO OFFLINE ---
unsigned long ultimoSalvamentoOffline = 0;
const unsigned long INTERVALO_OFFLINE = 600000; // 10 minutos em milissegundos (10 * 60 * 1000)
bool redeEstavaOffline = false;

// --- A NOSSA ARMA SECRETA ---
String enviaComando(String comando, const int timeout) {
  String resposta = "";
  while (espSerial.available()) { espSerial.read(); }
  
  espSerial.print(comando + "\r\n");
  unsigned long tempoLimit = millis() + timeout; 
  
  while (tempoLimit > millis()) {
    while (espSerial.available()) {
      char c = espSerial.read();
      resposta += c; 
    }
  }
  return resposta;
}

// --- FUNÇÕES DA EEPROM ---
void formatarEEPROM() {
  byte assinatura = EEPROM.read(ENDERECO_ASSINATURA);
  if (assinatura != ASSINATURA_FORMATACAO) {
    Serial.println(F("[EEPROM] Memoria virgem. Formatando com zeros..."));
    for (int i = 0; i < 1024; i++) EEPROM.write(i, 0); 
    EEPROM.write(ENDERECO_ASSINATURA, ASSINATURA_FORMATACAO); 
    EEPROM.write(ENDERECO_CONTADOR, 0); 
    Serial.println(F("[EEPROM] Formatacao concluida!"));
  }
}

void salvarDadosOffline(float temp) {
  byte quantidade = EEPROM.read(ENDERECO_CONTADOR);
  
  if (quantidade < MAX_REGISTROS) {
    int enderecoAtual = ENDERECO_DADOS + (quantidade * sizeof(Registro));
    
    Registro reg;
    reg.temperatura = temp;
    reg.tempoSalvo = millis(); 
    
    EEPROM.put(enderecoAtual, reg); 
    
    quantidade++;
    EEPROM.write(ENDERECO_CONTADOR, quantidade); 
    
    Serial.print(F("[EEPROM] Offline. Dado e Tempo salvos. Fila: "));
    Serial.print(quantidade);
    Serial.println(F("/100"));
  } else {
    Serial.println(F("[EEPROM] ERRO: Fila de memoria cheia!"));
  }
}

void enviarParaNuvem(float temp, unsigned long idadeSegundos) {
    String url = "/update?api_key=" + apiKey + "&field1=" + String(temp, 2) + "&field2=" + String(idadeSegundos);
    String requisicao = "GET " + url + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    String cmdSend = "AT+CIPSEND=" + String(requisicao.length());
    
    enviaComando(cmdSend, 2000); 
    enviaComando(requisicao, 5000); 
    enviaComando("AT+CIPCLOSE", 1000);
}

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  delay(2000); 

  Serial.println(F("\n--- SISTEMA IOT: RESILIENCIA AVANCADA (10 MIN) ---"));
  formatarEEPROM(); 
  
  enviaComando("AT+RST", 4000); 
  enviaComando("AT+CWQAP", 1000); 
  enviaComando("AT+CWMODE=1", 1000); 
  enviaComando("AT+CWDHCP_DEF=1,1", 1000); 
  
  String comandoConexao = "AT+CWJAP=\"";
  comandoConexao += ssid;
  comandoConexao += "\",\"";
  comandoConexao += password;
  comandoConexao += "\"";
  
  Serial.print(F("Conectando ao Wi-Fi..."));
  enviaComando(comandoConexao, 15000); 
}

void loop() {
  Serial.println(F("\n--- NOVA LEITURA ---"));
  
  int valorADC = analogRead(pinoSensor);
  if (valorADC == 0) return;
  
  float rNTC = resistorFixo * ((1023.0 / (float)valorADC) - 1.0);
  float tempAtual = 1.0 / ((1.0 / 298.15) + (1.0 / 3950.0) * log(rNTC / 10000.0)) - 273.15;

  Serial.print(F("Temperatura atual: ")); Serial.print(tempAtual); Serial.println(F(" C"));

  String respTCP = enviaComando("AT+CIPSTART=\"TCP\",\"" + host + "\",80", 5000);

  if (respTCP.indexOf("CONNECT") != -1 || respTCP.indexOf("OK") != -1) {
    Serial.println(F("[REDE] Conectado a internet."));
    redeEstavaOffline = false; // Reseta o estado de rede

    // 1. VERIFICA SE TEM DADOS ATRASADOS NA EEPROM
    byte dadosAtrasados = EEPROM.read(ENDERECO_CONTADOR);
    
    if (dadosAtrasados > 0 && dadosAtrasados <= MAX_REGISTROS) {
      Serial.print(F("[SYNC] Descarregando ")); Serial.print(dadosAtrasados); Serial.println(F(" registros atrasados..."));
      
      for (int i = 0; i < dadosAtrasados; i++) {
        Registro regAtrasado;
        int endereco = ENDERECO_DADOS + (i * sizeof(Registro));
        EEPROM.get(endereco, regAtrasado);
        
        unsigned long tempoAtual = millis();
        unsigned long idadeSegundos = 0;
        
        if (tempoAtual >= regAtrasado.tempoSalvo) {
          idadeSegundos = (tempoAtual - regAtrasado.tempoSalvo) / 1000;
        } else {
           idadeSegundos = ((0xFFFFFFFF - regAtrasado.tempoSalvo) + tempoAtual) / 1000;
        }
        
        Serial.print(F("Dado offline: ")); Serial.print(regAtrasado.temperatura);
        Serial.print(F(" | Idade: ")); Serial.print(idadeSegundos); Serial.println(F(" segundos atras."));
        
        enviaComando("AT+CIPSTART=\"TCP\",\"" + host + "\",80", 4000);
        enviarParaNuvem(regAtrasado.temperatura, idadeSegundos);
        delay(16000); // Respeita Rate Limit do ThingSpeak
      }
      
      EEPROM.write(ENDERECO_CONTADOR, 0); // Limpa a fila
      Serial.println(F("[SYNC] Memoria limpa. Sincronizacao concluida."));
      enviaComando("AT+CIPSTART=\"TCP\",\"" + host + "\",80", 4000);
    }

    // 2. ENVIA O DADO ATUAL (Idade 0)
    Serial.println(F("Enviando pacote HTTP em tempo real..."));
    enviarParaNuvem(tempAtual, 0);

  } else {
    Serial.println(F("[REDE] ERRO TCP. Sem internet."));
    unsigned long tempoAtual = millis();

    // Se acabou de cair OU já passou 10 minutos desde o último salvamento
    if (!redeEstavaOffline || (tempoAtual - ultimoSalvamentoOffline >= INTERVALO_OFFLINE)) {
      Serial.println(F("Ativando Resiliencia. Gravando na EEPROM..."));
      salvarDadosOffline(tempAtual);
      ultimoSalvamentoOffline = tempoAtual;
      redeEstavaOffline = true;
    } else {
      unsigned long tempoRestante = (INTERVALO_OFFLINE - (tempoAtual - ultimoSalvamentoOffline)) / 1000;
      Serial.print(F("Aguardando intervalo. Proxima gravacao na EEPROM em: "));
      Serial.print(tempoRestante);
      Serial.println(F(" segundos."));
    }
  }

  Serial.println(F("Aguardando proximo ciclo de leitura...\n"));
  delay(20000); 
}