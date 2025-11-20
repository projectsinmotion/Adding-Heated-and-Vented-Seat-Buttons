# LIN Bus Protocol Analysis Results
## 2020 Ram 1500 Seat Button Module

### Executive Summary
This document outlines the findings from analyzing 10 LIN bus captures from a 2020 Ram 1500, focusing on driver and passenger seat button modules for heated and ventilated seat controls.

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

### Heat Control (ID 0xB1)
The driver's side heat control uses **LIN ID 0xB1** with varying data bytes to command LED level:

| LED Level | Data Bytes | Checksum | Description |
|-----------|------------|----------|-------------|
| Off | `0x0C, 0x00` | `0x42` | Heat disabled (baseline) |
| High (3 LEDs) | `0x3C, 0x00` | `0x12` | Maximum heat setting |
| Medium (2 LEDs) | `0x2C, 0x00` | `0x22` | Medium heat setting |
| Low (1 LED) | `0x1C, 0x00` | `0x32` | Minimum heat setting |

**Pattern observed:** Data byte decrements by `0x10` for each level after starting at high:
- `0x3C` (high) → `0x2C` (medium) → `0x1C` (low) →  `0x0C` (off)

### Ventilation Control (ID 0xB1)
The driver's side ventilation also uses **LIN ID 0xB1** with different data values:

| LED Level | Data Bytes | Checksum | Description |
|-----------|------------|----------|-------------|
| Off | `0x0C, 0x00` | `0x42` | Vent disabled (baseline) |
| High (3 LEDs) | `0xCC, 0x00` | `0x81` | Maximum vent setting |
| Medium (2 LEDs) | `0x8C, 0x00` | `0xC1` | Medium vent setting |
| Low (1 LED) | `0x4C, 0x00` | `0x02` | Minimum vent setting |

**Pattern observed:** Data byte decrements by `0x40` for each level after starting at high:
- `0xCC` (high) → `0x8C` (medium) → `0x4C` → (low) `0x0C` (off)

### Button Press Detection (ID 0xC4)
Driver button presses are detected via **LIN ID 0xC4**:

| Button | Data Bytes | Checksum | Description |
|--------|------------|----------|-------------|
| Heat button | `0x41, 0xFF` | `0xF9` | Heat button press event |
| Vent button | `0x11, 0xFF` | `0x2A` | Vent button press event |
| No press | `0x01, 0xFF` | `0x3A` | Idle state with module connected |
| No module | (empty) | `0x00` | No button module present |

---

## Passenger Side Button Controls

### Heat Control (ID 0x32)
The passenger's side heat control uses **LIN ID 0x32**:

| LED Level | Data Bytes | Checksum | Description |
|-----------|------------|----------|-------------|
| Off | `0x09, 0x00` | `0xC4` | Heat disabled (baseline) |
| High (3 LEDs) | `0xC9, 0x00` | `0x04` | Maximum heat setting |
| Medium (2 LEDs) | `0x89, 0x00` | `0x44` | Medium heat setting |
| Low (1 LED) | `0x49, 0x00` | `0x84` | Minimum heat setting |

**Pattern observed:** Data byte decrements by `0x40` for each level after starting at high:
- `0xC9` (high) → `0x89` (medium) → `0x49` (low) → `0x09` (off)

### Ventilation Control (ID 0x32)
Passenger ventilation also uses **LIN ID 0x32**:

| LED Level | Data Bytes | Checksum | Description |
|-----------|------------|----------|-------------|
| Off | `0x09, 0x00` | `0xC4` | Vent disabled (baseline) |
| High (3 LEDs) | `0x39, 0x00` | `0x94` | Maximum vent setting |
| Medium (2 LEDs) | `0x29, 0x00` | `0xA4` | Medium vent setting |
| Low (1 LED) | `0x19, 0x00` | `0xB4` | Minimum vent setting |

**Pattern observed:** Data byte decrements by `0x10` for each level after starting at high:
- `0x39` (high) → `0x29` (medium) → `0x19` (low) → `0x09` (off)

