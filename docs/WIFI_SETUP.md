# Configuração Wi-Fi — v0.3.0

## Primeiro uso

Quando não existem credenciais salvas, o CYDify cria automaticamente a rede:

```text
CYDify-Setup
```

1. Conecte o celular ou computador nessa rede.
2. Se o portal não abrir sozinho, acesse `http://192.168.4.1`.
3. Escolha a rede Wi-Fi da casa e informe a senha.
4. Informe o endereço exibido pelo CYDify Server, por exemplo
`http://192.168.1.100:8000`.
5. Toque em **Salvar e conectar**.
6. O CYDify reiniciará e mostrará seu endereço IP no botão Wi-Fi.

O ESP32-2432S028 trabalha apenas com redes Wi-Fi de 2,4 GHz. A rede de
configuração é aberta durante esta fase; não informe outras senhas no portal.

## Reabrir o portal

Toque no indicador Wi-Fi no canto superior direito da tela. A rede
`CYDify-Setup` será aberta novamente sem apagar imediatamente a configuração
anterior. Salvar outra rede substitui as credenciais armazenadas.

Quando o CYD já estiver conectado, também é possível abrir diretamente no
navegador o IP verde mostrado na tela. O painel web permite alterar a rede e o
servidor sem ativar primeiro o ponto de acesso.

## Persistência

SSID e senha são armazenados no `Preferences` do ESP32, no namespace
`cydify`. Eles permanecem gravados depois de reiniciar ou desligar a placa e
não fazem parte do código-fonte.

O endereço do servidor é salvo na chave `server_url`. Ao reabrir o portal, a
senha pode ficar vazia quando a rede selecionada continua sendo a mesma.

## Diagnóstico

Abra o Monitor Serial em 115200 baud. As mensagens informam:

- tentativa de conexão;
- IP recebido quando a conexão funciona;
- abertura automática do portal quando a conexão falha;
- endereço do portal de configuração.
