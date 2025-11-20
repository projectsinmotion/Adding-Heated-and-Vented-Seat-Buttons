# Test Sketches

## Overview

This directory contains Arduino sketches used to validate LIN protocol communication, test button detection, and develop LED/backlighting control functionality. These sketches were essential for understanding the seat button module behavior before building the final gateway.

## Test Progression

The sketches were developed in this order to incrementally build understanding of the LIN protocol:

1. **backlighting_sequence.ino** - Discovery tool to reverse engineer backlighting commands (first step)
2. **buttons.ino** - Basic button press detection and LED control
3. **buttons_with_backlighting.ino** - Complete LIN master implementation combining all functionality
4. **backlighting_toggle.ino** - Backlighting control with brightness adjustment

**Note:** The backlighting_sequence tool was used first to discover which LIN messages controlled backlighting, which informed the implementation of the other sketches.

## Sketches

### backlighting_sequence.ino

**Purpose:** Discovery tool to systematically identify which LIN messages activate backlighting on the seat button modules.

**Functionality:**
- Acts as LIN master
- Sends pre-defined test messages one at a time (3 second intervals)
- User watches physical button modules for backlighting activation
- User marks which messages activate driver side (press 'd') or passenger side (press 'p')
- Generates summary report of successful messages at end of sequence

**Use Case:**
- Reverse engineering LIN protocol
- Discovering correct backlighting control messages
- Testing multiple LIN ID and data byte combinations
- Identifying which messages affect which module side

**Key Features:**
- 30+ test message variations (different IDs and data bytes)
- Interactive manual observation and marking
- Tracks driver and passenger side activations separately
- Summary report showing all successful messages
- Serial commands: 'start', 'd', 'p', 'reset', 'list', 'help'

---

### buttons.ino

**Purpose:** Validate basic LIN communication and button press detection.

**Functionality:**
- Acts as LIN master
- Polls driver and passenger button modules (IDs 0xC4, 0x80)
- Detects and logs button presses to Serial Monitor
- No LED control - buttons only

**Use Case:**
- Verify LIN bus wiring and communication
- Test button module responses
- Understand button press data patterns
- Validate debounce timing

**Key Features:**
- 50Hz polling rate (20ms intervals)
- Button press detection with 300ms debounce
- Serial logging of all button events
- RGB LED feedback: solid green (normal), flash red (heat press), flash blue (vent press)
- Heat/vent mutual exclusivity per side

---

### buttons_with_backlighting.ino

**Purpose:** Complete LIN master implementation combining all functionality. This sketch served as the foundation for the final gateway.

**Functionality:**
- Acts as LIN master
- Polls both button modules continuously
- Controls LEDs based on button presses
- Controls backlighting independently
- Implements full heat/vent cycling logic
- Button debounce and state management

**Use Case:**
- Final validation of complete LIN protocol
- Test all button and LED combinations
- Verify button-to-LED mapping
- Validate backlighting independence
- Demonstrate end-to-end functionality

**Key Features:**
- **Button Detection:** All four buttons (driver heat/vent, passenger heat/vent)
- **LED Control:** Full heat/vent level cycling (off → high → medium → low → off)
- **Backlighting:** Independent control via Serial command
- **Mutual Exclusivity:** Heat and vent cannot be active simultaneously on same side
- **RGB Diagnostic LED:** Visual feedback for button presses and backlight state
- **State Machine:** Proper LIN polling cycle (button RX → LED TX → button RX → LED TX)
- **Debounce:** 300ms debounce prevents multiple triggers
- **Serial Interface:** Commands for backlighting control and status display

**Button Logic:**
```
Press 1: Off → High (3 LEDs)
Press 2: High → Medium (2 LEDs)
Press 3: Medium → Low (1 LED)
Press 4: Low → Off
```

**Serial Commands:**
- `b` - Toggle backlighting on/off
- `status` - Display current system state
- `help` - Show available commands

**RGB LED Indicators:**
- Green: Normal operation, backlight off
- Cyan: Normal operation, backlight on
- Red flash: Heat button pressed
- Blue flash: Vent button pressed

---

### backlighting_toggle.ino

**Purpose:** Validate LIN backlighting control with on/off toggle and variable brightness control.

**Functionality:**
- Acts as LIN master
- Sends LED/backlight commands to button modules (IDs 0xB1, 0x32)
- Serial command `b` toggles backlighting on/off
- Numeric input (34-255) sets brightness level
- LEDs remain off - backlighting only

**Use Case:**
- Verify backlighting byte ranges (0x00 = off, 0x22 = minimum, 0xC8 = factory default, 0xFF = maximum)
- Test continuous command sending requirement
- Validate independent driver/passenger control
- Experiment with brightness levels
- Understand LIN command structure

