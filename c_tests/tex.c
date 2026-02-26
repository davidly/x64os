#include <stdio.h>
#include <exception>

#ifdef WATCOM
#include <new>
#define noexcept
#endif

using namespace std;

#ifdef WATCOM
void my_oom_handler() { throw std::bad_alloc(); }
#endif

class CUnwound
{
    private:
        int x;
    public:
        CUnwound() : x( 44 ) {}
        ~CUnwound() { printf( "I am unwound, x should be 44: %d\n", x ); }
        void set( int val ) { x = val; }
};

struct exceptional : std::exception
{
    const char * what() const noexcept { return "exceptional"; }
};

// without this, the call to operator new is optimized out

#pragma GCC optimize("O0")
#pragma clang optimize off

int main()
{
    printf( "top of tex\n" );
#ifdef WATCOM
    std::set_new_handler(my_oom_handler);
#endif

    try
    {
        CUnwound unwound;
        throw exceptional();
        unwound.set( 33 ); // should never be executed
    }
    catch ( exception & e )
    {
        printf( "caught exception %s\n", e.what() );
    }

    int successful = 0;

    try
    {
        printf( "attempting large allocations\n" );
        for ( size_t i = 0; i < 1000; i++ )
        {
            int volatile * myarray = new int[ 1000000 ];
            if ( myarray )
                successful++;
            else
                printf( "new failed but didn't raise!?!\n" );
            //printf( "allocation %zd succeeded %p\n", i, myarray );
        }
        printf( "large allocations succeeded?!? (%d)\n", successful );
    }
#ifdef WATCOM
    catch ( std::bad_alloc & e )
    {
        printf( "caught exception bad_alloc\n" );
    }
#endif
    catch ( exception & e )
    {
        printf( "caught a standard execption: %s\n", e.what() );
#ifndef WATCOM
        fflush( stdout );
#endif
    }
    catch ( ... )
    {
        printf( "caught generic exception\n" ); 
    }

    printf( "leaving main\n" );
    return 0;
}

