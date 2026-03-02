#include "cpu.h"
#include "translate_common.h"

void gen_srshr64_i64(TCGv_i64 d, TCGv_i64 a, int64_t sh)
{
    TCGv_i64 t = tcg_temp_new_i64();

    tcg_gen_extract_i64(t, a, sh - 1, 1);
    tcg_gen_sari_i64(d, a, sh);
    tcg_gen_add_i64(d, d, t);
    tcg_temp_free_i64(t);
}

void gen_urshr64_i64(TCGv_i64 d, TCGv_i64 a, int64_t sh)
{
    TCGv_i64 t = tcg_temp_new_i64();

    tcg_gen_extract_i64(t, a, sh - 1, 1);
    tcg_gen_shri_i64(d, a, sh);
    tcg_gen_add_i64(d, d, t);
    tcg_temp_free_i64(t);
}
