#pragma once

#include "audio_base.h"

namespace drivers {

class fbdev_audio: public audio_base {
public:
    using audio_base::audio_base;

    void reset() override {}

protected:
    bool open(unsigned) override { return true; }
    void close() override {}
    void on_input(const int16_t *, size_t) override {}

public:
    void pause(bool) override {}
};

}
