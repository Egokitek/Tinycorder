///////////////////////////////////////////////////////////
///                  TINYCORDER V2.0                    ///
///////////////////////////////////////////////////////////
///                    MAIN PROGRAM                     ///
///////////////////////////////////////////////////////////
///            Victor Barahona - July 2025              ///
///               www.victorbarahona.com                ///
///////////////////////////////////////////////////////////

/*  TINYCORDER: UN TRICORDER BASADO EN EL ADAFRUIT SHARP MEMORY DISPLAY DE 400x240px */

#include <Arduino.h>
#include <esp_sleep.h>
#include <Adafruit_SharpMem.h>  //Librería especifica Pantalla 400x240 de Adafruit
#include <Adafruit_AS7341.h>    //Sensor espectrometro
#include "Adafruit_GFX.h"       //Librería gráfica Adafruit
#include <SPI.h>                //La pantalla necesita bus SPI
#include <Wire.h>               //Para tareas que requieren bus I2C
#include <SensirionI2CScd4x.h>  //Libreria sensor CO2, temp, humedad


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

//Instanciar clase sensor CO2
SensirionI2CScd4x scd4x;

void printUint16Hex(uint16_t value) {
  Serial.print(value < 4096 ? "0" : "");
  Serial.print(value < 256 ? "0" : "");
  Serial.print(value < 16 ? "0" : "");
  Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
  Serial.print("Serial: 0x");
  printUint16Hex(serial0);
  printUint16Hex(serial1);
  printUint16Hex(serial2);
  Serial.println();
}

// --- PARAMETRIZACIÓN DE IDIOMA --- // --- LANGUAGE PARAMETERIZATION ---
enum Idioma { ESPANOL, INGLES }; // Idioma actual // Current language
Idioma idiomaActual = INGLES; // Cambia a ESPANOL si lo prefieres // Change to ESPANOL if you prefer
void setIdioma(Idioma idioma); // Prototipo necesario para evitar error de compilación // Prototype needed to avoid compilation error

//Botones para controlar el dispositivo // Buttons to control the device
#define BUTTON_UP 5       // Cable Rojo, pin 3, gpio 5 // Red wire, pin 3, gpio 5
#define BUTTON_DOWN 21    // Cable amarillo, pin 6, gpio 21 // Yellow wire, pin 6, gpio 21
#define BUTTON_ENTER 20   // Cable verde, pin 7, gpio 20 // Green wire, pin 7, gpio 20
#define DEBOUNCE_TIME 50  // Tiempo de debounce en ms // Debounce time in ms

unsigned long lastDebounceTimeUP = 0;
unsigned long lastDebounceTimeDOWN = 0;
unsigned long lastDebounceTimeENTER = 0;
bool lastStateUP = HIGH;
bool lastStateDOWN = HIGH;
bool lastStateENTER = HIGH;
bool currentStateUP = HIGH;
bool currentStateDOWN = HIGH;
bool currentStateENTER = HIGH;

const char *menus_es[] = { "Badge electronico", "Timer", "Dashboard Tricorder", "Calidad del aire", "Espectrometro", "Acerca de..." }; // Menú en español // Menu in Spanish
const char *menus_en[] = { "Electronic badge", "Timer", "Tricorder Dashboard", "Air quality", "Spectrometer", "About..." }; // Menú en inglés // Menu in English
const char **menus = menus_en; // Por defecto inglés // Default to English
const int totalMenus = 6;
int menuIndex = 0;
bool menuSelected = false;
unsigned long lastButtonPress = 0;

/* Definición de la clase para el sensor AS7341, espectrometro de 8 bandas mas NIR */
/* Definition of the class for the AS7341 sensor, 8-band spectrometer plus NIR */
Adafruit_AS7341 as7341;

// --- DASHBOARD: HISTÓRICO DE 24 MEDIDAS ---
#define DASHBOARD_HISTORY 24
unsigned long dashboard_interval_ms = 5UL * 60UL * 1000UL; // Intervalo de muestreo en ms (por defecto 5 minutos)
uint16_t co2_history[DASHBOARD_HISTORY] = {0};
float temp_history[DASHBOARD_HISTORY] = {0};
float hum_history[DASHBOARD_HISTORY] = {0};
uint8_t dashboard_index = 0;
uint8_t dashboard_count = 0;
unsigned long dashboard_last_update = 0;

