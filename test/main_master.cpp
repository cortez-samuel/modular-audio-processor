#include "pico/stdlib.h"
#include "../libraries/oled.h"
#include "../libraries/adc.hpp"
#include "../libraries/I2S_Tx_naive.pio.h"
#include "../libraries/I2S_Tx_compact.pio.h"
#include "../libraries/I2S_Rx_naive.pio.h"
#include "hardware/pio.h"
#include <math.h>
#include <cstdio>
#include <stdio.h>
#include <cstdint>


#define SPI_PORT spi1
#define PIN_SCK 10  //SCL, 10 on feather
#define PIN_TX 11   //SDA, 11 on feather
#define PIN_RST 28  //RES, A2 on feather
#define PIN_DC 29   //DC,  A3 on feather
#define PIN_CS 24   //CS,  24 on feather

#define MASTER false

#define PIN_I2S_TX 0
#define PIN_I2S_RX 1
#define PIN_I2S_BS 2
#define PIN_I2S_WS 3
#define I2S_SAMPLERATE 1000
#define I2S_WIDTH 16


int main(){
    stdio_init_all();

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX, GPIO_FUNC_SPI);
    OLED oled(SPI_PORT, PIN_CS, PIN_DC, PIN_RST, 128, 64);

    sleep_ms(500);
    printf("Starting up...");
    if (!oled.begin(10 * 1000 * 1000)) {
        // OLED Initialization failed, LED on feather will blink
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
    if(MASTER){
        oled.print("TX mode");
    }
    else{
        oled.print("RX mode");
    }
    oled.display();
    sleep_ms(2000);

    PIO pio;
    uint sm;
    uint offset;
    uint16_t wave_buffer[128];
    for (int i = 0; i < 128; i++) {
        wave_buffer[i] = 25;
    }

    if(MASTER){
        pio_claim_free_sm_and_add_program(&I2S_Tx_compact_program, &pio, &sm, &offset);
        I2S_Tx_compact_init(pio, sm, offset, PIN_I2S_BS, PIN_I2S_WS, PIN_I2S_RX, I2S_SAMPLERATE, I2S_WIDTH);

        uint16_t num = 0;
        while (1) {
            I2S_Tx_compact_stereo_write(pio, sm, 0xFFFF, 0x0000, I2S_WIDTH);

        }
    }
    else{
        pio_claim_free_sm_and_add_program(&I2S_Rx_naive_program, &pio, &sm, &offset);
        I2S_Rx_naive_init(pio, sm, offset, PIN_I2S_BS, PIN_I2S_WS, PIN_I2S_RX, I2S_SAMPLERATE, I2S_WIDTH);
        uint32_t lc = 0, rc = 0;

        while(1) {
            I2S_Rx_naive_read(pio, sm, &lc, &rc);
            printf("(%d, %d)\n", lc, rc);
        }
    }

    
}