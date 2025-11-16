#include "persistent_state.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <cstring>
#include <cmath>

// Persistent storage: store one record per 4 KiB sector at 4MB offset
// Write whole sector to satisfy alignment rules
static const uint32_t FLASH_SECTOR_SIZE = 4096u;
static const uint32_t BASE_FLASH_OFFSET = 0x00400000u;  // start at 4MB
static const uint32_t FLASH_PAGE_SIZE = 256u;
// Signature placed at start of each stored record to indicate valid entry
// (avoids treating erased flash (0xFFFFFFFF) as valid data).
static const uint32_t PERSIST_RECORD_SIGNATURE = 0x50E5CAFEu;

struct FlashRecord {
    // 32-bit signature identifying a valid stored record
    uint32_t signature;
    PersistentState state;
};

static_assert(sizeof(FlashRecord) <= FLASH_SECTOR_SIZE, "Persistent data too large for one flash sector");

// In-memory cache of persistent state for access during runtime
static PersistentState cached_state;
// Small static page buffer to avoid large stack allocations when programming flash
static uint8_t page_buf[FLASH_PAGE_SIZE];

void persistent_init() {
    // Read the record at the start of the sector
    const uint8_t* flash_ptr = reinterpret_cast<const uint8_t*>(XIP_BASE + BASE_FLASH_OFFSET);
    FlashRecord rec;
    memcpy(&rec, flash_ptr, sizeof(rec));

    if (rec.signature == PERSIST_RECORD_SIGNATURE) {
        cached_state = rec.state;
    } else {
        // No valid record found; set defaults
        cached_state.effectType = 0;
        cached_state.alphaParam = 0.5f;
        cached_state.writeCounter = 0;
    }
}

PersistentState persistent_load() {
    return cached_state;
}

bool persistent_save(const PersistentState& state) {
    // Prepare new state with incremented counter
    PersistentState new_state = state;
    new_state.writeCounter = cached_state.writeCounter + 1;

    FlashRecord rec_out;
    rec_out.signature = PERSIST_RECORD_SIGNATURE;
    rec_out.state = new_state;

    // Setup buffer to be "empty" and place the record at the start
    memset(page_buf, 0xFF, FLASH_PAGE_SIZE);
    memcpy(&page_buf[0], &rec_out, sizeof(rec_out));

    uint32_t saved = save_and_disable_interrupts();
    // Erase whole sector
    flash_range_erase(BASE_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    // Program the first page with the new record
    flash_range_program(BASE_FLASH_OFFSET, page_buf, FLASH_PAGE_SIZE);
    restore_interrupts(saved);

    // Update in-memory cache
    cached_state = new_state;

    // Verify write (read back from XIP)
    FlashRecord verify;
    const uint8_t* verify_ptr = reinterpret_cast<const uint8_t*>(XIP_BASE + BASE_FLASH_OFFSET);
    memcpy(&verify, verify_ptr, sizeof(verify));
    if (verify.signature != PERSIST_RECORD_SIGNATURE) return false;
    if (verify.state.effectType != new_state.effectType) return false;
    if (fabsf(verify.state.alphaParam - new_state.alphaParam) > 1e-6f) return false;
    if (verify.state.writeCounter != new_state.writeCounter) return false;
    return true;
}