void setup() {
  Serial.begin(115200);
  display.begin();
  Wire.begin();

  //while (!Serial) {
  //  delay(100);
  //}

  uint16_t error;
  char errorMessage[256];
  scd4x.begin(Wire);

  // stop potentially previously started measurement // Detener medición previa si existe
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
    Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  error = scd4x.getSerialNumber(serial0, serial1, serial2);
  if (error) {
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    printSerialNumber(serial0, serial1, serial2);
  }

  // Start Measurement // Iniciar medición
  error = scd4x.startPeriodicMeasurement();
  if (error) {
    Serial.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  Serial.println("Waiting for first measurement... (5 sec)"); // Esperando primera medición

  //setup de botones y su tiempo de rebote // Button setup and debounce time
  pinMode(BUTTON_UP, INPUT_PULLUP);     // Activa la resistencia pull-up interna // Enable internal pull-up resistor
  pinMode(BUTTON_DOWN, INPUT_PULLUP);   // Activa la resistencia pull-up interna // Enable internal pull-up resistor
  pinMode(BUTTON_ENTER, INPUT_PULLUP);  // Activa la resistencia pull-up interna // Enable internal pull-up resistor

  display.clearDisplay();  // clears the screen and buffer // Limpia la pantalla y el buffer

  // Several shapes are drawn centered on the screen.  Calculate 1/2 of
  // lesser of display width or height, this is used repeatedly later.
  // Varias formas se dibujan centradas en la pantalla. Calcula 1/2 del menor de ancho o alto, se usa repetidamente.
  minorHalfSize = min(display.width(), display.height()) / 2;

  if (!as7341.begin()) {
    Serial.println("Could not find AS7341"); // No se pudo encontrar el AS7341
    while (1) { delay(10); }
  }

  as7341.setATIME(100);
  as7341.setASTEP(999);
  as7341.setGain(AS7341_GAIN_256X);

  drawMenu();
}

void loop() {
  display.setRotation(0);
  showMenu();
}


void checkButtons() {
  unsigned long currentMillis = millis();

  // --- Debounce para BUTTON_UP  ---
  bool readingUP = digitalRead(BUTTON_UP);
  if (readingUP != lastStateUP) {
    lastDebounceTimeUP = currentMillis;
  }
  if ((currentMillis - lastDebounceTimeUP) > DEBOUNCE_TIME) {
    if (readingUP == LOW && currentStateUP == HIGH) {
      menuIndex = (menuIndex - 1 + totalMenus) % totalMenus;
      drawMenu();
    }
    currentStateUP = readingUP;
  }
  lastStateUP = readingUP;

  // --- Debounce para BUTTON_DOWN  ---
  bool readingDOWN = digitalRead(BUTTON_DOWN);
  if (readingDOWN != lastStateDOWN) {
    lastDebounceTimeDOWN = currentMillis;
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
    lastDebounceTimeENTER = currentMillis;
  }
  if ((currentMillis - lastDebounceTimeENTER) > DEBOUNCE_TIME) {
    if (readingENTER == LOW && currentStateENTER == HIGH) {
      executeTask(menuIndex);
      drawMenu(); // Volver al menú en la misma posición
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
  display.println("TINYCORDER V2.0");
  display.setCursor(60, 40);
  display.setTextSize(2);
  display.println(idiomaActual == ESPANOL ? "< Elige opcion >" : "< Choose option >");

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

// --- PARAMETRIZACIÓN DE IDIOMA ---
//enum Idioma { ESPANOL, INGLES };
//Idioma idiomaActual = INGLES; // Cambia a ESPANOL si lo prefieres
void setIdioma(Idioma idioma) {
  idiomaActual = idioma;
  menus = (idioma == ESPANOL) ? menus_es : menus_en;
}

void executeTask(int index) {
  display.clearDisplay();
  display.setCursor(60, 200);
  display.setTextSize(1);
  display.setTextColor(BLACK);

  switch (index) {
    case 0:
      badge();
      break;
    case 1:
      timer();
      break;
    case 2:
      dashboard();
      break;
    case 3:
      calidad_aire();
      break;
    case 4:
      espectrometro();
      break;
    case 5:
      acercade();
      break;
  }

  display.refresh();
  delay(2000);  // Mantener el mensaje 2 segundos antes de volver al menú
}



void badge() {
  display.setTextColor(BLACK);
  //Muestra Badge personal
  display.drawBitmap(0, 0, vj_personal_badge, 400, 240, WHITE, BLACK);
  display.refresh();
  esp_deep_sleep_start();

}


// --- MENÚ ESPECTRÓMETRO: SELECCIÓN DIRECTA ---
void espectrometro() {
  int modo = 0;
  bool seleccionando = true;
  // Esperar a que todos los botones estén soltados antes de mostrar el menú
  while (digitalRead(BUTTON_UP) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_ENTER) == LOW) {
    delay(10);
  }
  while (seleccionando) {
    display.clearDisplay();
    display.setTextSize(3);
    display.setCursor(60, 40);
    display.setTextColor(BLACK);
    display.println("Spectrometer:");
    display.setTextSize(2);
    display.setCursor(60, 90);
    display.print(modo == 0 ? "> " : "  "); display.println("Histogram");
    display.setCursor(60, 120);
    display.print(modo == 1 ? "> " : "  "); display.println("Curve");
    display.setCursor(60, 150);
    display.print(modo == 2 ? "> " : "  "); display.println("Data");
    display.refresh();
    // Navegación
    bool salir = false;
    while (!salir) {
      if (digitalRead(BUTTON_UP) == LOW) { modo = (modo + 2) % 3; delay(200); salir = true; }
      else if (digitalRead(BUTTON_DOWN) == LOW) { modo = (modo + 1) % 3; delay(200); salir = true; }
      else if (digitalRead(BUTTON_ENTER) == LOW) { delay(200); seleccionando = false; salir = true; }
      delay(10);
    }
    // Esperar a que se suelten todos los botones antes de permitir otra navegación
    while (digitalRead(BUTTON_UP) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_ENTER) == LOW) {
      delay(10);
    }
  }
  // Ejecutar modo seleccionado
  switch (modo) {
    case 0: espectro_histograma(); break;
    case 1: espectro_grafica(); break;
    case 2: espectro_numerico(); break;
  }
}

// --- TIMER: AJUSTE Y CUENTA ATRÁS ---
void timer() {
  int minutos = 5;
  bool ajustando = true;
  // Esperar a que todos los botones estén soltados antes de mostrar el menú
  while (digitalRead(BUTTON_UP) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_ENTER) == LOW) {
    delay(10);
  }
  while (ajustando) {
    display.clearDisplay();
    display.setTextSize(3);
    display.setCursor(80, 60);
    display.setTextColor(BLACK);
    display.print(idiomaActual == ESPANOL ? "Temporizador:" : "Timer:");
    display.setCursor(140, 120);
    display.setTextSize(5);
    display.print(minutos);
    display.setTextSize(3);
    display.print(" min");
    if (minutos == 25) {
      display.setTextSize(2);
      display.setCursor(140, 160);
      display.setTextColor(BLACK);
      display.print("Pomodoro Timer !!");
    }
    display.setTextSize(2);
    display.setCursor(80, 210);
    display.print(idiomaActual == ESPANOL ? "UP/DOWN: Ajustar  ENTER: Iniciar" : "UP/DOWN: Set  ENTER: Start");
    display.refresh();
    // Ajuste de minutos
    bool salir = false;
    while (!salir) {
      if (digitalRead(BUTTON_UP) == LOW) {
        minutos += 5;
        if (minutos > 120) minutos = 5;
        delay(200);
        salir = true;
      }
      if (digitalRead(BUTTON_DOWN) == LOW) {
        minutos -= 5;
        if (minutos < 5) minutos = 120;
        delay(200);
        salir = true;
      }
      if (digitalRead(BUTTON_ENTER) == LOW) {
        ajustando = false;
        delay(200);
        salir = true;
      }
      delay(10);
    }
    // Esperar a que se suelten todos los botones antes de permitir otra navegación
    while (digitalRead(BUTTON_UP) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_ENTER) == LOW) {
      delay(10);
    }
  }
  // Cuenta atrás sin deep sleep, solo delay y refresco
  while (minutos > 0) {
    display.clearDisplay();
    display.setTextSize(4);
    display.setCursor(150, 80);
    display.setTextColor(BLACK);
    display.print(minutos);
    display.setTextSize(2);
    display.setCursor(170, 140);
    display.print("min");
    display.setTextSize(2);
    display.setCursor(100, 180);
    display.print(idiomaActual == ESPANOL ? "Cuenta atras..." : "Counting down...");
    display.refresh();
    for (int s = 0; s < 60; s++) {
      delay(1000); // 1 segundo
      // Permitir salir con botón
      if (digitalRead(BUTTON_ENTER) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_UP) == LOW) {
        delay(200);
        return;
      }
    }
    minutos--;
  }
  // Al llegar a cero, parpadeo círculo centrado
  int circleState = 0;
  int cx = ANCHO / 2;
  int cy = ALTO / 2;
  int r = 50;
  while (true) {
    display.clearDisplay();
    if (circleState % 2 == 0) {
      display.fillCircle(cx, cy, r, BLACK);
      display.drawCircle(cx, cy, r, WHITE);
    } else {
      display.fillCircle(cx, cy, r, WHITE);
      display.drawCircle(cx, cy, r, BLACK);
    }
    display.setTextSize(3);
    display.setTextColor(circleState % 2 == 0 ? WHITE : BLACK);
    display.setCursor(cx - 40, cy - 20);
    display.print(idiomaActual == ESPANOL ? "FIN" : "END");
    display.setTextSize(2);
    display.setCursor(cx - 60, cy + 40);
    display.print(idiomaActual == ESPANOL ? "Pulsa boton" : "Press button");
    display.refresh();
    for (int t = 0; t < 100; t++) {
      if (digitalRead(BUTTON_ENTER) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_UP) == LOW) {
        delay(200);
        return;
      }
      delay(10);
    }
    circleState++;
  }
}

