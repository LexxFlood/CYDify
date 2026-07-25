# CYDify v0.8.1 - correção de compilação

O linker da Arduino IDE informou:

```text
region `dram0_0_seg' overflowed by 872 bytes
```

A causa era o consumo de DRAM estática no ESP32 sem PSRAM. O buffer parcial
do LVGL foi reduzido de 8 para 4 linhas, liberando 2.560 bytes. Nenhuma tela
ou função da versão 0.8.0 foi removida.

As configurações validadas neste hardware continuam:

```cpp
TFT_ROTATION = 3;
TFT_INVERTED = false;
```
