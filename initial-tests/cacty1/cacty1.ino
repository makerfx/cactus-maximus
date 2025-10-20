#include <Servo.h>

Servo myServo;

void setup() {
  myServo.attach(3);  // Attach servo to pin 3
  myServo.write(93);
  Serial.begin(115200);
}

void danceOFF() {
  myServo.write(93);
  Serial.println("OFF: 93");
}

void danceON() {
  myServo.write(93);
  for (int pos = 93; pos <= 110; pos++) {
    myServo.write(pos);
    Serial.print("ON: ");
    Serial.println(pos);
    delay(200);  // Adjust delay for speed
  }
}

void loop() {
  danceON();
  delay (3000);
  danceOFF();
  delay (3000);
}
