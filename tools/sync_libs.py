#!/usr/bin/env python3
"""
Syncs library dependencies from common-libs.nix to platformio.ini.
Uses ARDUINO_LIBRARY_INDEX to resolve library repositories.
"""

import re
import sys
import os
import json


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


def load_library_index():
    index_path = os.environ.get("ARDUINO_LIBRARY_INDEX")
    if not index_path:
        print("Warning: ARDUINO_LIBRARY_INDEX not set. Cannot lookup repository URLs.")
        return None

    if not os.path.exists(index_path):
        print(f"Warning: Library index not found at {index_path}")
        return None

    print(f"Loading library index from {index_path}...")
    try:
        with open(index_path, "r") as f:
            return json.load(f)
    except Exception as e:
        print(f"Error loading index: {e}")
        return None


def get_repo_name(url):
    if not url:
        return None
    # Match github.com/Owner/Repo
    # Handle .git suffix
    match = re.search(r"github\.com/([^/]+)/([^/]+?)(?:\.git)?$", url)
    if match:
        return f"{match.group(1)}/{match.group(2)}"
    return None


def resolve_lib_name(lib, index_data):
    name = lib["name"]
    version = lib["version"]

    if not index_data:
        return name

    libraries = index_data.get("libraries", [])

    # Find matching library
    # We look for exact name and version match
    entry = next(
        (l for l in libraries if l.get("name") == name and l.get("version") == version),
        None,
    )

    if not entry:
        print(f"Warning: {name} v{version} not found in index.")
        return name

    # Try to extract repository URL
    repo = entry.get("repository", "")
    website = entry.get("website", "")
    url = entry.get("url", "")

    repo_name = get_repo_name(repo)
    if not repo_name:
        repo_name = get_repo_name(website)
    if not repo_name:
        repo_name = get_repo_name(url)

    if repo_name:
        return repo_name

    print(f"Warning: Could not determine GitHub repo for {name}. Using name.")
    return name


def update_platformio_ini(ini_file, libs, index_data):
    with open(ini_file, "r") as f:
        lines = f.readlines()

    new_lines = []
    in_lib_deps = False

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("lib_deps"):
            in_lib_deps = True
            new_lines.append("lib_deps =\n")
            # Add synced libs
            for lib in libs:
                resolved_name = resolve_lib_name(lib, index_data)
                version = lib["version"]
                new_lines.append(f"    {resolved_name} @ ^{version}\n")
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

    index_data = load_library_index()

    print(f"Found {len(libs)} libraries:")
    for lib in libs:
        print(f"  - {lib['name']} ({lib['version']})")

    print(f"Updating {ini_file}...")
    update_platformio_ini(ini_file, libs, index_data)
    print("Done.")


if __name__ == "__main__":
    main()
