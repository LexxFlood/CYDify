# CYDify v0.9.0 - estatísticas de reprodução

## O que foi adicionado

O servidor registra localmente:

- tempo ouvido hoje;
- tempo ouvido na semana;
- quantidade de reproduções do dia;
- artistas e álbuns diferentes;
- artista mais ouvido;
- música mais ouvida.

Os dados ficam em `server/data/statistics.db`, usando SQLite. Eles permanecem
salvos quando o servidor ou o computador são reiniciados.

## Comunicação eficiente

As estatísticas são incluídas na resposta já existente de `/api/player`.
O CYD não cria outra requisição nem outra tarefa de rede para atualizar a página.

O endpoint abaixo também permite consultar os dados no navegador:

```text
http://IP_DO_SERVIDOR:8000/api/statistics
```

## Navegação

```text
Home -> Music Info -> Estatísticas -> Settings
```

Deslize para a esquerda para avançar e para a direita para retornar.

## Privacidade

O histórico permanece somente no computador que executa o CYDify Server. O
banco de estatísticas não deve ser incluído em pacotes públicos ou commits.
