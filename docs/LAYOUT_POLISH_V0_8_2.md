# CYDify v0.8.2 - ajuste de layout

## Modo descanso

- Todos os textos foram centralizados na área útil da tela.
- O aviso para tocar e voltar foi afastado da borda inferior.
- O relógio usa agora uma fonte de 32 px renderizada diretamente.
- Foi removida a ampliação artificial da fonte de 14 px, que deixava os
  números granulados.

## Music Info

- O endereço completo da música no Spotify foi retirado da tela.
- Título, artista, álbum, dispositivo e métricas foram centralizados.
- Lançamento, duração, faixa e popularidade agora usam uma grade de duas
  colunas e duas linhas, evitando sobreposição.

## Hardware

Permanecem confirmados:

```cpp
TFT_ROTATION = 3;
TFT_INVERTED = false;
```
