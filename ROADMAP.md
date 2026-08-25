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

## Next — Integrações

- [ ] Adaptador OmniRoute com uso por provedor/modelo.
- [ ] Agente de clima, calendário, GitHub e Home Assistant.
- [ ] MQTT opcional com tópicos documentados.
- [ ] Biblioteca de manifests no diretório `cards/`.
- [ ] Import/export de pacotes de cards.

## Later — Plataforma

- [ ] Autenticação do painel web.
- [ ] OTA com rollback e migração de schema.
- [ ] LittleFS/SD para imagens e fontes.
- [ ] Editor visual no painel.
- [ ] Compatibilidade ampliada com outras placas ESP32.

## Fora da primeira fatia

- JavaScript ou código arbitrário dentro do ESP.
- HTTP configurável diretamente por cards sem proxy/allowlist.
- Gráficos históricos grandes, vídeo ou animações bitmap pesadas.
