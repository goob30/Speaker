/*
  TODO: Valid track checking (MP3, WAV, OGG, FLAC),
  visual effects (flashing period on no track screen)
  Add setting to either automatically go to Clock screen after n seconds on player screen or not
*/ 
#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>

const int SCREEN_WIDTH = 160;
const int SCREEN_HEIGHT = 128;

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
const unsigned long PLAYER_SCROLL_INTERVAL = 80; //ms
const int PLAYER_TEXT_PADDING = 10;

const int MAX_TRACKS = 100;
bool trackListFound = true;
String tracks[MAX_TRACKS];
int trackCount = 0;
int selectedTrackIndex = 0;

const int TRACKS_PER_PAGE = 5;

const unsigned long PLAYER_SCROLL_PAUSE = 900; //ms
unsigned long playerScrollPauseUntil = 0;

TFT_eSPI display = TFT_eSPI();

bool isSupportedAudioFile(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".mp3");
}

String getDisplayTrackName(const String& name) {
  String displayName = name;

  int slashIndex = displayName.lastIndexOf('/');
  if (slashIndex >= 0) {
    displayName = displayName.substring(slashIndex + 1);
  }

  String lower = displayName;
  lower.toLowerCase();

  if (lower.endsWith(".mp3")) {
    displayName = displayName.substring(0, displayName.length() - 4);
  }

  return displayName;
}

void loadTracksFromSD() {
  trackCount = 0;
  trackListFound = false;

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

    file.close();
    file = root.openNextFile();
  }

  root.close();
  trackListFound = (trackCount > 0);
}

enum Screen : uint8_t {
  CLOCK,
  PLAYER,
  TRACKS,
  SETTINGS
};

Screen currentScreen = CLOCK;
Screen lastScreen = CLOCK;

int counter = 0;
int lastEncoderAState = HIGH;
bool needsRedraw = true;

void drawClockScreen() {
  display.setTextFont(2);
  display.drawString("Clock", 0, 10);

  display.drawFastHLine(0, 24, SCREEN_WIDTH, TFT_WHITE);
  
  String time = "12:45";
  display.setTextFont(7);
  display.setTextDatum(MC_DATUM);
  display.drawString(time, SCREEN_WIDTH / 2, 44);
}

void drawPlayerScreen() {
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextFont(2);

  display.drawString("Player", 0, 10);
  display.drawFastHLine(0, 24, SCREEN_WIDTH, TFT_WHITE);

  display.fillRect(0, 40, SCREEN_WIDTH, 50, TFT_BLACK);

  if (!trackListFound || selectedTrackIndex < 0 || selectedTrackIndex >= trackCount) {
    display.setTextFont(1);
    display.drawString("No track selected", 2, 44);
    return;
  }

  String displayTrack = getDisplayTrackName(tracks[selectedTrackIndex]);
  const char* track = displayTrack.c_str();
  int trackTextWidth = display.textWidth(track, 2);

  const char* statusText = isSongPaused ? "Paused" : "Playing";
  int statusTextWidth = display.textWidth(statusText, 2);

  int xTrack;
  int xStatus = (SCREEN_WIDTH - statusTextWidth) / 2;

  if (trackTextWidth <= SCREEN_WIDTH) {
    xTrack = (SCREEN_WIDTH - trackTextWidth) / 2;
  } else {
    xTrack = playerScreenScrollX;
  }

  if (xTrack < SCREEN_WIDTH - trackTextWidth) xTrack = SCREEN_WIDTH - trackTextWidth;
  if (xStatus < 0) xStatus = 0;

  display.drawString(track, xTrack, 44);
  display.drawString(statusText, xStatus, 72);
}


