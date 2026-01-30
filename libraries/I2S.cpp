#include "I2S.hpp"

I2S_Tx::I2S_Tx(const pio_program_t* I2S_program) :
        I2S_program(I2S_program), txBuffer(), head(0), headAddr(txBuffer), WS_frame_size(0) {
    pio_claim_free_sm_and_add_program(I2S_program, &pio, &sm, &offset);
    
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

