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
      inputs.nixpkgs.follows = "nixpkgs";
    };

    # NmraDcc Library
    NmraDcc = {
      url = "github:mrrwa/NmraDcc";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, arduino-indexes, arduino-nix, NmraDcc, ... }@inputs:
  let
    supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
    forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
  in
  {
    packages = forAllSystems (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        # Clean build method using arduino-nix-env
        default = import ./build-with-env.nix {
          inherit pkgs arduino-nix arduino-indexes NmraDcc;
          src = ./.;
        };
      });

    # Development shells
    devShells = forAllSystems (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        
        # Helper to get the package set with overlays
        pkgsWithArduino = import nixpkgs {
          inherit system;
          overlays = [
            arduino-nix.overlay
            (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_index.json"))
            (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_esp32_index.json"))
            (arduino-nix.mkArduinoLibraryOverlay (arduino-indexes + "/index/library_index.json"))
          ];
        };

        # Wrapped arduino-cli with ESP32 platform
        arduino-cli-wrapped = pkgsWithArduino.wrapArduinoCLI {
          packages = with pkgsWithArduino.arduinoPackages; [
             platforms.esp32.esp32."2.0.14"
          ];
        };

        # Script to setup local source
        setupLocalSource = pkgs.writeShellScriptBin "setup-local-source" ''
          echo "Setting up local source in .direnv/src..."
          mkdir -p .direnv/src
          if [ -d ".direnv/src/NIMRS-Firmware" ]; then
            echo "Removing existing source..."
            rm -rf .direnv/src/NIMRS-Firmware
          fi
          
          # Copy current directory files to the work dir, excluding git and direnv
          echo "Copying source..."
          mkdir -p .direnv/src/NIMRS-Firmware
          rsync -av --exclude '.git' --exclude '.direnv' --exclude '.arduino-data' . .direnv/src/NIMRS-Firmware/
          chmod -R +w .direnv/src/NIMRS-Firmware
          
          # Create config.h
          if [ ! -f ".direnv/src/NIMRS-Firmware/config.h" ]; then
            echo "Creating config.h from example..."
            cp .direnv/src/NIMRS-Firmware/config.example.h .direnv/src/NIMRS-Firmware/config.h
          fi
          
          echo "Done. Source is in .direnv/src/NIMRS-Firmware"
        '';

        # Script to build firmware manually
        buildFirmware = pkgs.writeShellScriptBin "build-firmware" ''
          if [ ! -d ".direnv/src/NIMRS-Firmware" ]; then
            echo "Error: Local source not found. Run 'setup-local-source' first."
            exit 1
          fi
          
          echo "Building firmware from .direnv/src/NIMRS-Firmware..."
          cd .direnv/src/NIMRS-Firmware
          
          # Ensure config.h exists
          if [ ! -f "config.h" ]; then
             cp config.example.h config.h
          fi

          # The wrapped arduino-cli already knows where tools and core are.
          arduino-cli compile \
            --fqbn esp32:esp32:esp32s3 \
            --output-dir build \
            --warnings all \
            .
        '';

        # Script to upload firmware
        uploadFirmware = pkgs.writeShellScriptBin "upload-firmware" ''
          if [ -z "$1" ]; then
            echo "Usage: upload-firmware <port>"
            echo "Example: upload-firmware /dev/ttyACM0"
            exit 1
          fi

          if [ ! -d ".direnv/src/NIMRS-Firmware" ]; then
            echo "Error: Local source not found. Run 'setup-local-source' first."
            exit 1
          fi
          
          echo "Uploading firmware to $1..."
          cd .direnv/src/NIMRS-Firmware
          
          arduino-cli upload -p "$1" --fqbn esp32:esp32:esp32s3 .
        '';

      in
      {
        default = pkgs.mkShell {
          packages = with pkgs; [
            # Core Arduino tools
            arduino-cli-wrapped
            # Tools for ESP32 often needed
            python3
            esptool
            
            # Helper scripts
            setupLocalSource
            buildFirmware
            uploadFirmware
          ];

          shellHook = ''
            echo "NIMRS-Firmware Development Environment (ESP32-S3)"
            echo "------------------------------------------------"
            echo ""
            
            # Use the wrapped data directory
            export ARDUINO_DIRECTORIES_DATA=${arduino-cli-wrapped.dataPath}
            
            echo "Commands available:"
            echo "  setup-local-source     : Copy source to .direnv/src/NIMRS-Firmware for dev"
            echo "  build-firmware         : Build the firmware from .direnv/src/NIMRS-Firmware"
            echo "  upload-firmware <port> : Upload the firmware (e.g. /dev/ttyACM0)"
            echo "  nix build              : Clean build of the firmware"
          '';
        };
      });
  };
}
