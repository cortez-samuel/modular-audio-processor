#ifndef I2S__H
#define I2S__H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#include "RxPingPong.h"

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
    uint WS_frame_size;

public:
    uint32_t txBuffer[4 << 1];  // [LC][RC][LC][RC][...]
    uint8_t  head;
    uint32_t* headAddr;


    PIO pio;
    uint sm;
    uint offset;

    int dataChannel;
    int ctrlChannel;

public:
    I2S_Tx();
    
    inline bool init(uint BCLK_pin, uint WS_pin, uint SD_pin, float fs, uint WS_frame_size) {
        this->WS_frame_size = WS_frame_size;

        #if     I2S_TX_PROGRAM == I2S_TX_PROGRAM__NAIVE
            I2S_Tx_naive_init(pio, sm, offset, BCLK_pin, WS_pin, SD_pin, fs, WS_frame_size);
        #elif   I2S_TX_PROGRAM == I2S_TX_PROGRAM__COMPACT
            I2S_Tx_compact_init(pio, sm, BCLK_pin, WS_pin, SD_pin, fs, WS_frame_size);
        #endif

        dma_channel_config_t c;
        c = dma_channel_get_default_config(ctrlChannel);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, false);
        channel_config_set_write_increment(&c, false);

        dma_channel_configure(ctrlChannel, &c, 
            &dma_hw->ch[dataChannel].al3_read_addr_trig,
            &headAddr,
            1,
            false
        );

        c = dma_channel_get_default_config(dataChannel);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
        channel_config_set_chain_to(&c, ctrlChannel);
        
        dma_channel_configure(dataChannel, &c, 
            &pio->txf[sm],
            NULL,
            2,
            false
        );
        
        return true;
    }
    
    void enable(bool start);

public:
    void queue(uint32_t LC, uint32_t RC);

public:
    const uint32_t* getData(uint32_t i) const;
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