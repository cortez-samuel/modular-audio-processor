#include "../libraries/I2S_Tx_naive.pio.h"
#include "../libraries/I2S_Tx_compact.pio.h"

#include "hardware/pio.h"

#include "../libraries/I2S.h"

#include "pico/stdlib.h"
#include <cstdio>


int main() {

    stdio_init_all();
    
    gpio_init(13);
    gpio_set_dir(13, GPIO_OUT);
    gpio_put(13, 1);

    sleep_ms(5000);
    printf("1\n");

    static const uint8_t width = 16;
    static const uint8_t depth = 128;
    uint32_t reservedMem[width * depth];
    uint32_t defaultData[depth];
    for (int i = 0; i < depth; i++) { defaultData[i] = 0x12345678; }

    printf("2\n");

    I2S_Tx i2sTx(reservedMem, width, depth);
    i2sTx.setDefaultData(defaultData);
    printf("3\n");
    i2sTx.init(2, 3, 1, 1000, 16);
    printf("4\n");
    i2sTx.enable(true);
    printf("5\n");

    bool pin13 = 1;
    while (1) {
        tight_loop_contents();
    }
}