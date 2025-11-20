#!/usr/bin/env python3
"""
CAN Message Grouping and Analysis Script

This script reads CAN message CSV files and groups messages by ID,
identifying unique message patterns and tracking their occurrence counts and timestamps.

For each unique message (defined by ID and data bytes combination),
the script records:
- The message ID and data bytes
- How many times it appeared
- All timestamps where it appeared

Output is organized by ID for easy analysis of message patterns.

Author: Projects In Motion (https://github.com/projectsinmotion)
"""

import csv
import os
import sys
from pathlib import Path
from typing import Dict, List, Tuple
from collections import defaultdict


class CANMessage:
    """Represents a unique CAN message pattern"""

    def __init__(self, msg_id: str, extended: str, rtr: str, length: str, data_bytes: Tuple[str, ...]):
        self.msg_id = msg_id
        self.extended = extended
        self.rtr = rtr
        self.length = length
        self.data_bytes = data_bytes  # Tuple of data bytes (D0-D7)
        self.count = 0
        self.timestamps = []

    def add_occurrence(self, timestamp: float):
        """Record an occurrence of this message"""
        self.count += 1
        self.timestamps.append(timestamp)

    def get_signature(self) -> Tuple[str, str, str, str, Tuple[str, ...]]:
        """Get unique signature for this message"""
        return (self.msg_id, self.extended, self.rtr, self.length, self.data_bytes)

    def to_csv_row(self) -> List[str]:
        """Convert to CSV row format"""
        timestamps_str = ','.join(str(ts) for ts in self.timestamps)
        # Convert data bytes tuple to comma-separated string
        data_str = ','.join(db if db else '' for db in self.data_bytes)
        return [
            self.msg_id,
            self.extended,
            self.rtr,
            self.length,
            data_str,
            str(self.count),
            timestamps_str
        ]

    def __repr__(self):
        return f"CANMessage(id={self.msg_id}, data={self.data_bytes}, count={self.count})"


class CANMessageGrouper:
    """Groups and analyzes CAN messages by ID"""

    def __init__(self, input_file: str):
        self.input_file = input_file
        # Dictionary: msg_id -> list of CANMessage objects
        self.messages_by_id = defaultdict(list)
        # Dictionary to track unique messages: signature -> CANMessage
        self.unique_messages = {}

    def load_and_group(self) -> bool:
        """Load CAN messages and group them"""
        print(f"Loading and grouping messages from: {Path(self.input_file).name}")

        try:
            with open(self.input_file, 'r', newline='') as f:
                reader = csv.DictReader(f)

                for row in reader:
                    timestamp = float(row['Timestamp_s'])
                    msg_id = row['ID']
                    extended = row['Extended']
                    rtr = row['RTR']
                    length = row['Length']

                    # Extract data bytes (D0-D7)
                    data_bytes = tuple(
                        row.get(f'D{i}', '') for i in range(8)
                    )

                    # Create signature for this message
                    signature = (msg_id, extended, rtr, length, data_bytes)

                    # Check if we've seen this exact message before
                    if signature in self.unique_messages:
                        # Add timestamp to existing message
                        self.unique_messages[signature].add_occurrence(timestamp)
                    else:
                        # Create new unique message
                        msg = CANMessage(msg_id, extended, rtr, length, data_bytes)
                        msg.add_occurrence(timestamp)
                        self.unique_messages[signature] = msg
                        self.messages_by_id[msg_id].append(msg)

            total_unique = len(self.unique_messages)
            total_ids = len(self.messages_by_id)
            print(f"  Found {total_unique} unique messages across {total_ids} different IDs")

            return True

        except Exception as e:
            print(f"Error loading file: {e}")
            return False

    def save_grouped_csv(self, output_file: str) -> bool:
        """Save grouped messages to CSV, organized by ID"""
        print(f"Saving grouped analysis to: {Path(output_file).name}")

        try:
            with open(output_file, 'w', newline='') as f:
                writer = csv.writer(f)

                # Write header
                writer.writerow(['ID', 'Extended', 'RTR', 'Length', 'Data', 'Count', 'Timestamps'])

                # Sort IDs for consistent output (numerically)
                sorted_ids = sorted(self.messages_by_id.keys(), key=lambda x: int(x))

                # Write messages grouped by ID
                for msg_id in sorted_ids:
                    messages = self.messages_by_id[msg_id]

                    # Sort messages within each ID by count (descending)
                    messages.sort(key=lambda m: m.count, reverse=True)

                    # Write all messages for this ID
                    for msg in messages:
                        writer.writerow(msg.to_csv_row())

            print(f"  Saved {len(self.unique_messages)} unique message patterns")
            return True

        except Exception as e:
            print(f"Error saving file: {e}")
            return False

    def print_summary(self):
        """Print summary statistics"""
        print("\n  Summary by ID:")
        sorted_ids = sorted(self.messages_by_id.keys(), key=lambda x: int(x))

        for msg_id in sorted_ids:
            messages = self.messages_by_id[msg_id]
            unique_count = len(messages)
            total_count = sum(msg.count for msg in messages)
            print(f"    ID {msg_id}: {unique_count} unique message(s), {total_count} total occurrence(s)")


def main():
    """Main function to process all CAN capture CSV files"""

    # Define paths
    script_dir = Path(__file__).parent
    captures_dir = script_dir.parent.parent / 'captures'
    grouping_dir = script_dir

    # Find all capture CSV files
    capture_files = sorted(captures_dir.glob('capture_*.csv'))

    if not capture_files:
        print(f"No capture CSV files found in {captures_dir}")
        return 1

    print(f"Found {len(capture_files)} capture CSV files to analyze")
    print("=" * 70)

    # Process each file
    success_count = 0
    for capture_file in capture_files:
        print(f"\nProcessing: {capture_file.name}")
        print("-" * 70)

        # Create output filename
        # Add '_grouped' to the filename
        output_name = capture_file.stem + '_grouped.csv'
        output_file = grouping_dir / output_name

        # Group and analyze
        grouper = CANMessageGrouper(str(capture_file))

        if not grouper.load_and_group():
            print(f"Failed to load file: {capture_file.name}")
            continue

        # Print summary
        grouper.print_summary()

        # Save results
        if grouper.save_grouped_csv(str(output_file)):
            success_count += 1
            print(f"[OK] Successfully processed {capture_file.name}")
        else:
            print(f"[FAIL] Failed to save output for {capture_file.name}")

    print("\n" + "=" * 70)
    print(f"Processing complete: {success_count}/{len(capture_files)} files successfully analyzed")

    return 0 if success_count == len(capture_files) else 1


if __name__ == '__main__':
    sys.exit(main())
