/* diskio.c - ANSI C Disk I/O - for .ISO and simulator disks */

/* Timothe Litt Feb 2016
 *
 *       This is part of ODS2 written by Paul Nankervis,
 *       email address:  Paulnank@au1.ibm.com
 *
 *      ODS2 is distributed freely for all members of the
 *      VMS community to use. However all derived works
 *      must maintain comments in their source to acknowledge
 *      the contibution of the original author.
 */

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#if __FILE_OFFSET_BITS == 64
# undef fseek
# ifdef _WIN32
#  define fseek _fseeki64
#  undef off_t
#  define off_t __int64
# else
#  define fseek fseeko
# endif
#endif

#include "compat.h"
#include "phyio.h"
#include "ssdef.h"

unsigned init_count = 0;
unsigned read_count = 0;
unsigned write_count = 0;

#define MAX_DEVS 64
FILE *handles[1+MAX_DEVS];

void phyio_show(void)
{
    printf("PHYIO_SHOW Initializations: %d Reads: %d Writes: %d\n",
           init_count,read_count,write_count);
}

void phyio_help(FILE *fp ) {
    fprintf( fp, "Specify the file containing a disk image to be mounted.\n" );
    fprintf( fp, "E.g. mount mydisk.iso\n" );
    fprintf( fp, "A drive letter will be assigned to the image.  Use that\n" );
    fprintf( fp, "letter to refer to the VMS volume in subsequent commands\n" );
    return;
}

static char *diskfiles[26];
static char drives[26][4];

/* Deeper code assumes that the device name is terminated by ':' and that this is
 * the physical device.  To keep it happy, map the next free drive letter to this
 * filename, and use it to identify the device.  phyio_init will check the mapping table
 * to get the actual filename to open.
 */

char *diskio_mapfile( const char *filename, int options ) {
    int l, L = -1;
    for( l = 0; l < 26; l++ ) {
        if( diskfiles[l] == NULL && L < 0 ) L = l;
        if( diskfiles[l] && strcmp( diskfiles[l], filename ) == 0 ) {
            printf( "%s is already mounted as drive %s\n", filename, drives[l] );
            return NULL;
        }
    }
    if( L < 0 ) {
        printf( "No free drive letters for %s\n", filename );
        return 0;
    }
    snprintf(drives[L], 3, "%c:", L + 'A');
    drives[L][3] = options;
    free(diskfiles[L]);
    diskfiles[L] = (char *) malloc( (l = strlen(filename) + 1) );
    snprintf( diskfiles[L], l, "%s", filename );

    printf( "%s assigned to %s\n", drives[L], diskfiles[L] );

    return drives[L];
}

int diskio_showdrives( void ) {
    int n = 0;
    int l;
    size_t max;
    int sts = 1;

    max = sizeof( "Filename" ) -1;

    for( l = 0; l < 26; l++ ) {
        if( diskfiles[l] != NULL && strlen(diskfiles[l]) > max )
            max = strlen(diskfiles[l]);
    }
    for( l = 0; l < 26; l++ ) {
        if( diskfiles[l] == NULL )
            continue;

        if( !n++ ) {
            printf( "Drv Sts Filename\n"
                    "--- --- " );
            while( max-- ) fputc(  '-', stdout );
            printf( "\n" );
        }
        printf( " %s %-3s %s\n", drives[l], (drives[l][3] &1)? "RW": "RO", diskfiles[l] );
    }
    if( n == 0 ) {
        printf( "No drives assigned\n" );
        sts = 0;
    }
    return sts;
}

int diskio_unmapdrive( const char *drive ) {
    if( diskfiles[drive[0]-'A'] == NULL ) {
        return 0;
    }
    free(diskfiles[drive[0]-'A']);
    diskfiles[drive[0]-'A'] = NULL;
    return 1;
}

unsigned phyio_init( int devlen,char *devnam,unsigned *handle,struct phyio_info *info ) {
    size_t hi;
    int i;

    UNUSED ( devlen );

    info->status = 0;
    info->sectors = 0;
    info->sectorsize = 0;
    *handle = ~0;

    for( hi = 1; hi < MAX_DEVS && handles[hi] != NULL; hi++ )
        ;

    if( hi >= MAX_DEVS ) {
        return SS$_IVCHAN;
    }

    if( (devnam[0] < 'A' || devnam[0] > 'Z') || devnam[1] != ':' ) {
        printf ( "%s is not a mapped device name\n", devnam );
        return SS$_NOSUCHFILE;
    }

    i = *devnam - 'A';
    if( diskfiles[i] == NULL ) {
        printf ( "%s is not a mapped device name\n", devnam );
        return SS$_NOSUCHFILE;
    }

    handles[hi] = openf ( diskfiles[i], (drives[i][3] & 1)? "rb+": "rb" );
    if( handles[hi] == NULL ) {
        perror( diskfiles[i] );
        return SS$_NOSUCHFILE;
    }

    ++init_count;

    *handle = hi;
    return SS$_NORMAL;
}

unsigned phyio_close(unsigned handle) {
    if( handle == 0 || handle > MAX_DEVS || handles[handle] == NULL ) {
        return SS$_NOIOCHAN;
    }
    fclose ( handles[handle] );
    handles[handle] = NULL;

    return SS$_NORMAL;
}

unsigned phyio_read( unsigned handle,unsigned block,unsigned length,char *buffer ) {
    if( handle == 0 || handle > MAX_DEVS || handles[handle] == NULL ) {
        return SS$_IVCHAN;
    }

    if( fseek( handles[handle], block * 512, SEEK_SET ) == -1 ) {
        perror( "seek" );
        return SS$_DATACHECK;
    }
    if( fread( buffer, 1, length,  handles[handle] ) != length ) {
        perror( "read" );
        return SS$_DATACHECK;
    }

    ++read_count;
    return SS$_NORMAL;
}

unsigned phyio_write( unsigned handle,unsigned block,unsigned length,char *buffer ) {
    if( handle == 0 || handle > MAX_DEVS || handles[handle] == NULL ) {
        return SS$_IVCHAN;
    }

    if( fseek( handles[handle], block * 512, SEEK_SET ) == -1 ) {
        perror( "seek" );
        return SS$_DATACHECK;
    }
    abort();
    if( fwrite( buffer, 1, length,  handles[handle] ) != length ) {
        perror( "write" );
        return SS$_DATACHECK;
    }
    ++write_count;
    return SS$_NORMAL;
}

