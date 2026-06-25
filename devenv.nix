{
  pkgs,
  config,
  inputs,
  lib,
  options,
  ...
}:
let
  system = pkgs.system;
  espRustToolchain = pkgs.callPackage ./nix/esp-rust.nix {
    inherit (pkgs) rustup;
  };
  espIdfFull =
    (builtins.getFlake "github:mirrexagon/nixpkgs-esp-dev/5287d6e1ca9e15ebd5113c41b9590c468e1e001b")
    .packages.${system}.esp-idf-full;
  lamejs = pkgs.fetchurl {
    url = "https://raw.githubusercontent.com/zhuker/lamejs/master/lame.min.js";
    sha256 = "1x3dxi4c7h9dv8akhb58h1s4y1yc0z7fd3d633yxnfvvb3i8blhm";
  };
  scripts = pkgs.callPackage ./nix/scripts.nix { inherit pkgs; };

  formattingTools = with pkgs; [
    treefmt
    clang-tools
    prettier
    nixfmt
    black
    shfmt
    git
  ];
in
{
  cachix.enable = false;

  packages = [
    espRustToolchain
    pkgs.ldproxy
    pkgs.espflash
    pkgs.python3
    pkgs.esptool
  ]
  ++ formattingTools
  ++ [
    scripts.nimrsLogs
    scripts.nimrsTelemetry
    scripts.ciReady
    scripts.agentCheck
    scripts.uploadFirmware
    scripts.monitorFirmware
    scripts.flashAll
    scripts.flashFactory
    scripts.eraseFlash
    scripts.resetOta
    scripts.generateApiDocs
  ];

  env = {
    IDF_PATH = "${espIdfFull}";
    LAMEJS_PATH = "${lamejs}";
    GIT_HASH = if inputs.self ? rev then inputs.self.rev else "unknown";
    ESP_IDF_TOOLS_INSTALL_DIR = "fromenv";
    LIBCLANG_PATH = "${pkgs.llvmPackages.libclang.lib}/lib";
    MCU = "esp32s3";
  };

  enterShell = ''
        PATH="$IDF_PATH/tools:$PATH"

        if [ -d .git ] && [ ! -f .git/hooks/pre-commit ]; then
          echo "Installing treefmt pre-commit hook..."
          cat > .git/hooks/pre-commit <<'PRECOMMIT'
    #!/bin/sh
    treefmt --fail-on-change || exit 1
    PRECOMMIT
          chmod +x .git/hooks/pre-commit
        fi

        echo ""
        echo "  NIMRS-Firmware Development Environment (Rust)"
        echo "  ----------------------------------------------------"
        echo ""
        echo "  BUILD"
        echo "    cargo build                 : Build Rust firmware (dev)"
        echo "    nix build                   : Sandboxed Rust firmware build"
        echo ""
        echo "  FLASH & MONITOR"
        echo "    espflash flash              : Build + flash Rust firmware"
        echo "    espflash monitor            : Flash + open serial monitor"
        echo "    upload-firmware <PORT|IP>   : Upload via Serial or OTA"
        echo "    flash-all <PORT>            : Flash bootloader + partition + app"
        echo "    erase-flash <PORT>          : Wipe entire chip"
        echo "    monitor-firmware <PORT|IP>  : Serial monitor or WiFi log stream"
        echo "    nimrs-logs <IP>             : Stream text logs via WiFi"
        echo "    nimrs-telemetry <IP>        : Stream motor debug data via WiFi"
        echo ""
        echo "  TEST & QUALITY"
        echo "    cargo test                  : Run Rust unit tests (host)"
        echo "    cargo clippy                : Rust linter"
        echo "    cargo fmt --check           : Check Rust formatting"
        echo "    ci-ready                    : Formatting + tests + build"
        echo "    agent-check                 : ci-ready + merge check (REQUIRED)"
        echo "    treefmt                     : Format all code"
        echo "    nix flake check             : All checks (formatting, tests, docs)"
        echo ""
  '';

  scripts.commands.exec = ''
    echo ""
    echo "Commands available:"
    echo "  cargo build                 : Build Rust firmware (dev)"
    echo "  cargo test                  : Run Rust unit tests (host)"
    echo "  cargo clippy                : Rust linter"
    echo "  espflash flash              : Build + flash Rust firmware"
    echo "  upload-firmware <PORT|IP>   : Upload via Serial or OTA"
    echo "  flash-all <PORT>            : Flash bootloader + partition + app"
    echo "  erase-flash <PORT>          : Wipe entire chip"
    echo "  monitor-firmware <PORT|IP>  : Serial monitor or WiFi log stream"
    echo "  nimrs-telemetry <IP>        : Stream motor debug data via WiFi"
    echo "  nimrs-logs <IP>             : Stream text logs via WiFi"
    echo "  ci-ready                    : Formatting + tests + build"
    echo "  agent-check                 : ci-ready + merge check (REQUIRED)"
    echo "  treefmt                     : Format all code"
    echo "  nix build                   : Sandboxed Rust firmware build"
    echo "  nix flake check             : All checks (formatting, tests, docs)"
  '';
}
