#include "hst.h"
#include "cpu.h"
#include "tcg.h"
#include "stdatomic.h"

void register_thread_address_access(CPUState* cpu_env, target_phys_addr_t address)
{
    uint32_t* tid_address = (uint32_t*) address_hash(cpu_env, address);
    uint32_t atomic_id = cpu_env->atomic_id;
    *tid_address = atomic_id;
}

bool check_thread_address_access(CPUState* cpu_env, target_phys_addr_t address)
{
    /* TODO: what does it return when not yet assigned? */
    return cpu_env->atomic_id == *((uint32_t*) address_hash(cpu_env, address));
}

void hash_table_lock(CPUState* cpu_env, target_phys_addr_t address)
{
    bool* lock_address = (bool*) (address_hash(cpu_env, address) + sizeof(uint32_t));
    bool expected = false;
    while (!atomic_compare_exchange_strong(lock_address, &expected, true));
}
void hash_table_unlock(CPUState* cpu_env, target_phys_addr_t address)
{
    bool* lock_address = (bool*) (address_hash(cpu_env, address) + sizeof(uint32_t));
    bool expected = true;
    while (!atomic_compare_exchange_strong(lock_address, &expected, false));
}
