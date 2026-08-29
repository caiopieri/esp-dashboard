# OTA do Desk Assistant

## Estado atual

O firmware expõe `GET /api/firmware` e `GET /api/status` com a versão, o
transporte e a política de segurança. O transporte disponível nesta fase é o
`ArduinoOTA`, apropriado somente para desenvolvimento em uma LAN confiável.

Uma senha pode ser injetada sem gravá-la no repositório. Crie um arquivo local
`platformio_override.ini` (ignorado pelo projeto) com:

```ini
[env:esp32-s3-jc3248w535]
build_flags =
  ${env.build_flags}
  -D DESK_OTA_PASSWORD=\"senha-local-de-desenvolvimento\"
```

Depois execute `pio run -e esp32-s3-jc3248w535`. Não faça commit desse
override.

Para uma compilação de produção, ainda é necessário o fluxo abaixo:

1. publicar um manifesto conforme [schemas/ota-manifest.schema.json](../schemas/ota-manifest.schema.json);
2. verificar HTTPS, versão mínima, placa, SHA-256 e assinatura antes de baixar;
3. gravar uma imagem em uma partição A/B inativa;
4. reiniciar com health check de display, armazenamento e rede;
5. confirmar a partição somente após o health check, fazendo rollback em caso de falha.

O binário não deve ser aceito a partir de uma URL fornecida por um card ou por
uma resposta de IA. O catálogo/servidor de atualização será a única origem
confiável.
