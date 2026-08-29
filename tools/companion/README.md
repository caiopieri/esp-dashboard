# Companion local

Este processo é o plano de controle do Desk Assistant. Ele deve rodar no
computador do usuário, não no ESP32. Por padrão escuta somente em
`127.0.0.1:8787`.

## Endpoints

```text
GET  /healthz
POST /api/cards/validate  {"card": {...}}
POST /api/cards/package/validate  {"packageVersion": 1, ...}
POST /api/ai/draft        {"prompt": "..."}
GET  /api/actions
POST /api/actions/execute {"id": "open_card_studio", "confirmationToken": "..."}
```

O endpoint de IA usa um provedor compatível com Chat Completions configurado
por ambiente:

```bash
export DESK_AI_ENDPOINT="https://seu-proxy.example/v1/chat/completions"
export DESK_AI_API_KEY="..."
export DESK_AI_MODEL="seu-modelo"
python3 tools/companion/desk_assistant_server.py
```

O retorno é sempre validado contra o contrato declarativo. Sem essas variáveis,
o servidor continua útil para validar cards, mas retorna `503` para geração de
IA. Não coloque a chave da IA no firmware nem no HTML do Card Studio.

O servidor permite CORS para o Card Studio local. Não altere o host para
`0.0.0.0` sem adicionar autenticação e TLS.

## Ações locais

As ações são opt-in e ficam em um arquivo confiável apontado por
`DESK_ACTIONS_FILE`. O card só conhece o ID da ação; ele nunca fornece um
comando ou argumentos. `open_url` aceita somente `http(s)` e `launch` executa
uma lista pré-configurada com `shell=False`. A confirmação é obrigatória por
padrão e usa um token descartável com validade de 30 segundos.

Exemplo:

```bash
export DESK_ACTIONS_FILE="$PWD/tools/companion/actions.example.json"
python3 tools/companion/desk_assistant_server.py
```

No S3, o firmware preserva a metadata `card.action`, mostra o botão e publica
eventos. Com o Card Studio aberto, a ponte touch→companion faz polling,
confirma a ação no navegador e envia o ack ao dispositivo.
