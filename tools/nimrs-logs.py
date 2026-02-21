#!/usr/bin/env python3
"""
NIMRS Log Streamer
Connects to the NIMRS Decoder HTTP API and streams system logs in real-time.

Usage:
    ./tools/nimrs-logs.py <IP_ADDRESS>
"""

import sys
import time
import json
import urllib.request
import urllib.error
import socket


def get_logs(ip):
    # Fetch system logs (no filter = system buffer)
    url = f"http://{ip}/api/logs"
    try:
        with urllib.request.urlopen(url, timeout=2) as response:
            return json.load(response)
    except (urllib.error.URLError, socket.timeout, ConnectionResetError):
        return []
    except json.JSONDecodeError:
        return []


def main():
    if len(sys.argv) < 2:
        print("Usage: ./tools/nimrs-logs.py <IP_ADDRESS>")
        sys.exit(1)

    ip = sys.argv[1]
    print(f"--- Streaming logs from {ip} (Ctrl+C to stop) ---")

    last_ts = 0

    try:
        while True:
            lines = get_logs(ip)

            # Identify new lines based on timestamp [12345]
            new_lines = []
            current_max_ts = last_ts

            for line in lines:
                match = line.split("]", 1)
                if len(match) > 0 and match[0].startswith("["):
                    try:
                        ts = int(match[0][1:])
                        if ts > last_ts:
                            new_lines.append(line)
                            if ts > current_max_ts:
                                current_max_ts = ts
                    except ValueError:
                        # Fallback for lines without clear timestamps
                        new_lines.append(line)
                else:
                    new_lines.append(line)

            for line in new_lines:
                print(line)

            last_ts = current_max_ts
            time.sleep(0.5)  # Poll logs twice a second

    except KeyboardInterrupt:
        print("\nExiting...")


if __name__ == "__main__":
    main()
