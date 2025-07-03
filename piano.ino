 /*
 * ESP32 BlueDisplay Electronic Piano
 * * Author: Your Name
 * Date: July 3, 2025
 * * Description:
 * This sketch turns an ESP32 into a simple electronic piano using the 
 * BlueDisplay library by ArminJo. The piano keyboard is displayed on an 
 * Android device running the BlueDisplay app, and tones are played through 
 * the ESP32's DAC output.
 * * Hardware Requirements:
 * - ESP32 development board
 * - Bluetooth module (like HC-05) connected to the ESP32's Serial2 pins
 * (ESP32 TX2 to HC-05 RX, ESP32 RX2 to HC-05 TX)
 * - An Android device with the BlueDisplay app installed.
 * - An amplifier and speaker connected to GPIO25 (DAC1) and GND.
 * * BlueDisplay Library:
 * Make sure you have the latest version of the BlueDisplay library installed.
 * You can find it here: https://github.com/ArminJo/Arduino-BlueDisplay
 * * How it works:
 * 1. The ESP32 connects to the BlueDisplay app via Bluetooth.
 * 2. A piano keyboard is drawn on the app's screen.
 * 3. When a key is touched on the app, a callback is triggered on the ESP32.
 * 4. The ESP32 generates a sine wave corresponding to the note of the pressed key.
 * 5. The sine wave is output to the DAC on GPIO25, producing an audible tone.
 */

#include <BlueDisplay.h>

// Use Serial2 for communication with the Bluetooth module
#define BLUETOOTH_SERIAL Serial2

// DAC pin for audio output
#define DAC_PIN 25

// Notes for the piano keyboard (C4 to C5)
const int notes[] = {262, 294, 330, 349, 392, 440, 494, 523};
const char* noteNames[] = {"C", "D", "E", "F", "G", "A", "B", "C5"};

// BlueDisplay object
BlueDisplay blueDisplay(&BLUETOOTH_SERIAL);

// Currently playing note frequency
volatile int currentFrequency = 0;

// Task handle for the tone generation task
TaskHandle_t toneTaskHandle = NULL;

// Function to generate a sine wave on the DAC
void toneTask(void *parameter) {
  int frequency = 0;
  uint32_t sampling_rate = 20000;
  
  while (true) {
    if (currentFrequency != 0) {
      frequency = currentFrequency;
      uint32_t period = sampling_rate / frequency;
      for (int i = 0; i < period; i++) {
        // Simple sine wave generation
        float val = 127.5 + 127.5 * sin(2 * PI * i / period);
        dacWrite(DAC_PIN, (int)val);
        delayMicroseconds(1000000 / sampling_rate);
      }
    } else {
      // No note playing, output silence
      dacWrite(DAC_PIN, 128);
      vTaskDelay(1);
    }
  }
}

// Callback function for button presses
void buttonCallback(const char* aCallbackString, int aValue) {
  if (aValue == 1) { // Key pressed
    // Find the note frequency from the callback string
    for (int i = 0; i < 8; i++) {
      if (strcmp(aCallbackString, noteNames[i]) == 0) {
        currentFrequency = notes[i];
        break;
      }
    }
  } else { // Key released
    currentFrequency = 0;
  }
}

void setup() {
  // Start serial for debugging
  Serial.begin(115200);
  
  // Start serial for BlueDisplay
  BLUETOOTH_SERIAL.begin(9600);

  // Wait for BlueDisplay to connect
  while (!blueDisplay.begin()) {
    Serial.println("Waiting for BlueDisplay connection...");
    delay(1000);
  }

  // Clear the screen and set background color
  blueDisplay.clear(BLUE_DISPLAY_BLACK);

  // Draw the piano keys
  int keyWidth = blueDisplay.getDisplayWidth() / 8;
  for (int i = 0; i < 8; i++) {
    blueDisplay.createButton(
      i * keyWidth, 0, keyWidth, blueDisplay.getDisplayHeight(),
      noteNames[i], buttonCallback, noteNames[i]
    );
  }
  
  // Create and start the tone generation task
  xTaskCreate(
    toneTask,         // Task function
    "ToneTask",       // Name of the task
    10000,            // Stack size of task
    NULL,             // Parameter of the task
    1,                // Priority of the task
    &toneTaskHandle   // Task handle to keep track of created task
  );

  Serial.println("Piano ready!");
}

void loop() {
  // Let BlueDisplay handle events
  blueDisplay.checkForEvents();
}