### Button Press Detection (ID 0x80)
Passenger button presses are detected via **LIN ID 0x80**:

| Button | Data Bytes | Checksum | Description |
|--------|------------|----------|-------------|
| Heat button | `0x88, 0xFF` | `0xF6` | Heat button press event |
| Vent button | `0xA0, 0xFF` | `0xDE` | Vent button press event |
| No press | `0x80, 0xFF` | `0x00` | Idle state with module connected |
| No module | (empty) | `0x00` | No button module present |

---

## Backlighting Control

**✅ CONFIRMED via active testing and implementation**

Backlighting is controlled via the **same LIN messages** used for LED control, using a **two-byte format** where the second byte controls backlighting state.

### Message Format Discovery

Each LED control message uses a 2-byte format:
```
[Byte 1: LED State, Byte 2: Backlighting]
```

### Confirmed Backlighting Commands

#### Driver Side (ID 0xB1)

| LED State | Backlight | Byte 1 | Byte 2 | Description |
|-----------|-----------|--------|--------|-------------|
| Off | Off | `0x0C` | `0x00` | Standard off state |
| Off | **ON** | `0x0C` | `0xC8` | Backlight on, LEDs off |
| Heat High | Off | `0x3C` | `0x00` | Heat level 3, no backlight |
| Heat High | **ON** | `0x3C` | `0xC8` | Heat level 3 + backlight |
| Heat Medium | Off | `0x2C` | `0x00` | Heat level 2, no backlight |
| Heat Medium | **ON** | `0x2C` | `0xC8` | Heat level 2 + backlight |
| Heat Low | Off | `0x1C` | `0x00` | Heat level 1, no backlight |
| Heat Low | **ON** | `0x1C` | `0xC8` | Heat level 1 + backlight |
| Vent High | Off | `0xCC` | `0x00` | Vent level 3, no backlight |
| Vent High | **ON** | `0xCC` | `0xC8` | Vent level 3 + backlight |
| Vent Medium | Off | `0x8C` | `0x00` | Vent level 2, no backlight |
| Vent Medium | **ON** | `0x8C` | `0xC8` | Vent level 2 + backlight |
| Vent Low | Off | `0x4C` | `0x00` | Vent level 1, no backlight |
| Vent Low | **ON** | `0x4C` | `0xC8` | Vent level 1 + backlight |

#### Passenger Side (ID 0x32)

| LED State | Backlight | Byte 1 | Byte 2 | Description |
|-----------|-----------|--------|--------|-------------|
| Off | Off | `0x09` | `0x00` | Standard off state |
| Off | **ON** | `0x09` | `0xC8` | Backlight on, LEDs off |
| Heat High | Off | `0xC9` | `0x00` | Heat level 3, no backlight |
| Heat High | **ON** | `0xC9` | `0xC8` | Heat level 3 + backlight |
| Heat Medium | Off | `0x89` | `0x00` | Heat level 2, no backlight |
| Heat Medium | **ON** | `0x89` | `0xC8` | Heat level 2 + backlight |
| Heat Low | Off | `0x49` | `0x00` | Heat level 1, no backlight |
| Heat Low | **ON** | `0x49` | `0xC8` | Heat level 1 + backlight |
| Vent High | Off | `0x39` | `0x00` | Vent level 3, no backlight |
| Vent High | **ON** | `0x39` | `0xC8` | Vent level 3 + backlight |
| Vent Medium | Off | `0x29` | `0x00` | Vent level 2, no backlight |
| Vent Medium | **ON** | `0x29` | `0xC8` | Vent level 2 + backlight |
| Vent Low | Off | `0x19` | `0x00` | Vent level 1, no backlight |
| Vent Low | **ON** | `0x19` | `0xC8` | Vent level 1 + backlight |

### Backlighting Control Byte

**Byte 2** controls backlighting state:
- `0xC8` = Backlighting **ON**
- `0x00` = Backlighting **OFF**

This byte is **independent** of LED state and can be combined with any LED level, including when LEDs are off.

