#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>

constexpr struct CRC8Settings_t {
    const uint8_t encodedBytes      = 4;     // (N / 8)  ::  N <= 32
    const uint32_t encodedBytes_bm  = 0xFFFFFFFF;
    typedef uint32_t encoded_t;
    const uint8_t dataBytes         = 3;      // (d / 8)  ::  d <= 24
    const uint32_t dataBytes_bm     = 0x00FFFFFF;
    typedef uint32_t data_t;
    const uint32_t generator = 0b100110111;
} CRC8Settings;


typedef uint8_t CRC8__ErrorCode;
enum {
    CRC8__valid,
    CRC8__error,
};

void CRC8Encode(CRC8Settings_t::data_t& data, CRC8Settings_t::encoded_t& encoded);

CRC8__ErrorCode CRC8Decode(CRC8Settings_t::encoded_t& encoded, CRC8Settings_t::data_t& data);

#endif