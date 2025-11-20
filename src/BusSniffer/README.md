# Bus Sniffer

## Overview

The Bus Sniffer is a versatile tool for capturing and logging CAN or LIN bus traffic for reverse engineering and protocol analysis. It supports both passive listen-only mode (CAN) and true passive monitoring (LIN), making it ideal for capturing real-world automotive bus traffic without interfering with the existing network.

## Features

### CAN Bus Mode
- **Passive listen-only mode** - No ACK transmission, zero bus interference
- **125 kbps** (configurable)
- **Automatic capture** of all CAN IDs
- **Timestamps** in microseconds
- Captures up to 9,000 messages
- **CSV export** for analysis with external tools

### LIN Bus Mode
- **True passive monitoring** - No transmission, zero interference
- **19,200 baud** (configurable)
- **Two capture modes:**
  - **Frame parsing mode** (default) - Parses LIN protocol and captures structured frames
  - **Raw UART mode** - Captures raw byte stream with microsecond timestamps
- Captures all LIN IDs including header-only frames
- Enhanced checksum validation (classic and enhanced modes)
- **CSV export** with checksum status for each frame
- Dynamic length detection for non-standard LIN implementations

### Interactive Capture System
The Bus Sniffer includes a complete capture workflow with automated logging:

1. **capture_interactive.bat** - Windows batch script launcher
2. **serial_capture_interactive.py** - Python capture tool with CSV extraction
3. **BusSniffer.ino** - ESP32 firmware

This system allows you to:
- Send commands to the ESP32 (start/stop/export)
- Automatically save all output to timestamped files
- Extract and save CSV data separately
- View real-time output in the console

## Usage

### 1. Hardware Setup

Connect the ESP32 board to the target bus:
- **CAN:** Connect CAN_H and CAN_L to the bus (pins 11/12)
- **LIN:** Connect LIN bus to the RX pin (pin 3)
- **Power:** 7-14V DC or USB

**Important LIN Configuration:**
- **Remove the LIN master jumper** on the board before capturing LIN traffic
- The jumper enables LIN master mode, which interferes with passive monitoring
- With jumper removed, the board operates in passive receive-only mode
- The bus must already have an active LIN master

**Important CAN Configuration:**
- Ensure proper bus termination for CAN (120Ω), not necessarily on this board, but on the two ends of the CAN bus trunk

### 2. Configure Bus Mode

Edit `BusSniffer.ino` to select the bus:

```cpp
// Select bus to monitor: 0 = CAN, 1 = LIN
#define BUS_MODE 0  // SET TO 0 FOR CAN BUS SNIFFING
```

Also configure speed if needed:
```cpp
#define CAN_SPEED 125  // kbps
#define LIN_SPEED 19200  // baud
```

### 3. Upload Firmware

1. Open `BusSniffer.ino` in Arduino IDE
2. Select board: **ESP32S3 Dev Module**
3. Upload to the board
4. The RGB LED will cycle colors at startup

### 4. Start Interactive Capture

#### Option A: Using Batch Script (Windows)

Double-click `capture_interactive.bat` in the BusSniffer directory.

#### Option B: Direct Python Execution

```bash
python serial_capture_interactive.py
```

**Python Requirements:**
- Python 3.x
- `pyserial` package: `pip install pyserial`

### 5. Interactive Capture Process

When you run the capture script:

1. **Port Selection:**
   ```
   Available Serial Ports:
   ------------------------------------------------------------
   0: COM3 - USB Serial Device (COM3)
   1: COM5 - USB-SERIAL CH340 (COM5)
   ------------------------------------------------------------
   Enter port number (or full port name): 0
   ```

2. **Baudrate Configuration:**
   ```
   Baudrate [115200]:
   ```
   Press Enter to use default (115200), or enter a different value.

3. **Capture Session:**
   ```
   ============================================================
   Interactive Serial Monitor
   ============================================================
   Port: COM3
   Baudrate: 115200
   Output: captures/bus_capture_20251113_141219.txt
   CSV: captures/bus_capture_20251113_141219.csv
   ============================================================

   Type commands and press Enter to send to ESP32
   Common commands: s (start), t (stop), c (export CSV), h (help)
   Press Ctrl+C to exit
   ```

4. **Send Commands:**
   - Type `h` for help menu
   - Type `s` to start capture
   - Type `t` to stop capture
   - Type `c` to export CSV
   - All output is automatically logged to `.txt` file
   - CSV exports are automatically extracted to `.csv` file

5. **Exit:**
   - Press `Ctrl+C` to close the session
   - All files are saved in the `captures/` subdirectory

