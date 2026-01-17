#include "../libraries/I2S_naive.pio.h"

#include "hardware/pio.h"

#include "pico/stdlib.h"
#include <cstdio>

int main() {

    stdio_init_all();


    PIO pio;
    uint sm;
    uint offset;

    pio_claim_free_sm_and_add_program(&I2S_naive_program, &pio, &sm, &offset);
    I2S_naive_init(pio, sm, offset, 2, 3, 1, 1000, 12);

    while (1) {
        I2S_naive_stereo_write(pio, sm, 0xFF000000, 0xAAA00000); // check transmitting correctly w oscilloscope
    }
}