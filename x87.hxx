/*
  coded by ChatGPT v5.2. vibe from David Lee
  x87_ext80.hpp  (single-header, no deps)

  Software implementation of Intel x87 80-bit extended precision (1 sign, 15 exp, 64 sig with explicit integer bit).

  ✅ Compiles with: MSVC, GCC, Clang
  ✅ Runs on: little-endian AND big-endian hosts
     - Raw 10-byte x87 format is ALWAYS interpreted/emitted as little-endian (Intel memory layout).
  ✅ Basic ops: +, -, *, /, sqrt

  Non-goals / limitations:
    - No x87 exception flags, precision-control, signaling NaN traps, or alternate rounding modes.
    - Some “unsupported” ext80 encodings are canonicalized.
*/

#ifndef X87_EXT80_HPP_INCLUDED
#define X87_EXT80_HPP_INCLUDED

#include <cstdint>
#include <cstring>
#include <cmath>    // sqrt for initial approximation in ext80::sqrt()

#if defined(_MSC_VER)
  #include <intrin.h>
  #define X87_EXT80_FORCEINLINE __forceinline
#else
  #define X87_EXT80_FORCEINLINE inline __attribute__((always_inline))
#endif

namespace x87 {

// ------------------------
// Host-endian independent little-endian load/store
// ------------------------
static X87_EXT80_FORCEINLINE uint16_t load_u16_le(const uint8_t* p) {
  return (uint16_t)p[0] | (uint16_t(p[1]) << 8);
}
static X87_EXT80_FORCEINLINE uint64_t load_u64_le(const uint8_t* p) {
  return (uint64_t)p[0]
    | (uint64_t(p[1]) << 8)
    | (uint64_t(p[2]) << 16)
    | (uint64_t(p[3]) << 24)
    | (uint64_t(p[4]) << 32)
    | (uint64_t(p[5]) << 40)
    | (uint64_t(p[6]) << 48)
    | (uint64_t(p[7]) << 56);
}
static X87_EXT80_FORCEINLINE void store_u16_le(uint8_t* p, uint16_t v) {
  p[0] = uint8_t(v);
  p[1] = uint8_t(v >> 8);
}
static X87_EXT80_FORCEINLINE void store_u64_le(uint8_t* p, uint64_t v) {
  for (int i = 0; i < 8; ++i) p[i] = uint8_t(v >> (8 * i));
}

// ------------------------
// 128-bit support
//   - Native __uint128_t on GCC/Clang
//   - Portable {hi,lo} on MSVC (and any compiler lacking __int128)
// ------------------------
#if !defined(_MSC_VER) && !defined( __mc68000__ ) && !defined( sparc ) && (defined(__GNUC__) || defined(__clang__))
  #define X87_HAS_NATIVE_U128 1
  using u128 = __uint128_t;

  static X87_EXT80_FORCEINLINE u128 u128_from64(uint64_t x) { return (u128)x; }
  static X87_EXT80_FORCEINLINE uint64_t u128_lo(u128 x) { return (uint64_t)x; }
  static X87_EXT80_FORCEINLINE uint64_t u128_hi(u128 x) { return (uint64_t)(x >> 64); }
  static X87_EXT80_FORCEINLINE bool u128_is_zero(u128 x) { return x == 0; }

  static X87_EXT80_FORCEINLINE int clz64(uint64_t x) { return x ? __builtin_clzll(x) : 64; }

  static X87_EXT80_FORCEINLINE int msb_index_u128(u128 x) {
    uint64_t hi = u128_hi(x);
    if (hi) return 127 - clz64(hi);
    uint64_t lo = u128_lo(x);
    return lo ? (63 - clz64(lo)) : -1;
  }

  static X87_EXT80_FORCEINLINE u128 shl(u128 x, int sh) { return (sh <= 0) ? x : (sh >= 128 ? 0 : (x << sh)); }
  static X87_EXT80_FORCEINLINE u128 shr(u128 x, int sh) { return (sh <= 0) ? x : (sh >= 128 ? 0 : (x >> sh)); }

  static X87_EXT80_FORCEINLINE u128 shr_sticky(u128 x, int sh) {
    if (sh <= 0) return x;
    if (sh >= 128) return x ? 1 : 0;
    u128 mask = ((u128)1 << sh) - 1;
    bool sticky = (x & mask) != 0;
    x >>= sh;
    if (sticky) x |= 1;
    return x;
  }

  static X87_EXT80_FORCEINLINE u128 mul64(uint64_t a, uint64_t b) { return (u128)a * (u128)b; }

  // For our usage, quotient fits in <=64 bits in the hot path (see division derivation).
  static X87_EXT80_FORCEINLINE u128 div_u128_u64(u128 num, uint64_t den, uint64_t& rem_out) {
    rem_out = (uint64_t)(num % den);
    return num / den;
  }

#else
  #undef X87_HAS_NATIVE_U128
  struct u128 { uint64_t hi, lo; };

  static X87_EXT80_FORCEINLINE bool u128_is_zero(const u128& x) { return x.hi == 0 && x.lo == 0; }
  static X87_EXT80_FORCEINLINE u128 u128_from64(uint64_t x) { return u128{0, x}; }
  static X87_EXT80_FORCEINLINE uint64_t u128_lo(const u128& x) { return x.lo; }
  static X87_EXT80_FORCEINLINE uint64_t u128_hi(const u128& x) { return x.hi; }

  static X87_EXT80_FORCEINLINE int clz64(uint64_t x) {
    if (!x) return 64;
    #if defined(_MSC_VER)
      unsigned long idx = 0;

      #if defined(_M_X64) || defined(_M_ARM64)
        _BitScanReverse64(&idx, x);
        return 63 - (int)idx;
      #else
        // 32-bit: use two 32-bit scans
        uint32_t hi = (uint32_t)(x >> 32);
        if (hi) {
          _BitScanReverse(&idx, hi);
          return 31 - (int)idx;
        }
        uint32_t lo = (uint32_t)x;
        _BitScanReverse(&idx, lo);
        return 63 - (int)idx;
      #endif
    #else
      int n = 0;
      while ((x & (1ull << 63)) == 0) { x <<= 1; ++n; }
      return n;
    #endif
  } //clz64

  static X87_EXT80_FORCEINLINE bool lt(const u128& a, const u128& b) {
    return (a.hi < b.hi) || (a.hi == b.hi && a.lo < b.lo);
  }
  static X87_EXT80_FORCEINLINE bool ge(const u128& a, const u128& b) { return !lt(a, b); }

