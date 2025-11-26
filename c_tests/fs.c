#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stddef.h> 

int main( int argc, char** argv )
{
    struct stat buf;
//    printf( "sizeof stat: %lu\n", sizeof( buf ) );
//    printf( " offset of st_dev: %lu, sizeof st_dev: %lu\n", offsetof( struct stat, st_dev ), sizeof( buf.st_dev ) );
//    printf( " offset of st_ino: %lu, sizeof st_ino: %lu\n", offsetof( struct stat, st_ino ), sizeof( buf.st_ino ) );
//    printf( " offset of st_nlink: %lu, sizeof st_nlink: %lu\n", offsetof( struct stat, st_nlink ), sizeof( buf.st_nlink ) );
//    printf( " offset of st_uid: %lu, sizeof st_uid: %lu\n", offsetof( struct stat, st_uid ), sizeof( buf.st_uid ) );
//    printf( " offset of st_gid: %lu, sizeof st_gid: %lu\n", offsetof( struct stat, st_gid ), sizeof( buf.st_gid ) );
//    printf( " offset of st_rdev: %lu, sizeof st_rdev: %lu\n", offsetof( struct stat, st_rdev ), sizeof( buf.st_rdev ) );
//    printf( " offset of st_mode: %lu, sizeof st_mode: %lu\n", offsetof( struct stat, st_mode ), sizeof( buf.st_mode ) );
//    printf( " offset of st_size: %lu, sizeof st_size: %lu\n", offsetof( struct stat, st_size ) , sizeof( buf.st_size ) );
//    printf( " offset of st_blksize: %lu, sizeof st_blksize: %lu\n", offsetof( struct stat, st_blksize ), sizeof( buf.st_blksize ) );
//    printf( " offset of st_atime: %lu\n", offsetof( struct stat, st_atime ) );
    if ( fstat( fileno( stdout ), &buf ) != 0 )
    {
        fprintf( stderr, "error: fstat failed\n" );
        return 1;
    }

    printf( " st_dev: %#lx\n", buf.st_dev );
    printf( " st_ino: %#lx\n", buf.st_ino );
    printf( " mode: %#x\n", buf.st_mode );
    printf( " st_nlink: %#lx\n", buf.st_nlink );
    printf( " st_uid: %#x\n", buf.st_uid ); 
    printf( " st_gid: %#x\n", buf.st_gid );
    printf( " st_rdev: %#lx\n", buf.st_rdev );  
    printf( " st_size: %#lx\n", buf.st_size );
    printf( " st_blksize: %#lx\n", buf.st_blksize );

    printf( "S_IFCHR: %#x\n", S_IFCHR );
    printf( "S_IFREG: %#x\n", S_IFREG );    

    printf( "done\n" );
}