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
  };

  outputs = { self, nixpkgs, arduino-indexes, arduino-nix, ... }@inputs:
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
          inherit pkgs arduino-nix arduino-indexes;
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

          ${import ./build-command.nix { outputDir = "build"; }}
        '';

        # Script to upload firmware
        uploadFirmware = pkgs.writeShellScriptBin "upload-firmware" ''
          if [ -z "$1" ]; then
            echo "Usage: upload-firmware <port>"
            echo "Example: upload-firmware /dev/ttyUSB0"
            exit 1
          fi

          echo "Uploading firmware to $1..."
          
          # Fix permissions on the port (requires sudo)
          sudo chmod 666 "$1"
          
          arduino-cli upload -p "$1" \
            --fqbn esp32:esp32:esp32s3 \
            --board-options "FlashSize=8M" \
            --board-options "PartitionScheme=default_8MB" \
            --board-options "UploadSpeed=115200" \
            --input-dir build \
            .
        '';

        # Script to monitor firmware
        monitorFirmware = pkgs.writeShellScriptBin "monitor-firmware" ''
          if [ -z "$1" ]; then
            echo "Usage: monitor-firmware <port>"
            echo "Example: monitor-firmware /dev/ttyUSB0"
            exit 1
          fi

          echo "Starting Serial Monitor on $1..."
          echo "Note: DTR/RTS are disabled to prevent resetting the board."
          
          # Fix permissions on the port (requires sudo)
          sudo chmod 666 "$1"
          
          arduino-cli monitor -p "$1" --config baudrate=115200,dtr=off,rts=off
        '';

      in
      {
        default = pkgs.mkShell {
          packages = with pkgs; [
            # Core Arduino tools (Wrapped)
            arduino-cli-wrapped
            # Tools for ESP32 often needed
            python3
            esptool
            
            # Helper scripts
            buildFirmware
            uploadFirmware
            monitorFirmware
          ];

          shellHook = ''
            echo "NIMRS-Firmware Development Environment (ESP32-S3)"
            echo "------------------------------------------------"
            echo "Arduino CLI is wrapped with libraries and platform."
            echo ""
            
            # We do NOT set ARDUINO_DIRECTORIES_... here to avoid breaking the wrapper.
            # But we might need a user directory for temp files? 
            # The wrapper typically handles the read-only parts.
            
            echo "Commands available:"
            echo "  build-firmware         : Build the firmware from current directory"
            echo "  upload-firmware <port> : Upload the firmware (e.g. /dev/ttyACM0)"
            echo "  monitor-firmware <port>: Monitor serial output (prevents reset loop)"
            echo "  nix build              : Clean build of the firmware"
          '';
        };
      });
  };
}
