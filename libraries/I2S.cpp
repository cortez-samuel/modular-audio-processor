#include "I2S.hpp"

I2S_Tx::I2S_Tx() :
        txBuffer(), head(0), headAddr(txBuffer), WS_frame_size(0) {

    #if     I2S_TX_PROGRAM == I2S_TX_PROGRAM__NAIVE
        pio_claim_free_sm_and_add_program(&I2S_Tx_naive_program, &pio, &sm, &offset);
    #elif   I2S_TX_PROGRAM == I2S_TX_PROGRAM__COMPACT
        pio_claim_free_sm_and_add_program(&I2S_Tx_compact_program, &pio, &sm, &offset);
    #endif

    this->ctrlChannel = dma_claim_unused_channel(true);
    this->dataChannel = dma_claim_unused_channel(true);
}

void I2S_Tx::enable(bool start) {
    if (start) {
        dma_channel_start(ctrlChannel);
        pio_sm_set_enabled(pio, sm, true);
    }
    else {
        dma_channel_abort(ctrlChannel);
        dma_hw->ch[ctrlChannel].ctrl_trig &= ~(0b1);
        pio_sm_set_enabled(pio, sm, false);
        pio_sm_clear_fifos(pio, sm);
        pio_sm_restart(pio, sm);
    }
}

void I2S_Tx::queue(uint32_t LC, uint32_t RC) {
    head = (head + 2) & 0x07;

    txBuffer[head] = LC << (32 - WS_frame_size);
    txBuffer[head+1] = RC << (32 - WS_frame_size);
    headAddr = &txBuffer[head];
}

const uint32_t* I2S_Tx::getData(uint32_t i) const {
    return &txBuffer[(head - 2*i) & 0x07];
}



I2S_Rx::I2S_Rx() :
        rxBuffer(), top(0), bottom(0), topAddr(rxBuffer), 
        WS_frame_size(0), 
        irqHandler(nullptr), irqn(2), cleared(1) {

    pio_claim_free_sm_and_add_program(&I2S_Rx_naive_program, &pio, &sm, &offset);

    this->dataChannel = dma_claim_unused_channel(true);
}
I2S_Rx::I2S_Rx(irq_handler_t irqHandler, uint irqn) : 
        rxBuffer(), top(0), bottom(0), topAddr(rxBuffer), 
        WS_frame_size(0), 
        irqHandler(nullptr), irqn(2), cleared(1) {

    pio_claim_free_sm_and_add_program(&I2S_Rx_naive_program, &pio, &sm, &offset);

    this->dataChannel = dma_claim_unused_channel(true);
    
    setIRQHandler(irqHandler, irqn);
}

void I2S_Rx::enable(bool start) {
    if (start) {
        irq_set_exclusive_handler(DMA_IRQ_NUM(irqn), irqHandler);
        irq_set_enabled(DMA_IRQ_NUM(irqn), true);

        defaultIRQHandler();

        pio_sm_set_enabled(pio, sm, true);
    }
    else {
        dma_channel_abort(dataChannel);
        dma_hw->ch[dataChannel].ctrl_trig &= ~(0b1);
        pio_sm_set_enabled(pio, sm, false);
        pio_sm_clear_fifos(pio, sm);
        pio_sm_restart(pio, sm);
    }
}

void I2S_Rx::setIRQHandler(irq_handler_t handler, uint irqn) {
    this->irqHandler = handler;
    this->irqn = irqn;
}