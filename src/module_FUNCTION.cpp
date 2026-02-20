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
static volatile uint8_t DOWNSAMPLE_FACTOR   = 16;
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
// CORE 1 (OLED DISPLAY) MAIN
void core1_entry() {
    // INITIALIZE OLED SCREEN
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX, GPIO_FUNC_SPI);
    OLED oled(SPI_INSTANCE, PIN_CS, PIN_DC, PIN_RST, 128, 64);

    sleep_ms(500);
    if (!oled.begin(10 * 1000 * 1000)) {
        // OLED Initialization failed, LED on feather will blink
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(500);
            gpio_put(LED_PIN, 0);
            sleep_ms(500);
        }
    }

    // Startup splash screen
    oled.clearDisplay();
    oled.setCursor(10, 5);
    oled.setTextSize(2);
    oled.setTextColor(true);
    oled.print("Modular \n Audio \n Processor");
    oled.display();
    sleep_ms(2000);

    // Circular buffer for samples
    static const int CIRCULAR_BUFFER_SIZE = 512; // Larger buffer for better history
    uint32_t circularBuffer[CIRCULAR_BUFFER_SIZE] = {0};
    int bufferWriteIndex = 0;
    
    // For display timing
    uint32_t lastDisplayTime = 0;
    const uint32_t DISPLAY_INTERVAL_MS = 33; // ~30fps
    
    while(1) {
        // Read all available samples from queue (non-blocking)
        uint32_t sample;
        while (queue_try_remove(&sharedQueue, &sample)) {
            circularBuffer[bufferWriteIndex] = sample;
            bufferWriteIndex = (bufferWriteIndex + 1) % CIRCULAR_BUFFER_SIZE;
        }
        
        // Update display at fixed interval
        uint32_t now = time_us_32() / 1000; // Convert to ms
        if (now - lastDisplayTime >= DISPLAY_INTERVAL_MS) {
            lastDisplayTime = now;
            oled.clearDisplay();
        for (int x = 0; x < 128; x++) {
            if (x % 4 < 2) {
                oled.drawPixel(x, 32, true); 
            }
        }
            // Draw the waveform
            int lastY = 32; // Start at center
            for (int x = 0; x < 128; x++) {
                // Map screen x coordinate to buffer index
                // This ensures we use the entire circular buffer across the screen width
                int bufferIndex = (bufferWriteIndex - CIRCULAR_BUFFER_SIZE + 
                                  (x * CIRCULAR_BUFFER_SIZE) / 128 + CIRCULAR_BUFFER_SIZE) % CIRCULAR_BUFFER_SIZE;
                
                // Get sample and scale to screen height (64 pixels)
                // Shift right by (I2S_WS_FRAME_WIDTH - 6) to map 16-bit to 0-63 range
                uint8_t y = circularBuffer[bufferIndex] >> (I2S_WS_FRAME_WIDTH - 6);
                
                // Invert Y if needed (0 at top, 63 at bottom)
                y = 63 - y;
                
                // Constrain to screen bounds
                if (y > 63) y = 63;
                
                // Draw line from last point to current point (interpolation)
                if (x > 0) {
                    int diff = y - lastY;
                    if (abs(diff) > 1) {
                        // Draw vertical line to connect points
                        int startY = lastY;
                        int endY = y;
                        if (diff > 0) {
                            for (int dy = startY; dy <= endY; dy++) {
                                oled.drawPixel(x, dy, true);
                            }
                        } else {
                            for (int dy = startY; dy >= endY; dy--) {
                                oled.drawPixel(x, dy, true);
                            }
                        }
                    }
                }
                
                // Draw the main point
                oled.drawPixel(x, y, true);
                lastY = y;
            }
            
            oled.display();
        }
        
        // Small delay to prevent tight loop
        sleep_us(100);
    }
}

    // CORE 0 (IO / FILTER) MAIN
int main() {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    queue_init(&sharedQueue, sizeof(uint32_t), 256);
    multicore_launch_core1(core1_entry);
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

    uint32_t downsampleCounter = 0;
    const uint32_t SAMPLES_PER_PIXEL = 12; // Approx (44100/30)/128
    bool displayQueueFull = false;
    bool displayQueueEmpty = true;
 while(1) {
    if (adc0.newValue()) alpha = clamp(alphaMin, adc0.trueValue() / 3.3f, alphaMax);
    if (alpha == alphaMin) alpha = 0.0f;

    uint32_t rxBuf[reservedMemDepth];
    if (i2sRx.readBuffer(rxBuf)) {
    for (int i = 0; i < reservedMemDepth; i++) {
        filterOutput = currentFilter(uint2float(rxBuf[i]));
        uint32_t output = float2uint(filterOutput);
        i2sTx.queue(output, output);
    downsampleCounter++;
    if (downsampleCounter >= DOWNSAMPLE_FACTOR) {
        queue_try_add(&sharedQueue, &output);
        downsampleCounter = 0;
    }
}
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