// g++ -std=c++17 -I . tests/trace_tests.cxx x64.cxx -o /tmp/trace_tests
// /tmp/trace_tests /tmp/trace_tests.log
#include <stdint.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <djl_128.hxx>
#include <djltrace.hxx>
#include "x64.hxx"

CDJLTrace tracer;

void emulator_invoke_svc( x64 & ) { throw std::runtime_error( "unexpected syscall" ); }
const char * emulator_symbol_lookup( uint64_t, uint64_t & offset ) { offset = 0; return ""; }
const char * emulator_symbol_lookup( uint32_t, uint32_t & offset ) { offset = 0; return ""; }
void emulator_hard_termination( x64 &, const char * message, uint64_t ) { throw std::runtime_error( message ); }

struct TraceCase
{
    const char * name;
    std::vector<uint8_t> code;
    const char * expected;
    bool mode32 = false;
    bool at_end = false;
    const char * expected_header = nullptr;
};

static void check_case( const TraceCase & test, const char * log_path )
{
    std::vector<uint8_t> plain_memory( test.at_end ? 16 + test.code.size() : 256, 0 );
    std::copy( test.code.begin(), test.code.end(), plain_memory.begin() + 16 );
    if ( !test.at_end )
        plain_memory[ 16 + test.code.size() ] = 0xf4;
    else
    {
        // RET at the memory boundary returns to HLT without fetching past memory.
        plain_memory[ 0 ] = 8;
        plain_memory[ 8 ] = 0xf4;
    }
    std::vector<uint8_t> traced_memory = plain_memory;
    x64 plain( plain_memory, 0, 16, 0, test.at_end ? 0 : 240 );
    x64 traced( traced_memory, 0, 16, 0, test.at_end ? 0 : 240 );
    plain.Mode32( test.mode32 );
    traced.Mode32( test.mode32 );
    plain.regs[ x64::rsi ].q = traced.regs[ x64::rsi ].q = 128;
    plain.regs[ x64::rdi ].q = traced.regs[ x64::rdi ].q = 144;
    plain.trace_instructions( false );
    uint64_t plain_count = plain.run();
    if ( !tracer.Enable( true, log_path, true ) )
        throw std::runtime_error( "cannot open trace log" );
    traced.trace_instructions( true );
    uint64_t traced_count = traced.run();
    traced.trace_instructions( false );
    tracer.Shutdown();

    if ( plain_count != traced_count || plain.rip != traced.rip || plain.rflags != traced.rflags ||
         memcmp( plain.regs, traced.regs, sizeof( plain.regs ) ) ||
         memcmp( plain.xregs, traced.xregs, sizeof( plain.xregs ) ) || plain_memory != traced_memory )
        throw std::runtime_error( "tracing changed execution" );
    std::ifstream log( log_path );
    std::string output( ( std::istreambuf_iterator<char>( log ) ), std::istreambuf_iterator<char>() );
    size_t instruction = output.find( test.expected );
    if ( instruction == std::string::npos )
        throw std::runtime_error( std::string( "missing trace: " ) + test.expected + "\n" + output );
    size_t address = output.rfind( "rip ", instruction );
    if ( address == std::string::npos || output.compare( address, 12, "rip       10" ) )
        throw std::runtime_error( "incorrect instruction start address\n" + output );
    if ( test.expected_header && output.find( test.expected_header, address ) != address )
        throw std::runtime_error( "incorrect instruction header\n" + output );
}

static int benchmark( const char * log_path, bool mode32 )
{
    std::vector<uint8_t> memory( 256, 0 );
    const uint8_t code[] = { 0x83, 0xe9, 1, 0x75, 0xfb, 0xf4 }; // sub ecx, 1; jnz; hlt
    memcpy( memory.data() + 16, code, sizeof( code ) );
    x64 cpu( memory, 0, 16, 0, 240 );
    cpu.Mode32( mode32 );
    for ( unsigned i = 0; i < 16; i++ )
        cpu.regs[ i ].q = mode32 ? 0x12345678u + i : 0x123456789abcdef0ull + i;
    cpu.regs[ x64::rsp ].q = 240;
    cpu.regs[ x64::rcx ].q = 100000;
    if ( !tracer.Enable( true, log_path, true ) )
        return 1;
    cpu.trace_instructions( true );
    auto start = std::chrono::steady_clock::now();
    cpu.run();
    tracer.Shutdown();
    auto elapsed = std::chrono::steady_clock::now() - start;
    std::cout << ( mode32 ? 32 : 64 ) << "-bit: "
              << std::chrono::duration<double>( elapsed ).count() << " seconds\n";
    return 0;
}

