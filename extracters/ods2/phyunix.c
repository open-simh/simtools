/* PHYUNIX.c V2.1    Physical I/O module for Unix */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/*
 *      If the user mounts  cd0   we open up /dev/cd0 for access.
 */

/******************************************************************************/
/*                                                                            */
/* THE DEFINITION OF _FILE_OFFSET_BITS MUST APPEAR BEFORE ANY OTHER INCLUDES  */
/*                                                                            */
/*                        DO NOT MOVE THIS DEFINITION                         */
/*                                                                            */
#ifndef _FILE_OFFSET_BITS       /*                                            */
#define _FILE_OFFSET_BITS 64    /* 64-bit off_t (glibc V2.2.x and later)      */
#endif                          /*                                            */
/*                                                                            */
/******************************************************************************/

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "device.h"
#include "ods2.h"
#include "phyio.h"
#include "ssdef.h"
#include "phyvirt.h"
#include "compat.h"

#ifndef TRUE
#define TRUE ( 0 == 0 )
#endif
#ifndef FALSE
#define FALSE ( 0 != 0 )
#endif

#if defined(__digital__) && defined(__unix__)
#define DEV_PREFIX "/devices/rdisk/"
#else
#ifdef sun
#define DEV_PREFIX "/dev/dsk/"
#else
#define DEV_PREFIX "/dev/"
#endif
#endif

unsigned init_count = 0;
unsigned read_count = 0;
unsigned write_count = 0;

struct devdat {
    char *name;
    unsigned low;
    unsigned high;
};

/*************************************************************** showdevs() */

static int devcmp( const void *d1, const void *d2 ) {
    return strcmp( ((struct devdat *)d1)->name, ((struct devdat *)d2)->name );
}

static void showdevs( void ) {
    DIR *dirp;
    struct dirent *dp;
    size_t i, cpl, n = 0, max = 0;
    struct devdat (*list)[1] = NULL;

    if( (dirp = opendir( DEV_PREFIX )) == NULL )
        return;

    do {
        struct stat stb;
        errno = 0;
        if( (dp = readdir(dirp)) != NULL ) {
            char *fs;
            size_t len;

            len = strlen( dp->d_name );
            fs = malloc( sizeof( DEV_PREFIX ) + len );
            if( fs == NULL ) {
                perror( "malloc" );
                exit( 1 );
            }
            memcpy( fs, DEV_PREFIX, sizeof( DEV_PREFIX ) -1 );
            memcpy( fs + sizeof( DEV_PREFIX ) -1, dp->d_name, len +1 );

            if( stat( fs, &stb ) == -1 ) {
                perror( fs );
                free( fs );
                continue;
            }
            if( S_ISBLK( stb.st_mode ) ) {
                struct devdat (*nl)[1];
                unsigned unit = ~0;
                int nml;
                struct devdat *devp;

                if( sscanf( fs, DEV_PREFIX "%*[^0123456789]%n%u",
                                 &nml, &unit ) == 1 ) {
                    fs[nml] = '\0';
                }
                for( i = 0; i < n; i++ ) {
                    devp = (*list)+i;
                    if( strcmp( devp->name, fs ) == 0 ) {
                        if( unit != ~0U ) {
                            if( unit > devp->high )
                                devp->high = unit;
                            if( unit < devp->low )
                                devp->low = unit;
                        }
                        break;
                    }
                }
                if( i >= n ) {
                    nl = (struct devdat (*)[1]) realloc( list, (n+1) * sizeof( struct devdat ) );
                    if( nl == NULL ) {
                        for( i = 0; i < n; i++ )
                            free( (*list)[i].name );
                        free( list );
                        perror( "malloc" );
                        exit( 1 );
                    }
                    list = nl;
                    (*list)[n].name = fs;
                    (*list)[n].low = unit;
                    (*list)[n++].high = unit;
                }
            }
        }
    } while( dp != NULL );
    if( errno != 0 ) {
        printf( "%%ODS2-W-DIRERR, Error reading %s\n", DEV_PREFIX );
        perror( " - " );
    }
    (void) closedir(dirp);
    if( n == 0 ) {
        printf( "%%ODS2-I-NODEV, No devices found\n" );
        return;
    }

    qsort( list, n, sizeof( struct devdat ), &devcmp );
    for( i = 0; i < n; i++ ) {
        char buf[128];
        int p;
        p = snprintf( buf, sizeof(buf), "%s", (*list)[i].name + sizeof( DEV_PREFIX)-1 );
        if( (*list)[i].low != ~0U ) {
            p+= snprintf( buf+p, sizeof(buf)-p, "%u", (*list)[i].low );
            if( (*list)[i].high != (*list)[i].low )
                p+= snprintf( buf+p, sizeof(buf)-p, "-%u", (*list)[i].high );
        }
        if( p > (int)max )
            max = p;
    }
    cpl = 50 / (max + 1);
    if( cpl == 0 )
        cpl = 1;
    for( i = 0; i < n; i++ ) {
        int p;
        if( (i % cpl) == 0 ) {
            if( i ) putchar( '\n' );
            putchar( ' ' );
        }
        p = printf( " %s", (*list)[i].name + sizeof( DEV_PREFIX)-1 ) -1;
        free( (*list)[i].name );
        if( (*list)[i].low != ~0U ) {
            p += printf( "%u", (*list)[i].low );
            if( (*list)[i].high != (*list)[i].low )
                p += printf( "-%u", (*list)[i].high );
        }
        for( ; p < (int)max; p++ )
            putchar( ' ' );
    }
    printf ( "\n" );
    free( list );

    return;
}

/*************************************************************** phyio_show() */

