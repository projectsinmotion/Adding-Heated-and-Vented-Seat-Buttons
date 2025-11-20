/*
 * LIN Master - Backlighting Discovery Tool
 *
 * Systematically tests LIN messages to discover which command turns on
 * the seat button module backlighting for 2020 Ram 1500.
 *
 * Hardware: ESP32-S3-WROOM-1-N8R8 CAN & LIN Bus Board
 * Manufacturer: SK Pang Electronics (skpang.co.uk)
 *
 * Libraries required:
 * - LIN_master_portable_Arduino by Georg Icking
 *
 * Usage:
 * 1. Open Serial Monitor at 115200 baud
 * 2. Type 'start' and press Enter to begin testing
 * 3. Watch both driver and passenger seat button modules
 * 4. Type 'd' when driver's side backlighting turns ON
 * 5. Type 'p' when passenger's side backlighting turns ON
 * 6. After all messages tested, review which messages activated each side
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

// Test interval (milliseconds)
#define TEST_INTERVAL  3000  // 3 seconds per message

// ============================================================================
// TEST MESSAGE DEFINITIONS
// ============================================================================

// Structure to hold test messages
struct TestMessage {
  uint8_t id;
  uint8_t data[8];
  uint8_t length;
  const char* description;
};

// List of potential backlighting messages based on analysis
// Format: {ID, {data bytes}, length, "description"}
TestMessage testMessages[] = {
  // ID 0x42 - Appears active when backlighting is ON
  {0x42, {0xC8, 0xFE}, 2, "ID 0x42: Backlight enable (0xC8,0xFE)"},
  {0x42, {0xC8, 0xFF}, 2, "ID 0x42: Backlight enable variant (0xC8,0xFF)"},
  {0x42, {0x00, 0xFE}, 2, "ID 0x42: Baseline with FE second byte"},

  // ID 0xB1 - Backlight intensity control (driver side)
  {0xB1, {0x04, 0xC8}, 2, "ID 0xB1: Backlight intensity (0x04,0xC8)"},
  {0xB1, {0x08, 0xC8}, 2, "ID 0xB1: Backlight intensity variant (0x08,0xC8)"},
  {0xB1, {0x0C, 0xC8}, 2, "ID 0xB1: Backlight intensity variant (0x0C,0xC8)"},
  {0xB1, {0x04, 0x00}, 2, "ID 0xB1: Low intensity (0x04,0x00)"},

  // ID 0x32 - Passenger side backlight
  {0x32, {0x08, 0xC8}, 2, "ID 0x32: Passenger backlight (0x08,0xC8)"},
  {0x32, {0x04, 0xC8}, 2, "ID 0x32: Passenger backlight variant (0x04,0xC8)"},
  {0x32, {0x0C, 0xC8}, 2, "ID 0x32: Passenger backlight variant (0x0C,0xC8)"},

  // ID 0x1F - System status (changes when backlighting changes)
  {0x1F, {0x7B, 0xC9, 0x00, 0x0C, 0x00, 0xC0, 0xFF, 0xFF}, 8, "ID 0x1F: System status ON (0x7B...)"},
  {0x1F, {0x7A, 0xC9, 0x00, 0x0C, 0x00, 0xC0, 0xFF, 0xFF}, 8, "ID 0x1F: System status transitional (0x7A...)"},
  {0x1F, {0x7C, 0xC9, 0x00, 0x0C, 0x00, 0xC0, 0xFF, 0xFF}, 8, "ID 0x1F: System status variant (0x7C...)"},

  // Additional combinations
  {0xB1, {0x04, 0xC8}, 2, "ID 0xB1: Repeat intensity test"},
  {0x42, {0xC8, 0xFE}, 2, "ID 0x42: Repeat enable test"},

  // Try different intensity values
  {0xB1, {0x10, 0xC8}, 2, "ID 0xB1: Higher intensity (0x10,0xC8)"},
  {0xB1, {0x20, 0xC8}, 2, "ID 0xB1: Higher intensity (0x20,0xC8)"},
  {0xB1, {0xFF, 0xC8}, 2, "ID 0xB1: Max intensity (0xFF,0xC8)"},

  // Try C8 in first byte position
  {0xB1, {0xC8, 0x04}, 2, "ID 0xB1: Swapped bytes (0xC8,0x04)"},
  {0x32, {0xC8, 0x08}, 2, "ID 0x32: Swapped bytes (0xC8,0x08)"},
  {0x42, {0xFE, 0xC8}, 2, "ID 0x42: Swapped bytes (0xFE,0xC8)"},

  // Other potentially related IDs from analysis
  {0xCA, {0xFC, 0x7F}, 2, "ID 0xCA: Status message"},
  {0x0D, {0x00, 0x00}, 2, "ID 0x0D: Status message"},
  {0xC1, {0x00, 0x10, 0x00, 0xFF}, 4, "ID 0xC1: Status message"},

  // Try all zeros and all FFs
  {0x42, {0xFF, 0xFF}, 2, "ID 0x42: All FF"},
  {0xB1, {0xFF, 0xFF}, 2, "ID 0xB1: All FF"},
  {0x32, {0xFF, 0xFF}, 2, "ID 0x32: All FF"},
};

const int NUM_TEST_MESSAGES = sizeof(testMessages) / sizeof(TestMessage);

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// LIN master instance
LIN_Master_HardwareSerial_ESP32 LIN(Serial2, PIN_LIN_RX, PIN_LIN_TX, "Master");

// Test state
enum TestState {
  STATE_IDLE,
  STATE_RUNNING,
  STATE_COMPLETE
};

TestState currentState = STATE_IDLE;
int currentMessageIndex = 0;
uint32_t lastMessageTime = 0;
bool waitingForLIN = false;

// Tracking arrays for which messages activated each side
bool driverSideActive[30];   // Track driver's side activations (max 30 messages)
bool passengerSideActive[30]; // Track passenger's side activations (max 30 messages)

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

// ============================================================================
// LIN MESSAGE FUNCTIONS
// ============================================================================

void sendTestMessage(int index) {
  if (index >= NUM_TEST_MESSAGES) {
    return;
  }

  TestMessage* msg = &testMessages[index];

  // Send master request
  LIN.sendMasterRequest(LIN_VERSION, msg->id, msg->length, msg->data);
  waitingForLIN = true;

  // Print to serial
  Serial.println("========================================");
  Serial.print("Testing message #");
  Serial.print(index + 1);
  Serial.print(" of ");
  Serial.println(NUM_TEST_MESSAGES);
  Serial.println(msg->description);
  Serial.print("  ID: 0x");
  Serial.print(msg->id, HEX);
  Serial.print("  Data: ");
  for (int i = 0; i < msg->length; i++) {
    Serial.print("0x");
    if (msg->data[i] < 0x10) Serial.print("0");
    Serial.print(msg->data[i], HEX);
    if (i < msg->length - 1) Serial.print(", ");
  }
  Serial.println();
  Serial.println("Watching for backlight activation...");
  Serial.println("  Type 'd' if DRIVER side backlighting turns ON");
  Serial.println("  Type 'p' if PASSENGER side backlighting turns ON");
  Serial.println("========================================");
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

void printSummary() {
  Serial.println("\n\n");
  Serial.println("************************************************");
  Serial.println("***     BACKLIGHTING DISCOVERY COMPLETE     ***");
  Serial.println("************************************************");
  Serial.println();

  // Count how many messages were marked for each side
  int driverCount = 0;
  int passengerCount = 0;

  for (int i = 0; i < NUM_TEST_MESSAGES; i++) {
    if (driverSideActive[i]) driverCount++;
    if (passengerSideActive[i]) passengerCount++;
  }

  Serial.print("Total messages tested: ");
  Serial.println(NUM_TEST_MESSAGES);
  Serial.print("Driver side activations: ");
  Serial.println(driverCount);
  Serial.print("Passenger side activations: ");
  Serial.println(passengerCount);
  Serial.println();

  // Print driver side results
  if (driverCount > 0) {
    Serial.println("=== DRIVER SIDE BACKLIGHTING ===");
    for (int i = 0; i < NUM_TEST_MESSAGES; i++) {
      if (driverSideActive[i]) {
        TestMessage* msg = &testMessages[i];
        Serial.print("Message #");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.println(msg->description);
        Serial.print("  ID: 0x");
        Serial.print(msg->id, HEX);
        Serial.print("  Data: ");
        for (int j = 0; j < msg->length; j++) {
          Serial.print("0x");
          if (msg->data[j] < 0x10) Serial.print("0");
          Serial.print(msg->data[j], HEX);
          if (j < msg->length - 1) Serial.print(", ");
        }
        Serial.println();
      }
    }
    Serial.println();
  } else {
    Serial.println("No driver side backlighting activations detected.");
    Serial.println();
  }

  // Print passenger side results
  if (passengerCount > 0) {
    Serial.println("=== PASSENGER SIDE BACKLIGHTING ===");
    for (int i = 0; i < NUM_TEST_MESSAGES; i++) {
      if (passengerSideActive[i]) {
        TestMessage* msg = &testMessages[i];
        Serial.print("Message #");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.println(msg->description);
        Serial.print("  ID: 0x");
        Serial.print(msg->id, HEX);
        Serial.print("  Data: ");
        for (int j = 0; j < msg->length; j++) {
          Serial.print("0x");
          if (msg->data[j] < 0x10) Serial.print("0");
          Serial.print(msg->data[j], HEX);
          if (j < msg->length - 1) Serial.print(", ");
        }
        Serial.println();
      }
    }
    Serial.println();
  } else {
    Serial.println("No passenger side backlighting activations detected.");
    Serial.println();
  }

  Serial.println("Type 'start' to run the sequence again");
  Serial.println("************************************************\n");
}

void processSerialCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "start") {
    if (currentState == STATE_IDLE || currentState == STATE_COMPLETE) {
      Serial.println("\n*** STARTING BACKLIGHTING DISCOVERY SEQUENCE ***");
      Serial.print("Total messages to test: ");
      Serial.println(NUM_TEST_MESSAGES);
      Serial.println("Testing will proceed at 3 seconds per message");
      Serial.println("  Type 'd' when DRIVER side backlighting turns ON");
      Serial.println("  Type 'p' when PASSENGER side backlighting turns ON");
      Serial.println("======================================\n");

      // Clear tracking arrays
      for (int i = 0; i < 30; i++) {
        driverSideActive[i] = false;
        passengerSideActive[i] = false;
      }

      currentState = STATE_RUNNING;
      currentMessageIndex = 0;
      lastMessageTime = millis();
      setLED(false, false, true);  // Blue = testing

      // Send first message immediately
      sendTestMessage(currentMessageIndex);

    } else if (currentState == STATE_RUNNING) {
      Serial.println("Test sequence is already running!");
    }
  }
  else if (cmd == "d") {
    if (currentState == STATE_RUNNING) {
      driverSideActive[currentMessageIndex] = true;
      Serial.println("  >> MARKED: Driver side backlighting ON");
    } else {
      Serial.println("No test sequence is running. Type 'start' to begin.");
    }
  }
  else if (cmd == "p") {
    if (currentState == STATE_RUNNING) {
      passengerSideActive[currentMessageIndex] = true;
      Serial.println("  >> MARKED: Passenger side backlighting ON");
    } else {
      Serial.println("No test sequence is running. Type 'start' to begin.");
    }
  }
  else if (cmd == "help") {
    printHelp();
  }
  else if (cmd == "reset") {
    currentState = STATE_IDLE;
    currentMessageIndex = 0;
    setLED(true, false, false);  // Red = idle
    Serial.println("Test sequence reset. Type 'start' to begin.");
  }
  else if (cmd == "list") {
    Serial.println("\n=== List of Test Messages ===");
    for (int i = 0; i < NUM_TEST_MESSAGES; i++) {
      TestMessage* msg = &testMessages[i];
      Serial.print(i + 1);
      Serial.print(". ");
      Serial.println(msg->description);
    }
    Serial.println("==============================\n");
  }
  else if (cmd.length() > 0) {
    Serial.print("Unknown command: '");
    Serial.print(cmd);
    Serial.println("'");
    Serial.println("Type 'help' for available commands");
  }
}

void printHelp() {
  Serial.println("\n=== BACKLIGHTING DISCOVERY COMMANDS ===");
  Serial.println("start  - Begin testing sequence");
  Serial.println("d      - Mark driver side backlighting ON");
  Serial.println("p      - Mark passenger side backlighting ON");
  Serial.println("reset  - Stop and reset the sequence");
  Serial.println("list   - Show all test messages");
  Serial.println("help   - Show this help message");
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
  setLED(true, false, false);  // End with red (idle)

  // Initialize Serial for commands
  Serial.begin(115200);
  delay(1000);

  // Print banner
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("  Backlighting Discovery Tool");
  Serial.println("  2020 Ram 1500 Seat Button Modules");
  Serial.println("========================================");
  Serial.print("LIN Speed: ");
  Serial.print(LIN_SPEED);
  Serial.println(" baud");
  Serial.print("Total test messages: ");
  Serial.println(NUM_TEST_MESSAGES);
  Serial.println("Test interval: 3 seconds per message");
  Serial.println("========================================\n");

  // Configure LIN transceiver control pins
  pinMode(LIN_FAULT, INPUT);
  pinMode(LIN_CS, OUTPUT);
  digitalWrite(LIN_CS, HIGH);  // Enable LIN transceiver

  // Initialize LIN master
  LIN.begin(LIN_SPEED);

  Serial.println("LIN Master initialized successfully!");
  Serial.println();

  printHelp();

  Serial.println("Type 'start' to begin backlighting discovery");
  Serial.println("Watch BOTH seat button modules for backlighting");
  Serial.println("  Type 'd' when driver side turns ON");
  Serial.println("  Type 'p' when passenger side turns ON\n");
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

  // State machine for testing
  if (currentState == STATE_RUNNING) {
    // Wait for LIN transaction to complete and interval to pass
    if (!waitingForLIN && (now - lastMessageTime >= TEST_INTERVAL)) {
      lastMessageTime = now;
      currentMessageIndex++;

      if (currentMessageIndex < NUM_TEST_MESSAGES) {
        // Send next message
        sendTestMessage(currentMessageIndex);
      } else {
        // Reached end of sequence
        currentState = STATE_COMPLETE;
        setLED(false, true, false);  // Green = complete

        // Print summary of results
        printSummary();
      }
    }
  }
}
