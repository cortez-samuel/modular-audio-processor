#ifndef I2S__H
#define I2S__H

#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/dma.h"

#include "RxPingPong.h"
#include "TxPingPong.h"
#include "AudioSample.hpp"

#include "I2S_Tx_naive.pio.h"
#include "I2S_Tx_compact.pio.h"

#include "I2S_Rx_naive.pio.h"
#include "I2S_Rx_autoFrame.pio.h"

#define I2S_TX_PROGRAM__NAIVE       0
#define I2S_TX_PROGRAM__COMPACT     1

#ifndef I2S_TX_PROGRAM
#define I2S_TX_PROGRAM  I2S_TX_PROGRAM__NAIVE
#endif

#define I2S_RX_PROGRAM__NAIVE       0
#define I2S_RX_PROGRAM__AUTOFRAME   1

#ifndef I2S_RX_PROGRAM
#define I2S_RX_PROGRAM  I2S_RX_PROGRAM__AUTOFRAME
#endif

struct I2SSettings_t {
    uint frameSize;
    float fs;
    bool stereo;
};

class I2S_Tx {
public:
    struct Settings_t {
        I2SSettings_t i2sSettings;
        uint32_t bufferWidth;
        uint32_t bufferDepth;
        AudioSample_t* reservedMem;
        AudioSample_t* defaultMem;
    };

    inline static constexpr Settings_t defaultSettings {
        .i2sSettings    = {
            .frameSize      = 16,
            .fs             = 44100,
            .stereo         = true,
        },
        .bufferWidth    = 16,
        .bufferDepth    = 128,
        .reservedMem    = nullptr,
        .defaultMem     = nullptr,
    };
    Settings_t settings;

public:
    TxPingPong txPingPong;

    PIO pio;
    uint sm;
    uint offset;

public:
    I2S_Tx();
    
    inline bool init(uint BCLK_pin, uint WS_pin, uint SD_pin) {
        #if     I2S_TX_PROGRAM == I2S_TX_PROGRAM__NAIVE
            I2S_Tx_naive_init(pio, sm, offset, BCLK_pin, WS_pin, SD_pin, 
                              settings.i2sSettings.fs, settings.i2sSettings.frameSize
                            );
        #elif   I2S_TX_PROGRAM == I2S_TX_PROGRAM__COMPACT
            I2S_Tx_compact_init(pio, sm, BCLK_pin, WS_pin, SD_pin, 
                                settings.i2sSettings.fs, settings.i2sSettings.frameSize
                            );
        #endif

        return true;
    }
    void enable(bool start);

public:
    inline bool queue(AudioSample_t sample) {
        bool LC_valid, RC_valid;
        
        LC_valid = txPingPong.queue(sample.LC, settings.i2sSettings.frameSize);
        RC_valid = txPingPong.queue(sample.RC, settings.i2sSettings.frameSize);
        return LC_valid && RC_valid;
    }
    inline void queueBlocking(AudioSample_t sample) {
        txPingPong.queueBlocking(sample.LC, settings.i2sSettings.frameSize);
        txPingPong.queueBlocking(sample.RC, settings.i2sSettings.frameSize);
    }
    inline bool queueBuffer(AudioSample_t* in) {
        return txPingPong.queueBuffer((uint32_t*)in, settings.i2sSettings.frameSize);
    }

public:
    inline bool getUnderflow() const {
        return txPingPong.underflow();
    }
    inline void clearUnderflow() {
        txPingPong.clearUnderflow();
    }
};


class I2S_Rx {
public:
    static const uint8_t BUFFER_WIDTH = RxPingPong::WIDTH;

    struct Settings_t {
        I2SSettings_t i2sSettings;
        uint32_t bufferDepth;
        AudioSample_t* reservedMem;
    };

    inline static constexpr Settings_t defaultSettings {
        .i2sSettings    = {
            .frameSize      = 16,
            .fs             = 44100,
            .stereo         = true,
        },
        .bufferDepth    = 128,
        .reservedMem    = nullptr,
    };
    Settings_t settings;

private:
    RxPingPong rxPingPong;

    PIO pio;
    uint sm;
    uint offset;

public:
    I2S_Rx();

public:
    bool init(uint BCLK_pin, uint WS_pin, uint SD_pin);
    void enable(bool start);

public:
    inline bool read(AudioSample_t& sample) {
        bool valid = rxPingPong.read(&sample.LC); rxPingPong.read(&sample.RC);
        return valid;
    }
    inline bool readBuffer(AudioSample_t* out) {
        return rxPingPong.readBuffer((uint32_t*)out);
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