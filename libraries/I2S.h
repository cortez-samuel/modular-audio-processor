#ifndef I2S__H
#define I2S__H

#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/dma.h"

#include "RxPingPong.h"
#include "TxPingPong.h"
#include "I2S_Tx_naive.pio.h"
#include "I2S_Tx_compact.pio.h"
#include "I2S_Rx_naive.pio.h"

#define I2S_TX_PROGRAM__NAIVE       0
#define I2S_TX_PROGRAM__COMPACT     1

#ifndef I2S_TX_PROGRAM
#define I2S_TX_PROGRAM  I2S_TX_PROGRAM__NAIVE
#endif


class I2S_Tx {
public:
    static const uint8_t BUFFER_WIDTH = TxPingPong::WIDTH;

public:
    uint WS_frame_size;

public:
    TxPingPong txPingPong;

    PIO pio;
    uint sm;
    uint offset;

public:
    I2S_Tx();
    I2S_Tx(uint32_t* reserved, uint8_t depth);
    
    void setReservedMem(uint32_t* reserved, uint8_t depth);
    inline bool init(uint BCLK_pin, uint WS_pin, uint SD_pin, float fs, uint WS_frame_size) {
        this->WS_frame_size = WS_frame_size;

        #if     I2S_TX_PROGRAM == I2S_TX_PROGRAM__NAIVE
            I2S_Tx_naive_init(pio, sm, offset, BCLK_pin, WS_pin, SD_pin, fs, WS_frame_size);
        #elif   I2S_TX_PROGRAM == I2S_TX_PROGRAM__COMPACT
            I2S_Tx_compact_init(pio, sm, BCLK_pin, WS_pin, SD_pin, fs, WS_frame_size);
        #endif

        return true;
    }
    void enable(bool start);

public:
    inline bool queue(uint32_t LC, uint32_t RC) {
        bool LC_valid, RC_valid;
        LC_valid = txPingPong.queue(&LC);
        RC_valid = txPingPong.queue(&RC);
        return LC_valid && RC_valid;
    }
    inline bool queueBuffer(uint32_t* buff) {
        return txPingPong.queueBuffer(buff);
    }
};


class I2S_Rx {
public:
    static const uint8_t BUFFER_WIDTH = RxPingPong::WIDTH;

private:
    RxPingPong rxPingPong;

    PIO pio;
    uint sm;
    uint offset;

public:
    I2S_Rx();
    I2S_Rx(uint32_t* reserved, uint8_t depth);

public:
    void setReservedMem(uint32_t* reservedMem, uint8_t depth);
    bool init(uint BCLK_pin, uint WS_pin, uint SD_pin, float fs, uint WS_frame_size);
    void enable(bool start);

public:
    inline bool read(uint32_t& LC, uint32_t& RC) {
        bool valid = rxPingPong.read(&LC); rxPingPong.read(&RC);
        return valid;
    }
    inline bool readBuffer(uint32_t* out) {
        return rxPingPong.readBuffer(out);
    }

public:
    inline bool getOverflow() const {
        return rxPingPong.overflow();
    }
    inline void clearOverflow() {
        rxPingPong.clearOverflow();
    }
};

#endif