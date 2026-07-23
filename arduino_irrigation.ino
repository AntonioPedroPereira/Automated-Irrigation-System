#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

Adafruit_AlphaNum4 display = Adafruit_AlphaNum4();

// Definicao dos pinos do Keypad, Buzzer e Rele
const int pinosLinhas[] = {6, 2, 3, 5};   
const int pinosColunas[] = {7, 8, 4};    
const int BUZZER_PIN = 9;
const int RELE_PIN = 10; 

enum Estados {
  SELECAO_MODO,
  MODO_CONFIG_REGA,       
  MODO_CONFIG_ESPERA_24H, 
  MODO_REPETITIVO_CUSTOM, 
  CONTAGEM,               
  MODO_CONFIG_BRILHO,     
  MODO_DESLIGADO,
  MODO_TESTE_LEDS
};

Estados estadoAtual = SELECAO_MODO; 

String sequenciaNumeros = "";
long tempoRegaMs = 1000;       
long tempoEsperaSegundos = 0;   
bool interromperCicloCustom = false; 
int nivelBrilhoAtual = 7;       

char mapaTeclas[4][3] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

String nomeSegmento[] = {
  "A (Topo Horiz. Esq)", "A2 (Topo Horiz. Dir)", "B (Topo Vert. Dir)", "C (Baixo Vert. Dir)",
  "D (Base Horiz. Dir)", "D2 (Base Horiz. Esq)", "E (Baixo Vert. Esq)", "F (Topo Vert. Esq)",
  "G (Centro Horiz. Esq)", "G2 (Centro Horiz. Dir)", "H (Diag. Topo Esq)", "I (Diag. Topo Dir)",
  "J (Vert. Centro Cima)", "K (Diag. Baixo Dir)", "L (Diag. Baixo Esq)", "M (Vert. Centro Baixo)"
};

void beepNormal() { tone(BUZZER_PIN, 2000, 80); }
void somErro() { tone(BUZZER_PIN, 150, 250); delay(300); tone(BUZZER_PIN, 150, 250); delay(300); }
void somAviso() { tone(BUZZER_PIN, 2000, 100); delay(150); tone(BUZZER_PIN, 2000, 100); }
void somSucesso() { tone(BUZZER_PIN, 523, 100); delay(120); tone(BUZZER_PIN, 659, 100); delay(120); tone(BUZZER_PIN, 784, 250); delay(300); }

void atualizarDisplay(String texto, bool forcarPonto = false) {
  display.clear(); 
  if (texto != "ERR" && texto != "MODE" && texto != "REST" && texto != "    " && texto.length() < 4) {
    while (texto.length() < 4) { texto = "0" + texto; }
  }
  int tamanho = texto.length();
  for (int i = 0; i < 4; i++) {
    if (i < tamanho) { 
      if (forcarPonto && (3 - i) == 1) {
        display.writeDigitAscii(3 - i, texto[tamanho - 1 - i], true); 
      } else {
        display.writeDigitAscii(3 - i, texto[tamanho - 1 - i], false); 
      }
    } else { display.writeDigitAscii(3 - i, ' '); }
  }
  display.writeDisplay(); 
}

void piscarConfirmacao(String valor, bool comPonto) {
  for (int i = 0; i < 2; i++) {
    atualizarDisplay("    ", false); delay(250);
    atualizarDisplay(valor, comPonto); delay(250);
  }
}

char verificarTecladoGeral() {
  for (int linha = 0; linha < 4; linha++) {
    digitalWrite(pinosLinhas[linha], LOW);
    for (int col = 0; col < 3; col++) {
      if (digitalRead(pinosColunas[col]) == LOW) {
        char tecla = mapaTeclas[linha][col];
        while (digitalRead(pinosColunas[col]) == LOW) { delay(10); } 
        digitalWrite(pinosLinhas[linha], HIGH);
        return tecla; 
      }
    }
    digitalWrite(pinosLinhas[linha], HIGH);
  }
  return '\0'; 
}
void executarContagem(long tempoMs) {
  unsigned long tempoAnteriorMsg = millis();
  unsigned long tempoAnteriorSubtracao = micros();
  Serial.println("--- REGA ATIVADA (BOMBA LIGADA) ---");
  digitalWrite(RELE_PIN, HIGH); // Liga o relé no modo NC

  while (tempoMs > 0) {
    if (millis() - tempoAnteriorMsg >= 100) {
      atualizarDisplay(String(tempoMs), false); 
      tempoAnteriorMsg = millis();
    }
    if (micros() - tempoAnteriorSubtracao >= 1000) {
      tempoMs--;
      tempoAnteriorSubtracao += 1000;
    }
  }
  digitalWrite(RELE_PIN, LOW); // Desliga a bomba no modo NC imediatamente
  Serial.println("--- REGA FINALIZADA (BOMBA DESLIGADA) ---");
  atualizarDisplay("0000", false); 
  somAviso(); delay(150); somSucesso(); 
}

