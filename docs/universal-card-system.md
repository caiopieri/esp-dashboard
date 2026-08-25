# Universal Card System

## Objetivo

Permitir que o painel web crie e organize cards sem recompilar o firmware,
mantendo os cards nativos para interfaces que exigem comportamento especial.
O firmware deve funcionar no CYD e no ESP32-S3 sem que o card conheça o
driver de display, o barramento SPI ou o controlador de touch.

## Catálogo inicial

O catálogo é dividido por função. A primeira versão não precisa implementar
todas as fontes; ela define o formato estável para que novos adaptadores sejam
adicionados sem mudar a UI.

### Sistema e dispositivo

- Relógio, data, fuso e calendário do dia.
- Clima atual, previsão e sensação térmica.
- Wi-Fi: conectado, RSSI, SSID, IP e gateway.
- Uptime, reinícios, temperatura do chip e motivo do boot.
- CPU, heap, PSRAM, flash, LittleFS e cartão SD.
- Estado do touch, brilho, modo de tela e versão do firmware.
- Saúde dos serviços: agente do Mac/VPS, OmniRoute, MQTT e APIs.
- Log recente e contador de erros.

### Uso de IA

- Claude: janela de 5 horas, semanal, tokens e requisições.
- ChatGPT/Codex: janela semanal, tokens e requisições.
- Gemini: janela atual, semanal, tokens e requisições.
- Uso por modelo e por conta/conexão.
- Comparativo dos provedores: percentual consumido e disponibilidade.
- Custo estimado, latência média e taxa de erro.
- Estado do roteamento OmniRoute e conexão ativa.

### Casa e sensores

- Temperatura, umidade, pressão e qualidade do ar.
- Movimento, presença, luminosidade e ruído.
- Porta/janela, tomadas, lâmpadas e relés.
- Energia instantânea, consumo do dia, tensão e corrente.
- Nível de água, bateria, carregamento e energia solar.

### Produtividade e serviços

- Agenda: próximo evento e agenda do dia.
- Tarefas, lembretes e notas rápidas.
- GitHub: issues, pull requests, workflow e notificações.
- Docker/servidor: containers, CPU, memória, disco e uptime.
- Home Assistant, MQTT e webhooks.
- Música reproduzindo, podcast e mídia atual.
- Ações rápidas: webhook, cena, relé ou comando autorizado.

### Conteúdo e visualização

- Texto, Markdown limitado e anúncio/notificação.
- Métrica/KPI com valor, unidade, variação e timestamp.
- Progresso circular ou barra.
- Lista de até oito itens.
- Gráfico de linha compacto com até 24 pontos.
- Imagem, ícone, QR Code e avatar.
- Contagem regressiva e cronômetro.
- Card composto com duas ou quatro métricas.
- Tema, cor, ícone e animação de mascote.

## Arquitetura

```text
Painel web / agente / MQTT
          │  POST /api/data
          ▼
     DataStore em RAM ◄── VariableStore persistente
          │
          ▼
     CardDefinitionStore (Preferences)
          │
          ▼
       CardRenderer ──► LVGL 8
          │                 │
          └── AppManager ──► tileview/carrossel
```

O ESP não deve executar JavaScript, interpretar HTML arbitrário ou fazer
requisições externas configuradas por terceiros. Fontes remotas são resolvidas
pelo agente/MQTT/servidor e chegam como dados validados. Isso reduz consumo de
RAM, evita SSRF e mantém o loop do LVGL previsível.

## Contrato de definição

```json
{
  "schemaVersion": 1,
  "id": "claude_usage",
  "title": "Claude",
  "type": "usage",
  "enabled": true,
  "deleted": false,
  "order": 0,
  "theme": {"accent": "#F2CDCD", "icon": "claude"},
  "data": {"source": "runtime", "namespace": "claude"},
  "body": {
    "windows": ["session", "weekly"],
    "metrics": ["tokens_today", "requests_today"]
  }
}
```

Tipos declarativos previstos: `text`, `metric`, `progress`, `status`, `clock`,
`list`, `chart`, `image`, `countdown`, `action`, `usage` e `composite`.
Cada tipo tem um schema próprio e limites de tamanho; o cliente web valida
antes do POST e o firmware valida novamente antes de persistir.

## Limites da primeira versão

- Até 8 cards ativos e 16 definições totais.
- Até 6 KB de configuração de cards no CYD; 12 KB no S3 quando o backend
  confirmar a capacidade da partição.
- Até 8 campos por card, 32 caracteres por chave e 256 por valor visual.
- Até 16 valores runtime distintos em RAM no CYD; o S3 pode aumentar esse
  limite quando houver uma política de memória específica.
- Lista com até 8 itens e gráfico com até 24 pontos.
- Um único renderer LVGL ativo por vez; ao trocar de tile, o renderer anterior
  libera seus objetos e timers.
- Atualização de dados em RAM; Preferences só recebe alterações de definição.

## Decisões e trade-offs

- Cards declarativos tornam o painel universal, mas não substituem apps nativos
  para touch complexo, câmera, áudio ou animações pesadas.
- Runtime em RAM evita desgaste de flash, mas perde o último valor após reboot;
  o agente deve reenviar o snapshot.
- Dados remotos chegam por host/MQTT em vez de HTTP arbitrário no ESP. Isso
  custa um componente externo, mas melhora segurança e confiabilidade.
- O catálogo é versionado no GitHub; o dispositivo recebe apenas a definição
  validada, não código executável.

## Critérios de aceitação

1. Criar um card `metric` pelo painel sem recompilar.
2. Alterar ordem, ativar, desativar, restaurar e excluir sem corromper o
   carrossel.
3. Enviar dados por `/api/data` e atualizar apenas o card correspondente.
4. Rejeitar IDs, tipos, tamanhos, ordens duplicadas e payloads inválidos.
5. CYD e S3 compilarem com o mesmo modelo de dados.
6. Nenhum valor secreto retornar em GET ou aparecer no log.
