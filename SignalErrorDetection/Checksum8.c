#include "Checksum8.h"

void Checksum8Encode(uint8_t* data, uint8_t* encoded) {
    uint8_t checksum = 0; 

    for (uint8_t k = 0; k < Checksum8Settings.dataBytes; k++) {
        checksum += data[k];
        encoded[k + 1] = data[k];
    }

    encoded[0] = -1 * checksum;
}

enum Checksum8__ErrorCode Checksum8Decode(uint8_t* encoded, uint8_t* data) {
    uint8_t sum = encoded[0];

    for (uint8_t k = 1; k < Checksum8Settings.dataBytes + 1; k++) {
        sum += encoded[k];
        data[k-1] = encoded[k];
    }

    return sum == 0 ? Checksum8__valid : Checksum8__error;
}

