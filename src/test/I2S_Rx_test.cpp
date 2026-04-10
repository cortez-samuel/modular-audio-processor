#include "../lib/I2S_Rx_naive.pio.h"

#define I2S_RX_PROGRAM I2S_RX_PROGRAM__AUTOFRAME

#include "hardware/pio.h"
#include "hardware/irq.h"

#include "../lib/I2S.h"
#include "../lib/RxPingPong.h"

#include "pico/stdlib.h"

#include "hardware/sync.h"

#include <cstdio>


uint pin13 = 13;


int main() {

    stdio_init_all();

    gpio_init(pin13);
    gpio_set_dir(pin13, GPIO_OUT);

    static const uint8_t depth = 8;
    AudioSample_t reserved[RxPingPong::WIDTH * depth];
    I2S_Rx i2sRx;
    i2sRx.settings = {
        .i2sSettings = I2S_Rx::defaultSettings.i2sSettings,
        .bufferDepth = depth,
        .reservedMem = reserved,
    };
    i2sRx.settings.i2sSettings.frameSize = 16;
    

    i2sRx.init(8, 9, 7);

    sleep_ms(5000);
    stdio_printf("START\n");
    stdio_printf("%u\n", clock_get_hz(clk_sys));

    /*  
    dma_channel_start(ch1);
    pio_sm_set_enabled(pio, sm, true);
    */

    //pio_sm_set_enabled(pio, sm, true);

    i2sRx.enable(true);


    AudioSample_t buff[depth];
    while(1) {
        //stdio_printf("%u\n", i++);
        //bool valid = ppb.read(buff);
        //I2S_Rx_naive_read(i2sRx.pio, i2sRx.sm, &LC, &RC);
        //bool valid = true;
        bool valid = i2sRx.readBuffer(buff);

        if (valid) {
            printf("----- %u\n", i2sRx.getOverflow());
            for (uint i = 0; i < depth; i++) {
                printf("LC 0x%02x  |  RC 0x%02x\n", buff[i].LC, buff[i].RC);
            }
        }
    }
}   
