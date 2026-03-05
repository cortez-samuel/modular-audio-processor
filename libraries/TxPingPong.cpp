#include "TxPingPong.h"

TxPingPong::TxPingPong() {
    _reservedMem = nullptr;
    _bufferDepth = 0;
    _running        = false;
    _underflow      = false;
    _offset         = 0;
    _empty          = nullptr;
    _active         = nullptr;
    _queued         = nullptr;
    _filled         = nullptr;
    _defaultData    = nullptr;
}

TxPingPong::TxPingPong(uint32_t* reserved, uint32_t* defaultData, uint32_t depth) {
    _reservedMem    = reserved;
    _bufferDepth    = depth;
    _running        = false;
    _underflow      = false;
    _offset         = 0;
    _empty          = nullptr;
    _active         = nullptr;
    _queued         = nullptr;
    _filled         = nullptr;
    _defaultData    = nullptr;

    for (int i = 0; i < WIDTH; i++) {
        _buffers[i].data = &_reservedMem[_bufferDepth * i];
        _appendBuffer(&_empty, &_buffers[i]); 
    }
    _defaultData->data = defaultData;
}

void TxPingPong::setReservedSpace(uint32_t* reserved, uint32_t depth) {
    _reservedMem = reserved;
    _bufferDepth = depth;
    _empty          = nullptr;
    _active         = nullptr;
    _queued         = nullptr;
    _filled         = nullptr;
    for (int i = 0; i < WIDTH; i++) {
        _buffers[i].data = &_reservedMem[_bufferDepth * i];
        _appendBuffer(&_empty, &_buffers[i]); 
    }
}

void TxPingPong::setDefaultData(uint32_t* defaultData) {
    _defaultData->data = defaultData;
}

void TxPingPong::begin(PIO pio, uint sm) {
    uint ch0, ch1;
    ch0 = dma_claim_unused_channel(true);
    ch1 = dma_claim_unused_channel(true);
    dma_channel_config_t c;

    c = dma_channel_get_default_config(ch0);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
        channel_config_set_chain_to(&c, ch1);
        channel_config_set_irq_quiet(&c, false);
        dma_channel_configure(ch0, &c,
            _defaultData->data,
            &pio->txf[sm],
            _bufferDepth,
            false
        );
            // config irq
        dma_irqn_set_channel_enabled(1, ch0, true);

    c = dma_channel_get_default_config(ch1);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
        channel_config_set_chain_to(&c, ch0);
        channel_config_set_irq_quiet(&c, false);
        dma_channel_configure(ch1, &c,
            _defaultData->data,
            &pio->txf[sm],
            _bufferDepth,
            false
        );
            // config irq
        dma_irqn_set_channel_enabled(1, ch1, true);

    irq_set_exclusive_handler(DMA_IRQ_NUM(1), _clsIRQ);

    __dmaMap[ch0] = this;
    __dmaMap[ch1] = this;

    dma_channel_start(ch0);
    irq_set_enabled(DMA_IRQ_NUM(1), true);
}

bool TxPingPong::queueBuffer(uint32_t* buff) {
    if (_empty == nullptr) return false;

    Buffer_t* filledBuffer = _popBuffer(&_empty);
    for (int i = 0; i < _bufferDepth; i++) {
        filledBuffer->data[i] = buff[i];
    }
    _appendBuffer(&_filled, filledBuffer);
    _underflow = false;
    _offset = 0;
    return true;
}
bool TxPingPong::queue(uint32_t* in) {
    if (_empty == nullptr) return false;

    _empty->data[_offset++] = *in;
    if (_offset == _bufferDepth) {
        _offset = 0;
        Buffer_t* done = _popBuffer(&_empty);
        _appendBuffer(&_filled, done);
        _underflow = false;
    }
    return true;
}


