#pragma once

#include <bitset>
#include <djl_os.hxx>
#include "f80_double.h"

struct x64;

extern void emulator_invoke_svc( x64 & cpu );                                                // called when the syscall instruction is executed
extern const char * emulator_symbol_lookup( uint64_t address, uint64_t & offset );           // returns the best guess for a symbol name and offset for the address
extern const char * emulator_symbol_lookup( uint32_t address, uint32_t & offset );           // returns the best guess for a symbol name and offset for the address
extern void emulator_hard_termination( x64 & cpu, const char *pcerr, uint64_t error_value ); // show an error and exit

template <typename T> inline bool val_signed( T x )
{
    uint64_t mask = ( 0x80ull << ( ( sizeof( T ) - 1 ) * 8 ) );
    return ( 0 != ( x & (T) mask ) );
} //val_signed

template <typename T> inline bool second_highest_bit( T x )
{
    uint64_t mask = ( 0x40ull << ( ( sizeof( T ) - 1 ) * 8 ) );
    return ( 0 != ( x & (T) mask ) );
} //second_highest_bit

template <typename T> inline T mk_signed( T x )
{
    T hibit = ( ( (T) 0x80 ) << ( ( (uint8_t) sizeof( T ) - 1 ) * 8 ) );
    return ( x | hibit );
} //mk_signed

typedef struct vec16_t // 16-byte vector
{
    #ifdef TARGET_BIG_ENDIAN
        uint16_t get16( uint64_t elem ) { return flip_endian16( ui16[ elem ] ); }
        void set16( uint64_t elem, uint16_t val ) { ui16[ elem ] = flip_endian16( val ); }
        uint32_t get32( uint64_t elem ) { return flip_endian32( ui32[ elem ] ); }
        void set32( uint64_t elem, uint32_t val ) { ui32[ elem ] = flip_endian32( val ); }
        uint64_t get64( uint64_t elem ) { return flip_endian64( ui64[ elem ] ); }
        void set64( uint64_t elem, uint64_t val ) { ui64[ elem ] = flip_endian64( val ); }
        float getf( uint64_t elem ) { uint32_t x = get32( elem ); return * (float *) & x; }
        void setf( uint64_t elem, float val ) { set32( elem, * (uint32_t *) & val); }
        double getd( uint64_t elem ) { uint64_t x = get64( elem ); return * (double *) & x; }
        void setd( uint64_t elem, double val ) { set64( elem, * (uint64_t *) & val); }
    #else
        uint16_t get16( uint64_t elem ) { return ui16[ elem ];  }
        void set16( uint64_t elem, uint16_t val ) { ui16[ elem ] = val; }
        uint32_t get32( uint64_t elem ) { return ui32[ elem ];  }
        void set32( uint64_t elem, uint32_t val ) { ui32[ elem ] = val; }
        uint64_t get64( uint64_t elem ) { return ui64[ elem ];  }
        void set64( uint64_t elem, uint64_t val ) { ui64[ elem ] = val; }
        float getf( uint64_t elem ) { return f[ elem ]; }
        void setf( uint64_t elem, float val ) { f[ elem ] = val; }
        double getd( uint64_t elem ) { return d[ elem ]; }
        void setd( uint64_t elem, double val ) { d[ elem ] = val; }
    #endif

    uint8_t get8( uint64_t elem ) { return ui8[ elem ]; }
    void set8( uint64_t elem, uint8_t val ) { ui8[ elem ] = val; }

    vec16_t() { zero(); }
    void zero() { ui64[ 0 ] = 0; ui64[ 1 ] = 0; }

    private: // private to force use of the endian-safe member functions
        union
        {
            uint64_t ui64[ 2 ];
            uint32_t ui32[ 4 ];
            uint16_t ui16[ 8 ];
            uint8_t ui8[ 16 ];
            double d[ 2 ];
            float f[ 4 ];
        };
} vec16_t;

typedef struct reg8_t // 8-byte register
{
    union
    {
        uint64_t q;
        #ifdef TARGET_BIG_ENDIAN
            struct { uint32_t filler_d; uint32_t d; };
            struct { uint32_t filler_w1; uint16_t filler_w2; uint16_t w; };
            struct { uint32_t filler_b1; uint8_t filler_b2; uint8_t filler_b3; uint8_t h; uint8_t b; };
        #else
            uint32_t d;
            uint16_t w;
            struct { uint8_t b; uint8_t h; };
        #endif
    };
} reg8_t;

