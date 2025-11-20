#!/usr/bin/env python3
"""
LIN Message Grouping and Analysis Script

This script reads parsed LIN message CSV files and groups messages by ID,
identifying unique message patterns and tracking their occurrence counts and timestamps.

For each unique message (defined by Break, Sync, ID, Data, Checksum combination),
the script records:
- The message components
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


class LINMessage:
    """Represents a unique LIN message pattern"""

    def __init__(self, break_byte: str, sync: str, msg_id: str, data: str, checksum: str):
        self.break_byte = break_byte
        self.sync = sync
        self.msg_id = msg_id
        self.data = data
        self.checksum = checksum
        self.count = 0
        self.timestamps = []

    def add_occurrence(self, timestamp: int):
        """Record an occurrence of this message"""
        self.count += 1
        self.timestamps.append(timestamp)

    def get_signature(self) -> Tuple[str, str, str, str, str]:
        """Get unique signature for this message"""
        return (self.break_byte, self.sync, self.msg_id, self.data, self.checksum)

    def to_csv_row(self) -> List[str]:
        """Convert to CSV row format"""
        timestamps_str = ','.join(str(ts) for ts in self.timestamps)
        return [
            self.msg_id,
            self.break_byte,
            self.sync,
            self.data,
            self.checksum,
            str(self.count),
            timestamps_str
        ]

    def __repr__(self):
        return f"LINMessage(id={self.msg_id}, data={self.data}, count={self.count})"


class LINMessageGrouper:
    """Groups and analyzes LIN messages by ID"""

    def __init__(self, input_file: str):
        self.input_file = input_file
        # Dictionary: msg_id -> list of LINMessage objects
        self.messages_by_id = defaultdict(list)
        # Dictionary to track unique messages: signature -> LINMessage
        self.unique_messages = {}

    def load_and_group(self) -> bool:
        """Load parsed LIN messages and group them"""
        print(f"Loading and grouping messages from: {Path(self.input_file).name}")

        try:
            with open(self.input_file, 'r', newline='') as f:
                reader = csv.DictReader(f)

                for row in reader:
                    timestamp = int(row['Timestamp_us'])
                    break_byte = row['Break']
                    sync = row['Sync']
                    msg_id = row['ID']
                    data = row['Data']
                    checksum = row['Checksum']

                    # Create signature for this message
                    signature = (break_byte, sync, msg_id, data, checksum)

                    # Check if we've seen this exact message before
                    if signature in self.unique_messages:
                        # Add timestamp to existing message
                        self.unique_messages[signature].add_occurrence(timestamp)
                    else:
                        # Create new unique message
                        msg = LINMessage(break_byte, sync, msg_id, data, checksum)
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
                writer.writerow(['ID', 'Break', 'Sync', 'Data', 'Checksum', 'Count', 'Timestamps'])

                # Sort IDs for consistent output
                sorted_ids = sorted(self.messages_by_id.keys(), key=lambda x: int(x, 16))

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
        sorted_ids = sorted(self.messages_by_id.keys(), key=lambda x: int(x, 16))

        for msg_id in sorted_ids:
            messages = self.messages_by_id[msg_id]
            unique_count = len(messages)
            total_count = sum(msg.count for msg in messages)
            print(f"    ID {msg_id}: {unique_count} unique message(s), {total_count} total occurrence(s)")


def main():
    """Main function to process all parsed CSV files"""

    # Define paths
    script_dir = Path(__file__).parent
    parsing_dir = script_dir.parent / 'parsing'
    grouping_dir = script_dir

    # Find all parsed CSV files
    parsed_files = sorted(parsing_dir.glob('*_parsed.csv'))

    if not parsed_files:
        print(f"No parsed CSV files found in {parsing_dir}")
        return 1

    print(f"Found {len(parsed_files)} parsed CSV files to analyze")
    print("=" * 70)

    # Process each file
    success_count = 0
    for parsed_file in parsed_files:
        print(f"\nProcessing: {parsed_file.name}")
        print("-" * 70)

        # Create output filename
        # Remove '_parsed' and add '_grouped'
        output_name = parsed_file.stem.replace('_parsed', '_grouped') + '.csv'
        output_file = grouping_dir / output_name

        # Group and analyze
        grouper = LINMessageGrouper(str(parsed_file))

        if not grouper.load_and_group():
            print(f"Failed to load file: {parsed_file.name}")
            continue

        # Print summary
        grouper.print_summary()

        # Save results
        if grouper.save_grouped_csv(str(output_file)):
            success_count += 1
            print(f"[OK] Successfully processed {parsed_file.name}")
        else:
            print(f"[FAIL] Failed to save output for {parsed_file.name}")

    print("\n" + "=" * 70)
    print(f"Processing complete: {success_count}/{len(parsed_files)} files successfully analyzed")

    return 0 if success_count == len(parsed_files) else 1


if __name__ == '__main__':
    sys.exit(main())
