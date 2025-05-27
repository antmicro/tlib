#pragma once

#include <stdint.h>
#include "targphys.h"
#include <stdbool.h>
#include "hash-table-store-test.h"

void register_thread_address_access(CPUState *cpu_env, target_phys_addr_t address);
bool check_thread_address_access(CPUState *cpu_env, target_phys_addr_t address);
void hash_table_lock(CPUState *cpu_env, target_phys_addr_t address);
void hash_table_unlock(CPUState *cpu_env, target_phys_addr_t address);
