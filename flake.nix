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

          # Extract git hash for versioning
          gitHash = self.rev or self.dirtyRev or "unknown";

          # Import scripts
          scripts = pkgs.callPackage ./nix/scripts.nix {
            inherit pkgs;
          };

          inherit (scripts)
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

          # Vendored Rust crates including build-std deps (sandbox-safe).
          # cargoLock FOD runs cargo build (downloads all deps including std's
          # internal deps) and captures the entire cargo registry cache.
          cargoLock = pkgs.stdenv.mkDerivation {
            name = "cargo-vendored-deps";
            src = ./.;
            nativeBuildInputs = [
              espRustToolchain
              pkgs.cacert
            ];
            buildPhase = ''
              export HOME=$TMPDIR
              export GIT_SSL_CAINFO="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"
              export SSL_CERT_FILE="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"
              mkdir -p vendor
              ln -s ${inputs.esp-idf-sys} vendor/esp-idf-sys
              # Run cargo build to download all deps (including build-std's
              # internal deps like hashbrown). The build will fail (no ESP-IDF)
              # but all crates are cached in ~/.cargo/registry/.
              cargo build --release --target xtensa-esp32s3-espidf 2>&1 || true
            '';
            installPhase = ''
              mkdir -p $out
              # Capture the full cargo registry cache (includes build-std deps)
              cp -r $HOME/.cargo/registry $out/registry
              # Copy the generated Cargo.lock (has correct project dep versions)
              cp Cargo.lock $out/
            '';
            outputHashAlgo = "sha256";
            outputHashMode = "recursive";
            outputHash = "sha256-RYjgx8Fb9rDU+EOYGBmIksM3dEpavBUkUWzWovZ8z7Y=";
          };
        in
        {
          packages = {
            # ESP-IDF (from esp-dev flake input)
            esp-idf-full = espIdfFull;

            # Xtensa Rust toolchain (FOD — first build downloads ~500MB)
            "esp-rust-toolchain" = espRustToolchain;

            # Generated Cargo.lock (uses nightly toolchain for correct std deps)
            cargoLock = cargoLock;

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
                export IDF_PATH="${espIdfFull}"
                export ESP_IDF_TOOLS_INSTALL_DIR="fromenv"
                export LIBCLANG_PATH="${pkgs.llvmPackages.libclang.lib}/lib"
                export MCU="esp32s3"
                export GIT_SSL_CAINFO="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"
                export SSL_CERT_FILE="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"

                # Copy vendored esp-idf-sys from flake input (gitignored, not in src)
                mkdir -p vendor
                ln -s ${inputs.esp-idf-sys} vendor/esp-idf-sys

                # Set up CARGO_HOME with the cached registry (includes build-std deps)
                export CARGO_HOME=$TMPDIR/.cargo
                mkdir -p $CARGO_HOME
                cp -r ${cargoLock}/registry $CARGO_HOME/registry
                # Use the generated Cargo.lock (correct versions for nightly toolchain)
                cp ${cargoLock}/Cargo.lock Cargo.lock

                echo "=== Rust/ESP-IDF toolchain check ==="
                command -v rustc && rustc --version
                command -v cargo && cargo --version
                command -v xtensa-esp32s3-elf-gcc && xtensa-esp32s3-elf-gcc --version 2>&1 | head -1
                echo "IDF_PATH=$IDF_PATH"
                python3 --version
                cmake --version 2>&1 | head -1

                echo "=== Single-pass build (esp-idf-sys build script + ldproxy emit all link args) ==="
                cargo build --frozen --release --target xtensa-esp32s3-espidf --verbose 2>&1
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
                export IDF_PATH="${espIdfFull}"
                export ESP_IDF_TOOLS_INSTALL_DIR="fromenv"
                export LIBCLANG_PATH="${pkgs.llvmPackages.libclang.lib}/lib"
                export MCU="esp32s3"
                export GIT_SSL_CAINFO="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"
                export SSL_CERT_FILE="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"

                mkdir -p vendor
                ln -s ${inputs.esp-idf-sys} vendor/esp-idf-sys

                export CARGO_HOME=$TMPDIR/.cargo
                mkdir -p $CARGO_HOME
                cp -r ${cargoLock}/registry $CARGO_HOME/registry
                cp ${cargoLock}/Cargo.lock Cargo.lock

                cargo build --frozen --release --target xtensa-esp32s3-espidf --verbose 2>&1
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
          };

          formatter = pkgs.nixfmt;
        };
    };
}
