#pragma once

#include <stdbool.h>
#include <stdint.h>

// That's currently because of using 'uint64_t included_signals_mask' in ConfigurationSignalsState.
#define CONFIGURATION_SIGNALS_MAX 64

#define CONFIGURATION_SIGNALS_COUNT 5

#if CONFIGURATION_SIGNALS_COUNT > CONFIGURATION_SIGNALS_MAX
# error Number of configuration signals is too large.
#endif

// Remember to update CONFIGURATION_SIGNALS_COUNT when modifying this enum. Don't assign any values.
typedef enum {
    IN_DBGROMADDR,
    IN_DBGSELFADDR,
    IN_INITRAM,
    IN_PERIPHBASE,
    IN_VINITHI,
} ConfigurationSignals;

typedef struct CPUState CPUState;
void configuration_signals__on_resume_after_reset_or_init(CPUState *env);

// Will be gathered through a callback.
typedef struct {
    // Each bit says whether to apply the signal's effect.
    // Bit positions are based on the ConfigurationSignals enum.
    uint64_t included_signals_mask;

    uint32_t dbgromaddr;
    uint32_t dbgselfaddr;
    bool initram;
    uint32_t periphbase;
    bool vinithi;
} ConfigurationSignalsState;