typedef struct REXInfo
{
    union // use a union because some compilers don't otherwise optimize setting all to 0 at once for each instruction in 32-bit mode
    {
        struct { uint8_t W, R, X, B; };
        uint32_t All;
    };
} REXInfo_t;

// gnu on amd64 has 10-byte long doubles with the same format as x87 floating point (ieee80).
// msvc maps long double to 8-byte doubles. this emulator produces incorrect results when built with msvc.
// gnu on non-amd64 ISAs claim to have 80-bit long doubles, but I haven't been able to get them to work. they use 8-byte doubles like msvc.
// In some cases, the difference is small -- a loss of precision that's not generally noticable.
// In other cases, it's catastrophic. for example, multiplying a floating by -1 stored in an int128_t
// turns into addition by 1 beyond the precision of the number as a double and then a subtraction leading
// to completely different results for apps.
// There are other changes in behavior that dramatically alter emulator behavor with doubles due to the
// loss in precision when using various sqrtl(), fpreml() and other instructions. For example: 3.2 can't
// be expressed exactly, so fpreml( 16.0, 3.2 ) results in 3.2 with 10-byte fp and 0.0 with 8-byte fp.
// Obviously, apps not using long double (i.e. just use float and double) work fine when using 8-byte long doubles.
// clang++ v19.1.5 building with -mlong-double-80 exposes many bugs in their implementation and so is avoided here.

#if defined( __GNUC__ ) && ( defined( __amd64__ ) || defined( __i386__ ) )
#define NATIVE_LONG_DOUBLE 1         // use native 10-byte x87 long doubles on amd64
#else
#define NATIVE_LONG_DOUBLE 0         // use 8-byte long double with a loss in precision
#endif

typedef struct float80_t // 10-byte x87 floating point register
{
    public:
        #if NATIVE_LONG_DOUBLE
            static float80_t float80_from_ld( long double val ) { float80_t x; x.setld( val ); return x; }
            static float80_t float80_from_d( double val ) { float80_t x; x.setd( val ); return x; }

            long double getld() { return ld; }
            void setld( long double val ) { ld = val; }
            double getd() { return (double) ld; }
            void setd( double val ) { ld = (long double) val; }
            float getf() { return (float) ld; }
            void setf( float val ) { ld = (long double) val; }
        #else
            static float80_t float80_from_ld( long double val ) { float80_t x; double_to_ieee80( (double) val, x.bytes ); return x; }
            static float80_t float80_from_d( double val ) { float80_t x; double_to_ieee80( val, x.bytes ); return x; }

            long double getld() { return (long double) ieee80_to_double( bytes ); }
            void setld( long double val ) { double_to_ieee80( (double) val, bytes ); }
            double getd() { return ieee80_to_double( bytes ); }
            void setd( double val ) { double_to_ieee80( val, bytes ); }
            float getf() { return (float) ieee80_to_double( bytes ); }
            void setf( float val ) { double_to_ieee80( (double) val, bytes ); }
        #endif
    private:
        union
        {
            #if NATIVE_LONG_DOUBLE
                long double ld; // may be padded to 16 bytes, but with gcc on amd64 the first 80 bits are in x87 ieee80 format
            #endif

            uint8_t bytes[ 10 ]; // little-endian x87 ieee80 80-bit
            uint64_t alignment_bytes[ 2 ];
        };
} float80_t;

struct x64
{
    static const size_t rax =  0;
    static const size_t rcx =  1;
    static const size_t rdx =  2;
    static const size_t rbx =  3;
    static const size_t rsp =  4;
    static const size_t rbp =  5;
    static const size_t rsi =  6;
    static const size_t rdi =  7;
    static const size_t r8 =   8;
    static const size_t r9 =   9;
    static const size_t r10 = 10;
    static const size_t r11 = 11;
    static const size_t r12 = 12;
    static const size_t r13 = 13;
    static const size_t r14 = 14;
    static const size_t r15 = 15;

    bool trace_instructions( bool trace );         // enable/disable tracing each instruction
    void end_emulation( void );                    // make the emulator return at the start of the next instruction
    uint64_t run( void );

