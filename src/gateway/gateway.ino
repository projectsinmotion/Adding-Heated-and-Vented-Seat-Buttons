/*
 * LIN-CAN Gateway for Seat Button Module
 *
 * Functions as a gateway between isolated LIN bus and live CAN bus
 * - LIN Master: Polls seat button modules and sends LED/backlight commands
 * - CAN Node: Sends button press events and receives intensity/backlight commands
 *
 * Hardware: ESP32-S3-WROOM-1-N8R8 CAN & LIN Bus Board
 * Manufacturer: SK Pang Electronics (skpang.co.uk)
 *
 * Libraries required:
 * - LIN_master_portable_Arduino by Georg Icking
 * - ESP32-TWAI-CAN by sorek
 *
 * Message Flow:
 * 1. LIN → CAN: Button presses from seat modules → CAN button press events
 * 2. CAN → LIN: CAN intensity/backlight commands → LIN LED/backlight control
 *
 * Diagnostic LED:
 * - Green: Normal operation, no activity
 * - Cyan: CAN message received
 * - Magenta: Button press detected
 * - Red: Error state
 * - Yellow: Initialization
 *
 * Author: Projects In Motion (https://github.com/projectsinmotion)
 * Date: 2025-11-13
 */

#include "LIN_master_HardwareSerial_ESP32.h"
#include <ESP32-TWAI-CAN.hpp>
#include <WiFi.h>  // Needed to disable WiFi

// ============================================================================
// HARDWARE PIN DEFINITIONS (ESP32S3 CAN & LIN Board)
// ============================================================================

// LIN pins (TJA1021T transceiver)
#define PIN_LIN_TX    10
#define PIN_LIN_RX    3
#define LIN_CS        46  // LIN chip select
#define LIN_FAULT     9   // LIN fault detection

// CAN pins (SN65HVD230 transceiver)
#define PIN_CAN_TX    11
#define PIN_CAN_RX    12

// Onboard RGB LED
#define LED_R 39
#define LED_G 38
#define LED_B 40
#define RGB_ON  LOW
#define RGB_OFF HIGH

// ============================================================================
// LIN PROTOCOL DEFINITIONS
// ============================================================================

// LIN IDs
#define LIN_ID_DRIVER_BUTTON    0xC4  // Driver button press detection
#define LIN_ID_DRIVER_LED       0xB1  // Driver LED control (includes backlighting)
#define LIN_ID_PASSENGER_BUTTON 0x80  // Passenger button press detection
#define LIN_ID_PASSENGER_LED    0x32  // Passenger LED control (includes backlighting)

// LIN configuration
#define LIN_SPEED      19200
#define LIN_VERSION    LIN_Master_Base::LIN_V2
#define LIN_POLL_INTERVAL  20  // 50Hz polling rate (milliseconds)

// Driver button press patterns (ID 0xC4)
#define DRIVER_IDLE         0x01
#define DRIVER_HEAT_PRESS   0x41
#define DRIVER_VENT_PRESS   0x11

// Passenger button press patterns (ID 0x80)
#define PASSENGER_IDLE      0x80
#define PASSENGER_HEAT_PRESS 0x88
#define PASSENGER_VENT_PRESS 0xA0

// Driver LED control patterns (ID 0xB1, Byte 1)
#define DRIVER_LED_OFF         0x0C
#define DRIVER_HEAT_HIGH       0x3C
#define DRIVER_HEAT_MEDIUM     0x2C
#define DRIVER_HEAT_LOW        0x1C
#define DRIVER_VENT_HIGH       0xCC
#define DRIVER_VENT_MEDIUM     0x8C
#define DRIVER_VENT_LOW        0x4C

// Passenger LED control patterns (ID 0x32, Byte 1)
#define PASSENGER_LED_OFF         0x09
#define PASSENGER_HEAT_HIGH       0xC9
#define PASSENGER_HEAT_MEDIUM     0x89
#define PASSENGER_HEAT_LOW        0x49
#define PASSENGER_VENT_HIGH       0x39
#define PASSENGER_VENT_MEDIUM     0x29
#define PASSENGER_VENT_LOW        0x19

// Backlighting control (Byte 2 for both IDs)
// Range: 0x00 (off) to 0xC8 (200 decimal, max brightness)
// CAN brightness values (34-200) are passed directly to LIN

// ============================================================================
// CAN PROTOCOL DEFINITIONS
// ============================================================================

