# CAN Bus Protocol Analysis Workflow

## Overview

This directory contains Python scripts and documentation for reverse-engineering the CAN bus protocol used by heated/ventilated seat controls. The analysis workflow processes raw bus captures to identify message patterns, group unique messages, and compare different operational states to determine protocol structure.

## Directory Structure

```
CAN_analysis/
├── 00_captures/               # Raw capture files from BusSniffer
│   └── [raw CSV captures]
├── 01_grouping/               # Message grouping scripts and output
│   ├── can_message_grouper.py
│   └── [grouped CSVs]
├── 02_comparison/             # Comparison scripts and output
│   ├── can_comparison.py
│   └── [comparison CSVs]
├── 03_results/                # Analysis results and documentation
│   └── CAN_Protocol_Analysis_Results.md
└── README.md                  # This file
```

## Analysis Workflow

### Stage 0: Capture (see /src/BusSniffer)

Before analysis begins, CAN bus traffic must be captured under various conditions:

**Capture Scenarios:**
1. **Baseline** - No button presses, no lights (idle state)
2. **Lights cycling** - Button backlighting on/off
3. **Brightness variation** - Backlight min/max brightness
4. **Driver heat** - Button pressed every 3 seconds (cycling through levels)
5. **Driver vent** - Button pressed every 3 seconds
6. **Passenger heat** - Button pressed every 3 seconds
7. **Passenger vent** - Button pressed every 3 seconds

**Output:** Raw CSV files stored in `00_captures/` directory

### Stage 1: Grouping

**Script:** `01_grouping/can_message_grouper.py`

**Purpose:** Group messages by ID and identify unique data patterns within each ID.

**Input:** Raw CAN capture CSV files
```csv
Timestamp_s,ID,Extended,RTR,Length,D0,D1,D2,D3,D4,D5,D6,D7
0.000000,302,0,0,8,0,2,16,0,8,0,0,0
0.023456,31E,0,0,6,3,64,0,192,0,0,,
```

**Process:**
1. Reads all messages from CSV
2. Groups messages by CAN ID
3. Identifies unique data patterns within each ID
4. Counts occurrences of each unique pattern
5. Records all timestamps for each pattern

**Output:** Grouped CSV files (one per capture)
```csv
ID,Extended,RTR,Length,Data,Count,Timestamps
302,0,0,8,"0, 2, 16, 0, 8, 0, 0, 0",4,"2.301, 5.549, 8.629, 11.499"
31E,0,0,6,"3, 64, 0, 192, 0, 0, ,",7,"2.367, 2.802, 3.234, ..."
```

**Usage:**
```bash
python can_message_grouper.py ../00_captures/capture_00_no_presses_no_lights.csv
```

Or process all captures:
```bash
for file in ../00_captures/*.csv; do
    python can_message_grouper.py "$file"
done
```

**Key Features:**
- Handles variable data lengths (empty bytes represented as empty strings)
- Preserves all timing information for temporal analysis
- Groups by complete message signature (ID + data)
- Sorts output by CAN ID for easy navigation

### Stage 2: Comparison

**Script:** `02_comparison/can_comparison.py`

**Purpose:** Compare two grouped capture files to identify state-dependent messages.

**Input:** Two grouped CSV files from Stage 1

**Process:**
1. Loads both grouped captures
2. Identifies messages unique to File A (not in File B)
3. Identifies messages unique to File B (not in File A)
4. Identifies messages common to both files
5. Generates comparison reports

**Output:** Comparison CSV files
```csv
ID,Extended,RTR,Length,Data,Count,Timestamps,Source
302,0,0,8,"0, 2, 16, 0, 8, 0, 0, 0",4,"2.301, ...",capture_03
31E,0,0,6,"3, 64, 0, 192, 0, 0, ,",7,"2.367, ...",capture_03
```

**Usage:**
```bash
python can_comparison.py capture_00_grouped.csv capture_03_grouped.csv
```

**Output Files:**
- `comparison_[A]_vs_[B].csv` - All unique messages from A
- `comparison_[B]_vs_[A].csv` - All unique messages from B
- Terminal output shows statistics and summary

**Key Comparisons:**
- **Baseline vs. Action** - Identifies messages triggered by specific actions
- **Action vs. Action** - Distinguishes between different button types
- **Lights On vs. Off** - Identifies backlighting messages
- **Max vs. Min** - Identifies variable brightness messages

### Stage 3: Manual Analysis

After automated grouping and comparison:

1. **Review unique messages** - Focus on messages unique to action captures
2. **Identify ID patterns** - Group related functionality by CAN ID
3. **Decode data bytes** - Determine meaning of each byte position
4. **Map state changes** - Connect message data to observed behavior
5. **Document protocol** - Create protocol specification

**Results:** `03_results/CAN_Protocol_Analysis_Results.md`

## Script Details

