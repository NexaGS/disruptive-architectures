#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include "secrets.h"

#define BUZZER 7
#define LED_STATUS 15

const byte LINHAS = 4;
const byte COLUNAS = 4;
char mapaTeclas[LINHAS][COLUNAS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte pinosLinhas[LINHAS] = {4, 5, 6, 16};
byte pinosColunas[COLUNAS] = {17, 18, 21, 35};
Keypad teclado = Keypad(makeKeymap(mapaTeclas), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Config de rede (carregada do secrets.h)
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
const int WIFI_CHANNEL = SECRET_WIFI_CHANNEL;
const char* SERVER_URL = SECRET_SERVER_URL;
const long ID_UNIDADE = SECRET_ID_UNIDADE;

//Estado do totem
enum Estado { OCIOSO, DIGITANDO_CPF, SELECIONANDO_SERVICO, ENVIANDO, MOSTRANDO_SENHA, MOSTRANDO_ERRO };
Estado estadoAtual = OCIOSO;

String cpfDigitado = "";
unsigned long marcaTempoTela = 0;
const unsigned long TEMPO_EXIBICAO = 4000;

unsigned long ultimaTentativaReconexao = 0;
const unsigned long INTERVALO_RECONEXAO = 10000;
bool wifiLogado = false;

// Servicos disponiveis - a posicao no array define a tecla (1 a 5)
const char* SERVICOS[] = { "CLINICO_GERAL", "TRIAGEM", "VACINACAO", "PEDIATRIA", "ODONTOLOGIA" };
const char* SERVICOS_LABEL[] = { "Clinico Geral", "Triagem", "Vacinacao", "Pediatria", "Odontologia" };
const int QTD_SERVICOS = 5;

//  Wi-Fi 
void conectarWiFi() {
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

void verificarWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiLogado) {
      wifiLogado = true;
      Serial.println("WiFi conectado! IP: " + WiFi.localIP().toString());
    }
  } else {
    wifiLogado = false;
    unsigned long agora = millis();
    if (agora - ultimaTentativaReconexao > INTERVALO_RECONEXAO) {
      ultimaTentativaReconexao = agora;
      Serial.println("WiFi caiu, tentando reconectar...");
      WiFi.disconnect();
      conectarWiFi();
    }
  }
}

bool cpfValido(const String &cpf) {
  if (cpf.length() != 11) return false;

  bool todosIguais = true;
  for (int i = 1; i < 11; i++) {
    if (cpf[i] != cpf[0]) { todosIguais = false; break; }
  }
  if (todosIguais) return false;

  return true;
}

//  Display
void beep() {
  tone(BUZZER, 1000, 100);
}

void mostrarOcioso() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("Bem-vindo!");
  display.println("Digite seu CPF");
  display.println("e aperte #");
  display.display();
}

void mostrarCpfDigitado() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("CPF (11 digitos):");
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(cpfDigitado);
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.println("* limpa   # confirma");
  display.display();
}

void mostrarMenuServico() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Atendimento:");
  for (int i = 0; i < QTD_SERVICOS; i++) {
    display.setCursor(0, 10 + i * 10);
    display.print(i + 1);
    display.print("-");
    display.println(SERVICOS_LABEL[i]);
  }
  display.display();
}

void mostrarEnviando() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 25);
  display.println("Enviando...");
  display.display();
}

void mostrarSenha(const String &senha) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 5);
  display.println("SENHA:");
  display.setTextSize(2);
  display.setCursor(10, 25);
  display.println(senha);
  display.display();
}

void mostrarErro(const String &mensagem) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.println(mensagem);
  display.setCursor(0, 45);
  display.println("Procure a recepcao.");
  display.display();
}

