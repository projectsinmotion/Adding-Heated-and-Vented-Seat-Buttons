# CAN Bus Protocol Analysis Results
## 2020 Ram 1500 Seat Button Module

### Executive Summary
This document outlines the findings from analyzing 7 CAN bus captures from a 2020 Ram 1500, focusing on heated and ventilated seat controls for both driver and passenger seats, as well as button backlighting control. The analysis reveals clear patterns for button press events, intensity status feedback (displayed as icons on the touchscreen), and brightness control.

---

## Table of Contents
1. [Driver Side Button Controls](#driver-side-button-controls)
2. [Passenger Side Button Controls](#passenger-side-button-controls)
3. [Backlighting Control](#backlighting-control)
4. [Timing Analysis](#timing-analysis)
5. [System Architecture](#system-architecture)
6. [Analysis Methodology](#analysis-methodology)

---

## Driver Side Button Controls

### Button Press Detection (ID 770)
Driver button presses are detected via **CAN ID 770** (0x302):

| Button | Data Bytes (D0-D7) | Description |
|--------|-------------------|-------------|
| Heat button | `0, 2, 16, 0, 8, 0, 0, 0` | Heat button press event |
| Vent button | `0, 2, 64, 0, 8, 0, 0, 0` | Vent button press event |

**Pattern observed:**
- Byte 2 (D2) distinguishes button type:
  - `16` (0x10) = Heat button
  - `64` (0x40) = Vent button

### Heat Intensity Control (ID 798)
The driver's side heat intensity status uses **CAN ID 798** (0x31E):

| Intensity Level | Data Bytes (D0-D7) | Description |
|-----------------|-------------------|-------------|
| High (3 bars) | `3, 64, 0, 192, 0, 0, , ` | Maximum heat setting |
| Medium (2 bars) | `2, 64, 0, 192, 0, 0, , ` | Medium heat setting |
| Low (1 bar) | `1, 64, 0, 192, 0, 0, , ` | Minimum heat setting |

**Pattern observed:**
- Byte 0 (D0) directly indicates intensity level: `3`, `2`, or `1`
- Byte 1 (D1) = `64` (0x40) indicates driver's side heat
- Length = 6 bytes
- Appears ~7 times per button press cycle (~every 500ms for 3.5 seconds)
- **Displayed on touchscreen:** Seat icon with red squiggly lines (e.g., 3 lines for high heat)

### Ventilation Intensity Control (ID 798)
The driver's side vent also uses **CAN ID 798** with different data values:

| Intensity Level | Data Bytes (D0-D7) | Description |
|-----------------|-------------------|-------------|
| High (3 bars) | `12, 64, 0, 192, 0, 0, , ` | Maximum vent setting |
| Medium (2 bars) | `8, 64, 0, 192, 0, 0, , ` | Medium vent setting |
| Low (1 bar) | `4, 64, 0, 192, 0, 0, , ` | Minimum vent setting |

**Pattern observed:**
- Byte 0 (D0) = multiples of 4: `12` (0x0C), `8` (0x08), `4` (0x04)
  - High (3 bars) = 12 (4 × 3)
  - Medium (2 bars) = 8 (4 × 2)
  - Low (1 bar) = 4 (4 × 1)
- Byte 1 (D1) = `64` (0x40) indicates driver's side vent
- Length = 6 bytes
- **Displayed on touchscreen:** Seat icon with blue arrows (e.g., 3 arrows for high vent)

---

## Passenger Side Button Controls

### Button Press Detection (ID 770)
Passenger button presses are also detected via **CAN ID 770** (0x302):

| Button | Data Bytes (D0-D7) | Description |
|--------|-------------------|-------------|
| Heat button | `0, 2, 0, 4, 8, 0, 0, 0` | Heat button press event |
| Vent button | `0, 2, 0, 16, 8, 0, 0, 0` | Vent button press event |

**Pattern observed:**
- Byte 3 (D3) distinguishes button type:
  - `4` (0x04) = Heat button
  - `16` (0x10) = Vent button
- Same CAN ID (770) is used for both driver and passenger, differentiated by data bytes

### Heat Intensity Control (ID 798)
The passenger's side heat intensity status uses **CAN ID 798** (0x31E):

| Intensity Level | Data Bytes (D0-D7) | Description |
|-----------------|-------------------|-------------|
| High (3 bars) | `0, 67, 0, 192, 0, 0, , ` | Maximum heat setting |
| Medium (2 bars) | `0, 66, 0, 192, 0, 0, , ` | Medium heat setting |
| Low (1 bar) | `0, 65, 0, 192, 0, 0, , ` | Minimum heat setting |

**Pattern observed:**
- Byte 0 (D0) = `0` for passenger side
- Byte 1 (D1) indicates intensity level:
  - `67` (0x43) = High (3 bars)
  - `66` (0x42) = Medium (2 bars)
  - `65` (0x41) = Low (1 bar)
- Sequential values: decrements by 1 for each level
- Length = 6 bytes
- **Displayed on touchscreen:** Passenger seat icon with red squiggly lines

### Ventilation Intensity Control (ID 798)
The passenger's side vent also uses **CAN ID 798**:

| Intensity Level | Data Bytes (D0-D7) | Description |
|-----------------|-------------------|-------------|
| High (3 bars) | `0, 76, 0, 192, 0, 0, , ` | Maximum vent setting |
| Medium (2 bars) | `0, 72, 0, 192, 0, 0, , ` | Medium vent setting |
| Low (1 bar) | `0, 68, 0, 192, 0, 0, , ` | Minimum vent setting |

**Pattern observed:**
- Byte 0 (D0) = `0` for passenger side
- Byte 1 (D1) = multiples of 4:
  - `76` (0x4C) = High (3 bars) = 64 + 12
  - `72` (0x48) = Medium (2 bars) = 64 + 8
  - `68` (0x44) = Low (1 bar) = 64 + 4
- Decrements by 4 for each level
- Length = 6 bytes
- **Displayed on touchscreen:** Passenger seat icon with blue arrows

---

## Backlighting Control

### Button Backlighting (ID 762)
Backlighting is controlled via **CAN ID 762** (0x2FA) using an 8-byte format:

| Brightness Level | Data Bytes (D0-D7) | Description |
|------------------|-------------------|-------------|
| Maximum | `0, 0, 200, 24, 0, 0, 200, 0` | Highest brightness |
| Minimum | `0, 0, 34, 24, 0, 0, 200, 0` | Lowest brightness |
| Variable | `0, 0, [34-200], 24, 0, 0, 200, 0` | Variable brightness levels |

**Pattern observed:**
- Byte 2 (D2) controls brightness level:
  - Range: `34` (minimum) to `200` (maximum)
  - Intermediate values observed: 35, 36, 37...199
  - Allows for smooth dimming control
- Byte 3 (D3) = `24` (constant)
- Byte 6 (D6) = `200` (constant)
- Length = 8 bytes

### Brightness Range
The brightness control provides a wide range from minimum to maximum:
- **Minimum brightness:** Byte 2 = `34` (0x22)
- **Maximum brightness:** Byte 2 = `200` (0xC8)
- **Range:** 166 discrete levels
- **Resolution:** ~0.6% brightness change per step

---

## Timing Analysis

### Button Press to Status Response Delay
Analysis of captures with ~3-second button press intervals reveals consistent timing patterns:

#### Example from Driver Heat Press (Capture 03):
- **Button presses (ID 770):** 2.301, 5.549, 8.629, 11.499 seconds
- **Status responses (ID 798):**
  - High (3 bars): 2.367 to 5.234 seconds (~2.9s duration)
  - Medium (2 bars): 5.604 to 8.232 seconds (~2.6s duration)
  - Low (1 bar): 8.682 to 11.230 seconds (~2.5s duration)

**Finding:** Intensity state changes occur approximately **0.05 to 0.15 seconds** after button press detection.

### Status Message Frequency
Intensity status messages (ID 798) are broadcast continuously during active states:
- **Update rate:** Approximately 7 messages per 3-second period
- **Frequency:** ~2.3 Hz (every ~430ms)
- **Purpose:** Continuous status feedback to maintain touchscreen display state

### Backlight Update Rate
Backlighting messages (ID 762) show variable update patterns:
- **During transitions:** Higher frequency updates
- **Stable state:** Lower frequency updates
- **Typical interval:** 10-50ms during active changes

---

## System Architecture

### Message Flow Diagram
```
Button Press Event (ID 770)
    ↓  (~50-150ms delay)
Intensity State Broadcast (ID 798)
    ↓  (continuous, ~430ms intervals)
Status Feedback to Touchscreen (ID 798)

Backlighting Control (ID 762)
    ↓  (independent from button press)
Brightness Adjustment
```

### CAN ID Summary

| CAN ID | Decimal | Direction | Function | Update Pattern |
|--------|---------|-----------|----------|----------------|
| **0x2FA (762)** | 762 | **System →** | **Button backlighting control** | **Continuous during changes** |
| **0x302 (770)** | 770 | **Module →** | **Button press detection (driver & passenger)** | **Event-driven** |
| **0x31E (798)** | 798 | **System →** | **Intensity status feedback (driver & passenger)** | **Continuous (~2.3 Hz)** |

### Data Byte Organization

#### ID 770 (Button Press) - 8 bytes
```
D0  D1  D2      D3      D4  D5  D6  D7
0   2   [Type1] [Type2] 8   0   0   0

Type1 (D2):
  - Driver Heat: 16 (0x10)
  - Driver Vent: 64 (0x40)
  - Passenger: 0

Type2 (D3):
  - Driver: 0
  - Passenger Heat: 4 (0x04)
  - Passenger Vent: 16 (0x10)
```

#### ID 798 (Intensity Status) - 6 bytes
```
D0          D1          D2  D3   D4  D5
[Level/Cfg] [Mode/Lvl]  0   192  0   0

Driver Side:
  D0 = Intensity level or config (1-3 for heat, 4/8/12 for vent)
  D1 = 64 (0x40)

Passenger Side:
  D0 = 0
  D1 = Intensity level code (65-67 for heat, 68-76 for vent)
```

#### ID 762 (Backlighting) - 8 bytes
```
D0  D1  D2          D3  D4  D5  D6   D7
0   0   [Brightness] 24  0   0   200  0

Brightness (D2): 34-200 (min-max)
```

---

## Analysis Methodology

### Capture Conditions
All captures were performed with approximately **3-second intervals** between test actions:
1. **Capture 00:** Baseline (no presses, no lights)
2. **Capture 01:** Lights cycled between off and on
3. **Capture 02:** Lights cycled between max and min brightness
4. **Capture 03:** Driver heat button pressed every 3s
5. **Capture 04:** Driver vent button pressed every 3s
6. **Capture 05:** Passenger heat button pressed every 3s
7. **Capture 06:** Passenger vent button pressed every 3s

### Tools Created
1. **can_message_grouper.py** - Groups CAN messages by ID and identifies unique patterns
2. **can_comparison.py** - Compares captures to identify state-dependent messages

### Process Flow
```
Raw CAN Capture (7 files)
    ↓ [can_message_grouper.py]
Grouped Messages (Unique patterns with occurrence counts)
    ↓ [can_comparison.py]
Differential Analysis (Messages unique to each condition)
    ↓ [Manual Analysis]
Protocol Documentation (this file)
```

### Key Findings from Comparison Analysis

#### Comparison: Driver Heat vs. Baseline (03 vs. 00)
- **ID 770** unique message: `0, 2, 16, 0, 8, 0, 0, 0` (4 occurrences)
- **ID 798** unique messages:
  - `3, 64, 0, 192, 0, 0, , ` (7 occurrences)
  - `2, 64, 0, 192, 0, 0, , ` (7 occurrences)
  - `1, 64, 0, 192, 0, 0, , ` (7 occurrences)

#### Comparison: Lights On vs. Off (01 vs. 00)
- **ID 762** unique message: `0, 0, 200, 24, 0, 0, 200, 0` (12 occurrences at high brightness)

#### Comparison: Lights Max vs. Min (02 vs. 01)
- **ID 762** shows 166+ unique brightness levels ranging from 34 to 200 in byte 2

---

## Conclusions

### Protocol Characteristics
1. **Shared CAN IDs:** Both driver and passenger controls use the same CAN IDs (770, 798)
2. **Data Byte Differentiation:** Side and function are distinguished by specific data bytes
3. **Encoding Patterns:**
   - Driver side uses D0 for intensity indication
   - Passenger side uses D1 for intensity indication
   - Heat uses lower numerical values
   - Vent uses higher numerical values (offset by 64 or multiplied by 4)
4. **Display Integration:** Intensity levels are displayed on touchscreen as seat icons with:
   - Red squiggly lines for heat (1-3 lines)
   - Blue arrows for ventilation (1-3 arrows)

### Practical Applications
This protocol analysis enables:
- **Custom Status Display:** Direct control of heat/vent intensity indicators on touchscreen
- **Button State Monitoring:** Real-time detection of button presses
- **Backlighting Integration:** Independent brightness control for button illumination
- **System Integration:** Integration with aftermarket climate control systems

---

**Document Version:** 1.1
**Last Updated:** 2025-11-13
**Analyst:** Claude Code
**Vehicle:** 2020 Ram 1500

### Revision History
- **v1.1** (2025-11-13): Updated terminology from "LED" to "intensity/setting/status" to accurately reflect touchscreen icon display (red squiggly lines for heat, blue arrows for vent)
- **v1.0** (2025-11-13): Initial analysis and documentation of CAN bus protocol for seat button controls