// CAN IDs
#define CAN_ID_BUTTON_PRESS  0x302  // 770 decimal - Button press events
#define CAN_ID_INTENSITY     0x31E  // 798 decimal - Heat/vent intensity status
#define CAN_ID_BACKLIGHT     0x2FA  // 762 decimal - Button backlighting control

// CAN configuration
#define CAN_SPEED      125  // 125 kbps

// ============================================================================
// STATE TRACKING
// ============================================================================

// LED intensity levels for each function
uint8_t driverHeatLevel = 0;      // 0=off, 1=low, 2=medium, 3=high
uint8_t driverVentLevel = 0;
uint8_t passengerHeatLevel = 0;
uint8_t passengerVentLevel = 0;

// Backlighting state
uint8_t backlightBrightness = 0;  // 0=off, 34-200=brightness level

// Button debouncing
uint32_t lastDriverHeatPress = 0;
uint32_t lastDriverVentPress = 0;
uint32_t lastPassengerHeatPress = 0;
uint32_t lastPassengerVentPress = 0;
#define DEBOUNCE_DELAY 300  // milliseconds

// ============================================================================
// INSTANCES
// ============================================================================

// LIN master instance
LIN_Master_HardwareSerial_ESP32 LIN(Serial2, PIN_LIN_RX, PIN_LIN_TX, "Master");

// Polling state machine
enum PollState {
  POLL_DRIVER_BUTTON,
  POLL_DRIVER_LED,
  POLL_PASSENGER_BUTTON,
  POLL_PASSENGER_LED
};

PollState currentPollState = POLL_DRIVER_BUTTON;
uint32_t lastPollTime = 0;

// Diagnostic LED
uint32_t ledUpdateTime = 0;
#define LED_FLASH_DURATION 50  // milliseconds

// ============================================================================
// DIAGNOSTIC LED FUNCTIONS
// ============================================================================

void setLED(bool red, bool green, bool blue) {
  digitalWrite(LED_R, red ? RGB_ON : RGB_OFF);
  digitalWrite(LED_G, green ? RGB_ON : RGB_OFF);
  digitalWrite(LED_B, blue ? RGB_ON : RGB_OFF);
}

void flashLED(bool red, bool green, bool blue) {
  setLED(red, green, blue);
  ledUpdateTime = millis();
}

void updateDiagnosticLED() {
  // Return to green (idle) after flash duration
  if (ledUpdateTime > 0 && (millis() - ledUpdateTime) > LED_FLASH_DURATION) {
    setLED(false, true, false);  // Green = normal operation
    ledUpdateTime = 0;
  }
}

// ============================================================================
// LIN TO CAN TRANSLATION (BUTTON PRESSES)
// ============================================================================

void sendCANButtonPress(uint8_t d2, uint8_t d3) {
  CanFrame frame = {0};
  frame.identifier = CAN_ID_BUTTON_PRESS;
  frame.extd = 0;
  frame.data_length_code = 8;
  frame.data[0] = 0;
  frame.data[1] = 2;
  frame.data[2] = d2;
  frame.data[3] = d3;
  frame.data[4] = 8;
  frame.data[5] = 0;
  frame.data[6] = 0;
  frame.data[7] = 0;

  Serial.print("CAN TX: ID 0x");
  Serial.print(frame.identifier, HEX);
  Serial.print(" [");
  for (int i = 0; i < 8; i++) {
    Serial.print(frame.data[i]);
    if (i < 7) Serial.print(", ");
  }
  Serial.print("]");

  if (ESP32Can.writeFrame(frame, 10)) {
    Serial.println(" - SUCCESS");
    flashLED(true, false, true);  // Magenta flash for button press
  } else {
    Serial.println(" - FAILED!");
    flashLED(true, false, false);  // Red flash for error
  }
}

void handleDriverHeatPress() {
  uint32_t now = millis();
  if (now - lastDriverHeatPress > DEBOUNCE_DELAY) {
    lastDriverHeatPress = now;
    Serial.println("LIN: Driver Heat button pressed");
    sendCANButtonPress(16, 0);  // D2=16 (0x10), D3=0
  }
}

void handleDriverVentPress() {
  uint32_t now = millis();
  if (now - lastDriverVentPress > DEBOUNCE_DELAY) {
    lastDriverVentPress = now;
    Serial.println("LIN: Driver Vent button pressed");
    sendCANButtonPress(64, 0);  // D2=64 (0x40), D3=0
  }
}

