#ifndef SIGNALERRORDETECTION_H
#define SIGNALERRORDETECTION_H


#include "pico/stdlib.h"


typedef uint8_t Checksum8__ErrorCode;
enum {
    Checksum8__valid = 0,
    Checksum8__error = 1,
};


void Checksum8Encode(uint32_t* data, uint32_t* encoded);

Checksum8__ErrorCode Checksum8Decode(uint32_t* encoded, uint32_t* data);


////////////////////

typedef uint8_t CRC8__ErrorCode;
enum {
    CRC8__valid = 0,
    CRC8__error = 1,
};

void CRC8Encode(uint32_t* data, uint32_t* encoded);

CRC8__ErrorCode CRC8Decode(uint32_t* encoded, uint32_t* data);


#endif