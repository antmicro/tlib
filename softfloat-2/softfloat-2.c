/*
 * QEMU float support
 *
 * Derived from SoftFloat.
 */

/*============================================================================

   This C source file is part of the SoftFloat IEC/IEEE Floating-point Arithmetic
   Package, Release 2b.

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

/*
 * Parts of this code are based upon newer Softfloat implementations, under the following license:
 */

/*============================================================================

   This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
   Package, Release 3e, by John R. Hauser.

   Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the
   University of California.  All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions, and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions, and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    3. Neither the name of the University nor the names of its contributors may
       be used to endorse or promote products derived from this software without
       specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
   DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   =============================================================================*/

/* softfloat (and in particular the code in softfloat-specialize.h) is
 * target-dependent and needs the TARGET_* macros.
 */
#include "softfloat-2.h"

/*----------------------------------------------------------------------------
 | Primitive arithmetic functions, including multi-word arithmetic, and
 | division and square root approximations.  (Can be specialized to target if
 | desired.)
 *----------------------------------------------------------------------------*/
#include "softfloat-2-macros.h"

/*----------------------------------------------------------------------------
 | Functions and definitions to determine:  (1) whether tininess for underflow
 | is detected before or after rounding by default, (2) what (if anything)
 | happens when exceptions are raised, (3) how signaling NaNs are distinguished
 | from quiet NaNs, (4) the default generated quiet NaNs, and (5) how NaNs
 | are propagated from function inputs to output.  These details are target-
 | specific.
 *----------------------------------------------------------------------------*/
#include "softfloat-2-specialize.h"

void set_float_rounding_mode(int val STATUS_PARAM)
{
    STATUS(float_rounding_mode) = val;
}

void set_float_exception_flags(int val STATUS_PARAM)
{
    STATUS(float_exception_flags) = val;
}

void set_floatx80_rounding_precision(int val STATUS_PARAM)
{
    STATUS(floatx80_rounding_precision) = val;
}

/*----------------------------------------------------------------------------
 | Returns the fraction bits of the half-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE uint32_t extractFloat16Frac(float16 a)
{
    return float16_val(a) & 0x3ff;
}

/*----------------------------------------------------------------------------
 | Returns the exponent bits of the half-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE int16 extractFloat16Exp(float16 a)
{
    return (float16_val(a) >> 10) & 0x1f;
}

/*----------------------------------------------------------------------------
 | Returns the sign bit of the single-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE flag extractFloat16Sign(float16 a)
{
    return float16_val(a) >> 15;
}

/*----------------------------------------------------------------------------
 | Takes a 64-bit fixed-point value `absZ' with binary point between bits 6
 | and 7, and returns the properly rounded 32-bit integer corresponding to the
 | input.  If `zSign' is 1, the input is negated before being converted to an
 | integer.  Bit 63 of `absZ' must be zero.  Ordinarily, the fixed-point input
 | is simply rounded to an integer, with the inexact exception raised if the
 | input cannot be represented exactly as an integer.  However, if the fixed-
 | point input is too large, the invalid exception is raised and the largest
 | positive or negative integer is returned.
 *----------------------------------------------------------------------------*/

