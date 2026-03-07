#include <Arduino.h>

int rotaryA = 27;
int rotaryB = 14;
int rotarySw = 12;

int counter = 0;
int aState;
int aLastState;


void setup() {
  pinMode(rotaryA, INPUT);
  pinMode(rotaryB, INPUT);
  pinMode(rotarySw, INPUT);
  Serial.begin(115200);
  aLastState = digitalRead(rotaryA);
}

void loop() {
  aState = digitalRead(rotaryA);
  if (aState != aLastState) {
    if (digitalRead(rotaryB) != aState) {
      counter++;
    } else {
      counter--;
    }
    Serial.print("Position: ");
    Serial.println(counter);
  }
  aLastState = aState;
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}