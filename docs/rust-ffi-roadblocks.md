# FFI Deep-Dive: The Two Roadblock Libraries

> **Companion to:** `docs/rust-migration-feasibility.md`
> **Question:** What is FFI, and how does it help with the DCC (NmraDcc) and Audio (libhelix + arduino-audio-tools) roadblocks?

---

## TL;DR

After reading the actual headers, the FFI story is **much better than the feasibility report assumed**:

| Library                          | Language    | ABI                                                                                       | FFI difficulty                                                                                                | Why |
| -------------------------------- | ----------- | ----------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- | --- |
| **libhelix-mp3** (audio decoder) | **Pure C**  | `extern "C"`, 6 functions, opaque `void*` handle, no callbacks                            | **Trivial** — `bindgen` auto-generates bindings; ~30 lines of safe Rust wrapper                               |
| **NmraDcc** (DCC protocol)       | C++ _class_ | Methods are name-mangled C++… **but the callbacks are already `extern "C"` weak symbols** | **Easy** — the hard direction (C++→Rust) is _free_; only the method calls (Rust→C++) need a ~40-line C++ shim |

The "roadblocks" are real (no pure-Rust equivalent exists), but FFI lets you **keep the working C/C++ code and drive it from Rust** — you don't have to rewrite either library to migrate the firmware.

---

## 1. What FFI Is

**FFI** = _Foreign Function Interface_. It's the mechanism for code in one language to call code written in another language. On ESP32 (and most platforms) the **C ABI (Application Binary Interface)** is the lingua franca: almost every compiled language can produce and consume C-ABI functions. Rust is no exception.

There are two directions, and both matter for this project:

### Direction A — Rust calls C/C++ (Rust → foreign)

```rust
// Rust declares the foreign function's signature and calls it (in an unsafe block)
extern "C" {
    fn MP3Decode(handle: *mut core::ffi::c_void, inbuf: *mut *mut u8,
                 bytesLeft: *mut i32, outbuf: *mut i16, useSize: i32) -> i32;
}

// At the call site:
unsafe {
    let ret = MP3Decode(handle, &mut inptr, &mut bytes_left, out_buf.as_mut_ptr(), 0);
}
```

The linker resolves `MP3Decode` to the symbol in the compiled libhelix `.a`. The `extern "C"` tells Rust to use the C calling convention (no name mangling, known argument registers).

### Direction B — C/C++ calls Rust (foreign → Rust)

```rust
// Rust EXPORTS a function with a stable C ABI that C/C++ can link against
#[no_mangle]
pub extern "C" fn notifyDccSpeed(addr: u16, addr_type: u8, speed: u8, dir: u8, _steps: u8) {
    // Rust code runs here, called from the C++ library's interrupt-driven decoder
}
```

`#[no_mangle]` prevents Rust from renaming the symbol; `extern "C"` makes it C-ABI callable. C/C++ sees it as an ordinary `extern void notifyDccSpeed(...)` declaration.

### Why `unsafe`?

Rust's safety guarantees rely on the compiler checking borrows, lifetimes, and nullability. None of that exists across the FFI boundary — the C side could hand Rust a wild pointer, a freed buffer, or call a callback re-entrantly. So every call _into_ foreign code, and every dereference of a foreign pointer, must be inside `unsafe`. The standard pattern is a **thin `unsafe` FFI layer + a safe wrapper** that enforces invariants (non-null handles, valid lengths, mutex around shared state).

### How `bindgen` automates Direction A

Writing `extern "C"` blocks by hand is tedious and error-prone for large APIs. `bindgen` reads C headers and emits the Rust `extern` blocks automatically. This is exactly what `vendor/esp-idf-sys/build/build.rs` does for the entire ESP-IDF SDK — it runs `bindgen` over `bindings.h` and `#include`s the whole ESP-IDF, producing one giant Rust module of FFI declarations. The same machinery can run over `mp3dec.h` and a DCC shim header.

---

## 2. Roadblock #1: Audio (libhelix) — the _easy_ FFI case

### What the header actually looks like

`managed_components/chmorgan__esp-libhelix-mp3/libhelix-mp3/pub/mp3dec.h` is a **textbook C API**:

```c
#ifdef __cplusplus
extern "C" {            // <-- already C-ABI callable from C++ too
#endif

typedef void *HMP3Decoder;                                   // opaque handle

typedef struct _MP3FrameInfo { int bitrate, nChans, samprate, ...; } MP3FrameInfo;

HMP3Decoder MP3InitDecoder(void);                            // construct
void        MP3FreeDecoder(HMP3Decoder h);                   // destruct
int         MP3Decode(HMP3Decoder h, unsigned char **inbuf,
                      int *bytesLeft, short *outbuf, int useSize);   // decode one frame
void        MP3GetLastFrameInfo(HMP3Decoder h, MP3FrameInfo *info);
int         MP3FindSyncWord(unsigned char *buf, int nBytes);

#ifdef __cplusplus
}
#endif
```

