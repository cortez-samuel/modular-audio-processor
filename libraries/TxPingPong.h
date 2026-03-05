#ifndef TX_PING_PONG__H
#define TX_PING_PONG__H

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/sync.h"

class TxPingPong {
public:
    static const uint8_t WIDTH = 16;

private:
    static inline TxPingPong* __dmaMap[12];

    struct Buffer_t {
        uint32_t* data;
        Buffer_t* next;
    };

    uint _bufferDepth;
    Buffer_t _buffers[WIDTH];
    uint32_t* _reservedMem;

    Buffer_t* _empty;
    Buffer_t* _queued;
    Buffer_t* _active;
    Buffer_t* _filled;

    Buffer_t* _defaultData;

    bool _running;
    bool _underflow;
    uint _offset;

public:
    TxPingPong();
    TxPingPong(uint32_t* reserved, uint32_t* defaultData, uint32_t depth);
 
public:
    void setReservedSpace(uint32_t* reserved, uint32_t depth);
    void setDefaultData(uint32_t* defaultData);

public:
    void begin(PIO pio, uint sm);

public:
    bool queueBuffer(uint32_t* buff);
    bool queue(uint32_t* in);

public:
    inline bool underflow() const {
        return _underflow;
    }
    inline void clearUnderflow() {
        _underflow = false;
    }

private:
    void __time_critical_func(_appendBuffer)(Buffer_t** FIFO, Buffer_t* element) {
        if (element == nullptr) return;

        uint32_t intStatus = save_and_disable_interrupts();

        element->next = nullptr;
        if (*FIFO == nullptr) {
            *FIFO = element;
        } 
        else {
            Buffer_t** tail = FIFO;
            while ((*tail)->next != nullptr) { 
                tail = &((*tail)->next); 
            }
            (*tail)->next = element;
        }

        restore_interrupts(intStatus);
    }
    Buffer_t* __time_critical_func(_popBuffer)(Buffer_t** FIFO) {
        if (*FIFO == nullptr) { 
            return nullptr; 
        
        }
        uint32_t intStatus = save_and_disable_interrupts();

        Buffer_t* res = *FIFO;
        *FIFO = (*FIFO)->next;
        
        restore_interrupts(intStatus);
        return res;
    }

    void __time_critical_func(_IRQ)(int ch) {
        _appendBuffer(&_empty, _active);
        _active = _queued;

        if (_filled != nullptr) {
            _queued = _popBuffer(&_filled);
            dma_channel_set_read_addr(ch, _queued->data, false);
        }
        else {
            _underflow = true;
            _queued = nullptr;
            dma_channel_set_read_addr(ch, _defaultData->data, false);
        }

        dma_irqn_acknowledge_channel(1, ch);
    }
    static void __time_critical_func(_clsIRQ)() {
        for (int ch = 0; ch < 12; ch++) {
            if (dma_channel_get_irq1_status(ch) && __dmaMap[ch] != nullptr) {
                return __dmaMap[ch]->_IRQ(ch);
            }
        }
    }
    
};

#endif