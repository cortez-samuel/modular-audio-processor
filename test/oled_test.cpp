#include "pico/stdlib.h"
#include "../libraries/oled.h"
#include "../libraries/adc.hpp"
#include <math.h>
#include <cstdio>
#include <stdio.h>
#include "stdint.h"

#define SPI_PORT spi1
#define PIN_SCK 26  //SCL, A0 on feather
#define PIN_TX 27   //SDA, A1 on feather
#define PIN_RST 28  //RES, A2 on feather
#define PIN_DC 29   //DC,  A3 on feather
#define PIN_CS 24   //CS,  24 on feather

// For the sine wave animation
const uint8_t sineLookupTable[] = {
    25, 26, 27, 29, 30, 31, 32, 33,
    35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 44, 45, 46, 46, 47, 48,
    48, 49, 49, 49, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 49, 49, 49,
    48, 48, 47, 46, 46, 45, 44, 44,
    43, 42, 41, 40, 39, 38, 37, 36,
    35, 33, 32, 31, 30, 29, 27, 26,
    25, 24, 23, 21, 20, 19, 18, 17,
    15, 14, 13, 12, 11, 10, 9, 8,
    7, 6, 6, 5, 4, 4, 3, 2,
    2, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 1,
    2, 2, 3, 4, 4, 5, 6, 6,
    7, 8, 9, 10, 11, 12, 13, 14,
    15, 17, 18, 19, 20, 21, 23, 24
};

int main() {
    stdio_init_all();

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX, GPIO_FUNC_SPI);
    OLED oled(SPI_PORT, PIN_CS, PIN_DC, PIN_RST, 128, 64);
    
    if (!oled.begin(10 * 1000 * 1000)) {
        // Initialization failed, LED on feather will blink
        const uint LED_PIN = 13;
        gpio_init(LED_PIN);
        gpio_set_dir(LED_PIN, GPIO_OUT);
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(500);
            gpio_put(LED_PIN, 0);
            sleep_ms(500);
        }
    }
    
    // Startup splash screen
    oled.clearDisplay();
    oled.setCursor(10, 28);
    oled.setTextSize(2);
    oled.setTextColor(true);
    oled.print("haaii :3");
    oled.display();
    sleep_ms(2000);
    
    // Stuff for FPS counter
    uint32_t frame_count = 0;
    uint32_t last_fps_time = to_ms_since_boot(get_absolute_time());
    uint32_t fps = 0;
    char fps_buffer[16];
    
    // Stuff for sine wave anim
    uint8_t sine_index = 0;
    uint8_t wave_buffer[128];
    for (int i = 0; i < 128; i++) {
        wave_buffer[i] = 25;
    }

    // Main display loop
    while (1) {
        oled.clearDisplay();

        // Can put other stuff here

        // Sine wave anim
        for (int i = 0; i < 127; i++) {
            wave_buffer[i] = wave_buffer[i + 1];
        }
        wave_buffer[127] = sineLookupTable[sine_index];
        for (int x = 0; x < 128; x++) {
            uint8_t y = wave_buffer[x] + 13;
            oled.drawPixel(x, y, true);
        }
        sine_index++;
        if (sine_index >= 128) {
            sine_index = 0;
        }

        // FPS counter
        oled.setTextSize(1);
        oled.setCursor(80, 0);
        snprintf(fps_buffer, sizeof(fps_buffer), "FPS:%lu", fps);
        oled.print(fps_buffer);
        frame_count++;
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        uint32_t elapsed = current_time - last_fps_time;
        if (elapsed >= 1000) {
            fps = (frame_count * 1000) / elapsed;
            frame_count = 0;
            last_fps_time = current_time;
        }
        
        oled.display();
    }
    
    return 0;
}