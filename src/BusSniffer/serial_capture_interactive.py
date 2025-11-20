#!/usr/bin/env python3
"""
Interactive Serial Capture - Send commands and log output
"""

import serial
import serial.tools.list_ports
import datetime
import os
import sys
import threading
import time

def list_ports():
    """List all available serial ports"""
    ports = serial.tools.list_ports.comports()
    print("\nAvailable Serial Ports:")
    print("-" * 60)
    for i, port in enumerate(ports):
        print(f"{i}: {port.device} - {port.description}")
    print("-" * 60)
    return [p.device for p in ports]

def read_serial(ser, output_file, csv_file):
    """Thread to read from serial port"""
    csv_started = False
    csv_lines = []

    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            while ser.is_open:
                if ser.in_waiting > 0:
                    try:
                        line = ser.readline()
                        line_str = line.decode('utf-8', errors='replace').rstrip()

                        # Write to file
                        f.write(line_str + '\n')
                        f.flush()

                        # Print to console
                        if len(line_str) > 100:
                            print(f"{line_str[:100]}... [{len(line_str)} chars]")
                        else:
                            print(line_str)

                        # Detect CSV output
                        if "=== CSV Export" in line_str or "Timestamp_s,ID" in line_str:
                            csv_started = True
                            csv_lines = []
                            if "Timestamp_s,ID" in line_str:
                                csv_lines.append(line_str)
                        elif csv_started:
                            if "=== End CSV" in line_str:
                                # Save CSV file
                                with open(csv_file, 'w', encoding='utf-8') as csv_f:
                                    csv_f.write('\n'.join(csv_lines))
                                print(f"\n>>> CSV saved to: {csv_file} <<<\n")
                                csv_started = False
                            else:
                                if line_str.strip():
                                    csv_lines.append(line_str)
                    except:
                        pass
                else:
                    time.sleep(0.01)
    except:
        pass

def main():
    """Main function"""

    # List available ports
    available_ports = list_ports()

    if not available_ports:
        print("\nNo serial ports found!")
        print("Make sure ESP32 is connected.")
        sys.exit(1)

    # Get port selection
    print("\nEnter port number (or full port name): ", end='')
    port_input = input().strip()

    # Parse input
    if port_input.isdigit():
        port_idx = int(port_input)
        if 0 <= port_idx < len(available_ports):
            port = available_ports[port_idx]
        else:
            print(f"Invalid port number. Must be 0-{len(available_ports)-1}")
            sys.exit(1)
    else:
        port = port_input

    # Get baudrate
    print("Baudrate [115200]: ", end='')
    baudrate_input = input().strip()
    baudrate = int(baudrate_input) if baudrate_input else 115200

    # Create output directory
    output_dir = "captures"
    os.makedirs(output_dir, exist_ok=True)

    # Generate timestamped filenames
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    output_file = os.path.join(output_dir, f"bus_capture_{timestamp}.txt")
    csv_file = os.path.join(output_dir, f"bus_capture_{timestamp}.csv")

    print(f"\n{'='*60}")
    print(f"Interactive Serial Monitor")
    print(f"{'='*60}")
    print(f"Port: {port}")
    print(f"Baudrate: {baudrate}")
    print(f"Output: {output_file}")
    print(f"CSV: {csv_file}")
    print(f"{'='*60}")
    print("\nType commands and press Enter to send to ESP32")
    print("Common commands: s (start), t (stop), c (export CSV), h (help)")
    print("Press Ctrl+C to exit\n")

    try:
        # Open serial port
        ser = serial.Serial(port, baudrate, timeout=1)
        time.sleep(2)  # Wait for connection to stabilize

        # Start reading thread
        read_thread = threading.Thread(target=read_serial, args=(ser, output_file, csv_file), daemon=True)
        read_thread.start()

        # Main loop - send commands
        while True:
            try:
                # Get user input
                cmd = input()

                # Send to ESP32
                if cmd:
                    ser.write((cmd + '\n').encode('utf-8'))
                    ser.flush()

            except EOFError:
                break

    except KeyboardInterrupt:
        print(f"\n\n{'='*60}")
        print("Logging stopped by user")
        print(f"Complete output saved to: {output_file}")
        if os.path.exists(csv_file):
            print(f"CSV data saved to: {csv_file}")
        print(f"{'='*60}\n")

    except serial.SerialException as e:
        print(f"\nSerial port error: {e}")
        print("Make sure:")
        print("  1. ESP32 is connected")
        print("  2. Correct COM port selected")
        print("  3. Arduino IDE Serial Monitor is CLOSED")
        sys.exit(1)

    except Exception as e:
        print(f"\nUnexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()
