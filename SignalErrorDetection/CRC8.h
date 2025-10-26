#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>

struct {
    uint8_t totalBytes; // N / 8
    uint8_t dataBytes;  // d / 8
} CRC8Settings;
const uint16_t CRC8__generator = 0b100110111;

typedef uint8_t CRC8__ErrorCode;
enum {
    CRC8__valid,
    CRC8__error,
};

void CRC8Encode(uint8_t* data, uint8_t* encoded);

CRC8__ErrorCode CRC8Decode(uint8_t* encoded, uint8_t* data);

#endif