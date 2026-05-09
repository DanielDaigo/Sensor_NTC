# Nota de Entrega - Sensor NTC

## Visão geral

Projeto de telemetria com leitura de sensor NTC de 10k usando Arduino Uno com ESP-01S em modo AT.

## Referência arquitetural

Para detalhes completos da arquitetura tolerante a falhas, consulte o [registro arquitetural](registro_arquitetural.md). Este documento resume o estado final do sistema sem repetir as decisões de engenharia.

## Arquitetura

- Sensor NTC em `A0`.
- Arduino Uno/ESP-01S faz o processamento e a comunicação Wi-Fi.
- O firmware usa TCP puro na porta 80 para publicar no ThingSpeak.
- O ThingSpeak atua como painel de borda e relay para o backend.
- O backend FastAPI na Vercel recebe `idade_segundos` e grava no Firestore.
- A EEPROM funciona como fila offline quando não há internet.

## Fluxo de dados

### Operação online

1. O firmware lê a temperatura e publica no ThingSpeak.
2. O ThingSpeak encaminha o dado para o backend por webhook.
3. O backend grava o documento na coleção `telemetria` do Firestore.

### Operação offline

1. O firmware tenta reconectar ao hotspot.
2. Se continuar sem rede, grava a temperatura na EEPROM no máximo a cada 10 minutos.
3. Cada registro guarda temperatura e `millis()` do instante da falha.
4. Quando a conexão volta, os dados são reenviados com idade retroativa calculada em `idade_segundos`.

## Backend

O backend está documentado no repositório próprio e usa:

- FastAPI
- Firebase Admin
- Firestore
- Vercel

Endpoint atual:

- `GET /update`
- `POST /update`

Parâmetros usados pelo firmware:

- `temp`
- `idade_segundos`
- `origem`
- `dispositivo`

## Estado final do firmware

- Compila com PlatformIO.
- Usa ThingSpeak como relay e painel de borda.
- Grava e sincroniza dados offline via EEPROM.
- Envia `field1` com temperatura e `field2` com idade do dado.
- Mantém `thingspeak_relay` como origem lógica esperada no fluxo normal.

## Observações

- Intervalo de leitura atual: 20 segundos.
- Intervalo offline de gravação: 10 minutos.
- O descarregamento da fila respeita o rate limit de 15 segundos do ThingSpeak.
- O Firestore grava o horário UTC do servidor e reconstrói o timestamp quando `idade_segundos` é informado.
