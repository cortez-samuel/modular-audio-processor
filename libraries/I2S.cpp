#include "I2S.h"

I2S_Tx::I2S_Tx() :
        WS_frame_size(0) {

    #if     I2S_TX_PROGRAM == I2S_TX_PROGRAM__NAIVE
        pio_claim_free_sm_and_add_program(&I2S_Tx_naive_program, &pio, &sm, &offset);
    #elif   I2S_TX_PROGRAM == I2S_TX_PROGRAM__COMPACT
        pio_claim_free_sm_and_add_program(&I2S_Tx_compact_program, &pio, &sm, &offset);
    #endif
}
I2S_Tx::I2S_Tx(uint32_t* reservedMem, uint32_t* defaultSpace, uint32_t width, uint32_t depth) : 
        WS_frame_size(0) {
            
    #if     I2S_TX_PROGRAM == I2S_TX_PROGRAM__NAIVE
        pio_claim_free_sm_and_add_program(&I2S_Tx_naive_program, &pio, &sm, &offset);
    #elif   I2S_TX_PROGRAM == I2S_TX_PROGRAM__COMPACT
        pio_claim_free_sm_and_add_program(&I2S_Tx_compact_program, &pio, &sm, &offset);
    #endif

    setReservedMem(reservedMem, defaultSpace, width, depth);
}

void I2S_Tx::setReservedMem(uint32_t* reservedMem, uint32_t* defaultSpace, uint32_t width, uint32_t depth) {
    txPingPong.setReservedSpace(reservedMem, defaultSpace, width, depth);
}
void I2S_Tx::setDefaultData(uint32_t* defaultData) {
    txPingPong.setDefaultData(defaultData, WS_frame_size);
}

void I2S_Tx::enable(bool start) {
    if (start) {
        txPingPong.begin(pio, sm);
        pio_sm_set_enabled(pio, sm, true);
    }
}


I2S_Rx::I2S_Rx() :
        rxPingPong() {
    pio_claim_free_sm_and_add_program(&I2S_Rx_naive_program, &pio, &sm, &offset);
}
I2S_Rx::I2S_Rx(uint32_t* reserved, uint8_t depth) : 
        rxPingPong(reserved, depth) {
    pio_claim_free_sm_and_add_program(&I2S_Rx_naive_program, &pio, &sm, &offset);
}

void I2S_Rx::setReservedMem(uint32_t* reservedMem, uint8_t depth) {
    rxPingPong.setReservedSpace(reservedMem, depth);
}

bool I2S_Rx::init(uint BCLK_pin, uint WS_pin, uint SD_pin, float fs, uint WS_frame_size) {
    I2S_Rx_naive_init(pio, sm, offset, BCLK_pin, WS_pin, SD_pin, fs, WS_frame_size);

    return true;
}

void I2S_Rx::enable(bool start) {
    if (start) {
        rxPingPong.begin(pio, sm);
        pio_sm_set_enabled(pio, sm, true);
    }
}