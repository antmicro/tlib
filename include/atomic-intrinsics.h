#pragma once

void tcg_try_gen_atomic_fetch_add_intrinsic_i32(TCGv_i32 result, TCGv_ptr guestAddress, TCGv_i32 toAdd, uint32_t memIndex,
                                                int fallbackLabel);

void tcg_try_gen_atomic_fetch_add_intrinsic_i64(TCGv_i64 result, TCGv_ptr guestAddress, TCGv_i64 toAdd, uint32_t memIndex,
                                                int fallbackLabel);
