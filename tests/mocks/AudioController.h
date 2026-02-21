#ifndef AUDIOCONTROLLER_MOCK_H
#define AUDIOCONTROLLER_MOCK_H

#include "Arduino.h"

class AudioController {
public:
    static AudioController &getInstance();
    AudioController();
    void loadAssets();
    void playFile(const char* file);
};

#endif
