/*
 * QEMU float support
 *
 * Derived from SoftFloat.
 *
 * This file is licensed with the SoftFloat-2b license.
 *
 * These specific parts of the file are licensed with the SoftFloat-2a license as
 * they come from QEMU's fpu/softfloat-specialize.c.inc after the SoftFloat
 * relicensing (see https://wiki.qemu.org/Features/SoftFloatRelicensing):
 * * the 'use_signaling_nans' function,
 * * 'if (no_signaling_nans(status)) { ... }' blocks in 'float*_is_signaling_nan'
 *   and 'float*_is_quiet_nan' functions,
 * * 'pickNaN' and 'pickNaNMulAdd' Xtensa implementations,
 *
 * The original license notices can be found below.
 */

/*============================================================================
   The "SoftFloat-2b" license:

   This C source fragment is part of the SoftFloat IEC/IEEE Floating-point
   Arithmetic Package, Release 2b.

   Written by John R. Hauser.  This work was made possible in part by the
   International Computer Science Institute, located at Suite 600, 1947 Center
   Street, Berkeley, California 94704.  Funding was partially provided by the
   National Science Foundation under grant MIP-9311980.  The original version
   of this code was written as part of a project to build a fixed-point vector
   processor in collaboration with the University of California at Berkeley,
   overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
   is available through the Web page `http://www.cs.berkeley.edu/~jhauser/
   arithmetic/SoftFloat.html'.

   THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort has
   been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT TIMES
   RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO PERSONS
   AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ALL LOSSES,
   COSTS, OR OTHER PROBLEMS THEY INCUR DUE TO THE SOFTWARE, AND WHO FURTHERMORE
   EFFECTIVELY INDEMNIFY JOHN HAUSER AND THE INTERNATIONAL COMPUTER SCIENCE
   INSTITUTE (possibly via similar legal warning) AGAINST ALL LOSSES, COSTS, OR
   OTHER PROBLEMS INCURRED BY THEIR CUSTOMERS AND CLIENTS DUE TO THE SOFTWARE.

   Derivative works are acceptable, even for commercial purposes, so long as
   (1) the source code for the derivative work includes prominent notice that
   the work is derivative, and (2) the source code includes prominent notice with
   these four paragraphs for those parts of this code that are retained.

   =============================================================================*/

/*============================================================================
   The "SoftFloat-2a" license:

   This C source fragment is part of the SoftFloat IEC/IEEE Floating-point
   Arithmetic Package, Release 2a.

   Written by John R. Hauser.  This work was made possible in part by the
   International Computer Science Institute, located at Suite 600, 1947 Center
   Street, Berkeley, California 94704.  Funding was partially provided by the
   National Science Foundation under grant MIP-9311980.  The original version
   of this code was written as part of a project to build a fixed-point vector
   processor in collaboration with the University of California at Berkeley,
   overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
   is available through the Web page `http://HTTP.CS.Berkeley.EDU/~jhauser/
   arithmetic/SoftFloat.html'.

   THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort
   has been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT
   TIMES RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO
   PERSONS AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ANY
   AND ALL LOSSES, COSTS, OR OTHER PROBLEMS ARISING FROM ITS USE.

   Derivative works are acceptable, even for commercial purposes, so long as
   (1) they include prominent notice that the work is derivative, and (2) they
   include prominent notice akin to these four paragraphs for those parts of
   this code that are retained.

   =============================================================================*/

/*
 * Define whether architecture deviates from IEEE in not supporting
 * signaling NaNs (so all NaNs are treated as quiet).
 */
static inline uint8_t no_signaling_nans(float_status *status)
{
#if defined(TARGET_XTENSA)
    return STATUS(no_signaling_nans);
#else
    return 0;
#endif
}

/*----------------------------------------------------------------------------
 | The pattern for a default generated half-precision NaN.
 *----------------------------------------------------------------------------*/
#if defined(TARGET_ARM) || defined(TARGET_ARM64) || defined(TARGET_RISCV)
const float16 float16_default_nan = const_float16(0x7E00);
#else
const float16 float16_default_nan = const_float16(0xFE00);
#endif

/*----------------------------------------------------------------------------
 | The pattern for a default generated single-precision NaN.
 *----------------------------------------------------------------------------*/
#if defined(TARGET_SPARC)
const float32 float32_default_nan = const_float32(0x7FFFFFFF);
#elif defined(TARGET_PPC) || defined(TARGET_ARM) || defined(TARGET_RISCV) || defined(TARGET_ARM64)
const float32 float32_default_nan = const_float32(0x7FC00000);
#else
const float32 float32_default_nan = const_float32(0xFFC00000);
#endif

