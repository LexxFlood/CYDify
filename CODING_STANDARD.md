# Padrão de código

## Firmware

- C++ para Arduino, com classes em `PascalCase`.
- Métodos e variáveis em `camelCase`.
- Constantes em `UPPER_SNAKE_CASE` ou no namespace de configuração existente.
- Nenhum `delay()` em fluxos interativos ou tarefas de rede.
- A UI não acessa diretamente a internet.
- Alterações no LVGL devem ocorrer no contexto protegido da interface.
- Cores e dimensões compartilhadas devem permanecer centralizadas.
- Preserve `TFT_ROTATION = 3`, `TFT_INVERTED = false` e
  `LVGL_BUFFER_ROWS = 4` para o hardware validado.

## Servidor

- Python com responsabilidades separadas por módulo.
- Configuração sensível somente por variáveis de ambiente.
- Endpoints retornam erros explícitos e não expõem credenciais.
- Respostas HTTP 429 devem respeitar `Retry-After` e os caches existentes.

## Documentação

- Exemplos não devem conter dados pessoais, IPs reais, tokens ou senhas.
- Toda funcionalidade visível ao usuário deve atualizar o README ou o changelog.