void drawTracksScreen() {
  display.setTextFont(2);
  display.drawString("Tracks", 0, 10);
  display.drawFastHLine(0, 24, SCREEN_WIDTH, TFT_WHITE);
  display.setTextFont(1);

  if (!trackListFound || trackCount == 0) {
    display.drawString("No tracks found.", 2, 36);
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

    int y = 36 + (i * 16);
    String displayTrack = getDisplayTrackName(tracks[trackIndex]);
    const char* trackName = displayTrack.c_str();

    if (trackIndex == counter) {
      display.fillRect(0, y - 2, SCREEN_WIDTH, 14, TFT_WHITE);
      display.setTextColor(TFT_BLACK, TFT_WHITE);
      display.drawString(trackName, 2, y);
      display.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
      display.fillRect(0, y - 2, SCREEN_WIDTH, 14, TFT_BLACK);
      display.drawString(trackName, 2, y);
    }
  }
}


void drawSettingsScreen() {
  display.setTextFont(2);
  display.drawString("Settings", 0, 10);

  display.drawFastHLine(0, 24, SCREEN_WIDTH, TFT_WHITE);

  display.setTextFont(2);
  display.drawString("Settings menu", 0, 44);
}

void drawCurrentScreen() {
  if (currentScreen != lastScreen) {
    display.fillScreen(TFT_BLACK);
    lastScreen = currentScreen;
  }

  switch (currentScreen) {
    case CLOCK:    drawClockScreen(); break;
    case PLAYER:   drawPlayerScreen(); break;
    case TRACKS:   drawTracksScreen(); break;
    case SETTINGS: drawSettingsScreen(); break;
  }

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

void resetPlayerScroll() {
  playerScreenScrollDirection = -1;
  lastPlayerScroll = millis();
  playerScrollPauseUntil = 0;

  if (!trackListFound || selectedTrackIndex < 0 || selectedTrackIndex >= trackCount) {
    playerScreenScrollX = 0;
    return;
  }

  display.setTextFont(2);

  String displayTrack = getDisplayTrackName(tracks[selectedTrackIndex]);
  int trackTextWidth = display.textWidth(displayTrack.c_str(), 2);

  if (trackTextWidth <= SCREEN_WIDTH) {
    playerScreenScrollX = (SCREEN_WIDTH - trackTextWidth) / 2;
  } else {
    playerScreenScrollX = 0;
  }
}

void updatePlayerScroll() {
  if (currentScreen != PLAYER) {
    return;
  }

  if (!trackListFound || selectedTrackIndex < 0 || selectedTrackIndex >= trackCount) {
    return;
  }

  display.setTextFont(2);

  String displayTrack = getDisplayTrackName(tracks[selectedTrackIndex]);
  int trackTextWidth = display.textWidth(displayTrack.c_str(), 2);

  if (trackTextWidth <= SCREEN_WIDTH) {
    int centeredX = (SCREEN_WIDTH - trackTextWidth) / 2;
    if (playerScreenScrollX != centeredX) {
      playerScreenScrollX = centeredX;
      needsRedraw = true;
    }
    return;
  }

  unsigned long now = millis();

  if (playerScrollPauseUntil > now) {
    return;
  }

  if (now - lastPlayerScroll < PLAYER_SCROLL_INTERVAL) {
    return;
  }

  lastPlayerScroll = now;

  int minX = SCREEN_WIDTH - trackTextWidth;
  int maxX = 0;

  playerScreenScrollX += playerScreenScrollDirection;

  if (playerScreenScrollX <= minX) {
    playerScreenScrollX = minX;
    playerScreenScrollDirection = 1;
    playerScrollPauseUntil = now + PLAYER_SCROLL_PAUSE;
  } else if (playerScreenScrollX >= maxX) {
    playerScreenScrollX = maxX;
    playerScreenScrollDirection = -1;
    playerScrollPauseUntil = now + PLAYER_SCROLL_PAUSE;
  }

  needsRedraw = true;
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
        resetPlayerScroll();
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
  
  display.init();
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);

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
  updatePlayerScroll();

  if (needsRedraw) {
    drawCurrentScreen();
  }
}