  static X87_EXT80_FORCEINLINE u128 add(const u128& a, const u128& b) {
    u128 r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo ? 1 : 0);
    return r;
  }
  static X87_EXT80_FORCEINLINE u128 sub(const u128& a, const u128& b) {
    u128 r;
    r.lo = a.lo - b.lo;
    r.hi = a.hi - b.hi - (a.lo < b.lo ? 1 : 0);
    return r;
  }

  static X87_EXT80_FORCEINLINE u128 shl(const u128& x, int sh) {
    if (sh <= 0) return x;
    if (sh >= 128) return u128{0, 0};
    if (sh >= 64) return u128{ x.lo << (sh - 64), 0 };
    return u128{ (x.hi << sh) | (x.lo >> (64 - sh)), x.lo << sh };
  }
  static X87_EXT80_FORCEINLINE u128 shr(const u128& x, int sh) {
    if (sh <= 0) return x;
    if (sh >= 128) return u128{0, 0};
    if (sh >= 64) return u128{ 0, x.hi >> (sh - 64) };
    return u128{ x.hi >> sh, (x.lo >> sh) | (x.hi << (64 - sh)) };
  }

  static X87_EXT80_FORCEINLINE u128 shr_sticky(u128 x, int sh) {
    if (sh <= 0) return x;
    if (sh >= 128) return u128_is_zero(x) ? u128{0,0} : u128{0,1};

    bool sticky = false;
    if (sh >= 64) {
      int k = sh - 64;
      sticky = (x.lo != 0);
      if (k > 0) {
        uint64_t mask = (k >= 64) ? ~0ull : ((1ull << k) - 1);
        sticky = sticky || ((x.hi & mask) != 0);
      }
    } else {
      uint64_t mask = (sh == 64) ? ~0ull : ((1ull << sh) - 1);
      sticky = (x.lo & mask) != 0;
    }

    x = shr(x, sh);
    if (sticky) x.lo |= 1;
    return x;
  } //shr_sticky

  static X87_EXT80_FORCEINLINE int msb_index_u128(const u128& x) {
    if (x.hi) return 127 - clz64(x.hi);
    if (x.lo) return 63 - clz64(x.lo);
    return -1;
  }

  static X87_EXT80_FORCEINLINE u128 mul64(uint64_t a, uint64_t b) {
    #if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
      u128 r{};
      r.lo = _umul128(a, b, &r.hi);
      return r;
    #else
      // portable 64x64->128 using 32-bit halves
      uint64_t a0 = (uint32_t)a, a1 = a >> 32;
      uint64_t b0 = (uint32_t)b, b1 = b >> 32;
      uint64_t p00 = a0 * b0;
      uint64_t p01 = a0 * b1;
      uint64_t p10 = a1 * b0;
      uint64_t p11 = a1 * b1;
      uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
      u128 r;
      r.lo = (p00 & 0xFFFFFFFFull) | (mid << 32);
      r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
      return r;
    #endif
  } //mul64

  static X87_EXT80_FORCEINLINE u128 div_u128_u64(u128 num, uint64_t den, uint64_t& rem_out) {
    // den != 0
    #if 0 //defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
      // quotient fits in 64 bits for our main use; still handle full u128.
      // Divide high limb first: q_hi = num.hi / den, rem = num.hi % den
      uint64_t q_hi = num.hi / den;
      uint64_t rem1 = num.hi % den;
  
      unsigned __int64 q_lo = 0;
      unsigned __int64 rem2 = 0;
      q_lo = _udiv128(rem1, num.lo, den, &rem2);
      rem_out = rem2;
      return u128{ q_hi, q_lo };
    #else
      // portable long division (bitwise)
      u128 q{0,0};
      u128 r{0,0};
      for (int i = 127; i >= 0; --i) {
        // r = (r<<1) | bit(num,i)
        r = shl(r, 1);
        uint64_t bit = (i >= 64) ? ((num.hi >> (i - 64)) & 1ull) : ((num.lo >> i) & 1ull);
        r.lo |= bit;
  
        bool r_ge_den = (r.hi != 0) || (r.lo >= den);
        if (r_ge_den) {
          r = sub(r, u128{0, den});
          if (i >= 64) q.hi |= (1ull << (i - 64));
          else         q.lo |= (1ull << i);
        }
      }
      rem_out = r.lo;
      return q;
    #endif
  } //div_u128_u64
#endif

// ------------------------
// ext80: 80-bit extended float
// ------------------------
struct ext80 {
  uint16_t exp = 0;  // biased exponent (15-bit)
  uint64_t sig = 0;  // 64-bit significand with explicit integer bit at bit63
  bool sign = false;

  static constexpr uint16_t EXP_INF_NAN = 0x7FFF;
  static constexpr int      EXP_BIAS    = 16383;

  static constexpr uint64_t SIG_INT_BIT   = 0x8000000000000000ull;
  static constexpr uint64_t SIG_FRAC_MASK = 0x7FFFFFFFFFFFFFFFull;
  static constexpr uint64_t QNAN_BIT      = 0x4000000000000000ull;

  enum class fpclass { zero, subnormal, normal, inf, nan };

  // ===================== x87-style control: precision + rounding =====================
  // Header-only, pre-C++17, single-thread. Uses one process-wide control word.
  //
  // Bits implemented (x87 control word convention):
  //   PC (bits 8..9): precision control
  //     00 = 24-bit, 10 = 53-bit, 11 = 64-bit, 01 reserved -> treat as 64-bit
  //   RC (bits 10..11): rounding control
  //     00 = nearest-even, 01 = down (-inf), 10 = up (+inf), 11 = toward zero
  //
  // apply_fp_control(v) rounds finite results to PC precision with RC mode.
  
  static constexpr uint16_t CW_PC_MASK = 0x0300u; // bits 8..9
  static constexpr uint16_t CW_RC_MASK = 0x0C00u; // bits 10..11
  
  static constexpr uint16_t CW_PC_24   = 0x0000u; // 00b
  static constexpr uint16_t CW_PC_53   = 0x0200u; // 10b
  static constexpr uint16_t CW_PC_64   = 0x0300u; // 11b
  
  static constexpr uint16_t CW_RC_NEAR = 0x0000u; // 00b: nearest-even
  static constexpr uint16_t CW_RC_DOWN = 0x0400u; // 01b: toward -inf
  static constexpr uint16_t CW_RC_UP   = 0x0800u; // 10b: toward +inf
  static constexpr uint16_t CW_RC_TRUNC= 0x0C00u; // 11b: toward 0
  
  // Typical x87 default is 0x037F: masks, nearest-even, PC=64
  static constexpr uint16_t CW_DEFAULT_X87 = 0x037Fu;
  
  // Global control word accessor (function-local static => header-only, single-thread)
  static X87_EXT80_FORCEINLINE uint16_t& control_word_ref() {
    static uint16_t cw = CW_DEFAULT_X87;
    return cw;
  }
  
  static X87_EXT80_FORCEINLINE uint16_t get_control_word() { return control_word_ref(); }
  static X87_EXT80_FORCEINLINE void set_control_word(uint16_t cw) { control_word_ref() = cw; }
  
  // PC
  static X87_EXT80_FORCEINLINE uint16_t get_pc_bits() { return (uint16_t)(control_word_ref() & CW_PC_MASK); }
  static X87_EXT80_FORCEINLINE void set_pc_bits(uint16_t pc_bits) {
    uint16_t& cw = control_word_ref();
    cw = (uint16_t)((cw & ~CW_PC_MASK) | (pc_bits & CW_PC_MASK));
  }
  
  // RC
  static X87_EXT80_FORCEINLINE uint16_t get_rc_bits() { return (uint16_t)(control_word_ref() & CW_RC_MASK); }
  static X87_EXT80_FORCEINLINE void set_rc_bits(uint16_t rc_bits) {
    uint16_t& cw = control_word_ref();
    cw = (uint16_t)((cw & ~CW_RC_MASK) | (rc_bits & CW_RC_MASK));
  }
  
  // RAII guard to temporarily set PC/RC bits (like fldcw patterns)
  struct fp_control_guard {
    uint16_t old;
    X87_EXT80_FORCEINLINE fp_control_guard(uint16_t new_pc_bits, uint16_t new_rc_bits)
      : old(get_control_word()) {
      uint16_t cw = old;
      cw = (uint16_t)((cw & ~CW_PC_MASK) | (new_pc_bits & CW_PC_MASK));
      cw = (uint16_t)((cw & ~CW_RC_MASK) | (new_rc_bits & CW_RC_MASK));
      set_control_word(cw);
    }
    X87_EXT80_FORCEINLINE ~fp_control_guard() { set_control_word(old); }
  };

  private:
  static X87_EXT80_FORCEINLINE int pc_to_Pbits(uint16_t pc_bits) {
    int pc = (pc_bits >> 8) & 3;
    if (pc == 0) return 24;
    if (pc == 2) return 53;
    return 64; // pc==3 or reserved
  } //pc_to_Pbits
  
  static X87_EXT80_FORCEINLINE uint16_t rc_bits_norm(uint16_t rc_bits) {
    return (uint16_t)(rc_bits & CW_RC_MASK);
  }
  
