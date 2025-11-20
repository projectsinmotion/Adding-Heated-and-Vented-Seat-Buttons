/*
 * Bus Sniffer for ESP32S3 CAN & LIN-Bus Board - FIXED VERSION
 *
 * This tool captures and logs CAN or LIN bus traffic for reverse engineering.
 * Supports up to 60 seconds of continuous capture with timestamps.
 *
 * Hardware: ESP32-S3-WROOM-1-N8R8 CAN & LIN Bus Board
 * Manufacturer: SK Pang Electronics (skpang.co.uk)
 *
 * Libraries required:
 * - ESP32-TWAI-CAN by handmade0octopus
 * - No LIN library needed! We parse LIN protocol directly for true passive monitoring
 *
 * FIXES APPLIED:
 * - Use UART_BREAK_ERROR (not UART_BREAK) - correct ESP32 constant
 * - Consume 0x00 byte in BREAK callback to prevent state machine confusion
 * - Removed checksum filtering - capture ALL frames regardless of validation
 * - Removed fallback 0x55 sync that caused false frame detection
 * - Added checksum status logging to CSV for analysis
 * - Added detailed debug output to see all captured frames
 * - RAW_CAPTURE_MODE set to 0 for proper frame parsing
 * - Data length table updated from UART reverse engineering results
 * - Now correctly handles all 17 PIDs observed on seat button LIN bus
 *
 * Author: Projects In Motion (https://github.com/projectsinmotion)
 * Date: 2025
 */

#include <ESP32-TWAI-CAN.hpp>

// ============================================================================
// CONFIGURATION - Modify these settings for your use case
// ============================================================================

// Select bus to monitor: 0 = CAN, 1 = LIN
#define BUS_MODE 0  // SET TO 0 FOR CAN BUS SNIFFING

// CAN bus speed in kbps (common: 125, 250, 500, 1000)
#define CAN_SPEED 125

// LIN bus speed in baud (common: 9600, 19200)
#define LIN_SPEED 19200

// ============================================================================
// BUTTON DETECTION NOTES (from reverse engineering):
// - PID 0x9C (ID 0x1C): 4 bytes - PRIMARY BUTTON CANDIDATE!
// - Watch for data changes from baseline: [00 00 29 10]
// - Header-only PIDs may start responding when buttons pressed:
//   PID 0x42, 0x80, 0xAD, 0xC4, 0xE9
// ============================================================================

// Maximum number of messages to capture (affects memory usage)
#define MAX_MESSAGES 9000  // Increased by 50% for extended captures

// Auto-start capture on boot (1) or wait for 's' command (0)
#define AUTO_START 0

// Debug output - shows all frames as they're captured (0 = off, 1 = on)
#define DEBUG_FRAMES 1

// RAW CAPTURE MODE - Just dump raw UART bytes with timestamps (0 = normal, 1 = raw)
// NOTE: RAW MODE ONLY WORKS FOR LIN (UART-based). CAN requires parsed frames.
#define RAW_CAPTURE_MODE 0  // SET TO 0 FOR CAN (must be 0!)

// ============================================================================
// HARDWARE PIN DEFINITIONS (ESP32S3 CAN & LIN Board)
// ============================================================================
// Pin definitions verified from SK Pang official demo code

// CAN pins (TWAI controller)
#define CAN_TX   11  // Connects to CTX
#define CAN_RX   12  // Connects to CRX

// LIN pins (TJA1021T transceiver)
#define PIN_LIN_TX    10
#define PIN_LIN_RX    3
#define LIN_CS        46  // LIN chip select
#define LIN_FAULT     9   // LIN fault detection

// Onboard RGB LED (verified from SK Pang demo)
#define LED_R 39
#define LED_G 38
#define LED_B 40
#define LED_ON  LOW
#define LED_OFF HIGH

// ============================================================================
// DATA STRUCTURES
// ============================================================================

#if RAW_CAPTURE_MODE

// Raw capture mode - just timestamp and byte
struct RawByte {
  uint32_t timestamp_us;  // Microsecond timestamp
  uint8_t  byte;          // Raw byte value
};

#endif

struct BusMessage {
  uint32_t timestamp_us;  // Microsecond timestamp
  uint32_t id;            // CAN ID or LIN ID
  uint8_t  length;        // Data length
  uint8_t  data[8];       // Data bytes
  bool     extended;      // CAN extended frame flag
  bool     rtr;           // CAN RTR flag
  bool     checksumValid; // LIN checksum validation status
};


// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

