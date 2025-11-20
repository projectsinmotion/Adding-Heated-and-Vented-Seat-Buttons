#!/usr/bin/env python3
"""
LIN Bus Raw UART Data Parser

This script parses raw UART capture files from a LIN bus and extracts
individual LIN frames with their components: break, sync, ID, data, and checksum.

LIN Frame Structure:
- Break Field: Dominant state for extended period (appears as 0x00 in UART)
- Sync Byte: Always 0x55 (binary 01010101)
- Protected ID (PID): 1 byte (6-bit ID + 2-bit parity)
- Data: 0-8 bytes
- Checksum: 1 byte

Author: Projects In Motion (https://github.com/projectsinmotion)
"""

import csv
import os
import sys
from pathlib import Path
from typing import List, Tuple, Optional


class LINFrame:
    """Represents a single LIN frame"""

    def __init__(self, timestamp: int, break_byte: Optional[str], sync: str,
                 pid: str, data: List[str], checksum: str):
        self.timestamp = timestamp
        self.break_byte = break_byte if break_byte else ""
        self.sync = sync
        self.pid = pid
        self.data = data
        self.checksum = checksum

    def to_csv_row(self) -> List[str]:
        """Convert frame to CSV row"""
        data_str = ','.join(self.data) if self.data else ""
        return [
            str(self.timestamp),
            self.break_byte,
            self.sync,
            self.pid,
            data_str,
            self.checksum
        ]

    def __repr__(self):
        data_str = ' '.join(self.data) if self.data else "[]"
        return f"LINFrame(ts={self.timestamp}, break={self.break_byte}, sync={self.sync}, pid={self.pid}, data=[{data_str}], chk={self.checksum})"


