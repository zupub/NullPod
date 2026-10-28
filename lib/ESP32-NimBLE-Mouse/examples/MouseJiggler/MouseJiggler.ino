#include <BleMouse.h>

BleMouse bleMouse("ESP32-MouseJiggler");

static const int frequency = 60 * 5; // every 300secs

void setup() {
  Serial.begin(115200);
  Serial.println("Starting MouseJiggler");
  bleMouse.begin();
}

void loop() {
  if(bleMouse.isConnected()) {
    static int movement = 1;
    Serial.printf("move %d dot(s)", movement);
    Serial.println();
    
    bleMouse.move(movement, 0);
    
    movement *= -1;
    delay(1000 * frequency);
  }
}
