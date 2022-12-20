#ifndef ARCH_EXPORTS_H_
#define ARCH_EXPORTS_H_

#include <stdint.h>

void tlib_allow_feature(uint32_t feature_bit);

uint32_t tlib_is_feature_enabled(uint32_t feature_bit);

uint32_t tlib_is_feature_allowed(uint32_t feature_bit);

void tlib_set_privilege_architecture(int32_t privilege_architecture);

void tlib_set_nmi_vector(uint64_t nmi_adress, uint32_t nmi_lenght);

void tlib_set_nmi(int32_t nmi, int32_t state);

void tlib_allow_unaligned_accesses(int32_t allowed);

uint32_t tlib_set_vlen(uint32_t vlen);
uint32_t tlib_set_elen(uint32_t elen);

uint64_t tlib_get_vector(int regn, int idx);
void tlib_set_vector(int regn, int idx, uint64_t value);

uint32_t tlib_get_whole_vector(uint32_t regn, uint8_t *bytes);
uint32_t tlib_set_whole_vector(uint32_t regn, uint8_t *bytes);
uint32_t tlib_install_post_opcode_execution_hook(uint64_t mask, uint64_t value);
void tlib_enable_post_opcode_execution_hooks(uint32_t value);
void tlib_enable_post_gpr_access_hooks(uint32_t value);
void tlib_enable_post_gpr_access_hook_on(uint32_t register_index, uint32_t value);

#endif
