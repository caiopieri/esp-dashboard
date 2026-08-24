# ESP Dashboard

Firmware modular para displays inteligentes baseados em ESP32, com carrossel de mini-apps, LVGL, LovyanGFX, touch resistivo, configuração por painel web e logs remotos.

## Estado atual

O alvo validado é o **ESP32-2432S028R / Cheap Yellow Display (CYD)**:

- ESP32 clássico, 240 MHz, 4 MB flash;
- display ILI9341 320×240 em paisagem;
- touch resistivo XPT2046;
- LVGL 8 + LovyanGFX + DMA;
- Wi‑Fi 2,4 GHz;
- carrossel de mini-apps;
- painel web local para Wi‑Fi, cards, variáveis e diagnóstico.

O **ESP32-S3 N16R8** é o próximo alvo de desenvolvimento. Ele ainda não é considerado suportado: será necessário criar uma variante de hardware com mapa de pinos, driver de display/touch, alvo PlatformIO e validação de memória próprios.

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
LovyanGFX + XPT2046
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

## Roadmap

- autenticação/token para o painel web;
- motor genérico de cards com templates e fontes de dados;
- adaptador para cards que consultam APIs, incluindo uso de variáveis secretas;
- imagens e temas via LittleFS com limites de tamanho;
- variante `esp32-s3-n16r8` com mapa de hardware separado;
- testes automatizados para validação de JSON e configuração persistente.

## Licença

Ainda não definida.