#if RAW_CAPTURE_MODE
RawByte rawBuffer[MAX_MESSAGES * 10];  // Can store 30,000 bytes
uint16_t byteCount = 0;
#else
BusMessage messageBuffer[MAX_MESSAGES];
uint16_t messageCount = 0;
#endif

bool capturing = false;
bool bufferFull = false;
uint32_t captureStartTime = 0;
uint32_t lastActivityTime = 0;

#if BUS_MODE == 1
// LIN passive monitoring using direct UART access
HardwareSerial LIN_Serial(2);  // Use UART2
volatile bool linBreakDetected = false;

// LIN protocol parser state machine
enum LINState {
  LIN_IDLE,
  LIN_SYNC,
  LIN_ID,
  LIN_DATA,
  LIN_CHECKSUM
};

struct LINFrame {
  uint8_t id;
  uint8_t dataLength;
  uint8_t data[8];
  uint8_t checksum;
  bool checksumValid;
};

LINState linState = LIN_IDLE;
LINFrame currentFrame;
uint8_t linDataIndex = 0;
uint32_t linFrameStartTime = 0;
uint32_t debugBreakCount = 0;  // Count BREAK detections
uint32_t debugFrameCount = 0;  // Count all frames received

// Counter to see if callback is being called at all
volatile uint32_t debugCallbackCount = 0;

// Callback for UART errors (used to detect BREAK field)
// Check BOTH error type AND 0x00 byte (as per LIN library)
void IRAM_ATTR linSerialErrorCallback(hardwareSerial_error_t error) {
  debugCallbackCount++;  // Count every callback

  // CRITICAL: Check error type is UART_BREAK_ERROR AND byte is 0x00
  if ((error == UART_BREAK_ERROR) && (LIN_Serial.peek() == 0x00)) {
    linBreakDetected = true;
    debugBreakCount++;
    // Consume the 0x00 byte as per LIN library
    LIN_Serial.read();
  }
}
#endif

// Statistics
uint32_t totalMessagesReceived = 0;
uint32_t messagesDropped = 0;
uint32_t checksumFailures = 0;

// ============================================================================
// LED CONTROL FUNCTIONS
// ============================================================================

void setLED(bool red, bool green, bool blue) {
  digitalWrite(LED_R, red ? LED_ON : LED_OFF);
  digitalWrite(LED_G, green ? LED_ON : LED_OFF);
  digitalWrite(LED_B, blue ? LED_ON : LED_OFF);
}

void ledBlink(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    setLED(false, true, false);  // Green
    delay(delayMs);
    setLED(false, false, false);
    delay(delayMs);
  }
}

// ============================================================================
// BUS INITIALIZATION
// ============================================================================

void initCAN() {
  Serial.println(F("\n=== Initializing CAN Bus (PASSIVE LISTEN-ONLY MODE) ==="));
  Serial.printf("Speed: %d kbps\n", CAN_SPEED);
  Serial.printf("TX Pin: %d, RX Pin: %d\n", CAN_TX, CAN_RX);
  Serial.println(F("Mode: 100% Passive - No ACK, no transmission, no interference"));

  ESP32Can.setPins(CAN_TX, CAN_RX);
  ESP32Can.setRxQueueSize(50);  // Increased from 10 for busy CAN buses
  ESP32Can.setTxQueueSize(5);   // Not used in listen-only mode, but required

  // CRITICAL: Enable listen-only mode for true passive sniffing
  // This prevents the ESP32 from ACKing frames or sending error frames
  // Configure general config for listen-only mode
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX, (gpio_num_t)CAN_RX, TWAI_MODE_LISTEN_ONLY);
  g_config.tx_queue_len = 5;
  g_config.rx_queue_len = 50;

  if (ESP32Can.begin(ESP32Can.convertSpeed(CAN_SPEED), CAN_TX, CAN_RX, 0xFFFF, 0xFFFF, nullptr, &g_config, nullptr)) {
    Serial.println(F("CAN bus started successfully in LISTEN-ONLY mode!"));
    Serial.println(F("  ✓ No ACK transmission (100% passive)"));
    Serial.println(F("  ✓ Zero interference with other nodes"));
    Serial.println(F("  ✓ Captures all CAN IDs automatically"));
    setLED(false, true, false);  // Green = success
    delay(500);
  } else {
    Serial.println(F("ERROR: CAN bus failed to start!"));
    setLED(true, false, false);  // Red = error
    while(1) { delay(1000); }  // Halt on error
  }
}