void dashboard() {
  // --- Nuevo dashboard: 3 histogramas, autoescalado, sin leyendas ni min/max ---
  while (true) {
    unsigned long now = millis();
    if (dashboard_count == 0 || now - dashboard_last_update >= dashboard_interval_ms) {
      uint16_t error;
      char errorMessage[256];
      uint16_t co2 = 0;
      float temperature = 0.0f;
      float humidity = 0.0f;
      bool isDataReady = false;
      error = scd4x.getDataReadyFlag(isDataReady);
      if (!error && isDataReady) {
        error = scd4x.readMeasurement(co2, temperature, humidity);
      }
      co2_history[dashboard_index] = co2;
      temp_history[dashboard_index] = temperature;
      hum_history[dashboard_index] = humidity;
      dashboard_index = (dashboard_index + 1) % DASHBOARD_HISTORY;
      if (dashboard_count < DASHBOARD_HISTORY) dashboard_count++;
      dashboard_last_update = now;
    }
    display.clearDisplay();
    // Título
    display.setTextSize(3);
    display.setTextColor(BLACK);
    display.setCursor(120, 5);
    display.print("Dashboard");
    int n = dashboard_count;
    int start = (dashboard_index + DASHBOARD_HISTORY - n) % DASHBOARD_HISTORY;
    int x0 = 70, w = 300, bar_w = 10, sep = 3;
    // --- CO2 ---
    int y0 = 60, h = 40;
    display.setTextSize(2);
    display.setCursor(10, y0 - 20);
    display.print("CO2");
    uint16_t co2_min = 10000, co2_max = 0;
    for (int i = 0; i < n; i++) {
      int idx = (start + i) % DASHBOARD_HISTORY;
      if (co2_history[idx] < co2_min) co2_min = co2_history[idx];
      if (co2_history[idx] > co2_max) co2_max = co2_history[idx];
    }
    if (co2_min == co2_max) { co2_min -= 10; co2_max += 10; }
    // Eje X
    display.drawLine(x0, y0, x0 + w, y0, BLACK);
    for (int i = 0; i < n; i++) {
      int idx = (start + i) % DASHBOARD_HISTORY;
      int bar_h = (co2_max > co2_min) ? (int)((co2_history[idx] - co2_min) * h / (co2_max - co2_min)) : 0;
      int x = x0 + i * (bar_w + sep);
      display.fillRect(x, y0 - bar_h, bar_w, bar_h, WHITE);
      display.drawRect(x, y0 - bar_h, bar_w, bar_h, BLACK);
      display.setTextSize(1);
      display.setTextColor(BLACK);
      display.setCursor(x, y0 - bar_h - 12);
      display.print(co2_history[idx]);
    }
    // --- TEMP ---
    y0 = 120; h = 40;
    display.setTextSize(2);
    display.setCursor(10, y0 - 20);
    display.print("Temp");
    float temp_min = 1000, temp_max = -1000;
    for (int i = 0; i < n; i++) {
      int idx = (start + i) % DASHBOARD_HISTORY;
      if (temp_history[idx] < temp_min) temp_min = temp_history[idx];
      if (temp_history[idx] > temp_max) temp_max = temp_history[idx];
    }
    if (temp_min == temp_max) { temp_min -= 1; temp_max += 1; }
    display.drawLine(x0, y0, x0 + w, y0, BLACK);
    for (int i = 0; i < n; i++) {
      int idx = (start + i) % DASHBOARD_HISTORY;
      int bar_h = (temp_max > temp_min) ? (int)((temp_history[idx] - temp_min) * h / (temp_max - temp_min)) : 0;
      int x = x0 + i * (bar_w + sep);
      display.fillRect(x, y0 - bar_h, bar_w, bar_h, WHITE);
      display.drawRect(x, y0 - bar_h, bar_w, bar_h, BLACK);
      display.setTextSize(1);
      display.setTextColor(BLACK);
      display.setCursor(x, y0 - bar_h - 12);
      display.print(temp_history[idx], 1);
    }
    // --- HUM ---
    y0 = 180; h = 40;
    display.setTextSize(2);
    display.setCursor(10, y0 - 20);
    display.print("Hum");
    float hum_min = 1000, hum_max = -1000;
    for (int i = 0; i < n; i++) {
      int idx = (start + i) % DASHBOARD_HISTORY;
      if (hum_history[idx] < hum_min) hum_min = hum_history[idx];
      if (hum_history[idx] > hum_max) hum_max = hum_history[idx];
    }
    if (hum_min == hum_max) { hum_min -= 1; hum_max += 1; }
    display.drawLine(x0, y0, x0 + w, y0, BLACK);
    for (int i = 0; i < n; i++) {
      int idx = (start + i) % DASHBOARD_HISTORY;
      int bar_h = (hum_max > hum_min) ? (int)((hum_history[idx] - hum_min) * h / (hum_max - hum_min)) : 0;
      int x = x0 + i * (bar_w + sep);
      display.fillRect(x, y0 - bar_h, bar_w, bar_h, WHITE);
      display.drawRect(x, y0 - bar_h, bar_w, bar_h, BLACK);
      display.setTextSize(1);
      display.setTextColor(BLACK);
      display.setCursor(x, y0 - bar_h - 12);
      display.print(hum_history[idx], 1);
    }
    display.setTextSize(2);
    display.setCursor(10, 200);
    display.print("Samples every 5 min, 24 records");
    display.refresh();
    // Salir si se pulsa cualquier botón
    for (int t = 0; t < 300; t++) {
      if (digitalRead(BUTTON_ENTER) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_UP) == LOW) {
        delay(200);
        return;
      }
      delay(10);
    }
  }
}

