# Preparação da Arduino IDE

## Placa

Instale `esp32 by Espressif Systems` e selecione:

```text
Placa: ESP32 Dev Module
Flash Size: 4 MB
PSRAM: Disabled
CPU Frequency: 240 MHz
Partition Scheme: No FS 4MB (2MB APP x2)
```

## Bibliotecas utilizadas

Instale pelo Gerenciador de Bibliotecas:

- Adafruit GFX Library 1.12.6 ou compatível
- Adafruit ST7735 and ST7789 Library 1.11.0 ou compatível
- XPT2046_Touchscreen 1.4
- LVGL 9.5.0
- ArduinoJson 7.4.3

WiFi, WebServer, DNSServer e Preferences já acompanham o pacote
`esp32 by Espressif Systems`; não instale bibliotecas extras para o portal.

Se existirem duas bibliotecas para `XPT2046_Touchscreen.h`, mantenha selecionada
a biblioteca de Paul Stoffregen (`XPT2046_Touchscreen`).

## Configuração do LVGL

Copie:

```text
config/lv_conf.h
```

para:

```text
%USERPROFILE%\Documents\Arduino\libraries\lv_conf.h
```

O arquivo deve ficar ao lado da pasta `lvgl`, e não dentro dela.

## Abrir o firmware

Abra:

```text
firmware\CYDify\CYDify.ino
```

A pasta e o arquivo `.ino` possuem o mesmo nome, conforme exigido pela Arduino
IDE. Na primeira compilação, o LVGL pode demorar vários minutos para formar o
cache.

O arquivo `partitions.csv` do sketch é aplicado automaticamente e reserva duas
partições de aproximadamente 1,94 MB. Isso permite que uma versão continue
funcionando enquanto a próxima é recebida pelo navegador.

Na Arduino IDE 1.8.19, selecione também:

```text
Ferramentas > Partition Scheme > No FS 4MB (2MB APP x2)
```

Essa opção aplica à verificação da IDE o mesmo limite usado pelo CYDify. A
tabela realmente gravada continua sendo o `partitions.csv` que acompanha o
sketch.

Depois de gravar a versão `v0.10.0-web-control-ota`, siga
[`WIFI_SETUP.md`](WIFI_SETUP.md).

## Erro de arquivo temporário no Arduino 1.8.19

Se aparecer:

```text
fatal error: could not close temporary response file
```

feche todas as janelas da Arduino IDE, abra `%TEMP%` pelo Executar do Windows e
apague o arquivo `cc...` citado no erro, além das pastas `arduino_build_*` e
`arduino_cache_*`. Reabra o sketch, confirme novamente `No FS 4MB` e compile
uma única vez. Esse erro pertence ao compilador/Temp do Windows e não ao
firmware.
