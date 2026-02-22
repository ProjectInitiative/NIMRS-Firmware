#include "BackupManager.h"

// Mock Implementation for BackupManager

BackupManager &BackupManager::getInstance() {
  static BackupManager instance;
  return instance;
}

BackupManager::BackupManager() : _state(IDLE) {}

void BackupManager::generateBackup(Print *output) {
  // Stub
}

void BackupManager::beginRestore() {
  // Stub
}

void BackupManager::writeChunk(uint8_t *data, size_t len) {
  // Stub
}

bool BackupManager::endRestore(String &errorMsg) {
  // Stub
  return true;
}