Six functions. Opaque handle (`void*`). No callbacks, no classes, no virtual dispatch, no `new`/`delete`. The whole API is `extern "C"`.

### How FFI helps: a ~30-line safe Rust wrapper

`esp-idf-sys` can compile libhelix as an `extra_component` and run `bindgen` over `mp3dec.h` to emit:

```rust
// auto-generated by bindgen (lives in esp_idf_sys::mp3 module)
extern "C" {
    pub fn MP3InitDecoder() -> *mut core::ffi::c_void;
    pub fn MP3FreeDecoder(h: *mut core::ffi::c_void);
    pub fn MP3Decode(h: *mut core::ffi::c_void, inbuf: *mut *mut u8,
                     bytesLeft: *mut i32, outbuf: *mut i16, useSize: i32) -> i32;
    pub fn MP3FindSyncWord(buf: *mut u8, nBytes: i32) -> i32;
    // ...
}
```

Then a hand-written safe wrapper:

```rust
pub struct Mp3Decoder { handle: *mut core::ffi::c_void }

impl Mp3Decoder {
    pub fn new() -> Result<Self, i32> {
        let h = unsafe { esp_idf_sys::mp3::MP3InitDecoder() };
        if h.is_null() { return Err(-9999); }
        Ok(Self { handle: h })
    }

    pub fn decode(&mut self, in_buf: &[u8], out_buf: &mut [i16]) -> Result<usize, i32> {
        let mut in_ptr = in_buf.as_ptr() as *mut u8;
        let mut bytes_left = in_buf.len() as i32;
        let ret = unsafe {
            esp_idf_sys::mp3::MP3Decode(self.handle, &mut in_ptr, &mut bytes_left,
                                        out_buf.as_mut_ptr(), 0)
        };
        if ret != 0 { return Err(ret); }
        let consumed = in_buf.len() - bytes_left as usize;
        Ok(consumed)
    }
}

impl Drop for Mp3Decoder {
    fn drop(&mut self) { unsafe { esp_idf_sys::mp3::MP3FreeDecoder(self.handle); } }
}
```

The `Drop` impl guarantees `MP3FreeDecoder` runs — no leaks even on panic. The `is_null` check turns a C "returned NULL on failure" into a `Result`. Callers never touch `unsafe`.

### What this means for the migration

You **don't port the MP3 codec**. libhelix stays as a C component, compiled by ESP-IDF's C toolchain exactly as it is today (the same `managed_components` FOD already pins it). Rust just calls the 6 functions. The only Rust you write is the audio _pipeline_ around it — I2S output, volume scaling, file reading from LittleFS, WAV-vs-MP3 dispatch. That's the ~300–400 lines of pure Rust identified in the feasibility report.

**The C++ `arduino-audio-tools` framework is NOT needed via FFI.** That framework (with `I2SStream`, `VolumeStream`, `EncodedAudioStream`, virtual `AudioDecoder` classes) would be painful to drive from Rust because it's a C++ class hierarchy with virtual dispatch. The good news: you never needed the _framework_, you needed _libhelix_. And libhelix is plain C. So the plan is: drop `arduino-audio-tools` entirely, keep libhelix via FFI, rebuild the pipeline in idiomatic Rust.

---

## 3. Roadblock #2: DCC (NmraDcc) — the _interesting_ FFI case

NmraDcc is a C++ _class_ (`class NmraDcc { void init(...); uint8_t process(); uint8_t getCV(uint16_t); ... }`). C++ mangles symbol names and ties methods to a `this` pointer, so you can't `extern "C"` a class method directly. **But look at what the header actually does for the callbacks:**

```c
// from NmraDcc.h, lines 430-476 (verbatim)
#if defined (__cplusplus)
extern "C" {
#endif

extern void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed,
                           DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps)
    __attribute__((weak));

extern void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp,
                          uint8_t FuncState)
    __attribute__((weak));

extern uint8_t notifyCVWrite(uint16_t CV, uint8_t Value) __attribute__((weak));
extern void    notifyCVAck(void) __attribute__((weak));
extern void    notifyCVResetFactoryDefault(void) __attribute__((weak));
// ... ~15 more notify* callbacks, all weak, all extern "C"

#if defined (__cplusplus)
}
#endif
```

Two crucial attributes:

1. **`extern "C"`** — these are plain C-ABI symbols, _not_ mangled. Rust can define them with `#[no_mangle] extern "C"` and the linker will see them.
2. **`__attribute__((weak))`** — the library declares them but leaves them _undefined_. If you provide a definition, the linker uses yours. If you don't, the library's stubs (empty functions) are used.

