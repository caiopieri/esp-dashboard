# Biblioteca de cards

Esta pasta é o catálogo público dos cards disponíveis no ESP Dashboard.
Cada card possui um manifesto `card.json` com o contrato que agentes e
contribuidores precisam conhecer: ID, variáveis, provedores e placas
compatíveis.

## Cards atuais

- `gemini-usage`: uso de sessão e janela de 7 dias do Gemini;
- `chatgpt-usage`: uso de sessão e janela de 7 dias do ChatGPT/OpenAI;
- `claude-usage`: uso de sessão e janela de 7 dias do Claude;
- `clock-system`: relógio e informações básicas do dispositivo.

Além dos built-ins, o painel pode criar cards declarativos com os tipos
`text`, `metric`, `progress`, `status`, `clock`, `list` e `chart`. Eles são
definidos por JSON validado, usam valor fixo, variável persistente ou snapshot
runtime enviado por `/api/data`, e não exigem recompilação.

Os cards acima são `built-in`: o código deles é compilado no firmware em
`src/apps/`. O painel web pode ativar, desativar, ordenar, excluir e restaurar
esses cards, mas não executa código enviado por JSON. Isso mantém o dispositivo
seguro e previsível.

## Criando um card para a comunidade

1. Crie uma pasta com um ID em minúsculas e hífens.
2. Adicione um `card.json` seguindo os manifestos existentes.
3. Implemente o card em `src/apps/` usando a interface `App`.
4. Registre-o em `src/main.cpp` e atualize `cards/catalog.json`.
5. Compile os perfis `esp32-cyd` e `esp32-s3-jc3248w535` antes de abrir o PR.

Um manifesto descreve o card; ele não é um plugin executável. Apps nativos só
aceitam IDs compilados e registrados, enquanto cards declarativos passam pelo
renderer universal com limites de tamanho e de memória.
