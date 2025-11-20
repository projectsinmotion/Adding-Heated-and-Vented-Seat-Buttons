#!/usr/bin/env python3
"""
CAN Message Comparison Script

This script compares grouped CAN message CSV files to identify messages that appear
in one capture but not another. This helps identify what changes when:
- Buttons are pressed
- Lights are cycled

Comparisons are based on ID, Extended, RTR, Length, and Data fields.

Author: Projects In Motion (https://github.com/projectsinmotion)
"""

import csv
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple
from collections import defaultdict


class CANMessage:
    """Represents a unique CAN message for comparison"""

    def __init__(self, msg_id: str, extended: str, rtr: str, length: str,
                 data: str, count: int, timestamps: str):
        self.msg_id = msg_id
        self.extended = extended
        self.rtr = rtr
        self.length = length
        self.data = data
        self.count = count
        self.timestamps = timestamps

    def get_signature(self) -> Tuple[str, str, str, str, str]:
        """Get unique signature for comparison (ID, Extended, RTR, Length, Data)"""
        return (self.msg_id, self.extended, self.rtr, self.length, self.data)

    def to_csv_row(self) -> List[str]:
        """Convert to CSV row format"""
        return [
            self.msg_id,
            self.extended,
            self.rtr,
            self.length,
            self.data,
            str(self.count),
            self.timestamps
        ]

    def __repr__(self):
        return f"CANMessage(id={self.msg_id}, data={self.data}, count={self.count})"


class MessageSet:
    """Container for a set of messages from a capture file"""

    def __init__(self, filename: str):
        self.filename = filename
        self.messages: Dict[Tuple[str, str, str, str, str], CANMessage] = {}

    def add_message(self, msg: CANMessage):
        """Add a message to the set"""
        signature = msg.get_signature()
        self.messages[signature] = msg

    def get_signatures(self) -> Set[Tuple[str, str, str, str, str]]:
        """Get all message signatures in this set"""
        return set(self.messages.keys())

    def get_message(self, signature: Tuple[str, str, str, str, str]) -> CANMessage:
        """Get a specific message by signature"""
        return self.messages.get(signature)

    def __len__(self):
        return len(self.messages)


class CANComparator:
    """Compares two sets of CAN messages"""

    def __init__(self, file_a: str, file_b: str):
        self.file_a = file_a
        self.file_b = file_b
        self.set_a = MessageSet(Path(file_a).stem)
        self.set_b = MessageSet(Path(file_b).stem)

    def load_messages(self) -> bool:
        """Load messages from both files"""
        try:
            # Load file A
            with open(self.file_a, 'r', newline='') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    msg = CANMessage(
                        msg_id=row['ID'],
                        extended=row['Extended'],
                        rtr=row['RTR'],
                        length=row['Length'],
                        data=row['Data'],
                        count=int(row['Count']),
                        timestamps=row['Timestamps']
                    )
                    self.set_a.add_message(msg)

            # Load file B
            with open(self.file_b, 'r', newline='') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    msg = CANMessage(
                        msg_id=row['ID'],
                        extended=row['Extended'],
                        rtr=row['RTR'],
                        length=row['Length'],
                        data=row['Data'],
                        count=int(row['Count']),
                        timestamps=row['Timestamps']
                    )
                    self.set_b.add_message(msg)

            return True

        except Exception as e:
            print(f"Error loading files: {e}")
            return False

    def compare(self) -> Tuple[List[CANMessage], List[CANMessage]]:
        """
        Compare the two message sets

        Returns:
            Tuple of (messages in A but not B, messages in B but not A)
        """
        sigs_a = self.set_a.get_signatures()
        sigs_b = self.set_b.get_signatures()

        # Find unique messages
        unique_to_a = sigs_a - sigs_b
        unique_to_b = sigs_b - sigs_a

        # Get full message objects
        msgs_unique_to_a = [self.set_a.get_message(sig) for sig in unique_to_a]
        msgs_unique_to_b = [self.set_b.get_message(sig) for sig in unique_to_b]

        # Sort by ID then by data
        msgs_unique_to_a.sort(key=lambda m: (int(m.msg_id), m.data))
        msgs_unique_to_b.sort(key=lambda m: (int(m.msg_id), m.data))

        return msgs_unique_to_a, msgs_unique_to_b

    def save_comparison(self, output_file: str) -> bool:
        """Save comparison results to CSV file"""
        try:
            msgs_in_a_not_b, msgs_in_b_not_a = self.compare()

            with open(output_file, 'w', newline='') as f:
                writer = csv.writer(f)

                # Write header
                writer.writerow(['Section', 'ID', 'Extended', 'RTR', 'Length', 'Data', 'Count', 'Timestamps'])

                # Write messages in A but not B
                if msgs_in_a_not_b:
                    writer.writerow([f'=== In {Path(self.file_a).stem} but NOT in {Path(self.file_b).stem} ===', '', '', '', '', '', '', ''])
                    for msg in msgs_in_a_not_b:
                        writer.writerow(['IN_A_NOT_B'] + msg.to_csv_row())
                    writer.writerow(['', '', '', '', '', '', '', ''])  # Blank row separator

                # Write messages in B but not A
                if msgs_in_b_not_a:
                    writer.writerow([f'=== In {Path(self.file_b).stem} but NOT in {Path(self.file_a).stem} ===', '', '', '', '', '', '', ''])
                    for msg in msgs_in_b_not_a:
                        writer.writerow(['IN_B_NOT_A'] + msg.to_csv_row())

                # Write summary at the end
                writer.writerow(['', '', '', '', '', '', '', ''])  # Blank row
                writer.writerow(['=== SUMMARY ===', '', '', '', '', '', '', ''])
                writer.writerow(['Total messages in A', str(len(self.set_a)), '', '', '', '', '', ''])
                writer.writerow(['Total messages in B', str(len(self.set_b)), '', '', '', '', '', ''])
                writer.writerow(['Unique to A', str(len(msgs_in_a_not_b)), '', '', '', '', '', ''])
                writer.writerow(['Unique to B', str(len(msgs_in_b_not_a)), '', '', '', '', '', ''])
                writer.writerow(['Messages in common', str(len(self.set_a.get_signatures() & self.set_b.get_signatures())), '', '', '', '', '', ''])

            return True

        except Exception as e:
            print(f"Error saving comparison: {e}")
            return False


