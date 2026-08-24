# ADR-0001: perfis de hardware para displays

Status: aceito

## Contexto

O firmware já funciona no CYD ESP32-2432S028R com ILI9341 em SPI, touch XPT2046 em SPI dedicado e LVGL 8. A próxima placa é a Guition/Jingcai JC3248W535EN, baseada em ESP32-S3 N16R8, com display AXS15231B em QSPI e touch capacitivo AXS15231B em I²C.

Esses barramentos e controladores não são equivalentes ao conjunto do CYD. O callback de flush e o callback de input precisam continuar compartilhando a mesma interface LVGL, mas não podem compartilhar classes ou pinos específicos de uma placa.

## Decisão

- O `DisplayDriver` seleciona o backend por macro de ambiente PlatformIO.
- O perfil `esp32-cyd` mantém LovyanGFX + XPT2046 sem alteração de hardware.
- O perfil `esp32-s3-jc3248w535` usa Arduino_GFX 1.4.9 para AXS15231B/QSPI.
- O touch S3 usa um driver I²C próprio e isolado, com a sequência de leitura de 11 bytes do controlador.
- O mapa de pinos fica nos headers da variante, não em `AppManager` nem nos apps.
- A camada de apps, rede, variáveis e logs continua comum às duas variantes.

## Alternativas consideradas

### Usar LovyanGFX nos dois alvos

Não escolhido: o LovyanGFX disponível no projeto não fornece o painel AXS15231B/QSPI usado pela JC3248W535EN.

### Duplicar todo o firmware por placa

Não escolhido: duplicaria a lógica do carrossel, rede e persistência, aumentando o risco de divergência.

### Migrar tudo para Arduino_GFX

Não escolhido nesta etapa: o CYD já está validado com LovyanGFX + DMA. Uma migração global aumentaria o risco sem benefício para o objetivo imediato.

## Consequências

- O firmware tem duas dependências gráficas, mas cada ambiente carrega somente a sua.
- O S3 tem uma variante de build reproduzível, mas ainda não é declarado hardware validado.
- A validação física do S3 precisa confirmar imagem, rotação, coordenadas, backlight, Wi‑Fi e cartão SD.

## Onde isto pode dar errado

- O anúncio comercial pode ter revisões de PCB com pinouts diferentes; a primeira gravação precisa confirmar os pinos contra a placa real.
- A tela pode inicializar e ainda apresentar rotação, cores ou janela de flush incorretas; por isso o perfil S3 não é marcado como validado.
- O cartão SD não foi habilitado nesta etapa porque seus pinos e modo de compartilhamento precisam ser confirmados no hardware.

## Próximos passos

1. Conectar a JC3248W535EN somente na etapa de teste.
2. Identificar a porta USB e gravar o ambiente `esp32-s3-jc3248w535`.
3. Executar o teste de display/touch antes de testar o painel web.
4. Adicionar o driver do cartão SD após confirmar o pinout da revisão física.