int main( int argc, char ** argv )
{
    tracer.SetQuiet( true );
    if ( argc == 4 && !strcmp( argv[ 1 ], "--benchmark" ) )
        return benchmark( argv[ 2 ], !strcmp( argv[ 3 ], "32" ) );
    if ( argc != 2 )
    {
        std::cerr << "usage: trace_tests log_path | --benchmark log_path 32|64\n";
        return 2;
    }
    const TraceCase cases[] = {
        { "cmc", { 0xf5 }, "=> cmc\n" },
        { "last byte", { 0xc3 }, "=> ret\n", false, true,
          "rip       10  c3             rsi:80 rdi:90 cpazsdo => " },
        { "last two bytes", { 0x90, 0xc3 }, "=> nop\n", false, true,
          "rip       10  90 c3          rsi:80 rdi:90 cpazsdo => " },
        { "last three bytes", { 0x90, 0x90, 0xc3 }, "=> nop\n", false, true,
          "rip       10  90 90 c3       rsi:80 rdi:90 cpazsdo => " },
        { "last four bytes", { 0x90, 0x90, 0x90, 0xc3 }, "=> nop\n", false, true,
          "rip       10  90 90 90 c3    rsi:80 rdi:90 cpazsdo => " },
        { "last byte 32-bit", { 0xc3 }, "=> ret\n", true, true,
          "rip       10  c3             esi:80 edi:90 cpazsdo => " },
        { "repeated prefix address", { 0x66, 0x66, 0x90 }, "=> nop\n" },
        { "segment prefix address", { 0x64, 0x66, 0x90 }, "=> nop\n" },
        { "lock prefix address", { 0xf0, 0x83, 0x06, 1 }, "=> addd [ rsi ], 0x1\n" },
        { "byte registers", { 0x8a, 0xc4 }, "=> mov al, ah\n" },
        { "REX byte registers", { 0x40, 0x8a, 0xc4 }, "=> mov al, spl\n" },
        { "movsxd source", { 0x48, 0x63, 0xc1 }, "=> movsxd rax, ecx\n" },
        { "movsxd word", { 0x66, 0x63, 0xc1 }, "=> movsxd ax, cx\n" },
        { "movzx word destination", { 0x66, 0x0f, 0xb6, 0xc1 }, "=> movzxb ax, cl\n" },
        { "cmov word", { 0x66, 0x0f, 0x44, 0xc1 }, "=> cmove ax, cx\n" },
        { "cmov dword", { 0x0f, 0x44, 0xc1 }, "=> cmove eax, ecx\n" },
        { "cmov qword", { 0x48, 0x0f, 0x44, 0xc1 }, "=> cmove rax, rcx\n" },
        { "push word", { 0x66, 0x50 }, "=> push ax\n" },
        { "pop word", { 0x66, 0x58 }, "=> pop ax\n" },
        { "push word 32-bit", { 0x66, 0x50 }, "=> push ax\n", true },
        { "negative word immediate", { 0x66, 0x83, 0xc0, 0xff }, "=> addw ax, 0xffff\n" },
        { "lea dword", { 0x8d, 0x06 }, "=> lea eax, [ rsi ]\n" },
        { "lea word", { 0x66, 0x8d, 0x06 }, "=> lea ax, [ rsi ]\n" },
        { "cmps qword", { 0x48, 0xa7 }, "=> cmpsq\n" },
        { "mov immediate", { 0xc7, 0xc0, 0x34, 0x12, 0, 0 }, "=> movd eax, 0x1234\n" },
        { "jrcxz", { 0xe3, 0 }, "=> jrcxz 0\n" },
        { "jrcxz operand prefix", { 0x66, 0xe3, 0 }, "=> jrcxz 0\n" },
        { "jecxz address prefix", { 0x67, 0xe3, 0 }, "=> jecxz 0\n" },
        { "jecxz 32-bit", { 0xe3, 0 }, "=> jecxz 0\n", true },
        { "jcxz 32-bit", { 0x67, 0xe3, 0 }, "=> jcxz 0\n", true },
        { "movmskps", { 0x0f, 0x50, 0xc1 }, "=> movmskps eax, xmm1\n" },
        { "movhlps source", { 0x0f, 0x12, 0xc1 }, "=> movhlps xmm0, xmm1\n" },
        { "movhps memory", { 0x0f, 0x16, 0x06 }, "=> movhps xmm0, [ rsi ]\n" },
        { "movmskpd", { 0x66, 0x0f, 0x50, 0xc1 }, "=> movmskpd eax, xmm1\n" },
        { "movdqu", { 0xf3, 0x0f, 0x6f, 0xc1 }, "=> movdqu xmm0, xmm1\n" },
        { "pcmpeqd destination", { 0x66, 0x0f, 0x76, 0xc1 }, "=> pcmpeqd xmm0, xmm1\n" },
        { "movd from xmm", { 0x66, 0x0f, 0x7e, 0xc0 }, "=> movd eax, xmm0\n" },
        { "movq from xmm", { 0x66, 0x48, 0x0f, 0x7e, 0xc0 }, "=> movq rax, xmm0\n" },
        { "paddb", { 0x66, 0x0f, 0xfc, 0xc1 }, "=> paddb xmm0, xmm1\n" },
        { "paddw", { 0x66, 0x0f, 0xfd, 0xc1 }, "=> paddw xmm0, xmm1\n" },
    };
    unsigned failures = 0;
    for ( const TraceCase & test : cases )
    {
        try { check_case( test, argv[ 1 ] ); }
        catch ( const std::exception & e )
        {
            tracer.Shutdown();
            std::cerr << test.name << ": " << e.what() << '\n';
            failures++;
        }
    }
    std::cout << sizeof( cases ) / sizeof( cases[ 0 ] ) - failures << "/"
              << sizeof( cases ) / sizeof( cases[ 0 ] ) << " trace tests passed\n";
    return failures ? 1 : 0;
}
