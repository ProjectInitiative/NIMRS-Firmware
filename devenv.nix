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
        echo "  NIMRS-Firmware Development Environment (Rust)"
        echo "  ----------------------------------------------------"
        echo ""
        echo "  BUILD"
        echo "    cargo build                 : Build Rust firmware (dev)"
        echo "    nix build                   : Sandboxed Rust firmware build"
        echo ""
        echo "  FLASH & MONITOR"
        echo "    cargo espflash flash        : Build + flash Rust firmware"
        echo "    cargo espflash monitor      : Flash + open serial monitor"
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
    echo "  cargo espflash flash        : Build + flash Rust firmware"
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
