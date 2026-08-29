# Simulador LVGL para PC

Este simulador executa o LVGL de verdade em uma janela SDL2 e renderiza o
contrato declarativo de cards usado pelo firmware. Ele ajuda a validar layout,
fontes, widgets, carrossel, foco visual e navegação antes do flash no ESP32.

## Compilar e executar

Depois de instalar as dependências do projeto com PlatformIO:

```bash
cmake -S simulator -B simulator/build
cmake --build simulator/build -j1
./simulator/build/card_simulator simulator/demo-config.json
```

O argumento final pode ser uma configuração JSON ou um pacote `.card.json`
exportado pelo Card Studio. Use as setas esquerda/direita ou arraste na janela
para navegar; o simulador também avança automaticamente a cada cinco segundos.
Cards com `action.id` exibem o botão e registram o ID solicitado no terminal,
sem executar nada no computador.

O simulador reproduz o renderer LVGL e o tamanho lógico da tela. Ele não
valida GPIO, sequência de inicialização do controlador LCD, DMA, PSRAM,
calibração do touch ou desempenho do barramento físico; essas verificações
continuam sendo feitas no ESP32 e pela inspeção da webcam.