void handlePassengerHeatPress() {
  uint32_t now = millis();
  if (now - lastPassengerHeatPress > DEBOUNCE_DELAY) {
    lastPassengerHeatPress = now;
    Serial.println("LIN: Passenger Heat button pressed");
    sendCANButtonPress(0, 4);  // D2=0, D3=4
  }
}

void handlePassengerVentPress() {
  uint32_t now = millis();
  if (now - lastPassengerVentPress > DEBOUNCE_DELAY) {
    lastPassengerVentPress = now;
    Serial.println("LIN: Passenger Vent button pressed");
    sendCANButtonPress(0, 16);  // D2=0, D3=16 (0x10)
  }
}

// ============================================================================
// CAN TO LIN TRANSLATION (INTENSITY & BACKLIGHT)
// ============================================================================

uint8_t getDriverLEDCommand() {
  // Priority: heat > vent > off
  if (driverHeatLevel > 0) {
    switch (driverHeatLevel) {
      case 1: return DRIVER_HEAT_LOW;
      case 2: return DRIVER_HEAT_MEDIUM;
      case 3: return DRIVER_HEAT_HIGH;
      default: return DRIVER_LED_OFF;
    }
  } else if (driverVentLevel > 0) {
    switch (driverVentLevel) {
      case 1: return DRIVER_VENT_LOW;
      case 2: return DRIVER_VENT_MEDIUM;
      case 3: return DRIVER_VENT_HIGH;
      default: return DRIVER_LED_OFF;
    }
  }
  return DRIVER_LED_OFF;
}

uint8_t getPassengerLEDCommand() {
  // Priority: heat > vent > off
  if (passengerHeatLevel > 0) {
    switch (passengerHeatLevel) {
      case 1: return PASSENGER_HEAT_LOW;
      case 2: return PASSENGER_HEAT_MEDIUM;
      case 3: return PASSENGER_HEAT_HIGH;
      default: return PASSENGER_LED_OFF;
    }
  } else if (passengerVentLevel > 0) {
    switch (passengerVentLevel) {
      case 1: return PASSENGER_VENT_LOW;
      case 2: return PASSENGER_VENT_MEDIUM;
      case 3: return PASSENGER_VENT_HIGH;
      default: return PASSENGER_LED_OFF;
    }
  }
  return PASSENGER_LED_OFF;
}

uint8_t getBacklightCommand() {
  // Pass CAN brightness (34-200) directly to LIN
  // 0 = off, 34-200 = variable brightness levels
  return backlightBrightness;
}

void processCANIntensityMessage(const CanFrame& frame) {
  // CAN ID 0x31E (798) - 6 bytes
  // D0 = Driver state (0=off, 1-3=heat, 4/8/12=vent)
  // D1 = Passenger state (64=off, 65-67=heat, 68/72/76=vent)
  // Both states are in the SAME message!

  if (frame.data_length_code < 2) return;

  uint8_t d0 = frame.data[0];
  uint8_t d1 = frame.data[1];

  Serial.print("CAN RX: Intensity ID 0x");
  Serial.print(frame.identifier, HEX);
  Serial.print(" [");
  uint8_t len = (frame.data_length_code < 8) ? frame.data_length_code : 8;
  for (int i = 0; i < len; i++) {
    Serial.print(frame.data[i]);
    if (i < len - 1) Serial.print(", ");
  }
  Serial.print("] - ");

  // Process DRIVER state (D0)
  if (d0 == 0) {
    driverHeatLevel = 0;
    driverVentLevel = 0;
    Serial.print("Driver OFF");
  }
  else if (d0 >= 1 && d0 <= 3) {
    driverHeatLevel = d0;
    driverVentLevel = 0;
    Serial.print("Driver Heat ");
    Serial.print(d0);
  }
  else if (d0 == 4 || d0 == 8 || d0 == 12) {
    driverVentLevel = d0 / 4;  // 4->1, 8->2, 12->3
    driverHeatLevel = 0;
    Serial.print("Driver Vent ");
    Serial.print(driverVentLevel);
  }
  else {
    Serial.print("Driver ???");
  }

  Serial.print(", ");

  // Process PASSENGER state (D1)
  if (d1 == 64) {
    passengerHeatLevel = 0;
    passengerVentLevel = 0;
    Serial.println("Passenger OFF");
  }
  else if (d1 >= 65 && d1 <= 67) {
    passengerHeatLevel = d1 - 64;  // 65->1, 66->2, 67->3
    passengerVentLevel = 0;
    Serial.print("Passenger Heat ");
    Serial.println(passengerHeatLevel);
  }
  else if (d1 == 68 || d1 == 72 || d1 == 76) {
    passengerVentLevel = (d1 - 64) / 4;  // 68->1, 72->2, 76->3
    passengerHeatLevel = 0;
    Serial.print("Passenger Vent ");
    Serial.println(passengerVentLevel);
  }
  else {
    Serial.println("Passenger ???");
  }

  flashLED(false, true, true);  // Cyan flash for CAN message
}

