/*
 * LIN Master - Backlighting Toggle & Brightness Controller
 *
 * Continuously sends LIN messages to control backlighting on both
 * driver and passenger seat button modules.
 *
 * Commands:
 * - Press 'b' in Serial Monitor to toggle backlighting ON/OFF
 * - Enter a number (34-255) to set brightness level
 *
 * Hardware: ESP32-S3-WROOM-1-N8R8 CAN & LIN Bus Board
 * Manufacturer: SK Pang Electronics (skpang.co.uk)
 *
 * Libraries required:
 * - LIN_master_portable_Arduino by Georg Icking
 *
 * Brightness Range:
 * - Minimum: 34 (0x22)
 * - Maximum: 200 (0xC8) - factory default
 * - Extended: 201-255 (0xFF) - experimental
 *
 * Author: Projects In Motion (https://github.com/projectsinmotion)
 * Date: 2025
 */

#include "LIN_master_HardwareSerial_ESP32.h"

// ============================================================================
// HARDWARE PIN DEFINITIONS (ESP32S3 CAN & LIN Board)
// ============================================================================

// LIN pins (TJA1021T transceiver)
#define PIN_LIN_TX    10
#define PIN_LIN_RX    3
#define LIN_CS        46  // LIN chip select
#define LIN_FAULT     9   // LIN fault detection

// Onboard RGB LED
#define LED_R 39
#define LED_G 38
#define LED_B 40
#define RGB_ON  LOW
#define RGB_OFF HIGH

// ============================================================================
// LIN PROTOCOL DEFINITIONS
// ============================================================================

// LIN configuration
#define LIN_SPEED      19200
#define LIN_VERSION    LIN_Master_Base::LIN_V2

// LIN IDs
#define LIN_ID_DRIVER_LED    0xB1  // Driver backlight control
#define LIN_ID_PASSENGER_LED 0x32  // Passenger backlight control

// Message interval (milliseconds)
#define MESSAGE_INTERVAL  50  // 20Hz update rate

// ============================================================================
// BACKLIGHTING COMMANDS
// ============================================================================

// Brightness range
#define BRIGHTNESS_MIN     34   // Minimum valid brightness (0x22)
#define BRIGHTNESS_MAX     255  // Maximum brightness (0xFF - uint8_t max)
#define BRIGHTNESS_DEFAULT 200  // Factory default (0xC8)

// Driver side commands (ID 0xB1)
uint8_t DRIVER_ON[2]  = {0x04, 0xC8};  // Variable command with brightness
const uint8_t DRIVER_OFF[2] = {0x0C, 0x00};  // Standard off command

// Passenger side commands (ID 0x32)
uint8_t PASSENGER_ON[2]  = {0x08, 0xC8};  // Variable command with brightness
const uint8_t PASSENGER_OFF[2] = {0x09, 0x00};  // Standard off command

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// LIN master instance
LIN_Master_HardwareSerial_ESP32 LIN(Serial2, PIN_LIN_RX, PIN_LIN_TX, "Master");

// Backlighting state
bool backlightingOn = false;
uint8_t currentBrightness = BRIGHTNESS_DEFAULT;  // Current brightness level

// Message timing
uint32_t lastMessageTime = 0;
bool sendingDriver = true;  // Alternate between driver and passenger

// LIN state tracking
bool waitingForLIN = false;

// Serial input buffer
String serialInput = "";

// ============================================================================
// LED CONTROL FUNCTIONS
// ============================================================================

void setLED(bool red, bool green, bool blue) {
  digitalWrite(LED_R, red ? RGB_ON : RGB_OFF);
  digitalWrite(LED_G, green ? RGB_ON : RGB_OFF);
  digitalWrite(LED_B, blue ? RGB_ON : RGB_OFF);
}

void updateBoardLED() {
  if (backlightingOn) {
    setLED(false, true, false);  // Green = backlighting ON
  } else {
    setLED(false, false, true);  // Blue = backlighting OFF
  }
}

// ============================================================================
// BRIGHTNESS CONTROL FUNCTIONS
// ============================================================================

void setBrightness(uint8_t brightness) {
  // Clamp brightness to valid range (will be 0-255 after uint8_t conversion)
  // but we'll validate before calling this function
  currentBrightness = brightness;

  // Update the ON command arrays with new brightness value
  DRIVER_ON[1] = brightness;
  PASSENGER_ON[1] = brightness;

  Serial.println("\n========================================");
  Serial.print("BRIGHTNESS SET TO: ");
  Serial.print(brightness);
  Serial.print(" (0x");
  if (brightness < 16) Serial.print("0");  // Add leading zero for single hex digit
  Serial.print(brightness, HEX);
  Serial.println(")");

  if (backlightingOn) {
    Serial.print("  Driver:    ID 0xB1, Data: 0x04, 0x");
    if (brightness < 16) Serial.print("0");
    Serial.println(brightness, HEX);
    Serial.print("  Passenger: ID 0x32, Data: 0x08, 0x");
    if (brightness < 16) Serial.print("0");
    Serial.println(brightness, HEX);
    Serial.println("  Status: Brightness updated (backlighting ON)");
  } else {
    Serial.println("  Status: Brightness value saved");
    Serial.println("  Note: Turn backlighting ON to apply");
  }
  Serial.println("========================================\n");
}

