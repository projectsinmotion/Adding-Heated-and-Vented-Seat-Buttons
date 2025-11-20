/*
 * LIN Master - Seat Button, LED & Backlighting Controller
 *
 * Controls heated/ventilated seat LEDs AND backlighting based on button presses
 * for seat button modules (driver and passenger)
 *
 * Hardware: ESP32-S3-WROOM-1-N8R8 CAN & LIN Bus Board
 * Manufacturer: SK Pang Electronics (skpang.co.uk)
 *
 * Libraries required:
 * - LIN_master_portable_Arduino by Georg Icking
 *
 * Button Logic:
 * - Each button press cycles through: Off(0) -> High(3) -> Medium(2) -> Low(1) -> Off(0)
 * - Driver heat button -> controls driver heat LEDs
 * - Driver vent button -> controls driver vent LEDs
 * - Passenger heat button -> controls passenger heat LEDs
 * - Passenger vent button -> controls passenger vent LEDs
 *
 * Backlighting Control:
 * - Press 'b' in Serial Monitor to toggle backlighting ON/OFF
 * - Backlighting is independent from LED state
 * - Both controlled via same LIN message (different bytes)
 *
 * Board LED Indicators:
 * - Solid green: Normal operation (backlight off)
 * - Solid cyan: Normal operation (backlight on)
 * - Flash blue: Vent button pressed
 * - Flash red: Heat button pressed
 *
 * Message Format:
 * - ID 0xB1 (driver): [LED_control, Backlight_control]
 * - ID 0x32 (passenger): [LED_control, Backlight_control]
 * - Byte 1: LED state (heat/vent levels)
 * - Byte 2: Backlighting (0xC8=ON, 0x00=OFF)
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

// LIN IDs
#define LIN_ID_DRIVER_BUTTON    0xC4  // Driver button press detection
#define LIN_ID_DRIVER_LED       0xB1  // Driver LED control (includes backlighting)
#define LIN_ID_PASSENGER_BUTTON 0x80  // Passenger button press detection
#define LIN_ID_PASSENGER_LED    0x32  // Passenger LED control (includes backlighting)

// LIN configuration
#define LIN_SPEED      19200
#define LIN_VERSION    LIN_Master_Base::LIN_V2

// Polling interval (milliseconds)
#define POLL_INTERVAL  20  // 50Hz polling rate

// LED flash duration (milliseconds)
#define FLASH_DURATION 100

// ============================================================================
// BUTTON STATE DEFINITIONS
// ============================================================================

// LED brightness levels
enum LEDLevel {
  LED_OFF = 0,
  LED_LOW = 1,
  LED_MEDIUM = 2,
  LED_HIGH = 3
};

// Button types
enum ButtonType {
  BUTTON_HEAT,
  BUTTON_VENT
};

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
#define DRIVER_BACKLIGHT_ONLY  0x0C  // When LEDs off but backlight on

// Passenger LED control patterns (ID 0x32, Byte 1)
#define PASSENGER_LED_OFF         0x09
#define PASSENGER_HEAT_HIGH       0xC9
#define PASSENGER_HEAT_MEDIUM     0x89
#define PASSENGER_HEAT_LOW        0x49
#define PASSENGER_VENT_HIGH       0x39
#define PASSENGER_VENT_MEDIUM     0x29
#define PASSENGER_VENT_LOW        0x19
#define PASSENGER_BACKLIGHT_ONLY  0x09  // When LEDs off but backlight on

// Backlighting control (Byte 2 for both IDs)
#define BACKLIGHT_ON   0xC8
#define BACKLIGHT_OFF  0x00

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// LIN master instance
LIN_Master_HardwareSerial_ESP32 LIN(Serial2, PIN_LIN_RX, PIN_LIN_TX, "Master");

// LED state tracking
LEDLevel driverHeatLevel = LED_OFF;
LEDLevel driverVentLevel = LED_OFF;
LEDLevel passengerHeatLevel = LED_OFF;
LEDLevel passengerVentLevel = LED_OFF;

// Backlighting state
bool backlightingOn = false;

// Button press debouncing
uint32_t lastDriverHeatPress = 0;
uint32_t lastDriverVentPress = 0;
uint32_t lastPassengerHeatPress = 0;
uint32_t lastPassengerVentPress = 0;
#define DEBOUNCE_DELAY 300  // milliseconds

// Polling state machine
enum PollState {
  POLL_DRIVER_BUTTON,
  POLL_DRIVER_LED,
  POLL_PASSENGER_BUTTON,
  POLL_PASSENGER_LED
};

PollState currentPollState = POLL_DRIVER_BUTTON;
uint32_t lastPollTime = 0;

// Board LED flash control
uint32_t flashStartTime = 0;
bool flashActive = false;
bool flashRed = false;  // true = red (heat), false = blue (vent)

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
  if (flashActive) {
    if (millis() - flashStartTime < FLASH_DURATION) {
      // Flash active
      if (flashRed) {
        setLED(true, false, false);  // Red for heat
      } else {
        setLED(false, false, true);  // Blue for vent
      }
    } else {
      // Flash complete, return to normal state
      flashActive = false;
      if (backlightingOn) {
        setLED(false, true, true);  // Cyan = backlight on
      } else {
        setLED(false, true, false);  // Green = backlight off
      }
    }
  } else {
    // Normal operation
    if (backlightingOn) {
      setLED(false, true, true);  // Cyan = backlight on
    } else {
      setLED(false, true, false);  // Green = backlight off
    }
  }
}

void triggerFlash(bool isHeat) {
  flashActive = true;
  flashRed = isHeat;
  flashStartTime = millis();
}

// ============================================================================
// LED STATE MANAGEMENT
// ============================================================================

LEDLevel cycleLevel(LEDLevel current) {
  // Cycle: Off(0) -> High(3) -> Medium(2) -> Low(1) -> Off(0)
  switch (current) {
    case LED_OFF:    return LED_HIGH;
    case LED_HIGH:   return LED_MEDIUM;
    case LED_MEDIUM: return LED_LOW;
    case LED_LOW:    return LED_OFF;
    default:         return LED_OFF;
  }
}

uint8_t getDriverLEDCommand(ButtonType btnType, LEDLevel level) {
  if (btnType == BUTTON_HEAT) {
    switch (level) {
      case LED_OFF:    return DRIVER_LED_OFF;
      case LED_LOW:    return DRIVER_HEAT_LOW;
      case LED_MEDIUM: return DRIVER_HEAT_MEDIUM;
      case LED_HIGH:   return DRIVER_HEAT_HIGH;
      default:         return DRIVER_LED_OFF;
    }
  } else {  // BUTTON_VENT
    switch (level) {
      case LED_OFF:    return DRIVER_LED_OFF;
      case LED_LOW:    return DRIVER_VENT_LOW;
      case LED_MEDIUM: return DRIVER_VENT_MEDIUM;
      case LED_HIGH:   return DRIVER_VENT_HIGH;
      default:         return DRIVER_LED_OFF;
    }
  }
}

uint8_t getPassengerLEDCommand(ButtonType btnType, LEDLevel level) {
  if (btnType == BUTTON_HEAT) {
    switch (level) {
      case LED_OFF:    return PASSENGER_LED_OFF;
      case LED_LOW:    return PASSENGER_HEAT_LOW;
      case LED_MEDIUM: return PASSENGER_HEAT_MEDIUM;
      case LED_HIGH:   return PASSENGER_HEAT_HIGH;
      default:         return PASSENGER_LED_OFF;
    }
  } else {  // BUTTON_VENT
    switch (level) {
      case LED_OFF:    return PASSENGER_LED_OFF;
      case LED_LOW:    return PASSENGER_VENT_LOW;
      case LED_MEDIUM: return PASSENGER_VENT_MEDIUM;
      case LED_HIGH:   return PASSENGER_VENT_HIGH;
      default:         return PASSENGER_LED_OFF;
    }
  }
}

// ============================================================================
// BUTTON PRESS HANDLERS
// ============================================================================

void handleDriverHeatPress() {
  uint32_t now = millis();
  if (now - lastDriverHeatPress > DEBOUNCE_DELAY) {
    lastDriverHeatPress = now;

    // Turn off vent if active
    if (driverVentLevel != LED_OFF) {
      driverVentLevel = LED_OFF;
    }

    // Cycle heat level
    driverHeatLevel = cycleLevel(driverHeatLevel);

    // Trigger red flash
    triggerFlash(true);

    Serial.print("Driver Heat: ");
    Serial.println(driverHeatLevel);
  }
}

void handleDriverVentPress() {
  uint32_t now = millis();
  if (now - lastDriverVentPress > DEBOUNCE_DELAY) {
    lastDriverVentPress = now;

    // Turn off heat if active
    if (driverHeatLevel != LED_OFF) {
      driverHeatLevel = LED_OFF;
    }

    // Cycle vent level
    driverVentLevel = cycleLevel(driverVentLevel);

    // Trigger blue flash
    triggerFlash(false);

    Serial.print("Driver Vent: ");
    Serial.println(driverVentLevel);
  }
}

void handlePassengerHeatPress() {
  uint32_t now = millis();
  if (now - lastPassengerHeatPress > DEBOUNCE_DELAY) {
    lastPassengerHeatPress = now;

    // Turn off vent if active
    if (passengerVentLevel != LED_OFF) {
      passengerVentLevel = LED_OFF;
    }

    // Cycle heat level
    passengerHeatLevel = cycleLevel(passengerHeatLevel);

    // Trigger red flash
    triggerFlash(true);

    Serial.print("Passenger Heat: ");
    Serial.println(passengerHeatLevel);
  }
}

void handlePassengerVentPress() {
  uint32_t now = millis();
  if (now - lastPassengerVentPress > DEBOUNCE_DELAY) {
    lastPassengerVentPress = now;

    // Turn off heat if active
    if (passengerHeatLevel != LED_OFF) {
      passengerHeatLevel = LED_OFF;
    }

    // Cycle vent level
    passengerVentLevel = cycleLevel(passengerVentLevel);

    // Trigger blue flash
    triggerFlash(false);

    Serial.print("Passenger Vent: ");
    Serial.println(passengerVentLevel);
  }
}

// ============================================================================
// LIN POLLING FUNCTIONS
// ============================================================================

void pollDriverButton() {
  // Request slave response from driver button module
  LIN.receiveSlaveResponse(LIN_VERSION, LIN_ID_DRIVER_BUTTON, 2);
}

void sendDriverLEDCommand() {
  uint8_t data[2];

  // Byte 1: LED control
  if (driverHeatLevel != LED_OFF) {
    data[0] = getDriverLEDCommand(BUTTON_HEAT, driverHeatLevel);
  } else if (driverVentLevel != LED_OFF) {
    data[0] = getDriverLEDCommand(BUTTON_VENT, driverVentLevel);
  } else {
    // LEDs are off - check if we need backlight-only mode
    if (backlightingOn) {
      data[0] = DRIVER_BACKLIGHT_ONLY;  // 0x04 - special backlight-only byte
    } else {
      data[0] = DRIVER_LED_OFF;  // 0x0C - standard off
    }
  }

  // Byte 2: Backlighting control
  data[1] = backlightingOn ? BACKLIGHT_ON : BACKLIGHT_OFF;

  // Send master request to control driver LEDs and backlighting
  LIN.sendMasterRequest(LIN_VERSION, LIN_ID_DRIVER_LED, 2, data);
}

void pollPassengerButton() {
  // Request slave response from passenger button module
  LIN.receiveSlaveResponse(LIN_VERSION, LIN_ID_PASSENGER_BUTTON, 2);
}

void sendPassengerLEDCommand() {
  uint8_t data[2];

  // Byte 1: LED control
  if (passengerHeatLevel != LED_OFF) {
    data[0] = getPassengerLEDCommand(BUTTON_HEAT, passengerHeatLevel);
  } else if (passengerVentLevel != LED_OFF) {
    data[0] = getPassengerLEDCommand(BUTTON_VENT, passengerVentLevel);
  } else {
    // LEDs are off - check if we need backlight-only mode
    if (backlightingOn) {
      data[0] = PASSENGER_BACKLIGHT_ONLY;  // 0x08 - special backlight-only byte
    } else {
      data[0] = PASSENGER_LED_OFF;  // 0x09 - standard off
    }
  }

  // Byte 2: Backlighting control
  data[1] = backlightingOn ? BACKLIGHT_ON : BACKLIGHT_OFF;

  // Send master request to control passenger LEDs and backlighting
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
        // Ignore DRIVER_IDLE (0x01)
      }

      // Process passenger button response
      else if (id == LIN_ID_PASSENGER_BUTTON && numData >= 2) {
        uint8_t buttonData = data[0];

        if (buttonData == PASSENGER_HEAT_PRESS) {
          handlePassengerHeatPress();
        } else if (buttonData == PASSENGER_VENT_PRESS) {
          handlePassengerVentPress();
        }
        // Ignore PASSENGER_IDLE (0x80)
      }
    }

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
  cmd.toLowerCase();

  if (cmd == "b") {
    // Toggle backlighting
    backlightingOn = !backlightingOn;

    Serial.println("\n========================================");
    Serial.print("BACKLIGHTING: ");
    Serial.println(backlightingOn ? "ON" : "OFF");
    Serial.println("========================================\n");

    updateBoardLED();
  }
  else if (cmd == "status") {
    Serial.println("\n=== SYSTEM STATUS ===");
    Serial.print("Backlighting: ");
    Serial.println(backlightingOn ? "ON" : "OFF");
    Serial.print("Driver Heat:  ");
    Serial.println(driverHeatLevel);
    Serial.print("Driver Vent:  ");
    Serial.println(driverVentLevel);
    Serial.print("Passenger Heat: ");
    Serial.println(passengerHeatLevel);
    Serial.print("Passenger Vent: ");
    Serial.println(passengerVentLevel);
    Serial.println("====================\n");
  }
  else if (cmd == "help") {
    Serial.println("\n=== COMMANDS ===");
    Serial.println("b      - Toggle backlighting ON/OFF");
    Serial.println("status - Show system status");
    Serial.println("help   - Show this help");
    Serial.println("================\n");
  }
  else if (cmd.length() > 0) {
    Serial.print("Unknown command: '");
    Serial.print(cmd);
    Serial.println("'");
    Serial.println("Type 'help' for available commands");
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
  setLED(true, false, false); delay(200);
  setLED(false, true, false); delay(200);
  setLED(false, false, true); delay(200);
  setLED(false, true, false);  // End with green (backlight off)

  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);

  // Print banner
  Serial.println("\n========================================");
  Serial.println("   LIN Master - Full Seat Controller");
  Serial.println("   Buttons, LEDs & Backlighting");
  Serial.println("   2020 Ram 1500");
  Serial.println("========================================");
  Serial.print("LIN Speed: ");
  Serial.print(LIN_SPEED);
  Serial.println(" baud");
  Serial.println("Poll Rate: 50 Hz (20ms interval)");
  Serial.println("========================================\n");

  // Configure LIN transceiver control pins
  pinMode(LIN_FAULT, INPUT);
  pinMode(LIN_CS, OUTPUT);
  digitalWrite(LIN_CS, HIGH);  // Enable LIN transceiver

  // Initialize LIN master
  LIN.begin(LIN_SPEED);

  Serial.println("LIN Master initialized successfully!");
  Serial.println();
  Serial.println("Polling for button presses...");
  Serial.println("Press 'b' to toggle backlighting");
  Serial.println("Press 'help' for more commands\n");
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

  // Update board LED
  updateBoardLED();

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

  // State machine for polling
  if (now - lastPollTime >= POLL_INTERVAL) {
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
