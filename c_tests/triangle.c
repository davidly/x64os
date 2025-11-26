// with gcc -O1, actual recursion is used and this tests sparc register windowing
// with gcc -O2, optimizations are used and this does not test sparc register windowing

#include <stdint.h>
#include <stdio.h>

int64_t triangle( int64_t n )
{
    if ( 0 == n )
        return 0;

    return n + triangle( n - 1 );
} //triangle

int main()
{
    int64_t n = 200;
    printf( "triangle( %lld ) = %lld\n", n, triangle( n ) );
} //main