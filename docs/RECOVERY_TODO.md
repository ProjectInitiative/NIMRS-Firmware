# NIMRS Firmware Recovery Roadmap

The repository history was rewritten to remove large binaries, which orphaned several active Pull Requests. This document serves as a "Todo List" to re-implement those features and fixes on the current clean history.

---

## 🚨 Category 1: Security (Highest Priority)

### 1. Fix Buffer Overflows

- **Source Branch**: `security/fix-buffer-overflow-sprintf`
- **Task**: Replace all instances of `sprintf` with `snprintf` in `ConnectivityManager.cpp` and `MotorController.cpp`.
- **Goal**: Prevent potential memory corruption during string generation for API responses and telemetry.

### 2. Mask WiFi Credentials

- **Source Branch**: `security-fix-wifi-cleartext`
- **Task**: Update `ConnectivityManager` handlers to ensure that WiFi passwords are never logged or returned in plain text via the status API.

---

## ⚙️ Category 2: CI & Infrastructure

### 3. Firmware Size Guard

- **Source Branch**: `ci/firmware-size-check`
- **Task**: Add a check to `ci-ready` or a standalone script to verify that `build/NIMRS-Firmware.ino.bin` does not exceed the 2MB partition limit.

### 4. Add Logger Unit Tests

- **Source Branch**: `add-logger-tests`
- **Task**: Create `tests/test_Logger.cpp`. Test log level filtering, ring-buffer rotation, and JSON output formatting.

---

## ⚡ Category 3: Performance Optimizations

### 5. Optimize Motor JSON Generation

- **Source Branch**: `perf/optimize-motor-json-generation`
- **Task**: Refactor `MotorController::getTestJSON` to use a more efficient serialization method (like `ArduinoJson` with a pre-allocated buffer) instead of multiple `sprintf` calls.

### 6. JSON Helper Refactor

- **Source Branch**: `refactor/send-json-helper`
- **Task**: Create a unified `sendJsonResponse(JsonDocument& doc)` method in `ConnectivityManager` to standardize API responses and reduce code size.

### 7. Optimize CV String Conversion

- **Source Branch**: `perf/optimize-cv-update-string-conversion`
- **Task**: Refactor the Bulk CV update logic in `handleCvAll` to avoid heavy `String` objects when parsing JSON keys.

---

## 🧪 Category 4: Component Testing

### 8. DCC Controller Tests

- **Source Branch**: `test-dcc-controller`
- **Task**: Implement unit tests for `DccController` to verify packet parsing for 14, 28, and 128 speed steps.

### 9. Audio Controller Tests

- **Source Branch**: `test-audiocontroller`
- **Task**: Create tests for the sound asset engine to verify JSON config loading and file-to-function mapping.

### 10. Motor Controller Tests

- **Source Branch**: `test-motor-controller`
- **Task**: Implement tests for the PI loop and Dither logic using the hardware mocks provided in the test environment.
