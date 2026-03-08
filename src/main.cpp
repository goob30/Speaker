/*
  TODO: Valid track checking (MP3, WAV, OGG, FLAC),
  visual effects (flashing period on no track screen)
  Add setting to either automatically go to Clock screen after n seconds on player screen or not
*/ 
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>

uint8_t SD_CS = 5;
uint8_t SDA_PIN   = 21;
uint8_t SCL_PIN   = 22;
uint8_t ROTARY_A  = 27;
uint8_t ROTARY_B  = 14;
uint8_t ROTARY_SW = 12;
uint8_t MODE_SW   = 16;
unsigned long lastModePress = 0;
unsigned long lastRotaryPress = 0;
const int debounceDelay = 200;

bool lastModeState = HIGH;
bool lastRotaryState = HIGH;

bool isSongPaused = true;

int playerScreenScrollX = 0;
int playerScreenScrollDirection = -1;
unsigned long lastPlayerScroll = 0;
const unsigned long PLAYER_SCROLL_INTERVAL = 100; //ms
const int PLAYER_TEXT_PADDING = 10;

const int MAX_TRACKS = 100;
bool trackListFound = true;
String tracks[MAX_TRACKS];
int trackCount = 0;
int selectedTrackIndex = 0;

const int TRACKS_PER_PAGE = 5;

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

bool isSupportedAudioFile(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".mp3");
}

void loadTracksFromSD() {
  trackCount = 0;

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("Failed to open root");
    trackListFound = false;
    return;
  }

  File file = root.openNextFile();

  while (file && trackCount < MAX_TRACKS) {
    if (!file.isDirectory()) {
      String name = String(file.name());

      if (isSupportedAudioFile(name)) {
        tracks[trackCount] = name;
        Serial.print("Indexed ");
        Serial.println(tracks[trackCount]);
        trackCount++;
      }
    }

    file = root.openNextFile();
  }

  root.close();
}

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

  if (!trackListFound || selectedTrackIndex < 0 || selectedTrackIndex >= trackCount) {
    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(2, 32, "No track selected");
    return;
  }

  display.setFont(u8g2_font_ncenB08_tr);

  const char* track = tracks[selectedTrackIndex].c_str();
  int trackTextWidth = display.getStrWidth(track);

  const char* statusText = isSongPaused ? "Paused" : "Playing";
  int statusTextWidth = display.getStrWidth(statusText);

  int screenWidth = 128;
  int xTrack = (screenWidth - trackTextWidth) / 2;
  int xStatus = (screenWidth - statusTextWidth) / 2;

  if (xTrack < 0) {
    xTrack = 0;
  }

  display.drawStr(xTrack, 32, track);
  display.drawStr(xStatus, 50, statusText);
}


void drawTracksScreen() {
  display.setFont(u8g2_font_u8glib_4_tf);
  display.drawStr(0, 10, "Tracks");
  display.drawHLine(0, 14, 128);
  display.setFont(u8g2_font_5x7_tr);

  if (!trackListFound || trackCount == 0) {
    display.drawStr(2, 25, "No tracks found.");
    return;
  }

  if (counter < 0) counter = 0;
  if (counter >= trackCount) counter = trackCount - 1;

  int scrollOffset = counter - TRACKS_PER_PAGE / 2;

  if (scrollOffset < 0) {
    scrollOffset = 0;
  }

  if (scrollOffset > trackCount - TRACKS_PER_PAGE) {
    scrollOffset = trackCount - TRACKS_PER_PAGE;
  }

  if (scrollOffset < 0) {
    scrollOffset = 0;
  }

  for (int i = 0; i < TRACKS_PER_PAGE; i++) {
    int trackIndex = scrollOffset + i;
    if (trackIndex >= trackCount) {
      break;
    }

    int y = 24 + (i * 8);
    const char* trackName = tracks[trackIndex].c_str();

    if (trackIndex == counter) {
      display.drawBox(0, y - 7, 128, 9);
      display.setDrawColor(0);
      display.drawStr(2, y, trackName);
      display.setDrawColor(1);
    } else {
      display.drawStr(2, y, trackName);
    }
  }
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

void rotaryPressed() {
  switch (currentScreen) {
    case CLOCK:
      break;
    case PLAYER:
      isSongPaused = !isSongPaused;
      needsRedraw = true;
      break;
    case TRACKS:
      if (trackListFound) {
        selectedTrackIndex = counter;
        currentScreen = PLAYER;
        needsRedraw = true;
        break;
      }
    case SETTINGS:
      break;
    default:
      break;
  }
}

void handleButtons() {
  bool modeState = digitalRead(MODE_SW);
  bool rotaryState = digitalRead(ROTARY_SW);

  unsigned long now = millis();

  // MODE button (change screen)
  if (modeState == LOW && lastModeState == HIGH && (now - lastModePress > debounceDelay)) {
    nextScreen();
    lastModePress = now;
  }

  // ROTARY button (reset counter)
  if (rotaryState == LOW && lastRotaryState == HIGH && (now - lastRotaryPress > debounceDelay)) {
    needsRedraw = true;
    lastRotaryPress = now;
    rotaryPressed();
  }

  lastModeState = modeState;
  lastRotaryState = rotaryState;
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

  SPI.begin(18, 19, 23, SD_CS);
  SPI.setDataMode(SPI_MODE0);

  if (!SD.begin(SD_CS, SPI, 1000000)) {
    Serial.println("SD failed");
  } else {
    Serial.println("SD OK");
    loadTracksFromSD();
  }
}

void loop() {

  handleEncoder();
  handleButtons();

  if (needsRedraw) {
    drawCurrentScreen();
  }
}