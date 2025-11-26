#include <stdint.h>
#include <stdio.h>

int64_t factorial( int64_t n )
{
    if ( 0 == n )
        return 1;

    return n * factorial( n - 1 );
} //factorial

int main()
{
    int64_t n = 15;
    printf( "factorial( %lld ) = %lld\n", n, factorial( n ) );
} //main