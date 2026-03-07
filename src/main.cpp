#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

uint8_t SDA_PIN   = 21;
uint8_t SCL_PIN   = 22;
uint8_t ROTARY_A  = 27;
uint8_t ROTARY_B  = 14;
uint8_t ROTARY_SW = 12;
uint8_t MODE_SW   = 16;

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

enum Screen : uint8_t {
  CLOCK,
  PLAYER,
  TRACKS,
  SETTINGS
};

Screen currentScreen = CLOCK;

int counter = 0;
int lastEncoderAState = HIGH;
bool needsRedraw = true;

void drawClockScreen() {
  display.setFont(u8g2_font_u8glib_4_tf);
  display.drawStr(0, 10, "Clock");

  display.drawHLine(0, 14, 128);

  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 32, "Count:");

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d", counter);
  display.drawStr(60, 32, buffer);

  display.setFont(u8g2_font_5x7_tr);
  display.drawStr(0, 50, "Press to reset");
}

void drawPlayerScreen() {
  display.setFont(u8g2_font_u8glib_4_tf);
  display.drawStr(0, 10, "Player");

  display.drawHLine(0, 14, 128);

  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 32, "Player screen");
}

void drawTracksScreen() {
  display.setFont(u8g2_font_u8glib_4_tf);
  display.drawStr(0, 10, "Tracks");

  display.drawHLine(0, 14, 128);

  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 32, "Track list");
}

void drawSettingsScreen() {
  display.setFont(u8g2_font_u8glib_4_tf);
  display.drawStr(0, 10, "Settings");

  display.drawHLine(0, 14, 128);

  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 32, "Settings menu");
}

void drawCurrentScreen() {
  display.clearBuffer();

  switch (currentScreen) {
    case CLOCK:    drawClockScreen(); break;
    case PLAYER:   drawPlayerScreen(); break;
    case TRACKS:   drawTracksScreen(); break;
    case SETTINGS: drawSettingsScreen(); break;
  }

  display.sendBuffer();
  needsRedraw = false;
}

void nextScreen() {
  currentScreen = static_cast<Screen>((currentScreen + 1) % 4);
  needsRedraw = true;
}

void handleEncoder() {
  int encoderAState = digitalRead(ROTARY_A);

  if (lastEncoderAState == HIGH && encoderAState == LOW) {

    if (digitalRead(ROTARY_B) != encoderAState)
      counter++;
    else
      counter--;

    Serial.print("Position: ");
    Serial.println(counter);

    needsRedraw = true;
  }

  lastEncoderAState = encoderAState;
}

void handleButtons() {
  static bool modeHandled = false;
  static bool rotaryHandled = false;

  bool modeState = digitalRead(MODE_SW);
  bool rotaryState = digitalRead(ROTARY_SW);

  if (modeState == LOW && !modeHandled) {
    nextScreen();
    modeHandled = true;
  }
  if (modeState == HIGH) {
    modeHandled = false;
  }

  if (rotaryState == LOW && !rotaryHandled) {
    counter = 0;
    needsRedraw = true;
    rotaryHandled = true;
  }
  if (rotaryState == HIGH) {
    rotaryHandled = false;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ROTARY_A, INPUT_PULLUP);
  pinMode(ROTARY_B, INPUT_PULLUP);
  pinMode(ROTARY_SW, INPUT_PULLUP);
  pinMode(MODE_SW, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  display.begin();

  lastEncoderAState = digitalRead(ROTARY_A);
  needsRedraw = true;
}

void loop() {

  handleEncoder();
  handleButtons();

  if (needsRedraw) {
    drawCurrentScreen();
  }
}