// --- CALIDAD DEL AIRE: VALORES EN TIEMPO REAL ---
void calidad_aire() {
  // Muestra valores en tiempo real cada 5 segundos
  while (true) {
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    bool isDataReady = false;
    uint16_t error = scd4x.getDataReadyFlag(isDataReady);
    if (!error && isDataReady) {
      error = scd4x.readMeasurement(co2, temperature, humidity);
    }
    display.clearDisplay();
    // Marco general
    display.drawRect(40, 20, 320, 200, BLACK);
    // Marco para título
    display.fillRect(60, 30, 280, 40, WHITE);
    display.drawRect(60, 30, 280, 40, BLACK);
    display.setTextSize(3);
    display.setCursor(70, 38);
    display.setTextColor(BLACK);
    display.print(idiomaActual == ESPANOL ? "Calidad del aire" : "Air quality");
    // Marco para CO2
    display.drawRect(60, 85, 280, 35, BLACK);
    display.setTextSize(2);
    display.setCursor(80, 92);
    display.print("CO2: "); display.print(co2); display.print(" ppm");
    // Marco para Temp
    display.drawRect(60, 125, 280, 35, BLACK);
    display.setCursor(80, 132);
    display.print(idiomaActual == ESPANOL ? "Temp: " : "Temp: "); display.print(temperature, 1); display.print("C");
    // Marco para Humedad
    display.drawRect(60, 165, 280, 35, BLACK);
    display.setCursor(80, 172);
    display.print(idiomaActual == ESPANOL ? "Humedad: " : "Hum: "); display.print(humidity, 1); display.print("%");
    display.refresh();
    // Espera 5 segundos o hasta pulsación NUEVA de botón
    unsigned long t0 = millis();
    bool botonSoltado = false;
    // Esperar a que todos los botones estén soltados antes de empezar a contar
    while (digitalRead(BUTTON_ENTER) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_UP) == LOW) {
      delay(10);
    }
    while (millis() - t0 < 5000) {
      if (digitalRead(BUTTON_ENTER) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_UP) == LOW) {
        delay(200);
        return;
      }
      delay(10);
    }
  }
}

