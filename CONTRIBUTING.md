# Contribuindo

Obrigado por contribuir com o ESP Dashboard.

## Fluxo

1. Faça um fork do repositório.
2. Crie uma branch com uma mudança específica.
3. Não inclua credenciais, tokens, dumps ou arquivos `.pio`.
4. Explique no pull request o hardware testado e o comportamento observado.
5. Mantenha o diff focado e atualize a documentação quando a API mudar.

## Validação local

```bash
pio run
pio check --skip-packages
```

Para mudanças de hardware, informe placa, display, touch, mapa de pinos e porta de upload usados no teste.

## Cards e variáveis

Mudanças no formato de cards ou variáveis devem atualizar `/api/schema`, o README e exemplos de configuração. Valores secretos devem ser usados somente em ambiente local e nunca entrar no Git.
