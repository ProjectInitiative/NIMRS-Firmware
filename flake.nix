{
  description = "NIMRS-Firmware Development Environment (ESP-IDF Native, devenv)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    flake-parts.inputs.nixpkgs-lib.follows = "nixpkgs";
    devenv.url = "github:cachix/devenv";
    devenv.inputs.nixpkgs.follows = "nixpkgs";
    nix2container.url = "github:nlewo/nix2container";
    nix2container.inputs.nixpkgs.follows = "nixpkgs";
    mk-shell-bin.url = "github:rrbutani/nix-mk-shell-bin";
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

    # Vendored esp-idf-sys for Rust build
    esp-idf-sys = {
      url = "github:esp-rs/esp-idf-sys/v0.37.2";
      flake = false;
    };
  };

  nixConfig = {
    extra-trusted-public-keys = "devenv.cachix.org-1:w1cLUi8dv3hnoSPGAuibQv+f9TZLr6cv/Hm9XgU50cw=";
    extra-substituters = "https://devenv.cachix.org";
  };

  outputs =
    inputs@{ flake-parts, self, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [ inputs.devenv.flakeModule ];

      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      perSystem =
        {
          config,
          self',
          pkgs,
          system,
          ...
        }:
        let
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
            (inputs.arduino-nix.overlay)
            (inputs.arduino-nix.mkArduinoPackageOverlay (inputs.arduino-indexes + "/index/package_index.json"))
            (inputs.arduino-nix.mkArduinoPackageOverlay (
              inputs.arduino-indexes + "/index/package_esp32_index.json"
            ))
            (inputs.arduino-nix.mkArduinoLibraryOverlay (inputs.arduino-indexes + "/index/library_index.json"))
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
            esp-idf = inputs.esp-dev.packages.${system}.esp-idf-full;
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

          espIdfFull = inputs.esp-dev.packages.${system}.esp-idf-full;

          # ---------------------------------------------------------
          # Rust / Xtensa Toolchain
          # ---------------------------------------------------------

          espRustToolchain = pkgs.callPackage ./nix/esp-rust.nix {
            inherit (pkgs) rustup;
          };
        in
        {
          packages = {
            dependencies = nimrsDeps;
            arduino-components = arduinoComponents;

            # Xtensa Rust toolchain (FOD — first build downloads ~500MB)
            "esp-rust-toolchain" = espRustToolchain;

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

            # Rust firmware (Phase 0 spike)
            rust-firmware = pkgs.stdenv.mkDerivation {
              name = "nimrs-firmware-rust";
              src = ./.;
              nativeBuildInputs = [
                espIdfFull
                espRustToolchain
                pkgs.llvmPackages.libclang
                pkgs.ldproxy
                pkgs.python3
                pkgs.cmake
                pkgs.ninja
                pkgs.pkg-config
                pkgs.git
              ];
              dontConfigure = true;
              buildPhase = ''
                export HOME=$TMPDIR
                unset IDF_PATH
                export GIT_SSL_CAINFO="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"
                export SSL_CERT_FILE="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"

                # Copy vendored esp-idf-sys from flake input
                mkdir -p vendor
                ln -s ${inputs.esp-idf-sys} vendor/esp-idf-sys

                export LIBCLANG_PATH="${pkgs.llvmPackages.libclang.lib}/lib"

                echo "=== Rust/ESP-IDF toolchain check ==="
                command -v rustc && rustc --version
                command -v cargo && cargo --version
                command -v xtensa-esp32s3-elf-gcc && xtensa-esp32s3-elf-gcc --version 2>&1 | head -1
                python3 --version
                cmake --version 2>&1 | head -1

                echo "=== Building Rust firmware ==="
                cargo build --release --target xtensa-esp32s3-espidf
              '';
              installPhase = ''
                mkdir -p $out
                if [ -f target/xtensa-esp32s3-espidf/release/nimrs-firmware ]; then
                  cp target/xtensa-esp32s3-espidf/release/nimrs-firmware $out/
                fi
                if [ -f target/xtensa-esp32s3-espidf/release/nimrs-firmware.elf ]; then
                  cp target/xtensa-esp32s3-espidf/release/nimrs-firmware.elf $out/
                fi
                ls -la $out/
              '';
            };

            default = pkgs.stdenv.mkDerivation {
              pname = "nimrs-firmware";
              version = "0.1.0";
              src = ./.;
              nativeBuildInputs = [
                espIdfFull
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

            # Suppress devenv auto-generated packages (nixpkgs compat)
            "container-processes" = pkgs.lib.mkForce pkgs.emptyDirectory;
            "container-shell" = pkgs.lib.mkForce pkgs.emptyDirectory;
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

            tests = self'.packages.tests;
          };

          devenv.shells.default = {
            imports = [ ./devenv.nix ];

            packages = [
              espIdfFull
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

            env = {
              IDF_PATH = "${espIdfFull}";
              LAMEJS_PATH = "${lamejs}";
              ARDUINO_COMPONENTS_PATH = "${arduinoComponents}";
              MANAGED_COMPONENTS_PATH = "${nimrsDeps}/managed_components";
              NIMRS_DEPS_PATH = "${nimrsDeps}";
              GIT_HASH = "${gitHash}";
              ESP_IDF_TOOLS_INSTALL_DIR = "fromenv";
              LIBCLANG_PATH = "${pkgs.llvmPackages.libclang.lib}/lib";
              MCU = "esp32s3";
            };

          };

          formatter = pkgs.nixfmt;
        };
    };
}
