# Guia Didático do Projeto

## Como entender o sistema sem jargão

Pense no projeto como um corpo que sente calor, guarda memória quando fica sem contato com o mundo e depois conta para um painel o que aconteceu.

## 1. O calor encosta no sensor

O sensor NTC funciona como uma pele sensível ao calor. Quando a temperatura sobe ou desce, a resistência elétrica muda. Essa mudança vira um número que o microcontrolador consegue ler.

## 2. O microcontrolador interpreta esse número

O valor lido não é ainda uma temperatura pronta. É como se o sensor dissesse apenas "estou mais apertado" ou "estou mais folgado". O firmware pega essa informação e transforma em graus Celsius.

Isso é importante porque o dispositivo já entrega o dado pronto. Assim, a nuvem não precisa gastar esforço com contas extras.

## 3. Se a internet está funcionando, o dado segue na hora

Quando existe conexão, o sensor manda a temperatura imediatamente para a API. É como mandar uma mensagem pelo celular quando a rede está no ar.

## 4. Se a internet cai, a EEPROM vira um caderno de anotações

Se a rede some, o projeto não perde o que mediu. Em vez disso, ele grava a leitura na EEPROM, que é uma memória que continua guardando informação mesmo sem energia.

Imagine um caderno pequeno que o sensor mantém consigo. Toda vez que o envio falha, ele anota ali a temperatura e o horário interno daquele registro.

## 5. Quando a conexão volta, o caderno é lido e esvaziado

Assim que a internet retorna, o firmware lê as anotações antigas e envia tudo para a nuvem. Não envia só a temperatura; envia também a idade do dado.

Essa idade é o tempo que passou desde a leitura original. Com isso, o servidor consegue colocar cada ponto no instante correto da linha do tempo.

## 6. O servidor organiza o dado no tempo certo

Se a leitura é atual, o ponto entra no presente. Se a leitura ficou guardada durante a queda da rede, o servidor joga esse ponto para o passado correto.

É como reconstruir uma fila de fotos tiradas fora de ordem e reorganizá-las pelo horário real em que aconteceram.

## 7. O banco de dados guarda tudo como histórico

O sistema usa um banco feito para tempo. Ele não pensa apenas em "temperatura agora", mas em "temperatura ao longo dos dias, meses e anos".

Isso permite analisar padrões, comparar estações e verificar eventos antigos sem precisar recomeçar a medição do zero.

## 8. O Grafana mostra a história como gráfico

O Grafana pega os dados guardados e desenha uma linha no gráfico. Quando a temperatura sobe, a linha sobe. Quando cai, a linha desce.

Para alguém da banca, dá para pensar no Grafana como um painel do carro: ele não cria a informação, só mostra de forma clara o que já está acontecendo no sistema.

## 9. O que torna o projeto resiliente

O ponto mais importante é este: mesmo sem internet, o sistema continua trabalhando.

Ele não para de medir, não joga os dados fora e não depende de um serviço externo para lembrar do que aconteceu. Isso dá robustez ao projeto e evita lacunas na telemetria.

## 10. Resumo final em uma frase

O sensor sente o calor, guarda o que mediu se a internet cair, reencontra o tempo correto quando volta a se conectar e mostra tudo em um gráfico fácil de entender.
