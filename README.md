# Termômetro IoT Fase 4

Sistema de telemetria térmica com nó de borda em Arduino/ESP-01S, API Python na Oracle Cloud, persistência em InfluxDB e visualização em Grafana.

Status atual: Fase 4 consolidada. A arquitetura definitiva elimina os caminhos antigos de Vercel, Firestore e ThingSpeak e passa a operar com ingestão direta, bufferização local em EEPROM e sincronização retroativa quando a rede retorna.

## Visão rápida

- Sensor NTC de 10 kΩ no firmware principal
- Conversão local da temperatura na borda
- Fallback em EEPROM quando a conectividade cai
- API Python para ingestão na Oracle Cloud
- Armazenamento em séries temporais com retenção de longo prazo
- Painéis no Grafana para operação e demonstração

## Estrutura principal

- [src/main.cpp](src/main.cpp) - firmware principal do sensor
- [platformio.ini](platformio.ini) - configuração do PlatformIO
- API Python hospedada na Oracle Cloud - serviço de ingestão da Fase 4
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - registro técnico definitivo
- [docs/GUIA_DIDATICO.md](docs/GUIA_DIDATICO.md) - explicação didática para banca e leigos

## Como buildar o firmware

1. Abra a pasta `Sensor_NTC` no PlatformIO.
2. Ajuste `src/secrets.h` com SSID, senha e chave da API.
3. Compile o projeto:

```powershell
platformio run
```

4. Faça upload para a placa, se necessário:

```powershell
platformio run --target upload
```

5. Abra o monitor serial para acompanhar a leitura e o envio.

## Como rodar a API localmente

O serviço Python da Fase 4 é publicado na Oracle Cloud e não depende mais de uma pasta local neste workspace.

```powershell
curl http://seu-endpoint-oracle/api/health
```

## Fluxo operacional

1. O sensor lê a temperatura.
2. O firmware converte o valor localmente.
3. Se a rede estiver disponível, o dado é enviado imediatamente.
4. Se a rede cair, a leitura entra em fila na EEPROM.
5. Ao reconectar, a fila é descarregada com reconstrução temporal.
6. O back-end grava a telemetria e o Grafana mostra a evolução.

## Observações

- O código do sensor trabalha com o intervalo de leitura definido no firmware.
- O diretório `src/secrets.h` não deve ser versionado.
- Os documentos históricos antigos foram substituídos pela documentação consolidada em `docs/`.
