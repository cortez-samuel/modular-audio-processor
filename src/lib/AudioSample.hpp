#ifndef AUDIO_SAMPLE_HPP_included
#define AUDIO_SAMPLE_HPP_included

#include "pico/stdlib.h"

struct AudioSample_t {
    using _T = uint32_t;

    uint32_t LC;
    uint32_t RC;
};

#endif