void processCANBacklightMessage(const CanFrame& frame) {
  // CAN ID 0x2FA (762) - 8 bytes
  // Byte 2 (D2) controls brightness (34-200)

  if (frame.data_length_code < 3) return;

  uint8_t brightness = frame.data[2];

  if (brightness != backlightBrightness) {
    backlightBrightness = brightness;
    Serial.print("CAN RX: Backlight ID 0x");
    Serial.print(frame.identifier, HEX);
    Serial.print(" brightness = ");
    Serial.println(brightness);
    flashLED(false, true, true);  // Cyan flash for CAN message
  }
}

// ============================================================================
// LIN POLLING FUNCTIONS
// ============================================================================

void pollDriverButton() {
  LIN.receiveSlaveResponse(LIN_VERSION, LIN_ID_DRIVER_BUTTON, 2);
}

void sendDriverLEDCommand() {
  uint8_t data[2];
  data[0] = getDriverLEDCommand();
  data[1] = getBacklightCommand();

  static uint8_t lastData[2] = {0xFF, 0xFF};  // Track changes
  if (data[0] != lastData[0] || data[1] != lastData[1]) {
    Serial.print("LIN TX: Driver LED 0x");
    Serial.print(LIN_ID_DRIVER_LED, HEX);
    Serial.print(" [0x");
    Serial.print(data[0], HEX);
    Serial.print(", 0x");
    Serial.print(data[1], HEX);
    Serial.println("]");
    lastData[0] = data[0];
    lastData[1] = data[1];
  }

  LIN.sendMasterRequest(LIN_VERSION, LIN_ID_DRIVER_LED, 2, data);
}

void pollPassengerButton() {
  LIN.receiveSlaveResponse(LIN_VERSION, LIN_ID_PASSENGER_BUTTON, 2);
}

void sendPassengerLEDCommand() {
  uint8_t data[2];
  data[0] = getPassengerLEDCommand();
  data[1] = getBacklightCommand();

  static uint8_t lastData[2] = {0xFF, 0xFF};  // Track changes
  if (data[0] != lastData[0] || data[1] != lastData[1]) {
    Serial.print("LIN TX: Passenger LED 0x");
    Serial.print(LIN_ID_PASSENGER_LED, HEX);
    Serial.print(" [0x");
    Serial.print(data[0], HEX);
    Serial.print(", 0x");
    Serial.print(data[1], HEX);
    Serial.println("]");
    lastData[0] = data[0];
    lastData[1] = data[1];
  }

  LIN.sendMasterRequest(LIN_VERSION, LIN_ID_PASSENGER_LED, 2, data);
}

// ============================================================================
// LIN RESPONSE PROCESSING
// ============================================================================

