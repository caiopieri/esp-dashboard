# Card Studio

`card-studio.html` é o primeiro editor/preview independente do Desk Assistant.
Ele não precisa de Node, npm ou servidor de build:

```bash
python3 -m http.server 4173 --directory web
```

Depois abra <http://127.0.0.1:4173/card-studio.html>.

## O que já funciona

- preview no formato 320×240;
- navegação manual e deslize automático simulado;
- criação, edição, duplicação e remoção de cards;
- tipos declarativos compatíveis com o firmware atual;
- importação e exportação do contrato JSON;
- exportação do card selecionado como pacote versionado para a biblioteca;
- importação de pacotes `.card.json` com verificação de compatibilidade e preview;
- metadata opcional de ação local, sempre referenciada por ID allowlisted;
- polling de eventos do ESP e confirmação explícita antes de executar uma ação;
- leitura e publicação no ESP por `/api/config`.

O firmware atual libera CORS para permitir que o Studio seja servido em outro
endereço durante o desenvolvimento. Como o servidor ainda é HTTP local e sem
autenticação, não exponha o ESP diretamente à internet. A publicação respeita
a validação do firmware e pode provocar o reinício previsto pelo contrato
atual.

Após o provisionamento, prefira abrir o portal pelo hostname mDNS retornado em
`GET /api/status` (por exemplo, `http://desk-assistant-d9059db4.local/`). Se a
rede ou o sistema do usuário não resolver `.local`, use o IP exibido no mesmo
endpoint. O BLE de primeira configuração ainda usa o protocolo padrão
`WiFiProv`; uma página Web Bluetooth própria continua sendo uma extensão do
produto, não uma abertura automática do navegador.

## Próximas extensões

- preview visual de touch e feedback de ações;
- biblioteca remota de cards com pacotes assinados;
- validação/instalação de OTA de produção;
- perfis completos 480×320 e e-ink;
- conexão por BLE para provisionamento inicial (o firmware S3 já anuncia pelo
  `WiFiProv`; o fluxo Web Bluetooth customizado fica para a próxima camada).
