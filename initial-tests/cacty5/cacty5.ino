#include <Servo.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include "USBHost_t36.h"

const int myInput = AUDIO_INPUT_LINEIN;
// const int myInput = AUDIO_INPUT_MIC;

AudioInputI2S        audioInput;         // audio shield: mic or line-in
AudioAnalyzePeak     peak_L;
AudioRecordQueue     recordQueue;        // recording queue
AudioPlayQueue       playQueue;          // playback queue
AudioPlaySdWav       playWav1;           // SD card WAV player
AudioEffectGranular  granular1;          // granular effect
AudioMixer4          mixer1;             // mixer for granular and WAV
AudioMixer4          mixer2;             // mixer for right channel
AudioOutputI2S       audioOutput;        // audio shield: headphones & line-out

AudioConnection c1(audioInput,0,peak_L,0);
AudioConnection c2(audioInput,0,recordQueue,0);
AudioConnection c3(playQueue,0,granular1,0);
AudioConnection c4(granular1,0,mixer1,0);
AudioConnection c5(granular1,0,mixer2,0);
AudioConnection c6(playWav1,0,mixer1,1);
AudioConnection c7(playWav1,1,mixer2,1);
AudioConnection c8(mixer1,0,audioOutput,0);
AudioConnection c9(mixer2,0,audioOutput,1);

AudioControlSGTL5000 audioShield;

// Granular effect buffer
#define GRANULAR_MEMORY_SIZE 16384
short granularMemory[GRANULAR_MEMORY_SIZE];

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
unsigned long recordingStartTime = 0;
int playbackBlock = 0;

// Servo
Servo myServo;

// USB Host for TV remote
USBHost myusb;
USBHub hub1(myusb);
USBHIDParser hid1(myusb);
KeyboardController keyboard1(myusb);


void setup() {
  Serial.begin(9600);

  // Audio setup
  AudioMemory(40);  // Increased from 20 to prevent glitches
  audioShield.enable();
  audioShield.inputSelect(myInput);
  audioShield.volume(1.0);
  audioShield.micGain(50);

  // Setup granular effect
  granular1.begin(granularMemory, GRANULAR_MEMORY_SIZE);
  granular1.setSpeed(0.8);  // Pitch down with speed less than 1
  granular1.beginPitchShift(20); // Smaller number is less robotic

  // Setup mixers - channel 0 for granular, channel 1 for WAV playback
  mixer1.gain(0, 1.0);  // Granular output (left)
  mixer1.gain(1, 1.0);  // WAV output (left)
  mixer2.gain(0, 1.0);  // Granular output (right)
  mixer2.gain(1, 1.0);  // WAV output (right)


 

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

 
  // Serial.println("Listening for sound...");
  Serial.println("Type 'play' to replay last recording");
  Serial.println("Press TV remote UP button to replay");
  Serial.println("Press TV remote DOWN button to play KART.wav");
}

elapsedMillis fps;
elapsedMillis playDelayTime;

void loop() {
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
    playDelay("DANCING.WAV");
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
  }
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

void danceOFF() {
  myServo.write(93);
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
}

void OnHIDExtrasPress(uint32_t top, uint16_t key) {
  OnPress(key);
}

void triggerPlayback() {
  if (recordedBlocks > 0) {
    Serial.println("TV Remote playback triggered!");
    playbackBlock = 0;
    currentState = PLAYING;
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

}