**Key Features:**
- Continuous LIN command transmission at 20Hz (required for backlighting)
- Variable brightness control (34-255 decimal range)
- Serial command interface: 'b' (toggle), numeric value (brightness), 'status', 'help'
- Tests backlighting without LED state changes
- RGB LED shows state (green=on, blue=off)

---

## Common Configuration

All test sketches use the same hardware configuration:

### Pin Definitions
```cpp
// LIN pins
#define PIN_LIN_TX    10
#define PIN_LIN_RX    3
#define LIN_CS        46
#define LIN_FAULT     9

// RGB LED
#define LED_R 39
#define LED_G 38
#define LED_B 40
```

### LIN Protocol
```cpp
#define LIN_SPEED      19200
#define LIN_VERSION    LIN_Master_Base::LIN_V2
#define POLL_INTERVAL  20  // 50Hz
```

### LIN Message IDs
```cpp
// Button detection (slave responses)
#define LIN_ID_DRIVER_BUTTON    0xC4
#define LIN_ID_PASSENGER_BUTTON 0x80

// LED/backlight control (master requests)
#define LIN_ID_DRIVER_LED       0xB1
#define LIN_ID_PASSENGER_LED    0x32
```

## Usage

### 1. Select Test Sketch

Choose the appropriate sketch based on what you want to test:
- Start with **backlighting_sequence.ino** for protocol discovery (if working with unknown modules)
- Use **buttons.ino** to verify basic button detection and LED control
- Use **buttons_with_backlighting.ino** for complete functionality testing
- Use **backlighting_toggle.ino** to test brightness levels

**Recommended testing order:**
1. Start with **buttons.ino** to verify LIN communication works
2. Then **buttons_with_backlighting.ino** for integrated testing
3. Finally **backlighting_toggle.ino** to test backlighting brightness


### 2. Upload and Monitor

1. Open sketch in Arduino IDE
2. Select board: **ESP32S3 Dev Module**
3. Upload to board
4. Open Serial Monitor at **115,200 baud**

### 3. Test Functionality

Follow the on-screen instructions and/or press physical buttons to test functionality.

## Development Notes

### Lessons Learned

1. **Continuous Commands Required:** Backlighting requires continuous LIN commands every 20ms, not one-time sends

2. **Two-Byte Format:** LED control messages use format `[LED_state, Backlight_brightness]`

3. **Checksum Handling:** Enhanced LIN checksum works correctly with the LIN library

4. **State Machine Timing:** Proper polling cycle with 20ms intervals provides reliable communication

5. **Debounce Necessity:** 300ms debounce prevents double-triggers from mechanical buttons

6. **Heat/Vent Exclusivity:** Activating heat automatically turns off vent (and vice versa) on the same side

### Protocol Discoveries

These test sketches helped discover:
- **backlighting_sequence.ino** identified the correct LIN IDs (0xB1 driver, 0x32 passenger) and data format for backlighting
- Exact LED command bytes for all 7 states per side (3 heat levels, 3 vent levels, off)
- Backlight byte range (0x00 = off, 0x22 = minimum, 0xC8 = factory default, 0xFF = maximum)
- Two-byte message format: [LED_control, Backlight_brightness]
- Button press patterns (0x41 driver heat, 0x11 driver vent, 0x88 passenger heat, 0xA0 passenger vent)
- Idle state patterns (0x01 driver, 0x80 passenger)
- Required polling frequency for responsive buttons (50Hz / 20ms intervals)
- Independent control of driver/passenger sides
- Continuous command transmission required (not one-shot)

### Progression to Gateway

The **buttons_with_backlighting.ino** sketch was the direct predecessor to the gateway:
- LIN communication code was kept largely intact
- Button press handlers were modified to generate CAN messages instead of cycling LEDs
- LED command generation was modified to use CAN-derived state instead of local state
- CAN communication was added for bidirectional protocol translation

## Troubleshooting

### No button detection
1. Check LIN CS pin (should be HIGH to enable transceiver)
2. Verify LIN wiring (RX to bus)
3. Ensure button modules are powered
4. Check Serial Monitor for polling activity

### LEDs not responding
1. Verify continuous command sending (not one-shot)
2. Check LIN TX pin connection
3. Ensure correct LIN IDs (0xB1 for driver, 0x32 for passenger)
4. Verify data byte values match protocol

### Backlighting issues
1. Confirm byte 2 is being sent (not just byte 1)
2. Check brightness value (0xC8 for full, not 0x00)
3. Ensure continuous commands at 20ms intervals
4. Test with backlighting_toggle.ino first

## Hardware Requirements

- ESP32-S3-WROOM-1-N8R8 CAN & LIN Bus Board
- Heated/ventilated seat button modules (driver and/or passenger)
- 12V power supply for button modules
- LIN bus connections

## Library Requirements

- **LIN_master_portable_Arduino** by Georg Icking
  - GitHub: https://github.com/gandrewstone/LIN

## Author

[Projects In Motion](https://github.com/projectsinmotion), 2025
