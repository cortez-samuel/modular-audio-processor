#ifndef ADC_HPP
#define ADC_HPP

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/irq.h"

#define ADC_GET_CHANNEL_PIN(channel) (channel + 26)


class ADC {
public:
    static constexpr float CONVERSION_FACTOR = 3.3f / (1 << 12);
    static constexpr float ADC_CLK_HZ = 48 * 1000 * 1000;

public:
    struct Settings_t {
        float fs;
        uint resolution;
        bool freeRun;
        bool rrEnabled;
    };

    inline static constexpr Settings_t defaultSettings {
        .fs         = 44100,
        .resolution = 12,
        .freeRun    = true,
        .rrEnabled  = false,
    };
    static inline Settings_t settings = defaultSettings;

public:
    static inline ADC* __availableChannels[4];
    static inline uint8_t __activeChannelMask;
    static inline uint8_t __activeChannel;
    static inline bool __running;

    uint8_t _channel;
    bool _enabled;
    volatile uint _raw;
    volatile bool _newValue;

public:
    static inline bool init(uint startChannel) {
        if (__availableChannels[startChannel] == nullptr) return false;

        adc_init();

        float div = ADC_CLK_HZ / settings.fs - 1.0f;
        adc_set_clkdiv(div);
        
        adc_fifo_setup(true, false, 1, false, false);

        adc_irq_set_enabled(true);
        irq_set_exclusive_handler(ADC_IRQ_FIFO, _clsIRQ);
        irq_set_enabled(ADC_IRQ_FIFO, true);

        if (settings.rrEnabled) adc_set_round_robin(__activeChannelMask);

        adc_select_input(startChannel);

        return true;
    }

    static inline void run(bool en) {
        adc_run(en);
    }

    static inline uint8_t getActiveChannel() {
        return __activeChannel;
    }
    static inline uint8_t getNextChannel() {
        if (!settings.rrEnabled) return __activeChannel;

        for (uint i = 1; i <= 4; i++) {
            uint j = (__activeChannel + i) % 4;
            if (__availableChannels[j] != nullptr) {
                return j;
            }
        }

        return __activeChannel;
    }

    static void __time_critical_func(_clsIRQ)() {
        uint16_t rawValue = adc_fifo_get();
        __availableChannels[__activeChannel]->_IRQ(rawValue);
        __activeChannel = getNextChannel();
    }

    void __time_critical_func(_IRQ)(uint rawValue) {
        _raw = rawValue;
        _newValue = true;
    }


public:
    ADC(uint channel) {
        _channel    = channel;
        _enabled    = false;
        _raw        = 0;
        _newValue   = false;
    }

    inline uint8_t getChannel() const {
        return _channel;
    }
    inline volatile bool newValue() const {
        return _newValue;
    }
    inline volatile uint16_t getRaw() {
        _newValue = false;
        return _raw;
    }
    inline volatile float getTrue() {
        _newValue = false;
        return _raw * CONVERSION_FACTOR;
    }

public:
    inline bool enable(bool en) {   
        if (__running) return false;

        if (en) adc_gpio_init(ADC_GET_CHANNEL_PIN(_channel));

        _enabled = en; 
        __availableChannels[_channel] = this;
        __activeChannelMask = __activeChannelMask | (1 << _channel);

        return true;
    }
    inline bool read() {
        if (settings.rrEnabled) return false;

        __activeChannel = _channel;
        adc_select_input(_channel);
        _raw = adc_read();
        _newValue = true;

        return true;
    }
};

#endif // ADC_HPP