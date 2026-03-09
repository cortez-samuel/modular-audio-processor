#ifndef TX_PING_PONG__H
#define TX_PING_PONG__H

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/sync.h"

#include <cstdio>

class TxPingPong {
public:
    static inline TxPingPong* __dmaMap[12];

    struct DataBuffer_t {
        uint32_t* start;
        volatile uint startIndex;
        volatile uint size;
    };

    uint _bufferDepth;
    uint _bufferWidth;
    uint32_t* _reservedMem;

    uint32_t* _defaultDataSpace;

    DataBuffer_t _empty;
    uint32_t* _queued;
    uint32_t* _active;
    DataBuffer_t _filled;

    bool _running;
    bool _underflow;
    uint32_t _offset;

public:
    TxPingPong();
    TxPingPong(uint32_t* reserved, uint32_t* defaultDataSpace, uint32_t width, uint32_t depth);
 
public:
    void setReservedSpace(uint32_t* reserved, uint32_t* defaultDataSpace, uint32_t width, uint32_t depth);
    void setDefaultData(uint32_t* defaultData, uint WS_frame_size);

public:
    void begin(PIO pio, uint sm);

public:
    inline uint getQueueLevel() const {
        return _filled.size;
    }
    bool queueBuffer(uint32_t* buff, uint WS_frame_size);
    void queueBufferBlocking(uint32_t* buff, uint WS_frame_size);
    bool queue(uint32_t in, uint WS_frame_size);
    void queueBlocking(uint32_t in, uint WS_frame_size);

public:
    inline bool underflow() const {
        return _underflow;
    }
    inline void clearUnderflow() {
        _underflow = false;
    }

public:
    bool _appendBufferArray(DataBuffer_t& bufferArray) {
        if (bufferArray.size == _bufferWidth - 2) return false;

        bufferArray.size++;

        return true;
    }
    uint32_t* _popBufferArray(DataBuffer_t& bufferArray) {
        if (bufferArray.size == 0) return nullptr;
        
        uint32_t* ret = bufferArray.start;
        bufferArray.size--;
        bufferArray.startIndex = (bufferArray.startIndex + 1) % _bufferWidth;
        bufferArray.start = _reservedMem + _bufferDepth * bufferArray.startIndex;

        return ret;
    }

    void __time_critical_func(_IRQ)(int ch) {
            // _active always points to non-inclusive tail of _empty, so appendBufferArray(_empty) 'adds' _active to it
            // // will fail if reserved mem looks like [empty | active | queued]
            // move _queued to _active
            // // basically 'increments' active in the array
            // // if _queued == _defaultData, then _active still 'holds' the tail for _empty, but filled with _defaultData data
            // pop head of _filled into _queued to mark it as queued
            // // _filled is empty => _queued == nullptr, so 'fill' the (non-inclusive) head of _filled with _defaultData data
        _appendBufferArray(_empty);
        _active = _queued;
        _queued = _popBufferArray(_filled);
        if (_queued == nullptr) {
            // underflow
            printf("underflow %u\n", ch);
            _queued = _defaultDataSpace;
            _underflow = true;
        }

        dma_irqn_acknowledge_channel(1, ch);
        dma_channel_set_read_addr(ch, _queued, false);
    }
    static void __time_critical_func(_clsIRQ)() {
        static bool pin13val = 0;
        gpio_put(13, pin13val);
        pin13val = !pin13val;  
        //printf("clsIRQ\n"); 
        for (int ch = 0; ch < 12; ch++) {
            if (dma_channel_get_irq1_status(ch) && __dmaMap[ch] != nullptr) {
                //printf("clsIRQ2\n"); 
                return __dmaMap[ch]->_IRQ(ch);
            }
        }
    }
    
    void __time_critical_func(_debugIRQ)() {
        _appendBufferArray(_empty);
        _active = _queued;
        _queued = _popBufferArray(_filled);
        if (_queued == nullptr) {
            // underflow
            printf("underflow %u\n", 0);
            _queued = _defaultDataSpace;
            _underflow = true;
        }
    }

    inline void _printdetails() const {
        printf("%u :: \n\tempty{%u, %u} \n\tactive{%u} \n\tqueued{%u} \n\tfilled{%u, %u} \n\tdefault{%u} \n", 
            0, _empty.start, _empty.size, _active, _queued, _filled.start, _filled.size, _defaultDataSpace);
    }
};

#endif