class LINParser:
    """Parser for LIN bus raw UART captures"""

    SYNC_BYTE = '0x55'

    def __init__(self, input_file: str):
        self.input_file = input_file
        self.bytes_data = []  # List of (timestamp, byte_hex) tuples

    def load_capture(self) -> bool:
        """Load raw UART capture from CSV format"""
        print(f"Loading capture file: {self.input_file}")

        try:
            with open(self.input_file, 'r') as f:
                lines = f.readlines()

            # Find the start of CSV data (after "Timestamp_us,Byte_Hex" header)
            csv_start = -1
            for i, line in enumerate(lines):
                if 'Timestamp_us,Byte_Hex' in line:
                    csv_start = i + 1
                    break
                elif line.strip().startswith('===') and 'End' in line:
                    # Reached end marker without finding data
                    return False

            if csv_start == -1:
                print("Error: Could not find CSV header in capture file")
                return False

            # Parse CSV data
            for line in lines[csv_start:]:
                line = line.strip()

                # Stop at end marker
                if line.startswith('===') or not line:
                    break

                parts = line.split(',')
                if len(parts) == 2:
                    try:
                        timestamp = int(parts[0])
                        byte_hex = parts[1].strip()
                        self.bytes_data.append((timestamp, byte_hex))
                    except ValueError:
                        continue

            print(f"Loaded {len(self.bytes_data)} bytes from capture")
            return len(self.bytes_data) > 0

        except Exception as e:
            print(f"Error loading capture file: {e}")
            return False

    def parse_frames(self) -> List[LINFrame]:
        """Parse LIN frames from raw UART bytes"""
        frames = []
        i = 0

        print("Parsing LIN frames...")

        while i < len(self.bytes_data):
            # Look for sync byte (0x55)
            if self.bytes_data[i][1].upper() == self.SYNC_BYTE.upper():
                frame = self._extract_frame(i)
                if frame:
                    frames.append(frame)
                    # Move past this frame
                    # Skip: sync + PID + data + checksum
                    frame_len = 1 + 1 + len(frame.data) + 1
                    i += frame_len
                else:
                    i += 1
            else:
                i += 1

        print(f"Parsed {len(frames)} LIN frames")
        return frames

    def _extract_frame(self, sync_idx: int) -> Optional[LINFrame]:
        """
        Extract a single LIN frame starting at sync_idx

        Args:
            sync_idx: Index of the sync byte (0x55)

        Returns:
            LINFrame object or None if frame cannot be parsed
        """
        # Check if we have enough bytes for at least sync + PID + checksum
        if sync_idx + 2 >= len(self.bytes_data):
            return None

        # Get timestamp of sync byte
        timestamp = self.bytes_data[sync_idx][0]

        # Get break byte (byte before sync, if it's 0x00)
        break_byte = None
        if sync_idx > 0:
            prev_byte = self.bytes_data[sync_idx - 1][1]
            if prev_byte.upper() == '0X00':
                break_byte = prev_byte

        # Sync byte
        sync = self.bytes_data[sync_idx][1]

        # PID (protected ID)
        if sync_idx + 1 >= len(self.bytes_data):
            return None
        pid = self.bytes_data[sync_idx + 1][1]

        # Determine frame length by looking ahead
        # Strategy: Look for the next 0x00 followed by 0x55 pattern, or end of data
        # The byte just before the next 0x00->0x55 pattern (or before next 0x55) is the checksum

        data_bytes = []
        checksum = None
        current_idx = sync_idx + 2  # Start after PID

        # Look ahead for frame boundary
        # Max LIN frame: PID + 8 data bytes + 1 checksum = 10 bytes after sync
        max_idx = min(sync_idx + 11, len(self.bytes_data))

        # Find next sync byte
        next_sync_idx = None
        for j in range(current_idx, len(self.bytes_data)):
            if self.bytes_data[j][1].upper() == self.SYNC_BYTE.upper():
                next_sync_idx = j
                break

        # If we found the next sync, frame ends before it
        if next_sync_idx is not None:
            frame_end_idx = next_sync_idx

            # Check if there's a 0x00 before the next sync (break delimiter)
            if next_sync_idx > 0 and self.bytes_data[next_sync_idx - 1][1].upper() == '0X00':
                frame_end_idx = next_sync_idx - 1
        else:
            # No next sync found, use max_idx or end of data
            frame_end_idx = max_idx

        # Enforce maximum LIN frame length (PID + max 8 data bytes + checksum = 10 bytes after sync)
        # This prevents including inter-frame bytes as part of the frame
        frame_end_idx = min(frame_end_idx, max_idx)

        # Additional check: Look for timing gaps that indicate inter-frame spacing
        # LIN bytes within a frame typically have <1ms spacing
        # Inter-frame gaps are typically >1ms
        for j in range(current_idx, min(frame_end_idx, len(self.bytes_data)) - 1):
            current_time = self.bytes_data[j][0]
            next_time = self.bytes_data[j + 1][0]
            time_gap = next_time - current_time

            # If gap > 1000 microseconds (1ms), likely an inter-frame gap
            if time_gap > 1000:
                # Frame ends after this byte
                frame_end_idx = j + 1
                break

        # Extract data and checksum
        # Last byte before frame_end_idx is checksum
        # Everything between PID and checksum is data

        if frame_end_idx <= current_idx:
            # No data, no checksum - unusual but handle it
            checksum = "0x00"
        elif frame_end_idx == current_idx + 1:
            # Only one byte: it's the checksum
            checksum = self.bytes_data[current_idx][1]
        else:
            # Multiple bytes: last is checksum, rest is data
            for j in range(current_idx, frame_end_idx - 1):
                data_bytes.append(self.bytes_data[j][1])
            checksum = self.bytes_data[frame_end_idx - 1][1]

        return LINFrame(
            timestamp=timestamp,
            break_byte=break_byte,
            sync=sync,
            pid=pid,
            data=data_bytes,
            checksum=checksum
        )

    def save_to_csv(self, frames: List[LINFrame], output_file: str):
        """Save parsed frames to CSV file"""
        print(f"Saving {len(frames)} frames to: {output_file}")

        try:
            with open(output_file, 'w', newline='') as f:
                writer = csv.writer(f)

                # Write header
                writer.writerow(['Timestamp_us', 'Break', 'Sync', 'ID', 'Data', 'Checksum'])

                # Write frames
                for frame in frames:
                    writer.writerow(frame.to_csv_row())

            print(f"Successfully saved CSV file")
            return True

        except Exception as e:
            print(f"Error saving CSV file: {e}")
            return False


def main():
    """Main function to parse all LIN capture files"""

    # Define paths
    script_dir = Path(__file__).parent
    captures_dir = script_dir.parent.parent / 'captures'
    analysis_dir = script_dir

    # Find all capture files
    capture_files = sorted(captures_dir.glob('lin_capture_*.txt'))

    if not capture_files:
        print(f"No capture files found in {captures_dir}")
        return 1

    print(f"Found {len(capture_files)} capture files to process")
    print("=" * 70)

    # Process each capture file
    success_count = 0
    for capture_file in capture_files:
        print(f"\nProcessing: {capture_file.name}")
        print("-" * 70)

        # Create output filename
        output_name = capture_file.stem + '_parsed.csv'
        output_file = analysis_dir / output_name

        # Parse the capture
        parser = LINParser(str(capture_file))

        if not parser.load_capture():
            print(f"Failed to load capture file: {capture_file.name}")
            continue

        frames = parser.parse_frames()

        if not frames:
            print(f"No frames parsed from: {capture_file.name}")
            continue

        # Save to CSV
        if parser.save_to_csv(frames, str(output_file)):
            success_count += 1
            print(f"[OK] Successfully processed {capture_file.name}")
        else:
            print(f"[FAIL] Failed to save output for {capture_file.name}")

    print("\n" + "=" * 70)
    print(f"Processing complete: {success_count}/{len(capture_files)} files successfully processed")

    return 0 if success_count == len(capture_files) else 1


if __name__ == '__main__':
    sys.exit(main())
