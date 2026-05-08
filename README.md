# Sensor NTC com ESP8266, Blynk, EEPROM e Vercel

Projeto de telemetria para leitura de um sensor NTC de 10k usando uma placa Arduino com ESP8266 via AT commands.

## Visão geral

O firmware faz o seguinte:

- lê a temperatura no pino `A0`;
- envia a leitura em tempo real para o Blynk;
- envia a mesma leitura para uma API FastAPI hospedada na Vercel;
- grava leituras na EEPROM quando a rede cai;
- reenvia a fila offline quando a conexão volta.

## Comportamento atual

- Quando há internet:
  - envia a leitura a cada 5 segundos;
  - tenta publicar na Vercel com `temp`, `timestamp` e `origem=realtime`;
  - se a publicação falhar, salva o valor na EEPROM.
- Quando está offline:
  - tenta reconectar ao hotspot;
  - grava na EEPROM a cada 30 minutos;
  - marca os dados reenviados depois como `origem=offline_sync`.

## Backend

O backend fica em outro projeto e usa:

- FastAPI
- Firebase Admin
- Firestore
- Vercel

O endpoint atual é:

- `GET /update`

Parâmetros aceitos:

- `temp` - temperatura lida pelo sensor;
- `timestamp` - timestamp do dispositivo em segundos, opcional;
- `origem` - `realtime` ou `offline_sync`, opcional;
- `dispositivo` - identificador lógico do sensor, opcional.

Os dados são gravados na coleção `telemetria` do Firestore.

## Arquitetura

```text
Sensor NTC -> Arduino/ESP8266 -> Blynk
                         └----> Vercel -> Firestore
                         └----> EEPROM quando offline
```

## Arquivos principais

- `src/main.cpp` - firmware principal.
- `platformio.ini` - configuração do PlatformIO.

## Requisitos

- PlatformIO
- Arduino compatível com ESP8266 via AT
- Sensor NTC 10k
- Hotspot Wi-Fi
- Projeto Vercel com API FastAPI configurada para o Firestore

## Como compilar

```powershell
C:\Users\danie\.platformio\penv\Scripts\platformio.exe run
```

## Como testar

1. Abra o monitor serial em 9600.
2. Ligue o hotspot e confirme o envio para Blynk e Vercel.
3. Desligue a rede e aguarde o firmware registrar valores na EEPROM.
4. Refaça a conexão e confirme o reenvio dos dados com `origem=offline_sync`.

## Observações

- O intervalo online é de 5 segundos.
- O intervalo offline para gravação na EEPROM é de 30 minutos.
- O timestamp local é enviado ao backend para referência, mas o Firestore também grava o horário UTC do servidor.
