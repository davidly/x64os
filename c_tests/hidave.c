#include <stdio.h>
#include <math.h>
#include <float.h>

int main( int argc, char * argv[] )
{
#if 0
    long double ld = 8.9;
    printf( "%Lf\n", ld );
#else    
    printf( "hi dave\n" );
    printf( "%.6f\n", -3.1415927 );
    double d = 20.2 * -1.342;
    printf( "%.6f\n", d );
//    printf( "%.6f\n", nextafter( d, DBL_MAX ) );
    printf( "int 666: %llu\n", 666llu );
    fflush( stdout );
#endif    

    return 37;
}
