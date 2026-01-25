#include "../libraries/SPI.pio.h"
#include "../libraries/SPI_Byte.pio.h"

#include "pico/stdlib.h"
#include <cstdio>

int main() {

    stdio_init_all();

    PIO pio;
    uint sm;
    uint offset;

    //pio_claim_free_sm_and_add_program(&SPI_Byte_Master_program, &pio, &sm, &offset);
    //SPI_Byte_Master_init(pio, sm, offset, 18, 19, 20, 1, 2000, true);

    pio_claim_free_sm_and_add_program(&SPI_Byte_Slave_program, &pio, &sm, &offset);
    SPI_Byte_Slave_init(pio, sm, offset, 19, 18, 1, 20, 2000, false);

    uint32_t src = 0;
    //uint32_t src = 0xAABBCCDD;
    uint8_t* srcptr = (uint8_t*)&src;

    while(1) {
        //SPI_Byte_Master_w(pio, sm, 1, srcptr, 4);
        uint len = SPI_Byte_Slave_r(pio, sm, 20, srcptr, 4);
        printf("0x%02x 0x%02x 0x%02x 0x%02x :: %u\n", 
            src & 0xFF, (src >> 8) & 0xFF, (src >> 16) & 0xFF, (src >> 24) & 0xFF, 
            len
        );
        sleep_us(250);
    }
}