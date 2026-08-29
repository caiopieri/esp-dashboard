# Desk Assistant — arquitetura do produto

## Visão

O Desk Assistant é um dispositivo de informação persistente: ele fica ligado
na mesa, mostra cards escolhidos pelo usuário e recebe dados de fontes externas
sem transformar o ESP32 em um navegador ou em um executor de código remoto.

```text
                         ┌─────────────────────────┐
                         │ Card Studio / Web Portal │
                         │ editar · preview · IA    │
                         └────────────┬────────────┘
                                      │ manifestos validados
       ┌──────────────────────────────┼──────────────────────────────┐
       │                              │                              │
┌──────▼──────┐                ┌──────▼──────┐                ┌──────▼──────┐
│ Comunidade  │                │ Companion   │                │ Firmware    │
│ catálogo    │                │ Agent/MQTT  │                │ ESP32       │
│ pacotes     │                │ dados/ações │                │ LVGL/cards  │
└─────────────┘                └─────────────┘                └──────┬──────┘
                                                                      │
                                                       ┌──────────────▼─────────────┐
                                                       │ LCD · touch · e-ink profile │
                                                       └────────────────────────────┘
```

## Limites importantes

- O ESP executa somente renderers e ações previamente permitidos pelo firmware.
- Um card é um manifesto declarativo; a IA nunca envia C/C++, JavaScript ou
  comandos arbitrários para o dispositivo.
- APIs externas são acessadas por um agente/bridge com allowlist, ou por
  integrações compiladas no firmware. Isso evita SSRF e reduz a carga de RAM.
- Ações no computador passam por um agente local com allowlist, confirmação e
  logs. Um card não recebe acesso direto ao shell.
- Segredos ficam no agente ou no armazenamento protegido do dispositivo e não
  são devolvidos em endpoints de leitura, logs ou pacotes comunitários.

## Componentes

### Firmware

- `DisplayDriver`: backend físico por placa, LVGL e touch;
- `AppManager`: carrossel, navegação, seleção e ciclo de vida;
- `CardDefinitionStore`: configuração persistente e migrações de schema;
- `CardRenderer`: tipos declarativos com limites de memória;
- `DataStore`: snapshots voláteis enviados pelo agente/MQTT;
- `NetworkManager`: Wi-Fi, portal local, API, logs e OTA;
- `ProvisioningManager`: BLE de configuração inicial no S3, com SoftAP leve no
  CYD;
- `UpdateManager`: contrato de manifesto de versão; assinatura, rollback e
  health check entram no OTA de produção.

### Plano de controle

O portal web deve editar definições, ordenar cards, ajustar o intervalo do
carrossel, escolher tema e publicar uma configuração. O preview usa a mesma
resolução e o mesmo contrato do firmware, mas não tenta reproduzir o driver do
LCD pixel a pixel.

O firmware atual usa `WiFiProv`/BLE GATT no ESP32-S3 com sessão efêmera. No CYD,
onde o pacote BLE estoura o orçamento de DRAM, o mesmo contrato cai para um
SoftAP local protegido pelo PoP. A próxima camada poderá trocar o app padrão
do ESP-IDF por uma interface Web Bluetooth customizada sem alterar o modelo de
credenciais. Após a conexão, o ESP32-S3 publica o portal por mDNS com um
hostname único; isso resolve a descoberta do IP, mas não transforma o BLE em um
navegador nem abre uma página automaticamente.

O fluxo de produto é:

1. o dispositivo anuncia um identificador curto (na fatia atual, o QR/PoP
   também é impresso no serial; a exibição no LCD entra no perfil de produto);
2. o usuário confirma o código exibido no onboarding;
3. o telefone envia credenciais Wi-Fi e um token local;
4. o dispositivo apaga a sessão após conectar ou expirar;
5. o dispositivo anuncia um hostname mDNS e o portal HTTP local passa a ser o
   canal de configuração normal.

Nunca devemos enviar senha Wi-Fi em broadcast BLE nem deixar uma credencial
fixa gravada em todos os dispositivos.

### IA para cards

O conector recebe um prompt e contexto limitado (resolução, tipos e limites) e
retorna um objeto intermediário:

```json
{
  "intent": "card_draft",
  "card": {
    "title": "Temperatura",
    "type": "metric",
    "data": {"source": "runtime", "namespace": "sala", "key": "temperature"},
    "body": {"label": "Sala", "unit": "°C"}
  }
}
```

O Studio valida, mostra o preview, apresenta o diff e pede confirmação. O
firmware valida novamente antes de persistir. A resposta da IA não deve conter
URLs arbitrárias, segredos ou código executável.

Cards compartilháveis usam um pacote pequeno e versionado, separado da
configuração pessoal do dispositivo:

```json
{
  "packageVersion": 1,
  "kind": "desk-assistant-card",
  "minimumSchemaVersion": 1,
  "compatibility": {"displays": ["320x240", "480x320", "e-ink"]},
  "card": {"id": "temperature", "title": "Temperatura", "type": "metric"}
}
```

O companion já valida esse pacote e os limites antes de qualquer instalação.
O catálogo comunitário ainda deve verificar assinatura e só então oferecer
instalação. A instalação nunca deve importar código executável.

### OTA

O OTA atual é um ponto de partida de desenvolvimento via ArduinoOTA; o
firmware já expõe versão, transporte, autenticação e prontidão no endpoint
`/api/firmware`. Para produção, a atualização deve usar partições A/B, imagem
assinada, versão mínima, checksum, health check após boot e rollback
automático. O portal deve mostrar a versão atual e o resultado da última
atualização, sem aceitar um binário não verificado por HTTP simples.

### Ações no computador

O companion oferece `GET /api/actions` e `POST /api/actions/execute`. A lista
vem de um arquivo local confiável; o manifesto envia somente um ID. Ações que
abrem uma URL ou lançam um programa usam allowlist, `shell=False` e confirmação
descartável de 30 segundos. No S3, o firmware já preserva `card.action`, mostra
um botão e publica um evento numerado em `/api/events`; o Card Studio faz o
polling, pede confirmação e confirma a entrega em `/api/events/ack`.

### E-ink

E-ink deve ser um perfil de renderização, não um `if` espalhado pelos cards:

- sem animações contínuas;
- refresh parcial opcional;
- política de atualização e desgaste configurável;
- mesma definição declarativa sempre que o tipo for compatível;
- cards interativos e animações marcados como incompatíveis no preview.

## Fases de entrega

1. **Fundação atual:** cards universais, carrossel, portal local e Card Studio.
2. **Configuração:** BLE de provisionamento, CORS/token local e configurações
   de auto-slide.
3. **Dados:** agentes para clima, calendário, GitHub, Home Assistant e uso de
   IA, usando snapshots validados.
4. **IA:** gerar → validar → visualizar → confirmar → publicar.
5. **Ações:** agente de desktop com allowlist, confirmação e auditoria.
6. **Plataforma:** biblioteca comunitária, pacotes assinados, OTA robusto e
   perfil e-ink.

Cada fase deve manter o firmware capaz de iniciar offline com a última
configuração válida.
