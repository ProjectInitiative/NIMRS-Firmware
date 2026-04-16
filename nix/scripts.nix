{
  pkgs,
  arduinoLibs,
  nimrsDeps,
  lamejs,
}:

let
  mkFormattingTools =
    pkgs: with pkgs; [
      treefmt
      clang-tools
      nodePackages.prettier
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

  setupProject = pkgs.writeShellScriptBin "setup-project" ''
    set -e
    echo "=== Setting up NIMRS-Firmware Project Environment ==="

    # 0. Generate LameJs.h
    echo "-> Generating LameJs.h..."
    if [ -n "$LAMEJS_PATH" ] && [ -f "$LAMEJS_PATH" ]; then
        mkdir -p main/src
        python3 tools/generate_lamejs_header.py "$LAMEJS_PATH" "main/src/LameJs.h"
    else
        echo "Warning: LAMEJS_PATH not set or file not found. Skipping LameJs.h generation."
    fi

    # 1. Setup Component Symlinks
    echo "-> Linking components from Nix store..."

    # Link Arduino components
    if [ -n "$ARDUINO_COMPONENTS_PATH" ]; then
        if [ -L components ]; then rm components; fi
        if [ -d components ]; then echo "Warning: components/ is a real directory, skipping link"; else
            ln -sf "$ARDUINO_COMPONENTS_PATH" components
            echo "   Linked components -> $ARDUINO_COMPONENTS_PATH"
        fi
    fi

    # Link Managed components
    if [ -n "$NIMRS_DEPS_PATH" ]; then
        if [ -L managed_components ]; then rm managed_components; fi
        if [ -d managed_components ]; then echo "Warning: managed_components/ is a real directory, skipping link"; else
            ln -sf "$NIMRS_DEPS_PATH/managed_components" managed_components
            echo "   Linked managed_components -> $NIMRS_DEPS_PATH/managed_components"
        fi
        
        # Sync dependencies.lock
        if [ -f "$NIMRS_DEPS_PATH/dependencies.lock" ]; then
            cp -f "$NIMRS_DEPS_PATH/dependencies.lock" dependencies.lock
            chmod u+w dependencies.lock
            echo "   Synced dependencies.lock from Nix store"
        fi
    fi

    # 2. Ensure config.h exists
    echo "-> Checking config.h..."
    if [ ! -f main/config.h ]; then
        if [ -f config.example.h ]; then
            cp config.example.h main/config.h
        elif [ -f main/config.example.h ]; then
            cp main/config.example.h main/config.h
        fi
    fi

    echo "=== Setup Complete ==="
  '';

  # Build Firmware Script (Local Wrapper)
  buildFirmware = pkgs.writeShellScriptBin "build-firmware" ''
    set -e
    # Run setup first
    ${setupProject}/bin/setup-project

    echo "=== Building Firmware (IDF) ==="
    idf.py build

    echo "=== Checking Firmware Size ==="
    python3 tools/check_firmware_size.py build/nimrs-firmware.bin partitions.csv app0
  '';

  motorSim = pkgs.writeShellScriptBin "motor-sim" ''
    set -e
    echo "=== Compiling PID Simulator Harness ==="
    g++ -o build/motor-sim \
        -I main/src \
        -I tests/mocks \
        -I tests/simulator \
        -DUNIT_TEST \
        -DSKIP_MOCK_MOTOR_CONTROLLER \
        tests/test_PID_Simulator.cpp \
        tests/simulator/MotorSimulator.cpp \
        tests/mocks/SimulatedMotorHal.cpp \
        tests/mocks/mocks.cpp \
        main/src/MotorTask.cpp \
        main/src/BemfEstimator.cpp \
        main/src/DspFilters.cpp \
        main/src/RippleDetector.cpp \
        main/src/Logger.cpp

    echo "=== Running Simulation ==="
    ./build/motor-sim | tee build/sim_results.txt

    if [ -f "tools/plot_sim_results.py" ]; then
        echo "=== Plotting Results ==="
        python3 tools/plot_sim_results.py build/sim_results.txt
    fi
  '';

in
{
  inherit
    setupProject
    buildFirmware
    motorSim
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
