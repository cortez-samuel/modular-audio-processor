#include "../libraries/I2S_Tx_naive.pio.h"
#include "../libraries/I2S_Tx_compact.pio.h"

#include "hardware/pio.h"

#include "../libraries/I2S.hpp"

#include "pico/stdlib.h"
#include <cstdio>


int main() {

    stdio_init_all();
    
    I2S_Tx i2s(&I2S_Tx_naive_program);
    i2s.init(2, 3, 1, 1000, 12);
    i2s.enable(true);
    //I2S_Tx_compact_init(pio, sm, offset, 2, 3, 1, 1000, 12);

    bool pin13 = 1;
    while (1) {
        //I2S_Tx_naive_stereo_write(i2s.pio, i2s.sm, 0xFF0, 0xAAA, 12); // check transmitting correctly w oscilloscope
        i2s.queue(0x111, 0x333);
        sleep_ms(2);
        i2s.queue(0xAAA, 0xFF0);
        sleep_ms(5);

        printf(
            "----------\n "\
            "[%3x, %3x, %3x, %3x, %3x, %3x, %3x, %3x]\n",
        i2s.getData(0), i2s.getData(1), i2s.getData(2), i2s.getData(3),
        i2s.getData(4), i2s.getData(5), i2s.getData(6), i2s.getData(7)
        );
    }
}