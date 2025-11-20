# LIN-CAN Gateway

## Overview

The gateway is the final production firmware that provides bidirectional translation between LIN and CAN buses for heated/ventilated seat button integration. It operates autonomously once powered, requiring no user interaction.

## Functionality

### LIN Side (Master Mode)
- **Polls button modules** at 50Hz (20ms intervals)
  - Driver button module: LIN ID 0xC4
  - Passenger button module: LIN ID 0x80
- **Sends LED and backlighting commands** continuously
  - Driver LEDs: LIN ID 0xB1
  - Passenger LEDs: LIN ID 0x32
- **Detects button presses** with 300ms debounce

### CAN Side (Normal Mode)
- **Transmits button press events** (ID 0x302)
- **Receives heat or vent intensity status** (ID 0x31E) for both driver and passenger
- **Receives backlight command** (ID 0x2FA)

### Translation Logic

#### LIN → CAN (Button Presses)
When a button press is detected on the LIN bus, the gateway immediately generates the corresponding CAN message:

| LIN Button | Function | CAN Message Sent |
|------------|----------|------------------|
| Driver Heat (0x41) | Heat button pressed | ID 0x302: [0, 2, 16, 0, 8, 0, 0, 0] |
| Driver Vent (0x11) | Vent button pressed | ID 0x302: [0, 2, 64, 0, 8, 0, 0, 0] |
| Passenger Heat (0x88) | Heat button pressed | ID 0x302: [0, 2, 0, 4, 8, 0, 0, 0] |
| Passenger Vent (0xA0) | Vent button pressed | ID 0x302: [0, 2, 0, 16, 8, 0, 0, 0] |

#### CAN → LIN (Status Display)
The gateway maintains internal state for each function and continuously sends LIN commands based on the last received CAN status:

**State Variables:**
- `driverHeatLevel` (0-3): Driver heat intensity
- `driverVentLevel` (0-3): Driver vent intensity
- `passengerHeatLevel` (0-3): Passenger heat intensity
- `passengerVentLevel` (0-3): Passenger vent intensity
- `backlightBrightness` (0-200): Button backlight brightness

**CAN ID 0x31E Processing:**
Each message contains both driver (byte 0) and passenger (byte 1) states:

- **Byte 0 (Driver):**
  - 0 = Off
  - 1-3 = Heat Low/Medium/High
  - 4/8/12 = Vent Low/Medium/High

- **Byte 1 (Passenger):**
  - 64 = Off
  - 65-67 = Heat Low/Medium/High
  - 68/72/76 = Vent Low/Medium/High

**CAN ID 0x2FA Processing:**
- Byte 2 contains brightness (0-200 decimal)
- Passed directly to LIN backlight commands

**LIN Command Generation:**
The gateway sends two-byte LIN commands every 20ms:
- `[LED_state, backlight_brightness]`

LED state byte is determined by the corresponding heat/vent level, with heat and vent being mutually exclusive on each side.

## Code Structure

### Key Functions

#### LIN to CAN Translation
- `handleDriverHeatPress()` - Processes driver heat button with debounce
- `handleDriverVentPress()` - Processes driver vent button with debounce
- `handlePassengerHeatPress()` - Processes passenger heat button with debounce
- `handlePassengerVentPress()` - Processes passenger vent button with debounce
- `sendCANButtonPress(d2, d3)` - Constructs and transmits CAN button event

#### CAN to LIN Translation
- `processCANIntensityMessage()` - Decodes CAN intensity status and updates state
- `processCANBacklightMessage()` - Extracts backlight brightness from CAN
- `getDriverLEDCommand()` - Returns appropriate LED command byte for driver side
- `getPassengerLEDCommand()` - Returns appropriate LED command byte for passenger side
- `getBacklightCommand()` - Returns current backlight brightness value

#### LIN Communication
- `pollDriverButton()` - Requests button status from driver module
- `sendDriverLEDCommand()` - Sends LED and backlight command to driver module
- `pollPassengerButton()` - Requests button status from passenger module
- `sendPassengerLEDCommand()` - Sends LED and backlight command to passenger module
- `processLINResponse()` - Handles completed LIN transactions

#### Diagnostics
- `updateDiagnosticLED()` - Controls onboard RGB LED for status indication
- `flashLED(r, g, b)` - Triggers brief color flash
- Debug output to Serial (115,200 baud) for all message translations

### State Machine

The gateway uses a polling state machine that cycles through four states every 80ms (20ms per state):