  // Decide whether to increment the truncated significand according to RC and sign.
  // Inputs:
  //   sign      : result sign (true if negative)
  //   guard     : highest dropped bit
  //   sticky    : OR of all lower dropped bits
  //   lsb_odd   : LSB of retained "main" (for ties-to-even)
  // Return: round_up?
  static X87_EXT80_FORCEINLINE bool should_round_up(bool sign, bool guard, bool sticky, bool lsb_odd, uint16_t rc_bits) {
    if (!guard && !sticky) return false; // already exact
  
    switch (rc_bits_norm(rc_bits)) {
      default:
      case CW_RC_NEAR: {
        // nearest, ties to even: round_up = guard && (sticky || lsb_odd)
        return guard && (sticky || lsb_odd);
      }
      case CW_RC_DOWN:
        // toward -inf: if negative and any dropped bits => more negative
        return sign;
      case CW_RC_UP:
        // toward +inf: if positive and any dropped bits => more positive
        return !sign;
      case CW_RC_TRUNC:
        // toward 0: never increase magnitude
        return false;
    }
  } //should_round_up
  
  // Apply PC+RC rounding to v.sig (64-bit) by dropping low bits and rounding.
  static X87_EXT80_FORCEINLINE void round_sig_to_P(ext80& v, int P, uint16_t rc_bits) {
    // v assumed finite, nonzero; may be normal or subnormal; v.sig holds payload bits.
    if (P >= 64) return;
  
    const int drop = 64 - P;
    if (drop <= 0) return;
  
    uint64_t sig = v.sig;
    if (sig == 0) { v = make_zero(v.sign); return; }
  
    uint64_t main = sig >> drop;
    uint64_t lost = (drop == 64) ? sig : (sig & ((1ull << drop) - 1));
  
    bool guard = ((lost >> (drop - 1)) & 1ull) != 0;
    bool sticky = false;
    if (drop > 1) sticky = (lost & ((1ull << (drop - 1)) - 1)) != 0;
  
    bool lsb_odd = (main & 1ull) != 0;
    bool round_up = should_round_up(v.sign, guard, sticky, lsb_odd, rc_bits);
  
    if (round_up) {
      main += 1;
  
      // Overflow into bit P => renormalize + increment exponent.
      if (P < 64 && ((main >> P) & 1ull)) {
        main >>= 1;
  
        if (v.exp != 0) {
          if (v.exp + 1 >= EXP_INF_NAN) { v = make_inf(v.sign); return; }
          v.exp = (uint16_t)(v.exp + 1);
        } else {
          // subnormal -> normal
          v.exp = 1;
        }
      }
    }
  
    v.sig = main << drop;
  } //round_sig_to_P
  
  public:
  // Apply configured precision+rounding to a result value.
  // Call this at the end of each arithmetic op after normalize().
  static X87_EXT80_FORCEINLINE void apply_fp_control(ext80& v) {
    v.canonicalize();
    if (v.is_nan() || v.is_inf() || v.is_zero()) return;
  
    // Normalize first so "precision" applies to the canonical 64-bit sig form.
    v.normalize();
    if (v.is_nan() || v.is_inf() || v.is_zero()) return;
  
    const int P = pc_to_Pbits(get_pc_bits());
    const uint16_t rc = get_rc_bits();
  
    if (P < 64) {
      round_sig_to_P(v, P, rc);
      v.normalize();
    } else {
      // Even at P=64, RC can matter only if we were going to drop bits,
      // which we are not. So nothing to do.
    }
  } //apply_fp_control
  
  // ===================== end x87-style control =====================

  // ---- constructors ----
  static X87_EXT80_FORCEINLINE ext80 make_zero(bool s=false) {
    ext80 r; r.sign = s; r.exp = 0; r.sig = 0; return r;
  }
  static X87_EXT80_FORCEINLINE ext80 make_inf(bool s=false) {
    ext80 r; r.sign = s; r.exp = EXP_INF_NAN; r.sig = SIG_INT_BIT; return r;
  }

  static X87_EXT80_FORCEINLINE ext80 make_qnan(uint64_t payload = 1) {
    ext80 r; r.sign = false; r.exp = EXP_INF_NAN;
    uint64_t frac = payload & SIG_FRAC_MASK;
    if (frac == 0) frac = 1;
    frac |= QNAN_BIT;
    r.sig = SIG_INT_BIT | frac;
    return r;
  } //make_qnan

  X87_EXT80_FORCEINLINE ext80 negated() const {
    ext80 r = *this;
    r.sign = !r.sign;
    return r;
  } //negated

  X87_EXT80_FORCEINLINE ext80& abs_self() {
    sign = false;
    return *this;
  } //abs_self

  X87_EXT80_FORCEINLINE ext80 abs() const {
    ext80 r = *this;
    r.sign = false;
    return r;
  } //abs

  // ---- integer rounding helpers ----
  
  // Truncate toward zero (like std::trunc)
  X87_EXT80_FORCEINLINE ext80 trunc() const {
    ext80 a = *this;
    a.canonicalize();
  
    if (a.is_nan() || a.is_inf() || a.is_zero()) return a;
  
    // For subnormals in our representation, |x| < 1 => trunc -> signed zero
    if (a.exp == 0) return make_zero(a.sign);
  
    // value = sig * 2^(exp_unb - 63)
    int exp_unb = int(a.exp) - EXP_BIAS;
    int k = exp_unb - 63;
  
    // Already an integer (or huge): no fractional bits
    if (k >= 0) return a;
  
    int n = -k; // number of fractional bits in the fixed-point interpretation
  
    // If shifting right by >=64, integer part becomes zero
    if (n >= 64) return make_zero(a.sign);
  
    uint64_t ip = a.sig >> n; // integer part
  
    if (ip == 0) return make_zero(a.sign);
  
    // Pack exact integer ip back into ext80:
    // integer ip has msb at position p, value = (ip << (63-p)) * 2^(p-63)
    int p = 63 - clz64(ip); // 0..63
    uint64_t sig64 = ip << (63 - p);
  
    ext80 r;
    r.sign = a.sign;
    r.exp  = uint16_t(p + EXP_BIAS);
    r.sig  = sig64;
    r.normalize();
    ext80::apply_fp_control(r);
    return r;
  } //trunc
  
  // Round to nearest integer, ties away from zero (like std::round)
  X87_EXT80_FORCEINLINE ext80 round() const {
    ext80 a = *this;
    a.canonicalize();
  
    if (a.is_nan() || a.is_inf() || a.is_zero()) return a;
  
    // Subnormals are in (-1,1): rounding depends on magnitude vs 0.5
    if (a.exp == 0) {
      // magnitude = sig * 2^(1-bias-63) is tiny; always < 0.5
      return make_zero(a.sign);
    }
  
    int exp_unb = int(a.exp) - EXP_BIAS;
    int k = exp_unb - 63;
  
    // No fractional bits -> already integral
    if (k >= 0) return a;
  
    int n = -k; // fractional bits count
  
    // If n > 64 then |x| < 0.5 always (since sig < 2^64), so rounds to signed zero
    if (n > 64) return make_zero(a.sign);
  
    // Compute integer part and whether to increment (ties away from zero)
    u128 ip = u128_from64(0);
    bool inc = false;
  
    if (n == 64) {
      // value = sig / 2^64, integer part is 0, fractional is sig.
      // Round to 1 iff sig >= 2^63 (i.e., >= 0.5)
      ip = u128_from64(0);
      inc = (a.sig >= (1ull << 63));
    } else {
      // 1 <= n <= 63
      uint64_t ip64 = a.sig >> n;
      uint64_t frac = a.sig & ((1ull << n) - 1);
      uint64_t half = (1ull << (n - 1));
  
      if (frac > half) inc = true;
      else if (frac < half) inc = false;
      else inc = true; // tie => away from zero
  
      ip = u128_from64(ip64);
    }
  
    if (inc) {
    #if defined(X87_HAS_NATIVE_U128)
      ip = (u128)ip + 1;
    #else
      // ip is <= 2^64 in our use; increment safely
      ip.lo += 1;
      if (ip.lo == 0) ip.hi += 1;
    #endif
    }
  
    // If result is zero, preserve sign of input (std::round does)
    if (
      #if defined(X87_HAS_NATIVE_U128)
        (ip == 0)
      #else
        u128_is_zero(ip)
      #endif
    ) {
      return make_zero(a.sign);
    }
  
    // Pack exact integer ip back into ext80 (ip can be up to 2^64 here)
    int p = msb_index_u128(ip); // >=0
    u128 sigN;
  
    if (p > 63) sigN = shr(ip, p - 63);
    else        sigN = shl(ip, 63 - p);
  
    ext80 r;
    r.sign = a.sign;
    r.exp  = uint16_t(p + EXP_BIAS);
    r.sig  = u128_lo(sigN);
    r.normalize();
    ext80::apply_fp_control(r);
    return r;
  } //round
  
