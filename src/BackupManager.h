#ifndef BACKUP_MANAGER_H
#define BACKUP_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <vector>

class BackupManager {
public:
    static BackupManager& getInstance();

    // Stream backup tarball to output
    void generateBackup(Print* output);

    // Process a chunk of uploaded tarball
    void beginRestore();
    void writeChunk(uint8_t* data, size_t len);
    bool endRestore(String& errorMsg);

private:
    BackupManager();

    // Tar Helpers
    void writeTarHeader(Print* out, const String& filename, size_t size);
    void writePadding(Print* out, size_t size);

    // Config JSON Helper
    String createConfigJson();
    bool applyConfigJson(const String& json, String& errorMsg);

    // Restore State
    enum State {
        IDLE,
        READ_HEADER,
        READ_CONTENT,
        IGNORE_PADDING
    };

    State _state;
    uint8_t _headerBuf[512];
    size_t _headerPos;

    size_t _bytesNeeded;
    size_t _currentFileSize;
    String _currentFileName;
    File _currentFile;
    String _configJsonBuffer;
    bool _isConfigJson;
};

#endif // BACKUP_MANAGER_H
