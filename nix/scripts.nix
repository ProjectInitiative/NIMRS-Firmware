{ pkgs }:

let
  mkFormattingTools =
    pkgs: with pkgs; [
      treefmt
      clang-tools
      prettier
      nixfmt
      black
      shfmt
      git
    ];

  # Helper Scripts
  nimrsLogs = pkgs.writeShellScriptBin "nimrs-logs" ''
    python3 tools/nimrs-logs.py "$@"
  '';

  nimrsTelemetry = pkgs.writeShellScriptBin "nimrs-telemetry" ''
    if [ ! -f "tools/nimrs-telemetry.py" ]; then
       echo "Error: tools/nimrs-telemetry.py not found."
       exit 1
    fi
    python3 tools/nimrs-telemetry.py "$@"
  '';

  ciReady = pkgs.writeShellScriptBin "ci-ready" ''
    set -e
    echo "1. Checking Git Status..."
    if [ -n "$(git status --porcelain)" ]; then
      echo "Error: Working directory is dirty."
      git status
      exit 1
    fi
    echo "2. Verifying Formatting & Tests..."
    nix flake check
    echo "3. Verifying Firmware Build..."
    nix build
    echo "All checks passed! Ready for CI."
  '';

  agentCheck = pkgs.writeShellScriptBin "agent-check" ''
    set -e
    echo "=== Running Agent Pre-Submission Check ==="
    echo "1. Merging with origin/main..."
    git fetch origin main
    CURRENT_BRANCH=$(git symbolic-ref --short HEAD)
    if [ "$CURRENT_BRANCH" != "main" ]; then
        git merge origin/main --no-edit
        echo "✔ Merge check passed."
    fi
    echo "2. Verifying CI Readiness..."
    ${ciReady}/bin/ci-ready
    echo "=== Agent Check Complete: READY FOR REVIEW ==="
  '';

  uploadFirmware = pkgs.writeShellScriptBin "upload-firmware" ''
    if [ -z "$1" ]; then
      echo "Usage: upload-firmware <PORT|IP> [app|all|monitor]"
      echo "  <PORT|IP>: Serial port (e.g. /dev/ttyACM0) OR IP Address (e.g. 192.168.1.100)"
      echo "  app      : Flash only the application (Serial only, preserves NVS/SPIFFS)"
      echo "  all      : Flash everything (Serial only, default)"
      echo "  monitor  : Flash and monitor (Serial only)"
      exit 1
    fi

    TARGET="$1"

    # Check if target is an IP address or hostname (not a serial port)
    if [[ ! "$TARGET" =~ ^(/dev/|COM) ]]; then
        echo "Uploading via OTA to $TARGET..."
        BIN_FILE="build/nimrs-firmware.bin"

        if [ ! -f "$BIN_FILE" ]; then
            echo "Error: Binary not found at $BIN_FILE. Run build-firmware first."
            exit 1
        fi

        curl --progress-bar -F "update=@$BIN_FILE" "http://$TARGET/update" | cat
        echo -e "\nDone."
    else
        # Serial Upload
        MODE="app-flash" # Default to app-flash for safety as requested

        if [ "$2" == "all" ]; then
            MODE="flash"
        elif [ "$2" == "monitor" ]; then
            MODE="app-flash monitor"
        fi
        
        # Ensure we have permissions to the serial port
        if [ -c "$TARGET" ]; then
            sudo chmod 666 "$TARGET" 2>/dev/null || true
        fi

        echo "Flashing to $TARGET with mode: $MODE"
        idf.py -p "$TARGET" $MODE
    fi
  '';

  flashAll = pkgs.writeShellScriptBin "flash-all" ''
    if [ -z "$1" ]; then
      echo "Usage: flash-all <PORT>"
      echo "  Flashes EVERYTHING: bootloader, partition table, and app."
      exit 1
    fi

    # Ensure we have permissions to the serial port
    if [ -c "$1" ]; then
        sudo chmod 666 "$1" 2>/dev/null || true
    fi

    echo "=== Flashing Full Firmware Stack to $1 ==="
    echo "1. Bootloader (0x0)"
    echo "2. Partition Table (0x8000)"
    echo "3. Application (0x10000)"

    idf.py -p "$1" flash
    echo "=== Flash Complete ==="
  '';

  monitorFirmware = pkgs.writeShellScriptBin "monitor-firmware" ''
    if [ -z "$1" ]; then
      echo "Usage: monitor-firmware <PORT|IP>"
      exit 1
    fi

    TARGET="$1"

    if [[ "$TARGET" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "Starting IP Log Monitor for $TARGET..."
        python3 tools/nimrs-logs.py "$TARGET"
    else
        echo "Starting Serial Monitor on $TARGET (115200)..."
        echo "Press Ctrl+C to exit."
        # Use miniterm (from pyserial) instead of idf.py monitor for a non-sticky experience
        python3 -m serial.tools.miniterm --raw "$TARGET" 115200
    fi
  '';

  resetOta = pkgs.writeShellScriptBin "reset-ota" ''
    if [ -z "$1" ]; then
      echo "Usage: reset-ota <PORT>"
      exit 1
    fi
    echo "Erasing OTA data partition (0xE000) to reset rollback state..."
    esptool.py -p "$1" erase_region 0xE000 0x2000
  '';

  eraseFlash = pkgs.writeShellScriptBin "erase-flash" ''
    if [ -z "$1" ]; then
      echo "Usage: erase-flash <PORT>"
      exit 1
    fi
    echo "=== ERASING ENTIRE FLASH (Factory Reset) ==="
    esptool.py -p "$1" erase_flash
  '';

  flashFactory = pkgs.writeShellScriptBin "flash-factory" ''
    if [ -z "$1" ]; then
      echo "Usage: flash-factory <PORT>"
      exit 1
    fi
    ${eraseFlash}/bin/erase-flash "$1"
    ${flashAll}/bin/flash-all "$1"
  '';

  generateApiDocs = pkgs.writeShellScriptBin "generate-api-docs" ''
    python3 tools/generate_api_docs.py
  '';

in
{
  inherit
    nimrsLogs
    nimrsTelemetry
    ciReady
    agentCheck
    uploadFirmware
    flashAll
    flashFactory
    eraseFlash
    monitorFirmware
    resetOta
    generateApiDocs
    mkFormattingTools
    ;
}
