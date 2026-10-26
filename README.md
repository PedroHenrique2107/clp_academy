# CLP Academy

Firmware para a placa de CLP **HF-006** (ESP32): 6 entradas virtuais que acionam
6 relés (1:1), controláveis por Wi-Fi (servidor web com portal cativo) e por
Bluetooth Low Energy (serviço Nordic UART).

## Hardware

- Placa: ESP32 Dev Module (ou DOIT ESP32 DEVKIT V1)
- Relés: pinos `4, 16, 17, 18, 19, 23` (um por entrada virtual)
- LCD I2C (PCF8574) opcional nos pinos SDA `21` / SCL `22` — se não detectado
  no boot, o firmware segue sem ele
- Conexão com o PC: USB-C

## Funcionalidades

- **Wi-Fi (modo AP):** o ESP32 cria a rede `CLP-HF006` (senha `clp12345`) e
  sobe um servidor web em `http://192.168.4.1`, com portal cativo (a página
  abre sozinha ao conectar na rede)
- **BLE:** serviço Nordic UART (NUS); comandos de texto como `E3=1`, `E3=0`,
  `E3=T` (inverte), `ALL=0` / `ALL=1`, `?` (consulta estado). Resposta no
  formato `E:101000` (entrada 1 a 6)
- Os mesmos comandos também funcionam pelo Monitor Serial (115200 baud)

## Estrutura do repositório

| Caminho | O que é |
|---|---|
| `legado/` | Cópia de referência do sketch, usada hoje via **Arduino IDE** (gravação por USB-C). Não é tocada pelo build do PlatformIO — serve como "versão que sabidamente funciona" na placa física. |
| `src/main.cpp` | Mesmo firmware, adaptado para compilar via **PlatformIO/VSCode** (veja a nota técnica abaixo sobre por que é `.cpp` e não `.ino`). |
| `platformio.ini` | Configuração do projeto PlatformIO (placa, partição de memória, bibliotecas). |

## Gravando o firmware

Hoje existem dois caminhos possíveis, equivalentes em resultado:

### Opção 1 — Arduino IDE (fluxo legado)
Abrir `legado/CLP_HF006_WiFi_BLE.ino` na Arduino IDE com o ESP32 conectado via
USB-C e usar Upload normalmente. Configurações necessárias na IDE:
- Placa: **ESP32 Dev Module**
- Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)** — Wi-Fi + BLE juntos
  não cabem na partição padrão
- Biblioteca **LiquidCrystal I2C** (Frank de Brabander), via Gerenciador de
  Bibliotecas

### Opção 2 — VSCode + PlatformIO (fluxo atual)
1. Abrir a pasta do repositório no VSCode com a extensão **PlatformIO IDE**
   instalada
2. Conectar o ESP32 via USB-C
3. Usar os botões da barra inferior do PlatformIO: ✓ **Build**, → **Upload**,
   🔌 **Monitor** (ou pela CLI, dentro da pasta do projeto):
   ```
   python -m platformio run                 # compila
   python -m platformio run --target upload # grava na placa
   python -m platformio device monitor      # abre o monitor serial (115200)
   ```
`platformio.ini` já define a placa (`esp32dev`), o esquema de partição
(`huge_app.csv`, equivalente ao "Huge APP" da Arduino IDE) e a dependência
`LiquidCrystal_I2C`, então não é necessário configurar nada manualmente antes
de compilar.

#### Comandos úteis (PlatformIO via CLI)

```
python -m platformio device list                                # lista as portas COM disponíveis
python -m platformio run                                         # compila
python -m platformio run --target upload --upload-port COM5      # grava na placa (ajuste a porta)
python -m platformio device monitor --port COM5                  # abre o monitor serial (115200)
```

> Dica: se `python -m platformio device list` não listar nenhuma porta, o
> Windows não tem o driver USB-serial da placa instalado (chip **CP2102**,
> aparece como "Outros dispositivos" com erro no Gerenciador de Dispositivos —
> Código 28). Baixe o **CP210x Universal Windows Driver** em
> https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers,
> extraia e instale pelo Gerenciador de Dispositivos (botão direito no
> dispositivo → Atualizar driver → Procurar no meu computador → apontar para
> a pasta extraída) ou, num terminal **como administrador**:
> ```
> pnputil /add-driver "C:\caminho\para\CP210x_Universal_Windows_Driver\silabser.inf" /install
> ```
> Depois reconecte o cabo USB; a porta passa a aparecer como "Silicon Labs
> CP210x USB to UART Bridge (COMx)".

**Teste rápido pelo Monitor Serial** (depois do upload, com o monitor aberto):
digite `ALL=1` e Enter — os 6 relés devem acionar; `ALL=0` desliga todos; `?`
mostra o estado atual (`E:101000`).

## Nota técnica: por que `src/main.cpp` e não um `.ino`

O PlatformIO converte automaticamente arquivos `.ino` para `.cpp` antes de
compilar, inserindo protótipos de função por conta própria. Esse conversor é
mais simples que o da Arduino IDE e se confunde com o HTML/JavaScript embutido
na string bruta (`R"rawliteral(...)rawliteral"`) usada para servir a página
web — o `{` e `}` do CSS/JS dentro da string bagunçam a contagem de escopo do
conversor, gerando erros de compilação incompreensíveis (ex.: `'async' does
not name a type`).

Por isso, no PlatformIO o firmware foi renomeado para `src/main.cpp` (com
`#include <Arduino.h>` explícito no topo), pulando essa conversão automática.
O conteúdo é o mesmo do `legado/`; como o arquivo já declara as funções na
ordem em que são usadas, nenhum protótipo extra precisou ser adicionado.

## Observação

Existe um arquivo duplicado em `legado/CLP_HF006_WiFi_BLE (1).ino` (idêntico
ao `CLP_HF006_WiFi_BLE.ino`), provavelmente de um download duplicado — pode
ser removido com segurança quando quiser.