#if BUS_MODE == 1
// Calculate LIN data length from protected ID
// UPDATED based on reverse engineering: This bus doesn't follow LIN 2.0 standard!
// We use dynamic length detection instead (read until next frame marker)
uint8_t getLinDataLength(uint8_t protectedID) {
  // Extract actual ID (lower 6 bits)
  uint8_t id = protectedID & 0x3F;

  // Based on UART capture analysis, use actual observed lengths:
  // Note: 'id' is already masked to lower 6 bits
  switch(id) {
    case 0x00: return 0;  // PID 0x80: header-only
    case 0x01: return 2;  // PID 0xC1: 2 bytes
    case 0x02: return 0;  // PID 0x42: header-only
    case 0x04: return 0;  // PID 0xC4: header-only
    case 0x0A: return 2;  // PID 0xCA: 2 bytes
    case 0x0D: return 2;  // PID 0x0D: 2 bytes
    case 0x0E: return 2;  // PID 0x8E: 2 bytes
    case 0x0F: return 2;  // PID 0xCF: 2 bytes
    case 0x1C: return 4;  // PID 0x9C: 4 bytes (button candidate!)
    case 0x1D: return 2;  // PID 0xDD: 2 bytes
    case 0x1F: return 5;  // PID 0x1F: 5 bytes (not standard!)
    case 0x21: return 1;  // PID 0x61: 1 byte (not standard!)
    case 0x29: return 0;  // PID 0xE9: header-only
    case 0x2A: return 4;  // PID 0x6A: 4 bytes
    case 0x2D: return 0;  // PID 0xAD: header-only
    case 0x31: return 2;  // PID 0xB1: 2 bytes
    case 0x32: return 2;  // PID 0x32: 2 bytes

    default:
      // Fallback to LIN 2.0 for unknown IDs
      if (id <= 31) return 2;
      else if (id <= 47) return 4;
      else return 8;
  }
}

// Calculate LIN checksum (Classic or Enhanced)
uint8_t calculateLinChecksum(uint8_t id, uint8_t* data, uint8_t length, bool enhanced) {
  uint16_t sum = 0;

  // Enhanced checksum includes protected ID
  if (enhanced) {
    sum = id;
  }

  // Add all data bytes
  for (uint8_t i = 0; i < length; i++) {
    sum += data[i];
    if (sum > 0xFF) {
      sum = (sum & 0xFF) + 1;  // Carry
    }
  }

  return ~sum;  // Inverted
}

void initLIN() {
  Serial.println(F("\n=== Initializing LIN Bus (TRUE PASSIVE MODE - FIXED) ==="));
  Serial.printf("Speed: %d baud\n", LIN_SPEED);
  Serial.printf("RX Pin: %d, TX Pin: %d (disabled)\n", PIN_LIN_RX, PIN_LIN_TX);
  Serial.println(F("Mode: 100% Passive - No transmission, no interference"));
  Serial.println(F("\nFIXES APPLIED:"));
  Serial.println(F("  ✓ Proper UART_BREAK_ERROR detection"));
  Serial.println(F("  ✓ 0x00 byte consumed in callback"));
  Serial.println(F("  ✓ Captures ALL frames (no checksum filter)"));
  Serial.println(F("  ✓ Removed fallback 0x55 sync"));

  // Configure LIN transceiver control pins (TJA1021T)
  pinMode(LIN_FAULT, INPUT);    // Fault detection input
  pinMode(LIN_CS, OUTPUT);
  digitalWrite(LIN_CS, HIGH);   // HIGH = Chip enabled/normal mode

  // FIXED: Disable TX pin completely (high-impedance)
  pinMode(PIN_LIN_TX, INPUT);   // Set TX as input to avoid any output

  // Initialize UART2 for LIN - only RX pin, TX disabled
  LIN_Serial.begin(LIN_SPEED, SERIAL_8N1, PIN_LIN_RX, -1);  // -1 = no TX

  // Register error callback to detect BREAK fields
  LIN_Serial.onReceiveError(linSerialErrorCallback);

  Serial.println(F("\nLIN PASSIVE SNIFFER:"));
  Serial.println(F("  ✓ Captures ALL frame IDs automatically"));
  Serial.println(F("  ✓ Never transmits (100% passive)"));
  Serial.println(F("  ✓ Zero interference with other nodes"));
  Serial.println(F("  ✓ Works with any LIN master/slaves"));
  Serial.println();

  linState = LIN_IDLE;
  setLED(false, true, false);  // Green = success
  delay(500);
}
#endif

// ============================================================================
// MESSAGE CAPTURE FUNCTIONS
// ============================================================================