### can_message_grouper.py

**Features:**
- Efficient message deduplication using signature-based hashing
- Handles large capture files (thousands of messages)
- Preserves timing information for frequency analysis
- Supports variable-length CAN messages
- Outputs organized, sorted CSV files

**Algorithm:**
1. Parse CSV line by line
2. Create message signature: `(ID, Extended, RTR, Length, Data)`
3. Use dictionary to track unique signatures
4. Accumulate timestamps for each occurrence
5. Write sorted output by ID

**Requirements:**
- Python 3.6+
- Standard library only (csv, pathlib, typing, collections)

### can_comparison.py

**Features:**
- Set-based comparison for efficient difference detection
- Preserves occurrence counts from grouped files
- Generates bidirectional comparison reports
- Summary statistics in terminal output
- Handles common baseline subtraction

**Algorithm:**
1. Load both grouped CSV files into message sets
2. Compute set difference: `A - B` (unique to A)
3. Compute set difference: `B - A` (unique to B)
4. Write comparison results to CSV
5. Display summary statistics

**Requirements:**
- Python 3.6+
- Standard library only

## Example Workflow

### Complete Analysis Example

```bash
# Stage 0: Capture data (using BusSniffer)
# Already completed - files in 00_captures/

# Stage 1: Group all captures
cd 01_grouping
python can_message_grouper.py ../00_captures/capture_00_no_presses_no_lights.csv
python can_message_grouper.py ../00_captures/capture_03_drivers_heat_presses_every_3s_no_lights.csv
python can_message_grouper.py ../00_captures/capture_04_drivers_vent_presses_every_3s_no_lights.csv
# ... repeat for all captures

# Stage 2: Compare baseline to action captures
cd ../02_comparison

# Find driver heat messages
python can_comparison.py \
    ../01_grouping/capture_00_grouped.csv \
    ../01_grouping/capture_03_grouped.csv

# Find driver vent messages
python can_comparison.py \
    ../01_grouping/capture_00_grouped.csv \
    ../01_grouping/capture_04_grouped.csv

# Compare heat vs. vent (understand differences)
python can_comparison.py \
    ../01_grouping/capture_03_grouped.csv \
    ../01_grouping/capture_04_grouped.csv

# Stage 3: Review comparison outputs and document findings
# Results in: ../03_results/
```

### Key Findings from This Analysis

**CAN ID 0x302 (770)** - Button Press Events
- Driver heat: D2=16, D3=0
- Driver vent: D2=64, D3=0
- Passenger heat: D2=0, D3=4
- Passenger vent: D2=0, D3=16

**CAN ID 0x31E (798)** - Heat/Vent Intensity Status
- Driver state in D0 (0=off, 1-3=heat, 4/8/12=vent)
- Passenger state in D1 (64=off, 65-67=heat, 68/72/76=vent)
- Continuous broadcast at ~2.3Hz during active states

**CAN ID 0x2FA (762)** - Button Backlighting
- D2 controls brightness (34=min, 200=max)
- 166 discrete brightness levels available
- Independent from LED state

## Tips and Best Practices

### Capturing Data
1. **Controlled tests** - One action per capture for clean comparison
2. **Consistent timing** - Use 3-second intervals between actions
3. **Baseline first** - Always capture idle state for reference
4. **Document conditions** - Note exactly what you did during each capture

### Grouping
1. **Process all captures** - Complete grouping before comparison
2. **Check message counts** - Verify expected number of unique patterns
3. **Review timestamps** - Look for periodic vs. event-driven messages

### Comparison
1. **Start with baseline** - Compare actions against idle state first
2. **Cross-compare actions** - Compare similar actions to find patterns
3. **Look for persistence** - Messages in all captures are status/heartbeat
4. **Focus on differences** - Unique messages reveal functional triggers

### Analysis
1. **Group by ID** - Related functionality shares CAN IDs
2. **Watch for patterns** - Sequential values, bit flags, enumerations
3. **Validate hypotheses** - Test predictions against multiple captures
4. **Document as you go** - Don't rely on memory for complex protocols

## Troubleshooting

### Script errors
- **File not found:** Check relative paths from script directory
- **Empty output:** Verify input CSV has correct format
- **Python errors:** Ensure Python 3.6+ installed

### Analysis issues
- **No unique messages:** Check if action actually triggered something
- **Too many unique messages:** Bus might be very active; filter by relevant IDs
- **Timing inconsistent:** Normal for automotive buses; focus on patterns not exact timing

## Further Analysis

After completing this workflow, you may want to:

1. **Frequency analysis** - Use timestamps to determine message rates
2. **Temporal correlation** - Map button press times to status change times
3. **Byte-level analysis** - Decode individual bits within data bytes
4. **Cross-bus correlation** - Compare with LIN analysis results

## Author

[Projects In Motion](https://github.com/projectsinmotion), 2025
