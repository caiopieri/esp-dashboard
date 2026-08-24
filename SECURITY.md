# Segurança

## Reportando vulnerabilidades

Não publique credenciais, chaves ou detalhes exploráveis em uma issue pública. Use uma Security Advisory privada do GitHub para reportar vulnerabilidades.

Inclua:

- versão ou commit afetado;
- placa e ambiente de teste;
- passos para reproduzir;
- impacto esperado e observado;
- logs sanitizados, sem senhas ou tokens.

## Considerações atuais

- O painel web é HTTP sem autenticação e deve ser usado somente na rede local.
- Não faça port-forward da porta 80.
- Variáveis marcadas como secretas não são retornadas pela API de leitura, mas ficam persistidas no dispositivo.
- Nunca faça commit de credenciais Wi‑Fi, API keys ou dados pessoais.