/*----------------------------------------------------------------------------
 | The pattern for a default generated double-precision NaN.
 *----------------------------------------------------------------------------*/
#if defined(TARGET_SPARC)
const float64 float64_default_nan = const_float64(LIT64(0x7FFFFFFFFFFFFFFF));
#elif defined(TARGET_PPC) || defined(TARGET_ARM) || defined(TARGET_RISCV) || defined(TARGET_ARM64)
const float64 float64_default_nan = const_float64(LIT64(0x7FF8000000000000));
#else
const float64 float64_default_nan = const_float64(LIT64(0xFFF8000000000000));
#endif

/*----------------------------------------------------------------------------
 | The pattern for a default generated extended double-precision NaN.
 *----------------------------------------------------------------------------*/
#define floatx80_default_nan_high 0xFFFF
#define floatx80_default_nan_low  LIT64(0xC000000000000000)

const floatx80 floatx80_default_nan = make_floatx80(floatx80_default_nan_high, floatx80_default_nan_low);

/*----------------------------------------------------------------------------
 | The pattern for a default generated quadruple-precision NaN.  The `high' and
 | `low' values hold the most- and least-significant bits, respectively.
 *----------------------------------------------------------------------------*/
#define float128_default_nan_high LIT64(0xFFFF800000000000)
#define float128_default_nan_low  LIT64(0x0000000000000000)

const float128 float128_default_nan = make_float128(float128_default_nan_high, float128_default_nan_low);

/*----------------------------------------------------------------------------
 | Raises the exceptions specified by `flags'.  Floating-point traps can be
 | defined here if desired.  It is currently not possible for such a trap
 | to substitute a result value.  If traps are not implemented, this routine
 | should be simply `float_exception_flags |= flags;'.
 *----------------------------------------------------------------------------*/

#ifdef TARGET_PROTO_ARM_M
#include "../arch/arm/cpu.h"
#endif

void float_raise(int8 flags STATUS_PARAM)
{
    STATUS(float_exception_flags) |= flags;
#ifdef TARGET_PROTO_ARM_M
    vfp_trigger_exception();
#endif
}

/*----------------------------------------------------------------------------
 | Internal canonical NaN format.
 *----------------------------------------------------------------------------*/
typedef struct {
    flag sign;
    uint64_t high, low;
} commonNaNT;

/*----------------------------------------------------------------------------
 | Returns 1 if the half-precision floating-point value `a' is a signaling
 | NaN; otherwise returns 0.
 *----------------------------------------------------------------------------*/

int float16_is_signaling_nan(float16 a_ STATUS_PARAM)
{
    if(no_signaling_nans(status)) {
        return 0;
    }
    uint16_t a = float16_val(a_);
    return (((a >> 9) & 0x3F) == 0x3E) && (a & 0x1FF);
}

/*----------------------------------------------------------------------------
 | Returns a quiet NaN if the half-precision floating point value `a' is a
 | signaling NaN; otherwise returns `a'.
 *----------------------------------------------------------------------------*/
