#ifndef AUDIO_UTILS_H
#define AUDIO_UTILS_H

#include <cstring>
#include <strings.h>

/**
 * @brief Checks if the given filename has an .mp3 extension (case-insensitive).
 *
 * Optimized to avoid String allocation and use strcasecmp directly on the
 * suffix.
 *
 * @param filename The filename to check.
 * @return true if the filename ends with .mp3, false otherwise.
 */
inline bool isMp3File(const char *filename) {
  if (!filename)
    return false;
  size_t len = strlen(filename);
  return (len >= 4 && strcasecmp(filename + len - 4, ".mp3") == 0);
}

#endif // AUDIO_UTILS_H