1. **POLL_DRIVER_BUTTON** - Request driver button status (LIN RX)
2. **POLL_DRIVER_LED** - Send driver LED command (LIN TX)
3. **POLL_PASSENGER_BUTTON** - Request passenger button status (LIN RX)
4. **POLL_PASSENGER_LED** - Send passenger LED command (LIN TX)

CAN messages are processed asynchronously in the main loop using non-blocking reads.

## Configuration

### Pin Assignments
All pins are defined in the code header and match the ESP32-S3 CAN & LIN Bus Board:

```cpp
// LIN pins
#define PIN_LIN_TX    10
#define PIN_LIN_RX    3
#define LIN_CS        46
#define LIN_FAULT     9

// CAN pins
#define PIN_CAN_TX    11
#define PIN_CAN_RX    12

// RGB LED pins
#define LED_R 39
#define LED_G 38
#define LED_B 40
```

### Protocol Parameters
```cpp
#define LIN_SPEED          19200  // baud
#define LIN_POLL_INTERVAL  20     // ms (50Hz)
#define CAN_SPEED          125    // kbps
#define DEBOUNCE_DELAY     300    // ms
```

### CAN IDs
```cpp
#define CAN_ID_BUTTON_PRESS  0x302  // TX: Button events
#define CAN_ID_INTENSITY     0x31E  // RX: Heat/vent levels
#define CAN_ID_BACKLIGHT     0x2FA  // RX: Brightness
```

### LIN IDs
```cpp
#define LIN_ID_DRIVER_BUTTON    0xC4  // RX: Button presses
#define LIN_ID_DRIVER_LED       0xB1  // TX: LEDs + backlight
#define LIN_ID_PASSENGER_BUTTON 0x80  // RX: Button presses
#define LIN_ID_PASSENGER_LED    0x32  // TX: LEDs + backlight
```

## Operation

### Startup Sequence
1. RGB LED cycles through colors (red → green → blue → yellow)
2. WiFi and Bluetooth disabled for security and power savings
3. Serial console started at 115,200 baud
4. LIN master initialized at 19,200 baud
5. CAN bus initialized at 125 kbps in normal mode
6. RGB LED turns green - system ready
7. Polling begins immediately

### Normal Operation
The gateway operates continuously with the following behavior:

- **Green LED**: Idle, normal operation
- **Cyan flash**: CAN message received and processed
- **Magenta flash**: Button press detected and transmitted

Serial output provides detailed logging:
```
CAN RX: Intensity ID 0x31E [3, 67, 0, 192, 0, 0] - Driver Heat 3, Passenger Heat 3
LIN TX: Driver LED 0xB1 [0x3C, 0xC8]
LIN TX: Passenger LED 0x32 [0xC9, 0xC8]
```

### Error Conditions
- **Red LED on startup**: CAN bus initialization failed - check wiring and transceiver
- **CAN TX failures**: "FAILED!" logged to Serial - check CAN bus termination and connections
- **No button detection**: Verify LIN CS pin is HIGH and LIN transceiver is enabled

## Power Consumption

- **Typical draw**: 0.36W (0.03A @ 12V)
- **Components active**:
  - ESP32-S3 dual-core @ 240MHz
  - LIN master polling @ 50Hz
  - CAN transceiver in normal mode
  - WiFi/Bluetooth disabled

## Debugging

### Serial Monitor (115,200 baud)
Enable to see:
- All CAN messages received with decoded meaning
- All LIN commands sent (when changed)
- Button press events with timestamps
- CAN transmission success/failure status
- System initialization sequence

### Diagnostic LED
Visual feedback without Serial connection:
- Steady green = normal operation, ready
- Brief cyan flash = processing CAN message
- Brief magenta flash = button pressed, CAN sent
- Yellow = initializing
- Red = critical error (halted)

## Dependencies

- **LIN_master_portable_Arduino** library (Georg Icking)
- **ESP32-TWAI-CAN** library (handmade0octopus)
- Arduino ESP32 core (v3.x or later)

## Known Limitations

- No sleep mode implemented - continuous 0.36W draw
- No CAN message filtering - processes all bus traffic
- Fixed 300ms debounce (not configurable at runtime)
- Driver and passenger sides operate independently (cannot be linked)

## Future Enhancements

Potential improvements for future versions:
- Sleep mode triggered by vehicle power state CAN message
- Configurable debounce via Serial commands
- CAN acceptance filters to reduce processing load
- Activity logging to SD card or EEPROM
- WiFi diagnostics interface (optional, security considered)

## Author

[Projects In Motion](https://github.com/projectsinmotion), 2025