float16 float16_maybe_silence_nan(float16 a_ STATUS_PARAM)
{
    if(float16_is_signaling_nan(a_ STATUS_VAR)) {
        uint16_t a = float16_val(a_);
        a |= (1 << 9);
        return make_float16(a);
    }
    return a_;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the half-precision floating-point NaN
 | `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
 | exception is raised.
 *----------------------------------------------------------------------------*/

static commonNaNT float16ToCommonNaN(float16 a STATUS_PARAM)
{
    commonNaNT z;

    if(float16_is_signaling_nan(a STATUS_VAR)) {
        float_raise(float_flag_invalid STATUS_VAR);
    }
    z.sign = float16_val(a) >> 15;
    z.low = 0;
    z.high = ((uint64_t)float16_val(a)) << 54;
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the canonical NaN `a' to the half-
 | precision floating-point format.
 *----------------------------------------------------------------------------*/

static float16 commonNaNToFloat16(commonNaNT a STATUS_PARAM)
{
    uint16_t mantissa = a.high >> 54;

    if(STATUS(default_nan_mode)) {
        return float16_default_nan;
    }

    if(mantissa) {
        return make_float16(((((uint16_t)a.sign) << 15) | (0x1F << 10) | mantissa));
    } else {
        return float16_default_nan;
    }
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point value `a' is a quiet
 | NaN; otherwise returns 0.
 *----------------------------------------------------------------------------*/

int float32_is_quiet_nan(float32 a_ STATUS_PARAM)
{
    if(no_signaling_nans(status)) {
        return float32_is_any_nan(a_);
    }
    uint32_t a = float32_val(a_);
    return (0xFF800000 <= (uint32_t)(a << 1));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point value `a' is a signaling
 | NaN; otherwise returns 0.
 *----------------------------------------------------------------------------*/

int float32_is_signaling_nan(float32 a_ STATUS_PARAM)
{
    if(no_signaling_nans(status)) {
        return 0;
    }
    uint32_t a = float32_val(a_);
    return (((a >> 22) & 0x1FF) == 0x1FE) && (a & 0x003FFFFF);
}

/*----------------------------------------------------------------------------
 | Returns a quiet NaN if the single-precision floating point value `a' is a
 | signaling NaN; otherwise returns `a'.
 *----------------------------------------------------------------------------*/

float32 float32_maybe_silence_nan(float32 a_ STATUS_PARAM)
{
    if(float32_is_signaling_nan(a_ STATUS_VAR)) {
        uint32_t a = float32_val(a_);
        a |= (1 << 22);
        return make_float32(a);
    }
    return a_;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the single-precision floating-point NaN
 | `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
 | exception is raised.
 *----------------------------------------------------------------------------*/

static commonNaNT float32ToCommonNaN(float32 a STATUS_PARAM)
{
    commonNaNT z;

    if(float32_is_signaling_nan(a STATUS_VAR)) {
        float_raise(float_flag_invalid STATUS_VAR);
    }
    z.sign = float32_val(a) >> 31;
    z.low = 0;
    z.high = ((uint64_t)float32_val(a)) << 41;
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the canonical NaN `a' to the single-
 | precision floating-point format.
 *----------------------------------------------------------------------------*/

static float32 commonNaNToFloat32(commonNaNT a STATUS_PARAM)
{
    uint32_t mantissa = a.high >> 41;

    if(STATUS(default_nan_mode)) {
        return float32_default_nan;
    }

    if(mantissa) {
        return make_float32((((uint32_t)a.sign) << 31) | 0x7F800000 | (a.high >> 41));
    } else {
        return float32_default_nan;
    }
}

/*----------------------------------------------------------------------------
 | Select which NaN to propagate for a two-input operation.
 | IEEE754 doesn't specify all the details of this, so the
 | algorithm is target-specific.
 | The routine is passed various bits of information about the
 | two NaNs and should return 0 to select NaN a and 1 for NaN b.
 | Note that signalling NaNs are always squashed to quiet NaNs
 | by the caller, by calling floatXX_maybe_silence_nan() before
 | returning them.
 |
 | aIsLargerSignificand is only valid if both a and b are NaNs
 | of some kind, and is true if a has the larger significand,
 | or if both a and b have the same significand but a is
 | positive but b is negative. It is only needed for the x87
 | tie-break rule.
 *----------------------------------------------------------------------------*/

static int pickNaN(flag aIsQNaN, flag aIsSNaN, flag bIsQNaN, flag bIsSNaN, flag aIsLargerSignificand STATUS_PARAM)
{
#if defined(TARGET_ARM) || defined(TARGET_ARM64)
    /* ARM mandated NaN propagation rules: take the first of:
     *  1. A if it is signaling
     *  2. B if it is signaling
     *  3. A (quiet)
     *  4. B (quiet)
     * A signaling NaN is always quietened before returning it.
     */
    if(aIsSNaN) {
        return 0;
    } else if(bIsSNaN) {
        return 1;
    } else if(aIsQNaN) {
        return 0;
    } else {
        return 1;
    }
#elif defined(TARGET_PPC)
    /* PowerPC propagation rules:
     *  1. A if it sNaN or qNaN
     *  2. B if it sNaN or qNaN
     * A signaling NaN is always silenced before returning it.
     */
    if(aIsSNaN || aIsQNaN) {
        return 0;
    } else {
        return 1;
    }
#elif defined(TARGET_XTENSA)
    /*
     * Xtensa has two NaN propagation modes.
     * Which one is active is controlled by float_status::use_first_nan.
     */
    if(STATUS(use_first_nan)) {
        if(aIsQNaN || aIsSNaN) {
            return 0;
        } else {
            return 1;
        }
    } else {
        if(bIsQNaN || bIsSNaN) {
            return 1;
        } else {
            return 0;
        }
    }
#else
    /* This implements x87 NaN propagation rules:
     * SNaN + QNaN => return the QNaN
     * two SNaNs => return the one with the larger significand, silenced
     * two QNaNs => return the one with the larger significand
     * SNaN and a non-NaN => return the SNaN, silenced
     * QNaN and a non-NaN => return the QNaN
     *
     * If we get down to comparing significands and they are the same,
     * return the NaN with the positive sign bit (if any).
     */
    if(aIsSNaN) {
        if(bIsSNaN) {
            return aIsLargerSignificand ? 0 : 1;
        }
        return bIsQNaN ? 1 : 0;
    } else if(aIsQNaN) {
        if(bIsSNaN || !bIsQNaN) {
            return 0;
        } else {
            return aIsLargerSignificand ? 0 : 1;
        }
    } else {
        return 1;
    }
#endif
}

/*----------------------------------------------------------------------------
 | Select which NaN to propagate for a three-input operation.
 | For the moment we assume that no CPU needs the 'larger significand'
 | information.
 | Return values : 0 : a; 1 : b; 2 : c; 3 : default-NaN
 *----------------------------------------------------------------------------*/
#if defined(TARGET_ARM) || defined(TARGET_ARM64)
static int pickNaNMulAdd(flag aIsQNaN, flag aIsSNaN, flag bIsQNaN, flag bIsSNaN, flag cIsQNaN, flag cIsSNaN,
                         flag infzero STATUS_PARAM)
{
    /* For ARM, the (inf,zero,qnan) case sets InvalidOp and returns
     * the default NaN
     */
    if(infzero && cIsQNaN) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 3;
    }

    /* This looks different from the ARM ARM pseudocode, because the ARM ARM
     * puts the operands to a fused mac operation (a*b)+c in the order c,a,b.
     */
    if(cIsSNaN) {
        return 2;
    } else if(aIsSNaN) {
        return 0;
    } else if(bIsSNaN) {
        return 1;
    } else if(cIsQNaN) {
        return 2;
    } else if(aIsQNaN) {
        return 0;
    } else {
        return 1;
    }
}
#elif defined(TARGET_PPC)
static int pickNaNMulAdd(flag aIsQNaN, flag aIsSNaN, flag bIsQNaN, flag bIsSNaN, flag cIsQNaN, flag cIsSNaN,
                         flag infzero STATUS_PARAM)
{
    /* For PPC, the (inf,zero,qnan) case sets InvalidOp, but we prefer
     * to return an input NaN if we have one (ie c) rather than generating
     * a default NaN
     */
    if(infzero) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 2;
    }

    /* If fRA is a NaN return it; otherwise if fRB is a NaN return it;
     * otherwise return fRC. Note that muladd on PPC is (fRA * fRC) + frB
     */
    if(aIsSNaN || aIsQNaN) {
        return 0;
    } else if(cIsSNaN || cIsQNaN) {
        return 2;
    } else {
        return 1;
    }
}
#elif defined(TARGET_XTENSA)
static int pickNaNMulAdd(flag aIsQNaN, flag aIsSNaN, flag bIsQNaN, flag bIsSNaN, flag cIsQNaN, flag cIsSNaN,
                         flag infzero STATUS_PARAM)
{
    /*
     * For Xtensa, the (inf,zero,nan) case sets InvalidOp and returns
     * an input NaN if we have one (ie c).
     */
    if(infzero) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 2;
    }
    if(STATUS(use_first_nan)) {
        if(aIsQNaN || aIsSNaN) {
            return 0;
        } else if(bIsQNaN || bIsSNaN) {
            return 1;
        } else {
            return 2;
        }
    } else {
        if(cIsQNaN || cIsSNaN) {
            return 2;
        } else if(bIsQNaN || bIsSNaN) {
            return 1;
        } else {
            return 0;
        }
    }
}
#else
/* A default implementation: prefer a to b to c.
 * This is unlikely to actually match any real implementation.
 */
