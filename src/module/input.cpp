#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "../lib/adc.hpp"
#include "../lib/I2S.h"
#include <cstdio>
#include <cstdint>

    // ADC / AUDIO INPUT
static const uint8_t ADC_CHANNEL = 0;
static const uint8_t PIN_ADC_CHANNEL = ADC_GET_CHANNEL_PIN(ADC_CHANNEL);

    // I2S Tx
static const uint8_t PIN_I2S_Tx_SD = 0;
static const uint8_t PIN_I2S_Tx_BCLK = 2;
static const uint8_t PIN_I2S_Tx_WS = 3;
static const uint I2S_WS_FRAME_SIZE = 16;
static const float fs = 44100;

    // FIXED POINT
static const int _E = 15;
static const float _E_SCALE  = (float)(1 << _E);

static inline int16_t float_to_fix(float x) {
    if(x >=  1.0f){
        return  0x7FFF;
    }
    if(x <= -1.0f){
        return (int16_t)0x8000;
    }
    return (int16_t)(x * _E_SCALE);
}

static inline int16_t adc_raw_to_fix(uint16_t raw12) {
    float u_vol = raw12 * (3.3f / 4096.0f);
    float s_vol = (u_vol - 1.65f) / 1.65f;
    return float_to_fix(s_vol);
}

int main() {
    stdio_init_all();

        // ADC INIT
    ADC::settings = {
        .fs = fs,
        .resolution = 12,
        .freeRun = true,
        .rrEnabled = false,
    };
    ADC adc0(0);
    adc0.enable(true);
    ADC::init(0);

        // I2S_Tx INIT
    static I2S_Tx i2sTx;
    static const uint Tx_reservedMemDepth = 64;
    static const uint Tx_reservedMemWidth = 8;
    static AudioSample_t Tx_reservedMem[Tx_reservedMemDepth * Tx_reservedMemWidth];
    static AudioSample_t Tx_defaultMem[Tx_reservedMemDepth] = {0};
    i2sTx.settings = {
        .i2sSettings    = I2S_Tx::defaultSettings.i2sSettings,
        .bufferWidth    = Tx_reservedMemWidth,
        .bufferDepth    = Tx_reservedMemDepth,
        .reservedMem    = Tx_reservedMem,
        .defaultMem     = Tx_defaultMem,
    };
    i2sTx.settings.i2sSettings.fs           = fs;
    i2sTx.settings.i2sSettings.frameSize    = I2S_WS_FRAME_SIZE;

    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD);

        // ENABLE
    ADC::run(true);
    i2sTx.enable(true);

    int16_t lastSample = 0;
    while (true) {
        if (adc0.newValue()) {
            lastSample = adc_raw_to_fix(adc0.getRaw());
            AudioSample_t word = { (
                uint32_t)(uint16_t)lastSample, 
                (uint32_t)(uint16_t)lastSample 
            };
            bool queued = false;
            while(!queued) { queued = i2sTx.queue(word); }
        }
    }
}
