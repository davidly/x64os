/*
    This is an AMD64 emulator. It supports just real mode, long mode, 64-bit mode.
    Integer, x87, and SSE2 are partially implemented. No other vector instructions are implemented at all (MMX/AVX/etc.).
    That's a tiny fraction of the CPU but enough to run the regression test static Linux binaries.
    Tested with C/ASM regression tests in the c_tests folder, Fortran tests in f_tests, and Rust tests in rust_tests.
    Also tested running nested emulators and their regression tests: this one (x64os), sparcos, m68, rvos, armos, ntvao, ntvcm, ntvdm
    Builds and runs on both little and big endian machines for 32 and 64 bit. set TARGET_BIG_ENDIAN for those machines.

    Written by David Lee in October & November 2025

    Useful:
        https://www.felixcloutier.com/x86/
        https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-1-2a-2b-2c-2d-3a-3b-3c-3d-and-4
        https://wiki.osdev.org/X86-64_Instruction_Encoding#Mandatory_prefix
        http://ref.x86asm.net/coder64.html
        http://ref.x86asm.net/coder64.html#two-byte
*/

#include <stdint.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <chrono>
#include <type_traits>

#include <djl_128.hxx>
#include <djltrace.hxx>

#include "x64.hxx"

using namespace std;

static const uint64_t g_NAN = 0x7ff8000000000000;
#define MY_NAN ( * (double *) & g_NAN )
template <typename T> bool my_isnan( T x ) { return ( FP_NAN == fpclassify( x ) ); } // fpclassify instead of isnan because isnan() takes a double and we don't want type conversions here from long double
template <typename T> bool my_isinf( T x ) { return ( FP_INFINITE == fpclassify( x ) ); }
template <typename T> bool my_issubnormal( T x ) { return ( FP_SUBNORMAL == fpclassify( x ) ); }

#if defined( __clang__ ) && defined( _WIN32 )  // clang 19.1.5 for Windows has a buggy implementation of truncl()
#define truncl( x ) trunc( (double) x )
#endif

static uint32_t g_State = 0;

const uint32_t stateTraceInstructions = 1;
const uint32_t stateEndEmulation = 2;

bool x64::trace_instructions( bool t )
{
    bool prev = ( 0 != ( g_State & stateTraceInstructions ) );
    if ( t )
        g_State |= stateTraceInstructions;
    else
        g_State &= ~stateTraceInstructions;
    return prev;
} //trace_instructions

void x64::end_emulation() { g_State |= stateEndEmulation; }