This is the **callback-via-weak-linking** pattern, and it's the _hard_ direction (C++ calling into your code from an interrupt context) made trivial. The existing C++ firmware already exploits this — `DccController.cpp` defines `notifyDccSpeed`, `notifyDccFunc`, etc. as free functions. Rust can do the _exact same thing_.

### How FFI helps — the Rust side of the callbacks

```rust
// crate::dcc::callbacks.rs
//
// These replace the C++ free functions in DccController.cpp.
// The linker resolves NmraDcc's weak `notifyDccSpeed` symbol to THIS function.

use crate::ctx::{SystemContext, SystemState};
use std::sync::Mutex;

// A global the C callbacks can reach (mirrors the current singleton pattern).
// Mutex because the DCC interrupt can fire concurrently with the control loop.
static CTX: once_cell::sync::Lazy<Mutex<SystemContext>> =
    once_cell::sync::Lazy::new(|| Mutex::new(SystemContext::new()));

#[no_mangle]
pub extern "C" fn notifyDccSpeed(
    addr: u16,
    _addr_type: u8,
    speed: u8,
    dir: u8,
    _steps: u8,
) {
    let direction = dir == 1; // DCC_DIR_FWD
    let target = if speed > 1 { speed } else { 0 };
    if let Ok(mut ctx) = CTX.lock() {
        ctx.state.speed = target;
        ctx.state.direction = direction;
        ctx.state.last_dcc_packet_ms = current_millis();
    }
}

#[no_mangle]
pub extern "C" fn notifyCVWrite(cv: u16, value: u8) -> u8 {
    // Persist to NVS, handle CV8 factory reset, SuperCap pin, etc.
    // (port of notifyCVWrite in DccController.cpp)
    crate::cv::write(cv, value)
}
```

That's the entire C++→Rust bridge for the callback direction. No shim, no wrapper, no bindgen needed — the symbols match by name.

### The Rust→C++ direction: a thin C++ shim

The class methods (`init`, `process`, `getCV`, `setCV`) _are_ name-mangled and need a `this` pointer. You can't call them directly from Rust. The fix is a ~40-line C++ shim that exposes them as `extern "C"` free functions:

```cpp
// dcc_shim.cpp — the ONLY C++ file that stays in the Rust project
#include <NmraDcc.h>

static NmraDcc dcc;   // singleton, mirrors the current `_dcc` member

extern "C" void dcc_pin(uint8_t pin)           { dcc.pin(pin, 1); }
extern "C" void dcc_init(uint8_t mfr, uint8_t ver,
                         uint8_t flags, uint8_t opsBase) {
    dcc.init(mfr, ver, flags, opsBase);
}
extern "C" uint8_t dcc_process()               { return dcc.process(); }
extern "C" uint8_t dcc_get_cv(uint16_t cv)     { return dcc.getCV(cv); }
extern "C" uint8_t dcc_set_cv(uint16_t cv, uint8_t v) { return dcc.setCV(cv, v); }
extern "C" uint16_t dcc_get_addr()             { return dcc.getAddr(); }
```

Then a `bindings.h`:

```c
// bindings.h — fed to esp-idf-sys bindgen via extra_components
#pragma once
#include <stdint.h>
void dcc_pin(uint8_t pin);
void dcc_init(uint8_t mfr, uint8_t ver, uint8_t flags, uint8_t opsBase);
uint8_t dcc_process(void);
uint8_t dcc_get_cv(uint16_t cv);
uint8_t dcc_set_cv(uint16_t cv, uint8_t v);
uint16_t dcc_get_addr(void);
```

`bindgen` generates the Rust `extern` block automatically; the Rust `DccController` becomes:

```rust
pub struct DccController;
impl DccController {
    pub fn setup(&self, pin: u8) {
        unsafe {
            esp_idf_sys::dcc::dcc_pin(pin);
            esp_idf_sys::dcc::dcc_init(MAN_ID_DIY, 10, 0x02, 0);
        }
    }
    pub fn loop_step(&self) {
        unsafe { esp_idf_sys::dcc::dcc_process(); }
    }
    pub fn get_cv(&self, cv: u16) -> u8 {
        unsafe { esp_idf_sys::dcc::dcc_get_cv(cv) }
    }
}
```

### Wiring it into `esp-idf-sys`

In the root `Cargo.toml`:

```toml
[[package.metadata.esp-idf-sys.extra_components]]
component_dirs = ["vendor/nmradcc"]        # the shim + NmraDcc source
bindings_header = "vendor/nmradcc/bindings.h"
bindings_module = "dcc"                    # generates esp_idf_sys::dcc
```