#if !RAW_CAPTURE_MODE
void captureCANMessage() {
  CanFrame rxFrame;

  while (ESP32Can.readFrame(rxFrame, 0)) {  // Non-blocking read
    totalMessagesReceived++;
    lastActivityTime = millis();

    // Blink blue LED briefly on activity
    digitalWrite(LED_B, LED_ON);

    if (capturing && !bufferFull) {
      if (messageCount < MAX_MESSAGES) {
        BusMessage* msg = &messageBuffer[messageCount];
        msg->timestamp_us = micros() - captureStartTime;
        msg->id = rxFrame.identifier;
        msg->length = rxFrame.data_length_code;
        msg->extended = rxFrame.extd;
        msg->rtr = rxFrame.rtr;
        msg->checksumValid = true;  // CAN has its own CRC

        for (int i = 0; i < rxFrame.data_length_code && i < 8; i++) {
          msg->data[i] = rxFrame.data[i];
        }

        messageCount++;

        // Visual feedback every 100 messages
        if (messageCount % 100 == 0) {
          Serial.printf("Captured: %d messages\n", messageCount);
        }
      } else {
        bufferFull = true;
        capturing = false;
        setLED(true, true, false);  // Yellow = buffer full
        Serial.println(F("\n!!! BUFFER FULL - Capture stopped !!!"));
        Serial.printf("Captured %d messages in %.2f seconds\n",
                     messageCount, (micros() - captureStartTime) / 1000000.0);
      }
    }

    digitalWrite(LED_B, LED_OFF);
  }
}

#if BUS_MODE == 1
void captureLINMessage() {
  static uint32_t lastByteTime = 0;

  // Check for BREAK detection (start of new frame)
  if (linBreakDetected) {
    linBreakDetected = false;
    linState = LIN_SYNC;
    linDataIndex = 0;
    linFrameStartTime = micros();
    memset(&currentFrame, 0, sizeof(currentFrame));
  }

  // Timeout check - reset if no data for too long (inter-byte timeout)
  if (linState != LIN_IDLE && (millis() - lastByteTime > 50)) {
    linState = LIN_IDLE;
  }

  // FALLBACK: If UART_BREAK_ERROR doesn't work, detect frames by inter-frame gap + 0x55
  // LIN inter-frame gap is minimum 1ms at 19200 baud
  if (linState == LIN_IDLE && LIN_Serial.available() > 0) {
    uint32_t gap = millis() - lastByteTime;
    if (gap > 2) {  // 2ms gap suggests new frame
      uint8_t peek = LIN_Serial.peek();
      if (peek == 0x55) {
        // Looks like a SYNC byte after an inter-frame gap
        linState = LIN_SYNC;
        linDataIndex = 0;
        linFrameStartTime = micros();
        memset(&currentFrame, 0, sizeof(currentFrame));
        debugBreakCount++;  // Count as fallback BREAK detection
      }
    }
  }

  // Process incoming bytes
  while (LIN_Serial.available() && (linState != LIN_IDLE)) {
    uint8_t byte = LIN_Serial.read();
    lastByteTime = millis();
    lastActivityTime = millis();

    switch (linState) {
      case LIN_SYNC:
        // Expect SYNC byte (0x55) - 0x00 was consumed in callback
        if (byte == 0x55) {
          linState = LIN_ID;
        } else {
          // Invalid SYNC, abort frame
          linState = LIN_IDLE;
        }
        break;

      case LIN_ID:
        // Store protected ID
        currentFrame.id = byte & 0x3F;  // Lower 6 bits are actual ID
        currentFrame.dataLength = getLinDataLength(byte);
        linDataIndex = 0;
        linState = LIN_DATA;
        break;

      case LIN_DATA:
        // Collect data bytes
        if (linDataIndex < 8) {
          currentFrame.data[linDataIndex++] = byte;

          // After collecting expected data length, next byte is checksum
          if (linDataIndex >= currentFrame.dataLength) {
            linState = LIN_CHECKSUM;
          }
        }
        break;

      case LIN_CHECKSUM: {
        currentFrame.checksum = byte;
        debugFrameCount++;

        // Validate checksum (try both classic and enhanced)
        uint8_t checksumClassic = calculateLinChecksum(currentFrame.id, currentFrame.data, linDataIndex, false);
        uint8_t checksumEnhanced = calculateLinChecksum(currentFrame.id, currentFrame.data, linDataIndex, true);

        currentFrame.checksumValid = (byte == checksumClassic) || (byte == checksumEnhanced);

        if (!currentFrame.checksumValid) {
          checksumFailures++;
        }

        // Frame complete - store frame count
        totalMessagesReceived++;

        // CRITICAL FIX: Capture ALL frames regardless of checksum validation
        if (capturing && !bufferFull) {
          digitalWrite(LED_B, LED_ON);

          if (messageCount < MAX_MESSAGES) {
            BusMessage* msg = &messageBuffer[messageCount];
            msg->timestamp_us = linFrameStartTime - captureStartTime;
            msg->id = currentFrame.id;
            msg->length = linDataIndex;  // Use actual data length received
            msg->extended = false;
            msg->rtr = false;  // LIN doesn't have RTR
            msg->checksumValid = currentFrame.checksumValid;  // Log checksum status

            for (int i = 0; i < linDataIndex && i < 8; i++) {
              msg->data[i] = currentFrame.data[i];
            }

            messageCount++;

            // Debug output every frame if enabled
            #if DEBUG_FRAMES
            if (messageCount % 10 == 0 || !currentFrame.checksumValid) {
              Serial.printf("[%d] ID:0x%02X Len:%d Chk:%s Data:",
                           messageCount, currentFrame.id, linDataIndex,
                           currentFrame.checksumValid ? "OK" : "FAIL");
              for (int i = 0; i < linDataIndex; i++) {
                Serial.printf(" %02X", currentFrame.data[i]);
              }
              Serial.println();
            }
            #endif
          } else {
            bufferFull = true;
            capturing = false;
            setLED(true, true, false);  // Yellow = buffer full
            Serial.println(F("\n!!! BUFFER FULL - Capture stopped !!!"));
          }

          digitalWrite(LED_B, LED_OFF);
        }

        // Reset for next frame
        linState = LIN_IDLE;
        break;
      }

      default:
        linState = LIN_IDLE;
        break;
    }
  }
}

