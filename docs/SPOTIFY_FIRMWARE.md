# Integração do firmware com o Spotify — v0.9.0

## Pré-requisitos

- CYD e computador na mesma rede Wi-Fi de 2,4 GHz;
- CYDify Server aberto e autenticado;
- `/health` do servidor acessível pelo celular;
- ArduinoJson 7.4.3 instalado na Arduino IDE.

## Configurar o endereço

Depois de gravar o firmware, toque no botão Wi-Fi do CYD. Conecte o celular à
rede `CYDify-Setup` e abra `http://192.168.4.1`.

No campo **Endereço do CYDify Server**, informe somente a origem, sem endpoint
e sem barra final:

```text
http://192.168.1.100:8000
```

O firmware acrescenta `/api/player` e os endpoints das capas automaticamente.

## Funcionamento

- O estado real é atualizado a cada dois segundos.
- Entre sincronizações, a barra progride localmente a cada segundo.
- Play, pause, próxima, anterior e LIKE são enviados ao servidor por HTTP.
- Consultas do player, controles, capas e letras executam no Core 0.
- Somente o `update()` no Core 1 publica resultados nos objetos LVGL.
- A capa é baixada somente quando o identificador da faixa muda.
- O servidor redimensiona a imagem para 100 × 100, converte para RGB565 e
  mantém uma cópia em cache.
- O servidor também recorta, desfoca e escurece a capa em 64 × 48. O firmware
  baixa esses 6.144 bytes no Core 0 e o LVGL amplia o fundo para 320 × 240.
- Se o fundo estiver indisponível, a interface continua usando o gradiente
  dinâmico anterior; capa, controles e letras permanecem funcionais.
- O firmware baixa a letra completa uma vez em uma tarefa no Core 0 e mantém a
  interface LVGL livre no Core 1.
- Ao abrir `LYRICS`, cinco linhas próximas ao tempo atual são montadas
  localmente; a linha sincronizada fica destacada a cada 100 ms.
- A busca na LRCLIB ocorre em segundo plano e o resultado fica em cache.
- Um swipe para a esquerda abre `Music Info`; outro abre `Settings`.
- A tela Music Info mostra lançamento, disco/faixa, duração, popularidade,
  dispositivo ativo e link do Spotify.
- O brilho é controlado por PWM e salvo em `Preferences`.
- Após 90 segundos sem reprodução, o modo descanso mostra relógio e data.
- Quando a reprodução retorna, o CYDify volta automaticamente ao player.

## Fluxo de comunicação

```text
Spotify → CYDify Server no notebook → Wi-Fi local → CYD
```

O notebook deve permanecer ligado com `start_server.bat` aberto. CYD e notebook
devem estar na mesma rede. Não é necessário conectar um cabo entre eles.

## Indicadores

| Texto | Significado |
|---|---|
| `SPOTIFY ONLINE` | Dados recebidos corretamente |
| `SEM REPRODUCAO` | Servidor acessível, mas nada tocando |
| `CONFIGURE SERVER` | Endereço ainda não salvo no portal |
| `SERVER OFFLINE` | Computador, servidor ou Firewall indisponível |
| `ERRO JSON` | Resposta recebida, mas não pôde ser interpretada |