    x64( vector<uint8_t> & memory, uint64_t base_address, uint64_t start, uint64_t stack_commit, uint64_t top_of_stack )
    {
        memset( this, 0, sizeof( *this ) );
        mode32 = false;                            // start in 64-bit long mode
        x87_fpu_control_word = 0x37f;              // hardware boots in this state
        rip.q = start;                             // execution starts here
        stack_size = stack_commit;                 // remember how much of the top of RAM is allocated to the stack
        stack_top = top_of_stack;                  // where the stack started
        regs[ rsp ].q = top_of_stack;              // points at argc with argv, penv, and aux records above it
        base = base_address;                       // lowest valid address in the app's address space, maps to offset 0 in mem
        mem = memory.data();                       // save the pointer, but don't take ownership
        mem_size = memory.size();
        beyond = mem + memory.size();              // addresses beyond and later are illegal
        membase = mem - base;                      // real pointer to the start of the app's memory (prior to offset)
    } //x64

    uint8_t * mem;
    uint8_t * beyond;
    uint64_t base;
    uint8_t * membase;                             // host pointer to base of vm's memory
    uint64_t stack_size;
    uint64_t stack_top;
    uint64_t mem_size;

    uint64_t getoffset( uint64_t address )
    {
        return address - base;
    } //getoffset

    uint64_t get_vm_address( uint64_t offset )
    {
        return base + offset;
    } //get_vm_address

    uint64_t host_to_vm_address( void * p )
    {
        return (uint64_t) ( (uint8_t *) p - mem + base );
    } //host_to_vm_address

    inline uint8_t * getmem( uint64_t offset )
    {
        #ifdef NDEBUG
            return membase + offset;
        #else
            uint8_t * r = membase + offset;

            if ( r >= beyond )
                emulator_hard_termination( *this, "memory reference beyond address space:", offset );

            if ( r < mem )
                emulator_hard_termination( *this, "memory reference prior to address space:", offset );

            return r;
        #endif
    } //getmem

    bool is_address_valid( uint64_t offset )
    {
        uint8_t * r = membase + offset;
        return ( ( r < beyond ) && ( r >= mem ) );
    } //is_address_valid

    #ifdef TARGET_BIG_ENDIAN
        uint64_t getui64( uint64_t o ) { return flip_endian64( * (uint64_t *) getmem( o ) ); }
        uint32_t getui32( uint64_t o ) { return flip_endian32( * (uint32_t *) getmem( o ) ); }
        uint16_t getui16( uint64_t o ) { return flip_endian16( * (uint16_t *) getmem( o ) ); }
        float getfloat( uint64_t o ) { uint32_t x = getui32( o ); return * (float *) & x; }
        double getdouble( uint64_t o ) { uint64_t x = getui64( o ); return * (double *) & x; }

        void setui64( uint64_t o, uint64_t val ) { * (uint64_t *) getmem( o ) = flip_endian64( val ); }
        void setui32( uint64_t o, uint32_t val ) { * (uint32_t *) getmem( o ) = flip_endian32( val ); }
        void setui16( uint64_t o, uint16_t val ) { * (uint16_t *) getmem( o ) = flip_endian16( val ); }
        void setfloat( uint64_t o, float val ) { uint32_t x = * (uint32_t *) & val; setui32( o, x ); }
        void setdouble( uint64_t o, double val ) { uint64_t x = * (uint64_t *) & val; setui64( o, x ); }
    #else
        uint64_t getui64( uint64_t o ) { return * (uint64_t *) getmem( o ); }
        uint32_t getui32( uint64_t o ) { return * (uint32_t *) getmem( o ); }
        uint16_t getui16( uint64_t o ) { return * (uint16_t *) getmem( o ); }
        float getfloat( uint64_t o ) { return * (float *) getmem( o ); }
        double getdouble( uint64_t o ) { return * (double *) getmem( o ); }

        void setui64( uint64_t o, uint64_t val ) { * (uint64_t *) getmem( o ) = val; }
        void setui32( uint64_t o, uint32_t val ) { * (uint32_t *) getmem( o ) = val; }
        void setui16( uint64_t o, uint16_t val ) { * (uint16_t *) getmem( o ) = val; }
        void setfloat( uint64_t o, float val ) { * (float *) getmem( o ) = val; }
        void setdouble( uint64_t o, double val ) { * (double *) getmem( o ) = val; }
    #endif //TARGET_BIG_ENDIAN

