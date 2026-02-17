#include "../libraries/I2S_Rx_naive.pio.h"

#include "hardware/pio.h"
#include "hardware/irq.h"

#include "../libraries/I2S.h"
#include "../libraries/RxPingPong.h"

#include "pico/stdlib.h"

#include "hardware/sync.h"

#include <cstdio>


uint pin13 = 13;
/*
struct PPB {
    static inline PPB* __dmaMap[12];

    //const uint32_t DEPTH = 256;
    static const uint8_t WIDTH = 4;

    struct Buffer_t {
        uint32_t *data;
        Buffer_t* next;
    };

    uint _bufferDepth;
    Buffer_t _buffers[WIDTH];
    uint32_t* _reservedMem;

    Buffer_t* _empty;
    Buffer_t* _queued;
    Buffer_t* _active;
    Buffer_t* _filled;

    bool _running;
    bool _overflow;
    uint _offset;

    PPB(uint32_t* reserved, uint32_t depth) {
        _reservedMem    = reserved;
        _bufferDepth    = depth;

        _running        = false;
        _overflow       = false;
        _offset         = 0;

        _empty          = nullptr;
        _active         = nullptr;
        _queued         = nullptr;
        _filled         = nullptr;

        for (int i = 0; i < WIDTH; i++) {
            _buffers[i].data = &_reservedMem[_bufferDepth * i];
            _appendBuffer(&_empty, &_buffers[i]); 
        }
        _active = _popBuffer(&_empty);
        _queued = _popBuffer(&_empty);
    }

    void __time_critical_func(_appendBuffer)(Buffer_t** FIFO, Buffer_t* element) {
        if (*FIFO == nullptr) {
            *FIFO = element;
            element->next = nullptr;
            return;
        }
        uint32_t intStatus = save_and_disable_interrupts();
        while ((*FIFO)->next != nullptr) { FIFO = &((*FIFO)->next); }
        (*FIFO)->next = element;
        element->next = nullptr;
        restore_interrupts(intStatus);
    }
    Buffer_t* __time_critical_func(_popBuffer)(Buffer_t** FIFO) {
        if (*FIFO == nullptr) { return nullptr; }
        uint32_t intStatus = save_and_disable_interrupts();
        Buffer_t* res = *FIFO;
        *FIFO = (*FIFO)->next;
        restore_interrupts(intStatus);
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
                _bufferDepth,
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
                _bufferDepth,
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

    void _IRQ(int ch) {
            // for debug purposes
        static uint pin13value = 0;
        gpio_put(pin13, pin13value);
        pin13value = 1-pin13value;
            //
        if (_empty != nullptr) {
            _appendBuffer(&_filled, _active);
            _active = _queued;
            _queued = _popBuffer(&_empty);
        }
        else { _overflow = false; }

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

    bool readBuffer(uint32_t* buff) {
        if (_filled == nullptr) return false;

        Buffer_t* readBuffer = _popBuffer(&_filled);
        for (int i = 0; i < _bufferDepth; i++) {
            buff[i] = readBuffer->data[i];
        }
        _appendBuffer(&_empty, readBuffer);
        _overflow = false;
        _offset = 0;
        return true;
    }
    bool read(uint32_t* out) {
        if (_filled == nullptr) return false;

        uint32_t ret = _filled->data[_offset++];
        if (_offset == _bufferDepth) {
            _offset = 0;
            _appendBuffer(&_empty, _popBuffer(&_filled));
        }
        *out = ret;
        return true;
    }
};
*/

int main() {

    stdio_init_all();

    gpio_init(pin13);
    gpio_set_dir(pin13, GPIO_OUT);

    uint8_t depth = 64;
    uint32_t reserved[RxPingPong::WIDTH * 64];
    I2S_Rx i2sRx(reserved, depth);

    //set_sys_clock_48mhz();    

    /*
    PIO pio;
    uint sm;
    uint offset;
    pio_claim_free_sm_and_add_program(&I2S_Rx_naive_program, &pio, &sm, &offset);
    I2S_Rx_naive_init(pio, sm, offset, 2, 3, 1, 1000, 12);

    uint32_t DEPTH = 64;
    uint32_t reserved[RxPingPong::WIDTH * 64];
    RxPingPong ppb(reserved, DEPTH);
    ppb.begin(pio, sm);
    uint32_t ppb_aligned = alignof(RxPingPong);

    //pingpong.begin(pio, sm);
    */
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

    i2sRx.init(8, 9, 7, 50000, 16);

    sleep_ms(5000);
    stdio_printf("START\n");
    stdio_printf("%u\n", clock_get_hz(clk_sys));

    /*  
    dma_channel_start(ch1);
    pio_sm_set_enabled(pio, sm, true);
    */

    //pio_sm_set_enabled(pio, sm, true);

    i2sRx.enable(true);


    //uint32_t buff[PPB::DEPTH];
    uint32_t LC = 0;
    uint32_t RC = 0;
    stdio_printf("enabled called\n");
    uint i = 0;
    while(1) {
        //stdio_printf("%u\n", i++);
        //bool valid = ppb.read(buff);
        //I2S_Rx_naive_read(i2sRx.pio, i2sRx.sm, &LC, &RC);
        //bool valid = true;
        bool valid = i2sRx.read(LC, RC);

        if (valid) {
            printf("0x%02x 0x%02x\n", LC, RC);
        }

        sleep_ms(2);
    }
}   