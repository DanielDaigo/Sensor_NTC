# Registro de Arquitetura e Decisões: Termômetro IoT Maricá

## Visão Geral

Este documento registra as decisões de engenharia, os desafios de hardware e a topologia final do sistema de telemetria de temperatura ambiente. O sistema coleta dados via NTC 10k, envia para a nuvem utilizando um ESP-01S (Firmware AT) e garante resiliência de dados contra quedas de internet.

## Fonte da Verdade

Este arquivo é a referência canônica da arquitetura do projeto. Quando houver divergência entre comentários de código, READMEs antigos ou anotações parciais, este documento deve prevalecer para decisões sobre fluxo de dados, contratos de API, limites de hardware e estratégia de tolerância a falhas.

## 1. Topologia da Arquitetura

A arquitetura foi redesenhada para ser totalmente auto-hospedada na VM Oracle Cloud, eliminando dependências externas (Vercel, Firestore).

- **Borda (Edge):** Arduino Uno + NTC 10k + ESP-01S. O firmware C++ coleta dados e gerencia a conexão via comandos AT via comunicação serial (SoftwareSerial). Comunica-se exclusivamente via MQTT (Porta 1883, TCP puro) para poupar memória RAM.
- **Ingestão Central:** Mosquitto MQTT Broker. Recebe publicações do firmware via AT+MQTTPUB em tópicos estruturados (`sensores/{device_id}/temperatura`). Atua como concentrador de dados.
- **Transformação (ETL):** Bridge Python (systemd service). Assina o tópico wildcard `sensores/+/temperatura`, interpreta o payload JSON, recalcula timestamps retroativos quando `idade_segundos > 0`, e grava em InfluxDB via HTTP line protocol.
- **Persistência:** InfluxDB v1. Database `telemetria`, measurement `temperatura`, com tag `dispositivo` (indexada) e fields `valor` + `idade_segundos`.
- **Visualização:** Grafana Cloud Free (SaaS externo). Consome dados do InfluxDB via HTTP (porta 8086) e exibe dashboards interativos.

## 2. Decisões Críticas e Desafios Superados

### 2.0. Migração de ThingSpeak para MQTT (2026)

- **Contexto Anterior:** A v1.0 usava ThingSpeak como intermediário para contornar limitações de SSL no ESP-01S, Vercel como backend e Firestore como persistência.
- **Desafios com a Arquitetura Anterior:**
  - Dependência de serviços terceirizados (ThingSpeak, Vercel, Firebase).
  - Complexidade de pipeline (firmware → HTTP → webhook → HTTPS → Firestore).
  - Latência agregada e pontos únicos de falha.
  - Dificuldade em reconstruir timestamps retroativos sem RTC no Arduino.
- **Solução MQTT:** Migração para arquitetura self-hosted em VM Always Free Oracle Cloud:
  - **Vantagens:** Controle total, sem taxa de serviço, observabilidade total, latência reduzida.
  - **Contrato Simplificado:** Firmware → MQTT JSON → Bridge ETL → InfluxDB → Grafana.
  - **Preservação:** 100% da lógica offline, EEPROM magic byte, cálculo de `idade_segundos`, sync retroativa — mantida intacta.
  - **Trade-off Segurança:** MQTT sem TLS na porta 1883 (aceitável para contexto acadêmico; documentado em ADR-06).

### 2.1. O Estouro de Buffer SSL (Legado — resolvido com MQTT)

- **Desafio:** A tentativa inicial de conectar o ESP-01S diretamente à Vercel (HTTPS/Porta 443) resultou no erro `CLOSED`. O firmware AT possui um buffer limitado (mesmo expandido para 4096 bytes via `AT+CIPSSLSIZE`) que não suporta o tamanho dos certificados SSL modernos da Vercel.
- **Solução:** O uso do ThingSpeak como intermediário, permitindo que o microcontrolador envie dados via texto plano (Porta 80) sem custos criptográficos.

### 2.2. Migração do Blynk para o ThingSpeak

- **Motivo:** O Blynk operava bem em tempo real, mas era inadequado para manipulação de séries temporais atrasadas (Offline Sync). O ThingSpeak forneceu a flexibilidade necessária para receber dados assíncronos e o motor de Webhook nativo (ThingHTTP) essencial para a integração com o Firebase.

