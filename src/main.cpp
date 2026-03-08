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
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

const int I2S_BCLK = 33;
const int I2S_LRC = 32;
const int I2S_DOUT = 26;

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSD *audioFile = nullptr;
AudioOutputI2S *audioOut = nullptr;

const int SCREEN_WIDTH = 160;
const int SCREEN_HEIGHT = 128;

uint8_t SD_CS = 22;
uint8_t ROTARY_A  = 27;
uint8_t ROTARY_B  = 35;
uint8_t ROTARY_SW = 34;
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


void stopAudio() {
  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }

  if (audioFile) {
    delete audioFile;
    audioFile = nullptr;
  }

  isSongPaused = true;
}

void playSelectedTrack() {
  Serial.println("playSelectedTrack called");

  if (!trackListFound || selectedTrackIndex < 0 || selectedTrackIndex >= trackCount) {
    Serial.println("Invalid track selection");
    return;
  }

  stopAudio();

  String path = tracks[selectedTrackIndex];
  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  Serial.print("Playing: ");
  Serial.println(path);

  audioFile = new AudioFileSourceSD(path.c_str());
  if (!audioFile || !audioFile->isOpen()) {
    Serial.println("Failed to open audio file");
    stopAudio();
    return;
  }

  mp3 = new AudioGeneratorMP3();

  bool ok = mp3->begin(audioFile, audioOut);
  Serial.print("mp3->begin returned: ");
  Serial.println(ok ? "true" : "false");

  if (ok) {
    isSongPaused = false;
  } else {
    Serial.println("MP3 begin failed");
    stopAudio();
  }
}

void syncAudioToScreen() {
  if (currentScreen == PLAYER) {
    if (!mp3 || !mp3->isRunning()) {
      playSelectedTrack();
    }
  } else {
    if (mp3) {
      stopAudio();
      isSongPaused = true;
    }
  }
}

void drawClockScreen() {
  display.setTextFont(2);
  display.drawString("Clock", 0, 10);

  display.drawFastHLine(0, 24, SCREEN_WIDTH, TFT_WHITE);

  display.setTextFont(2);
  display.drawString("Count:", 0, 44);

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d", counter);
  display.drawString(buffer, 60, 44);

  display.setTextFont(1);
  display.drawString("Press to reset", 0, 72);
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
  Serial.print("rotaryPressed currentScreen = ");
  Serial.println((int)currentScreen);

  switch (currentScreen) {
    case CLOCK:
      Serial.println("CLOCK");
      break;

    case PLAYER:
      Serial.println("PLAYER");
      isSongPaused = !isSongPaused;
      needsRedraw = true;
      playSelectedTrack();
      break;

    case TRACKS:
      Serial.println("TRACKS");
      if (trackListFound) {
        selectedTrackIndex = counter;
        currentScreen = PLAYER;
        resetPlayerScroll();
        needsRedraw = true;
        break;
      }

    case SETTINGS:
      Serial.println("SETTINGS");
      break;

    default:
      Serial.println("DEFAULT");
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

  audioOut = new AudioOutputI2S();
  audioOut->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audioOut->SetGain(0.3);
}

void loop() {

  handleEncoder();
  handleButtons();
  updatePlayerScroll();

  if (mp3) {
    if (mp3->isRunning()) {
      bool ok = mp3->loop();
      if (!ok) {
        Serial.println("mp3->loop() returned false");
        mp3->stop();
        stopAudio();
        needsRedraw = true;
      }
    } else {
      Serial.println("mp3 exists but is not running");
    }
  }

    if (needsRedraw) {
      drawCurrentScreen();
    }
  }