    uint8_t getui8( uint64_t o ) { return * (uint8_t *) getmem( o ); }
    void setui8( uint64_t o, uint8_t val ) { * (uint8_t *) getmem( o ) = val; }

    reg8_t regs[ 16 ];               // rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8..r15
    vec16_t xregs[ 16 ];             // xmm0 through 15
    float80_t fregs[ 8 ];            // 80-bit numbers are stored in this fp stack, while math is done as 8-byte or 10-byte doubles depending on the compiler and ISA
    reg8_t rip;                      // instruction pointer
    reg8_t res, rcs, rss, rds, rfs, rgs; // fs is used by glibc for thread state on x64 and on x32 it's gs. as a simplification, store and use the address the segment refers to.
    uint32_t mxcsr;
    uint16_t x87_fpu_control_word;   // for fldcw, fstcw/fnstcw. applies to sse as well as x87
    uint16_t x87_fpu_status_word;    // for fstsw/fnstsw.
    uint8_t fp_sp;                   // current stack pointer for fregs[]
    bool mode32;                     // true for 32-bit CPU vs 64-bit

    void Mode32( bool m32 ) { mode32 = m32; } // flip from 64-bit long mode to 32-bit compatibility mode for running 32-bit apps. or back.

    uint64_t & reg_fs() { return rfs.q; }
    uint64_t & reg_gs() { return rgs.q; }

private:
                      // 0                                   8                                16
    uint64_t rflags;  // C, n/a, P, n/a, A, n/a, Z, S,   :   T, I, D, O, IOPL+IOPL, n/a   :   RF, VM, AC, VIF, VIP, ID, 22.31 n/a

    void setflag_c( bool f ) { rflags &= ~( 1 << 0 );  rflags |= ( ( 0 != f ) << 0 );  } // carry
    void setflag_p( bool f ) { rflags &= ~( 1 << 2 );  rflags |= ( ( 0 != f ) << 2 );  } // parity even
    void setflag_a( bool f ) { rflags &= ~( 1 << 4 );  rflags |= ( ( 0 != f ) << 4 );  } // auxiliary carry
    void setflag_z( bool f ) { rflags &= ~( 1 << 6 );  rflags |= ( ( 0 != f ) << 6 );  } // zero
    void setflag_s( bool f ) { rflags &= ~( 1 << 7 );  rflags |= ( ( 0 != f ) << 7 );  } // signed
    void setflag_i( bool f ) { rflags &= ~( 1 << 9 );  rflags |= ( ( 0 != f ) << 9 );  } // interrupt
    void setflag_d( bool f ) { rflags &= ~( 1 << 10 ); rflags |= ( ( 0 != f ) << 10 ); } // direction
    void setflag_o( bool f ) { rflags &= ~( 1 << 11 ); rflags |= ( ( 0 != f ) << 11 ); } // overflow

    bool flag_c() { return ( 0 != ( rflags & ( 1 << 0 ) ) );  } // carry
    bool flag_p() { return ( 0 != ( rflags & ( 1 << 2 ) ) );  } // parity even
    bool flag_a() { return ( 0 != ( rflags & ( 1 << 4 ) ) );  } // auxiliary carry
    bool flag_z() { return ( 0 != ( rflags & ( 1 << 6 ) ) );  } // zero
    bool flag_s() { return ( 0 != ( rflags & ( 1 << 7 ) ) );  } // signed
    bool flag_i() { return ( 0 != ( rflags & ( 1 << 9 ) ) );  } // interrupt
    bool flag_d() { return ( 0 != ( rflags & ( 1 << 10 ) ) ); } // direction
    bool flag_o() { return ( 0 != ( rflags & ( 1 << 11 ) ) ); } // overflow

    const char * render_flags()
    {
        static char buf[ 8 ];
        buf[ 0 ] = flag_c() ? 'C' : 'c';
        buf[ 1 ] = flag_p() ? 'P' : 'p';
        buf[ 2 ] = flag_a() ? 'A' : 'a';
        buf[ 3 ] = flag_z() ? 'Z' : 'z';
        buf[ 4 ] = flag_s() ? 'S' : 's';
        buf[ 5 ] = flag_d() ? 'D' : 'd';
        buf[ 6 ] = flag_o() ? 'O' : 'o';
        buf[ 7 ] = 0;
        return buf;
    } //render_flags

