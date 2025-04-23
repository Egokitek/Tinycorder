///////////////////////////////////////////////////////////
///                  TINYCORDER V1.0                    ///
///////////////////////////////////////////////////////////
///                  PROGRAMA PRINCIPAL                 ///
///////////////////////////////////////////////////////////
///           Victor Barahona - Diciembre 2024          ///
///                www.victorbarahona.com               ///
///////////////////////////////////////////////////////////

/*  TINYCORDER: UN TRICORDER BASADO EN EL ADAFRUIT SHARP MEMORY DISPLAY DE 400x240px */

#include <Arduino.h>
#include <Adafruit_SharpMem.h>  //Librería especifica Pantalla 400x240 de Adafruit
#include <Adafruit_AS7341.h>    //Sensor espectrometro
#include "Adafruit_GFX.h"       //Librería gráfica Adafruit
#include <SPI.h>                //La pantalla necesita bus SPI
#include <Wire.h>               //Para tareas que requieren bus I2C
#include "esp_sleep.h"          //Para mandar a dormir el MCU y ahorrar energía

#include "Badges.h"  //Contiene los badges que voy implementando de forma personalizada

// any pins can be used
#define SHARP_SCK 8
#define SHARP_MOSI 9
#define SHARP_SS 10

#define ANCHO 400
#define ALTO 240
#define BLACK 0
#define WHITE 1

//Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 144, 168);
Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, ANCHO, ALTO);

int minorHalfSize;  // 1/2 of lesser of display width or height

//Botones para controlar el dispositivo
#define BUTTON_UP 5       // Cable Rojo, pin 3, gpio 5
#define BUTTON_DOWN 21    // Cable amarillo, pin 6, gpio 21
#define BUTTON_ENTER 20   // Cable verde, pin 7, gpio 20
#define DEBOUNCE_TIME 50  // Tiempo de debounce en ms

unsigned long lastDebounceTimeUP = 0;
unsigned long lastDebounceTimeDOWN = 0;
unsigned long lastDebounceTimeENTER = 0;
bool lastStateUP = HIGH;
bool lastStateDOWN = HIGH;
bool lastStateENTER = HIGH;
bool currentStateUP = HIGH;
bool currentStateDOWN = HIGH;
bool currentStateENTER = HIGH;

const char *menus[] = { "Badge electronico", "Reloj / Timer", "Dashboard Tricorder", "Calidad del aire", "Espectrometro", "Osciloscopio", "Acerca de..." };
//const int totalMenus = sizeof(menus) / sizeof(menus[0]);
const int totalMenus = 7;
int menuIndex = 0;
bool menuSelected = false;
unsigned long lastButtonPress = 0;

/* Definición de la clase para el sensor AS7341, espectrometro de 8 bandas mas NIR */
Adafruit_AS7341 as7341;

//void spectro_numerico();
void spectro_histograma();
//void spectro_grafica();

void setup() {
  Serial.begin(115200);
  display.begin();

  //setup de botones y su tiempo de rebote
  pinMode(BUTTON_UP, INPUT_PULLUP);     // Activa la resistencia pull-up interna
  pinMode(BUTTON_DOWN, INPUT_PULLUP);   // Activa la resistencia pull-up interna
  pinMode(BUTTON_ENTER, INPUT_PULLUP);  // Activa la resistencia pull-up interna

  display.clearDisplay();  // clears the screen and buffer

  // Several shapes are drawn centered on the screen.  Calculate 1/2 of
  // lesser of display width or height, this is used repeatedly later.
  minorHalfSize = min(display.width(), display.height()) / 2;

  if (!as7341.begin()) {
    Serial.println("Could not find AS7341");
    while (1) { delay(10); }
  }

  as7341.setATIME(100);
  as7341.setASTEP(999);
  as7341.setGain(AS7341_GAIN_256X);

  drawMenu();
}

void loop() {

  display.setRotation(0);
 
  checkButtons();
  
}


