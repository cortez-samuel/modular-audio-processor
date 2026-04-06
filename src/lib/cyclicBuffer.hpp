#ifndef cyclicBuffer_hpp_INCLUDED
#define cyclicBuffer_hpp_INCLUDED

#include "pico/stdlib.h"

template<typename T>
class CyclicBuffer_t {
    using _T = T;

    _T* buffer;
    uint N;
    
    uint head;


public:
    CyclicBuffer_t(_T* buff, uint length) {
        buffer = buff;
        N = length;
        head = N - 1;
    }

public: 
    inline _T getHead() const {
        return buffer[head];
    }
    inline _T get(uint index) const {
        return buffer[(head - index) % N];
    }
    inline void appendHead(_T D) {
        head = (head + 1) % N;
        buffer[head] = D;
    }
    inline void set(_T D, uint index) {
        buffer[(head - index) % N] = D;
    }
};  

#endif