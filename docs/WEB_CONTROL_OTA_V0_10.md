# Painel web e OTA — v0.10.0

## O que esta versão adiciona

Ao acessar o IP verde mostrado na tela do CYDify, o navegador abre um painel
local com:

- estado do Wi-Fi e IP do CYD;
- memória livre e tempo ligado;
- endereço atual do CYDify Server;
- controle de brilho sincronizado com a tela Settings;
- alteração de Wi-Fi e servidor;
- reinicialização;
- atualização do firmware pelo navegador;
- diagnóstico JSON em `/api/device`.

Exemplo:

```text
http://192.168.1.50/
```

O painel funciona dentro da rede local e não depende da internet.

## Primeira instalação obrigatoriamente por USB

A versão `v0.10.0` altera a tabela da memória flash para criar duas partições de
firmware. Por isso, grave esta versão normalmente pela Arduino IDE e pelo cabo
USB.

Use:

```text
Placa: ESP32 Dev Module
Flash Size: 4 MB
Partition Scheme: No FS 4MB (2MB APP x2)
PSRAM: Disabled
```

As preferências de Wi-Fi, servidor e brilho permanecem no NVS.

## Atualizações seguintes pelo navegador

1. Abra a versão nova na Arduino IDE.
2. Use **Sketch > Exportar binário compilado**.
3. Localize o arquivo `CYDify.ino.bin` dentro da pasta do sketch.
4. Abra o IP do CYDify no navegador.
5. Em **Atualização OTA**, selecione `CYDify.ino.bin`.
6. Toque em **Instalar firmware**.
7. Mantenha o CYD ligado até o navegador confirmar a conclusão.

Envie somente `CYDify.ino.bin`. Não envie arquivos `merged.bin`,
`bootloader.bin` ou `partitions.bin` no formulário OTA.

Se o arquivo não for válido ou não couber na partição, o firmware atual
permanece ativo.

## Diagnóstico

O endpoint abaixo retorna dados básicos em JSON:

```text
http://IP_DO_CYD/api/device
```

Ele informa versão, Wi-Fi, IP, heap livre, tempo ligado, brilho e endereço do
servidor.

## Segurança

O painel não é publicado na internet, mas qualquer dispositivo conectado à
mesma rede local pode acessá-lo. Não redirecione a porta 80 do CYD no roteador
e não exponha esse endereço publicamente.
