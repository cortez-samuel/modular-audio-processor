#include "TxPingPong.h"

TxPingPong::TxPingPong() {
    _reservedMem = nullptr;
    _bufferWidth = 0;
    _bufferDepth = 0;
    _defaultDataSpace = nullptr;

    _empty          = {_reservedMem, 0, 0};
    _active         = nullptr;
    _queued         = nullptr;
    _filled         = {_reservedMem, 0, 0};

    _running        = false;
    _underflow      = false;
    _offset         = 0;
}

TxPingPong::TxPingPong(uint32_t* reserved, uint32_t* defaultDataSpace, 
                       uint32_t width, uint32_t depth) 
        {
    _reservedMem = reserved;
    _defaultDataSpace = defaultDataSpace;
    _bufferWidth = width;
    _bufferDepth = depth;

    _empty          = {_reservedMem, 0, 0};
    _active         = _defaultDataSpace;
    _queued         = _defaultDataSpace;
    _filled         = {_reservedMem, 0, 0};
    for (uint i = 0; i < width; i++) {
        _appendBufferArray(_empty);
    }

    _running        = false;
    _underflow      = false;
    _offset         = 0;
}

void TxPingPong::setReservedSpace(uint32_t* reserved, uint32_t* defaultDataSpace,
                                  uint32_t width, uint32_t depth) 
    {
    _reservedMem = reserved;
    _defaultDataSpace = defaultDataSpace;
    _bufferWidth = width;
    _bufferDepth = depth;

    _empty          = {_reservedMem, 0, 0};
    _active         = _defaultDataSpace;
    _queued         = _defaultDataSpace;
    _filled         = {_reservedMem, 0, 0} ;
    for (uint i = 0; i < _bufferWidth; i++) {
        _appendBufferArray(_empty); 
    }
}
void TxPingPong::setDefaultData(uint32_t* defaultData, uint WS_frame_size) {
    for (uint i = 0; i < _bufferDepth; i++) {
        _defaultDataSpace[i] = defaultData[i] << (32 - WS_frame_size);
    }
}

void TxPingPong::begin(PIO pio, uint sm) {
    _active = _defaultDataSpace;
    _queued = _defaultDataSpace;

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
            &pio->txf[sm],
            _active,
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
            &pio->txf[sm],
            _queued,
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

bool TxPingPong::queueBuffer(uint32_t* buff, uint WS_frame_size) {
    if (_empty.size == 0) return false;

    uint32_t* filledBuffer = _popBufferArray(_empty);
    for (int i = 0; i < _bufferDepth; i++) {
        filledBuffer[i] = buff[i] << (32 - WS_frame_size);
    }
    _appendBufferArray(_filled);

    _underflow = false;
    _offset = 0;

    return true;
}
void TxPingPong::queueBufferBlocking(uint32_t* buff, uint WS_frame_size) {
    while (_empty.size == 0);

    queueBuffer(buff, WS_frame_size);
}
bool TxPingPong::queue(uint32_t data, uint WS_frame_size) {
    if (_empty.size == 0) return false;

    _empty.start[_offset] = data << (32 - WS_frame_size);
    _offset++;
    if (_offset == _bufferDepth) {
        _offset = 0;
        uint32_t* done = _popBufferArray(_empty);
        _appendBufferArray(_filled);
        _underflow = false;
    }

    return true;
}
void TxPingPong::queueBlocking(uint32_t data, uint WS_frame_size) {
    while(1) {
        if (queue(data, WS_frame_size)) { return; }
    }
}