  // Round to nearest integer, ties-to-even (like std::rint / nearbyint under FE_TONEAREST)
  X87_EXT80_FORCEINLINE ext80 rint() const {
    ext80 a = *this;
    a.canonicalize();
  
    if (a.is_nan() || a.is_inf() || a.is_zero()) return a;
  
    // subnormals are tiny in our representation => always round to signed zero
    if (a.exp == 0) return make_zero(a.sign);
  
    int exp_unb = int(a.exp) - EXP_BIAS;
    int k = exp_unb - 63;
  
    // No fractional bits => already integral
    if (k >= 0) return a;
  
    int n = -k; // number of fractional bits
  
    // If n > 64 then |x| < 0.5 always => rounds to signed zero
    if (n > 64) return make_zero(a.sign);
  
    u128 ip = u128_from64(0);
    bool inc = false;
  
    if (n == 64) {
      // value = sig / 2^64
      // integer part 0, remainder sig
      // compare remainder to 0.5 ulp => 2^63
      uint64_t rem = a.sig;
      bool gt_half = rem > (1ull << 63);
      bool eq_half = rem == (1ull << 63);
      // ties-to-even: increment if > half, or if == half AND result would be odd (1 is odd)
      inc = gt_half || (eq_half /* && odd */); // odd because ip==0, ip+1 => 1 (odd); but tie chooses even => do NOT inc
      // Wait: tie should go to even => 0 is even, so on exact half we should NOT inc.
      if (eq_half) inc = false;
      ip = u128_from64(0);
    } else {
      // 1 <= n <= 63
      uint64_t ip64 = a.sig >> n;
      uint64_t frac = a.sig & ((1ull << n) - 1);
      uint64_t half = (1ull << (n - 1));
  
      bool gt_half = frac > half;
      bool eq_half = frac == half;
      bool ip_odd  = (ip64 & 1ull) != 0;
  
      // ties-to-even
      inc = gt_half || (eq_half && ip_odd);
      ip = u128_from64(ip64);
    }
  
    if (inc) {
    #if defined(X87_HAS_NATIVE_U128)
        ip = (u128)ip + 1;
    #else
        ip.lo += 1;
        if (ip.lo == 0) ip.hi += 1;
    #endif
    }
  
    #if defined(X87_HAS_NATIVE_U128)
      if (ip == 0) return make_zero(a.sign);
    #else
      if (u128_is_zero(ip)) return make_zero(a.sign);
    #endif
  
    // Pack exact integer ip into ext80
    int p = msb_index_u128(ip);
    u128 sigN = (p > 63) ? shr(ip, p - 63) : shl(ip, 63 - p);
  
    ext80 r;
    r.sign = a.sign;
    r.exp  = uint16_t(p + EXP_BIAS);
    r.sig  = u128_lo(sigN);
    r.normalize();
    return r;
  } //rint

  // ---- classification ----
  X87_EXT80_FORCEINLINE fpclass classify() const {
    if (exp == 0) return (sig == 0) ? fpclass::zero : fpclass::subnormal;
    if (exp == EXP_INF_NAN) return (sig == SIG_INT_BIT) ? fpclass::inf : fpclass::nan;
    return fpclass::normal;
  } //classify

  X87_EXT80_FORCEINLINE bool is_nan()  const { return classify() == fpclass::nan; }
  X87_EXT80_FORCEINLINE bool is_inf()  const { return classify() == fpclass::inf; }
  X87_EXT80_FORCEINLINE bool is_zero() const { return classify() == fpclass::zero; }
  X87_EXT80_FORCEINLINE bool signbit() const { return sign; }

  // ---- raw 10-byte Intel LE encoding ----
  static X87_EXT80_FORCEINLINE ext80 from_bytes_le(const uint8_t b[10]) {
    ext80 r;
    r.sig = load_u64_le(b);
    uint16_t se = load_u16_le(b + 8);
    r.sign = (se & 0x8000u) != 0;
    r.exp  = (se & 0x7FFFu);
    r.canonicalize();
    return r;
  } //from_bytes_le

  X87_EXT80_FORCEINLINE void to_bytes_le(uint8_t out[10]) const {
    store_u64_le(out, sig);
    uint16_t se = (uint16_t)(exp & 0x7FFFu);
    if (sign) se |= 0x8000u;
    store_u16_le(out + 8, se);
  } //to_bytes_le

  // ---- conversion: double <-> ext80 (for IO/testing, and sqrt initial guess) ----
  static X87_EXT80_FORCEINLINE ext80 from_double(double d) {
    uint64_t bits; std::memcpy(&bits, &d, 8);
    bool s = (bits >> 63) != 0;
    uint16_t e = (uint16_t)((bits >> 52) & 0x7FFu);
    uint64_t m = bits & ((1ull << 52) - 1);

    if (e == 0x7FFu) {
      if (m == 0) return make_inf(s);
      return make_qnan((m ? m : 1) | QNAN_BIT);
    }
    if (e == 0 && m == 0) return make_zero(s);

    ext80 r; r.sign = s;

    if (e == 0) {
      // subnormal double: normalize mantissa
      int msb = 63 - clz64(m);          // in [0..51]
      int norm_shift = 52 - (msb + 1);  // shift left to put leading 1 at bit51
      uint64_t mm = m << norm_shift;
      int exp_unb = (1 - 1023) - norm_shift;

      uint64_t frac52 = (mm & ((1ull << 52) - 1));
      uint64_t frac63 = frac52 << (63 - 52);
      r.sig = SIG_INT_BIT | (frac63 & SIG_FRAC_MASK);
      r.exp = (uint16_t)(exp_unb + EXP_BIAS);
      r.normalize();
      return r;
    } else {
      int exp_unb = int(e) - 1023;
      uint64_t frac63 = m << (63 - 52);
      r.sig = SIG_INT_BIT | (frac63 & SIG_FRAC_MASK);
      r.exp = (uint16_t)(exp_unb + EXP_BIAS);
      r.normalize();
      return r;
    }
  } //from_double

