# Agent Working Guide

As an automated agent, you are operating in a hermetic development environment provided by **Nix** and **direnv**.

## 🚀 Working in the Environment

The repository environment is automatically loaded via `direnv`. Ensure you have run `direnv allow` in the project root.

### Available Commands

Use these commands via `nix develop --command <cmd>` or directly if your shell is hooked:

- **`build-firmware`**: Compiles the project using `arduino-cli`.
- **`run-tests`**: Executes host-side unit tests.
- **`treefmt`**: Automatically formats all code (C++, JSON, MD, Python).
- **`agent-check`**: **(REQUIRED)** Comprehensive pre-submission validation.

---

## 🛡️ Mandatory Pre-Submission Check

Before you request a review, submit a PR, or consider your task "done", you **MUST** run:

```bash
nix develop --command agent-check
```

This command enforces:

1.  **Clean Tree**: You must commit all your changes first.
2.  **Formatting**: Verifies that your code matches the project style.
3.  **Unit Tests**: Ensures no regressions were introduced.
4.  **Build**: Verifies the firmware still compiles for the ESP32-S3.
5.  **Merge Conflicts**: Verifies your branch can merge cleanly into `origin/main`.

**If `agent-check` fails, you are NOT finished. Resolve all errors before proceeding.**
