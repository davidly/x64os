/* f80_double.h Portable x87 80-bit <-> double
   Works with MSVC, GCC, Clang. C99/C++11.
   Written by ChatCPT. It seems to work but I haven't really tested it well.
*/
#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
  #include <intrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 80-bit extended (x87) layout in memory (little-endian):
   Bytes 0..7  : 64-bit significand (bit63 = integer bit)
   Bytes 8..9  : 1 sign bit (bit15) + 15-bit exponent (bits 14..0), bias = 16383
*/
#define EXT80_EXP_BIAS  16383
#define DBL_EXP_BIAS     1023

/* Safe bit punning helpers */
static inline uint64_t u64_from_le(const unsigned char *p) {
    return (uint64_t)p[0]
         | ((uint64_t)p[1] << 8)
         | ((uint64_t)p[2] << 16)
         | ((uint64_t)p[3] << 24)
         | ((uint64_t)p[4] << 32)
         | ((uint64_t)p[5] << 40)
         | ((uint64_t)p[6] << 48)
         | ((uint64_t)p[7] << 56);
}
static inline uint16_t u16_from_le(const unsigned char *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline void u64_to_le(uint64_t v, unsigned char *p) {
    p[0] = (unsigned char)(v      );
    p[1] = (unsigned char)(v >>  8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
    p[4] = (unsigned char)(v >> 32);
    p[5] = (unsigned char)(v >> 40);
    p[6] = (unsigned char)(v >> 48);
    p[7] = (unsigned char)(v >> 56);
}
static inline void u16_to_le(uint16_t v, unsigned char *p) {
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
}

/* Count leading zeros for 64-bit (portable fallback included). */
static inline int clz64(uint64_t x) {
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long idx;
    if (x == 0) return 64;
    #if defined(_M_X64) || defined(_M_ARM64)
        _BitScanReverse64(&idx, x);
        return 63 - (int)idx;
    #else
        /* 32-bit MSVC: emulate with two 32-bit scans */
        uint32_t hi = (uint32_t)(x >> 32);
        if (hi) {
            _BitScanReverse(&idx, hi);
            return 31 - (int)idx;
        } else {
            uint32_t lo = (uint32_t)x;
            _BitScanReverse(&idx, lo);
            return 63 - 32 - (int)idx;
        }
    #endif
#elif defined(__clang__) || defined(__GNUC__)
    if (x == 0) return 64;
    return __builtin_clzll(x);
#else
    /* Portable fallback */
    int n = 0;
    if (x == 0) return 64;
    for (int i = 63; i >= 0; --i) {
        if ((x >> i) & 1) return n;
        ++n;
    }
    return n; /* not reached */
#endif
}

/* Bit-level access to double without aliasing UB */
static inline uint64_t double_to_bits(double d) {
    uint64_t u;
    memcpy(&u, &d, sizeof u);
    return u;
}
static inline double bits_to_double(uint64_t u) {
    double d;
    memcpy(&d, &u, sizeof d);
    return d;
}

/* === ieee80 -> double ==================================================== */

inline double ieee80_to_double(const unsigned char in[10]) {
    uint64_t sig = u64_from_le(in + 0);   /* includes integer bit at bit63 */
    uint16_t se  = u16_from_le(in + 8);
    int sign     = (se >> 15) & 1;
    uint16_t e80 = se & 0x7FFF;

    /* Special cases per IEEE-754 */
    if (e80 == 0x7FFF) {
        /* Inf/NaN */
        uint64_t frac = sig & 0x7FFFFFFFFFFFFFFFULL; /* drop integer bit for testing */
        int integer_bit = (int)(sig >> 63);
        if (integer_bit && frac == 0) {
            /* Infinity */
            uint64_t out = ((uint64_t)sign << 63) | (0x7FFULL << 52);
            return bits_to_double(out);
        } else {
            /* NaN: make a quiet NaN, preserve payload best-effort */
            uint64_t payload = sig & 0x7FFFFFFFFFFFFFFFULL; /* lower 63 bits */
            /* Map top payload bits into double's 52-bit payload. Ensure quiet bit set. */
            uint64_t mant = (payload >> (63 - 52));
            mant |= (1ULL << 51); /* set quiet bit */
            if (mant == 0) mant = 1ULL << 51;
            uint64_t out = ((uint64_t)sign << 63) | (0x7FFULL << 52) | (mant & ((1ULL<<52)-1));
            return bits_to_double(out);
        }
    }

    if (e80 == 0) {
        if (sig == 0) {
            /* Zero */
            uint64_t out = ((uint64_t)sign << 63); /* +0/-0 */
            return bits_to_double(out);
        }
        /* Subnormal extended: exponent = 1 - bias, no integer bit */
        int32_t e_unb = 1 - EXT80_EXP_BIAS; /* -16382 */
        /* Normalize: shift left until MSB becomes 1 at bit63 */
        int lz = clz64(sig);
        if (lz == 64) { /* shouldn't happen because sig!=0 */
            uint64_t out = ((uint64_t)sign << 63);
            return bits_to_double(out);
        }
        int shift = lz; /* bring highest 1 to bit63 */
        sig <<= shift;
        e_unb -= shift;
        /* Now treat as normal with bit63 == 1 */
        /* Continue below with shared path */
        /* fallthrough using e_unb and sig */
        /* Convert to double with rounding. */
        /* Target unbiased exponent for double */
        int64_t e_d = e_unb; /* same real exponent */
        /* Rounding: need to go from 64 sig bits (1+63) to 53 (1+52) */
        const int drop = 63 - 52; /* 11 */
        uint64_t mant53 = sig >> drop;          /* keep top 53 bits (includes integer bit) */
        uint64_t lower  = sig & ((1ULL << drop) - 1); /* dropped bits */

        /* Round to nearest, ties to even */
        uint64_t halfway = 1ULL << (drop - 1); /* 0x400 */
        int increment = (lower > halfway) || (lower == halfway && (mant53 & 1));
        mant53 += increment;

        /* Handle carry */
        if (mant53 == (1ULL << 54)) { /* overflowed 53 bits -> renorm */
            mant53 >>= 1;
            e_d += 1;
        }

        /* Now pack to double, handling exponent range */
        int64_t e_d_biased = e_d + DBL_EXP_BIAS;
        if (e_d_biased >= 0x7FF) {
            /* overflow -> inf */
            uint64_t out = ((uint64_t)sign << 63) | (0x7FFULL << 52);
            return bits_to_double(out);
        } else if (e_d_biased <= 0) {
            /* subnormal double: shift right by (1 - e_d_biased) */
            int shift2 = (int)(1 - e_d_biased);
            if (shift2 >= 54) {
                /* Too small, may round to zero or min subnormal */
                /* Recompute rounding using the full bits we have */
                uint64_t sticky = (mant53 != 0) || (lower != 0);
                uint64_t out = ((uint64_t)sign << 63); /* zero */
                (void)sticky; /* no increment because less than half ULP of min subnormal */
                return bits_to_double(out);
            }
            /* Create a 53-bit significand (with implicit 1) then shift into 52-bit subnormal */
            uint64_t acc      = mant53; /* 53 bits including leading 1 */
            uint64_t shifted  = acc >> shift2;
//            uint64_t lostmask = (shift2 ? (acc & ((1ULL << shift2) - 1)) : 0) | lower;
            /* Round-to-nearest-even on the lowest bit of resulting subnormal payload */
            uint64_t halfway2 = (shift2 ? (1ULL << (shift2 - 1)) : 0);
            int incr = 0;
            if (shift2) {
                incr = ( (acc & ((1ULL << shift2) - 1)) > halfway2 )
                    || ( (acc & ((1ULL << shift2) - 1)) == halfway2 && (shifted & 1) )
                    || (lower != 0 && ( (acc & ((1ULL << shift2) - 1)) == 0) ); /* sticky beyond */
            }
            uint64_t frac52 = shifted & ((1ULL<<52)-1); /* drop implicit for subnormal (it gets lost anyway) */
            if (incr) {
                uint64_t t = frac52 + 1;
                frac52 = t & ((1ULL<<52)-1);
                /* If it carried here, it would make it a minimal normal, but that can only happen when shift2==1 and frac52 was all ones; handle it: */
                if (t == (1ULL<<52)) {
                    /* becomes min normal with exponent 1 and fraction 0 */
                    uint64_t out = ((uint64_t)sign << 63) | (1ULL << 52);
                    return bits_to_double(out);
                }
            }
            uint64_t out = ((uint64_t)sign << 63) | frac52;
            return bits_to_double(out);
        } else {
            /* Normal double */
            uint64_t frac52 = (mant53 & ((1ULL<<52)-1)); /* drop leading 1 */
            uint64_t out = ((uint64_t)sign << 63)
                         | ((uint64_t)e_d_biased << 52)
                         | frac52;
            return bits_to_double(out);
        }
    } /* end subnormal e80 */

    /* Normal extended */
    int32_t e_unb = (int32_t)e80 - EXT80_EXP_BIAS;
    /* If integer bit isn't set, input is non-canonical; normalize if possible */
    if ((sig >> 63) == 0) {
        if (sig == 0) {
            uint64_t out = ((uint64_t)sign << 63); /* treat as zero */
            return bits_to_double(out);
        }
        int lz = clz64(sig);
        int shift = lz;
        sig <<= shift;
        e_unb -= shift;
    }

    /* Convert to double with rounding */
    int64_t e_d = e_unb;

    const int drop = 63 - 52; /* 11 */
    uint64_t mant53 = sig >> drop;                 /* top 53 bits including integer bit */
    uint64_t lower  = sig & ((1ULL << drop) - 1);  /* dropped bits */
    uint64_t halfway = 1ULL << (drop - 1);

    int increment = (lower > halfway) || (lower == halfway && (mant53 & 1));
    mant53 += increment;

    if (mant53 == (1ULL << 54)) { /* carry */
        mant53 >>= 1;
        e_d += 1;
    }

    int64_t e_d_biased = e_d + DBL_EXP_BIAS;
    if (e_d_biased >= 0x7FF) {
        /* overflow -> inf */
        uint64_t out = ((uint64_t)sign << 63) | (0x7FFULL << 52);
        return bits_to_double(out);
    } else if (e_d_biased <= 0) {
        /* subnormal double */
        int shift2 = (int)(1 - e_d_biased);
        if (shift2 >= 54) {
            /* Too small -> zero (may round up to min subnormal, but conservatively zero) */
            uint64_t out = ((uint64_t)sign << 63);
            return bits_to_double(out);
        }
        uint64_t acc      = mant53;
        uint64_t shifted  = acc >> shift2;
//        uint64_t lostmask = (shift2 ? (acc & ((1ULL << shift2) - 1)) : 0);
        int incr = 0;
        if (shift2) {
            uint64_t half2 = 1ULL << (shift2 - 1);
            incr = ( (acc & ((1ULL << shift2) - 1)) > half2 )
                || ( (acc & ((1ULL << shift2) - 1)) == half2 && (shifted & 1) );
        }
        uint64_t frac52 = shifted & ((1ULL<<52)-1);
        if (incr) {
            uint64_t t = frac52 + 1;
            if (t == (1ULL<<52)) {
                /* bumps to min normal */
                uint64_t out = ((uint64_t)sign << 63) | (1ULL << 52);
                return bits_to_double(out);
            }
            frac52 = t;
        }
        uint64_t out = ((uint64_t)sign << 63) | frac52;
        return bits_to_double(out);
    } else {
        /* normal double */
        uint64_t frac52 = (mant53 & ((1ULL<<52)-1));
        uint64_t out = ((uint64_t)sign << 63)
                     | ((uint64_t)e_d_biased << 52)
                     | frac52;
        return bits_to_double(out);
    }
}

/* === double -> ieee80 ==================================================== */

inline void double_to_ieee80(double d, unsigned char out[10]) {
    uint64_t u = double_to_bits(d);
    int sign = (int)(u >> 63);
    uint16_t e64 = (uint16_t)((u >> 52) & 0x7FF);
    uint64_t frac = u & ((1ULL<<52)-1);

    uint16_t e80;
    uint64_t sig;

    if (e64 == 0x7FF) {
        /* Inf / NaN */
        if (frac == 0) {
            /* Infinity: set integer bit=1, rest zero */
            e80 = 0x7FFF;
            sig = 1ULL << 63;
        } else {
            /* NaN: map payload; set quiet bit in 80-bit payload (bit62) */
            e80 = 0x7FFF;
            uint64_t payload = frac;
            /* Spread 52-bit payload into lower 63 bits, keep MS quiet bit */
            uint64_t lower63 = (payload << (63 - 52));
            /* Set integer bit=1 and quiet bit (bit62) */
            sig = (1ULL << 63) | (1ULL << 62) | (lower63 & ((1ULL<<62)-1));
        }
        uint16_t se = (uint16_t)((sign << 15) | e80);
        u64_to_le(sig, out + 0);
        u16_to_le(se,  out + 8);
        return;
    }

    if (e64 == 0) {
        if (frac == 0) {
            /* Zero */
            e80 = 0;
            sig = 0;
            uint16_t se = (uint16_t)((sign << 15) | e80);
            u64_to_le(sig, out + 0);
            u16_to_le(se,  out + 8);
            return;
        }
        /* Subnormal double: E_unb = -1022, mantissa has no implicit 1 */
        int64_t e_unb = -1022;
        uint64_t mant53 = frac; /* 52-bit value; leading 1 not present */

        /* Normalize to 53 bits with leading 1 at bit52 */
        int lz = clz64(mant53) - (64 - 52); /* leading zeros within 52-bit field */
        /* clz64 returns 64 for 0; but mant53!=0 here. lz in [0..?] */
        int bitlen = 52 - lz; /* position of highest 1 is bitlen-1 (0..51) */
        int shift_up = 53 - bitlen; /* how many to shift left to get 53 bits with leading 1 */
        mant53 <<= shift_up;
        e_unb -= shift_up; /* shifting left increases magnitude -> decrease exponent */

        /* Build 80-bit: set integer bit and place 52 fraction bits at top of 63 */
        sig = ((mant53) << (63 - 52)) & 0xFFFFFFFFFFFFFFFFULL; /* mant53 already has leading 1 at bit52 */
        e80 = (uint16_t)(e_unb + EXT80_EXP_BIAS);

        uint16_t se = (uint16_t)((sign << 15) | e80);
        u64_to_le(sig, out + 0);
        u16_to_le(se,  out + 8);
        return;
    } else {
        /* Normal double */
        int64_t e_unb = (int64_t)e64 - DBL_EXP_BIAS;
        uint64_t mant53 = (1ULL << 52) | frac; /* implicit 1 restored */

        /* Map to 80-bit: place mant53 (53 bits) into [bit63 .. bit11] */
        sig = mant53 << (63 - 52);
        e80 = (uint16_t)(e_unb + EXT80_EXP_BIAS);

        uint16_t se = (uint16_t)((sign << 15) | e80);
        u64_to_le(sig, out + 0);
        u16_to_le(se,  out + 8);
        return;
    }
}

#ifdef __cplusplus
} /* extern "C" */
#endif