void processLINResponse() {
  if (LIN.getState() == LIN_Master_Base::STATE_DONE) {
    LIN_Master_Base::frame_t frameType;
    uint8_t id;
    uint8_t numData;
    uint8_t data[8];

    LIN.getFrame(frameType, id, numData, data);
    LIN_Master_Base::error_t error = LIN.getError();

    // Only process successful slave responses
    if (error == LIN_Master_Base::NO_ERROR && frameType == LIN_Master_Base::SLAVE_RESPONSE) {

      // Process driver button response
      if (id == LIN_ID_DRIVER_BUTTON && numData >= 2) {
        uint8_t buttonData = data[0];

        if (buttonData == DRIVER_HEAT_PRESS) {
          handleDriverHeatPress();
        } else if (buttonData == DRIVER_VENT_PRESS) {
          handleDriverVentPress();
        }
      }

      // Process passenger button response
      else if (id == LIN_ID_PASSENGER_BUTTON && numData >= 2) {
        uint8_t buttonData = data[0];

        if (buttonData == PASSENGER_HEAT_PRESS) {
          handlePassengerHeatPress();
        } else if (buttonData == PASSENGER_VENT_PRESS) {
          handlePassengerVentPress();
        }
      }
    }

    // Reset state machine for next frame
    LIN.resetStateMachine();
    LIN.resetError();
  }
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
  setLED(true, true, false);  // Yellow during init

  // Disable WiFi and Bluetooth for security and power savings
  WiFi.mode(WIFI_OFF);
  btStop();

  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);

  // Print banner
  Serial.println("\n========================================");
  Serial.println("   LIN-CAN Gateway");
  Serial.println("   Seat Button Module Interface");
  Serial.println("========================================");

  // Configure LIN transceiver control pins
  pinMode(LIN_FAULT, INPUT);
  pinMode(LIN_CS, OUTPUT);
  digitalWrite(LIN_CS, HIGH);  // Enable LIN transceiver

  // Initialize LIN master
  LIN.begin(LIN_SPEED);
  Serial.print("LIN Master: ");
  Serial.print(LIN_SPEED);
  Serial.println(" baud");

  // Initialize CAN bus in normal mode (not listen-only)
  Serial.println("Initializing CAN Bus...");
  Serial.printf("  TX Pin: %d, RX Pin: %d\n", PIN_CAN_TX, PIN_CAN_RX);
  Serial.printf("  Speed: %d kbps\n", CAN_SPEED);

  ESP32Can.setPins(PIN_CAN_TX, PIN_CAN_RX);
  ESP32Can.setRxQueueSize(20);  // Larger queue for busy bus
  ESP32Can.setTxQueueSize(10);

  // Normal mode config (can transmit and receive)
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
    (gpio_num_t)PIN_CAN_TX,
    (gpio_num_t)PIN_CAN_RX,
    TWAI_MODE_NORMAL  // Normal mode, not listen-only
  );
  g_config.tx_queue_len = 10;
  g_config.rx_queue_len = 20;

  if (ESP32Can.begin(ESP32Can.convertSpeed(CAN_SPEED), PIN_CAN_TX, PIN_CAN_RX, 10, 20, nullptr, &g_config, nullptr)) {
    Serial.println("CAN Bus: INITIALIZED");
    Serial.println("  Mode: NORMAL (TX + RX)");
  } else {
    Serial.println("ERROR: CAN Bus initialization failed!");
    setLED(true, false, false);  // Red = error
    while (1) { delay(1000); }  // Halt on error
  }

  Serial.println("========================================");
  Serial.println("Gateway active - ready to translate");
  Serial.println("========================================\n");

  // Set LED to green (normal operation)
  setLED(false, true, false);
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

  // Update diagnostic LED
  updateDiagnosticLED();

  // Process CAN messages (non-blocking)
  CanFrame rxFrame;
  if (ESP32Can.readFrame(rxFrame, 0)) {  // 0ms timeout = non-blocking

    // Process intensity status messages
    if (rxFrame.identifier == CAN_ID_INTENSITY) {
      processCANIntensityMessage(rxFrame);
    }
    // Process backlight control messages
    else if (rxFrame.identifier == CAN_ID_BACKLIGHT) {
      processCANBacklightMessage(rxFrame);
    }
  }

  // LIN polling state machine
  if (now - lastPollTime >= LIN_POLL_INTERVAL) {
    lastPollTime = now;

    // Only start new frame if previous one is complete
    if (LIN.getState() == LIN_Master_Base::STATE_IDLE ||
        LIN.getState() == LIN_Master_Base::STATE_DONE) {

      switch (currentPollState) {
        case POLL_DRIVER_BUTTON:
          pollDriverButton();
          currentPollState = POLL_DRIVER_LED;
          break;

        case POLL_DRIVER_LED:
          sendDriverLEDCommand();
          currentPollState = POLL_PASSENGER_BUTTON;
          break;

        case POLL_PASSENGER_BUTTON:
          pollPassengerButton();
          currentPollState = POLL_PASSENGER_LED;
          break;

        case POLL_PASSENGER_LED:
          sendPassengerLEDCommand();
          currentPollState = POLL_DRIVER_BUTTON;
          break;
      }
    }
  }
}
