# Histórico dos testes

## 1. Hipótese ILI9341

O primeiro sketch utilizou `Adafruit_ILI9341`. A comunicação SPI funcionou,
mas somente parte da tela foi atualizada e surgiu uma faixa de ruído. Isso
demonstrou que o painel não era compatível com a inicialização ILI9341.

## 2. Confirmação ST7789

O teste seguinte utilizou `Adafruit_ST7789`, inicialização física 240 x 320 e
rotação 1. A tela passou a ocupar toda a área. A inversão de cores foi corrigida
na unidade final com:

```cpp
display.invertDisplay(false); // valor final confirmado posteriormente
```

## 3. Touch XPT2046

O touch respondeu pelo segundo barramento SPI. Os quatro pontos medidos estão
registrados em `HARDWARE.md`; deles foram extrapolados os limites utilizados no
firmware.

## 4. LVGL 9.5

Foi criado `lv_conf.h` com RGB565 e fonte Montserrat 14. O teste final exibiu
uma tela LVGL e um botão cujo contador respondeu aos toques.

## Resultado

```text
Display ST7789: OK
Resolução 320 x 240: OK
Cores/inversão: OK
Touch XPT2046: OK
Calibração: OK
LVGL 9.5: OK
PSRAM: ausente
```
# v0.8.0-navigation-dashboard

- Navegação Home → Music Info → Settings por gestos horizontais.
- Metadados adicionais do Spotify consolidados no mesmo `/api/player`.
- Controle de brilho PWM de 5% a 100%, persistido em NVS.
- Relógio sincronizado por NTP e modo descanso após 90 segundos.
- Retorno automático à Home quando a reprodução recomeça.
- Tela Settings com versão, heap livre e estado de PSRAM.
- Indicador temporário durante o carregamento de capa e fundo.
- Barra de progresso adicionada à tela de letras.
- Nova camada de navegação validada contra os cabeçalhos da LVGL 9.5.

# v0.7.2-visual-blur

- Fundo da capa recortado, desfocado e escurecido no servidor.
- Miniatura RGB565 de 64 × 48 (6.144 bytes), adequada ao ESP32 sem PSRAM.
- Ampliação independente de 5× no LVGL, atrás de toda a interface.
- Download assíncrono no Core 0 e fallback para o gradiente dinâmico.
- Cache antigo de capas convertido localmente sem novo download do Spotify.

# v0.7.1-visual-fixes

- Nome CYDify, IP e estados de conexão preservados em verde fixo.
- Tema dinâmico limitado aos elementos visuais associados à faixa.
- Alterações de LIKE aplicadas de forma otimista e enfileiradas no servidor.
- Respostas 429 respeitam `Retry-After` e são repetidas sem bloquear o CYD.

# v0.7.0-visual

- Cor dominante calculada pelo servidor a partir da capa RGB565.
- Fundo, progresso, botão principal e tela de letras recebem acento dinâmico.
- Equalizador procedural de sete barras atualizado sem acesso ao áudio.
- Tema mantém contraste de texto e não adiciona buffers gráficos grandes.

# v0.6.3-fully-async

- Consulta do player e controles movidos para tarefa no Core 0.
- Download da capa movido para tarefa no Core 0.
- Callbacks da interface publicados somente pelo `update()` no Core 1.
- Capa anterior liberada antes do novo download para limitar o pico de heap.

# v0.6.2-lyrics-async

- Corrigida a incompatibilidade entre `int` e `int16_t` na seleção da janela de letras com ESP32 Core 3.3.10.
- Mantido o download assíncrono das letras no Core 0 e a sincronização local no Core 1.

# v0.6.1-lyrics-async

- Download da letra movido para tarefa FreeRTOS no Core 0.
- LVGL e touch permanecem no Core 1 sem bloqueio HTTP.
- Letra completa baixada uma única vez por faixa.
- Sincronização local atualizada a cada 100 ms.
- Removidas consultas de janela a cada 1,2 segundo.

# v0.6.0-lyrics

- Integração do servidor com LRCLIB usando assinatura completa da faixa.
- Busca antecipada em segundo plano para não bloquear o display.
- Parser LRC validado para centésimos e milésimos de segundo.
- Cache persistente em `server/data/lyrics`.
- Firmware recebe somente cinco linhas por vez para economizar RAM.
- Linha atual destacada de acordo com o progresso estimado localmente.

# v0.5.1-cover

- Buffer RGB565 movido da DRAM estática para o heap.
- Corrigido `dram0_0_seg overflowed by 18160 bytes` no ESP32 sem PSRAM.

# v0.5.0-cover

- Adicionado `AlbumArtClient` ao firmware.
- Download ocorre apenas quando o ID da faixa muda.
- Buffer RGB565 fixo de 20.000 bytes, adequado ao ESP32 sem PSRAM.
- Servidor v0.2.0 converte capas para 100 × 100 com Pillow.
- Cache persistente de capas em `server/data/covers`.
- Conversão RGB565 validada com saída exata de 20.000 bytes.

# v0.4.1-spotify

- Adicionada tabela de partições própria para flash de 4 MB.
- Duas partições OTA de `0x190000` bytes cada.
- Área SPIFFS de 768 KB e partição de coredump preservadas.
- Corrigido `Sketch too big` com o firmware de 1.319.876 bytes.

# v0.4.0-spotify

- Endereço do CYDify Server adicionado ao portal web e ao Preferences.
- Leitura de `/api/player` a cada dois segundos.
- Nome, artista, álbum, progresso, volume, reprodução e LIKE reais.
- Controles touch conectados aos endpoints do servidor.
- JSON filtrado e lido como stream para reduzir o uso de RAM.

# v0.3.2-setup

- Buffer parcial do LVGL reduzido de 24 para 8 linhas.
- Uso estático de DRAM reduzido em 10.240 bytes.
- Corrigido `dram0_0_seg overflowed by 6312 bytes` após adicionar Wi-Fi.

# Server v0.3.3

- Separados os períodos de espera do player e da consulta `liked`.
- Uma resposta 429 da biblioteca não congela mais faixa, progresso ou letras.
- O JSON passa a indicar quando o próprio endpoint do player estiver em espera.

# Server v0.3.2

- Cache temporário do estado do player com extrapolação local do progresso.
- Cache do estado `liked` para evitar uma chamada extra à biblioteca a cada atualização.
- Respeito automático ao `Retry-After` do Spotify e fallback local durante HTTP 429.

# v0.3.1-setup

- Módulo de rede renomeado para `CydifyNetwork`.
- Corrigida colisão com a classe `NetworkManager` do ESP32 Core 3.3.10.

# v0.3.0-setup

- `TFT_INVERTED` fixado em `false`, conforme validação visual do hardware.
- Portal Wi-Fi criado com componentes nativos do ESP32.
- Credenciais salvas com `Preferences`.
- Modo de configuração automático quando não existe conexão salva.
- Botão Wi-Fi adicionado à interface para reabrir o portal.
