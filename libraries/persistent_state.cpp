#include "persistent_state.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <cstring>
#include <cmath>

// Circular log: store 4 records within a single 4 KiB sector at 4MB offset
// Each record slot is written sequentially; when full, wrap back to slot 0
static const uint32_t NUM_SLOTS = 4u;
static const uint32_t FLASH_SECTOR_SIZE = 4096u;
static const uint32_t BASE_FLASH_OFFSET = 0x00400000u;  // start at 4MB
// Signature placed at start of each stored record to indicate a valid entry
// (avoids treating erased flash (0xFFFFFFFF) as valid data).
static const uint32_t PERSIST_RECORD_SIGNATURE = 0x50E5CAFEu;

struct FlashRecord {
    // 32-bit signature identifying a valid stored record
    uint32_t signature;
    PersistentState state;
};

static_assert(sizeof(FlashRecord) <= FLASH_SECTOR_SIZE, "Persistent data too large for one flash sector");

// In-memory cache of the persistent state for quicker access during runtime
static PersistentState cached_state;
// Track which slot we last loaded from (for next write destination)
static uint32_t last_loaded_slot = 0;

// Helper: compute flash offset for a given slot index within the sector
static uint32_t get_slot_offset(uint32_t slot_index) {
    return BASE_FLASH_OFFSET + (slot_index * sizeof(FlashRecord));
}

void persistent_init() {
    // Scan all 4 slots to find the newest valid record (highest writeCounter)
    // If tied, use the last slot with that count
    PersistentState best_state = { 0, 0.5f, 0 };
    uint32_t best_write_counter = 0;
    uint32_t best_slot = 0;
    bool found_valid = false;

    for (uint32_t slot = 0; slot < NUM_SLOTS; slot++) {
        uint32_t offset = get_slot_offset(slot);
        const uint8_t* flash_ptr = reinterpret_cast<const uint8_t*>(XIP_BASE + offset);
        FlashRecord rec;
        memcpy(&rec, flash_ptr, sizeof(rec));

        if (rec.signature == PERSIST_RECORD_SIGNATURE) {
            // Valid record found; keep it if writeCounter is higher or equal (prefer last slot if tied)
            if (!found_valid || rec.state.writeCounter >= best_write_counter) {
                best_state = rec.state;
                best_write_counter = rec.state.writeCounter;
                best_slot = slot;
                found_valid = true;
            }
        }
    }

    if (found_valid) {
        cached_state = best_state;
        last_loaded_slot = best_slot;
    } else {
        // No valid records found; set defaults
        cached_state.effectType = 0;
        cached_state.alphaParam = 0.5f;
        cached_state.writeCounter = 0;
        last_loaded_slot = 0;
    }
}

PersistentState persistent_load() {
    return cached_state;
}

bool persistent_save(const PersistentState& state) {
    // Increment writeCounter and prepare the new state to save
    PersistentState new_state = state;
    new_state.writeCounter = cached_state.writeCounter + 1;

    // Write to the next slot (round-robin) without erasing
    uint32_t next_slot = (last_loaded_slot + 1) % NUM_SLOTS;
    uint32_t offset = get_slot_offset(next_slot);

    FlashRecord rec;
    rec.signature = PERSIST_RECORD_SIGNATURE;
    rec.state = new_state;

    // Ensure interrupts disabled while writing to flash
    uint32_t saved = save_and_disable_interrupts();
    flash_range_program(offset, reinterpret_cast<const uint8_t*>(&rec), sizeof(rec));
    restore_interrupts(saved);

    // Update in-memory cache and track where we wrote
    cached_state = new_state;
    last_loaded_slot = next_slot;

    // Verify write (read back from XIP)
    FlashRecord verify;
    const uint8_t* flash_ptr = reinterpret_cast<const uint8_t*>(XIP_BASE + offset);
    memcpy(&verify, flash_ptr, sizeof(verify));
    if (verify.signature != PERSIST_RECORD_SIGNATURE) return false;
    if (verify.state.effectType != new_state.effectType) return false;
    if (fabsf(verify.state.alphaParam - new_state.alphaParam) > 1e-6f) return false;
    if (verify.state.writeCounter != new_state.writeCounter) return false;
    return true;
}