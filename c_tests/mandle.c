#include <cstdio>
#include <stdio.h>

template <typename T> void mandle()
{
    const T cx = 0.0458;
    const T cy = 0.08333;
    const T c2 = 2.0;
    const T c4 = 4.0;

    for ( int y = -12; y <= 12; y++ )
    {
        for ( int x = -39; x <= 39; x++ )
        {
            const T ca = x * cx;
            const T cb = y * cy;
            T a = ca;
            T b = cb;
            int i;
            for ( i = 0; i <= 15; i++ )
            {
                T t = a * a - b * b + ca;
                b = c2 * a * b + cb;
                a = t;
                if ( ( a * a + b * b ) > c4 )
                    break;
            }
            if ( i > 15 )
                printf( " " );
            else
            {
                if ( i > 9 )
                    i = i + 7;
                printf( "%c", 48 + i );
            }
        }
        printf("\n");
    }
} //mandle

int main()
{
    mandle<float>();
    mandle<double>();
    mandle<long double>();

    printf( "mandle completed with great success\n" );
    return 0;
} //main
