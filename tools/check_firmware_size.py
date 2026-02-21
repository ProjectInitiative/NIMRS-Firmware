#!/usr/bin/env python3
import sys
import os
import csv

def parse_partition_size(partitions_csv_path, partition_name="app0"):
    try:
        with open(partitions_csv_path, 'r') as f:
            reader = csv.reader(f)
            for row in reader:
                # Skip comments and empty lines
                if not row or row[0].strip().startswith("#"):
                    continue

                parts = [p.strip() for p in row]
                if len(parts) >= 5 and parts[0] == partition_name:
                    size_str = parts[4]
                    try:
                        return int(size_str, 0)
                    except ValueError:
                        print(f"Error: Invalid size format for partition '{partition_name}': {size_str}")
                        return None
    except FileNotFoundError:
        print(f"Error: partitions.csv not found at {partitions_csv_path}")
        return None
    return None

def check_size(bin_path, max_size):
    if not os.path.exists(bin_path):
        # Try finding any .bin file in the directory if exact match fails
        # This handles potential naming variations if the user didn't specify the exact file
        directory = os.path.dirname(bin_path)
        if not directory: directory = "."

        candidates = [f for f in os.listdir(directory) if f.endswith(".bin") and "partitions" not in f and "bootloader" not in f]
        if len(candidates) == 1:
            bin_path = os.path.join(directory, candidates[0])
            print(f"Using binary: {bin_path}")
        else:
            print(f"Error: File not found: {bin_path}")
            return False

    size = os.path.getsize(bin_path)
    print(f"Checking firmware size for: {bin_path}")
    print(f"Binary size: {size} bytes")
    print(f"Max size:    {max_size} bytes")

    percentage = (size / max_size) * 100
    print(f"Usage:       {percentage:.2f}%")

    if size > max_size:
        print(f"ERROR: Firmware binary exceeds maximum allowed size by {size - max_size} bytes!")
        return False

    print("Check passed.")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 check_firmware_size.py <bin_path> <partitions_csv_path> [partition_name]")
        sys.exit(1)

    bin_path = sys.argv[1]
    partitions_csv_path = sys.argv[2]
    partition_name = sys.argv[3] if len(sys.argv) > 3 else "app0"

    max_size = parse_partition_size(partitions_csv_path, partition_name)
    if max_size is None:
        print(f"Error: Could not find size for partition '{partition_name}' in {partitions_csv_path}")
        sys.exit(1)

    if not check_size(bin_path, max_size):
        sys.exit(1)

    sys.exit(0)
