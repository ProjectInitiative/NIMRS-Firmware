# Building with Nix

This project uses a reproducible Nix-based development environment to ensure consistent builds across all machines.

## Prerequisites

1.  **Install Nix:** Follow the instructions at [nixos.org/download](https://nixos.org/download.html).
2.  **Enable Flakes:** Ensure your `~/.config/nix/nix.conf` contains:
    ```
    experimental-features = nix-command flakes
    ```
3.  **Install Direnv (Optional but Recommended):** [direnv.net](https://direnv.net/) automatically loads the environment when you enter the directory.

## Entering the Environment

If you have `direnv` installed:

```bash
direnv allow
```

Otherwise, you can manually enter the shell:

```bash
nix develop
```

Once inside, you will have access to all necessary tools (`arduino-cli`, `python`, `esptool`, etc.) and helper scripts.

## Helper Scripts

The environment provides several scripts to simplify development:

### Building

- **`build-firmware`**: Compiles the firmware using `arduino-cli`.
  - Output is placed in the `build/` directory.
  - Automatically checks firmware size against the `app0` partition.

### Flashing

- **`upload-firmware <port|IP>`**: Uploads the compiled binary.
  - **Serial:** `upload-firmware /dev/ttyACM0`
  - **OTA:** `upload-firmware 192.168.1.50` (Requires the device to be on WiFi)

### Monitoring

- **`monitor-firmware <port|IP>`**: Connects to the device logs.
  - **Serial:** Connects via USB. Automatically disables DTR/RTS to prevent resetting the board.
  - **WiFi:** Connects to the WebSocket log stream.

- **`nimrs-logs <IP>`**: View live logs over WiFi.
- **`nimrs-telemetry <IP>`**: View real-time motor telemetry graphs (requires a specific Python tool setup).

### Testing & Quality

- **`run-tests`**: Runs the host-side unit tests defined in `tests/`.
- **`treefmt`**: Formats all code (C++, JSON, Markdown) using standard formatters.
- **`ci-ready`**: Runs formatting, tests, and a build check to simulate CI validation.

## Manual Nix Build

You can also build the firmware directly without entering the shell:

```bash
nix build .#firmware
```

This produces a result in `./result`.

To run tests:

```bash
nix build .#tests
```