  X87_EXT80_FORCEINLINE double to_double() const {
    auto c = classify();
    if (c == fpclass::nan) {
      uint64_t out = 0x7FF8000000000000ull;
      double d; std::memcpy(&d, &out, 8); return d;
    }
    if (c == fpclass::inf) {
      uint64_t out = (uint64_t(sign) << 63) | 0x7FF0000000000000ull;
      double d; std::memcpy(&d, &out, 8); return d;
    }
    if (c == fpclass::zero) {
      uint64_t out = (uint64_t(sign) << 63);
      double d; std::memcpy(&d, &out, 8); return d;
    }

    ext80 t = *this;
    t.canonicalize();
    int e_unb = int(t.exp) - EXP_BIAS;

    // Convert 64-bit sig -> 53-bit for double with RNE
    const int SHIFT = 11;
    uint64_t top = t.sig >> SHIFT;                 // 53 bits
    uint64_t rem = t.sig & ((1ull << SHIFT) - 1);  // 11 bits

    bool guard  = ((rem >> (SHIFT - 1)) & 1ull) != 0;
    bool sticky = (rem & ((1ull << (SHIFT - 1)) - 1)) != 0;
    bool lsb_odd = (top & 1ull) != 0;
    if (guard && (sticky || lsb_odd)) {
      top += 1;
      if (top == (1ull << 53)) { top >>= 1; e_unb += 1; }
    }

    int de = e_unb + 1023;
    if (de >= 0x7FF) {
      uint64_t out = (uint64_t(sign) << 63) | 0x7FF0000000000000ull;
      double d; std::memcpy(&d, &out, 8); return d;
    }
    if (de <= 0) {
      // subnormal/zero in double
      int rshift = 1 - de;
      if (rshift >= 64) {
        uint64_t out = (uint64_t(sign) << 63);
        double d; std::memcpy(&d, &out, 8); return d;
      }
      uint64_t mant = top & ((1ull << 52) - 1);
      uint64_t full = (1ull << 52) | mant;
      uint64_t cut = full & ((rshift >= 64) ? ~0ull : ((1ull << rshift) - 1));
      uint64_t subm = full >> rshift;

      bool g = (rshift > 0) ? ((cut >> (rshift - 1)) & 1ull) : false;
      bool st = (rshift > 1) ? ((cut & ((1ull << (rshift - 1)) - 1)) != 0) : false;
      bool odd = (subm & 1ull) != 0;
      if (g && (st || odd)) subm += 1;

      uint64_t out = (uint64_t(sign) << 63) | (subm & ((1ull << 52) - 1));
      double d; std::memcpy(&d, &out, 8); return d;
    }

    uint64_t mant52 = top & ((1ull << 52) - 1);
    uint64_t out = (uint64_t(sign) << 63) | (uint64_t(de) << 52) | mant52;
    double d; std::memcpy(&d, &out, 8); return d;
  } //to_double

  // ---- arithmetic operators ----
  friend X87_EXT80_FORCEINLINE ext80 operator-(ext80 a) { a.sign = !a.sign; return a; }
  friend X87_EXT80_FORCEINLINE ext80 operator+(const ext80& a, const ext80& b) { return addsub(a, b, false); }
  friend X87_EXT80_FORCEINLINE ext80 operator-(const ext80& a, const ext80& b) { return addsub(a, b, true); }
  friend X87_EXT80_FORCEINLINE ext80 operator*(const ext80& a, const ext80& b) { return mul(a, b); }
  friend X87_EXT80_FORCEINLINE ext80 operator/(const ext80& a, const ext80& b) { return div(a, b); }

  X87_EXT80_FORCEINLINE ext80& operator+=(const ext80& o){ *this = *this + o; return *this; }
  X87_EXT80_FORCEINLINE ext80& operator-=(const ext80& o){ *this = *this - o; return *this; }
  X87_EXT80_FORCEINLINE ext80& operator*=(const ext80& o){ *this = *this * o; return *this; }
  X87_EXT80_FORCEINLINE ext80& operator/=(const ext80& o){ *this = *this / o; return *this; }

  // log2(x)  — base-2 logarithm
  X87_EXT80_FORCEINLINE ext80 log2() const {
    ext80 a = *this;
    a.canonicalize();
  
    // Special cases
    if (a.is_nan()) return a;
    if (a.is_zero()) return make_inf(true);   // log(0) = -inf
    if (a.sign) return make_qnan(1);          // log(negative) = NaN
    if (a.is_inf()) return make_inf(false);   // log(+inf) = +inf
  
    // Initial approximation using double
    double xd = a.to_double();
    ext80 y = ext80::from_double(::log2(xd));
  
    // Optional single Newton refinement:
    // Solve f(y) = 2^y - x = 0  =>  y' = y - (2^y - x)/(2^y * ln(2))
    // This greatly improves accuracy beyond double precision.
  
    const ext80 ln2 = ext80::from_double(std::log(2.0));
    ext80 two_to_y = ext80::from_double(::exp2(y.to_double()));
    y = y - (two_to_y - a) / (two_to_y * ln2);
  
    y.normalize();
    ext80::apply_fp_control(y);
    return y;
  } //log2
  
  // pow(x, y)  — this^y
  X87_EXT80_FORCEINLINE ext80 pow(const ext80& y) const {
    ext80 a = *this;
    ext80 b = y;
    a.canonicalize();
    b.canonicalize();
  
    // NaN propagation
    if (a.is_nan()) return a;
    if (b.is_nan()) return b;
  
    // Special cases
    if (b.is_zero()) return ext80::from_double(1.0); // x^0 = 1
    if (a.is_zero()) {
      if (b.sign) return make_inf(false);  // 0^negative = +inf
      return make_zero(false);             // 0^positive = 0
    }
    if (a.is_inf()) {
      if (b.sign) return make_zero(false);
      return make_inf(false);
    }
    if (a.sign) return make_qnan(1); // negative base not supported (needs integer exponent handling)
  
    // Compute y * log2(x)
    ext80 t = b * a.log2();
  
    // Compute 2^t using double for initial, then one refinement step
    ext80 result = ext80::from_double(::exp2(t.to_double()));
  
    // One Newton refinement for 2^t:  f(z)=log2(z)-t
    // z' = z - (log2(z) - t)/(1/(z*ln2)) = z * (1 - (log2(z)-t)*ln2)
    const ext80 ln2 = ext80::from_double(std::log(2.0));
    ext80 logz = result.log2();
    result = result * (ext80::from_double(1.0) - (logz - t) * ln2);
  
    result.normalize();
    apply_fp_control(result);
    return result;
  } //pow

  // atan2(y, x) — angle of vector (x,y), range (-pi, pi]
  static X87_EXT80_FORCEINLINE ext80 atan2(const ext80& y_in, const ext80& x_in) {
    ext80 y = y_in;
    ext80 x = x_in;
    y.canonicalize();
    x.canonicalize();
  
    // NaN propagation
    if (y.is_nan()) return y;
    if (x.is_nan()) return x;
  
    const ext80 zero = ext80::from_double(0.0);
    const ext80 pi   = ext80::from_double(3.141592653589793238462643383279502884L);
    const ext80 half_pi = pi * ext80::from_double(0.5);
  
    // Both zero → undefined → NaN (matches libm/x87)
    if (x.is_zero() && y.is_zero())
      return make_qnan(1);
  
    // y = 0
    if (y.is_zero()) {
      if (!x.sign) return y;                 // +0 or -0
      return y.sign ? -pi : pi;              // ±pi depending on sign of y
    }
  
    // x = 0
    if (x.is_zero()) {
      return y.sign ? -half_pi : half_pi;
    }
  
    // infinities
    if (x.is_inf() || y.is_inf()) {
      // Reduce to signs only (like hardware)
      double yd = y.sign ? -1.0 : 1.0;
      double xd = x.sign ? -1.0 : 1.0;
      return ext80::from_double(std::atan2(yd, xd));
    }
  
    // --- Core computation ---
  
    // Initial approximation from double
    double yd = y.to_double();
    double xd = x.to_double();
    ext80 theta = ext80::from_double(std::atan2(yd, xd));
  
    // Refine using Newton on f(t) = tan(t) - y/x
    // f'(t) = sec^2(t) = 1 + tan^2(t)
    // t' = t - (tan(t) - y/x) / (1 + tan^2(t))
  
    ext80 r = y / x;
    ext80 t = theta;
  
    // Two iterations are plenty for 80-bit
    for (int i = 0; i < 2; ++i) {
      ext80 tan_t = ext80::from_double(std::tan(t.to_double())); // seed via double
      ext80 f = tan_t - r;
      ext80 fp = ext80::from_double(1.0) + tan_t * tan_t;
      t = t - f / fp;
    }
  
    t.normalize();
    ext80::apply_fp_control(t);
    return t;
  } //atan2