### Output Files

The capture script creates timestamped files in the `captures/` directory:

- **bus_capture_YYYYMMDD_HHMMSS.txt** - Complete console output
- **bus_capture_YYYYMMDD_HHMMSS.csv** - Extracted CSV data (if exported)

**CSV Format (CAN):**
```csv
Timestamp_s,ID,Extended,RTR,Length,D0,D1,D2,D3,D4,D5,D6,D7
0.000000,302,0,0,8,0,2,16,0,8,0,0,0
0.023456,31E,0,0,6,3,64,0,192,0,0,,
```

**CSV Format (LIN - Frame Parsing Mode):**
```csv
Timestamp_s,ID,Extended,RTR,Length,ChecksumOK,D0,D1,D2,D3,D4,D5,D6,D7
0.000000,196,0,0,2,1,12,0,,,,,,
0.020000,177,0,0,2,1,1,255,,,,,,
```

**CSV Format (LIN - Raw UART Mode):**
```csv
Timestamp_us,Byte_Hex
0,0x55
520,0xC1
1040,0x12
1560,0x00
2080,0xED
```

## Serial Commands

When connected via Serial Monitor or the capture script:

| Command | Function |
|---------|----------|
| `s` | Start capture |
| `t` | Stop capture |
| `p` | Print captured data (formatted) |
| `c` | Export to CSV format |
| `r` | Reset buffer (clear all data) |
| `i` | Show statistics |
| `d` | Diagnostics (LIN mode only) |
| `h` | Show help menu |

## LED Indicators

| Color | Meaning |
|-------|---------|
| RGB startup sequence | Power-on self-test |
| Green | Ready, waiting for command |
| Blue (blinking) | Capturing data |
| Yellow | Buffer full, capture stopped |
| Red | Error (failed to initialize) |
| Brief blue flash | Bus activity detected |

## Configuration Options

### Buffer Size
```cpp
#define MAX_MESSAGES 9000  // Maximum messages to capture
```

Increase if you need longer captures (uses more RAM).

### Auto-Start
```cpp
#define AUTO_START 0  // 1 = auto-start on boot, 0 = wait for 's' command
```

Set to 1 for unattended captures.

### Debug Output
```cpp
#define DEBUG_FRAMES 1  // 1 = show frames as captured, 0 = silent
```

Disable for cleaner output when capturing many messages.

### Raw UART Capture Mode (LIN Only)
```cpp
#define RAW_CAPTURE_MODE 0  // 0 = frame parsing (default), 1 = raw UART bytes
```

**Important:** This mode only works for LIN (UART-based). Must be set to 0 for CAN.

**When to use Raw UART Mode:**
- Debugging LIN protocol parsing issues
- Capturing timing information between individual bytes
- Analyzing non-standard or corrupted LIN traffic
- Recording bus activity when frame structure is unknown
- Investigating inter-byte timing and gaps

**Raw Mode Behavior:**
- Captures every UART byte with microsecond timestamp
- No protocol parsing or frame detection
- Larger buffer capacity (up to 90,000 bytes)
- CSV export format: `Timestamp_us,Byte_Hex`
- Useful for detailed timing analysis and protocol reverse engineering

## Python Script Details

### serial_capture_interactive.py

**Features:**
- Automatic port discovery and selection
- Configurable baudrate
- Timestamped output files
- Real-time console display
- Automatic CSV extraction when device sends CSV data
- Thread-safe serial reading
- Graceful error handling

**How CSV Extraction Works:**

The script monitors the serial output for CSV markers:
1. Detects `=== CSV Export` or CSV header line
2. Collects all CSV data lines
3. Detects `=== End CSV Export`
4. Writes CSV data to separate `.csv` file
5. Notifies user of saved file

**Requirements:**
- Python 3.6 or later
- pyserial: `pip install pyserial`

**Error Handling:**
The script provides helpful error messages:
- Port not found → Lists available ports
- Permission denied → Suggests closing other serial programs
- Connection failed → Checks cable and board power

### capture_interactive.bat

Simple Windows launcher that:
1. Displays banner
2. Changes to script directory
3. Runs Python script
4. Pauses at end to show any errors

## Tips and Best Practices

### CAN Capture
1. **Termination:** Ensure bus has proper 120Ω termination
2. **Speed:** Verify CAN speed matches the bus (common: 125, 250, 500 kbps)
3. **Power:** Use isolated power when possible
4. **Duration:** Start capture, perform test action, stop capture (keep focused)

