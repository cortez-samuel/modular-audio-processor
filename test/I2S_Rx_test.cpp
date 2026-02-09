#include "../libraries/I2S_Rx_naive.pio.h"

#include "hardware/pio.h"
#include "hardware/irq.h"

#include "../libraries/I2S.h"

#include "pico/stdlib.h"


I2S_Rx i2sRx;

uint pin13 = 13;

struct alignas(void*) PPB {
    static const uint32_t DEPTH = 128;
    static const uint8_t WIDTH = 4;

    struct Buffer_t {
        uint32_t data[DEPTH];
        Buffer_t* next;
    };

    Buffer_t _buffers[WIDTH];

    Buffer_t* _empty;
    Buffer_t* _queued;
    Buffer_t* _active;
    Buffer_t* _filled;

    bool _running;
    bool _overflow;

    uint _ch0, _ch1;

    PPB() {
        _active = &_buffers[0];
        _queued = &_buffers[1];

        //_running = false;
        //_overflow = false;
    }

    void __time_critical_func(_queueBuffer)(Buffer_t** FIFO, Buffer_t* element) {
        if (*FIFO == nullptr) {
            *FIFO = element;
            element->next = nullptr;
            return;
        }
        while ((*FIFO)->next != nullptr) { FIFO = &((*FIFO)->next); }
        (*FIFO)->next = element;
        element->next = nullptr;
    }

    Buffer_t* __time_critical_func(_popBuffer)(Buffer_t** FIFO) {
        if (*FIFO == nullptr) { return nullptr; }
        Buffer_t* res = *FIFO;
        *FIFO = (*FIFO)->next;
        return res;
    }

    void begin(PIO pio, uint sm) {
        uint ch0, ch1;
        ch0 = dma_claim_unused_channel(true);
        ch1 = dma_claim_unused_channel(true);
        dma_channel_config_t c;
        c = dma_channel_get_default_config(ch0);
            channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
            channel_config_set_read_increment(&c, false);
            channel_config_set_write_increment(&c, true);
            channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
            channel_config_set_chain_to(&c, ch1);
            channel_config_set_irq_quiet(&c, false);
            dma_channel_configure(ch0, &c,
                _active->data,
                &pio->rxf[sm],
                DEPTH,
                false
            );
                // config irq
            dma_irqn_set_channel_enabled(0, ch0, true);

        c = dma_channel_get_default_config(ch1);
            channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
            channel_config_set_read_increment(&c, false);
            channel_config_set_write_increment(&c, true);
            channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
            channel_config_set_chain_to(&c, ch0);
            channel_config_set_irq_quiet(&c, false);
            dma_channel_configure(ch1, &c,
                _queued->data,
                &pio->rxf[sm],
                DEPTH,
                false
            );
                // config irq
            dma_irqn_set_channel_enabled(0, ch1, true);
    
        irq_set_exclusive_handler(DMA_IRQ_NUM(0), _clsIRQ);

        __dmaMap[ch0] = this;
        __dmaMap[ch1] = this;

        dma_channel_start(ch0);
        irq_set_enabled(DMA_IRQ_NUM(0), true);
    }

    static inline PPB* __dmaMap[12];

    void _IRQ(int ch) {
        static uint pin13value = 0;
        gpio_put(pin13, pin13value);
        pin13value = 1-pin13value;

        if (_empty != nullptr) {
            _queueBuffer(&_filled, _active);
            _active = _queued;
            _queued = _popBuffer(&_empty);
        }
        //else { _overflow = false; }

        dma_irqn_acknowledge_channel(0, ch); 
        dma_channel_set_write_addr(ch, _queued->data, false);
    }

    static void _clsIRQ() {
        for (int ch = 0; ch < 12; ch++) {
            if (dma_channel_get_irq0_status(ch) && __dmaMap[ch] != nullptr) {
                return __dmaMap[ch]->_IRQ(ch);
            }
        }
    }
};


int main() {

    stdio_init_all();

    gpio_init(pin13);
    gpio_set_dir(pin13, GPIO_OUT);

    //set_sys_clock_48mhz();    

    ///*
    PIO pio;
    uint sm;
    uint offset;
    pio_claim_free_sm_and_add_program(&I2S_Rx_naive_program, &pio, &sm, &offset);
    I2S_Rx_naive_init(pio, sm, offset, 2, 3, 1, 1000, 12);

    PPB ppb;
    ppb.begin(pio, sm);
    uint32_t ppb_aligned = alignof(PPB);

    //pingpong.begin(pio, sm);
    //*/
    /*
    ch1 = dma_claim_unused_channel(true);
    ch2 = dma_claim_unused_channel(true);
    dma_channel_config_t c;
        c = dma_channel_get_default_config(ch1);
            channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
            channel_config_set_read_increment(&c, false);
            channel_config_set_write_increment(&c, true);
            channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
            channel_config_set_chain_to(&c, ch2);
            channel_config_set_irq_quiet(&c, false);
            dma_channel_configure(ch1, &c,
                buffer,
                &pio->rxf[sm],
                128,
                false
            );
                // config irq
            dma_irqn_set_channel_enabled(0, ch1, true);
        c = dma_channel_get_default_config(ch2);
            channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
            channel_config_set_read_increment(&c, false);
            channel_config_set_write_increment(&c, true);
            channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
            channel_config_set_chain_to(&c, ch1);
            channel_config_set_irq_quiet(&c, false);
            dma_channel_configure(ch2, &c,
                buffer,
                &pio->rxf[sm],
                128,
                false
            );
                // config irq
            dma_irqn_set_channel_enabled(0, ch2, true);
    
        irq_set_exclusive_handler(DMA_IRQ_NUM(0), tempIRQ);
        irq_set_enabled(DMA_IRQ_NUM(0), true);
    */

    //i2sRx.init(2, 3, 1, 50000, 12);

    sleep_ms(5000);
    stdio_printf("START\n");
    stdio_printf("%u\n", ppb_aligned);
    stdio_printf("%u\n", clock_get_hz(clk_sys));

    /*  
    dma_channel_start(ch1);
    pio_sm_set_enabled(pio, sm, true);
    */

    pio_sm_set_enabled(pio, sm, true);

    //i2sRx.enable(true);


    uint32_t LC = 0;
    uint32_t RC = 0;
    stdio_printf("enabled called\n");
    uint i = 0;
    while(1) {
        stdio_printf("%u\n", i++);
        //bool valid = i2sRx.read(LC, RC);
        //I2S_Rx_naive_read(i2sRx.pio, i2sRx.sm, &LC, &RC);
        bool valid = true;
        if (valid) stdio_printf("%02x %02x\n", LC, RC);
    }
}