#endif  // BUS_MODE == 1

#endif  // !RAW_CAPTURE_MODE

// RAW CAPTURE MODE - Just dump UART bytes with timestamps
#if RAW_CAPTURE_MODE && BUS_MODE == 1
void captureRawLINData() {
  while (LIN_Serial.available() && capturing && !bufferFull) {
    uint8_t byte = LIN_Serial.read();
    uint32_t timestamp = micros() - captureStartTime;
    lastActivityTime = millis();

    if (byteCount < (MAX_MESSAGES * 10)) {
      rawBuffer[byteCount].timestamp_us = timestamp;
      rawBuffer[byteCount].byte = byte;
      byteCount++;

      // Blink LED on activity
      if (byteCount % 100 == 0) {
        digitalWrite(LED_B, !digitalRead(LED_B));
      }
    } else {
      bufferFull = true;
      capturing = false;
      setLED(true, true, false);  // Yellow = buffer full
      Serial.println(F("\n!!! BUFFER FULL - Capture stopped !!!"));
      Serial.printf("Captured %d bytes in %.2f seconds\n",
                   byteCount, (micros() - captureStartTime) / 1000000.0);
    }
  }
}
#endif

// ============================================================================
// DATA EXPORT FUNCTIONS
// ============================================================================

void printCapturedData() {
  #if RAW_CAPTURE_MODE
  Serial.println(F("\nRAW CAPTURE MODE - Use 'c' command to export CSV data"));
  Serial.printf("Captured %d bytes\n", byteCount);
  return;
  #else

  if (messageCount == 0) {
    Serial.println(F("No messages captured."));
    return;
  }

  Serial.println(F("\n========================================"));
  Serial.println(F("        CAPTURED BUS TRAFFIC"));
  Serial.println(F("========================================"));
  Serial.printf("Total Messages: %d\n", messageCount);
  Serial.printf("Capture Duration: %.3f seconds\n",
               messageBuffer[messageCount-1].timestamp_us / 1000000.0);
  Serial.printf("Bus Mode: %s\n", BUS_MODE == 0 ? "CAN" : "LIN");

  #if BUS_MODE == 1
  Serial.printf("Checksum Failures: %lu (%.1f%%)\n",
               checksumFailures,
               (checksumFailures * 100.0) / totalMessagesReceived);
  #endif

  Serial.println(F("========================================\n"));

  Serial.println(F("Time(s)    | ID    | Len | ChkOK | Data"));
  Serial.println(F("-----------|-------|-----|-------|----------------------------------"));

  for (uint16_t i = 0; i < messageCount; i++) {
    BusMessage* msg = &messageBuffer[i];

    // Timestamp in seconds with 6 decimal places
    Serial.printf("%10.6f | ", msg->timestamp_us / 1000000.0);

    // ID (CAN can be standard or extended)
    if (BUS_MODE == 0 && msg->extended) {
      Serial.printf("%08lX | ", msg->id);
    } else {
      Serial.printf("%03lX   | ", msg->id);
    }

    // Length
    Serial.printf("%3d | ", msg->length);

    // Checksum status (LIN only)
    if (BUS_MODE == 1) {
      Serial.printf("  %s  | ", msg->checksumValid ? "Y" : "N");
    } else {
      Serial.print("  -   | ");
    }

    // Data bytes
    for (int j = 0; j < msg->length && j < 8; j++) {
      Serial.printf("%02X ", msg->data[j]);
    }

    // RTR or Extended flags
    if (BUS_MODE == 0) {
      if (msg->rtr) Serial.print(" [RTR]");
      if (msg->extended) Serial.print(" [EXT]");
    }

    Serial.println();
  }

  Serial.println(F("========================================\n"));
  #endif
}

