#!/usr/bin/env python3
"""
Syncs library dependencies from common-libs.nix to platformio.ini.
"""

import re
import sys
import os


def parse_nix_libs(nix_file):
    libs = []
    with open(nix_file, "r") as f:
        content = f.read()

    # Matches: Name."Version"
    pattern = re.compile(r'(\w+)\."([\d\.]+)"')
    matches = pattern.findall(content)

    for name, version in matches:
        libs.append({"name": name, "version": version})

    return libs


def update_platformio_ini(ini_file, libs):
    with open(ini_file, "r") as f:
        lines = f.readlines()

    new_lines = []
    in_lib_deps = False

    # Define mapping inside function for now.
    # In a real scenario, this might be external or more robust.
    org_map = {
        "NmraDcc": "nmradcc/NmraDcc",
        "WiFiManager": "tzapu/WiFiManager",
        "ArduinoJson": "bblanchon/ArduinoJson",
        "ESP8266Audio": "earlephilhower/ESP8266Audio",
    }

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("lib_deps"):
            in_lib_deps = True
            new_lines.append("lib_deps =\n")
            # Add synced libs
            for lib in libs:
                name = lib["name"]
                pio_name = org_map.get(name, name)
                version = lib["version"]
                new_lines.append(f"    {pio_name} @ ^{version}\n")
            continue

        if in_lib_deps:
            # Skip lines until next section or empty line if it was indented
            if stripped == "" or stripped.startswith("["):
                in_lib_deps = False
                new_lines.append(line)
            # Else skip existing lib_deps lines
        else:
            new_lines.append(line)

    with open(ini_file, "w") as f:
        f.writelines(new_lines)


def main():
    # Use repo root relative to this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(script_dir)
    nix_file = os.path.join(repo_root, "common-libs.nix")
    ini_file = os.path.join(repo_root, "platformio.ini")

    if not os.path.exists(nix_file):
        print(f"Error: {nix_file} not found.")
        sys.exit(1)

    if not os.path.exists(ini_file):
        print(f"Error: {ini_file} not found.")
        sys.exit(1)

    print(f"Reading libraries from {nix_file}...")
    libs = parse_nix_libs(nix_file)

    print(f"Found {len(libs)} libraries:")
    for lib in libs:
        print(f"  - {lib['name']} ({lib['version']})")

    print(f"Updating {ini_file}...")
    update_platformio_ini(ini_file, libs)
    print("Done.")


if __name__ == "__main__":
    main()