void showMenu() {
  while (true) {
    drawMenu();
    while (true) {
      unsigned long currentMillis = millis();
      // --- Debounce para BUTTON_UP  ---
      bool readingUP = digitalRead(BUTTON_UP);
      if (readingUP != lastStateUP) {
        lastDebounceTimeUP = currentMillis;
      }
      if ((currentMillis - lastDebounceTimeUP) > DEBOUNCE_TIME) {
        if (readingUP == LOW && currentStateUP == HIGH) {
          menuIndex = (menuIndex - 1 + totalMenus) % totalMenus;
          break; // Redibujar menú
        }
        currentStateUP = readingUP;
      }
      lastStateUP = readingUP;
      // --- Debounce para BUTTON_DOWN  ---
      bool readingDOWN = digitalRead(BUTTON_DOWN);
      if (readingDOWN != lastStateDOWN) {
        lastDebounceTimeDOWN = currentMillis;
      }
      if ((currentMillis - lastDebounceTimeDOWN) > DEBOUNCE_TIME) {
        if (readingDOWN == LOW && currentStateDOWN == HIGH) {
          menuIndex = (menuIndex + 1) % totalMenus;
          break; // Redibujar menú
        }
        currentStateDOWN = readingDOWN;
      }
      lastStateDOWN = readingDOWN;
      // --- Debounce para BUTTON_ENTER  ---
      bool readingENTER = digitalRead(BUTTON_ENTER);
      if (readingENTER != lastStateENTER) {
        lastDebounceTimeENTER = currentMillis;
      }
      if ((currentMillis - lastDebounceTimeENTER) > DEBOUNCE_TIME) {
        if (readingENTER == LOW && currentStateENTER == HIGH) {
          executeTask(menuIndex);
          return; // Salir del menú tras ejecutar
        }
        currentStateENTER = readingENTER;
      }
      lastStateENTER = readingENTER;
      delay(10);
    }
  }
}