    uint8_t _prefix_rex;                // 0 for none. 0x4x
    uint8_t _prefix_size;               // 0 for none. can be 0x66 (operand size 16) or 0x67 (address size 32)
    uint8_t _prefix_sse2_repeat;        // 0 for none. f2 repne/repnz, f3 rep/repe/repz. f3 can also mean multibyte. f2 can also mean bnd (memory protection)
    uint8_t _prefix_segment;            // 0 for none. 0x64 for fs: or 0x65 for gs:

    // decoding state and functions

    uint64_t _instruction_start;        // rip where decoding of the current instruction started
    int64_t _displacement;
    REXInfo _rex;
    uint8_t _rm, _reg, _mod;
    uint8_t _sibScale, _sibIndex, _sibBase;

    void decode_sib();
    void decode_rex();
    void decode_rm();
    void clear_decoding()
    {
        _rex.All = 0;
        _rm = _reg = _mod = 0;
        _sibScale = _sibIndex = _sibBase = 0;
        _displacement = 0;
    } //clear_decoding

    uint64_t effective_address();

    inline uint64_t pop()
    {
        if ( mode32 )
        {
            uint32_t val = getui32( regs[ rsp ].q );
            regs[ rsp ].q += 4;
            return val;
        }

        uint64_t val = getui64( regs[ rsp ].q );
        regs[ rsp ].q += 8;
        return val;
    } //pop

    inline void push( uint64_t val )
    {
        if ( mode32 )
        {
            regs[ rsp ].q -= 4;
            setui32( regs[ rsp ].d, (uint32_t) val );
        }
        else
        {
            regs[ rsp ].q -= 8;
            setui64( regs[ rsp ].q, val );
        }
    } //push

    static inline int64_t sign_extend( uint64_t x, uint64_t high_bit )
    {
        assert( high_bit < 63 );
        x &= ( 1ull << ( high_bit + 1 ) ) - 1; // clear bits above the high bit since they may contain noise
        const int64_t m = 1ull << high_bit;
        return ( x ^ m ) - m;
    } //sign_extend

    static inline uint32_t sign_extend32( uint32_t x, uint32_t high_bit )
    {
        assert( high_bit < 31 );
        x &= ( 1u << ( high_bit + 1 ) ) - 1; // clear bits above the high bit since they may contain noise
        const int32_t m = ( (uint32_t) 1 ) << high_bit;
        return ( x ^ m ) - m;
    } //sign_extend32

    static inline uint16_t sign_extend16( uint16_t x, uint16_t high_bit )
    {
        assert( high_bit < 15 );
        x &= ( 1u << ( high_bit + 1 ) ) - 1; // clear bits above the high bit since they may contain noise
        const int16_t m = ( (uint16_t) 1 ) << high_bit;
        return ( x ^ m ) - m;
    } //sign_extend16

    static inline bool is_parity_even8( uint8_t x ) // unused by apps and expensive to compute.
    {
#if defined( _M_AMD64 ) || defined( _M_IX86 )
        return ( ! ( __popcnt16( x ) & 1 ) ); // less portable, but faster. Not on Q9650 CPU and other older Intel CPUs. use code below instead if needed.
#elif defined( __aarch64__ )
        return ( ! ( std::bitset<8>( x ).count() & 1 ) );
#else
        x ^= ( x >> 4 );
        x ^= ( x >> 2 );
        x ^= ( x >> 1 );
        return ! ( x & 1 );
#endif
    } //is_parity_even8

    template <typename T> inline void set_PSZ( T val )
    {
        setflag_p( is_parity_even8( 0xff & val ) );
        setflag_z( 0 == val );
        setflag_s( val_signed( val ) );
    } //set_PSZ

    void reset_CO() { rflags &= ~( 0x801 ); }

    template <typename T> T op_add( T lhs, T rhs, bool carry = false );
    template <typename T> T op_sub( T lhs, T rhs, bool carry = false );
    template <typename T> T op_or( T lhs, T rhs );
    template <typename T> T op_and( T lhs, T rhs );
    template <typename T> T op_xor( T lhs, T rhs );
    template <typename T> void do_math( uint8_t math, T * pdst, T src );

