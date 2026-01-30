#ifndef I2S__H
#define I2S__H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#include "I2S_Tx_naive.pio.h"
#include "I2S_Tx_compact.pio.h"

#include "I2S_Rx_naive.pio.h"

class I2S_Tx {
public:
    uint WS_frame_size;

private:
    uint32_t txBuffer[4 << 1];  // [LC][RC][LC][RC][...]
    uint8_t head;
    uint32_t* headAddr;


    PIO pio;
    uint sm;
    uint offset;

    int dataChannel;
    int ctrlChannel;

    const pio_program_t* I2S_program;

public:
    I2S_Tx(const pio_program_t* I2Sprogram);
    
    inline bool init(uint BCLK_pin, uint WS_pin, uint SD_pin, float fs, uint WS_frame_size) {
        this->WS_frame_size = WS_frame_size;

        I2S_Tx_naive_init(pio, sm, offset, BCLK_pin, WS_pin, SD_pin, fs, WS_frame_size);

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
    uint WS_frame_size;
};

#endif