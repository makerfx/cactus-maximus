
#include <Servo.h>

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>


// GUItool: begin automatically generated code
AudioInputI2S            mic1;           //xy=217.375,447.3750066757202
AudioPlaySdWav           playWav1;       //xy=223.375,502.0000057220459
AudioMixer4              mixer1;         //xy=407.375,458.375
AudioEffectGranular      granular1;      //xy=556.3750076293945,458.0000057220459
AudioOutputI2S           audioOutput;    //xy=775.3750076293945,459.0000057220459
AudioConnection          patchCord1(mic1, 0, mixer1, 0);
AudioConnection          patchCord2(playWav1, 0, mixer1, 1);
AudioConnection          patchCord3(mixer1, granular1);
AudioConnection          patchCord4(granular1, 0, audioOutput, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=237.375,578.0000085830688
// GUItool: end automatically generated code

bool dir = 0; 

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

  mixer1.gain(1,1);
  mixer1.gain(2,1);


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
  playFile("DANCING.WAV");
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
  if (dir) {
    for (int pos = 93; pos <= 110; pos++) {
      myServo.write(pos);
      Serial.print("ON: ");
      Serial.println(pos);
      delay(50);  // Adjust delay for speed
    }
  }
  else {
    for (int pos = 93; pos >= 82; pos--) {
      myServo.write(pos);
      Serial.print("ON: ");
      Serial.println(pos);
      delay(50);  // Adjust delay for speed
    }
  }

}