    void op_stos( uint8_t width );
    void op_movs( uint8_t width );
    void op_scas( uint8_t width );

    inline uint8_t get_rip8() { return getui8( rip.q++ ); }
    inline uint16_t get_rip16() { uint16_t val = getui16( rip.q ); rip.q += 2; return val; }
    inline uint32_t get_rip32() { uint32_t val = getui32( rip.q ); rip.q += 4; return val; }
    inline uint64_t get_rip64() { uint64_t val = getui64( rip.q ); rip.q += 8; return val; }

    inline uint8_t get_reg8()
    {
        if ( 0 == _prefix_rex && _reg >= 4 )
        {
            assert( _reg <= 7 );
            return regs[ _reg & 3 ].h;
        }
        return regs[ _reg ].b;
    } //get_reg8

    inline void set_reg8( uint8_t val )
    {
        if ( 0 == _prefix_rex && _reg >= 4 )
        {
            assert( _reg <= 7 );
            regs[ _reg & 3 ].h = val;
        }
        else
            regs[ _reg ].b = val;
    } //set_reg8

    inline uint8_t * get_rm_ptr8()
    {
        if ( _mod < 3 )
            return getmem( effective_address() );

        if ( ( 0 == _prefix_rex ) && ( _rm >= 4 ) )
        {
            assert( _rm <= 7 );
            return ( & regs[ _rm & 3 ].h ); // ah, ch, dh, bh
        }

        return & regs[ _rm ].b;
    } //get_rm_ptr8

    inline uint64_t get_rm() { return _rex.W ? get_rm64() : ( 0x66 == _prefix_size ) ? get_rm16() : get_rm32(); }

    inline uint8_t get_rm8()
    {
        if ( _mod < 3 )
            return getui8( effective_address() );

        if ( ( 0 == _prefix_rex ) && ( _rm >= 4 ) )
        {
            assert( _rm <= 7 );
            return ( regs[ _rm & 3 ].h ); // ah, ch, dh, bh
        }

        return regs[ _rm ].b;
    } //get_rm8

    inline uint16_t get_rm16()
    {
        if ( _mod < 3 )
            return getui16( effective_address() );

        return regs[ _rm ].w;
    } //get_rm16

    inline uint32_t get_rm32()
    {
        if ( _mod < 3 )
            return getui32( effective_address() );

        return regs[ _rm ].d;
    } //get_rm32

    inline uint64_t get_rm64()
    {
        if ( _mod < 3 )
            return getui64( effective_address() );

        return regs[ _rm ].q;
    } //get_rm64

    inline double get_rmdouble()
    {
        assert( _mod < 3 );
        return getdouble( effective_address() );
    } //get_rmdouble

    inline float get_rmfloat()
    {
        assert( _mod < 3 );
        return getfloat( effective_address() );
    } //get_rmfloat

    inline void set_rm8( uint8_t val )
    {
        if ( _mod < 3 )
            setui8( effective_address(), val );
        else
        {
            if ( 0 == _prefix_rex && _rm >= 4 )
            {
                assert( _rm <= 7 );
                regs[ _rm & 3 ].h = val; // ah, ch, dh, bh
            }
            else
                regs[ _rm ].b = val;
        }
    } //set_rm8

    inline void set_rm16( uint16_t val )
    {
        if ( _mod < 3 )
            setui16( effective_address(), val );
        else
            regs[ _rm ].w = val;
    } //set_rm16

    inline void set_rm32( uint32_t val )
    {
        if ( _mod < 3 )
            setui32( effective_address(), val );
        else
            regs[ _rm ].d = val; // no zero-extend
    } //set_rm32

    inline void set_rm32z( uint32_t val )
    {
        if ( _mod < 3 )
            setui32( effective_address(), val );
        else
            regs[ _rm ].q = val; // zero-extend
    } //set_rm32z

    inline void set_rm64( uint64_t val )
    {
        if ( _mod < 3 )
            setui64( effective_address(), val );
        else
            regs[ _rm ].q = val;
    } //set_rm64

    inline void set_rmdouble( double val )
    {
        assert( _mod < 3 );
        setdouble( effective_address(), val );
    } //set_rmdouble