  public:
  // ---- sqrt via Newton–Raphson refinement in ext80 arithmetic ----
  static X87_EXT80_FORCEINLINE ext80 sqrt(const ext80& x) {
    ext80 a = x; a.canonicalize();

    if (a.is_nan()) return quiet_nan(a);
    if (a.is_inf()) return a.sign ? make_qnan(1) : a;
    if (a.is_zero()) return a;       // keep signed zero
    if (a.sign) return make_qnan(1); // sqrt(negative) = NaN

    // Initial guess from double
    double ad = a.to_double();
    ext80 y = ext80::from_double(std::sqrt(ad));

    // Newton iterations: y = 0.5*(y + a/y)
    const ext80 half = ext80::from_double(0.5);
    y = half * (y + (a / y));
    y = half * (y + (a / y));

    y.canonicalize();
    y.normalize();
    apply_fp_control(y);
    return y;
  } //sqrt

private:
  // ---- canonicalization and normalization ----
  X87_EXT80_FORCEINLINE void canonicalize() {
    auto c = classify();
    if (c == fpclass::nan) {
      sig |= SIG_INT_BIT;
      sig |= QNAN_BIT;
      if ((sig & SIG_FRAC_MASK) == 0) sig |= 1;
      return;
    }
    if (c == fpclass::inf) { sig = SIG_INT_BIT; return; }
    if (c == fpclass::zero) { sig = 0; exp = 0; return; }

    // If exp != 0 but integer bit missing, normalize left.
    if (exp != 0 && (sig & SIG_INT_BIT) == 0) {
      while (sig && (sig & SIG_INT_BIT) == 0) {
        sig <<= 1;
        if (exp > 0) exp -= 1;
        else break;
      }
      if (sig == 0) { exp = 0; return; }
    }
    // If exp==0 but integer bit set, coerce to exp=1.
    if (exp == 0 && (sig & SIG_INT_BIT)) exp = 1;
  } //canonicalize

  X87_EXT80_FORCEINLINE void normalize() {
    // Bring finite nonzero to preferred form: if exp!=0 then integer bit set.
    auto c = classify();
    if (c == fpclass::nan || c == fpclass::inf || c == fpclass::zero) return;

    if (exp == 0) {
      sig &= SIG_FRAC_MASK;
      if (sig == 0) { /* stays zero */ }
      return;
    }

    if (sig == 0) { exp = 0; return; }

    while ((sig & SIG_INT_BIT) == 0) {
      sig <<= 1;
      if (exp == 0) break;
      exp -= 1;
    }

    if (exp >= EXP_INF_NAN) { exp = EXP_INF_NAN; sig = SIG_INT_BIT; return; }
    if (exp == 0) sig &= SIG_FRAC_MASK;
  } //normalize

  static X87_EXT80_FORCEINLINE ext80 quiet_nan(const ext80& x) {
    ext80 r = x;
    r.canonicalize();
  
    // If it's not NaN after canonicalize, return a default qNaN
    if (!r.is_nan()) return make_qnan(1);
  
    // Canonicalize sign to match your observed hardware behavior for NaN propagation
    r.sign = false;
  
    // Ensure exponent is all-ones (NaN/Inf class)
    r.exp = EXP_INF_NAN;
  
    // Ensure it's a quiet NaN and payload is non-zero.
    // For ext80, treat fraction MSB (bit 62) as the quiet bit.
    // Keep integer bit set (many NaN encodings already do); if not, set it.
    r.sig |= 0x8000000000000000ull; // integer bit (bit 63)
    r.sig |= 0x4000000000000000ull; // quiet bit (bit 62)
  
    // Make sure it's not accidentally "infinity" (fraction all-zero).
    if ((r.sig & SIG_FRAC_MASK) == 0) r.sig |= 1ull;
  
    return r;
  } //quiet_nan

  static X87_EXT80_FORCEINLINE ext80 nan_binop(const ext80& a, const ext80& b) {
    if (a.is_nan()) return quiet_nan(a);
    if (b.is_nan()) return quiet_nan(b);
    return make_qnan(1);
  } //nan_binop

  // ---- rounding helper: round u128 value with EXTRA low bits ----
  // Interpret v as (main << EXTRA) | extraBits, round to 64-bit significand with RNE.
  // Returns:
  //   out_sig64  : 64-bit significand (explicit integer bit at bit63 for normal results)
  //   exp_adjust : +1 if rounding overflowed and required renormalizing right by 1.
  static X87_EXT80_FORCEINLINE void round_from_extra(const u128& v, int EXTRA,
                                                     uint64_t& out_sig64, int& exp_adjust) {
    exp_adjust = 0;
  
    #if defined(X87_HAS_NATIVE_U128)
      u128 vv   = v;
      u128 main = vv >> EXTRA;
      u128 rem  = (EXTRA > 0) ? (vv & (((u128)1 << EXTRA) - 1)) : 0;
    
      bool guard  = (EXTRA > 0) ? (((rem >> (EXTRA - 1)) & 1) != 0) : false;
      bool sticky = (EXTRA > 1) ? ((rem & (((u128)1 << (EXTRA - 1)) - 1)) != 0) : false;
    
      uint64_t m64 = (uint64_t)main;
    #else
      u128 main = shr(v, EXTRA);
    
      bool guard = false, sticky = false;
      if (EXTRA > 0) {
        // guard bit = bit (EXTRA-1) of v
        if (EXTRA <= 64) guard = ((v.lo >> (EXTRA - 1)) & 1ull) != 0;
        else             guard = ((v.hi >> (EXTRA - 65)) & 1ull) != 0;
    
        if (EXTRA > 1) {
          // sticky = any lower bits below guard
          if (EXTRA <= 64) {
            uint64_t mask = (EXTRA == 64) ? ~0ull : ((1ull << (EXTRA - 1)) - 1);
            sticky = (v.lo & mask) != 0;
          } else {
            sticky = (v.lo != 0);
            int hibits = EXTRA - 65;
            if (hibits > 0) {
              uint64_t mask = (hibits >= 64) ? ~0ull : ((1ull << hibits) - 1);
              sticky = sticky || ((v.hi & mask) != 0);
            }
          }
        }
      }
    
      uint64_t m64 = main.lo;
    #endif
  
    // Round-to-nearest, ties-to-even:
    // round_up = guard && (sticky || (lsb is 1))
    bool lsb_odd = (m64 & 1ull) != 0;
    bool round_up = guard && (sticky || lsb_odd);
  
    if (round_up) {
      uint64_t old = m64;
      m64 += 1;
  
      // BUGFIX:
      // If old was 0xFFFFFFFFFFFFFFFF, increment wraps to 0.
      // That indicates an overflow into bit64 and must carry into the exponent.
      if (m64 == 0 && old == 0xFFFFFFFFFFFFFFFFull) {
        m64 = 0x8000000000000000ull; // SIG_INT_BIT
        exp_adjust += 1;
      } else if ((m64 & (0x8000000000000000ull << 1)) != 0) { // carry into bit64
        m64 >>= 1;
        exp_adjust += 1;
      }
    }
  
    out_sig64 = m64;
  } //round_from_extra

