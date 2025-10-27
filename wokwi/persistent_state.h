#ifndef PERSISTENT_STATE_H
#define PERSISTENT_STATE_H

#include <stdint.h>

// Persistent state struct
typedef struct {
    uint8_t effectType;   // currently selected effect
    float alphaParam;     // potential effect parameter
} PersistentState;

void persistent_init();
PersistentState persistent_load();
void persistent_save(PersistentState state);

#endif
