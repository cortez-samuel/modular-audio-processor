#include "I2S.h"

I2S_Tx::I2S_Tx() : settings(defaultSettings) {
    #if     I2S_TX_PROGRAM == I2S_TX_PROGRAM__NAIVE
        pio_claim_free_sm_and_add_program(&I2S_Tx_naive_program, &pio, &sm, &offset);
    #elif   I2S_TX_PROGRAM == I2S_TX_PROGRAM__COMPACT
        pio_claim_free_sm_and_add_program(&I2S_Tx_compact_program, &pio, &sm, &offset);
    #endif
    settings = defaultSettings;
}

void I2S_Tx::enable(bool start) {
    if (start) {
        txPingPong.setReservedSpace(
            (uint32_t*)settings.reservedMem, (uint32_t*)settings.defaultMem, 
            settings.bufferWidth, settings.bufferDepth * 2
        );
        txPingPong.begin(pio, sm);
        pio_sm_set_enabled(pio, sm, true);
    }
}


I2S_Rx::I2S_Rx() :
        rxPingPong() {
    #if     I2S_RX_PROGRAM == I2S_RX_PROGRAM__NAIVE
        pio_claim_free_sm_and_add_program(&I2S_Rx_naive_program, &pio, &sm, &offset);
    #elif   I2S_RX_PROGRAM == I2S_RX_PROGRAM__AUTOFRAME
        pio_claim_free_sm_and_add_program(&I2S_Rx_autoFrame_program, &pio, &sm, &offset);
    #endif
    settings = defaultSettings;
}

bool I2S_Rx::init(uint BCLK_pin, uint WS_pin, uint SD_pin) {
    #if     I2S_RX_PROGRAM == I2S_RX_PROGRAM__NAIVE
        I2S_Rx_naive_init(pio, sm, offset, BCLK_pin, WS_pin, SD_pin, 
            settings.i2sSettings.fs, settings.i2sSettings.frameSize
        );
    #elif   I2S_RX_PROGRAM == I2S_RX_PROGRAM__AUTOFRAME
        I2S_Rx_autoFrame_init(pio, sm, offset, BCLK_pin, WS_pin, SD_pin, 
            settings.i2sSettings.fs, settings.i2sSettings.frameSize
        );
    #endif

    return true;
}

void I2S_Rx::enable(bool start) {
    if (start) {
        rxPingPong.setReservedSpace(
            (uint32_t*)settings.reservedMem, 
            settings.bufferDepth * 2
        );
        rxPingPong.begin(pio, sm);
        pio_sm_set_enabled(pio, sm, true);
    }
}