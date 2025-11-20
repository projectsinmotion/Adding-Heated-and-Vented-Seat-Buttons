# LIN Bus Protocol Analysis Workflow

## Overview

This directory contains Python scripts and documentation for reverse-engineering the LIN bus protocol used by heated/ventilated seat button modules. The analysis workflow processes raw UART captures through parsing, grouping, and comparison stages to identify message patterns and determine protocol structure.

## Directory Structure

```
LIN_analysis/
├── 00_captures/               # Raw UART capture files from BusSniffer
│   └── [raw UART txt captures]
├── 01_parsing/                # UART parsing scripts and output
│   ├── lin_parser.py
│   └── [parsed CSVs]
├── 02_grouping/               # Message grouping scripts and output
│   ├── lin_message_grouper.py
│   └── [grouped CSVs]
├── 03_comparison/             # Comparison scripts and output
│   ├── lin_comparison.py
│   └── [comparison CSVs]
├── 04_results/                # Analysis results and documentation
│   └── LIN_Protocol_Analysis_Results.md
└── README.md                  # This file
```

## Analysis Workflow

### Stage 0: Capture (see /src/BusSniffer)

Before analysis begins, LIN bus traffic must be captured in raw UART mode under various conditions:

**Capture Scenarios:**
1. **Baseline** - No button presses, no lights (idle state)
2. **Lights on** - Button backlighting activated
3. **Lights cycle** - Backlighting toggled on/off repeatedly
4. **Driver heat cycle** - Driver heat button cycled (off → high → med → low → off)
5. **Driver vent cycle** - Driver vent button cycled
6. **Passenger heat cycle** - Passenger heat button cycled
7. **Passenger vent cycle** - Passenger vent button cycled
8. **Driver heat levels** - Individual level captures (high, medium, low)
9. **Passenger heat levels** - Individual level captures
10. **Combined states** - Multiple functions active simultaneously

**Output:** Raw UART txt files, with diagnostic headers, stored in `00_captures/` directory
```txt
Timestamp_us,Byte_Hex
0,0x00
520,0x55
1040,0xB1
```

### Stage 1: Parsing

**Script:** `01_parsing/lin_parser.py`

**Purpose:** Parse raw UART byte stream into structured LIN frames.

**LIN Frame Structure:**
```
[Break] [Sync] [PID] [Data...] [Checksum]
  0x00   0x55   0xB1   0x0C 0x00   0x42
```

**Input:** Raw UART CSV files
```csv
Timestamp_us,Byte_Hex
0,0x00
520,0x55
1040,0xB1
1560,0x0C
2080,0x00
2600,0x42
```

**Process:**
1. Reads raw UART bytes sequentially
2. Detects break field (0x00 after inter-frame gap)
3. Validates sync byte (0x55)
4. Extracts Protected ID (PID)
5. Collects data bytes based on LIN 2.0 length rules
6. Captures checksum byte
7. Validates frame structure

**Output:** Parsed LIN frame CSV files
```csv
Timestamp_us,Break,Sync,PID,Data,Checksum
0,00,55,B1,0C 00,42
20000,00,55,C4,01 FF,3A
```

**Usage:**
```bash
python lin_parser.py ../00_captures/capture_00_no_button_modules_no_presses_no_lights.txt
```

Or batch process:
```bash
for file in ../00_captures/*.txt; do
    python lin_parser.py "$file"
done
```

**Key Features:**
- State machine-based parsing for robust frame detection
- Inter-frame gap detection for frame boundaries
- Handles header-only frames (no data bytes)
- Variable data length based on PID
- Checksum validation (both classic and enhanced modes)
- Detailed error reporting for malformed frames

### Stage 2: Grouping

**Script:** `02_grouping/lin_message_grouper.py`

**Purpose:** Group parsed frames by ID and identify unique data patterns.

**Input:** Parsed LIN frame CSV files from Stage 1
```csv
Timestamp_us,Break,Sync,PID,Data,Checksum
0,00,55,B1,0C 00,42
20000,00,55,B1,3C 00,12
```

**Process:**
1. Reads parsed frame CSV
2. Extracts actual ID from Protected ID (lower 6 bits)
3. Groups frames by LIN ID
4. Identifies unique data patterns for each ID
5. Counts occurrences of each pattern
6. Records timestamps for frequency analysis

**Output:** Grouped CSV files
```csv
ID,Data,Count,Timestamps
177,"0C 00",150,"0, 20000, 40000, ..."
177,"3C 00",50,"60000, 80000, 100000, ..."
```

**Usage:**
```bash
python lin_message_grouper.py ../01_parsing/uart_capture_baseline_parsed.csv
```

