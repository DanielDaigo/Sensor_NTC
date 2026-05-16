# Sensor NTC com ESP-01S, MQTT, InfluxDB e Grafana

Projeto de telemetria para leitura de um sensor NTC de 10k usando Arduino Uno com ESP-01S em modo AT. Arquitetura distribuída com MQTT broker local, persistência em InfluxDB e visualização em Grafana Cloud.

## Visão geral

O firmware faz o seguinte:

- lê a temperatura no pino `A0`;
- publica a leitura para um broker MQTT via comandos AT (AT+MQTTPUB);
- usa tópico estruturado: `sensores/{device_id}/temperatura`;
- grava leituras na EEPROM quando a rede cai;
- recalcula a idade do dado e descarrega a fila offline quando a conexão volta.

## Comportamento atual

- Quando há internet (MQTT conectado):
  - lê a temperatura e publica no broker MQTT;
  - envia payload JSON: `{"t": 23.45, "i": 0, "d": "marica_x"}` (temperatura, idade em segundos, device_id);
  - descarrega qualquer fila offline antes de enviar a leitura atual.
- Quando está offline:
  - tenta reconectar ao hotspot;
  - grava na EEPROM no máximo uma vez a cada 10 minutos;
  - preserva a fila até o retorno da conectividade.

## Stack de Ingestão e Visualização

O stack completo roda na VM Oracle Cloud Always Free:

- **Mosquitto:** Broker MQTT na porta 1883, aguardando publicações do firmware (tópico: `sensores/{device_id}/temperatura`).
- **Bridge Python:** Serviço systemd que assina o tópico MQTT wildcard `sensores/+/temperatura`, interpreta o JSON e grava em InfluxDB.
- **InfluxDB v1:** Banco de dados de séries temporais. Database: `telemetria`, measurement: `temperatura`, com tags `dispositivo` e fields `valor` + `idade_segundos`.
- **Grafana Cloud Free:** Dashboard externo que consome dados do InfluxDB via HTTP (porta 8086).

Para detalhes completos de configuração, credenciais e contrato de dados, consulte [dicionario.md](../dicionario.md) e [plan.md](../plan.md).

## Arquitetura

```text
Sensor NTC -> Arduino Uno/ESP-01S -> Mosquitto Broker -> Bridge Python -> InfluxDB -> Grafana Cloud
                               └────────────── EEPROM quando offline ──────────────┘
```

## Arquivos principais

- `src/main.cpp` - firmware principal com MQTT.
- `src/secrets.h` - credenciais locais (ignorado pelo git).
- `src/secrets_example.h` - template de credenciais.
- `platformio.ini` - configuração do PlatformIO.

## Requisitos

- PlatformIO
- Arduino compatível com ESP8266 via AT
- Sensor NTC 10k
- Hotspot Wi-Fi
- VM Oracle Cloud (Always Free) com Mosquitto, InfluxDB, Bridge Python (conforme [plan.md](../plan.md))
- Grafana Cloud Free (conta gratuita)

## Como compilar

```powershell
C:\Users\danie\.platformio\penv\Scripts\platformio.exe run
```

## Como testar

1. **Setup local:** Configure `src/secrets.h` com a credencial MQTT e IP da VM Oracle.
2. **Compilar:** `platformio run`
3. **Flash e monitore:**
   - Conecte Arduino + ESP-01S
   - Abra monitor serial em 9600 baud
   - Confirme a conexão ao hotspot e ao broker MQTT
4. **Validar publicação:**
   - Na VM Oracle, rode: `mosquitto_sub -h localhost -p 1883 -u sensor_user -P senha_iot_123 -t "sensores/#" -v`
   - Veja as mensagens JSON sendo publicadas
5. **Teste offline:**
   - Desligue a rede do Arduino (desconecte hotspot)
   - Aguarde o firmware registrar valores na EEPROM (a cada 10 minutos)
   - Reconecte e confirme o descarregamento da fila com `idade_segundos > 0`
6. **Validar InfluxDB:**
   - Na VM: `curl -G 'http://localhost:8086/query' --data-urlencode "db=telemetria" --data-urlencode "u=grafana_user" --data-urlencode "p=GrafanaPass789" --data-urlencode "q=SELECT * FROM temperatura LIMIT 10"`
7. **Dashboard Grafana:** Acesse sua stack Grafana Cloud e visualize o painel com últimas leituras

## Observações

- O intervalo de leitura continua em 20 segundos no firmware atual.
- O intervalo offline para gravação na EEPROM é de 10 minutos.
- A comunicação com o broker MQTT usa TCP puro na porta 1883 (sem TLS) para economizar RAM no ESP-01S.
- O payload JSON é compacto (`{"t": 23.45, "i": 0, "d": "marica_x"}`) para respeitar o limite de 256 bytes do AT+MQTTPUB.
- A sincronização offline preserva `idade_segundos` para reconstruir timestamps retroativos no InfluxDB.
