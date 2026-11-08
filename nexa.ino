#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define BTN_AZUL 4
#define BTN_AMARELO 5
#define BTN_VERMELHO 6
#define BUZZER 7
#define LED_STATUS 15

Adafruit_SSD1306 display(128, 64, &Wire, -1);

int contadorNormal = 0;
int contadorPreferencial = 0;
int contadorUrgencia = 0;

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

void setup() {
  // put your setup code here, to run once:
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

  digitalWrite(LED_STATUS, HIGH);
}

void loop() {
  if (digitalRead(BTN_AZUL) == LOW) {
    String senha = "N" + String(++contadorNormal);
    Serial.println("AZUL -> " + senha);
    mostrarSenha(senha);
    beep();
    delay(3000);
    mostrarTelaInicial();
  }
  if (digitalRead(BTN_AMARELO) == LOW) {
    String senha = "P" + String(++contadorPreferencial);
    Serial.println("AMARELO -> " + senha);
    mostrarSenha(senha);
    beep();
    delay(3000);
    mostrarTelaInicial();
  }
  if (digitalRead(BTN_VERMELHO) == LOW) {
    String senha = "U" + String(++contadorUrgencia);
    Serial.println("VERMELHO -> " + senha);
    mostrarSenha(senha);
    beep();
    delay(3000);
    mostrarTelaInicial();
  }
}