void checkButtons() {
  unsigned long currentMillis = millis();

  // --- Debounce para BUTTON_UP  ---
  bool readingUP = digitalRead(BUTTON_UP);
  if (readingUP != lastStateUP) {
    lastDebounceTimeUP = currentMillis;  // Reiniciar contador de debounce
  }
  if ((currentMillis - lastDebounceTimeUP) > DEBOUNCE_TIME) {
    if (readingUP == LOW && currentStateUP == HIGH) {
      menuIndex = (menuIndex -1 + totalMenus) % totalMenus;
      drawMenu();
    }
    currentStateUP = readingUP;
  }
  lastStateUP = readingUP;

  // --- Debounce para BUTTON_DOWN  ---
  bool readingDOWN = digitalRead(BUTTON_DOWN);
  if (readingDOWN != lastStateDOWN) {
    lastDebounceTimeDOWN = currentMillis;  // Reiniciar contador de debounce
  }
  if ((currentMillis - lastDebounceTimeDOWN) > DEBOUNCE_TIME) {
    if (readingDOWN == LOW && currentStateDOWN == HIGH) {
      menuIndex = (menuIndex + 1) % totalMenus;
      drawMenu();
    }
    currentStateDOWN = readingDOWN;
  }
  lastStateDOWN = readingDOWN;

  // --- Debounce para BUTTON_ENTER  ---
  bool readingENTER = digitalRead(BUTTON_ENTER);
  if (readingENTER != lastStateENTER) {
    lastDebounceTimeENTER = currentMillis;  // Reiniciar contador de debounce
  }
  if ((currentMillis - lastDebounceTimeENTER) > DEBOUNCE_TIME) {
    if (readingENTER == LOW && currentStateENTER == HIGH) {
      menuIndex = (menuIndex + 1) % totalMenus;
      executeTask(menuIndex-1);
    }
    currentStateENTER = readingENTER;
  }
  lastStateENTER = readingENTER;
}

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(BLACK);
  display.setCursor(60, 10);
  display.println("TINYCORDER V1.0");
  display.setCursor(60, 40);
  display.setTextSize(2);
  display.println("< Elige opcion >");

  for (int i = 0; i < totalMenus; i++) {
    display.setCursor(60, 70 + (i * 18));  // Espaciado vertical entre opciones

    if (i == menuIndex) {
      display.print("* ");  // Asterisco para la opción seleccionada
    } else {
      display.print("  ");  // Espacio en las demás opciones
    }

    display.println(menus[i]);  // Mostrar nombre del menú
  }

  display.refresh();
  delay(50);
}

void executeTask(int index) {
  display.clearDisplay();
  display.setCursor(60, 200);
  display.setTextSize(1);
  display.setTextColor(BLACK);

  switch (index) {
    case 0:
      //display.println("Badge electrónico");
      badge();
      break;
    case 1:
      //display.println("Reloj / Timer");
      reloj();
      break;
    case 2:
      //display.println("Dashboard Tricorder");
      dashboard();
      break;
    case 3:
      //display.println("Espectrómetro");
      espectrometro();
      break;
    case 4:
      //display.println("Calidad del aire");
      calidad_aire();
      break;
    case 5:
      //display.println("Osciloscopio");
      osciloscopio();
      break;
    case 6:
      //display.println("Acerca de:");
      acercade();
      break;
  }

  display.refresh();
  delay(500);  // Mantener el mensaje 0.5 segundos antes de volver al menú
}

// Funciones para cada menú (simuladas)
void badge() {
  display.setTextColor(BLACK);
  //Muestra Badge personal
  display.drawBitmap(0, 0, vj_personal_badge, 400, 240, WHITE, BLACK);
  display.refresh();
  // Entra en deep sleep
  esp_deep_sleep_start(); //Asi solo salimos con apagado/encendido, hay que poner interrup por gpio o similar

}
void reloj() {
  display.setTextSize(3);
  display.setCursor(60, 150);
  display.println("Timer iniciado");
  display.refresh();
}
void dashboard() {
  display.setTextSize(3);
  display.setCursor(60, 150);
  display.println("Dashboard iniciado");
  display.refresh();
}
void espectrometro() {
  //display.println("Espectrometro iniciado");
  while (true) {
    spectro_histograma(); 
  }

}
void calidad_aire() {
  display.setTextSize(3);
  display.setCursor(60, 150);
  display.println("Calidad del Aire iniciado");
  display.refresh();
}
void osciloscopio() {
  display.setTextSize(3);
  display.setCursor(60, 150);
  display.println("Osciloscopio iniciado");
  display.refresh();
}

void acercade() {
  display.setTextSize(2);
  display.setCursor(60, 100);
  display.println("Acerca de:");
  //display.setTextSize(2);
  display.setCursor(60, 140);
  display.println("www.victorbarahona.com");
  //display.setCursor(60, 180);
  //display.print("Unikare");
  display.refresh();
}

