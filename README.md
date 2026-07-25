# CYDify

Dashboard musical para o ESP32-2432S028 (Cheap Yellow Display), com integração
ao Spotify, letras sincronizadas, controles touch, estatísticas, interface web
e atualização OTA.

Esta é a versão de desenvolvimento `v0.10.0-web-control-ota`, validada no
hardware descrito abaixo e pronta para ser aberta na Arduino IDE.

> O CYDify controla um dispositivo Spotify já ativo e exibe seus metadados.
> Ele não transmite o áudio pelo ESP32.

## Hardware confirmado

- Placa: ESP32-2432S028, display de 2,8 polegadas
- Display: ST7789, 240 x 320 físico, utilizado em 320 x 240
- Rotação: 3 (paisagem, orientação confirmada)
- Inversão do painel: desativada (`TFT_INVERTED = false`)
- Touch: XPT2046 em barramento SPI separado
- PSRAM: não disponível
- Flash: 4 MB

O mapeamento completo de pinos e a calibração estão em
[`docs/HARDWARE.md`](docs/HARDWARE.md).

## Conteúdo

```text
CYDify/
├── config/
│   └── lv_conf.h
├── docs/
│   ├── ARDUINO_SETUP.md
│   ├── HARDWARE.md
│   ├── TEST_HISTORY.md
│   └── WIFI_SETUP.md
├── firmware/
│   └── CYDify/
│       ├── CYDify.ino
│       ├── DisplayPort.cpp
│       ├── DisplayPort.h
│       ├── DashboardPages.cpp
│       ├── DashboardPages.h
│       ├── HardwareConfig.h
│       ├── CydifyNetwork.cpp
│       ├── CydifyNetwork.h
│       ├── PlayerClient.cpp
│       ├── PlayerClient.h
│       ├── partitions.csv
│       ├── UserInterface.cpp
│       └── UserInterface.h
├── server/
│   ├── app/
│   ├── data/
│   ├── .env.example
│   ├── requirements.txt
│   ├── setup_server.bat
│   └── start_server.bat
└── tests/
    ├── DisplayTest/
    ├── LvglTouchTest/
    └── TouchCalibration/
```

## Começar

1. Prepare a Arduino IDE seguindo
   [`docs/ARDUINO_SETUP.md`](docs/ARDUINO_SETUP.md).
2. Configure e inicie o backend conforme
   [`server/README.md`](server/README.md).
3. Abra `firmware/CYDify/CYDify.ino` na Arduino IDE.
4. Selecione `ESP32 Dev Module`, PSRAM desativada, 4 MB de flash e o esquema
   `No FS 4MB (2MB APP x2)`.
5. Compile e grave por USB.
6. Conclua o primeiro acesso Wi-Fi pelo portal do CYDify.

O firmware `v0.10.0-web-control-ota` consulta o servidor local, exibe os dados reais da
música, baixa a capa preparada em RGB565 e envia os controles do player. Wi-Fi
e endereço do servidor são configurados pelo navegador. Player, capas e letras
executam suas operações HTTP no Core 0; o LVGL permanece isolado no Core 1. A
cor dominante controla os acentos e o equalizador procedural. O servidor também
prepara um fundo desfocado de 64 × 48, ampliado pelo LVGL sem exigir PSRAM.
O `CYDify Server v0.6.0` implementa OAuth, renovação de token, leitura do player, controles,
estado da biblioteca, cache das capas e letras via LRCLIB. O estado do player e
o indicador de curtida também possuem cache de curta duração, reduzindo chamadas
ao Spotify e respeitando automaticamente respostas `429`.

## Estado

- [x] Display identificado e validado
- [x] Touch identificado e calibrado
- [x] LVGL 9.5 integrado
- [x] Estrutura inicial do firmware
- [x] Player demonstrativo
- [x] Portal Wi-Fi
- [x] Credenciais persistentes com Preferences
- [x] Base do backend FastAPI
- [x] OAuth e refresh token Spotify
- [x] API real do player e controles
- [x] Comunicação do firmware com o servidor
- [x] Download, cache e exibição da capa
- [x] Letras sincronizadas e cache LRCLIB
- [x] Equalizador procedural
- [x] Tema dinâmico com cor dominante da capa
- [x] Fundo dinâmico desfocado, processado e armazenado no servidor
- [x] Navegação por swipe entre Home, Music Info, Estatísticas e Settings
- [x] Informações detalhadas da música e do dispositivo
- [x] Brilho PWM persistente
- [x] Relógio NTP e modo descanso automático
- [x] Estatísticas persistentes de reprodução
- [x] Página de estatísticas de hoje e da semana
- [x] Painel web do dispositivo com diagnóstico e controle de brilho
- [x] Alteração de Wi-Fi e servidor pelo painel web
- [x] Reinicialização pelo navegador
- [x] Atualização de firmware OTA pelo navegador

## Painel web do CYD

Depois que o CYDify estiver conectado, abra no navegador o endereço IP verde
mostrado na tela, por exemplo:

```text
http://192.168.1.50/
```

O painel permite consultar o estado do dispositivo, ajustar o brilho, trocar
Wi-Fi e servidor, reiniciar e instalar futuras versões sem cabo USB. A primeira
gravação desta versão ainda deve ser feita por USB para instalar a nova tabela
de partições OTA. Veja [`docs/WEB_CONTROL_OTA_V0_10.md`](docs/WEB_CONTROL_OTA_V0_10.md).

## Segurança e privacidade

- Nunca publique `server/.env`, tokens OAuth ou senhas Wi-Fi.
- O repositório inclui somente `server/.env.example`, com valores fictícios.
- O servidor foi projetado para uso na rede local; não exponha a porta `8000`
  diretamente à internet.
- Tokens, estatísticas e caches gerados localmente estão excluídos pelo Git.

## Documentação do projeto

- [Arquitetura](ARCHITECTURE.md)
- [Histórico de versões](CHANGELOG.md)
- [Roadmap](ROADMAP.md)
- [Como contribuir](CONTRIBUTING.md)
- [Padrão de código](CODING_STANDARD.md)

## Licença

Distribuído sob a [licença MIT](LICENSE).
