#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include <stdio.h>
#include "oled.h"
#include "SignalErrorDetection.h"
#include "filters.h"
#include "persistent_state.h"

float alpha = 0.5;

FilterFunc cur_filter;
unsigned int _cur_filter_index = 0;

unsigned int input_buffer[128];
unsigned int filter_buffer[128];
unsigned int index = 0;

void next_filter() {
    _cur_filter_index = (_cur_filter_index + 1) % FILTER_COUNT;
    cur_filter = available_filters[_cur_filter_index];
}

const char* cur_filter_name(void) {
    return filter_names[_cur_filter_index];
}

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

void testChecksum8();
void testCRC8();

int main(){
    stdio_init_all();
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);

    gpio_init(28);
    gpio_set_dir(28, GPIO_IN);
    gpio_pull_up(28);

	gpio_init(12)
    gpio_set_dir(12, GPIO_IN);
    gpio_pull_up(12);

    gpio_init(22);
    gpio_set_dir(22, GPIO_IN);
    gpio_pull_up(22);

    spi_init(spi0, 1000 * 1000);
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(7, GPIO_FUNC_SPI);
    gpio_set_function(6, GPIO_FUNC_SPI);
    gpio_set_function(5, GPIO_FUNC_SPI);

    persistent_init();
    PersistentState state = persistent_load();
    alpha = state.alphaParam;

    oled_init(0);
    oled_init(1);
    adc_select_input(0);
    int init_val = adc_read();
    for(unsigned int i = 0; i < 128; i++){
      input_buffer[i] = init_val;
      filter_buffer[i] = init_val;
    }

    testChecksum8();
    testCRC8();

    cur_filter = available_filters[_cur_filter_index];

    while(1){
        if(!gpio_get(28)){
            adc_select_input(1);
            int cutoff = adc_read();
            alpha = map(cutoff, 0, 4095, 5, 95) / 100.0;
            sleep_ms(100);
        }

        if(!gpio_get(22)){
            state.alphaParam = alpha;
            persistent_save(state);
        }

        if(!gpio_get(12)){
            next_filter();
        }

        adc_select_input(0);
        int og_signal = adc_read();
        uint16_t spi_data = (uint16_t)og_signal;
        spi_write16_blocking(spi0, &spi_data, 1);
        input_buffer[index] = og_signal;
        float filter_output = cur_filter(input_buffer, filter_buffer, index, alpha);
        filter_buffer[index] = (int)filter_output;
        index = (index + 1) % 128;
        oled_fill(0, 0);
        oled_fill(1, 0);
        draw_waveform(0, input_buffer, 5, "OG");
        draw_waveform(1, filter_buffer, 5, cur_filter_name());
        oled_update(0);
        oled_update(1);
        sleep_ms(50);
    }
}

#include <stdlib.h>
#include <time.h>
void testChecksum8() {
  uint32_t data = 0x0027A0FF;
  uint32_t expectedEncoding = 0x27A1FF3A;
  uint32_t encoding;
  uint32_t badEncoding;
  Checksum8__ErrorCode err;
  uint32_t decodedData;

  char printBuffer[128];

  Checksum8Encode(&data, &encoding);
  //printf("%08x", encoding);
  err = Checksum8Decode(&encoding, &decodedData);

  printf("--- Checksum8 detecting errors in valid data ---\n");
  printf("error vector: %x\n", 0);
  printf("error detected: %i\n", err);

  printf("----- Checksum8 testing for error detection -----\n");
  srand(0x7ff5);
  uint16_t missed;
  for (int k = 0; k < 1024; k++) {
    uint32_t errVector = (uint32_t)(rand() & 0xffffffff);
    badEncoding = data ^ errVector;
    err = Checksum8Decode(&badEncoding, &decodedData);
    missed += 1 - err;
  }
  printf("errors: %i\nerrors missed: %i\n", 1024, missed);
}

void testCRC8() {
  uint32_t data = 0x0027A0FF;
  uint32_t expectedEncoding = 0x27A1FFC6;
  uint32_t encoding;
  uint32_t badEncoding;
  Checksum8__ErrorCode err;
  uint32_t decodedData;

  char printBuffer[128];

  CRC8Encode(&data, &encoding);
  //printf("%08x", encoding);
  err = CRC8Decode(&encoding, &decodedData);

  printf("--- CRC8 detecting errors in valid data ---\n");
  printf("error vector: %x\n", 0);
  printf("error detected: %i\n", err);

  printf("----- CRC8 testing for error detection -----\n");
  srand(0x7ff5);
  uint16_t missed = 0;
  for (int k = 0; k < 1024; k++) {
    uint32_t errVector = (uint32_t)(rand() & 0xffffffff);
    badEncoding = data ^ errVector;
    err = Checksum8Decode(&badEncoding, &decodedData);
    missed += 1 - err;
  }
  printf("errors: %i\nerrors missed: %i\n", 1024, missed);
}