// --- ACERCA DE... ---
void acercade() {
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(60, 20);
  display.setTextColor(BLACK);
  display.print("TINYCORDER V2.0");
  // Texto informativo
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(10, 60);
  display.print("by Victor Barahona - July 2025");
  display.setCursor(10, 80);
  display.print("www.victorbarahona.com");
  display.setCursor(10, 120);
  display.print("MCU: SEEEDSTUDIO XIAO ESP32-C3");
  display.setCursor(10, 140);
  display.print("Display:  Adafruit Sharp 400x240");
  display.setCursor(10, 180);
  display.print("CO2, Temperature, Humidity");
  display.setCursor(10, 200);
  display.print("VIS-NIR Spectrometer");
  display.refresh();
  delay(3000);
  
}

// --- ESPECTRÓMETRO: HISTOGRAMA ---
void espectro_histograma() {
  while (true) {
    if (!as7341.readAllChannels()) {
      Serial.println("Error reading all channels!");
      return;
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(3, 3);
    display.setTextColor(WHITE, BLACK);
    display.println(" VIS-NIR Spectrum: Histogram ");
    display.setTextColor(BLACK);
    uint16_t valores[9] = {
      as7341.getChannel(AS7341_CHANNEL_415nm_F1),
      as7341.getChannel(AS7341_CHANNEL_445nm_F2),
      as7341.getChannel(AS7341_CHANNEL_480nm_F3),
      as7341.getChannel(AS7341_CHANNEL_515nm_F4),
      as7341.getChannel(AS7341_CHANNEL_555nm_F5),
      as7341.getChannel(AS7341_CHANNEL_590nm_F6),
      as7341.getChannel(AS7341_CHANNEL_630nm_F7),
      as7341.getChannel(AS7341_CHANNEL_680nm_F8),
      as7341.getChannel(AS7341_CHANNEL_NIR)
    };
    uint16_t maxVal = 1;
    for (int i = 0; i < 9; i++) {
      if (valores[i] > maxVal) maxVal = valores[i];
    }
    float escala = maxVal > 0 ? 120.0f / maxVal : 1.0f;
    int x0 = 5;
    int ancho = 39;
    int sep = 42;
    int y_base = 220;
    const char* etiquetas[9] = {"VIO","BLU","CYA","GRE","YEL","ORA","R-O","RED","NIR"};
    for (int i = 0; i < 8; i++) {
      int altura = -(int)(valores[i] * escala);
      display.setCursor(x0 + i * sep, 225);
      display.print(etiquetas[i]);
      display.drawRect(x0 + i * sep, y_base, ancho, altura, BLACK);
      display.setTextSize(2);
      display.setCursor(x0 + i * sep, y_base + altura - 28);
      display.print(valores[i]);
      display.setTextSize(2);
    }
    display.setTextSize(2);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(350, 50);
    display.print("NIR");
    display.setCursor(350, 70);
    display.setTextSize(2);
    display.print(valores[8]);
    display.setTextSize(2);
    display.setTextColor(BLACK);
    display.refresh();
    // Esperar pulsación para salir o refrescar cada 5s
    unsigned long t0 = millis();
    while (millis() - t0 < 5000) {
      if (digitalRead(BUTTON_ENTER) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_UP) == LOW) {
        delay(200);
        return;
      }
      delay(10);
    }
    // Se repite el bucle para refrescar la lectura
  }
}

