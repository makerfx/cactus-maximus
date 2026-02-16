#include <Servo.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include "USBHost_t36.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "autotune.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
//this version works on mac and windows
#define __FILENAME__ ({ \
    const char *p = strrchr(__FILE__, '/'); \
    p ? p + 1 : (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__); \
})


// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int myInput = AUDIO_INPUT_LINEIN;
// const int myInput = AUDIO_INPUT_MIC;

AudioInputI2S        audioInput;         // audio shield: mic or line-in
AudioAnalyzePeak     peak_L;
AudioRecordQueue     recordQueue;        // recording queue
AudioPlayQueue       playQueue;          // playback queue
AudioPlaySdWav       playWav1;           // SD card WAV player
AudioEffectGranular  granular1;          // granular effect
AudioEffectGranular  granular2;          // granular effect
AudioMixer4          mixer1;             // mixer for granular and WAV
AudioMixer4          mixer2;             // mixer for right channel
AudioOutputI2S       audioOutput;        // audio shield: headphones & line-out

// --------------------------------------------------------------------------------------------
// autotune
AudioAnalyzeNoteFrequency notefreq;        // frequency detector
AudioFilterBiquad        autotuneFilter;   // filter (biquad, easy lowpass filter)    
CustomAutoTune autotuner;
// --------------------------------------------------------------------------------------------



AudioConnection c1(audioInput,0,peak_L,0);
AudioConnection c2(audioInput,0,recordQueue,0);
AudioConnection c3(playQueue,0,autotuner,0);
AudioConnection c4(autotuner,0,mixer1,0);
//AudioConnection c5(granular2,0,mixer2,0);
AudioConnection c6(playWav1,0,mixer1,1);
AudioConnection c7(playWav1,1,mixer2,1);
AudioConnection c8(mixer1,0,audioOutput,0);
AudioConnection c9(mixer2,0,audioOutput,1);


AudioControlSGTL5000 audioShield;



// Granular effect buffer
#define GRANULAR_MEMORY_SIZE 16384
short granularMemory1[GRANULAR_MEMORY_SIZE];
short granularMemory2[GRANULAR_MEMORY_SIZE];

// SD card pins for Teensy built-in SD
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used

// Recording buffer - stores up to 5 seconds at 44.1kHz
// ~172 blocks/sec × 5 sec = 860 blocks for 5 seconds
#define MAX_RECORDING_BLOCKS 860  // 5 seconds
int16_t recordingBuffer[MAX_RECORDING_BLOCKS][128];  // Each block is 128 samples
int recordedBlocks = 0;

// State machine
enum State {
  WAITING,
  RECORDING,
  PLAYING,
  PLAYING_WAV
};

State currentState = WAITING;
String stateText = "Initializing";
unsigned long recordingStartTime = 0;
int playbackBlock = 0;

// Servo
Servo myServo;

// USB Host for TV remote
USBHost myusb;
USBHub hub1(myusb);
USBHIDParser hid1(myusb);
KeyboardController keyboard1(myusb);

#define dancePartyPin1 4
#define dancePartyPin2 5


