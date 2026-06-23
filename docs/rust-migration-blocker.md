# Rust Migration Phase 0 — Blocker RESOLVED

## Date

2026-06-23 (resolved)

## Status

**RESOLVED** — Rust firmware builds and links successfully.

## Original Blocker (Incorrect Diagnosis)

The original blocker doc claimed `force_ldproxy(true)` in esp-idf-sys v0.37.2 "does NOT emit `cargo:rustc-link-*` directives." This was **incorrect**.

Reading the embuild source (`embuild/src/build.rs` + `embuild/src/cargo.rs`):

- `LinkArgs::output()` calls `add_link_arg()` for every arg
- `add_link_arg()` (`embuild/src/cargo.rs`) emits `cargo:rustc-link-arg=<arg>`
- esp-idf-sys `build/build.rs:280-283` calls **both** `link_args.propagate()` **and** `link_args.output()`

With `force_ldproxy(true)` + `linker = "ldproxy"` (so `detected_ldproxy = true`), the args include `--ldproxy-linker <gcc>`, `--ldproxy-cwd <cmake_build_dir>`, then all CMake-computed `libdirflags` (`-L`), `libflags` (`-l`), and `linkflags` (`-Wl,...`, `-T <scripts>`, `-u <ROM symbols>`).

However: `cargo:rustc-link-arg` from a **dependency's** build script does NOT automatically propagate to the final binary's link. Only `cargo:rustc-link-lib` and `cargo:rustc-link-search` auto-propagate. The `propagate()` method sets metadata (`DEP_ESP_IDF_EMBUILD_LINK_ARGS`), which must be explicitly consumed by the root crate.

## Root Cause

**The root crate (nimrs-firmware) was missing a `build.rs`** that calls `embuild::espidf::sysenv::output()`. This function (from embuild's `src/espidf.rs:sysenv` module):

1. Reads `DEP_ESP_IDF_EMBUILD_LINK_ARGS` (metadata set by esp-idf-sys's `propagate()`)
2. Emits each link arg as `cargo:rustc-link-arg=<arg>` for the **root crate's** link step

Without this, the complete CMake-computed link command (libs + linker scripts + `-u` ROM symbols + `-Wl` flags) never reached the final binary's linker invocation.

The two-pass workaround (manual RUSTFLAGS injection of `-L`/`-l` flags) only reconstructed libraries from `.a` files, **dropping `linkflags`** entirely. This caused:
- `esp_rom_printf` — needs `-u` linker flag (in `linkflags`)
- `memcpy` / `vprintf` — need newlib linker-script conditional includes (in `linkflags`)
- `systimer_hal_get_counter_value` — needs `-Wl` symbol resolution order

## The Fix

### 1. Added `build.rs` (root crate)

```rust
fn main() {
    embuild::espidf::sysenv::output();
}
```

This is the exact pattern used by the official [esp-idf-template](https://github.com/esp-rs/esp-idf-template/blob/master/cargo/build.rs).

### 2. Added embuild as a build-dependency

```toml
[build-dependencies]
embuild = "0.33"
```

### 3. Simplified `.cargo/config.toml` rustflags

Removed redundant `-C link-args=--ldproxy-linker` (embuild now emits these via `cargo:rustc-link-arg`):

```toml
[target.'cfg(target_os = "espidf")']
linker = "ldproxy"
rustflags = ["--cfg", "espidf_time64"]
```

### 4. Removed two-pass build from `flake.nix`

Replaced the complex two-pass buildPhase (Pass 1 compile + Pass 2 link with manual RUSTFLAGS) with a single:

```bash
cargo build --release --target xtensa-esp32s3-espidf --verbose
```

### 5. Added espRustToolchain + ldproxy to devenv shell

Added `espRustToolchain` and `pkgs.ldproxy` to the devenv shell packages so `cargo build` in the dev shell uses the nightly Xtensa toolchain.

## Verification

```bash
nix build .#rust-firmware --option sandbox false --no-link
```

Produces a valid Xtensa ELF:
```
nimrs-firmware: ELF 32-bit LSB executable, Tensilica Xtensa, version 1 (SYSV), statically linked, stripped
```

## Sandboxed Build (RESOLVED)

`nix build .#rust-firmware` now works fully sandboxed. The `cargoLock` FOD:
1. Runs `cargo build` with network access (`--option sandbox false` for the FOD only)
2. Captures the entire `~/.cargo/registry/` cache (includes `build-std` internal deps like `hashbrown` that aren't in the project's `Cargo.lock`)
3. Outputs the registry cache + generated `Cargo.lock`

The main `rust-firmware` derivation then:
- Copies the registry cache to `CARGO_HOME`
- Uses `cargo build --frozen` (no network, lockfile-as-is)

This handles the subtlety that `build-std` resolves std library dependencies separately from the project's `Cargo.lock` — the vendored crates must include both project deps and std's internal deps.

## Working Configuration Files

- `build.rs` — root build script calling `embuild::espidf::sysenv::output()`
- `Cargo.toml` — embuild 0.33 as `[build-dependencies]`
- `.cargo/config.toml` — target, ldproxy linker, build-std, sdkconfig defaults
- `src/main.rs` — minimal blink + println firmware
- `flake.nix` — single-pass build, cargoLock FOD, espRustToolchain in devenv shell
- `sdkconfig.rust.defaults` — override partition table for Rust build
