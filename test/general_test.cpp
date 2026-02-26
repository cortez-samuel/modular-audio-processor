#include "../libraries/adc.hpp"
#include "../libraries/I2S.h"

#include "pico/stdlib.h"
#include "pico/float.h"
#include <cstdio>

static const uint8_t ADC_CHANNEL        = 0;
static const uint8_t PIN_ADC_CHANNEL    = ADC_GET_CHANNEL_PIN(ADC_CHANNEL);
static const uint8_t PIN_I2S_Tx_SD      = 1;
static const uint8_t PIN_I2S_Tx_BCLK    = 2;
static const uint8_t PIN_I2S_Tx_WS      = 3;
static const uint I2S_WS_FRAME_SIZE     = 16;
static const float fs = 48000;

static const float ADC_conversion_factor    = 3.3f / (1 << 12);
static const uint _E = 12;


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

    float voltage;

    while(1) {
        if (adc0.newValue()) {
            voltage = adc0.rawValue() * ADC_conversion_factor;
            printf("Voltage: %f\n", voltage);
            int32_t sentData = (int32_t)(((voltage - 1.5) / 3.3) * 0x7FFF);
            printf("\t sent data: %04x\n", sentData);
            i2sTx.queue(sentData, sentData);
        }
    }
}