void setup() {
  Serial.begin(115200);
  pinMode(dancePartyPin1, OUTPUT);
  pinMode(dancePartyPin2, OUTPUT);
  analogWrite(dancePartyPin1, 0);
  analogWrite(dancePartyPin2, 0);
  
    
  // Audio setup
  AudioMemory(40);  // Increased from 20 to prevent glitches
  audioShield.enable();
  audioShield.inputSelect(myInput);
  audioShield.volume(1.0);
  audioShield.micGain(100);

/*
  // Setup granular effect
  granular1.begin(granularMemory1, GRANULAR_MEMORY_SIZE);
  granular1.setSpeed(0.8);  // Pitch down with speed less than 1
  granular1.beginPitchShift(20); // Smaller number is less robotic

  granular2.begin(granularMemory2, GRANULAR_MEMORY_SIZE);
  granular2.setSpeed(1.2);  // Pitch down with speed less than 1
  granular2.beginPitchShift(20); // Smaller number is less robotic
*/

  autotuneFilter.setLowpass(0, 3400, 0.707); // Butterworth filter configuration
  notefreq.begin(0.15); // Initialize Yin Algorithm with a 15% threshold
  autotuner.currFrequency = 20;
  autotuner.manualPitchOffset = -.3;

  // Setup mixers - channel 0 for granular, channel 1 for WAV playback
  mixer1.gain(0, 1.0);  // Granular output (left)
  mixer1.gain(1, 0.1);  // WAV output (left)
  mixer2.gain(0, 1.0);  // Granular output (right)
  mixer2.gain(1, 0.1);  // WAV output (right)


  //setup display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  Serial.println("OLED begun");
  
  // Clear the buffer
  display.clearDisplay();

  // text display tests
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(__FILENAME__);
  display.print(__DATE__);
  display.print(" ");
  display.println(__TIME__);
  display.println("Initializing...");
  display.display(); // actually display all of the above



  // Setup USB Host for TV remote
  myusb.begin();
  keyboard1.attachPress(OnPress);
  keyboard1.attachExtrasPress(OnHIDExtrasPress);

  // Setup SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

   // Warning
  Serial.println("Playing WARN.WAV from SD card...");
  playFile("WARN.WAV");

  // Setup servo
  myServo.attach(3);
  myServo.write(93);  // Neutral position

 
  analogWrite(dancePartyPin1, 255);
  analogWrite(dancePartyPin2, 255);
  delay(2000);
  analogWrite(dancePartyPin1, 0);
  analogWrite(dancePartyPin2, 0);
  

  // Serial.println("Listening for sound...");
  Serial.println("Type 'play' to replay last recording");
  Serial.println("Press TV remote UP button to replay");
  Serial.println("Press TV remote DOWN button to play KART.wav");

  updateDisplay();

  AudioProcessorUsageMaxReset();                                
  AudioMemoryUsageMaxReset();
  //filter1.processorUsageMaxReset();
  autotuner.processorUsageMaxReset();

}

elapsedMillis fps;
elapsedMillis playDelayTime;

void loop() {

  autotuneLoop();

  // Process USB Host events
  myusb.Task();

  // Check for serial commands
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "play" && recordedBlocks > 0) {
      Serial.println("Manual playback triggered!");
      playbackBlock = 0;
      currentState = PLAYING;
      updateDisplay();

    } else if (command == "play" && recordedBlocks == 0) {
      Serial.println("No recording available to play");
    }
  }
  // Serial.println(currentState);
  switch(currentState) {
    case WAITING:
      handleWaiting();
      break;
    case RECORDING:
      handleRecording();
      break;
    case PLAYING:
      handlePlaying();
      break;
    case PLAYING_WAV:
      handlePlayingWav();
      break;
  }
}

void handleWaiting() {
  if (playDelayTime > 60000) {
    //playDelay("DANCING.WAV");
    playDelayTime = 0;
  }
  if(fps > 24) {
    if (peak_L.available()) {
      fps=0;
      // DISABLED Peak Detection
      // uint8_t leftPeak=peak_L.read() * 30.0;

      // if (leftPeak > 1) {
      //   Serial.println("SOUND DETECTED - Starting recording...");

      //   recordQueue.begin();
      //   recordedBlocks = 0;
      //   recordingStartTime = millis();
      //   currentState = RECORDING;
      // }
    }
  }
}

void handleRecording() {
  // Copy audio data to buffer
  if (recordQueue.available() >= 1) {
    if (recordedBlocks < MAX_RECORDING_BLOCKS) {
      memcpy(recordingBuffer[recordedBlocks], recordQueue.readBuffer(), 256);
      recordQueue.freeBuffer();
      recordedBlocks++;

      // Print progress every 172 blocks (~1 second)
      if (recordedBlocks % 172 == 0) {
        Serial.print("Recording... ");
        Serial.print(recordedBlocks / 172);
        Serial.println(" sec");
      }
    } else {
      // Buffer full - stop recording
      recordQueue.end();
      recordQueue.clear();
      Serial.println("Recording complete! Playing back...");
      playbackBlock = 0;
      currentState = PLAYING;
      updateDisplay();
    }
  }

  // Stop after 5 seconds
  if (millis() - recordingStartTime > 5000 && recordedBlocks > 0) {
    recordQueue.end();
    recordQueue.clear();
    Serial.print("Recording timeout complete! Blocks recorded: ");
    Serial.println(recordedBlocks);
    Serial.println("Playing back...");
    playbackBlock = 0;
    currentState = PLAYING;
    updateDisplay();
  }
}

