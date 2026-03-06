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

    static const uint8_t width = 8;
    static const uint32_t depth = 256;
    uint32_t reservedMem[width * depth];
    uint32_t defaultData[depth];
    for (int i = 0; i < depth; i++) { defaultData[i] = 0x10000000; }

    printf("2\n");

    //TxPingPong txPingPong(reservedMem, defaultData, width, depth);

    printf("-----\n");

    
    I2S_Tx i2sTx(reservedMem, width, depth);
    i2sTx.setDefaultData(defaultData);
    printf("3\n");
    i2sTx.init(2, 3, 1, 5000, 16);
    printf("4\n");
    i2sTx.enable(true);
    printf("5\n");

    /*
    for (uint i = 0; i < depth; i++) {
        printf("--- %u ---\n", i);
        txPingPong._printdetails();
        txPingPong.queueBuffer(defaultData);
        txPingPong._debugIRQ();
    }
    printf("--- %u ---\n", depth);
    txPingPong._printdetails();
    */

    bool pin13 = 1;
    while (1) {
        //i2sTx.queue(0x11223344, 0x55667788);
        tight_loop_contents();
    }
}