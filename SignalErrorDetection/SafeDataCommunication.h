#ifndef SAFEDATACOMMUNICATION_H
#define SAFEDATACOMMUNICATION_H

#include <stdint.h>

typedef uint16_t D_t;
struct {
    uint8_t dataErrorMasterPin;
    uint8_t dataErrorSlavePin;
    uint8_t maximumRetransmitCount;
} SafeDataCommunicationSettings;


struct TransmissionInfo_t{
    D_t transmittedData;
    uint16_t retransmitCount;
};

void SafeDataCommunicationInit();

TransmissionInfo_t SafeDataCommunication_Tx(D_t* data);

void _RequestDataResend();

TransmissionInfo_t SafeDataCommunication_Rx();

void _EstimateTrueData(void* recievedData, uint8_t length);  // choose most-common bit from retransmissions

#endif