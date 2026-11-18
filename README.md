# CLP Academy

Firmware para a placa de CLP **HF-006** (ESP32): 6 entradas virtuais que acionam
6 relés (1:1), controláveis por Wi-Fi (servidor web com portal cativo) e por
Bluetooth Low Energy (serviço Nordic UART).

## Hardware

- Placa: ESP32 Dev Module (ou DOIT ESP32 DEVKIT V1)
- Relés: pinos `4, 16, 17, 18, 19, 23` (um por entrada virtual)
- LCD I2C 20x2 (PCF8574) opcional nos pinos SDA `21` / SCL `22` — se não
  detectado no boot, o firmware segue sem ele
- Conexão com o PC: USB-C

## Funcionalidades

- **Wi-Fi (modo AP):** o ESP32 cria a rede `CLP-HF006` (senha `clp12345`) e
  sobe um servidor web em `http://192.168.4.1`, com portal cativo (a página
  abre sozinha ao conectar na rede)
- **BLE:** serviço Nordic UART (NUS); comandos de texto como `E3=1`, `E3=0`,
  `E3=T` (inverte), `ALL=0` / `ALL=1`, `?` (consulta estado). Resposta no
  formato `E:101000` (entrada 1 a 6)
- Os mesmos comandos também funcionam pelo Monitor Serial (115200 baud)
- **Descrição no LCD:** a página tem um campo de texto (até 120 caracteres) e o
  botão **Mandar**; o texto rola uma vez na linha 2 do LCD, no lugar do `OUT`
  (o `IN` da linha 1 fica fixo), e depois o `OUT` volta. **Limpar**
  cancela a mensagem. O display
  (HD44780) não tem acentos, então o firmware troca por a letra base
  (`ação` → `acao`). Rota usada: `/msg?t=<texto>`

## Estrutura do repositório

| Caminho | O que é |
|---|---|
| `legado/` | Cópia de referência do sketch, usada hoje via **Arduino IDE** (gravação por USB-C). Não é tocada pelo build do PlatformIO — serve como "versão que sabidamente funciona" na placa física. |
| `src/main.cpp` | Mesmo firmware, adaptado para compilar via **PlatformIO/VSCode** (veja a nota técnica abaixo sobre por que é `.cpp` e não `.ino`). |
| `platformio.ini` | Configuração do projeto PlatformIO (placa, partição de memória, bibliotecas). |

## Preparando o ambiente do zero (VSCode + PlatformIO)

Passo a passo para quem está com um computador Windows limpo. Ao final, você
terá compilado e gravado o firmware na placa e validado tudo pelo Monitor Serial.

### 1. Instalar os pré-requisitos

| Ferramenta | Para quê | Download |
|---|---|---|
| **Visual Studio Code** | Editor onde o PlatformIO roda | https://code.visualstudio.com/download |
| **Python 3.8+** | O PlatformIO Core precisa dele | https://www.python.org/downloads/ |
| **Git** (opcional, para clonar o repo) | Baixar o projeto | https://git-scm.com/downloads |

> **Importante (Python):** no instalador, marque a caixa **"Add python.exe to
> PATH"** antes de clicar em *Install Now*. Se esquecer, o PlatformIO não
> encontra o Python.

Confirme no PowerShell (feche e reabra o terminal após instalar):

```
python --version     # deve mostrar Python 3.x
git --version        # deve mostrar a versão do Git
```

### 2. Instalar a extensão PlatformIO IDE no VSCode

1. Abra o VSCode e clique no ícone de **Extensões** (`Ctrl+Shift+X`).
2. Pesquise por **PlatformIO IDE** (autor: PlatformIO) e clique em **Install**.
   Link direto: https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide
3. Aguarde a instalação do PlatformIO Core (aparece uma notificação no canto
   inferior direito; pode levar alguns minutos na primeira vez, pois baixa
   ferramentas e cria um ambiente virtual Python).
4. Quando terminar, clique em **Reload Now** / reinicie o VSCode. Deve aparecer
   o ícone de formiga 👽 (PlatformIO) na barra lateral e a barra azul/inferior
   com os botões ✓ → 🔌.

Documentação oficial: https://docs.platformio.org/en/latest/integration/ide/vscode.html

### 3. Baixar o projeto

