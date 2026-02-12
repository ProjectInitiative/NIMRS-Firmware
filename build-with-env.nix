{
  pkgs,
  src,
  arduino-nix,
  arduino-indexes,
  gitHash ? "unknown",
  lamejs ? null,
  ...
}:

let
  overlays = [
    (arduino-nix.overlay)
    (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_index.json"))
    (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_esp32_index.json"))
    (arduino-nix.mkArduinoLibraryOverlay (arduino-indexes + "/index/library_index.json"))
  ];

  pkgsWithArduino = import pkgs.path {
    system = pkgs.stdenv.hostPlatform.system;
    inherit overlays;
  };

  # The wrapped environment (executable)
  arduinoEnv = pkgsWithArduino.mkArduinoEnv {
    packages = with pkgsWithArduino.arduinoPackages; [
      platforms.esp32.esp32."2.0.14"
    ];
    libraries = import ./common-libs.nix { inherit pkgsWithArduino; };
  };

  # Prepare source
  preparedSrc = pkgs.runCommand "nimrs-firmware-src" { } ''
    mkdir -p $out/NIMRS-Firmware
    cp -r ${src}/* $out/NIMRS-Firmware/
    chmod -R +w $out

    if [ ! -f $out/NIMRS-Firmware/config.h ]; then
      cp $out/NIMRS-Firmware/config.example.h $out/NIMRS-Firmware/config.h
    fi
  '';

in
pkgs.stdenv.mkDerivation {
  name = "nimrs-firmware";
  src = "${preparedSrc}/NIMRS-Firmware";

  nativeBuildInputs = [
    arduinoEnv
    pkgs.git
    (pkgs.python3.withPackages (ps: [ ps.pyserial ]))
  ];

  # Pass git hash if provided
  GIT_HASH_ENV = gitHash;

  buildPhase = ''
    echo "Building NIMRS Firmware..."
    ${import ./build-command.nix {
      outputDir = "$out";
      inherit lamejs;
    }}
  '';

  installPhase = ''
    # Artifacts are already in $out due to --output-dir
    echo "Build complete. Artifacts in $out"
  '';
}
