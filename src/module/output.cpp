#include "pico/stdlib.h"

#include "../lib/I2S.h"

#include <cstdio>


int main() {

    stdio_init_all();

    static I2S_Rx i2sRx;
    static const uint32_t reservedMemDepth = 128;
    static AudioSample_t reservedMem[reservedMemDepth * I2S_Rx::BUFFER_WIDTH];
    i2sRx.settings = {
        .i2sSettings    = I2S_Rx::defaultSettings.i2sSettings,
        .bufferDepth    = reservedMemDepth,
        .reservedMem    = reservedMem,
    };
    i2sRx.init(8, 9, 7);
    i2sRx.enable(true);

    AudioSample_t sample;
    while(1) {
        if (i2sRx.read(sample)) {
            printf("0x%02x | 0x%02x\n", sample.LC, sample.RC);
        }
    }
}