// ============================================================================
// LIN MESSAGE FUNCTIONS
// ============================================================================

void sendDriverBacklightCommand() {
  const uint8_t* data = backlightingOn ? DRIVER_ON : DRIVER_OFF;
  LIN.sendMasterRequest(LIN_VERSION, LIN_ID_DRIVER_LED, 2, (uint8_t*)data);
  waitingForLIN = true;
}

void sendPassengerBacklightCommand() {
  const uint8_t* data = backlightingOn ? PASSENGER_ON : PASSENGER_OFF;
  LIN.sendMasterRequest(LIN_VERSION, LIN_ID_PASSENGER_LED, 2, (uint8_t*)data);
  waitingForLIN = true;
}

void processLINResponse() {
  if (LIN.getState() == LIN_Master_Base::STATE_DONE) {
    waitingForLIN = false;

    // Reset state machine for next frame
    LIN.resetStateMachine();
    LIN.resetError();
  }
}

// ============================================================================
// SERIAL COMMAND PROCESSING
// ============================================================================

void processSerialCommand(String cmd) {
  cmd.trim();

  // Check if it's a numeric brightness value
  bool isNumeric = true;
  for (int i = 0; i < cmd.length(); i++) {
    if (!isDigit(cmd.charAt(i))) {
      isNumeric = false;
      break;
    }
  }

  if (isNumeric && cmd.length() > 0) {
    // Parse brightness value
    int brightnessValue = cmd.toInt();

    // Validate range
    if (brightnessValue >= BRIGHTNESS_MIN && brightnessValue <= BRIGHTNESS_MAX) {
      uint8_t brightness = (uint8_t)brightnessValue;
      setBrightness(brightness);
    } else {
      Serial.println("\n========================================");
      Serial.println("ERROR: Invalid brightness value");
      Serial.print("Range: ");
      Serial.print(BRIGHTNESS_MIN);
      Serial.print(" to ");
      Serial.println(BRIGHTNESS_MAX);
      Serial.println("========================================\n");
    }
    return;
  }

  // Convert to lowercase for text commands
  cmd.toLowerCase();

  if (cmd == "b") {
    // Toggle backlighting state
    backlightingOn = !backlightingOn;

    Serial.println("\n========================================");
    if (backlightingOn) {
      Serial.println("BACKLIGHTING: ON");
      Serial.print("  Driver:    ID 0xB1, Data: 0x04, 0x");
      if (currentBrightness < 16) Serial.print("0");
      Serial.println(currentBrightness, HEX);
      Serial.print("  Passenger: ID 0x32, Data: 0x08, 0x");
      if (currentBrightness < 16) Serial.print("0");
      Serial.println(currentBrightness, HEX);
      Serial.print("  Brightness: ");
      Serial.print(currentBrightness);
      Serial.print(" (0x");
      if (currentBrightness < 16) Serial.print("0");
      Serial.print(currentBrightness, HEX);
      Serial.println(")");
    } else {
      Serial.println("BACKLIGHTING: OFF");
      Serial.println("  Driver:    ID 0xB1, Data: 0x0C, 0x00");
      Serial.println("  Passenger: ID 0x32, Data: 0x09, 0x00");
    }
    Serial.println("========================================\n");

    updateBoardLED();
  }
  else if (cmd == "status") {
    printStatus();
  }
  else if (cmd == "help") {
    printHelp();
  }
  else if (cmd.length() > 0) {
    Serial.print("Unknown command: '");
    Serial.print(cmd);
    Serial.println("'");
    Serial.println("Type 'help' for available commands");
  }
}

