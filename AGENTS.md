# Agent Working Guide

As an automated agent, you are operating in a hermetic development environment provided by **Nix**, **direnv**, and **devenv**.

## Working in the Environment

The repository environment is automatically loaded via `direnv`. Ensure you have run `direnv allow` in the project root.

All development commands are available directly in the devenv shell. If your shell is hooked via `direnv`, they are already in your PATH:

| Command                       | Description                                             |
| ----------------------------- | ------------------------------------------------------- |
| `build-firmware`              | Build firmware via `idf.py build` (wrapper)             |
| `upload-firmware <PORT\|IP>`  | Upload firmware via Serial or OTA                       |
| `flash-all <PORT>`            | Flash bootloader + partition table + app via Serial     |
| `flash-factory <PORT>`        | Erase entire flash then flash factory image             |
| `erase-flash <PORT>`          | Wipe the entire chip                                    |
| `reset-ota <PORT>`            | Erase OTA data partition to reset rollback state        |
| `monitor-firmware <PORT\|IP>` | Monitor logs via Serial (miniterm) or WiFi (nimrs-logs) |
| `nimrs-telemetry <IP>`        | Stream live motor debug data (WiFi)                     |
| `nimrs-logs <IP>`             | Stream text logs (WiFi)                                 |
| `motor-sim`                   | Run high-fidelity PID control loop simulation           |
| `generate-api-docs`           | Generate API documentation at `docs/API.md`             |
| `ci-ready`                    | Run formatting + tests + build to verify CI readiness   |
| `agent-check`                 | **(REQUIRED)** Run ci-ready + check for merge conflicts |
| `treefmt`                     | Format all code (C++, JSON, MD, Python, Nix)            |
| `nix build`                   | Clean sandboxed build of the firmware                   |
| `nix flake check`             | Run all checks (formatting, api-docs, tests)            |

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
