#ifndef I2S__H
#define I2S__H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

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

private:
    uint32_t txBuffer[4 << 1];  // [LC][RC][LC][RC][...]
    uint8_t head;
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
    uint WS_frame_size;

public:
    volatile uint32_t rxBuffer[8 << 1];  // [LC][RC][LC][RC][...] 0x0F
    volatile uint32_t top;
    volatile uint32_t bottom;
    volatile uint32_t* topAddr;

    PIO pio;
    uint sm;
    uint offset;

    int dataChannel;

    irq_handler_t irqHandler;
    uint irqn;

    volatile bool cleared;

public:
    I2S_Rx();
    I2S_Rx(irq_handler_t irqHandler, uint irqn);

    inline bool init(uint BCLK_pin, uint WS_pin, uint SD_pin, float fs, uint WS_frame_size) {
        this->WS_frame_size = WS_frame_size;

        I2S_Rx_naive_init(pio, sm, offset, BCLK_pin, WS_pin, SD_pin, fs, WS_frame_size);
 
        dma_channel_config_t c;
        c = dma_channel_get_default_config(dataChannel);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, false);
        channel_config_set_write_increment(&c, true);
        channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
        dma_irqn_set_channel_enabled(irqn, dataChannel, true);
        dma_channel_configure(dataChannel, &c,
            NULL,
            &pio->rxf[sm],
            2,
            false
        );

        return true;
    }

    void enable(bool start);

    void setIRQHandler(irq_handler_t handler, uint irqn);

    inline void defaultIRQHandler() {
        if (!full() && !cleared) {
            top = (top + 2) & 0x0F;
            topAddr = &rxBuffer[top];
        }
        cleared = false;
        
            // clear IRQ0 and trigger next transfer to topAddr
        dma_irqn_acknowledge_channel(irqn, dataChannel);
        dma_channel_set_write_addr(dataChannel, topAddr, true);
    }
    
public:
    inline bool empty() const {
        return top == bottom;
    }
    inline bool full() const {
        return ((top + 2) & 0x0F) == bottom;
    }

    inline uint depth() const {
        return ((top - bottom) & 0x0F) >> 1;
    }

    inline void clearFIFO() {
        bottom = 0;
        top = bottom;
        topAddr = &rxBuffer[0];
        rxBuffer[0] = 0;
        rxBuffer[1] = 0;

        cleared = true;
    }

    inline const bool pop(uint32_t& LC, uint32_t& RC) {
        if (empty()) return 0;

        LC = rxBuffer[bottom];
        RC = rxBuffer[bottom+1];
        bottom = (bottom + 2) & 0x0F;
        return 1;
    }

    inline void popBlocking(uint32_t& LC, uint32_t& RC) {
        while (empty()) {tight_loop_contents();}

        LC = rxBuffer[bottom];
        RC = rxBuffer[bottom+1];
        bottom = (bottom + 2) & 0x0F;
    }
};

#endif