  public:
  // Intel-style floating compare condition codes
  // fccG = 0  (greater)
  // fccL = 1  (less)
  // fccE = 2  (equal)
  // fccU = 3  (unordered  if either operand is NaN)
  static X87_EXT80_FORCEINLINE uint32_t compare(const ext80& a_in, const ext80& b_in) {
    ext80 a = a_in;
    ext80 b = b_in;
    a.canonicalize();
    b.canonicalize();
  
    // Unordered if either is NaN
    if (a.is_nan() || b.is_nan())
      return 3; // fccU
  
    // Handle zeros: +0 == -0
    if (a.is_zero() && b.is_zero())
      return 2; // fccE
  
    // Sign-based quick decisions when signs differ
    if (a.sign != b.sign) {
      // negative < positive
      return a.sign ? 1 : 0; // fccL or fccG
    }
  
    // Same sign: compare magnitudes
    // First by exponent
    if (a.exp != b.exp) {
      bool a_less = (a.exp < b.exp);
      if (a.sign) a_less = !a_less; // reverse for negatives
      return a_less ? 1 : 0;        // fccL or fccG
    }
  
    // Same exponent: compare significands
    if (a.sig != b.sig) {
      bool a_less = (a.sig < b.sig);
      if (a.sign) a_less = !a_less; // reverse for negatives
      return a_less ? 1 : 0;        // fccL or fccG
    }
  
    return 2; // fccE
  } //compare

  private:

  // x87 "indefinite" qNaN (typical): sign=1, exp=all 1s, sig=0xC000000000000000
  static X87_EXT80_FORCEINLINE ext80 make_indefinite() {
    ext80 r;
    r.sign = true;
    r.exp  = EXP_INF_NAN;                 // 0x7FFF
    r.sig  = 0xC000000000000000ull;       // integer bit + quiet bit
    return r;
  } //make_indefinite

  // ---- add/sub ----
  static X87_EXT80_FORCEINLINE ext80 addsub(ext80 a, ext80 b, bool subop) {
    a.canonicalize(); b.canonicalize();
  
    // Handle NaNs using the *original* operands (do NOT flip b for subtraction here)
    if (a.is_nan() || b.is_nan()) return nan_binop(a, b);
  
    // For non-NaN values, subtraction is addition with flipped sign on b
    if (subop) b.sign = !b.sign;
  
    if (a.is_inf() || b.is_inf()) {
      if (a.is_inf() && b.is_inf() && a.sign != b.sign) return make_qnan(1);
      return a.is_inf() ? a : b;
    }
  
    if (a.is_zero() && b.is_zero()) return make_zero(a.sign && b.sign);
    if (a.is_zero()) return b;
    if (b.is_zero()) return a;
  
    constexpr int EXTRA = 3;
  
    // Unpack into (unbiased exponent, signed magnitude with EXTRA rounding bits)
    int ea = (a.exp == 0) ? (1 - EXP_BIAS) : (int(a.exp) - EXP_BIAS);
    int eb = (b.exp == 0) ? (1 - EXP_BIAS) : (int(b.exp) - EXP_BIAS);
  
    u128 sa = u128_from64((a.exp == 0) ? (a.sig & SIG_FRAC_MASK) : a.sig);
    u128 sb = u128_from64((b.exp == 0) ? (b.sig & SIG_FRAC_MASK) : b.sig);
    sa = shl(sa, EXTRA);
    sb = shl(sb, EXTRA);
  
    int e = ea;
    if (eb > ea) { e = eb; sa = shr_sticky(sa, eb - ea); }
    else if (ea > eb) { e = ea; sb = shr_sticky(sb, ea - eb); }
  
    // Signed add/sub by magnitude
    bool out_sign = false;
    #if defined(X87_HAS_NATIVE_U128)
    u128 mag = 0;
    #else
    u128 mag{0,0};
    #endif
  
    if (a.sign == b.sign) {
  #if defined(X87_HAS_NATIVE_U128)
      mag = (u128)(sa) + (u128)(sb);
  #else
      mag = add(sa, sb);
  #endif
      out_sign = a.sign;
    } else {
  #if defined(X87_HAS_NATIVE_U128)
      if ((u128)sa >= (u128)sb) { mag = (u128)sa - (u128)sb; out_sign = a.sign; }
      else                      { mag = (u128)sb - (u128)sa; out_sign = b.sign; }
      if (mag == 0) return make_zero(false);
  #else
      if (ge(sa, sb)) { mag = sub(sa, sb); out_sign = a.sign; }
      else            { mag = sub(sb, sa); out_sign = b.sign; }
      if (u128_is_zero(mag)) return make_zero(false);
  #endif
    }
  
    // Normalize to put MSB at bit (63+EXTRA)
    int p = msb_index_u128(mag);
    if (p < 0) return make_zero(out_sign);
    int sh = p - (63 + EXTRA);
    if (sh > 0) { mag = shr_sticky(mag, sh); e += sh; }
    else if (sh < 0) { mag = shl(mag, -sh); e += sh; }
  
    // Round down to 64-bit sig
    uint64_t sig64 = 0;
    int exp_adj = 0;
    round_from_extra(mag, EXTRA, sig64, exp_adj);
    e += exp_adj;
  
    ext80 r;
    r.sign = out_sign;
  
    int exp_biased = e + EXP_BIAS;
    if (exp_biased >= EXP_INF_NAN) return make_inf(out_sign);
    if (exp_biased <= 0) {
      int rshift = 1 - exp_biased;
      u128 m = u128_from64(sig64);
      m = shr_sticky(m, rshift);
      r.exp = 0;
      r.sig = u128_lo(m) & SIG_FRAC_MASK;
      if (r.sig == 0) return make_zero(out_sign);
      return r;
    }
  
    r.exp = (uint16_t)exp_biased;
    r.sig = sig64;
    r.normalize();
    ext80::apply_fp_control(r);
    return r;
  } //addsub

  // ---- multiply (fixed rounding: keep EXTRA bits during normalization) ----
  static X87_EXT80_FORCEINLINE ext80 mul(ext80 a, ext80 b) {
    a.canonicalize(); b.canonicalize();
  
    if (a.is_nan() || b.is_nan()) return nan_binop(a, b);
    if ((a.is_zero() && b.is_inf()) || (b.is_zero() && a.is_inf())) return make_indefinite();
    if (a.is_inf() || b.is_inf()) return make_inf(a.sign ^ b.sign);
    if (a.is_zero() || b.is_zero()) return make_zero(a.sign ^ b.sign);
  
    int ea = (a.exp == 0) ? (1 - EXP_BIAS) : (int(a.exp) - EXP_BIAS);
    int eb = (b.exp == 0) ? (1 - EXP_BIAS) : (int(b.exp) - EXP_BIAS);
    uint64_t sa = (a.exp == 0) ? (a.sig & SIG_FRAC_MASK) : a.sig;
    uint64_t sb = (b.exp == 0) ? (b.sig & SIG_FRAC_MASK) : b.sig;
  
    u128 prod = mul64(sa, sb);                 // exact 128-bit product
    int p = msb_index_u128(prod);
    if (p < 0) return make_zero(a.sign ^ b.sign);
  
    // Keep EXTRA low bits for rounding *while* normalizing.
    constexpr int EXTRA = 3;
  
    // Shift so MSB ends up at bit (63+EXTRA).
    // The low EXTRA bits become true guard/round/sticky bits after shr_sticky().
    int shift_keep = p - (63 + EXTRA);
    u128 v = (shift_keep > 0) ? shr_sticky(prod, shift_keep) : shl(prod, -shift_keep);
  
    uint64_t sig64 = 0;
    int exp_adj = 0;
    round_from_extra(v, EXTRA, sig64, exp_adj);
  
    // Exponent derivation:
    // value = (sa * 2^(ea-63)) * (sb * 2^(eb-63)) = (sa*sb) * 2^(ea+eb-126)
    // Let sh = p - 63.  We used shift_keep = p - (63+EXTRA) = sh - EXTRA.
    // So sh = shift_keep + EXTRA.
    int sh = shift_keep + EXTRA;
    int e_out = ea + eb - 63 + sh + exp_adj;
  
    ext80 r;
    r.sign = a.sign ^ b.sign;
  
    int exp_biased = e_out + EXP_BIAS;
    if (exp_biased >= EXP_INF_NAN) return make_inf(r.sign);
    if (exp_biased <= 0) {
      int rshift = 1 - exp_biased;
      u128 m = u128_from64(sig64);
      m = shr_sticky(m, rshift);
      r.exp = 0;
      r.sig = u128_lo(m) & SIG_FRAC_MASK;
      if (r.sig == 0) return make_zero(r.sign);
      return r;
    }
  
    r.exp = (uint16_t)exp_biased;
    r.sig = sig64;
    r.normalize();
    apply_fp_control(r);
    return r;
  } //mul
  