void handlePlaying() {
  // Start dancing on first playback block
  if (playbackBlock == 0) {
    danceON();
  }

  // Play back recorded audio
  if (playbackBlock < recordedBlocks) {
    int16_t *ptr = playQueue.getBuffer();
    if (ptr) {
      memcpy(ptr, recordingBuffer[playbackBlock], 256);
      playQueue.playBuffer();
      playbackBlock++;
    }
  } else {
    // Playback complete
    danceOFF();
    // Serial.println("Playback complete! Listening for sound...");

    // Add a delay to prevent immediate re-trigger
    delay(1000);

    // Clear peak detector reading to avoid false trigger
    if (peak_L.available()) {
      peak_L.read();
    }

    // DON'T reset recordedBlocks - keep the recording in memory!
    playbackBlock = 0;
    fps = 0;
    currentState = WAITING;
    updateDisplay();
  }
}


void danceON() {
  myServo.write(93);
  analogWrite(dancePartyPin1, 255);
  analogWrite(dancePartyPin2, 255);
  for (int pos = 93; pos <= 110; pos++) {
    myServo.write(pos);
    Serial.print("ON: ");
    Serial.println(pos);
    delay(50);  // Adjust delay for speed
  }
}

void danceOFF() {
  myServo.write(93);
  analogWrite(dancePartyPin1, 0);
  analogWrite(dancePartyPin2, 0);
  Serial.println("OFF: 93");
}

// USB Host callback functions
void OnPress(int key) {
  Serial.print("Key pressed: ");
  Serial.println(key);

  // TV remote UP button = key 218
  if (key == 218) {
    triggerPlayback();
  }
  // TV remote DOWN button = key 217
  else if (key == 217) {
    currentState = PLAYING_WAV;
  }
  updateDisplay();
}

void OnHIDExtrasPress(uint32_t top, uint16_t key) {
  OnPress(key);
}

void triggerPlayback() {
  if (recordedBlocks > 0) {
    Serial.println("TV Remote playback triggered!");
    playbackBlock = 0;
    currentState = PLAYING;
    updateDisplay();
  } else {
    Serial.println("No recording available to play");
  }
}

void playFile(const char *filename)
{
  
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playWav1.play(filename);

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
}

void playDelay(const char *filename)
{
  
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playWav1.play(filename);

  // A brief delay for the library read WAV info
  delay(25);

  // Simply wait for the file to finish playing.
  while (playWav1.isPlaying()) {
    // uncomment these lines if you audio shield
    // has the optional volume pot soldered
    //float vol = analogRead(15);
    //vol = vol / 1024;
    // sgtl5000_1.volume(vol);
    danceON();
  }
 danceOFF();
}

void handlePlayingWav() {
  Serial.println("Playing KART.WAV from SD card...");

  // Make sure recordQueue is NOT running during WAV playback
  // recordQueue.end();
  // recordQueue.clear();

  // Play the WAV file and switch to PLAYING_WAV state
  playFile("KART.WAV");
  recordQueue.begin();
  recordedBlocks = 0;
  recordingStartTime = millis();
  currentState = RECORDING;
  updateDisplay();


}

void updateDisplay() {

   switch(currentState) {
    case WAITING:
      stateText = "Waiting...";
      break;
    case RECORDING:
      stateText = "Recording...";
      break;
    case PLAYING:
      stateText = "Dance Party!";
      break;
    case PLAYING_WAV:
      stateText = "Get Ready!";
      break;
  }

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("DOWN BTN -> Record");
  display.println("UP BTN   -> Repeat");
  display.println("");
  display.print(stateText);
  display.display(); // actually display all of the above
}
void autotuneLoop() {
  if(notefreq.available()) {
    float note = notefreq.read();
    float prob = notefreq.probability();
    if(prob > 0.99) {
      if(note > 80 && note < 880) {
        autotuner.currFrequency = note + 10;
      }
    }
  }
}