// --- ESPECTRÓMETRO: CURVA ---
void espectro_grafica() {
  while (true) {
    if (!as7341.readAllChannels()) {
      Serial.println("Error reading all channels!");
      return;
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(3, 3);
    display.setTextColor(WHITE, BLACK);
    display.println("VIS-NIR Spectrum: Curve");
    uint16_t valores[9] = {
      as7341.getChannel(AS7341_CHANNEL_415nm_F1),
      as7341.getChannel(AS7341_CHANNEL_445nm_F2),
      as7341.getChannel(AS7341_CHANNEL_480nm_F3),
      as7341.getChannel(AS7341_CHANNEL_515nm_F4),
      as7341.getChannel(AS7341_CHANNEL_555nm_F5),
      as7341.getChannel(AS7341_CHANNEL_590nm_F6),
      as7341.getChannel(AS7341_CHANNEL_630nm_F7),
      as7341.getChannel(AS7341_CHANNEL_680nm_F8),
      as7341.getChannel(AS7341_CHANNEL_NIR)
    };
    uint16_t maxVal = 1;
    for (int i = 0; i < 9; i++) {
      if (valores[i] > maxVal) maxVal = valores[i];
    }
    float escala = maxVal > 0 ? 120.0f / maxVal : 1.0f;
    int x[] = {20, 60, 100, 140, 180, 220, 260, 300, 340};
    int y_base = 180;
    for (int i = 0; i < 8; i++) {
      int y1 = y_base - (int)(valores[i] * escala);
      int y2 = y_base - (int)(valores[i+1] * escala);
      display.drawLine(x[i], y1, x[i+1], y2, BLACK);
      display.fillCircle(x[i], y1, 2, BLACK);
      display.setTextSize(2);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(x[i] - 10, y1 - 25);
      display.print(valores[i]);
      display.setTextColor(BLACK);
      display.setTextSize(2);
    }
    display.fillCircle(x[8], y_base - (int)(valores[8] * escala), 2, BLACK);
    display.setTextSize(2);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(350, 50);
    display.print("NIR");
    display.setCursor(350, 70);
    display.setTextSize(2);
    display.print(valores[8]);
    display.setTextSize(2);
    display.setTextColor(BLACK);
    const char* etiquetas[9] = {"VIO","BLU","CYA","GRE","YEL","ORA","R-O","RED","NIR"};
    display.setTextSize(2);
    for (int i = 0; i < 9; i++) {
      display.setCursor(x[i] - 10, 210);
      display.print(etiquetas[i]);
    }
    display.refresh();
    unsigned long t0 = millis();
    while (millis() - t0 < 5000) {
      if (digitalRead(BUTTON_ENTER) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_UP) == LOW) {
        delay(200);
        return;
      }
      delay(10);
    }
    // Se repite el bucle para refrescar la lectura
  }
}

