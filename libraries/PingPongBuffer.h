#ifndef PINGPONGBUFFER__H
#define PINGPONGBUFFER__H

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

template<typename T, uint32_t bufferSize, uint8_t numBuffers>
class PingPongDMA {
    static PingPongDMA _channelArray[12];

    struct Buffer_t {
        T data[bufferSize];
        Buffer_t* next;
    };

    Buffer_t m_bufferArray[numBuffers]; 
    Buffer_t* m_empty;
    Buffer_t m_active[2]; // [writing][queued]
    Buffer_t* m_full;

    int m_dmaChannels[2];
    bool m_running;

    bool overflow;


public:
    PingPongDMA() {
        m_empty = nullptr;
        m_active = {m_bufferArray[0], m_bufferArray[1]};
        m_full = nullptr;

        
        for (uint i = 0; i < 2; i++) {
            m_dmaChannels[i] = -1;
        }
    }

private:
    inline void _queueBuffer(Buffer_t** FIFO, Buffer_t* element) {
        if (*list == nullptr) {
            *list = element;
            element->next = nullptr;
            return;
        }
        while ((*list)->next != nullptr) {
            list = &(*list)->next;
        }
        (*list)->next = element;
        element->next = nullptr;
    }

    inline Buffer_t* _popBuffer(Buffer_t** FIFO) {
        if (*list == nullptr) {
            return nullptr;
        }
        Buffer_t* ret = *FIFO;
        *FIFO = ret->next;
        return ret;
    }

public:
    bool initDMA(enum dma_channel_transfer_size txSize, bool readInc, int dreq, 
            volatile void* peripheralAddr) {
        for (uint i = 0; i < 2; i++) {
            m_dmaChannels[i] = dma_claim_unused_channel(true);
            PingPongDMA::_channelArray[i] = this;
        }

        for (uint i = 0; i < 2; i++) {
            dma_channel_config_t c = dma_channel_get_default_config(m_dmaChannels[i]);
            channel_config_set_transfer_data_size(&c, txSize);
            channel_config_set_read_increment(&c, readInc);
            channel_config_set_write_increment(&c, true);
            channel_config_set_dreq(&c, dreq);
            channel_config_set_chain_to(&c, m_dmaChannels[1-i]);
            channel_config_set_irq_quiet(&c, false);
            dma_channel_configure(m_dmaChannels[i], &c, 
                m_active[i],
                peripheralAddr,
                bufferSize,
                false
            );
        }
        
        return true;
    }
    
    bool initIRQ() {
        for (uint i = 0; i < 2; i++) {
            dma_irqn_set_channel_enabled(0, m_dmaChannels[i], true);
        }
        irq_set_exclusive_handler(dma_get_irq_num(0), defaultIRQ);

        return true;
    }

    bool begin(float fs) {return true;}

private:
    static void __time_critical_func(defaultIRQ)() {
        static int channel = 0;

        dma_irqn_acknowledge_channel(dma_get_irq_num(0), channel);
        PingPongDMA* instance = PingPongDMA::_channelArray[channel];
        if (!instance->m_running) return;
        
        if (instance->m_empty) { 
            _queueBuffer(&instance->m_full, &instance->m_active[0]);
            instance->m_active[0] = instance->m_active[1];
            instance->m_active[1] = _popBuffer(&instance->m_empty);
        }
        else {
            instance->overflow = true;
        }

        dma_channel_set_write_addr(instance->m_dmaChannels[channel], &m_active[1], false);
        dma_irqn_acknowledge_channel(0, instance->m_dmaChannels[channel]);

        channel = 1 - channel;
    }

};

#endif