// Envio a API
void enviarCheckIn(int indiceServico) {
  estadoAtual = ENVIANDO;
  mostrarEnviando();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi - nao foi possivel enviar o check-in");
    estadoAtual = MOSTRANDO_ERRO;
    marcaTempoTela = millis();
    mostrarErro("Sem conexao.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); 

  HTTPClient http;
  http.begin(client, SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Bypass-Tunnel-Reminder", "true");
  http.setTimeout(5000);

  JsonDocument doc;
  doc["idUnidade"] = ID_UNIDADE;
  doc["cpf"] = cpfDigitado;
  doc["servico"] = SERVICOS[indiceServico];

  String corpo;
  serializeJson(doc, corpo);
  Serial.println("Enviando: " + corpo);

  int codigo = http.POST(corpo);
  Serial.println("Codigo HTTP: " + String(codigo));

  if (codigo == 201) {
    String resposta = http.getString();
    Serial.println("Resposta: " + resposta);

    JsonDocument respostaJson;
    DeserializationError erro = deserializeJson(respostaJson, resposta);

    if (!erro && respostaJson["senha"]) {
      String senha = respostaJson["senha"].as<String>();
      estadoAtual = MOSTRANDO_SENHA;
      marcaTempoTela = millis();
      beep();
      mostrarSenha(senha);
    } else {
      estadoAtual = MOSTRANDO_ERRO;
      marcaTempoTela = millis();
      mostrarErro("Resposta invalida.");
    }
  } else if (codigo == 404) {
    estadoAtual = MOSTRANDO_ERRO;
    marcaTempoTela = millis();
    mostrarErro("CPF nao encontrado.");
  } else if (codigo > 0) {
    estadoAtual = MOSTRANDO_ERRO;
    marcaTempoTela = millis();
    mostrarErro("Erro no servidor.");
  } else {
    Serial.println("Falha ao conectar: " + http.errorToString(codigo));
    estadoAtual = MOSTRANDO_ERRO;
    marcaTempoTela = millis();
    mostrarErro("Falha na conexao.");
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_STATUS, OUTPUT);

  Wire.begin(8, 9);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERRO: display nao respondeu no I2C");
  } else {
    Serial.println("Display OK");
  }

  digitalWrite(LED_STATUS, HIGH);
  conectarWiFi();
  mostrarOcioso();
}

void loop() {
  verificarWiFi();

  char tecla = teclado.getKey();

  switch (estadoAtual) {

    case OCIOSO:
      if (tecla) {
        cpfDigitado = "";
        estadoAtual = DIGITANDO_CPF;
        if (isDigit(tecla)) {
          cpfDigitado += tecla;
          beep();
        }
        mostrarCpfDigitado();
      }
      break;

    case DIGITANDO_CPF:
      if (tecla) {
        if (tecla == '*') {
          cpfDigitado = "";
          mostrarCpfDigitado();
        } else if (tecla == '#') {
          if (cpfValido(cpfDigitado)) {
            estadoAtual = SELECIONANDO_SERVICO;
            mostrarMenuServico();
          } else {
            estadoAtual = MOSTRANDO_ERRO;
            marcaTempoTela = millis();
            mostrarErro("CPF invalido.");
          }
        } else if (isDigit(tecla) && cpfDigitado.length() < 11) {
          cpfDigitado += tecla;
          beep();
          mostrarCpfDigitado();
        }
      }
      break;

    case SELECIONANDO_SERVICO:
      if (tecla) {
        if (tecla == '*') {
          estadoAtual = DIGITANDO_CPF;
          mostrarCpfDigitado();
        } else if (tecla >= '1' && tecla <= ('0' + QTD_SERVICOS)) {
          int indice = tecla - '1';
          beep();
          enviarCheckIn(indice);
        }
      }
      break;

    case MOSTRANDO_SENHA:
    case MOSTRANDO_ERRO:
      if (millis() - marcaTempoTela > TEMPO_EXIBICAO) {
        estadoAtual = OCIOSO;
        mostrarOcioso();
      }
      break;

    case ENVIANDO:
      // tratado de forma sincrona dentro de enviarCheckIn()
      break;
  }
}