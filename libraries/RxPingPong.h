#ifndef RX_PING_PONG__H
#define RX_PING_PONG__H

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/sync.h"


class RxPingPong {
public:
    static const uint8_t WIDTH = 4;

private:
    static inline RxPingPong* __dmaMap[12];

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

public:
    RxPingPong(uint32_t* reserved, uint32_t depth);

public:
    void begin(PIO pio, uint sm);

public:
    bool readBuffer(uint32_t* buff);
    bool read(uint32_t* out);
    
private:
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

    void __time_critical_func(_IRQ)(int ch) {
            // for debug purposes
        static uint pin13value = 0;
        gpio_put(13, pin13value);
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
    static void __time_critical_func(_clsIRQ)() {
        for (int ch = 0; ch < 12; ch++) {
            if (dma_channel_get_irq0_status(ch) && __dmaMap[ch] != nullptr) {
                return __dmaMap[ch]->_IRQ(ch);
            }
        }
    }
};

#endif