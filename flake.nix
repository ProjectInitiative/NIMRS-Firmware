{
  description = "NIMRS-21Pin-Decoder Firmware Development Environment (ESP32-S3)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # Arduino Indexes for arduino-nix
    arduino-indexes = {
      url = "github:bouk/arduino-indexes";
      flake = false;
    };

    # Arduino Nix with Env patch
    arduino-nix = {
      url = "github:clerie/arduino-nix/clerie/arduino-env";
    };

    # Lame.js for client-side MP3 compression
    lamejs = {
      url = "https://raw.githubusercontent.com/zhuker/lamejs/master/lame.min.js";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      arduino-indexes,
      arduino-nix,
      lamejs,
      ...
    }@inputs:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      # Get git hash from flake input
      gitHash = self.shortRev or "dirty";

      # Shared formatting tools for CI and devShell
      mkFormattingTools =
        pkgs: with pkgs; [
          treefmt
          clang-tools # clang-format
          nodePackages.prettier # prettier
          nixfmt
          black
          shfmt
          git
        ];
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          # Clean build method using arduino-nix-env
          default = import ./build-with-env.nix {
            inherit pkgs arduino-nix arduino-indexes;
            inherit gitHash lamejs;
            src = ./.;
          };

          # Host-side unit tests
          tests = pkgs.stdenv.mkDerivation {
            name = "nimrs-tests";
            src = ./.;
            nativeBuildInputs = [
              pkgs.gcc
              pkgs.python3
            ];
            buildPhase = import ./test-command.nix { };
            installPhase = ''
              mkdir -p $out
              # Copy all test executables to output from tests/bin
              if [ -d tests/bin ]; then
                find tests/bin -maxdepth 1 -type f -executable -exec cp {} $out/ \;
              fi
            '';
          };
        }
      );

      # Checks (Formatting & Tests)
      checks = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          formatting =
            pkgs.runCommand "check-formatting"
              {
                nativeBuildInputs = mkFormattingTools pkgs;
                src = ./.;
              }
              ''
                cp -r $src/. .
                chmod -R +w .
                export XDG_CACHE_HOME=$TMPDIR
                treefmt --fail-on-change
                touch $out
              '';

          api-docs =
            pkgs.runCommand "check-api-docs"
              {
                nativeBuildInputs = [ pkgs.python3 ];
                src = ./.;
              }
              ''
                cp -r $src/. .
                chmod -R +w .
                python3 tools/generate_api_docs.py
                diff -u $src/docs/API.md docs/API.md
                touch $out
              '';

          tests = self.packages.${system}.tests;
        }
      );

      # Development shells
      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};

          # Helper to get the package set with overlays
          pkgsWithArduino = import nixpkgs {
            system = pkgs.stdenv.hostPlatform.system;
            overlays = [
              arduino-nix.overlay
              (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_index.json"))
              (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_esp32_index.json"))
              (arduino-nix.mkArduinoLibraryOverlay (arduino-indexes + "/index/library_index.json"))
            ];
          };

          # Libraries to link for local dev
          buildLibs = import ./common-libs.nix { inherit pkgsWithArduino; };

          # Wrapped arduino-cli with ESP32 platform and libraries
          arduino-cli-wrapped = pkgsWithArduino.wrapArduinoCLI {
            packages = with pkgsWithArduino.arduinoPackages; [
              platforms.esp32.esp32."2.0.14"
            ];
            libraries = buildLibs;
          };

          # Script to build firmware manually
          buildFirmware = pkgs.writeShellScriptBin "build-firmware" ''
            echo "Building firmware from current directory..."

            # Ensure config.h exists
            if [ ! -f "config.h" ]; then
               cp config.example.h config.h
            fi

            ${import ./build-command.nix {
              outputDir = "build";
              inherit lamejs;
            }}
          '';

          # Script to upload firmware
          uploadFirmware = pkgs.writeShellScriptBin "upload-firmware" ''
            if [ -z "$1" ]; then
              echo "Usage: upload-firmware <port|IP>"
              echo "Example Serial: upload-firmware /dev/ttyUSB0"
              echo "Example OTA:    upload-firmware 192.168.1.100"
              exit 1
            fi

            TARGET="$1"

            if [[ "$TARGET" == /dev/* ]]; then
                echo "Uploading via Serial to $TARGET..."
                
                # Fix permissions on the port (requires sudo)
                sudo chmod 666 "$TARGET"
                
                arduino-cli upload -p "$TARGET" \
                  --fqbn esp32:esp32:esp32s3 \
                  --board-options "FlashSize=8M" \
                  --board-options "PartitionScheme=default_8MB" \
                  --board-options "UploadSpeed=115200" \
                  --input-dir build \
                  .
            else
                echo "Uploading via OTA to $TARGET..."
                BIN_FILE="build/NIMRS-Firmware.ino.bin"
                
                if [ ! -f "$BIN_FILE" ]; then
                    echo "Error: Binary not found at $BIN_FILE. Run build-firmware first."
                    exit 1
                fi

                curl --progress-bar -F "update=@$BIN_FILE" "http://$TARGET/update" | cat
                echo -e "\nDone."
            fi
          '';

          # Script to run logs
          nimrsLogs = pkgs.writeShellScriptBin "nimrs-logs" ''
            if [ -z "$1" ]; then
              echo "Usage: nimrs-logs <IP_ADDRESS>"
              exit 1
            fi
            python3 tools/nimrs-logs.py "$@"
          '';

          # Script to monitor firmware (Serial or IP)
          monitorFirmware = pkgs.writeShellScriptBin "monitor-firmware" ''
            if [ -z "$1" ]; then
              echo "Usage: monitor-firmware <port|IP>"
              echo "Example Serial: monitor-firmware /dev/ttyUSB0"
              echo "Example IP:     monitor-firmware 192.168.21.166"
              exit 1
            fi

            TARGET="$1"

            if [[ "$TARGET" == /dev/* ]]; then
                echo "Starting Serial Monitor on $TARGET..."
                echo "Note: DTR/RTS are disabled to prevent resetting the board."
                # Fix permissions on the port (requires sudo)
                sudo chmod 666 "$TARGET"
                arduino-cli monitor -p "$TARGET" --config baudrate=115200,dtr=off,rts=off
            else
                echo "Starting IP Log Monitor for $TARGET..."
                # Delegate to our log streamer
                ${nimrsLogs}/bin/nimrs-logs "$TARGET"
            fi
          '';

          # Script to run telemetry
          nimrsTelemetry = pkgs.writeShellScriptBin "nimrs-telemetry" ''
            if [ -z "$1" ]; then
              echo "Usage: nimrs-telemetry <IP_ADDRESS>"
              exit 1
            fi

            if [ ! -f "tools/nimrs-telemetry.py" ]; then
               echo "Error: tools/nimrs-telemetry.py not found. Are you in the project root?"
               exit 1
            fi

            # Uses the python environment provided by the dev shell
            python3 tools/nimrs-telemetry.py "$@"
          '';

          # Script to run unit tests
          runTests = pkgs.writeShellScriptBin "run-tests" ''
            echo "Running NIMRS Firmware Unit Tests..."
            # Clean up old tests if any
            rm -rf tests/bin
            ${import ./test-command.nix { }}
          '';

          # Script to verify CI readiness
          ciReady = pkgs.writeShellScriptBin "ci-ready" ''
            set -e # Exit on error

            echo "1. Checking Git Status..."
            if [ -n "$(git status --porcelain)" ]; then
              echo "Error: Working directory is dirty. Please commit all changes before running ci-ready."
              git status
              exit 1
            fi

            echo "2. Verifying Formatting & Tests..."
            nix flake check

            echo "3. Verifying Firmware Build..."
            nix build

            echo "--------------------------------"
            echo "All checks passed! Ready for CI."
          '';

          # Script tailored for Jules agents to verify readiness and conflicts
          agentCheck = pkgs.writeShellScriptBin "agent-check" ''
            set -e
            echo "=== Running Agent Pre-Submission Check ==="

            # 1. Check for Merge Conflicts / Update from main
            echo "--------------------------------"
            echo "1. Merging with origin/main..."

            # Ensure we have the latest remote refs
            git fetch origin main

            # Check if we are on a branch
            CURRENT_BRANCH=$(git symbolic-ref --short HEAD)
            if [ "$CURRENT_BRANCH" != "main" ]; then
                echo "Attempting to merge origin/main into $CURRENT_BRANCH..."
                # Perform a real merge. 
                # If successful, it creates a merge commit.
                # If it fails, it leaves conflict markers and exits due to 'set -e'.
                git merge origin/main --no-edit
                echo "✔ Merge check passed: Successfully merged origin/main."
            else
                echo "On main branch. Skipping conflict check."
            fi

            # 2. Run CI Readiness (Builds, Tests, Formatting check, Dirty check)
            # This now runs ON the merged result.
            echo "--------------------------------"
            echo "2. Verifying CI Readiness on merged result..."
            ${ciReady}/bin/ci-ready

            echo "--------------------------------"
            echo "=== Agent Check Complete: READY FOR REVIEW ==="
          '';

          # Script to analyze core dump
          analyzeCoredump = pkgs.writeShellScriptBin "analyze-coredump" ''
            # Ensure virtualenv exists
            VENV_DIR=".venv-coredump"
            if [ ! -d "$VENV_DIR" ]; then
                echo "Creating virtualenv for esp-coredump in $VENV_DIR..."
                python3 -m venv "$VENV_DIR"
                source "$VENV_DIR/bin/activate"
                echo "Installing esp-coredump..."
                pip install esp-coredump
            else
                source "$VENV_DIR/bin/activate"
            fi

            if [ $# -eq 0 ]; then
                echo "Usage: analyze-coredump <args>"
                echo "Example: analyze-coredump info_corefile -t b64 -c coredump.txt build/NIMRS-Firmware.ino.elf"
                echo "Running esp-coredump --help..."
                esp-coredump --help
                exit 0
            fi

            echo "Running esp-coredump $@"
            esp-coredump "$@"
          '';

          # Script to sync platformio.ini libs
          syncLibs = pkgs.writeShellScriptBin "sync-libs" ''
            export ARDUINO_LIBRARY_INDEX="${arduino-indexes}/index/library_index.json"
            python3 tools/sync_libs.py
          '';

          # Script to generate API documentation
          generateApiDocs = pkgs.writeShellScriptBin "generate-api-docs" ''
            python3 tools/generate_api_docs.py
          '';

        in
        {
          default = pkgs.mkShell {
            packages =
              with pkgs;
              [
                # Core Arduino tools (Wrapped)
                arduino-cli-wrapped
                # Tools for ESP32 often needed
                (python3.withPackages (ps: [ ps.pyserial ]))
                esptool
                # For OTA upload
                curl
              ]
              ++ mkFormattingTools pkgs
              ++ [
                # Helper scripts
                buildFirmware
                uploadFirmware
                monitorFirmware
                nimrsTelemetry
                nimrsLogs
                runTests
                ciReady
                agentCheck
                syncLibs
                generateApiDocs
                analyzeCoredump
              ];

            shellHook = ''
                            echo "NIMRS-Firmware Development Environment (ESP32-S3)"
                            echo "------------------------------------------------"
                            echo "Arduino CLI is wrapped with libraries and platform."
                            echo ""

                            # We do NOT set ARDUINO_DIRECTORIES_... here to avoid breaking the wrapper.
                            # But we might need a user directory for temp files? 
                            # The wrapper typically handles the read-only parts.

                            # Setup pre-commit hook
                            HOOK_FILE=".git/hooks/pre-commit"
                            if [ -d .git ] && [ ! -f "$HOOK_FILE" ]; then
                              echo "Installing treefmt pre-commit hook..."
                              cat > "$HOOK_FILE" <<EOF
              #!/bin/sh
              # Auto-generated by flake.nix shellHook
              # Ensure treefmt is available
              if ! command -v treefmt >/dev/null 2>&1; then
                  echo "treefmt not found. Are you in the nix dev shell?"
                  exit 1
              fi

              echo "Running treefmt (pre-commit)..."
              # Fail if changes are needed
              treefmt --fail-on-change
              EOF
                              chmod +x "$HOOK_FILE"
                            fi

                            echo "Commands available:"
                            echo "  build-firmware            : Build the firmware from current directory"
                            echo "  upload-firmware <port|IP> : Upload the firmware (e.g. /dev/ttyACM0 or IP)"
                            echo "  monitor-firmware <port|IP>: Monitor logs (Serial or WiFi)"
                            echo "  nimrs-telemetry <IP>      : Stream live motor debug data (WiFi)"
                            echo "  nimrs-logs <IP>           : Stream text logs (WiFi)"
                            echo "  run-tests                 : Run host-side unit tests"
                            echo "  ci-ready                  : Run formatting, tests, and build to verify CI readiness"
                            echo "  agent-check               : Run ci-ready + check for merge conflicts (REQUIRED for Agents)"
                            echo "  analyze-coredump <args>   : Analyze core dump (installs esp-coredump in venv)"
                            echo "  treefmt                   : Format all code (C++, JSON, MD)"
                            echo "  sync-libs                 : Sync libs from common-libs.nix to platformio.ini"
                            echo "  generate-api-docs         : Generate API documentation (docs/API.md)"
                            echo "  nix build                 : Clean build of the firmware"
                            echo "  nix build .#tests         : Build and run tests in a sandbox"
            '';
          };
        }
      );
    };
}
