# ESP Dashboard

Firmware modular para displays inteligentes baseados em ESP32, com carrossel de mini-apps, LVGL, LovyanGFX, touch resistivo, configuração por painel web e logs remotos.

## Estado atual

O alvo de produção validado é o **ESP32-2432S028R / Cheap Yellow Display (CYD)**:

- ESP32 clássico, 240 MHz, 4 MB flash;
- display ILI9341 320×240 em paisagem;
- touch resistivo XPT2046;
- LVGL 8 + LovyanGFX + DMA;
- Wi‑Fi 2,4 GHz;
- carrossel de mini-apps;
- painel web local para Wi‑Fi, cards, variáveis e diagnóstico.

O **ESP32-S3 N16R8 com tela JC3248W535EN** tem um perfil de compilação experimental:

- display AXS15231B 320×480 em QSPI, usado pelo firmware em 480×320 paisagem;
- touch capacitivo AXS15231B em I²C;
- 16 MB flash com PSRAM OPI;
- Arduino_GFX 1.4.9 no driver dessa variante, mantendo LovyanGFX no CYD.

Esse perfil já passa na compilação e teve o firmware universal gravado no hardware
conectado. Wi‑Fi, painel HTTP, criação de card e ingestão de dados foram
validados; a validação visual completa de display, touch e cartão SD continua
separada da validação do contrato universal.

| Perfil | Hardware | Estado | Comando |
| --- | --- | --- | --- |
| `esp32-cyd` | ESP32-2432S028R / ILI9341 + XPT2046 | Validado | `pio run -e esp32-cyd` |
| `esp32-s3-jc3248w535` | ESP32-S3 N16R8 / AXS15231B QSPI + I²C | Compilado e API validada no hardware | `pio run -e esp32-s3-jc3248w535` |

## Painel web

Depois que o dispositivo conectar ao Wi‑Fi, abra no navegador o endereço local
anunciado pelo ESP (ou o IP, caso a rede não resolva mDNS):

```text
http://desk-assistant-XXXXXXXX.local/
http://IP_DO_ESP32/
```

O painel permite:

- escanear e trocar a rede Wi‑Fi;
- ativar/desativar e ordenar cards;
- excluir e restaurar cards sem apagar o manifesto da configuração;
- exportar/importar a configuração dos cards em JSON;
- salvar variáveis persistentes, inclusive segredos;
- consultar logs e o schema da API para agentes.

Para editar e visualizar cards em uma interface maior, use o [Card Studio](web/card-studio.html):

```bash
python3 -m http.server 4173 --directory web
```

Abra `http://127.0.0.1:4173/card-studio.html`. Ele simula a tela em 320×240,
permite testar o carrossel e exporta o mesmo JSON validado pelo firmware.
Consulte [docs/product-architecture.md](docs/product-architecture.md) para a
arquitetura de BLE, OTA, IA, ações no computador e e-ink.

Para executar o LVGL real no computador, consulte [simulator/README.md](simulator/README.md).
O simulador usa SDL2, aceita o JSON exportado pelo Card Studio e mantém a
mesma resolução lógica do firmware.

Para preparar rascunhos por IA sem colocar credenciais no ESP, consulte
[tools/companion/README.md](tools/companion/README.md). O companion devolve um
manifesto validado para o preview; publicar no dispositivo continua sendo uma
ação explícita do usuário.

Endpoints principais:

```text
GET  /                         painel
GET  /api/status               status, IP e hostname local
GET  /api/config               configuração dos cards
POST /api/config               salva cards e reinicia
GET  /api/settings             comportamento do carrossel
POST /api/settings             salva deslize automático e intervalo
POST /api/data                 atualiza valores runtime em RAM
GET  /api/variables            lista somente metadados
POST /api/variables            cria ou atualiza uma variável
POST /api/wifi/scan            inicia scan
GET  /api/wifi/scan            consulta o scan
POST /api/wifi                 solicita conexão
GET  /api/schema               contrato para agentes
GET  /api/provisioning         estado do onboarding BLE/SoftAP
GET  /api/firmware             versão e política do OTA
GET  /api/events               evento de ação pendente (S3)
POST /api/events/ack           confirma entrega do evento
GET  /logs                     log circular do dispositivo
```

Exemplo de variável:

```bash
curl -X POST http://IP_DO_ESP32/api/variables \
  -H 'Content-Type: application/json' \
  -d '{"name":"GEMINI_API_KEY","value":"valor-secreto","secret":true}'
```

Referências de variável usam o formato `{{NOME_DA_VARIAVEL}}`. Os valores não são retornados pelos endpoints de leitura.

