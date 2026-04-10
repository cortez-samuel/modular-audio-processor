#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"

#include "../lib/AudioSample.hpp"
#include "../lib/I2S.h"
#include "../lib/oled.h"
#include "../lib/adc.hpp"
#include "../lib/fft.hpp"
#include "../lib/PushButton.hpp"
#include "../lib/RotaryEncoder.hpp"
#include "../lib/filters.h"

    // LED PIN
static const uint LED_PIN           = 13;
static volatile uint LED_PIN_VALUE  = 0;

    // OLED
static const auto    SPI_INSTANCE   = spi1;
static const uint8_t PIN_SCK        = 10;
static const uint8_t PIN_TX         = 11;
static const uint8_t PIN_RST        = 28;
static const uint8_t PIN_DC         = 29;
static const uint8_t PIN_CS         = 24;

    // SHARED STUFF
static const uint8_t DOWNSAMPLE_FACTOR   = 16;
static const uint8_t SHARED_BUFFER_WIDTH = 64;
static const uint8_t SHARED_BUFFER_DEPTH = 128;
static queue_t sharedQueue;

    // I2S Tx
static const uint8_t PIN_I2S_Tx_SD = 0;
static const uint8_t PIN_I2S_Tx_BCLK = 2;
static const uint8_t PIN_I2S_Tx_WS = 3;

    // I2S Rx
static const uint8_t PIN_I2S_Rx_SD   = 7;
static const uint8_t PIN_I2S_Rx_BCLK = 8;
static const uint8_t PIN_I2S_Rx_WS   = 9;

    // I2S GENERAL
static const uint  I2S_WS_FRAME_WIDTH = 16;
static const float fs                 = 44100;

    // FILTERS
static float volatile alpha          = 0.0f;
static float alphaMin                = 0.0f;
static float alphaMax                = 1.0f;
static const uint FILTER_BUFFER_DEPTH   = 128;
static float buffer_X[FILTER_BUFFER_DEPTH];
static float buffer_Y[FILTER_BUFFER_DEPTH];
static CyclicBuffer_t<float> cyclicBuffer_X(buffer_X, FILTER_BUFFER_DEPTH);
static CyclicBuffer_t<float> cyclicBuffer_Y(buffer_Y, FILTER_BUFFER_DEPTH);
static FilterInstance_t* currentFilter  = nullptr;

static FilterInstance_t FILTER_PASS {
    .filter_name    = "SRC",
    .filter         = Filters::PASS,
    .x              = &cyclicBuffer_X,
    .y              = &cyclicBuffer_Y,
};
static FilterInstance_t FILTER_LPF {
    .filter_name    = "LPF",
    .filter         = Filters::FirstOrderIIR::LPF,
    .x              = &cyclicBuffer_X,
    .y              = &cyclicBuffer_Y,
};
static FilterInstance_t FILTER_HPF {
    .filter_name    = "HPF",
    .filter         = Filters::FirstOrderIIR::HPF,
    .x              = &cyclicBuffer_X,
    .y              = &cyclicBuffer_Y,
};

enum class Mode { 
    Pass, 
    Lowpass, 
    Highpass, 
    FFT 
};
static Mode mode = Mode::Pass;

    // PUSH BUTTON
static const uint8_t PIN_PUSH_BUTTON_CHANGEMODE     = 12;
static const uint64_t PUSH_BUTTON_DEBOUNCE_TIME_us  = 50000;

    // ROTARY ENCODER
static const uint8_t PIN_ROTARY_ENCODER_A               = 1;
static const uint8_t PIN_ROTARY_ENCODER_B               = 6;
static const uint64_t ROTARY_ENCODER_DEBOUNCE_TIME_us   = 10000;
static const uint8_t ROTARY_ENCODER_MIN_POSITION        = 0;
static const uint8_t ROTARY_ENCODER_MAX_POSITION        = 100;


    //  FIXEDPOINT CONVERSIONS
static const float _E_SCALE = 32768.0f;
static inline float fix_to_float(int16_t x) {
    return (float)x / _E_SCALE;
}
        // float in [-1, 1)  ->  int16_t  (saturating)
static inline int16_t float_to_fix(float x) {
    if (x >=  1.0f) return  0x7FFF;
    if (x <= -1.0f) return (int16_t)0x8000;
    return (int16_t)(x * 32767.0f);
}

    // HELPER FUNCTIONS
