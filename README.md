# Sensor NTC com ESP-01S, ThingSpeak, EEPROM e backend na Vercel

Projeto de telemetria para leitura de um sensor NTC de 10k usando Arduino Uno com ESP-01S em modo AT.

## Visão geral

O firmware faz o seguinte:

- lê a temperatura no pino `A0`;
- envia a leitura para o ThingSpeak usando TCP puro na porta 80;
- usa o ThingSpeak como painel de borda e relay para o backend;
- grava leituras na EEPROM quando a rede cai;
- recalcula a idade do dado e descarrega a fila offline quando a conexão volta.

## Comportamento atual

- Quando há internet:
  - lê a temperatura e publica no ThingSpeak em HTTP puro;
  - envia `field1` com a temperatura e `field2` com a idade do dado em segundos;
  - descarrega qualquer fila offline antes de enviar a leitura atual.
- Quando está offline:
  - tenta reconectar ao hotspot;
  - grava na EEPROM no máximo uma vez a cada 10 minutos;
  - preserva a fila até o retorno da conectividade.

## Backend

O backend fica em outro projeto e usa:

- FastAPI
- Firebase Admin
- Firestore
- Vercel

O endpoint atual é:

- `GET /update`
- `POST /update`

Parâmetros aceitos:

- `temp` - temperatura lida pelo sensor;
- `idade_segundos` - idade do dado em segundos, opcional;
- `origem` - origem lógica da leitura, opcional;
- `dispositivo` - identificador lógico do sensor, opcional.

Se `idade_segundos` for maior que zero, a API subtrai esse valor do relógio UTC do servidor para gerar o timestamp retroativo exato no Firestore.

A origem esperada para o fluxo normal é `thingspeak_relay`, recebida a partir do webhook do ThingSpeak.

Os dados são gravados na coleção `telemetria` do Firestore.

Para detalhes completos da arquitetura tolerante a falhas, consulte o [registro arquitetural](registro_arquitetural.md).

## Arquitetura

```text
Sensor NTC -> Arduino Uno/ESP-01S -> ThingSpeak -> Webhook -> Backend Vercel -> Firestore
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
- ThingSpeak configurado como relay e painel de borda

## Como compilar

```powershell
C:\Users\danie\.platformio\penv\Scripts\platformio.exe run
```

## Como testar

1. Abra o monitor serial em 9600.
2. Ligue o hotspot e confirme o envio para o ThingSpeak e o webhook do backend.
3. Desligue a rede e aguarde o firmware registrar valores na EEPROM.
4. Refaça a conexão e confirme o descarregamento da fila offline com idade do dado retroativa.

## Observações

- O intervalo de leitura continua em 20 segundos no firmware atual.
- O intervalo offline para gravação na EEPROM é de 10 minutos.
- A sincronização offline respeita o rate limit de 15 s do ThingSpeak.
- A comunicação com o ThingSpeak usa TCP puro na porta 80 para economizar RAM no ESP-01S.
