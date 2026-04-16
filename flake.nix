{
  description = "NIMRS-Firmware Development Environment (ESP-IDF Native)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";

    # Arduino Indexes for arduino-nix
    arduino-indexes = {
      url = "github:bouk/arduino-indexes";
      flake = false;
    };

    # Arduino Nix with Env patch
    arduino-nix = {
      url = "github:clerie/arduino-nix/clerie/arduino-env";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      esp-dev,
      arduino-indexes,
      arduino-nix,
    }:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" ] (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        # LameJS source for embedding
        lamejs = pkgs.fetchurl {
          url = "https://raw.githubusercontent.com/zhuker/lamejs/master/lame.min.js";
          sha256 = "1x3dxi4c7h9dv8akhb58h1s4y1yc0z7fd3d633yxnfvvb3i8blhm";
        };

        # ---------------------------------------------------------
        # Arduino Library Handling (Ported from arduino-nix context)
        # ---------------------------------------------------------

        # Setup overlays for arduino-nix
        overlays = [
          (arduino-nix.overlay)
          (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_index.json"))
          (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_esp32_index.json"))
          (arduino-nix.mkArduinoLibraryOverlay (arduino-indexes + "/index/library_index.json"))
        ];

        # Create pkgs with Arduino overlays
        pkgsWithArduino = import pkgs.path { inherit system overlays; };

        # Extract git hash for versioning
        gitHash = self.rev or self.dirtyRev or "unknown";

        # Get libraries from nix/common-libs.nix
        arduinoLibs = import ./nix/common-libs.nix { inherit pkgsWithArduino pkgs; };

        # Arduino components derivation
        arduinoComponents = pkgs.callPackage ./nix/arduino-components.nix {
          inherit arduinoLibs;
        };

        # The dependency derivation (vendored managed components)
        nimrsDeps = pkgs.callPackage ./nix/dependencies.nix {
          esp-idf = esp-dev.packages.${system}.esp-idf-full;
        };

        # Import scripts
        scripts = pkgs.callPackage ./nix/scripts.nix {
          inherit
            pkgs
            arduinoLibs
            nimrsDeps
            lamejs
            ;
        };

        inherit (scripts)
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
      in
      {
        packages = {
          dependencies = nimrsDeps;
          arduino-components = arduinoComponents;

          # Host-side unit tests
          tests = pkgs.stdenv.mkDerivation {
            name = "nimrs-tests";
            src = ./.;
            nativeBuildInputs = [
              pkgs.gcc
              pkgs.python3
            ];
            buildPhase = ''
              # Fix path for tests which expect src/ at root
              ln -s main/src src
              # Ensure main/config.h exists for tests
              if [ -f config.example.h ]; then
                  cp config.example.h main/config.h
              elif [ -f main/config.example.h ]; then
                  cp main/config.example.h main/config.h
              fi

              # Generate LameJs.h for tests
              if [ ! -f "${lamejs}" ]; then
                  echo "Error: lamejs source not found at ${lamejs}"
                  exit 1
              fi
              mkdir -p src
              python3 tools/generate_lamejs_header.py "${lamejs}" "src/LameJs.h"

              python3 tools/test_runner.py
            '';
            installPhase = ''
              mkdir -p $out
              if [ -d tests/bin ]; then
                find tests/bin -maxdepth 1 -type f -executable -exec cp {} $out/ \;
              fi
            '';
          };

          default = pkgs.stdenv.mkDerivation {
            pname = "nimrs-firmware";
            version = "0.1.0";
            src = ./.;
            nativeBuildInputs = [
              esp-dev.packages.${system}.esp-idf-full
              setupProject
            ];
            IDF_TARGET = "esp32s3";

            # Export paths for CMake and setup-project
            LAMEJS_PATH = "${lamejs}";
            ARDUINO_COMPONENTS_PATH = "${arduinoComponents}";
            MANAGED_COMPONENTS_PATH = "${nimrsDeps}/managed_components";
            NIMRS_DEPS_PATH = "${nimrsDeps}";
            GIT_HASH = "${gitHash}";

            configurePhase = ''
              export HOME=$TMPDIR
              setup-project
            '';
            buildPhase = ''
              export IDF_COMPONENT_MANAGER=1
              export IDF_COMPONENT_MANAGER_OFFLINE=1
              idf.py build
              echo "=== Checking Firmware Size ==="
              python3 tools/check_firmware_size.py build/nimrs-firmware.bin partitions.csv app0
            '';
            installPhase = ''
              mkdir -p $out
              cp build/nimrs-firmware.bin $out/
              cp build/bootloader/bootloader.bin $out/
              cp build/partition_table/partition-table.bin $out/
              cp build/nimrs-firmware.elf $out/
            '';
          };
        };

        checks = {
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
                nativeBuildInputs = [ pkgs.python3 ] ++ mkFormattingTools pkgs;
                src = ./.;
              }
              ''
                cp -r $src/. .
                chmod -R +w .
                export XDG_CACHE_HOME=$TMPDIR
                # Fix path for generate_api_docs.py which expects src/
                ln -s main/src src
                python3 tools/generate_api_docs.py
                treefmt docs/API.md
                diff -u $src/docs/API.md docs/API.md
                touch $out
              '';

          tests = self.packages.${system}.tests;
        };

        devShells.default = esp-dev.devShells.${system}.esp-idf-full.overrideAttrs (old: {
          buildInputs =
            old.buildInputs
            ++ [
              setupProject
              buildFirmware
              motorSim
            ]
            ++ (mkFormattingTools pkgs)
            ++ [
              nimrsLogs
              nimrsTelemetry
              ciReady
              agentCheck
              uploadFirmware
              monitorFirmware
              flashAll
              flashFactory
              eraseFlash
              resetOta
              generateApiDocs
              pkgs.python3
              pkgs.esptool
            ];

          # Export paths for CMake and setup-project
          LAMEJS_PATH = "${lamejs}";
          ARDUINO_COMPONENTS_PATH = "${arduinoComponents}";
          MANAGED_COMPONENTS_PATH = "${nimrsDeps}/managed_components";
          NIMRS_DEPS_PATH = "${nimrsDeps}";
          GIT_HASH = "${gitHash}";

          shellHook = ''
                        ${old.shellHook or ""}
                        echo "NIMRS-Firmware Development Environment (ESP-IDF Native)"
                        echo "-------------------------------------------------------"
                        echo "This environment provides a full ESP-IDF toolchain managed by Nix."
                        echo ""

                        # Setup pre-commit hook
                        HOOK_FILE=".git/hooks/pre-commit"
                        if [ -d .git ] && [ ! -f "$HOOK_FILE" ]; then
                          echo "Installing treefmt pre-commit hook..."
                          cat > "$HOOK_FILE" <<EOF
            #!/bin/sh
            if ! command -v treefmt >/dev/null 2>&1; then
              echo "treefmt not found. Are you in the nix dev shell?"
              exit 1
            fi
            echo "Running treefmt (pre-commit)..."
            treefmt --fail-on-change
            EOF
                          chmod +x "$HOOK_FILE"
                        fi

                        echo "Commands available:"
                        echo "  build-firmware            : Build the firmware from current directory (wrapper for idf.py build)"
                        echo "  upload-firmware <PORT|IP> : Upload firmware via Serial (default: app only) or OTA (curl)"
                        echo "  monitor-firmware <PORT|IP>: Monitor logs via Serial (miniterm) or WiFi (nimrs-logs)"
                        echo "  flash-all <PORT>          : Flash EVERYTHING via Serial (bootloader + partition table + app)"
                        echo "  flash-factory <PORT>      : FULL CHIP ERASE then Flash everything (solves filesystem issues)"
                        echo "  erase-flash <PORT>        : Wipe the entire chip (Factory Reset)"
                        echo "  reset-ota <PORT>          : Erase OTA data to reset rollback state (enables testing of rollback)"
                        echo "  nimrs-telemetry <IP>      : Stream live motor debug data (WiFi)"
                        echo "  nimrs-logs <IP>           : Stream text logs (WiFi)"
                        echo "  ci-ready                  : Run formatting, tests, and build to verify CI readiness"
                        echo "  agent-check               : Run ci-ready + check for merge conflicts (REQUIRED for Agents)"
                        echo "  treefmt                   : Format all code (C++, JSON, MD)"
                        echo "  generate-api-docs         : Generate API documentation (docs/API.md)"
                        echo "  motor-sim                 : Run high-fidelity PID control loop simulation"
                        echo "  nix build                 : Clean build of the firmware (sandboxed)"
                        echo "  nix flake check           : Run all checks (formatting, tests, docs)"
          '';
        });
      }
    );
}