static int32 roundAndPackInt32(flag zSign, uint64_t absZ STATUS_PARAM)
{
    int8 roundingMode;
    flag roundNearestEven;
    int8 roundIncrement, roundBits;
    int32 z;

    roundingMode = STATUS(float_rounding_mode);
    roundNearestEven = (roundingMode == float_round_nearest_even);
    roundIncrement = 0x40;
    if(!roundNearestEven && (roundingMode != float_round_ties_away)) {
        if(roundingMode == float_round_to_zero) {
            roundIncrement = 0;
        } else {
            roundIncrement = 0x7F;
            if(zSign) {
                if(roundingMode == float_round_up) {
                    roundIncrement = 0;
                }
            } else {
                if(roundingMode == float_round_down) {
                    roundIncrement = 0;
                }
            }
        }
    }
    roundBits = absZ & 0x7F;
    absZ = (absZ + roundIncrement) >> 7;
    absZ &= ~(((roundBits ^ 0x40) == 0) & roundNearestEven);
    z = absZ;
    if(zSign) {
        z = -z;
    }
    if((absZ >> 32) || (z && ((z < 0) ^ zSign))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return zSign ? (int32_t)0x80000000 : 0x7FFFFFFF;
    }
    if(roundBits) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Takes the 128-bit fixed-point value formed by concatenating `absZ0' and
 | `absZ1', with binary point between bits 63 and 64 (between the input words),
 | and returns the properly rounded 64-bit integer corresponding to the input.
 | If `zSign' is 1, the input is negated before being converted to an integer.
 | Ordinarily, the fixed-point input is simply rounded to an integer, with
 | the inexact exception raised if the input cannot be represented exactly as
 | an integer.  However, if the fixed-point input is too large, the invalid
 | exception is raised and the largest positive or negative integer is
 | returned.
 *----------------------------------------------------------------------------*/

static int64 roundAndPackInt64(flag zSign, uint64_t absZ0, uint64_t absZ1 STATUS_PARAM)
{
    int8 roundingMode;
    flag roundNearestEven, increment;
    int64 z;

    roundingMode = STATUS(float_rounding_mode);
    roundNearestEven = (roundingMode == float_round_nearest_even);
    increment = ((int64_t)absZ1 < 0);
    if(!roundNearestEven && (roundingMode != float_round_ties_away)) {
        if(roundingMode == float_round_to_zero) {
            increment = 0;
        } else {
            if(zSign) {
                increment = (roundingMode == float_round_down) && absZ1;
            } else {
                increment = (roundingMode == float_round_up) && absZ1;
            }
        }
    }
    if(increment) {
        ++absZ0;
        if(absZ0 == 0) {
            goto overflow;
        }
        absZ0 &= ~(((uint64_t)(absZ1 << 1) == 0) & roundNearestEven);
    }
    z = absZ0;
    if(zSign) {
        z = -z;
    }
    if(z && ((z < 0) ^ zSign)) {
    overflow:
        float_raise(float_flag_invalid STATUS_VAR);
        return zSign ? (int64_t)LIT64(0x8000000000000000) : LIT64(0x7FFFFFFFFFFFFFFF);
    }
    if(absZ1) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

static int64_t roundAndPackUint64(flag zSign, uint64_t absZ0, uint64_t absZ1, float_status *status)
{
    int8_t roundingMode;
    flag roundNearestEven, increment = 0;

    roundingMode = status->float_rounding_mode;
    roundNearestEven = (roundingMode == float_round_nearest_even);
    increment = ((int64_t)absZ1 < 0);
    if(!roundNearestEven && (roundingMode != float_round_ties_away)) {
        if(roundingMode == float_round_to_zero) {
            increment = 0;
        } else if(absZ1) {
            if(zSign) {
                increment = (roundingMode == float_round_down) && absZ1;
            } else {
                increment = (roundingMode == float_round_up) && absZ1;
            }
        }
    }
    if(increment) {
        ++absZ0;
        if(absZ0 == 0) {
            float_raise(float_flag_invalid, status);
            return LIT64(0xFFFFFFFFFFFFFFFF);
        }
        absZ0 &= ~(((uint64_t)(absZ1 << 1) == 0) & roundNearestEven);
    }

    if(zSign && absZ0) {
        float_raise(float_flag_invalid, status);
        return 0;
    }

    if(absZ1) {
        status->float_exception_flags |= float_flag_inexact;
    }
    return absZ0;
}

/*----------------------------------------------------------------------------
 | Returns the fraction bits of the single-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE uint32_t extractFloat32Frac(float32 a)
{

    return float32_val(a) & 0x007FFFFF;
}

/*----------------------------------------------------------------------------
 | Returns the exponent bits of the single-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE int16 extractFloat32Exp(float32 a)
{

    return (float32_val(a) >> 23) & 0xFF;
}

/*----------------------------------------------------------------------------
 | Returns the sign bit of the single-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE flag extractFloat32Sign(float32 a)
{

    return float32_val(a) >> 31;
}

/*----------------------------------------------------------------------------
 | If `a' is denormal and we are in flush-to-zero mode then set the
 | input-denormal exception and return zero. Otherwise just return the value.
 *----------------------------------------------------------------------------*/
float32 float32_squash_input_denormal(float32 a STATUS_PARAM)
{
    if(STATUS(flush_inputs_to_zero)) {
        if(extractFloat32Exp(a) == 0 && extractFloat32Frac(a) != 0) {
            float_raise(float_flag_input_denormal STATUS_VAR);
            return make_float32(float32_val(a) & 0x80000000);
        }
    }
    return a;
}

/*----------------------------------------------------------------------------
 | Normalizes the subnormal single-precision floating-point value represented
 | by the denormalized significand `aSig'.  The normalized exponent and
 | significand are stored at the locations pointed to by `zExpPtr' and
 | `zSigPtr', respectively.
 *----------------------------------------------------------------------------*/

static void normalizeFloat32Subnormal(uint32_t aSig, int16 *zExpPtr, uint32_t *zSigPtr)
{
    int8 shiftCount;

    shiftCount = countLeadingZeros32(aSig) - 8;
    *zSigPtr = aSig << shiftCount;
    *zExpPtr = 1 - shiftCount;
}

/*----------------------------------------------------------------------------
 | Packs the sign `zSign', exponent `zExp', and significand `zSig' into a
 | single-precision floating-point value, returning the result.  After being
 | shifted into the proper positions, the three fields are simply added
 | together to form the result.  This means that any integer portion of `zSig'
 | will be added into the exponent.  Since a properly normalized significand
 | will have an integer portion equal to 1, the `zExp' input should be 1 less
 | than the desired result exponent whenever `zSig' is a complete, normalized
 | significand.
 *----------------------------------------------------------------------------*/

INLINE float32 packFloat32(flag zSign, int16 zExp, uint32_t zSig)
{

    return make_float32((((uint32_t)zSign) << 31) + (((uint32_t)zExp) << 23) + zSig);
}

/*----------------------------------------------------------------------------
 | Takes an abstract floating-point value having sign `zSign', exponent `zExp',
 | and significand `zSig', and returns the proper single-precision floating-
 | point value corresponding to the abstract input.  Ordinarily, the abstract
 | value is simply rounded and packed into the single-precision format, with
 | the inexact exception raised if the abstract input cannot be represented
 | exactly.  However, if the abstract value is too large, the overflow and
 | inexact exceptions are raised and an infinity or maximal finite value is
 | returned.  If the abstract value is too small, the input value is rounded to
 | a subnormal number, and the underflow and inexact exceptions are raised if
 | the abstract input cannot be represented exactly as a subnormal single-
 | precision floating-point number.
 |     The input significand `zSig' has its binary point between bits 30
 | and 29, which is 7 bits to the left of the usual location.  This shifted
 | significand must be normalized or smaller.  If `zSig' is not normalized,
 | `zExp' must be 0; in that case, the result returned is a subnormal number,
 | and it must not require rounding.  In the usual case that `zSig' is
 | normalized, `zExp' must be 1 less than the ``true'' floating-point exponent.
 | The handling of underflow and overflow follows the IEC/IEEE Standard for
 | Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static float32 roundAndPackFloat32(flag zSign, int16 zExp, uint32_t zSig STATUS_PARAM)
{
    int8 roundingMode;
    flag roundNearestEven;
    int8 roundIncrement, roundBits;
    flag isTiny;

    roundingMode = STATUS(float_rounding_mode);
    roundNearestEven = (roundingMode == float_round_nearest_even);
    roundIncrement = 0x40;
    if(!roundNearestEven && roundingMode != float_round_ties_away) {
        if(roundingMode == float_round_to_zero) {
            roundIncrement = 0;
        } else {
            roundIncrement = 0x7F;
            if(zSign) {
                if(roundingMode == float_round_up) {
                    roundIncrement = 0;
                }
            } else {
                if(roundingMode == float_round_down) {
                    roundIncrement = 0;
                }
            }
        }
    }
    roundBits = zSig & 0x7F;
    if(0xFD <= (uint16_t)zExp) {
        if((0xFD < zExp) || ((zExp == 0xFD) && ((int32_t)(zSig + roundIncrement) < 0))) {
            float_raise(float_flag_overflow | float_flag_inexact STATUS_VAR);
            return packFloat32(zSign, 0xFF, -(roundIncrement == 0));
        }
        if(zExp < 0) {
            if(STATUS(flush_to_zero)) {
                float_raise((int8)float_flag_output_denormal STATUS_VAR);
                return packFloat32(zSign, 0, 0);
            }
            isTiny = (STATUS(float_detect_tininess) == float_tininess_before_rounding) || (zExp < -1) ||
                     (zSig + roundIncrement < 0x80000000);
            shift32RightJamming(zSig, -zExp, &zSig);
            zExp = 0;
            roundBits = zSig & 0x7F;
            if(isTiny && roundBits) {
                float_raise(float_flag_underflow STATUS_VAR);
            }
        }
    }
    if(roundBits) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    zSig = (zSig + roundIncrement) >> 7;
    zSig &= ~(((roundBits ^ 0x40) == 0) & roundNearestEven);
    if(zSig == 0) {
        zExp = 0;
    }
    return packFloat32(zSign, zExp, zSig);
}

/*----------------------------------------------------------------------------
 | Takes an abstract floating-point value having sign `zSign', exponent `zExp',
 | and significand `zSig', and returns the proper single-precision floating-
 | point value corresponding to the abstract input.  This routine is just like
 | `roundAndPackFloat32' except that `zSig' does not have to be normalized.
 | Bit 31 of `zSig' must be zero, and `zExp' must be 1 less than the ``true''
 | floating-point exponent.
 *----------------------------------------------------------------------------*/

static float32 normalizeRoundAndPackFloat32(flag zSign, int16 zExp, uint32_t zSig STATUS_PARAM)
{
    int8 shiftCount;

    shiftCount = countLeadingZeros32(zSig) - 1;
    return roundAndPackFloat32(zSign, zExp - shiftCount, zSig << shiftCount STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the fraction bits of the double-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE uint64_t extractFloat64Frac(float64 a)
{

    return float64_val(a) & LIT64(0x000FFFFFFFFFFFFF);
}

/*----------------------------------------------------------------------------
 | Returns the exponent bits of the double-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE int16 extractFloat64Exp(float64 a)
{

    return (float64_val(a) >> 52) & 0x7FF;
}

/*----------------------------------------------------------------------------
 | Returns the sign bit of the double-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE flag extractFloat64Sign(float64 a)
{

    return float64_val(a) >> 63;
}

/*----------------------------------------------------------------------------
 | If `a' is denormal and we are in flush-to-zero mode then set the
 | input-denormal exception and return zero. Otherwise just return the value.
 *----------------------------------------------------------------------------*/
float64 float64_squash_input_denormal(float64 a STATUS_PARAM)
{
    if(STATUS(flush_inputs_to_zero)) {
        if(extractFloat64Exp(a) == 0 && extractFloat64Frac(a) != 0) {
            float_raise(float_flag_input_denormal STATUS_VAR);
            return make_float64(float64_val(a) & (1ULL << 63));
        }
    }
    return a;
}

/*----------------------------------------------------------------------------
 | Normalizes the subnormal double-precision floating-point value represented
 | by the denormalized significand `aSig'.  The normalized exponent and
 | significand are stored at the locations pointed to by `zExpPtr' and
 | `zSigPtr', respectively.
 *----------------------------------------------------------------------------*/

static void normalizeFloat64Subnormal(uint64_t aSig, int16 *zExpPtr, uint64_t *zSigPtr)
{
    int8 shiftCount;

    shiftCount = countLeadingZeros64(aSig) - 11;
    *zSigPtr = aSig << shiftCount;
    *zExpPtr = 1 - shiftCount;
}

/*----------------------------------------------------------------------------
 | Packs the sign `zSign', exponent `zExp', and significand `zSig' into a
 | double-precision floating-point value, returning the result.  After being
 | shifted into the proper positions, the three fields are simply added
 | together to form the result.  This means that any integer portion of `zSig'
 | will be added into the exponent.  Since a properly normalized significand
 | will have an integer portion equal to 1, the `zExp' input should be 1 less
 | than the desired result exponent whenever `zSig' is a complete, normalized
 | significand.
 *----------------------------------------------------------------------------*/

INLINE float64 packFloat64(flag zSign, int16 zExp, uint64_t zSig)
{

    return make_float64((((uint64_t)zSign) << 63) + (((uint64_t)zExp) << 52) + zSig);
}

/*----------------------------------------------------------------------------
 | Takes an abstract floating-point value having sign `zSign', exponent `zExp',
 | and significand `zSig', and returns the proper double-precision floating-
 | point value corresponding to the abstract input.  Ordinarily, the abstract
 | value is simply rounded and packed into the double-precision format, with
 | the inexact exception raised if the abstract input cannot be represented
 | exactly.  However, if the abstract value is too large, the overflow and
 | inexact exceptions are raised and an infinity or maximal finite value is
 | returned.  If the abstract value is too small, the input value is rounded
 | to a subnormal number, and the underflow and inexact exceptions are raised
 | if the abstract input cannot be represented exactly as a subnormal double-
 | precision floating-point number.
 |     The input significand `zSig' has its binary point between bits 62
 | and 61, which is 10 bits to the left of the usual location.  This shifted
 | significand must be normalized or smaller.  If `zSig' is not normalized,
 | `zExp' must be 0; in that case, the result returned is a subnormal number,
 | and it must not require rounding.  In the usual case that `zSig' is
 | normalized, `zExp' must be 1 less than the ``true'' floating-point exponent.
 | The handling of underflow and overflow follows the IEC/IEEE Standard for
 | Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static float64 roundAndPackFloat64(flag zSign, int16 zExp, uint64_t zSig STATUS_PARAM)
{
    int8 roundingMode;
    flag roundNearestEven;
    int16 roundIncrement, roundBits;
    flag isTiny;

    roundingMode = STATUS(float_rounding_mode);
    roundNearestEven = (roundingMode == float_round_nearest_even);
    roundIncrement = 0x200;
    if(!roundNearestEven && roundingMode != float_round_ties_away) {
        if(roundingMode == float_round_to_zero) {
            roundIncrement = 0;
        } else {
            roundIncrement = 0x3FF;
            if(zSign) {
                if(roundingMode == float_round_up) {
                    roundIncrement = 0;
                }
            } else {
                if(roundingMode == float_round_down) {
                    roundIncrement = 0;
                }
            }
        }
    }
    roundBits = zSig & 0x3FF;
    if(0x7FD <= (uint16_t)zExp) {
        if((0x7FD < zExp) || ((zExp == 0x7FD) && ((int64_t)(zSig + roundIncrement) < 0))) {
            float_raise(float_flag_overflow | float_flag_inexact STATUS_VAR);
            return packFloat64(zSign, 0x7FF, -(roundIncrement == 0));
        }
        if(zExp < 0) {
            if(STATUS(flush_to_zero)) {
                float_raise((int8)float_flag_output_denormal STATUS_VAR);
                return packFloat64(zSign, 0, 0);
            }
            isTiny = (STATUS(float_detect_tininess) == float_tininess_before_rounding) || (zExp < -1) ||
                     (zSig + roundIncrement < LIT64(0x8000000000000000));
            shift64RightJamming(zSig, -zExp, &zSig);
            zExp = 0;
            roundBits = zSig & 0x3FF;
            if(isTiny && roundBits) {
                float_raise(float_flag_underflow STATUS_VAR);
            }
        }
    }
    if(roundBits) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    zSig = (zSig + roundIncrement) >> 10;
    zSig &= ~(((roundBits ^ 0x200) == 0) & roundNearestEven);
    if(zSig == 0) {
        zExp = 0;
    }
    return packFloat64(zSign, zExp, zSig);
}

/*----------------------------------------------------------------------------
 | Takes an abstract floating-point value having sign `zSign', exponent `zExp',
 | and significand `zSig', and returns the proper double-precision floating-
 | point value corresponding to the abstract input.  This routine is just like
 | `roundAndPackFloat64' except that `zSig' does not have to be normalized.
 | Bit 63 of `zSig' must be zero, and `zExp' must be 1 less than the ``true''
 | floating-point exponent.
 *----------------------------------------------------------------------------*/

static float64 normalizeRoundAndPackFloat64(flag zSign, int16 zExp, uint64_t zSig STATUS_PARAM)
{
    int8 shiftCount;

    shiftCount = countLeadingZeros64(zSig) - 1;
    return roundAndPackFloat64(zSign, zExp - shiftCount, zSig << shiftCount STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the fraction bits of the extended double-precision floating-point
 | value `a'.
 *----------------------------------------------------------------------------*/

INLINE uint64_t extractFloatx80Frac(floatx80 a)
{

    return a.low;
}

/*----------------------------------------------------------------------------
 | Returns the exponent bits of the extended double-precision floating-point
 | value `a'.
 *----------------------------------------------------------------------------*/

INLINE int32 extractFloatx80Exp(floatx80 a)
{

    return a.high & 0x7FFF;
}

/*----------------------------------------------------------------------------
 | Returns the sign bit of the extended double-precision floating-point value
 | `a'.
 *----------------------------------------------------------------------------*/

INLINE flag extractFloatx80Sign(floatx80 a)
{

    return a.high >> 15;
}

/*----------------------------------------------------------------------------
 | Normalizes the subnormal extended double-precision floating-point value
 | represented by the denormalized significand `aSig'.  The normalized exponent
 | and significand are stored at the locations pointed to by `zExpPtr' and
 | `zSigPtr', respectively.
 *----------------------------------------------------------------------------*/

static void normalizeFloatx80Subnormal(uint64_t aSig, int32 *zExpPtr, uint64_t *zSigPtr)
{
    int8 shiftCount;

    shiftCount = countLeadingZeros64(aSig);
    *zSigPtr = aSig << shiftCount;
    *zExpPtr = 1 - shiftCount;
}

/*----------------------------------------------------------------------------
 | Packs the sign `zSign', exponent `zExp', and significand `zSig' into an
 | extended double-precision floating-point value, returning the result.
 *----------------------------------------------------------------------------*/

INLINE floatx80 packFloatx80(flag zSign, int32 zExp, uint64_t zSig)
{
    floatx80 z;

    z.low = zSig;
    z.high = (((uint16_t)zSign) << 15) + zExp;
    return z;
}

/*----------------------------------------------------------------------------
 | Takes an abstract floating-point value having sign `zSign', exponent `zExp',
 | and extended significand formed by the concatenation of `zSig0' and `zSig1',
 | and returns the proper extended double-precision floating-point value
 | corresponding to the abstract input.  Ordinarily, the abstract value is
 | rounded and packed into the extended double-precision format, with the
 | inexact exception raised if the abstract input cannot be represented
 | exactly.  However, if the abstract value is too large, the overflow and
 | inexact exceptions are raised and an infinity or maximal finite value is
 | returned.  If the abstract value is too small, the input value is rounded to
 | a subnormal number, and the underflow and inexact exceptions are raised if
 | the abstract input cannot be represented exactly as a subnormal extended
 | double-precision floating-point number.
 |     If `roundingPrecision' is 32 or 64, the result is rounded to the same
 | number of bits as single or double precision, respectively.  Otherwise, the
 | result is rounded to the full precision of the extended double-precision
 | format.
 |     The input significand must be normalized or smaller.  If the input
 | significand is not normalized, `zExp' must be 0; in that case, the result
 | returned is a subnormal number, and it must not require rounding.  The
 | handling of underflow and overflow follows the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static floatx80 roundAndPackFloatx80(int8 roundingPrecision, flag zSign, int32 zExp, uint64_t zSig0, uint64_t zSig1 STATUS_PARAM)
{
    int8 roundingMode;
    flag roundNearestEven, increment, isTiny;
    int64 roundIncrement, roundMask, roundBits;

    roundingMode = STATUS(float_rounding_mode);
    roundNearestEven = (roundingMode == float_round_nearest_even);
    if(roundingPrecision == 80) {
        goto precision80;
    }
    if(roundingPrecision == 64) {
        roundIncrement = LIT64(0x0000000000000400);
        roundMask = LIT64(0x00000000000007FF);
    } else if(roundingPrecision == 32) {
        roundIncrement = LIT64(0x0000008000000000);
        roundMask = LIT64(0x000000FFFFFFFFFF);
    } else {
        goto precision80;
    }
    zSig0 |= (zSig1 != 0);
    if(!roundNearestEven) {
        if(roundingMode == float_round_to_zero) {
            roundIncrement = 0;
        } else {
            roundIncrement = roundMask;
            if(zSign) {
                if(roundingMode == float_round_up) {
                    roundIncrement = 0;
                }
            } else {
                if(roundingMode == float_round_down) {
                    roundIncrement = 0;
                }
            }
        }
    }
    roundBits = zSig0 & roundMask;
    if(0x7FFD <= (uint32_t)(zExp - 1)) {
        if((0x7FFE < zExp) || ((zExp == 0x7FFE) && (zSig0 + roundIncrement < zSig0))) {
            goto overflow;
        }
        if(zExp <= 0) {
            if(STATUS(flush_to_zero)) {
                float_raise((int8)float_flag_output_denormal STATUS_VAR);
                return packFloatx80(zSign, 0, 0);
            }
            isTiny = (STATUS(float_detect_tininess) == float_tininess_before_rounding) || (zExp < 0) ||
                     (zSig0 <= zSig0 + roundIncrement);
            shift64RightJamming(zSig0, 1 - zExp, &zSig0);
            zExp = 0;
            roundBits = zSig0 & roundMask;
            if(isTiny && roundBits) {
                float_raise(float_flag_underflow STATUS_VAR);
            }
            if(roundBits) {
                STATUS(float_exception_flags) |= float_flag_inexact;
            }
            zSig0 += roundIncrement;
            if((int64_t)zSig0 < 0) {
                zExp = 1;
            }
            roundIncrement = roundMask + 1;
            if(roundNearestEven && (roundBits << 1 == roundIncrement)) {
                roundMask |= roundIncrement;
            }
            zSig0 &= ~roundMask;
            return packFloatx80(zSign, zExp, zSig0);
        }
    }
    if(roundBits) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    zSig0 += roundIncrement;
    if(zSig0 < roundIncrement) {
        ++zExp;
        zSig0 = LIT64(0x8000000000000000);
    }
    roundIncrement = roundMask + 1;
    if(roundNearestEven && (roundBits << 1 == roundIncrement)) {
        roundMask |= roundIncrement;
    }
    zSig0 &= ~roundMask;
    if(zSig0 == 0) {
        zExp = 0;
    }
    return packFloatx80(zSign, zExp, zSig0);
precision80:
    increment = ((int64_t)zSig1 < 0);
    if(!roundNearestEven) {
        if(roundingMode == float_round_to_zero) {
            increment = 0;
        } else {
            if(zSign) {
                increment = (roundingMode == float_round_down) && zSig1;
            } else {
                increment = (roundingMode == float_round_up) && zSig1;
            }
        }
    }
    if(0x7FFD <= (uint32_t)(zExp - 1)) {
        if((0x7FFE < zExp) || ((zExp == 0x7FFE) && (zSig0 == LIT64(0xFFFFFFFFFFFFFFFF)) && increment)) {
            roundMask = 0;
        overflow:
            float_raise(float_flag_overflow | float_flag_inexact STATUS_VAR);
            if((roundingMode == float_round_to_zero) || (zSign && (roundingMode == float_round_up)) ||
               (!zSign && (roundingMode == float_round_down))) {
                return packFloatx80(zSign, 0x7FFE, ~roundMask);
            }
            return packFloatx80(zSign, 0x7FFF, LIT64(0x8000000000000000));
        }
        if(zExp <= 0) {
            isTiny = (STATUS(float_detect_tininess) == float_tininess_before_rounding) || (zExp < 0) || !increment ||
                     (zSig0 < LIT64(0xFFFFFFFFFFFFFFFF));
            shift64ExtraRightJamming(zSig0, zSig1, 1 - zExp, &zSig0, &zSig1);
            zExp = 0;
            if(isTiny && zSig1) {
                float_raise(float_flag_underflow STATUS_VAR);
            }
            if(zSig1) {
                STATUS(float_exception_flags) |= float_flag_inexact;
            }
            if(roundNearestEven) {
                increment = ((int64_t)zSig1 < 0);
            } else {
                if(zSign) {
                    increment = (roundingMode == float_round_down) && zSig1;
                } else {
                    increment = (roundingMode == float_round_up) && zSig1;
                }
            }
            if(increment) {
                ++zSig0;
                zSig0 &= ~(((uint64_t)(zSig1 << 1) == 0) & roundNearestEven);
                if((int64_t)zSig0 < 0) {
                    zExp = 1;
                }
            }
            return packFloatx80(zSign, zExp, zSig0);
        }
    }
    if(zSig1) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    if(increment) {
        ++zSig0;
        if(zSig0 == 0) {
            ++zExp;
            zSig0 = LIT64(0x8000000000000000);
        } else {
            zSig0 &= ~(((uint64_t)(zSig1 << 1) == 0) & roundNearestEven);
        }
    } else {
        if(zSig0 == 0) {
            zExp = 0;
        }
    }
    return packFloatx80(zSign, zExp, zSig0);
}

/*----------------------------------------------------------------------------
 | Takes an abstract floating-point value having sign `zSign', exponent
 | `zExp', and significand formed by the concatenation of `zSig0' and `zSig1',
 | and returns the proper extended double-precision floating-point value
 | corresponding to the abstract input.  This routine is just like
 | `roundAndPackFloatx80' except that the input significand does not have to be
 | normalized.
 *----------------------------------------------------------------------------*/

static floatx80 normalizeRoundAndPackFloatx80(int8 roundingPrecision, flag zSign, int32 zExp, uint64_t zSig0,
                                              uint64_t zSig1 STATUS_PARAM)
{
    int8 shiftCount;

    if(zSig0 == 0) {
        zSig0 = zSig1;
        zSig1 = 0;
        zExp -= 64;
    }
    shiftCount = countLeadingZeros64(zSig0);
    shortShift128Left(zSig0, zSig1, shiftCount, &zSig0, &zSig1);
    zExp -= shiftCount;
    return roundAndPackFloatx80(roundingPrecision, zSign, zExp, zSig0, zSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the least-significant 64 fraction bits of the quadruple-precision
 | floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE uint64_t extractFloat128Frac1(float128 a)
{

    return a.low;
}

/*----------------------------------------------------------------------------
 | Returns the most-significant 48 fraction bits of the quadruple-precision
 | floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE uint64_t extractFloat128Frac0(float128 a)
{

    return a.high & LIT64(0x0000FFFFFFFFFFFF);
}

/*----------------------------------------------------------------------------
 | Returns the exponent bits of the quadruple-precision floating-point value
 | `a'.
 *----------------------------------------------------------------------------*/

INLINE int32 extractFloat128Exp(float128 a)
{

    return (a.high >> 48) & 0x7FFF;
}

/*----------------------------------------------------------------------------
 | Returns the sign bit of the quadruple-precision floating-point value `a'.
 *----------------------------------------------------------------------------*/

INLINE flag extractFloat128Sign(float128 a)
{

    return a.high >> 63;
}

/*----------------------------------------------------------------------------
 | Normalizes the subnormal quadruple-precision floating-point value
 | represented by the denormalized significand formed by the concatenation of
 | `aSig0' and `aSig1'.  The normalized exponent is stored at the location
 | pointed to by `zExpPtr'.  The most significant 49 bits of the normalized
 | significand are stored at the location pointed to by `zSig0Ptr', and the
 | least significant 64 bits of the normalized significand are stored at the
 | location pointed to by `zSig1Ptr'.
 *----------------------------------------------------------------------------*/

static void normalizeFloat128Subnormal(uint64_t aSig0, uint64_t aSig1, int32 *zExpPtr, uint64_t *zSig0Ptr, uint64_t *zSig1Ptr)
{
    int8 shiftCount;

    if(aSig0 == 0) {
        shiftCount = countLeadingZeros64(aSig1) - 15;
        if(shiftCount < 0) {
            *zSig0Ptr = aSig1 >> (-shiftCount);
            *zSig1Ptr = aSig1 << (shiftCount & 63);
        } else {
            *zSig0Ptr = aSig1 << shiftCount;
            *zSig1Ptr = 0;
        }
        *zExpPtr = -shiftCount - 63;
    } else {
        shiftCount = countLeadingZeros64(aSig0) - 15;
        shortShift128Left(aSig0, aSig1, shiftCount, zSig0Ptr, zSig1Ptr);
        *zExpPtr = 1 - shiftCount;
    }
}

/*----------------------------------------------------------------------------
 | Packs the sign `zSign', the exponent `zExp', and the significand formed
 | by the concatenation of `zSig0' and `zSig1' into a quadruple-precision
 | floating-point value, returning the result.  After being shifted into the
 | proper positions, the three fields `zSign', `zExp', and `zSig0' are simply
 | added together to form the most significant 32 bits of the result.  This
 | means that any integer portion of `zSig0' will be added into the exponent.
 | Since a properly normalized significand will have an integer portion equal
 | to 1, the `zExp' input should be 1 less than the desired result exponent
 | whenever `zSig0' and `zSig1' concatenated form a complete, normalized
 | significand.
 *----------------------------------------------------------------------------*/

INLINE float128 packFloat128(flag zSign, int32 zExp, uint64_t zSig0, uint64_t zSig1)
{
    float128 z;

    z.low = zSig1;
    z.high = (((uint64_t)zSign) << 63) + (((uint64_t)zExp) << 48) + zSig0;
    return z;
}

/*----------------------------------------------------------------------------
 | Takes an abstract floating-point value having sign `zSign', exponent `zExp',
 | and extended significand formed by the concatenation of `zSig0', `zSig1',
 | and `zSig2', and returns the proper quadruple-precision floating-point value
 | corresponding to the abstract input.  Ordinarily, the abstract value is
 | simply rounded and packed into the quadruple-precision format, with the
 | inexact exception raised if the abstract input cannot be represented
 | exactly.  However, if the abstract value is too large, the overflow and
 | inexact exceptions are raised and an infinity or maximal finite value is
 | returned.  If the abstract value is too small, the input value is rounded to
 | a subnormal number, and the underflow and inexact exceptions are raised if
 | the abstract input cannot be represented exactly as a subnormal quadruple-
 | precision floating-point number.
 |     The input significand must be normalized or smaller.  If the input
 | significand is not normalized, `zExp' must be 0; in that case, the result
 | returned is a subnormal number, and it must not require rounding.  In the
 | usual case that the input significand is normalized, `zExp' must be 1 less
 | than the ``true'' floating-point exponent.  The handling of underflow and
 | overflow follows the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static float128 roundAndPackFloat128(flag zSign, int32 zExp, uint64_t zSig0, uint64_t zSig1, uint64_t zSig2 STATUS_PARAM)
{
    int8 roundingMode;
    flag roundNearestEven, increment, isTiny;

    roundingMode = STATUS(float_rounding_mode);
    roundNearestEven = (roundingMode == float_round_nearest_even);
    increment = ((int64_t)zSig2 < 0);
    if(!roundNearestEven) {
        if(roundingMode == float_round_to_zero) {
            increment = 0;
        } else {
            if(zSign) {
                increment = (roundingMode == float_round_down) && zSig2;
            } else {
                increment = (roundingMode == float_round_up) && zSig2;
            }
        }
    }
    if(0x7FFD <= (uint32_t)zExp) {
        if((0x7FFD < zExp) ||
           ((zExp == 0x7FFD) && eq128(LIT64(0x0001FFFFFFFFFFFF), LIT64(0xFFFFFFFFFFFFFFFF), zSig0, zSig1) && increment)) {
            float_raise(float_flag_overflow | float_flag_inexact STATUS_VAR);
            if((roundingMode == float_round_to_zero) || (zSign && (roundingMode == float_round_up)) ||
               (!zSign && (roundingMode == float_round_down))) {
                return packFloat128(zSign, 0x7FFE, LIT64(0x0000FFFFFFFFFFFF), LIT64(0xFFFFFFFFFFFFFFFF));
            }
            return packFloat128(zSign, 0x7FFF, 0, 0);
        }
        if(zExp < 0) {
            if(STATUS(flush_to_zero)) {
                float_raise((int8)float_flag_output_denormal STATUS_VAR);
                return packFloat128(zSign, 0, 0, 0);
            }
            isTiny = (STATUS(float_detect_tininess) == float_tininess_before_rounding) || (zExp < -1) || !increment ||
                     lt128(zSig0, zSig1, LIT64(0x0001FFFFFFFFFFFF), LIT64(0xFFFFFFFFFFFFFFFF));
            shift128ExtraRightJamming(zSig0, zSig1, zSig2, -zExp, &zSig0, &zSig1, &zSig2);
            zExp = 0;
            if(isTiny && zSig2) {
                float_raise(float_flag_underflow STATUS_VAR);
            }
            if(roundNearestEven) {
                increment = ((int64_t)zSig2 < 0);
            } else {
                if(zSign) {
                    increment = (roundingMode == float_round_down) && zSig2;
                } else {
                    increment = (roundingMode == float_round_up) && zSig2;
                }
            }
        }
    }
    if(zSig2) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    if(increment) {
        add128(zSig0, zSig1, 0, 1, &zSig0, &zSig1);
        zSig1 &= ~((zSig2 + zSig2 == 0) & roundNearestEven);
    } else {
        if((zSig0 | zSig1) == 0) {
            zExp = 0;
        }
    }
    return packFloat128(zSign, zExp, zSig0, zSig1);
}

/*----------------------------------------------------------------------------
 | Takes an abstract floating-point value having sign `zSign', exponent `zExp',
 | and significand formed by the concatenation of `zSig0' and `zSig1', and
 | returns the proper quadruple-precision floating-point value corresponding
 | to the abstract input.  This routine is just like `roundAndPackFloat128'
 | except that the input significand has fewer bits and does not have to be
 | normalized.  In all cases, `zExp' must be 1 less than the ``true'' floating-
 | point exponent.
 *----------------------------------------------------------------------------*/

static float128 normalizeRoundAndPackFloat128(flag zSign, int32 zExp, uint64_t zSig0, uint64_t zSig1 STATUS_PARAM)
{
    int8 shiftCount;
    uint64_t zSig2;

    if(zSig0 == 0) {
        zSig0 = zSig1;
        zSig1 = 0;
        zExp -= 64;
    }
    shiftCount = countLeadingZeros64(zSig0) - 15;
    if(0 <= shiftCount) {
        zSig2 = 0;
        shortShift128Left(zSig0, zSig1, shiftCount, &zSig0, &zSig1);
    } else {
        shift128ExtraRightJamming(zSig0, zSig1, 0, -shiftCount, &zSig0, &zSig1, &zSig2);
    }
    zExp -= shiftCount;
    return roundAndPackFloat128(zSign, zExp, zSig0, zSig1, zSig2 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the 32-bit two's complement integer `a'
 | to the single-precision floating-point format.  The conversion is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 int32_to_float32(int32 a STATUS_PARAM)
{
    flag zSign;

    if(a == 0) {
        return float32_zero;
    }
    if(a == (int32_t)0x80000000) {
        return packFloat32(1, 0x9E, 0);
    }
    zSign = (a < 0);
    return normalizeRoundAndPackFloat32(zSign, 0x9C, zSign ? -a : a STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the 32-bit two's complement integer `a'
 | to the double-precision floating-point format.  The conversion is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 int32_to_float64(int32 a STATUS_PARAM)
{
    flag zSign;
    uint32 absA;
    int8 shiftCount;
    uint64_t zSig;

    if(a == 0) {
        return float64_zero;
    }
    zSign = (a < 0);
    absA = zSign ? -a : a;
    shiftCount = countLeadingZeros32(absA) + 21;
    zSig = absA;
    return packFloat64(zSign, 0x432 - shiftCount, zSig << shiftCount);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the 32-bit two's complement integer `a'
 | to the extended double-precision floating-point format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 int32_to_floatx80(int32 a STATUS_PARAM)
{
    flag zSign;
    uint32 absA;
    int8 shiftCount;
    uint64_t zSig;

    if(a == 0) {
        return packFloatx80(0, 0, 0);
    }
    zSign = (a < 0);
    absA = zSign ? -a : a;
    shiftCount = countLeadingZeros32(absA) + 32;
    zSig = absA;
    return packFloatx80(zSign, 0x403E - shiftCount, zSig << shiftCount);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the 32-bit two's complement integer `a' to
 | the quadruple-precision floating-point format.  The conversion is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 int32_to_float128(int32 a STATUS_PARAM)
{
    flag zSign;
    uint32 absA;
    int8 shiftCount;
    uint64_t zSig0;

    if(a == 0) {
        return packFloat128(0, 0, 0, 0);
    }
    zSign = (a < 0);
    absA = zSign ? -a : a;
    shiftCount = countLeadingZeros32(absA) + 17;
    zSig0 = absA;
    return packFloat128(zSign, 0x402E - shiftCount, zSig0 << shiftCount, 0);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the 64-bit two's complement integer `a'
 | to the single-precision floating-point format.  The conversion is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 int64_to_float32(int64 a STATUS_PARAM)
{
    flag zSign;
    uint64 absA;
    int8 shiftCount;

    if(a == 0) {
        return float32_zero;
    }
    zSign = (a < 0);
    absA = zSign ? -a : a;
    shiftCount = countLeadingZeros64(absA) - 40;
    if(0 <= shiftCount) {
        return packFloat32(zSign, 0x95 - shiftCount, absA << shiftCount);
    } else {
        shiftCount += 7;
        if(shiftCount < 0) {
            shift64RightJamming(absA, -shiftCount, &absA);
        } else {
            absA <<= shiftCount;
        }
        return roundAndPackFloat32(zSign, 0x9C - shiftCount, absA STATUS_VAR);
    }
}

float32 uint64_to_float32(uint64 a STATUS_PARAM)
{
    int8 shiftCount;

    if(a == 0) {
        return float32_zero;
    }
    shiftCount = countLeadingZeros64(a) - 40;
    if(0 <= shiftCount) {
        return packFloat32(0, 0x95 - shiftCount, a << shiftCount);
    } else {
        shiftCount += 7;
        if(shiftCount < 0) {
            shift64RightJamming(a, -shiftCount, &a);
        } else {
            a <<= shiftCount;
        }
        return roundAndPackFloat32(0, 0x9C - shiftCount, a STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the 64-bit two's complement integer `a'
 | to the double-precision floating-point format.  The conversion is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 int64_to_float64(int64 a STATUS_PARAM)
{
    flag zSign;

    if(a == 0) {
        return float64_zero;
    }
    if(!(a & (int64_t)LIT64(0x7FFFFFFFFFFFFFFF))) {
        return packFloat64(1, 0x43E, 0);
    }
    zSign = (a < 0);
    return normalizeRoundAndPackFloat64(zSign, 0x43C, zSign ? -a : a STATUS_VAR);
}

float64 uint64_to_float64(uint64 a STATUS_PARAM)
{
    if(a == 0) {
        return float64_zero;
    }
    if(a & (uint64_t)LIT64(0x8000000000000000)) {
        shift64RightJamming(a, 1, &a);
        return roundAndPackFloat64(0, 0x43D, a STATUS_VAR);
    }
    return normalizeRoundAndPackFloat64(0, 0x43C, a STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the 64-bit two's complement integer `a'
 | to the extended double-precision floating-point format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 int64_to_floatx80(int64 a STATUS_PARAM)
{
    flag zSign;
    uint64 absA;
    int8 shiftCount;

    if(a == 0) {
        return packFloatx80(0, 0, 0);
    }
    zSign = (a < 0);
    absA = zSign ? -a : a;
    shiftCount = countLeadingZeros64(absA);
    return packFloatx80(zSign, 0x403E - shiftCount, absA << shiftCount);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the 64-bit two's complement integer `a' to
 | the quadruple-precision floating-point format.  The conversion is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 int64_to_float128(int64 a STATUS_PARAM)
{
    flag zSign;
    uint64 absA;
    int8 shiftCount;
    int32 zExp;
    uint64_t zSig0, zSig1;

    if(a == 0) {
        return packFloat128(0, 0, 0, 0);
    }
    zSign = (a < 0);
    absA = zSign ? -a : a;
    shiftCount = countLeadingZeros64(absA) + 49;
    zExp = 0x406E - shiftCount;
    if(64 <= shiftCount) {
        zSig1 = 0;
        zSig0 = absA;
        shiftCount -= 64;
    } else {
        zSig1 = absA;
        zSig0 = 0;
    }
    shortShift128Left(zSig0, zSig1, shiftCount, &zSig0, &zSig1);
    return packFloat128(zSign, zExp, zSig0, zSig1);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the single-precision floating-point value
 | `a' to the 32-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic---which means in particular that the conversion is rounded
 | according to the current rounding mode.  If `a' is a NaN, the largest
 | positive integer is returned.  Otherwise, if the conversion overflows, the
 | largest integer with the same sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int32 float32_to_int32(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint32_t aSig;
    uint64_t aSig64;

    a = float32_squash_input_denormal(a STATUS_VAR);
    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    if((aExp == 0xFF) && aSig) {
        aSign = 0;
    }
    if(aExp) {
        aSig |= 0x00800000;
    }
    shiftCount = 0xAF - aExp;
    aSig64 = aSig;
    aSig64 <<= 32;
    if(0 < shiftCount) {
        shift64RightJamming(aSig64, shiftCount, &aSig64);
    }
    return roundAndPackInt32(aSign, aSig64 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the single-precision floating-point value
 | `a' to the 32-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic, except that the conversion is always rounded toward zero.
 | If `a' is a NaN, the largest positive integer is returned.  Otherwise, if
 | the conversion overflows, the largest integer with the same sign as `a' is
 | returned.
 *----------------------------------------------------------------------------*/

int32 float32_to_int32_round_to_zero(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint32_t aSig;
    int32 z;
    a = float32_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    shiftCount = aExp - 0x9E;
    if(0 <= shiftCount) {
        if(float32_val(a) != 0xCF000000) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0xFF) && aSig)) {
                return 0x7FFFFFFF;
            }
        }
        return (int32_t)0x80000000;
    } else if(aExp <= 0x7E) {
        if(aExp | aSig) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
        return 0;
    }
    aSig = (aSig | 0x00800000) << 8;
    z = aSig >> (-shiftCount);
    if((uint32_t)(aSig << (shiftCount & 31))) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    if(aSign) {
        z = -z;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the single-precision floating-point value
 | `a' to the 16-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic, except that the conversion is always rounded toward zero.
 | If `a' is a NaN, the largest positive integer is returned.  Otherwise, if
 | the conversion overflows, the largest integer with the same sign as `a' is
 | returned.
 *----------------------------------------------------------------------------*/

int16 float32_to_int16_round_to_zero(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint32_t aSig;
    int32 z;

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    shiftCount = aExp - 0x8E;
    if(0 <= shiftCount) {
        if(float32_val(a) != 0xC7000000) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0xFF) && aSig)) {
                return 0x7FFF;
            }
        }
        return (int32_t)0xffff8000;
    } else if(aExp <= 0x7E) {
        if(aExp | aSig) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
        return 0;
    }
    shiftCount -= 0x10;
    aSig = (aSig | 0x00800000) << 8;
    z = aSig >> (-shiftCount);
    if((uint32_t)(aSig << (shiftCount & 31))) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    if(aSign) {
        z = -z;
    }
    return z;
}

uint16 float32_to_int16(float32 a STATUS_PARAM)
{
    int64_t v;
    uint16 res;

    v = float32_to_int64(a STATUS_VAR);
    if(v < INT16_MIN) {
        res = INT16_MIN;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > INT16_MAX) {
        res = INT16_MAX;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the single-precision floating-point value
 | `a' to the 64-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic---which means in particular that the conversion is rounded
 | according to the current rounding mode.  If `a' is a NaN, the largest
 | positive integer is returned.  Otherwise, if the conversion overflows, the
 | largest integer with the same sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int64 float32_to_int64(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint32_t aSig;
    uint64_t aSig64, aSigExtra;
    a = float32_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    shiftCount = 0xBE - aExp;
    if(shiftCount < 0) {
        float_raise(float_flag_invalid STATUS_VAR);
        if(!aSign || ((aExp == 0xFF) && aSig)) {
            return LIT64(0x7FFFFFFFFFFFFFFF);
        }
        return (int64_t)LIT64(0x8000000000000000);
    }
    if(aExp) {
        aSig |= 0x00800000;
    }
    aSig64 = aSig;
    aSig64 <<= 40;
    shift64ExtraRightJamming(aSig64, 0, shiftCount, &aSig64, &aSigExtra);
    return roundAndPackInt64(aSign, aSig64, aSigExtra STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the single-precision floating-point value
 | `a' to the 64-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic, except that the conversion is always rounded toward zero.  If
 | `a' is a NaN, the largest positive integer is returned.  Otherwise, if the
 | conversion overflows, the largest integer with the same sign as `a' is
 | returned.
 *----------------------------------------------------------------------------*/

int64 float32_to_int64_round_to_zero(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint32_t aSig;
    uint64_t aSig64;
    int64 z;
    a = float32_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    shiftCount = aExp - 0xBE;
    if(0 <= shiftCount) {
        if(float32_val(a) != 0xDF000000) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0xFF) && aSig)) {
                return LIT64(0x7FFFFFFFFFFFFFFF);
            }
        }
        return (int64_t)LIT64(0x8000000000000000);
    } else if(aExp <= 0x7E) {
        if(aExp | aSig) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
        return 0;
    }
    aSig64 = aSig | 0x00800000;
    aSig64 <<= 40;
    z = aSig64 >> (-shiftCount);
    if((uint64_t)(aSig64 << (shiftCount & 63))) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    if(aSign) {
        z = -z;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the single-precision floating-point value
 | `a' to the double-precision floating-point format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

float64 float32_to_float64(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint32_t aSig;
    a = float32_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    if(aExp == 0xFF) {
        if(aSig) {
            return commonNaNToFloat64(float32ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat64(aSign, 0x7FF, 0);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloat64(aSign, 0, 0);
        }
        normalizeFloat32Subnormal(aSig, &aExp, &aSig);
        --aExp;
    }
    return packFloat64(aSign, aExp + 0x380, ((uint64_t)aSig) << 29);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the single-precision floating-point value
 | `a' to the extended double-precision floating-point format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 float32_to_floatx80(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint32_t aSig;

    a = float32_squash_input_denormal(a STATUS_VAR);
    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    if(aExp == 0xFF) {
        if(aSig) {
            return commonNaNToFloatx80(float32ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloatx80(aSign, 0x7FFF, LIT64(0x8000000000000000));
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloatx80(aSign, 0, 0);
        }
        normalizeFloat32Subnormal(aSig, &aExp, &aSig);
    }
    aSig |= 0x00800000;
    return packFloatx80(aSign, aExp + 0x3F80, ((uint64_t)aSig) << 40);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the single-precision floating-point value
 | `a' to the double-precision floating-point format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

float128 float32_to_float128(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint32_t aSig;

    a = float32_squash_input_denormal(a STATUS_VAR);
    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    if(aExp == 0xFF) {
        if(aSig) {
            return commonNaNToFloat128(float32ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat128(aSign, 0x7FFF, 0, 0);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloat128(aSign, 0, 0, 0);
        }
        normalizeFloat32Subnormal(aSig, &aExp, &aSig);
        --aExp;
    }
    return packFloat128(aSign, aExp + 0x3F80, ((uint64_t)aSig) << 25, 0);
}

/*----------------------------------------------------------------------------
 | Rounds the single-precision floating-point value `a' to an integer, and
 | returns the result as a single-precision floating-point value.  The
 | operation is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 float32_round_to_int(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint32_t lastBitMask, roundBitsMask;
    int8 roundingMode;
    uint32_t z;
    a = float32_squash_input_denormal(a STATUS_VAR);

    aExp = extractFloat32Exp(a);
    if(0x96 <= aExp) {
        if((aExp == 0xFF) && extractFloat32Frac(a)) {
            return propagateFloat32NaN(a, a STATUS_VAR);
        }
        return a;
    }
    if(aExp <= 0x7E) {
        if((uint32_t)(float32_val(a) << 1) == 0) {
            return a;
        }
        STATUS(float_exception_flags) |= float_flag_inexact;
        aSign = extractFloat32Sign(a);
        switch(STATUS(float_rounding_mode)) {
            case float_round_nearest_even:
                if((aExp == 0x7E) && extractFloat32Frac(a)) {
                    return packFloat32(aSign, 0x7F, 0);
                }
                break;
            case float_round_down:
                return make_float32(aSign ? 0xBF800000 : 0);
            case float_round_up:
                return make_float32(aSign ? 0x80000000 : 0x3F800000);
        }
        return packFloat32(aSign, 0, 0);
    }
    lastBitMask = 1;
    lastBitMask <<= 0x96 - aExp;
    roundBitsMask = lastBitMask - 1;
    z = float32_val(a);
    roundingMode = STATUS(float_rounding_mode);
    if(roundingMode == float_round_nearest_even) {
        z += lastBitMask >> 1;
        if((z & roundBitsMask) == 0) {
            z &= ~lastBitMask;
        }
    } else if(roundingMode != float_round_to_zero) {
        if(extractFloat32Sign(make_float32(z)) ^ (roundingMode == float_round_up)) {
            z += roundBitsMask;
        }
    }
    z &= ~roundBitsMask;
    if(z != float32_val(a)) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return make_float32(z);
}

/*----------------------------------------------------------------------------
 | Returns the result of adding the absolute values of the single-precision
 | floating-point values `a' and `b'.  If `zSign' is 1, the sum is negated
 | before being returned.  `zSign' is ignored if the result is a NaN.
 | The addition is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static float32 addFloat32Sigs(float32 a, float32 b, flag zSign STATUS_PARAM)
{
    int16 aExp, bExp, zExp;
    uint32_t aSig, bSig, zSig;
    int16 expDiff;

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    bSig = extractFloat32Frac(b);
    bExp = extractFloat32Exp(b);
    expDiff = aExp - bExp;
    aSig <<= 6;
    bSig <<= 6;
    if(0 < expDiff) {
        if(aExp == 0xFF) {
            if(aSig) {
                return propagateFloat32NaN(a, b STATUS_VAR);
            }
            return a;
        }
        if(bExp == 0) {
            --expDiff;
        } else {
            bSig |= 0x20000000;
        }
        shift32RightJamming(bSig, expDiff, &bSig);
        zExp = aExp;
    } else if(expDiff < 0) {
        if(bExp == 0xFF) {
            if(bSig) {
                return propagateFloat32NaN(a, b STATUS_VAR);
            }
            return packFloat32(zSign, 0xFF, 0);
        }
        if(aExp == 0) {
            ++expDiff;
        } else {
            aSig |= 0x20000000;
        }
        shift32RightJamming(aSig, -expDiff, &aSig);
        zExp = bExp;
    } else {
        if(aExp == 0xFF) {
            if(aSig | bSig) {
                return propagateFloat32NaN(a, b STATUS_VAR);
            }
            return a;
        }
        if(aExp == 0) {
            if(STATUS(flush_to_zero)) {
                if(aSig | bSig) {
                    float_raise((int8)float_flag_output_denormal STATUS_VAR);
                }
                return packFloat32(zSign, 0, 0);
            }
            return packFloat32(zSign, 0, (aSig + bSig) >> 6);
        }
        zSig = 0x40000000 + aSig + bSig;
        zExp = aExp;
        goto roundAndPack;
    }
    aSig |= 0x20000000;
    zSig = (aSig + bSig) << 1;
    --zExp;
    if((int32_t)zSig < 0) {
        zSig = aSig + bSig;
        ++zExp;
    }
roundAndPack:
    return roundAndPackFloat32(zSign, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of subtracting the absolute values of the single-
 | precision floating-point values `a' and `b'.  If `zSign' is 1, the
 | difference is negated before being returned.  `zSign' is ignored if the
 | result is a NaN.  The subtraction is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static float32 subFloat32Sigs(float32 a, float32 b, flag zSign STATUS_PARAM)
{
    int16 aExp, bExp, zExp;
    uint32_t aSig, bSig, zSig;
    int16 expDiff;

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    bSig = extractFloat32Frac(b);
    bExp = extractFloat32Exp(b);
    expDiff = aExp - bExp;
    aSig <<= 7;
    bSig <<= 7;
    if(0 < expDiff) {
        goto aExpBigger;
    }
    if(expDiff < 0) {
        goto bExpBigger;
    }
    if(aExp == 0xFF) {
        if(aSig | bSig) {
            return propagateFloat32NaN(a, b STATUS_VAR);
        }
        float_raise(float_flag_invalid STATUS_VAR);
        return float32_default_nan;
    }
    if(aExp == 0) {
        aExp = 1;
        bExp = 1;
    }
    if(bSig < aSig) {
        goto aBigger;
    }
    if(aSig < bSig) {
        goto bBigger;
    }
    return packFloat32(STATUS(float_rounding_mode) == float_round_down, 0, 0);
bExpBigger:
    if(bExp == 0xFF) {
        if(bSig) {
            return propagateFloat32NaN(a, b STATUS_VAR);
        }
        return packFloat32(zSign ^ 1, 0xFF, 0);
    }
    if(aExp == 0) {
        ++expDiff;
    } else {
        aSig |= 0x40000000;
    }
    shift32RightJamming(aSig, -expDiff, &aSig);
    bSig |= 0x40000000;
bBigger:
    zSig = bSig - aSig;
    zExp = bExp;
    zSign ^= 1;
    goto normalizeRoundAndPack;
aExpBigger:
    if(aExp == 0xFF) {
        if(aSig) {
            return propagateFloat32NaN(a, b STATUS_VAR);
        }
        return a;
    }
    if(bExp == 0) {
        --expDiff;
    } else {
        bSig |= 0x40000000;
    }
    shift32RightJamming(bSig, expDiff, &bSig);
    aSig |= 0x40000000;
aBigger:
    zSig = aSig - bSig;
    zExp = aExp;
normalizeRoundAndPack:
    --zExp;
    return normalizeRoundAndPackFloat32(zSign, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of adding the single-precision floating-point values `a'
 | and `b'.  The operation is performed according to the IEC/IEEE Standard for
 | Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 float32_add(float32 a, float32 b STATUS_PARAM)
{
    flag aSign, bSign;
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    aSign = extractFloat32Sign(a);
    bSign = extractFloat32Sign(b);
    if(aSign == bSign) {
        return addFloat32Sigs(a, b, aSign STATUS_VAR);
    } else {
        return subFloat32Sigs(a, b, aSign STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns the result of subtracting the single-precision floating-point values
 | `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
 | for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 float32_sub(float32 a, float32 b STATUS_PARAM)
{
    flag aSign, bSign;
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    aSign = extractFloat32Sign(a);
    bSign = extractFloat32Sign(b);
    if(aSign == bSign) {
        return subFloat32Sigs(a, b, aSign STATUS_VAR);
    } else {
        return addFloat32Sigs(a, b, aSign STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns the result of multiplying the single-precision floating-point values
 | `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
 | for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 float32_mul(float32 a, float32 b STATUS_PARAM)
{
    flag aSign, bSign, zSign;
    int16 aExp, bExp, zExp;
    uint32_t aSig, bSig;
    uint64_t zSig64;
    uint32_t zSig;

    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    bSig = extractFloat32Frac(b);
    bExp = extractFloat32Exp(b);
    bSign = extractFloat32Sign(b);
    zSign = aSign ^ bSign;
    if(aExp == 0xFF) {
        if(aSig || ((bExp == 0xFF) && bSig)) {
            return propagateFloat32NaN(a, b STATUS_VAR);
        }
        if((bExp | bSig) == 0) {
            float_raise(float_flag_invalid STATUS_VAR);
            return float32_default_nan;
        }
        return packFloat32(zSign, 0xFF, 0);
    }
    if(bExp == 0xFF) {
        if(bSig) {
            return propagateFloat32NaN(a, b STATUS_VAR);
        }
        if((aExp | aSig) == 0) {
            float_raise(float_flag_invalid STATUS_VAR);
            return float32_default_nan;
        }
        return packFloat32(zSign, 0xFF, 0);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloat32(zSign, 0, 0);
        }
        normalizeFloat32Subnormal(aSig, &aExp, &aSig);
    }
    if(bExp == 0) {
        if(bSig == 0) {
            return packFloat32(zSign, 0, 0);
        }
        normalizeFloat32Subnormal(bSig, &bExp, &bSig);
    }
    zExp = aExp + bExp - 0x7F;
    aSig = (aSig | 0x00800000) << 7;
    bSig = (bSig | 0x00800000) << 8;
    shift64RightJamming(((uint64_t)aSig) * bSig, 32, &zSig64);
    zSig = zSig64;
    if(0 <= (int32_t)(zSig << 1)) {
        zSig <<= 1;
        --zExp;
    }
    return roundAndPackFloat32(zSign, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of dividing the single-precision floating-point value `a'
 | by the corresponding value `b'.  The operation is performed according to the
 | IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 float32_div(float32 a, float32 b STATUS_PARAM)
{
    flag aSign, bSign, zSign;
    int16 aExp, bExp, zExp;
    uint32_t aSig, bSig, zSig;
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    bSig = extractFloat32Frac(b);
    bExp = extractFloat32Exp(b);
    bSign = extractFloat32Sign(b);
    zSign = aSign ^ bSign;
    if(aExp == 0xFF) {
        if(aSig) {
            return propagateFloat32NaN(a, b STATUS_VAR);
        }
        if(bExp == 0xFF) {
            if(bSig) {
                return propagateFloat32NaN(a, b STATUS_VAR);
            }
            float_raise(float_flag_invalid STATUS_VAR);
            return float32_default_nan;
        }
        return packFloat32(zSign, 0xFF, 0);
    }
    if(bExp == 0xFF) {
        if(bSig) {
            return propagateFloat32NaN(a, b STATUS_VAR);
        }
        return packFloat32(zSign, 0, 0);
    }
    if(bExp == 0) {
        if(bSig == 0) {
            if((aExp | aSig) == 0) {
                float_raise(float_flag_invalid STATUS_VAR);
                return float32_default_nan;
            }
            float_raise(float_flag_divbyzero STATUS_VAR);
            return packFloat32(zSign, 0xFF, 0);
        }
        normalizeFloat32Subnormal(bSig, &bExp, &bSig);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloat32(zSign, 0, 0);
        }
        normalizeFloat32Subnormal(aSig, &aExp, &aSig);
    }
    zExp = aExp - bExp + 0x7D;
    aSig = (aSig | 0x00800000) << 7;
    bSig = (bSig | 0x00800000) << 8;
    if(bSig <= (aSig + aSig)) {
        aSig >>= 1;
        ++zExp;
    }
    zSig = (((uint64_t)aSig) << 32) / bSig;
    if((zSig & 0x3F) == 0) {
        zSig |= ((uint64_t)bSig * zSig != ((uint64_t)aSig) << 32);
    }
    return roundAndPackFloat32(zSign, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the remainder of the single-precision floating-point value `a'
 | with respect to the corresponding value `b'.  The operation is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 float32_rem(float32 a, float32 b STATUS_PARAM)
{
    flag aSign, zSign;
    int16 aExp, bExp, expDiff;
    uint32_t aSig, bSig;
    uint32_t q;
    uint64_t aSig64, bSig64, q64;
    uint32_t alternateASig;
    int32_t sigMean;
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    bSig = extractFloat32Frac(b);
    bExp = extractFloat32Exp(b);
    if(aExp == 0xFF) {
        if(aSig || ((bExp == 0xFF) && bSig)) {
            return propagateFloat32NaN(a, b STATUS_VAR);
        }
        float_raise(float_flag_invalid STATUS_VAR);
        return float32_default_nan;
    }
    if(bExp == 0xFF) {
        if(bSig) {
            return propagateFloat32NaN(a, b STATUS_VAR);
        }
        return a;
    }
    if(bExp == 0) {
        if(bSig == 0) {
            float_raise(float_flag_invalid STATUS_VAR);
            return float32_default_nan;
        }
        normalizeFloat32Subnormal(bSig, &bExp, &bSig);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return a;
        }
        normalizeFloat32Subnormal(aSig, &aExp, &aSig);
    }
    expDiff = aExp - bExp;
    aSig |= 0x00800000;
    bSig |= 0x00800000;
    if(expDiff < 32) {
        aSig <<= 8;
        bSig <<= 8;
        if(expDiff < 0) {
            if(expDiff < -1) {
                return a;
            }
            aSig >>= 1;
        }
        q = (bSig <= aSig);
        if(q) {
            aSig -= bSig;
        }
        if(0 < expDiff) {
            q = (((uint64_t)aSig) << 32) / bSig;
            q >>= 32 - expDiff;
            bSig >>= 2;
            aSig = ((aSig >> 1) << (expDiff - 1)) - bSig * q;
        } else {
            aSig >>= 2;
            bSig >>= 2;
        }
    } else {
        if(bSig <= aSig) {
            aSig -= bSig;
        }
        aSig64 = ((uint64_t)aSig) << 40;
        bSig64 = ((uint64_t)bSig) << 40;
        expDiff -= 64;
        while(0 < expDiff) {
            q64 = estimateDiv128To64(aSig64, 0, bSig64);
            q64 = (2 < q64) ? q64 - 2 : 0;
            aSig64 = -((bSig * q64) << 38);
            expDiff -= 62;
        }
        expDiff += 64;
        q64 = estimateDiv128To64(aSig64, 0, bSig64);
        q64 = (2 < q64) ? q64 - 2 : 0;
        q = q64 >> (64 - expDiff);
        bSig <<= 6;
        aSig = ((aSig64 >> 33) << (expDiff - 1)) - bSig * q;
    }
    do {
        alternateASig = aSig;
        ++q;
        aSig -= bSig;
    } while(0 <= (int32_t)aSig);
    sigMean = aSig + alternateASig;
    if((sigMean < 0) || ((sigMean == 0) && (q & 1))) {
        aSig = alternateASig;
    }
    zSign = ((int32_t)aSig < 0);
    if(zSign) {
        aSig = -aSig;
    }
    return normalizeRoundAndPackFloat32(aSign ^ zSign, bExp, aSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of multiplying the single-precision floating-point values
 | `a' and `b' then adding 'c', with no intermediate rounding step after the
 | multiplication.  The operation is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic 754-2008.
 | The flags argument allows the caller to select negation of the
 | addend, the intermediate product, or the final result. (The difference
 | between this and having the caller do a separate negation is that negating
 | externally will flip the sign bit on NaNs.)
 *----------------------------------------------------------------------------*/

float32 float32_muladd(float32 a, float32 b, float32 c, int flags STATUS_PARAM)
{
    flag aSign, bSign, cSign, zSign;
    int aExp, bExp, cExp, pExp, zExp, expDiff;
    uint32_t aSig, bSig, cSig;
    flag pInf, pZero, pSign;
    uint64_t pSig64, cSig64, zSig64;
    int shiftcount;
    flag signflip, infzero;

    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);
    c = float32_squash_input_denormal(c STATUS_VAR);
    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    bSig = extractFloat32Frac(b);
    bExp = extractFloat32Exp(b);
    bSign = extractFloat32Sign(b);
    cSig = extractFloat32Frac(c);
    cExp = extractFloat32Exp(c);
    cSign = extractFloat32Sign(c);

    infzero = ((aExp == 0 && aSig == 0 && bExp == 0xff && bSig == 0) || (aExp == 0xff && aSig == 0 && bExp == 0 && bSig == 0));

    /* It is implementation-defined whether the cases of (0,inf,qnan)
     * and (inf,0,qnan) raise InvalidOperation or not (and what QNaN
     * they return if they do), so we have to hand this information
     * off to the target-specific pick-a-NaN routine.
     */
    if(((aExp == 0xff) && aSig) || ((bExp == 0xff) && bSig) || ((cExp == 0xff) && cSig)) {
        return propagateFloat32MulAddNaN(a, b, c, infzero STATUS_VAR);
    }

    if(infzero) {
        float_raise(float_flag_invalid STATUS_VAR);
        return float32_default_nan;
    }

    if(flags & float_muladd_negate_c) {
        cSign ^= 1;
    }

    signflip = (flags & float_muladd_negate_result) ? 1 : 0;

    /* Work out the sign and type of the product */
    pSign = aSign ^ bSign;
    if(flags & float_muladd_negate_product) {
        pSign ^= 1;
    }
    pInf = (aExp == 0xff) || (bExp == 0xff);
    pZero = ((aExp | aSig) == 0) || ((bExp | bSig) == 0);

    if(cExp == 0xff) {
        if(pInf && (pSign ^ cSign)) {
            /* addition of opposite-signed infinities => InvalidOperation */
            float_raise(float_flag_invalid STATUS_VAR);
            return float32_default_nan;
        }
        /* Otherwise generate an infinity of the same sign */
        return packFloat32(cSign ^ signflip, 0xff, 0);
    }

    if(pInf) {
        return packFloat32(pSign ^ signflip, 0xff, 0);
    }

    if(pZero) {
        if(cExp == 0) {
            if(cSig == 0) {
                /* Adding two exact zeroes */
                if(pSign == cSign) {
                    zSign = pSign;
                } else if(STATUS(float_rounding_mode) == float_round_down) {
                    zSign = 1;
                } else {
                    zSign = 0;
                }
                return packFloat32(zSign ^ signflip, 0, 0);
            }
            /* Exact zero plus a denorm */
            if(STATUS(flush_to_zero)) {
                float_raise((int8)float_flag_output_denormal STATUS_VAR);
                return packFloat32(cSign ^ signflip, 0, 0);
            }
        }
        /* Zero plus something non-zero : just return the something */
        return c ^ (signflip << 31);
    }

    if(aExp == 0) {
        normalizeFloat32Subnormal(aSig, &aExp, &aSig);
    }
    if(bExp == 0) {
        normalizeFloat32Subnormal(bSig, &bExp, &bSig);
    }

    /* Calculate the actual result a * b + c */

    /* Multiply first; this is easy. */
    /* NB: we subtract 0x7e where float32_mul() subtracts 0x7f
     * because we want the true exponent, not the "one-less-than"
     * flavour that roundAndPackFloat32() takes.
     */
    pExp = aExp + bExp - 0x7e;
    aSig = (aSig | 0x00800000) << 7;
    bSig = (bSig | 0x00800000) << 8;
    pSig64 = (uint64_t)aSig * bSig;
    if((int64_t)(pSig64 << 1) >= 0) {
        pSig64 <<= 1;
        pExp--;
    }

    zSign = pSign ^ signflip;

    /* Now pSig64 is the significand of the multiply, with the explicit bit in
     * position 62.
     */
    if(cExp == 0) {
        if(!cSig) {
            /* Throw out the special case of c being an exact zero now */
            shift64RightJamming(pSig64, 32, &pSig64);

            zExp = pExp - 1;
            zSig64 = pSig64;
            goto halve_round_and_pack_float32;
        }
        normalizeFloat32Subnormal(cSig, &cExp, &cSig);
    }

    cSig64 = (uint64_t)cSig << (62 - 23);
    cSig64 |= LIT64(0x4000000000000000);
    expDiff = pExp - cExp;

    if(pSign == cSign) {
        /* Addition */
        if(expDiff > 0) {
            /* scale c to match p */
            shift64RightJamming(cSig64, expDiff, &cSig64);
            zExp = pExp;
        } else if(expDiff < 0) {
            /* scale p to match c */
            shift64RightJamming(pSig64, -expDiff, &pSig64);
            zExp = cExp;
        } else {
            /* no scaling needed */
            zExp = cExp;
        }
        /* Add significands and make sure explicit bit ends up in posn 62 */
        zSig64 = pSig64 + cSig64;
        if((int64_t)zSig64 < 0) {
            shift64RightJamming(zSig64, 1, &zSig64);
        } else {
            zExp--;
        }
    } else {
        /* Subtraction */
        if(expDiff > 0) {
            shift64RightJamming(cSig64, expDiff, &cSig64);
            zSig64 = pSig64 - cSig64;
            zExp = pExp;
        } else if(expDiff < 0) {
            shift64RightJamming(pSig64, -expDiff, &pSig64);
            zSig64 = cSig64 - pSig64;
            zExp = cExp;
            zSign ^= 1;
        } else {
            zExp = pExp;
            if(cSig64 < pSig64) {
                zSig64 = pSig64 - cSig64;
            } else if(pSig64 < cSig64) {
                zSig64 = cSig64 - pSig64;
                zSign ^= 1;
            } else {
                /* Exact zero */
                zSign = signflip;
                if(STATUS(float_rounding_mode) == float_round_down) {
                    zSign ^= 1;
                }
                return packFloat32(zSign, 0, 0);
            }
        }
        --zExp;
        /* Normalize to put the explicit bit back into bit 62. */
        shiftcount = countLeadingZeros64(zSig64) - 1;
        zSig64 <<= shiftcount;
        zExp -= shiftcount;
    }
    shift64RightJamming(zSig64, 32, &zSig64);

halve_round_and_pack_float32:
    if((flags & float_muladd_halve_result) && (zExp >= 1)) {
        zExp -= 1;
    }
    return roundAndPackFloat32(zSign, zExp, zSig64 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the square root of the single-precision floating-point value `a'.
 | The operation is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 float32_sqrt(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, zExp;
    uint32_t aSig, zSig;
    uint64_t rem, term;
    a = float32_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    if(aExp == 0xFF) {
        if(aSig) {
            return propagateFloat32NaN(a, float32_zero STATUS_VAR);
        }
        if(!aSign) {
            return a;
        }
        float_raise(float_flag_invalid STATUS_VAR);
        return float32_default_nan;
    }
    if(aSign) {
        if((aExp | aSig) == 0) {
            return a;
        }
        float_raise(float_flag_invalid STATUS_VAR);
        return float32_default_nan;
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return float32_zero;
        }
        normalizeFloat32Subnormal(aSig, &aExp, &aSig);
    }
    zExp = ((aExp - 0x7F) >> 1) + 0x7E;
    aSig = (aSig | 0x00800000) << 8;
    zSig = estimateSqrt32(aExp, aSig) + 2;
    if((zSig & 0x7F) <= 5) {
        if(zSig < 2) {
            zSig = 0x7FFFFFFF;
            goto roundAndPack;
        }
        aSig >>= aExp & 1;
        term = ((uint64_t)zSig) * zSig;
        rem = (((uint64_t)aSig) << 32) - term;
        while((int64_t)rem < 0) {
            --zSig;
            rem += (((uint64_t)zSig) << 1) | 1;
        }
        zSig |= (rem != 0);
    }
    shift32RightJamming(zSig, 1, &zSig);
roundAndPack:
    return roundAndPackFloat32(0, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the binary exponential of the single-precision floating-point value
 | `a'. The operation is performed according to the IEC/IEEE Standard for
 | Binary Floating-Point Arithmetic.
 |
 | Uses the following identities:
 |
 | 1. -------------------------------------------------------------------------
 |      x    x*ln(2)
 |     2  = e
 |
 | 2. -------------------------------------------------------------------------
 |                      2     3     4     5           n
 |      x        x     x     x     x     x           x
 |     e  = 1 + --- + --- + --- + --- + --- + ... + --- + ...
 |               1!    2!    3!    4!    5!          n!
 *----------------------------------------------------------------------------*/

static const float64 float32_exp2_coefficients[15] = {
    const_float64(0x3ff0000000000000ll), /*  1 */
    const_float64(0x3fe0000000000000ll), /*  2 */
    const_float64(0x3fc5555555555555ll), /*  3 */
    const_float64(0x3fa5555555555555ll), /*  4 */
    const_float64(0x3f81111111111111ll), /*  5 */
    const_float64(0x3f56c16c16c16c17ll), /*  6 */
    const_float64(0x3f2a01a01a01a01all), /*  7 */
    const_float64(0x3efa01a01a01a01all), /*  8 */
    const_float64(0x3ec71de3a556c734ll), /*  9 */
    const_float64(0x3e927e4fb7789f5cll), /* 10 */
    const_float64(0x3e5ae64567f544e4ll), /* 11 */
    const_float64(0x3e21eed8eff8d898ll), /* 12 */
    const_float64(0x3de6124613a86d09ll), /* 13 */
    const_float64(0x3da93974a8c07c9dll), /* 14 */
    const_float64(0x3d6ae7f3e733b81fll), /* 15 */
};

float32 float32_exp2(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint32_t aSig;
    float64 r, x, xn;
    int i;
    a = float32_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);

    if(aExp == 0xFF) {
        if(aSig) {
            return propagateFloat32NaN(a, float32_zero STATUS_VAR);
        }
        return (aSign) ? float32_zero : a;
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return float32_one;
        }
    }

    float_raise(float_flag_inexact STATUS_VAR);

    /* ******************************* */
    /* using float64 for approximation */
    /* ******************************* */
    x = float32_to_float64(a STATUS_VAR);
    x = float64_mul(x, float64_ln2 STATUS_VAR);

    xn = x;
    r = float64_one;
    for(i = 0; i < 15; i++) {
        float64 f;

        f = float64_mul(xn, float32_exp2_coefficients[i] STATUS_VAR);
        r = float64_add(r, f STATUS_VAR);

        xn = float64_mul(xn, x STATUS_VAR);
    }

    return float64_to_float32(r, status);
}

/*----------------------------------------------------------------------------
 | Returns the binary log of the single-precision floating-point value `a'.
 | The operation is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/
float32 float32_log2(float32 a STATUS_PARAM)
{
    flag aSign, zSign;
    int16 aExp;
    uint32_t aSig, zSig, i;

    a = float32_squash_input_denormal(a STATUS_VAR);
    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);

    if(aExp == 0) {
        if(aSig == 0) {
            return packFloat32(1, 0xFF, 0);
        }
        normalizeFloat32Subnormal(aSig, &aExp, &aSig);
    }
    if(aSign) {
        float_raise(float_flag_invalid STATUS_VAR);
        return float32_default_nan;
    }
    if(aExp == 0xFF) {
        if(aSig) {
            return propagateFloat32NaN(a, float32_zero STATUS_VAR);
        }
        return a;
    }

    aExp -= 0x7F;
    aSig |= 0x00800000;
    zSign = aExp < 0;
    zSig = aExp << 23;

    for(i = 1 << 22; i > 0; i >>= 1) {
        aSig = ((uint64_t)aSig * aSig) >> 23;
        if(aSig & 0x01000000) {
            aSig >>= 1;
            zSig |= i;
        }
    }

    if(zSign) {
        zSig = -zSig;
    }

    return normalizeRoundAndPackFloat32(zSign, 0x85, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point value `a' is equal to
 | the corresponding value `b', and 0 otherwise.  The invalid exception is
 | raised if either operand is a NaN.  Otherwise, the comparison is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float32_eq(float32 a, float32 b STATUS_PARAM)
{
    uint32_t av, bv;
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat32Exp(a) == 0xFF) && extractFloat32Frac(a)) || ((extractFloat32Exp(b) == 0xFF) && extractFloat32Frac(b))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    av = float32_val(a);
    bv = float32_val(b);
    return (av == bv) || ((uint32_t)((av | bv) << 1) == 0);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point value `a' is less than
 | or equal to the corresponding value `b', and 0 otherwise.  The invalid
 | exception is raised if either operand is a NaN.  The comparison is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float32_le(float32 a, float32 b STATUS_PARAM)
{
    flag aSign, bSign;
    uint32_t av, bv;
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat32Exp(a) == 0xFF) && extractFloat32Frac(a)) || ((extractFloat32Exp(b) == 0xFF) && extractFloat32Frac(b))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    aSign = extractFloat32Sign(a);
    bSign = extractFloat32Sign(b);
    av = float32_val(a);
    bv = float32_val(b);
    if(aSign != bSign) {
        return aSign || ((uint32_t)((av | bv) << 1) == 0);
    }
    return (av == bv) || (aSign ^ (av < bv));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point value `a' is less than
 | the corresponding value `b', and 0 otherwise.  The invalid exception is
 | raised if either operand is a NaN.  The comparison is performed according
 | to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float32_lt(float32 a, float32 b STATUS_PARAM)
{
    flag aSign, bSign;
    uint32_t av, bv;
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat32Exp(a) == 0xFF) && extractFloat32Frac(a)) || ((extractFloat32Exp(b) == 0xFF) && extractFloat32Frac(b))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    aSign = extractFloat32Sign(a);
    bSign = extractFloat32Sign(b);
    av = float32_val(a);
    bv = float32_val(b);
    if(aSign != bSign) {
        return aSign && ((uint32_t)((av | bv) << 1) != 0);
    }
    return (av != bv) && (aSign ^ (av < bv));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point values `a' and `b' cannot
 | be compared, and 0 otherwise.  The invalid exception is raised if either
 | operand is a NaN.  The comparison is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float32_unordered(float32 a, float32 b STATUS_PARAM)
{
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat32Exp(a) == 0xFF) && extractFloat32Frac(a)) || ((extractFloat32Exp(b) == 0xFF) && extractFloat32Frac(b))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 1;
    }
    return 0;
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point value `a' is equal to
 | the corresponding value `b', and 0 otherwise.  Quiet NaNs do not cause an
 | exception.  The comparison is performed according to the IEC/IEEE Standard
 | for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float32_eq_quiet(float32 a, float32 b STATUS_PARAM)
{
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat32Exp(a) == 0xFF) && extractFloat32Frac(a)) || ((extractFloat32Exp(b) == 0xFF) && extractFloat32Frac(b))) {
        if(float32_is_signaling_nan(a STATUS_VAR) || float32_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    return (float32_val(a) == float32_val(b)) || ((uint32_t)((float32_val(a) | float32_val(b)) << 1) == 0);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point value `a' is less than or
 | equal to the corresponding value `b', and 0 otherwise.  Quiet NaNs do not
 | cause an exception.  Otherwise, the comparison is performed according to the
 | IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float32_le_quiet(float32 a, float32 b STATUS_PARAM)
{
    flag aSign, bSign;
    uint32_t av, bv;
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat32Exp(a) == 0xFF) && extractFloat32Frac(a)) || ((extractFloat32Exp(b) == 0xFF) && extractFloat32Frac(b))) {
        if(float32_is_signaling_nan(a STATUS_VAR) || float32_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    aSign = extractFloat32Sign(a);
    bSign = extractFloat32Sign(b);
    av = float32_val(a);
    bv = float32_val(b);
    if(aSign != bSign) {
        return aSign || ((uint32_t)((av | bv) << 1) == 0);
    }
    return (av == bv) || (aSign ^ (av < bv));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point value `a' is less than
 | the corresponding value `b', and 0 otherwise.  Quiet NaNs do not cause an
 | exception.  Otherwise, the comparison is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float32_lt_quiet(float32 a, float32 b STATUS_PARAM)
{
    flag aSign, bSign;
    uint32_t av, bv;
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat32Exp(a) == 0xFF) && extractFloat32Frac(a)) || ((extractFloat32Exp(b) == 0xFF) && extractFloat32Frac(b))) {
        if(float32_is_signaling_nan(a STATUS_VAR) || float32_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    aSign = extractFloat32Sign(a);
    bSign = extractFloat32Sign(b);
    av = float32_val(a);
    bv = float32_val(b);
    if(aSign != bSign) {
        return aSign && ((uint32_t)((av | bv) << 1) != 0);
    }
    return (av != bv) && (aSign ^ (av < bv));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the single-precision floating-point values `a' and `b' cannot
 | be compared, and 0 otherwise.  Quiet NaNs do not cause an exception.  The
 | comparison is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float32_unordered_quiet(float32 a, float32 b STATUS_PARAM)
{
    a = float32_squash_input_denormal(a STATUS_VAR);
    b = float32_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat32Exp(a) == 0xFF) && extractFloat32Frac(a)) || ((extractFloat32Exp(b) == 0xFF) && extractFloat32Frac(b))) {
        if(float32_is_signaling_nan(a STATUS_VAR) || float32_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 1;
    }
    return 0;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the double-precision floating-point value
 | `a' to the 32-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic---which means in particular that the conversion is rounded
 | according to the current rounding mode.  If `a' is a NaN, the largest
 | positive integer is returned.  Otherwise, if the conversion overflows, the
 | largest integer with the same sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int32 float64_to_int32(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint64_t aSig;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if((aExp == 0x7FF) && aSig) {
        aSign = 0;
    }
    if(aExp) {
        aSig |= LIT64(0x0010000000000000);
    }
    shiftCount = 0x42C - aExp;
    if(0 < shiftCount) {
        shift64RightJamming(aSig, shiftCount, &aSig);
    }
    return roundAndPackInt32(aSign, aSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the double-precision floating-point value
 | `a' to the 32-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic, except that the conversion is always rounded toward zero.
 | If `a' is a NaN, the largest positive integer is returned.  Otherwise, if
 | the conversion overflows, the largest integer with the same sign as `a' is
 | returned.
 *----------------------------------------------------------------------------*/

int32 float64_to_int32_round_to_zero(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint64_t aSig, savedASig;
    int32 z;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(0x41E < aExp) {
        if((aExp == 0x7FF) && aSig) {
            aSign = 0;
        }
        goto invalid;
    } else if(aExp < 0x3FF) {
        if(aExp || aSig) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
        return 0;
    }
    aSig |= LIT64(0x0010000000000000);
    shiftCount = 0x433 - aExp;
    savedASig = aSig;
    aSig >>= shiftCount;
    z = aSig;
    if(aSign) {
        z = -z;
    }
    if((z < 0) ^ aSign) {
    invalid:
        float_raise(float_flag_invalid STATUS_VAR);
        return aSign ? (int32_t)0x80000000 : 0x7FFFFFFF;
    }
    if((aSig << shiftCount) != savedASig) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the double-precision floating-point value
 | `a' to the 16-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic, except that the conversion is always rounded toward zero.
 | If `a' is a NaN, the largest positive integer is returned.  Otherwise, if
 | the conversion overflows, the largest integer with the same sign as `a' is
 | returned.
 *----------------------------------------------------------------------------*/

int16 float64_to_int16_round_to_zero(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint64_t aSig, savedASig;
    int32 z;

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(0x40E < aExp) {
        if((aExp == 0x7FF) && aSig) {
            aSign = 0;
        }
        goto invalid;
    } else if(aExp < 0x3FF) {
        if(aExp || aSig) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
        return 0;
    }
    aSig |= LIT64(0x0010000000000000);
    shiftCount = 0x433 - aExp;
    savedASig = aSig;
    aSig >>= shiftCount;
    z = aSig;
    if(aSign) {
        z = -z;
    }
    if(((int16_t)z < 0) ^ aSign) {
    invalid:
        float_raise(float_flag_invalid STATUS_VAR);
        return aSign ? (int32_t)0xffff8000 : 0x7FFF;
    }
    if((aSig << shiftCount) != savedASig) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the double-precision floating-point value
 | `a' to the 64-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic---which means in particular that the conversion is rounded
 | according to the current rounding mode.  If `a' is a NaN, the largest
 | positive integer is returned.  Otherwise, if the conversion overflows, the
 | largest integer with the same sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int64 float64_to_int64(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint64_t aSig, aSigExtra;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp) {
        aSig |= LIT64(0x0010000000000000);
    }
    shiftCount = 0x433 - aExp;
    if(shiftCount <= 0) {
        if(shiftCount < -11) {
            goto invalid;
        }
        if(0x43E < aExp) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0x7FF) && (aSig != LIT64(0x0010000000000000)))) {
                return LIT64(0x7FFFFFFFFFFFFFFF);
            }
            return (int64_t)LIT64(0x8000000000000000);
        }
        aSigExtra = 0;
        aSig <<= -shiftCount;
    } else {
        shift64ExtraRightJamming(aSig, 0, shiftCount, &aSig, &aSigExtra);
    }
    return roundAndPackInt64(aSign, aSig, aSigExtra STATUS_VAR);
invalid:
    return (aExp == 0x7FF) && extractFloat64Frac(a) ? LIT64(0x7FFFFFFFFFFFFFFF)
           : aSign                                  ? (-LIT64(0x7FFFFFFFFFFFFFFF) - 1)
                                                    : LIT64(0x7FFFFFFFFFFFFFFF);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the double-precision floating-point value
 | `a' to the 64-bit two's complement integer format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic, except that the conversion is always rounded toward zero.
 | If `a' is a NaN, the largest positive integer is returned.  Otherwise, if
 | the conversion overflows, the largest integer with the same sign as `a' is
 | returned.
 *----------------------------------------------------------------------------*/

int64 float64_to_int64_round_to_zero(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint64_t aSig;
    int64 z;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp) {
        aSig |= LIT64(0x0010000000000000);
    }
    shiftCount = aExp - 0x433;
    if(0 <= shiftCount) {
        if(0x43E <= aExp) {
            if(float64_val(a) != LIT64(0xC3E0000000000000)) {
                float_raise(float_flag_invalid STATUS_VAR);
                if(!aSign || ((aExp == 0x7FF) && (aSig != LIT64(0x0010000000000000)))) {
                    return LIT64(0x7FFFFFFFFFFFFFFF);
                }
            }
            return (int64_t)LIT64(0x8000000000000000);
        }
        z = aSig << shiftCount;
    } else {
        if(aExp < 0x3FE) {
            if(aExp | aSig) {
                STATUS(float_exception_flags) |= float_flag_inexact;
            }
            return 0;
        }
        z = aSig >> (-shiftCount);
        if((uint64_t)(aSig << (shiftCount & 63))) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
    }
    if(aSign) {
        z = -z;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the double-precision floating-point value
 | `a' to the single-precision floating-point format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

float32 float64_to_float32(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint64_t aSig;
    uint32_t zSig;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp == 0x7FF) {
        if(aSig) {
            return commonNaNToFloat32(float64ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat32(aSign, 0xFF, 0);
    }
    shift64RightJamming(aSig, 22, &aSig);
    zSig = aSig;
    if(aExp || zSig) {
        zSig |= 0x40000000;
        aExp -= 0x381;
    }
    return roundAndPackFloat32(aSign, aExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Packs the sign `zSign', exponent `zExp', and significand `zSig' into a
 | half-precision floating-point value, returning the result.  After being
 | shifted into the proper positions, the three fields are simply added
 | together to form the result.  This means that any integer portion of `zSig'
 | will be added into the exponent.  Since a properly normalized significand
 | will have an integer portion equal to 1, the `zExp' input should be 1 less
 | than the desired result exponent whenever `zSig' is a complete, normalized
 | significand.
 *----------------------------------------------------------------------------*/
static float16 packFloat16(flag zSign, int16 zExp, uint16_t zSig)
{
    return make_float16((((uint32_t)zSign) << 15) + (((uint32_t)zExp & 0x1F) << 10) + zSig);
}

/* Half precision floats come in two formats: standard IEEE and "ARM" format.
   The latter gains extra exponent range by omitting the NaN/Inf encodings.  */

float32 float16_to_float32(float16 a, flag ieee STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint32_t aSig;

    aSign = extractFloat16Sign(a);
    aExp = extractFloat16Exp(a);
    aSig = extractFloat16Frac(a);

    if(aExp == 0x1f && ieee) {
        if(aSig) {
            return commonNaNToFloat32(float16ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat32(aSign, 0xff, aSig << 13);
    }
    if(aExp == 0) {
        int8 shiftCount;

        if(aSig == 0) {
            return packFloat32(aSign, 0, 0);
        }

        shiftCount = countLeadingZeros32(aSig) - 21;
        aSig = aSig << shiftCount;
        aExp = -shiftCount;
    }
    return packFloat32(aSign, aExp + 0x70, aSig << 13);
}

float16 float32_to_float16(float32 a, flag ieee STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint32_t aSig;
    uint32_t mask;
    uint32_t increment;
    int8 roundingMode;
    a = float32_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    if(aExp == 0xFF) {
        if(aSig) {
            /* Input is a NaN */
            float16 r = commonNaNToFloat16(float32ToCommonNaN(a STATUS_VAR) STATUS_VAR);
            if(!ieee) {
                return packFloat16(aSign, 0, 0);
            }
            return r;
        }
        /* Infinity */
        if(!ieee) {
            float_raise(float_flag_invalid STATUS_VAR);
            return packFloat16(aSign, 0x1f, 0x3ff);
        }
        return packFloat16(aSign, 0x1f, 0);
    }
    if(aExp == 0 && aSig == 0) {
        return packFloat16(aSign, 0, 0);
    }
    /* Decimal point between bits 22 and 23.  */
    aSig |= 0x00800000;
    aExp -= 0x7f;
    if(aExp < -14) {
        mask = 0x00ffffff;
        if(aExp >= -24) {
            mask >>= 25 + aExp;
        }
    } else {
        mask = 0x00001fff;
    }
    if(aSig & mask) {
        float_raise(float_flag_underflow STATUS_VAR);
        roundingMode = STATUS(float_rounding_mode);
        switch(roundingMode) {
            case float_round_nearest_even:
                increment = (mask + 1) >> 1;
                if((aSig & mask) == increment) {
                    increment = aSig & (increment << 1);
                }
                break;
            case float_round_up:
                increment = aSign ? 0 : mask;
                break;
            case float_round_down:
                increment = aSign ? mask : 0;
                break;
            default: /* round_to_zero */
                increment = 0;
                break;
        }
        aSig += increment;
        if(aSig >= 0x01000000) {
            aSig >>= 1;
            aExp++;
        }
    } else if(aExp < -14 && STATUS(float_detect_tininess) == float_tininess_before_rounding) {
        float_raise(float_flag_underflow STATUS_VAR);
    }

    if(ieee) {
        if(aExp > 15) {
            float_raise(float_flag_overflow | float_flag_inexact STATUS_VAR);
            return packFloat16(aSign, 0x1f, 0);
        }
    } else {
        if(aExp > 16) {
            float_raise(float_flag_invalid | float_flag_inexact STATUS_VAR);
            return packFloat16(aSign, 0x1f, 0x3ff);
        }
    }
    if(aExp < -24) {
        return packFloat16(aSign, 0, 0);
    }
    if(aExp < -14) {
        aSig >>= -14 - aExp;
        aExp = -14;
    }
    return packFloat16(aSign, aExp + 14, aSig >> 13);
}

float16 float64_to_float16(float64 a, flag ieee STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint64_t aFrac;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aFrac = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp == 0x7FF) {
        if(aFrac) {
            return commonNaNToFloat16(float64ToCommonNaN(a STATUS_VAR) STATUS_VAR);  //  NaN
        }
        return packFloat16(aSign, 0xFF, 0);  //  Infinity
    }
    if(aExp > 0x40F) {
        return packFloat16(aSign, 0xFF, 0);  //  Infinity
    }
    return packFloat16(aSign, aExp | ((aExp & 0x400) >> 6), aFrac >> 42);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the double-precision floating-point value
 | `a' to the extended double-precision floating-point format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 float64_to_floatx80(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint64_t aSig;

    a = float64_squash_input_denormal(a STATUS_VAR);
    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp == 0x7FF) {
        if(aSig) {
            return commonNaNToFloatx80(float64ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloatx80(aSign, 0x7FFF, LIT64(0x8000000000000000));
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloatx80(aSign, 0, 0);
        }
        normalizeFloat64Subnormal(aSig, &aExp, &aSig);
    }
    return packFloatx80(aSign, aExp + 0x3C00, (aSig | LIT64(0x0010000000000000)) << 11);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the double-precision floating-point value
 | `a' to the quadruple-precision floating-point format.  The conversion is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

float128 float64_to_float128(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint64_t aSig, zSig0, zSig1;

    a = float64_squash_input_denormal(a STATUS_VAR);
    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp == 0x7FF) {
        if(aSig) {
            return commonNaNToFloat128(float64ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat128(aSign, 0x7FFF, 0, 0);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloat128(aSign, 0, 0, 0);
        }
        normalizeFloat64Subnormal(aSig, &aExp, &aSig);
        --aExp;
    }
    shift128Right(aSig, 0, 4, &zSig0, &zSig1);
    return packFloat128(aSign, aExp + 0x3C00, zSig0, zSig1);
}

/*----------------------------------------------------------------------------
 | Rounds the double-precision floating-point value `a' to an integer, and
 | returns the result as a double-precision floating-point value.  The
 | operation is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 float64_round_to_int(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint64_t lastBitMask, roundBitsMask;
    int8 roundingMode;
    uint64_t z;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aExp = extractFloat64Exp(a);
    if(0x433 <= aExp) {
        if((aExp == 0x7FF) && extractFloat64Frac(a)) {
            return propagateFloat64NaN(a, a STATUS_VAR);
        }
        return a;
    }
    if(aExp < 0x3FF) {
        if((uint64_t)(float64_val(a) << 1) == 0) {
            return a;
        }
        STATUS(float_exception_flags) |= float_flag_inexact;
        aSign = extractFloat64Sign(a);
        switch(STATUS(float_rounding_mode)) {
            case float_round_nearest_even:
                if((aExp == 0x3FE) && extractFloat64Frac(a)) {
                    return packFloat64(aSign, 0x3FF, 0);
                }
                break;
            case float_round_down:
                return make_float64(aSign ? LIT64(0xBFF0000000000000) : 0);
            case float_round_up:
                return make_float64(aSign ? LIT64(0x8000000000000000) : LIT64(0x3FF0000000000000));
        }
        return packFloat64(aSign, 0, 0);
    }
    lastBitMask = 1;
    lastBitMask <<= 0x433 - aExp;
    roundBitsMask = lastBitMask - 1;
    z = float64_val(a);
    roundingMode = STATUS(float_rounding_mode);
    if(roundingMode == float_round_nearest_even) {
        z += lastBitMask >> 1;
        if((z & roundBitsMask) == 0) {
            z &= ~lastBitMask;
        }
    } else if(roundingMode != float_round_to_zero) {
        if(extractFloat64Sign(make_float64(z)) ^ (roundingMode == float_round_up)) {
            z += roundBitsMask;
        }
    }
    z &= ~roundBitsMask;
    if(z != float64_val(a)) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return make_float64(z);
}

float64 float64_trunc_to_int(float64 a STATUS_PARAM)
{
    int oldmode;
    float64 res;
    oldmode = STATUS(float_rounding_mode);
    STATUS(float_rounding_mode) = float_round_to_zero;
    res = float64_round_to_int(a STATUS_VAR);
    STATUS(float_rounding_mode) = oldmode;
    return res;
}

/*----------------------------------------------------------------------------
 | Returns the result of adding the absolute values of the double-precision
 | floating-point values `a' and `b'.  If `zSign' is 1, the sum is negated
 | before being returned.  `zSign' is ignored if the result is a NaN.
 | The addition is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static float64 addFloat64Sigs(float64 a, float64 b, flag zSign STATUS_PARAM)
{
    int16 aExp, bExp, zExp;
    uint64_t aSig, bSig, zSig;
    int16 expDiff;

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    bSig = extractFloat64Frac(b);
    bExp = extractFloat64Exp(b);
    expDiff = aExp - bExp;
    aSig <<= 9;
    bSig <<= 9;
    if(0 < expDiff) {
        if(aExp == 0x7FF) {
            if(aSig) {
                return propagateFloat64NaN(a, b STATUS_VAR);
            }
            return a;
        }
        if(bExp == 0) {
            --expDiff;
        } else {
            bSig |= LIT64(0x2000000000000000);
        }
        shift64RightJamming(bSig, expDiff, &bSig);
        zExp = aExp;
    } else if(expDiff < 0) {
        if(bExp == 0x7FF) {
            if(bSig) {
                return propagateFloat64NaN(a, b STATUS_VAR);
            }
            return packFloat64(zSign, 0x7FF, 0);
        }
        if(aExp == 0) {
            ++expDiff;
        } else {
            aSig |= LIT64(0x2000000000000000);
        }
        shift64RightJamming(aSig, -expDiff, &aSig);
        zExp = bExp;
    } else {
        if(aExp == 0x7FF) {
            if(aSig | bSig) {
                return propagateFloat64NaN(a, b STATUS_VAR);
            }
            return a;
        }
        if(aExp == 0) {
            if(STATUS(flush_to_zero)) {
                if(aSig | bSig) {
                    float_raise((int8)float_flag_output_denormal STATUS_VAR);
                }
                return packFloat64(zSign, 0, 0);
            }
            return packFloat64(zSign, 0, (aSig + bSig) >> 9);
        }
        zSig = LIT64(0x4000000000000000) + aSig + bSig;
        zExp = aExp;
        goto roundAndPack;
    }
    aSig |= LIT64(0x2000000000000000);
    zSig = (aSig + bSig) << 1;
    --zExp;
    if((int64_t)zSig < 0) {
        zSig = aSig + bSig;
        ++zExp;
    }
roundAndPack:
    return roundAndPackFloat64(zSign, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of subtracting the absolute values of the double-
 | precision floating-point values `a' and `b'.  If `zSign' is 1, the
 | difference is negated before being returned.  `zSign' is ignored if the
 | result is a NaN.  The subtraction is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static float64 subFloat64Sigs(float64 a, float64 b, flag zSign STATUS_PARAM)
{
    int16 aExp, bExp, zExp;
    uint64_t aSig, bSig, zSig;
    int16 expDiff;

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    bSig = extractFloat64Frac(b);
    bExp = extractFloat64Exp(b);
    expDiff = aExp - bExp;
    aSig <<= 10;
    bSig <<= 10;
    if(0 < expDiff) {
        goto aExpBigger;
    }
    if(expDiff < 0) {
        goto bExpBigger;
    }
    if(aExp == 0x7FF) {
        if(aSig | bSig) {
            return propagateFloat64NaN(a, b STATUS_VAR);
        }
        float_raise(float_flag_invalid STATUS_VAR);
        return float64_default_nan;
    }
    if(aExp == 0) {
        aExp = 1;
        bExp = 1;
    }
    if(bSig < aSig) {
        goto aBigger;
    }
    if(aSig < bSig) {
        goto bBigger;
    }
    return packFloat64(STATUS(float_rounding_mode) == float_round_down, 0, 0);
bExpBigger:
    if(bExp == 0x7FF) {
        if(bSig) {
            return propagateFloat64NaN(a, b STATUS_VAR);
        }
        return packFloat64(zSign ^ 1, 0x7FF, 0);
    }
    if(aExp == 0) {
        ++expDiff;
    } else {
        aSig |= LIT64(0x4000000000000000);
    }
    shift64RightJamming(aSig, -expDiff, &aSig);
    bSig |= LIT64(0x4000000000000000);
bBigger:
    zSig = bSig - aSig;
    zExp = bExp;
    zSign ^= 1;
    goto normalizeRoundAndPack;
aExpBigger:
    if(aExp == 0x7FF) {
        if(aSig) {
            return propagateFloat64NaN(a, b STATUS_VAR);
        }
        return a;
    }
    if(bExp == 0) {
        --expDiff;
    } else {
        bSig |= LIT64(0x4000000000000000);
    }
    shift64RightJamming(bSig, expDiff, &bSig);
    aSig |= LIT64(0x4000000000000000);
aBigger:
    zSig = aSig - bSig;
    zExp = aExp;
normalizeRoundAndPack:
    --zExp;
    return normalizeRoundAndPackFloat64(zSign, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of adding the double-precision floating-point values `a'
 | and `b'.  The operation is performed according to the IEC/IEEE Standard for
 | Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 float64_add(float64 a, float64 b STATUS_PARAM)
{
    flag aSign, bSign;
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    aSign = extractFloat64Sign(a);
    bSign = extractFloat64Sign(b);
    if(aSign == bSign) {
        return addFloat64Sigs(a, b, aSign STATUS_VAR);
    } else {
        return subFloat64Sigs(a, b, aSign STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns the result of subtracting the double-precision floating-point values
 | `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
 | for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 float64_sub(float64 a, float64 b STATUS_PARAM)
{
    flag aSign, bSign;
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    aSign = extractFloat64Sign(a);
    bSign = extractFloat64Sign(b);
    if(aSign == bSign) {
        return subFloat64Sigs(a, b, aSign STATUS_VAR);
    } else {
        return addFloat64Sigs(a, b, aSign STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns the result of multiplying the double-precision floating-point values
 | `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
 | for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 float64_mul(float64 a, float64 b STATUS_PARAM)
{
    flag aSign, bSign, zSign;
    int16 aExp, bExp, zExp;
    uint64_t aSig, bSig, zSig0, zSig1;

    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    bSig = extractFloat64Frac(b);
    bExp = extractFloat64Exp(b);
    bSign = extractFloat64Sign(b);
    zSign = aSign ^ bSign;
    if(aExp == 0x7FF) {
        if(aSig || ((bExp == 0x7FF) && bSig)) {
            return propagateFloat64NaN(a, b STATUS_VAR);
        }
        if((bExp | bSig) == 0) {
            float_raise(float_flag_invalid STATUS_VAR);
            return float64_default_nan;
        }
        return packFloat64(zSign, 0x7FF, 0);
    }
    if(bExp == 0x7FF) {
        if(bSig) {
            return propagateFloat64NaN(a, b STATUS_VAR);
        }
        if((aExp | aSig) == 0) {
            float_raise(float_flag_invalid STATUS_VAR);
            return float64_default_nan;
        }
        return packFloat64(zSign, 0x7FF, 0);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloat64(zSign, 0, 0);
        }
        normalizeFloat64Subnormal(aSig, &aExp, &aSig);
    }
    if(bExp == 0) {
        if(bSig == 0) {
            return packFloat64(zSign, 0, 0);
        }
        normalizeFloat64Subnormal(bSig, &bExp, &bSig);
    }
    zExp = aExp + bExp - 0x3FF;
    aSig = (aSig | LIT64(0x0010000000000000)) << 10;
    bSig = (bSig | LIT64(0x0010000000000000)) << 11;
    mul64To128(aSig, bSig, &zSig0, &zSig1);
    zSig0 |= (zSig1 != 0);
    if(0 <= (int64_t)(zSig0 << 1)) {
        zSig0 <<= 1;
        --zExp;
    }
    return roundAndPackFloat64(zSign, zExp, zSig0 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of dividing the double-precision floating-point value `a'
 | by the corresponding value `b'.  The operation is performed according to
 | the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 float64_div(float64 a, float64 b STATUS_PARAM)
{
    flag aSign, bSign, zSign;
    int16 aExp, bExp, zExp;
    uint64_t aSig, bSig, zSig;
    uint64_t rem0, rem1;
    uint64_t term0, term1;
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    bSig = extractFloat64Frac(b);
    bExp = extractFloat64Exp(b);
    bSign = extractFloat64Sign(b);
    zSign = aSign ^ bSign;
    if(aExp == 0x7FF) {
        if(aSig) {
            return propagateFloat64NaN(a, b STATUS_VAR);
        }
        if(bExp == 0x7FF) {
            if(bSig) {
                return propagateFloat64NaN(a, b STATUS_VAR);
            }
            float_raise(float_flag_invalid STATUS_VAR);
            return float64_default_nan;
        }
        return packFloat64(zSign, 0x7FF, 0);
    }
    if(bExp == 0x7FF) {
        if(bSig) {
            return propagateFloat64NaN(a, b STATUS_VAR);
        }
        return packFloat64(zSign, 0, 0);
    }
    if(bExp == 0) {
        if(bSig == 0) {
            if((aExp | aSig) == 0) {
                float_raise(float_flag_invalid STATUS_VAR);
                return float64_default_nan;
            }
            float_raise(float_flag_divbyzero STATUS_VAR);
            return packFloat64(zSign, 0x7FF, 0);
        }
        normalizeFloat64Subnormal(bSig, &bExp, &bSig);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloat64(zSign, 0, 0);
        }
        normalizeFloat64Subnormal(aSig, &aExp, &aSig);
    }
    zExp = aExp - bExp + 0x3FD;
    aSig = (aSig | LIT64(0x0010000000000000)) << 10;
    bSig = (bSig | LIT64(0x0010000000000000)) << 11;
    if(bSig <= (aSig + aSig)) {
        aSig >>= 1;
        ++zExp;
    }
    zSig = estimateDiv128To64(aSig, 0, bSig);
    if((zSig & 0x1FF) <= 2) {
        mul64To128(bSig, zSig, &term0, &term1);
        sub128(aSig, 0, term0, term1, &rem0, &rem1);
        while((int64_t)rem0 < 0) {
            --zSig;
            add128(rem0, rem1, 0, bSig, &rem0, &rem1);
        }
        zSig |= (rem1 != 0);
    }
    return roundAndPackFloat64(zSign, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the remainder of the double-precision floating-point value `a'
 | with respect to the corresponding value `b'.  The operation is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 float64_rem(float64 a, float64 b STATUS_PARAM)
{
    flag aSign, zSign;
    int16 aExp, bExp, expDiff;
    uint64_t aSig, bSig;
    uint64_t q, alternateASig;
    int64_t sigMean;

    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);
    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    bSig = extractFloat64Frac(b);
    bExp = extractFloat64Exp(b);
    if(aExp == 0x7FF) {
        if(aSig || ((bExp == 0x7FF) && bSig)) {
            return propagateFloat64NaN(a, b STATUS_VAR);
        }
        float_raise(float_flag_invalid STATUS_VAR);
        return float64_default_nan;
    }
    if(bExp == 0x7FF) {
        if(bSig) {
            return propagateFloat64NaN(a, b STATUS_VAR);
        }
        return a;
    }
    if(bExp == 0) {
        if(bSig == 0) {
            float_raise(float_flag_invalid STATUS_VAR);
            return float64_default_nan;
        }
        normalizeFloat64Subnormal(bSig, &bExp, &bSig);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return a;
        }
        normalizeFloat64Subnormal(aSig, &aExp, &aSig);
    }
    expDiff = aExp - bExp;
    aSig = (aSig | LIT64(0x0010000000000000)) << 11;
    bSig = (bSig | LIT64(0x0010000000000000)) << 11;
    if(expDiff < 0) {
        if(expDiff < -1) {
            return a;
        }
        aSig >>= 1;
    }
    q = (bSig <= aSig);
    if(q) {
        aSig -= bSig;
    }
    expDiff -= 64;
    while(0 < expDiff) {
        q = estimateDiv128To64(aSig, 0, bSig);
        q = (2 < q) ? q - 2 : 0;
        aSig = -((bSig >> 2) * q);
        expDiff -= 62;
    }
    expDiff += 64;
    if(0 < expDiff) {
        q = estimateDiv128To64(aSig, 0, bSig);
        q = (2 < q) ? q - 2 : 0;
        q >>= 64 - expDiff;
        bSig >>= 2;
        aSig = ((aSig >> 1) << (expDiff - 1)) - bSig * q;
    } else {
        aSig >>= 2;
        bSig >>= 2;
    }
    do {
        alternateASig = aSig;
        ++q;
        aSig -= bSig;
    } while(0 <= (int64_t)aSig);
    sigMean = aSig + alternateASig;
    if((sigMean < 0) || ((sigMean == 0) && (q & 1))) {
        aSig = alternateASig;
    }
    zSign = ((int64_t)aSig < 0);
    if(zSign) {
        aSig = -aSig;
    }
    return normalizeRoundAndPackFloat64(aSign ^ zSign, bExp, aSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of multiplying the double-precision floating-point values
 | `a' and `b' then adding 'c', with no intermediate rounding step after the
 | multiplication.  The operation is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic 754-2008.
 | The flags argument allows the caller to select negation of the
 | addend, the intermediate product, or the final result. (The difference
 | between this and having the caller do a separate negation is that negating
 | externally will flip the sign bit on NaNs.)
 *----------------------------------------------------------------------------*/

float64 float64_muladd(float64 a, float64 b, float64 c, int flags STATUS_PARAM)
{
    flag aSign, bSign, cSign, zSign;
    int aExp, bExp, cExp, pExp, zExp, expDiff;
    uint64_t aSig, bSig, cSig, zSig;
    flag pInf, pZero, pSign;
    uint64_t pSig0, pSig1, cSig0, cSig1, zSig0, zSig1;
    int shiftcount;
    flag signflip, infzero;

    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);
    c = float64_squash_input_denormal(c STATUS_VAR);
    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    bSig = extractFloat64Frac(b);
    bExp = extractFloat64Exp(b);
    bSign = extractFloat64Sign(b);
    cSig = extractFloat64Frac(c);
    cExp = extractFloat64Exp(c);
    cSign = extractFloat64Sign(c);

    infzero = ((aExp == 0 && aSig == 0 && bExp == 0x7ff && bSig == 0) || (aExp == 0x7ff && aSig == 0 && bExp == 0 && bSig == 0));

    /* It is implementation-defined whether the cases of (0,inf,qnan)
     * and (inf,0,qnan) raise InvalidOperation or not (and what QNaN
     * they return if they do), so we have to hand this information
     * off to the target-specific pick-a-NaN routine.
     */
    if(((aExp == 0x7ff) && aSig) || ((bExp == 0x7ff) && bSig) || ((cExp == 0x7ff) && cSig)) {
        return propagateFloat64MulAddNaN(a, b, c, infzero STATUS_VAR);
    }

    if(infzero) {
        float_raise(float_flag_invalid STATUS_VAR);
        return float64_default_nan;
    }

    if(flags & float_muladd_negate_c) {
        cSign ^= 1;
    }

    signflip = (flags & float_muladd_negate_result) ? 1 : 0;

    /* Work out the sign and type of the product */
    pSign = aSign ^ bSign;
    if(flags & float_muladd_negate_product) {
        pSign ^= 1;
    }
    pInf = (aExp == 0x7ff) || (bExp == 0x7ff);
    pZero = ((aExp | aSig) == 0) || ((bExp | bSig) == 0);

    if(cExp == 0x7ff) {
        if(pInf && (pSign ^ cSign)) {
            /* addition of opposite-signed infinities => InvalidOperation */
            float_raise(float_flag_invalid STATUS_VAR);
            return float64_default_nan;
        }
        /* Otherwise generate an infinity of the same sign */
        return packFloat64(cSign ^ signflip, 0x7ff, 0);
    }

    if(pInf) {
        return packFloat64(pSign ^ signflip, 0x7ff, 0);
    }

    if(pZero) {
        if(cExp == 0) {
            if(cSig == 0) {
                /* Adding two exact zeroes */
                if(pSign == cSign) {
                    zSign = pSign;
                } else if(STATUS(float_rounding_mode) == float_round_down) {
                    zSign = 1;
                } else {
                    zSign = 0;
                }
                return packFloat64(zSign ^ signflip, 0, 0);
            }
            /* Exact zero plus a denorm */
            if(STATUS(flush_to_zero)) {
                float_raise((int8)float_flag_output_denormal STATUS_VAR);
                return packFloat64(cSign ^ signflip, 0, 0);
            }
        }
        /* Zero plus something non-zero : just return the something */
        return c ^ ((uint64_t)signflip << 63);
    }

    if(aExp == 0) {
        normalizeFloat64Subnormal(aSig, &aExp, &aSig);
    }
    if(bExp == 0) {
        normalizeFloat64Subnormal(bSig, &bExp, &bSig);
    }

    /* Calculate the actual result a * b + c */

    /* Multiply first; this is easy. */
    /* NB: we subtract 0x3fe where float64_mul() subtracts 0x3ff
     * because we want the true exponent, not the "one-less-than"
     * flavour that roundAndPackFloat64() takes.
     */
    pExp = aExp + bExp - 0x3fe;
    aSig = (aSig | LIT64(0x0010000000000000)) << 10;
    bSig = (bSig | LIT64(0x0010000000000000)) << 11;
    mul64To128(aSig, bSig, &pSig0, &pSig1);
    if((int64_t)(pSig0 << 1) >= 0) {
        shortShift128Left(pSig0, pSig1, 1, &pSig0, &pSig1);
        pExp--;
    }

    zSign = pSign ^ signflip;

    /* Now [pSig0:pSig1] is the significand of the multiply, with the explicit
     * bit in position 126.
     */
    if(cExp == 0) {
        if(!cSig) {
            /* Throw out the special case of c being an exact zero now */
            shift128RightJamming(pSig0, pSig1, 64, &pSig0, &pSig1);

            zExp = pExp - 1;
            zSig = pSig1;
            goto halve_round_and_pack_float64;
        }
        normalizeFloat64Subnormal(cSig, &cExp, &cSig);
    }

    /* Shift cSig and add the explicit bit so [cSig0:cSig1] is the
     * significand of the addend, with the explicit bit in position 126.
     */
    cSig0 = cSig << (126 - 64 - 52);
    cSig1 = 0;
    cSig0 |= LIT64(0x4000000000000000);
    expDiff = pExp - cExp;

    if(pSign == cSign) {
        /* Addition */
        if(expDiff > 0) {
            /* scale c to match p */
            shift128RightJamming(cSig0, cSig1, expDiff, &cSig0, &cSig1);
            zExp = pExp;
        } else if(expDiff < 0) {
            /* scale p to match c */
            shift128RightJamming(pSig0, pSig1, -expDiff, &pSig0, &pSig1);
            zExp = cExp;
        } else {
            /* no scaling needed */
            zExp = cExp;
        }
        /* Add significands and make sure explicit bit ends up in posn 126 */
        add128(pSig0, pSig1, cSig0, cSig1, &zSig0, &zSig1);
        if((int64_t)zSig0 < 0) {
            shift128RightJamming(zSig0, zSig1, 1, &zSig0, &zSig1);
        } else {
            zExp--;
        }
        shift128RightJamming(zSig0, zSig1, 64, &zSig0, &zSig1);
        zSig = zSig1;
    } else {
        /* Subtraction */
        if(expDiff > 0) {
            shift128RightJamming(cSig0, cSig1, expDiff, &cSig0, &cSig1);
            sub128(pSig0, pSig1, cSig0, cSig1, &zSig0, &zSig1);
            zExp = pExp;
        } else if(expDiff < 0) {
            shift128RightJamming(pSig0, pSig1, -expDiff, &pSig0, &pSig1);
            sub128(cSig0, cSig1, pSig0, pSig1, &zSig0, &zSig1);
            zExp = cExp;
            zSign ^= 1;
        } else {
            zExp = pExp;
            if(lt128(cSig0, cSig1, pSig0, pSig1)) {
                sub128(pSig0, pSig1, cSig0, cSig1, &zSig0, &zSig1);
            } else if(lt128(pSig0, pSig1, cSig0, cSig1)) {
                sub128(cSig0, cSig1, pSig0, pSig1, &zSig0, &zSig1);
                zSign ^= 1;
            } else {
                /* Exact zero */
                zSign = signflip;
                if(STATUS(float_rounding_mode) == float_round_down) {
                    zSign ^= 1;
                }
                return packFloat64(zSign, 0, 0);
            }
        }
        --zExp;
        /* Do the equivalent of normalizeRoundAndPackFloat64() but
         * starting with the significand in a pair of uint64_t.
         */
        if(zSig0) {
            shiftcount = countLeadingZeros64(zSig0) - 1;
            shortShift128Left(zSig0, zSig1, shiftcount, &zSig0, &zSig1);
            if(zSig1) {
                zSig0 |= 1;
            }
            zExp -= shiftcount;
        } else {
            shiftcount = countLeadingZeros64(zSig1) - 1;
            zSig0 = zSig1 << shiftcount;
            zExp -= (shiftcount + 64);
        }
        zSig = zSig0;
    }

halve_round_and_pack_float64:
    if((flags & float_muladd_halve_result) && (zExp >= 1)) {
        zExp -= 1;
    }
    return roundAndPackFloat64(zSign, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the square root of the double-precision floating-point value `a'.
 | The operation is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 float64_sqrt(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, zExp;
    uint64_t aSig, zSig, doubleZSig;
    uint64_t rem0, rem1, term0, term1;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp == 0x7FF) {
        if(aSig) {
            return propagateFloat64NaN(a, a STATUS_VAR);
        }
        if(!aSign) {
            return a;
        }
        float_raise(float_flag_invalid STATUS_VAR);
        return float64_default_nan;
    }
    if(aSign) {
        if((aExp | aSig) == 0) {
            return a;
        }
        float_raise(float_flag_invalid STATUS_VAR);
        return float64_default_nan;
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return float64_zero;
        }
        normalizeFloat64Subnormal(aSig, &aExp, &aSig);
    }
    zExp = ((aExp - 0x3FF) >> 1) + 0x3FE;
    aSig |= LIT64(0x0010000000000000);
    zSig = estimateSqrt32(aExp, aSig >> 21);
    aSig <<= 9 - (aExp & 1);
    zSig = estimateDiv128To64(aSig, 0, zSig << 32) + (zSig << 30);
    if((zSig & 0x1FF) <= 5) {
        doubleZSig = zSig << 1;
        mul64To128(zSig, zSig, &term0, &term1);
        sub128(aSig, 0, term0, term1, &rem0, &rem1);
        while((int64_t)rem0 < 0) {
            --zSig;
            doubleZSig -= 2;
            add128(rem0, rem1, zSig >> 63, doubleZSig | 1, &rem0, &rem1);
        }
        zSig |= ((rem0 | rem1) != 0);
    }
    return roundAndPackFloat64(0, zExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the binary log of the double-precision floating-point value `a'.
 | The operation is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/
float64 float64_log2(float64 a STATUS_PARAM)
{
    flag aSign, zSign;
    int16 aExp;
    uint64_t aSig, aSig0, aSig1, zSig, i;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);

    if(aExp == 0) {
        if(aSig == 0) {
            return packFloat64(1, 0x7FF, 0);
        }
        normalizeFloat64Subnormal(aSig, &aExp, &aSig);
    }
    if(aSign) {
        float_raise(float_flag_invalid STATUS_VAR);
        return float64_default_nan;
    }
    if(aExp == 0x7FF) {
        if(aSig) {
            return propagateFloat64NaN(a, float64_zero STATUS_VAR);
        }
        return a;
    }

    aExp -= 0x3FF;
    aSig |= LIT64(0x0010000000000000);
    zSign = aExp < 0;
    zSig = (uint64_t)aExp << 52;
    for(i = 1LL << 51; i > 0; i >>= 1) {
        mul64To128(aSig, aSig, &aSig0, &aSig1);
        aSig = (aSig0 << 12) | (aSig1 >> 52);
        if(aSig & LIT64(0x0020000000000000)) {
            aSig >>= 1;
            zSig |= i;
        }
    }

    if(zSign) {
        zSig = -zSig;
    }
    return normalizeRoundAndPackFloat64(zSign, 0x408, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point value `a' is equal to the
 | corresponding value `b', and 0 otherwise.  The invalid exception is raised
 | if either operand is a NaN.  Otherwise, the comparison is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float64_eq(float64 a, float64 b STATUS_PARAM)
{
    uint64_t av, bv;
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat64Exp(a) == 0x7FF) && extractFloat64Frac(a)) || ((extractFloat64Exp(b) == 0x7FF) && extractFloat64Frac(b))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    av = float64_val(a);
    bv = float64_val(b);
    return (av == bv) || ((uint64_t)((av | bv) << 1) == 0);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point value `a' is less than or
 | equal to the corresponding value `b', and 0 otherwise.  The invalid
 | exception is raised if either operand is a NaN.  The comparison is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float64_le(float64 a, float64 b STATUS_PARAM)
{
    flag aSign, bSign;
    uint64_t av, bv;
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat64Exp(a) == 0x7FF) && extractFloat64Frac(a)) || ((extractFloat64Exp(b) == 0x7FF) && extractFloat64Frac(b))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    aSign = extractFloat64Sign(a);
    bSign = extractFloat64Sign(b);
    av = float64_val(a);
    bv = float64_val(b);
    if(aSign != bSign) {
        return aSign || ((uint64_t)((av | bv) << 1) == 0);
    }
    return (av == bv) || (aSign ^ (av < bv));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point value `a' is less than
 | the corresponding value `b', and 0 otherwise.  The invalid exception is
 | raised if either operand is a NaN.  The comparison is performed according
 | to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float64_lt(float64 a, float64 b STATUS_PARAM)
{
    flag aSign, bSign;
    uint64_t av, bv;

    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);
    if(((extractFloat64Exp(a) == 0x7FF) && extractFloat64Frac(a)) || ((extractFloat64Exp(b) == 0x7FF) && extractFloat64Frac(b))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    aSign = extractFloat64Sign(a);
    bSign = extractFloat64Sign(b);
    av = float64_val(a);
    bv = float64_val(b);
    if(aSign != bSign) {
        return aSign && ((uint64_t)((av | bv) << 1) != 0);
    }
    return (av != bv) && (aSign ^ (av < bv));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point values `a' and `b' cannot
 | be compared, and 0 otherwise.  The invalid exception is raised if either
 | operand is a NaN.  The comparison is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float64_unordered(float64 a, float64 b STATUS_PARAM)
{
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat64Exp(a) == 0x7FF) && extractFloat64Frac(a)) || ((extractFloat64Exp(b) == 0x7FF) && extractFloat64Frac(b))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 1;
    }
    return 0;
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point value `a' is equal to the
 | corresponding value `b', and 0 otherwise.  Quiet NaNs do not cause an
 | exception.The comparison is performed according to the IEC/IEEE Standard
 | for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float64_eq_quiet(float64 a, float64 b STATUS_PARAM)
{
    uint64_t av, bv;
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat64Exp(a) == 0x7FF) && extractFloat64Frac(a)) || ((extractFloat64Exp(b) == 0x7FF) && extractFloat64Frac(b))) {
        if(float64_is_signaling_nan(a STATUS_VAR) || float64_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    av = float64_val(a);
    bv = float64_val(b);
    return (av == bv) || ((uint64_t)((av | bv) << 1) == 0);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point value `a' is less than or
 | equal to the corresponding value `b', and 0 otherwise.  Quiet NaNs do not
 | cause an exception.  Otherwise, the comparison is performed according to the
 | IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float64_le_quiet(float64 a, float64 b STATUS_PARAM)
{
    flag aSign, bSign;
    uint64_t av, bv;
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat64Exp(a) == 0x7FF) && extractFloat64Frac(a)) || ((extractFloat64Exp(b) == 0x7FF) && extractFloat64Frac(b))) {
        if(float64_is_signaling_nan(a STATUS_VAR) || float64_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    aSign = extractFloat64Sign(a);
    bSign = extractFloat64Sign(b);
    av = float64_val(a);
    bv = float64_val(b);
    if(aSign != bSign) {
        return aSign || ((uint64_t)((av | bv) << 1) == 0);
    }
    return (av == bv) || (aSign ^ (av < bv));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point value `a' is less than
 | the corresponding value `b', and 0 otherwise.  Quiet NaNs do not cause an
 | exception.  Otherwise, the comparison is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float64_lt_quiet(float64 a, float64 b STATUS_PARAM)
{
    flag aSign, bSign;
    uint64_t av, bv;
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat64Exp(a) == 0x7FF) && extractFloat64Frac(a)) || ((extractFloat64Exp(b) == 0x7FF) && extractFloat64Frac(b))) {
        if(float64_is_signaling_nan(a STATUS_VAR) || float64_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    aSign = extractFloat64Sign(a);
    bSign = extractFloat64Sign(b);
    av = float64_val(a);
    bv = float64_val(b);
    if(aSign != bSign) {
        return aSign && ((uint64_t)((av | bv) << 1) != 0);
    }
    return (av != bv) && (aSign ^ (av < bv));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the double-precision floating-point values `a' and `b' cannot
 | be compared, and 0 otherwise.  Quiet NaNs do not cause an exception.  The
 | comparison is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float64_unordered_quiet(float64 a, float64 b STATUS_PARAM)
{
    a = float64_squash_input_denormal(a STATUS_VAR);
    b = float64_squash_input_denormal(b STATUS_VAR);

    if(((extractFloat64Exp(a) == 0x7FF) && extractFloat64Frac(a)) || ((extractFloat64Exp(b) == 0x7FF) && extractFloat64Frac(b))) {
        if(float64_is_signaling_nan(a STATUS_VAR) || float64_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 1;
    }
    return 0;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the extended double-precision floating-
 | point value `a' to the 32-bit two's complement integer format.  The
 | conversion is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic---which means in particular that the conversion
 | is rounded according to the current rounding mode.  If `a' is a NaN, the
 | largest positive integer is returned.  Otherwise, if the conversion
 | overflows, the largest integer with the same sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int32 floatx80_to_int32(floatx80 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, shiftCount;
    uint64_t aSig;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    if((aExp == 0x7FFF) && (uint64_t)(aSig << 1)) {
        aSign = 0;
    }
    shiftCount = 0x4037 - aExp;
    if(shiftCount <= 0) {
        shiftCount = 1;
    }
    shift64RightJamming(aSig, shiftCount, &aSig);
    return roundAndPackInt32(aSign, aSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the extended double-precision floating-
 | point value `a' to the 32-bit two's complement integer format.  The
 | conversion is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic, except that the conversion is always rounded
 | toward zero.  If `a' is a NaN, the largest positive integer is returned.
 | Otherwise, if the conversion overflows, the largest integer with the same
 | sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int32 floatx80_to_int32_round_to_zero(floatx80 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, shiftCount;
    uint64_t aSig, savedASig;
    int32 z;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    if(0x401E < aExp) {
        if((aExp == 0x7FFF) && (uint64_t)(aSig << 1)) {
            aSign = 0;
        }
        goto invalid;
    } else if(aExp < 0x3FFF) {
        if(aExp || aSig) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
        return 0;
    }
    shiftCount = 0x403E - aExp;
    savedASig = aSig;
    aSig >>= shiftCount;
    z = aSig;
    if(aSign) {
        z = -z;
    }
    if((z < 0) ^ aSign) {
    invalid:
        float_raise(float_flag_invalid STATUS_VAR);
        return aSign ? (int32_t)0x80000000 : 0x7FFFFFFF;
    }
    if((aSig << shiftCount) != savedASig) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the extended double-precision floating-
 | point value `a' to the 64-bit two's complement integer format.  The
 | conversion is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic---which means in particular that the conversion
 | is rounded according to the current rounding mode.  If `a' is a NaN,
 | the largest positive integer is returned.  Otherwise, if the conversion
 | overflows, the largest integer with the same sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int64 floatx80_to_int64(floatx80 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, shiftCount;
    uint64_t aSig, aSigExtra;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    shiftCount = 0x403E - aExp;
    if(shiftCount <= 0) {
        if(shiftCount) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0x7FFF) && (aSig != LIT64(0x8000000000000000)))) {
                return LIT64(0x7FFFFFFFFFFFFFFF);
            }
            return (int64_t)LIT64(0x8000000000000000);
        }
        aSigExtra = 0;
    } else {
        shift64ExtraRightJamming(aSig, 0, shiftCount, &aSig, &aSigExtra);
    }
    return roundAndPackInt64(aSign, aSig, aSigExtra STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the extended double-precision floating-
 | point value `a' to the 64-bit two's complement integer format.  The
 | conversion is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic, except that the conversion is always rounded
 | toward zero.  If `a' is a NaN, the largest positive integer is returned.
 | Otherwise, if the conversion overflows, the largest integer with the same
 | sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int64 floatx80_to_int64_round_to_zero(floatx80 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, shiftCount;
    uint64_t aSig;
    int64 z;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    shiftCount = aExp - 0x403E;
    if(0 <= shiftCount) {
        aSig &= LIT64(0x7FFFFFFFFFFFFFFF);
        if((a.high != 0xC03E) || aSig) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0x7FFF) && aSig)) {
                return LIT64(0x7FFFFFFFFFFFFFFF);
            }
        }
        return (int64_t)LIT64(0x8000000000000000);
    } else if(aExp < 0x3FFF) {
        if(aExp | aSig) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
        return 0;
    }
    z = aSig >> (-shiftCount);
    if((uint64_t)(aSig << (shiftCount & 63))) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    if(aSign) {
        z = -z;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the extended double-precision floating-
 | point value `a' to the single-precision floating-point format.  The
 | conversion is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float32 floatx80_to_float32(floatx80 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp;
    uint64_t aSig;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    if(aExp == 0x7FFF) {
        if((uint64_t)(aSig << 1)) {
            return commonNaNToFloat32(floatx80ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat32(aSign, 0xFF, 0);
    }
    shift64RightJamming(aSig, 33, &aSig);
    if(aExp || aSig) {
        aExp -= 0x3F81;
    }
    return roundAndPackFloat32(aSign, aExp, aSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the extended double-precision floating-
 | point value `a' to the double-precision floating-point format.  The
 | conversion is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float64 floatx80_to_float64(floatx80 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp;
    uint64_t aSig, zSig;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    if(aExp == 0x7FFF) {
        if((uint64_t)(aSig << 1)) {
            return commonNaNToFloat64(floatx80ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat64(aSign, 0x7FF, 0);
    }
    shift64RightJamming(aSig, 1, &zSig);
    if(aExp || aSig) {
        aExp -= 0x3C01;
    }
    return roundAndPackFloat64(aSign, aExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the extended double-precision floating-
 | point value `a' to the quadruple-precision floating-point format.  The
 | conversion is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 floatx80_to_float128(floatx80 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint64_t aSig, zSig0, zSig1;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    if((aExp == 0x7FFF) && (uint64_t)(aSig << 1)) {
        return commonNaNToFloat128(floatx80ToCommonNaN(a STATUS_VAR) STATUS_VAR);
    }
    shift128Right(aSig << 1, 0, 16, &zSig0, &zSig1);
    return packFloat128(aSign, aExp, zSig0, zSig1);
}

/*----------------------------------------------------------------------------
 | Rounds the extended double-precision floating-point value `a' to an integer,
 | and returns the result as an extended quadruple-precision floating-point
 | value.  The operation is performed according to the IEC/IEEE Standard for
 | Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 floatx80_round_to_int(floatx80 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp;
    uint64_t lastBitMask, roundBitsMask;
    int8 roundingMode;
    floatx80 z;

    aExp = extractFloatx80Exp(a);
    if(0x403E <= aExp) {
        if((aExp == 0x7FFF) && (uint64_t)(extractFloatx80Frac(a) << 1)) {
            return propagateFloatx80NaN(a, a STATUS_VAR);
        }
        return a;
    }
    if(aExp < 0x3FFF) {
        if((aExp == 0) && ((uint64_t)(extractFloatx80Frac(a) << 1) == 0)) {
            return a;
        }
        STATUS(float_exception_flags) |= float_flag_inexact;
        aSign = extractFloatx80Sign(a);
        switch(STATUS(float_rounding_mode)) {
            case float_round_nearest_even:
                if((aExp == 0x3FFE) && (uint64_t)(extractFloatx80Frac(a) << 1)) {
                    return packFloatx80(aSign, 0x3FFF, LIT64(0x8000000000000000));
                }
                break;
            case float_round_down:
                return aSign ? packFloatx80(1, 0x3FFF, LIT64(0x8000000000000000)) : packFloatx80(0, 0, 0);
            case float_round_up:
                return aSign ? packFloatx80(1, 0, 0) : packFloatx80(0, 0x3FFF, LIT64(0x8000000000000000));
        }
        return packFloatx80(aSign, 0, 0);
    }
    lastBitMask = 1;
    lastBitMask <<= 0x403E - aExp;
    roundBitsMask = lastBitMask - 1;
    z = a;
    roundingMode = STATUS(float_rounding_mode);
    if(roundingMode == float_round_nearest_even) {
        z.low += lastBitMask >> 1;
        if((z.low & roundBitsMask) == 0) {
            z.low &= ~lastBitMask;
        }
    } else if(roundingMode != float_round_to_zero) {
        if(extractFloatx80Sign(z) ^ (roundingMode == float_round_up)) {
            z.low += roundBitsMask;
        }
    }
    z.low &= ~roundBitsMask;
    if(z.low == 0) {
        ++z.high;
        z.low = LIT64(0x8000000000000000);
    }
    if(z.low != a.low) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of adding the absolute values of the extended double-
 | precision floating-point values `a' and `b'.  If `zSign' is 1, the sum is
 | negated before being returned.  `zSign' is ignored if the result is a NaN.
 | The addition is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static floatx80 addFloatx80Sigs(floatx80 a, floatx80 b, flag zSign STATUS_PARAM)
{
    int32 aExp, bExp, zExp;
    uint64_t aSig, bSig, zSig0, zSig1;
    int32 expDiff;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    bSig = extractFloatx80Frac(b);
    bExp = extractFloatx80Exp(b);
    expDiff = aExp - bExp;
    if(0 < expDiff) {
        if(aExp == 0x7FFF) {
            if((uint64_t)(aSig << 1)) {
                return propagateFloatx80NaN(a, b STATUS_VAR);
            }
            return a;
        }
        if(bExp == 0) {
            --expDiff;
        }
        shift64ExtraRightJamming(bSig, 0, expDiff, &bSig, &zSig1);
        zExp = aExp;
    } else if(expDiff < 0) {
        if(bExp == 0x7FFF) {
            if((uint64_t)(bSig << 1)) {
                return propagateFloatx80NaN(a, b STATUS_VAR);
            }
            return packFloatx80(zSign, 0x7FFF, LIT64(0x8000000000000000));
        }
        if(aExp == 0) {
            ++expDiff;
        }
        shift64ExtraRightJamming(aSig, 0, -expDiff, &aSig, &zSig1);
        zExp = bExp;
    } else {
        if(aExp == 0x7FFF) {
            if((uint64_t)((aSig | bSig) << 1)) {
                return propagateFloatx80NaN(a, b STATUS_VAR);
            }
            return a;
        }
        zSig1 = 0;
        zSig0 = aSig + bSig;
        if(aExp == 0) {
            normalizeFloatx80Subnormal(zSig0, &zExp, &zSig0);
            goto roundAndPack;
        }
        zExp = aExp;
        goto shiftRight1;
    }
    zSig0 = aSig + bSig;
    if((int64_t)zSig0 < 0) {
        goto roundAndPack;
    }
shiftRight1:
    shift64ExtraRightJamming(zSig0, zSig1, 1, &zSig0, &zSig1);
    zSig0 |= LIT64(0x8000000000000000);
    ++zExp;
roundAndPack:
    return roundAndPackFloatx80(STATUS(floatx80_rounding_precision), zSign, zExp, zSig0, zSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of subtracting the absolute values of the extended
 | double-precision floating-point values `a' and `b'.  If `zSign' is 1, the
 | difference is negated before being returned.  `zSign' is ignored if the
 | result is a NaN.  The subtraction is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static floatx80 subFloatx80Sigs(floatx80 a, floatx80 b, flag zSign STATUS_PARAM)
{
    int32 aExp, bExp, zExp;
    uint64_t aSig, bSig, zSig0, zSig1;
    int32 expDiff;
    floatx80 z;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    bSig = extractFloatx80Frac(b);
    bExp = extractFloatx80Exp(b);
    expDiff = aExp - bExp;
    if(0 < expDiff) {
        goto aExpBigger;
    }
    if(expDiff < 0) {
        goto bExpBigger;
    }
    if(aExp == 0x7FFF) {
        if((uint64_t)((aSig | bSig) << 1)) {
            return propagateFloatx80NaN(a, b STATUS_VAR);
        }
        float_raise(float_flag_invalid STATUS_VAR);
        z.low = floatx80_default_nan_low;
        z.high = floatx80_default_nan_high;
        return z;
    }
    if(aExp == 0) {
        aExp = 1;
        bExp = 1;
    }
    zSig1 = 0;
    if(bSig < aSig) {
        goto aBigger;
    }
    if(aSig < bSig) {
        goto bBigger;
    }
    return packFloatx80(STATUS(float_rounding_mode) == float_round_down, 0, 0);
bExpBigger:
    if(bExp == 0x7FFF) {
        if((uint64_t)(bSig << 1)) {
            return propagateFloatx80NaN(a, b STATUS_VAR);
        }
        return packFloatx80(zSign ^ 1, 0x7FFF, LIT64(0x8000000000000000));
    }
    if(aExp == 0) {
        ++expDiff;
    }
    shift128RightJamming(aSig, 0, -expDiff, &aSig, &zSig1);
bBigger:
    sub128(bSig, 0, aSig, zSig1, &zSig0, &zSig1);
    zExp = bExp;
    zSign ^= 1;
    goto normalizeRoundAndPack;
aExpBigger:
    if(aExp == 0x7FFF) {
        if((uint64_t)(aSig << 1)) {
            return propagateFloatx80NaN(a, b STATUS_VAR);
        }
        return a;
    }
    if(bExp == 0) {
        --expDiff;
    }
    shift128RightJamming(bSig, 0, expDiff, &bSig, &zSig1);
aBigger:
    sub128(aSig, 0, bSig, zSig1, &zSig0, &zSig1);
    zExp = aExp;
normalizeRoundAndPack:
    return normalizeRoundAndPackFloatx80(STATUS(floatx80_rounding_precision), zSign, zExp, zSig0, zSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of adding the extended double-precision floating-point
 | values `a' and `b'.  The operation is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 floatx80_add(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aSign, bSign;

    aSign = extractFloatx80Sign(a);
    bSign = extractFloatx80Sign(b);
    if(aSign == bSign) {
        return addFloatx80Sigs(a, b, aSign STATUS_VAR);
    } else {
        return subFloatx80Sigs(a, b, aSign STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns the result of subtracting the extended double-precision floating-
 | point values `a' and `b'.  The operation is performed according to the
 | IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 floatx80_sub(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aSign, bSign;

    aSign = extractFloatx80Sign(a);
    bSign = extractFloatx80Sign(b);
    if(aSign == bSign) {
        return subFloatx80Sigs(a, b, aSign STATUS_VAR);
    } else {
        return addFloatx80Sigs(a, b, aSign STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns the result of multiplying the extended double-precision floating-
 | point values `a' and `b'.  The operation is performed according to the
 | IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 floatx80_mul(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aSign, bSign, zSign;
    int32 aExp, bExp, zExp;
    uint64_t aSig, bSig, zSig0, zSig1;
    floatx80 z;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    bSig = extractFloatx80Frac(b);
    bExp = extractFloatx80Exp(b);
    bSign = extractFloatx80Sign(b);
    zSign = aSign ^ bSign;
    if(aExp == 0x7FFF) {
        if((uint64_t)(aSig << 1) || ((bExp == 0x7FFF) && (uint64_t)(bSig << 1))) {
            return propagateFloatx80NaN(a, b STATUS_VAR);
        }
        if((bExp | bSig) == 0) {
            goto invalid;
        }
        return packFloatx80(zSign, 0x7FFF, LIT64(0x8000000000000000));
    }
    if(bExp == 0x7FFF) {
        if((uint64_t)(bSig << 1)) {
            return propagateFloatx80NaN(a, b STATUS_VAR);
        }
        if((aExp | aSig) == 0) {
        invalid:
            float_raise(float_flag_invalid STATUS_VAR);
            z.low = floatx80_default_nan_low;
            z.high = floatx80_default_nan_high;
            return z;
        }
        return packFloatx80(zSign, 0x7FFF, LIT64(0x8000000000000000));
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloatx80(zSign, 0, 0);
        }
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    if(bExp == 0) {
        if(bSig == 0) {
            return packFloatx80(zSign, 0, 0);
        }
        normalizeFloatx80Subnormal(bSig, &bExp, &bSig);
    }
    zExp = aExp + bExp - 0x3FFE;
    mul64To128(aSig, bSig, &zSig0, &zSig1);
    if(0 < (int64_t)zSig0) {
        shortShift128Left(zSig0, zSig1, 1, &zSig0, &zSig1);
        --zExp;
    }
    return roundAndPackFloatx80(STATUS(floatx80_rounding_precision), zSign, zExp, zSig0, zSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of dividing the extended double-precision floating-point
 | value `a' by the corresponding value `b'.  The operation is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 floatx80_div(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aSign, bSign, zSign;
    int32 aExp, bExp, zExp;
    uint64_t aSig, bSig, zSig0, zSig1;
    uint64_t rem0, rem1, rem2, term0, term1, term2;
    floatx80 z;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    bSig = extractFloatx80Frac(b);
    bExp = extractFloatx80Exp(b);
    bSign = extractFloatx80Sign(b);
    zSign = aSign ^ bSign;
    if(aExp == 0x7FFF) {
        if((uint64_t)(aSig << 1)) {
            return propagateFloatx80NaN(a, b STATUS_VAR);
        }
        if(bExp == 0x7FFF) {
            if((uint64_t)(bSig << 1)) {
                return propagateFloatx80NaN(a, b STATUS_VAR);
            }
            goto invalid;
        }
        return packFloatx80(zSign, 0x7FFF, LIT64(0x8000000000000000));
    }
    if(bExp == 0x7FFF) {
        if((uint64_t)(bSig << 1)) {
            return propagateFloatx80NaN(a, b STATUS_VAR);
        }
        return packFloatx80(zSign, 0, 0);
    }
    if(bExp == 0) {
        if(bSig == 0) {
            if((aExp | aSig) == 0) {
            invalid:
                float_raise(float_flag_invalid STATUS_VAR);
                z.low = floatx80_default_nan_low;
                z.high = floatx80_default_nan_high;
                return z;
            }
            float_raise(float_flag_divbyzero STATUS_VAR);
            return packFloatx80(zSign, 0x7FFF, LIT64(0x8000000000000000));
        }
        normalizeFloatx80Subnormal(bSig, &bExp, &bSig);
    }
    if(aExp == 0) {
        if(aSig == 0) {
            return packFloatx80(zSign, 0, 0);
        }
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }
    zExp = aExp - bExp + 0x3FFE;
    rem1 = 0;
    if(bSig <= aSig) {
        shift128Right(aSig, 0, 1, &aSig, &rem1);
        ++zExp;
    }
    zSig0 = estimateDiv128To64(aSig, rem1, bSig);
    mul64To128(bSig, zSig0, &term0, &term1);
    sub128(aSig, rem1, term0, term1, &rem0, &rem1);
    while((int64_t)rem0 < 0) {
        --zSig0;
        add128(rem0, rem1, 0, bSig, &rem0, &rem1);
    }
    zSig1 = estimateDiv128To64(rem1, 0, bSig);
    if((uint64_t)(zSig1 << 1) <= 8) {
        mul64To128(bSig, zSig1, &term1, &term2);
        sub128(rem1, 0, term1, term2, &rem1, &rem2);
        while((int64_t)rem1 < 0) {
            --zSig1;
            add128(rem1, rem2, 0, bSig, &rem1, &rem2);
        }
        zSig1 |= ((rem1 | rem2) != 0);
    }
    return roundAndPackFloatx80(STATUS(floatx80_rounding_precision), zSign, zExp, zSig0, zSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the remainder of the extended double-precision floating-point value
 | `a' with respect to the corresponding value `b'.  The operation is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 floatx80_rem(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aSign, zSign;
    int32 aExp, bExp, expDiff;
    uint64_t aSig0, aSig1, bSig;
    uint64_t q, term0, term1, alternateASig0, alternateASig1;
    floatx80 z;

    aSig0 = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    bSig = extractFloatx80Frac(b);
    bExp = extractFloatx80Exp(b);
    if(aExp == 0x7FFF) {
        if((uint64_t)(aSig0 << 1) || ((bExp == 0x7FFF) && (uint64_t)(bSig << 1))) {
            return propagateFloatx80NaN(a, b STATUS_VAR);
        }
        goto invalid;
    }
    if(bExp == 0x7FFF) {
        if((uint64_t)(bSig << 1)) {
            return propagateFloatx80NaN(a, b STATUS_VAR);
        }
        return a;
    }
    if(bExp == 0) {
        if(bSig == 0) {
        invalid:
            float_raise(float_flag_invalid STATUS_VAR);
            z.low = floatx80_default_nan_low;
            z.high = floatx80_default_nan_high;
            return z;
        }
        normalizeFloatx80Subnormal(bSig, &bExp, &bSig);
    }
    if(aExp == 0) {
        if((uint64_t)(aSig0 << 1) == 0) {
            return a;
        }
        normalizeFloatx80Subnormal(aSig0, &aExp, &aSig0);
    }
    bSig |= LIT64(0x8000000000000000);
    zSign = aSign;
    expDiff = aExp - bExp;
    aSig1 = 0;
    if(expDiff < 0) {
        if(expDiff < -1) {
            return a;
        }
        shift128Right(aSig0, 0, 1, &aSig0, &aSig1);
        expDiff = 0;
    }
    q = (bSig <= aSig0);
    if(q) {
        aSig0 -= bSig;
    }
    expDiff -= 64;
    while(0 < expDiff) {
        q = estimateDiv128To64(aSig0, aSig1, bSig);
        q = (2 < q) ? q - 2 : 0;
        mul64To128(bSig, q, &term0, &term1);
        sub128(aSig0, aSig1, term0, term1, &aSig0, &aSig1);
        shortShift128Left(aSig0, aSig1, 62, &aSig0, &aSig1);
        expDiff -= 62;
    }
    expDiff += 64;
    if(0 < expDiff) {
        q = estimateDiv128To64(aSig0, aSig1, bSig);
        q = (2 < q) ? q - 2 : 0;
        q >>= 64 - expDiff;
        mul64To128(bSig, q << (64 - expDiff), &term0, &term1);
        sub128(aSig0, aSig1, term0, term1, &aSig0, &aSig1);
        shortShift128Left(0, bSig, 64 - expDiff, &term0, &term1);
        while(le128(term0, term1, aSig0, aSig1)) {
            ++q;
            sub128(aSig0, aSig1, term0, term1, &aSig0, &aSig1);
        }
    } else {
        term1 = 0;
        term0 = bSig;
    }
    sub128(term0, term1, aSig0, aSig1, &alternateASig0, &alternateASig1);
    if(lt128(alternateASig0, alternateASig1, aSig0, aSig1) || (eq128(alternateASig0, alternateASig1, aSig0, aSig1) && (q & 1))) {
        aSig0 = alternateASig0;
        aSig1 = alternateASig1;
        zSign = !zSign;
    }
    return normalizeRoundAndPackFloatx80(80, zSign, bExp + expDiff, aSig0, aSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the square root of the extended double-precision floating-point
 | value `a'.  The operation is performed according to the IEC/IEEE Standard
 | for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 floatx80_sqrt(floatx80 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, zExp;
    uint64_t aSig0, aSig1, zSig0, zSig1, doubleZSig0;
    uint64_t rem0, rem1, rem2, rem3, term0, term1, term2, term3;
    floatx80 z;

    aSig0 = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    if(aExp == 0x7FFF) {
        if((uint64_t)(aSig0 << 1)) {
            return propagateFloatx80NaN(a, a STATUS_VAR);
        }
        if(!aSign) {
            return a;
        }
        goto invalid;
    }
    if(aSign) {
        if((aExp | aSig0) == 0) {
            return a;
        }
    invalid:
        float_raise(float_flag_invalid STATUS_VAR);
        z.low = floatx80_default_nan_low;
        z.high = floatx80_default_nan_high;
        return z;
    }
    if(aExp == 0) {
        if(aSig0 == 0) {
            return packFloatx80(0, 0, 0);
        }
        normalizeFloatx80Subnormal(aSig0, &aExp, &aSig0);
    }
    zExp = ((aExp - 0x3FFF) >> 1) + 0x3FFF;
    zSig0 = estimateSqrt32(aExp, aSig0 >> 32);
    shift128Right(aSig0, 0, 2 + (aExp & 1), &aSig0, &aSig1);
    zSig0 = estimateDiv128To64(aSig0, aSig1, zSig0 << 32) + (zSig0 << 30);
    doubleZSig0 = zSig0 << 1;
    mul64To128(zSig0, zSig0, &term0, &term1);
    sub128(aSig0, aSig1, term0, term1, &rem0, &rem1);
    while((int64_t)rem0 < 0) {
        --zSig0;
        doubleZSig0 -= 2;
        add128(rem0, rem1, zSig0 >> 63, doubleZSig0 | 1, &rem0, &rem1);
    }
    zSig1 = estimateDiv128To64(rem1, 0, doubleZSig0);
    if((zSig1 & LIT64(0x3FFFFFFFFFFFFFFF)) <= 5) {
        if(zSig1 == 0) {
            zSig1 = 1;
        }
        mul64To128(doubleZSig0, zSig1, &term1, &term2);
        sub128(rem1, 0, term1, term2, &rem1, &rem2);
        mul64To128(zSig1, zSig1, &term2, &term3);
        sub192(rem1, rem2, 0, 0, term2, term3, &rem1, &rem2, &rem3);
        while((int64_t)rem1 < 0) {
            --zSig1;
            shortShift128Left(0, zSig1, 1, &term2, &term3);
            term3 |= 1;
            term2 |= doubleZSig0;
            add192(rem1, rem2, rem3, 0, term2, term3, &rem1, &rem2, &rem3);
        }
        zSig1 |= ((rem1 | rem2 | rem3) != 0);
    }
    shortShift128Left(0, zSig1, 1, &zSig0, &zSig1);
    zSig0 |= doubleZSig0;
    return roundAndPackFloatx80(STATUS(floatx80_rounding_precision), 0, zExp, zSig0, zSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point value `a' is equal
 | to the corresponding value `b', and 0 otherwise.  The invalid exception is
 | raised if either operand is a NaN.  Otherwise, the comparison is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int floatx80_eq(floatx80 a, floatx80 b STATUS_PARAM)
{

    if(((extractFloatx80Exp(a) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(a) << 1)) ||
       ((extractFloatx80Exp(b) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(b) << 1))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    return (a.low == b.low) && ((a.high == b.high) || ((a.low == 0) && ((uint16_t)((a.high | b.high) << 1) == 0)));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point value `a' is
 | less than or equal to the corresponding value `b', and 0 otherwise.  The
 | invalid exception is raised if either operand is a NaN.  The comparison is
 | performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

int floatx80_le(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloatx80Exp(a) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(a) << 1)) ||
       ((extractFloatx80Exp(b) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(b) << 1))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    aSign = extractFloatx80Sign(a);
    bSign = extractFloatx80Sign(b);
    if(aSign != bSign) {
        return aSign || ((((uint16_t)((a.high | b.high) << 1)) | a.low | b.low) == 0);
    }
    return aSign ? le128(b.high, b.low, a.high, a.low) : le128(a.high, a.low, b.high, b.low);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point value `a' is
 | less than the corresponding value `b', and 0 otherwise.  The invalid
 | exception is raised if either operand is a NaN.  The comparison is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int floatx80_lt(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloatx80Exp(a) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(a) << 1)) ||
       ((extractFloatx80Exp(b) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(b) << 1))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    aSign = extractFloatx80Sign(a);
    bSign = extractFloatx80Sign(b);
    if(aSign != bSign) {
        return aSign && ((((uint16_t)((a.high | b.high) << 1)) | a.low | b.low) != 0);
    }
    return aSign ? lt128(b.high, b.low, a.high, a.low) : lt128(a.high, a.low, b.high, b.low);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point values `a' and `b'
 | cannot be compared, and 0 otherwise.  The invalid exception is raised if
 | either operand is a NaN.   The comparison is performed according to the
 | IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/
int floatx80_unordered(floatx80 a, floatx80 b STATUS_PARAM)
{
    if(((extractFloatx80Exp(a) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(a) << 1)) ||
       ((extractFloatx80Exp(b) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(b) << 1))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 1;
    }
    return 0;
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point value `a' is
 | equal to the corresponding value `b', and 0 otherwise.  Quiet NaNs do not
 | cause an exception.  The comparison is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int floatx80_eq_quiet(floatx80 a, floatx80 b STATUS_PARAM)
{

    if(((extractFloatx80Exp(a) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(a) << 1)) ||
       ((extractFloatx80Exp(b) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(b) << 1))) {
        if(floatx80_is_signaling_nan(a STATUS_VAR) || floatx80_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    return (a.low == b.low) && ((a.high == b.high) || ((a.low == 0) && ((uint16_t)((a.high | b.high) << 1) == 0)));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point value `a' is less
 | than or equal to the corresponding value `b', and 0 otherwise.  Quiet NaNs
 | do not cause an exception.  Otherwise, the comparison is performed according
 | to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int floatx80_le_quiet(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloatx80Exp(a) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(a) << 1)) ||
       ((extractFloatx80Exp(b) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(b) << 1))) {
        if(floatx80_is_signaling_nan(a STATUS_VAR) || floatx80_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    aSign = extractFloatx80Sign(a);
    bSign = extractFloatx80Sign(b);
    if(aSign != bSign) {
        return aSign || ((((uint16_t)((a.high | b.high) << 1)) | a.low | b.low) == 0);
    }
    return aSign ? le128(b.high, b.low, a.high, a.low) : le128(a.high, a.low, b.high, b.low);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point value `a' is less
 | than the corresponding value `b', and 0 otherwise.  Quiet NaNs do not cause
 | an exception.  Otherwise, the comparison is performed according to the
 | IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int floatx80_lt_quiet(floatx80 a, floatx80 b STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloatx80Exp(a) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(a) << 1)) ||
       ((extractFloatx80Exp(b) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(b) << 1))) {
        if(floatx80_is_signaling_nan(a STATUS_VAR) || floatx80_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    aSign = extractFloatx80Sign(a);
    bSign = extractFloatx80Sign(b);
    if(aSign != bSign) {
        return aSign && ((((uint16_t)((a.high | b.high) << 1)) | a.low | b.low) != 0);
    }
    return aSign ? lt128(b.high, b.low, a.high, a.low) : lt128(a.high, a.low, b.high, b.low);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the extended double-precision floating-point values `a' and `b'
 | cannot be compared, and 0 otherwise.  Quiet NaNs do not cause an exception.
 | The comparison is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/
int floatx80_unordered_quiet(floatx80 a, floatx80 b STATUS_PARAM)
{
    if(((extractFloatx80Exp(a) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(a) << 1)) ||
       ((extractFloatx80Exp(b) == 0x7FFF) && (uint64_t)(extractFloatx80Frac(b) << 1))) {
        if(floatx80_is_signaling_nan(a STATUS_VAR) || floatx80_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 1;
    }
    return 0;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the quadruple-precision floating-point
 | value `a' to the 32-bit two's complement integer format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic---which means in particular that the conversion is rounded
 | according to the current rounding mode.  If `a' is a NaN, the largest
 | positive integer is returned.  Otherwise, if the conversion overflows, the
 | largest integer with the same sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int32 float128_to_int32(float128 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, shiftCount;
    uint64_t aSig0, aSig1;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    if((aExp == 0x7FFF) && (aSig0 | aSig1)) {
        aSign = 0;
    }
    if(aExp) {
        aSig0 |= LIT64(0x0001000000000000);
    }
    aSig0 |= (aSig1 != 0);
    shiftCount = 0x4028 - aExp;
    if(0 < shiftCount) {
        shift64RightJamming(aSig0, shiftCount, &aSig0);
    }
    return roundAndPackInt32(aSign, aSig0 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the quadruple-precision floating-point
 | value `a' to the 32-bit two's complement integer format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic, except that the conversion is always rounded toward zero.  If
 | `a' is a NaN, the largest positive integer is returned.  Otherwise, if the
 | conversion overflows, the largest integer with the same sign as `a' is
 | returned.
 *----------------------------------------------------------------------------*/

int32 float128_to_int32_round_to_zero(float128 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, shiftCount;
    uint64_t aSig0, aSig1, savedASig;
    int32 z;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    aSig0 |= (aSig1 != 0);
    if(0x401E < aExp) {
        if((aExp == 0x7FFF) && aSig0) {
            aSign = 0;
        }
        goto invalid;
    } else if(aExp < 0x3FFF) {
        if(aExp || aSig0) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
        return 0;
    }
    aSig0 |= LIT64(0x0001000000000000);
    shiftCount = 0x402F - aExp;
    savedASig = aSig0;
    aSig0 >>= shiftCount;
    z = aSig0;
    if(aSign) {
        z = -z;
    }
    if((z < 0) ^ aSign) {
    invalid:
        float_raise(float_flag_invalid STATUS_VAR);
        return aSign ? (int32_t)0x80000000 : 0x7FFFFFFF;
    }
    if((aSig0 << shiftCount) != savedASig) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the quadruple-precision floating-point
 | value `a' to the 64-bit two's complement integer format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic---which means in particular that the conversion is rounded
 | according to the current rounding mode.  If `a' is a NaN, the largest
 | positive integer is returned.  Otherwise, if the conversion overflows, the
 | largest integer with the same sign as `a' is returned.
 *----------------------------------------------------------------------------*/

int64 float128_to_int64(float128 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, shiftCount;
    uint64_t aSig0, aSig1;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    if(aExp) {
        aSig0 |= LIT64(0x0001000000000000);
    }
    shiftCount = 0x402F - aExp;
    if(shiftCount <= 0) {
        if(0x403E < aExp) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0x7FFF) && (aSig1 || (aSig0 != LIT64(0x0001000000000000))))) {
                return LIT64(0x7FFFFFFFFFFFFFFF);
            }
            return (int64_t)LIT64(0x8000000000000000);
        }
        shortShift128Left(aSig0, aSig1, -shiftCount, &aSig0, &aSig1);
    } else {
        shift64ExtraRightJamming(aSig0, aSig1, shiftCount, &aSig0, &aSig1);
    }
    return roundAndPackInt64(aSign, aSig0, aSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the quadruple-precision floating-point
 | value `a' to the 64-bit two's complement integer format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic, except that the conversion is always rounded toward zero.
 | If `a' is a NaN, the largest positive integer is returned.  Otherwise, if
 | the conversion overflows, the largest integer with the same sign as `a' is
 | returned.
 *----------------------------------------------------------------------------*/

int64 float128_to_int64_round_to_zero(float128 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, shiftCount;
    uint64_t aSig0, aSig1;
    int64 z;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    if(aExp) {
        aSig0 |= LIT64(0x0001000000000000);
    }
    shiftCount = aExp - 0x402F;
    if(0 < shiftCount) {
        if(0x403E <= aExp) {
            aSig0 &= LIT64(0x0000FFFFFFFFFFFF);
            if((a.high == LIT64(0xC03E000000000000)) && (aSig1 < LIT64(0x0002000000000000))) {
                if(aSig1) {
                    STATUS(float_exception_flags) |= float_flag_inexact;
                }
            } else {
                float_raise(float_flag_invalid STATUS_VAR);
                if(!aSign || ((aExp == 0x7FFF) && (aSig0 | aSig1))) {
                    return LIT64(0x7FFFFFFFFFFFFFFF);
                }
            }
            return (int64_t)LIT64(0x8000000000000000);
        }
        z = (aSig0 << shiftCount) | (aSig1 >> ((-shiftCount) & 63));
        if((uint64_t)(aSig1 << shiftCount)) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
    } else {
        if(aExp < 0x3FFF) {
            if(aExp | aSig0 | aSig1) {
                STATUS(float_exception_flags) |= float_flag_inexact;
            }
            return 0;
        }
        z = aSig0 >> (-shiftCount);
        if(aSig1 || (shiftCount && (uint64_t)(aSig0 << (shiftCount & 63)))) {
            STATUS(float_exception_flags) |= float_flag_inexact;
        }
    }
    if(aSign) {
        z = -z;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the quadruple-precision floating-point
 | value `a' to the single-precision floating-point format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

float32 float128_to_float32(float128 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp;
    uint64_t aSig0, aSig1;
    uint32_t zSig;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    if(aExp == 0x7FFF) {
        if(aSig0 | aSig1) {
            return commonNaNToFloat32(float128ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat32(aSign, 0xFF, 0);
    }
    aSig0 |= (aSig1 != 0);
    shift64RightJamming(aSig0, 18, &aSig0);
    zSig = aSig0;
    if(aExp || zSig) {
        zSig |= 0x40000000;
        aExp -= 0x3F81;
    }
    return roundAndPackFloat32(aSign, aExp, zSig STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the quadruple-precision floating-point
 | value `a' to the double-precision floating-point format.  The conversion
 | is performed according to the IEC/IEEE Standard for Binary Floating-Point
 | Arithmetic.
 *----------------------------------------------------------------------------*/

float64 float128_to_float64(float128 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp;
    uint64_t aSig0, aSig1;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    if(aExp == 0x7FFF) {
        if(aSig0 | aSig1) {
            return commonNaNToFloat64(float128ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat64(aSign, 0x7FF, 0);
    }
    shortShift128Left(aSig0, aSig1, 14, &aSig0, &aSig1);
    aSig0 |= (aSig1 != 0);
    if(aExp || aSig0) {
        aSig0 |= LIT64(0x4000000000000000);
        aExp -= 0x3C01;
    }
    return roundAndPackFloat64(aSign, aExp, aSig0 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of converting the quadruple-precision floating-point
 | value `a' to the extended double-precision floating-point format.  The
 | conversion is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

floatx80 float128_to_floatx80(float128 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp;
    uint64_t aSig0, aSig1;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    if(aExp == 0x7FFF) {
        if(aSig0 | aSig1) {
            return commonNaNToFloatx80(float128ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloatx80(aSign, 0x7FFF, LIT64(0x8000000000000000));
    }
    if(aExp == 0) {
        if((aSig0 | aSig1) == 0) {
            return packFloatx80(aSign, 0, 0);
        }
        normalizeFloat128Subnormal(aSig0, aSig1, &aExp, &aSig0, &aSig1);
    } else {
        aSig0 |= LIT64(0x0001000000000000);
    }
    shortShift128Left(aSig0, aSig1, 15, &aSig0, &aSig1);
    return roundAndPackFloatx80(80, aSign, aExp, aSig0, aSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Rounds the quadruple-precision floating-point value `a' to an integer, and
 | returns the result as a quadruple-precision floating-point value.  The
 | operation is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 float128_round_to_int(float128 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp;
    uint64_t lastBitMask, roundBitsMask;
    int8 roundingMode;
    float128 z;

    aExp = extractFloat128Exp(a);
    if(0x402F <= aExp) {
        if(0x406F <= aExp) {
            if((aExp == 0x7FFF) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) {
                return propagateFloat128NaN(a, a STATUS_VAR);
            }
            return a;
        }
        lastBitMask = 1;
        lastBitMask = (lastBitMask << (0x406E - aExp)) << 1;
        roundBitsMask = lastBitMask - 1;
        z = a;
        roundingMode = STATUS(float_rounding_mode);
        if(roundingMode == float_round_nearest_even) {
            if(lastBitMask) {
                add128(z.high, z.low, 0, lastBitMask >> 1, &z.high, &z.low);
                if((z.low & roundBitsMask) == 0) {
                    z.low &= ~lastBitMask;
                }
            } else {
                if((int64_t)z.low < 0) {
                    ++z.high;
                    if((uint64_t)(z.low << 1) == 0) {
                        z.high &= ~1;
                    }
                }
            }
        } else if(roundingMode != float_round_to_zero) {
            if(extractFloat128Sign(z) ^ (roundingMode == float_round_up)) {
                add128(z.high, z.low, 0, roundBitsMask, &z.high, &z.low);
            }
        }
        z.low &= ~roundBitsMask;
    } else {
        if(aExp < 0x3FFF) {
            if((((uint64_t)(a.high << 1)) | a.low) == 0) {
                return a;
            }
            STATUS(float_exception_flags) |= float_flag_inexact;
            aSign = extractFloat128Sign(a);
            switch(STATUS(float_rounding_mode)) {
                case float_round_nearest_even:
                    if((aExp == 0x3FFE) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) {
                        return packFloat128(aSign, 0x3FFF, 0, 0);
                    }
                    break;
                case float_round_down:
                    return aSign ? packFloat128(1, 0x3FFF, 0, 0) : packFloat128(0, 0, 0, 0);
                case float_round_up:
                    return aSign ? packFloat128(1, 0, 0, 0) : packFloat128(0, 0x3FFF, 0, 0);
            }
            return packFloat128(aSign, 0, 0, 0);
        }
        lastBitMask = 1;
        lastBitMask <<= 0x402F - aExp;
        roundBitsMask = lastBitMask - 1;
        z.low = 0;
        z.high = a.high;
        roundingMode = STATUS(float_rounding_mode);
        if(roundingMode == float_round_nearest_even) {
            z.high += lastBitMask >> 1;
            if(((z.high & roundBitsMask) | a.low) == 0) {
                z.high &= ~lastBitMask;
            }
        } else if(roundingMode != float_round_to_zero) {
            if(extractFloat128Sign(z) ^ (roundingMode == float_round_up)) {
                z.high |= (a.low != 0);
                z.high += roundBitsMask;
            }
        }
        z.high &= ~roundBitsMask;
    }
    if((z.low != a.low) || (z.high != a.high)) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

/*----------------------------------------------------------------------------
 | Returns the result of adding the absolute values of the quadruple-precision
 | floating-point values `a' and `b'.  If `zSign' is 1, the sum is negated
 | before being returned.  `zSign' is ignored if the result is a NaN.
 | The addition is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static float128 addFloat128Sigs(float128 a, float128 b, flag zSign STATUS_PARAM)
{
    int32 aExp, bExp, zExp;
    uint64_t aSig0, aSig1, bSig0, bSig1, zSig0, zSig1, zSig2;
    int32 expDiff;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    bSig1 = extractFloat128Frac1(b);
    bSig0 = extractFloat128Frac0(b);
    bExp = extractFloat128Exp(b);
    expDiff = aExp - bExp;
    if(0 < expDiff) {
        if(aExp == 0x7FFF) {
            if(aSig0 | aSig1) {
                return propagateFloat128NaN(a, b STATUS_VAR);
            }
            return a;
        }
        if(bExp == 0) {
            --expDiff;
        } else {
            bSig0 |= LIT64(0x0001000000000000);
        }
        shift128ExtraRightJamming(bSig0, bSig1, 0, expDiff, &bSig0, &bSig1, &zSig2);
        zExp = aExp;
    } else if(expDiff < 0) {
        if(bExp == 0x7FFF) {
            if(bSig0 | bSig1) {
                return propagateFloat128NaN(a, b STATUS_VAR);
            }
            return packFloat128(zSign, 0x7FFF, 0, 0);
        }
        if(aExp == 0) {
            ++expDiff;
        } else {
            aSig0 |= LIT64(0x0001000000000000);
        }
        shift128ExtraRightJamming(aSig0, aSig1, 0, -expDiff, &aSig0, &aSig1, &zSig2);
        zExp = bExp;
    } else {
        if(aExp == 0x7FFF) {
            if(aSig0 | aSig1 | bSig0 | bSig1) {
                return propagateFloat128NaN(a, b STATUS_VAR);
            }
            return a;
        }
        add128(aSig0, aSig1, bSig0, bSig1, &zSig0, &zSig1);
        if(aExp == 0) {
            if(STATUS(flush_to_zero)) {
                if(zSig0 | zSig1) {
                    float_raise((int8)float_flag_output_denormal STATUS_VAR);
                }
                return packFloat128(zSign, 0, 0, 0);
            }
            return packFloat128(zSign, 0, zSig0, zSig1);
        }
        zSig2 = 0;
        zSig0 |= LIT64(0x0002000000000000);
        zExp = aExp;
        goto shiftRight1;
    }
    aSig0 |= LIT64(0x0001000000000000);
    add128(aSig0, aSig1, bSig0, bSig1, &zSig0, &zSig1);
    --zExp;
    if(zSig0 < LIT64(0x0002000000000000)) {
        goto roundAndPack;
    }
    ++zExp;
shiftRight1:
    shift128ExtraRightJamming(zSig0, zSig1, zSig2, 1, &zSig0, &zSig1, &zSig2);
roundAndPack:
    return roundAndPackFloat128(zSign, zExp, zSig0, zSig1, zSig2 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of subtracting the absolute values of the quadruple-
 | precision floating-point values `a' and `b'.  If `zSign' is 1, the
 | difference is negated before being returned.  `zSign' is ignored if the
 | result is a NaN.  The subtraction is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

static float128 subFloat128Sigs(float128 a, float128 b, flag zSign STATUS_PARAM)
{
    int32 aExp, bExp, zExp;
    uint64_t aSig0, aSig1, bSig0, bSig1, zSig0, zSig1;
    int32 expDiff;
    float128 z;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    bSig1 = extractFloat128Frac1(b);
    bSig0 = extractFloat128Frac0(b);
    bExp = extractFloat128Exp(b);
    expDiff = aExp - bExp;
    shortShift128Left(aSig0, aSig1, 14, &aSig0, &aSig1);
    shortShift128Left(bSig0, bSig1, 14, &bSig0, &bSig1);
    if(0 < expDiff) {
        goto aExpBigger;
    }
    if(expDiff < 0) {
        goto bExpBigger;
    }
    if(aExp == 0x7FFF) {
        if(aSig0 | aSig1 | bSig0 | bSig1) {
            return propagateFloat128NaN(a, b STATUS_VAR);
        }
        float_raise(float_flag_invalid STATUS_VAR);
        z.low = float128_default_nan_low;
        z.high = float128_default_nan_high;
        return z;
    }
    if(aExp == 0) {
        aExp = 1;
        bExp = 1;
    }
    if(bSig0 < aSig0) {
        goto aBigger;
    }
    if(aSig0 < bSig0) {
        goto bBigger;
    }
    if(bSig1 < aSig1) {
        goto aBigger;
    }
    if(aSig1 < bSig1) {
        goto bBigger;
    }
    return packFloat128(STATUS(float_rounding_mode) == float_round_down, 0, 0, 0);
bExpBigger:
    if(bExp == 0x7FFF) {
        if(bSig0 | bSig1) {
            return propagateFloat128NaN(a, b STATUS_VAR);
        }
        return packFloat128(zSign ^ 1, 0x7FFF, 0, 0);
    }
    if(aExp == 0) {
        ++expDiff;
    } else {
        aSig0 |= LIT64(0x4000000000000000);
    }
    shift128RightJamming(aSig0, aSig1, -expDiff, &aSig0, &aSig1);
    bSig0 |= LIT64(0x4000000000000000);
bBigger:
    sub128(bSig0, bSig1, aSig0, aSig1, &zSig0, &zSig1);
    zExp = bExp;
    zSign ^= 1;
    goto normalizeRoundAndPack;
aExpBigger:
    if(aExp == 0x7FFF) {
        if(aSig0 | aSig1) {
            return propagateFloat128NaN(a, b STATUS_VAR);
        }
        return a;
    }
    if(bExp == 0) {
        --expDiff;
    } else {
        bSig0 |= LIT64(0x4000000000000000);
    }
    shift128RightJamming(bSig0, bSig1, expDiff, &bSig0, &bSig1);
    aSig0 |= LIT64(0x4000000000000000);
aBigger:
    sub128(aSig0, aSig1, bSig0, bSig1, &zSig0, &zSig1);
    zExp = aExp;
normalizeRoundAndPack:
    --zExp;
    return normalizeRoundAndPackFloat128(zSign, zExp - 14, zSig0, zSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of adding the quadruple-precision floating-point values
 | `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
 | for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 float128_add(float128 a, float128 b STATUS_PARAM)
{
    flag aSign, bSign;

    aSign = extractFloat128Sign(a);
    bSign = extractFloat128Sign(b);
    if(aSign == bSign) {
        return addFloat128Sigs(a, b, aSign STATUS_VAR);
    } else {
        return subFloat128Sigs(a, b, aSign STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns the result of subtracting the quadruple-precision floating-point
 | values `a' and `b'.  The operation is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 float128_sub(float128 a, float128 b STATUS_PARAM)
{
    flag aSign, bSign;

    aSign = extractFloat128Sign(a);
    bSign = extractFloat128Sign(b);
    if(aSign == bSign) {
        return subFloat128Sigs(a, b, aSign STATUS_VAR);
    } else {
        return addFloat128Sigs(a, b, aSign STATUS_VAR);
    }
}

/*----------------------------------------------------------------------------
 | Returns the result of multiplying the quadruple-precision floating-point
 | values `a' and `b'.  The operation is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 float128_mul(float128 a, float128 b STATUS_PARAM)
{
    flag aSign, bSign, zSign;
    int32 aExp, bExp, zExp;
    uint64_t aSig0, aSig1, bSig0, bSig1, zSig0, zSig1, zSig2, zSig3;
    float128 z;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    bSig1 = extractFloat128Frac1(b);
    bSig0 = extractFloat128Frac0(b);
    bExp = extractFloat128Exp(b);
    bSign = extractFloat128Sign(b);
    zSign = aSign ^ bSign;
    if(aExp == 0x7FFF) {
        if((aSig0 | aSig1) || ((bExp == 0x7FFF) && (bSig0 | bSig1))) {
            return propagateFloat128NaN(a, b STATUS_VAR);
        }
        if((bExp | bSig0 | bSig1) == 0) {
            goto invalid;
        }
        return packFloat128(zSign, 0x7FFF, 0, 0);
    }
    if(bExp == 0x7FFF) {
        if(bSig0 | bSig1) {
            return propagateFloat128NaN(a, b STATUS_VAR);
        }
        if((aExp | aSig0 | aSig1) == 0) {
        invalid:
            float_raise(float_flag_invalid STATUS_VAR);
            z.low = float128_default_nan_low;
            z.high = float128_default_nan_high;
            return z;
        }
        return packFloat128(zSign, 0x7FFF, 0, 0);
    }
    if(aExp == 0) {
        if((aSig0 | aSig1) == 0) {
            return packFloat128(zSign, 0, 0, 0);
        }
        normalizeFloat128Subnormal(aSig0, aSig1, &aExp, &aSig0, &aSig1);
    }
    if(bExp == 0) {
        if((bSig0 | bSig1) == 0) {
            return packFloat128(zSign, 0, 0, 0);
        }
        normalizeFloat128Subnormal(bSig0, bSig1, &bExp, &bSig0, &bSig1);
    }
    zExp = aExp + bExp - 0x4000;
    aSig0 |= LIT64(0x0001000000000000);
    shortShift128Left(bSig0, bSig1, 16, &bSig0, &bSig1);
    mul128To256(aSig0, aSig1, bSig0, bSig1, &zSig0, &zSig1, &zSig2, &zSig3);
    add128(zSig0, zSig1, aSig0, aSig1, &zSig0, &zSig1);
    zSig2 |= (zSig3 != 0);
    if(LIT64(0x0002000000000000) <= zSig0) {
        shift128ExtraRightJamming(zSig0, zSig1, zSig2, 1, &zSig0, &zSig1, &zSig2);
        ++zExp;
    }
    return roundAndPackFloat128(zSign, zExp, zSig0, zSig1, zSig2 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the result of dividing the quadruple-precision floating-point value
 | `a' by the corresponding value `b'.  The operation is performed according to
 | the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 float128_div(float128 a, float128 b STATUS_PARAM)
{
    flag aSign, bSign, zSign;
    int32 aExp, bExp, zExp;
    uint64_t aSig0, aSig1, bSig0, bSig1, zSig0, zSig1, zSig2;
    uint64_t rem0, rem1, rem2, rem3, term0, term1, term2, term3;
    float128 z;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    bSig1 = extractFloat128Frac1(b);
    bSig0 = extractFloat128Frac0(b);
    bExp = extractFloat128Exp(b);
    bSign = extractFloat128Sign(b);
    zSign = aSign ^ bSign;
    if(aExp == 0x7FFF) {
        if(aSig0 | aSig1) {
            return propagateFloat128NaN(a, b STATUS_VAR);
        }
        if(bExp == 0x7FFF) {
            if(bSig0 | bSig1) {
                return propagateFloat128NaN(a, b STATUS_VAR);
            }
            goto invalid;
        }
        return packFloat128(zSign, 0x7FFF, 0, 0);
    }
    if(bExp == 0x7FFF) {
        if(bSig0 | bSig1) {
            return propagateFloat128NaN(a, b STATUS_VAR);
        }
        return packFloat128(zSign, 0, 0, 0);
    }
    if(bExp == 0) {
        if((bSig0 | bSig1) == 0) {
            if((aExp | aSig0 | aSig1) == 0) {
            invalid:
                float_raise(float_flag_invalid STATUS_VAR);
                z.low = float128_default_nan_low;
                z.high = float128_default_nan_high;
                return z;
            }
            float_raise(float_flag_divbyzero STATUS_VAR);
            return packFloat128(zSign, 0x7FFF, 0, 0);
        }
        normalizeFloat128Subnormal(bSig0, bSig1, &bExp, &bSig0, &bSig1);
    }
    if(aExp == 0) {
        if((aSig0 | aSig1) == 0) {
            return packFloat128(zSign, 0, 0, 0);
        }
        normalizeFloat128Subnormal(aSig0, aSig1, &aExp, &aSig0, &aSig1);
    }
    zExp = aExp - bExp + 0x3FFD;
    shortShift128Left(aSig0 | LIT64(0x0001000000000000), aSig1, 15, &aSig0, &aSig1);
    shortShift128Left(bSig0 | LIT64(0x0001000000000000), bSig1, 15, &bSig0, &bSig1);
    if(le128(bSig0, bSig1, aSig0, aSig1)) {
        shift128Right(aSig0, aSig1, 1, &aSig0, &aSig1);
        ++zExp;
    }
    zSig0 = estimateDiv128To64(aSig0, aSig1, bSig0);
    mul128By64To192(bSig0, bSig1, zSig0, &term0, &term1, &term2);
    sub192(aSig0, aSig1, 0, term0, term1, term2, &rem0, &rem1, &rem2);
    while((int64_t)rem0 < 0) {
        --zSig0;
        add192(rem0, rem1, rem2, 0, bSig0, bSig1, &rem0, &rem1, &rem2);
    }
    zSig1 = estimateDiv128To64(rem1, rem2, bSig0);
    if((zSig1 & 0x3FFF) <= 4) {
        mul128By64To192(bSig0, bSig1, zSig1, &term1, &term2, &term3);
        sub192(rem1, rem2, 0, term1, term2, term3, &rem1, &rem2, &rem3);
        while((int64_t)rem1 < 0) {
            --zSig1;
            add192(rem1, rem2, rem3, 0, bSig0, bSig1, &rem1, &rem2, &rem3);
        }
        zSig1 |= ((rem1 | rem2 | rem3) != 0);
    }
    shift128ExtraRightJamming(zSig0, zSig1, 0, 15, &zSig0, &zSig1, &zSig2);
    return roundAndPackFloat128(zSign, zExp, zSig0, zSig1, zSig2 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the remainder of the quadruple-precision floating-point value `a'
 | with respect to the corresponding value `b'.  The operation is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 float128_rem(float128 a, float128 b STATUS_PARAM)
{
    flag aSign, zSign;
    int32 aExp, bExp, expDiff;
    uint64_t aSig0, aSig1, bSig0, bSig1, q, term0, term1, term2;
    uint64_t allZero, alternateASig0, alternateASig1, sigMean1;
    int64_t sigMean0;
    float128 z;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    bSig1 = extractFloat128Frac1(b);
    bSig0 = extractFloat128Frac0(b);
    bExp = extractFloat128Exp(b);
    if(aExp == 0x7FFF) {
        if((aSig0 | aSig1) || ((bExp == 0x7FFF) && (bSig0 | bSig1))) {
            return propagateFloat128NaN(a, b STATUS_VAR);
        }
        goto invalid;
    }
    if(bExp == 0x7FFF) {
        if(bSig0 | bSig1) {
            return propagateFloat128NaN(a, b STATUS_VAR);
        }
        return a;
    }
    if(bExp == 0) {
        if((bSig0 | bSig1) == 0) {
        invalid:
            float_raise(float_flag_invalid STATUS_VAR);
            z.low = float128_default_nan_low;
            z.high = float128_default_nan_high;
            return z;
        }
        normalizeFloat128Subnormal(bSig0, bSig1, &bExp, &bSig0, &bSig1);
    }
    if(aExp == 0) {
        if((aSig0 | aSig1) == 0) {
            return a;
        }
        normalizeFloat128Subnormal(aSig0, aSig1, &aExp, &aSig0, &aSig1);
    }
    expDiff = aExp - bExp;
    if(expDiff < -1) {
        return a;
    }
    shortShift128Left(aSig0 | LIT64(0x0001000000000000), aSig1, 15 - (expDiff < 0), &aSig0, &aSig1);
    shortShift128Left(bSig0 | LIT64(0x0001000000000000), bSig1, 15, &bSig0, &bSig1);
    q = le128(bSig0, bSig1, aSig0, aSig1);
    if(q) {
        sub128(aSig0, aSig1, bSig0, bSig1, &aSig0, &aSig1);
    }
    expDiff -= 64;
    while(0 < expDiff) {
        q = estimateDiv128To64(aSig0, aSig1, bSig0);
        q = (4 < q) ? q - 4 : 0;
        mul128By64To192(bSig0, bSig1, q, &term0, &term1, &term2);
        shortShift192Left(term0, term1, term2, 61, &term1, &term2, &allZero);
        shortShift128Left(aSig0, aSig1, 61, &aSig0, &allZero);
        sub128(aSig0, 0, term1, term2, &aSig0, &aSig1);
        expDiff -= 61;
    }
    if(-64 < expDiff) {
        q = estimateDiv128To64(aSig0, aSig1, bSig0);
        q = (4 < q) ? q - 4 : 0;
        q >>= -expDiff;
        shift128Right(bSig0, bSig1, 12, &bSig0, &bSig1);
        expDiff += 52;
        if(expDiff < 0) {
            shift128Right(aSig0, aSig1, -expDiff, &aSig0, &aSig1);
        } else {
            shortShift128Left(aSig0, aSig1, expDiff, &aSig0, &aSig1);
        }
        mul128By64To192(bSig0, bSig1, q, &term0, &term1, &term2);
        sub128(aSig0, aSig1, term1, term2, &aSig0, &aSig1);
    } else {
        shift128Right(aSig0, aSig1, 12, &aSig0, &aSig1);
        shift128Right(bSig0, bSig1, 12, &bSig0, &bSig1);
    }
    do {
        alternateASig0 = aSig0;
        alternateASig1 = aSig1;
        ++q;
        sub128(aSig0, aSig1, bSig0, bSig1, &aSig0, &aSig1);
    } while(0 <= (int64_t)aSig0);
    add128(aSig0, aSig1, alternateASig0, alternateASig1, (uint64_t *)&sigMean0, &sigMean1);
    if((sigMean0 < 0) || (((sigMean0 | sigMean1) == 0) && (q & 1))) {
        aSig0 = alternateASig0;
        aSig1 = alternateASig1;
    }
    zSign = ((int64_t)aSig0 < 0);
    if(zSign) {
        sub128(0, 0, aSig0, aSig1, &aSig0, &aSig1);
    }
    return normalizeRoundAndPackFloat128(aSign ^ zSign, bExp - 4, aSig0, aSig1 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns the square root of the quadruple-precision floating-point value `a'.
 | The operation is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

float128 float128_sqrt(float128 a STATUS_PARAM)
{
    flag aSign;
    int32 aExp, zExp;
    uint64_t aSig0, aSig1, zSig0, zSig1, zSig2, doubleZSig0;
    uint64_t rem0, rem1, rem2, rem3, term0, term1, term2, term3;
    float128 z;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    if(aExp == 0x7FFF) {
        if(aSig0 | aSig1) {
            return propagateFloat128NaN(a, a STATUS_VAR);
        }
        if(!aSign) {
            return a;
        }
        goto invalid;
    }
    if(aSign) {
        if((aExp | aSig0 | aSig1) == 0) {
            return a;
        }
    invalid:
        float_raise(float_flag_invalid STATUS_VAR);
        z.low = float128_default_nan_low;
        z.high = float128_default_nan_high;
        return z;
    }
    if(aExp == 0) {
        if((aSig0 | aSig1) == 0) {
            return packFloat128(0, 0, 0, 0);
        }
        normalizeFloat128Subnormal(aSig0, aSig1, &aExp, &aSig0, &aSig1);
    }
    zExp = ((aExp - 0x3FFF) >> 1) + 0x3FFE;
    aSig0 |= LIT64(0x0001000000000000);
    zSig0 = estimateSqrt32(aExp, aSig0 >> 17);
    shortShift128Left(aSig0, aSig1, 13 - (aExp & 1), &aSig0, &aSig1);
    zSig0 = estimateDiv128To64(aSig0, aSig1, zSig0 << 32) + (zSig0 << 30);
    doubleZSig0 = zSig0 << 1;
    mul64To128(zSig0, zSig0, &term0, &term1);
    sub128(aSig0, aSig1, term0, term1, &rem0, &rem1);
    while((int64_t)rem0 < 0) {
        --zSig0;
        doubleZSig0 -= 2;
        add128(rem0, rem1, zSig0 >> 63, doubleZSig0 | 1, &rem0, &rem1);
    }
    zSig1 = estimateDiv128To64(rem1, 0, doubleZSig0);
    if((zSig1 & 0x1FFF) <= 5) {
        if(zSig1 == 0) {
            zSig1 = 1;
        }
        mul64To128(doubleZSig0, zSig1, &term1, &term2);
        sub128(rem1, 0, term1, term2, &rem1, &rem2);
        mul64To128(zSig1, zSig1, &term2, &term3);
        sub192(rem1, rem2, 0, 0, term2, term3, &rem1, &rem2, &rem3);
        while((int64_t)rem1 < 0) {
            --zSig1;
            shortShift128Left(0, zSig1, 1, &term2, &term3);
            term3 |= 1;
            term2 |= doubleZSig0;
            add192(rem1, rem2, rem3, 0, term2, term3, &rem1, &rem2, &rem3);
        }
        zSig1 |= ((rem1 | rem2 | rem3) != 0);
    }
    shift128ExtraRightJamming(zSig0, zSig1, 0, 14, &zSig0, &zSig1, &zSig2);
    return roundAndPackFloat128(0, zExp, zSig0, zSig1, zSig2 STATUS_VAR);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point value `a' is equal to
 | the corresponding value `b', and 0 otherwise.  The invalid exception is
 | raised if either operand is a NaN.  Otherwise, the comparison is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float128_eq(float128 a, float128 b STATUS_PARAM)
{

    if(((extractFloat128Exp(a) == 0x7FFF) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) ||
       ((extractFloat128Exp(b) == 0x7FFF) && (extractFloat128Frac0(b) | extractFloat128Frac1(b)))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    return (a.low == b.low) && ((a.high == b.high) || ((a.low == 0) && ((uint64_t)((a.high | b.high) << 1) == 0)));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point value `a' is less than
 | or equal to the corresponding value `b', and 0 otherwise.  The invalid
 | exception is raised if either operand is a NaN.  The comparison is performed
 | according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float128_le(float128 a, float128 b STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloat128Exp(a) == 0x7FFF) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) ||
       ((extractFloat128Exp(b) == 0x7FFF) && (extractFloat128Frac0(b) | extractFloat128Frac1(b)))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    aSign = extractFloat128Sign(a);
    bSign = extractFloat128Sign(b);
    if(aSign != bSign) {
        return aSign || ((((uint64_t)((a.high | b.high) << 1)) | a.low | b.low) == 0);
    }
    return aSign ? le128(b.high, b.low, a.high, a.low) : le128(a.high, a.low, b.high, b.low);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point value `a' is less than
 | the corresponding value `b', and 0 otherwise.  The invalid exception is
 | raised if either operand is a NaN.  The comparison is performed according
 | to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float128_lt(float128 a, float128 b STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloat128Exp(a) == 0x7FFF) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) ||
       ((extractFloat128Exp(b) == 0x7FFF) && (extractFloat128Frac0(b) | extractFloat128Frac1(b)))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 0;
    }
    aSign = extractFloat128Sign(a);
    bSign = extractFloat128Sign(b);
    if(aSign != bSign) {
        return aSign && ((((uint64_t)((a.high | b.high) << 1)) | a.low | b.low) != 0);
    }
    return aSign ? lt128(b.high, b.low, a.high, a.low) : lt128(a.high, a.low, b.high, b.low);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point values `a' and `b' cannot
 | be compared, and 0 otherwise.  The invalid exception is raised if either
 | operand is a NaN. The comparison is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float128_unordered(float128 a, float128 b STATUS_PARAM)
{
    if(((extractFloat128Exp(a) == 0x7FFF) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) ||
       ((extractFloat128Exp(b) == 0x7FFF) && (extractFloat128Frac0(b) | extractFloat128Frac1(b)))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return 1;
    }
    return 0;
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point value `a' is equal to
 | the corresponding value `b', and 0 otherwise.  Quiet NaNs do not cause an
 | exception.  The comparison is performed according to the IEC/IEEE Standard
 | for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float128_eq_quiet(float128 a, float128 b STATUS_PARAM)
{

    if(((extractFloat128Exp(a) == 0x7FFF) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) ||
       ((extractFloat128Exp(b) == 0x7FFF) && (extractFloat128Frac0(b) | extractFloat128Frac1(b)))) {
        if(float128_is_signaling_nan(a STATUS_VAR) || float128_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    return (a.low == b.low) && ((a.high == b.high) || ((a.low == 0) && ((uint64_t)((a.high | b.high) << 1) == 0)));
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point value `a' is less than
 | or equal to the corresponding value `b', and 0 otherwise.  Quiet NaNs do not
 | cause an exception.  Otherwise, the comparison is performed according to the
 | IEC/IEEE Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float128_le_quiet(float128 a, float128 b STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloat128Exp(a) == 0x7FFF) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) ||
       ((extractFloat128Exp(b) == 0x7FFF) && (extractFloat128Frac0(b) | extractFloat128Frac1(b)))) {
        if(float128_is_signaling_nan(a STATUS_VAR) || float128_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    aSign = extractFloat128Sign(a);
    bSign = extractFloat128Sign(b);
    if(aSign != bSign) {
        return aSign || ((((uint64_t)((a.high | b.high) << 1)) | a.low | b.low) == 0);
    }
    return aSign ? le128(b.high, b.low, a.high, a.low) : le128(a.high, a.low, b.high, b.low);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point value `a' is less than
 | the corresponding value `b', and 0 otherwise.  Quiet NaNs do not cause an
 | exception.  Otherwise, the comparison is performed according to the IEC/IEEE
 | Standard for Binary Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float128_lt_quiet(float128 a, float128 b STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloat128Exp(a) == 0x7FFF) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) ||
       ((extractFloat128Exp(b) == 0x7FFF) && (extractFloat128Frac0(b) | extractFloat128Frac1(b)))) {
        if(float128_is_signaling_nan(a STATUS_VAR) || float128_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 0;
    }
    aSign = extractFloat128Sign(a);
    bSign = extractFloat128Sign(b);
    if(aSign != bSign) {
        return aSign && ((((uint64_t)((a.high | b.high) << 1)) | a.low | b.low) != 0);
    }
    return aSign ? lt128(b.high, b.low, a.high, a.low) : lt128(a.high, a.low, b.high, b.low);
}

/*----------------------------------------------------------------------------
 | Returns 1 if the quadruple-precision floating-point values `a' and `b' cannot
 | be compared, and 0 otherwise.  Quiet NaNs do not cause an exception.  The
 | comparison is performed according to the IEC/IEEE Standard for Binary
 | Floating-Point Arithmetic.
 *----------------------------------------------------------------------------*/

int float128_unordered_quiet(float128 a, float128 b STATUS_PARAM)
{
    if(((extractFloat128Exp(a) == 0x7FFF) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) ||
       ((extractFloat128Exp(b) == 0x7FFF) && (extractFloat128Frac0(b) | extractFloat128Frac1(b)))) {
        if(float128_is_signaling_nan(a STATUS_VAR) || float128_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return 1;
    }
    return 0;
}

/* misc functions */
float32 uint32_to_float32(uint32 a STATUS_PARAM)
{
    return int64_to_float32(a STATUS_VAR);
}

float64 uint32_to_float64(uint32 a STATUS_PARAM)
{
    return int64_to_float64(a STATUS_VAR);
}

uint32 float32_to_uint32(float32 a STATUS_PARAM)
{
    int64_t v;
    uint32 res;

    v = float32_to_int64(a STATUS_VAR);
    if(v < 0) {
        res = 0;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > 0xffffffff) {
        res = 0xffffffff;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

uint32 float32_to_uint32_round_to_zero(float32 a STATUS_PARAM)
{
    int64_t v;
    uint32 res;

    v = float32_to_int64_round_to_zero(a STATUS_VAR);
    if(v < 0) {
        res = 0;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > 0xffffffff) {
        res = 0xffffffff;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

uint16 float32_to_uint16(float32 a STATUS_PARAM)
{
    int64_t v;
    uint16 res;

    v = float32_to_int64(a STATUS_VAR);
    if(v < 0) {
        res = 0;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > UINT16_MAX) {
        res = UINT16_MAX;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

uint16 float32_to_uint16_round_to_zero(float32 a STATUS_PARAM)
{
    int64_t v;
    uint16 res;

    v = float32_to_int64_round_to_zero(a STATUS_VAR);
    if(v < 0) {
        res = 0;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > 0xffff) {
        res = 0xffff;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

uint32 float64_to_uint32(float64 a STATUS_PARAM)
{
    int64_t v;
    uint32 res;

    v = float64_to_int64(a STATUS_VAR);
    if(v < 0) {
        res = 0;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > 0xffffffff) {
        res = 0xffffffff;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

uint32 float64_to_uint32_round_to_zero(float64 a STATUS_PARAM)
{
    int64_t v;
    uint32 res;

    v = float64_to_int64_round_to_zero(a STATUS_VAR);
    if(v < 0) {
        res = 0;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > 0xffffffff) {
        res = 0xffffffff;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

uint16 float64_to_uint16_round_to_zero(float64 a STATUS_PARAM)
{
    int64_t v;
    uint16 res;

    v = float64_to_int64_round_to_zero(a STATUS_VAR);
    if(v < 0) {
        res = 0;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > 0xffff) {
        res = 0xffff;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

uint64_t float64_to_uint64(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint64_t aSig, aSigExtra;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp) {
        aSig |= LIT64(0x0010000000000000);
    }
    shiftCount = 0x433 - aExp;
    if(shiftCount <= 0) {
        if(shiftCount < -11) {
            goto invalid;
        }
        if(0x43E < aExp) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0x7FF) && (aSig != LIT64(0x0010000000000000)))) {
                return LIT64(0x7FFFFFFFFFFFFFFF);
            }
            return (int64_t)LIT64(0x8000000000000000);
        }
        aSigExtra = 0;
        aSig <<= -shiftCount;
    } else {
        shift64ExtraRightJamming(aSig, 0, shiftCount, &aSig, &aSigExtra);
    }
    return roundAndPackUint64(aSign, aSig, aSigExtra STATUS_VAR);
invalid:
    return (aExp == 0x7FF) && extractFloat64Frac(a) ? LIT64(0xFFFFFFFFFFFFFFFF) : aSign ? 0 : LIT64(0xFFFFFFFFFFFFFFFF);
}

uint64_t float64_to_uint64_round_to_zero(float64 a STATUS_PARAM)
{
    int64_t v;

    v = float64_val(int64_to_float64(INT64_MIN STATUS_VAR));
    v += float64_val(a);
    v = float64_to_int64_round_to_zero(make_float64(v) STATUS_VAR);

    return v - INT64_MIN;
}

uint64_t float32_to_uint64(float32 a, float_status *status)
{
    flag aSign;
    int aExp;
    int shiftCount;
    uint32_t aSig;
    uint64_t aSig64, aSigExtra;
    a = float32_squash_input_denormal(a, status);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    if((aSign) && (aExp > 126)) {
        float_raise(float_flag_invalid, status);
        if(float32_is_any_nan(a)) {
            return LIT64(0xFFFFFFFFFFFFFFFF);
        } else {
            return 0;
        }
    }
    shiftCount = 0xBE - aExp;
    if(aExp) {
        aSig |= 0x00800000;
    }
    if(shiftCount < 0) {
        float_raise(float_flag_invalid, status);
        return LIT64(0xFFFFFFFFFFFFFFFF);
    }

    aSig64 = aSig;
    aSig64 <<= 40;
    shift64ExtraRightJamming(aSig64, 0, shiftCount, &aSig64, &aSigExtra);
    return roundAndPackUint64(aSign, aSig64, aSigExtra, status);
}

#define COMPARE(s, nan_exp)                                                                                           \
    INLINE int float##s##_compare_internal(float##s a, float##s b, int is_quiet STATUS_PARAM)                         \
    {                                                                                                                 \
        flag aSign, bSign;                                                                                            \
        uint##s##_t av, bv;                                                                                           \
        a = float##s##_squash_input_denormal(a STATUS_VAR);                                                           \
        b = float##s##_squash_input_denormal(b STATUS_VAR);                                                           \
                                                                                                                      \
        if(((extractFloat##s##Exp(a) == nan_exp) && extractFloat##s##Frac(a)) ||                                      \
           ((extractFloat##s##Exp(b) == nan_exp) && extractFloat##s##Frac(b))) {                                      \
            if(!is_quiet || float##s##_is_signaling_nan(a STATUS_VAR) || float##s##_is_signaling_nan(b STATUS_VAR)) { \
                float_raise(float_flag_invalid STATUS_VAR);                                                           \
            }                                                                                                         \
            return float_relation_unordered;                                                                          \
        }                                                                                                             \
        aSign = extractFloat##s##Sign(a);                                                                             \
        bSign = extractFloat##s##Sign(b);                                                                             \
        av = float##s##_val(a);                                                                                       \
        bv = float##s##_val(b);                                                                                       \
        if(aSign != bSign) {                                                                                          \
            if((uint##s##_t)((av | bv) << 1) == 0) {                                                                  \
                /* zero case */                                                                                       \
                return float_relation_equal;                                                                          \
            } else {                                                                                                  \
                return 1 - (2 * aSign);                                                                               \
            }                                                                                                         \
        } else {                                                                                                      \
            if(av == bv) {                                                                                            \
                return float_relation_equal;                                                                          \
            } else {                                                                                                  \
                return 1 - 2 * (aSign ^ (av < bv));                                                                   \
            }                                                                                                         \
        }                                                                                                             \
    }                                                                                                                 \
                                                                                                                      \
    int float##s##_compare(float##s a, float##s b STATUS_PARAM)                                                       \
    {                                                                                                                 \
        return float##s##_compare_internal(a, b, 0 STATUS_VAR);                                                       \
    }                                                                                                                 \
                                                                                                                      \
    int float##s##_compare_quiet(float##s a, float##s b STATUS_PARAM)                                                 \
    {                                                                                                                 \
        return float##s##_compare_internal(a, b, 1 STATUS_VAR);                                                       \
    }

COMPARE(32, 0xff)
COMPARE(64, 0x7ff)

INLINE int floatx80_compare_internal(floatx80 a, floatx80 b, int is_quiet STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloatx80Exp(a) == 0x7fff) && (extractFloatx80Frac(a) << 1)) ||
       ((extractFloatx80Exp(b) == 0x7fff) && (extractFloatx80Frac(b) << 1))) {
        if(!is_quiet || floatx80_is_signaling_nan(a STATUS_VAR) || floatx80_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return float_relation_unordered;
    }
    aSign = extractFloatx80Sign(a);
    bSign = extractFloatx80Sign(b);
    if(aSign != bSign) {

        if((((uint16_t)((a.high | b.high) << 1)) == 0) && ((a.low | b.low) == 0)) {
            /* zero case */
            return float_relation_equal;
        } else {
            return 1 - (2 * aSign);
        }
    } else {
        if(a.low == b.low && a.high == b.high) {
            return float_relation_equal;
        } else {
            return 1 - 2 * (aSign ^ (lt128(a.high, a.low, b.high, b.low)));
        }
    }
}

int floatx80_compare(floatx80 a, floatx80 b STATUS_PARAM)
{
    return floatx80_compare_internal(a, b, 0 STATUS_VAR);
}

int floatx80_compare_quiet(floatx80 a, floatx80 b STATUS_PARAM)
{
    return floatx80_compare_internal(a, b, 1 STATUS_VAR);
}

INLINE int float128_compare_internal(float128 a, float128 b, int is_quiet STATUS_PARAM)
{
    flag aSign, bSign;

    if(((extractFloat128Exp(a) == 0x7fff) && (extractFloat128Frac0(a) | extractFloat128Frac1(a))) ||
       ((extractFloat128Exp(b) == 0x7fff) && (extractFloat128Frac0(b) | extractFloat128Frac1(b)))) {
        if(!is_quiet || float128_is_signaling_nan(a STATUS_VAR) || float128_is_signaling_nan(b STATUS_VAR)) {
            float_raise(float_flag_invalid STATUS_VAR);
        }
        return float_relation_unordered;
    }
    aSign = extractFloat128Sign(a);
    bSign = extractFloat128Sign(b);
    if(aSign != bSign) {
        if((((a.high | b.high) << 1) | a.low | b.low) == 0) {
            /* zero case */
            return float_relation_equal;
        } else {
            return 1 - (2 * aSign);
        }
    } else {
        if(a.low == b.low && a.high == b.high) {
            return float_relation_equal;
        } else {
            return 1 - 2 * (aSign ^ (lt128(a.high, a.low, b.high, b.low)));
        }
    }
}

int float128_compare(float128 a, float128 b STATUS_PARAM)
{
    return float128_compare_internal(a, b, 0 STATUS_VAR);
}

int float128_compare_quiet(float128 a, float128 b STATUS_PARAM)
{
    return float128_compare_internal(a, b, 1 STATUS_VAR);
}

/* min() and max() functions. These can't be implemented as
 * 'compare and pick one input' because that would mishandle
 * NaNs and +0 vs -0.
 */
#define MINMAX(s, nan_exp)                                                              \
    INLINE float##s float##s##_minmax(float##s a, float##s b, int ismin STATUS_PARAM)   \
    {                                                                                   \
        flag aSign, bSign;                                                              \
        uint##s##_t av, bv;                                                             \
        a = float##s##_squash_input_denormal(a STATUS_VAR);                             \
        b = float##s##_squash_input_denormal(b STATUS_VAR);                             \
        if(float##s##_is_any_nan(a) && float##s##_is_any_nan(b)) {                      \
            return propagateFloat##s##NaN(a, b STATUS_VAR);                             \
        }                                                                               \
        aSign = extractFloat##s##Sign(a);                                               \
        bSign = extractFloat##s##Sign(b);                                               \
        av = float##s##_val(a);                                                         \
        bv = float##s##_val(b);                                                         \
        if(float##s##_is_any_nan(a)) {                                                  \
            return b;                                                                   \
        } else if(float##s##_is_any_nan(b)) {                                           \
            return a;                                                                   \
        } else {                                                                        \
            if(aSign != bSign) {                                                        \
                return ismin ^ aSign ? b : a;                                           \
            } else {                                                                    \
                return ismin ^ aSign ^ (av < bv) ? b : a;                               \
            }                                                                           \
        }                                                                               \
    }                                                                                   \
                                                                                        \
    float##s float##s##_min(float##s a, float##s b STATUS_PARAM)                        \
    {                                                                                   \
        return float##s##_minmax(a, b, 1 STATUS_VAR);                                   \
    }                                                                                   \
                                                                                        \
    float##s float##s##_max(float##s a, float##s b STATUS_PARAM)                        \
    {                                                                                   \
        return float##s##_minmax(a, b, 0 STATUS_VAR);                                   \
    }                                                                                   \
                                                                                        \
    float##s float##s##_maxnum(float##s a, float##s b STATUS_PARAM)                     \
    {                                                                                   \
        if(float##s##_is_quiet_nan(a STATUS_VAR) && !float##s##_is_any_nan(b)) {        \
            return b;                                                                   \
        } else if(!float##s##_is_any_nan(a) && float##s##_is_quiet_nan(b STATUS_VAR)) { \
            return a;                                                                   \
        } else {                                                                        \
            return float##s##_max(a, b STATUS_VAR);                                     \
        }                                                                               \
    }                                                                                   \
                                                                                        \
    float##s float##s##_minnum(float##s a, float##s b STATUS_PARAM)                     \
    {                                                                                   \
        if(float##s##_is_quiet_nan(a STATUS_VAR) && !float##s##_is_any_nan(b)) {        \
            return b;                                                                   \
        } else if(!float##s##_is_any_nan(a) && float##s##_is_quiet_nan(b STATUS_VAR)) { \
            return a;                                                                   \
        } else {                                                                        \
            return float##s##_min(a, b STATUS_VAR);                                     \
        }                                                                               \
    }

MINMAX(32, 0xff)
MINMAX(64, 0x7ff)

/* Multiply A by 2 raised to the power N.  */
float32 float32_scalbn(float32 a, int n STATUS_PARAM)
{
    flag aSign;
    int16_t aExp;
    uint32_t aSig;

    a = float32_squash_input_denormal(a STATUS_VAR);
    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);

    if(aExp == 0xFF) {
        if(aSig) {
            return propagateFloat32NaN(a, a STATUS_VAR);
        }
        return a;
    }
    if(aExp != 0) {
        aSig |= 0x00800000;
    } else if(aSig == 0) {
        return a;
    }

    if(n > 0x200) {
        n = 0x200;
    } else if(n < -0x200) {
        n = -0x200;
    }

    aExp += n - 1;
    aSig <<= 7;
    return normalizeRoundAndPackFloat32(aSign, aExp, aSig STATUS_VAR);
}

float64 float64_scalbn(float64 a, int n STATUS_PARAM)
{
    flag aSign;
    int16_t aExp;
    uint64_t aSig;

    a = float64_squash_input_denormal(a STATUS_VAR);
    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);

    if(aExp == 0x7FF) {
        if(aSig) {
            return propagateFloat64NaN(a, a STATUS_VAR);
        }
        return a;
    }
    if(aExp != 0) {
        aSig |= LIT64(0x0010000000000000);
    } else if(aSig == 0) {
        return a;
    }

    if(n > 0x1000) {
        n = 0x1000;
    } else if(n < -0x1000) {
        n = -0x1000;
    }

    aExp += n - 1;
    aSig <<= 10;
    return normalizeRoundAndPackFloat64(aSign, aExp, aSig STATUS_VAR);
}

floatx80 floatx80_scalbn(floatx80 a, int n STATUS_PARAM)
{
    flag aSign;
    int32_t aExp;
    uint64_t aSig;

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);

    if(aExp == 0x7FFF) {
        if(aSig << 1) {
            return propagateFloatx80NaN(a, a STATUS_VAR);
        }
        return a;
    }

    if(aExp == 0 && aSig == 0) {
        return a;
    }

    if(n > 0x10000) {
        n = 0x10000;
    } else if(n < -0x10000) {
        n = -0x10000;
    }

    aExp += n;
    return normalizeRoundAndPackFloatx80(STATUS(floatx80_rounding_precision), aSign, aExp, aSig, 0 STATUS_VAR);
}

float128 float128_scalbn(float128 a, int n STATUS_PARAM)
{
    flag aSign;
    int32_t aExp;
    uint64_t aSig0, aSig1;

    aSig1 = extractFloat128Frac1(a);
    aSig0 = extractFloat128Frac0(a);
    aExp = extractFloat128Exp(a);
    aSign = extractFloat128Sign(a);
    if(aExp == 0x7FFF) {
        if(aSig0 | aSig1) {
            return propagateFloat128NaN(a, a STATUS_VAR);
        }
        return a;
    }
    if(aExp != 0) {
        aSig0 |= LIT64(0x0001000000000000);
    } else if(aSig0 == 0 && aSig1 == 0) {
        return a;
    }

    if(n > 0x10000) {
        n = 0x10000;
    } else if(n < -0x10000) {
        n = -0x10000;
    }

    aExp += n - 1;
    return normalizeRoundAndPackFloat128(aSign, aExp, aSig0, aSig1 STATUS_VAR);
}

//  Same as roundAndPackUInt64 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
static int64_t rodAndPackUint64(flag zSign, uint64_t absZ0, uint64_t absZ1, float_status *status)
{
    absZ0 |= absZ1 != 0;
    if(zSign && absZ0) {
        float_raise(float_flag_invalid, status);
        return 0;
    }

    if(absZ1) {
        status->float_exception_flags |= float_flag_inexact;
    }
    return absZ0;
}

//  Same as roundAndPackInt64 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
static uint64_t rodAndPackInt64(flag zSign, uint64_t absZ0, uint64_t absZ1 STATUS_PARAM)
{
    int64 z;

    absZ0 |= absZ1 != 0;
    z = absZ0;
    if(zSign) {
        z = -z;
    }
    if(z && ((z < 0) ^ zSign)) {
        float_raise(float_flag_invalid STATUS_VAR);
        return zSign ? (int64_t)LIT64(0x8000000000000000) : LIT64(0x7FFFFFFFFFFFFFFF);
    }
    if(absZ1) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

//  Same as roundAndPackInt32 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
static int32 rodAndPackInt32(flag zSign, uint64_t absZ STATUS_PARAM)
{
    int8 roundBits;
    int32 z;

    roundBits = absZ & 0x7F;
    absZ = absZ >> 7 | (roundBits != 0);
    z = absZ;
    if(zSign) {
        z = -z;
    }
    if((absZ >> 32) || (z && ((z < 0) ^ zSign))) {
        float_raise(float_flag_invalid STATUS_VAR);
        return zSign ? (int32_t)0x80000000 : 0x7FFFFFFF;
    }
    if(roundBits) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    return z;
}

//  Same as float64_to_uint64 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
uint64_t float64_to_uint64_rod(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint64_t aSig, aSigExtra;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp) {
        aSig |= LIT64(0x0010000000000000);
    }
    shiftCount = 0x433 - aExp;
    if(shiftCount <= 0) {
        if(shiftCount < -11) {
            goto invalid;
        }
        if(0x43E < aExp) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0x7FF) && (aSig != LIT64(0x0010000000000000)))) {
                return LIT64(0x7FFFFFFFFFFFFFFF);
            }
            return (int64_t)LIT64(0x8000000000000000);
        }
        aSigExtra = 0;
        aSig <<= -shiftCount;
    } else {
        shift64ExtraRightJamming(aSig, 0, shiftCount, &aSig, &aSigExtra);
    }
    return rodAndPackUint64(aSign, aSig, aSigExtra STATUS_VAR);
invalid:
    return (aExp == 0x7FF) && extractFloat64Frac(a) ? LIT64(0xFFFFFFFFFFFFFFFF) : aSign ? 0 : LIT64(0xFFFFFFFFFFFFFFFF);
}

//  Same as float64_to_int64 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
int64 float64_to_int64_rod(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint64_t aSig, aSigExtra;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp) {
        aSig |= LIT64(0x0010000000000000);
    }
    shiftCount = 0x433 - aExp;
    if(shiftCount <= 0) {
        if(shiftCount < -11) {
            goto invalid;
        }
        if(0x43E < aExp) {
            float_raise(float_flag_invalid STATUS_VAR);
            if(!aSign || ((aExp == 0x7FF) && (aSig != LIT64(0x0010000000000000)))) {
                return LIT64(0x7FFFFFFFFFFFFFFF);
            }
            return (int64_t)LIT64(0x8000000000000000);
        }
        aSigExtra = 0;
        aSig <<= -shiftCount;
    } else {
        shift64ExtraRightJamming(aSig, 0, shiftCount, &aSig, &aSigExtra);
    }
    return rodAndPackInt64(aSign, aSig, aSigExtra STATUS_VAR);
invalid:
    return (aExp == 0x7FF) && extractFloat64Frac(a) ? LIT64(0x7FFFFFFFFFFFFFFF)
           : aSign                                  ? (-LIT64(0x7FFFFFFFFFFFFFFF) - 1)
                                                    : LIT64(0x7FFFFFFFFFFFFFFF);
}

//  Same as roundAndPackFloat32 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
static float32 rodAndPackFloat32(flag zSign, int16 zExp, uint32_t zSig STATUS_PARAM)
{
    uint8 roundIncrement;
    int8 roundBits;
    flag isTiny;

    roundBits = zSig & 0x7F;
    roundIncrement = roundBits && !(zSig & 0x80) ? 0x80 : 0x0;
    if(0xFD <= (uint16_t)zExp) {
        if(0xFD < zExp) {
            float_raise(float_flag_overflow | float_flag_inexact STATUS_VAR);
            return packFloat32(zSign, 0xFF, -(roundIncrement == 0));
        }
        if(zExp < 0) {
            if(STATUS(flush_to_zero)) {
                float_raise((int8)float_flag_output_denormal STATUS_VAR);
                return packFloat32(zSign, 0, 0);
            }
            isTiny = (STATUS(float_detect_tininess) == float_tininess_before_rounding) || (zExp < -1) ||
                     (zSig + roundIncrement < 0x80000000);
            shift32RightJamming(zSig, -zExp, &zSig);
            zExp = 0;
            roundBits = zSig & 0x7F;
            if(isTiny && roundBits) {
                float_raise(float_flag_underflow STATUS_VAR);
            }
        }
    }
    if(roundBits) {
        STATUS(float_exception_flags) |= float_flag_inexact;
    }
    zSig = (zSig + roundIncrement) >> 7;
    if(zSig == 0) {
        zExp = 0;
    }
    return packFloat32(zSign, zExp, zSig);
}

//  Same as float64_to_uint32 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
uint32 float64_to_uint32_rod(float64 a STATUS_PARAM)
{
    int64_t v;
    uint32 res;

    v = float64_to_int64_rod(a STATUS_VAR);
    if(v < 0) {
        res = 0;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > 0xffffffff) {
        res = 0xffffffff;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

//  Same as float64_to_int32 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
int32 float64_to_int32_rod(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint64_t aSig;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if((aExp == 0x7FF) && aSig) {
        aSign = 0;
    }
    if(aExp) {
        aSig |= LIT64(0x0010000000000000);
    }
    shiftCount = 0x42C - aExp;
    if(0 < shiftCount) {
        shift64RightJamming(aSig, shiftCount, &aSig);
    }
    return rodAndPackInt32(aSign, aSig STATUS_VAR);
}

//  Same as float32_to_uint64 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
uint64_t float32_to_uint64_rod(float32 a, float_status *status)
{
    flag aSign;
    int aExp;
    int shiftCount;
    uint32_t aSig;
    uint64_t aSig64, aSigExtra;
    a = float32_squash_input_denormal(a, status);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    if((aSign) && (aExp > 126)) {
        float_raise(float_flag_invalid, status);
        if(float32_is_any_nan(a)) {
            return LIT64(0xFFFFFFFFFFFFFFFF);
        } else {
            return 0;
        }
    }
    shiftCount = 0xBE - aExp;
    if(aExp) {
        aSig |= 0x00800000;
    }
    if(shiftCount < 0) {
        float_raise(float_flag_invalid, status);
        return LIT64(0xFFFFFFFFFFFFFFFF);
    }

    aSig64 = aSig;
    aSig64 <<= 40;
    shift64ExtraRightJamming(aSig64, 0, shiftCount, &aSig64, &aSigExtra);
    return rodAndPackUint64(aSign, aSig64, aSigExtra, status);
}

//  Same as float32_to_int64 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
int64 float32_to_int64_rod(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint32_t aSig;
    uint64_t aSig64, aSigExtra;
    a = float32_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    shiftCount = 0xBE - aExp;
    if(shiftCount < 0) {
        float_raise(float_flag_invalid STATUS_VAR);
        if(!aSign || ((aExp == 0xFF) && aSig)) {
            return LIT64(0x7FFFFFFFFFFFFFFF);
        }
        return (int64_t)LIT64(0x8000000000000000);
    }
    if(aExp) {
        aSig |= 0x00800000;
    }
    aSig64 = aSig;
    aSig64 <<= 40;
    shift64ExtraRightJamming(aSig64, 0, shiftCount, &aSig64, &aSigExtra);
    return rodAndPackInt64(aSign, aSig64, aSigExtra STATUS_VAR);
}

//  Same as float32_to_uint32 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
uint32 float32_to_uint32_rod(float32 a STATUS_PARAM)
{
    int64_t v;
    uint32 res;

    v = float32_to_int64_rod(a STATUS_VAR);
    if(v < 0) {
        res = 0;
        float_raise(float_flag_invalid STATUS_VAR);
    } else if(v > 0xffffffff) {
        res = 0xffffffff;
        float_raise(float_flag_invalid STATUS_VAR);
    } else {
        res = v;
    }
    return res;
}

//  Same as float32_to_int32 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
int32 float32_to_int32_rod(float32 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp, shiftCount;
    uint32_t aSig;
    uint64_t aSig64;

    a = float32_squash_input_denormal(a STATUS_VAR);
    aSig = extractFloat32Frac(a);
    aExp = extractFloat32Exp(a);
    aSign = extractFloat32Sign(a);
    if((aExp == 0xFF) && aSig) {
        aSign = 0;
    }
    if(aExp) {
        aSig |= 0x00800000;
    }
    shiftCount = 0xAF - aExp;
    aSig64 = aSig;
    aSig64 <<= 32;
    if(0 < shiftCount) {
        shift64RightJamming(aSig64, shiftCount, &aSig64);
    }
    return rodAndPackInt32(aSign, aSig64 STATUS_VAR);
}

//  Same as float64_to_float32 but assumes round-to-odd (OR bits into LSB, aka "jam") rounding mode
float32 float64_to_float32_rod(float64 a STATUS_PARAM)
{
    flag aSign;
    int16 aExp;
    uint64_t aSig;
    uint32_t zSig;
    a = float64_squash_input_denormal(a STATUS_VAR);

    aSig = extractFloat64Frac(a);
    aExp = extractFloat64Exp(a);
    aSign = extractFloat64Sign(a);
    if(aExp == 0x7FF) {
        if(aSig) {
            return commonNaNToFloat32(float64ToCommonNaN(a STATUS_VAR) STATUS_VAR);
        }
        return packFloat32(aSign, 0xFF, 0);
    }
    shift64RightJamming(aSig, 22, &aSig);
    zSig = aSig;
    if(aExp || zSig) {
        zSig |= 0x40000000;
        aExp -= 0x381;
    }
    return rodAndPackFloat32(aSign, aExp, zSig STATUS_VAR);
}
