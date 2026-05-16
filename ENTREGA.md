# Nota de Entrega - Sensor NTC

## Visão geral

Projeto de telemetria com leitura de sensor NTC de 10k usando Arduino Uno com ESP-01S em modo AT. Arquitetura distribuída com MQTT broker local, persistência em InfluxDB e visualização em Grafana Cloud.

## Referência arquitetural

Para detalhes completos da arquitetura tolerante a falhas, consulte o [registro arquitetural](registro_arquitetural.md). Este documento resume o estado final do sistema sem repetir as decisões de engenharia.

## Arquitetura

- Sensor NTC em `A0`.
- Arduino Uno/ESP-01S faz o processamento e a comunicação Wi-Fi.
- O firmware publica em MQTT via AT+MQTTPUB na porta 1883.
- Mosquitto broker atua como concentrador central de dados.
- Bridge Python assina os tópicos MQTT e grava em InfluxDB.
- InfluxDB armazena séries temporais com tags e fields.
- Grafana Cloud Free exibe dashboards com as leituras.

## Fluxo de dados

### Operação online

1. O firmware lê a temperatura e publica no broker MQTT.
2. Tópico: `sensores/{device_id}/temperatura` — Payload JSON: `{"t": 23.45, "i": 0, "d": "marica_x"}`
3. Bridge Python assina `sensores/+/temperatura`, recebe o JSON e grava em InfluxDB.
4. Grafana Cloud consulta InfluxDB e exibe os dados em dashboard.

### Operação offline

1. O firmware tenta reconectar ao hotspot.
2. Se continuar sem rede, grava a temperatura na EEPROM no máximo a cada 10 minutos.
3. Cada registro guarda temperatura e `millis()` do instante da falha.
4. Quando a conexão volta, os dados são reenviados com idade retroativa calculada em `idade_segundos`.
5. Bridge transforma o JSON em escrita InfluxDB com timestamp reconstruído.

## Stack de Ingestão

Todo o stack roda na VM Oracle Cloud Always Free (136.248.96.131):

| Componente    | Tipo             | Porta | Descrição                                       |
| ------------- | ---------------- | ----- | ----------------------------------------------- |
| Mosquitto     | MQTT Broker      | 1883  | Aguarda publicações do firmware e da bridge     |
| Bridge Python | ETL (systemd)    | —     | Assina MQTT, interpreta JSON, grava em InfluxDB |
| InfluxDB v1   | Time Series DB   | 8086  | Armazena telemetria com tags e fields           |
| Grafana Cloud | Dashboard (SaaS) | 443   | Visualiza dados de InfluxDB remotamente         |

Para detalhes de credenciais e configuração, consulte [dicionario.md](../dicionario.md) e [plan.md](../plan.md).

## Contrato de Dados

**Tópico MQTT:** `sensores/{device_id}/temperatura`

**Payload JSON:**

```json
{ "t": 23.45, "i": 0, "d": "marica_x" }
```

| Campo | Tipo   | Descrição                                  |
| ----- | ------ | ------------------------------------------ |
| `t`   | float  | Temperatura em graus Celsius               |
| `i`   | int    | Idade do dado em segundos (0 = tempo real) |
| `d`   | string | Device ID (ex: marica_x)                   |

**Schema InfluxDB:**

- **Database:** `telemetria`
- **Measurement:** `temperatura`
- **Tag:** `dispositivo` (indexada, usada para filtro)
- **Fields:** `valor` (float), `idade_segundos` (int)
- **Timestamp:** Reconstruído: `agora - timedelta(seconds=i)`

## Estado final do firmware

- Compila com PlatformIO.
- Publica via AT+MQTTPUB em broker MQTT local.
- Grava e sincroniza dados offline via EEPROM (magic byte, DHCP forçado, sync com `idade_segundos`).
- Envia payload JSON compacto: `{"t": 23.45, "i": 0, "d": "marica_x"}`
- Mantém a lógica de resiliência offline 100% preservada da versão anterior.

## Observações

- Intervalo de leitura atual: 20 segundos.
- Intervalo offline de gravação EEPROM: 10 minutos.
- Payload MQTT está limitado a 256 bytes total; formato compacto respeita esse limite.
- Sem TLS na porta 1883 (trade-off segurança vs. economia de RAM no ESP-01S); documentado em ADR-06.
- InfluxDB recalcula timestamps retroativos: `timestamp = now - timedelta(seconds=idade_segundos)`.
