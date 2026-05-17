# Arquitetura Definitiva do Sistema de Telemetria IoT

## 1. Visão geral

Este documento consolida a arquitetura definitiva do sistema de telemetria térmica em tempo real. O projeto foi pensado para operar com resiliência na borda, ingestão confiável na nuvem e armazenamento otimizado para séries temporais.

O fluxo central é simples: o sensor mede a temperatura, o firmware converte o valor localmente, a rede recebe o dado quando está disponível e, quando há falha de conectividade, o próprio dispositivo segura a informação em memória não volátil para reenviá-la depois com o tempo correto.

## 2. Evolução arquitetural

### Fase 1 - Blynk

A primeira versão dependia de uma plataforma fechada. A limitação principal era a falta de controle sobre o caminho de ida e volta dos dados. Isso impedia o desenho de uma estratégia robusta de bufferização offline no firmware.

### Fase 2 - Vercel + Firestore

A segunda fase migrou para uma arquitetura serverless e baseada em documentos. O problema surgiu quando a telemetria passou a exigir frequências curtas e previsíveis: o cold start e o custo de manter dados brutos em um banco orientado a documentos se tornaram gargalos.

### Fase 3 - ThingSpeak

A terceira fase trouxe um relay comercial com limitação de taxa. Para um sensor que precisa esvaziar fila acumulada após uma queda de rede, isso virou bloqueio técnico.

### Fase 4 - Oracle Cloud com API própria, InfluxDB e Grafana

A fase atual elimina intermediários comerciais e concentra o controle na infraestrutura própria. Isso permite política de firewall dedicada, ingestão direta, descarregamento rápido de dados acumulados e retenção de longo prazo.

## 3. Camada de borda

O nó de borda faz mais do que apenas medir. Ele filtra leituras inválidas, converte o sinal do termistor em temperatura e decide o comportamento conforme o estado da rede.

### 3.1 Condicionamento de sinal

O sensor NTC de 10 kΩ trabalha com divisor de tensão. Leituras equivalentes a zero são descartadas porque normalmente representam curto, desconexão física ou leitura inválida do ADC.

A conversão em Celsius acontece localmente com a equação baseada no parâmetro Beta, o que reduz o trabalho do back-end e deixa o payload mais compacto.

### 3.2 Resiliência local em EEPROM

Quando a rede cai, o firmware deixa de confiar na transmissão imediata e passa a guardar os dados na EEPROM. O controle usa uma assinatura de memória para validar o estado da área gravada. Se a EEPROM não estiver formatada corretamente, ela é reinicializada.

Para reduzir desgaste, a gravação offline é espaçada. Em vez de escrever continuamente, o dispositivo grava a cada 10 minutos em modo restrito.

Cada registro guarda o valor de temperatura e um marcador temporal interno. Quando a conectividade retorna, o firmware calcula a idade do dado e envia essa informação junto ao payload para reconstrução cronológica no servidor.

## 4. Camada de ingestão

A API Python atua como porta de entrada da telemetria. Sua responsabilidade é receber a leitura, validar o conteúdo e encaminhar a gravação no armazenamento de séries temporais.

Na infraestrutura alvo, essa camada fica atrás de firewall dedicado e só expõe as portas necessárias para a operação. A autenticidade da requisição é protegida por chave de API no cabeçalho HTTP.

## 5. Persistência em InfluxDB

O InfluxDB é a escolha natural para telemetria porque foi feito para tempo e volume. Em vez de modelar cada leitura como documento genérico, o sistema registra pontos com timestamp, tag de dispositivo e valor de temperatura.

### 5.1 Volumetria

Com amostragem a cada 5 segundos, um dispositivo gera:

$$\text{Registros por dia} = \frac{86400}{5} = 17280$$

Em 10 anos:

$$17280 \times 365 \times 10 = 63072000$$

Considerando aproximadamente 3 bytes por ponto após compactação, a ordem de grandeza fica em torno de 189 MB por dispositivo, o que é compatível com armazenamento de longo prazo em um cenário de telemetria compacta.

### 5.2 Retenção

A política de retenção de 10 anos preserva histórico suficiente para auditoria, comparação sazonal e análise de anomalias sem inflar o banco de forma desnecessária.

## 6. Visualização em Grafana

O Grafana é a camada de leitura humana do sistema. Ele transforma pontos de série temporal em painéis operacionais.

### 6.1 Painéis

O painel de valor instantâneo exibe a última temperatura conhecida. O painel de tendência mostra a curva ao longo do tempo, com agregação por intervalo para evitar excesso de dados na tela.

### 6.2 Segurança de compartilhamento

O acesso público controlado usa modo anônimo somente como visualização, com permissões limitadas ao papel de leitura. Para demonstrações, o dashboard pode ser publicado em modo kiosk e com refresh periódico, sem liberar edição nem acesso administrativo.

## 7. Contrato de dados

O firmware envia um payload enxuto com três campos essenciais:

```json
{ "t": 25.57, "i": 1421, "d": "marica_x" }
```

| Chave | Tipo    | Descrição                                                        |
| ----- | ------- | ---------------------------------------------------------------- |
| t     | float   | Temperatura em Celsius                                           |
| i     | integer | Idade do dado em segundos (importante para reconstrução offline) |
| d     | string  | Device ID do dispositivo sensor                                  |

As chaves curtas (`t`, `i`, `d`) em vez de nomes descritivos têm o objetivo de prevenir o estouro de buffer do comando AT+CIPSEND durante a transmissão, mantendo o payload compacto mesmo em condições de conectividade limitada.

A chave de autenticação (`X-API-Key`) é consumida de forma modular através do `secrets.h`, sendo injetada no cabeçalho HTTP via cofre de segredos Infisical.

Esse formato é suficiente para reconstruir o momento real da leitura, mesmo que ela tenha sido capturada offline.

## 8. Resultado de engenharia

O resultado da Fase 4 é um sistema mais previsível, mais controlável e mais barato de operar. A borda faz o trabalho pesado, a rede carrega apenas o necessário, o armazenamento é apropriado para séries temporais e a visualização fica leve para operação e apresentação.
