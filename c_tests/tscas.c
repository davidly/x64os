#include <iostream>
#include <cstring>
#include <cstdint>

// Function to find the first occurrence of a character in a buffer using SCASB
char* find_char_scasb(char* buffer, char char_to_find, size_t size) 
{
    // We need to set up registers according to the SCASB instruction's requirements:
    // AL    - the value to search for (char_to_find)
    // EDI/RDI - the address of the buffer to scan (buffer)
    // ECX/RCX - the number of elements to scan (size)
    // DF    - the direction flag (must be cleared with 'cld' for forward scan)

    unsigned char result_al = (unsigned char)char_to_find;
    char* result_edi = buffer;
    size_t result_ecx = size;
    bool is_z_set;

    __asm__ volatile (
        "cld\n\t"        // Clear direction flag for forward scan
        "repne scasb\n\t" // Repeat while not equal (NE) and ECX != 0
        : "=a" (result_al), "=D" (result_edi), "=c" (result_ecx), "=@ccz"(is_z_set) // Output operands
        : "0" ((unsigned char)char_to_find), "1" (buffer), "2" (size) // Input operands
        : "memory", "cc" // Clobbered registers and condition codes
    );

    return is_z_set ? result_edi - 1 : nullptr; // If found, return pointer to found char, else nullptr
} //find_char_scasb

const uint16_t* find_val_16(const uint16_t* arr, uint16_t val, size_t n) 
{
    const uint16_t* result = arr;
    size_t count = n;
    bool is_z_set;

    asm volatile (
        "cld\n\t"             // Clear direction flag (scan forward)
        "repne scasw"         // Repeat SCASW while [EDI] != EAX and ECX > 0
        : "+D"(result), "+c"(count), "=@ccz"(is_z_set)
        : "a"(val)
        : "cc", "memory"
    );

    // If found, 'result' points to the element *after* the match due to auto-increment
    return is_z_set ? (result - 1) : nullptr;
} //find_val_16

const uint32_t* find_val_32(const uint32_t* arr, uint32_t val, size_t n) 
{
    const uint32_t* result = arr;
    size_t count = n;
    bool is_z_set;

    asm volatile (
        "cld\n\t"             // Clear direction flag (scan forward)
        "repne scasl"         // Repeat SCASD while [EDI] != EAX and ECX > 0
        : "+D"(result), "+c"(count), "=@ccz"(is_z_set)
        : "a"(val)
        : "cc", "memory"
    );

    // If found, 'result' points to the element *after* the match due to auto-increment
    return is_z_set ? (result - 1) : nullptr;
} //find_val_32

#if defined( __amd64__ )

const uint64_t* find_val_64(const uint64_t* arr, uint64_t val, size_t n) 
{
    const uint64_t* result = arr;
    size_t count = n;
    bool is_z_set;

    asm volatile (
        "cld\n\t"             // Clear direction flag (scan forward)
        "repne scasq"         // Repeat SCASD while [EDI] != EAX and ECX > 0
        : "+D"(result), "+c"(count), "=@ccz"(is_z_set)
        : "a"(val)
        : "cc", "memory"
    );

    // If found, 'result' points to the element *after* the match due to auto-increment
    return is_z_set ? (result - 1) : nullptr;
} //find_val_64

/**
 * Tests basic LOOP (RCX/ECX decrement only).
 * Increments 'result' until 'count' reaches zero.
 */
uint64_t test_loop(uint64_t count) 
{
    uint64_t result = 0;
    asm volatile (
        "mov %[cnt], %%rcx\n\t"  // Load count into RCX (for x64)
        "1:\n\t"                 // Local label
        "inc %[res]\n\t"         // Increment result
        "loop 1b\n\t"            // Decrement RCX, jump to 1 if RCX != 0
        : [res] "+r" (result)    // Output: result (read/write)
        : [cnt] "r" (count)      // Input: starting count
        : "rcx", "cc"            // Clobbers: RCX and Condition Codes
    );
    return result;
} //test_loop

/**
 * Tests LOOPNE (Loop While Not Equal / Zero Flag is 0).
 * Loops until RCX is 0 OR the Zero Flag is set by 'cmp'.
 */
uint64_t test_loopne(uint64_t count, uint64_t target) 
{
    uint64_t iterations = 0;
    asm volatile (
        "mov %[cnt], %%rcx\n\t"
        "2:\n\t"
        "inc %[iter]\n\t"
        "cmp %[iter], %[tgt]\n\t" // Sets ZF=1 when iterations == target
        "loopne 2b\n\t"           // Loop if RCX != 0 AND ZF == 0
        : [iter] "+&r" (iterations)
        : [cnt] "r" (count), [tgt] "r" (target)
        : "rcx", "cc"
    );
    return iterations;
} //test_loopne

#else // 32-bit version

/**
 * Tests basic LOOP (ECX decrement only).
 * Increments 'result' until 'count' reaches zero.
 */
