#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"

// Definição de Pinos (ESP32-S3)
#define BTN_AZUL 4
#define BTN_AMARELO 5
#define BTN_VERMELHO 6
#define BUZZER 7
#define LED_STATUS 15

// Config de Rede (carregadas do secrets.h)
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
const int WIFI_CHANNEL = SECRET_WIFI_CHANNEL;

// Config do Servidor e Totem (carregadas do secrets.h)
const char* SERVER_URL = SECRET_SERVER_URL;
const long ID_UNIDADE = SECRET_ID_UNIDADE;
const char* DEVICE_ID = SECRET_DEVICE_ID;

Adafruit_SSD1306 display(128, 64, &Wire, -1);

int contadorNormal = 0;
int contadorPreferencial = 0;
int contadorUrgencia = 0;

unsigned long ultimaTentativaReconexao = 0;
const unsigned long INTERVALO_RECONEXAO = 10000;

void connectWiFi() {
  Serial.println("Conectando ao WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  if (WIFI_CHANNEL > 0) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - ultimaTentativaReconexao > INTERVALO_RECONEXAO) {
      ultimaTentativaReconexao = now;
      Serial.println("Conexao com WiFi perdida, tentando reconectar...");
      WiFi.disconnect();
      if (WIFI_CHANNEL > 0) {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
      } else {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
    }
  }
}

void mostrarTelaInicial() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Totem pronto.");
  display.println("Escolha o atendimento.");
  display.display();
}

void mostrarSenha(String senha) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("SENHA:");
  display.setCursor(10, 35);
  display.println(senha);
  display.display();
}

void beep() {
  tone(BUZZER, 1000, 150);
}

void notifyServer(const char* tipoAtendimento) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi: aviso nao enviado, totem segue offline.");
    return;
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  JsonDocument doc;
  doc["idUnidade"] = ID_UNIDADE;
  doc["deviceId"] = DEVICE_ID;
  doc["tipoAtendimento"] = tipoAtendimento;

  String body;
  serializeJson(doc, body);

  Serial.println("Enviado: " + body);
  int codigo = http.POST(body);

  if (codigo > 0) {
    Serial.println("Servidor respondeu: " + String(codigo));
  } else {
    Serial.println("Falha ao enviar: " + http.errorToString(codigo));
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_AZUL, INPUT_PULLUP);
  pinMode(BTN_AMARELO, INPUT_PULLUP);
  pinMode(BTN_VERMELHO, INPUT_PULLUP);
  pinMode(LED_STATUS, OUTPUT);

  Wire.begin(8, 9);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERRO: display nao respondeu no I2C");
  } else {
    Serial.println("Display OK");
    display.clearDisplay();
    mostrarTelaInicial();
  }

  digitalWrite(LED_STATUS, HIGH); // totem pronto
  connectWiFi();
}

void loop() {
  checkWiFi();

  if (digitalRead(BTN_AZUL) == LOW) {
    String senha = "N" + String(++contadorNormal);
    Serial.println("AZUL -> " + senha);
    mostrarSenha(senha);
    beep();
    notifyServer("NORMAL");
    delay(3000);
    mostrarTelaInicial();
  }

  if (digitalRead(BTN_AMARELO) == LOW) {
    String senha = "P" + String(++contadorPreferencial);
    Serial.println("AMARELO -> " + senha);
    mostrarSenha(senha);
    beep();
    notifyServer("PREFERENCIAL");
    delay(3000);
    mostrarTelaInicial();
  }

  if (digitalRead(BTN_VERMELHO) == LOW) {
    String senha = "U" + String(++contadorUrgencia);
    Serial.println("VERMELHO -> " + senha);
    mostrarSenha(senha);
    beep();
    notifyServer("URGENCIA");
    delay(3000);
    mostrarTelaInicial();
  }
}