### Discovery Method

Backlighting commands were discovered through systematic testing:
1. **Initial captures** showed correlation between 0xB1/0x32 messages and backlighting state
2. **Active testing** (27 test messages) identified that byte 2 controls backlighting
3. **Integration testing** confirmed:
   - Byte 2 = `0xC8` activates backlighting
   - Byte 2 = `0x00` deactivates backlighting
   - Byte 2 is independent and works with any LED state (including off)
4. **Final verification** proved the LED control byte remains constant:
   - Driver LEDs off: `0x0C, 0xC8` (backlight on) or `0x0C, 0x00` (backlight off)
   - Passenger LEDs off: `0x09, 0xC8` (backlight on) or `0x09, 0x00` (backlight off)

### Implementation Notes

1. **Continuous commanding required**: Backlighting must be continuously commanded to maintain state
2. **Both sides independent**: Driver (0xB1) and passenger (0x32) are controlled separately
3. **Compatible with LED control**: Backlighting and LED states can be combined in a single message
4. **Update rate**: 20Hz (50ms interval) recommended for stable backlighting

---

## Timing Analysis

### Button Press to LED Response Delay
**Finding:** LED state changes occur approximately **0.14 to 0.29 seconds** after button press detection.

#### Example from Driver Heat Press (Capture 03):
- **Button press (0xC4, 0x41,0xFF)**: 2.753 seconds
- **LED change (0xB1, 0x3C,0x00)**: 3.043 seconds
- **Delay**: 0.290 seconds (~290 milliseconds)

Additional samples confirm delays in the range of **140-290 milliseconds**, averaging around **~250ms**.

### Backlight Response Time
Backlight changes appear to be **near-instantaneous** (within same message frame) when triggered, as they occur within the normal LIN frame timing (~20ms frame periods).

---

## System Architecture

### Message Flow Diagram
```
Button Press Detection (0xC4 or 0x80)
    ↓  (~250ms delay)
LED State Update (0xB1 or 0x32)
```

### Message ID Summary

| LIN ID | Direction | Function | Update Frequency |
|--------|-----------|----------|------------------|
| **0x32** | **Master →** | **Passenger LED + backlighting control** | **Continuous (20Hz)** |
| **0x80** | **Slave →** | **Passenger button press detection** | **Event-driven** |
| **0xB1** | **Master →** | **Driver LED + backlighting control** | **Continuous (20Hz)** |
| **0xC4** | **Slave →** | **Driver button press detection** | **Event-driven** |

---

## Analysis Methodology

### Tools Created
1. **lin_parser.py** - Parses raw UART data into structured LIN frames
2. **lin_message_grouper.py** - Groups frames by ID and identifies unique message patterns
3. **lin_comparison.py** - Compares captures to identify state-dependent messages

### Process Flow
```
Raw UART Capture (10 files)
    ↓ [lin_parser.py]
Parsed CSV (ID, Data, Checksum, Timestamps)
    ↓ [lin_message_grouper.py]
Grouped Messages (Unique patterns with occurrence counts)
    ↓ [lin_comparison.py]
Differential Analysis (Messages unique to each condition)
    ↓ [Manual Analysis]
Protocol Documentation (this file)
```

---

**Document Version:** 1.3
**Last Updated:** 2025-11-13 (Revised)
**Analyst:** Claude Code
**Vehicle:** 2020 Ram 1500

### Revision History
- **v1.3** (2025-11-13): Corrected backlight-only mode - confirmed that standard LED off bytes (0x0C/0x09) work with backlight byte (0xC8); no special case required
- **v1.2** (2025-11-13): Added comprehensive backlighting control documentation from active testing; removed irrelevant IDs from other LIN modules; streamlined documentation to focus on seat button module functionality
- **v1.1** (2025-11-12): Updated parser implementation notes, corrected mode cycling patterns (Off → High → Medium → Low → Off), reordered tables to reflect cycling sequence
- **v1.0** (2025-11-12): Initial analysis and documentation
