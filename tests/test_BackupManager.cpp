// clang-format off
// TEST_SOURCES: src/BackupManager.cpp src/DccController.cpp tests/mocks/mocks.cpp
// TEST_FLAGS: -DSKIP_MOCK_BACKUP_MANAGER -DSKIP_MOCK_DCC_CONTROLLER
// clang-format on

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm> // for std::min

#include "Arduino.h"
#include "WebServer.h"
#include "Preferences.h"
#include "LittleFS.h"
#include "NmraDcc.h"
#include "DccController.h"

// Mock Print for capturing output
class MockPrint : public Print {
public:
    std::string buffer;
    size_t write(uint8_t c) override { buffer += (char)c; return 1; }
    size_t write(const uint8_t *buf, size_t size) override {
        buffer.append((const char*)buf, size);
        return size;
    }
};

#include "../src/BackupManager.h"

// Helper to reset state
void resetState() {
    Preferences::storage.clear();
    File::writtenFiles.clear();
    File::fileContent.clear();
    LittleFS.mockFiles.clear();
}

#define TEST_CASE(name) void name()
#define RUN_TEST(name)                                                         \
  std::cout << "Running " << #name << "... ";                                  \
  name();                                                                      \
  std::cout << "PASSED" << std::endl;


TEST_CASE(test_generate_backup) {
    resetState();

    // Setup Prefs
    Preferences::storage["hostname"] = "TestHost";
    Preferences::storage["web_user"] = "user";
    Preferences::storage["web_pass"] = "pass";

    // Setup LittleFS file
    File::fileContent["test.txt"] = "Hello World";
    LittleFS.mockFiles.push_back(File("test.txt", 11));

    MockPrint output;
    BackupManager::getInstance().generateBackup(&output);

    // Verify
    // 1. Check for tar magic "ustar"
    assert(output.buffer.find("ustar") != std::string::npos);

    // 2. Check for config.json header
    assert(output.buffer.find("config.json") != std::string::npos);

    // 3. Check for config content
    assert(output.buffer.find("\"hostname\":\"TestHost\"") != std::string::npos);

    // 4. Check for test.txt header
    assert(output.buffer.find("test.txt") != std::string::npos);

    // 5. Check for test.txt content
    assert(output.buffer.find("Hello World") != std::string::npos);
}

TEST_CASE(test_restore_backup) {
    resetState();

    // Construct a tar file in memory using generateBackup
    Preferences::storage["hostname"] = "RestoreHost";
    File::fileContent["restore.txt"] = "Restored Content";
    LittleFS.mockFiles.push_back(File("restore.txt", 16));

    MockPrint output;
    BackupManager::getInstance().generateBackup(&output);

    // Clear state
    Preferences::storage.clear();
    File::writtenFiles.clear();
    LittleFS.mockFiles.clear(); // Clear source files

    // Restore
    BackupManager::getInstance().beginRestore();

    // Feed in chunks
    const uint8_t* data = (const uint8_t*)output.buffer.data();
    size_t len = output.buffer.size();
    size_t chunkSize = 100;

    for (size_t i = 0; i < len; i += chunkSize) {
        size_t n = std::min(chunkSize, len - i);
        BackupManager::getInstance().writeChunk((uint8_t*)(data + i), n);
    }

    String err;
    bool success = BackupManager::getInstance().endRestore(err);

    if (!success) {
        std::cout << "Restore Failed: " << err.c_str() << std::endl;
    }
    assert(success);

    // Verify Preferences
    assert(std::string(Preferences::storage["hostname"].c_str()) == "RestoreHost");

    // Verify Files
    // Note: BackupManager writes to "/" + filename.
    // Check File::writtenFiles
    assert(File::writtenFiles.count("/restore.txt") > 0);
    assert(File::writtenFiles["/restore.txt"] == "Restored Content");
}

int main() {
    RUN_TEST(test_generate_backup);
    RUN_TEST(test_restore_backup);
    return 0;
}
