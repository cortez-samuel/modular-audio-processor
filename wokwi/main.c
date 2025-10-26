#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "oled.h"

float alpha = 0.5;
float last_out = 0;
unsigned int input_buffer[128];
unsigned int filter_buffer[128];
unsigned int index = 0;

float map(float x, float input_min, float input_max, float output_min, float output_max) {
    return (x - input_min) * (output_max - output_min) / (input_max - input_min) + output_min;
}

void draw_waveform(uint8_t id, int *buffer, int y_offset, const char* label){
    oled_draw_text(id, 0, y_offset, label);
    for(unsigned int i = 0; i < 128; i++){
        oled_set_pixel(id, i, y_offset + 10, 1);
        oled_set_pixel(id, i, y_offset + 119, 1);
    }
    unsigned int next_index = (index + 127) % 128;
    for(unsigned int i = 0; i < 128; i++){
        int offset = 127 - i;
        int buffer_index = (next_index - offset + 128) % 128;
        float y = map(buffer[buffer_index], 0, 4095, 0, 107); 
        oled_set_pixel(id, i, (y_offset + 118) - y, 1);
    }
}

int main(){
    stdio_init_all();
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);
    gpio_init(28);
    gpio_set_dir(28, GPIO_IN);
    gpio_pull_up(28);
    oled_init(0);
    oled_init(1);
    adc_select_input(0);
    int init_val = adc_read();
    last_out = (float)init_val;
    for(unsigned int i = 0; i < 128; i++){
      input_buffer[i] = init_val;
      filter_buffer[i] = init_val;
    }
    while(1){
        if(!gpio_get(28)){
            adc_select_input(1);
            int cutoff = adc_read();
            alpha = map(cutoff, 0, 4095, 5, 95) / 100.0;
            sleep_ms(100);
        }
        adc_select_input(0);
        int og_signal = adc_read();
        float filter_output = (1.0 - alpha) * og_signal + alpha * last_out;
        last_out = filter_output;
        input_buffer[index] = og_signal;
        filter_buffer[index] = (int)filter_output;
        index = (index + 1) % 128;
        oled_fill(0, 0);
        oled_fill(1, 0);
        draw_waveform(0, input_buffer, 5, "OG");
        draw_waveform(1, filter_buffer, 5, "LPF");
        oled_update(0);
        oled_update(1);
        sleep_ms(10);
    }
}