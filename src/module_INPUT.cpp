#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "../libraries/adc.hpp"
#include "../libraries/I2S.h"

#include <cstdio>
#include <cstdint>

static const uint8_t ADC_CHANNEL        = 0;
static const uint8_t PIN_ADC_CHANNEL    = ADC_GET_CHANNEL_PIN(ADC_CHANNEL);

static const uint8_t PIN_I2S_Tx_SD      = 0;
static const uint8_t PIN_I2S_Tx_BCLK    = 2;
static const uint8_t PIN_I2S_Tx_WS      = 3;
static const uint I2S_WS_FRAME_SIZE     = 16;

static const float fs = 44100;


int main() {

    stdio_init_all();

    ADC::init(fs, true, false, 1, defaultADCRIQHandler);
    ADC::enableChannel(ADC_CHANNEL, true);
    ADC::setActiveChannel(ADC_CHANNEL);
    ADC::enableIRQ(true);
    ADC& adc0 = ADC::getActiveChannel();

    I2S_Tx i2sTx;
    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD, fs, I2S_WS_FRAME_SIZE);

    ADC::run(true);
    i2sTx.enable(true);
    static const float conversion_factor = 3.3f / (1 << 12);
    while(1) {
        if (adc0.newValue()) {
            uint16_t rawInputValue = adc0.rawValue();
            i2sTx.queue(rawInputValue << (I2S_WS_FRAME_SIZE - 12), 0);
            printf("%02x :: %f\n", rawInputValue, rawInputValue * conversion_factor);
        }
    }
}