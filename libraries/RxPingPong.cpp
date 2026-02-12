#include "RxPingPong.h"

RxPingPong::RxPingPong(uint32_t* reserved, uint32_t depth) {
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

void RxPingPong::begin(PIO pio, uint sm) {
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

bool RxPingPong::readBuffer(uint32_t* buff) {
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
bool RxPingPong::read(uint32_t* out) {
    if (_filled == nullptr) return false;

    uint32_t ret = _filled->data[_offset++];
    if (_offset == _bufferDepth) {
        _offset = 0;
        _appendBuffer(&_empty, _popBuffer(&_filled));
    }
    *out = ret;
    return true;
}

