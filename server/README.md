# CYDify Server v0.6.0

Backend local que mantém o Client Secret fora do ESP32, realiza OAuth com o
Spotify, renova tokens automaticamente e oferece uma API simples ao CYD.

## Requisitos

- Windows 10 ou 11;
- Python 3.12 ou mais recente;
- computador e CYD conectados à mesma rede;
- conta Spotify Premium para o proprietário de um aplicativo em Development
  Mode, conforme as regras do Spotify vigentes em 2026.

## 1. Spotify Developer Dashboard

No aplicativo criado no Spotify, registre exatamente:

```text
http://127.0.0.1:8000/callback
```

Não use `localhost`. Copie o Client ID e o Client Secret, sem publicá-los ou
gravá-los no firmware.

## 2. Instalação

Execute `setup_server.bat`. O script cria `.venv`, instala as dependências e
gera `.env` a partir do exemplo.

Abra `.env` em um editor e substitua:

```dotenv
SPOTIFY_CLIENT_ID=cole_seu_client_id_aqui
SPOTIFY_CLIENT_SECRET=cole_seu_client_secret_aqui
```

Mantenha o Redirect URI sem alterações:

```dotenv
SPOTIFY_REDIRECT_URI=http://127.0.0.1:8000/callback
```

## 3. Primeiro login

1. Execute `start_server.bat` e mantenha a janela aberta.
2. Abra `http://127.0.0.1:8000` no navegador.
3. Clique em **Conectar Spotify**.
4. Autorize o aplicativo.
5. Coloque uma música para tocar.
6. Abra `http://127.0.0.1:8000/api/player`.

O arquivo `data/spotify_token.json` será criado localmente. Ele está ignorado
pelo Git e não deve ser compartilhado.

## Endpoints da primeira entrega

| Método | Endpoint | Função |
|---|---|---|
| GET | `/health` | Diagnóstico do servidor |
| GET | `/auth/login` | Inicia o login Spotify |
| GET | `/callback` | Conclui o OAuth |
| GET | `/api/player` | Estado completo do player |
| GET | `/api/statistics` | Estatísticas locais de hoje e da semana |
| GET | `/api/cover/{track_id}.rgb565` | Capa 100 × 100 pronta para o CYD |
| GET | `/api/background/{track_id}.rgb565` | Fundo blur 64 × 48 pronto para ampliação |
| GET | `/api/lyrics/{track_id}?progress_ms=...` | Janela de letra sincronizada |
| GET | `/api/lyrics/{track_id}/full` | Letra completa para sincronização local |
| POST | `/api/player/play` | Reproduzir |
| POST | `/api/player/pause` | Pausar |
| POST | `/api/player/next` | Próxima faixa |
| POST | `/api/player/previous` | Faixa anterior |
| POST | `/api/player/liked/set` | Adicionar/remover da biblioteca |

A documentação interativa fica em `http://127.0.0.1:8000/docs`.

As estatísticas são registradas automaticamente enquanto o CYD consulta o
player. O banco `data/statistics.db` permanece somente neste computador e pode
ser consultado por `GET /api/statistics`.

## Observações

- O servidor escuta em `0.0.0.0:8000` para permitir acesso pelo CYD.
- A página prioriza o IP privado do Wi-Fi e ignora endereços do Radmin VPN.
- Talvez o Windows solicite autorização no Firewall. Autorize apenas em redes
  privadas.
- O servidor não transmite áudio; ele consulta metadados e controla dispositivos
  Spotify já ativos.
- As capas convertidas ficam em `data/covers` e são reutilizadas nas próximas
  reproduções da mesma faixa.
- Os fundos desfocados também ficam no cache. O servidor realiza o blur e envia
  somente 6.144 bytes ao CYD, preservando a RAM da placa sem PSRAM.
- O cabeçalho `X-CYDify-Dominant` acompanha cada capa com a cor dinâmica em RGB
  hexadecimal.
- O estado da faixa inclui lançamento, disco/faixa, popularidade, dispositivo e
  link do Spotify para alimentar a página Music Info.
- Alterações de LIKE são enfileiradas e repetidas após o `Retry-After` quando o
  Spotify responde HTTP 429.
- Letras são obtidas da LRCLIB sem chave de API e armazenadas em `data/lyrics`.
- Esta versão é destinada à rede local. Não exponha a porta 8000 na internet.
