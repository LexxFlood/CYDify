# Dashboard e navegação — v0.8.0

## Páginas

O player continua sendo a página inicial.

```text
Home  →  Music Info  →  Estatísticas  →  Settings
      swipe esquerdo
```

Use swipe para a direita para retornar. Os gestos devem começar fora do slider
de brilho para evitar troca de página durante o ajuste.

## Music Info

Exibe os dados já incluídos na resposta do player:

- título, artista e álbum;
- data de lançamento;
- disco e número da faixa;
- duração;
- popularidade Spotify;
- nome e tipo do dispositivo ativo;
- link da faixa.

Nenhuma chamada adicional ao Spotify é feita para abrir essa página.

## Settings

O slider controla o backlight por PWM entre 5% e 100%. O valor é gravado apenas
quando o usuário solta o controle, evitando gravações repetidas na flash.

A página também mostra:

- versão do firmware;
- heap livre;
- disponibilidade de PSRAM;
- tempo configurado para o modo descanso;
- botão para abrir o portal Wi-Fi.

## Modo descanso

Depois de 90 segundos sem reprodução, o CYDify abre uma tela escura com hora,
data e última faixa conhecida. Toque na tela para retornar.

Quando o Spotify começa a reproduzir novamente, o player reaparece
automaticamente. A hora é sincronizada por NTP quando o Wi-Fi conecta e usa o
fuso UTC-3.
