# Perfil de hardware confirmado

Os valores deste documento vieram dos testes executados na unidade física.

## Display

| Campo | Valor |
|---|---|
| Controlador | ST7789 |
| Resolução física | 240 x 320 |
| Resolução utilizada | 320 x 240 |
| Rotação | 3 (orientação final escolhida) |
| Inversão | `false` |
| PSRAM | Ausente |

### Pinos do display

| Sinal | GPIO |
|---|---:|
| MOSI | 13 |
| MISO | 12 |
| SCLK | 14 |
| CS | 15 |
| DC | 2 |
| RESET | -1 (não conectado) |
| Backlight | 21 |

## Touch

| Campo | Valor |
|---|---|
| Controlador | XPT2046 |
| Barramento | SPI separado |

### Pinos do touch

| Sinal | GPIO |
|---|---:|
| CLK | 25 |
| MISO | 39 |
| MOSI | 32 |
| CS | 33 |
| IRQ | 36 |

## Calibração medida

Pontos brutos registrados:

```text
Superior esquerdo: X=3550 Y=3491 Z=1540
Superior direito:  X=470  Y=3529 Z=2462
Inferior direito:  X=473  Y=697  Z=2163
Inferior esquerdo: X=3563 Y=567  Z=831
```

Limites extrapolados utilizados pelo firmware:

```cpp
RAW_X_LEFT   = 3780;
RAW_X_RIGHT  = 250;
RAW_Y_TOP    = 3800;
RAW_Y_BOTTOM = 340;
```

Os eixos X e Y do touch são invertidos em relação à tela. Como a rotação final
foi alterada de 1 para 3, a imagem girou 180 graus e os dois eixos do touch
também precisam ser invertidos. A conversão dependente da rotação está
implementada em `DisplayPort.cpp`.

## Observações descobertas nos testes

- O controlador inicialmente presumido como ILI9341 atualizava apenas parte da
  tela e deixava uma faixa de ruído. O ST7789 preencheu toda a área.
- A opção PSRAM deve permanecer desativada.
- O painel desta unidade exige `TFT_INVERTED = false` para as cores corretas.
  Este valor foi confirmado no hardware e deve permanecer assim.
- Blur e tratamento pesado das capas serão feitos pelo servidor, não pelo ESP32.
