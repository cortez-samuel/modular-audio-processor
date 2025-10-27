#include "persistent_state.h"

// Emulated persistent state
static PersistentState currentState;

// Initialize defaults
void persistent_init() {
    // On startup, set default values (read from EEPROM in reality)
    currentState.effectType = 0;   // default effect
    currentState.alphaParam = 0; // default parameter
}

// Load state (returns current emulated state)
PersistentState persistent_load() {
    return currentState;
}

// Save state (updates emulated state)
void persistent_save(PersistentState state) {
    currentState = state;
}
