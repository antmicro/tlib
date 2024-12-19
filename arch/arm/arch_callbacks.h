#pragma once

#include <stdint.h>

#ifdef TARGET_PROTO_ARM_M
int32_t tlib_nvic_acknowledge_irq(void);
void tlib_nvic_complete_irq(int32_t number);
void tlib_nvic_write_basepri(int32_t number, uint32_t secure);
int32_t tlib_nvic_find_pending_irq(void);
int32_t tlib_nvic_get_pending_masked_irq(void);
void tlib_nvic_set_pending_irq(int32_t no);
uint32_t tlib_has_enabled_trustzone(void);
uint32_t tlib_nvic_interrupt_targets_secure(int32_t no);
int32_t tlib_custom_idau_handler(void *external_idau_request, void *attribution, void *region);

typedef struct {
    uint32_t address;
    int32_t secure;
    int32_t access_type;
    int32_t access_width;
} ExternalIDAURequest;
#endif

uint32_t tlib_read_cp15_32(uint32_t instruction);
void tlib_write_cp15_32(uint32_t instruction, uint32_t value);
uint64_t tlib_read_cp15_64(uint32_t instruction);
void tlib_write_cp15_64(uint32_t instruction, uint64_t value);
uint32_t tlib_is_wfi_as_nop(void);
uint32_t tlib_is_wfe_and_sev_as_nop(void);
uint32_t tlib_do_semihosting(void);
void tlib_set_system_event(int32_t value);
void tlib_report_pmu_overflow(int32_t counter);

#include "configuration_signals.h"
void tlib_fill_configuration_signals_state(void *state_pointer);