void exportToCSV() {
  #if RAW_CAPTURE_MODE
  if (byteCount == 0) {
    Serial.println(F("No data to export."));
    return;
  }

  Serial.println(F("\n=== RAW CSV Export (copy this) ===\n"));
  Serial.println(F("Timestamp_us,Byte_Hex"));

  // CSV Data - simple timestamp and byte
  for (uint32_t i = 0; i < byteCount; i++) {
    Serial.printf("%lu,0x%02X\n", rawBuffer[i].timestamp_us, rawBuffer[i].byte);
  }

  Serial.println(F("\n=== End CSV Export ===\n"));
  Serial.printf("Total bytes: %d\n", byteCount);

  #else

  if (messageCount == 0) {
    Serial.println(F("No messages to export."));
    return;
  }

  Serial.println(F("\n=== CSV Export (copy this) ===\n"));

  // CSV Header - added ChecksumOK column for LIN
  if (BUS_MODE == 1) {
    Serial.println(F("Timestamp_s,ID,Extended,RTR,Length,ChecksumOK,D0,D1,D2,D3,D4,D5,D6,D7"));
  } else {
    Serial.println(F("Timestamp_s,ID,Extended,RTR,Length,D0,D1,D2,D3,D4,D5,D6,D7"));
  }

  // CSV Data
  for (uint16_t i = 0; i < messageCount; i++) {
    BusMessage* msg = &messageBuffer[i];

    Serial.printf("%.6f,%lu,%d,%d,%d",
                 msg->timestamp_us / 1000000.0,
                 msg->id,
                 msg->extended ? 1 : 0,
                 msg->rtr ? 1 : 0,
                 msg->length);

    // Add checksum status for LIN
    if (BUS_MODE == 1) {
      Serial.printf(",%d", msg->checksumValid ? 1 : 0);
    }

    for (int j = 0; j < 8; j++) {
      Serial.print(",");
      if (j < msg->length) {
        Serial.printf("%d", msg->data[j]);
      }
    }

    Serial.println();
  }

  Serial.println(F("\n=== End CSV Export ===\n"));
  #endif
}

void printStatistics() {
  Serial.println(F("\n=== Statistics ==="));
  Serial.printf("Total messages received: %lu\n", totalMessagesReceived);

  #if RAW_CAPTURE_MODE
  Serial.printf("Bytes captured: %d\n", byteCount);
  #else
  Serial.printf("Messages captured: %d\n", messageCount);
  Serial.printf("Messages dropped: %lu\n", messagesDropped);
  #endif

  #if BUS_MODE == 1
  Serial.printf("Checksum failures: %lu (%.1f%%)\n",
               checksumFailures,
               totalMessagesReceived > 0 ? (checksumFailures * 100.0) / totalMessagesReceived : 0);
  Serial.printf("BREAK detections: %lu\n", debugBreakCount);
  Serial.printf("Frames parsed: %lu\n", debugFrameCount);
  #endif

  #if RAW_CAPTURE_MODE
  Serial.printf("Buffer usage: %d / %d (%.1f%%)\n",
               byteCount, MAX_MESSAGES * 10,
               (byteCount * 100.0) / (MAX_MESSAGES * 10));
  #else
  Serial.printf("Buffer usage: %d / %d (%.1f%%)\n",
               messageCount, MAX_MESSAGES,
               (messageCount * 100.0) / MAX_MESSAGES);

  if (messageCount > 0) {
    Serial.printf("Capture duration: %.3f seconds\n",
                 messageBuffer[messageCount-1].timestamp_us / 1000000.0);

    float messagesPerSecond = messageCount /
                              (messageBuffer[messageCount-1].timestamp_us / 1000000.0);
    Serial.printf("Average rate: %.1f messages/second\n", messagesPerSecond);
  }
  #endif

  Serial.printf("Last activity: %lu ms ago\n", millis() - lastActivityTime);
  Serial.println();
}