**Key Features:**
- ID extraction from Protected ID (masks off parity bits)
- Handles header-only frames (empty data field)
- Temporal tracking for message frequency analysis
- Sorted output by ID for easy navigation

### Stage 3: Comparison

**Script:** `03_comparison/lin_comparison.py`

**Purpose:** Compare grouped captures to identify state-dependent messages.

**Input:** Two grouped CSV files from Stage 2

**Process:**
1. Loads both grouped captures into memory
2. Identifies messages unique to File A (not in File B)
3. Identifies messages unique to File B (not in File A)
4. Identifies common messages (appear in both)
5. Generates comparison reports with occurrence counts

**Output:** Comparison CSV files
```csv
ID,Data,Count,Timestamps,Source
177,"3C 00",50,"60000, ...",capture_04
177,"0C 00",150,"0, ...",capture_00
```

**Usage:**
```bash
python lin_comparison.py \
    ../02_grouping/capture_00_grouped.csv \
    ../02_grouping/capture_04_grouped.csv
```

**Output Files:**
- `comparison_[A]_vs_[B].csv` - Messages unique to A
- `comparison_[B]_vs_[A].csv` - Messages unique to B
- Terminal output with statistics and summary

**Key Comparisons:**
- **Baseline vs. Button Press** - Identifies button-triggered messages
- **Off vs. Level** - Identifies LED state messages
- **Heat vs. Vent** - Distinguishes between function types
- **Level vs. Level** - Identifies intensity patterns

### Stage 4: Manual Analysis

After automated processing:

1. **Review unique messages** - Focus on state-specific patterns
2. **Identify master vs. slave** - Determine message direction
3. **Decode data bytes** - Map bytes to LED states, backlight levels
4. **Validate findings** - Test hypotheses with additional captures
5. **Document protocol** - Create comprehensive protocol specification

**Results:** `04_results/LIN_Protocol_Analysis_Results.md`

## Script Details

### lin_parser.py

**Features:**
- Robust state machine for frame boundary detection
- Handles non-standard LIN implementations
- Inter-frame gap detection (>2ms silence)
- Fallback sync detection if BREAK not captured properly
- Comprehensive error handling and reporting

**Parsing Algorithm:**
1. Detect BREAK field (0x00 after >2ms gap)
2. Expect SYNC byte (0x55)
3. Read Protected ID
4. Determine data length from PID or standard LIN rules
5. Collect data bytes
6. Read checksum byte
7. Validate frame completeness

**LIN Data Length Rules:**
```python
# Standard LIN 2.0:
if ID <= 31: length = 2
elif ID <= 47: length = 4
else: length = 8

# Or use custom lookup table for non-standard implementations
```

**Requirements:**
- Python 3.6+
- Standard library only

### lin_message_grouper.py

**Features:**
- ID extraction with parity bit masking
- Signature-based message deduplication
- Handles variable data lengths
- Temporal analysis support
- Sorted, organized output

**Algorithm:**
1. Parse each frame from CSV
2. Extract base ID: `ID = PID & 0x3F`
3. Create signature: `(ID, Data)`
4. Track occurrences and timestamps
5. Write sorted output

**Requirements:**
- Python 3.6+
- Standard library only

### lin_comparison.py

**Features:**
- Set-based efficient comparison
- Bidirectional difference detection
- Occurrence count preservation
- Summary statistics reporting
- Handles empty data fields

**Algorithm:**
1. Load grouped CSVs into message sets
2. Compute: `A - B` (unique to A)
3. Compute: `B - A` (unique to B)
4. Write comparison results
5. Display statistics

**Requirements:**
- Python 3.6+
- Standard library only

## Example Workflow

### Complete Analysis Example

```bash
# Stage 0: Capture UART data (using BusSniffer in LIN mode)
# Files saved to: 00_captures/

# Stage 1: Parse all UART captures
cd 01_parsing
python lin_parser.py ../00_captures/capture_02_drivers_button_module_no_presses_no_lights.txt
python lin_parser.py ../00_captures/capture_03_drivers_button_module_heat_presses_every_3s_no_lights.txt
python lin_parser.py ../00_captures/capture_04_drivers_button_module_vent_presses_every_3s_no_lights.txt
python lin_parser.py ../00_captures/capture_05_drivers_button_module_no_presses_lights_cycled_every_3s.txt
# ... repeat for all captures

# Stage 2: Group all parsed captures
cd ../02_grouping
python lin_message_grouper.py ../01_parsing/capture_02_parsed.csv
python lin_message_grouper.py ../01_parsing/capture_03_parsed.csv
# ... repeat for all parsed files

# Stage 3: Compare to find patterns
cd ../03_comparison

# Find driver heat button press messages
python lin_comparison.py \
    ../02_grouping/capture_02_grouped.csv \
    ../02_grouping/capture_03_grouped.csv

# Find driver vent button press messages
python lin_comparison.py \
    ../02_grouping/capture_02_grouped.csv \
    ../02_grouping/capture_04_grouped.csv

# Compare heat vs vent (understand differences)
python lin_comparison.py \
    ../02_grouping/capture_03_grouped.csv \
    ../02_grouping/capture_04_grouped.csv

# Stage 4: Review and document findings
# Output: ../04_results/
```

