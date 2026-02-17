#include "pico/stdlib.h"

#include "../libraries/I2S.h"

#include <cstdio>

uint pins[8] {6,0,1,2,3,10,11,12};

I2S_Rx i2sRx;
static const uint32_t reservedMemDepth = 128;
uint32_t reservedMem[reservedMemDepth * I2S_Rx::BUFFER_WIDTH];

void gpioWrite(uint8_t D) {
    for (int i = 0; i < 8; i++) {
        gpio_put(pins[i], D & 1);
        D = D >> 1;
    }
}

int main() {

    stdio_init_all();

    for (int i = 0; i < 8; i++) {
        gpio_set_dir(pins[i], true);
    }

    i2sRx.setReservedMem(reservedMem, reservedMemDepth);
    i2sRx.init(8, 9, 7, 2000, 12);
    i2sRx.enable(1);

    uint32_t LC = 0, RC = 0;
    while(1) {
        if (i2sRx.read(LC, RC)) {
            gpioWrite(LC >> (12 - 5));
            printf("%02x\n", LC);
        }
    }
}