# Primeira interface — v0.2.0-demo

A primeira interface funcional foi desenhada diretamente para 320 x 240 e
evita imagens grandes ou efeitos pesados, pois a placa não possui PSRAM.

## Elementos

- identificação CYDify e estado Wi-Fi demonstrativo;
- capa simulada por gradiente;
- nome da música, artista e álbum;
- botões LIKE e LYRICS;
- volume fictício;
- barra e tempos de reprodução;
- anterior, play/pause e próxima;
- tela de letra demonstrativa com botão de fechar;
- três faixas fictícias para testar a troca de estado.

## Interações

- Play/Pause interrompe ou retoma o contador.
- Anterior/Próxima troca a faixa e a cor da capa.
- LIKE alterna o estado visual.
- LYRICS abre a segunda tela.
- O progresso avança automaticamente a cada segundo.

Nenhum dado nesta versão vem do Spotify. O objetivo é validar layout, memória,
touch e atualização parcial antes de adicionar rede.

## Correção v0.2.1

A orientação física passou de `TFT_ROTATION = 1` para `3`. Como isso representa
uma rotação de 180 graus, as coordenadas X e Y do XPT2046 também foram
invertidas. Assim, a região sensível permanece exatamente sobre os botões.

## Correção v0.2.2

Cada faixa demonstrativa passou a possuir seu próprio estado LIKE. Trocar de
música e retornar não apaga mais a seleção. O estado permanece em RAM até o
ESP32 reiniciar; futuramente o backend Spotify informará se a faixa está salva
na biblioteca do usuário.

## Evolução v0.3.0

O indicador demonstrativo de Wi-Fi foi substituído por um botão funcional. Ele
mostra `WIFI SETUP`, `WIFI OFFLINE` ou o endereço IP recebido. Um toque abre o
portal `CYDify-Setup`, acessível em `http://192.168.4.1`.
