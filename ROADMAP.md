# Roadmap

## Now — Universal cards v1

- [x] Catálogo inicial de cards e fontes.
- [x] Contrato declarativo versionado.
- [x] `DataStore` em RAM para snapshots por namespace.
- [x] Renderer declarativo para `text`, `metric`, `progress`, `status`,
  `clock`, `list` e `chart`.
- [x] Criação e edição desses cards pelo painel web.
- [x] `/api/data` com validação, limite e log sem segredos.
- [ ] Testes de persistência, payload inválido e migração.
- [x] Card Studio independente com preview 320×240, carrossel e import/export.
- [x] Simulador LVGL/SDL no PC aceitando o mesmo JSON de cards.
- [x] Companion local com validação de manifesto e ponte de IA para rascunhos.
- [x] Pacotes versionados com validação de compatibilidade e ações locais opt-in.

## Next — Integrações

- [ ] Adaptador OmniRoute com uso por provedor/modelo.
- [ ] Agente de clima, calendário, GitHub e Home Assistant.
- [ ] MQTT opcional com tópicos documentados.
- [x] Biblioteca de manifests no diretório `cards/`.
- [x] Import/export de pacotes de cards.

## Later — Plataforma

- [ ] Autenticação do painel web.
- [ ] OTA de produção com assinatura, rollback e migração de schema (contrato dev já exposto).
- [ ] LittleFS/SD para imagens e fontes.
- [ ] Editor visual no painel.
- [ ] Compatibilidade ampliada com outras placas ESP32.
- [x] Provisionamento inicial por BLE no S3 e SoftAP leve no CYD.
- [x] Descoberta do portal por hostname mDNS após conexão Wi‑Fi.
- [x] Conector IA: prompt → JSON validado → preview → confirmação → publicação.
- [ ] Biblioteca comunitária com pacotes assinados e compatibilidade por placa.
- [ ] Perfil de renderização e-ink compartilhando o contrato declarativo.

## Fora da primeira fatia

- JavaScript ou código arbitrário dentro do ESP.
- HTTP configurável diretamente por cards sem proxy/allowlist.
- Gráficos históricos grandes, vídeo ou animações bitmap pesadas.