    inline void set_rmfloat( float val )
    {
        assert( _mod < 3 );
        setfloat( effective_address(), val );
    } //set_rmfloat

    uint8_t get_rmx8( uint32_t e )
    {
        assert( e < 16 );
        if ( _mod < 3 )
            return getui8( effective_address() + e );
        return xregs[ _rm ].get8( e );
    } //get_rmx8

    uint16_t get_rmx16( uint32_t e )
    {
        assert( e < 8 );
        if ( _mod < 3 )
            return getui16( effective_address() + 2 * e );
        return xregs[ _rm ].get16( e );
    } //get_rmx16

    uint32_t get_rmx32( uint32_t e )
    {
        assert( e < 4 );
        if ( _mod < 3 )
            return getui32( effective_address() + 4 * e );
        return xregs[ _rm ].get32( e );
    } //get_rmx32

    uint64_t get_rmx64( uint32_t e )
    {
        assert( e < 2 );
        if ( _mod < 3 )
            return getui64( effective_address() + 8 * e );
        return xregs[ _rm ].get64( e );
    } //get_rmx64

    double get_rmxdouble( uint32_t e )
    {
        assert( e < 2 );
        if ( _mod < 3 )
            return getdouble( effective_address() + 8 * e );
        return xregs[ _rm ].getd( e );
    } //get_rmxdouble

    float get_rmxfloat( uint32_t e )
    {
        assert( e < 4 );
        if ( _mod < 3 )
            return getfloat( effective_address() + 4 * e );
        return xregs[ _rm ].getf( e );
    } //get_rmxfloat

    void set_rmx32( uint32_t e, uint32_t val )
    {
        assert( e < 4 );
        if ( _mod < 3 )
            setui32( effective_address() + e * 4, val );
        else
            xregs[ _rm ].set32( e, val );
    } //set_rmx32

    void set_rmxfloat( uint32_t e, float val )
    {
        assert( e < 4 );
        if ( _mod < 3 )
            setfloat( effective_address() + e * 4, val );
        else
            xregs[ _rm ].setf( e, val );
    } //set_rmxfloat

    void set_rmx32_2( uint32_t val, uint32_t val2 )
    {
        if ( _mod < 3 )
        {
            uint64_t ea = effective_address();
            setui32( ea, val );
            setui32( 4 + ea, val2 );
        }
        else
        {
            xregs[ _rm ].set32( 0, val );
            xregs[ _rm ].set32( 1, val2 );
        }
    } //set_rmx32_2

    void set_rmx64( uint32_t e, uint64_t val )
    {
        assert( e < 2 );
        if ( _mod < 3 )
            setui64( effective_address() + e * 8, val );
        else
            xregs[ _rm ].set64( e, val );
    } //set_rmx64

    void set_rmx64_2( uint64_t val, uint64_t val2 )
    {
        if ( _mod < 3 )
        {
            uint64_t ea = effective_address();
            setui64( ea, val );
            setui64( 8 + ea, val2 );
        }
        else
        {
            xregs[ _rm ].set64( 0, val );
            xregs[ _rm ].set64( 1, val2 );
        }
    } //set_rmx64_2

    void set_rmx32_4( uint32_t val, uint32_t val2, uint32_t val3, uint32_t val4 )
    {
        if ( _mod < 3 )
        {
            uint64_t ea = effective_address();
            setui32( ea, val );
            setui32( 4 + ea, val2 );
            setui32( 8 + ea, val3 );
            setui32( 12 + ea, val4 );
        }
        else
        {
            xregs[ _rm ].set32( 0, val );
            xregs[ _rm ].set32( 1, val2 );
            xregs[ _rm ].set32( 2, val3 );
            xregs[ _rm ].set32( 3, val4 );
        }
    } //set_rmx32

    uint8_t op_width() { return ( _rex.W ? 8 : ( 0x66 == _prefix_size ) ? 2 : 4 ); }
    const char * rm_displacement_string();
    const char * rm_string( uint8_t width, bool is_xmm = false );
    const char * register_name( uint8_t reg, uint8_t width = 8, bool is_xmm = false );

    bool check_condition( uint8_t condition );
    template <typename T> inline uint32_t compare_floating( T a, T b );
    template <typename T> inline bool floating_comparison_true( T a, T b, uint8_t predicate );
    void set_eflags_from_fcc( uint32_t fcc );

