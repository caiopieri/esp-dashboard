# ESP Dashboard Usage Agent

Agente leve para macOS que roda em segundo plano via `launchd`. Ele não abre
porta, não serve páginas e não transforma o Mac em servidor: apenas faz
requisições de saída para os provedores e envia snapshots para o ESP.

## Instalação no Mac

Com o ESP conectado ao Wi‑Fi:

```bash
./tools/usage_agent/install_macos.sh http://192.168.15.64
```

O instalador cria a configuração em:

```text
~/Library/Application Support/esp-dashboard-usage-agent/config.json
```

E registra o processo em `launchd`. O log fica em:

```text
~/Library/Logs/esp-dashboard-usage-agent.log
```

Para testar uma coleta sem deixar o serviço instalado:

```bash
python3 tools/usage_agent/usage_agent.py --once
```

Para remover:

```bash
python3 tools/usage_agent/usage_agent.py --uninstall
```

## Fonte OmniRoute (recomendada)

Se o OmniRoute já estiver instalado no Mac, o agente lê em modo somente leitura
o cache `~/.omniroute/storage.sqlite`. Nenhum token é copiado para o agente ou
enviado ao ESP. Esse cache fornece:

- Claude: uso atual de 5 horas e semanal de 7 dias;
- GPT/ChatGPT: a janela disponível no OmniRoute é apresentada como semanal;
- Gemini: janela atual por modelo e semanal do projeto.

O instalador habilita essa fonte por padrão. O cache precisa ser atualizado
recentemente pelo OmniRoute; por padrão, snapshots com mais de dez minutos são
ignorados.

## Fontes diretas (fallback)

- Claude: lê o token OAuth do Keychain `Claude Code-credentials`, usado pelo
  Claude Code. O token não é salvo no arquivo do agente nem enviado ao ESP.
- OpenAI: usa `OPENAI_ADMIN_KEY` ou uma entrada do Keychain configurada. A API
  de Usage exige uma chave administrativa; o agente envia tokens e requisições
  dos últimos sete dias. Um limite opcional pode ser configurado para calcular
  o percentual semanal. Isso mede a organização da OpenAI API, não uma
  assinatura do ChatGPT web.
- Gemini: como fallback, usa o projeto e o Cloud Monitoring API. É necessário um OAuth do
  Google Cloud com permissão `monitoring.timeSeries.list`. O agente consulta as
  métricas de quota `generate_requests_per_model/usage` e `/limit`. Também
  aceita um `snapshot_file` local para instalações sem Monitoring.

Para usar o coletor direto do GPT, salve a chave administrativa no Keychain:

```bash
security add-generic-password -U \
  -s 'ESP Dashboard OpenAI Admin Key' \
  -a "$USER" -w
```

Para o fallback direto do Gemini, configure `project_id` no `config.json` e forneça um token OAuth
em `GOOGLE_OAUTH_ACCESS_TOKEN`, no Keychain `ESP Dashboard Google OAuth Token`
ou em um ADC de usuário. Depois reinicie o agente:

```bash
launchctl kickstart -k "gui/$(id -u)/com.caiopieri.esp-dashboard-usage-agent"
```

As chaves ficam no Mac. O ESP recebe somente dados normalizados:

```json
{
  "provider": "claude",
  "session_percent": 45,
  "weekly_percent": 28,
  "tokens": "120k",
  "requests": "340",
  "status": "allowed",
  "ok": true
}
```

Percentuais são de uso consumido. Quando uma fonte não oferece uma janela, o
agente envia `null` e o firmware mostra `--` em vez de exibir um falso `0%`.

O mesmo agente poderá ser executado em uma VPS depois, trocando o serviço de
credenciais e o destino do dispositivo; o contrato HTTP não muda.
