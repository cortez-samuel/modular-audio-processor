#include "Checksum8.h"


void Checksum8Encode(Checksum8Settings_t::data_t& data, Checksum8Settings_t::encoded_t& encoded) {
    uint8_t checksum = 0; 

    Checksum8Settings_t::data_t X = data;
    for (uint8_t k = 0; k < Checksum8Settings.dataBytes; k++) {
        checksum += X & 0xFF;
        X = X >> 8;
    }
    encoded = (data << 8) | checksum;
}

Checksum8__ErrorCode Checksum8Decode(Checksum8Settings_t::encoded_t& encoded, Checksum8Settings_t::data_t& data) {
    uint8_t sum = 0;

    Checksum8Settings_t::encoded_t X = encoded;
    for (uint8_t k = 0; k < Checksum8Settings.encodedBytes; k++) {
        sum += X & 0xFF;
        X = X >> 8;
    }
    data = encoded >> 8;

    return sum == 0 ? Checksum8__valid : Checksum8__error;
}