static const char * register_names[16] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
static const char * register_names32[16] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
static const char * register_names16[16] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di", "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" };
static const char * register_names8[16] = { "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil", "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };
static const char * register_names8_old[8] = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
static const char * xmm_names[16] = { "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" };
static const char * math_names[8] = { "add", "or", "addc", "sbb", "and", "sub", "xor", "cmp" };
static const char * condition_names[16] = { "o", "no", "b", "ae", "e", "ne", "be", "a", "s", "ns", "p", "np", "l", "ge", "le", "g" };
static const char * shift_names[ 8 ] = { "rol", "ror", "rcl", "rcr", "sal", "shr", "!invalid!", "sar" };
static const char * float_d9_e8[ 7 ] = { "fld1", "fldl2t", "fldl2e", "fldpi", "fldgl2", "fldln2", "fldz" };
static const double float_d9_e8_constants[ 7 ] = { 1.0, 3.3219280949, 1.44269504088, 3.14159265358979, 0.301029995664, 0.6931471805599453, 0.0 };
static const char * float_d9_f0[ 8 ] = { "f2xm1 st", "fyl2x st1, st", "fptan st", "fpatan st1, st", "extract st", "fprem1 st st1", "fdecstp", "fincstp" };
static const char * float_d9_f8[ 8 ] = { "fprem st, st1", "fyl2xp1 st1, st", "fsqrt st", "fsincos st", "frndint st", "fscale st, st1", "fsin st", "fcos st" };

template <typename T> inline void do_swap( T & a, T & b ) { T tmp = a; a = b; b = tmp; }

#if defined( __GNUC__ ) && !defined( __APPLE__ ) && !defined( __clang__ )     // bogus warning in g++ (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
#pragma GCC diagnostic ignored "-Wformat="
#endif

void x64::trace_state()
{
    uint64_t rip_save = rip;
    uint8_t op = getui8( rip );
    if ( ( 0x66 == op ) || ( ( op >= 0x40 ) && ( op <= 0x4f ) ) || ( 0xf3 == op ) || ( 0xf2 == op ) )
        return;

//    tracer.TraceBinaryData( getmem( 0x4018cb + 0x2b40d0 ), 8, 2 );

    uint64_t ip = ( 0 == _prefix_rex ) ? rip : ( rip - 1 );
    if ( 0x66 == _prefix_size )
        ip--;
    if ( 0 != _prefix_sse2_repeat )
        ip--;

    rip++;

    static const char * previous_symbol = 0;
    uint64_t offset;
    const char * symbol_name = emulator_symbol_lookup( ip, offset );
    if ( symbol_name == previous_symbol )
        symbol_name = "";
    else
        previous_symbol = symbol_name;

    char symbol_offset[40];
    symbol_offset[ 0 ] = 0;

    if ( 0 != symbol_name[ 0 ] )
    {
        if ( 0 != offset )
            snprintf( symbol_offset, _countof( symbol_offset ), " + %llx", offset );
        strcat( symbol_offset, "\n             " );
    }

    static char reg_string[ 34 * 32 ];
    reg_string[ 0 ] = 0;
    int len = 0;
    for ( int r = 0; r <= 15; r++ )
        if ( 0 != regs[ r ].q )
            len += snprintf( & reg_string[ len ], 32, "%s:%llx ", register_names[ r ], regs[ r ].q );

#if 0 // too verbose and almost never used except for glibc using fs for global state
    if ( 0 != fs )
        len += snprintf( & reg_string[ len ], 32, "fs:%llx ", fs );

    if ( 0 != gs )
        len += snprintf( & reg_string[ len ], 32, "gs:%llx ", gs );
#endif

    tracer.Trace( "rip %8llx %s%s %02x %02x %02x %02x %02x %s%s => ", ip, symbol_name, symbol_offset,
                  getui8( ip ), getui8( ip + 1 ), getui8( ip + 2 ), getui8( ip + 3 ), getui8( ip + 4 ), reg_string, render_flags() );

    switch( op )
    {
        case 0x00: case 0x08: case 0x10: case 0x18: case 0x20: case 0x28: case 0x30: case 0x38:  // math r/m8, r8. rex math r/m8, r8 sign-extended to 64 bits
        {
            decode_rm();
            uint8_t math = ( op >> 3 ) & 7;
            tracer.Trace( "%sb %s, %s\n", math_names[ math ], rm_string( 1 ), register_name( _reg, 1 ) );
            break;
        }
        case 0x01: case 0x09: case 0x11: case 0x19: case 0x21: case 0x29: case 0x31: case 0x39:  // math r/m, r (32 or 64 bit depending on _rexW)
        {
            decode_rm();
            uint8_t math = ( op >> 3 ) & 7;
            if ( 0x66 == _prefix_size )
                tracer.Trace( "%s %s, %s\n", math_names[ math ], rm_string( 8 ), register_name( _reg, 2 ) );
            else
                tracer.Trace( "%s %s, %s\n", math_names[ math ], rm_string( _rexW ? 8 : 4 ), register_name( _reg, ( _rexW ? 8 : 4 ) ) );
            break;
        }
        case 0x02: case 0x0a: case 0x12: case 0x1a: case 0x22: case 0x2a: case 0x32: case 0x3a:  // math r8, rm/8. rex math r/m8 sign-extended to 64 bits
        {
            decode_rm();
            uint8_t math = ( op >> 3 ) & 7;
            tracer.Trace( "%sb %s, %s\n", math_names[ math ], register_name( _reg, ( _rexW ? 8 : 4 ) ), rm_string( 8 ) );
            break;
        }
        case 0x03: case 0x0b: case 0x13: case 0x1b: case 0x23: case 0x2b: case 0x33: case 0x3b: // math r, r/m (32 or 64 bit depending on _rexW)
        {
            decode_rm();
            uint8_t math = ( op >> 3 ) & 7;
            if ( 0x66 == _prefix_size )
                tracer.Trace( "%s %s, %s\n", math_names[ math ], register_name( _reg, 2 ), rm_string( 2 ) );
            else
                tracer.Trace( "%s %s, %s\n", math_names[ math ], register_name( _reg, ( _rexW ? 8 : 4 ) ), rm_string( 8 ) );
            break;
        }
        case 0x04: case 0x0c: case 0x14: case 0x1c: case 0x24: case 0x2c: case 0x34: case 0x3c: // math al, imm8
        {
            uint8_t math = ( op >> 3 ) & 7;
            tracer.Trace( "%s al, %#x\n", math_names[ math ], get_rip8() );
            break;
        }
        case 0x05: case 0x0d: case 0x15: case 0x1d: case 0x25: case 0x2d: case 0x35: case 0x3d: // math ax, imm16 / math eax, imm32 / math rax, se( imm32 )
        {
            uint8_t math = ( op >> 3 ) & 7;
            decode_rex();
            if ( 0x66 == _prefix_size )
            {
                uint16_t imm = (int16_t) (int16_t) get_rip16();
                tracer.Trace( "%sw ax, %#x\n", math_names[ math ], imm );
            }
            else
            {
                uint32_t imm = (int32_t) get_rip32();
                if ( _rexW )
                    tracer.Trace( "%sq rax, %#llx\n", math_names[ math ], sign_extend( imm, 31 ) );
                else
                    tracer.Trace( "%sd eax, %#x\n", math_names[ math ], imm );
            }
            break;
        }
        case 0x0f:
        {
            uint8_t op1 = get_rip8();

            switch( op1 )
            {
                case 5:
                {
                    tracer.Trace( "syscall\n" );
                    break;
                }
                case 0x0d:
                {
                    decode_rm();
                    switch ( _reg )
                    {
                        case 1: { tracer.Trace( "prefetchw\n" ); }
                        default: { unhandled(); }
                    }
                    break;
                }
                case 0x10:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // movupd xmm1, xmm2/m128 move 128 bits of unaligned double from xmm2/mem to xmm1
                        tracer.Trace( "movupd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else if ( 0xf2 == _prefix_sse2_repeat ) // movsd xmm1, xmm2/m64. move scalar double from source to xmm1
                        tracer.Trace( "movsd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else if ( 0xf3 == _prefix_sse2_repeat ) // movss xmm1, xmm2/m32
                        tracer.Trace( "movss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else // movups xmm, xmm/m128. move unaligned 128 bits of single precision fp from xmm/mem to xmm
                        tracer.Trace( "movups %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    break;
                }
                case 0x11:
                {
                    decode_rm();
                    if ( 0xf2 == _prefix_sse2_repeat ) // movsd xmm1/m64, xmm2  move scalar double from xmm2 to xmm1/m64
                        tracer.Trace( "movsd %s, %s\n", rm_string( 8, true ), xmm_names[ _reg ] );
                    else if ( 0xf3 == _prefix_sse2_repeat ) // movss xmm1/m64, xmm2  move scalar float from xmm2 to xmm1/m64
                        tracer.Trace( "movss %s, %s\n", rm_string( 4, true ), xmm_names[ _reg ] );
                    else if ( 0x66 == _prefix_size ) // movupd xmm2/m128, xmm1   move 128 bits of unaligned packed double precision fp from xmm1 to xmm2/mem
                        tracer.Trace( "movupd %s, %s\n", rm_string( 8, true ), xmm_names[ _reg ] );
                    else   // movups xmm2/m128, xmm1   move 128 bits of unaligned packed single precision fp from xmm1 to xmm2/mem
                        tracer.Trace( "movups %s, %s\n", rm_string( 4, true ), xmm_names[ _reg ] );
                    break;
                }
                case 0x12:
                {
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // movlpd xmm1, m64. moves double from m64 to low qword of xmm1 and doesn't touch high qword
                        tracer.Trace( "movlpd %s, %s\n", xmm_names[ _reg ], rm_string( 8 ) );
                    else if ( 3 == _mod ) // movhlps xmm1, xmm2   move two packed floats from high qw of xmm2 to low qw of xmm1
                        tracer.Trace( "movhlps %s, %s\n", xmm_names[ _reg ], rm_string( 4 ) );
                    else // movlps xmm1, m64   move two floats from m64 to low qw of xmm1
                        tracer.Trace( "movlps %s, %s\n", xmm_names[ _reg ], rm_string( 4 ) );
                    break;
                }
                case 0x13:
                {
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // movlpd m64, xmm1  moves double from m64 to low qword of xmm1 and doesn't touch high qword
                        tracer.Trace( "movlpd %s, %s\n", rm_string( 8 ), xmm_names[ _reg ] );
                    else // movlps m64, xmm1
                        tracer.Trace( "movlps %s, %s\n", rm_string( 4 ), xmm_names[ _reg ] );
                    break;
                }
                case 0x14:
                {
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // unpcklpd xmm1, xmm2/m128 unpack doubles from low of xmm1 and xmm2/m128
                        tracer.Trace( "unpcklpd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else // unpcklps xmm1, xmm2/m128 unpack singles from low of xmm1 and xmm2/m128
                        tracer.Trace( "unpcklps %s, %s\n", xmm_names[ _reg ], rm_string( 5, true ) );
                    break;
                }
                case 0x15:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // unpckhpd xmm1, xmm2/m128
                        tracer.Trace( "unpckhpd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else // unpckhps xmm1, xmm2/m128
                        tracer.Trace( "unpckhps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    break;
                }
                case 0x16:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // movhpd xmm1, m64. moves double from m64 to high qword of xmm1 and doesn't touch low qword
                        tracer.Trace( "movhpd %s, %s\n", xmm_names[ _reg ], rm_string( 8 ) );
                    else   // movlhps xmm1, m64  or movlhps xmm1, xmm2
                        tracer.Trace( "movlhps %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    break;
                }
                case 0x17:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // movhpd m64, xmm1. moves double from high qword of xmm1 to m64
                        tracer.Trace( "movhpd %s, %s\n", rm_string( 8 ), xmm_names[ _reg ] );
                    else   // movlhps m64, xmm1  or movlhps xmm1, xmm2
                        tracer.Trace( "movhps %s, %s\n", rm_string( 8, true ), xmm_names[ _reg ] );
                    break;
                }
                case 0x18:
                {
                    decode_rm();
                    switch ( _reg )
                    {
                        case 0: case 1: case 2: case 3: { tracer.Trace( "prefetch\n" ); }
                        default: { unhandled(); }
                    }
                    break;
                }
                case 0x1e:
                {
                    uint8_t op2 = getui8( rip );
                    if ( 0xfa == op2 ) // endbr64
                        tracer.Trace( "endbr64\n" );
                    else
                    {
                        decode_rm();
                        if ( 1 == _reg ) // rdsspq / rdsspd
                            tracer.Trace( "rdsspq\n" );
                        else
                            unhandled();
                    }
                    break;
                }
                case 0x1f:
                {
                    tracer.Trace( "nopl\n" );
                    break;
                }
                case 0x28: // movaps xmm, xmm/m128   move aligned packed single precision fp values from xmm2/m128 to xmm1. and movapd
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "movapd %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else
                        tracer.Trace( "movaps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    break;
                }
                case 0x29:
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();

                    if ( 0x66 == _prefix_size ) // movapd xmm2/m128, xmm1
                        tracer.Trace( "movapd %s, %s\n", rm_string( 8, true ), xmm_names[ _reg ] );
                    else   // movaps xmm/m128, xmm   move aligned packed single precision fp values from xmm1 to xmm2/m128
                        tracer.Trace( "movaps %s, %s\n", rm_string( 4, true ), xmm_names[ _reg ] );
                    break;
                }
                case 0x2a: // cvtsi2sd xmm1, r32/m32   convert dword to scalar double
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        unhandled();
                    if ( 0xf2 == _prefix_sse2_repeat )
                        tracer.Trace( "cvtsi2sd %s, %s\n", xmm_names[ _reg ], rm_string( op_width() ) );
                    else if ( 0xf3 == _prefix_sse2_repeat )
                        tracer.Trace( "cvtsi2ss %s, %s\n", xmm_names[ _reg ], rm_string( op_width() ) );
                    else
                        unhandled();
                    break;
                }
                case 0x2c: // cvttsd2si r32/r64, xmm1/m64  convert scalar double to signed integer
                {
                    decode_rm();
                    if ( 0xf2 == _prefix_sse2_repeat )
                        tracer.Trace( "cvttsd2si %s, %s\n", register_name( _reg, _rexW ? 8 : 4 ), rm_string( 8, true ) );
                    else if ( 0xf3 == _prefix_sse2_repeat )
                        tracer.Trace( "cvttss2si %s, %s\n", register_name( _reg, _rexW ? 8 : 4 ), rm_string( 4, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x2e: // ucomisd xmm1, xmm2/m64   compare low doubles and set eflags accordingly
                {
                    decode_rm();
                    if ( 0 == _prefix_sse2_repeat )
                    {
                        if ( 0x66 == _prefix_size )
                            tracer.Trace( "ucomisd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                        else
                            tracer.Trace( "ucomiss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    }
                    else
                        unhandled();
                    break;
                }
                case 0x2f:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )// comisd xmm1, xmm2/m32  compare low double scalar values and set eflags
                        tracer.Trace( "comisd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else if ( 0 == _prefix_sse2_repeat ) // comiss xmm1, xmm2/m32  compare low float scalar values and set eflags
                        tracer.Trace( "comiss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: // cmovcc reg, r/m
                case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
                {
                    decode_rm();
                    tracer.Trace( "cmov%s %s, %s\n", condition_names[ op1 & 0xf ], register_name( _reg, ( _rexW ? 8 : 4 ) ), rm_string( 8 ) );
                    break;
                }
                case 0x50:
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    if ( 0x66 == _prefix_size ) // movmskpd reg, xmm. extract 2-bit sign mask from xmm and store in reg. the upper bits are filled with zeroes
                        tracer.Trace( "movmskpd %s, %s\n", register_names[ _reg ], rm_string( 8, true ) );
                    else // movmkps reg, xmm    extract 4 bit sign mask from xmm and store in reg
                        tracer.Trace( "movmkps %s, %s\n", register_names[ _reg ], rm_string( 8, true ) );
                    break;
                }
                case 0x51:
                {
                    decode_rm();
                    if ( 0xf2 == _prefix_sse2_repeat ) // sqrtsd xmm1, xmm2/m64  sqrt of low double
                        tracer.Trace( "sqrtsd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else if ( 0xf3 == _prefix_sse2_repeat ) // sqrtss xmm1 / xmm2/m32  sqrt of low float
                        tracer.Trace( "sqrtss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else if ( 0x66 == _prefix_size ) // sqrtpd xmm1, xmm2/m128  compute sqrt of packed doubles
                        tracer.Trace( "sqrtpd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else if ( 0 == _prefix_sse2_repeat && 0 == _prefix_size ) // sqrtps xmm1, xmm2/m128. compute sqrt of packed singles
                        tracer.Trace( "sqrtps %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x52:
                {
                    decode_rm();
                    if ( 0 != _prefix_size || 0 != _prefix_sse2_repeat )
                        unhandled();
                    // rsqrtps xmm1, xmm2/m128 compute reciprocals of the square roots of packed floats
                    tracer.Trace( "rsqrtps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    break;
                }
                case 0x54: // andpd xmm, xmm/m128   bitwise and of doubles and singles
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "andpd %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else
                        tracer.Trace( "andps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    break;
                }
                case 0x55: // andnps/andnpd xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    bool wide = ( 0x66 == _prefix_size );
                    tracer.Trace( "andnp%c %s, %s\n", wide ? 'd' : 's', xmm_names[ _reg ], rm_string( wide ? 8 : 4, true ) );
                    break;
                }
                case 0x56: // orps/orpd xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    bool wide = ( 0x66 == _prefix_size );
                    tracer.Trace( "orp%c %s, %s\n", wide ? 'd' : 's', xmm_names[ _reg ], rm_string( wide ? 8 : 4, true ) );
                    break;
                }
                case 0x57: // xorpd/xorps xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    bool wide = ( 0x66 == _prefix_size );
                    tracer.Trace( "xorp%c %s, %s\n", wide ? 'd' : 's', xmm_names[ _reg ], rm_string( wide ? 8 : 4, true ) );
                    break;
                }
                case 0x58:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // addpd xmm1, xmm2/m128   add packed double values
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        tracer.Trace( "addpd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    }
                    else
                    {
                        if ( 0xf2 == _prefix_sse2_repeat ) // addsd xmm1, xmm2/m64
                            tracer.Trace( "addsd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // addss xmm1, xmm2/m64
                            tracer.Trace( "addss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else if ( 0 == _prefix_sse2_repeat )
                            tracer.Trace( "addps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else
                            unhandled();
                    }
                    break;
                }
                case 0x59:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // mulpd xmm1, xmm2/m128   multiply packed double values
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        tracer.Trace( "mulpd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    }
                    else
                    {
                        if ( 0xf2 == _prefix_sse2_repeat ) // mulsd xmm1, xmm2/m64
                            tracer.Trace( "mulsd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // mulss xmm1, xmm2/m64
                            tracer.Trace( "mulss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else if ( 0 == _prefix_sse2_repeat )
                            tracer.Trace( "mulps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else
                            unhandled();
                    }
                    break;
                }
                case 0x5a:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // cvtpd2ps xmm1, xmm2/m128   convert two packed doubles in 2 to two floats in 1
                        tracer.Trace( "cvtpd2ps %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else if ( 0xf3 == _prefix_sse2_repeat ) // cvtss2sd xmm1, xmm2/m32   convert scalar float to scalar double
                        tracer.Trace( "cvtss2sd %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else if ( 0xf2 == _prefix_sse2_repeat ) // cvtsd2ss xmm1, xmm2/m32   convert scalar double to scalar float
                        tracer.Trace( "cvtsd2ss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else // cvtps2pd xmm1, xmm2/m64 convert two packed floats in 2 to two packed doubles in 1
                        tracer.Trace( "cvtps2pd %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    break;
                }
                case 0x5b:
                {
                    decode_rm();
                    if ( 0 != _prefix_size )
                        unhandled();
                    if ( 0xf3 == _prefix_sse2_repeat ) // cvttps2dq xmm1, xmm2/m128   convert 4 packed floats to signed dwords using trucation
                        tracer.Trace( "cvttps2dq %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else if ( 0 == _prefix_sse2_repeat ) // cvtdq2ps xmm1, xmm2/m128   convert 4 packed signed dwords from xmm2/m128 t 4 packed signed floats in xmm1
                        tracer.Trace( "cvtdq2ps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x5c:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // subpd xmm1, xmm2/m128   subtract packed double values
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        tracer.Trace( "subpd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    }
                    else
                    {
                        if ( 0xf2 == _prefix_sse2_repeat ) // subsd xmm1, xmm2/m64
                            tracer.Trace( "subsd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // subss xmm1, xmm2/m64
                            tracer.Trace( "subss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else if ( 0 == _prefix_sse2_repeat )
                            tracer.Trace( "subps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else
                            unhandled();
                    }
                    break;
                }
                case 0x5d:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // minpd xmm1, xmm2/m128   min packed double values
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        tracer.Trace( "minpd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    }
                    else
                    {
                        if ( 0xf2 == _prefix_sse2_repeat ) // minsd xmm1, xmm2/m64
                            tracer.Trace( "minsd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // minss xmm1, xmm2/m64
                            tracer.Trace( "minss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else if ( 0 == _prefix_sse2_repeat )
                            tracer.Trace( "minps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else
                            unhandled();
                    }
                    break;
                }
                case 0x5e:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // divpd xmm1, xmm2/m128   divide packed double values
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        tracer.Trace( "divpd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    }
                    else
                    {
                        if ( 0xf2 == _prefix_sse2_repeat ) // divsd xmm1, xmm2/m64
                            tracer.Trace( "divsd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // divss xmm1, xmm2/m64
                            tracer.Trace( "divss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else if ( 0 == _prefix_sse2_repeat )
                            tracer.Trace( "divps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else
                            unhandled();
                    }
                    break;
                }
                case 0x5f:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // maxpd xmm1, xmm2/m128   max packed double values
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        tracer.Trace( "maxpd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    }
                    else
                    {
                        if ( 0xf2 == _prefix_sse2_repeat ) // maxsd xmm1, xmm2/m64
                            tracer.Trace( "maxsd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // maxss xmm1, xmm2/m64
                            tracer.Trace( "maxss %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else if ( 0 == _prefix_sse2_repeat )
                            tracer.Trace( "maxps %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                        else
                            unhandled();
                    }
                    break;
                }
                case 0x60: // punpcklbw xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "punpcklbw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x61: // punpcklwd xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "punpcklwd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x62: // punpckldq xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "punpckldq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x63:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // packsswb xmm1, xmm2/m128   convert 8 packed int16_t from xmm1 and xmm2/m128 into 16 packed int8_t using signed saturation
                        tracer.Trace( "packsswb %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x64: // pcmpgtb xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pcmpgtb %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x65: // pcmpgtw xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pcmpgtw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x66: // pcmpgtd xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pcmpgtd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x67: // packuswb xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "packuswb %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x68: // punpckhbw xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "punpckhbw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x69: // punpckhwd xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "punpckhwd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x6a: // punpckhdq xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "punpckhdq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x6b:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // packssdw xmm1, xmm2/m128  converts 4 packed dwords from xmm1 and xmm2/m128 into 8 packed signed word integers using signed saturation
                        tracer.Trace( "packssdw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x6c: // punpcklqdq xmm, xmm/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "punpcklqdq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x6d: // punpckhqdq xmm, xmm/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "punpckhqdq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x6e: // movd xmm, r/m32   movq xmm, r/m64
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                    {
                        if ( _rexW )
                            tracer.Trace( "movq %s, %s\n", xmm_names[ _reg ], rm_string( 8 ) );
                        else
                            tracer.Trace( "movd %s, %s\n", xmm_names[ _reg ], rm_string( 4 ) );
                    }
                    else
                        unhandled(); // mmx not supported
                    break;
                }
                case 0x6f: // movdqa xmm1, xmm2/m128   move 128 bits of aligned packed integer values from xmm2/m128 to xmm1
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "movdqa %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else if ( 0xf3 == _prefix_sse2_repeat )
                        tracer.Trace( "movqdu %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                    {
                        tracer.Trace( "_prefix_sse2_repeat %#x, _prefix_size %#x\n", _prefix_sse2_repeat, _prefix_size );
                        unhandled(); // mmx not supported
                    }
                    break;
                }
                case 0x70:
                {
                    decode_rm();
                    if ( 0xf2 == _prefix_sse2_repeat ) // pshuflw xmm1, xmm2/m128, imm8
                        tracer.Trace( "pshuflw %s, %s, %#x\n", xmm_names[ _reg ], rm_string( 8, true ), get_rip8() );
                    else if ( 0xf3 == _prefix_sse2_repeat ) // pshufhw xmm1, xmm2/m128, imm8
                        tracer.Trace( "pshufhw %s, %s, %#x\n", xmm_names[ _reg ], rm_string( 8, true ), get_rip8() );
                    else if ( 0x66 == _prefix_size ) // pshufd xmm, xmm/m128, imm8
                        tracer.Trace( "pshufd %s, %s, %#x\n", xmm_names[ _reg ], rm_string( 8, true ), get_rip8() );
                    else
                        unhandled();
                    break;
                }
                case 0x71:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                    {
                        uint8_t shift = get_rip8();
                        if ( 2 == _reg ) // psrlw
                            tracer.Trace( "psrlw %s, %u\n", xmm_names[ _rm ], shift );
                        else if ( 4 == _reg ) // psraw xmm1, imm8 shift signed words
                            tracer.Trace( "psraw %s, %u\n", xmm_names[ _rm ], shift );
                        else if ( 6 == _reg ) // psllw
                            tracer.Trace( "psllw %s, %u\n", xmm_names[ _rm ], shift );
                        else
                            unhandled();
                    }
                    else
                        unhandled();
                    break;
                }
                case 0x72:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                    {
                        uint8_t shift = get_rip8();
                        if ( 2 == _reg ) // psrld xmm1, imm8
                            tracer.Trace( "psrld %s, %u\n", xmm_names[ _rm ], shift );
                        else if ( 4 == _reg ) // psrad
                            tracer.Trace( "psrad %s, %u\n", xmm_names[ _rm ], shift );
                        else if ( 6 == _reg ) // pslld
                            tracer.Trace( "pslld %s, %u\n", xmm_names[ _rm ], shift );
                        else
                            unhandled();
                    }
                    else
                        unhandled();
                    break;
                }
                case 0x73:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                    {
                        if ( 2 == _reg ) // psrlq xmm1, imm8   shift quad right logical
                            tracer.Trace( "psrlq %s, %u\n", xmm_names[ _rm ], get_rip8() );
                        else if ( 3 == _reg ) // psrldq xmm1, imm8   shift dquad right logical
                            tracer.Trace( "psrldq %s, %u\n", xmm_names[ _rm ], get_rip8() );
                        else if ( 6 == _reg ) // psllq xmm1, imm8
                            tracer.Trace( "psllq %s, %u\n", xmm_names[ _rm ], get_rip8() );
                        else if ( 7 == _reg ) // pslldq xmm1, imm8   shift dquad left logical
                            tracer.Trace( "pslldq %s, %u\n", xmm_names[ _rm ], get_rip8() );
                        else
                            unhandled();
                    }
                    else
                        unhandled();
                    break;
                }
                case 0x74:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // pcmpeqb xmm, xmm/m128   compare packed bytes in rm to r for equality. if eq, set byte to 1 else 0
                        tracer.Trace( "pcmpeqb %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x75:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // pcmpeqw xmm, xmm/m128   compare packed words in rm to r for equality. if eq, set byte to 1 else 0
                        tracer.Trace( "pcmpeqw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0x76:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // pcmpeqd xmm/m128, xmm   compare packed doublewords in xmm/m128 and xmm1 for equality
                        tracer.Trace( "pcmpeqd %s, %s\n", rm_string( 8, true ), xmm_names[ _reg ] );
                    else
                        unhandled();
                    break;
                }
                case 0x7e: // movq r/m64, xmm
                {
                    if ( 0xf3 == _prefix_sse2_repeat )
                    {
                        decode_rm();
                        tracer.Trace( "movq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    }
                    else if ( 0x66 == _prefix_size ) // mov r/m, xmm
                    {
                        decode_rm();
                        tracer.Trace( "movq %s, %s\n", rm_string( 8 ), xmm_names[ _reg ] );
                    }
                    else
                        unhandled();
                    break;
                }
                case 0x7f:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size || 0xf3 == _prefix_sse2_repeat ) // movdqa xmm2/m128, xmm1   move aligned packed integer values. or movdqu (unaligned)
                        tracer.Trace( "movdq%c %s, %s\n", ( 0x66 == _prefix_size ) ? 'a' : 'u', rm_string( 8, true ), xmm_names[ _reg ] );
                    else
                        unhandled();
                    break;
                }
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: // jcc rel32
                case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
                {
                    uint64_t disp = sign_extend( get_rip32(), 31 );
                    tracer.Trace( "j%s %lld  # %#llx\n", condition_names[ op1 & 0xf ], disp, rip + disp );
                    break;
                }
                case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: // setcc
                case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
                {
                    decode_rm();
                    tracer.Trace( "set%s %s\n", condition_names[ op1 & 0xf ], rm_string( 1 ) );
                    break;
                }
                case 0xa2: // cpuid
                {
                    tracer.Trace( "cpuid\n" );
                    break;
                }
                case 0xa3: // bt r/m, r 16/32/64
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "bt %s %s\n", rm_string( 2 ), register_name( _reg, 2 ) );
                    else if ( _rexW )
                        tracer.Trace( "bt %s %s\n", rm_string( 8 ), register_name( _reg, 8 ) );
                    else
                        tracer.Trace( "bt %s %s\n", rm_string( 4 ), register_name( _reg, 4 ) );
                    break;
                }
                case 0xa4: // shld r/m, r, imm   double precision left shift and fill with bits from r
                {
                    decode_rm();
                    uint8_t imm = get_rip8();
                    if ( _rexW )
                        tracer.Trace( "shld %s, %s, %u\n", rm_string( 8 ), register_name( _reg, 8 ), imm );
                    else if ( 0x66 == _prefix_size )
                        tracer.Trace( "shld %s, %s, %u\n", rm_string( 2 ), register_name( _reg, 2 ), imm );
                    else
                        tracer.Trace( "shld %s, %s, %u\n", rm_string( 4 ), register_name( _reg, 4 ), imm );
                    break;
                }
                case 0xa5: // shld r/m, r, cl   double precision left shift and fill with bits from r
                {
                    decode_rm();
                    if ( _rexW )
                        tracer.Trace( "shld %s, %s, cl\n", rm_string( 8 ), register_name( _reg, 8 ) );
                    else if ( 0x66 == _prefix_size )
                        tracer.Trace( "shld %s, %s, cl\n", rm_string( 2 ), register_name( _reg, 2 ) );
                    else
                        tracer.Trace( "shld %s, %s, cl\n", rm_string( 4 ), register_name( _reg, 4 ) );
                    break;
                }
                case 0xab: // bts r/m, r 16/32/64
                {
                    decode_rm();
                    if ( _rexW )
                        tracer.Trace( "bts %s %s\n", rm_string( 8 ), register_name( _reg, 8 ) );
                    else if ( 0x66 == _prefix_size )
                        tracer.Trace( "bts %s %s\n", rm_string( 2 ), register_name( _reg, 2 ) );
                    else
                        tracer.Trace( "bts %s %s\n", rm_string( 4 ), register_name( _reg, 4 ) );
                    break;
                }
                case 0xac: // shrd r/m, r, imm   double precision right shift and fill with bits from left
                {
                    decode_rm();
                    uint8_t imm8 = get_rip8();
                    if ( _rexW )
                        tracer.Trace( "shrd %s, %s, %u\n", rm_string( 8 ), register_name( _reg, 8 ), imm8 );
                    else if ( 0x66 == _prefix_size )
                        tracer.Trace( "shrd %s, %s, %u\n", rm_string( 2 ), register_name( _reg, 2 ), imm8 );
                    else
                        tracer.Trace( "shrd %s, %s, %u\n", rm_string( 4 ), register_name( _reg, 4 ), imm8 );
                    break;
                }
                case 0xad: // shrd r/m, r, cl   double precision right shift and fill with bits from r
                {
                    decode_rm();
                    if ( _rexW )
                        tracer.Trace( "shrd %s, %s, cl\n", rm_string( 8 ), register_name( _reg, 8 ) );
                    else if ( 0x66 == _prefix_size )
                        tracer.Trace( "shrd %s, %s, cl\n", rm_string( 2 ), register_name( _reg, 2 ) );
                    else
                        tracer.Trace( "shrd %s, %s, cl\n", rm_string( 4 ), register_name( _reg, 4 ) );
                    break;
                }
                case 0xae: // stmxcsr / ldmxcsr
                {
                    uint8_t imm = get_rip8();
                    if ( 0xf0 == imm )
                        tracer.Trace( "mfence\n" );
                    else if ( 0xf8 == imm )
                        tracer.Trace( "sfence\n" );
                    else
                    {
                        decode_rm();
                        if ( 2 == _reg )
                            tracer.Trace( "ldmxcsr %s\n", rm_string( 8 ) );
                        else if ( 3 == _reg )
                            tracer.Trace( "stmxcsr %s\n", rm_string( 8 ) );
                        else
                            unhandled();
                    }
                    break;
                }
                case 0xaf: // imul r, r/m  in 16, 32, and 64
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "imul %s, %s\n", register_names16[ _reg ], rm_string( 2 ) );
                    else if ( _rexW )
                        tracer.Trace( "imul %s, %s\n", register_names[ _reg ], rm_string( 8 ) );
                    else
                        tracer.Trace( "imul %s, %s\n", register_names32[ _reg ], rm_string( 4 ) );
                    break;
                }
                case 0xb0: // cmpxchg r/m8, r8
                {
                    decode_rm();
                    tracer.Trace( "cmpxchg %s, %s\n", rm_string( 1 ), register_name( _reg, 1 ) );
                    break;
                }
                case 0xb1: // cmpxchg r/m, r   16, 32, 64 compare ax with r/m
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "cmpxchg %s, %s\n", rm_string( 2 ), register_names16[ _reg ] );
                    else if ( _rexW )
                        tracer.Trace( "cmpxchg %s, %s\n", rm_string( 8 ), register_names[ _reg ] );
                    else
                        tracer.Trace( "cmpxchg %s, %s\n", rm_string( 4 ), register_names32[ _reg ] );
                    break;
                }
                case 0xb6: // movzbq reg, r/m8
                {
                    decode_rm();
                    tracer.Trace( "movzxb %s, %s\n", register_name( _reg, ( _rexW ? 8 : 4 ) ), rm_string( 1 ) );
                    break;
                }
                case 0xb7: // movzbq reg, r/m16
                {
                    decode_rm();
                    tracer.Trace( "movzxw %s, %s\n", register_name( _reg, ( _rexW ? 8 : 4 ) ), rm_string( 2 ) );
                    break;
                }
                case 0xb3: // btr r/m, r  (16, 32, 64 bit test and reset)
                {
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "btr %s, %s\n", rm_string( 2 ), register_name( _reg, 2 ) );
                    else if ( _rexW )
                        tracer.Trace( "btr %s, %s\n", rm_string( 8 ), register_name( _reg, 8 ) );
                    else
                        tracer.Trace( "btr %s, %s\n", rm_string( 4 ), register_name( _reg, 4 ) );
                    break;
                }
                case 0xba:
                {
                    decode_rm();
                    uint8_t imm = get_rip8();
                    if ( 4 == _reg ) // bt r/m imm8 (16, 32, 64 bit bit test)
                    {
                        if ( 0x66 == _prefix_size )
                            tracer.Trace( "bt %s, %u\n", rm_string( 2 ), imm );
                        else if ( _rexW )
                            tracer.Trace( "bt %s, %u\n", rm_string( 8 ), imm );
                        else
                            tracer.Trace( "bt %s, %u\n", rm_string( 4 ), imm );
                    }
                    else if ( 5 == _reg ) // bts r/m, imm8  (16, 32, 64 bit test and set)
                    {
                        if ( 0x66 == _prefix_size )
                            tracer.Trace( "bts %s, %u\n", rm_string( 2 ), imm );
                        else if ( _rexW )
                            tracer.Trace( "bts %s, %u\n", rm_string( 8 ), imm );
                        else
                            tracer.Trace( "bts %s, %u\n", rm_string( 4 ), imm );
                    }
                    else if ( 6 == _reg ) // btr r/m, imm8  (16, 32, 64 bit test and reset)
                    {
                        if ( 0x66 == _prefix_size )
                            tracer.Trace( "btr %s, %u\n", rm_string( 2 ), imm );
                        else if ( _rexW )
                            tracer.Trace( "btr %s, %u\n", rm_string( 8 ), imm );
                        else
                            tracer.Trace( "btr %s, %u\n", rm_string( 4 ), imm );
                    }
                    else if ( 7 == _reg ) // btc r/m, imm8  (16, 32, 64 bit test and complement)
                    {
                        if ( 0x66 == _prefix_size )
                            tracer.Trace( "btc %s, %u\n", rm_string( 2 ), imm );
                        else if ( _rexW )
                            tracer.Trace( "btc %s, %u\n", rm_string( 8 ), imm );
                        else
                            tracer.Trace( "btc %s, %u\n", rm_string( 4 ), imm );
                    }
                    else
                        unhandled();
                    break;
                }
                case 0xbc: // bsf r, r/m   16, 32, 64  bit scan forward
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "bsf %s, %s\n", register_names16[ _reg ], rm_string( 2 ) );
                    else if ( _rexW )
                        tracer.Trace( "bsf %s, %s\n", register_names[ _reg ], rm_string( 8 ) );
                    else
                        tracer.Trace( "bsf %s, %s\n", register_names32[ _reg ], rm_string( 4 ) );
                    break;
                }
                case 0xbd: // bsf r, r/m   16, 32, 64  bit scan reverse
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "bsr %s, %s\n", register_names16[ _reg ], rm_string( 2 ) );
                    else if ( _rexW )
                        tracer.Trace( "bsr %s, %s\n", register_names[ _reg ], rm_string( 8 ) );
                    else
                        tracer.Trace( "bsr %s, %s\n", register_names32[ _reg ], rm_string( 4 ) );
                    break;
                }
                case 0xbe: // movsx r, r/m. 16/32/64 from 8
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "movsx %s, %s\n", register_names16[ _reg ], rm_string( 1 ) );
                    else if ( _rexW )
                        tracer.Trace( "movsx %s, %s\n", register_names[ _reg ], rm_string( 1 ) );
                    else
                        tracer.Trace( "movsx %s, %s\n", register_names32[ _reg ], rm_string( 1 ) );
                    break;
                }
                case 0xbf: // movsx r, r/m16. 32/64 from 16
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        unhandled();
                    else if ( _rexW )
                        tracer.Trace( "movsx %s, %s\n", register_names[ _reg ], rm_string( 2 ) );
                    else
                        tracer.Trace( "movsx %s, %s\n", register_names32[ _reg ], rm_string( 2 ) );
                    break;
                }
                case 0xc0: // xadd r/m8, r8
                {
                    decode_rm();
                    tracer.Trace( "xadd %s, %s\n", rm_string( 1 ), register_name( _reg, 1 ) );
                    break;
                }
                case 0xc1: // xadd r/m, r  16/32/64
                {
                    decode_rm();
                    if ( _rexW )
                        tracer.Trace( "xadd %s, %s\n", rm_string( 8 ), register_name( _reg, 8 ) );
                    else if ( 0x66 == _prefix_size )
                        tracer.Trace( "xadd %s, %s\n", rm_string( 2 ), register_name( _reg, 2 ) );
                    else
                        tracer.Trace( "xadd %s, %s\n", rm_string( 4 ), register_name( _reg, 4 ) );
                    break;
                }
                case 0xc2: // cmpps / cmppd / cmpsd / cmpss xmm1, xmm2/m128, imm8   compare floats/doubles using bits2:0 of imm8 as a comparison predicate
                {
                    decode_rm();
                    uint8_t imm = get_rip8();
                    if ( 0x66 == _prefix_size ) // packed double
                        tracer.Trace( "cmppd %s, %s, %#x\n", xmm_names[ _reg ], rm_string( 8, true ), imm );
                    else if ( 0xf2 == _prefix_sse2_repeat ) // scalar double
                        tracer.Trace( "cmpsd %s, %s, %#x\n", xmm_names[ _reg ], rm_string( 8, true ), imm );
                    else if ( 0xf3 == _prefix_sse2_repeat ) // scalar single
                        tracer.Trace( "cmpss %s, %s, %#x\n", xmm_names[ _reg ], rm_string( 8, true ), imm );
                    else // packed single
                        tracer.Trace( "cmpps %s, %s, %#x\n", xmm_names[ _reg ], rm_string( 4, true ), imm );
                    break;
                }
                case 0xc4:
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    uint8_t imm = get_rip8();
                    if ( 0x66 == _prefix_size ) // pinsrw xmm, r32/m16, imm8
                        tracer.Trace( "pinsrw %s, %s, %u\n", xmm_names[ _reg ], rm_string( 2, true ), imm );
                    else
                        unhandled();
                    break;
                }
                case 0xc5: // pextrw reg, xmm, imm8
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pextrw %s %s, %u\n", register_name( _reg, ( _rexW ? 8 : 4 ) ), xmm_names[ _rm ], get_rip8() );
                    else
                        unhandled();
                    break;
                }
                case 0xc6:
                {
                    decode_rm();
                    uint8_t imm8 = get_rip8();
                    if ( 0x66 == _prefix_size ) // shufpd xmm, xmm/m128, imm8
                        tracer.Trace( "shufpd, %s, %s, %#x\n", xmm_names[ _reg ], rm_string( 8, true ), imm8 );
                    else // shufps xmm, xmm/m128, imm8
                        tracer.Trace( "shufps, %s, %s, %#x\n", xmm_names[ _reg ], rm_string( 8, true ), imm8 );
                    break;
                }
                case 0xc8: case 0xc9: case 0xca: case 0xcb: case 0xcc: case 0xcd: case 0xce: case 0xcf: // bswap r32/r64
                {
                    if ( 0 != _prefix_size || 0 != _prefix_sse2_repeat )
                        unhandled();
                    _rm = ( op1 & 7 );
                    decode_rex();
                    tracer.Trace( "bswap %s\n", register_name( _rm, _rexW ? 8 : 4 ) );
                    break;
                }
                case 0xd2:
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    if ( 0x66 == _prefix_size ) // psrld xmm1, xmm2/m128   shift dwords in xmm1 right by amount specified
                        tracer.Trace( "paddq, %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xd3:
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    if ( 0x66 == _prefix_size ) // psrlq xmm1, xmm2/m128   shift qwords right while shifting in 0s
                        tracer.Trace( "psrlq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xd4:
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    if ( 0x66 == _prefix_size )  // paddq xmm1, xmm2/m128
                        tracer.Trace( "paddq, %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xd5:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // pmullw xmm1, xmm2/m128  multiply signed words and store low 16 bits of results
                        tracer.Trace( "pmullw %s, %s\n", xmm_names[ _reg ], rm_string( 2, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xd6: // movq r/m64, xmm
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "movq %s, %s\n", rm_string( 8, true ), xmm_names[ _reg ] );
                    else
                        unhandled();
                    break;
                }
                case 0xd7: // pmovmskb reg, xmm
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pmovmskb %s, %s\n", register_names[ _reg ], xmm_names[ _rm ] );
                    else
                        unhandled();
                    break;
                }
                case 0xd8: // psubsub xmm1, xmm2/m128 subtract unsigned bytes and saturate result
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "psubusb %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xd9: // psubsuw xmm1, xmm2/m128 subtract unsigned words and saturate result
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "psubusw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xda: // pminub xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pminub %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xdb:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // pand xmm1, xmm2/m128
                        tracer.Trace( "pand %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xde: // pmaxub xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pmaxub %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xdf: // pandn xmm1, xmm2/m128
                {
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pandn %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xe2:
                {
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // psrad xmm1, xmm2/m128   shift signed dword in xmm1 right by xmm2/m128
                        tracer.Trace( "psrad %s, %s\n",  xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xe4: // pmulhuw xmm1, xmm2/m128
                {
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pmulhuw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xe5:
                {
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // pmulhw xmm1, xmm2/m128   multiply packed signed words and store high 16 bits of results
                        tracer.Trace( "pmulhuw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xe6:
                {
                    decode_rm();
                    if ( 0xf3 == _prefix_sse2_repeat ) // cvtdq2pd xmm1, xmm2/m64  convert two packed signed dword ints from xmm2/mem to 2 double fp values in xmm1
                        tracer.Trace( "cvtdq2pd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else if ( 0x66 == _prefix_size ) // cvttpd2dq xmm1, xmm2/m128  convert two doubles to two signed dword integers
                        tracer.Trace( "cvttpd2dq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xe7:
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size ) // movntdq m128, xmm1  move packed integers from xmm to mm using non-temporal hint
                        tracer.Trace( "movntdq %s, %s\n", rm_string( 8, true ), xmm_names[ _reg ] );
                    else
                        unhandled();
                    break;
                }
                case 0xea: // pminsw xmm, xmm/m128
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pminsw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xeb: // por xmm, xmm/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "por %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xee: // pmaxsw xmm, xmm/m128
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pmaxsw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xef: // pxor xmm, xmm/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pxor %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xf2:
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    if ( 0x66 == _prefix_size ) // pslld xmm1, xmm2/m128   shift dwords left while shifting in 0s
                        tracer.Trace( "pslld %s, %s\n", xmm_names[ _reg ], rm_string( 4, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xf3:
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    if ( 0x66 == _prefix_size ) // psllq xmm1, xmm2/m128   shift qwords left while shifting in 0s
                        tracer.Trace( "psllq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xf4: // pmuludq xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "pmuludq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xf5: // cmc complement carry flag
                {
                    tracer.Trace( "cmc\n" );
                    break;
                }
                case 0xf6: // psadbw xmm1, xmm2/m128  compute absolute differences on bytes and store results in low words of each part of result
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "psadbw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xf8: // psubb xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "psubb %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xf9: // psubw xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "psubw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xfa: // psubd xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "psubd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xfb:
                {
                    decode_rm();
                    if ( 0 != _prefix_sse2_repeat )
                        unhandled();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "psubq %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xfc: // paddb xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "padddb %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xfd: // paddw xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "padddw %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                case 0xfe: // paddd xmm1, xmm2/m128
                {
                    decode_rm();
                    if ( 0x66 == _prefix_size )
                        tracer.Trace( "paddd %s, %s\n", xmm_names[ _reg ], rm_string( 8, true ) );
                    else
                        unhandled();
                    break;
                }
                default:
                    unhandled();
            }
            break;
        }
        case 0x2e: // prefix for CS or branch not taken prediction
        {
            tracer.Trace( "prefixCS_Branch  # ignored\n" );
            break;
        }
        case 0x3e: // prefix for DS or branch prediction
        {
            tracer.Trace( "prefixDS_Branch  # ignored\n" );
            break;
        }
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: // REX prefix
        case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f: // REX prefix
        {
            //tracer.Trace( "rex %c %c %c %c\n", (op & 8) ? 'W' : 'w', (op & 4) ? 'R' : 'r', (op & 2) ? 'X' : 'x', (op & 1) ? 'B' : 'b' );
            break;
        }
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: // push
        {
            _rm = op & 7;
            decode_rex();
            tracer.Trace( "push %s\n", register_names[ _rm ] );
            break;
        }
        case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: // pop
        {
            _rm = op & 7;
            decode_rex();
            tracer.Trace( "pop %s\n", register_names[ _rm ] );
            break;
        }
        case 0x63: // movsxd reg, r/m. aka gcc movslq
        {
            decode_rm();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "movsxw %s, %s\n", register_names[ _reg ], rm_string( 2 ) );
            else if ( _rexW )
                tracer.Trace( "movsxq %s, %s\n", register_names[ _reg ], rm_string( 8 ) );
            else
                tracer.Trace( "movsxd %s, %s\n", register_names[ _reg ], rm_string( 4 ) );
            break;
        }
        case 0x64: case 0x65: // prefix for fs: and gs:
        {
            tracer.Trace( "prefix_segment %s  # %#llx\n", ( 0x64 == op ) ? "fs:" : "gs", ( 0x64 == op ) ? fs : gs );
            break;
        }
        case 0x66: // prefix x66 make operands 16-bit or xmm
        {
            tracer.Trace( "prefix66 # 16-bit or xmm op\n" );
            break;
        }
        case 0x67: // prefix x67 promote 32-bit relative pointer to 64-bit
        {
            tracer.Trace( "prefix67 # promote 32-bit address to 64-bit\n" );
            break;
        }
        case 0x68: // push imm16 / imm32
        {
            if ( 0x66 == _prefix_size )
                tracer.Trace( "push %#x\n", get_rip16() );
            else
                tracer.Trace( "push %#x\n", get_rip32() );
            break;
        }
        case 0x69: // imul reg, r/m, imm. 16, 32, and 64-bit values (imm 16 or 32)
        {
            decode_rm();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "imul %s, %s, %d\n", register_names8[ _reg ], rm_string( 2 ), get_rip16() );
            else if ( _rexW )
                tracer.Trace( "imul %s, %s, %d\n", register_names[ _reg ], rm_string( 8 ), get_rip32() );
            else
                tracer.Trace( "imul %s, %s, %d\n", register_names32[ _reg ], rm_string( 4 ), get_rip32() );
            break;
        }
        case 0x6a: // push imm8. sign-extended to 64 bits
        {
            tracer.Trace( "push %#llx\n", (int64_t) (int8_t) get_rip8() );
            break;
        }
        case 0x6b: // imul reg, r/m, se( imm8 ). sizes 16/32/64
        {
            decode_rm();
            uint8_t imm = get_rip8();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "imul %s, %s, %lld\n", register_names8[ _reg ], rm_string( 2 ), sign_extend( imm, 7 ) );
            else if ( _rexW )
                tracer.Trace( "imul %s, %s, %lld\n", register_names[ _reg ], rm_string( 8 ), sign_extend( imm, 7 ) );
            else
                tracer.Trace( "imul %s, %s, %lld\n", register_names32[ _reg ], rm_string( 4 ), sign_extend( imm, 7 ) );
            break;
        }
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: // jcc
        case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
        {
            int8_t val = (int8_t) get_rip8();
            tracer.Trace( "j%s %d  # %#llx\n", condition_names[ op & 0xf ], val, rip + (int64_t) val );
            break;
        }
        case 0x80: // math r/m8, i8
        {
            decode_rm();
            uint8_t math = _reg;
            tracer.Trace( "%sb %s, %#x\n", math_names[ math ], rm_string( 1 ), get_rip8() );
            break;
        }
        case 0x81: // math r/m, imm32
        {
            decode_rm();
            uint8_t math = _reg;
            if ( 0x66 == _prefix_size )
                tracer.Trace( "%sw %s, %#x\n", math_names[ math ], rm_string( 2 ), get_rip16() );
            else
            {
                uint64_t imm = get_rip32();
                tracer.Trace( "%s%c %s, %#llx\n", math_names[ math ], _rexW ? 'q' : 'd', rm_string( _rexW ? 8 : 4 ), _rexW ? sign_extend( imm, 31 ) : imm );
            }
            break;
        }
        case 0x83: // math r/m, imm8
        {
            decode_rm();
            if ( 0x66 == _prefix_size )
            {
                uint16_t imm = (int16_t) (int16_t) get_rip8();
                uint8_t math = _reg;
                tracer.Trace( "%sw %s, %#x\n", math_names[ math ], rm_string( 8 ), imm );
            }
            else
            {
                uint32_t imm = (int32_t) (int8_t) get_rip8();
                uint8_t math = _reg;
                if ( _rexW )
                    tracer.Trace( "%sq %s, %#llx\n", math_names[ math ], rm_string( 8 ), sign_extend( imm, 31 ) );
                else
                    tracer.Trace( "%sd %s, %#x\n", math_names[ math ], rm_string( 4 ), imm );
            }
            break;
        }
        case 0x84: // test r/m8, r8
        {
            decode_rm();
            tracer.Trace( "test %s, %s\n", rm_string( 1 ), register_name( _reg, 1 ) );
            break;
        }
        case 0x85: // test r/m, reg
        {
            decode_rm();
            tracer.Trace( "test %s, %s\n", rm_string( op_width() ), register_name( _reg, op_width() ) );
            break;
        }
        case 0x86: // xchg r/m8, r8
        {
            decode_rm();
            tracer.Trace( "xchg %s, %s\n", rm_string( 1 ), register_name( _reg, 1 ) );
            break;
        }
        case 0x87: // xchg r/m, reg  16, 32, 64
        {
            decode_rm();
            tracer.Trace( "xchg %s, %s\n", rm_string( op_width() ), register_name( _reg, op_width() ) );
            break;
        }
        case 0x88: // mov r/m8, r8
        {
            decode_rm();
            tracer.Trace( "mov %s, %s\n", rm_string( 1 ), register_name( _reg, 1 ) );
            break;
        }
        case 0x89: // mov r/m, reg
        {
            decode_rm();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "movw %s, %s\n", rm_string( 2 ), register_name( _reg, 2 ) );
            else
                tracer.Trace( "mov %s, %s\n", rm_string( op_width() ), register_name( _reg, op_width() ) );
            break;
        }
        case 0x8a: // mov r8, r/m8
        {
            decode_rm();
            tracer.Trace( "mov %s, %s\n", register_name( _reg, 1 ), rm_string( _rexW ? 8 : 4 ) );
            break;
        }
        case 0x8b: // mov reg, r/m
        {
            decode_rm();
            tracer.Trace( "mov %s, %s\n", register_name( _reg, op_width() ), rm_string( op_width() ) );
            break;
        }
        case 0x8d: // lea
        {
            decode_rm();
            tracer.Trace( "lea %s, %s\n", register_names[ _reg ], rm_string( 8 ) );
            break;
        }
        case 0x90:
        {
            tracer.Trace( "nop\n" ); // aka exch ax ax
            break;
        }
        case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: // xchg ax, r  (widths 16, 32, 64)
        {
            _rm = op & 0xf;
            if ( 0 != _prefix_rex )
            {
                decode_rex();
                tracer.Trace( "xchg rax, %s\n", register_names[ _rm ] );
            }
            else if ( 0x66 == _prefix_size )
                tracer.Trace( "xchg ax, %s\n", register_names16[ _rm ] );
            else
                tracer.Trace( "xchg eax, %s\n", register_names32[ _rm ] );
            break;
        }
        case 0x98: // cbw/cwde/cdqe
        {
            decode_rex();
            tracer.Trace( "%s\n", _rexW ? "cdqe" : ( 0x66 == _prefix_size ) ? "cbw" : "cwde" );
            break;
        }
        case 0x99: // cwd/cdq/cqo
        {
            decode_rex();
            tracer.Trace( "%s\n", _rexW ? "cqo" : ( 0x66 == _prefix_size ) ? "cwd" : "cdq" );
            break;
        }
        case 0xa4: // movsb m, m  RSI to RDI
        {
            decode_rex();
            tracer.Trace( "movsb (rdi), (rsi)\n" );
            break;
        }
        case 0xa5: // movsq m, m  RSI to RDI 16/32/64
        {
            decode_rex();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "movsw (rdi), (rsi)\n" );
            else if ( _rexW )
                tracer.Trace( "movsq (rdi), (rsi)\n" );
            else
                tracer.Trace( "movsd (rdi), (rsi)\n" );
            break;
        }
        case 0xa8: // test al, imm8
        {
            tracer.Trace( "test al, %#x\n", get_rip8() );
            break;
        }
        case 0xa9: // test ax, imm16    text eax, imm32   test rax, se(imm32)
        {
            decode_rex();
            if ( 0 != _prefix_rex )
                tracer.Trace( "test rax, %#x\n", sign_extend( get_rip32(), 31 ) );
            else if ( 0x66 == _prefix_size )
                tracer.Trace( "test ax, %#x\n", get_rip16() );
            else
                tracer.Trace( "test eax, %#x\n", get_rip32() );
            break;
        }
        case 0xaa: // stos m8
        {
            tracer.Trace( "stob rdi\n" );
            break;
        }
        case 0xab: // stos
        {
            char w = ( 0 != _prefix_rex ) ? 'q' : ( 0x66 == _prefix_size ) ? 'w' : 'd';
            tracer.Trace( "sto%c rdi\n", w ); // 32-bit edi addressing ignored
            break;
        }
        case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7: // mov r8, imm8
        {
            _rm = ( op & 7 );
            decode_rex();
            tracer.Trace( "mov %s, %#x\n", register_name( _rm, 8 ), getui8( rip + 1 ) );
            break;
        }
        case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf: // mov reg, 64-bit immediate
        {
            _rm = ( op & 7 );
            decode_rex();
            uint64_t val = 0;
            char w;
            uint8_t width;

            if ( 0x66 == _prefix_size )
            {
                w = 'w';
                val = get_rip16();
                width = 2;
            }
            else if ( _rexW )
            {
                w = 'q';
                val = get_rip64();
                width = 8;
            }
            else
            {
                w = 'd';
                val = get_rip32();
                width = 4;
            }

            tracer.Trace( "mov%c %s, %#llx\n", w, register_name( _rm, width ), val );
            break;
        }
        case 0xc0: // shift r8/m8, imm8
        {
            decode_rm();
            tracer.Trace( "%s %s, %u\n", shift_names[ _reg ], rm_string( 1 ), get_rip8() );
            break;
        }
        case 0xc1: // sal/shr r/m, imm8
        {
            decode_rm();
            uint8_t val = get_rip8();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "%s %s, %#x\n", shift_names[ _reg ], rm_string( 2 ), val );
            else if ( _rexW )
                tracer.Trace( "%s %s, %#x\n", shift_names[ _reg ], rm_string( 8 ), val );
            else
                tracer.Trace( "%s %s, %#x\n", shift_names[ _reg ], rm_string( 4 ), val );
            break;
        }
        case 0xc3: // ret
        {
            tracer.Trace( "ret\n" );
            break;
        }
        case 0xc6: // mov r/m8, imm8
        {
            decode_rm();
            if ( 0 == _reg )
                tracer.Trace( "movb %s, %d\n", rm_string( 1 ), get_rip8() );
            else
                unhandled();
            break;
        }
        case 0xc7: // mov r/m, imm. 32-bit zero-extended immediate value
        {
            decode_rm();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "movw %s, %#x\n", rm_string( 8 ), get_rip16() );
            else if ( _rexW )
                tracer.Trace( "movq %s, %#llx\n", rm_string( 8 ), sign_extend( get_rip32(), 31 ) );
            else
                tracer.Trace( "movd %s, %##x\n", rm_string( 4 ), get_rip32() );
            break;
        }
        case 0xc9:
        {
            tracer.Trace( "leave\n" );
            break;
        }
        case 0xd0: // shift r/m8, 1
        {
            decode_rm();
            tracer.Trace( "%s %s, 1\n", shift_names[ _reg ], rm_string( 1 ) );
            break;
        }
        case 0xd1: // shift r/m (, 1)   16/32/64
        {
            decode_rm();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "%s %s\n", shift_names[ _reg ], rm_string( 2 ) );
            else if ( _rexW )
                tracer.Trace( "%s %s\n", shift_names[ _reg ], rm_string( 8 ) );
            else
                tracer.Trace( "%s %s\n", shift_names[ _reg ], rm_string( 4 ) );
            break;
        }
        case 0xd3: // shift r/m, cl   16/32/64
        {
            decode_rm();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "%s %s, cl\n", shift_names[ _reg ], rm_string( 2 ) );
            else if ( _rexW )
                tracer.Trace( "%s %s, cl\n", shift_names[ _reg ], rm_string( 8 ) );
            else
                tracer.Trace( "%s %s, cl\n", shift_names[ _reg ], rm_string( 4 ) );
            break;
        }
        case 0xd8:
        {
            uint8_t op1 = get_rip8();
            if ( op1 >= 0xc0 && op1 <= 0xc7 ) // fmul st(0), st(i)
                tracer.Trace( "fadd st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xc8 && op1 <= 0xcf ) // fmul st(0), st(i)
                tracer.Trace( "fmul st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xe0 && op1 <= 0xe7 ) // fsub st(0), st(i)
                tracer.Trace( "fsub st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xe8 && op1 <= 0xef ) // fsubr st(0), st(i)
                tracer.Trace( "fsubr st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xf0 && op1 <= 0xf7 ) // fdiv st(0), st(i)
                tracer.Trace( "fdiv st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xf8 && op1 <= 0xff ) // fdivr st(0), st(i)
                tracer.Trace( "fdivr st(0), st(%u)\n", op1 & 7 );
            else
            {
                rip--;
                decode_rm();
                if ( 0 == _reg ) // fadd m32fp
                    tracer.Trace( "fadd %s  # m32fp\n", rm_string( 4, true ) );
                else if ( 1 == _reg ) // fmul m32fp
                    tracer.Trace( "fmul %s  # m32fp\n", rm_string( 4, true ) );
                else if ( 4 == _reg ) // fsub m32fp
                    tracer.Trace( "fsub %s  # m32fp\n", rm_string( 4, true ) );
                else if ( 5 == _reg ) // fsubr m32fp
                    tracer.Trace( "fsubr %s  # m32fp\n", rm_string( 4, true ) );
                else if ( 6 == _reg ) // fdiv m32fp
                    tracer.Trace( "fdiv %s  # m32fp\n", rm_string( 4, true ) );
                else if ( 7 == _reg ) // fdivr m32fp
                    tracer.Trace( "fdivr %s  # m32fp\n", rm_string( 4, true ) );
                else
                    unhandled();
            }
            break;
        }
        case 0xd9:
        {
            uint8_t op1 = get_rip8();
            if ( op1 >= 0xc0 && op1 <= 0xc7 )
                tracer.Trace( "fld st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xc8 && op1 <= 0xcf )
                tracer.Trace( "fxch st(%u)\n", ( op1 & 7 ) % _countof( fregs ) );
            else if ( 0xd0 == op1 )
                tracer.Trace( "fnop\n" );
            else if ( 0xe0 == op1 )
                tracer.Trace( "fchs\n" );
            else if ( 0xe1 == op1 )
                tracer.Trace( "fabs\n" );
            else if ( 0xe4 == op1 )
                tracer.Trace( "test\n" );
            else if ( 0xe5 == op1 )
                tracer.Trace( "fxam\n" );
            else if ( op1 >= 0xe8 && op1 <= 0xee )
                tracer.Trace( "%s  # %lf\n", float_d9_e8[ op1 & 7 ], float_d9_e8_constants[ op1 & 7 ] );
            else if ( op1 >= 0xf0 && op1 <= 0xf7 )
                tracer.Trace( "%s\n", float_d9_f0[ op1 & 7 ] );
            else if ( op1 >= 0xf8 && op1 <= 0xff )
                tracer.Trace( "%s\n", float_d9_f8[ op1 & 7 ] );
            else
            {
                rip--;
                decode_rm();
                if ( 0 == _reg ) // fld m32fp. pushes m32fp onto the fpu register stack
                    tracer.Trace( "fld %s  # m32fp\n", rm_string( 4 ) );
                else if ( 2 == _reg ) // fst m32fp copy st to m32fp
                    tracer.Trace( "fst %s  # m32fp\n", rm_string( 4 ) );
                else if ( 3 == _reg ) // fstp m32fp copy st to m32fp and pop register stack
                    tracer.Trace( "fstp %s  # m32fp\n", rm_string( 4 ) );
                else if ( 4 == _reg ) // fldenv   load x87 fpu environment from a memory location
                    tracer.Trace( "fnldenv %s\n", rm_string( 8 ) );
                else if ( 5 == _reg ) // fldcw  load fpu control word from m2byte
                    tracer.Trace( "fldcw %s\n", rm_string( 2 ) );
                else if ( 6 == _reg ) // fnstenv m14/28byte  stores fpu status info in mrmory
                    tracer.Trace( "fnstenv %s\n", rm_string( 8 ) );
                else if ( 7 == _reg ) // fnstcw m2byte. store fpu control word to m2byte
                    tracer.Trace( "fnstcw %s\n", rm_string( 8 ) );
                else
                    unhandled();
            }
            break;
        }
        case 0xda:
        {
            uint8_t op1 = get_rip8();
            if ( op1 >= 0xc0 && op1 <= 0xc7 ) // move if below
                tracer.Trace( "fcmovb st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xc8 && op1 <= 0xcf ) // move if equal
                tracer.Trace( "fcmove st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xd0 && op1 <= 0xd7 ) // move if below or equal
                tracer.Trace( "fcmovbe st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xd8 && op1 <= 0xdf ) // move if unordered
                tracer.Trace( "fcmovu st(0), st(%u)\n", op1 & 7 );
            else
            {
                rip--;
                decode_rm();
                if ( 0 == _reg ) // fiadd m32int  add m32int to st(0) and store in st(0)
                    tracer.Trace( "fiadd %d  # m32int\n", get_rm32() );
                else if ( 1 == _reg ) // fimul m32int  multiply st(0) by m32int and store in st(0)
                    tracer.Trace( "fimul %d  # m32int\n", get_rm32() );
                else
                    unhandled();
            }
            break;
        }
        case 0xdb:
        {
            uint8_t op1 = get_rip8();
            if ( op1 >= 0xc0 && op1 <= 0xc7 ) // move if not below
                tracer.Trace( "fcmovnb st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xc8 && op1 <= 0xcf ) // move if not equal
                tracer.Trace( "fcmovne st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xd0 && op1 <= 0xd7 ) // move if not below or equal
                tracer.Trace( "fcmovnbe st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xd8 && op1 <= 0xdf ) // move if not unordered
                tracer.Trace( "fcmovnu st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xf0 && op1 <= 0xf7 ) // fcomi st, st(i)  compare st(0) with st(i) and set status flags
                tracer.Trace( "fcomi st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xe8 && op1 <= 0xef ) // fucomi st, st(i)
                tracer.Trace( "fucomi st(0), st(%u)\n", op1 & 7 );
            {
                rip--;
                decode_rm();
                if ( 0 == _reg ) // fild m32int
                    tracer.Trace( "fild %d\n", get_rip32() );
                else if ( 3 == _reg ) // fistp m32int
                    tracer.Trace( "fistp %s  # m32int\n", rm_string( 4 ) );
                else if ( 4 == _reg ) // nop, clear exceptions, init fp unit
                    tracer.Trace( "f nop of some sort\n" );
                else if ( 5 == _reg ) // fld m80fp push m80fp onto the fpu register stack
                    tracer.Trace( "fld %s  # push m80fp\n", rm_string( 8 ) );
                else if ( 7 == _reg ) // fstp m80fp   copy top of stack to m80fp and pop it
                    tracer.Trace( "fstp %s  # m80fp\n", rm_string( 8 ) );
                else
                    unhandled();
            }
            break;
        }
        case 0xdc:
        {
            uint8_t op1 = get_rip8();
            if ( op1 >= 0xe0 && op1 <= 0xe7 ) // fsubr st(i), st(0)
                tracer.Trace( "fsubr st(%u), st(0)\n", op1 & 7 );
            else if ( op1 >= 0xe8 && op1 <= 0xef ) // fsub st(i), st(0)
                tracer.Trace( "fsub st(%u), st(0)\n", op1 & 7 );
            else if ( op1 >= 0xc0 && op1 <= 0xc7 ) // fadd st(i), st(0)
                tracer.Trace( "fadd st(%u), st(0)\n", op1 & 7 );
            else if ( op1 >= 0xc8 && op1 <= 0xcf ) // fmul st(i), st(0)
                tracer.Trace( "fmul st(%u), st(0)\n", op1 & 7 );
            else if ( op1 >= 0xf0 && op1 <= 0xf7 ) // fdivr st(i), st(0)
                tracer.Trace( "fdivr st(%u), st(0)\n", op1 & 7 );
            else if ( op1 >= 0xf8 && op1 <= 0xff ) // fdiv st(i), st(0)
                tracer.Trace( "fdiv st(%u), st(0)\n", op1 & 7 );
            else
            {
                rip--;
                decode_rm();
                if ( 0 == _reg ) // fadd m64fp
                    tracer.Trace( "fadd %s  # m64fp\n", rm_string( 8 ) );
                else if ( 1 == _reg ) // fmul m64fp
                    tracer.Trace( "fmul %s  # m64fp\n", rm_string( 8 ) );
                else if ( 2 == _reg ) // fcom m64fp
                    tracer.Trace( "fcom %s  # m64fp\n", rm_string( 8 ) );
                else if ( 3 == _reg ) // fcomp m64fp
                    tracer.Trace( "fcomp %s  # m64fp\n", rm_string( 8 ) );
                else if ( 4 == _reg ) // fsub m64fp
                    tracer.Trace( "fsub %s  # m64fp\n", rm_string( 8 ) );
                else if ( 5 == _reg ) // fsubr m64fp
                    tracer.Trace( "fsubr %s  # m64fp\n", rm_string( 8 ) );
                else if ( 6 == _reg ) // fdiv m64fp
                    tracer.Trace( "fdiv %s  # m64fp\n", rm_string( 8 ) );
                else if ( 7 == _reg ) // fdivr m64fp
                    tracer.Trace( "fdivr %s  # m64fp\n", rm_string( 8 ) );
                else
                    unhandled();
            }
            break;
        }
        case 0xdd:
        {
            uint8_t op1 = get_rip8();
            if ( op1 >= 0xd8 && op1 <= 0xdf ) // fstp st(i)
                tracer.Trace( "fstp st(%u), st(0)\n", op1 & 7 );
            else
            {
                rip--;
                decode_rm();
                if ( 0 == _reg ) // fld m64fp. convert then push double on the fp stack
                    tracer.Trace( "fld %s\n", rm_string( 8 ) );
                else if ( 2 == _reg ) // fst m64fp   copy st(0) to m64fp
                    tracer.Trace( "fst %s\n", rm_string( 8 ) );
                else if ( 3 == _reg ) // fstp m64fp  copy st(0) to m64fp and pop register stack
                    tracer.Trace( "fstp %s\n", rm_string( 8 ) );
                else
                    unhandled();
            }
            break;
        }
        case 0xde:
        {
            uint8_t op1 = get_rip8();
            if ( op1 >= 0xe0 && op1 <= 0xe7 ) // fsubrp st(i), st(0)
                tracer.Trace( "fsubrp st(%u), st(0)\n", ( op1 & 7 ) % _countof( fregs ) );
            else if ( op1 >= 0xe8 && op1 <= 0xef ) // fsubp st(i), st(0)
                tracer.Trace( "fsubp st(%u), st(0)\n", ( op1 & 7 ) % _countof( fregs ) );
            else if ( op1 >= 0xc0 && op1 <= 0xc7 ) // faddp st(i), st(0)
                tracer.Trace( "faddp st(%u), st(0)\n", ( op1 & 7 ) % _countof( fregs ) );
            else if ( op1 >= 0xc8 && op1 <= 0xcf ) // fmulp st(i), st(0)
                tracer.Trace( "fmulp st(%u), st(0)\n", ( op1 & 7 ) % _countof( fregs ) );
            else if ( op1 >= 0xf0 && op1 <= 0xf7 ) // fdivrp st(i), st(0)
                tracer.Trace( "fdivrp st(%u), st(0)\n", ( op1 & 7 ) % _countof( fregs ) );
            else if ( op1 >= 0xf8 && op1 <= 0xff ) // fdivp st(i), st(0)
                tracer.Trace( "fdivp st(%u), st(0)\n", ( op1 & 7 ) % _countof( fregs ) );
            else
            {
                rip--;
                decode_rm();
                if ( 0 == _reg ) // fiadd m16int   add m16int to st(0) and store in st(0)
                    tracer.Trace( "fiadd %s  # m16int\n", rm_string( 2 ) );
                else
                    unhandled();
            }
            break;
        }
        case 0xdf:
        {
            uint8_t op1 = get_rip8();
            if ( op1 >= 0xe8 && op1 <= 0xef ) // fucomip st, st(i)
                tracer.Trace( "fucomip st(0), st(%u)\n", op1 & 7 );
            else if ( op1 >= 0xf0 && op1 <= 0xf7 ) // fcomip st(0), st(i)
                tracer.Trace( "fcomip st(0), st(%u)\n", op1 & 7 );
            else if ( 0xe0 == op1 )
                tracer.Trace( "fnstsw ax\n" );
            else if ( 0 == _prefix_sse2_repeat )
            {
                rip--;
                decode_rm();
                if ( 0 == _reg ) // fild m16int
                    tracer.Trace( "fild %s  # m16int\n", rm_string( 2 ) );
                else if ( 3 == _reg ) // fistp m16int   store st(0) in m16int and pop register stack
                    tracer.Trace( "fistp %s  # m16int\n", rm_string( 2 ) );
                else if ( 5 == _reg ) // fild m64int   loads signed 64 bit integer converted to a float and pushed to fp stack
                    tracer.Trace( "fild %s  # m64int\n", rm_string( 8 ) );
                else if ( 7 == _reg ) // fistp m64int store st(0) as an m64int and pop fp stack
                    tracer.Trace( "fistp %s\n", rm_string( 8 ) );
                else
                    unhandled();
            }
            else
                unhandled();
            break;
        }
        case 0xe3: // jcxz / jecxz / jrcxz rel8
        {
            int8_t rel = get_rip8();
            if ( 0x66 == _prefix_size )
                tracer.Trace( "jcxz %d\n", (int32_t) rel );
            else if ( _rexW )
                tracer.Trace( "jrcxz %d\n", (int32_t) rel );
            else
                tracer.Trace( "jecxz %d\n", (int32_t) rel );
            break;
        }
        case 0xe8: // call rel32
        {
            uint32_t imm = get_rip32();
            tracer.Trace( "call %d  # %#llx\n", imm, rip + (int32_t) imm );
            break;
        }
        case 0xe9: // jmp cd  (relative to rip sign-extended 32-bit immediate)
        {
            uint32_t imm = get_rip32();
            tracer.Trace( "jmp %d  # %#llx\n", (int32_t) imm, rip + (int32_t) imm );
            break;
        }
        case 0xeb: // jmp
        {
            int8_t imm = (int8_t) get_rip8();
            tracer.Trace( "jmp %d  # %#llx\n", imm, rip + imm );
            break;
        }
        case 0xf0: // lock
        {
            tracer.Trace( "lock\n" );
            break;
        }
        case 0xf2: case 0xf3:
        {
            tracer.Trace( "repeat/multi-byte/bnd prefix\n" );
            break;
        }
        case 0xf4: // hlt
        {
            tracer.Trace( "hlt  # exit the emulator\n" );
            break;
        }
        case 0xf6:
        {
            decode_rm();
            if ( 0 == _reg ) // test r/m8, imm8
                tracer.Trace( "test %s, %#x\n", rm_string( 1 ), get_rip8() );
            else if ( 2 == _reg ) // not r/m8
                tracer.Trace( "not %s\n", rm_string( 1 ) );
            else if ( 3 == _reg ) // neg r/m8
                tracer.Trace( "neg %s\n", rm_string( 1 ) );
            else if ( 4 == _reg ) // mul r/m8
                tracer.Trace( "mul %s\n", rm_string( 1 ) );
            else if ( 6 == _reg ) // div r/m8
                tracer.Trace( "div %s\n", rm_string( 1 ) );
            else if ( 7 == _reg ) // div r/m8
                tracer.Trace( "idiv %s\n", rm_string( 1 ) );
            else
                unhandled();
            break;
        }
        case 0xf7:
        {
            decode_rm();
            if ( 0 == _reg ) // test r/m16, imm16   test r/m32, imm32    test r/m64, se( imm32 )
            {
                if ( 0x66 == _prefix_size )
                    tracer.Trace( "testw %s, %#x\n", rm_string( 2 ), get_rip16() );
                else if ( _rexW )
                    tracer.Trace( "testq %s, %#llx\n", rm_string( 8 ), sign_extend( get_rip32(), 31 ) );
                else
                    tracer.Trace( "testd %s, %#x\n", rm_string( 4 ), get_rip32() );
            }
            else if ( 2 == _reg )
            {
                if ( 0x66 == _prefix_size )
                    tracer.Trace( "not %s\n", rm_string( 2 ) );
                else if ( _rexW )
                    tracer.Trace( "not %s\n", rm_string( 8 ) );
                else
                    tracer.Trace( "not %s\n", rm_string( 4 ) );
            }
            else if ( 3 == _reg )
            {
                if ( 0x66 == _prefix_size )
                    tracer.Trace( "negw %s\n", rm_string( 2 ) );
                else if ( _rexW )
                    tracer.Trace( "negq %s\n", rm_string( 8 ) );
                else
                    tracer.Trace( "negd %s\n", rm_string( 4 ) );
            }
            else if ( 4 == _reg )
                tracer.Trace( "mul %s\n", rm_string( op_width() ) );
            else if ( 5 == _reg )
                tracer.Trace( "imul %s\n", rm_string( op_width() ) );
            else if ( 6 == _reg )
                tracer.Trace( "div %s\n", rm_string( op_width() ) );
            else if ( 7 == _reg )
                tracer.Trace( "idiv %s\n", rm_string( op_width() ) );
            else
                unhandled();
            break;
        }
        case 0xf8: // clc
        {
            tracer.Trace( "clc\n" );
            break;
        }
        case 0xf9: // stc
        {
            tracer.Trace( "stc\n" );
            break;
        }
        case 0xfa: // cli
        {
            tracer.Trace( "cli\n" );
            break;
        }
        case 0xfb: // sti
        {
            tracer.Trace( "sti\n" );
            break;
        }
        case 0xfc: // cld
        {
            tracer.Trace( "cld\n" );
            break;
        }
        case 0xfd: // std
        {
            tracer.Trace( "std\n" );
            break;
        }
        case 0xfe:
        {
            decode_rm();
            switch( _reg )
            {
                case 0: // inc r/m8
                {
                    tracer.Trace( "inc %s\n", rm_string( op_width() ) );
                    break;
                }
                case 1: // dec r/m8
                {
                    tracer.Trace( "dec %s\n", rm_string( op_width() ) );
                    break;
                }
                default: unhandled();
            }
            break;
        }
        case 0xff:
        {
            decode_rm();
            switch( _reg )
            {
                case 0: // inc r/m
                {
                    tracer.Trace( "inc %s\n", rm_string( op_width() ) );
                    break;
                }
                case 1: // dec r/m
                {
                    tracer.Trace( "dec %s\n", rm_string( op_width() ) );
                    break;
                }
                case 4: // jmp
                {
                    tracer.Trace( "jmp %s\n", rm_string( 8 ) );
                    break;
                }
                case 2: // call
                {
                    tracer.Trace( "call %s\n", rm_string( 8 ) );
                    break;
                }
                case 3: // call
                    unhandled();
                case 5: // jmp
                    unhandled();
                case 6: // push
                {
                    if ( 0x66 == _prefix_size )
                        unhandled();
                    tracer.Trace( "push %s\n", rm_string( 8 ) );
                    break;
                }
                default: unhandled();
            }
            break;
        }
        default:
            unhandled();
    }

    rip = rip_save;
    clear_decoding();
} //trace_state

#ifdef _WIN32
__declspec(noinline)
#endif
void x64::force_trace_xreg( uint32_t i)
{
    tracer.Trace( "    xmm%u: ", i );
    tracer.TraceBinaryData( (uint8_t *) & xregs[ i ], 16, 4 );
} //force_trace_xreg

#ifdef _WIN32
__declspec(noinline)
#endif
void x64::force_trace_xregs()
{
    for ( uint32_t i = 0; i < _countof( xregs ); i++ )
        if ( memcmp( &vec_zeroes, &xregs[ i ], sizeof( vec_zeroes ) ) )
            force_trace_xreg( i );
} //trace_xregs

void x64::trace_xreg( uint32_t i )
{
    if ( !tracer.IsEnabled() ) // can happen when an app enables instruction tracing via a syscall but overall tracing is turned off.
        return;

    if ( ! ( g_State & stateTraceInstructions ) )
        return;

    force_trace_xreg( i );
} //trace_xreg

void x64::trace_xregs()
{
    if ( !tracer.IsEnabled() ) // can happen when an app enables instruction tracing via a syscall but overall tracing is turned off.
        return;

    if ( ! ( g_State & stateTraceInstructions ) )
        return;

    force_trace_xregs();
} //trace_xregs

void x64::trace_fregs()
{
    if ( !tracer.IsEnabled() ) // can happen when an app enables instruction tracing via a syscall but overall tracing is turned off.
        return;

    if ( ! ( g_State & stateTraceInstructions ) )
        return;

    for ( uint8_t spot = 0; spot < _countof( fregs ); spot++ )
    {
        uint8_t offset = spot + fp_sp;
        offset = offset % _countof( fregs );
        tracer.Trace( " f%u:%13.6lf", offset, (double) fregs[ offset ].getld() ); // msft C runtime can't trace long double
    }
    tracer.Trace( "\n" );
} //trace_fregs

#define ROUNDING_MODE_NEAREST  0
#define ROUNDING_MODE_FLOOR    1
#define ROUNDING_MODE_CEILING  2
#define ROUNDING_MODE_TRUNCATE 3

int32_t round_i32_from_double( double d, uint8_t rm )
{
    if ( my_isnan( d ) || isinf( d ) )
        return INT32_MAX;

    if ( d > (double) INT32_MAX )
        return INT32_MAX;

    if ( d < (double) INT32_MIN )
        return INT32_MIN;

    if ( ROUNDING_MODE_NEAREST == rm ) // nearest
        return (int32_t) round( d );
    if ( ROUNDING_MODE_FLOOR == rm ) // towards -infinity
        return (int32_t) floor( d );
    if ( ROUNDING_MODE_CEILING == rm ) // towards +infinity
        return (int32_t) ceil( d );

    assert( ROUNDING_MODE_TRUNCATE == rm );
    return (int32_t) trunc( d ); // towards 0 (truncate)
} //round_i32_from_double

double round_double_from_double( double d, uint8_t rm )
{
    if ( my_isnan( d ) || isinf( d ) )
        return d;

    if ( ROUNDING_MODE_NEAREST == rm ) // nearest
        return round( d );
    if ( ROUNDING_MODE_FLOOR == rm ) // towards -infinity
        return floor( d );
    if ( ROUNDING_MODE_CEILING == rm ) // towards +infinity
        return ceil( d );

    assert( ROUNDING_MODE_TRUNCATE == rm );
    return trunc( d ); // towards 0 (truncate)
} //round_double_from_double

long double round_ldouble_from_ldouble( long double d, uint8_t rm )
{
    if ( my_isnan( d ) || isinf( d ) )
        return d;

    if ( ROUNDING_MODE_NEAREST == rm ) // nearest
        return roundl( d );
    if ( ROUNDING_MODE_FLOOR == rm ) // towards -infinity
#if defined( __clang__ ) && defined( _WIN32 ) // clang version 19.1.5 Target: x86_64-pc-windows-msvc doesn't provide floorl or ceill
        return floor( (double) d );
#else
        return floorl( d );
#endif
    if ( ROUNDING_MODE_CEILING == rm ) // towards +infinity
#if defined( __clang__ ) && defined( _WIN32 )
        return ceil( (double) d );
#else
        return ceill( d );
#endif

    assert( ROUNDING_MODE_TRUNCATE == rm );
    return truncl( d ); // towards 0 (truncate)
} //round_ldouble_from_ldouble

void x64::push_fp( float80_t f80 )
{
    if ( 0 == fp_sp )
        fp_sp = _countof( fregs ) - 1;
    else
        fp_sp--;

    fregs[ fp_sp ] = f80;
    trace_fregs();
} //push_fp

void x64::push_fp( long double val )
{
    float80_t x;
    x.setld( val );
    push_fp( x );
} //push_fp

float80_t x64::pop_fp()
{
    uint8_t location = fp_sp;
    fp_sp = ( fp_sp + 1 ) % _countof( fregs );
    trace_fregs();
    return fregs[ location ];
} //pop_fp

float80_t x64::peek_fp( uint8_t offset )
{
    offset += fp_sp;
    offset = offset % _countof( fregs );
    return fregs[ offset ];
} //peek_fp

void x64::poke_fp( uint8_t offset, float80_t f80 )
{
    offset += fp_sp;
    offset = offset % _countof( fregs );
    fregs[ offset ] = f80;
    trace_fregs();
} //poke_fp

void x64::poke_fp( uint8_t offset, long double val )
{
    poke_fp( offset, float80_t::float80_from_ld( val ) );
} //poke_fp

void x64::poke_fp( uint8_t offset, double val )
{
    poke_fp( offset, float80_t::float80_from_d( val ) );
} //poke_fp

template <typename T> T absolute_difference( T a, T b )
{
    if ( a > b )
        return a - b;
    return b - a;
} //absolute_difference

template <typename T> T subtract_and_saturate_unsigned( T a, T b )
{
    if ( a >= b )
        return a - b;
    return 0;
} //subtract_and_saturate_unsigned

template <typename T> bool get_bit( T x, uint8_t bit_number )
{
    assert( bit_number < ( 8 * sizeof( T ) ) );
    return ( ( x >> bit_number ) & 1 );
} //get_bit

template <typename T> T gen_bitmask( T n )
{
    if ( 0 == n )
        return 0;

    return ( ~ (T) 0 ) >> ( ( 8 * sizeof( T ) ) - n );
} //gen_bitmask

template <typename T> T top2bits( T x )
{
    return ( 3 & ( x >> ( 6 + ( sizeof( T ) - 1 ) * 8 ) ) );
} //top2bits

void x64::op_sto( uint8_t width )
{
    if ( 1 == width )
        setui8( regs[ rdi ].q, regs[ rax ].b );
    else if ( 2 == width )
        setui16( regs[ rdi ].q, regs[ rax ].w );
    else if ( 4 == width )
        setui32( regs[ rdi ].q, regs[ rax ].d );
    else if ( 8 == width )
        setui64( regs[ rdi ].q, regs[ rax ].q );
    else
        unhandled();

    if ( flag_d() )
        regs[ rdi ].q -= width;
    else
        regs[ rdi ].q += width;
} //_op_sto

void x64::op_movs( uint8_t width )
{
    if ( 1 == width )
        setui8( regs[ rdi ].q, getui8( regs[ rsi ].q ) );
    else if ( 2 == width )
        setui16( regs[ rdi ].q, getui16( regs[ rsi ].q ) );
    else if ( 4 == width )
        setui32( regs[ rdi ].q, getui32( regs[ rsi ].q ) );
    else if ( 8 == width )
        setui64( regs[ rdi ].q, getui64( regs[ rsi ].q ) );
    else
        unhandled();

    if ( flag_d() )
    {
        regs[ rdi ].q -= width;
        regs[ rsi ].q -= width;
    }
    else
    {
        regs[ rdi ].q += width;
        regs[ rsi ].q += width;
    }
} //_op_movs

template <typename T> T x64::op_sub( T a, T b, bool borrow )
{
    static_assert( std::is_unsigned_v<T>, "Template parameter must be an unsigned type." );
    T result = a - b - (T) borrow;
    set_PSZ( result );
    setflag_c( ( a < b ) || ( ( a - b ) < (T) borrow ) ); // borrow if a < (b + borrow).

    // Overflow occurs when subtracting a positive from a negative gives a positive or subtracting a negative from a positive gives a negative.
    using ST = std::make_signed_t<T>;
    ST a_s = (ST) a;
    ST b_s = (ST) b;
    ST result_s = (ST) result;
    setflag_o( ( ( a_s ^ b_s ) < 0 ) && ( ( a_s ^ result_s ) < 0 ) ); // Overflow is detected if signs were different AND the result's sign is also different.
    setflag_a( ( 0 != ( ( ( a & 0xf ) - ( b & 0xf ) - (T) borrow ) & ~0xf ) ) );
    return result;
} //op_sub

template <typename T> T x64::op_add( T a, T b, bool carry )
{
    static_assert( std::is_unsigned_v<T>, "Template parameter must be an unsigned type." );
    T result = a + b + (T) carry;
    set_PSZ( result );
    setflag_c( ( ( result < a || result < b ) ) || ( result < ( a + b ) ) );
    setflag_o( ( ! val_signed( a ^ b ) ) && ( val_signed( a ^ result ) ) );
    setflag_a( 0 != ( ( ( a & 0xf ) + ( b & 0xf ) + (T) carry ) & 0x10 ) );
    return result;
} //op_add

template <typename T> T x64::op_xor( T lhs, T rhs )
{
    lhs ^= rhs;
    set_PSZ( lhs );
    reset_carry_overflow();
    return lhs;
} //op_xor

template <typename T> T x64::op_and( T lhs, T rhs )
{
    lhs &= rhs;
    set_PSZ( lhs );
    reset_carry_overflow();
    return lhs;
} //op_xor

template <typename T> T x64::op_or( T lhs, T rhs )
{
    lhs |= rhs;
    set_PSZ( lhs );
    reset_carry_overflow();
    return lhs;
} //op_or

template <typename T> void x64::do_math( uint8_t math, T * pdst, T src )
{
    static_assert(std::is_unsigned_v<T>, "Template parameter must be an unsigned type.");
    assert( math <= 7 );
    switch ( math )
    {
        case 0: *pdst = op_add( *pdst, src ); break;
        case 1: *pdst = op_or( *pdst, src ); break;
        case 2: *pdst = op_add( *pdst, src, flag_c() ); break;
        case 3: *pdst = op_sub( *pdst, src, flag_c() ); break;
        case 4: *pdst = op_and( *pdst, src ); break;
        case 5: *pdst = op_sub( *pdst, src ); break;
        case 6: *pdst = op_xor( *pdst, src ); break;
        default: op_sub( *pdst, src ); break; // 7 is cmp
    }
} //do_math

template <typename T> void x64::op_rol( T * pval, uint8_t shift )
{
    if ( 0 == shift )
        return;

    T original = *pval;
    T val = original;
    for ( uint8_t sh = 0; sh < shift; sh++ )
    {
        bool highBit = val_signed( val );
        val <<= 1;
        if ( highBit )
            val |= 1;
        setflag_c( highBit );
    }

    if ( 1 == shift )
        setflag_o( val_signed( val ) != val_signed( original ) );
    *pval = val;
} //op_rol

template <typename T> void x64::op_ror( T * pval, uint8_t shift )
{
    if ( 0 == shift )
        return;

    T val = *pval;
    for ( uint8_t sh = 0; sh < shift; sh++ )
    {
        bool lowBit = ( 0 != ( 1 & val ) );
        val >>= 1;
        if ( lowBit )
            val = mk_signed( val );
        setflag_c( lowBit );
    }

    if ( 1 == shift )
        setflag_o( val_signed( val ) ^ ( 0 != ( val & 0x40 ) ) );
    *pval = val;
} //op_ror

template <typename T> void x64::op_rcl( T * pval, uint8_t shift )
{
    if ( 0 == shift )
        return;

    T val = *pval;
    for ( uint8_t sh = 0; sh < shift; sh++ )
    {
        bool newCarry = val_signed( val );
        val <<= 1;
        if ( flag_c() )
            val |= 1;
        setflag_c( newCarry );
    }

    if ( 1 == shift )
        setflag_o( val_signed( val ) ^ flag_c() );
    *pval = val;
} //op_rcl

template <typename T> void x64::op_rcr( T * pval, uint8_t shift )
{
    if ( 0 == shift )
        return;

    T val = *pval;
    for ( uint8_t sh = 0; sh < shift; sh++ )
    {
        bool newCarry = ( 0 != ( 1 & val ) );
        val >>= 1;
        if ( flag_c() )
            mk_signed( val );
        setflag_c( newCarry );
    }

    if ( shift )
        setflag_o( val_signed( val ) ^ ( 0 != ( val & 0x40 ) ) );
    *pval = val;
} //op_rcr

template <typename T> void x64::op_sal( T * pval, uint8_t shift ) // aka shl
{
    T x = *pval;
    if ( 1 == shift )
        setflag_o( 3 == top2bits( x ) );

    for ( uint8_t s = 0; s < shift; s++ )
    {
        setflag_c( val_signed( x ) );
        x <<= 1;
    }

    *pval = x;
    set_PSZ( x );
} //op_sal

template <typename T> void x64::op_shr( T * pval, uint8_t shift )
{
    T x = *pval;
    if ( 1 == shift )
        setflag_o( val_signed( x ) );
    x >>= shift;
    *pval = x;
    set_PSZ( x );
} //op_shr

template <typename T> void x64::op_sar( T * pval, uint8_t shift )
{
    using ST = std::make_signed_t<T>;
    ST x = *pval;
    if ( 1 == shift )
        setflag_o( false );
    x >>= shift;
    *pval = x;
    set_PSZ( x );
} //op_sar

template <typename T> void x64::op_shift( T * pval, uint8_t operation, uint8_t shift )
{
    assert( operation <= 7 );
    switch( operation )
    {
        case 0: op_rol( pval, shift ); break;
        case 1: op_ror( pval, shift ); break;
        case 2: op_rcl( pval, shift ); break;
        case 3: op_rcr( pval, shift ); break;
        case 4: op_sal( pval, shift ); break;   // aka shl
        case 5: op_shr( pval, shift ); break;
        case 6: unhandled(); break;
        case 7: op_sar( pval, shift ); break;
    }
} //op_shift

#ifdef _WIN32
__declspec(noinline)
#endif
void x64::unhandled()
{
    printf( "\n  rip %llx, op %x, base %llx, mem_size %llx, stack_top %llx, stack_size %llx\n", rip, getui8( rip ), base, mem_size, stack_top, stack_size );
    printf( "_prefix_rex %#x, _prefix_size %#x, _prefix_sse2_repeat %#x, _prefix_segment %#x\n", _prefix_rex, _prefix_size, _prefix_sse2_repeat, _prefix_segment );
    printf( "_rexW %#x, _rexR %#x, _rexX %#x, _rexB %#x\n", _rexW, _rexR, _rexX, _rexB );
    printf( "_mod %#x, _reg %#x, _rm %#x\n", _mod, _reg, _rm );
    tracer.Trace( "\n  rip %llx, op %x, base %llx, mem_size %llx, stack_top %llx, stack_size %llx\n", rip, getui8( rip ), base, mem_size, stack_top, stack_size );
    tracer.Trace( "  _mod %u rexW %u, rexR %u, rexX %u, rexB %u, _reg %#x, _rm %#x\n", _mod, _rexW, _rexR, _rexX, _rexB, _reg, _rm );
    tracer.Trace( "  _displacement: %#llx, sibScale %u, sibIndex %u, sibBase %#x\n", _displacement, _sibScale, _sibIndex, _sibBase );
    force_trace_xregs();
    emulator_hard_termination( *this, "opcode not handled:", getui8( rip ) );
} //unhandled

/*
    https://wiki.osdev.org/X86-64_Instruction_Encoding#SIB

    Instructions that default to 64-bit operand size in long mode are: (all others default to 32-bit operand size)
        CALL (near)  ENTER      Jcc JrCXZ    JMP (near)  LEAVE       LGDT    LIDT    LLDT       LOOP        LOOPcc        LTR
        MOV CR(n)    MOV DR(n)  POP reg/mem  POP reg     POP FS      POP GS  POPFQ   PUSH imm8  PUSH imm32  PUSH reg/mem  PUSH reg
        PUSH FS      PUSH GS    PUSHFQ       RET (near)

    R/M addressing modes:

          +---------------------------------------------------------------------------------------------------------------------------------------------------------+
          | B.R/M                                                                                                                                                   |
          | 0.000 0.001 0.010 0.011 | 0.100          | 0.101              | 0.110 0.111 1.000 1.001 1.010 1.011 | 1.100          | 1.101              | 1.110 1.111 |
          | AX    CX    DX    BX    | SP             | BP                 | SI    DI    R8    R9    R10   R11   | R12            | R13                | R14   R15   |
      Mod +-------------------------+----------------+--------------------+-------------------------------------+----------------+--------------------+-------------+
      00  | [r/m]                   | [sib]          | [rip + disp32]     | [r/m]                               | [sib]          | [rip + disp32]     | [r/m]       |
      01  | [r/m + disp8]           | [sib + disp8]  | [r/m + disp8]                                            | [sib + disp8]  | [r/m + disp8]                    |
      10  | [r/m + disp32]          | [sib + disp32] | [r/m + disp32]                                           | [sib + disp32] | [r/m + disp32]                   |
      11  | r/m                                                                                                                                                     |
          +---------------------------------------------------------------------------------------------------------------------------------------------------------+

    When R/M indicates that SIB should be used:

          +-----------+-------------------------------+------------------------+---------------------------------------------+------------------------+----------------------+
          |           | B.Base                                                                                                                                               |
      Mod |           | 0.000 0.001 0.010 0.011 0.100 | 0.101                  | 0.110 0.111 1.000 1.001 1.010 1.011 | 1.100 | 1.101                  | 1.110 1.111          |
      00  | X.Index   | AX    CX    DX    BX    SP    | BP                     | SI    DI    R8    R9    R10   R11   | R12   | R13                    | R14   R15            |
          +-----------+-------------------------------+------------------------+---------------------------------------------+------------------------+----------------------+
          | 0.000 AX  | [base + (index * s)]          | [(index * s) + disp32] | [base + (index * s)]                        | [(index * s) + disp32] | [base + (index * s)] |
          | 0.001 CX  |                               |                        |                                             |                        |                      |
          | 0.010 DX  |                               |                        |                                             |                        |                      |
          | 0.011 BX  |                               |                        |                                             |                        |                      |
          +-----------+-------------------------------+------------------------+---------------------------------------------+------------------------+----------------------+
          | 0.100 SP  | [base]                        | [disp32]               | [base]                                      | [disp32]               | [base]               |
          +-----------+-------------------------------+------------------------+---------------------------------------------+------------------------+----------------------+
          | 0.101 BP  | [base + (index * s)]          | [(index * s) + disp32] | [base + (index * s)]                        | [(index * s) + disp32] | [base + (index * s)] |
          | 0.110 SI  |                               |                        |                                             |                        |                      |
          | 0.111 DI  |                               |                        |                                             |                        |                      |
          | 1.000 R8  |                               |                        |                                             |                        |                      |
          | 1.001 R9  |                               |                        |                                             |                        |                      |
          | 1.010 R10 |                               |                        |                                             |                        |                      |
          | 1.000 R11 |                               |                        |                                             |                        |                      |
          | 1.011 R12 |                               |                        |                                             |                        |                      |
          | 1.101 R13 |                               |                        |                                             |                        |                      |
          | 1.110 R14 |                               |                        |                                             |                        |                      |
          | 1.111 R15 |                               |                        |                                             |                        |                      |
          +-----------+-------------------------------+------------------------+---------------------------------------------+------------------------+----------------------+

          +-----------+-------------------------------------------------------------------------------------------------+
          |           | B.Base                                                                                          |
      Mod |           | 0.000 0.001 0.010 0.011 0.100 0.101 0.110 0.111 1.000 1.001 1.010 1.011 1.100 1.101 1.110 1.111 |
      01  | X.Index   | AX    CX    DX    BX    SP    BP    SI    DI    R8    R9    R10   R11   R12   R13   R14   R15   |
          +-----------+-------------------------------------------------------------------------------------------------+
          | 0.000 AX  | [base + (index * s) + disp8]                                                                    |
          | 0.001 CX  |                                                                                                 |
          | 0.010 DX  |                                                                                                 |
          | 0.011 BX  |                                                                                                 |
          +-----------+-------------------------------------------------------------------------------------------------+
          | 0.100 SP  | [base + disp8]                                                                                  |
          +-----------+-------------------------------------------------------------------------------------------------+
          | 0.101 BP  | [base + (index * s) + disp8]                                                                    |
          | 0.110 SI  |                                                                                                 |
          | 0.111 DI  |                                                                                                 |
          | 1.000 R8  |                                                                                                 |
          | 1.001 R9  |                                                                                                 |
          | 1.010 R10 |                                                                                                 |
          | 1.000 R11 |                                                                                                 |
          | 1.011 R12 |                                                                                                 |
          | 1.101 R13 |                                                                                                 |
          | 1.110 R14 |                                                                                                 |
          | 1.111 R15 |                                                                                                 |
          +-----------+-------------------------------------------------------------------------------------------------+

          +-----------+-------------------------------------------------------------------------------------------------+
          |           | B.Base                                                                                          |
      Mod |           | 0.000 0.001 0.010 0.011 0.100 0.101 0.110 0.111 1.000 1.001 1.010 1.011 1.100 1.101 1.110 1.111 |
      10  | X.Index   | AX    CX    DX    BX    SP    BP    SI    DI    R8    R9    R10   R11   R12   R13   R14   R15   |
          +-----------+-------------------------------------------------------------------------------------------------+
          | 0.000 AX  | [base + (index * s) + disp32]                                                                   |
          | 0.001 CX  |                                                                                                 |
          | 0.010 DX  |                                                                                                 |
          | 0.011 BX  |                                                                                                 |
          +-----------+-------------------------------------------------------------------------------------------------+
          | 0.100 SP  | [base + disp32]                                                                                 |
          +-----------+-------------------------------------------------------------------------------------------------+
          | 0.101 BP  | [base + (index * s) + disp32]                                                                   |
          | 0.110 SI  |                                                                                                 |
          | 0.111 DI  |                                                                                                 |
          | 1.000 R8  |                                                                                                 |
          | 1.001 R9  |                                                                                                 |
          | 1.010 R10 |                                                                                                 |
          | 1.000 R11 |                                                                                                 |
          | 1.011 R12 |                                                                                                 |
          | 1.101 R13 |                                                                                                 |
          | 1.110 R14 |                                                                                                 |
          | 1.111 R15 |                                                                                                 |
          +-----------+-------------------------------------------------------------------------------------------------+

    REX byte structure
      Bit(s)  Value   Description
      ------  -----   -----------
      [7:4]   0100    Fixed bit pattern to identify a REX prefix.
      [3]     W       Width: 1 for 64-bit operand size. 0 for default (usually 32-bit).
      [2]     R       Reg field extension: Extension bit for the reg field in ModR/M.
      [1]     X       Index field extension: Extension bit for the index field in SIB.
      [0]     B       Base/r/m field extension: Extension bit for the r/m or base field.

    SIB byte structure
      Bit(s)  Value   Description
      ------  -----   -----------
      [7:6]   scale   Scale: Determines the scaling factor (1, 2, 4, or 8) to be applied to the index register.
      [5:3]   index   Index: Specifies a register to be used as an index into memory. The REX.X bit can extend this field.
                      5 in extended index indicates no index register is used. if the base register is rip, it's rip-relative addressing
      [2:0]   base    Base: Specifies a register to be used as the base for the address calculation. The REX.B bit can extend this field.

    R/M byte:
      mod      r/m     Interpretation
      ---      ---     --------------
      11       000-111 Register-direct addressing: The operand is a register. The r/m field, along with REX.B, selects one of the 16 general-purpose registers (RAX-R15).
      00       100     SIB byte required: The ModR/M byte is followed by a SIB byte to specify a complex memory address.
      00       101     RIP-relative addressing: This is a special case in 64-bit mode where the memory address is calculated relative to the instruction pointer (RIP).
                       The r/m=101 and mod=00 combination, which typically specifies a base register in other modes, is repurposed for this.
      01/10    100     SIB byte required, with 8-bit (mod=01) or 32-bit (mod=10) displacement.
*/

void x64::decode_sib()
{
    assert( ( _mod < 3 ) && ( 4 == ( _rm & 7 ) ) ); // SIB byte required
    uint8_t sib = get_rip8();
    _sibScale = sib >> 6;
    _sibIndex = ( ( sib >> 3 ) & 7 );
    if ( _rexX )
        _sibIndex |= 8;
    _sibBase = ( sib & 7 );

    if ( ( 2 == _mod ) || ( ( 0 == _mod ) && ( 5 == _sibBase ) ) ) // 32-bit displacement
        _displacement = sign_extend( get_rip32(), 31 );
    else if ( 1 == _mod ) // 8-bit displacement
        _displacement = sign_extend( get_rip8(), 7 );

    if ( _rexB )
        _sibBase |= 8;
} //decode_sib

inline void x64::decode_rex()
{
    if ( 0 == _prefix_rex )
        _rexB = _rexX = _rexR = _rexW = 0;
    else
    {
        assert( 0x40 == ( 0xf0 & _prefix_rex ) );
        _rexW = ( 0 != ( _prefix_rex & 8 ) );      // W = wide (64-bit), not 32-bit
        _rexR = ( 0 != ( _prefix_rex & 4 ) );      // high bit for _reg
        _rexX = ( 0 != ( _prefix_rex & 2 ) );      // high bit for _sibIndex
        _rexB = ( _prefix_rex & 1 );               // high bit for _rm or _sibBase

        if ( _rexR )
            _reg |= 8;
        if ( _rexB )
            _rm |= 8;
    }
} //decode_rex

void x64::decode_rm()
{
    uint8_t modRM = get_rip8();
    _rm = ( modRM & 7 );             // register or memory operand
    uint8_t saved_rm = _rm;
    _reg = ( ( modRM >> 3 ) & 7 );   // register operand
    _mod = ( modRM >> 6 );           // this indicates whether the _rm operand is a register or memory location

    decode_rex();

    if ( _mod < 3 ) // if r/m refers to memory
    {
        if ( 4 == saved_rm )
            decode_sib();
        else if ( ( 2 == _mod ) || ( ( 0 == _mod ) && ( 5 == saved_rm ) ) ) // 32-bit displacement
            _displacement = sign_extend( get_rip32(), 31 );
        else if ( 1 == _mod ) // 8-bit displacement
            _displacement = sign_extend( get_rip8(), 7 );
    }
} //decode_rm

const char * x64::register_name( uint8_t reg, uint8_t byte_width, bool is_xmm )
{
    if ( is_xmm )
        return xmm_names[ reg ];
    if ( 1 == byte_width )
    {
        if ( 0 != _prefix_rex )
            return register_names8[ reg ];
        return register_names8_old[ reg ];
    }
    if ( 2 == byte_width )
        return register_names16[ reg ];
    if ( 4 == byte_width )
        return register_names32[ reg ];
    if ( 8 != byte_width )
        unhandled();

    return register_names[ reg ];
} //register_name

uint64_t x64::effective_address()
{
    uint64_t ea;

    if ( _mod < 3 )
    {
        if ( 4 == ( _rm & 7 ) ) // SIB byte is used
        {
            if ( 0 == _mod )
            {
                if ( 4 == _sibIndex )
                {
                    if ( 5 == ( _sibBase & 7 ) ) // [disp32]
                        ea = _displacement;
                    else // [base]
                        ea = regs[ _sibBase ].q;
                }
                else if ( 5 == ( _sibBase & 7 ) ) // [(index * s) + disp32]
                    ea = ( ( regs[ _sibIndex ].q << _sibScale ) + _displacement );
                else // [base + (index * s)]
                    ea = regs[ _sibBase ].q + ( regs[ _sibIndex ].q << _sibScale );
            }
            else // ( 1 == _mod || 2 == _mod )
            {
                if ( 4 == _sibIndex ) // [base + disp8]
                    ea = regs[ _sibBase ].q + _displacement;
                else // [base + (index * s ) + disp8]
                    ea = regs[ _sibBase ].q + ( regs[ _sibIndex ].q << _sibScale ) + _displacement;
            }
        }
        else if ( 0 == _mod )
        {
            if ( 5 == ( _rm & 7 ) ) // [rip + disp32] RIP-relative addressing. relative to the following instruction
                ea = rip + _displacement;
            else // [ r/m ]
            {
                assert( ( _rm <= 3 ) || ( _rm >= 6 && _rm <= 11 ) || ( _rm >= 14 && _rm <= 15 ) );
                ea = regs[ _rm ].q;
            }
        }
        else // ( 1 == _mod || ( 2 == _mod ) // [r/m + disp8] or [r/m + disp32]
            ea = regs[ _rm ].q + _displacement;
    }
    else // [ r/m ]
        ea = regs[ _rm ].q;

    if ( 0 != _prefix_segment )
    {
        if ( 0x64 == _prefix_segment )
            ea += fs;
        else if ( 0x65 == _prefix_segment )
            ea += gs;
        else
            unhandled();
    }

    return ea;
} //effective_address

const char * x64::rm_string( uint8_t byte_width, bool is_xmm )
{
    static char rmstr[ 80 ] = {0};

    if ( _mod < 3 )
    {
        if ( 4 == ( _rm & 7 ) ) // SIB byte is used
        {
            if ( 0 == _mod )
            {
                if ( 4 == _sibIndex )
                {
                    if ( 5 == ( _sibBase & 7 ) ) // [disp32]
                        snprintf( rmstr, _countof( rmstr ), "[ %#llx ]", _displacement );
                    else // [base]
                        snprintf( rmstr, _countof( rmstr ), "[ %s ]", register_names[ _sibBase ] );
                }
                else if ( 5 == ( _sibBase & 7 ) ) // [(index * s) + disp32]
                {
                    if ( 0 == _sibScale )
                        snprintf( rmstr, _countof( rmstr ), "[ %s + %#llx ]", register_names[ _sibIndex ], _displacement );
                    else
                        snprintf( rmstr, _countof( rmstr ), "[ ( %s << %u ) + %#llx ]", register_names[ _sibIndex ], _sibScale, _displacement );
                }
                else // [base + (index * s)]
                {
                    if ( 0 == _sibScale )
                        snprintf( rmstr, _countof( rmstr ), "[ %s + %s ]", register_names[ _sibBase ], register_names[ _sibIndex ] );
                    else
                        snprintf( rmstr, _countof( rmstr ), "[ %s + ( %s << %u ) ]", register_names[ _sibBase ], register_names[ _sibIndex ], _sibScale );
                }
            }
            else // ( 1 == _mod || 2 == _mod )
            {
                if ( 4 == _sibIndex )
                    snprintf( rmstr, _countof( rmstr ), "[ %s + %#llx ]", register_names[ _sibBase ], _displacement );
                else if ( 0 == _sibScale )
                    snprintf( rmstr, _countof( rmstr ), "[ %s + %s + %#llx ]", register_names[ _sibBase ], register_names[ _sibIndex ], _displacement );
                else
                    snprintf( rmstr, _countof( rmstr ), "[ %s + ( %s << %u ) + %#llx ]", register_names[ _sibBase ], register_names[ _sibIndex ], _sibScale, _displacement );
            }
        }
        else if ( 0 == _mod )
        {
            if ( 5 == ( _rm & 7 ) ) // [rip + disp32] RIP-relative addressing
                snprintf( rmstr, _countof( rmstr ), "[ rip + %#llx ]", _displacement );
            else
            {
                assert( ( _rm <= 3 ) || ( _rm >= 6 && _rm <= 11 ) || ( _rm >= 14 && _rm <= 15 ) );
                snprintf( rmstr, _countof( rmstr ), "[ %s ]", register_names[ _rm ] );
            }
        }
        else if ( 1 == _mod || 2 == _mod ) // [r/m + disp8]
            snprintf( rmstr, _countof( rmstr ), "[ %s + %#llx ]", register_names[ _rm ], _displacement );
        else
            unhandled();

        return rmstr;
    }

    return register_name( _rm, byte_width, is_xmm );
} //rm_string

uint64_t bitscan( uint64_t x )
{
    uint64_t bit = 0;
    while ( 0 != x )
    {
        if ( x & 1 )
            return bit;
        bit++;
        x >>= 1;
    }
    return 0; // undefined
} //bitscan

uint64_t bitscan_reverse( uint64_t x )
{
    uint64_t bit = 63;
    while ( 0 != x )
    {
        if ( x & 0x8000000000000000 )
            return bit;
        bit--;
        x <<= 1;
    }
    return 0; // undefined
} //bitscan_reverse

uint8_t saturate_i16_to_ui8( int16_t x )
{
    if ( x > UINT8_MAX )
        return UINT8_MAX;
    if ( x < 0 )
        return 0;
    return (uint8_t) x;
} //saturate_i16_to_ui8

int16_t saturate_ui32_to_i16( uint32_t x )
{
    if ( x > INT16_MAX )
        return INT16_MAX;
    return (uint16_t) x;
} //saturate_ui32_to_i16

int16_t saturate_i32_to_i16( int32_t x )
{
    if ( x > INT16_MAX )
        return INT16_MAX;
    if ( x < INT16_MIN )
        return INT16_MIN;
    return (int16_t) x;
} //saturate_i32_to_i16

int8_t saturate_i16_to_i8( int16_t x )
{
    if ( x > INT8_MAX )
        return INT8_MAX;
    if ( x < INT8_MIN )
        return INT8_MIN;
    return (int8_t) x;
} //saturate_i16_to_i8

template <typename T> T do_fmin( T a, T b )
{
    if ( ( 0.0 == a ) && ( 0.0 == b ) )
        return b;

    if ( my_isnan( a ) || my_isnan( b ) )
        return b;

    return get_min( a, b );
} //do_fmin

template <typename T> T do_fmax( T a, T b )
{
    if ( ( 0.0 == a ) && ( 0.0 == b ) )
        return b;

    if ( my_isnan( a ) || my_isnan( b ) )
        return b;

    return get_max( a, b );
} //do_fmax

float set_float_sign( float f, bool sign )
{
    uint32_t val = sign ? ( ( * (uint32_t *) &f ) | 0x80000000 ) : ( ( * (uint32_t *) &f ) & 0x7fffffff );
    return * (float *) &val;
} //set_float_sign

double set_double_sign( double d, bool sign )
{
    uint64_t val = sign ? ( ( * (uint64_t *) &d ) | 0x8000000000000000 ) : ( ( * (uint64_t *) &d ) & 0x7fffffffffffffff );
    return * (double *) &val;
} //set_double_sign

template <typename T> T do_fadd( T a, T b )
{
    bool ainf = isinf( a );
    bool binf = isinf( b );

    if ( ainf && binf )
    {
        if ( signbit( a ) == signbit( b ) )
            return a;
        return (T) -MY_NAN;
    }

    if ( my_isnan( a ) )
        return a;

    if ( my_isnan( b ) )
        return b;

    if ( ainf )
        return a;

    if ( binf )
        return b;

    return a + b;
} //do_fadd

template <typename T> T do_fsub( T a, T b )
{
    if ( isinf( a ) && isinf( b ) )
    {
        if ( signbit( a ) != signbit( b ) )
            return a;
        return (T) -MY_NAN;
    }

    if ( my_isnan( a ) )
        return a;

    if ( my_isnan( b ) )
        return b;

    T result = a - b;
    if ( my_isnan( result ) ) // never return -NAN
        return (T) MY_NAN;

    return result;
} //do_fsub

template <typename T> T do_fmul( T a, T b )
{
    if ( my_isnan( a ) )
        return a;

    if ( my_isnan( b ) )
        return b;

    bool ainf = isinf( a );
    bool binf = isinf( b );
    bool azero = ( 0.0 == a );
    bool bzero = ( 0.0 == b );

    if ( ( ainf && bzero ) || ( azero && binf ) )
        return (T) -MY_NAN;

    if ( ainf && binf )
        return (T) set_double_sign( INFINITY, ( signbit( a ) != signbit( b ) ) );

    if ( ainf || binf )
        return (T) set_double_sign( INFINITY, signbit( a ) != signbit( b ) );

    if ( azero || bzero )
        return (T) set_double_sign( 0.0, signbit( a ) != signbit( b ) );

    return a * b;
} //do_fmul

template <typename T> T do_fdiv( T a, T b )
{
    if ( my_isnan( a ) )
        return a; // x64 implementations generally propagate the sign of the first nan in this case

    if ( my_isnan( b ) )
        return b;

    bool ainf = isinf( a );
    bool binf = isinf( b );
    bool azero = ( 0.0 == a );
    bool bzero = ( 0.0 == b );

    if ( ainf && binf )
        return (T) -MY_NAN;

    if ( azero && bzero )
        return (T) -MY_NAN;

    if ( ainf )
        return (T) set_double_sign( INFINITY, signbit( a ) != signbit( b ) );

    if ( binf )
        return (T) set_double_sign( 0.0, signbit( a ) != signbit( b ) );

    if ( azero )
        return (T) set_double_sign( 0.0, signbit( a ) != signbit( b ) );

    return a / b;
} //do_fdiv

const uint8_t ccB = 2;
const uint8_t ccNB = 3;
const uint8_t ccE = 4;
const uint8_t ccNE = 5;
const uint8_t ccBE = 6;
const uint8_t ccNBE = 7;
const uint8_t ccU = 10;
const uint8_t ccNU = 11;

bool x64::check_condition( uint8_t condition )
{
    assert( condition < 16 );
    switch( condition )
    {                                                           //                   hints:
        case 0:  return flag_o();                               // jo                o = overflow
        case 1:  return !flag_o();                              // jno               n = not
        case 2:  return flag_c();                               // jb / jnae / jc    b = below, ae = above or equal, c = carry. Unsigned here
        case 3:  return !flag_c();                              // jnb / jae / jnc
        case 4:  return flag_z();                               // je / jz           e = equal, z = zero
        case 5:  return !flag_z();                              // jne / jnz
        case 6:  return flag_c() || flag_z();                   // jbe / jna
        case 7:  return !flag_c() && !flag_z();                 // jnbe / ja
        case 8:  return flag_s();                               // js                s = signed
        case 9:  return !flag_s();                              // jns
        case 10: return flag_p();                               // jp / jpe          p / pe = parity even
        case 11: return !flag_p();                              // jnp / jpo         po = parity odd
        case 12: return ( flag_s() != flag_o() );               // jl / jnge         l = less than, nge = not greater than or equal. Signed here
        case 13: return ( flag_s() == flag_o() );               // jnl / jge
        case 14: return flag_z() || ( flag_s() != flag_o() );   // jle / jng         le = less than or equal, ng = not greather than
        default: return !flag_z() && ( flag_s() == flag_o()  ); // jnle / jg         must be 15, but to work around a bogus compiler warning
    }
} //check_condition

const uint32_t fccG = 0;   // greater than floating condition code
const uint32_t fccL = 1;   // less than
const uint32_t fccE = 2;   // equal
const uint32_t fccU = 3;   // unordered

static const char * fcc_names[4] = { "equal", "less", "greater", "unordered" };

template <typename T> inline uint32_t x64::compare_floating( T a, T b )
{
    #if defined( __clang__ ) // clang compares long doubles that are infinite incorretly

        tracer.Trace( "a %lf (nan %u, inf %u, subnormal %u), b %lf (nan %u, inf %u, subnormal %u)\n",
                      (double) a, my_isnan( a ), my_isinf( a ), my_issubnormal( a ),
                      (double) b, my_isnan( b ), my_isinf( b ), my_issubnormal( b ) );

    #endif

    if ( my_isnan( a ) || my_isnan( b ) )
        return fccU;

    if ( a == b )
        return fccE;
    if ( a < b )
        return fccL;
    if ( a > b )
        return fccG;

    return fccU;
} //compare_floating

void x64::set_x87_status_compare( uint32_t fcc )
{
    if ( fccG == fcc )
        set_x87_status_c320( false, false, false );
    else if ( fccL == fcc )
        set_x87_status_c320( false, false, true );
    else if ( fccE == fcc )
        set_x87_status_c320( true, false, false );
    else
        set_x87_status_c320( true, true, true );
} //set_x87_status_compare

// match table 3-1 comparison predicates for cmppd and cmpps instructions

static const bool floating_comparison_results[ 32 ][ 4 ] =
{
    /* EQ_OQ (EQ)  0H  Equal (ordered, non-signaling) */                    {  false, false, true,  false },
    /* LT_OS (LT)  1H  Less-than (ordered, signaling) */                    {  false, true,  false, false },
    /* LE_OS (LE)  2H  Less-than-or-equal (ordered, signaling) */           {  false, true,  true,  false },
    /* UNORD_Q (UNORD) 3H  Unordered (non-signaling) */                     {  false, false, false, true },
    /* NEQ_UQ (NEQ)    4H  Not-equal (unordered, non-signaling) */          {  true,  true,  false, true },
    /* NLT_US (NLT)    5H  Not-less-than (unordered, signaling) */          {  true,  false, true,  true },
    /* NLE_US (NLE)    6H  Not-less-than-or-equal (unordered, signaling) */ {  true,  false, false, true },
    /* ORD_Q (ORD) 7H  Ordered (non-signaling) */                           {  true,  true,  true,  false },
    /* EQ_UQ   8H  Equal (E) (unordered, non-signaling) */                  {  false, false, true,  true },
    /* NGE_US (NGE) 9H  Not-greater-than-or-equal (unordered, signaling) */ {  false, true,  false, true },
    /* NGT_US (NGT)    AH  Not-greater-than (unordered, signaling) */       {  false, true,  true,  true },
    /* FALSE_OQ(FALSE) BH  False (ordered, non-signaling) */                {  false, false, false, false },
    /* NEQ_OQ  CH (NE)  Not-equal (ordered, non-signaling) */               {  true,  true,  false, false },
    /* GE_OS (GE)  DH  Greater-than-or-equal (ordered, signaling) */        {  true,  false, true,  false },
    /* GT_OS (GT)  EH  Greater-than (ordered, signaling) */                 {  true,  false, false, false },
    /* TRUE_UQ(TRUE)   FH  True (unordered, non-signaling) */               {  true,  true,  true,  true },
    /* EQ_OS   10H Equal (ordered, signaling) */                            {  false, false, true,  false },
    /* LT_OQ   11H Less-than (ordered, nonsignaling) */                     {  false, true,  false, false },
    /* LE_OQ   12H Less-than-or-equal (ordered, nonsignaling) */            {  false, true,  true,  false },
    /* UNORD_S 13H Unordered (signaling) */                                 {  false, false, false, true },
    /* NEQ_US  14H Not-equal (unordered, signaling) */                      {  true,  true,  false, true },
    /* NLT_UQ  15H Not-less-than (unordered, nonsignaling) */               {  true,  false, true,  true },
    /* NLE_UQ  16H Not-less-than-or-equal (unordered, nonsignaling) */      {  true,  false, false, true },
    /* ORD_S   17H Ordered (signaling) */                                   {  true,  true,  true,  false },
    /* EQ_US   18H Equal (unordered, signaling) */                          {  false, false, true,  true },
    /* NGE_UQ  19H Not-greater-than-or-equal (unordered, non-signaling) */  {  false, true,  false, true },
    /* NGT_UQ  1AH Not-greater-than (unordered, nonsignaling) */            {  false, true,  true,  true },
    /* FALSE_OS    1BH False (ordered, signaling) */                        {  false, false, false, false },
    /* NEQ_OS  1CH Not-equal (ordered, signaling) */                        {  true,  true,  false, false },
    /* GE_OQ   1DH Greater-than-or-equal (ordered, nonsignaling) */         {  true,  false, true,  false },
    /* GT_OQ   1EH Greater-than (ordered, nonsignaling) */                  {  true,  false, false, false },
    /* TRUE_US 1FH True (unordered, signaling) */                           {  true,  true,  true,  true }
};

template <typename T> inline bool x64::floating_comparison_true( T a, T b, uint8_t predicate )
{
    uint32_t fcc = compare_floating( a, b );
    return floating_comparison_results[ predicate & 0x1f ][ fcc ];
} //floating_comparison_true

void x64::set_eflags_from_fcc( uint32_t fcc )
{
    setflag_o( false );
    setflag_a( false );
    setflag_s( false );

    if ( fccU == fcc )
    {
        setflag_z( true );
        setflag_p( true );
        setflag_c( true );
        return;
    }
    if ( fccG == fcc )
    {
        setflag_z( false );
        setflag_p( false );
        setflag_c( false );
        return;
    }
    if ( fccL == fcc )
    {
        setflag_z( false );
        setflag_p( false );
        setflag_c( true );
        return;
    }

    assert( fccE == fcc );
    setflag_z( true );
    setflag_p( false );
    setflag_c( false );
} //set_eflags_from_fcc

uint64_t x64::run()
{
    uint64_t instruction_count = 0;

    for ( ;; )
    {
        instruction_count++;        // 14.7% of runtime including _prefix initialization below
        _prefix_rex = 0;            // can be 0x40 through 0x4f to indicate high registers and 64-bit operands
        _prefix_size = 0;           // can be 0x66 (operand size 16)
        _prefix_sse2_repeat = 0;    // can be 0xf2 or 0xf3 for SSE2 instructions or string repeat opcodes
        _prefix_segment = 0;        // 0x64 for fs: or 0x65 for gs:

_prefix_is_set:

        #ifndef NDEBUG
            if ( regs[ rsp ].q <= ( stack_top - stack_size ) )
                emulator_hard_termination( *this, "stack pointer is below stack memory:", regs[ rsp ].q );
            if ( regs[ rsp ].q > stack_top + 0x100 ) // give space to get at arguments and AT records
                emulator_hard_termination( *this, "stack pointer is above the top of its starting point:", regs[ rsp ].q );
            if ( rip < base )
                emulator_hard_termination( *this, "rip is lower than memory:", rip );
            if ( rip >= ( base + mem_size - stack_size ) )
                emulator_hard_termination( *this, "rip is higher than it should be:", rip );
        #endif

        if ( 0 != g_State )         // 1.1% of runtime
        {
            if ( g_State & stateEndEmulation )
            {
                g_State &= ~stateEndEmulation;
                break;
            }

            if ( ( g_State & stateTraceInstructions ) && tracer.IsEnabled() )
                trace_state();
        }

        uint8_t op = get_rip8();    // 18% of runtime

        switch( op )                // 20.7% of runtime setting up for the switch
        {
            case 0x00: case 0x08: case 0x10: case 0x18: case 0x20: case 0x28: case 0x30: case 0x38:  // math r/m8, r8. rex math r/m8, r8 sign-extended to 64 bits
            {
                decode_rm();
                uint8_t math = ( op >> 3 ) & 7;
                do_math( math, get_rm_ptr8(), get_reg8() );
                break;
            }
            case 0x01: case 0x09: case 0x11: case 0x19: case 0x21: case 0x29: case 0x31: case 0x39:  // math r/m, r (32 or 64 bit depending on _rexW)
            {
                decode_rm();
                uint8_t math = ( op >> 3 ) & 7;
                if ( _rexW ) // if wide/64-bit
                {
                    uint64_t val = get_rm64();
                    do_math( math, &val, regs[ _reg ].q );
                    if ( 7 != math )
                        set_rm64( val );
                }
                else if ( 0x66 == _prefix_size )
                {
                    uint16_t val = get_rm16();
                    do_math( math, &val, regs[ _reg ].w );
                    if ( 7 != math )
                        set_rm16( val );
                }
                else
                {
                    uint32_t val = get_rm32();
                    do_math( math, &val, regs[ _reg ].d );
                    if ( 7 != math )
                        set_rm32z( val );
                }
                break;
            }
            case 0x02: case 0x0a: case 0x12: case 0x1a: case 0x22: case 0x2a: case 0x32: case 0x3a:  // math r8, rm/8. rex math r/m8 sign-extended to 64 bits
            {
                decode_rm();
                uint8_t math = ( op >> 3 ) & 7;
                if ( _rexW ) // if wide/64-bit
                    do_math( math, & regs[ _reg ].q, (uint64_t) sign_extend( get_rm8(), 7 ) );
                else
                {
                    uint8_t val = regs[ _reg ].b;
                    do_math( math, &val, get_rm8() );
                    if ( 7 != math )
                        regs[ _reg ].q = val; // 0-extend the result
                }
                break;
            }
            case 0x03: case 0x0b: case 0x13: case 0x1b: case 0x23: case 0x2b: case 0x33: case 0x3b: // math r, r/m (32 or 64 bit depending on _rexW)
            {
                decode_rm();
                uint8_t math = ( op >> 3 ) & 7;
                if ( _rexW ) // if wide/64-bit
                    do_math( math, & regs[ _reg ].q, get_rm64() );
                else if ( 0x66 == _prefix_size )
                    do_math( math, & regs[ _reg ].w, get_rm16() );
                else
                {
                    do_math( math, & regs[ _reg ].d, get_rm32() );
                    if ( 7 != math ) // don't mask the result on cmp
                        regs[ _reg ].q &= 0xffffffff;
                }
                break;
            }
            case 0x04: case 0x0c: case 0x14: case 0x1c: case 0x24: case 0x2c: case 0x34: case 0x3c: // math al, imm8
            {
                uint8_t math = ( op >> 3 ) & 7;
                uint8_t val = regs[ rax ].b;
                uint8_t imm = get_rip8();
                do_math( math, &val, imm );
                if ( 7 != math ) // don't write on cmp
                    regs[ rax ].b = val; // only modify the low byte
                break;
            }
            case 0x05: case 0x0d: case 0x15: case 0x1d: case 0x25: case 0x2d: case 0x35: case 0x3d: // math ax, imm16 / math eax, imm32 / math rax, se( imm32 )
            {
                uint8_t math = ( op >> 3 ) & 7;
                decode_rex();
                if ( _rexW )
                {
                    uint32_t imm = (int32_t) get_rip32();
                    do_math( math, & regs[ rax ].q, (uint64_t) sign_extend( imm, 31 ) );
                }
                else if ( 0x66 == _prefix_size )
                {
                    uint16_t imm = get_rip16();
                    uint16_t regval = regs[ rax ].w;
                    do_math( math, &regval, imm );
                    if ( 7 != math )
                        regs[ rax ].q = regval;
                }
                else
                {
                    uint32_t imm = (int32_t) get_rip32();
                    uint32_t regval = regs[ rax ].d;
                    do_math( math, &regval, imm );
                    if ( 7 != math )
                        regs[ rax ].q = regval;
                }
                break;
            }
            case 0x0f:
            {
                uint8_t op1 = get_rip8();

                switch ( op1 )
                {
                    case 5: // syscall
                    {
                        emulator_invoke_svc( *this );
                        break;
                    }
                    case 0x10:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // movupd xmm1, xmm2/m128 move 128 bits of unaligned double from xmm2/mem to xmm1
                        {
                            xregs[ _reg ].setd( 0, get_rmxdouble( 0 ) );
                            xregs[ _reg ].setd( 1, get_rmxdouble( 1 ) );
                        }
                        else if ( 0xf2 == _prefix_sse2_repeat ) // movsd xmm1, xmm2/m64. move scalar double from source to xmm1
                        {
                            xregs[ _reg ].set64( 0, get_rmx64( 0 ) );
                            if ( 3 != _mod )
                                xregs[ _reg ].set64( 1, 0 );
                        }
                        else if ( 0xf3 == _prefix_sse2_repeat ) // movss xmm1, xmm2/m32
                        {
                            xregs[ _reg ].set32( 0, get_rmx32( 0 ) );
                            if ( 3 != _mod )
                            {
                                xregs[ _reg ].set32( 1, 0 );
                                xregs[ _reg ].set64( 1, 0 );
                            }
                        }
                        else // movups xmm, xmm/m128. move unaligned 128 bits of single precision fp from xmm/mem to xmm
                        {
                            for ( uint32_t o = 0; o < 4; o++ )
                                xregs[ _reg ].set32( o, get_rmx32( o ) );
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x11:
                    {
                        decode_rm();
                        if ( 0xf2 == _prefix_sse2_repeat ) // movsd xmm1/m64, xmm2  move scalar double from xmm2 to xmm1/m64
                            set_rmx64( 0, xregs[ _reg ].get64( 0 ) );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // movss xmm1/m64, xmm2  move scalar float from xmm2 to xmm1/m64
                            set_rmx32( 0, xregs[ _reg ].get32( 0 ) );
                        else if ( 0x66 == _prefix_size ) // movupd xmm2/m128, xmm1  move 128 bits of doubles from xmm1 to xmm2/mem
                        {
                            for ( uint32_t o = 0; o < 2; o++ )
                                set_rmx64( o, xregs[ _reg ].get64( o ) );
                        }
                        else // movups xmm2/m128, xmm1   move 128 bits of unaligned packed single precision fp from xmm1 to xmm2/mem
                        {
                            for ( uint32_t o = 0; o < 4; o++ )
                                set_rmx32( o, xregs[ _reg ].get32( o ) );
                        }
                        if ( 3 == _rm )
                            trace_xreg( _reg );
                        break;
                    }
                    case 0x12:
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // movlpd xmm1, m64. moves double from m64 to low qword of xmm1 and doesn't touch high qword
                            xregs[ _reg ].set64( 0, get_rm64() );
                        else if ( 3 == _mod ) // movhlps xmm1, xmm2   move two packed floats from high qw of xmm2 to low qw of xmm1
                        {
                            xregs[ _reg ].set32( 0, get_rmx32( 2 ) );
                            xregs[ _reg ].set32( 1, get_rmx32( 3 ) );
                        }
                        else // movlps xmm1, m64   move two floats from m64 to low qw of xmm1. Same opcode as movhps but with memory as source read LOW floats
                        {
                            xregs[ _reg ].set32( 0, get_rmx32( 0 ) );
                            xregs[ _reg ].set32( 1, get_rmx32( 1 ) );
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x13:
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // movlpd m64, xmm1. moves double from low qword of xmm1 to m64
                            set_rmx64( 0, xregs[ _reg ].get64( 0 ) );
                        else   // movlps xmm1, m64   and movlps xmm1, xmm2
                            set_rmx32_2( xregs[ _reg ].get32( 0 ), xregs[ _reg ].get32( 1 ) );
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x14:
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // unpcklpd xmm1, xmm2/m128 unpack doubles from low of xmm1 and xmm2/m128 into low of xmm1 and high of xmm1
                            xregs[ _reg ].set64( 1, get_rmx64( 0 ) ); // don't copy low of xmm1 over itself
                        else // unpcklps xmm1, xmm2/m128 unpack singles from low of xmm1 and xmm2/m128
                        {
                            vec16_t src1 = xregs[ _reg ];
                            vec16_t target = xregs[ _reg ];
                            target.setf( 0, src1.getf( 0 ) );
                            target.setf( 1, get_rmxfloat( 0 ) );
                            target.setf( 2, src1.getf( 1 ) );
                            target.setf( 3, get_rmxfloat( 1 ) );
                            xregs[ _reg ] = target;
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x15:
                    {
                        decode_rm();
                        vec16_t src1 = xregs[ _reg ];
                        vec16_t & dst = xregs[ _reg ];
                        if ( 0x66 == _prefix_size ) // unpckhpd xmm1, xmm2/m128
                        {
                            dst.set64( 0, src1.get64( 1 ) );
                            dst.set64( 1, get_rmx64( 1 ) );
                        }
                        else // unpckhps xmm1, xmm2/m128
                        {
                            vec16_t target = xregs[ _reg ];
                            target.setf( 0, src1.getf( 2 ) );
                            target.setf( 1, get_rmxfloat( 2 ) );
                            target.setf( 2, src1.getf( 3 ) );
                            target.setf( 3, get_rmxfloat( 3 ) );
                            dst = target;
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x16:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // movlpd xmm1, m64. moves double from m64 to low qword of xmm1 and doesn't touch high qword
                            xregs[ _reg ].set64( 1, get_rm64() );
                        else // movlhps xmm1, xmm2/m64
                        {
                            xregs[ _reg ].set32( 2, get_rmx32( 0 ) );
                            xregs[ _reg ].set32( 3, get_rmx32( 1 ) );
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x17:
                    {
                        decode_rm();
                        vec16_t src1 = xregs[ _reg ];
                        if ( 0x66 == _prefix_size ) // movhpd m64, xmm1  moves double from high quadword to r/m
                            set_rmdouble( src1.getd( 1 ) );
                        else // movhps xmm2/m64, xmm1
                        {
                            set_rmxfloat( 0, src1.getf( 2 ) );
                            set_rmxfloat( 1, src1.getf( 3 ) );
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x18:
                    {
                        decode_rm();
                        switch ( _reg )
                        {
                            case 0: case 1: case 2: case 3: { break; }
                            default: { unhandled(); }
                        }
                        break;
                    }
                    case 0x1e:
                    {
                        uint8_t op2 = getui8( rip );
                        if ( 0xfa == op2 ) // endbr64
                            rip++;
                        else
                        {
                            decode_rm();
                            if ( 1 == _reg )
                                { /* cet isn't implemented */ }  // rdsspd/rdsspq
                            else
                                unhandled();
                        }
                        break;
                    }
                    case 0x1f: // nopl
                    {
                        uint8_t op2 = get_rip8();
                        if ( 0 == op2 )
                            { /* do nothing */ }
                        else if ( 0x40 == op2 )
                            rip += 1;
                        else if ( 0x44 == op2 )
                            rip += 2;
                        else if ( 0x80 == op2 )
                            rip += 4;
                        else if ( 0x84 == op2 )
                            rip += 5;
                        else
                            unhandled();
                        break;
                    }
                    case 0x28: // movaps xmm, xmm/m128   move 4 aligned packed single precision fp values from xmm/m128 to xmm
                    {
                        decode_rm();

                        if ( 0x66 == _prefix_size ) // movapd xmm1, xmm2/m128
                        {
                            vec16_t & dst = xregs[ _reg ];
                            dst.set64( 0, get_rmx64( 0 ) );
                            dst.set64( 1, get_rmx64( 1 ) );
                        }
                        else // movaps xmm, xmm/m128
                        {
                            vec16_t & dst = xregs[ _reg ];
                            dst.set32( 0, get_rmx32( 0 ) );
                            dst.set32( 1, get_rmx32( 1 ) );
                            dst.set32( 2, get_rmx32( 2 ) );
                            dst.set32( 3, get_rmx32( 3 ) );
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x29: // movaps xmm/m128, xmm   move 4 aligned packed single precision fp values from xmm1 to xmm2/m128
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 == _prefix_size )
                            set_rmx64_2( xregs[ _reg ].get64( 0 ), xregs[ _reg ].get64( 1 ) );
                        else
                            set_rmx32_4( xregs[ _reg ].get32( 0 ), xregs[ _reg ].get32( 1 ), xregs[ _reg ].get32( 2 ), xregs[ _reg ].get32( 3 ) );
                        break;
                    }
                    case 0x2a:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                            unhandled();
                        if ( 0xf2 == _prefix_sse2_repeat )  // cvtsi2sd xmm1, r32/m32   convert dword or qword to scalar double
                            xregs[ _reg ].setd( 0, (double) ( _rexW ? (int64_t) get_rm64() : (int32_t) get_rm32() ) );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // cvtsi2ss xmm1, r32/m32
                            xregs[ _reg ].setf( 0, (float) ( _rexW ? (int64_t) get_rm64() : (int32_t) get_rm32() ) );
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x2c: // cvttsd2si r32/r64, xmm1/m64  convert scalar double to signed integer
                    {
                        decode_rm();
                        if ( 0xf2 == _prefix_sse2_repeat )
                            regs[ _reg ].q = _rexW ? ( (int64_t) get_rmxdouble( 0 ) ) : ( (uint32_t) (int32_t) get_rmxdouble( 0 ) );
                        else if ( 0xf3 == _prefix_sse2_repeat )
                            regs[ _reg ].q = _rexW ? ( (int64_t) get_rmxfloat( 0 ) ) : ( (uint32_t) (int32_t) get_rmxfloat( 0 ) );
                        else
                            unhandled();
                        break;
                    }
                    case 0x2e: // ucomisd xmm1, xmm2/m64 compare low doubles and set eflags accordingly. also ucomiss
                    {
                        decode_rm();
                        if ( 0 == _prefix_sse2_repeat )
                        {
                            if ( 0x66 == _prefix_size )
                            {
                                uint32_t fcc = compare_floating( xregs[ _reg ].getd( 0 ), get_rmxdouble( 0 ) );
                                set_eflags_from_fcc( fcc );
                            }
                            else
                            {
                                uint32_t fcc = compare_floating( xregs[ _reg ].getf( 0 ), get_rmxfloat( 0 ) );
                                set_eflags_from_fcc( fcc );
                            }
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0x2f:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )// comisd xmm1, xmm2/m32  compare low double scalar values and set eflags
                        {
                            uint32_t fcc = compare_floating( xregs[ _reg ].getd( 0 ), get_rmxdouble( 0 ) );
                            set_eflags_from_fcc( fcc );
                        }
                        else if ( 0 == _prefix_sse2_repeat ) // comiss xmm1, xmm2/m32  compare low float scalar values and set eflags
                        {
                            uint32_t fcc = compare_floating( xregs[ _reg ].getf( 0 ), get_rmxfloat( 0 ) );
                            set_eflags_from_fcc( fcc );
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: // cmovcc reg, r/
                    case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
                    {
                        decode_rm();
                        if ( check_condition( op1 & 0xf ) )
                            regs[ _reg ].q = get_rm();
                        break;
                    }
                    case 0x50:
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 == _prefix_size ) // movmskpd reg, xmm. extract 2-bit sign mask from xmm and store in reg. the upper bits are filled with zeroes
                        {
                            uint32_t val = val_signed( xregs[ _rm ].get64( 0 ) );
                            val |= ( val_signed( xregs[ _rm ].get64( 1 ) ) << 1 );
                            regs[ _reg ].q = val; // zero-extend the value
                        }
                        else // movmkps reg, xmm    extract 4 bit sign mask from xmm and store in reg
                        {
                            uint32_t val = val_signed( xregs[ _rm ].get32( 0 ) );
                            val |= ( val_signed( xregs[ _rm ].get32( 1 ) ) << 1 );
                            val |= ( val_signed( xregs[ _rm ].get32( 2 ) ) << 2 );
                            val |= ( val_signed( xregs[ _rm ].get32( 3 ) ) << 3 );
                            regs[ _reg ].q = val; // zero-extend the value
                        }
                        break;
                    }
                    case 0x51:
                    {
                        decode_rm();
                        vec16_t & dst = xregs[ _reg ];
                        if ( 0xf2 == _prefix_sse2_repeat ) // sqrtsd xmm1, xmm2/m64  sqrt of low double
                            dst.setd( 0, sqrt( get_rmxdouble( 0 ) ) );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // sqrtss xmm1 / xmm2/m32  sqrt of low float
                            dst.setf( 0, sqrtf( get_rmxfloat( 0 ) ) );
                        else if ( 0x66 == _prefix_size ) // sqrtpd xmm1, xmm2/m128  compute sqrt of packed doubles
                        {
                            for ( uint32_t e = 0; e < 2; e++ )
                                dst.setd( e, sqrt( get_rmxdouble( e ) ) );
                        }
                        else if ( 0 == _prefix_sse2_repeat && 0 == _prefix_size ) // sqrtps xmm1, xmm2/m128. compute sqrt of packed singles
                        {
                            for ( uint32_t e = 0; e < 4; e++ )
                                dst.setf( e, sqrtf( get_rmxfloat( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x52:
                    {
                        decode_rm();
                        if ( 0 != _prefix_size || 0 != _prefix_sse2_repeat )
                            unhandled();
                        // rsqrtps xmm1, xmm2/m128 compute reciprocals of the square roots of packed floats
                        vec16_t & dst = xregs[ _reg ];
                        for ( uint32_t e = 0; e < 4; e++ )
                            dst.setf( e, 1.0f / sqrtf( get_rmxfloat( e ) ) );
                       break;
                    }
                    case 0x54: // andpd xmm, xmm/m128   bitwise and. also andps
                    {
                        decode_rm();

                        if ( 0x66 == _prefix_size ) // movapd xmm1, xmm2/m128
                        {
                            vec16_t & dst = xregs[ _reg ];
                            dst.set64( 0, dst.get64( 0 ) & get_rmx64( 0 ) );
                            dst.set64( 1, dst.get64( 1 ) & get_rmx64( 1 ) );
                        }
                        else // movaps xmm, xmm/m128
                        {
                            vec16_t & dst = xregs[ _reg ];
                            dst.set32( 0, dst.get32( 0 ) & get_rmx32( 0 ) );
                            dst.set32( 1, dst.get32( 1 ) & get_rmx32( 1 ) );
                            dst.set32( 2, dst.get32( 2 ) & get_rmx32( 2 ) );
                            dst.set32( 3, dst.get32( 3 ) & get_rmx32( 3 ) );
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x55: // andnpd/andnps xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        bool wide = ( 0x66 == _prefix_size );
                        vec16_t & dst = xregs[ _reg ];
                        if ( wide )
                            for ( uint32_t e = 0; e < 2; e++ )
                                dst.set64( e, ( ~dst.get64( e ) ) & get_rmx64( e ) );
                        else
                            for ( uint32_t e = 0; e < 4; e++ )
                                dst.set32( e, ( ~dst.get32( e ) ) & get_rmx32( e ) );
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x56: // orpd/orps xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        bool wide = ( 0x66 == _prefix_size );
                        vec16_t & dst = xregs[ _reg ];
                        if ( wide )
                            for ( uint32_t e = 0; e < 2; e++ )
                                dst.set64( e, dst.get64( e ) | get_rmx64( e ) );
                        else
                            for ( uint32_t e = 0; e < 4; e++ )
                                dst.set32( e, dst.get32( e ) | get_rmx32( e ) );
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x57: // xorpd/xorps xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        bool wide = ( 0x66 == _prefix_size );
                        vec16_t & dst = xregs[ _reg ];
                        if ( wide )
                            for ( uint32_t e = 0; e < 2; e++ )
                                dst.set64( e, dst.get64( e ) ^ get_rmx64( e ) );
                        else
                            for ( uint32_t e = 0; e < 4; e++ )
                                dst.set32( e, dst.get32( e ) ^ get_rmx32( e ) );
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x58:
                    {
                        decode_rm();
                        vec16_t & xmm1 = xregs[ _reg ];
                        if ( 0xf2 == _prefix_sse2_repeat ) // addsd xmm1, xmm2/m64
                            xmm1.setd( 0, do_fadd( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                        else if ( 0x66 == _prefix_size ) // addpd xmm1, xmm2/m128   add packed double values
                        {
                            xmm1.setd( 0, do_fadd( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                            xmm1.setd( 1, do_fadd( xmm1.getd( 1 ), get_rmxdouble( 1 ) ) );
                        }
                        else if ( 0xf3 == _prefix_sse2_repeat ) // addss xmm1, xmm2/m64
                            xmm1.setf( 0, do_fadd( xmm1.getf( 0 ), get_rmxfloat( 0 ) ) );
                        else if ( 0 == _prefix_size && 0 == _prefix_sse2_repeat ) // addps xmm1, xmm2/m128   add packed float values
                        {
                            for ( uint32_t e = 0; e < 4; e++ )
                                xmm1.setf( e, do_fadd( xmm1.getf( e ), get_rmxfloat( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x59:
                    {
                        decode_rm();
                        vec16_t & xmm1 = xregs[ _reg ];
                        if ( 0xf2 == _prefix_sse2_repeat ) // mulsd xmm1, xmm2/m64
                            xmm1.setd( 0, do_fmul( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                        else if ( 0x66 == _prefix_size ) // mulpd xmm1, xmm2/m128   multiply packed double values
                        {
                            xmm1.setd( 0, do_fmul( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                            xmm1.setd( 1, do_fmul( xmm1.getd( 1 ), get_rmxdouble( 1 ) ) );
                        }
                        else if ( 0xf3 == _prefix_sse2_repeat ) // mulss xmm1, xmm2/m64
                            xmm1.setf( 0, do_fmul( xmm1.getf( 0 ), get_rmxfloat( 0 ) ) );
                        else if ( 0 == _prefix_size && 0 == _prefix_sse2_repeat ) // mulps xmm1, xmm2/m128   multiply packed float values
                        {
                            for ( uint32_t e = 0; e < 4; e++ )
                                xmm1.setf( e, do_fmul( xmm1.getf( e ), get_rmxfloat( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x5a:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // cvtpd2ps xmm1, xmm2/m128   convert two packed doubles in 2 to two floats in 1
                        {
                            double val0 = get_rmxdouble( 0 );
                            double val1 = get_rmxdouble( 1 );
                            xregs[ _reg ].setf( 0, (float) val0 );
                            xregs[ _reg ].setf( 1, (float) val1 );
                        }
                        else if ( 0xf3 == _prefix_sse2_repeat ) // cvtss2sd xmm1, xmm2/m32   convert scalar float to scalar double
                            xregs[ _reg ].setd( 0, (double) get_rmxfloat( 0 ) );
                        else if ( 0xf2 == _prefix_sse2_repeat ) // cvtsd2ss xmm1, xmm2/m32   convert scalar double to scalar float
                            xregs[ _reg ].setf( 0, (float) get_rmxdouble( 0 ) );
                        else // cvtps2pd xmm1, xmm2/m64 convert two packed floats in 2 to two packed doubles in 1
                        {
                            float val0 = get_rmxfloat( 0 );
                            float val1 = get_rmxfloat( 1 );
                            xregs[ _reg ].setd( 0, (double) val0 );
                            xregs[ _reg ].setd( 1, (double) val1 );
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x5b:
                    {
                        decode_rm();
                        if ( 0 != _prefix_size )
                            unhandled();
                        vec16_t & xmm1 = xregs[ _reg ];
                        if ( 0xf3 == _prefix_sse2_repeat ) // cvttps2dq xmm1, xmm2/m128   convert 4 packed floats to signed dwords using trucation
                        {
                            xmm1.set32( 0, (int32_t) trunc( get_rmxfloat( 0 ) ) );
                            xmm1.set32( 1, (int32_t) trunc( get_rmxfloat( 1 ) ) );
                            xmm1.set32( 2, (int32_t) trunc( get_rmxfloat( 2 ) ) );
                            xmm1.set32( 3, (int32_t) trunc( get_rmxfloat( 3 ) ) );
                        }
                        else if ( 0 == _prefix_sse2_repeat ) // cvtdq2ps xmm1, xmm2/m128   convert 4 packed signed dwords from xmm2/m128 t 4 packed signed floats in xmm1
                        {
                            xmm1.setf( 0, (float) (int32_t) get_rmx32( 0 ) );
                            xmm1.setf( 1, (float) (int32_t) get_rmx32( 1 ) );
                            xmm1.setf( 2, (float) (int32_t) get_rmx32( 2 ) );
                            xmm1.setf( 3, (float) (int32_t) get_rmx32( 3 ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x5c:
                    {
                        decode_rm();
                        vec16_t & xmm1 = xregs[ _reg ];
                        if ( 0xf2 == _prefix_sse2_repeat ) // subsd xmm1, xmm2/m64
                            xmm1.setd( 0, do_fsub( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                        else if ( 0x66 == _prefix_size ) // subpd xmm1, xmm2/m128   subtract packed double values
                        {
                            xmm1.setd( 0, do_fsub( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                            xmm1.setd( 1, do_fsub( xmm1.getd( 1 ), get_rmxdouble( 1 ) ) );
                        }
                        else if ( 0xf3 == _prefix_sse2_repeat ) // subss xmm1, xmm2/m64
                            xmm1.setf( 0, do_fsub( xmm1.getf( 0 ), get_rmxfloat( 0 ) ) );
                        else if ( 0 == _prefix_size && 0 == _prefix_sse2_repeat ) // subps xmm1, xmm2/m128   subtract packed float values
                        {
                            for ( uint32_t e = 0; e < 4; e++ )
                                xmm1.setf( e, do_fsub( xmm1.getf( e ), get_rmxfloat( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x5d:
                    {
                        decode_rm();
                        vec16_t & xmm1 = xregs[ _reg ];
                        if ( 0xf2 == _prefix_sse2_repeat ) // minsd xmm1, xmm2/m64
                            xmm1.setd( 0, do_fmin( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                        else if ( 0x66 == _prefix_size ) // minpd xmm1, xmm2/m128   minide packed double values
                        {
                            xmm1.setd( 0, do_fmin( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                            xmm1.setd( 1, do_fmin( xmm1.getd( 1 ), get_rmxdouble( 1 ) ) );
                        }
                        else if ( 0xf3 == _prefix_sse2_repeat ) // minss xmm1, xmm2/m64
                            xmm1.setf( 0, do_fmin( xmm1.getf( 0 ), get_rmxfloat( 0 ) ) );
                        else if ( 0 == _prefix_size && 0 == _prefix_sse2_repeat ) // minps xmm1, xmm2/m128   minide packed float values
                        {
                            for ( uint32_t e = 0; e < 4; e++ )
                                xmm1.setf( e, do_fmin( xmm1.getf( e ), get_rmxfloat( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x5e:
                    {
                        decode_rm();
                        vec16_t & xmm1 = xregs[ _reg ];
                        if ( 0xf2 == _prefix_sse2_repeat ) // divsd xmm1, xmm2/m64
                            xmm1.setd( 0, do_fdiv( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                        else if ( 0x66 == _prefix_size ) // divpd xmm1, xmm2/m128   divide packed double values
                        {
                            xmm1.setd( 0, do_fdiv( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                            xmm1.setd( 1, do_fdiv( xmm1.getd( 1 ), get_rmxdouble( 1 ) ) );
                        }
                        else if ( 0xf3 == _prefix_sse2_repeat ) // divss xmm1, xmm2/m64
                            xmm1.setf( 0, do_fdiv( xmm1.getf( 0 ), get_rmxfloat( 0 ) ) );
                        else if ( 0 == _prefix_size && 0 == _prefix_sse2_repeat ) // divps xmm1, xmm2/m128   divide packed float values
                        {
                            for ( uint32_t e = 0; e < 4; e++ )
                                xmm1.setf( e, do_fdiv( xmm1.getf( e ), get_rmxfloat( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x5f:
                    {
                        decode_rm();
                        vec16_t & xmm1 = xregs[ _reg ];
                        if ( 0xf2 == _prefix_sse2_repeat ) // maxsd xmm1, xmm2/m64
                            xmm1.setd( 0, do_fmax( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                        else if ( 0x66 == _prefix_size ) // maxpd xmm1, xmm2/m128   get max packed double values
                        {
                            xmm1.setd( 0, do_fmax( xmm1.getd( 0 ), get_rmxdouble( 0 ) ) );
                            xmm1.setd( 1, do_fmax( xmm1.getd( 1 ), get_rmxdouble( 1 ) ) );
                        }
                        else if ( 0xf3 == _prefix_sse2_repeat ) // maxss xmm1, xmm2/m64
                            xmm1.setf( 0, do_fmax( xmm1.getf( 0 ), get_rmxfloat( 0 ) ) );
                        else if ( 0 == _prefix_size && 0 == _prefix_sse2_repeat ) // maxps xmm1, xmm2/m128   get max packed float values
                        {
                            for ( uint32_t e = 0; e < 4; e++ )
                                xmm1.setf( e, do_fmax( xmm1.getf( e ), get_rmxfloat( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x60: // punpcklbw xmm1, xmm2/m128
                    {
                        decode_rm();
                        vec16_t xmm1 = xregs[ _reg ];
                        vec16_t target = xregs[ _reg ];
                        for ( uint32_t x = 0; x < 8; x++ )
                        {
                            target.set8( 2 * x, xmm1.get8( x ) );
                            target.set8( 2 * x + 1, get_rmx8( x ) );
                        }
                        xregs[ _reg ] = target;
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x61: // punpcklwd xmm1, xmm2/m128
                    {
                        decode_rm();
                        vec16_t xmm1 = xregs[ _reg ];
                        vec16_t target = xregs[ _reg ];
                        for ( uint32_t x = 0; x < 4; x++ )
                        {
                            target.set16( 2 * x, xmm1.get16( x ) );
                            target.set16( 2 * x + 1, get_rmx16( x ) );
                        }
                        xregs[ _reg ] = target;
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x62: // punpckldq xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t target = xregs[ _reg ];
                            target.set32( 0, xregs[ _reg ].get32( 0 ) );
                            target.set32( 1, get_rmx32( 0 ) );
                            target.set32( 2, xregs[ _reg ].get32( 1 ) );
                            target.set32( 3, get_rmx32( 1 ) );
                            xregs[ _reg ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x63:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // packsswb xmm1, xmm2/m128   convert 8 packed int16_t from xmm1 and xmm2/m128 into 16 packed int8_t using signed saturation
                        {
                            vec16_t src1 = xregs[ _reg ];
                            vec16_t target = xregs[ _reg ];
                            for ( uint32_t e = 0; e < 8; e++ )
                            {
                                target.set8( e, saturate_i16_to_i8( src1.get16( e ) ) );
                                target.set8( 8 + e, saturate_i16_to_i8( get_rmx16( e ) ) );
                            }
                            xregs[ _reg ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x64: // pcmpgtb xmm1, xmm2/m128
                    {
                        decode_rm();
                        vec16_t dst = xregs[ _reg ];
                        if ( 0x66 == _prefix_size )
                        {
                            for ( uint32_t i = 0; i < 16; i++ )
                                xregs[ _reg ].set8( i, ( (int8_t) dst.get8( i ) > (int8_t) get_rmx8( i ) ) ? 0xff : 0 );
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0x65: // pcmpgtw xmm1, xmm2/m128
                    {
                        decode_rm();
                        vec16_t dst = xregs[ _reg ];
                        if ( 0x66 == _prefix_size )
                        {
                            for ( uint32_t i = 0; i < 8; i++ )
                                xregs[ _reg ].set16( i, ( (int16_t) dst.get16( i ) > (int16_t) get_rmx16( i ) ) ? 0xffff : 0 );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x66: // pcmpgtd xmm1, xmm2/m128
                    {
                        decode_rm();
                        vec16_t dst = xregs[ _reg ];
                        if ( 0x66 == _prefix_size )
                        {
                            for ( uint32_t i = 0; i < 4; i++ )
                                xregs[ _reg ].set32( i, ( (int32_t) dst.get32( i ) > (int32_t) get_rmx32( i ) ) ? ~0 : 0 );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x67: // packuswb xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t target = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 8; i++ )
                            {
                                target.set8( i, saturate_i16_to_ui8( xregs[ _reg ].get16( i ) ) );
                                target.set8( 8 + i, saturate_i16_to_ui8( get_rmx16( i ) ) );
                            }
                            xregs[ _reg ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x68: // punpckhbw xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t target = xregs[ _reg ];
                            vec16_t & src1 = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 16; i += 2 )
                            {
                                target.set8( i, src1.get8( 8 + i / 2 ) );
                                target.set8( i + 1, get_rmx8( 8 + i / 2 ) );
                            }
                            xregs[ _reg ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x69: // punpckhwd xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t dst = xregs[ _reg ];
                            vec16_t target = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 4; i++ )
                            {
                                target.set16( i * 2, dst.get16( i + 4 ) );
                                target.set16( i * 2 + 1, get_rmx16( i + 4 ) );
                            }
                            xregs[ _reg ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x6a: // punpckhdq xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t target = xregs[ _reg ];
                            target.set32( 0, xregs[ _reg ].get32( 2 ) );
                            target.set32( 1, get_rmx32( 2 ) );
                            target.set32( 2, xregs[ _reg ].get32( 3 ) );
                            target.set32( 3, get_rmx32( 3 ) );
                            xregs[ _reg ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x6b:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // packssdw xmm1, xmm2/m128  converts 4 packed dwords from xmm1 and xmm2/m128 into 8 packed signed word integers using signed saturation
                        {
                            vec16_t src1 = xregs[ _reg ];
                            vec16_t target = xregs[ _reg ];
                            for ( uint32_t e = 0; e < 4; e++ )
                            {
                                target.set16( e, saturate_i32_to_i16( src1.get32( e ) ) );
                                target.set16( 4 + e, saturate_i32_to_i16( get_rmx32( e ) ) );
                            }
                            xregs[ _reg ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x6c: // punpcklqdq xmm, xmm/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t target = xregs[ _reg ];
                            target.set64( 0, xregs[ _reg ].get64( 0 ) );
                            target.set64( 1, get_rmx64( 0 ) );
                            xregs[ _reg ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x6d: // punpckhqdq xmm, xmm/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            xregs[ _reg ].set64( 0, xregs[ _reg ].get64( 1 ) );
                            xregs[ _reg ].set64( 1, get_rmx64( 1 ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x6e: // movd xmm, r/m32   movq xmm, r/m64
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            xregs[ _reg ].zero();
                            if ( _rexW )
                                xregs[ _reg ].set64( 0, get_rm64() );
                            else
                                xregs[ _reg ].set32( 0, get_rm32() );
                        }
                        else
                            unhandled(); // mmx not supported
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x6f:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ||         // movdqa xmm1, xmm2/m128   move 128 bits of aligned packed integer values from xmm2/m128 to xmm1
                             0xf3 == _prefix_sse2_repeat ) // movdqu xmm1, xmm2/m128   move 128 bits of unaligned packed integer values from xmm2/m128 to xmm1
                        {
                            xregs[ _reg ].set64( 0, get_rmx64( 0 ) );
                            xregs[ _reg ].set64( 1, get_rmx64( 1 ) );
                        }
                        else
                            unhandled(); // mmx not supported
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x70:
                    {
                        decode_rm();
                        uint8_t imm8 = get_rip8();
                        if ( 0xf2 == _prefix_sse2_repeat ) // pshuflw xmm1, xmm2/m128, imm8
                        {
                            vec16_t & dst = xregs[ _reg ];
                            vec16_t xmm1 = xregs[ _reg ];
                            uint64_t src0 = get_rmx64( 0 );
                            uint64_t src1 = get_rmx64( 1 );
                            for ( uint32_t x = 0; x < 4; x++ )
                                dst.set16( x, (uint16_t) ( src0 >> 16 * ( ( 3 & ( imm8 >> ( 2 * x ) ) ) ) ) );
                            dst.set64( 1, src1 );
                        }
                        else if ( 0xf3 == _prefix_sse2_repeat ) // pshufhw xmm1, xmm2/m128, imm8
                        {
                            vec16_t & dst = xregs[ _reg ];
                            vec16_t xmm1 = xregs[ _reg ];
                            uint64_t src0 = get_rmx64( 0 );
                            uint64_t src1 = get_rmx64( 1 );
                            for ( uint32_t x = 4; x < 8; x++ )
                                dst.set16( x, (uint16_t) ( src1 >> 16 * ( ( 3 & ( imm8 >> ( 2 * ( x - 4 ) ) ) ) ) ) );
                            dst.set64( 0, src0 );
                        }
                        else if ( 0x66 == _prefix_size ) // pshufd xmm, xmm/m128, imm8
                        {
                            vec16_t target = xregs[ _reg ];
                            target.set32( 0, get_rmx32( imm8 & 3 ) );
                            target.set32( 1, get_rmx32( ( imm8 >> 2 ) & 3 ) );
                            target.set32( 2, get_rmx32( ( imm8 >> 4 ) & 3 ) );
                            target.set32( 3, get_rmx32( ( imm8 >> 6 ) & 3 ) );
                            xregs[ _reg ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x71:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // pshufd xmm, xmm/m128, imm8
                        {
                            uint8_t shift = get_rip8();
                            vec16_t target = xregs[ _rm ];
                            if ( 2 == _reg ) // psrlw right logical
                            {
                                for ( uint32_t x = 0; x < 8; x++ )
                                    target.set16( x, get_rmx16( x ) >> shift );
                            }
                            else if ( 4 == _reg ) // psraw xmm1, imm8 shift signed words
                            {
                                for ( uint32_t x = 0; x < 8; x++ )
                                    target.set16( x, ( (int16_t) get_rmx16( x ) ) >> shift );
                            }
                            else if ( 6 == _reg ) // psllw
                            {
                                for ( uint32_t x = 0; x < 8; x++ )
                                    target.set16( x, get_rmx16( x ) << shift );
                            }
                            else
                                unhandled();
                            xregs[ _rm ] = target;
                        }
                        else
                            unhandled();
                        trace_xreg( _rm );
                        break;
                    }
                    case 0x72:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            uint8_t shift = get_rip8();

                            if ( 2 == _reg ) // psrld xmm1, imm8
                            {
                                for ( uint32_t x = 0; x < 4; x++ )
                                    xregs[ _rm ].set32( x, xregs[ _rm ].get32( x ) >> shift );
                            }
                            else if ( 4 == _reg ) // psrad arithmetic right
                            {
                                for ( uint32_t x = 0; x < 4; x++ )
                                    xregs[ _rm ].set32( x, ( (int32_t) xregs[ _rm ].get32( x ) ) >> shift );
                            }
                            else if ( 6 == _reg ) // pslld xmm1, imm8  shift packed data left logical
                            {
                                for ( uint32_t x = 0; x < 4; x++ )
                                    xregs[ _rm ].set32( x, xregs[ _rm ].get32( x ) << shift );
                            }
                            else
                                unhandled();
                        }
                        else
                            unhandled();
                        trace_xreg( _rm );
                        break;
                    }
                    case 0x73:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            uint8_t shift = get_rip8();
                            if ( 2 == _reg ) // psrlq xmm1, imm8
                            {
                                shift &= 0x3f;
                                vec16_t & r = xregs[ _rm ];
                                r.set64( 0, r.get64( 0 ) >> shift );
                                r.set64( 1, r.get64( 1 ) >> shift );
                            }
                            else if ( 3 == _reg ) // psrldq xmm1, imm8   shift dquad right logical
                            {
                                vec16_t & r = xregs[ _rm ];
                                if ( shift > 15 )
                                    r.zero();
                                else
                                {
                                    shift *= 8; // it's in bytes originally
                                    uint64_t low = r.get64( 0 );
                                    uint64_t high = r.get64( 1 );
                                    if ( shift >= 64 )
                                    {
                                        low = ( high >> ( shift - 64 ) );
                                        high = 0;
                                    }
                                    else
                                    {
                                        low >>= shift;
                                        low |= ( high << ( 64 - shift ) );
                                        high >>= shift;
                                    }
                                    r.set64( 0, low );
                                    r.set64( 1, high );
                                }
                            }
                            else if ( 6 == _reg ) // psllq xmm1, imm8
                            {
                                shift &= 0x3f;
                                vec16_t & r = xregs[ _rm ];
                                r.set64( 0, r.get64( 0 ) << shift );
                                r.set64( 1, r.get64( 1 ) << shift );
                            }
                            else if ( 7 == _reg ) // pslldq xmm1, imm8   shift dquad left logical
                            {
                                vec16_t & r = xregs[ _rm ];
                                if ( shift > 15 )
                                    r.zero();
                                else
                                {
                                    shift *= 8; // it's in bytes originally
                                    uint64_t low = r.get64( 0 );
                                    uint64_t high = r.get64( 1 );
                                    if ( shift >= 64 )
                                    {
                                        high = low << ( shift - 64 );
                                        low = 0;
                                    }
                                    else
                                    {
                                        high <<= shift;
                                        high |= ( low >> ( 64 - shift ) );
                                        low <<= shift;
                                    }
                                    r.set64( 0, low );
                                    r.set64( 1, high );
                                }
                            }
                            else
                                unhandled();
                        }
                        else
                            unhandled();
                        trace_xreg( _rm );
                        break;
                    }
                    case 0x74:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // pcmpeqb xmm, xmm/m128   compare packed bytes in rm to r for equality. if eq, set byte to 1 else 0
                        {
                            for ( uint32_t x = 0; x < 16; x++ )
                                xregs[ _reg ].set8( x, ( xregs[ _reg ].get8( x ) == get_rmx8( x ) ) ? 0xff : 0 );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x75:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // pcmpeqw xmm/m128, xmm   compare packed words in xmm/m128 and xmm1 for equality
                        {
                            for ( uint32_t x = 0; x < 8; x++ )
                                xregs[ _reg ].set16( x, ( xregs[ _reg ].get16( x ) == get_rmx16( x ) ) ? 0xffff : 0 );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x76:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // pcmpeqd xmm/m128, xmm   compare packed doublewords in xmm/m128 and xmm1 for equality
                        {
                            for ( uint32_t x = 0; x < 4; x++ )
                                xregs[ _reg ].set32( x, ( xregs[ _reg ].get32( x ) == get_rmx32( x ) ) ? 0xffffffff : 0 );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0x7e:
                    {
                        if ( 0xf3 == _prefix_sse2_repeat ) // movq r/m64, xmm
                        {
                            decode_rm();
                            xregs[ _reg ].set64( 0, get_rmx64( 0 ) );
                        }
                        else if ( 0x66 == _prefix_size ) // mov r/m, xmm
                        {
                            decode_rm();
                            if ( _rexW )
                                set_rm64( xregs[ _reg ].get64( 0 ) );
                            else
                                set_rm32z( xregs[ _reg ].get32( 0 ) );
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0x7f:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size || 0xf3 == _prefix_sse2_repeat ) // movdqa xmm2/m128, xmm1   move aligned packed integer values. or movdqu (unaligned)
                            set_rmx64_2( xregs[ _reg ].get64( 0 ), xregs[ _reg ].get64( 1 ) );
                        else
                            unhandled();
                        if ( 3 == _mod )
                            trace_xreg( _rm );
                        break;
                    }
                    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: // jcc rel32
                    case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
                    {
                        uint32_t offset = get_rip32();
                        if ( check_condition( op1 & 0xf ) )
                            rip += sign_extend( offset, 31 );
                        break;
                    }
                    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: // setcc
                    case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
                    {
                        decode_rm();
                        set_rm8( check_condition( op1 & 0xf ) );
                        break;
                    }
                    case 0xa2: // cpuid
                    {
                        if ( 0 == regs[ rax ].q )
                        {
                            regs[ rbx ].q = 0x756e6547;  // Genu
                            regs[ rdx ].q = 0x49656e69;  // ineI
                            regs[ rcx ].q = 0x6c65746e;  // ntel
                        }
                        else if ( 1 == regs[ rax ].q )
                        {
                            regs[ rax ].q = 0;
                            regs[ rcx ].q = 0;
                            regs[ rdx ].q = 0; //0x06000000; // sse and sse2
                        }
                        else if ( 0x80000000 == regs[ rax ].d )
                            regs[ rax ].q = 0;
                        else
                            unhandled();
                        break;
                    }
                    case 0xa3: // bt r/m, r 16/32/64
                    {
                        decode_rm();
                        if ( _rexW )
                        {
                            uint64_t bit = 1ull << ( regs[ _reg ].q & 0x3f );
                            uint64_t val = get_rm64();
                            setflag_c( val & bit );
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            uint16_t bit = 1 << ( regs[ _reg ].w & 0xf );
                            uint16_t val = get_rm16();
                            setflag_c( val & bit );
                        }
                        else
                        {
                            uint32_t bit = 1 << ( regs[ _reg ].d & 0x1f );
                            uint32_t val = get_rm32();
                            setflag_c( val & bit );
                        }
                        break;
                    }
                    case 0xa4: // shld r/m, r, imm
                    case 0xa5: // shld r/m, r, cl   double precision left shift and fill with bits from the top of r
                    {
                        decode_rm();
                        uint8_t count;
                        if ( 0xa5 == op1 )
                            count = regs[ rcx ].b;
                        else
                           count = get_rip8();

                        if ( _rexW )
                        {
                            count &= 0x3f;
                            if ( 0 == count )
                                break;
                            uint64_t destination = get_rm64();
                            setflag_c( get_bit( destination, 64 - count ) );
                            destination <<= count;
                            destination |= ( regs[ _reg ].q >> ( 64 - count ) );
                            set_rm64( destination );
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            count &= 0xf;
                            if ( 0 == count )
                                break;
                            uint16_t destination = get_rm16();
                            setflag_c( get_bit( destination, 16 - count ) );
                            destination <<= count;
                            destination |= ( regs[ _reg ].w >> ( 16 - count ) );
                            set_rm16( destination );
                        }
                        else
                        {
                            count &= 0x1f;
                            if ( 0 == count )
                                break;
                            uint32_t destination = get_rm32();
                            setflag_c( get_bit( destination, 32 - count ) );
                            destination <<= count;
                            destination |= ( regs[ _reg ].d >> ( 32 - count ) );
                            set_rm32( destination );
                        }
                        break;
                    }
                    case 0xab: // bts r/m, r 16/32/64
                    {
                        decode_rm();
                        if ( _rexW )
                        {
                            uint64_t bit = 1ull << ( regs[ _reg ].q & 0x3f );
                            uint64_t val = get_rm64();
                            setflag_c( val & bit );
                            set_rm64( val | bit );
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            uint16_t bit = 1 << ( regs[ _reg ].w & 0xf );
                            uint16_t val = get_rm16();
                            setflag_c( val & bit );
                            set_rm16( val | bit );
                        }
                        else
                        {
                            uint32_t bit = 1 << ( regs[ _reg ].d & 0x1f );
                            uint32_t val = get_rm32();
                            setflag_c( val & bit );
                            set_rm32( val | bit );
                        }
                        break;
                    }
                    case 0xac: // shrd r/m, r, imm8   double precision right shift and fill with bits from the top of r
                    case 0xad: // shrd r/m, r, cl   double precision right shift and fill with bits from the top of r
                    {
                        decode_rm();
                        uint8_t count;
                        if ( 0xad == op1 )
                            count = regs[ rcx ].b;
                        else
                           count = get_rip8();

                        if ( _rexW )
                        {
                            count &= 0x3f;
                            if ( 0 == count )
                                break;
                            uint64_t destination = get_rm64();
                            setflag_c( get_bit( destination, count - 1 ) );
                            destination >>= count;
                            destination |= ( regs[ _reg ].q << ( 64 - count ) );
                            set_rm64( destination );
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            count &= 0xf;
                            if ( 0 == count )
                                break;
                            uint16_t destination = get_rm16();
                            setflag_c( get_bit( destination, count - 1 ) );
                            destination >>= count;
                            destination |= ( regs[ _reg ].w << ( 16 - count ) );
                            set_rm16( destination );
                        }
                        else
                        {
                            count &= 0x1f;
                            if ( 0 == count )
                                break;
                            uint32_t destination = get_rm32();
                            setflag_c( get_bit( destination, count - 1 ) );
                            destination >>= count;
                            destination |= ( regs[ _reg ].d << ( 32 - count ) );
                            set_rm32( destination );
                        }
                        break;
                    }
                    case 0xae: // stmxcsr / ldmxcsr
                    {
                        uint8_t imm = getui8( rip );
                        if ( 0xf0 == imm || 0xf8 == imm ) // mfence / sfence
                            rip++; // do nothing
                        else
                        {
                            decode_rm();
                            if ( 2 == _reg )
                                mxcsr = get_rm32();
                            else if ( 3 == _reg )
                                set_rm32( mxcsr );
                            else
                                unhandled();
                        }
                        break;
                    }
                    case 0xaf: // imul r, r/m  in 16, 32, and 64
                    {
                        decode_rm();
                        if ( _rexW )
                        {
                            int64_t result128H = 0;
                            int64_t result128L = CMultiply128::mul_s64_s64( regs[ _reg ].q, get_rm64(), &result128H );
                            setflag_o( val_signed( result128H) != val_signed( result128L ) );
                            setflag_c( flag_o() );
                            regs[ _reg ].q = result128L;
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            uint32_t a = (uint32_t) sign_extend( regs[ _reg ].d, 15 );
                            uint32_t b = (uint32_t) sign_extend( get_rm16(), 15 );
                            uint32_t result32 = a * b;
                            uint16_t result16 = result32 & 0xffff;
                            setflag_o( val_signed( result32 ) != val_signed( result16 ) );
                            setflag_c( flag_o() );
                            regs[ _reg ].q = result16;
                        }
                        else
                        {
                            uint64_t a = sign_extend( regs[ _reg ].d, 31 );
                            uint64_t b = sign_extend( get_rm32(), 31 );
                            uint64_t result64 = a * b;
                            uint32_t result32 = result64 & 0xffffffff;
                            setflag_o( val_signed( result64 ) != val_signed( result32 ) );
                            setflag_c( flag_o() );
                            regs[ _reg ].q = result64;
                        }
                        break;
                    }
                    case 0xb0: // cmpxchg r/m8, r8
                    {
                        decode_rm();
                        uint8_t val = get_rm8();
                        if ( val == regs[ rax ].b )
                        {
                            setflag_z( true );
                            set_rm8( get_reg8() );
                        }
                        else
                        {
                            setflag_z( false );
                            regs[ rax ].b = val;
                        }
                        break;
                    }
                    case 0xb1: // cmpxchg r/m, r   16, 32, 64 compare ax with r/m
                    {
                        decode_rm();
                        if ( _rexW )
                        {
                            uint64_t val = get_rm64();
                            if ( val == regs[ rax ].q )
                            {
                                setflag_z( true );
                                set_rm64( regs[ _reg ].q );
                            }
                            else
                            {
                                setflag_z( false );
                                regs[ rax ].q = val;
                            }
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            uint16_t val = get_rm16();
                            if ( val == regs[ rax ].w )
                            {
                                setflag_z( true );
                                set_rm16( regs[ _reg ].w );
                            }
                            else
                            {
                                setflag_z( false );
                                regs[ rax ].q = val;
                            }
                        }
                        else
                        {
                            uint32_t val = get_rm32();
                            if ( val == regs[ rax ].d )
                            {
                                setflag_z( true );
                                set_rm32( regs[ _reg ].d );
                            }
                            else
                            {
                                setflag_z( false );
                                regs[ rax ].q = val;
                            }
                        }
                        break;
                    }
                    case 0xb6: // movzbq reg, r/m8
                    {
                        decode_rm();
                        regs[ _reg ].q = get_rm8();
                        break;
                    }
                    case 0xb7: // movzbq reg, r/m16
                    {
                        decode_rm();
                        regs[ _reg ].q = get_rm16();
                        break;
                    }
                    case 0xb3: // btr r/m, r  (16, 32, 64 bit test and reset)
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        decode_rm();
                        if ( _rexW )
                        {
                            uint8_t imm = regs[ _reg ].q & 0x3f;
                            uint64_t bit = 1ull << imm;
                            uint64_t val = get_rm64();
                            setflag_c( val & bit );
                            set_rm64( val & ~bit );
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            uint8_t imm = regs[ _reg ].w & 0xf;
                            uint16_t bit = 1 << imm;
                            uint16_t val = get_rm16();
                            setflag_c( val & bit );
                            set_rm16( val & ~bit );
                        }
                        else
                        {
                            uint8_t imm = regs[ _reg ].d & 0x1f;
                            uint32_t bit = 1 << imm;
                            uint32_t val = get_rm32();
                            setflag_c( val & bit );
                            set_rm32( val & ~bit );
                        }
                        break;
                    }
                    case 0xba:
                    {
                        decode_rm();
                        uint8_t imm = get_rip8();

                        if ( 4 == _reg ) // bts r/m, imm8  (16, 32, 64 bit test
                        {
                            if ( _rexW )
                            {
                                uint64_t bit = 1ull << ( imm & 0x3f );
                                uint64_t val = get_rm64();
                                setflag_c( val & bit );
                            }
                            else if ( 0x66 == _prefix_size )
                            {
                                uint16_t bit = 1 << ( imm & 0xf );
                                uint16_t val = get_rm16();
                                setflag_c( val & bit );
                            }
                            else
                            {
                                uint32_t bit = 1 << ( imm & 0x1f );
                                uint32_t val = get_rm32();
                                setflag_c( val & bit );
                            }
                        }
                        else if ( 5 == _reg ) // bts r/m, imm8  (16, 32, 64 bit test and set
                        {
                            if ( _rexW )
                            {
                                uint64_t bit = 1ull << ( imm & 0x3f );
                                uint64_t val = get_rm64();
                                setflag_c( val & bit );
                                set_rm64( val | bit );
                            }
                            else if ( 0x66 == _prefix_size )
                            {
                                uint16_t bit = 1 << ( imm & 0xf );
                                uint16_t val = get_rm16();
                                setflag_c( val & bit );
                                set_rm16( val | bit );
                            }
                            else
                            {
                                uint32_t bit = 1 << ( imm & 0x1f );
                                uint32_t val = get_rm32();
                                setflag_c( val & bit );
                                set_rm32( val | bit );
                            }
                        }
                        else if ( 6 == _reg ) // btr r/m, imm8  (16, 32, 64 bit test and set
                        {
                            if ( _rexW )
                            {
                                uint64_t bit = 1ull << ( imm & 0x3f );
                                uint64_t val = get_rm64();
                                setflag_c( val & bit );
                                set_rm64( val & ~bit );
                            }
                            else if ( 0x66 == _prefix_size )
                            {
                                uint16_t bit = 1 << ( imm & 0xf );
                                uint16_t val = get_rm16();
                                setflag_c( val & bit );
                                set_rm16( val & ~bit );
                            }
                            else
                            {
                                uint32_t bit = 1 << ( imm & 0x1f );
                                uint32_t val = get_rm32();
                                setflag_c( val & bit );
                                set_rm32( val & ~bit );
                            }
                        }
                        else if ( 7 == _reg ) // btc r/m, imm8  (16, 32, 64 bit test and complement
                        {
                            if ( _rexW )
                            {
                                uint64_t bit = 1ull << ( imm & 0x3f );
                                uint64_t val = get_rm64();
                                setflag_c( val & bit );
                                set_rm64( val ^ bit );
                            }
                            else if ( 0x66 == _prefix_size )
                            {
                                uint16_t bit = 1 << ( imm & 0xf );
                                uint16_t val = get_rm16();
                                setflag_c( val & bit );
                                set_rm16( val ^ bit );
                            }
                            else
                            {
                                uint32_t bit = 1 << ( imm & 0x1f );
                                uint32_t val = get_rm32();
                                setflag_c( val & bit );
                                set_rm32( val ^ bit );
                            }
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0xbc: // bsf r, r/m   16, 32, 64  bit scan forward
                    {
                        decode_rm();
                        uint64_t val = get_rm();
                        setflag_z( 0 == val );
                        regs[ _reg ].q = bitscan( val );
                        break;
                    }
                    case 0xbd: // bsf r, r/m   16, 32, 64  bit scan reverse
                    {
                        decode_rm();
                        uint64_t val = get_rm();
                        setflag_z( 0 == val );
                        regs[ _reg ].q = bitscan_reverse( val );
                        break;
                    }
                    case 0xbe: // movsx r, r/m. 16/32/64 from 8
                    {
                        decode_rm();
                        uint8_t val = get_rm8();
                        if ( _rexW )
                            regs[ _reg ].q = sign_extend( val, 7 );
                        else if ( 0x66 == _prefix_size )
                            regs[ _reg ].q = sign_extend16( val, 7 );
                        else
                            regs[ _reg ].q = sign_extend32( val, 7 );
                        break;
                    }
                    case 0xbf: // movsx r, r/m. 16/32/64 from 8
                    {
                        decode_rm();
                        uint16_t val = get_rm16();
                        if ( _rexW )
                            regs[ _reg ].q = sign_extend( val, 15 );
                        else if ( 0x66 == _prefix_size )
                            unhandled();
                        else
                            regs[ _reg ].q = sign_extend32( val, 15 );
                        break;
                    }
                    case 0xc0: // xadd r/m8, r8
                    {
                        decode_rm();
                        uint8_t val = get_reg8();
                        set_reg8( get_rm8() );
                        set_rm8( get_rm8() + val );
                        break;
                    }
                    case 0xc1: // xadd r/m, r  16/32/64
                    {
                        decode_rm();
                        if ( _rexW )
                        {
                            uint64_t val = regs[ _reg ].q;
                            regs[ _reg ].q = get_rm64();
                            set_rm64( get_rm64() + val );
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            uint16_t val = regs[ _reg ].w;
                            regs[ _reg ].q = get_rm16();
                            set_rm16( get_rm16() + val );
                        }
                        else
                        {
                            uint32_t val = regs[ _reg ].d;
                            regs[ _reg ].q = get_rm32();
                            set_rm32( get_rm32() + val );
                        }
                        break;
                    }
                    case 0xc2: // cmpps / cmppd / cmpsd / cmpss xmm1, xmm2/m128, imm8   compare floats/doubles using bits2:0 of imm8 as a comparison predicate
                    {
                        decode_rm();
                        uint8_t imm = get_rip8();
                        vec16_t & xmm1 = xregs[ _reg ];

                        if ( 0x66 == _prefix_size ) // packed double
                            for ( uint32_t e = 0; e < 2; e++ )
                                xmm1.set64( e, ( floating_comparison_true( xmm1.getd( e ), get_rmxdouble( e ), imm ) ) ? ~0ull : 0ull );
                        else if ( 0xf2 == _prefix_sse2_repeat ) // scalar double
                            xmm1.set64( 0, ( floating_comparison_true( xmm1.getd( 0 ), get_rmxdouble( 0 ), imm ) ) ? ~0ull : 0ull );
                        else if ( 0xf3 == _prefix_sse2_repeat ) // scalar single
                            xmm1.set32( 0, ( floating_comparison_true( xmm1.getf( 0 ), get_rmxfloat( 0 ), imm ) ) ? ~0 : 0 );
                        else // packed single
                            for ( uint32_t e = 0; e < 4; e++ )
                                xmm1.set32( e, ( floating_comparison_true( xmm1.getf( e ), get_rmxfloat( e ), imm ) ) ? ~0 : 0 );
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xc4:
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        uint8_t imm = get_rip8();
                        vec16_t & xmm1 = xregs[ _reg ];
                        uint16_t val = ( 3 == _mod ) ? (uint16_t) regs[ _rm ].d : get_rm16();
                        if ( 0x66 == _prefix_size ) // pinsrw xmm, r32/m16, imm8
                            xmm1.set16( imm & 7, val );
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xc5: // pextrw reg, xmm, imm8
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                            regs[ _reg ].q = xregs[ _rm ].get16( get_rip8() );
                        else
                            unhandled();
                        break;
                    }
                    case 0xc6:
                    {
                        decode_rm();
                        uint8_t imm8 = get_rip8();
                        if ( 0x66 == _prefix_size ) // shufpd xmm, xmm/m128, imm8
                        {
                            vec16_t src1 = xregs[ _reg ];
                            vec16_t target = xregs[ _reg ];

                            if ( 0 == ( imm8 & 1 ) ) // dst[ low ] = src1[ low ]
                                target.set64( 0, src1.get64( 0 ) );
                            else // dst[ low ] = src1[ high ]
                                target.set64( 0, src1.get64( 1 ) );

                            if ( 0 == ( imm8 & 2 ) ) // dst[ high ] = src2[ low ]
                                target.set64( 1, get_rmx64( 0 ) );
                            else // dst[ high] = src2[ high ]
                                target.set64( 1, get_rmx64( 1 ) );

                            xregs[ _reg ] = target;
                        }
                        else // shufps xmm, xmm/m128, imm8
                        {
                            vec16_t src1 = xregs[ _reg ];
                            vec16_t target = xregs[ _reg ];
                            target.set32( 0, src1.get32( imm8 & 3 ) );
                            target.set32( 1, src1.get32( ( imm8 >> 2 ) & 3 ) );
                            target.set32( 2, get_rmx32( ( imm8 >> 4 ) & 3 ) );
                            target.set32( 3, get_rmx32( ( imm8 >> 6 ) & 3 ) );
                            xregs[ _reg ] = target;
                        }
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xc8: case 0xc9: case 0xca: case 0xcb: case 0xcc: case 0xcd: case 0xce: case 0xcf: // bswap r32/r64
                    {
                        if ( 0 != _prefix_size || 0 != _prefix_sse2_repeat )
                            unhandled();
                        _rm = ( op1 & 7 );
                        decode_rex();
                        if ( _rexW )
                            regs[ _rm ].q = flip_endian64( regs[ _rm ].q );
                        else
                            regs[ _rm ].q = flip_endian32( regs[ _rm ].d );
                        break;
                    }
                    case 0xd2:
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 == _prefix_size ) // psrld xmm1, xmm2/m128   shift dwords in xmm1 right by amount specified
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            uint64_t count = get_rmx64( 0 );
                            for ( uint32_t e = 0; e < 4; e++ )
                            {
                                if ( count > 31 )
                                    xmm1.set32( e, 0 );
                                else
                                    xmm1.set32( e, xmm1.get32( e ) >> count );
                            }
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xd3:
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 == _prefix_size ) // psrlq xmm1, xmm2/m128   shift qwords right while shifting in 0s
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            uint64_t count = get_rmx64( 0 );
                            if ( count > 63 )
                            {
                                xmm1.set64( 0, 0 );
                                xmm1.set64( 1, 0 );
                            }
                            else
                            {
                                xmm1.set64( 0, xmm1.get64( 0 ) >> count );
                                xmm1.set64( 1, xmm1.get64( 1 ) >> count );
                            }
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0xd4: // paddq xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            xmm1.set64( 0, xmm1.get64( 0 ) + get_rmx64( 0 ) );
                            xmm1.set64( 1, xmm1.get64( 1 ) + get_rmx64( 1 ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xd5:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // pmullw xmm1, xmm2/m128  multiply signed words and store low 16 bits of results
                        {
                            vec16_t xmm1 = xregs[ _reg ];
                            vec16_t & dst = xregs[ _reg ];

                            for ( uint32_t e = 0; e < 8; e++ )
                                dst.set16( e, (int16_t) xmm1.get16( e ) * (int16_t) get_rmx16( e ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xd6: // movq r/m64, xmm
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                            set_rm64( xregs[ _reg ].get64( 0 ) );
                        else
                            unhandled();
                        break;
                    }
                    case 0xd7:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // pmovmskb reg, xmm
                        {
                            // move the high bits of each byte to create a mask 16 bits long in reg
                            uint64_t mask = 0;
                            for ( uint32_t b = 0; b < 16; b++ )
                                if ( xregs[ _rm ].get8( b ) & 0x80 )
                                    mask |= ( 1ull << b );
                            regs[ _reg ].q = mask;
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0xd8: // psubsub xmm1, xmm2/m128 subtract unsigned bytes and saturate result
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t xmm1 = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 16; i++ )
                                xregs[ _reg ].set8( i, subtract_and_saturate_unsigned( xmm1.get8( i ), get_rmx8( i ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xd9: // psubsuw xmm1, xmm2/m128 subtract unsigned words and saturate result
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t xmm1 = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 8; i++ )
                                xregs[ _reg ].set16( i, subtract_and_saturate_unsigned( xmm1.get16( i ), get_rmx16( i ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xda: // pminub xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            for ( uint32_t x = 0; x < 16; x++ )
                                xregs[ _reg ].set8( x, get_min( xregs[ _reg ].get8( x ), get_rmx8( x ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xdb:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )  // pand xmm1, xmm2/m128
                        {
                            xregs[ _reg ].set64( 0, xregs[ _reg ].get64( 0 ) & get_rmx64( 0 ) );
                            xregs[ _reg ].set64( 1, xregs[ _reg ].get64( 1 ) & get_rmx64( 1 ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xde: // pmaxub xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            for ( uint32_t e = 0; e < 16; e++ )
                                xmm1.set8( e, get_max( xmm1.get8( e ), get_rmx8( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xdf: // pandn xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            xregs[ _reg ].set64( 0, ~( xregs[ _reg ].get64( 0 ) ) & get_rmx64( 0 ) );
                            xregs[ _reg ].set64( 1, ~( xregs[ _reg ].get64( 1 ) ) & get_rmx64( 1 ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xe2:
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // psrad xmm1, xmm2/m128   shift signed dword in xmm1 right by xmm2/m128
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            uint64_t shift = get_rmx64( 0 );
                            for ( uint32_t e = 0; e < 4; e++ )
                            {
                                if ( shift >= 32 )
                                    xmm1.set32( e, val_signed( xmm1.get32( e ) ) ? 0xffffffff : 0 );
                                else
                                    xmm1.set32( e, ( (int32_t) xmm1.get32( e ) ) >> shift );
                            }
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xe4: // pmulhuw xmm1, xmm2/m128
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 != _prefix_size )
                            unhandled();
                        decode_rm();
                        vec16_t target = xregs[ _reg ];
                        vec16_t xmm1 = xregs[ _reg ];
                        for ( uint32_t x = 0; x < 8; x++ )
                        {
                            uint32_t prod = (uint32_t) xmm1.get16( x ) * (uint32_t) get_rmx16( x );
                            target.set16( x, prod >> 16 );
                        }
                        xregs[ _reg ] = target;
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xe5: // pmulhuw xmm1, xmm2/m128
                    {
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 != _prefix_size )
                            unhandled();
                        decode_rm();
                        vec16_t target = xregs[ _reg ];
                        vec16_t xmm1 = xregs[ _reg ];
                        for ( uint32_t x = 0; x < 8; x++ )
                        {
                            int32_t prod = (int32_t) (int16_t) xmm1.get16( x ) * (int32_t) (int16_t) get_rmx16( x );
                            target.set16( x, prod >> 16 );
                        }
                        xregs[ _reg ] = target;
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xe6:
                    {
                        decode_rm();
                        if ( 0xf3 == _prefix_sse2_repeat ) // cvtdq2pd xmm1, xmm2/m64  convert two packed signed dword ints from xmm2/mem to 2 double fp values in xmm1
                        {
                            vec16_t xmm1 = xregs[ _reg ];
                            uint32_t val0 = get_rmx32( 0 );
                            uint32_t val1 = get_rmx32( 1 );
                            xregs[ _reg ].setd( 0, (double) (int32_t) val0 );
                            xregs[ _reg ].setd( 1, (double) (int32_t) val1 );
                        }
                        else if ( 0x66 == _prefix_size ) // cvttpd2dq xmm1, xmm2/m128  convert two doubles to two signed dword integers
                        {
                            double val0 = get_rmxdouble( 0 );
                            double val1 = get_rmxdouble( 1 );
                            xregs[ _reg ].set32( 0, round_i32_from_double( val0, ROUNDING_MODE_TRUNCATE ) );
                            xregs[ _reg ].set32( 1, round_i32_from_double( val1, ROUNDING_MODE_TRUNCATE ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xe7:
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size ) // movntdq m128, xmm1  move packed integers from xmm to mm using non-temporal hint
                        {
                            vec16_t xmm1 = xregs[ _reg ];
                            set_rmx64( 0, xmm1.get64( 0 ) );
                            set_rmx64( 1, xmm1.get64( 1 ) );
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0xea: // pminsw xmm, xmm/m128
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            for ( uint32_t e = 0; e < 8; e++ )
                                xmm1.set16( e, get_min( (int16_t) xmm1.get16( e ), (int16_t) get_rmx16( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xeb: // por xmm, xmm/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            if ( 3 == _mod )
                            {
                                xregs[ _reg ].set64( 0, xregs[ _reg ].get64( 0 ) | xregs[ _rm ].get64( 0 ) );
                                xregs[ _reg ].set64( 1, xregs[ _reg ].get64( 1 ) | xregs[ _rm ].get64( 1 ) );
                            }
                            else
                            {
                                xregs[ _reg ].set64( 0, xregs[ _reg ].get64( 0 ) | get_rmx64( 0 ) );
                                xregs[ _reg ].set64( 1, xregs[ _reg ].get64( 1 ) | get_rmx64( 1 ) );
                            }
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xee: // pmaxsw xmm, xmm/m128
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            for ( uint32_t e = 0; e < 8; e++ )
                                xmm1.set16( e, get_max( (int16_t) xmm1.get16( e ), (int16_t) get_rmx16( e ) ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xef: // pxor xmm, xmm/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            xmm1.set64( 0, xmm1.get64( 0 ) ^ get_rmx64( 0 ) );
                            xmm1.set64( 1, xmm1.get64( 1 ) ^ get_rmx64( 1 ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xf2:
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 == _prefix_size ) // pslld xmm1, xmm2/m128   shift dwords left while shifting in 0s
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            uint64_t count = get_rmx64( 0 );
                            for ( uint32_t e = 0; e < 4; e++ )
                            {
                                if ( count > 31 )
                                    xmm1.set32( e, 0 );
                                else
                                    xmm1.set32( e, xmm1.get32( e ) << count );
                            }
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0xf3:
                    {
                        decode_rm();
                        if ( 0 != _prefix_sse2_repeat )
                            unhandled();
                        if ( 0x66 == _prefix_size ) // psllq xmm1, xmm2/m128   shift qwords left while shifting in 0s
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            uint64_t count = get_rmx64( 0 );
                            if ( count > 63 )
                            {
                                xmm1.set64( 0, 0 );
                                xmm1.set64( 1, 0 );
                            }
                            else
                            {
                                xmm1.set64( 0, xmm1.get64( 0 ) << count );
                                xmm1.set64( 1, xmm1.get64( 1 ) << count );
                            }
                        }
                        else
                            unhandled();
                        break;
                    }
                    case 0xf4: // pmuludq xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t xmm1 = xregs[ _reg ];
                            xregs[ _reg ].set64( 0, (uint64_t) xmm1.get32( 0 ) * (uint64_t) get_rmx32( 0 ) );
                            xregs[ _reg ].set64( 1, (uint64_t) xmm1.get32( 2 ) * (uint64_t) get_rmx32( 2 ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xf6: // psadbw xmm1, xmm2/m128  compute absolute differences on bytes and store results in low words of each part of result
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t xmm1 = xregs[ _reg ];
                            uint16_t sumlow = 0;
                            uint16_t sumhigh = 0;

                            for ( uint32_t i = 0; i < 8; i++ )
                            {
                                sumlow += (uint16_t) absolute_difference( xmm1.get8( i ), get_rmx8( i ) );
                                sumhigh += (uint16_t) absolute_difference( xmm1.get8( i + 8 ), get_rmx8( i + 8 ) );
                            }

                            xregs[ _reg ].zero();
                            xregs[ _reg ].set16( 0, sumlow );
                            xregs[ _reg ].set16( 4, sumhigh );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xf8: // psubb xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t xmm1 = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 16; i++ )
                                xregs[ _reg ].set8( i, xmm1.get8( i ) - get_rmx8( i ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xf9: // psubw xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            for ( uint32_t i = 0; i < 8; i++ )
                                xregs[ _reg ].set16( i, xregs[ _reg ].get16( i ) - get_rmx16( i ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xfa: // psubd xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t xmm1 = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 4; i++ )
                                xregs[ _reg ].set32( i, xmm1.get32( i ) - get_rmx32( i ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xfb: // psubq xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 2; i++ )
                                xmm1.set64( i, xmm1.get64( i ) - get_rmx64( i ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xfc: // paddb xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 16; i++ )
                                xmm1.set8( i, xmm1.get8( i ) + get_rmx8( i ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xfd: // paddw xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 8; i++ )
                                xmm1.set16( i, xmm1.get16( i ) + get_rmx16( i ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    case 0xfe: // paddd xmm1, xmm2/m128
                    {
                        decode_rm();
                        if ( 0x66 == _prefix_size )
                        {
                            vec16_t & xmm1 = xregs[ _reg ];
                            for ( uint32_t i = 0; i < 4; i++ )
                                xmm1.set32( i, xmm1.get32( i ) + get_rmx32( i ) );
                        }
                        else
                            unhandled();
                        trace_xreg( _reg );
                        break;
                    }
                    default:
                        unhandled();
                }
                break;
            }
            case 0x2e: // prefix for CS or branch not taken prediction
            case 0x3e: // prefix for DS or branch prediction
            {
                break; // ignored
            }
            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: // REX prefix
            case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f: // REX prefix
            {
                _prefix_rex = op;
                goto _prefix_is_set;
            }
            case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: // push
            {
                _rm = op & 7;
                decode_rex();
                push( regs[ _rm ].q );
                break;
            }
            case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: // pop
            {
                _rm = op & 7;
                decode_rex();
                regs[ _rm ].q = pop();
                break;
            }
            case 0x63: // movsxd reg, r/m. also movsxq
            {
                decode_rm();
                if ( _rexW )
                    regs[ _reg ].q = sign_extend( get_rm32(), 31 );
                else if ( 0x66 == _prefix_size )
                    regs[ _reg ].q = get_rm16();
                else
                    regs[ _reg ].q = get_rm32();
                break;
            }
            case 0x64: case 0x65: // prefix for fs: and gs:
            {
                _prefix_segment = op;
                goto _prefix_is_set;
            }
            case 0x66: case 0x67:
            {
                _prefix_size = op;
                goto _prefix_is_set;
            }
            case 0x68: // push imm16 / imm32
            {
                uint64_t val;
                if ( 0x66 == _prefix_size )
                    val = sign_extend( get_rip16(), 15 );
                else
                    val = sign_extend( get_rip32(), 31 );
                push( val );
                break;
            }
            case 0x69: // imul reg, r/m, imm. 16, 32, and 64-bit values (imm 16 or 32)
            {
                decode_rm();
                if ( _rexW )
                {
                    uint64_t imm64 = sign_extend( get_rip32(), 31 );
                    int64_t result128H = 0;
                    uint64_t result128L = CMultiply128::mul_s64_s64( get_rm64(), imm64, &result128H );
                    setflag_o( val_signed( result128H) != val_signed( result128L ) );
                    setflag_c( flag_o() );
                    regs[ _reg ].q = result128L;
                }
                else if ( 0x66 == _prefix_size )
                {
                    uint16_t imm16 = get_rip16();
                    uint32_t a = (uint32_t) sign_extend( get_rm16(), 15 );
                    uint32_t b = (uint32_t) imm16;
                    uint32_t result32 = a * b;
                    uint16_t result16 = result32 & 0xffff;
                    setflag_o( val_signed( result32 ) != val_signed( result16 ) );
                    setflag_c( flag_o() );
                    regs[ _reg ].q = result16;
                }
                else
                {
                    uint32_t imm32 = get_rip32();
                    uint64_t a = sign_extend( get_rm32(), 31 );
                    uint64_t b = imm32;
                    uint64_t result64 = a * b;
                    uint32_t result32 = result64 & 0xffffffff;
                    setflag_o( val_signed( result64 ) != val_signed( result32 ) );
                    setflag_c( flag_o() );
                    regs[ _reg ].q = result32;
                }
                break;
            }
            case 0x6a: // push imm8. sign-extended to 64 bits
            {
                push( (int64_t) (int8_t) get_rip8() );
                break;
            }
            case 0x6b: // imul reg, r/m, se(imm8). 16, 32, and 64-bit values
            {
                decode_rm();
                uint8_t imm8 = get_rip8();
                if ( _rexW )
                {
                    int64_t result128H = 0;
                    int64_t result128L = CMultiply128::mul_s64_s64( get_rm64(), sign_extend( imm8, 7 ), &result128H );
                    setflag_o( val_signed( result128H) != val_signed( result128L ) );
                    setflag_c( flag_o() );
                    regs[ _reg ].q = result128L;
                }
                else if ( 0x66 == _prefix_size )
                {
                    uint32_t a = (uint32_t) sign_extend( get_rm16(), 15 );
                    uint32_t b = (uint32_t) sign_extend( imm8,7 );
                    uint32_t result32 = a * b;
                    uint16_t result16 = result32 & 0xffff;
                    setflag_o( val_signed( result32 ) != val_signed( result16 ) );
                    setflag_c( flag_o() );
                    regs[ _reg ].q = result16;
                }
                else
                {
                    uint64_t a = sign_extend( get_rm32(), 31 );
                    uint64_t b = sign_extend( imm8, 7 );
                    uint64_t result64 = a * b;
                    uint32_t result32 = result64 & 0xffffffff;
                    setflag_o( val_signed( result64 ) != val_signed( result32 ) );
                    setflag_c( flag_o() );
                    regs[ _reg ].q = result32;
                }
                break;
            }
            case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: // jcc
            case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
            {
                int16_t offset = (int16_t) (int8_t) get_rip8();
                if ( check_condition( op & 0xf ) )
                    rip += offset;
                break;
            }
            case 0x80: // math r/m8, i8
            {
                decode_rm();
                uint8_t value = get_rip8(); // must read this prior to the get_rm_ptr8() call in case it's rip-relative
                do_math( _reg, get_rm_ptr8(), value );
                break;
            }
            case 0x81: // math r/m, imm32
            {
                decode_rm();
                uint8_t math = _reg;
                if ( _rexW )
                {
                    uint64_t r = get_rip32();
                    uint64_t val = get_rm64();
                    do_math( math, &val, (uint64_t) sign_extend( r, 31 ) );
                    if ( 7 != math )
                        set_rm64( val );
                }
                else if ( 0x66 == _prefix_size )
                {
                    uint16_t r = get_rip16();
                    uint16_t val = get_rm16();
                    do_math( math, &val, r );
                    if ( 7 != math )
                        set_rm16( val );
                }
                else
                {
                    uint32_t r = get_rip32();
                    uint32_t val = get_rm32();
                    do_math( math, &val, r );
                    if ( 7 != math )
                        set_rm32z( val );
                }
                break;
            }
            case 0x83: // math sign_extend( imm8 )
            {
                decode_rm();
                uint8_t imm8 = get_rip8();
                uint8_t math = _reg;

                if ( _rexW ) // if wide/64-bit
                {
                    uint64_t val = get_rm64();
                    do_math( math, &val, (uint64_t) (int64_t) (int8_t) imm8 );
                    if ( 7 != math )
                        set_rm64( val );
                }
                else if ( 0x66 == _prefix_size )
                {
                    uint16_t val = get_rm16();
                    do_math( math, &val, (uint16_t) (int16_t) (int8_t) imm8 );
                    if ( 7 != math )
                        set_rm16( val );
                }
                else
                {
                    uint32_t val = get_rm32();
                    do_math( math, &val, (uint32_t) (int32_t) (int8_t) imm8 );
                    if ( 7 != math )
                        set_rm32z( val );
                }
                break;
            }
            case 0x84: // test r/m8, r8
            {
                decode_rm();
                set_PSZ( (uint8_t) ( get_rm8() & get_reg8() ) );
                reset_carry_overflow();
                break;
            }
            case 0x85: // test r/m, reg
            {
                decode_rm();
                if ( _rexW )
                    set_PSZ( get_rm64() & regs[ _reg ].q );
                else if ( 0x66 == _prefix_size )
                    set_PSZ( (uint16_t) ( get_rm16() & regs[ _reg ].w ) );
                else
                    set_PSZ( get_rm32() & regs[ _reg ].d );
                reset_carry_overflow();
                break;
            }
            case 0x86: // xchg r/m8, r8
            {
                decode_rm();
                uint8_t tmp = get_reg8();
                set_reg8( get_rm8() );
                set_rm8( tmp );
                break;
            }
            case 0x87: // xchg r/m, reg  16, 32, 64
            {
                decode_rm();
                if ( _rexW )
                {
                    uint64_t tmp = regs[ _reg ].q;
                    regs[ _reg ].q = get_rm64();
                    set_rm64( tmp );
                }
                else if ( 0x66 == _prefix_size )
                {
                    uint16_t tmp = regs[ _reg ].w;
                    regs[ _reg ].q = get_rm16();
                    set_rm16( tmp );
                }
                else
                {
                    uint32_t tmp = regs[ _reg ].d;
                    regs[ _reg ].q = get_rm32();
                    set_rm32( tmp );
                }
                break;
            }
            case 0x88: // mov r/m8, r8
            {
                decode_rm();
                set_rm8( get_reg8() );
                break;
            }
            case 0x89: // mov r/m, reg
            {
                decode_rm();
                if ( _rexW )
                    set_rm64( regs[ _reg ].q );
                else if ( 0x66 == _prefix_size )
                    set_rm16( regs[ _reg ].w );
                else
                    set_rm32z( regs[ _reg ].d );
                break;
            }
            case 0x8a: // mov r8, r/m8
            {
                decode_rm();
                set_reg8( get_rm8() );
                break;
            }
            case 0x8b: // mov reg, r/m
            {
                decode_rm();
                if ( _rexW )
                    regs[ _reg ].q = get_rm64();
                else if ( 0x66 == _prefix_size )
                    regs[ _reg ].q = get_rm16();
                else
                    regs[ _reg ].q = get_rm32();
                break;
            }
            case 0x8d: // lea
            {
                decode_rm();
                if ( _rexW )
                    regs[ _reg ].q = effective_address();
                else if ( 0x66 == _prefix_size )
                    regs[ _reg ].q = 0xffff & effective_address();
                else
                    regs[ _reg ].q = 0xffffffff & effective_address();
                break;
            }
            case 0x90: { break; } // nop exch ax, ax
            case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: // xchg ax, r  (widths 16, 32, 64)
            {
                _rm = op & 0xf;
                if ( 0 != _prefix_rex )
                {
                    decode_rex();
                    if ( _rexW )
                        do_swap( regs[ rax ].q, regs[ _rm ].q ); // can use swap here because the full q is updated
                    else
                    {
                        uint32_t tmp = regs[ rax ].d;
                        regs[ rax ].q = regs[ _rm ].d;
                        regs[ _rm ].q = tmp;
                    }
                }
                else if ( 0x66 == _prefix_size )
                {
                    uint16_t tmp = regs[ rax ].w;
                    regs[ rax ].q = regs[ _rm ].w;
                    regs[ _rm ].q = tmp;
                }
                else
                {
                    uint32_t tmp = regs[ rax ].d;
                    regs[ rax ].q = regs[ _rm ].d;
                    regs[ _rm ].q = tmp;
                }
                break;
            }
            case 0x98: // cbw/cwde/cdqe
            {
                decode_rex();
                if ( _rexW )
                    regs[ rax ].q = sign_extend( regs[ rax ].q, 31 );
                else if ( 0x66 == _prefix_size )
                    regs[ rax ].q = sign_extend( regs[ rax ].q, 7 );
                else
                    regs[ rax ].q = sign_extend( regs[ rax ].q, 15 );
                break;
            }
            case 0x99: // cwd/cdq/cqo
            {
                decode_rex();
                if ( _rexW )
                    regs[ rdx ].q = ( val_signed( regs[ rax ].q ) ) ? ~0 : 0;
                else if ( 0x66 == _prefix_size )
                    regs[ rdx ].q = ( val_signed( regs[ rax ].w ) ) ? 0xffff : 0;
                else
                    regs[ rdx ].q = ( val_signed( regs[ rax ].d ) ) ? 0xffffffff : 0;
                break;
            }
            case 0xa4: // movb m, m  RSI to RDI
            {
                decode_rex();
                if ( 0 != _prefix_sse2_repeat ) // f3 is legal. alllow f2
                {
                    while ( 0 != regs[ rcx ].q )
                    {
                        op_movs( 1 );
                        regs[ rcx ].q--;
                    }
                }
                else
                    op_movs( 1 );
                break;
            }
            case 0xa5: // movs m, m  RSI to RDI 16/32/64
            {
                decode_rex();
                uint8_t width = ( 0 != _prefix_rex ) ? 8: ( 0x66 == _prefix_size ) ? 2 : 4;

                if ( 0 != _prefix_sse2_repeat ) // f3 is legal. alllow f2
                {
                    while ( 0 != regs[ rcx ].q )
                    {
                        op_movs( width );
                        regs[ rcx ].q--;
                    }
                }
                else
                    op_movs( width );
                break;
            }
            case 0xa8: // test al, imm8
            {
                op_and( regs[ rax ].b, get_rip8() );
                break;
            }
            case 0xa9: // test ax, imm16    text eax, imm32   test rax, se(imm32)
            {
                decode_rex();
                if ( 0 != _prefix_rex )
                    op_and( regs[ rax ].q, (uint64_t) sign_extend( get_rip32(), 31 ) );
                else if ( 0x66 == _prefix_size )
                    op_and( regs[ rax ].w, get_rip16() );
                else
                    op_and( regs[ rax ].d, get_rip32() );
                break;
            }
            case 0xaa: // stoa
            {
                if ( 0 != _prefix_sse2_repeat ) // f3 is legal. alllow f2
                {
                    while ( 0 != regs[ rcx ].q )
                    {
                        op_sto( 1 );
                        regs[ rcx ].q--;
                    }
                }
                else
                    op_sto( 1 );
                break;
            }
            case 0xab: // stos
            {
                uint8_t width = ( 0 != _prefix_rex ) ? 8: ( 0x66 == _prefix_size ) ? 2 : 4;

                if ( 0 != _prefix_sse2_repeat ) // f3 is legal. alllow f2
                {
                    while ( 0 != regs[ rcx ].q )
                    {
                        op_sto( width );
                        regs[ rcx ].q--;
                    }
                }
                else
                    op_sto( width );
                break;
            }
            case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7: // mov r8, imm8
            {
                _rm = ( op & 7 );
                _mod = 3;
                decode_rex();
                set_rm8( get_rip8() ); // no sign or zero extension; just the one byte is updated
                break;
            }
            case 0xb8: case 0xb9 : case 0xba : case 0xbb : case 0xbc : case 0xbd : case 0xbe : case 0xbf: // mov reg, 64-bit immediate
            {
                _rm = ( op & 7 );
                decode_rex();
                if ( _rexW )
                    regs[ _rm ].q = get_rip64();
                else if ( 0x66 == _prefix_size )
                    regs[ _rm ].q = get_rip16();
                else
                    regs[ _rm ].q = get_rip32();
                break;
            }
            case 0xc0: // shift r8/m8, imm8
            {
                decode_rm();
                uint8_t shift = get_rip8();
                if ( 0 == shift )
                    break;
                shift &= 7;
                op_shift( get_rm_ptr8(), _reg, shift );
                break;
            }
            case 0xc1: // sal/shr/sar r/m, imm8
            {
                decode_rm();
                uint8_t shift = get_rip8();
                if ( 0 == shift )
                    break;

                if ( _rexW )
                {
                    shift &= 0x3f;
                    uint64_t val = get_rm64();
                    op_shift( &val, _reg, shift );
                    set_rm64( val );
                }
                else if ( 0x66 == _prefix_size )
                {
                    shift &= 0xf;
                    uint16_t val = get_rm16();
                    op_shift( &val, _reg, shift );
                    set_rm16( val );
                }
                else
                {
                    shift &= 0x1f;
                    uint32_t val = get_rm32();
                    op_shift( &val, _reg, shift );
                    set_rm32z( val );
                }
                break;
            }
            case 0xc3: // ret
            {
                rip = pop();
                break;
            }
            case 0xc6:
            {
                decode_rm();
                if ( 0 == _reg ) // mov r/m8, imm8
                    set_rm8( get_rip8() );
                else
                    unhandled();
                break;
            }
            case 0xc7: // mov r/m, imm 32-bit zero-extended immediate value
            {
                decode_rm();
                if ( _rexW )
                    set_rm64( sign_extend( get_rip32(), 31 ) );
                else if ( 0x66 == _prefix_size )
                    set_rm16( get_rip16() );
                else
                    set_rm32z( get_rip32() );
                break;
            }
            case 0xc9: // leave
            {
                regs[ rsp ].q = regs[ rbp ].q;
                regs[ rbp ].q = pop();
                break;
            }
            case 0xd0: // shift r/m8, 1
            {
                decode_rm();
                op_shift( get_rm_ptr8(), _reg, 1 );
                break;
            }
            case 0xd1: // shift r/m (, 1) 16/32/64
            {
                decode_rm();
                if ( _rexW )
                {
                    uint64_t val = get_rm64();
                    op_shift( &val, _reg, 1 );
                    set_rm64( val );
                }
                else if ( 0x66 == _prefix_size )
                {
                    uint16_t val = get_rm16();
                    op_shift( &val, _reg, 1 );
                    set_rm16( val );
                }
                else
                {
                    uint32_t val = get_rm32();
                    op_shift( &val, _reg, 1 );
                    set_rm32z( val );
                }
                break;
            }
            case 0xd3: // shift r/m, cl   16/32/64
            {
                decode_rm();
                uint8_t shift = regs[ rcx ].b;
                if ( 0 == shift )
                    break;
                if ( _rexW )
                {
                    shift &= 0x3f;
                    uint64_t val = get_rm64();
                    op_shift( &val, _reg, shift );
                    set_rm64( val );
                }
                else if ( 0x66 == _prefix_size )
                {
                    shift &= 0xf;
                    uint16_t val = get_rm16();
                    op_shift( &val, _reg, shift );
                    set_rm16( val );
                }
                else
                {
                    shift &= 0x1f;
                    uint32_t val = get_rm32();
                    op_shift( &val, _reg, shift );
                    set_rm32z( val );
                }
                break;
            }
            case 0xd8:
            {
                uint8_t op1 = get_rip8();
                uint8_t offset = op1 & 7;
                if ( op1 >= 0xc0 && op1 <= 0xc7 ) // fadd st(0), st(i)
                    poke_fp( 0, do_fadd( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                else if ( op1 >= 0xc8 && op1 <= 0xcf ) // fmul st(0), st(i)
                    poke_fp( 0, do_fmul( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                else if ( op1 >= 0xe0 && op1 <= 0xe7 ) // fsub st(0), st(i)
                    poke_fp( 0, do_fsub( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                else if ( op1 >= 0xe8 && op1 <= 0xef ) // fsubr st(0), st(i)
                    poke_fp( 0, do_fsub( peek_fp( offset ).getld(), peek_fp( 0 ).getld() ) );
                else if ( op1 >= 0xf0 && op1 <= 0xf7 ) // fdiv st(0), st(i)
                    poke_fp( 0, do_fdiv( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                else if ( op1 >= 0xf8 && op1 <= 0xff ) // fdivr st(0), st(i)
                    poke_fp( 0, do_fdiv( peek_fp( offset ).getld(), peek_fp( 0 ).getld() ) );
                else
                {
                    rip--;
                    decode_rm();
                    if ( 0 == _reg ) // fadd m32fp
                        poke_fp( 0, do_fadd( peek_fp( 0 ).getld(), (long double) get_rmfloat() ) );
                    else if ( 1 == _reg ) // fmul m32fp
                        poke_fp( 0, do_fmul( peek_fp( 0 ).getld(), (long double) get_rmfloat() ) );
                    else if ( 4 == _reg ) // fsub m32fp
                        poke_fp( 0, do_fsub( peek_fp( 0 ).getld(), (long double) get_rmfloat() ) );
                    else if ( 5 == _reg ) // fsubr m32fp
                        poke_fp( 0, do_fsub( (long double) get_rmfloat(), peek_fp( 0 ).getld() ) );
                    else if ( 6 == _reg ) // fdiv m32fp
                        poke_fp( 0, do_fdiv( peek_fp( 0 ).getld(), (long double) get_rmfloat() ) );
                    else if ( 7 == _reg ) // fdivr m32fp
                        poke_fp( 0, do_fdiv( (long double) get_rmfloat(), peek_fp( 0 ).getld() ) );
                    else
                        unhandled();
                }
                break;
            }
            case 0xd9:
            {
                uint8_t op1 = get_rip8();
                uint8_t offset = ( op1 & 7 );
                if ( op1 >= 0xc0 && op1 <= 0xc7 )
                    push_fp( peek_fp( offset ) );
                else if ( op1 >= 0xc8 && op1 <= 0xcf )
                {
                    float80_t tmp = peek_fp( 0 );
                    poke_fp( 0, peek_fp( offset ) );
                    poke_fp( offset, tmp );
                }
                else if ( 0xd0 == op1 ) // fnop
                    {}
                else if ( 0xe0 == op1 ) // fchs
                    poke_fp( 0, do_fsub( 0.0L, peek_fp( 0 ).getld() ) );
                else if ( 0xe1 == op1 ) // fabs
                    poke_fp( 0, fabsl( peek_fp( 0 ).getld() ) );
                else if ( 0xe4 == op1 ) // ftst
                    set_x87_status_compare( compare_floating( peek_fp( 0 ).getld(), 0.0L ) );
                else if ( 0xe5 == op1 ) // fxam
                {
                    long double d = peek_fp( 0 ).getld();
                    if ( my_isnan( d ) )
                        set_x87_status_c320( false, false, true );
                    else if ( isinf( d ) )
                        set_x87_status_c320( false, true, true );
                    else if ( 0.0 == d )
                        set_x87_status_c320( true, false, false );
                    else // normal number
                        set_x87_status_c320( false, true, false );

                    set_x87_status_c1( d < 0.0 );
                }
                else if ( op1 >= 0xe8 && op1 <= 0xee ) // load one of various constants
                    push_fp( float_d9_e8_constants[ offset ] );
                else if ( 0xf0 == op1 ) // f2xm1 -- compute 2 to the x minus 1
                    poke_fp( 0, do_fsub( powl( 2.0L, peek_fp( 0 ).getld() ), 1.0L ) );
                else if ( 0xf1 == op1 ) // fyl2x
                {
                    long double top = pop_fp().getld();
                    poke_fp( 0, do_fmul( peek_fp( 0 ).getld(), log2l( top ) ) );
                }
                else if ( 0xf2 == op1 ) // fptan
                {
                    set_x87_status_c2( false );
                    poke_fp( 0, tanl( peek_fp( 0 ).getld() ) );
                    push_fp( 1.0L );
                }
                else if ( 0xf3 == op1 ) // fpatan. replace st(1) with arctan(st(1)/st(0)). must use atan2() (not atan()) to get the correct quadrant
                {
                    poke_fp( 1, atan2l( peek_fp( 1 ).getld(), peek_fp( 0 ).getld() ) );
                    pop_fp();
                }
                else if ( 0xf4 == op1 ) // fxtract extract exponent and significand
                {
                    long double top = peek_fp( 0 ).getld();
                    int exponent;
                    long double significand = frexpl( top, &exponent );
                    poke_fp( 0, (long double) exponent );
                    push_fp( significand );
                }
                else if ( 0xf5 == op1 ) // fprem1 ieee partial remainder; roundl() not truncl() for Q
                {
                    long double d0 = peek_fp( 0 ).getld();
                    long double d1 = peek_fp( 1 ).getld();
                    long double Q = roundl( do_fdiv( d0, d1 ) ); // fprem1 uses round() while fprem uses trunc()
                    long double remainder = do_fsub( d0, do_fmul( Q, d1 ) );
                    // to do set c0, c3, c1 to be least significant bits of Q: q2, q1, q0
                    set_x87_status_c2( false );
                    poke_fp( 0, remainder );
                }
                else if ( 0xf6 == op1 ) // fdecstp decrement stack top
                {
                    if ( 0 == fp_sp )
                        fp_sp = _countof( fregs ) - 1;
                    else
                        fp_sp--;
                }
                else if ( 0xf7 == op1 ) // fincstp increment stack top
                    fp_sp = ( fp_sp + 1 ) % _countof( fregs );
                else if ( 0xf8 == op1 ) // fprem
                {
                    long double d0 = peek_fp( 0 ).getld();
                    long double d1 = peek_fp( 1 ).getld();
                    long double Q = truncl( do_fdiv( d0, d1 ) );
                    long double remainder = do_fsub( d0, do_fmul( Q, d1 ) );
                    tracer.Trace( "remainder %lf = d0 (%.20lf) - ( Q (%.20lf) * d1 (%.20lf) )\n", (double) remainder, (double) d0, (double) Q, (double) d1 );
                    // to do: set c0, c3, c1 to be least significant bits of Q: q2, q1, q0
                    set_x87_status_c2( false ); // we always complete the remainder calculation
                    poke_fp( 0, remainder );
                }
                else if ( 0xf9 == op1 ) // fyl2xp1    replace st(1) with st(1) * log2( st(0) + 1.0 ) and pop
                {
                    long double top = pop_fp().getld();
                    poke_fp( 0, do_fmul( peek_fp( 0 ).getld(), log2l( do_fadd( top, 1.0L ) ) ) );
                }
                else if ( 0xfa == op1 ) // fsqrt
                    poke_fp( 0, sqrtl( peek_fp( 0 ).getld() ) );
                else if ( 0xfb == op1 ) // fsincos
                {
                    long double top = peek_fp( 0 ).getld();
                    poke_fp( 0, sinl( top ) );
                    push_fp( cosl( top ) );
                }
                else if ( 0xfc == op1 ) // frndint. round st(0) to an integer
                    poke_fp( 0, round_ldouble_from_ldouble( peek_fp( 0 ).getld(), get_x87_rounding_mode() ) );
                else if ( 0xfd == op1 ) // fscale
                    poke_fp( 0, ldexpl( peek_fp( 0 ).getld(), (int) truncl( peek_fp( 1 ).getld() ) ) );
                else if ( 0xfe == op1 ) // fsin
                    poke_fp( 0, sinl( peek_fp( 0 ).getld() ) );
                else if ( 0xff == op1 ) // fcos
                    poke_fp( 0, cosl( peek_fp( 0 ).getld() ) );
                else
                {
                    rip--;
                    decode_rm();
                    if ( 0 == _reg ) // fld m32fp. pushes m32fp onto the fpu register stack
                        push_fp( (long double) getfloat( effective_address() ) );
                    else if ( 2 == _reg ) // fst m32fp copy st to m32fp
                        set_rmfloat( peek_fp( 0 ).getf() );
                    else if ( 3 == _reg ) // fstp m32fp copy st to m32fp and pop register stack
                        set_rmfloat( pop_fp().getf() );
                    else if ( 4 == _reg ) // fnldenv m14/28byte  loads fpu status info from memory
                    {
                        uint16_t * pstatus = (uint16_t *) getmem( effective_address() );
                        x87_fpu_status_word = pstatus[ 0 ];
                        x87_fpu_control_word = pstatus[ 1 ];
                    }
                    else if ( 5 == _reg ) // fldcw  load fpu control word from m2byte
                        x87_fpu_control_word = get_rm16();
                    else if ( 6 == _reg ) // fnstenv m14/28byte  stores fpu status info in memory
                    {
                        uint16_t * pstatus = (uint16_t *) getmem( effective_address() );
                        pstatus[ 0 ] = x87_fpu_status_word;
                        pstatus[ 1 ] = x87_fpu_control_word;
                    }
                    else if ( 7 == _reg ) // fnstcw m2byte. store fpu control word to m2byte
                        set_rm16( x87_fpu_control_word );
                    else
                        unhandled();
                }
                break;
            }
            case 0xda:
            {
                uint8_t op1 = get_rip8();
                uint8_t offset = op1 & 7;
                if ( op1 >= 0xc0 && op1 <= 0xc7 ) // move if below
                {
                    if ( check_condition( ccB ) )
                        poke_fp( 0, peek_fp( offset ) );
                }
                else if ( op1 >= 0xc8 && op1 <= 0xcf ) // move if equal
                {
                    if ( check_condition( ccE ) )
                        poke_fp( 0, peek_fp( offset ) );
                }
                else if ( op1 >= 0xd0 && op1 <= 0xd7 ) // move if below or equal
                {
                    if ( check_condition( ccBE ) )
                        poke_fp( 0, peek_fp( offset ) );
                }
                else if ( op1 >= 0xd8 && op1 <= 0xdf ) // move if unordered
                {
                    if ( check_condition( ccU ) )
                        poke_fp( 0, peek_fp( offset ) );
                }
                else
                {
                    rip--;
                    decode_rm();
                    if ( 0 == _reg ) // fiadd m32int  add m32int to st(0) and store in st(0)
                        poke_fp( 0, do_fadd( (long double) (int32_t) get_rm32(), peek_fp( 0 ).getld() ) );
                    else if ( 1 == _reg ) // fimul m32int  multiply st(0) by m32int and store in st(0)
                        poke_fp( 0, do_fmul( (long double) (int32_t) get_rm32(), peek_fp( 0 ).getld() ) );
                    else
                        unhandled();
                }
                break;
            }
            case 0xdb:
            {
                uint8_t op1 = get_rip8();
                uint8_t offset = op1 & 7;
                if ( op1 >= 0xc0 && op1 <= 0xc7 ) // move if not below
                {
                    if ( check_condition( ccNB ) )
                        poke_fp( 0, peek_fp( offset ) );
                }
                else if ( op1 >= 0xc8 && op1 <= 0xcf ) // move if not equal
                {
                    if ( check_condition( ccNE ) )
                        poke_fp( 0, peek_fp( offset ) );
                }
                else if ( op1 >= 0xd0 && op1 <= 0xd7 ) // move if not below or equal
                {
                    if ( check_condition( ccNBE ) )
                        poke_fp( 0, peek_fp( offset ) );
                }
                else if ( op1 >= 0xd8 && op1 <= 0xdf ) // move if not unordered
                {
                    if ( check_condition( ccNU ) )
                        poke_fp( 0, peek_fp( offset ) );
                }
                else if ( op1 >= 0xf0 && op1 <= 0xf7 ) // fcomi st, st(i)  compare st(0) with st(i) and set status flags
                    set_eflags_from_fcc( compare_floating( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                else if ( op1 >= 0xe8 && op1 <= 0xef ) // fucomi st, st(i)
                    set_eflags_from_fcc( compare_floating( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                else
                {
                    rip--;
                    decode_rm();
                    if ( 0 == _reg ) // fild m32int
                        push_fp( (long double) (int32_t) get_rm32() );
                    else if ( 3 == _reg ) // fistp m32int
                        set_rm32( round_i32_from_double( pop_fp().getd(), get_x87_rounding_mode() ) );
                    else if ( 5 == _reg ) // fld m80fp push m80fp onto the fpu register stack
                    {
                        float80_t f80;
                        memcpy( &f80, getmem( effective_address() ), 10 );
                        //tracer.TraceBinaryData( (uint8_t *) &f80, 10, 2 );
                        push_fp( f80 );
                    }
                    else if ( 7 == _reg ) // fstp m80fp   copy top of stack to m80fp and pop it
                    {
                        float80_t f80 = pop_fp();
                        //tracer.TraceBinaryData( (uint8_t *) &f80, 10, 2 );
                        memcpy( getmem( effective_address() ), &f80, 10 );
                    }
                    else
                        unhandled();
                }
                break;
            }
            case 0xdc:
            {
                uint8_t op1 = get_rip8();
                uint8_t offset = op1 & 7;
                if ( op1 >= 0xe0 && op1 <= 0xe7 ) // fsubr st(i), st(0)
                    poke_fp( offset, do_fsub( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                else if ( op1 >= 0xe8 && op1 <= 0xef ) // fsub st(i), st(0)
                    poke_fp( offset, do_fsub( peek_fp( offset ).getld(), peek_fp( 0 ).getld() ) );
                else if ( op1 >= 0xc0 && op1 <= 0xc7 ) // fadd st(i), st(0)
                    poke_fp( offset, do_fadd( peek_fp( offset ).getld(), peek_fp( 0 ).getld() ) );
                else if ( op1 >= 0xc8 && op1 <= 0xcf ) // fmul st(i), st(0)
                    poke_fp( offset, do_fmul( peek_fp( offset ).getld(), peek_fp( 0 ).getld() ) );
                else if ( op1 >= 0xf0 && op1 <= 0xf7 ) // fdivr st(i), st(0)
                    poke_fp( offset, do_fdiv( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                else if ( op1 >= 0xf8 && op1 <= 0xff ) // fdiv st(i), st(0)
                    poke_fp( offset, do_fdiv( peek_fp( offset ).getld(), peek_fp( 0 ).getld() ) );
                else
                {
                    rip--;
                    decode_rm();
                    if ( 0 == _reg ) // fadd m64fp
                        poke_fp( 0, do_fadd( peek_fp( 0 ).getld(), (long double) get_rmdouble() ) );
                    else if ( 1 == _reg ) // fmul m64fp
                        poke_fp( 0, do_fmul( peek_fp( 0 ).getld(), (long double) get_rmdouble() ) );
                    else if ( 2 == _reg ) // fcom m64fp
                        set_x87_status_compare( compare_floating( peek_fp( 0 ).getld(), (long double) get_rmdouble() ) );
                    else if ( 3 == _reg ) // fcomp m64fp
                        set_x87_status_compare( compare_floating( pop_fp().getld(), (long double) get_rmdouble() ) );
                    else if ( 4 == _reg ) // fsub m64fp
                        poke_fp( 0, do_fsub( peek_fp( 0 ).getld(), (long double) get_rmdouble() ) );
                    else if ( 5 == _reg ) // fsubr m64fp
                        poke_fp( 0, do_fsub( (long double) get_rmdouble(), peek_fp( 0 ).getld() ) );
                    else if ( 6 == _reg ) // fdiv m64fp
                        poke_fp( 0, do_fdiv( peek_fp( 0 ).getld(), (long double) get_rmdouble() ) );
                    else if ( 7 == _reg ) // fdivr m64fp
                        poke_fp( 0, do_fdiv( (long double) get_rmdouble(), peek_fp( 0 ).getld() ) );
                    else
                        unhandled();
                }
                break;
            }
            case 0xdd:
            {
                uint8_t op1 = get_rip8();
                uint8_t offset = op1 & 7;
                if ( op1 >= 0xd8 && op1 <= 0xdf ) // fstp st(i)
                {
                    poke_fp( offset, peek_fp( 0 ) );
                    pop_fp();
                }
                else
                {
                    rip--;
                    decode_rm();
                    if ( 0 == _reg ) // fld m64fp. convert then push double on the fp stack
                        push_fp( get_rmdouble() );
                    else if ( 2 == _reg ) // fstp m64fp  copy st(0) to m64fp
                        set_rmdouble( peek_fp( 0 ).getd() );
                    else if ( 3 == _reg ) // fstp m64fp  copy st(0) to m64fp and pop register stack
                        set_rmdouble( pop_fp().getd() );
                    else
                        unhandled();
                }
                break;
            }
            case 0xde:
            {
                uint8_t op1 = get_rip8();
                uint8_t offset = op1 & 7;
                if ( op1 >= 0xe0 && op1 <= 0xe7 ) // fsubrp st(i), st(0)
                {
                    poke_fp( offset, do_fsub( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                    pop_fp();
                }
                else if ( op1 >= 0xe8 && op1 <= 0xef ) // fsubp st(i), st(0)
                {
                    poke_fp( offset, do_fsub( peek_fp( offset ).getld(), peek_fp( 0 ).getld() ) );
                    pop_fp();
                }
                else if ( op1 >= 0xc0 && op1 <= 0xc7 ) // faddp st(i), st(0)
                {
                    poke_fp( offset, do_fadd( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                    pop_fp();
                }
                else if ( op1 >= 0xc8 && op1 <= 0xcf ) // fmulp st(i), st(0)
                {
                    poke_fp( offset, do_fmul( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                    pop_fp();
                }
                else if ( op1 >= 0xf0 && op1 <= 0xf7 ) // fdivrp st(i), st(0)
                {
                    poke_fp( offset, do_fdiv( peek_fp( 0 ).getld(), peek_fp( offset ).getld() ) );
                    pop_fp();
                }
                else if ( op1 >= 0xf8 && op1 <= 0xff ) // fdivp st(i), st(0)
                {
                    poke_fp( offset, do_fdiv( peek_fp( offset ).getld(), peek_fp( 0 ).getld() ) );
                    pop_fp();
                }
                else
                {
                    rip--;
                    decode_rm();
                    if ( 0 == _reg ) // fiadd m16int
                        poke_fp( 0, do_fadd( peek_fp( 0 ).getld(), (long double) (int16_t) get_rm16() ) );
                    else
                        unhandled();
                }
                break;
            }
            case 0xdf:
            {
                uint8_t op1 = get_rip8();
                uint8_t offset = op1 & 7;
                if ( ( op1 >= 0xe8 && op1 <= 0xef ) || // fucomip st, st(i)
                     ( op1 >= 0xf0 && op1 <= 0xf7 ) )  // fcomip st(0), st(i)
                {
                    uint32_t fcc = compare_floating( peek_fp( 0 ).getld(), peek_fp( offset ).getld() );
                    set_eflags_from_fcc( fcc );
                    pop_fp();
                }
                else if ( 0xe0 == op1 )
                {
                    update_x87_status_top();
                    regs[ rax ].q = x87_fpu_status_word;
                }
                else if ( 0 == _prefix_sse2_repeat )
                {
                    rip--;
                    decode_rm();
                    if ( 0 == _reg ) // fild m64int
                        push_fp( (double) (int16_t) get_rm16() );
                    else if ( 3 == _reg ) // fistp m16int   store st(0) in m16int and pop register stack
                    {
                        long double val = pop_fp().getld();
                        int16_t ival;
                        if ( ( val > INT16_MAX ) || ( val < INT16_MIN ) )
                            ival = INT16_MIN; // integer indefinte value my CPU stores in this case
                        else
                            ival = (int16_t) val;
                        set_rm16( ival );
                    }
                    else if ( 5 == _reg ) // fild m64int   loads signed 64 bit integer converted to a float and pushed to fp stack
                        push_fp( (long double) (int64_t) get_rm64() );
                    else if ( 7 == _reg ) // fistp m64int store st(0) as an m64int and pop fp stack
                        set_rm64( (int64_t) pop_fp().getld() );
                    else
                        unhandled();
                }
                else
                    unhandled();
                break;
            }
            case 0xe3: // jcxz / jecxz / jrcxz rel8
            {
                int64_t rel = (int8_t) get_rip8();

                bool jump;
                if ( _rexW )
                    jump = ( 0 == regs[ rcx ].q );
                else if ( 0x66 == _prefix_size )
                    jump = ( 0 == regs[ rcx ].w );
                else
                    jump = ( 0 == regs[ rcx ].d );

                if ( jump )
                    rip += rel;
                break;
            }
            case 0xe8: // call rel32
            {
                uint32_t offset = get_rip32();
                push( rip );
                rip += (int32_t) offset;
                break;
            }
            case 0xe9: // jmp cd  (relative to rip sign-extended 32-bit immediate)
            {
                rip += (int64_t) (int32_t) get_rip32();
                break;
            }
            case 0xeb: // jmp
            {
                rip += (int64_t) (int8_t) get_rip8();
                break;
            }
            case 0xf0: // lock (do nothing since there is just one thread and core supported
            {
                goto _prefix_is_set; // don't wipe other prefixes that may have been set
            }
            case 0xf2: case 0xf3: // repeat or multibyte or bnd (memory protection)
            {
                _prefix_sse2_repeat = op;
                goto _prefix_is_set;
            }
            case 0xf4: // hlt
            {
                g_State |= stateEndEmulation; // exit this function and catch fire
                break;
            }
            case 0xf5: // cmc complement carry flag
            {
                setflag_c( ! flag_c() );
                break;
            }
            case 0xf6:
            {
                decode_rm();
                if ( 0 == _reg ) // test r/m8, imm8
                {
                    uint8_t val = get_rip8();
                    op_and( get_rm8(), val );
                }
                else if ( 2 == _reg ) // not r/m8   no status flags are updated
                    set_rm8( ~ get_rm8() );
                else if ( 3 == _reg ) // neg r/m8
                {
                    uint8_t val = get_rm8();
                    setflag_c( 0 != val );
                    val = 0 - val;
                    set_rm8( val );
                    set_PSZ( val );
                }
                else if ( 4 == _reg ) // mul r/m8
                {
                    regs[ rax ].q = (uint16_t) get_rm8() * (uint16_t) regs[ rax ].b;
                    setflag_o( 0 != regs[ rax ].h );
                    setflag_c( 0 != regs[ rax ].h );
                }
                else if ( 6 == _reg ) // div r/m8
                {
                    uint16_t divisor = get_rm8();
                    if ( 0 == divisor )
                    {
                        // trap here
                    }
                    else
                    {
                        uint16_t dividend = regs[ rax ].w;
                        regs[ rax ].q = (uint8_t) ( dividend / divisor );
                        regs[ rax ].h = (uint8_t) ( dividend % divisor );
                    }
                }
                else if ( 7 == _reg ) // idiv r/m8
                {
                    int16_t divisor = get_rm8();
                    if ( 0 == divisor )
                    {
                        // trap here
                    }
                    else
                    {
                        int16_t dividend = regs[ rax ].w;
                        regs[ rax ].q = (int8_t) ( dividend / divisor );
                        regs[ rax ].h = (int8_t) ( dividend % divisor );
                    }
                }
                else
                    unhandled();
                break;
            }
            case 0xf7:
            {
                decode_rm();
                if ( 0 == _reg ) // test r/m16, imm16   test r/m32, imm32    test r/m64, se( imm32 )
                {
                    if ( _rexW )
                    {
                        uint64_t val = (uint64_t) sign_extend( get_rip32(), 31 ) ;
                        op_and( get_rm64(), val );
                    }
                    else if ( 0x66 == _prefix_size )
                    {
                        uint16_t val = get_rip16();
                        op_and( get_rm16(), val );
                    }
                    else
                    {
                        uint32_t val = get_rip32();
                        op_and( get_rm32(), val );
                    }
                }
                else if ( 2 == _reg ) // not
                {
                    if ( _rexW )
                        set_rm64( ~ get_rm64() );
                    else if ( 0x66 == _prefix_size )
                        set_rm16( ~ get_rm16() );
                    else
                        set_rm32( ~ get_rm32() );
                }
                else if ( 3 == _reg ) // neg
                {
                    if ( _rexW )
                    {
                        uint64_t val = get_rm64();
                        setflag_c( 0 != val );
                        val = 0 - val;
                        set_PSZ( val );
                        set_rm64( val );
                    }
                    else if ( 0x66 == _prefix_size )
                    {
                        uint16_t val = get_rm16();
                        setflag_c( 0 != val );
                        val = 0 - val;
                        set_PSZ( val );
                        set_rm16( val );
                    }
                    else
                    {
                        uint32_t val = get_rm32();
                        setflag_c( 0 != val );
                        val = 0 - val;
                        set_PSZ( val );
                        set_rm32( val );
                    }
                }
                else if ( 4 == _reg ) // mul
                {
                    if ( _rexW ) // 64-bit
                    {
                        uint64_t resultHigh;
                        regs[ rax ].q = CMultiply128::mul_u64_u64( regs[ rax ].q, get_rm64(), &resultHigh );
                        regs[ rdx ].q = resultHigh;
                    }
                    else if ( 0x66 == _prefix_size )
                    {
                        uint32_t result = (uint32_t) regs[ rax ].w * get_rm16();
                        regs[ rax ].q = result & 0xffff;
                        regs[ rdx ].q = result >> 16;
                    }
                    else // 32-bit
                    {
                        uint64_t result = (uint64_t) regs[ rax ].d * get_rm32();
                        regs[ rax ].q = result & 0xffffffff;
                        regs[ rdx ].q = result >> 32;
                    }
                }
                else if ( 5 == _reg ) // imul
                {
                    if ( _rexW ) // 64-bit
                    {
                        int64_t resultHigh;
                        regs[ rax ].q = CMultiply128::mul_s64_s64( regs[ rax ].q, get_rm64(), &resultHigh );
                        regs[ rdx ].q = resultHigh;
                    }
                    else if ( 0x66 == _prefix_size )
                    {
                        int32_t result = (int32_t) (int16_t) regs[ rax ].w * (int32_t) (int16_t) get_rm16();
                        regs[ rax ].q = result & 0xffff;
                        regs[ rdx ].q = result >> 16;
                    }
                    else // 32-bit
                    {
                        int64_t result = (int64_t) (int32_t) regs[ rax ].d * (int64_t) (int32_t) get_rm32();
                        regs[ rax ].q = result & 0xffffffff;
                        regs[ rdx ].q = result >> 32;
                    }
                }
                else if ( 6 == _reg ) // div
                {
                    if ( _rexW ) // 64-bit
                    {
                        uint64_t divisor = get_rm64();
                        if ( 0 == divisor )
                        {
                            // trap here
                        }
                        else
                        {
                            struct UInt128_t dividend;
                            dividend.high = regs[ rdx ].q;
                            dividend.low = regs[ rax ].q;
                            uint64_t quotient, remainder;
                            divideUInt128ByUInt64( dividend, divisor, quotient, remainder );
                            //tracer.Trace( "div. dividend high %#llx, dividend low %#llx, divisor %#llx, quotient %#llx, remainder %#llx\n",
                            //              dividend.high, dividend.low, divisor, quotient, remainder );
                            regs[ rax ].q = quotient;
                            regs[ rdx ].q = remainder;
                        }
                    }
                    else if ( 0x66 == _prefix_size )
                    {
                        uint32_t divisor = get_rm16();
                        if ( 0 == divisor )
                        {
                            // trap here
                        }
                        else
                        {
                            uint32_t dividend = ( ( (uint32_t) regs[ rdx ].w ) << 16 ) | regs[ rax ].w;
                            regs[ rax ].q = (uint16_t) ( dividend / divisor );
                            regs[ rdx ].q = (uint16_t) ( dividend % divisor );
                        }
                    }
                    else
                    {
                        uint64_t divisor = get_rm32();
                        if ( 0 == divisor )
                        {
                            // trap here
                        }
                        else
                        {
                            uint64_t dividend = ( regs[ rdx ].q << 32 ) | regs[ rax ].d;
                            regs[ rax ].q = (uint32_t) ( dividend / divisor );
                            regs[ rdx ].q = (uint32_t) ( dividend % divisor );
                        }
                    }
                }
                else if ( 7 == _reg ) // idiv
                {
                    if ( _rexW ) // 64-bit
                    {
                        int64_t divisor = get_rm64();
                        if ( 0 == divisor )
                        {
                            // trap here
                        }
                        else
                        {
                            struct Int128_t dividend;
                            dividend.high = regs[ rdx ].q;
                            dividend.low = regs[ rax ].q;
                            int64_t quotient, remainder;
                            divide_i128_by_i64( dividend, divisor, quotient, remainder );
                            regs[ rax ].q = quotient;
                            regs[ rdx ].q = remainder;
                        }
                    }
                    else if ( 0x66 == _prefix_size )
                    {
                        int32_t divisor = (int16_t) get_rm16();
                        if ( 0 == divisor )
                        {
                            // trap here
                        }
                        else
                        {
                            int32_t dividend = ( ( (int32_t) regs[ rdx ].w ) << 16 ) | regs[ rax ].w ;
                            regs[ rax ].w = (uint16_t) (int16_t) ( dividend / divisor );
                            regs[ rdx ].q = (uint16_t) (int16_t) ( dividend % divisor );
                        }
                    }
                    else
                    {
                        int64_t divisor = (int32_t) get_rm32();
                        if ( 0 == divisor )
                        {
                            // trap here
                        }
                        else
                        {
                            int64_t dividend = ( regs[ rdx ].q << 32 ) | regs[ rax ].d;
                            regs[ rax ].q = (uint32_t) (int32_t) ( dividend / divisor );
                            regs[ rdx ].q = (uint32_t) (int32_t) ( dividend % divisor );
                        }
                    }
                }
                else
                    unhandled();
                break;
            }
            case 0xf8: { setflag_c( false ); break; } // clc
            case 0xf9: { setflag_c( true ); break; } // stc
            case 0xfa: { setflag_i( false ); break; } // cli
            case 0xfb: { setflag_i( true ); break; } // sti
            case 0xfc: { setflag_d( false ); break; } // cld
            case 0xfd: { setflag_d( true ); break; } // std
            case 0xfe:
            {
                decode_rm();
                switch( _reg )
                {
                    case 0: // inc r/m8. carry is unaffected
                    {
                        uint8_t * pdst = get_rm_ptr8();
                        *pdst = *pdst + 1;
                        set_PSZ( *pdst );
                        setflag_o( 0 == *pdst );
                        break;
                    }
                    case 1: // dec r/m8
                    {
                        uint8_t * pdst = get_rm_ptr8();
                        *pdst = *pdst - 1;
                        set_PSZ( *pdst );
                        setflag_o( 0 == *pdst );
                        break;
                    }
                    default: unhandled();
                }
                break;
            }
            case 0xff:
            {
                decode_rm();
                switch( _reg )
                {
                    case 0: // inc r/m. carry is unaffected
                    {
                        if ( _rexW )
                        {
                            uint64_t val = 1 + get_rm64();
                            set_PSZ( val );
                            setflag_o( 0 == val );
                            set_rm64( val );
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            uint16_t val = 1 + get_rm16();
                            set_PSZ( val );
                            setflag_o( 0 == val );
                            set_rm16( val );
                        }
                        else
                        {
                            uint32_t val = 1 + get_rm32();
                            set_PSZ( val );
                            setflag_o( 0 == val );
                            set_rm32( val );
                        }
                        break;
                    }
                    case 1: // dec r/m
                    {
                        if ( _rexW )
                        {
                            uint64_t val = get_rm64() - 1;
                            set_PSZ( val );
                            setflag_o( ~0ull == val );
                            set_rm64( val );
                        }
                        else if ( 0x66 == _prefix_size )
                        {
                            uint16_t val = get_rm16() - 1;
                            set_PSZ( val );
                            setflag_o( 0xffff == val );
                            set_rm16( val );
                        }
                        else
                        {
                            uint32_t val = get_rm32() - 1;
                            set_PSZ( val );
                            setflag_o( ~0 == val );
                            set_rm32( val );
                        }
                        break;
                    }
                    case 2: // call
                    {
                        push( rip );
                        rip = get_rm64();
                        break;
                    }
                    case 3: // call
                        unhandled();
                    case 4: // jmp near absolute indirect. rip = 64 bit offset from r/m
                    {
                        rip = get_rm64();
                        break;
                    }
                    case 5: // jmp
                        unhandled();
                    case 6: // push
                    {
                        if ( 0x66 == _prefix_size )
                            unhandled();
                        push( get_rm64() );
                        break;
                    }
                    default: unhandled();
                }
                break;
            }
            default:
            {
                printf( "default unhandled opcode at rip %#llx, op %#x\n", rip, op );
                unhandled();
            }
        }
    }

    return instruction_count;
} //run