### 2.3. Resiliência e Lógica Offline (EEPROM) — Preservada em Migração MQTT

- **Desafio:** Como garantir a integridade dos dados durante quedas de internet sem saturar a memória flash do Arduino (limite de ~100.000 ciclos de gravação).
- **Implementação (v1.0, mantida em v2.0):**
  - Criação de uma rotina utilizando a biblioteca `EEPROM` e um "Magic Byte" (0x42) para formatar e prevenir leitura de lixo de memória.
  - Implementação de uma trava de tempo (10 minutos / 600.000 ms) limitando a frequência de gravação física na EEPROM quando offline.
  - Estrutura de dados armazenando a temperatura e o `millis()` exato do momento da falha.
- **Migração para MQTT:** O mesmo sistema de EEPROM permanece intacto. Quando a rede volta, a bridge Python recebe o JSON com `idade_segundos`, recalcula o timestamp original (`timestamp = now - timedelta(seconds=idade_segundos)`) e grava no InfluxDB com a data retroativa exata.

### 2.4. Sincronização Temporal sem Relógio Físico (RTC) — Migrado para InfluxDB

- **Desafio:** O Arduino Uno perde a noção absoluta de tempo ao operar offline, inviabilizando o envio de timestamps retroativos absolutos (ex: `created_at` em formato ISO).
- **Solução v1.0 (ThingSpeak/Firestore):** O firmware calcula a "idade do dado" em segundos subtraindo o `millis()` salvo do `millis()` atual no momento do restabelecimento da rede. O dado é repassado no `field2` do ThingSpeak. A API FastAPI na Vercel recebe o parâmetro `idade_segundos`, subtrai esse valor do horário UTC atual do servidor via `datetime.timedelta`, e grava o documento no Firestore com o timestamp retroativo exato do momento da leitura original.
- **Solução v2.0 (MQTT/InfluxDB):** O firmware continua calculando `idade_segundos` da mesma forma. O payload JSON inclui este valor: `{"t": 23.45, "i": 120, "d": "marica_x"}`. A bridge Python recebe esse JSON, subtrai `idade_segundos` do `datetime.now(timezone.utc)` via `timedelta`, e grava no InfluxDB com o timestamp reconstruído exato no momento da leitura original. O InfluxDB indexa por tag `dispositivo`, preservando a capacidade de filtrar por sensor.

## 3. Ambiente de Desenvolvimento

- **Firmware:** C++ / PlatformIO (Framework Arduino para board Uno). Configuração serial com monitor a 9600 baud.
- **Variáveis Sensíveis:** Credenciais MQTT isoladas em um arquivo não-versionado `secrets.h`.
- **Stack Backend:** Python 3, paho-mqtt, influxdb (biblioteca Python), python-dotenv. Bridge roda como systemd service na VM Oracle.
- **Infraestrutura:** Oracle Cloud Always Free (VM.Standard.A1.Flex, Ubuntu 22.04), Mosquitto, InfluxDB v1, Grafana Cloud Free.

## 4. Contratos Operacionais

- **MQTT:** o firmware publica via AT+MQTTPUB (TCP puro, porta 1883) para economizar RAM e evitar TLS.
- **Tópico:** `sensores/{device_id}/temperatura` (wildcard assinado pela bridge: `sensores/+/temperatura`).
- **Payload:** JSON compacto com chaves curtas: `{"t": 23.45, "i": 0, "d": "marica_x"}` (temp, idade, device_id).
- **EEPROM:** o `Magic Byte` `0x42` sinaliza que a memória foi formatada e evita leitura de lixo na primeira inicialização.
- **Offline Sync:** a gravação física na EEPROM é limitada a 10 minutos entre eventos para preservar a vida útil da memória.
- **Descarregamento da fila:** quando a conectividade volta, a fila offline é esvaziada respeitando a velocidade de publicação MQTT (ex: 500ms entre mensagens) antes do envio da leitura atual.
- **InfluxDB:** Bridge grava com timestamp reconstruído: `timestamp = now(UTC) - timedelta(seconds=idade_segundos)`. Tag `dispositivo` é indexada; fields `valor` e `idade_segundos` são numéricos.
