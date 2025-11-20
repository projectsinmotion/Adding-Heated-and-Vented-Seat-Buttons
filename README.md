# LIN-CAN Gateway for Heated/Ventilated Seat Buttons

## Overview

This project implements a bidirectional protocol gateway between LIN and CAN buses to integrate repurposed OEM heated/ventilated seat button modules into a **2020 Ram 1500**. The gateway translates button presses from the LIN bus into CAN messages for the vehicle's Body Control Module (BCM), and translates heat/vent intensity and backlighting commands from the CAN bus into LIN commands for the button module LEDs and backlighting.

**Final Product:** [`src/gateway/gateway.ino`](src/gateway/gateway.ino) | [Documentation](src/gateway/README.md)

## Demo Videos

- [Button Functionality - Controlling heat and vent settings](https://youtube.com/shorts/mwQezkEFtxM)
- [Backlighting Integration - Buttons working in concert with center stack](https://youtube.com/shorts/IfwBt91azg4)

## Project Structure

```
├── src/                          # Source code
│   ├── gateway/                  # Final LIN-CAN gateway (production)
│   ├── BusSniffer/               # Bus capture tool for reverse engineering
│   └── test/                     # Test sketches for protocol validation
├── docs/                         # Documentation and analysis
│   ├── CAN_analysis/             # Python scripts for CAN protocol analysis
│   ├── LIN_analysis/             # Python scripts for LIN protocol analysis
│   └── references/               # Datasheets and connector terminations
├── HARDWARE.md                   # Hardware specifications and components
├── WIRING.md                     # Wiring diagrams and connections
├── MOUNTING.md                   # Enclosure and mounting information
├── METHODOLOGY.md                # Software development methodology and approach
└── README.md                     # This file
```

## [Hardware](HARDWARE.md)

## [Mounting](MOUNTING.md)

## [Wiring](WIRING.md)

## [Software Development](METHODOLOGY.md)

## Required Libraries

Install these libraries via Arduino Library Manager:

- **[LIN_master_portable_Arduino](https://github.com/gicking/LIN_master_portable_Arduino)** by Georg Icking
- **[ESP32-TWAI-CAN](https://github.com/handmade0octopus/ESP32-TWAI-CAN)** by sorek (handmade0octopus)

## Features

### Gateway Functionality
- **LIN to CAN:** Translates button presses to CAN events (ID 0x302)
- **CAN to LIN (LEDs):** Translates heat/vent intensity commands (ID 0x31E) to button LED states
- **CAN to LIN (Backlight):** Translates brightness commands (ID 0x2FA) to button backlighting
- **Button Debounce:** 300ms debounce prevents multiple triggers
- **Diagnostic LED:** RGB LED provides visual feedback for system state

### Protocol Details
- **LIN Bus:** 19,200 baud, LIN 2.0, 50Hz polling rate
- **CAN Bus:** 125 kbps, normal mode (TX + RX)
- **Power Consumption:** ~0.36W (0.03A @ 12V)
- **Security:** WiFi and Bluetooth disabled

## Getting Started

### 1. Hardware Setup
1. Connect the ESP32 board to the vehicle's CAN bus and 12V DC power via the center stack display screen connector C1
2. Connect the LIN bus to the seat button modules (dedicated LIN bus with only ESP32 and button modules)

### 2. Arduino IDE Configuration
Select the following board settings:
- **Board:** ESP32S3 Dev Module
- **USB CDC On Boot:** Enabled
- **PSRAM:** OPI PSRAM
- **Upload Mode:** USB-OTG CDC (TinyUSB)

### 3. Upload Gateway Firmware
1. Open `src/gateway/gateway.ino`
2. Upload to the board
3. Open Serial Monitor at 115,200 baud to view diagnostic messages

## Pin Configuration

| Function | Pin | Description |
|----------|-----|-------------|
| LIN TX | 10 | LIN transmit |
| LIN RX | 3 | LIN receive |
| LIN CS | 46 | LIN chip select (active high) |
| LIN FAULT | 9 | LIN fault detection |
| CAN TX | 11 | CAN transmit |
| CAN RX | 12 | CAN receive |
| LED R | 39 | Onboard red LED |
| LED G | 38 | Onboard green LED |
| LED B | 40 | Onboard blue LED |

## Message Translation

### Button Press Translation (LIN → CAN)

| LIN Event | LIN ID | LIN Data | → | CAN ID | CAN Data |
|-----------|--------|----------|---|--------|----------|
| Driver Heat Press | 0xC4 | 0x41, 0xFF | → | 0x302 | [0, 2, 16, 0, 8, 0, 0, 0] |
| Driver Vent Press | 0xC4 | 0x11, 0xFF | → | 0x302 | [0, 2, 64, 0, 8, 0, 0, 0] |
| Passenger Heat Press | 0x80 | 0x88, 0xFF | → | 0x302 | [0, 2, 0, 4, 8, 0, 0, 0] |
| Passenger Vent Press | 0x80 | 0xA0, 0xFF | → | 0x302 | [0, 2, 0, 16, 8, 0, 0, 0] |

### Intensity Control Translation (CAN → LIN)

CAN ID 0x31E carries both driver (D0) and passenger (D1) states simultaneously:

**Driver States (D0):**
- 0 = Off → LIN 0xB1: [0x0C, brightness]
- 1-3 = Heat Low/Med/High → LIN 0xB1: [0x1C/0x2C/0x3C, brightness]
- 4/8/12 = Vent Low/Med/High → LIN 0xB1: [0x4C/0x8C/0xCC, brightness]

**Passenger States (D1):**
- 64 = Off → LIN 0x32: [0x09, brightness]
- 65-67 = Heat Low/Med/High → LIN 0x32: [0x49/0x89/0xC9, brightness]
- 68/72/76 = Vent Low/Med/High → LIN 0x32: [0x19/0x29/0x39, brightness]

### Backlighting Translation (CAN → LIN)

CAN ID 0x2FA byte 2 (range: 0-200 decimal) is passed directly to LIN messages as the second byte.

## Diagnostic LED Indicators

| Color | Meaning |
|-------|---------|
| Green | Normal operation (idle) |
| Cyan flash | CAN message received |
| Magenta flash | Button press detected |
| Yellow | Initialization in progress |
| Red | Error state |

## Troubleshooting

### No button detection
- Verify LIN connections and CS pin (should be HIGH)
- Check Serial Monitor for LIN polling activity

### LEDs not responding to vehicle commands
- Verify CAN bus connection and termination
- Check Serial Monitor for CAN RX messages on IDs 0x31E and 0x2FA

### CAN messages not sent
- Check CAN bus initialization status
- Verify CAN TX/RX pins and bus speed (125 kbps)

## License

This project is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](http://creativecommons.org/licenses/by-nc-sa/4.0/).

You are free to:
- **Share** — copy and redistribute the material in any medium or format
- **Adapt** — remix, transform, and build upon the material

Under the following terms:
- **Attribution** — You must give appropriate credit
- **NonCommercial** — You may not use the material for commercial purposes
- **ShareAlike** — Derivatives must use the same license

See the [LICENSE](LICENSE) file for the full license text.

## Acknowledgments

- Georg Icking for the LIN_master_portable_Arduino library
- sorek (handmade0octopus) for the ESP32-TWAI-CAN library
- SK Pang Electronics for the ESP32-S3 CAN & LIN Bus Board
- Claude Code (Anthropic) for development assistance

## Author

[Projects In Motion](https://github.com/projectsinmotion), 2025
