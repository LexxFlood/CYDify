# Como contribuir

Contribuições são bem-vindas. Antes de propor uma alteração:

1. abra uma issue descrevendo o problema ou melhoria;
2. crie uma branch curta e específica;
3. mantenha firmware, servidor e documentação consistentes;
4. execute os testes do servidor;
5. nunca inclua `.env`, tokens Spotify, senhas, bancos ou caches;
6. descreva no pull request o hardware e a versão testados.

## Testes do servidor

Na pasta `server`, com o ambiente virtual ativo:

```powershell
python -m unittest discover -s tests -v
```

## Testes de hardware

Os sketches de diagnóstico ficam em `tests/`. Mudanças de display ou touch
devem preservar a configuração validada em `firmware/CYDify/HardwareConfig.h`.

## Commits

Prefira mensagens no padrão:

```text
feat(scope): descrição curta
fix(scope): descrição curta
docs: descrição curta
```
