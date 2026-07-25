# Changelog

As mudanças relevantes do CYDify serão registradas neste arquivo.

## [0.10.0] - 2026-07-25

### Adicionado

- Painel web hospedado no próprio CYD.
- Diagnóstico do dispositivo e ajuste persistente de brilho.
- Configuração de Wi-Fi e endereço do servidor pelo navegador.
- Reinicialização remota.
- Atualização de firmware OTA pelo navegador.
- Tabela de partições com dois slots de aplicação para atualização segura.
- Endpoint local `/api/device`.

### Mantido

- Player Spotify com capa, progresso, volume, like e controles touch.
- Letras sincronizadas via LRCLIB.
- Fundo desfocado e tema dinâmico derivados da capa.
- Equalizador procedural.
- Páginas Home, Lyrics, Music Info, Statistics e Settings.
- Relógio NTP, modo descanso e estatísticas persistentes.

### Hardware validado

- ESP32-2432S028 sem PSRAM.
- Display ST7789 em 320 × 240, rotação 3.
- Touch XPT2046 calibrado.
- `TFT_INVERTED = false`.
