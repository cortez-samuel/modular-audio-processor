#include "../libraries/I2S_Tx_naive.pio.h"
#include "../libraries/I2S_Tx_compact.pio.h"

#include "hardware/pio.h"

#include "../libraries/I2S.h"

#include "pico/stdlib.h"
#include <cstdio>


int main() {

    stdio_init_all();
    
    I2S_Tx i2sTx;
    i2sTx.init(2, 3, 0, 50000, 12);
    i2sTx.enable(true);

    bool pin13 = 1;
    while (1) {
        //i2sTx.queue(0x111, 0x333);
        i2sTx.queue(0xAAA, 0xFF0);
        sleep_us(500);
        i2sTx.queue(0x111, 0x333);
        sleep_us(500);
    }
}