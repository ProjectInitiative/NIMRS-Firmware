#!/usr/bin/env python3
"""
NIMRS Telemetry Tool
Connects to the NIMRS Decoder HTTP API and streams motor telemetry data in real-time.

Usage:
    ./tools/nimrs-telemetry.py <IP_ADDRESS>

Example:
    ./tools/nimrs-telemetry.py 192.168.1.100
"""

import sys
import time
import json
import urllib.request
import urllib.error
import socket

def get_logs(ip):
    url = f"http://{ip}/api/logs?type=data"
    try:
        with urllib.request.urlopen(url, timeout=1) as response:
            data = json.load(response)
            return data
    except (urllib.error.URLError, socket.timeout, ConnectionResetError) as e:
        # Don't spam errors, just return empty to retry
        return []
    except json.JSONDecodeError:
        return []

def draw_bar(label, value, max_val, width=10, color_code=""):
    """Draws a simple ASCII bar chart."""
    # Clamp value
    val_clamped = max(0, min(value, max_val))
    normalized = val_clamped / max_val if max_val > 0 else 0
    bar_len = int(normalized * width)
    
    # Characters
    bar_char = "█"
    empty_char = "░"
    
    bar = bar_char * bar_len
    padding = empty_char * (width - bar_len)
    
    # Format: LBL [BAR.......] 123.4
    return f"{label} {color_code}{bar}{padding}\033[0m {value:5.1f}"

def main():
    if len(sys.argv) < 2:
        print("Usage: ./tools/nimrs-telemetry.py <IP_ADDRESS>")
        sys.exit(1)

    ip = sys.argv[1]
    
    # ANSI escape sequences
    CLEAR_SCREEN = "\033[2J"
    CURSOR_HOME = "\033[H"
    HIDE_CURSOR = "\033[?25l"
    SHOW_CURSOR = "\033[?25h"
    
    # Color codes
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[96m"
    RESET = "\033[0m"
    
    print(f"{CLEAR_SCREEN}{CURSOR_HOME}{HIDE_CURSOR}", end="")
    print(f"{BLUE}NIMRS Telemetry Dashboard{RESET} - {ip}")
    print("-" * 65)
    print("\n" * 5) # Space for the dashboard
    print("-" * 65)
    print("Press Ctrl+C to exit.")

    last_processed_line = None

    try:
        while True:
            logs = get_logs(ip)
            
            # Find the *latest* telemetry line
            telemetry_line = None
            for line in reversed(logs):
                if "[NIMRS_DATA]" in line:
                    telemetry_line = line
                    break
            
            if telemetry_line and telemetry_line != last_processed_line:
                last_processed_line = telemetry_line
                try:
                    # Expected: [NIMRS_DATA],target,speed,pwm,avg_i,fast_i,peak
                    parts = telemetry_line.split("[NIMRS_DATA],")[1].split(",")
                    
                    target = int(parts[0])
                    speed = float(parts[1])
                    pwm = int(parts[2])
                    avg_i = float(parts[3])
                    fast_i = float(parts[4])
                    peak_adc = int(parts[5])

                    # Status Indicator
                    if avg_i > 1.2:
                        status = f"{RED}[WARN] {RESET}" 
                    else:
                        status = f"{GREEN}[OK]   {RESET}"

                    # Move cursor to line 4
                    sys.stdout.write("\033[4;1H")
                    
                    # Construct and print the dashboard
                    print(f"{draw_bar('TARGET', target, 28, 30, BLUE)}")
                    print(f"{draw_bar('SPEED ', speed, 28, 30, GREEN)}")
                    print(f"{draw_bar('POWER ', pwm, 1023, 30, YELLOW)}")
                    
                    print(f"{draw_bar('AMPS  ', avg_i, 2.0, 30, RED)} | FAST: {fast_i:.3f} | PEAK: {peak_adc:4d}")
                    print(f"                                      | {status}      ")
                    
                    sys.stdout.flush()

                except (IndexError, ValueError):
                    pass
            
            time.sleep(0.1) 

    except KeyboardInterrupt:
        print(f"{SHOW_CURSOR}\nExiting...")

if __name__ == "__main__":
    main()