> O servidor é HTTP local e ainda não possui autenticação. Não faça port-forward da porta 80 nem exponha o dispositivo diretamente à internet.

No primeiro boot sem credenciais, o ESP32-S3 inicia o onboarding BLE usando o
provisionamento seguro do ESP-IDF. O nome do serviço e o PoP aparecem no log
serial (e o QR é impresso pelo `WiFiProv`). É necessário usar um aplicativo
compatível com `WiFiProv`: BLE não abre uma página web automaticamente. Depois
que a rede é configurada, o firmware reutiliza as credenciais persistidas e
anuncia o portal por mDNS como `desk-assistant-XXXXXXXX.local`. O CYD usa um AP
local leve chamado `PROV_XXXXXXXX`, com a mesma sequência como senha, para não
estourar sua RAM.

O OTA atual é somente para desenvolvimento na rede local via ArduinoOTA. O
endpoint de firmware deixa explícito quando não há autenticação e que ainda
faltam assinatura, partições A/B, health check e rollback para produção.

## Agente de uso no Mac

O repositório inclui um agente leve em [tools/usage_agent/README.md](tools/usage_agent/README.md). Ele roda como tarefa de fundo do macOS, faz somente conexões de saída e envia dados normalizados ao ESP. O Mac não precisa servir páginas nem aceitar conexões externas.

```bash
./tools/usage_agent/install_macos.sh http://IP_DO_ESP32
```

As credenciais dos provedores permanecem no Keychain ou no ambiente do Mac. O ESP recebe apenas percentuais, tokens, requisições e status. O mesmo agente poderá ser movido para uma VPS depois.

## Compilação e gravação

Pré-requisitos: VS Code + PlatformIO ou PlatformIO CLI.

```bash
pio run
pio run -t upload --upload-port /dev/cu.usbserial-XXXX
pio device monitor --port /dev/cu.usbserial-XXXX --baud 115200
```

As credenciais iniciais ficam em `src/config.h`. Deixe os valores de template e faça a configuração pelo display ou pelo painel web.

## Arquitetura

```text
LovyanGFX + XPT2046 (CYD)
          ↓
       LVGL 8
          ↓
     AppManager  ← configuração de cards em Preferences
          ↑
 NetworkManager ← WebServer / Wi‑Fi / OTA / logs
          ↑
 VariableStore  ← variáveis persistentes e referências {{VAR}}
```

O servidor web é mantido leve e as operações de rádio são agendadas fora do callback HTTP para não interromper o loop do LVGL. A configuração é validada e limitada antes de ser persistida.

As configurações do carrossel ficam separadas das definições dos cards:
`GET /api/settings` retorna `autoSlide` e `intervalSeconds`, enquanto `POST
/api/settings` aceita intervalos entre 5 e 3600 segundos. O avanço automático
fica desativado por padrão e é suspenso enquanto o painel de Wi-Fi está aberto.

Para o catálogo de tipos, limites, contrato JSON e exemplos de agentes, leia
[docs/universal-card-system.md](docs/universal-card-system.md). A primeira
fatia declarativa aceita `text`, `metric`, `progress`, `status`, `clock`,
`list` e `chart`; cards nativos continuam disponíveis para comportamento
complexo.

## Estrutura

```text
src/
├── apps/       mini-apps do carrossel
├── core/       ciclo de vida, rede, logs e variáveis
└── display/    LovyanGFX, LVGL e touch XPT2046
```

Na variante JC3248W535EN, a camada de display troca apenas o backend físico por Arduino_GFX + AXS15231B/QSPI, e o touch usa o driver I²C AXS15231B. A camada LVGL, o carrossel, a rede, os logs e as variáveis permanecem compartilhados.

## Roadmap

- autenticação/token para o painel web;
- imagens e temas via LittleFS com limites de tamanho;
- suporte ao cartão SD da variante S3;
- OTA de produção com imagem assinada, rollback e migração de schema;
- biblioteca comunitária remota com assinatura e compatibilidade por placa;
- aplicativo/interface Web Bluetooth customizada para o portal de provisionamento;
- perfil de renderização e-ink com política de refresh.

## Comunidade

Este é um projeto open source. Consulte:

- [CONTRIBUTING.md](CONTRIBUTING.md) para contribuir;
- [cards/README.md](cards/README.md) para criar e catalogar cards;
- [SECURITY.md](SECURITY.md) para reportar vulnerabilidades;
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) para as regras da comunidade.

## Licença

Distribuído sob a licença [MIT](LICENSE).
