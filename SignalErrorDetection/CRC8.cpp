#include "CRC8.h"


static uint8_t CalculateCRC8Remainder(CRC8Settings_t::encoded_t X) {
    uint8_t shift;

    for (uint8_t k = (CRC8Settings.encodedBytes << 3) - 1; k > 7; k--) {
        if (X & (1 << k)) {
            shift = k - 8 * (CRC8Settings.encodedBytes - CRC8Settings.dataBytes);
            X ^= (CRC8Settings.generator << shift);
        }
    }

    return X & 0xFF;
}

void CRC8Encode(CRC8Settings_t::data_t& data, CRC8Settings_t::encoded_t& encoded) {
    CRC8Settings_t::encoded_t X = data << 8;

    uint8_t R = CalculateCRC8Remainder(X);

    encoded = (data << 8) | R;
}

CRC8__ErrorCode CRC8Decode(CRC8Settings_t::encoded_t& encoded, CRC8Settings_t::data_t& data) {
    CRC8Settings_t::encoded_t X = encoded;

    uint8_t R = CalculateCRC8Remainder(X);

    data = (encoded >> 8);

    if (R == 0) {
        return CRC8__valid;
    }
    return CRC8__error;
}