### Key Findings from This Analysis

**LIN ID 0xB1 (Driver LED Control)**
- Master request (TX from gateway)
- 2 bytes: `[LED_state, Backlight]`
- LED states: 0x0C (off), 0x1C-0x3C (heat), 0x4C-0xCC (vent)
- Backlight: 0x00 (off), 0xC8 (on)

**LIN ID 0xC4 (Driver Button Detection)**
- Slave response (RX from button module)
- 2 bytes: `[Button_state, 0xFF]`
- States: 0x01 (idle), 0x41 (heat press), 0x11 (vent press)

**LIN ID 0x32 (Passenger LED Control)**
- Master request (TX from gateway)
- 2 bytes: `[LED_state, Backlight]`
- LED states: 0x09 (off), 0x49-0xC9 (heat), 0x19-0x39 (vent)

**LIN ID 0x80 (Passenger Button Detection)**
- Slave response (RX from button module)
- 2 bytes: `[Button_state, 0xFF]`
- States: 0x80 (idle), 0x88 (heat press), 0xA0 (vent press)

## Tips and Best Practices

### Capturing UART Data
1. **Use raw mode** - BusSniffer RAW_CAPTURE_MODE for direct UART bytes
2. **Long captures** - LIN is slower than CAN; capture for 30+ seconds
3. **Multiple samples** - Repeat critical actions to confirm patterns
4. **Document timing** - Note when you pressed buttons during capture

### Parsing
1. **Check frame counts** - Verify reasonable number of frames extracted
2. **Review errors** - Parser reports malformed frames; investigate causes
3. **Validate sync** - All frames should have 0x55 sync byte
4. **Inspect checksums** - Many failures might indicate electrical issues

### Grouping
1. **Focus on active IDs** - Ignore IDs with only one pattern (likely static)
2. **Count occurrences** - Frequent patterns are periodic status; rare are events
3. **Check timestamps** - Spacing reveals polling rate vs. event-driven

### Comparison
1. **Start simple** - Compare off vs. one level first
2. **Look for patterns** - LED commands often have clear bit patterns
3. **Compare similar** - High vs. Medium shows intensity encoding
4. **Validate bidirectional** - Ensure A vs. B and B vs. A make sense

### Analysis
1. **Identify master/slave** - Master sends commands, slave responds to polls
2. **Map data patterns** - Create lookup table for all state combinations
3. **Test hypotheses** - Write test code to verify protocol understanding
4. **Document completely** - Future you will thank present you

## Troubleshooting

### Parsing Issues
- **No frames extracted:** Check for break+sync patterns in raw data
- **Frame count too low:** May need to adjust gap detection threshold
- **Many checksum errors:** Verify LIN bus signal quality

### Grouping Issues
- **Empty output:** Check that parsed CSV has data
- **Wrong ID counts:** Verify PID masking logic is correct

### Comparison Issues
- **Everything unique:** Files might be from different buses/modules
- **Nothing unique:** Action may not have triggered protocol change
- **Unexpected patterns:** May have discovered unknown functionality

## LIN Protocol Notes

### Frame Structure
```
Break (dominant ~13 bit times)
Sync Byte: 0x55 (alternating bits)
Protected ID (PID): 6-bit ID + 2-bit parity
Data: 0-8 bytes (length depends on ID)
Checksum: Classic (data only) or Enhanced (PID + data)
```

### Master vs. Slave
- **Master:** Sends header (break, sync, PID)
- **Slave:** Responds with data + checksum (or master sends data)
- This project: ESP32 acts as master, button modules are slaves

### Message Types
- **Master Request:** Master sends header + data (LED commands)
- **Slave Response:** Master sends header, slave sends data (button status)

## Further Analysis

After completing this workflow:

1. **Frequency analysis** - Determine polling rates from timestamps
2. **Byte-level decode** - Map individual bits to LED segments
3. **Timing correlation** - Match button press timing to LIN events
4. **Active testing** - Implement test sketches to validate findings

## Author

[Projects In Motion](https://github.com/projectsinmotion), 2025