static inline float clamp_f(float lo, float x, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

    // CALLBACKS
static void changeModeCallback(PushButton* pushButton, PushButton::State_t next) {
    switch (mode) {
        case Mode::Pass: mode = Mode::Lowpass;  currentFilter = &FILTER_LPF; break;
        case Mode::Lowpass: mode = Mode::Highpass; currentFilter = &FILTER_HPF; break;
        case Mode::Highpass: mode = Mode::FFT; currentFilter = &FILTER_PASS; break;
        case Mode::FFT: mode = Mode::Pass; currentFilter = &FILTER_PASS; break;
    }
}

// CORE 1 (OLED DISPLAY) MAIN
void core1_entry() {
        // Initialize OLED
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX,  GPIO_FUNC_SPI);
    OLED oled(SPI_INSTANCE, PIN_CS, PIN_DC, PIN_RST, 128, 64);
    sleep_ms(500);
    if(!oled.begin(10 * 1000 * 1000)){
        while (true) {
            gpio_put(LED_PIN, 1); sleep_ms(500);
            gpio_put(LED_PIN, 0); sleep_ms(500);
        }
    }

        // Startup splash
    oled.clearDisplay();
    oled.setCursor(10, 5);
    oled.setTextSize(2);
    oled.setTextColor(true);
    oled.print("Modular \n Audio \n Processor");
    oled.display();
    sleep_ms(2000);


    GPIO_IRQManager::init();
        // PushButton init
    PushButton changeModePushButton;
    changeModePushButton.settings = {
        .debounceTime_us    = PUSH_BUTTON_DEBOUNCE_TIME_us,
        .onDown             = changeModeCallback,
        .onDownEnabled      = false,
        .onUp               = changeModeCallback,
        .onUpEnabled        = true,
    };
    changeModePushButton.begin(PIN_PUSH_BUTTON_CHANGEMODE);

        // Rotary Encoder init
    RotaryEncoder alphaRotaryEncoder;
    alphaRotaryEncoder.settings = {
        .debounceTime_us    = ROTARY_ENCODER_DEBOUNCE_TIME_us,
        .onInc              = nullptr,
        .onIncEnabled       = false,
        .onDec              = nullptr,
        .onDecEnabled       = false,
    };
    alphaRotaryEncoder.begin(PIN_ROTARY_ENCODER_A, PIN_ROTARY_ENCODER_B);

        // Circular buffer – stores int16_t bit-patterns in the low 16 bits of uint32_t.
    static const int CIRCULAR_BUFFER_SIZE = 512;
    AudioSample_t circularBuffer[CIRCULAR_BUFFER_SIZE] = {0};
    int bufferWriteIndex = 0;
    uint32_t lastDisplayTime = 0;
    const uint32_t DISPLAY_INTERVAL_MS = 33; // ~30 fps

    while (true) {
            // check alphaRotaryEncoder position
        int alphaRotaryEncoderPosition = alphaRotaryEncoder.getState();
        if (alphaRotaryEncoderPosition > ROTARY_ENCODER_MAX_POSITION) {
            alphaRotaryEncoderPosition = ROTARY_ENCODER_MAX_POSITION;
            alphaRotaryEncoder.setState(ROTARY_ENCODER_MAX_POSITION);
        }
        if (alphaRotaryEncoderPosition < ROTARY_ENCODER_MIN_POSITION) {
            alphaRotaryEncoderPosition = ROTARY_ENCODER_MIN_POSITION;
            alphaRotaryEncoder.setState(ROTARY_ENCODER_MIN_POSITION);
        }

        alpha = alphaRotaryEncoderPosition/100.0f;

            // Drain the inter-core queue
        AudioSample_t word;
        while(queue_try_remove(&sharedQueue, &word)){
            circularBuffer[bufferWriteIndex] = word;
            bufferWriteIndex = (bufferWriteIndex + 1) % CIRCULAR_BUFFER_SIZE;
        }
        uint32_t now = time_us_32() / 1000;
        if(now - lastDisplayTime >= DISPLAY_INTERVAL_MS){
            lastDisplayTime = now;
            oled.clearDisplay();
            if(mode == Mode::FFT){
                fi16 fftIn[256];
                for(int i = 0; i < 256; i++){
                    fftIn[i] = (fi16)(int16_t)(uint16_t)circularBuffer[i].LC;
                }
                int32_t mean = 0;
                for(int i = 0; i < 256; i++){ mean += fftIn[i];}
                mean >>= 8;
                for(int i = 0; i < 256; i++){ fftIn[i] -= (fi16)mean;}
                fi16_32 fftOut[256];
                fft_fixed(fftIn, fftOut);
                fi16_32 fftOut_max = 1;
                for(int i = 0; i < 128; i++){
                    if (fftOut[i] > fftOut_max) fftOut_max = fftOut[i];
                }
                bool max_gt_64k = fftOut_max > 65535;
                uint32_t scale = max_gt_64k
                                             ? (uint32_t)(fftOut_max / 65535 + 1)
                                             : (uint32_t)(65535 / fftOut_max);

                for(int x = 0; x < 128; x++){
                    uint16_t dp = (uint16_t)(max_gt_64k
                                                 ? (fftOut[x] / scale)
                                                 : (fftOut[x] * scale));
                    uint8_t y = 63 - (uint8_t)(dp >> 10);
                    for (int row = 63; row > (int)y; row--) {
                        oled.drawPixel(x, row, true);
                    }
                }
            }
            else{
                for(int x = 0; x < 128; x++){
                    if(x % 4 < 2){
                        oled.drawPixel(x, 32, true);
                    }
                }
                int lastY = 32;
                for(int x = 0; x < 128; x++){
                    int bufIdx = (bufferWriteIndex + x * 4) % CIRCULAR_BUFFER_SIZE;
                    int16_t s = (int16_t)(uint16_t)circularBuffer[bufIdx].LC;
                    uint16_t offset_bin = (uint16_t)s ^ 0x8000u;
                    uint8_t y = 63 - (uint8_t)(offset_bin >> 10);
                    if(y > 63){
                        y = 63;
                    }
                    if(x > 0){
                        int diff = (int)y - lastY;
                        if(diff > 0){
                            for(int dy = lastY; dy <= (int)y; dy++)
                                oled.drawPixel(x, dy, true);
                        }
                        else if (diff < 0) {
                            for (int dy = lastY; dy >= (int)y; dy--)
                                oled.drawPixel(x, dy, true);
                        }
                    }
                    oled.drawPixel(x, y, true);
                    lastY = y;
                }
            }
            oled.setTextSize(1);
            oled.setTextColor(true);
            oled.setCursor(0, 0);
            switch(mode){
                case Mode::FFT: oled.print("FFT"); break;
                default:        oled.print(currentFilter->filter_name); break;    
            }
            if(mode != Mode::Pass && mode != Mode::FFT){
                char buf[12];
                snprintf(buf, sizeof(buf), "a=%.2f", (float)alpha);
                oled.setCursor(128 - 6 * 6, 0);
                oled.print(buf);
            }
            oled.display();
        }
        sleep_us(100);
    }
}