void phyio_show( showtype_t type ) {
    switch( type ) {
    case SHOW_STATS:
        printf( "PHYIO_SHOW Initializations: %d Reads: %d Writes: %d\n",
                init_count, read_count, write_count );
        return;
    case SHOW_FILE64:
        printf( "\nLarge ODS-2 image files (>2GB) are supported.\n" );
        return;
    case SHOW_DEVICES:
        printf( " Physical devices\n" );
        showdevs();
        return;
    default:
        abort();
    }
}

void phyio_help(FILE *fp ) {
    fprintf( fp, "Specify the device to be mounted as using the format:\n" );
    fprintf( fp, " mount %s%s\n\n", DEV_PREFIX, "device" );

    fprintf( fp, "For example, if you are using " DEV_PREFIX "%s\n", "cdrom0" );
    fprintf( fp, "  ODS2$> mount cdrom0\n\n" );

    fprintf( fp, "Use ODS2-Image to work with disk images such as .ISO or simulator files.\n" );
    fprintf( fp, "Alternatively, you can mount the image on a loop device.\n" );
    return;
}

/*************************************************************** phyio_path() */

char *phyio_path( const char *filnam ) {

    size_t n;
    char *path, resolved_path[PATH_MAX+1];

    if ( filnam == NULL || realpath( filnam, resolved_path ) == NULL ) {
        return NULL;
    }
    n = strlen( resolved_path );
    path = (char *) malloc( n + 1 );
    if ( path != NULL ) {
        memcpy( path, resolved_path, n + 1 );
    }
    return path;
}

/*************************************************************** phyio_init() */

unsigned phyio_init( struct DEV *dev ) {

    size_t n;
    int fd;
    char *device;
    const char *virtual;
    struct stat statbuf;

    init_count++;

    dev->access &= ~MOU_VIRTUAL;
    dev->handle = 0;
    dev->sectors = 0;

    virtual = virt_lookup( dev->devnam );
    if ( virtual == NULL ) {
        n = sizeof( DEV_PREFIX ) + strlen( dev->devnam );
        device = (char *) malloc( n );
        if ( device != NULL ) {
            memcpy( device, DEV_PREFIX, sizeof( DEV_PREFIX ) -1 );
            memcpy( device+sizeof( DEV_PREFIX ) -1, dev->devnam, n +1 - sizeof( DEV_PREFIX ) );
            device[n - 2] = '\0'; /* Remove : from device name */
        }
    } else {
        n = strlen( virtual );
        device = (char *) malloc( n + 1 );
        if ( device != NULL ) {
            memcpy( device, virtual, n + 1 );
            dev->access |= MOU_VIRTUAL;
        }
    }
    if ( device == NULL ) {
        return SS$_INSFMEM;
    }
    stat( device, &statbuf );
    dev->sectors = ( statbuf.st_size + (off_t) 511 ) / (off_t) 512;
    fd = open( device, ( dev->access & MOU_WRITE ) ? O_RDWR : O_RDONLY );
#ifdef DEBUG
    printf( "%d = open( \"%s\", %s )\n", fd, device,
            ( dev->access & MOU_WRITE ) ? "O_RDWR" : "O_RDONLY" );
#endif
    if ( fd < 0 && dev->access & MOU_WRITE ) {
        dev->access &= ~MOU_WRITE;
        fd = open( device, O_RDONLY );
#ifdef DEBUG
        printf( "%d = open( \"%s\", O_RDONLY )\n", fd, device );
#endif
    }
    free( device );
    if ( fd < 0 ) {
#ifdef DEBUG
        perror( "open" );
#endif
        return ( ( dev->access & MOU_VIRTUAL ) ?
                 SS$_NOSUCHFILE : SS$_NOSUCHDEV );
    }
    dev->handle = fd;
    return SS$_NORMAL;
}

/*************************************************************** phyio_done() */

unsigned phyio_done( struct DEV *dev ) {

    close( dev->handle );
    dev->handle = 0;
     if( dev->access & MOU_VIRTUAL )
         virt_device( dev->devnam, NULL );
    return SS$_NORMAL;
}

/*************************************************************** phyio_read() */

unsigned phyio_read( struct DEV *dev, unsigned block, unsigned length,
                     char *buffer ) {

    ssize_t res;
    off_t pos;

#ifdef DEBUG
    printf("Phyio read block: %d into %p (%d bytes)\n",
            block, buffer, length );
#endif

    read_count++;
    pos = (off_t) block * (off_t) 512;
    if ( ( pos = lseek( dev->handle, pos, SEEK_SET ) ) < 0 ) {
        perror( "lseek " );
        printf("lseek failed %" PRIuMAX "u\n",(uintmax_t)pos);
        return SS$_PARITY;
    }
    if ( ( res = read( dev->handle, buffer, length ) ) != length ) {
        perror( "read " );
        printf("read failed %" PRIuMAX "u\n", (uintmax_t)res);
        return SS$_PARITY;
    }
    return SS$_NORMAL;
}

/************************************************************** phyio_write() */

unsigned phyio_write( struct DEV *dev, unsigned block, unsigned length,
                      const char *buffer ) {

    off_t pos;
    ssize_t res;

#ifdef DEBUG
    printf("Phyio write block: %d from %p (%d bytes)\n",
            block, buffer, length );
#endif

    write_count++;
    pos = (off_t) block * (off_t) 512;
    if ( ( pos = lseek( dev->handle, pos, SEEK_SET ) ) < 0 ) {
        perror( "lseek " );
        printf( "lseek failed %" PRIuMAX "u\n", (uintmax_t)pos );
        return SS$_PARITY;
    }
    if ( ( res = write( dev->handle, buffer, length ) ) != length ) {
        perror( "write " );
        printf( "write failed %" PRIuMAX "u\n", (uintmax_t)res );
        return SS$_PARITY;
    }
    return SS$_NORMAL;
}
