{ pkgs, src, arduino-nix, arduino-indexes, ... }:

let
  # Create overlays for the Arduino environment
  overlays = [
    (arduino-nix.overlay)
    (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_index.json"))
    (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_esp32_index.json"))
    (arduino-nix.mkArduinoLibraryOverlay (arduino-indexes + "/index/library_index.json"))
  ];

  # Helper to get the package set with the overlays applied
  pkgsWithArduino = import pkgs.path {
    inherit (pkgs) system;
    inherit overlays;
  };

  # Define the Arduino Environment for ESP32
  arduinoEnv = pkgsWithArduino.mkArduinoEnv {
    packages = with pkgsWithArduino.arduinoPackages; [
      platforms.esp32.esp32."2.0.14" # Stable version for S3
    ];
    libraries = [ ]; 
  };

  # Prepare source with config.h and correct directory name
  preparedSrc = pkgs.runCommand "nimrs-firmware-src" {} ''
    mkdir -p $out/NIMRS-Firmware
    cp -r ${src}/* $out/NIMRS-Firmware/
    chmod -R +w $out
    
    # Ensure config.h exists
    if [ ! -f $out/NIMRS-Firmware/config.h ]; then
      cp $out/NIMRS-Firmware/config.example.h $out/NIMRS-Firmware/config.h
    fi
  '';

in
  arduinoEnv.passthru.buildArduinoSketch {
    name = "nimrs-firmware";
    src = "${preparedSrc}/NIMRS-Firmware";
    fqbn = "esp32:esp32:esp32s3";
  }