```
git clone https://github.com/<seu-usuario>/clp_academy.git
cd clp_academy
code .
```

(ou baixe o ZIP pelo GitHub, extraia e use **File → Open Folder** no VSCode).
Ao abrir a pasta, o PlatformIO detecta o `platformio.ini` e, na primeira vez,
baixa sozinho a plataforma `espressif32`, o framework Arduino e a biblioteca
`LiquidCrystal_I2C` (precisa de internet; leva alguns minutos).

### 4. Instalar o driver USB da placa (Windows)

A HF-006 usa o chip **CP2102**. Conecte o ESP32 por USB-C (use um cabo **com
dados**, não só de carga) e abra o PowerShell:

```
python -m platformio device list
```

- Se aparecer uma porta `COMx`, o driver já está ok — pule para o passo 5.
- Se **não** aparecer nada, instale o driver:
  - **CP210x** (mais comum): https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers
  - Se a sua placa for outra revisão com chip **CH340/CH341**:
    https://www.wch-ic.com/downloads/CH341SER_EXE.html
  - Instruções detalhadas de instalação na dica logo abaixo, na seção
    "Comandos úteis".

Depois de instalar, reconecte o cabo e rode `device list` de novo. No
Gerenciador de Dispositivos deve aparecer em **Portas (COM e LPT)**.

### 5. Testar o ambiente (checklist)

Faça na ordem; se um item falhar, corrija antes de seguir.

| # | Teste | Como | Resultado esperado |
|---|---|---|---|
| 1 | PlatformIO instalado | `python -m platformio --version` (ou `pio --version` no terminal do PlatformIO) | Mostra `PlatformIO Core, version 6.x` |
| 2 | Projeto reconhecido | Abrir a pasta no VSCode | Barra inferior mostra `env:esp32dev` |
| 3 | Compilação | Botão ✓ **Build** ou `python -m platformio run` | Termina com `[SUCCESS]` |
| 4 | Placa detectada | `python -m platformio device list` | Lista uma porta `COMx` (CP210x) |
| 5 | Gravação | Botão → **Upload** ou `python -m platformio run --target upload` | `[SUCCESS]` e `Hard resetting via RTS pin...` |
| 6 | Monitor Serial | Botão 🔌 **Monitor** ou `python -m platformio device monitor` | Aparecem as mensagens de boot a 115200 baud |
| 7 | Relés | No monitor, digite `ALL=1` + Enter | Os 6 relés acionam; `ALL=0` desliga; `?` responde `E:000000` |
| 8 | Wi-Fi | No celular/PC, conectar à rede `CLP-HF006` (senha `clp12345`) | Portal cativo abre; senão acessar `http://192.168.4.1` |
| 9 | BLE | App de BLE (ex.: **nRF Connect** ou **Serial Bluetooth Terminal**) → conectar na placa → enviar `E1=1` | Relé 1 aciona e responde `E:100000` |

Links dos apps de teste BLE:
- nRF Connect (Android): https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp
- nRF Connect (iOS): https://apps.apple.com/app/nrf-connect-for-mobile/id1054362403
- Serial Bluetooth Terminal (Android, modo BLE): https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal

### 6. Problemas comuns

| Sintoma | Causa provável / solução |
|---|---|
| `'python' não é reconhecido` | Python não está no PATH — reinstale marcando "Add python.exe to PATH" |
| `No module named platformio` ao usar `python -m platformio` | A extensão instala o Core em um ambiente próprio. Use o terminal do PlatformIO no VSCode (ícone 👽 → *PIO Home* / *New Terminal*) e rode `pio run`, ou instale global: `python -m pip install -U platformio` |
| Build falha baixando pacotes | Sem internet / proxy / antivírus bloqueando. Tente novamente; confira a conexão |
| `Failed to connect to ESP32: Wrong boot mode` / `Timed out waiting for packet header` | Segure o botão **BOOT** da placa quando aparecer `Connecting...` no upload e solte após iniciar a gravação |
| `could not open port 'COMx'` / acesso negado | Outro programa usando a porta (Monitor Serial aberto, Arduino IDE). Feche-o e tente de novo |
| Porta COM não aparece | Driver não instalado ou cabo só de carga — veja o passo 4 |
| Erro de espaço/partição no build | Confirme que o `platformio.ini` tem `board_build.partitions = huge_app.csv` |

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
