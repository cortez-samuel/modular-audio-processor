#ifndef CHECKSUM8_H
#define CHECKSUM8_H

#include <stdint.h>

struct {
    uint8_t dataBytes;
} Checksum8Settings;

typedef uint8_t Checksum8__ErrorCode;
enum {
    Checksum8__valid,
    Checksum8__error,
};

void Checksum8Encode(uint8_t* data, uint8_t* encoded);

enum Checksum8__ErrorCode Checksum8Decode(uint8_t* encoded, uint8_t* data);

#endif