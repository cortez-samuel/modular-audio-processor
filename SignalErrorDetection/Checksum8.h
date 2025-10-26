#ifndef CHECKSUM8_H
#define CHECKSUM8_H

#include <stdint.h>

constexpr struct Checksum8Settings_t {
  const uint8_t dataBytes       = 3;
  typedef uint32_t data_t;
  Checksum8Settings_t::data_t data_bm = 0x00FFFFFF;
  
  const uint8_t encodedBytes    = 4;
  typedef uint32_t encoded_t;
  Checksum8Settings_t::encoded_t encoded_bm = 0xFFFFFFFF;
} Checksum8Settings;


typedef uint8_t Checksum8__ErrorCode;
enum {
    Checksum8__valid = 0,
    Checksum8__error = 1,
};


void Checksum8Encode(Checksum8Settings_t::data_t& data, Checksum8Settings_t::encoded_t& encoded);

Checksum8__ErrorCode Checksum8Decode(Checksum8Settings_t::encoded_t& encoded, Checksum8Settings_t::data_t& data);

#endif