def main():
    """Main function to perform all comparisons"""

    # Define paths
    script_dir = Path(__file__).parent
    grouping_dir = script_dir.parent / 'grouping'
    comparison_dir = script_dir

    # Define comparison pairs: (file_a, file_b, output_name)
    # Format: comparing B to A (what's different in B compared to A)
    comparisons = [
        ('00', '01', '01_vs_00_no_presses_lights_off_on_vs_no_lights'),
        ('01', '02', '02_vs_01_no_presses_lights_max_min_vs_lights_off_on'),
        ('00', '03', '03_vs_00_drivers_heat_presses_vs_no_presses'),
        ('00', '04', '04_vs_00_drivers_vent_presses_vs_no_presses'),
        ('00', '05', '05_vs_00_passengers_heat_presses_vs_no_presses'),
        ('00', '06', '06_vs_00_passengers_vent_presses_vs_no_presses'),
    ]

    print(f"Starting CAN message comparisons")
    print("=" * 70)

    success_count = 0

    for base_num, compare_num, output_name in comparisons:
        # Find the grouped files
        base_pattern = f"capture_{base_num}_*_grouped.csv"
        compare_pattern = f"capture_{compare_num}_*_grouped.csv"

        base_files = list(grouping_dir.glob(base_pattern))
        compare_files = list(grouping_dir.glob(compare_pattern))

        if not base_files or not compare_files:
            print(f"\n[SKIP] Missing files for comparison {compare_num} vs {base_num}")
            continue

        base_file = base_files[0]
        compare_file = compare_files[0]

        print(f"\nComparing: {compare_file.name} vs {base_file.name}")
        print("-" * 70)

        # Create comparator
        comparator = CANComparator(str(base_file), str(compare_file))

        if not comparator.load_messages():
            print(f"[FAIL] Could not load messages")
            continue

        # Generate output filename
        output_file = comparison_dir / f"comparison_{output_name}.csv"

        # Perform comparison and save
        if comparator.save_comparison(str(output_file)):
            # Get stats for display
            msgs_in_a_not_b, msgs_in_b_not_a = comparator.compare()
            common = len(comparator.set_a.get_signatures() & comparator.set_b.get_signatures())

            print(f"  Messages in {base_file.stem}: {len(comparator.set_a)}")
            print(f"  Messages in {compare_file.stem}: {len(comparator.set_b)}")
            print(f"  Unique to base file: {len(msgs_in_a_not_b)}")
            print(f"  Unique to compare file: {len(msgs_in_b_not_a)}")
            print(f"  Messages in common: {common}")
            print(f"  Saved to: {output_file.name}")
            print(f"[OK] Comparison complete")
            success_count += 1
        else:
            print(f"[FAIL] Could not save comparison")

    print("\n" + "=" * 70)
    print(f"Comparison complete: {success_count}/{len(comparisons)} comparisons successful")

    return 0 if success_count == len(comparisons) else 1


if __name__ == '__main__':
    sys.exit(main())
