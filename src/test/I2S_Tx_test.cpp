#include "../lib/I2S_Tx_naive.pio.h"
#include "../lib/I2S_Tx_compact.pio.h"

#include "hardware/pio.h"

#include "../lib/I2S.h"

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
    static const uint32_t depth = 32;
    uint32_t reservedMem[width * depth];
    uint32_t defaultDataSpace[depth];
    uint32_t defaultData[depth];
    for (int i = 0; i < depth; i++) { defaultData[i] = 0x10001111; }

    printf("2\n");

    //TxPingPong txPingPong(reservedMem, defaultData, width, depth);

    printf("-----\n");

    I2S_Tx i2sTx;
    i2sTx.settings = {
        .i2sSettings    = I2S_Tx::defaultSettings.i2sSettings,
        .bufferWidth    = width,
        .bufferDepth    = depth,
        .reservedMem    = reservedMem,
        .defaultMem     = defaultData,
    };
    i2sTx.settings.i2sSettings.fs           = 1000;
    i2sTx.settings.i2sSettings.frameSize    = 16;
    printf("3\n");
    i2sTx.init(2, 3, 1);
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

    uint32_t buff1[depth], buff2[depth];

    for (uint i = 0; i < depth; i++) {
        buff1[i] = i;
    }

    bool pin13 = 1;
    while (1) {
        i2sTx.queueBuffer(buff1);
        //i2sTx.queueBuffer(buff2);
        //sleep_us(1000);
    }
}
