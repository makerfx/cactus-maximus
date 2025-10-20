
#include <Servo.h>

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>


// GUItool: begin automatically generated code
AudioPlaySdWav           playWav1;     //xy=343.37500762939453,315.3750047683716
AudioEffectGranular      granular1;      //xy=590.3750076293945,306.3750047683716
AudioOutputI2S           audioOutput;           //xy=775.375,313.375
AudioConnection          patchCord1(playWav1, 0, granular1, 0);
AudioConnection          patchCord2(granular1, 0, audioOutput, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=336.375,469.375
// GUItool: end automatically generated code

// Define buffer size (in samples)
#define GRANULAR_MEMORY_SIZE 16384
short granularMemory[GRANULAR_MEMORY_SIZE];


// Use these with the Teensy 3.5 & 3.6 & 4.1 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used

Servo myServo;

void setup() {
  Serial.begin(115200);

  myServo.attach(3);  // Attach servo to pin 3
  danceOFF();
  
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(8);

  sgtl5000_1.enable();
  sgtl5000_1.volume(.8);

  granular1.begin(granularMemory, GRANULAR_MEMORY_SIZE);
  granular1.setSpeed(0.8);  // Pitch down with speed less than 1
  granular1.beginPitchShift(20); //smaller number is less robotic

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
}

void playFile(const char *filename)
{
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playWav1.play(filename);

    danceON();
  // A brief delay for the library read WAV info
  delay(25);

  // Simply wait for the file to finish playing.
  while (playWav1.isPlaying()) {
    // uncomment these lines if you audio shield
    // has the optional volume pot soldered
    //float vol = analogRead(15);
    //vol = vol / 1024;
    // sgtl5000_1.volume(vol);
  }
  danceOFF();
}


void loop() {

  playFile("BIRTHDAY.WAV");  // filenames are always uppercase 8.3 format
  delay(4000);
  playFile("CACTY1.WAV");
  delay(4000);

  /*
  playFile("SDTEST3.WAV");
  delay(500);
  playFile("SDTEST4.WAV");
  delay(1500);
  */
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
    delay(50);  // Adjust delay for speed
  }
}