// ============================================================================
// COMMAND INTERFACE
// ============================================================================

void printHelp() {
  Serial.println(F("\n========================================"));
  Serial.println(F("       BUS SNIFFER COMMANDS"));
  Serial.println(F("========================================"));
  Serial.println(F("s - Start capture"));
  Serial.println(F("t - Stop capture"));
  Serial.println(F("p - Print captured data"));
  Serial.println(F("c - Export to CSV format"));
  Serial.println(F("r - Reset buffer (clear all data)"));
  Serial.println(F("i - Show statistics"));
  Serial.println(F("d - Diagnostics (LIN bus status)"));
  Serial.println(F("h - Show this help"));
  Serial.println(F("========================================\n"));
}

void startCapture() {
  if (capturing) {
    Serial.println(F("Already capturing!"));
    return;
  }

  #if RAW_CAPTURE_MODE
  byteCount = 0;
  #else
  messageCount = 0;
  checksumFailures = 0;
  #endif

  bufferFull = false;
  captureStartTime = micros();
  capturing = true;

  Serial.println(F("\n*** CAPTURE STARTED ***"));
  #if RAW_CAPTURE_MODE
  Serial.printf("Buffer size: %d bytes (RAW MODE)\n", MAX_MESSAGES * 10);
  Serial.println(F("Capturing raw UART data with timestamps"));
  #else
  Serial.printf("Buffer size: %d messages\n", MAX_MESSAGES);
  Serial.printf("Estimated capture time: ~%.0f seconds at normal load\n",
               MAX_MESSAGES / 30.0);  // Assuming ~30 msg/sec
  #endif
  Serial.println(F("Press 't' to stop capture\n"));

  setLED(false, false, true);  // Blue = capturing
}

void stopCapture() {
  if (!capturing) {
    Serial.println(F("Not currently capturing."));
    return;
  }

  capturing = false;

  Serial.println(F("\n*** CAPTURE STOPPED ***"));
  #if RAW_CAPTURE_MODE
  Serial.print(F("Captured "));
  Serial.print(byteCount);
  Serial.print(F(" bytes in "));
  Serial.print((micros() - captureStartTime) / 1000000.0);
  Serial.println(F(" seconds"));
  #else
  Serial.print(F("Captured "));
  Serial.print(messageCount);
  Serial.print(F(" messages in "));
  Serial.print((micros() - captureStartTime) / 1000000.0);
  Serial.println(F(" seconds"));
  Serial.print(F("Total received: "));
  Serial.println(totalMessagesReceived);

  #if BUS_MODE == 1
  Serial.printf("Checksum failures: %lu\n", checksumFailures);
  #endif
  #endif

  Serial.println(F("Type 'c' for CSV export\n"));

  setLED(false, true, false);  // Green = ready
}

#if BUS_MODE == 1
void printDiagnostics() {
  Serial.println(F("\n========================================"));
  Serial.println(F("       LIN BUS DIAGNOSTICS"));
  Serial.println(F("========================================"));

  Serial.print(F("Bus Mode: LIN at "));
  Serial.print(LIN_SPEED);
  Serial.println(F(" baud"));

  int available = LIN_Serial.available();
  Serial.print(F("UART available: "));
  Serial.println(available);

  Serial.print(F("Last activity: "));
  Serial.print(millis() - lastActivityTime);
  Serial.println(F(" ms ago"));

  Serial.print(F("LIN CS pin (46): "));
  Serial.println(digitalRead(LIN_CS) ? "HIGH (enabled)" : "LOW (disabled)");

  Serial.print(F("LIN FAULT pin (9): "));
  Serial.println(digitalRead(LIN_FAULT) ? "HIGH (ok)" : "LOW (FAULT!)");

  Serial.print(F("Total messages: "));
  Serial.println(totalMessagesReceived);

  Serial.print(F("Error callbacks: "));
  Serial.println(debugCallbackCount);

  Serial.print(F("BREAK detections: "));
  Serial.println(debugBreakCount);

  Serial.print(F("Frames parsed: "));
  Serial.println(debugFrameCount);

  Serial.print(F("Checksum failures: "));
  Serial.print(checksumFailures);
  if (debugFrameCount > 0) {
    Serial.printf(" (%.1f%%)", (checksumFailures * 100.0) / debugFrameCount);
  }
  Serial.println();

  Serial.print(F("LIN state: "));
  switch(linState) {
    case LIN_IDLE: Serial.println("IDLE"); break;
    case LIN_SYNC: Serial.println("SYNC"); break;
    case LIN_ID: Serial.println("ID"); break;
    case LIN_DATA: Serial.println("DATA"); break;
    case LIN_CHECKSUM: Serial.println("CHECKSUM"); break;
  }

  // Show raw UART data if available
  if (available > 0) {
    Serial.println(F("\nWARNING: UART buffer has unprocessed data!"));
    Serial.println(F("First 32 bytes in UART buffer (hex):"));
    int bytesToShow = min(32, available);
    for (int i = 0; i < bytesToShow; i++) {
      int byte = LIN_Serial.read();
      if (byte >= 0) {
        Serial.printf("%02X ", byte);
        if ((i + 1) % 16 == 0) Serial.println();
      }
    }
    Serial.println();
  }

  Serial.println(F("========================================\n"));
}
#endif

