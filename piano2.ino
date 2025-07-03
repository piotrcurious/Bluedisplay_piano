 /*
 * ESP32 BlueDisplay Electronic Piano (Corrected Version)
 * Author: Your Name
 * Date: July 3, 2025
 *
 * Description:
 * This sketch turns an ESP32 into a simple electronic piano using the
 * BlueDisplay library by ArminJo. The piano keyboard is displayed on an
 * Android device running the BlueDisplay app, and tones are played through
 * the ESP32's DAC output.
 *
 * This version correctly handles screen resizing and redraw events from the
 * BlueDisplay app, ensuring the UI is always rendered correctly.
 *
 * Hardware Requirements:
 * - ESP32 development board
 * - Bluetooth module (like HC-05) connected to the ESP32's Serial2 pins
 * (ESP32 TX2 (GPIO 17) to HC-05 RX, ESP32 RX2 (GPIO 16) to HC-05 TX)
 * - An Android device with the BlueDisplay app installed.
 * - An amplifier and speaker connected to GPIO25 (DAC1) and GND.
 *
 * BlueDisplay Library:
 * Make sure you have the latest version of the BlueDisplay library installed.
 * You can find it here: https://github.com/ArminJo/Arduino-BlueDisplay
 *
 * How it works:
 * 1. The ESP32 connects to the BlueDisplay app via Bluetooth.
 * 2. A piano keyboard is drawn on the app's screen by the drawPiano() function.
 * 3. The main loop listens for events. If a redraw is requested (e.g., screen rotation),
 * it calls drawPiano() again to rebuild the UI.
 * 4. When a key is touched on the app, a callback is triggered on the ESP32.
 * 5. The ESP32 generates a sine wave corresponding to the note of the pressed key.
 * 6. The sine wave is output to the DAC on GPIO25, producing an audible tone.
 */

#include <BlueDisplay.h>

// Use Serial2 for communication with the Bluetooth module
// Default pins for Serial2 on most ESP32 boards are GPIO 16 (RX) and 17 (TX)
#define BLUETOOTH_SERIAL Serial2

// DAC pin for audio output
#define DAC_PIN 25

// Notes for the piano keyboard (C4 to C5)
const int notes[] = { 262, 294, 330, 349, 392, 440, 494, 523 };
const char* noteNames[] = { "C", "D", "E", "F", "G", "A", "B", "C5" };
const uint8_t NUM_KEYS = sizeof(notes) / sizeof(notes[0]);

// BlueDisplay object
BlueDisplay blueDisplay(&BLUETOOTH_SERIAL);

// Currently playing note frequency. Volatile because it's accessed by an ISR (task).
volatile int currentFrequency = 0;

// Task handle for the tone generation task
TaskHandle_t toneTaskHandle = NULL;

// Function to generate a sine wave on the DAC. Runs as a separate task.
void toneTask(void *parameter) {
  const uint32_t sampling_rate = 20000; // Hz
  uint32_t frequency = 0;

  while (true) {
    // Only update frequency if it has changed to avoid clicks
    if (frequency != currentFrequency) {
      frequency = currentFrequency;
    }

    if (frequency > 0) {
      uint32_t period_samples = sampling_rate / frequency;
      for (uint32_t i = 0; i < period_samples; i++) {
        // Simple sine wave generation
        // Output is 0-255, so we generate a wave from 0 to 255 centered at 128
        float val = 127.5 + 127.5 * sin(2 * PI * i / period_samples);
        dacWrite(DAC_PIN, (uint8_t)val);
        // We must check if the frequency changed mid-wave to avoid getting stuck
        if(frequency != currentFrequency) break; 
        delayMicroseconds(1000000 / sampling_rate);
      }
    } else {
      // No note playing, output silence (mid-point of DAC)
      dacWrite(DAC_PIN, 128);
      // Wait a bit to prevent this loop from starving other tasks
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

// Callback function for button presses (piano keys)
void buttonCallback(const char* aCallbackString, int aValue) {
  // aValue is 1 for press, 0 for release
  if (aValue == 1) { 
    // Find the note frequency from the callback string
    for (int i = 0; i < NUM_KEYS; i++) {
      if (strcmp(aCallbackString, noteNames[i]) == 0) {
        currentFrequency = notes[i];
        return; // Exit once found
      }
    }
  } else { // Key released
    currentFrequency = 0;
  }
}

// Function to draw the entire piano UI
void drawPiano() {
  // Clear the screen and set background color
  blueDisplay.clear(BLUE_DISPLAY_DARK_GRAY);
  blueDisplay.deleteAllButtons(); // Important: remove old buttons before redrawing

  // Get current screen dimensions to make the layout responsive
  int displayWidth = blueDisplay.getDisplayWidth();
  int displayHeight = blueDisplay.getDisplayHeight();

  // Draw the white keys
  int keyWidth = displayWidth / NUM_KEYS;
  for (int i = 0; i < NUM_KEYS; i++) {
    // Create a button for each key
    // Buttons are white with a black border and black text
    uint16_t tButtonHandle = blueDisplay.createButton(
      i * keyWidth, 0, keyWidth - 2, displayHeight, // x, y, width, height (-2 for a small gap)
      noteNames[i], buttonCallback, noteNames[i]
    );
    // Set button colors
    blueDisplay.setButtonColors(tButtonHandle, BLUE_DISPLAY_WHITE, BLUE_DISPLAY_BLACK, BLUE_DISPLAY_BLACK);
  }
  
  blueDisplay.println("Piano Ready!");
}


void setup() {
  // Start serial for debugging
  Serial.begin(115200);
  
  // Start serial for BlueDisplay at 9600 baud (default for HC-05)
  BLUETOOTH_SERIAL.begin(9600);

  Serial.println("Waiting for BlueDisplay app to connect...");
  // The begin() function waits for the initial START command from the app.
  // It returns false if it times out.
  while (!blueDisplay.begin()) {
    Serial.println("Timeout. Retrying...");
    delay(1000);
  }
  Serial.println("BlueDisplay connected!");
  
  // Initial drawing of the UI
  drawPiano();
  
  // Create and start the tone generation task on core 1
  xTaskCreatePinnedToCore(
    toneTask,         // Task function
    "ToneTask",       // Name of the task
    4096,             // Stack size of task
    NULL,             // Parameter of the task
    1,                // Priority of the task
    &toneTaskHandle,  // Task handle to keep track of created task
    1                 // Pin to core 1
  );
}

void loop() {
  // This is the main event loop for BlueDisplay
  uint8_t tResult = blueDisplay.checkForEvents();

  // Check if the app has requested a redraw (e.g. after screen rotation)
  if (tResult == BlueDisplay::EVENT_REDRAW_REQUEST) {
    Serial.println("Redraw event received.");
    drawPiano(); // Redraw the entire UI
  }
}