void spectro_numerico() {

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(3, 3);
  display.setTextColor(WHITE, BLACK);
  display.println("  Espectrometro 8 bandas + NIR   ");
 
  //while (true) {
  // Print out the stored values for each channel
  // Read all channels at the same time and store in as7341 object
  if (!as7341.readAllChannels()) {
    Serial.println("Error reading all channels!");
    return;
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(3, 3);
  display.setTextColor(WHITE, BLACK);
  display.println("  Espectrometro 8 bandas + NIR   ");
  display.setTextSize(2);
  display.setCursor(30, 30);
  display.setTextColor(BLACK);

  display.print("F1 VIOLETA 415nm:        ");
  display.println(as7341.getChannel(AS7341_CHANNEL_415nm_F1));
  display.setCursor(30, 50);
  display.print("F2 AZUL 445nm:           ");
  display.println(as7341.getChannel(AS7341_CHANNEL_445nm_F2));
  display.setCursor(30, 70);
  display.print("F3 CYAN 480nm:           ");
  display.println(as7341.getChannel(AS7341_CHANNEL_480nm_F3));
  display.setCursor(30, 90);
  display.print("F4 VERDE 515nm:          ");
  display.println(as7341.getChannel(AS7341_CHANNEL_515nm_F4));
  display.setCursor(30, 110);
  display.print("F5 AMARILLO 555nm:       ");
  display.println(as7341.getChannel(AS7341_CHANNEL_555nm_F5));
  display.setCursor(30, 130);
  display.print("F6 NARANJA 590nm:        ");
  display.println(as7341.getChannel(AS7341_CHANNEL_590nm_F6));
  display.setCursor(30, 150);
  display.print("F7 ROJO-NARANJA 630nm:   ");
  display.println(as7341.getChannel(AS7341_CHANNEL_630nm_F7));
  display.setCursor(30, 170);
  display.print("F8 ROJO 680nm:           ");
  display.println(as7341.getChannel(AS7341_CHANNEL_680nm_F8));
  display.setCursor(30, 190);
  display.print("MEDIA TOTAL ESPECTRO:    ");
  display.println(as7341.getChannel(AS7341_CHANNEL_CLEAR));
  display.setCursor(30, 210);
  display.print("NIR, INFRARROJO CERCANO: ");
  display.println(as7341.getChannel(AS7341_CHANNEL_NIR));

  display.refresh();
  delay(3000);
  //}
}

//Espectroscopio con valores de histograma
void spectro_histograma() {
  //while (true) {
  // Print out the stored values for each channel
  // Read all channels at the same time and store in as7341 object
  if (!as7341.readAllChannels()) {
    Serial.println("Error reading all channels!");
    return;
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(3, 3);
  display.setTextColor(WHITE, BLACK);
  display.println("   Histograma Espectro VIS-NIR   ");
  display.setCursor(5, 225);
  display.setTextColor(BLACK);

  display.print("415");
  display.drawRect(5, 220, 39, -as7341.getChannel(AS7341_CHANNEL_415nm_F1) / 10, BLACK);

  display.setCursor(45, 225);
  display.print("445");
  display.drawRect(45, 220, 39, -as7341.getChannel(AS7341_CHANNEL_445nm_F2) / 10, BLACK);

  display.setCursor(85, 225);
  display.print("480");
  display.drawRect(85, 220, 39, -as7341.getChannel(AS7341_CHANNEL_480nm_F3) / 10, BLACK);

  display.setCursor(125, 225);
  display.print("515");
  display.drawRect(125, 220, 39, -as7341.getChannel(AS7341_CHANNEL_515nm_F4) / 10, BLACK);

  display.setCursor(165, 225);
  display.print("555");
  display.drawRect(165, 220, 39, -as7341.getChannel(AS7341_CHANNEL_555nm_F5) / 10, BLACK);

  display.setCursor(205, 225);
  display.print("590");
  display.drawRect(205, 220, 39, -as7341.getChannel(AS7341_CHANNEL_590nm_F6) / 10, BLACK);

  display.setCursor(245, 225);
  display.print("630");
  display.drawRect(245, 220, 39, -as7341.getChannel(AS7341_CHANNEL_630nm_F7) / 10, BLACK);

  display.setCursor(285, 225);
  display.print("680");
  display.drawRect(285, 220, 39, -as7341.getChannel(AS7341_CHANNEL_680nm_F8) / 10, BLACK);

  display.setCursor(325, 225);
  display.print("NIR");
  display.fillRect(325, 220, 39, -as7341.getChannel(AS7341_CHANNEL_NIR) / 10, BLACK);

  display.refresh();
  delay(3000);
  //}
}

//Espectroscopio con valores de curva
void spectro_grafica() {
}


/// END OF FILE ///
