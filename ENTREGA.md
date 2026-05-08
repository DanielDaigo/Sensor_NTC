# Nota de Entrega - Sensor NTC

## Visão geral

Projeto de telemetria com leitura de sensor NTC de 10k usando Arduino com ESP8266 via AT commands.

## Arquitetura

- Sensor NTC em `A0`.
- Arduino/ESP8266 faz o processamento e a comunicação Wi-Fi.
- Blynk é usado para visualização em tempo real.
- Uma API FastAPI na Vercel recebe as leituras e grava no Firestore.
- A EEPROM funciona como fila offline quando não há internet.

## Fluxo de dados

### Operação online

1. O firmware lê a temperatura a cada 5 segundos.
2. A leitura é enviada ao Blynk.
3. A mesma leitura é enviada ao backend na Vercel.
4. O backend grava o documento na coleção `telemetria` do Firestore.

### Operação offline

1. O firmware tenta reconectar ao hotspot.
2. Se continuar sem rede, grava a temperatura na EEPROM a cada 30 minutos.
3. Cada registro guarda temperatura e timestamp local.
4. Quando a conexão volta, os dados são reenviados com `origem=offline_sync`.

## Backend

O backend está documentado em `BACKEND.md` e usa:

- FastAPI
- Firebase Admin
- Firestore
- Vercel

Endpoint atual:

- `GET /update`

Parâmetros usados pelo firmware:

- `temp`
- `timestamp`
- `origem`
- `dispositivo`

## Estado final do firmware

- Compila com PlatformIO.
- Mantém Blynk.
- Grava e sincroniza dados offline via EEPROM.
- Envia metadados para o backend com origem da leitura.

## Observações

- Intervalo online: 5 segundos.
- Intervalo offline: 30 minutos.
- O Firestore também grava o horário UTC do servidor.
