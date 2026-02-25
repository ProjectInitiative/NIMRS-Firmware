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

    # Check if target is an IP address
    if [[ "$TARGET" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
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

        echo "Flashing to $TARGET with mode: $MODE"
        idf.py -p "$TARGET" $MODE
    fi
  '';

  flashAll = pkgs.writeShellScriptBin "flash-all" ''
    if [ -z "$1" ]; then
      echo "Usage: flash-all <PORT>"
      echo "  Flashes bootloader, partition table, and app."
      exit 1
    fi
    echo "Flashing everything to $1..."
    idf.py -p "$1" flash
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
        echo "Starting Serial Monitor on $TARGET..."
        idf.py -p "$TARGET" monitor
    fi
  '';

  generateApiDocs = pkgs.writeShellScriptBin "generate-api-docs" ''
    python3 tools/generate_api_docs.py
  '';

  setupProject = pkgs.writeShellScriptBin "setup-project" ''
        set -e
        echo "=== Setting up NIMRS-Firmware Project Environment ==="

        # 0. Generate LameJs.h
        echo "-> Generating LameJs.h..."
        if [ ! -f "${lamejs}" ]; then
            echo "Error: lamejs source not found at ${lamejs}"
            exit 1
        fi
        mkdir -p main/src
        python3 tools/generate_lamejs_header.py "${lamejs}" "main/src/LameJs.h"

        # 1. Setup Arduino Components (components/LibName)
        echo "-> Configuring Arduino libraries..."
        mkdir -p components

        link_lib() {
          LIB_PATH=$1
          LIB_NAME_RAW=$(basename $LIB_PATH)
          LIB_NAME=$(echo "$LIB_NAME_RAW" | sed -E 's/^[a-z0-9]{32}-//')

          # Strip version for generic name (e.g., ArduinoJson-7.3.0 -> ArduinoJson)
          # Assume version starts with a digit after a dash or dot
          GENERIC_NAME=$(echo "$LIB_NAME" | sed -E 's/[-.][0-9].*$//')

          COMP_NAME=$(echo "$GENERIC_NAME" | tr '-' '_' | tr '.' '_')
          TARGET_DIR="components/$COMP_NAME"

          if [ -d "$TARGET_DIR" ]; then
            :
          else
             echo "   Linking $COMP_NAME from $LIB_PATH..."
             cp -rL "$LIB_PATH" "$TARGET_DIR"
             chmod -R u+w "$TARGET_DIR"

             # Patch ESP8266Audio for IDF 5 compatibility
             if [[ "$COMP_NAME" == "ESP8266Audio" ]]; then
                 echo "   Patching $COMP_NAME for IDF 5..."
                 find "$TARGET_DIR" -type f -name "*.cpp" -exec sed -i 's/I2S_MCLK_MULTIPLE_DEFAULT/I2S_MCLK_MULTIPLE_256/g' {} +
                 find "$TARGET_DIR" -type f -name "*.cpp" -exec sed -i 's/rtc_clk_apll_enable/\/\/rtc_clk_apll_enable/g' {} +
             fi

             # Create CMakeLists.txt
             REAL_LIB_ROOT="."
             if [ -d "$TARGET_DIR/libraries" ]; then
                 NESTED=$(ls "$TARGET_DIR/libraries" | head -n 1)
                 if [ -n "$NESTED" ]; then
                     REAL_LIB_ROOT="libraries/$NESTED"
                 fi
             fi

             INCDIRS="\"$REAL_LIB_ROOT\""
             if [ -d "$TARGET_DIR/$REAL_LIB_ROOT/src" ]; then
                 INCDIRS="\"$REAL_LIB_ROOT/src\" $INCDIRS"
                 GLOB_PATTERN="GLOB_RECURSE SOURCES \"$REAL_LIB_ROOT/src/*.cpp\" \"$REAL_LIB_ROOT/src/*.c\" \"$REAL_LIB_ROOT/src/*.S\""
             else
                 GLOB_PATTERN="GLOB SOURCES \"$REAL_LIB_ROOT/*.cpp\" \"$REAL_LIB_ROOT/*.c\" \"$REAL_LIB_ROOT/*.S\""
             fi

             # Write CMakeLists.txt
             cat > "$TARGET_DIR/CMakeLists.txt" <<EOF
    file($GLOB_PATTERN)
    if("\''${SOURCES}" STREQUAL "")
        idf_component_register(INCLUDE_DIRS $INCDIRS REQUIRES arduino-esp32)
    else()
        idf_component_register(SRCS \''${SOURCES} INCLUDE_DIRS $INCDIRS REQUIRES arduino-esp32)
        target_compile_options(\''${COMPONENT_LIB} PRIVATE
            \$<\$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
            \$<\$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
            -Wno-error=stringop-overflow
            -Wno-error=narrowing
        )
    endif()
    EOF
          fi
        }
        ${builtins.concatStringsSep "\n" (map (lib: ''link_lib "${lib}"'') arduinoLibs)}

        # 2. Ensure config.h exists
        echo "-> Checking config.h..."
        if [ ! -f main/config.h ]; then
            if [ -f config.example.h ]; then
                cp config.example.h main/config.h
            elif [ -f main/config.example.h ]; then
                cp main/config.example.h main/config.h
            fi
        fi

        # 3. Configure Managed Components (IDF Registry) via Nix
        NIMRS_DEPS_PATH="${nimrsDeps}"
        MANAGED_PATH="$NIMRS_DEPS_PATH/managed_components"
        echo "-> Configuring managed dependencies from $MANAGED_PATH..."

        # Generate dependencies.cmake to point EXTRA_COMPONENT_DIRS to Nix store
        cat > dependencies.cmake <<EOF
    # Generated by setup-project (Nix)
    # Points to immutable dependencies in Nix store
    set(EXTRA_COMPONENT_DIRS "$MANAGED_PATH")
    EOF
        echo "   Generated dependencies.cmake"

        # Copy dependencies.lock to satisfy IDF checks
        if [ -f "$NIMRS_DEPS_PATH/dependencies.lock" ]; then
            cp -f "$NIMRS_DEPS_PATH/dependencies.lock" dependencies.lock
            chmod u+w dependencies.lock
            echo "   Synced dependencies.lock"
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
  '';

in
{
  inherit
    setupProject
    buildFirmware
    nimrsLogs
    nimrsTelemetry
    ciReady
    agentCheck
    uploadFirmware
    flashAll
    monitorFirmware
    generateApiDocs
    mkFormattingTools
    ;
}
