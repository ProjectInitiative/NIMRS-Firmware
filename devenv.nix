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
        echo "NIMRS-Firmware Development Environment (ESP-IDF Native | devenv)"
        echo "------------------------------------------------------------------"
        echo ""
        echo "  build-firmware            : Build the firmware (idf.py build wrapper)"
        echo "  upload-firmware <PORT|IP> : Upload firmware via Serial or OTA"
        echo "  flash-all <PORT>          : Flash bootloader + partition table + app"
        echo "  flash-factory <PORT>      : Full chip erase then flash factory image"
        echo "  erase-flash <PORT>        : Wipe the entire chip"
        echo "  reset-ota <PORT>          : Erase OTA data partition"
        echo "  monitor-firmware <PORT|IP>: Monitor logs via Serial (miniterm) or WiFi"
        echo "  nimrs-telemetry <IP>      : Stream live motor debug data (WiFi)"
        echo "  nimrs-logs <IP>           : Stream text logs (WiFi)"
        echo "  motor-sim                 : Run PID control loop simulation"
        echo "  generate-api-docs         : Generate docs/API.md"
        echo "  ci-ready                  : Formatting + tests + build"
        echo "  agent-check               : ci-ready + merge check (REQUIRED)"
        echo "  treefmt                   : Format all code"
        echo "  nix build                 : Clean sandboxed build"
        echo "  nix flake check           : All checks (formatting, tests, docs)"
        echo ""
  '';

  scripts.commands.exec = ''
    echo ""
    echo "Commands available:"
    echo "  build-firmware            : Build the firmware (idf.py build wrapper)"
    echo "  upload-firmware <PORT|IP> : Upload firmware via Serial or OTA"
    echo "  flash-all <PORT>          : Flash bootloader + partition table + app"
    echo "  flash-factory <PORT>      : Full chip erase then flash factory image"
    echo "  erase-flash <PORT>        : Wipe the entire chip"
    echo "  reset-ota <PORT>          : Erase OTA data partition"
    echo "  monitor-firmware <PORT|IP>: Monitor logs via Serial or WiFi"
    echo "  nimrs-telemetry <IP>      : Stream live motor debug data (WiFi)"
    echo "  nimrs-logs <IP>           : Stream text logs (WiFi)"
    echo "  motor-sim                 : Run PID control loop simulation"
    echo "  generate-api-docs         : Generate docs/API.md"
    echo "  ci-ready                  : Formatting + tests + build"
    echo "  agent-check               : ci-ready + merge check (REQUIRED)"
    echo "  treefmt                   : Format all code"
    echo "  nix build                 : Clean sandboxed build"
    echo "  nix flake check           : All checks (formatting, tests, docs)"
  '';
}
