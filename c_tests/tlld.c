#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define _USE_MATH_DEFINES 
#include <math.h>
#include <float.h>
#include <stdint.h>

// less than full precision because libc only provides this much precision in trig functions

#define TRIG_FLT_EPSILON 0.00002  /* 0.00000011920928955078 */
#define TRIG_DBL_EPSILON 0.00000002 /* 0.00000000000000022204 */
#define TRIG_LDBL_EPSILON 0.0000000000000002 /* 0.0000000000000000000000000000000001925930 */

void check_same_ld( const char * operation, long double a, long double b, long double dbgval )
{
    long double diff = a - b;
    long double abs_diff = fabsl( diff );

    // the x64os emulator uses double precision for long double, so using DBL_EPSILON here
    bool eq = ( abs_diff <= TRIG_DBL_EPSILON );

    if ( !eq )
    {
        printf( "operation %s: long double %.20Lf is not the same as long double %.20Lf\n", operation, a, b );
        printf( "  original value: %.20Lf\n", dbgval );
        exit( 0 );
    }
} //check_same_ld

long double square_root_ld( long double num ) 
{
    long double x = num; 
    long double y = 1;

    // the x64os emulator uses double precision for long double, so using DBL_EPSILON here
    const long double e =  10.0f * DBL_EPSILON; // LDBL_EPSILON;

    while ( ( x - y ) > e ) 
    {
        x = ( x + y ) / 2;
        y = num / x;
    }
    return x;
} //square_root_ld

__int128 factorial( __int128 n )
{
    if ( 0 == n )
        return 1;

    return n * factorial( n - 1 );
} //factorial

long double my_sin_ld( long double x, int n = 18 )
{
    long double result = 0;
    int sign = 1; // can't use an __int128 here because gnu generates code that requires 10-byte long doubles, which don't exist in msvc.

    for ( __int128_t i = 1; i <= n; i++ ) 
    {
        long double powval = powl( x, ( 2 * i - 1 ) );
        //printf( "i: %d, powval( %.20Lf, %lld ): %.20Lf\n", (long long) i, x, ( 2 * i - 1 ), powval );
        long double factval = factorial( 2 * i - 1 );
        //result += sign * powl( x, ( 2 * i - 1 ) ) / factorial( 2 * i - 1 );
        long double delta = sign * powval / factval;
        result += delta;
        printf( "i %u  factval: %.20Lf, delta %.20Lf\n", (int) i, factval, delta );
        printf( "  result: %.20Lf, i: %d, powval( %.20Lf, %lld ): %.20Lf\n", result, (long long) i, x, ( 2 * i - 1 ), powval );
        sign *= -1;

        if ( i > 3 )
        exit( 0 );
    }

    return result;
} //my_sin_ld

extern "C" int main()
{
    long double ldresult, ldback;
    float fresult, fback;
    float f = 0.01 - ( M_PI / 2 ); // want to be >= negative half pi.
   // f = 0.78920155763626098633;
    while( f < -1.55 ) //( M_PI / 2 ) )
    {
        ldresult = sinl( (long double) f );
        ldback = my_sin_ld( (long double) f );
        printf( "f: %.20f, sinl: %.20Lf, my_sinl: %.20Lf\n", f, ldresult, ldback );
        check_same_ld( "sinl vs my_sinl", ldresult, ldback, f );

        f += .01f;
    }

    printf( "test tlld completed with great success\n" );
    exit( 0 );
} //main