    template <typename T> void op_shift( T * pval, uint8_t operation, uint8_t amount );
    template <typename T> void op_rol( T * pval, uint8_t amount );
    template <typename T> void op_ror( T * pval, uint8_t amount );
    template <typename T> void op_rcl( T * pval, uint8_t amount );
    template <typename T> void op_rcr( T * pval, uint8_t amount );
    template <typename T> void op_sal( T * pval, uint8_t amount ); // aka shl
    template <typename T> void op_shr( T * pval, uint8_t amount );
    template <typename T> void op_sar( T * pval, uint8_t amount );

    void push_fp( float80_t f80 );
    void push_fp( long double val );
    float80_t pop_fp();
    float80_t peek_fp( uint8_t offset );
    void poke_fp( uint8_t offset, float80_t f80 );
    void poke_fp( uint8_t offset, long double val );
    void poke_fp( uint8_t offset, double val );

    template <typename T> T handle_math_nan( T a, T b );
    template <typename T> T do_fadd( T a, T b );
    template <typename T> T do_fsub( T a, T b );
    template <typename T> T do_fmul( T a, T b );
    template <typename T> T do_fdiv( T a, T b );

    /*
        fp control word
        ---------------
        11-10  rounding control. 00 = nearest, 01 = round down, 10 = round up, 11 = round towards 0 / trunc
         9-8   precision control. 00 = float, 01 = reserved, 10 = double, 11 = extended 80-bit. used for intermediate results.
         7
         6
         5     exception mask: PM inexact result/precision
         4     exception mask: UM underflow
         3     exception mask: OM overflow
         2     exception mask: ZM zero divide
         1     exception mask: DM denormal operand
         0     excepiton mask: IM invalid operation
    */

    /*
        fp status word
        --------------
        15    B    Busy (currently executing an instruction)
        14    C3   Condition Code. used for comparision and test for branching
        13-11 TOP  Top of stack pointer. points to st(0). same as fp_sp
        10    C2
         9    C1 / ES Either depending on the context. error summary status. If any unmasked exciption bit is set, eS is set.
         8    C0
         7    SF   stack fault flag. indicated fp stack overflow
         6    IF   interrupt request flag set. set if an unmasked exception is pending
         5    PE   precision exception
         4    UE   underflow exception
         3    OE   overflow exception
         2    ZE   zero divide exception
         1    DE   denormal operand exception
         0    IE   invalid operation exception (e.g. stack underflow, inf - inf, 0 * inf, etc.
    */

    void set_x87_status_bit( uint8_t bit, bool val )
    {
        uint16_t mask = ( 1 << bit );
        if ( val )
            x87_fpu_status_word |= mask;
        else
            x87_fpu_status_word &= ~ mask;
    } //set_x87_status_bit

    void set_x87_status_compare( uint32_t fcc );
    void set_x87_status_c0( bool val ) { set_x87_status_bit( 8, val ); }
    void set_x87_status_c1( bool val ) { set_x87_status_bit( 9, val ); }
    void set_x87_status_c2( bool val ) { set_x87_status_bit( 10, val ); }
    void set_x87_status_c3( bool val ) { set_x87_status_bit( 14, val ); }
    void set_x87_status_c320( bool b3, bool b2, bool b0 )
    {
        set_x87_status_c3( b3 );
        set_x87_status_c2( b2 );
        set_x87_status_c0( b0 );
    } //set_x87_status_c320

    void update_x87_status_top()
    {
        uint16_t mask = ( 7 << 11 );
        x87_fpu_status_word &= ~mask;
        x87_fpu_status_word |= ( fp_sp << 11 );
    } //update_x87_status_top

    uint8_t get_x87_rounding_mode() { return ( x87_fpu_control_word >> 10 ) & 3; }

    inline uint64_t lower32_address( uint64_t a )
    {
        if ( mode32 )
            return 0xffffffff & a;
        return a;
    } //lower32_address

    void force_trace_xregs();
    void force_trace_xreg( uint32_t i );
    void trace_xreg( uint32_t i );
    void trace_xregs();
    void trace_fregs();
    void trace_state( void );                  // trace the machine's current status
    void unhandled( void );
};

