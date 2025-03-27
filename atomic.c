#include "atomic.h"
#include "cpu.h"
#include "pthread.h"
#include "tcg.h"

//  We only need to lock if there are multiple CPUs registered in the `atomic_memory_state`.
//  Reservations should be made regardless of it; atomic instructions need them even with a single CPU.
//  False is returned if `atomic_memory_state` hasn't been initialized at all.
static inline bool are_multiple_cpus_registered()
{
    return env->atomic_memory_state != NULL && env->atomic_memory_state->number_of_registered_cpus > 1;
}

static inline void ensure_locked_by_me(struct CPUState *env)
{
#if DEBUG
    if(!are_multiple_cpus_registered()) {
        return;
    }

    if(env->atomic_memory_state->locking_cpu_id != env->atomic_id) {
        tlib_abort("Tried to release global memory lock by the cpu that does not own it!");
    }
#endif
}

static void initialize_atomic_memory_state(atomic_memory_state_t *sm)
{
    //  `is_mutex_initialized` is reset during serialization.
    if(!sm->is_mutex_initialized) {
        sm->number_of_registered_cpus = 0;

        //  Initialize mutex.
        pthread_mutexattr_t attributes;
        if(unlikely(pthread_mutexattr_init(&attributes))) {
            tlib_abortf("Failed to initialize phthread_muttexattr_t");
        }
//  This flag is only supported on Linux
#if defined(__linux__)
        if(unlikely(pthread_mutexattr_setrobust(&attributes, PTHREAD_MUTEX_ROBUST))) {
            tlib_abortf("Failed to make the mutex robust");
        }
#endif
        if(unlikely(pthread_mutex_init(&sm->global_mutex, &attributes))) {
            tlib_abortf("Failed to initialize the pthread_mutex");
        }
        if(unlikely(pthread_mutexattr_destroy(&attributes))) {
            tlib_abortf("Failed to destroy the pthread_mutexattr");
        }

        pthread_cond_init(&sm->global_cond, NULL);
        sm->locking_cpu_id = NO_CPU_ID;
        sm->entries_count = 0;

        sm->is_mutex_initialized = 1;
    }

    //  `are_reservations_valid` is never reset.
    if(!sm->are_reservations_valid) {
        sm->reservations_count = 0;
        for(int i = 0; i < MAX_NUMBER_OF_CPUS; i++) {
            sm->reservations[i].id = i;
            sm->reservations[i].active_flag = 0;
            sm->reservations[i].address = 0;
            sm->reservations[i].locking_cpu_id = NO_CPU_ID;
            sm->reservations[i].manual_free = 0;
            sm->reservations_by_cpu[i] = NO_RESERVATION;
        }

        sm->are_reservations_valid = 1;
    }
}

static inline address_reservation_t *find_reservation_on_address(struct CPUState *env, target_phys_addr_t address,
                                                                 int starting_position)
{
    int i;
    for(i = starting_position; i < env->atomic_memory_state->reservations_count; i++) {
        if(env->atomic_memory_state->reservations[i].address == address) {
            return &env->atomic_memory_state->reservations[i];
        }
    }
    return NULL;
}

//  there can be only one reservation per cpu
static inline address_reservation_t *find_reservation_by_cpu(struct CPUState *env)
{
    int reservation_id = env->atomic_memory_state->reservations_by_cpu[env->atomic_id];
#if DEBUG
    if(reservation_id >= env->atomic_memory_state->reservations_count) {
        tlib_abort("Inconsistent reservation count detected.");
    }
#endif
    return (reservation_id == NO_RESERVATION) ? NULL : &env->atomic_memory_state->reservations[reservation_id];
}

static inline address_reservation_t *make_reservation(struct CPUState *env, target_phys_addr_t address, uint8_t manual_free)
{
    if(unlikely(env->atomic_memory_state->reservations_count == MAX_NUMBER_OF_CPUS)) {
        tlib_abort("No more address reservation slots");
    }

    address_reservation_t *reservation = &env->atomic_memory_state->reservations[env->atomic_memory_state->reservations_count];
    reservation->active_flag = 1;
    reservation->address = address;
    reservation->locking_cpu_id = env->atomic_id;
    reservation->manual_free = manual_free;

    env->atomic_memory_state->reservations_by_cpu[env->atomic_id] = env->atomic_memory_state->reservations_count;
    env->atomic_memory_state->reservations_count++;

    return reservation;
}

//  Returns true if reservation was freed, false otherwise.
static inline bool free_reservation(struct CPUState *env, address_reservation_t *reservation, uint8_t is_manual)
{
#if DEBUG
    if(reservation->active_flag == 0) {
        tlib_abort("Trying to free not active reservation");
    }
    if(env->atomic_memory_state->reservations_count == 0) {
        tlib_abort("Reservations count is 0, but trying to free one");
    }
#endif

    if(reservation->manual_free && !is_manual) {
        return false;
    }

    env->atomic_memory_state->reservations_by_cpu[reservation->locking_cpu_id] = NO_RESERVATION;
    if(reservation->id != env->atomic_memory_state->reservations_count - 1) {
        //  if this is not the last reservation, i must copy the last one in this empty place
        reservation->locking_cpu_id =
            env->atomic_memory_state->reservations[env->atomic_memory_state->reservations_count - 1].locking_cpu_id;
        reservation->address = env->atomic_memory_state->reservations[env->atomic_memory_state->reservations_count - 1].address;
        reservation->manual_free =
            env->atomic_memory_state->reservations[env->atomic_memory_state->reservations_count - 1].manual_free;
        //  active flag does not have to be copied as it's always 1
        tlib_assert(reservation->active_flag == 1 &&
                    env->atomic_memory_state->reservations[env->atomic_memory_state->reservations_count - 1].active_flag == 1);

        //  and update mapping
        env->atomic_memory_state->reservations_by_cpu[reservation->locking_cpu_id] = reservation->id;
    }

    env->atomic_memory_state->reservations[env->atomic_memory_state->reservations_count - 1].active_flag = 0;
    env->atomic_memory_state->reservations_count--;

    return true;
}

