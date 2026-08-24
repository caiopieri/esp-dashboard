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

Esse perfil já passa na compilação, mas ainda não foi gravado nem validado no hardware físico. A primeira validação exigirá a placa conectada e um teste específico de display, touch, Wi‑Fi e cartão SD.

| Perfil | Hardware | Estado | Comando |
| --- | --- | --- | --- |
| `esp32-cyd` | ESP32-2432S028R / ILI9341 + XPT2046 | Validado | `pio run -e esp32-cyd` |
| `esp32-s3-jc3248w535` | ESP32-S3 N16R8 / AXS15231B QSPI + I²C | Compilado; aguardando validação física | `pio run -e esp32-s3-jc3248w535` |

## Painel web

Depois que o dispositivo conectar ao Wi‑Fi, abra no navegador:

```text
http://IP_DO_ESP32/
```

O painel permite:

- escanear e trocar a rede Wi‑Fi;
- ativar/desativar e ordenar cards;
- exportar/importar a configuração dos cards em JSON;
- salvar variáveis persistentes, inclusive segredos;
- consultar logs e o schema da API para agentes.

Endpoints principais:

```text
GET  /                         painel
GET  /api/status               status e IP
GET  /api/config               configuração dos cards
POST /api/config               salva cards e reinicia
GET  /api/variables            lista somente metadados
POST /api/variables            cria ou atualiza uma variável
POST /api/wifi/scan            inicia scan
GET  /api/wifi/scan            consulta o scan
POST /api/wifi                 solicita conexão
GET  /api/schema               contrato para agentes
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
- motor genérico de cards com templates e fontes de dados;
- adaptador para cards que consultam APIs, incluindo uso de variáveis secretas;
- imagens e temas via LittleFS com limites de tamanho;
- validação física da variante `esp32-s3-jc3248w535`;
- suporte ao cartão SD da variante S3;
- testes automatizados para validação de JSON e configuração persistente.

## Comunidade

Este é um projeto open source. Consulte:

- [CONTRIBUTING.md](CONTRIBUTING.md) para contribuir;
- [SECURITY.md](SECURITY.md) para reportar vulnerabilidades;
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) para as regras da comunidade.

## Licença

Distribuído sob a licença [MIT](LICENSE).
