/*
 * CLP HF-006 - 6 entradas virtuais via Wi-Fi (servidor web) e Bluetooth (BLE)
 * ---------------------------------------------------------------------------
 *  - Entrada virtual N aciona o relé N (1:1)
 *  - Wi-Fi em modo AP: o ESP32 cria a rede "CLP-HF006" -> http://192.168.4.1
 *    com portal cativo: a página abre sozinha ao conectar na rede
 *  - BLE com o serviço "Nordic UART" (NUS): comandos de texto
 *  - LCD I2C (PCF8574) opcional: se não for detectado, o programa segue sem ele
 *  - Campo "Descrição" na página: o texto (até 120 caracteres, sem acentos) rola uma
 *    vez na linha 2 do LCD 20x2, no lugar do OUT, e depois o OUT volta
 *
 *  Build/gravação via PlatformIO — platformio.ini já define a placa (esp32dev),
 *  a partição huge_app.csv (Wi-Fi + BLE não cabem na partição padrão) e a
 *  biblioteca LiquidCrystal_I2C. Veja o README para os comandos e o fluxo
 *  equivalente pela Arduino IDE (legado/*.ino).
 *
 *  Comandos BLE (enviar texto, com ou sem quebra de linha):
 *    E3=1   liga a entrada 3          E3=0   desliga a entrada 3
 *    E3=T   inverte a entrada 3       ALL=0 / ALL=1  todas
 *    ?      pede o estado atual
 *  Resposta/notificação: "E:101000"  (entrada 1 ... entrada 6)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ======================= MAPEAMENTO DE PINOS (HF-006) =======================
const uint8_t PINO_SDA = 21;
const uint8_t PINO_SCL = 22;
const uint8_t RELES[6] = {4, 16, 17, 18, 19, 23};   // Relé 01..06

// ======================= CONFIGURAÇÕES =======================
const char* AP_SSID  = "CLP-HF006";
const char* AP_SENHA = "clp12345";                   // mínimo 8 caracteres
const char* BLE_NOME = "CLP-HF006";

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // celular -> ESP32 (write)
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // ESP32 -> celular (notify)

// ======================= ESTADO =======================
// Bit i = entrada virtual i+1. Uma variável de 1 byte é lida/escrita de forma atômica,
// então web (loop) e BLE (outra task) podem alterar sem conflito.
volatile uint8_t estado = 0;
uint8_t estadoAplicado = 0xFF;    // força a primeira atualização

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

WebServer server(80);
DNSServer dns;                    // portal cativo: responde qualquer endereço com o IP do ESP32
LiquidCrystal_I2C* lcd = nullptr;

BLECharacteristic* bleTx = nullptr;
volatile bool bleConectado = false;
volatile bool bleReiniciarAnuncio = false;
volatile bool bleEnviarEstado = false;

// ----------------------- alteração de estado (usada por web e BLE) -----------------------
void setEntrada(int i, int valor) {          // i = 0..5, valor: 0, 1 ou 2 (inverter)
  if (i < 0 || i > 5) return;
  portENTER_CRITICAL(&mux);
  if (valor == 2)      estado ^=  (1 << i);
  else if (valor == 1) estado |=  (1 << i);
  else                 estado &= ~(1 << i);
  portEXIT_CRITICAL(&mux);
}

void setTodas(bool ligado) {
  portENTER_CRITICAL(&mux);
  estado = ligado ? 0x3F : 0x00;
  portEXIT_CRITICAL(&mux);
}

String estadoTexto() {                       // "101000"
  String s;
  uint8_t e = estado;
  for (int i = 0; i < 6; i++) s += ((e >> i) & 1) ? '1' : '0';
  return s;
}

// ----------------------- comandos de texto (BLE e Serial) -----------------------
String processarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return "";
  if (cmd == "?") return "E:" + estadoTexto();

  if (cmd.startsWith("ALL=")) {
    char v = cmd.charAt(4);
    if (v == '1' || v == '0') { setTodas(v == '1'); return "OK"; }
    return "ERRO: use ALL=0 ou ALL=1";
  }

  // formato E<n>=<0|1|T>
  if (cmd.length() == 4 && cmd.charAt(0) == 'E' && cmd.charAt(2) == '=') {
    int n = cmd.charAt(1) - '0';
    char v = cmd.charAt(3);
    if (n >= 1 && n <= 6 && (v == '0' || v == '1' || v == 'T')) {
      setEntrada(n - 1, v == 'T' ? 2 : (v - '0'));
      return "OK";
    }
  }
  return "ERRO: use E1..E6 =0/1/T, ALL=0/1 ou ?";
}

// ======================= BLE =======================
class ServidorCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override    { bleConectado = true;  bleEnviarEstado = true; }
  void onDisconnect(BLEServer*) override { bleConectado = false; bleReiniciarAnuncio = true; }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String recebido = String(c->getValue().c_str());
    // aceita vários comandos separados por quebra de linha ou ';'
    recebido.replace(';', '\n');
    int inicio = 0;
    while (inicio <= (int)recebido.length()) {
      int fim = recebido.indexOf('\n', inicio);
      if (fim < 0) fim = recebido.length();
      String resp = processarComando(recebido.substring(inicio, fim));
      if (resp.startsWith("ERRO") || resp.startsWith("E:")) {
        bleTx->setValue((resp + "\n").c_str());
        bleTx->notify();
      }
      inicio = fim + 1;
    }
    // mudanças de estado são notificadas pelo loop()
  }
};

void iniciarBLE() {
  BLEDevice::init(BLE_NOME);
  BLEServer* srv = BLEDevice::createServer();
  srv->setCallbacks(new ServidorCallbacks());

  BLEService* svc = srv->createService(NUS_SERVICE_UUID);

  bleTx = svc->createCharacteristic(NUS_TX_UUID,
            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  bleTx->addDescriptor(new BLE2902());

  BLECharacteristic* rx = svc->createCharacteristic(NUS_RX_UUID,
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new RxCallbacks());

  svc->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("BLE anunciando como \"" + String(BLE_NOME) + "\"");
}

// ======================= MENSAGEM NO LCD =======================
// Texto digitado na página: rola uma vez na linha 2 do LCD, no lugar do OUT; a linha 1
// (IN) fica fixa e, ao terminar, a linha 2 volta a mostrar o OUT/W:.
// Tudo roda na task do loop() (a rota /msg é atendida por ele), sem seção crítica.
const size_t   MSG_MAX      = 120;    // caracteres úteis (a página limita igual)
const uint8_t  LCD_COLUNAS  = 20;     // módulo LCD 20x2
const uint8_t  LCD_LINHAS   = 2;
const uint16_t MSG_PASSO_MS = 350;    // tempo entre deslocamentos de 1 coluna

String   msgTexto;                    // vazio = sem mensagem, linha 2 mostra o OUT/W:
uint16_t msgPos = 0;
uint32_t msgUltimoPasso = 0;

// O HD44780 não tem acentos: troca por a letra base e deixa só ASCII imprimível.
String semAcento(const String& in) {
  String out;
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = in[i];
    if (c >= 32 && c < 127) { out += (char)c; continue; }
    if (c < 0x80) { out += ' '; continue; }                  // tab, quebra de linha etc.
    if (c == 0xC3 && i + 1 < in.length()) {                   // Latin-1 em UTF-8 (á, ç, õ...)
      uint8_t d = in[++i];
      char r = '?';
      if      ((d >= 0x80 && d <= 0x85) || (d >= 0xA0 && d <= 0xA5)) r = d < 0xA0 ? 'A' : 'a';
      else if (d == 0x87 || d == 0xA7)                              r = d == 0x87 ? 'C' : 'c';
      else if ((d >= 0x88 && d <= 0x8B) || (d >= 0xA8 && d <= 0xAB)) r = d < 0xA0 ? 'E' : 'e';
      else if ((d >= 0x8C && d <= 0x8F) || (d >= 0xAC && d <= 0xAF)) r = d < 0xA0 ? 'I' : 'i';
      else if (d == 0x91 || d == 0xB1)                              r = d == 0x91 ? 'N' : 'n';
      else if ((d >= 0x92 && d <= 0x96) || (d >= 0xB2 && d <= 0xB6)) r = d < 0xA0 ? 'O' : 'o';
      else if ((d >= 0x99 && d <= 0x9C) || (d >= 0xB9 && d <= 0xBC)) r = d < 0xA0 ? 'U' : 'u';
      out += r;
      continue;
    }
    if (c >= 0xC0) out += '?';                                // outro caractere multibyte: 1 só '?'
  }                                                           // (bytes de continuação são descartados)
  return out;
}

void definirMensagem(String bruto) {          // texto vazio cancela a mensagem
  if (bruto.length() > MSG_MAX * 4) bruto = bruto.substring(0, MSG_MAX * 4);   // UTF-8: até 4 bytes/char
  String t = semAcento(bruto);
  t.trim();
  if (t.length() > MSG_MAX) t = t.substring(0, MSG_MAX);
  msgTexto = t;
  msgPos = 0;
  msgUltimoPasso = 0;                         // primeiro passo já no próximo loop()
}

// Janela de LCD_COLUNAS sobre "espaços + texto + espaços": o texto entra pela direita,
// atravessa a linha e sai pela esquerda.
String linhaMensagem() {
  String s;
  for (uint8_t i = 0; i < LCD_COLUNAS; i++) {
    int idx = (int)msgPos + i - LCD_COLUNAS;
    s += (idx >= 0 && idx < (int)msgTexto.length()) ? msgTexto[idx] : ' ';
  }
  return s;
}

// ======================= SERVIDOR WEB =======================
// HTML/CSS/JS da página em arquivo separado (src/web/index_html.h) só para
// organização/syntax highlighting no editor; continua compilado junto no
// mesmo firmware (PROGMEM), deploy em um único passo, igual antes.
#include "web/index_html.h"

void enviarEstadoJSON() {
  String json = "{\"e\":\"" + estadoTexto() + "\",\"ble\":" + (bleConectado ? "true" : "false") +
                ",\"w\":" + String(WiFi.softAPgetStationNum()) +
                ",\"m\":" + (msgTexto.length() ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void iniciarWeb() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_SENHA);
  Serial.print("Wi-Fi AP \""); Serial.print(AP_SSID);
  Serial.print("\" -> http://"); Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send_P(200, "text/html", PAGINA); });
  server.on("/estado", enviarEstadoJSON);
  server.on("/set", []() {                     // /set?e=1..6|all&v=0|1|t
    String e = server.arg("e"), v = server.arg("v");
    if (e == "all") setTodas(v == "1");
    else            setEntrada(e.toInt() - 1, v == "t" ? 2 : v.toInt());
    enviarEstadoJSON();
  });
  server.on("/msg", []() {                     // /msg?t=texto  (t vazio cancela)
    definirMensagem(server.arg("t"));
    Serial.println("MSG: \"" + msgTexto + "\"");
    enviarEstadoJSON();
  });
  // Portal cativo: o celular testa endereços como /generate_204 (Android),
  // /hotspot-detect.html (iPhone) e /connecttest.txt (Windows). Como recebem um
  // redirecionamento em vez da resposta esperada, o sistema abre a página sozinho.
  server.onNotFound([]() {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
}

// ======================= LCD =======================
void iniciarLCD() {
  Wire.begin(PINO_SDA, PINO_SCL);
  const uint8_t enderecos[] = {0x27, 0x3F};
  for (uint8_t end : enderecos) {
    Wire.beginTransmission(end);
    if (Wire.endTransmission() == 0) {
      lcd = new LiquidCrystal_I2C(end, LCD_COLUNAS, LCD_LINHAS);
      lcd->init();
      lcd->backlight();
      Serial.printf("LCD encontrado em 0x%02X\n", end);
      return;
    }
  }
  Serial.println("LCD não encontrado (seguindo sem display)");
}

void atualizarLCD() {
  if (!lcd) return;
  String e = estadoTexto();
  String w = String(WiFi.softAPgetStationNum());
  String l0 = "IN  " + e + (bleConectado ? " BLE" : "");
  String l1 = msgTexto.length() ? linhaMensagem() : "OUT " + e + " W:" + w;   // a mensagem toma o lugar do OUT
  while (l0.length() < LCD_COLUNAS) l0 += ' ';  // completa a linha: apaga sobra da mensagem/status anterior
  while (l1.length() < LCD_COLUNAS) l1 += ' ';
  lcd->setCursor(0, 0);
  lcd->print(l0);
  lcd->setCursor(0, 1);
  lcd->print(l1);
}

// ======================= SETUP / LOOP =======================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== CLP HF-006 - Wi-Fi + BLE ===");

  for (uint8_t p : RELES) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }

  iniciarLCD();
  iniciarWeb();
  iniciarBLE();
  Serial.println("Comandos pela Serial também funcionam (ex.: E1=1, ALL=0, ?)");
}

void loop() {
  dns.processNextRequest();
  server.handleClient();

  // comandos pelo monitor serial (útil para testar sem celular)
  if (Serial.available()) {
    String r = processarComando(Serial.readStringUntil('\n'));
    if (r.length()) Serial.println(r);
  }

  // aplica mudanças: relés, BLE e LCD
  uint8_t e = estado;
  static uint32_t ultimoLCD = 0;
  if (e != estadoAplicado) {
    for (int i = 0; i < 6; i++) digitalWrite(RELES[i], (e >> i) & 1);
    estadoAplicado = e;
    Serial.println("E:" + estadoTexto());
    bleEnviarEstado = true;
    ultimoLCD = 0;                              // força atualizar o LCD agora
  }

  if (bleEnviarEstado && bleConectado) {
    bleEnviarEstado = false;
    bleTx->setValue(("E:" + estadoTexto() + "\n").c_str());
    bleTx->notify();
  }

  if (bleReiniciarAnuncio) {                    // volta a anunciar após desconectar
    bleReiniciarAnuncio = false;
    delay(300);
    BLEDevice::startAdvertising();
  }

  if (msgTexto.length() && millis() - msgUltimoPasso >= MSG_PASSO_MS) {   // rolagem da mensagem
    msgUltimoPasso = millis();
    atualizarLCD();
    if (++msgPos > msgTexto.length() + LCD_COLUNAS) {   // saiu toda pela esquerda: volta ao status
      msgTexto = "";
      atualizarLCD();
    }
  }

  if (millis() - ultimoLCD > 1000) {            // LCD: na mudança e a cada 1 s (contador Wi-Fi)
    ultimoLCD = millis();
    atualizarLCD();
  }
}