// --- ESPECTRÓMETRO: NUMÉRICO ---
void espectro_numerico() {
  while (true) {
    if (!as7341.readAllChannels()) {
      Serial.println("Error reading all channels!");
      return;
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(3, 3);
    display.setTextColor(WHITE, BLACK);
    display.println("VIS-NIR Spectrum: Data");
    display.setTextSize(2);
    display.setTextColor(BLACK);
    const char* etiquetas[9] = {
      "F1 VIOLET 415nm:    ", "F2 BLUE 445nm:      ", "F3 CYAN 480nm:      ", "F4 GREEN 515nm:     ",
      "F5 YELLOW 555nm:    ", "F6 ORANGE 590nm:    ", "F7 RED-ORANGE 630nm:", "F8 RED 680nm:       ", "NIR 850nm:          "
    };
    uint16_t valores[9] = {
      as7341.getChannel(AS7341_CHANNEL_415nm_F1),
      as7341.getChannel(AS7341_CHANNEL_445nm_F2),
      as7341.getChannel(AS7341_CHANNEL_480nm_F3),
      as7341.getChannel(AS7341_CHANNEL_515nm_F4),
      as7341.getChannel(AS7341_CHANNEL_555nm_F5),
      as7341.getChannel(AS7341_CHANNEL_590nm_F6),
      as7341.getChannel(AS7341_CHANNEL_630nm_F7),
      as7341.getChannel(AS7341_CHANNEL_680nm_F8),
      as7341.getChannel(AS7341_CHANNEL_NIR)
    };
    for (int i = 0; i < 9; i++) {
      display.setCursor(30, 30 + i * 22);
      display.print(etiquetas[i]);
      display.print(" ");
      display.println(valores[i]);
    }
    display.refresh();
    unsigned long t0 = millis();
    while (millis() - t0 < 5000) {
      if (digitalRead(BUTTON_ENTER) == LOW || digitalRead(BUTTON_DOWN) == LOW || digitalRead(BUTTON_UP) == LOW) {
        delay(200);
        return;
      }
      delay(10);
    }
    // Se repite el bucle para refrescar la lectura
  }
}


/// END OF FILE ///

