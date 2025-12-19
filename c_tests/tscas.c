#include <iostream>
#include <cstring>

// Function to find the first occurrence of a character in a buffer using SCASB
char* find_char_scasb(char* buffer, char char_to_find, size_t size) {
    // We need to set up registers according to the SCASB instruction's requirements:
    // AL    - the value to search for (char_to_find)
    // EDI/RDI - the address of the buffer to scan (buffer)
    // ECX/RCX - the number of elements to scan (size)
    // DF    - the direction flag (must be cleared with 'cld' for forward scan)

    unsigned char result_al = (unsigned char)char_to_find;
    char* result_edi = buffer;
    size_t result_ecx = size;

    __asm__ volatile (
        "cld\n\t"        // Clear direction flag for forward scan
        "repne scasb\n\t" // Repeat while not equal (NE) and ECX != 0
        : "=a" (result_al), "=D" (result_edi), "=c" (result_ecx) // Output operands
        : "0" ((unsigned char)char_to_find), "1" (buffer), "2" (size) // Input operands
        : "memory", "cc" // Clobbered registers and condition codes
    );

    // After repne scasb, if a match was found, ZF is set (zero flag). 
    // The instruction stops *after* the matching element.
    // The post-condition logic to determine if it was found and where is complex with inline asm constraints.
    // A simpler C approach to determine if found:
    if (result_ecx == 0 && (size > 0)) {
        // Did not find it in the given size. The above asm is tricky with results.
    }

    // A more straightforward way is often a simple loop or standard library function.
    // The above is mainly illustrative of how to call the instruction.
    return result_edi == buffer + size ? nullptr : result_edi - 1; // Basic post-processing (might need adjustment)
}

int main() {
    char data[] = "Hello, world!";
    char* found = find_char_scasb(data, 'w', strlen(data));

    if (found) {
        std::cout << "Character found at index: " << (found - data) << std::endl;
    } else {
        std::cout << "Character not found." << std::endl;
    }

    // A standard C++ equivalent using std::find (which might be faster due to modern compiler optimizations using SIMD):
    char* found_std = std::find(data, data + strlen(data), 'w');
    if (found_std != data + strlen(data)) {
        std::cout << "std::find found character at index: " << (found_std - data) << std::endl;
    }
    
    return 0;
}
