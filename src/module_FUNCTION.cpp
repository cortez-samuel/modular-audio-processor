#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/float.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"

#include "../libraries/I2S.h"
#include "../libraries/oled.h"
#include "../libraries/adc.hpp"

    // LED PIN
static const uint LED_PIN       = 13;
static volatile uint LED_PIN_VALUE     = 0;

    // OLED
static const auto SPI_INSTANCE          = spi1;
static const uint8_t PIN_SCK            = 10;
static const uint8_t PIN_TX             = 11;
static const uint8_t PIN_RST            = 28;
static const uint8_t PIN_DC             = 29;
static const uint8_t PIN_CS             = 24;

    // SHARED STUFF
static volatile uint8_t DOWNSAMPLE_FACTOR   = 1;

static const uint8_t SHARED_BUFFER_WIDTH    = 64;
static const uint8_t SHARED_BUFFER_DEPTH    = 128;

static queue_t sharedQueue;

    // I2S TX
static const uint8_t PIN_I2S_Tx_SD      = 0;
static const uint8_t PIN_I2S_Tx_BCLK    = 2;
static const uint8_t PIN_I2S_Tx_WS      = 3;
static I2S_Tx i2sTx;

    // I2S RX
static const uint8_t PIN_I2S_Rx_SD      = 7;
static const uint8_t PIN_I2S_Rx_BCLK    = 8;
static const uint8_t PIN_I2S_Rx_WS      = 9;
static I2S_Rx i2sRx;
static const uint32_t reservedMemDepth  = 128;
static uint32_t reservedMem[reservedMemDepth * RxPingPong::WIDTH];

    // I2S GENERAL
static const uint I2S_WS_FRAME_WIDTH    = 16;
static const float fs                   = 44100;

    // ADC / USER INPUT
static const uint8_t ADC_CHANNEL        = 0;
static const uint8_t PIN_ADC_CHANNEL    = ADC_GET_CHANNEL_PIN(ADC_CHANNEL);

    // FILTERS
static float alpha                      = 0.0f;
static float alphaMin                   = 0.033f;
static float alphaMax                   = 1.0f;
static float (*currentFilter)(float)    = nullptr;
static float filterOutput               = 0;

static float Pass(float x);
static float LowPass(float x);
static float HighPass(float x);

enum class Mode {
    Pass, 
    Lowpass,
    Highpass
};
static Mode mode = Mode::Pass;

    // helper functions
static inline float clamp(float min, float x, float max) {
    return min > x ? min: (max < x ? max: x);
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////


    // CORE 1 (OLED DISPLAY) MAIN
void core1_entry() {
        // INITIALIZE OLED SCREEN
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX, GPIO_FUNC_SPI);
    OLED oled(SPI_INSTANCE, PIN_CS, PIN_DC, PIN_RST, 128, 64);

    sleep_ms(500);
    printf("Starting up...");
    if (!oled.begin(10 * 1000 * 1000)) {
        // OLED Initialization failed, LED on feather will blink
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(500);
            gpio_put(LED_PIN, 0);
            sleep_ms(500);
            printf("failed\n");
        }
    }

        // Startup splash screen
    oled.clearDisplay();
    oled.setCursor(10, 28);
    oled.setTextSize(2);
    oled.setTextColor(true);
    oled.print("\\_(^v^)_/");
    oled.display();
    sleep_ms(2000);

    uint32_t displayBuffer[128];
    while(1) {
            // draw to display
        oled.clearDisplay();
        for (int x = 0; x < 127; x++) {
            displayBuffer[x] = displayBuffer[x+1];
            uint8_t y = displayBuffer[x] >> (I2S_WS_FRAME_WIDTH - 6);
            oled.drawPixel(x, y, true);
        }
        queue_remove_blocking(&sharedQueue, &displayBuffer[127]);
        uint8_t y = displayBuffer[127] >> (I2S_WS_FRAME_WIDTH - 6);
        oled.drawPixel(127, y, true);
        oled.display();


        gpio_put(LED_PIN, LED_PIN_VALUE);
        LED_PIN_VALUE = 1 - LED_PIN_VALUE;
    }
}

    // CORE 0 (IO / FILTER) MAIN
int main() {
        // init stdio io
    stdio_init_all();

        // init debug LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    //gpio_put(LED_PIN, LED_PIN_VALUE);

        // init shared queue
    queue_init(&sharedQueue, sizeof(uint32_t), SHARED_BUFFER_DEPTH * SHARED_BUFFER_WIDTH);


        // launch core 1
    multicore_launch_core1(core1_entry);


        // Initialize I2S_Tx and I2S_Rx
    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD, fs, I2S_WS_FRAME_WIDTH);

    i2sRx.setReservedMem(reservedMem, reservedMemDepth);
    i2sRx.init(PIN_I2S_Rx_BCLK, PIN_I2S_Rx_WS, PIN_I2S_Rx_SD, fs, I2S_WS_FRAME_WIDTH);

        // run I2S_Tx and I2S_Rx
    i2sTx.enable(true);
    i2sRx.enable(true);

        // Initialize ADC
    ADC::init(10, true, false, 1, defaultADCRIQHandler);
    ADC::enableChannel(ADC_CHANNEL, true);
    ADC::setActiveChannel(ADC_CHANNEL);
    ADC::enableIRQ(true);
    ADC& adc0 = ADC::getActiveChannel();

        // set current filter
    currentFilter = Pass;
    mode = Mode::Pass;

        // initialise filter alpha input
    gpio_init(6);
    gpio_set_dir(6, GPIO_IN);
    gpio_pull_down(6);

        // run ADC
    ADC::run(true);

        // main loop
    uint32_t LC = 0, RC = 0;

    uint8_t downsampleCounter = 0;
    bool displayQueueFull = false;
    bool displayQueueEmpty = true;
    while(1) {
            // get alpha from ADC
        if (adc0.newValue()) alpha = clamp(alphaMin, adc0.trueValue() / 3.3f, alphaMax);
        if (alpha == alphaMin) alpha = 0.0f;

            // get I2S_Rx data
        bool valid = i2sRx.read(LC, RC);
            // filter I2S_Rx data
        filterOutput = currentFilter(uint2float(LC));
        uint32_t output = float2uint(filterOutput);
            // send output to I2S_Tx
        i2sTx.queue(output, RC);

            // send output to buffer ot be displayed
        downsampleCounter++;
        if (downsampleCounter == DOWNSAMPLE_FACTOR) {
            queue_try_add(&sharedQueue, &output);
            downsampleCounter = 0;
        }
    }
    
}


/////////////////////////////////////

float Pass(float x) {
    return x;
}

float LowPass(float x) {
        // y[n] = alpha x[n] + (1 - alpha) y[n-1]
    static float y = 0;
    y = alpha * x + (1 - alpha) * y;
    printf("\ty_n = %f -- alpha = %f\n", y, alpha);
    return y;
}

float HighPass(float x) {
        // y[n] = alpha * (y[n-1] + x[n] - x[n-1]);
    static float y = 0;
    static float _x = 0;
    y = alpha * (y + x - _x);
    _x = x;
    return y;
}