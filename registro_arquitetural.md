# Registro de Arquitetura e Decisões: Termômetro IoT Maricá

## Visão Geral

Este documento registra as decisões de engenharia, os desafios de hardware e a topologia final do sistema de telemetria de temperatura ambiente. O sistema coleta dados via NTC 10k, envia para a nuvem utilizando um ESP-01S (Firmware AT) e garante resiliência de dados contra quedas de internet.

## Fonte da Verdade

Este arquivo é a referência canônica da arquitetura do projeto. Quando houver divergência entre comentários de código, READMEs antigos ou anotações parciais, este documento deve prevalecer para decisões sobre fluxo de dados, contratos de API, limites de hardware e estratégia de tolerância a falhas.

## 1. Topologia da Arquitetura

A arquitetura foi dividida em microsserviços devido às restrições criptográficas do hardware de borda.

- **Borda (Edge):** Arduino Uno + NTC 10k + ESP-01S. O firmware C++ coleta dados e gerencia a conexão via comandos AT via comunicação serial (SoftwareSerial). Comunica-se exclusivamente via TCP puro (Porta 80) para poupar memória RAM.
- **Ingestão e Painel de Borda:** ThingSpeak. Recebe os dados brutos via HTTP GET. Serve como dashboard de visualização rápida em tempo real e atua como acionador da camada de backend.
- **Middleware (Cloud Relay):** Webhook do ThingSpeak, apoiado por ThingHTTP e fluxos React quando necessário. Ao receber um dado, empacota o payload e realiza a requisição HTTPS para a Vercel.
- **Backend API:** Desenvolvido em Python com FastAPI, hospedado na Vercel. Recebe requisições via POST/GET no endpoint `/update` e aceita `idade_segundos` como contrato para reconstrução temporal.
- **Banco de Dados (Persistência Longa):** Google Cloud Firestore. A coleção `telemetria` armazena a temperatura, origem, identificador do dispositivo e o timestamp padronizado.

## 2. Decisões Críticas e Desafios Superados

### 2.1. O Estouro de Buffer SSL

- **Desafio:** A tentativa inicial de conectar o ESP-01S diretamente à Vercel (HTTPS/Porta 443) resultou no erro `CLOSED`. O firmware AT possui um buffer limitado (mesmo expandido para 4096 bytes via `AT+CIPSSLSIZE`) que não suporta o tamanho dos certificados SSL modernos da Vercel.
- **Solução:** O uso do ThingSpeak como intermediário, permitindo que o microcontrolador envie dados via texto plano (Porta 80) sem custos criptográficos.

### 2.2. Migração do Blynk para o ThingSpeak

- **Motivo:** O Blynk operava bem em tempo real, mas era inadequado para manipulação de séries temporais atrasadas (Offline Sync). O ThingSpeak forneceu a flexibilidade necessária para receber dados assíncronos e o motor de Webhook nativo (ThingHTTP) essencial para a integração com o Firebase.

### 2.3. Resiliência e Lógica Offline (EEPROM)

- **Desafio:** Como garantir a integridade dos dados durante quedas de internet sem saturar a memória flash do Arduino (limite de ~100.000 ciclos de gravação).
- **Implementação:**
  - Criação de uma rotina utilizando a biblioteca `EEPROM` e um "Magic Byte" (0x42) para formatar e prevenir leitura de lixo de memória.
  - Implementação de uma trava de tempo (10 minutos / 600.000 ms) limitando a frequência de gravação física na EEPROM quando offline.
  - Estrutura de dados armazenando a temperatura e o `millis()` exato do momento da falha.

### 2.4. Sincronização Temporal sem Relógio Físico (RTC)

- **Desafio:** O Arduino Uno perde a noção absoluta de tempo ao operar offline, inviabilizando o envio de timestamps retroativos absolutos (ex: `created_at` em formato ISO).
- **Solução Híbrida (Idade do Dado):** O firmware calcula a "idade do dado" em segundos subtraindo o `millis()` salvo do `millis()` atual no momento do restabelecimento da rede. O dado é repassado no `field2` do ThingSpeak.
  - A API FastAPI na Vercel recebe o parâmetro `idade_segundos`, subtrai esse valor do horário UTC atual do servidor via `datetime.timedelta`, e grava o documento no Firestore com o timestamp retroativo exato do momento da leitura original.
  - O fluxo de webhook esperado usa a origem lógica `thingspeak_relay`.

## 3. Ambiente de Desenvolvimento

- **Firmware:** C++ / PlatformIO (Framework Arduino para board Uno). Configuração serial com monitor a 9600 baud.
- **Variáveis Sensíveis:** Credenciais de rede isoladas em um arquivo não-versionado `secrets.h`.
- **Backend:** Python 3, FastAPI, uvicorn, Firebase Admin SDK.

## 4. Contratos Operacionais

- **Rede:** o firmware usa TCP puro na porta 80 com o ThingSpeak para economizar RAM e contornar limitações de SSL no ESP-01S.
- **EEPROM:** o `Magic Byte` `0x42` sinaliza que a memória foi formatada e evita leitura de lixo na primeira inicialização.
- **Offline Sync:** a gravação física na EEPROM é limitada a 10 minutos entre eventos para preservar a vida útil da memória.
- **Descarregamento da fila:** quando a conectividade volta, a fila offline é esvaziada respeitando o rate limit de 15 s do ThingSpeak antes do envio da leitura atual.