bool executarEsperaCustom(long segundosRestantes) {
  unsigned long ultimoSegundo = millis();
  unsigned long tempoUltimoClique = millis(); 
  bool ecraLigado = true;

  long horas = segundosRestantes / 3600;
  long minutes = (segundosRestantes % 3600) / 60;
  String strHoras = (horas < 10) ? "0" + String(horas) : String(horas);
  String strMinutos = (minutes < 10) ? "0" + String(minutes) : String(minutes);
  atualizarDisplay(strHoras + strMinutos, true); 

  while (segundosRestantes > 0) {
    if (millis() - ultimoSegundo >= 1000) {
      segundosRestantes--; ultimoSegundo = millis();
      horas = segundosRestantes / 3600; minutes = (segundosRestantes % 3600) / 60;
      strHoras = (horas < 10) ? "0" + String(horas) : String(horas);
      strMinutos = (minutes < 10) ? "0" + String(minutes) : String(minutes);
      if (ecraLigado) { atualizarDisplay(strHoras + strMinutos, true); }
    }
    if (ecraLigado && (millis() - tempoUltimoClique >= 15000)) {
      ecraLigado = false; 
      atualizarDisplay("REST", false); delay(1000);
      display.clear(); display.writeDisplay();
    }
    char teclaPressionada = verificarTecladoGeral();
    if (teclaPressionada != '\0') {
      if (teclaPressionada == '*') return false; 
      tempoUltimoClique = millis(); 
      if (!ecraLigado) {
        ecraLigado = true; somAviso(); atualizarDisplay(strHoras + strMinutos, true);
      } else { beepNormal(); }
    }
    delay(20); 
  }
  return true; 
}

bool ejecutarRotacaoLeds() {
  for (int digito = 0; digito < 4; digito++) {
    for (int barra = 0; barra < 16; barra++) {
      display.clear(); uint16_t mascaraBinaria = (1 << barra);
      display.writeDigitRaw(digito, mascaraBinaria); display.writeDisplay(); 
      
      unsigned long tempoInicioBarra = millis();
      while (millis() - tempoInicioBarra < 400) {
        char tecla = verificarTecladoGeral();
        if (tecla == '*') { display.clear(); display.writeDisplay(); return false; }
        delay(10);
      }
    }
  }
  display.clear(); display.writeDisplay(); somSucesso(); return true;
}

void setup() {
  Serial.begin(9600); pinMode(BUZZER_PIN, OUTPUT); pinMode(RELE_PIN, OUTPUT); digitalWrite(RELE_PIN, LOW); 
  if (!display.begin(0x70)) { while (1); }
  display.setBrightness(nivelBrilhoAtual); atualizarDisplay("MODE", false); 
  for (int i = 0; i < 4; i++) { pinMode(pinosLinhas[i], OUTPUT); digitalWrite(pinosLinhas[i], HIGH); }
  for (int j = 0; j < 3; j++) { pinMode(pinosColunas[j], INPUT_PULLUP); }
}

