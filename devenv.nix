{ pkgs, config, ... }:
{
  cachix.enable = false;

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
        echo "  NIMRS-Firmware Development Environment (ESP-IDF Native | devenv)"
        echo "  -----------------------------------------------------------------"
        echo ""
        echo "  BUILD"
        echo "    build-firmware              : Build C++ firmware (idf.py build)"
        echo "    cargo build                 : Build Rust firmware"
        echo "    nix build                   : Sandboxed C++ firmware build"
        echo "    nix build .#rust-firmware   : Sandboxed Rust firmware build"
        echo ""
        echo "  FLASH & MONITOR"
        echo "    upload-firmware <PORT|IP>   : Upload via Serial or OTA"
        echo "    flash-all <PORT>            : Flash bootloader + partition + app"
        echo "    flash-factory <PORT>        : Full erase then flash factory image"
        echo "    erase-flash <PORT>          : Wipe entire chip"
        echo "    reset-ota <PORT>            : Erase OTA data partition"
        echo "    monitor-firmware <PORT|IP>  : Serial monitor or WiFi log stream"
        echo "    nimrs-logs <IP>             : Stream text logs via WiFi"
        echo "    nimrs-telemetry <IP>        : Stream motor debug data via WiFi"
        echo "    cargo espflash flash        : Build + flash Rust firmware"
        echo ""
        echo "  TEST & SIM"
        echo "    cargo test                  : Run Rust unit tests (host)"
        echo "    cargo clippy                : Rust linter"
        echo "    motor-sim                   : C++ PID control loop simulation"
        echo ""
        echo "  QUALITY"
        echo "    ci-ready                    : Formatting + tests + build"
        echo "    agent-check                 : ci-ready + merge check (REQUIRED)"
        echo "    treefmt                     : Format all code"
        echo "    nix flake check             : All checks (formatting, tests, docs)"
        echo ""
  '';

  scripts.commands.exec = ''
    echo ""
    echo "Commands available:"
    echo "  build-firmware              : Build C++ firmware (idf.py build)"
    echo "  cargo build                 : Build Rust firmware"
    echo "  nix build .#rust-firmware   : Sandboxed Rust firmware build"
    echo "  cargo test                  : Run Rust unit tests (host)"
    echo "  cargo clippy                : Rust linter"
    echo "  cargo espflash flash        : Build + flash Rust firmware"
    echo "  upload-firmware <PORT|IP>   : Upload via Serial or OTA"
    echo "  flash-all <PORT>            : Flash bootloader + partition + app"
    echo "  flash-factory <PORT>        : Full erase then flash factory image"
    echo "  erase-flash <PORT>          : Wipe entire chip"
    echo "  reset-ota <PORT>            : Erase OTA data partition"
    echo "  monitor-firmware <PORT|IP>  : Serial monitor or WiFi log stream"
    echo "  nimrs-telemetry <IP>        : Stream motor debug data via WiFi"
    echo "  nimrs-logs <IP>             : Stream text logs via WiFi"
    echo "  motor-sim                   : C++ PID control loop simulation"
    echo "  generate-api-docs           : Generate docs/API.md"
    echo "  ci-ready                    : Formatting + tests + build"
    echo "  agent-check                 : ci-ready + merge check (REQUIRED)"
    echo "  treefmt                     : Format all code"
    echo "  nix build                   : Sandboxed C++ firmware build"
    echo "  nix flake check             : All checks (formatting, tests, docs)"
  '';
}