// CORE 0 (IO / FILTER) MAIN
int main() {
    stdio_init_all();

    // LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Inter-core queue: element = uint32_t holding an int16_t bit-pattern.
    queue_init(&sharedQueue, sizeof(AudioSample_t), 256);
    multicore_launch_core1(core1_entry);

    static I2S_Tx i2sTx;
    static const uint32_t Tx_reservedMemDepth = 64;
    static const uint32_t Tx_reservedMemWidth = 8;
    static AudioSample_t Tx_reservedMem[Tx_reservedMemDepth * Tx_reservedMemWidth];
    static AudioSample_t Tx_defaultMem[Tx_reservedMemDepth] = {0};
    i2sTx.settings = {
        .i2sSettings    = I2S_Tx::defaultSettings.i2sSettings,
        .bufferWidth    = Tx_reservedMemWidth,
        .bufferDepth    = Tx_reservedMemDepth,
        .reservedMem    = Tx_reservedMem,
        .defaultMem     = Tx_defaultMem,
    };
    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD);

    static I2S_Rx i2sRx;
    static const uint32_t Rx_reservedMemDepth = 64;
    static AudioSample_t Rx_reservedMem[Rx_reservedMemDepth * RxPingPong::WIDTH];
    i2sRx.settings = {
        .i2sSettings    = I2S_Rx::defaultSettings.i2sSettings,
        .bufferDepth    = Rx_reservedMemDepth,
        .reservedMem    = Rx_reservedMem,
    };
    i2sRx.init(PIN_I2S_Rx_BCLK, PIN_I2S_Rx_WS, PIN_I2S_Rx_SD);
    
    i2sTx.enable(true);
    i2sRx.enable(true);

    // initialize filter
    currentFilter = &FILTER_PASS;
    mode = Mode::Pass;

    uint32_t downsampleCounter = 0;

    while (true) {    

        AudioSample_t rxBuf[Rx_reservedMemDepth];
        AudioSample_t txBuf[Tx_reservedMemDepth];
        if (i2sRx.readBuffer(rxBuf)) {
            bool queuedValid = true;
            for (uint i = 0; i < Rx_reservedMemDepth; i++) {
                    // get sample
                int16_t raw_sample_LC = (int16_t)(uint16_t)rxBuf[i].LC;
                int16_t raw_sample_RC = (int16_t)(uint16_t)rxBuf[i].RC;
                int16_t raw_sample = (raw_sample_LC >> 1) + (raw_sample_RC >> 1);
                    // do filter
                float s_vol = fix_to_float(raw_sample);
                float filtered_vol = call_filter(currentFilter, s_vol, alpha);
                filtered_vol = clamp_f(-1.0f, filtered_vol, 1.0f);
                int16_t out_sample = float_to_fix(filtered_vol);
                    // output sample
                AudioSample_t tx_word {
                    (uint32_t)(uint16_t)out_sample,
                    (uint32_t)(uint16_t)out_sample,
                };
                queuedValid = i2sTx.queue(tx_word) & queuedValid;

                downsampleCounter++;
                if (downsampleCounter >= DOWNSAMPLE_FACTOR) {
                    downsampleCounter = 0;
                    queue_try_add(&sharedQueue, &tx_word);
                }
            }
        }
    }
}
