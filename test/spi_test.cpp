#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/timer.h"

volatile bool trigger_spi_transfer = false;
volatile uint16_t sample_to_send = 0;

bool repeating_timer_callback(struct repeating_timer *t){
    sample_to_send++; 
    trigger_spi_transfer = true;
    return true;
}

int main(){
    stdio_init_all();
    spi_init(spi0, 8000000);
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(16, GPIO_FUNC_SPI);
    gpio_set_function(18, GPIO_FUNC_SPI);
    gpio_set_function(19, GPIO_FUNC_SPI);
    bi_decl(bi_3pins_with_func(16, 19, 18, GPIO_FUNC_SPI));
    gpio_init(17);
    gpio_set_dir(17, GPIO_OUT);
    gpio_put(17, 1);
    bi_decl(bi_1pin_with_func(17, GPIO_FUNC_SIO));
    long period_us = 23; 
    struct repeating_timer timer;
    add_repeating_timer_us(period_us, repeating_timer_callback, NULL, &timer);
    uint16_t received_data;
    while(true){
        __wfi();
        if(trigger_spi_transfer){
            trigger_spi_transfer = false;
            uint16_t data_to_send = sample_to_send;
            gpio_put(17, 0);
            spi_write_read_16_blocking(spi0, &data_to_send, &received_data, 1);
            gpio_put(17, 1);
        }
    }
    return 0;
}