#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdlib>
#include <cstring>
#include <string.h>
#include <cerrno>

#ifdef WATCOM
    #define MREMAP_MAYMOVE 1
    
    /* mremap_ow.c - Open Watcom Linux i386 */
    
    #include <stdarg.h>
    #include <sys/mman.h>
    #include <errno.h>
    
    /* Linux i386 syscall number */
    #ifndef SYS_mremap
    #define SYS_mremap 163
    #endif
    
    #ifndef MREMAP_MAYMOVE
    #define MREMAP_MAYMOVE 1
    #endif
    
    #ifndef MREMAP_FIXED
    #define MREMAP_FIXED   2
    #endif
    
    /* Optional, only on newer Linux kernels */
    #ifndef MREMAP_DONTUNMAP
    #define MREMAP_DONTUNMAP 4
    #endif
    
    /* Raw 5-arg Linux i386 syscall via int 0x80 */
    static long linux_syscall5(long nr, long a, long b, long c, long d, long e);
    #pragma aux linux_syscall5 =        \
        "int 0x80"                      \
        __parm [__eax] [__ebx] [__ecx] [__edx] [__esi] [__edi] \
        __value [__eax]                 \
        __modify [__eax]
    
    void *mremap(void *old_address, size_t old_size,
                 size_t new_size, int flags, ...)
    {
        void *new_address = (void *)0;
        long rc;
        va_list ap;
    
        if (flags & MREMAP_FIXED) {
            va_start(ap, flags);
            new_address = va_arg(ap, void *);
            va_end(ap);
        }
    
        rc = linux_syscall5(
            SYS_mremap,
            (long)old_address,
            (long)old_size,
            (long)new_size,
            (long)flags,
            (long)new_address
        );
    
        /* Linux syscalls return -errno in range -1..-4095 on failure */
        if ((unsigned long)rc >= (unsigned long)-4095L) {
            errno = -(int)rc;
            return MAP_FAILED;
        }
    
        return (void *)rc;
    }
#endif // WATACOM

void validate( void * amaps[], size_t i, size_t size )
{
    uint8_t * p = (uint8_t *) amaps[ i ];
    uint8_t c = (uint8_t) ( i + 'a' );
    for ( size_t x = 0; x < size; x++ )
    {
        if ( p[x] != c )
        {
            printf( "buffer %p number %d size %d doesn't have value %c at offset %d -- it has integer %d\n", 
                    p, (int) i, (int) size, c, (int) x, (int) p[x] );
            exit( 1 );
        }
    }
} //validate

int main( int argc, char * argv[] )
{
    const size_t cmaps = 16; //32; // 64;
    void * amaps[ cmaps ];
    bool verbose = ( argc > 1 );

    printf( "MAP_PRIVATE: %#x\n", MAP_PRIVATE );
    printf( "MAP_ANONYMOUS: %#x\n", MAP_ANONYMOUS );
    printf( "MREMAP_MAYMOVE: %#x\n", MREMAP_MAYMOVE );
    printf( "PROT_READ: %#x\n", PROT_READ );
    printf( "PROT_WRITE: %#x\n", PROT_WRITE );

    for ( size_t i = 0; i < cmaps; i++ )
        amaps[ i ] = 0;

    for ( size_t i = 0; i < cmaps; i++ )
    {
        size_t size = ( i + 1 ) * 4096;
        void * p = mmap( 0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
        if ( (void *) -1 == p )
        {
            printf( "unable to mmap %zu bytes, error %d = %s", size, errno, strerror( errno ) );
            exit( 1 );
        }
        if ( verbose )
            printf( "mapped entry %d size %d as %p\n", (int) i, (int) size, p );
        memset( p, i + 'a', size );

        amaps[ i ] = p;
    }

    // free the even entries

    for ( size_t i = 0; i < cmaps; i += 2 )
    {
        size_t size = ( i + 1 ) * 4096;
        validate( amaps, i, size );
        int result = munmap( amaps[ i ], size );
        if ( -1 == result )
        {
            printf( "failed to unmap i %zu, error %d = %s\n", i, errno, strerror( errno ) );
            exit( 1 );
        }
        if ( verbose )
            printf( "unmapped size %d as %p\n", (int) size, amaps[ i ] );
        amaps[ i ] = 0;
    }

    // reallocate the odd entries to be twice or four times as large as they were

    for ( size_t i = 1; i < cmaps; i += 2 )
    {
        size_t size = ( i + 1 ) * 4096;
        validate( amaps, i, size );
        size_t new_size = ( i & 2 ) ? 2 * size : 4 * size;
        void * p = mremap( amaps[ i ], size, new_size, MREMAP_MAYMOVE );
        if ( (void *) -1 == p )
        {
            printf( "unable to mremap %d bytes, error %d = %s\n", (int) size, errno, strerror( errno ) );
            exit( 1 );
        }
        if ( verbose )
            printf( "remapped entry %d from size %d to size %d as %p\n", (int) i, (int) size, (int) new_size, p );
        memset( ( (uint8_t *) p ) + size, i + 'a', new_size - size ); // just initialize the new portion
        amaps[ i ] = p;
    }

    // allocate even entries as 8k each

    for ( size_t i = 0; i < cmaps; i += 2 )
    {
        size_t size = 8192;
        void * p = mmap( 0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
        if ( (void *) -1 == p )
        {
            printf( "pass two unable to mmap %zu bytes, error %d = %s\n", size, errno, strerror( errno ) );
            exit( 1 );
        }
        if ( verbose )
          printf( "mapped entry %d size %d as %p\n", (int) i, (int) size, p );
        memset( p, i + 'a', size );
        amaps[ i ] = p;
    }

    // free all entries

    for ( size_t i = 0; i < cmaps; i++ )
    {
        size_t size = ( i + 1 ) * 4096;
        size = ( i & 1 ) ? ( i & 2 ) ? 2 * size : 4 * size : 8192;
        validate( amaps, i, size );
        int result = munmap( amaps[ i ], size );
        if ( -1 == result )
        {
            printf( "failed to unmap i %zu, error %d = %s\n", i, errno, strerror( errno ) );
            exit( 1 );
        }
        if ( verbose )
            printf( "unmapped entry %d size %d as %p\n", (int) i, (int) size, amaps[ i ] );
        amaps[ i ] = 0;
    }

    printf( "mmap test completed with great success\n" );
    return 0;
} //main