uint32_t test_loop(uint32_t count) 
{
    uint32_t result = 0;
    asm volatile (
        "mov %[cnt], %%ecx\n\t"  // Load count into ECX
        "1:\n\t"                 // Local label
        "inc %[res]\n\t"         // Increment result
        "loop 1b\n\t"            // Decrement ECX, jump to 1 if ECX != 0
        : [res] "+r" (result)    // Output: result (read/write)
        : [cnt] "r" (count)      // Input: starting count
        : "ecx", "cc"            // Clobbers: RCX and Condition Codes
    );
    return result;
} //test_loop

/**
 * Tests LOOPNE (Loop While Not Equal / Zero Flag is 0).
 * Loops until ECX is 0 OR the Zero Flag is set by 'cmp'.
 */
uint32_t test_loopne(uint32_t count, uint32_t target) 
{
    uint32_t iterations = 0;
    asm volatile (
        "mov %[cnt], %%ecx\n\t"
        "2:\n\t"
        "inc %[iter]\n\t"
        "cmp %[iter], %[tgt]\n\t" // Sets ZF=1 when iterations == target
        "loopne 2b\n\t"           // Loop if ECX != 0 AND ZF == 0
        : [iter] "+r" (iterations)
        : [cnt] "r" (count), [tgt] "r" (target)
        : "ecx", "cc"
    );
    return iterations;
} //test_loopne

#endif

int test_loop_instructions() 
{
    std::cout << "--- x86/x64 LOOP Instruction Tests ---" << std::endl;

    // Test 1: Basic LOOP (Expect 10)
    uint64_t count = 10;
    uint64_t loop_result = test_loop( count );
    std::cout << "LOOP (count " << count << "): " << loop_result << std::endl;
    if ( count != loop_result ) 
    {
        std::cout << "error LOOP test failed!" << std::endl;
        exit( 1 ) ;
    }

    // Test 2: LOOPNE (Exit early due to ZF=1)
    // Starting with 100, but should stop when iterations == 5
    uint64_t early_target = 5;
    uint64_t result = test_loopne(100, early_target);
    std::cout << "LOOPNE (count 100, target " << early_target << "): "  << result << std::endl;
    if ( result != early_target ) 
    {
        std::cout << "error LOOPNE test failed!" << std::endl;
        exit( 1 ) ;
    }

    return 0;
} //test_loop_instructions

int main() 
{
    char data[] = "Hello, world!";
    char* found = find_char_scasb(data, 'w', strlen(data));

    if (found)
        std::cout << "Character found at index: " << (found - data) << std::endl;
    else 
    {
        std::cout << "error Character 'w' not found." << std::endl;
        exit(1);
    }

    found = find_char_scasb(data, 'z', strlen(data));
    if (found)  
    {
        std::cout << "error Character 'z' found at index: " << (found - data) << std::endl;
        exit( 1 ) ;
    }

    uint16_t numbers16[] = {100, 200, 300, 400, 500};
    const uint16_t* num_found16 = find_val_16(numbers16, 500, 5);

    if (num_found16)
        std::cout << "Found16 500 at index: " << (num_found16 - numbers16) << std::endl;
    else 
    {
        std::cout << "error 16-bit Number 500 not found." << std::endl;
        exit(1);
    }

    num_found16 = find_val_16( numbers16, 666, 5);
    if ( num_found16 )
    {
        std::cout << "error Found16 666 at index: " << (num_found16 - numbers16) << std::endl;
        exit( 1 ) ;
    }

    uint32_t numbers32[] = {100, 200, 300, 400, 500};
    const uint32_t* num_found32 = find_val_32(numbers32, 300, 5);

    if (num_found32)
        std::cout << "Found32 300 at index: " << (num_found32 - numbers32) << std::endl;
    else 
    {
        std::cout << "error 32-bit Number 300 not found." << std::endl;
        exit(1);
    }

    num_found32 = find_val_32( numbers32, 666, 5);
    if ( num_found32 )
    {
        std::cout << "error Found32 666 at index: " << (num_found16 - numbers16) << std::endl;
        exit( 1 ) ;
    }

#if defined( __amd64__ )

    uint64_t numbers64[] = {100, 200, 300, 400, 500};
    const uint64_t* num_found64 = find_val_64(numbers64, 400, 5);

    if (num_found64)
        std::cout << "Found64 400 at index: " << (num_found64 - numbers64) << std::endl;
    else 
    {
        std::cout << "error 64-bit Number not found." << std::endl;
        exit(1);
    }

    num_found64 = find_val_64( numbers64, 666, 5);
    if ( num_found64 )
    {
        std::cout << "error Found64 666 at index: " << (num_found16 - numbers16) << std::endl;
        exit( 1 ) ;
    }

#endif    

    test_loop_instructions();

    printf( "tscas completed with great success\n" );
    return 0;
}