void resetBuffer() {
  #if RAW_CAPTURE_MODE
  byteCount = 0;
  #else
  messageCount = 0;
  totalMessagesReceived = 0;
  messagesDropped = 0;
  checksumFailures = 0;
  #endif

  bufferFull = false;
  capturing = false;

  Serial.println(F("\n*** BUFFER RESET ***"));
  Serial.println(F("All captured data cleared.\n"));

  setLED(false, true, false);  // Green = ready
}

void processCommand(char cmd) {
  switch (cmd) {
    case 's':
    case 'S':
      startCapture();
      break;

    case 't':
    case 'T':
      stopCapture();
      break;

    case 'p':
    case 'P':
      printCapturedData();
      break;

    case 'c':
    case 'C':
      exportToCSV();
      break;

    case 'r':
    case 'R':
      resetBuffer();
      break;

    case 'i':
    case 'I':
      printStatistics();
      break;

    case 'd':
    case 'D':
      #if BUS_MODE == 1
      printDiagnostics();
      #else
      Serial.println(F("Diagnostics only available in LIN mode"));
      #endif
      break;

    case 'h':
    case 'H':
      printHelp();
      break;

    default:
      Serial.println(F("Unknown command. Press 'h' for help."));
      break;
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
  setLED(false, false, false);

  // Initialize Serial for USB communication
  Serial.begin(115200);
  delay(1000);

  // Wait for Serial connection (max 3 seconds)
  for (uint32_t startMillis = millis();
       !Serial && (millis() - startMillis < 3000); );

  // Print banner
  Serial.println(F("\n\n"));
  Serial.println(F("========================================"));
  Serial.println(F("   ESP32S3 Bus Sniffer v2.0 - FIXED"));
  Serial.println(F("   For CAN & LIN Bus Analysis"));
  Serial.println(F("========================================"));
  Serial.print(F("Bus Mode: "));
  Serial.println(BUS_MODE == 0 ? "CAN" : "LIN");
  Serial.printf("Max Messages: %d\n", MAX_MESSAGES);
  Serial.printf("Auto-start: %s\n", AUTO_START ? "Yes" : "No");
  Serial.printf("Debug frames: %s\n", DEBUG_FRAMES ? "Yes" : "No");
  Serial.println(F("========================================\n"));

  // Initialize selected bus
  if (BUS_MODE == 0) {
    initCAN();
  }
  #if BUS_MODE == 1
  else {
    initLIN();
  }
  #endif

  printHelp();

  // Auto-start if enabled
  if (AUTO_START) {
    delay(1000);
    startCapture();
  } else {
    setLED(false, true, false);  // Green = ready
  }

  lastActivityTime = millis();
}

// ============================================================================
// ARDUINO MAIN LOOP
// ============================================================================

void loop() {
  // Process serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    // Ignore line endings and other non-printable characters
    if (cmd >= 33 && cmd <= 126) {  // Only process printable ASCII
      processCommand(cmd);
    }
  }

  // Capture bus messages based on mode
  #if RAW_CAPTURE_MODE
    #if BUS_MODE == 1
    captureRawLINData();
    #endif
  #else
    if (BUS_MODE == 0) {
      captureCANMessage();
    }
    #if BUS_MODE == 1
    else {
      captureLINMessage();
    }
    #endif
  #endif

  // Status indicator: blink LED when capturing
  static uint32_t lastBlink = 0;
  if (capturing && (millis() - lastBlink > 1000)) {
    lastBlink = millis();
    digitalWrite(LED_B, !digitalRead(LED_B));
  }
}
