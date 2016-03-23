/* PHYUNIX.c    Physical I/O module for Unix */

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

#if !defined( DEBUG ) && defined( DEBUG_PHYUNIX )
#define DEBUG DEBUG_PHYUNIX
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

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

static unsigned phyio_read( struct DEV *dev, unsigned block, unsigned length,
                            char *buffer );
static unsigned phyio_write( struct DEV *dev, unsigned block, unsigned length,
                             const char *buffer );

/*************************************************************** showdevs() */

static int devcmp( const void *d1, const void *d2 ) {
    return strcmp( ((struct devdat *)d1)->name, ((struct devdat *)d2)->name );
}

static void showdevs( void ) {
    DIR *dirp;
    size_t i, cpl, n = 0, max = 0;
    struct devdat (*list)[1] = NULL;

    errno = 0;
    if( (dirp = opendir( DEV_PREFIX )) != NULL ) {
        do {
            int nml;
            size_t len;
            unsigned unit;
            char *fs;
            struct dirent *dp;
            struct devdat (*nl)[1];
            struct stat stb;

            errno = 0;
            if( (dp = readdir(dirp)) == NULL )
                break;

            len = strlen( dp->d_name );
            fs = malloc( sizeof( DEV_PREFIX ) + len );
            if( fs == NULL )
                break;
            memcpy( fs, DEV_PREFIX, sizeof( DEV_PREFIX ) -1 );
            memcpy( fs + sizeof( DEV_PREFIX ) -1, dp->d_name, len +1 );

            if( stat( fs, &stb ) == -1 ) {
                perror( fs );
                free( fs );
                continue;
            }
            if( !S_ISBLK( stb.st_mode ) ) {
                free( fs );
                continue;
            }

            memmove( fs, fs + sizeof( DEV_PREFIX ) -1, len +1 );
            unit = ~0;
            if( sscanf( fs, "%*[^0123456789]%n%u", &nml, &unit ) == 1 )
                fs[nml] = '\0';
            for( i = 0; i < n; i++ ) {
                if( strcmp( (*list)[i].name, fs ) == 0 ) {
                    if( unit != ~0U ) {
                        if( unit > (*list)[i].high )
                            (*list)[i].high = unit;
                        if( unit < (*list)[i].low )
                            (*list)[i].low = unit;
                    }
                    free( fs );
                    break;
                }
            }
            if( i >= n ) {
                nl = (struct devdat (*)[1])
                    realloc( list, (n+1) * sizeof( struct devdat ) );
                if( nl == NULL ) {
                    free( fs );
                    break;
                }
                list = nl;
                (*list)[n].name = fs;
                (*list)[n].low = unit;
                (*list)[n++].high = unit;
            }
        } while( TRUE );
        (void) closedir(dirp);
    }
    if( errno != 0 ) {
        printf( "%%ODS2-W-DIRERR, Error reading %s\n", DEV_PREFIX );
        perror( " - " );
        for( i = 0; i < n; i++ )
            free( (*list)[i].name );
        free( list );
        return;
    }

    if( n == 0 ) {
        printf( "%%ODS2-I-NODEV, No devices found\n" );
        return;
    }

    qsort( list, n, sizeof( struct devdat ), &devcmp );

    for( i = 0; i < n; i++ ) {
        char buf[128];
        int p;
        p = snprintf( buf, sizeof(buf), "%s", (*list)[i].name );
        if( (*list)[i].low != ~0U ) {
            p += snprintf( buf+p, sizeof(buf)-p, "%u", (*list)[i].low );
            if( (*list)[i].high != (*list)[i].low )
                p += snprintf( buf+p, sizeof(buf)-p, "-%u", (*list)[i].high );
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
        p = printf( " %s", (*list)[i].name ) -1;
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
        printf( "Large ODS-2 image files (>2GB) are %ssupported.\n",
                (sizeof( off_t ) < 8)? "NOT ": "" );
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
    fprintf( fp, " mount device\nThe device must be in %.*s.\n\n",
             (int)(sizeof( DEV_PREFIX ) -2), DEV_PREFIX );
    fprintf( fp, "For example, if you are using " DEV_PREFIX "%s\n", "cdrom0" );
    fprintf( fp, "  ODS2$> mount cdrom0\n" );
    fprintf( fp, "For a list of devices, see SHOW DEVICES\n\n" );
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
    int sts = SS$_NORMAL;
    size_t n;
    int fd, saverr = 0;
    char *device;
    const char *virtual;
    struct stat statbuf;

    init_count++;

    dev->handle = -1;
    dev->devread = phyio_read;
    dev->devwrite = phyio_write;

    if( dev->access & MOU_VIRTUAL ) {
        virtual = virt_lookup( dev->devnam );
        if ( virtual == NULL ) {
            return SS$_NOSUCHDEV;
        }
        n = strlen( virtual );
        device = (char *) malloc( n + 1 );
        if( device == NULL )
            return SS$_INSFMEM;

        memcpy( device, virtual, n + 1 );
    } else {
        n = sizeof( DEV_PREFIX ) + strlen( dev->devnam );
        device = (char *) malloc( n );
        if ( device == NULL )
            return SS$_INSFMEM;

        memcpy( device, DEV_PREFIX, sizeof( DEV_PREFIX ) -1 );
        memcpy( device+sizeof( DEV_PREFIX ) -1, dev->devnam, n +1 - sizeof( DEV_PREFIX ) );
        device[n - 2] = '\0'; /* Remove : from device name */
    }

    fd = open( device,
               ((dev->access & MOU_WRITE )? O_RDWR : O_RDONLY) |
               ((dev->access & (MOU_VIRTUAL|PHY_CREATE)) == (MOU_VIRTUAL|PHY_CREATE)?
                                                            O_CREAT | O_EXCL : 0),
               0644 );
    if( fd < 0 )
        saverr = errno;
#if DEBUG
    printf( "%d = open( \"%s\", %s%s )\n", fd, device,
            (dev->access & MOU_WRITE )? "O_RDWR" : "O_RDONLY",
            (dev->access & PHY_CREATE)? "|O_CREAT|O_EXCL, 0666" : "" );
#endif
    if ( fd < 0 && (dev->access & (MOU_WRITE|PHY_CREATE)) == MOU_WRITE ) {
        dev->access &= ~MOU_WRITE;
        fd = open( device, O_RDONLY );
#if DEBUG
        printf( "%d = open( \"%s\", O_RDONLY )\n", fd, device );
#endif
    }
    dev->handle = fd;

    if( fd >= 0 ) {
        if( fstat( fd, &statbuf ) == 0 ) {
            if( dev->access & MOU_VIRTUAL ) {
                if( !S_ISREG(statbuf.st_mode) ) {
                    printf( "%%ODS2-E-NOTFILE, %s is not a regular file\n", device );
                    close( fd );
                    dev->handle = -1;
                    sts = SS$_IVDEVNAM;
                } else
                    dev->eofptr = statbuf.st_size;
            } else {
                if( !S_ISBLK(statbuf.st_mode ) ) {
                    printf( "%%ODS2-E-NOTFILE, %s is not a block device\n", device );
                    close( fd );
                    dev->handle = -1;
                    sts = SS$_IVDEVNAM;
                }
            }
        } else {
            if( !saverr )
                saverr = errno;
            close( fd );
            dev->handle = -1;
            fd = -1;
        }
    }

    if ( fd < 0 ) {
#if DEBUG
        errno = saverr;
        perror( "open" );
#endif

        errno = saverr;
        switch( errno ) {
        case EEXIST:
            printf( "%%ODS2-E-EXISTS, File %s already exists, not superseded\n", device );
            sts = SS$_DUPFILENAME;
            break;
        default:
            printf( "%%ODS2-E-OPENERR, Open %s: %s\n", device, strerror( errno ) );
            sts =  (dev->access & MOU_VIRTUAL) ? SS$_NOSUCHFILE : SS$_NOSUCHDEV;
        }
    }
    free( device );

    return sts;
}

/*************************************************************** phyio_done() */

unsigned phyio_done( struct DEV *dev ) {

    if( dev->handle != -1 )
        close( dev->handle );
    dev->handle = -1;

    return SS$_NORMAL;
}

/*************************************************************** phyio_read() */

static unsigned phyio_read( struct DEV *dev, unsigned block, unsigned length,
                     char *buffer ) {

    ssize_t res;
    off_t pos;

#if DEBUG
    printf("Phyio read block: %d into %p (%d bytes)\n",
            block, buffer, length );
#endif

    read_count++;
    pos = (off_t) block * (off_t) 512;
    if ( ( pos = lseek( dev->handle, pos, SEEK_SET ) ) < 0 ) {
        perror( "lseek " );
        printf("lseek failed %" PRIuMAX "\n",(uintmax_t)pos);
        return SS$_PARITY;
    }
    if ( ( res = read( dev->handle, buffer, length ) ) != length ) {
        if( res == 0 ) {
            return SS$_ENDOFFILE;
        }
        if( res == (off_t)-1 ) {
            perror( "%%ODS2-F-READERR, read failed" );
        } else {
            printf( "%%ODS2-F-READERR, read failed with bc = %" PRIuMAX " for length %u, lbn %u\n",
                    (uintmax_t)res, length, block );
        }
        return SS$_PARITY;
    }
    return SS$_NORMAL;
}

/************************************************************** phyio_write() */

static unsigned phyio_write( struct DEV *dev, unsigned block, unsigned length,
                      const char *buffer ) {

    off_t pos;
    ssize_t res;

#if DEBUG
    printf("Phyio write block: %d from %p (%d bytes)\n",
            block, buffer, length );
#endif

    write_count++;
    pos = (off_t) block * (off_t) 512;
    if ( ( pos = lseek( dev->handle, pos, SEEK_SET ) ) < 0 ) {
        perror( "lseek " );
        printf( "lseek failed %" PRIuMAX "\n", (uintmax_t)pos );
        return SS$_PARITY;
    }
    if ( ( res = write( dev->handle, buffer, length ) ) != length ) {
        if( res == (off_t)-1 ) {
            perror( "%%ODS2-F-WRITEERR, write failed" );
        } else {
            printf( "%%ODS2-F-WRITEERR, write failed with bc = %" PRIuMAX " for length %u, lbn %u\n",
                    (uintmax_t)res, length, block );
        }
        return SS$_PARITY;
    }
    return SS$_NORMAL;
}
