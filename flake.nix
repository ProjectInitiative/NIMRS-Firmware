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
    arduino-nix = { url = "github:clerie/arduino-nix/clerie/arduino-env"; };
  };

  outputs =
    { self, nixpkgs, flake-utils, esp-dev, arduino-indexes, arduino-nix }:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" ] (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        # ---------------------------------------------------------
        # Arduino Library Handling (Ported from arduino-nix context)
        # ---------------------------------------------------------

        # Setup overlays for arduino-nix
        overlays = [
          (arduino-nix.overlay)
          (arduino-nix.mkArduinoPackageOverlay
            (arduino-indexes + "/index/package_index.json"))
          (arduino-nix.mkArduinoPackageOverlay
            (arduino-indexes + "/index/package_esp32_index.json"))
          (arduino-nix.mkArduinoLibraryOverlay
            (arduino-indexes + "/index/library_index.json"))
        ];

        # Create pkgs with Arduino overlays
        pkgsWithArduino = import pkgs.path { inherit system overlays; };

        # Get libraries from common-libs.nix
        arduinoLibs = import ./common-libs.nix { inherit pkgsWithArduino; };

        # Formatting tools
        mkFormattingTools = pkgs:
          with pkgs; [
            treefmt
            clang-tools
            nodePackages.prettier
            nixfmt-classic
            black
            shfmt
            git
          ];

        # Create a derivation that symlinks these libraries into a structure suitable for IDF components
        setupComponents = pkgs.writeShellScriptBin "setup-components" ''
                    echo "Setting up Arduino libraries as IDF components..."
                    mkdir -p components

                    # Function to link a library
                    link_lib() {
                      LIB_PATH=$1
                      LIB_NAME_RAW=$(basename $LIB_PATH)
                      LIB_NAME=$(echo "$LIB_NAME_RAW" | sed -E 's/^[a-z0-9]{32}-//')
                      COMP_NAME=$(echo "$LIB_NAME" | tr '-' '_' | tr '.' '_')
                      TARGET_DIR="components/$COMP_NAME"

                      if [ -d "$TARGET_DIR" ]; then
                        echo "  Skipping $COMP_NAME (already exists in components/)"
                        return
                      fi

                      echo "  Copying $COMP_NAME from $LIB_PATH..."
                      cp -rL "$LIB_PATH" "$TARGET_DIR"
                      chmod -R u+w "$TARGET_DIR"

                      # Patch ESP8266Audio for IDF 5 compatibility
                      if [[ "$COMP_NAME" == "ESP8266Audio_1_9_7" || "$COMP_NAME" == "ESP8266Audio_1_9_9" ]]; then
                          echo "    Patching $COMP_NAME for IDF 5..."
                          find "$TARGET_DIR" -type f -name "*.cpp" -exec sed -i 's/I2S_MCLK_MULTIPLE_DEFAULT/I2S_MCLK_MULTIPLE_256/g' {} +
                          find "$TARGET_DIR" -type f -name "*.cpp" -exec sed -i 's/rtc_clk_apll_enable/\/\/rtc_clk_apll_enable/g' {} +
                      fi

                      # Create CMakeLists.txt for the Arduino library
                      echo "    Creating CMakeLists.txt for $COMP_NAME..."
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
                    }
                    ${
                      builtins.concatStringsSep "\n"
                      (map (lib: ''link_lib "${lib}"'') arduinoLibs)
                    }
                    echo "Done."
        '';

        # The dependency derivation (vendored components)
        nimrsDeps = pkgs.callPackage ./dependencies.nix {
          esp-idf = esp-dev.packages.${system}.esp-idf-esp32s3;
        };

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

        idfFlash = pkgs.writeShellScriptBin "flash" ''
          if [ -z "$1" ]; then
            echo "Usage: flash <PORT>"
            exit 1
          fi
          idf.py -p "$1" flash
        '';

        idfMonitor = pkgs.writeShellScriptBin "monitor" ''
          if [ -z "$1" ]; then
            echo "Usage: monitor <PORT>"
            exit 1
          fi
          idf.py -p "$1" monitor
        '';

        generateApiDocs = pkgs.writeShellScriptBin "generate-api-docs" ''
          python3 tools/generate_api_docs.py
        '';

      in {
        packages = {
          dependencies = nimrsDeps;

          # Host-side unit tests
          tests = pkgs.stdenv.mkDerivation {
            name = "nimrs-tests";
            src = ./.;
            nativeBuildInputs = [ pkgs.gcc pkgs.python3 ];
            buildPhase = ''
              # Fix path for tests which expect src/ at root
              ln -s main/src src
               # Ensure main/config.h exists for tests
               if [ -f config.example.h ]; then
                   cp config.example.h main/config.h
               elif [ -f main/config.example.h ]; then
                   cp main/config.example.h main/config.h
               fi
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
            nativeBuildInputs =
              [ esp-dev.packages.${system}.esp-idf-esp32s3 setupComponents ];
            IDF_TARGET = "esp32s3";
            configurePhase = ''
              export HOME=$TMPDIR
              setup-components
              # Ensure config.h exists for the build
              if [ ! -f main/config.h ]; then
                  echo "main/config.h not found, copying config.example.h..."
                  if [ -f config.example.h ]; then
                      cp config.example.h main/config.h
                  elif [ -f main/config.example.h ]; then
                      cp main/config.example.h main/config.h
                  else
                      echo "Warning: config.example.h not found!"
                  fi
              fi
              cp -r ${nimrsDeps}/managed_components .
              if [ -f ${nimrsDeps}/dependencies.lock ]; then
                cp ${nimrsDeps}/dependencies.lock .
              fi
              chmod -R u+w managed_components dependencies.lock || true
            '';
            buildPhase = ''
              idf.py build
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
          formatting = pkgs.runCommand "check-formatting" {
            nativeBuildInputs = mkFormattingTools pkgs;
            src = ./.;
          } ''
            cp -r $src/. .
            chmod -R +w .
            export XDG_CACHE_HOME=$TMPDIR
            treefmt --fail-on-change
            touch $out
          '';

          api-docs = pkgs.runCommand "check-api-docs" {
            nativeBuildInputs = [ pkgs.python3 ] ++ mkFormattingTools pkgs;
            src = ./.;
          } ''
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

        devShells.default =
          esp-dev.devShells.${system}.esp-idf-full.overrideAttrs (old: {
            buildInputs = old.buildInputs ++ [ setupComponents ]
              ++ (mkFormattingTools pkgs) ++ [
                nimrsLogs
                nimrsTelemetry
                ciReady
                agentCheck
                idfFlash
                idfMonitor
                generateApiDocs
                pkgs.python3
                pkgs.esptool
              ];
            shellHook = ''
                          ${old.shellHook or ""}
                          echo "NIMRS-Firmware ESP-IDF Environment"
                          echo "Run 'setup-components' to populate components/ with Arduino libraries."

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
            '';
          });
      });
}