`esp-idf-sys`'s build script compiles NmraDcc + the shim with the ESP-IDF C/C++ toolchain (the same Xtensa GCC the Nix `esp-dev` flake provides), runs `bindgen` over `bindings.h`, and emits the `esp_idf_sys::dcc` module. The weak callback symbols are resolved at link time to the Rust `#[no_mangle]` functions above.

### What this means for the migration

- **No rewrite of the DCC bit-decode logic.** NmraDcc keeps doing the hard real-time interrupt work in C++.
- **All _policy_ moves to Rust.** Speed/function state, CV persistence, SuperCap control, factory reset — all the `notify*` callbacks you already wrote in `DccController.cpp` become Rust functions, with `std::sync::Mutex` instead of `ScopedLock`.
- **The shim is ~40 lines of C++ and never changes.** It's a pure pass-through. If you later port NmraDcc itself to Rust (Phase 6, optional), you delete the shim and the `extra_component` — nothing else in the Rust tree changes.
- **`EEPROM` → NVS.** The one behavioral change: NmraDcc's `getCV`/`setCV` currently hit Arduino `EEPROM`. In Rust you'd back the CV store with `esp-idf-svc::nvs::Nvs` and either keep NmraDcc's EEPROM (it works) or override `notifyCVWrite`/`notifyCVRead` to redirect to NVS (cleaner). This is the same decision the C++ codebase already faces.

---

## 4. The Asymmetry, Summarized

```
                Rust ──calls──►  C/C++          C/C++ ──calls──►  Rust
                (Direction A)                   (Direction B)

libhelix        bindgen over mp3dec.h            (none — no callbacks)
(audio)         6 extern "C" functions
                TRIVIAL

NmraDcc         ~40-line C++ shim exposes        weak extern "C" symbols
(DCC)           init/process/getCV as            → Rust #[no_mangle] fns
                extern "C"; bindgen over         FREE (no shim needed)
                bindings.h
                EASY
```

The direction that's usually hard (foreign code calling _into_ your language, especially from an interrupt) is **free for DCC** because the library author already chose the `extern "C"` weak-symbol pattern. The direction that's usually easy (calling a foreign function) is **trivial for audio** because libhelix is plain C. So the two "roadblocks" are, in FFI terms, about as cheap as foreign-code interop ever gets.

---

## 5. What FFI Does _Not_ Solve

FFI is a bridge, not a port. Be aware of:

1. **Two toolchains, one linker.** The C/C++ components compile with Xtensa GCC (from `esp-dev`); Rust compiles with the Xtensa Rust fork (from `espup`). Both produce Xtensa object code; the ESP-IDF CMake build (driven by `esp-idf-sys`) links them together. This works today — `esp-idf-sys` explicitly supports mixed Rust/C projects — but it's one more thing the Nix shell must provide (see feasibility report §5.3).

2. **`unsafe` is real.** The safe wrappers above hide it, but if you get a `void*` wrong you get memory unsafety, same as C. The wrappers must enforce: non-null handles, valid buffer lengths, no re-entrant mutation, `Send`/`Sync` boundaries for the global mutex.

3. **Interrupt-context callbacks are constrained.** `notifyDccSpeed` runs from NmraDcc's GPIO ISR (well, actually from `process()` on the loop task in the ESP32 variant — but the same discipline applies: no heap allocation, no long locks, no I/O). The Rust `#[no_mangle]` callbacks must be minimal: lock, copy, unlock. Defer everything else to the control-loop task.

4. **Debugging crosses languages.** GDB/`idf.py monitor` sees both Rust and C++ frames fine (DWARF is shared), but panics in Rust present differently from C++ exceptions. Worth setting up `esp-idf-svc::log` early so both sides log to the same UART.

5. **You still own the C/C++ code's correctness.** FFI doesn't make NmraDcc or libhelix Rust-safe; it just lets you call them. Bugs in the C/C++ libraries are still your bugs.

---

## 6. Recommendation (unchanged, now better-justified)

- **Audio:** keep libhelix as a C `extra_component`, bind with `bindgen`, write the I2S/Volume/WAV pipeline in Rust. Drop `arduino-audio-tools` entirely. (~300 lines Rust + 30-line FFI wrapper.)
- **DCC:** keep NmraDcc as a C++ `extra_component` with a ~40-line `extern "C"` shim. Define the `notify*` callbacks as Rust `#[no_mangle] extern "C"` functions that own the `SystemContext` mutex. (~150 lines Rust porting the callback bodies + 40-line C++ shim.)
- **Optional Phase 6:** port NmraDcc itself to Rust once everything else is stable. The shim makes this a clean swap — delete the component, delete the shim, the Rust `DccController` keeps its interface.

Both roadblocks are FFI-bridgeable with **less than 100 lines of C/C++** total remaining in the tree, and the bulk of _your_ logic (callbacks, CV policy, audio pipeline, state) lives in Rust with real tests.
