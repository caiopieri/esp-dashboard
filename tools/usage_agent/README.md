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

## Credenciais

- Claude: lê o token OAuth do Keychain `Claude Code-credentials`, usado pelo
  Claude Code. O token não é salvo no arquivo do agente nem enviado ao ESP.
- OpenAI: usa `OPENAI_ADMIN_KEY` ou uma entrada do Keychain configurada. A API
  de Usage exige uma chave administrativa; o agente envia tokens e requisições
  dos últimos sete dias. Um limite opcional pode ser configurado para calcular
  o percentual semanal.
- Gemini: fica desativado por padrão. O uso e as cotas são definidos por
  projeto, e a coleta correta precisa de acesso ao dashboard/Cloud Monitoring.
  Até o adaptador próprio ser adicionado, o agente aceita um `snapshot_file`
  local com o mesmo formato de `/api/usage`.

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

O mesmo agente poderá ser executado em uma VPS depois, trocando o serviço de
credenciais e o destino do dispositivo; o contrato HTTP não muda.