int32_t register_in_atomic_memory_state(atomic_memory_state_t *sm, int32_t atomic_id)
{
    cpu->atomic_id = -1;
    initialize_atomic_memory_state(sm);
    sm->number_of_registered_cpus++;
    if(sm->number_of_registered_cpus > MAX_NUMBER_OF_CPUS) {
        tlib_printf(LOG_LEVEL_ERROR, "atomic: Maximum number of supported cores exceeded: %d", MAX_NUMBER_OF_CPUS);
        return -1;
    }

    tcg_context_attach_number_of_registered_cpus(&sm->number_of_registered_cpus);
    cpu->atomic_id = atomic_id != -1 ? atomic_id : sm->number_of_registered_cpus - 1;
    return cpu->atomic_id;
}

void acquire_global_memory_lock(struct CPUState *env)
{
    if(!are_multiple_cpus_registered()) {
        return;
    }

    pthread_mutex_lock(&env->atomic_memory_state->global_mutex);
    if(env->atomic_memory_state->locking_cpu_id != env->atomic_id) {
        while(env->atomic_memory_state->locking_cpu_id != NO_CPU_ID) {
            pthread_cond_wait(&env->atomic_memory_state->global_cond, &env->atomic_memory_state->global_mutex);
        }
        env->atomic_memory_state->locking_cpu_id = env->atomic_id;
    }
    env->atomic_memory_state->entries_count++;
    pthread_mutex_unlock(&env->atomic_memory_state->global_mutex);
}

void release_global_memory_lock(struct CPUState *env)
{
    if(!are_multiple_cpus_registered()) {
        return;
    }

    pthread_mutex_lock(&env->atomic_memory_state->global_mutex);
    ensure_locked_by_me(env);
    env->atomic_memory_state->entries_count--;
    if(env->atomic_memory_state->entries_count == 0) {
        env->atomic_memory_state->locking_cpu_id = NO_CPU_ID;
        pthread_cond_signal(&env->atomic_memory_state->global_cond);
    }
    pthread_mutex_unlock(&env->atomic_memory_state->global_mutex);
}

void clear_global_memory_lock(struct CPUState *env)
{
    if(!are_multiple_cpus_registered()) {
        return;
    }

    pthread_mutex_lock(&env->atomic_memory_state->global_mutex);
    ensure_locked_by_me(env);
    env->atomic_memory_state->locking_cpu_id = NO_CPU_ID;
    env->atomic_memory_state->entries_count = 0;
    pthread_cond_signal(&env->atomic_memory_state->global_cond);
    pthread_mutex_unlock(&env->atomic_memory_state->global_mutex);
}

//  ! this function should be called when holding the mutex !
//  If manual_free is true then the performed reservation will only be able to be cancelled explicitly,
//  by calling `cancel_reservation` or by performing a different reservation on a CPU that already had
//  had a reserved address.
void reserve_address(struct CPUState *env, target_phys_addr_t address, uint8_t manual_free)
{
    ensure_locked_by_me(env);

    address_reservation_t *reservation = find_reservation_by_cpu(env);
    if(reservation != NULL) {
        if(reservation->address == address) {
            return;
        }
        //  cancel the previous reservation and set a new one
        free_reservation(env, reservation, 1);
    }
    make_reservation(env, address, manual_free);
}

//  Returns zero if the reservation was made for the given address
uint32_t check_address_reservation(struct CPUState *env, target_phys_addr_t address)
{
    ensure_locked_by_me(env);
    address_reservation_t *reservation = find_reservation_by_cpu(env);
    return (reservation == NULL || reservation->address != address);
}

void register_address_access(struct CPUState *env, target_phys_addr_t address)
{
    if(env->atomic_memory_state == NULL) {
        //  no atomic_memory_state so no registration needed
        return;
    }

    ensure_locked_by_me(env);

    int position = 0;
    address_reservation_t *reservation = find_reservation_on_address(env, address, position);
    while(reservation != NULL) {
        position = reservation->id + 1;
        if(reservation->locking_cpu_id != env->atomic_id) {
            bool freed = free_reservation(env, reservation, 0);
            if(freed) {
                //  Reservations were possibly reordered as a result of moving entry from the end of the list to the freed slot.
                //  To not miss reservation that should be cleared, look for the reservation from the same starting position as
                //  before. If there were more reservations in the list and the current one was cleared, the last one was moved
                //  into the slot of the cleared one.
                position--;
            }
        }
        reservation = find_reservation_on_address(env, address, position);
    }
}

void cancel_reservation(struct CPUState *env)
{
    ensure_locked_by_me(env);

    address_reservation_t *reservation = find_reservation_by_cpu(env);
    if(reservation != NULL) {
        free_reservation(env, reservation, 1);
    }
}