void printStatus() {
  Serial.println("\n=== BACKLIGHTING STATUS ===");
  Serial.print("State: ");
  Serial.println(backlightingOn ? "ON" : "OFF");

  Serial.print("Brightness: ");
  Serial.print(currentBrightness);
  Serial.print(" (0x");
  if (currentBrightness < 16) Serial.print("0");
  Serial.print(currentBrightness, HEX);
  Serial.println(")");

  Serial.println("\nCurrent Commands:");
  Serial.print("  Driver (0xB1):    ");
  if (backlightingOn) {
    Serial.print("0x04, 0x");
    if (currentBrightness < 16) Serial.print("0");
    Serial.print(currentBrightness, HEX);
    Serial.println(" (ON)");
  } else {
    Serial.println("0x0C, 0x00 (OFF)");
  }

  Serial.print("  Passenger (0x32): ");
  if (backlightingOn) {
    Serial.print("0x08, 0x");
    if (currentBrightness < 16) Serial.print("0");
    Serial.print(currentBrightness, HEX);
    Serial.println(" (ON)");
  } else {
    Serial.println("0x09, 0x00 (OFF)");
  }

  Serial.print("\nUpdate Rate: ");
  Serial.print(1000 / MESSAGE_INTERVAL);
  Serial.println(" Hz");

  Serial.println("\nCommands:");
  Serial.println("  'b' - Toggle backlighting");
  Serial.print("  ");
  Serial.print(BRIGHTNESS_MIN);
  Serial.print("-");
  Serial.print(BRIGHTNESS_MAX);
  Serial.println(" - Set brightness");
  Serial.println("==============================\n");
}

void printHelp() {
  Serial.println("\n=== BACKLIGHTING CONTROL COMMANDS ===");
  Serial.println("b         - Toggle backlighting ON/OFF");
  Serial.print(BRIGHTNESS_MIN);
  Serial.print("-");
  Serial.print(BRIGHTNESS_MAX);
  Serial.println("    - Set brightness level (e.g., '100')");
  Serial.println("status    - Show current backlighting status");
  Serial.println("help      - Show this help message");
  Serial.println();
  Serial.println("Brightness Range:");
  Serial.println("  Minimum:  34  (0x22) - Dimmest");
  Serial.println("  Default:  200 (0xC8) - Factory setting");
  Serial.println("  Maximum:  255 (0xFF) - Brightest (experimental)");
  Serial.println("======================================\n");
}

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
  // Initialize LED pins
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  setLED(false, false, false);

  // Startup LED sequence
  setLED(true, false, false); delay(200);
  setLED(false, true, false); delay(200);
  setLED(false, false, true); delay(200);
  setLED(false, false, true);  // End with blue (OFF state)

  // Initialize Serial
  Serial.begin(115200);
  delay(1000);

  // Print banner
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("  Backlighting Control");
  Serial.println("  2020 Ram 1500 Seat Button Modules");
  Serial.println("========================================");
  Serial.print("LIN Speed: ");
  Serial.print(LIN_SPEED);
  Serial.println(" baud");
  Serial.print("Update Rate: ");
  Serial.print(1000 / MESSAGE_INTERVAL);
  Serial.println(" Hz");
  Serial.print("Default Brightness: ");
  Serial.print(currentBrightness);
  Serial.print(" (0x");
  if (currentBrightness < 16) Serial.print("0");
  Serial.print(currentBrightness, HEX);
  Serial.println(")");
  Serial.println("========================================\n");

  // Configure LIN transceiver control pins
  pinMode(LIN_FAULT, INPUT);
  pinMode(LIN_CS, OUTPUT);
  digitalWrite(LIN_CS, HIGH);  // Enable LIN transceiver

  // Initialize LIN master
  LIN.begin(LIN_SPEED);

  Serial.println("LIN Master initialized successfully!");
  Serial.println();

  Serial.println("Backlighting Commands:");
  Serial.println("  Driver ON:     0xB1 -> 0x04, [brightness]");
  Serial.println("  Driver OFF:    0xB1 -> 0x0C, 0x00");
  Serial.println("  Passenger ON:  0x32 -> 0x08, [brightness]");
  Serial.println("  Passenger OFF: 0x32 -> 0x09, 0x00");
  Serial.println();

  printHelp();

  Serial.println("Backlighting is currently: OFF");
  Serial.println("Press 'b' to toggle backlighting ON");
  Serial.println("Enter 34-255 to set brightness level");
  Serial.println("Continuously sending commands...\n");
}

// ============================================================================
// ARDUINO MAIN LOOP
// ============================================================================

void loop() {
  uint32_t now = millis();

  // Call LIN handler continuously for background operation
  LIN.handler();

  // Process any completed LIN responses
  processLINResponse();

  // Process serial input
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialInput.length() > 0) {
        processSerialCommand(serialInput);
        serialInput = "";
      }
    } else {
      serialInput += c;
    }
  }

  // Continuously send LIN messages
  if (!waitingForLIN && (now - lastMessageTime >= MESSAGE_INTERVAL)) {
    lastMessageTime = now;

    // Alternate between driver and passenger messages
    if (sendingDriver) {
      sendDriverBacklightCommand();
    } else {
      sendPassengerBacklightCommand();
    }

    sendingDriver = !sendingDriver;  // Toggle for next time
  }
}