void loop() {
  for (int i_linha = 0; i_linha < 4; i_linha++) {
    digitalWrite(pinosLinhas[i_linha], LOW);
    for (int col = 0; col < 3; col++) {
      if (digitalRead(pinosColunas[col]) == LOW) {
        char teclaPressionada = mapaTeclas[i_linha][col];

        if (estadoAtual == MODO_DESLIGADO) {
          somAviso(); estadoAtual = SELECAO_MODO; sequenciaNumeros = "";
          atualizarDisplay("MODE", false);
          while (digitalRead(pinosColunas[col]) == LOW) { delay(10); }
          digitalWrite(pinosLinhas[i_linha], HIGH); return;
        }

        if (teclaPressionada == '*') {
          if (sequenciaNumeros.length() > 0) { sequenciaNumeros.remove(sequenciaNumeros.length() - 1); }
          if (estadoAtual == SELECAO_MODO && sequenciaNumeros.length() == 0) { atualizarDisplay("MODE", false); }
          else { atualizarDisplay(sequenciaNumeros.length() == 0 ? "0000" : sequenciaNumeros, false); }
          beepNormal();
        }
        else if (teclaPressionada == '#') {
          if (estadoAtual == SELECAO_MODO) {
            if (sequenciaNumeros == "1") { somAviso(); estadoAtual = MODO_CONFIG_REGA; sequenciaNumeros = ""; atualizarDisplay("0000", false); } 
            else if (sequenciaNumeros == "2") { somAviso(); estadoAtual = MODO_CONFIG_ESPERA_24H; sequenciaNumeros = ""; atualizarDisplay("0000", false); } 
            else if (sequenciaNumeros == "3") { somAviso(); estadoAtual = CONTAGEM; executarContagem(tempoRegaMs); digitalWrite(RELE_PIN, LOW); estadoAtual = SELECAO_MODO; sequenciaNumeros = ""; atualizarDisplay("MODE", false); } 
            else if (sequenciaNumeros == "4") { somAviso(); estadoAtual = MODO_CONFIG_BRILHO; sequenciaNumeros = String(nivelBrilhoAtual); atualizarDisplay(sequenciaNumeros, false); }
            else if (sequenciaNumeros == "0") { somAviso(); estadoAtual = MODO_DESLIGADO; atualizarDisplay("REST", false); delay(1000); display.clear(); display.writeDisplay(); } 
            else if (sequenciaNumeros == "5") { somAviso(); estadoAtual = MODO_TESTE_LEDS; atualizarDisplay("    ", false); delay(300); if (!ejecutarRotacaoLeds()) { somErro(); } estadoAtual = SELECAO_MODO; sequenciaNumeros = ""; atualizarDisplay("MODE", false); }
            else { sequenciaNumeros = ""; atualizarDisplay("ERR", false); somErro(); delay(500); atualizarDisplay("MODE", false); }
          }
          else if (estadoAtual == MODO_CONFIG_REGA) {
            if (sequenciaNumeros.length() >= 1 && sequenciaNumeros.length() <= 4) {
              tempoRegaMs = sequenciaNumeros.toInt(); somAviso(); piscarConfirmacao(sequenciaNumeros, false);
              estadoAtual = SELECAO_MODO; sequenciaNumeros = ""; atualizarDisplay("MODE", false);
            } else { sequenciaNumeros = ""; atualizarDisplay("ERR", false); somErro(); delay(500); atualizarDisplay("0000", false); }
          }
          else if (estadoAtual == MODO_CONFIG_ESPERA_24H) {
            if (sequenciaNumeros.length() >= 1 && sequenciaNumeros.length() <= 4) {
              String tempoFormatado = sequenciaNumeros;
              while (tempoFormatado.length() < 4) { tempoFormatado = "0" + tempoFormatado; }
              
              long hh = tempoFormatado.substring(0, 2).toInt();
              long mm = tempoFormatado.substring(2, 4).toInt();
              tempoEsperaSegundos = (hh * 3600) + (mm * 60);

              // REQUISITO: Bloqueia a ativacao se o tempo de espera for igual a 0
              if (tempoEsperaSegundos <= 0) {
                sequenciaNumeros = "";
                atualizarDisplay("ERR", false); somErro(); delay(500);
                atualizarDisplay("0000", false);
              } else {
                somAviso(); piscarConfirmacao(tempoFormatado, true);
                estadoAtual = MODO_REPETITIVO_CUSTOM;
                interromperCicloCustom = false;

                while (!interromperCicloCustom) {
                  if (!executarEsperaCustom(tempoEsperaSegundos)) { 
                    interromperCicloCustom = true; somErro(); break;
                  }
                  if (!interromperCicloCustom) { executarContagem(tempoRegaMs); digitalWrite(RELE_PIN, LOW); }
                }
                estadoAtual = SELECAO_MODO; sequenciaNumeros = ""; atualizarDisplay("MODE", false);
              }
            } else { sequenciaNumeros = ""; atualizarDisplay("ERR", false); somErro(); delay(500); atualizarDisplay("0000", false); }
          }
          else if (estadoAtual == MODO_CONFIG_BRILHO) { 
            int novoBrilho = sequenciaNumeros.toInt();
            if (novoBrilho >= 0 && novoBrilho <= 15) {
              nivelBrilhoAtual = novoBrilho; display.setBrightness(nivelBrilhoAtual); 
              somAviso(); estadoAtual = SELECAO_MODO; sequenciaNumeros = ""; atualizarDisplay("MODE", false);
            } else {
              sequenciaNumeros = String(nivelBrilhoAtual); atualizarDisplay("ERR", false); somErro(); delay(500); atualizarDisplay(sequenciaNumeros, false);
            }
          }
        }
        else {
          if (sequenciaNumeros.length() < 4) {
            sequenciaNumeros += teclaPressionada; atualizarDisplay(sequenciaNumeros, false); beepNormal();
          } else { somErro(); }
        }
        while (digitalRead(pinosColunas[col]) == LOW) { delay(10); }
        digitalWrite(pinosLinhas[i_linha], HIGH); return; 
      }
    }
    digitalWrite(pinosLinhas[i_linha], HIGH);
  }
  delay(10);
}