static int pickNaNMulAdd(flag aIsQNaN, flag aIsSNaN, flag bIsQNaN, flag bIsSNaN, flag cIsQNaN, flag cIsSNaN,
                         flag infzero STATUS_PARAM)
{
    if(aIsSNaN || aIsQNaN) {
        return 0;
    } else if(bIsSNaN || bIsQNaN) {
        return 1;
    } else {
        return 2;
    }
}
#endif

/*----------------------------------------------------------------------------
 | Takes two single-precision floating-point values `a' and `b', one of which
 | is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
 | signaling NaN, the invalid exception is raised.
 *----------------------------------------------------------------------------*/

static float32 propagateFloat32NaN(float32 a, float32 b STATUS_PARAM)
{
    flag aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN;
    flag aIsLargerSignificand;
    uint32_t av, bv;

    aIsQuietNaN = float32_is_quiet_nan(a STATUS_VAR);
    aIsSignalingNaN = float32_is_signaling_nan(a STATUS_VAR);
    bIsQuietNaN = float32_is_quiet_nan(b STATUS_VAR);
    bIsSignalingNaN = float32_is_signaling_nan(b STATUS_VAR);
    av = float32_val(a);
    bv = float32_val(b);

    if(aIsSignalingNaN | bIsSignalingNaN) {
        float_raise(float_flag_invalid STATUS_VAR);
    }

    if(STATUS(default_nan_mode)) {
        return float32_default_nan;
    }

    if((uint32_t)(av << 1) < (uint32_t)(bv << 1)) {
        aIsLargerSignificand = 0;
    } else if((uint32_t)(bv << 1) < (uint32_t)(av << 1)) {
        aIsLargerSignificand = 1;
    } else {
        aIsLargerSignificand = (av < bv) ? 1 : 0;
    }

    if(pickNaN(aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN, aIsLargerSignificand STATUS_VAR)) {
        return float32_maybe_silence_nan(b STATUS_VAR);
    } else {
        return float32_maybe_silence_nan(a STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Takes three single-precision floating-point values `a', `b' and `c', one of
 | which is a NaN, and returns the appropriate NaN result.  If any of  `a',
 | `b' or `c' is a signaling NaN, the invalid exception is raised.
 | The input infzero indicates whether a*b was 0*inf or inf*0 (in which case
 | obviously c is a NaN, and whether to propagate c or some other NaN is
 | implementation defined).
 *----------------------------------------------------------------------------*/

static float32 propagateFloat32MulAddNaN(float32 a, float32 b, float32 c, flag infzero STATUS_PARAM)
{
    flag aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN, cIsQuietNaN, cIsSignalingNaN;
    int which;

    aIsQuietNaN = float32_is_quiet_nan(a STATUS_VAR);
    aIsSignalingNaN = float32_is_signaling_nan(a STATUS_VAR);
    bIsQuietNaN = float32_is_quiet_nan(b STATUS_VAR);
    bIsSignalingNaN = float32_is_signaling_nan(b STATUS_VAR);
    cIsQuietNaN = float32_is_quiet_nan(c STATUS_VAR);
    cIsSignalingNaN = float32_is_signaling_nan(c STATUS_VAR);

    if(aIsSignalingNaN | bIsSignalingNaN | cIsSignalingNaN) {
        float_raise(float_flag_invalid STATUS_VAR);
    }

    which = pickNaNMulAdd(aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN, cIsQuietNaN, cIsSignalingNaN,
                          infzero STATUS_VAR);

    if(STATUS(default_nan_mode)) {
        /* Note that this check is after pickNaNMulAdd so that function
         * has an opportunity to set the Invalid flag.
         */
        return float32_default_nan;
    }

    switch(which) {
        case 0:
            return float32_maybe_silence_nan(a STATUS_VAR);
        case 1:
            return float32_maybe_silence_nan(b STATUS_VAR);
        case 2:
            return float32_maybe_silence_nan(c STATUS_VAR);
        case 3:
        default:
            return float32_default_nan;
    }
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point value `a' is a quiet
 | NaN; otherwise returns 0.
 *----------------------------------------------------------------------------*/

int float64_is_quiet_nan(float64 a_ STATUS_PARAM)
{
    if(no_signaling_nans(status)) {
        return float64_is_any_nan(a_);
    }
    uint64_t a = float64_val(a_);
    return (LIT64(0xFFF0000000000000) <= (uint64_t)(a << 1));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point value `a' is a signaling
 | NaN; otherwise returns 0.
 *----------------------------------------------------------------------------*/

int float64_is_signaling_nan(float64 a_ STATUS_PARAM)
{
    if(no_signaling_nans(status)) {
        return 0;
    }
    uint64_t a = float64_val(a_);
    return (((a >> 51) & 0xFFF) == 0xFFE) && (a & LIT64(0x0007FFFFFFFFFFFF));
}

/*----------------------------------------------------------------------------
 | Returns a quiet NaN if the double-precision floating point value `a' is a
 | signaling NaN; otherwise returns `a'.
 *----------------------------------------------------------------------------*/

float64 float64_maybe_silence_nan(float64 a_ STATUS_PARAM)
{
    if(float64_is_signaling_nan(a_ STATUS_VAR)) {
        uint64_t a = float64_val(a_);
        a |= LIT64(0x0008000000000000);
        return make_float64(a);
    }
    return a_;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the double-precision floating-point NaN
 | `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
 | exception is raised.
 *----------------------------------------------------------------------------*/

static commonNaNT float64ToCommonNaN(float64 a STATUS_PARAM)
{
    commonNaNT z;

    if(float64_is_signaling_nan(a STATUS_VAR)) {
        float_raise(float_flag_invalid STATUS_VAR);
    }
    z.sign = float64_val(a) >> 63;
    z.low = 0;
    z.high = float64_val(a) << 12;
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the canonical NaN `a' to the double-
 | precision floating-point format.
 *----------------------------------------------------------------------------*/

static float64 commonNaNToFloat64(commonNaNT a STATUS_PARAM)
{
    uint64_t mantissa = a.high >> 12;

    if(STATUS(default_nan_mode)) {
        return float64_default_nan;
    }

    if(mantissa) {
        return make_float64((((uint64_t)a.sign) << 63) | LIT64(0x7FF0000000000000) | (a.high >> 12));
    } else {
        return float64_default_nan;
    }
}

/*----------------------------------------------------------------------------
 | Takes two double-precision floating-point values `a' and `b', one of which
 | is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
 | signaling NaN, the invalid exception is raised.
 *----------------------------------------------------------------------------*/

static float64 propagateFloat64NaN(float64 a, float64 b STATUS_PARAM)
{
    flag aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN;
    flag aIsLargerSignificand;
    uint64_t av, bv;

    aIsQuietNaN = float64_is_quiet_nan(a STATUS_VAR);
    aIsSignalingNaN = float64_is_signaling_nan(a STATUS_VAR);
    bIsQuietNaN = float64_is_quiet_nan(b STATUS_VAR);
    bIsSignalingNaN = float64_is_signaling_nan(b STATUS_VAR);
    av = float64_val(a);
    bv = float64_val(b);

    if(aIsSignalingNaN | bIsSignalingNaN) {
        float_raise(float_flag_invalid STATUS_VAR);
    }

    if(STATUS(default_nan_mode)) {
        return float64_default_nan;
    }

    if((uint64_t)(av << 1) < (uint64_t)(bv << 1)) {
        aIsLargerSignificand = 0;
    } else if((uint64_t)(bv << 1) < (uint64_t)(av << 1)) {
        aIsLargerSignificand = 1;
    } else {
        aIsLargerSignificand = (av < bv) ? 1 : 0;
    }

    if(pickNaN(aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN, aIsLargerSignificand STATUS_VAR)) {
        return float64_maybe_silence_nan(b STATUS_VAR);
    } else {
        return float64_maybe_silence_nan(a STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Takes three double-precision floating-point values `a', `b' and `c', one of
 | which is a NaN, and returns the appropriate NaN result.  If any of  `a',
 | `b' or `c' is a signaling NaN, the invalid exception is raised.
 | The input infzero indicates whether a*b was 0*inf or inf*0 (in which case
 | obviously c is a NaN, and whether to propagate c or some other NaN is
 | implementation defined).
 *----------------------------------------------------------------------------*/

static float64 propagateFloat64MulAddNaN(float64 a, float64 b, float64 c, flag infzero STATUS_PARAM)
{
    flag aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN, cIsQuietNaN, cIsSignalingNaN;
    int which;

    aIsQuietNaN = float64_is_quiet_nan(a STATUS_VAR);
    aIsSignalingNaN = float64_is_signaling_nan(a STATUS_VAR);
    bIsQuietNaN = float64_is_quiet_nan(b STATUS_VAR);
    bIsSignalingNaN = float64_is_signaling_nan(b STATUS_VAR);
    cIsQuietNaN = float64_is_quiet_nan(c STATUS_VAR);
    cIsSignalingNaN = float64_is_signaling_nan(c STATUS_VAR);

    if(aIsSignalingNaN | bIsSignalingNaN | cIsSignalingNaN) {
        float_raise(float_flag_invalid STATUS_VAR);
    }

    which = pickNaNMulAdd(aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN, cIsQuietNaN, cIsSignalingNaN,
                          infzero STATUS_VAR);

    if(STATUS(default_nan_mode)) {
        /* Note that this check is after pickNaNMulAdd so that function
         * has an opportunity to set the Invalid flag.
         */
        return float64_default_nan;
    }

    switch(which) {
        case 0:
            return float64_maybe_silence_nan(a STATUS_VAR);
        case 1:
            return float64_maybe_silence_nan(b STATUS_VAR);
        case 2:
            return float64_maybe_silence_nan(c STATUS_VAR);
        case 3:
        default:
            return float64_default_nan;
    }
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point value `a' is a
 | quiet NaN; otherwise returns 0. This slightly differs from the same
 | function for other types as floatx80 has an explicit bit.
 *----------------------------------------------------------------------------*/

int floatx80_is_quiet_nan(floatx80 a STATUS_PARAM)
{
    if(no_signaling_nans(status)) {
        return floatx80_is_any_nan(a);
    }
    return ((a.high & 0x7FFF) == 0x7FFF) && (LIT64(0x8000000000000000) <= ((uint64_t)(a.low << 1)));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point value `a' is a
 | signaling NaN; otherwise returns 0. This slightly differs from the same
 | function for other types as floatx80 has an explicit bit.
 *----------------------------------------------------------------------------*/

int floatx80_is_signaling_nan(floatx80 a STATUS_PARAM)
{
    if(no_signaling_nans(status)) {
        return 0;
    }
    uint64_t aLow;

    aLow = a.low & ~LIT64(0x4000000000000000);
    return ((a.high & 0x7FFF) == 0x7FFF) && (uint64_t)(aLow << 1) && (a.low == aLow);
}

/*----------------------------------------------------------------------------
 | Returns a quiet NaN if the extended double-precision floating point value
 | `a' is a signaling NaN; otherwise returns `a'.
 *----------------------------------------------------------------------------*/

floatx80 floatx80_maybe_silence_nan(floatx80 a STATUS_PARAM)
{
    if(floatx80_is_signaling_nan(a STATUS_VAR)) {
        a.low |= LIT64(0xC000000000000000);
        return a;
    }
    return a;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the extended double-precision floating-
 | point NaN `a' to the canonical NaN format.  If `a' is a signaling NaN, the
 | invalid exception is raised.
 *----------------------------------------------------------------------------*/

static commonNaNT floatx80ToCommonNaN(floatx80 a STATUS_PARAM)
{
    commonNaNT z;

    if(floatx80_is_signaling_nan(a STATUS_VAR)) {
        float_raise(float_flag_invalid STATUS_VAR);
    }
    if(a.low >> 63) {
        z.sign = a.high >> 15;
        z.low = 0;
        z.high = a.low << 1;
    } else {
        z.sign = floatx80_default_nan_high >> 15;
        z.low = 0;
        z.high = floatx80_default_nan_low << 1;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the canonical NaN `a' to the extended
 | double-precision floating-point format.
 *----------------------------------------------------------------------------*/

static floatx80 commonNaNToFloatx80(commonNaNT a STATUS_PARAM)
{
    floatx80 z;

    if(STATUS(default_nan_mode)) {
        z.low = floatx80_default_nan_low;
        z.high = floatx80_default_nan_high;
        return z;
    }

    if(a.high >> 1) {
        z.low = LIT64(0x8000000000000000) | a.high >> 1;
        z.high = (((uint16_t)a.sign) << 15) | 0x7FFF;
    } else {
        z.low = floatx80_default_nan_low;
        z.high = floatx80_default_nan_high;
    }

    return z;
}

/*----------------------------------------------------------------------------
 | Takes two extended double-precision floating-point values `a' and `b', one
 | of which is a NaN, and returns the appropriate NaN result.  If either `a' or
 | `b' is a signaling NaN, the invalid exception is raised.
 *----------------------------------------------------------------------------*/

static floatx80 propagateFloatx80NaN(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN;
    flag aIsLargerSignificand;

    aIsQuietNaN = floatx80_is_quiet_nan(a STATUS_VAR);
    aIsSignalingNaN = floatx80_is_signaling_nan(a STATUS_VAR);
    bIsQuietNaN = floatx80_is_quiet_nan(b STATUS_VAR);
    bIsSignalingNaN = floatx80_is_signaling_nan(b STATUS_VAR);

    if(aIsSignalingNaN | bIsSignalingNaN) {
        float_raise(float_flag_invalid STATUS_VAR);
    }

    if(STATUS(default_nan_mode)) {
        a.low = floatx80_default_nan_low;
        a.high = floatx80_default_nan_high;
        return a;
    }

    if(a.low < b.low) {
        aIsLargerSignificand = 0;
    } else if(b.low < a.low) {
        aIsLargerSignificand = 1;
    } else {
        aIsLargerSignificand = (a.high < b.high) ? 1 : 0;
    }

    if(pickNaN(aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN, aIsLargerSignificand STATUS_VAR)) {
        return floatx80_maybe_silence_nan(b STATUS_VAR);
    } else {
        return floatx80_maybe_silence_nan(a STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point value `a' is a quiet
 | NaN; otherwise returns 0.
 *----------------------------------------------------------------------------*/

int float128_is_quiet_nan(float128 a STATUS_PARAM)
{
    if(no_signaling_nans(status)) {
        return float128_is_any_nan(a);
    }
    return (LIT64(0xFFFE000000000000) <= (uint64_t)(a.high << 1)) && (a.low || (a.high & LIT64(0x0000FFFFFFFFFFFF)));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point value `a' is a
 | signaling NaN; otherwise returns 0.
 *----------------------------------------------------------------------------*/

int float128_is_signaling_nan(float128 a STATUS_PARAM)
{
    if(no_signaling_nans(status)) {
        return 0;
    }
    return (((a.high >> 47) & 0xFFFF) == 0xFFFE) && (a.low || (a.high & LIT64(0x00007FFFFFFFFFFF)));
}

/*----------------------------------------------------------------------------
 | Returns a quiet NaN if the quadruple-precision floating point value `a' is
 | a signaling NaN; otherwise returns `a'.
 *----------------------------------------------------------------------------*/

float128 float128_maybe_silence_nan(float128 a STATUS_PARAM)
{
    if(float128_is_signaling_nan(a STATUS_VAR)) {
        a.high |= LIT64(0x0000800000000000);
        return a;
    }
    return a;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the quadruple-precision floating-point NaN
 | `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
 | exception is raised.
 *----------------------------------------------------------------------------*/

static commonNaNT float128ToCommonNaN(float128 a STATUS_PARAM)
{
    commonNaNT z;

    if(float128_is_signaling_nan(a STATUS_VAR)) {
        float_raise(float_flag_invalid STATUS_VAR);
    }
    z.sign = a.high >> 63;
    shortShift128Left(a.high, a.low, 16, &z.high, &z.low);
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the canonical NaN `a' to the quadruple-
 | precision floating-point format.
 *----------------------------------------------------------------------------*/

static float128 commonNaNToFloat128(commonNaNT a STATUS_PARAM)
{
    float128 z;

    if(STATUS(default_nan_mode)) {
        z.low = float128_default_nan_low;
        z.high = float128_default_nan_high;
        return z;
    }

    shift128Right(a.high, a.low, 16, &z.high, &z.low);
    z.high |= (((uint64_t)a.sign) << 63) | LIT64(0x7FFF000000000000);
    return z;
}

/*----------------------------------------------------------------------------
 | Takes two quadruple-precision floating-point values `a' and `b', one of
 | which is a NaN, and returns the appropriate NaN result.  If either `a' or
 | `b' is a signaling NaN, the invalid exception is raised.
 *----------------------------------------------------------------------------*/

static float128 propagateFloat128NaN(float128 a, float128 b STATUS_PARAM)
{
    flag aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN;
    flag aIsLargerSignificand;

    aIsQuietNaN = float128_is_quiet_nan(a STATUS_VAR);
    aIsSignalingNaN = float128_is_signaling_nan(a STATUS_VAR);
    bIsQuietNaN = float128_is_quiet_nan(b STATUS_VAR);
    bIsSignalingNaN = float128_is_signaling_nan(b STATUS_VAR);

    if(aIsSignalingNaN | bIsSignalingNaN) {
        float_raise(float_flag_invalid STATUS_VAR);
    }

    if(STATUS(default_nan_mode)) {
        a.low = float128_default_nan_low;
        a.high = float128_default_nan_high;
        return a;
    }

    if(lt128(a.high << 1, a.low, b.high << 1, b.low)) {
        aIsLargerSignificand = 0;
    } else if(lt128(b.high << 1, b.low, a.high << 1, a.low)) {
        aIsLargerSignificand = 1;
    } else {
        aIsLargerSignificand = (a.high < b.high) ? 1 : 0;
    }

    if(pickNaN(aIsQuietNaN, aIsSignalingNaN, bIsQuietNaN, bIsSignalingNaN, aIsLargerSignificand STATUS_VAR)) {
        return float128_maybe_silence_nan(b STATUS_VAR);
    } else {
        return float128_maybe_silence_nan(a STATUS_VAR);
    }
}