  // ---- divide (correct: avoids >128-bit numerator; keeps EXTRA bits via remainder) ----
  static X87_EXT80_FORCEINLINE ext80 div(ext80 a, ext80 b) {
    a.canonicalize(); b.canonicalize();
  
    if (a.is_nan() || b.is_nan()) return nan_binop(a, b);
    if (a.is_inf() && b.is_inf()) return make_indefinite();
    if (a.is_zero() && b.is_zero()) return make_indefinite();
    if (b.is_zero()) return make_inf(a.sign ^ b.sign);
    if (a.is_zero()) return make_zero(a.sign ^ b.sign);
    if (a.is_inf())  return make_inf(a.sign ^ b.sign);
    if (b.is_inf())  return make_zero(a.sign ^ b.sign);
  
    int ea = (a.exp == 0) ? (1 - EXP_BIAS) : (int(a.exp) - EXP_BIAS);
    int eb = (b.exp == 0) ? (1 - EXP_BIAS) : (int(b.exp) - EXP_BIAS);
    uint64_t sa = (a.exp == 0) ? (a.sig & SIG_FRAC_MASK) : a.sig;
    uint64_t sb = (b.exp == 0) ? (b.sig & SIG_FRAC_MASK) : b.sig;
  
    constexpr int EXTRA = 3;
  
    // Step 1: q0 ≈ (sa/sb) * 2^63. This numerator fits in 128 bits (max 2^127).
    u128 num0 = u128_from64(sa);
    num0 = shl(num0, 63);
  
    uint64_t rem0 = 0;
    u128 q0_u = div_u128_u64(num0, sb, rem0);
    uint64_t q0 = u128_lo(q0_u); // q0 fits in 64 bits for normalized inputs
  
    // Step 2: get EXTRA more quotient bits from remainder.
    // qext = floor((rem0 * 2^EXTRA) / sb)
    u128 num1 = u128_from64(rem0);
    num1 = shl(num1, EXTRA);
  
    uint64_t rem1 = 0;
    u128 qext_u = div_u128_u64(num1, sb, rem1);
    uint64_t qext = u128_lo(qext_u) & ((1u << EXTRA) - 1);
  
    // Build v = (q0 << EXTRA) | qext, and fold leftover remainder into sticky.
    u128 v = u128_from64(q0);
    v = shl(v, EXTRA);
  #if defined(X87_HAS_NATIVE_U128)
    v |= (u128)qext;
    if (rem1) v |= 1;
  #else
    v.lo |= qext;
    if (rem1) v.lo |= 1;
  #endif
  
    // Normalize so MSB ends up at bit (63+EXTRA), preserving sticky in low bits.
    int p = msb_index_u128(v);
    if (p < 0) return make_zero(a.sign ^ b.sign);
  
    int shift_keep = p - (63 + EXTRA);
    v = (shift_keep > 0) ? shr_sticky(v, shift_keep) : shl(v, -shift_keep);
  
    uint64_t sig64 = 0;
    int exp_adj = 0;
    round_from_extra(v, EXTRA, sig64, exp_adj);
  
    // Exponent:
    // q0 computed with scale 2^63, then we appended EXTRA bits => overall scale 2^(63+EXTRA).
    // After normalization by shift_keep, value ≈ sig64 * 2^(ea-eb + shift_keep - EXTRA) * 2^0 ??? (careful)
    //
    // Cleaner derivation:
    // Let V = (q0<<EXTRA) + qext ≈ (sa/sb) * 2^(63+EXTRA)
    // After normalization shift_keep: V_norm = V * 2^(-shift_keep)
    // round_from_extra takes V_norm and returns sig64 approximating V_norm / 2^EXTRA
    // i.e., sig64 ≈ (sa/sb) * 2^(63 + EXTRA - shift_keep) / 2^EXTRA
    // => sig64 ≈ (sa/sb) * 2^(63 - shift_keep)
    // quotient value = (sa/sb) * 2^(ea-eb)
    // ext80 packs: value = sig64 * 2^(e_out - 63)
    // => e_out - 63 = (ea-eb) + shift_keep
    int e_out = (ea - eb) + shift_keep + exp_adj;
  
    ext80 r;
    r.sign = a.sign ^ b.sign;
  
    int exp_biased = e_out + EXP_BIAS;
    if (exp_biased >= EXP_INF_NAN) return make_inf(r.sign);
    if (exp_biased <= 0) {
      int rshift = 1 - exp_biased;
      u128 m = u128_from64(sig64);
      m = shr_sticky(m, rshift);
      r.exp = 0;
      r.sig = u128_lo(m) & SIG_FRAC_MASK;
      if (r.sig == 0) return make_zero(r.sign);
      return r;
    }
  
    r.exp = (uint16_t)exp_biased;
    r.sig = sig64;
    r.normalize();
    apply_fp_control(r);
    return r;
  } //div
}; //ext80

} // namespace x87

// ------------------------
// Tiny test harness
// ------------------------
#ifdef X87_EXT80_TEST
#include <cstdio>

static void dump10(const x87::ext80& v) {
  uint8_t b[10]; v.to_bytes_le(b);
  for (int i = 9; i >= 0; --i) std::printf("%02X", b[i]);
}

int main() {
  using x87::ext80;

  auto a = ext80::from_double(1.5);
  auto b = ext80::from_double(2.25);
  auto c = a + b;
  auto d = a * b;
  auto e = ext80::sqrt(b);

  std::printf("a=1.5        bytes="); dump10(a); std::printf("  to_double=%.17g\n", a.to_double());
  std::printf("b=2.25       bytes="); dump10(b); std::printf("  to_double=%.17g\n", b.to_double());
  std::printf("c=a+b        bytes="); dump10(c); std::printf("  to_double=%.17g\n", c.to_double());
  std::printf("d=a*b        bytes="); dump10(d); std::printf("  to_double=%.17g\n", d.to_double());
  std::printf("e=sqrt(b)    bytes="); dump10(e); std::printf("  to_double=%.17g\n", e.to_double());

  auto inf  = ext80::make_inf(false);
  auto ninf = ext80::make_inf(true);
  auto z    = ext80::make_zero(false);

  auto t1 = inf + ninf; // NaN
  auto t2 = inf * z;    // NaN
  auto t3 = z / z;      // NaN
  auto t4 = inf / b;    // inf
  auto t5 = b / inf;    // 0
  auto t6 = ext80::sqrt(ninf); // NaN

  std::printf("inf + -inf => class=%d (nan=4)\n", (int)t1.classify());
  std::printf("inf * 0    => class=%d (nan=4)\n", (int)t2.classify());
  std::printf("0/0        => class=%d (nan=4)\n", (int)t3.classify());
  std::printf("inf/2.25   => class=%d sign=%d\n", (int)t4.classify(), (int)t4.sign);
  std::printf("2.25/inf   => class=%d (zero=0)\n", (int)t5.classify());
  std::printf("sqrt(-inf) => class=%d (nan=4)\n", (int)t6.classify());

  return 0;
}
#endif

#endif // X87_EXT80_HPP_INCLUDED
