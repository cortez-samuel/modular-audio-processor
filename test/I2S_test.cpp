#include "../libraries/I2S_Tx_naive.pio.h"
#include "../libraries/I2S_Tx_compact.pio.h"

#include "hardware/pio.h"

#include "pico/stdlib.h"
#include <cstdio>

int main() {

    stdio_init_all();


    PIO pio;
    uint sm;
    uint offset;

    pio_claim_free_sm_and_add_program(&I2S_compact_program, &pio, &sm, &offset);
    I2S_compact_init(pio, sm, offset, 2, 3, 1, 1000, 12);

    while (1) {
        I2S_compact_stereo_write(pio, sm, 0xFF0, 0xAAA, 12); // check transmitting correctly w oscilloscope
    }
}