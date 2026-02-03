#include "../libraries/I2S_Rx_naive.pio.h"

#include "hardware/pio.h"
#include "hardware/irq.h"

#include "../libraries/I2S.hpp"

#include "pico/stdlib.h"
#include <cstdio>

I2S_Rx i2sRx;

int channel = 0;
volatile bool triggered = false;

uint32_t src;
uint32_t dest[2] {0, 0};

void I2S_Rx_irqHandler() {
    triggered = true;

    //dma_irqn_acknowledge_channel(0, i2sRx.dataChannel);
    //dma_channel_set_write_addr(i2sRx.dataChannel, dest, true);

    i2sRx.defaultIRQHandler();
    //printf("IRQ Triggered\n");
}

int main() {

    stdio_init_all();

    
    i2sRx.setIRQHandler(I2S_Rx_irqHandler, 0);
    i2sRx.init(2, 3, 1, 1000, 12);
    

    /*
    PIO pio;
    uint sm;
    uint offset;
    pio_claim_free_sm_and_add_program(&I2S_Rx_naive_program, &pio, &sm, &offset);
    I2S_Rx_naive_init(pio, sm, offset, 2, 3, 1, 1000, 12);
    
    channel = dma_claim_unused_channel(true);
    dma_channel_config_t c = dma_channel_get_default_config(channel);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
    dma_channel_set_irq0_enabled(channel, true);
    dma_channel_configure(channel,
        &c,
        dest, 
        &pio->rxf[sm],
        2,
        false
    );

    irq_set_exclusive_handler(DMA_IRQ_0, I2S_Rx_irqHandler);
    irq_set_enabled(DMA_IRQ_0, true);
    */
    sleep_ms(5000);
    printf("START\n");

    i2sRx.enable(true);
    //irq_set_exclusive_handler(DMA_IRQ_0, I2S_Rx_irqHandler);
    //irq_set_enabled(DMA_IRQ_0, true);

    //I2S_Rx_irqHandler();
    //pio_sm_set_enabled(i2sRx.pio, i2sRx.sm, true);

    uint32_t LC = 0;
    uint32_t RC = 0;

    while(1) {
        triggered = false;
        bool good = i2sRx.pop(LC, RC); 
        if (good) {
        printf("%02x %02x :: %u -- %u %u\n", LC, RC, i2sRx.depth(), i2sRx.bottom, i2sRx.top);
        }
    }
}