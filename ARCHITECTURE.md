# Arquitetura

O CYDify é dividido em dois componentes.

## Firmware

O firmware Arduino executa no ESP32-2432S028 e é responsável por:

- display ST7789, touch XPT2046 e interface LVGL;
- conexão Wi-Fi e portal de configuração;
- consumo da API local do servidor;
- renderização do player, letras, informações e estatísticas;
- controles touch, brilho, relógio, modo descanso e OTA.

As operações HTTP de player, capa e letra são executadas no Core 0. A interface
LVGL permanece no Core 1 para evitar congelamentos perceptíveis.

## Servidor

O backend FastAPI é executado em um computador da rede local e é responsável
por:

- OAuth e renovação de token do Spotify;
- consulta e controle do player;
- obtenção e cache de letras via LRCLIB;
- conversão de capas para RGB565;
- geração do fundo desfocado e extração da cor dominante;
- armazenamento das estatísticas locais.

O Client Secret do Spotify nunca é gravado no firmware.

## Fluxo de dados

```text
Spotify / LRCLIB
       |
       v
CYDify Server (FastAPI)
       |
       | HTTP na rede local
       v
ESP32 CYD
       |
       v
LVGL + touch + painel web local
```

Consulte `docs/HARDWARE.md` para o mapeamento de pinos e `server/README.md`
para os endpoints disponíveis.
