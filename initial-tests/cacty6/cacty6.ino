#include <Servo.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include "USBHost_t36.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
AudioEffectGranular  granular3;          // granular effect
AudioEffectGranular  granular4;          // granular effect

AudioMixer4          mixer1;             // mixer for granular and WAV
AudioMixer4          mixer2;             // mixer for right channel
AudioOutputI2S       audioOutput;        // audio shield: headphones & line-out
AudioFilterStateVariable filter1; // low-pass filter
AudioFilterStateVariable filter2; // low-pass filter
AudioFilterStateVariable filter3; // low-pass filter
AudioFilterStateVariable filter4; // low-pass filter

//not using a chorus on 1 (yet?) but keeping the numbering consistent
AudioEffectChorus chorus2; // chorus effect
AudioEffectChorus chorus3; // chorus effect
AudioEffectChorus chorus4; // chorus effect

AudioEffectDelay delay2; 
AudioEffectDelay delay3; 
AudioEffectDelay delay4;


AudioConnection aiP1(audioInput,0,peak_L,0);
AudioConnection aiRq1(audioInput,0,recordQueue,0);

AudioConnection pqG1(playQueue,0,granular1,0); 
AudioConnection pqG2(playQueue,0,granular2,0);
AudioConnection pqG3(playQueue,0,granular3,0);
AudioConnection pqG4(playQueue,0,granular4,0); 

AudioConnection g1F1(granular1,0,filter1,0);
AudioConnection g2F2(granular2,0,filter2,0);
AudioConnection g3F3(granular3,0,filter3,0);
AudioConnection g4F4(granular4,0,filter4,0);

AudioConnection f2C2(filter2,0,delay2,0);
AudioConnection f3C3(filter3,0,delay3,0);
AudioConnection f4C4(filter4,0,delay4,0);

AudioConnection d2C2(delay2,0,chorus2,0);
AudioConnection d3C3(delay3,0,chorus3,0);
AudioConnection d4C4(delay4,0,chorus4,0);


AudioConnection f1M1(filter1,0,mixer1,1);   //left

AudioConnection c2M2(chorus2,0,mixer2,1); //right
AudioConnection c3M2(chorus3,0,mixer2,2); //right
AudioConnection c4M2(chorus4,0,mixer2,3); //right


AudioConnection pwM1(playWav1,0,mixer1,0); //left
AudioConnection pwM2(playWav1,1,mixer2,0); //right
AudioConnection m1AO(mixer1,0,audioOutput,0);
AudioConnection m2AO(mixer2,0,audioOutput,1);

AudioControlSGTL5000 audioShield;

// Granular effect buffer
#define GRANULAR_MEMORY_SIZE 16384
short granularMemory1[GRANULAR_MEMORY_SIZE];
short granularMemory2[GRANULAR_MEMORY_SIZE];
short granularMemory3[GRANULAR_MEMORY_SIZE];
short granularMemory4[GRANULAR_MEMORY_SIZE];

// Chorus delay buffer (must be audio_block_t sized) 
short chorusBuffer2[512]; 
short chorusBuffer3[640]; 
short chorusBuffer4[768]; 


// SD card pins for Teensy built-in SD
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used

// Recording buffer - stores up to 5 seconds at 44.1kHz
// ~172 blocks/sec × 5 sec = 860 blocks for 5 seconds
#define MAX_RECORDING_BLOCKS 860  // 5 seconds
DMAMEM int16_t recordingBuffer[MAX_RECORDING_BLOCKS][128];  // Each block is 128 samples
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

// ------------------------------------------------------------
// Convert semitone shift to granular speed ratio
// Handles positive and negative semitone values
// Formula: speed = 2^(semitones / 12)
// ------------------------------------------------------------
float semitone(int n) {
    return powf(2.0f, (float)n / 12.0f);
}

void randomizeDelays() {
  delay2.delay(0,random(20,60));
  delay3.delay(0,random(40,80));
  delay4.delay(0,random(60,100));
}

void setup() {
  Serial.begin(115200);
  pinMode(dancePartyPin1, OUTPUT);
  pinMode(dancePartyPin2, OUTPUT);
  analogWrite(dancePartyPin1, 0);
  analogWrite(dancePartyPin2, 0);
  
    
  // Audio setup
  AudioMemory(200);  // Increased from 20 to prevent glitches
  audioShield.enable();
  audioShield.inputSelect(myInput);
  audioShield.volume(.9);
  audioShield.micGain(100);

  // Setup granular effect
  granular1.begin(granularMemory1, GRANULAR_MEMORY_SIZE);
  granular1.setSpeed(semitone(-4));  // Pitch down with speed less than 1
  granular1.beginPitchShift(80); 


  granular2.begin(granularMemory2, GRANULAR_MEMORY_SIZE);
  granular2.setSpeed(semitone(+3));  
  granular2.beginPitchShift(80); 

  granular3.begin(granularMemory3, GRANULAR_MEMORY_SIZE);
  granular3.setSpeed(semitone(+4));  
  granular3.beginPitchShift(80);

  granular4.begin(granularMemory4, GRANULAR_MEMORY_SIZE);
  granular4.setSpeed(semitone(+5));  
  granular4.beginPitchShift(80);

  // --- Low-pass filter setup ---
  filter1.frequency(9000);
  filter2.frequency(9000);
  filter3.frequency(9000);
  filter4.frequency(9000);

  // smooths out grain edges 
  filter1.resonance(0.7); // gentle resonance
  filter2.resonance(2); // gentle resonance
  filter3.resonance(3); // gentle resonance
  filter4.resonance(4); // gentle resonance

  randomizeDelays();



  // --- Chorus setup --- 
  chorus2.begin(chorusBuffer2, 512,3); 
  delay(37); //spread out the LFOs
  chorus3.begin(chorusBuffer3, 640,3); 
  delay(53);
  chorus4.begin(chorusBuffer4, 768,3); 

  // Setup mixers - channel 0 for granular, channel 1 for WAV playback
  mixer1.gain(0, 1);  // WAV output (left)
  mixer1.gain(1, .7);  // Granular output (left)
 
  mixer2.gain(0, 1);  // WAV output (right)
  mixer2.gain(1, 1);  // Granular output (right)
  mixer2.gain(2, 1);  // Granular output (right)
  mixer2.gain(3, 1);  // Granular output (right)


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
      Serial.println(F("Unable to access the SD card"));
      delay(500);
    }
  }

   // Warning
  Serial.println(F("Playing WARN.WAV from SD card..."));
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

}

elapsedMillis fps;
elapsedMillis playDelayTime;


unsigned long last_time = 0;

void loop() {
  // Process USB Host events
  myusb.Task();


  if(millis() - last_time >= 1000) {
  randomizeDelays();

  Serial.print("Proc = ");
  Serial.print(AudioProcessorUsage());
  Serial.print(" (");    
  Serial.print(AudioProcessorUsageMax());
  Serial.print("),  Mem = ");
  Serial.print(AudioMemoryUsage());
  Serial.print(" (");    
  Serial.print(AudioMemoryUsageMax());
  Serial.println(")");
  last_time = millis();
}


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