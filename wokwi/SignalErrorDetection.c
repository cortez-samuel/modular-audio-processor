#include "SignalErrorDetection.h"

#include <stdio.h>

void Checksum8Encode(uint32_t* data, uint32_t* encoded) {
    uint8_t checksum = 0; 

    uint32_t X = *data;
    for (uint8_t k = 0; k < 3; k++) {
        checksum += X & 0xFF;
        X = X >> 8;
    }

    checksum = -1 * checksum;
    *encoded = ((*data) << 8) | checksum;
}

Checksum8__ErrorCode Checksum8Decode(uint32_t* encoded, uint32_t* data) {
    uint8_t sum = 0;

    uint32_t X = *encoded;
    for (uint8_t k = 0; k < 4; k++) {
        sum += X & 0xFF;
        X = X >> 8;
    }
    *data = (*encoded) >> 8;
    return sum == 0 ? Checksum8__valid : Checksum8__error;
}


////////////////////


static uint8_t CalculateCRC8Remainder(uint32_t X) {
    uint8_t shift;

    for (uint8_t k = (4 << 3) - 1; k > 7; k--) {
        if (X & (1 << k)) {
            shift = k - 8 * (4 - 3);
            X ^= (0b100110111 << shift);
        }
    }

    return X & 0xFF;
}

void CRC8Encode(uint32_t* data, uint32_t* encoded) {
    uint32_t X = (*data) << 8;

    uint8_t R = CalculateCRC8Remainder(X);

    *encoded = ((*data) << 8) | R;
}

CRC8__ErrorCode CRC8Decode(uint32_t* encoded, uint32_t* data) {
    uint32_t X = *encoded;

    uint8_t R = CalculateCRC8Remainder(X);

    *data = ((*encoded) >> 8);

    if (R == 0) {
        return CRC8__valid;
    }
    return CRC8__error;
}