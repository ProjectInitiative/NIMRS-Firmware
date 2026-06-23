# Rust Migration Phase 0 — Blocker Analysis

## Date

2026-06-23

## Status

Blocked at final linking stage.

## What Works

- Xtensa Rust toolchain (espup) — FOD cached, reproducible
- Rust `std` compilation for `xtensa-esp32s3-espidf` target (build-std)
- All Rust dependencies compile (esp-idf-sys, cmake crate, etc.)
- `ldproxy` linker correctly configured with `--ldproxy-linker xtensa-esp32s3-elf-gcc`
- ESP-IDF framework CMake build via `fromenv` mode — produces ALL `.a` libraries
- Two-pass build approach: first compiles deps, second links with ESP-IDF libraries

## The Blocker

### Symptom

Linker errors: undefined references to C library functions (`memcpy`, `vprintf`), ROM functions (`esp_rom_printf`), and some cross-component symbols (`systimer_hal_get_counter_value`).

### Current Build Phase Implementation

```nix
buildPhase = ''
  # Pass 1: compile everything (link expected to fail)
  cargo build --release ... || true

  # Collect .a files from esp-idf-sys build output
  ESP_IDF_OUT=$(ls -d target/.../build/esp-idf-sys-*/out)
  LIB_DIRS=""  # -L for each unique .a directory
  LIBS_FLAGS=""  # -C link-args=-l<lib> for each .a

  # Pass 2: rebuild with ESP-IDF library flags
  RUSTFLAGS="--cfg espidf_time64 \
    -C link-args=--ldproxy-linker -C link-args=xtensa-esp32s3-elf-gcc \
    $LIB_DIRS \
    -C link-args=-Wl,--start-group \
    $LIBS_FLAGS \
    -C link-args=-Wl,--end-group"
  cargo build --release --target xtensa-esp32s3-espidf
'';
```

Undefined symbols remaining:
| Symbol | Needed by | Should be in |
|--------|-----------|-------------|
| `memcpy` | `libheap.a`, `libnewlib.a` | `newlib` or `compiler-builtins` |
| `vprintf` | `liblog.a` | `newlib` |
| `esp_rom_printf` | `libxtensa.a` | `esp_rom` |
| `systimer_hal_get_counter_value` | `libesp_timer.a` | `hal` or `soc` |

### Root Cause

`esp-idf-sys` v0.37.2 build script does not emit `cargo:rustc-link-search` or `cargo:rustc-link-lib` directives when `force_ldproxy(true)` is used. The `.a` files are built by CMake but cargo is not told about them. Our workaround manually injects the library paths/flags via `RUSTFLAGS` + `-C link-args`.

The `-C link-args` approach avoids the dependency compilation issue (where `-l static=<lib>` would fail for all crates). But some C-level symbols remain unresolved, likely due to:

1. Link order within `--start-group`/`--end-group`
2. `newlib` conditional compilation excluding certain functions
3. Missing variant libraries (e.g., ESP32-S3 specific HAL vs generic HAL)

## Attempted Fixes

### Pass 1: Upgrade esp-idf-sys

Latest version is 0.37.2 — no newer version exists.

### Pass 2: Manual library injection via `cargo rustc`

Flags from `--` were NOT reaching the linker. Root cause unclear.

### Pass 3: Manual library injection via `RUSTFLAGS` + `-l static=`

`-l static=` flags poison ALL dependency compilation (even rustc-std-workspace-core). Rustc verifies native library existence during compilation, not just linking.

### Pass 4: Manual library injection via `RUSTFLAGS` + `-C link-args=-l<lib>`

Doesn't poison compilation (only applies at link time). But order of `-C link-args` within the linker invocation is: config flags first, then RUSTFLAGS flags. This CORRECTLY places `--start-group` around the libraries. But `--as-needed` combined with library ordering may still leave some symbols unresolved.

### Pass 5: Main-only libs (skip bootloader debug)

Helped reduce errors but still not fully resolved — C library functions still missing.

## Recommended Unblock Paths

### A. Patch vendored esp-idf-sys to disable force_ldproxy

The `force_ldproxy(true)` call in `cargo_driver.rs` suppresses `cargo:rustc-link-*` output. Patching this to `false` and using a GCC linker directly would let the standard cargo link mechanism work.

```rust
.force_ldproxy(false)
```

This requires modifying the vendored `esp-idf-sys` flake input (fork or overlay).

### B. Use older esp-idf-sys or switch to esp-idf-svc

`esp-idf-svc` v0.50+ might handle linking correctly. But this changes the dependency tree significantly.

### C. Add GccRuntime library for missing symbols

The `-nodefaultlibs` flag means GCC's runtime (libgcc) isn't linked. Adding `-C link-args=-lgcc` and `-C link-args=-lc` after the ESP-IDF libs might resolve the remaining C library symbols.

### D. Use xtensa-esp32s3-elf-gcc as linker (skip ldproxy)

Using GCC directly instead of ldproxy avoids ldproxy's argument processing. Combine with patching `force_ldproxy(false)` in esp-idf-sys.

## Working Configuration Files

- `nix/esp-rust.nix` — Xtensa Rust toolchain FOD
- `.cargo/config.toml` — target, ldproxy, build-std, sdkconfig defaults
- `src/main.rs` — minimal blink + println firmware
- `flake.nix` (rust-firmware package) — two-pass build with library injection
- `sdkconfig.rust.defaults` — override partition table for Rust build
- `devenv.nix` — Rust command documentation
- `AGENTS.md` — Rust migration notes
