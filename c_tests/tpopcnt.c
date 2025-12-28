#include <stdio.h>
#include <stdint.h>

int main() 
{
    uint64_t value = 0xFFFFFFFFFFFFFFFFULL;
    int count = __builtin_popcountll(value);
    printf("Popcount of 0x%llx is %d\n", value, count);

    value = 0x2537188291a0c76dULL;
    count = __builtin_popcountll(value);
    printf("Popcount of 0x%llx is %d\n", value, count);

    value = 0x0101010101010101ULL;
    count = __builtin_popcountll(value);
    printf("Popcount of 0x%llx is %d\n", value, count);

    return 0;
}
