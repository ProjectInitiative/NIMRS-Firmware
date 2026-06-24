# Agent Working Guide

As an automated agent, you are operating in a hermetic development environment provided by **Nix**, **direnv**, and **devenv**.

## Working in the Environment

The repository environment is automatically loaded via `direnv`. Ensure you have run `direnv allow` in the project root.

All development commands are available directly in the devenv shell:

| Command                       | Description                                             |
| ----------------------------- | ------------------------------------------------------- |
| `cargo build`                 | Build Rust firmware (dev, Xtensa target)                |
| `cargo build --release`       | Build Rust firmware (release, optimized)                |
| `cargo test`                  | Run unit tests (host target)                            |
| `cargo clippy`                | Run Rust linter                                         |
| `cargo fmt --check`           | Check Rust formatting                                   |
| `cargo espflash flash`        | Build (release) + flash via USB-Serial                  |
| `cargo espflash monitor`      | Flash + open serial monitor                             |
| `upload-firmware <PORT\|IP>`  | Upload firmware via Serial or OTA                       |
| `flash-all <PORT>`            | Flash bootloader + partition table + app via Serial     |
| `erase-flash <PORT>`          | Wipe the entire chip                                    |
| `monitor-firmware <PORT\|IP>` | Monitor logs via Serial (miniterm) or WiFi (nimrs-logs) |
| `nimrs-telemetry <IP>`        | Stream live motor debug data (WiFi)                     |
| `nimrs-logs <IP>`             | Stream text logs (WiFi)                                 |
| `nix build`                   | Sandboxed Rust firmware build                           |
| `nix flake check`             | Run all checks (formatting, tests, docs, build)         |

Commands can also be run without direnv hooking:

```bash
nix develop --command <cmd>
```

## Mandatory Pre-Submission Check

Before you request a review, submit a PR, or consider your task "done", you **MUST** run:

```bash
nix develop --command agent-check
```

This command enforces:

1. **Clean Tree**: You must commit all your changes first.
2. **Formatting**: Verifies that your code matches the project style.
3. **Unit Tests**: Ensures no regressions were introduced.
4. **Build**: Verifies the firmware still compiles for the ESP32-S3.
5. **Merge Conflicts**: Verifies your branch can merge cleanly into `origin/main`.

**If `agent-check` fails, you are NOT finished. Resolve all errors before proceeding.**

## Rust Firmware

The firmware is now fully written in Rust. Key crates:

- **`nimrs-core`** (`crates/core/`) — Pure logic (DSP, BEMF, ripple, CV, pinout, context, web assets, audio assets)
- **`nimrs-firmware`** (`src/`) — ESP-IDF binary (HAL, motor control, lighting, WiFi, HTTP, DCC, audio)

### Build Artifacts

- `nix build` produces `result/nimrs-firmware` — ELF 32-bit LSB executable, Tensilica Xtensa
- `cargo build --release` produces `target/xtensa-esp32s3-espidf/release/nimrs-firmware`

### Testing

- `cargo test -p nimrs-core` runs all pure-logic unit tests on the host target
- Cross-compilation verification via `nix build .#rust-firmware` or `nix build`