### LIN Capture
1. **Master presence:** LIN bus must have an active master
2. **Timing:** LIN is slower - expect longer capture times
3. **Checksums:** Review `ChecksumOK` column to identify communication issues
4. **Passive only:** TX pin is disabled - purely passive monitoring
5. **Raw mode:** Enable `RAW_CAPTURE_MODE` for byte-level analysis and timing investigation
6. **Mode selection:** Use frame parsing mode (default) for protocol analysis, raw mode for debugging

### General
1. **Organize captures:** Use descriptive filenames for each test scenario
2. **Document tests:** Note what you did during each capture
3. **Compare captures:** Use diff tools to find state-dependent messages
4. **Buffer management:** Stop/reset between different test scenarios

## Troubleshooting

### No messages captured
- **CAN:** Check termination, verify bus speed, ensure bus is active
- **LIN:**
  - **Remove LIN master jumper** - This is the most common issue
  - Verify LIN master is running on the bus
  - Check RX connection (pin 3)
  - Ensure CS pin is HIGH (enabled)

### Capture stops immediately
- Buffer might be full from previous session
- Send `r` command to reset before starting new capture

### Python script won't connect
- Close Arduino IDE Serial Monitor
- Check COM port number
- Verify USB cable supports data (not charge-only)
- Try different USB port

### CSV not extracted
- Ensure you sent `c` command while capture script is running
- Check that capture was stopped (`t`) before exporting
- Verify messages were actually captured (`i` shows count)

### Raw UART mode issues (LIN only)
- **CAN mode selected:** Raw mode only works with LIN (BUS_MODE must be 1)
- **No data captured:** Verify LIN master is active and transmitting
- **Buffer fills quickly:** Raw mode captures ~10x more data than frame mode
- **Timing analysis:** Use the Timestamp_us column to calculate inter-byte gaps
- **Frame reconstruction:** You'll need to manually parse BREAK/SYNC/ID/DATA/CHECKSUM from raw bytes

## LIN Capture Modes Comparison

### Frame Parsing Mode (Default - RAW_CAPTURE_MODE = 0)

**Best for:**
- Standard LIN protocol analysis
- Understanding message structure (ID, data, checksum)
- Comparing different message IDs
- Long capture sessions (up to 9,000 frames)
- Finding button press patterns or state changes

**Output:**
- Structured data: ID, Length, ChecksumOK, Data bytes
- Easy to analyze in spreadsheet tools
- Checksum validation included
- Filtered and organized by frame

**Example use case:** Capturing seat button presses to identify which LIN ID changes when buttons are pressed.

### Raw UART Mode (RAW_CAPTURE_MODE = 1)

**Best for:**
- Low-level protocol debugging
- Timing analysis (inter-byte, inter-frame gaps)
- Investigating checksum failures
- Non-standard LIN implementations
- Verifying BREAK field detection
- Analyzing bus errors or corruption

**Output:**
- Raw byte stream with microsecond timestamps
- Every UART byte captured individually
- No parsing or interpretation
- Requires manual frame reconstruction

**Example use case:** Debugging why LIN frames aren't being parsed correctly, or measuring exact timing between bytes.

## Analysis Workflow

Typical workflow for protocol reverse engineering:

1. **Baseline capture:** Record normal idle state
2. **Action captures:** Record specific actions (button press, etc.)
3. **Export CSVs:** Use `c` command for each capture
4. **Organize files:** Rename CSVs descriptively (e.g., `driver_heat_press.csv`)
5. **Group messages:** Use analysis scripts (see `/docs/CAN_analysis` or `/docs/LIN_analysis`)
6. **Compare captures:** Identify unique messages per action
7. **Document protocol:** Map message IDs and data patterns

**LIN-specific workflow:**
- Start with **frame parsing mode** for initial protocol discovery
- Switch to **raw UART mode** only if you encounter parsing issues or need timing data
- Use raw mode captures to debug and refine the frame parsing logic

## Hardware Specifications

### Pin Assignments (CAN Mode)
- CAN TX: Pin 11
- CAN RX: Pin 12

### Pin Assignments (LIN Mode)
- LIN RX: Pin 3 (TX disabled for passive mode)
- LIN CS: Pin 46 (set HIGH to enable transceiver)
- LIN FAULT: Pin 9 (input, monitors transceiver status)

### Performance
- **CAN:** Up to 1 Mbps (typically 125-500 kbps)
- **LIN:** 19,200 baud standard
- **Capture rate:** Handles high-traffic buses without message loss
- **Buffer:** 9,000 messages ≈ 30-60 seconds of typical automotive traffic

## Author

[Projects In Motion](https://github.com/projectsinmotion), 2025
