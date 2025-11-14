#ifndef PERSISTENT_STATE_H
#define PERSISTENT_STATE_H

#include <cstdint>

struct PersistentState {
    uint8_t effectType;
    float alphaParam;
    uint32_t writeCounter;
};

void persistent_init();
PersistentState persistent_load();
bool persistent_save(const PersistentState& state);

#endif // PERSISTENT_STATE_H