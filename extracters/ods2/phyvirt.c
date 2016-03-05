/* Phyvirt.c V2.1 Module to map, remember and find virtual devices */

/* Timothe Litt Feb 2016
 * Incorporates some code from Larry Baker of the USGS.
 */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/* A virtual device is used when normal file I/O is desired, e.g. to
 * a disk image (simulator, .iso).
 *
 * A virtual device is established on the command line by specifying
 * /virtual to the mount command.
 *  If the device name is of the form dev:file, dev: becomes the
 *  virtual device name.  Otherwise, a suitable name is created.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "phyio.h"
#include "ssdef.h"
#include "compat.h"

static struct VDEV {
    struct VDEV *next;
    char *devnam;
    char *path;
} *virt_device_list = NULL;

static int virt_compare( unsigned keylen, const char *keynam, 
                            const char *devnam );
static struct VDEV *virt_insert( const char *devnam, unsigned devsiz,
                                 const char *path );
static void virt_remove( const char *devnam, unsigned devsiz );

static char *autodev( void );

/************************************************************* virt_show() */

void virt_show( const char *devnam ) {

    unsigned devlen, devsiz;
    struct VDEV *vp;

    if ( devnam == NULL ) {
        size_t maxd = sizeof( "Device" ) -1,
               maxp = sizeof( "File" ) -1,
               n;

        if( virt_device_list == NULL ) {
            printf( " No virtual devices are assigned\n" );
            return;
        }
        for ( vp = virt_device_list; vp != NULL; vp = vp->next ) {
            n = strlen( vp->devnam );
            if( n > maxd )
                maxd = n;
            n = strlen( vp->path );
            if( n > maxp )
                maxp =n;
        }
        if( maxp > 64 )
            maxp = 64;

        printf( " Virtual devices\n" );
        printf( "  %-*s %s\n  ", (int)maxd, "Device", "File" );
        for( n = 0; n < maxd; n++ ) putchar( '-' );
        putchar( ' ');
        for( n = 0; n < maxp; n++ ) putchar( '-' );
        putchar( '\n' );

        for ( vp = virt_device_list; vp != NULL; vp = vp->next ) {
            n = strlen( vp->path );
            printf( "  %-*s %.*s\n", (int)maxd, vp->devnam, (int)maxp,
                    (n > 64)? vp->path+(n-64): vp->path );
        }
    } else {
        devlen = strlen( devnam );
        for ( devsiz = 0; devsiz < devlen; devsiz++ ) {
            if ( devnam[devsiz] == ':' ) {
                break;
            }
        }
        if ( devsiz == 0 ) {
            return;
        }
        for ( vp = virt_device_list; vp != NULL; vp = vp->next ) {
            if ( virt_compare( devsiz, (char *) devnam, vp->devnam ) == 0 ) {
                printf( " %s => %s\n", vp->devnam, vp->path );
                return;
            }
        }
    }
}

/********************************************************** virt_compare() */

static int virt_compare( unsigned keylen, const char *keynam,
                            const char *devnam ) {

    register int cmp;

    cmp = 0;
    while ( keylen-- > 0 ) {
        cmp = toupper( *keynam++ ) - toupper( *devnam++ );
        if ( cmp != 0 ) {
            break;
        }
    }
    if ( cmp == 0 && *devnam != '\0' && *devnam != ':' ) {
        cmp = -1;
    }
    return cmp;
}

/*********************************************************** virt_lookup() */

/* virt_lookup() finds virtual devices... */

char *virt_lookup( const char *devnam ) {

    unsigned devlen, devsiz;
    struct VDEV *vp;

    devlen = strlen( devnam );
    for ( devsiz = 0; devsiz < devlen; devsiz++ ) {
        if ( devnam[devsiz] == ':' ) {
            break;
        }
    }
    if ( devsiz == 0 ) {
        return NULL;
    }
    for ( vp = virt_device_list; vp != NULL; vp = vp->next ) {
        if ( virt_compare( devsiz, (char *) devnam, vp->devnam ) == 0 ) {
            return vp->path;
        }
    }
    return NULL;
}

/*********************************************************** virt_device() */

/* virt_device() creates/deletes virtual devices... */

unsigned virt_device( char *devnam, char **vname ) {

    unsigned devlen, devsiz;
    char *path = NULL;

    if( vname != NULL ) {
        char *p;
        struct VDEV **vpp;

        if( (p = strchr( devnam, '=' )) != NULL ) {
            *p = '\0';
            path = p+1;
        } else {
            path = devnam;
            devnam = autodev();
        }
        if( (p = phyio_path( path )) == NULL || strlen( p ) == 0 ) {
            printf( "%%ODS2-E-BADPATH, Invalid path: %s\n", path );
            free( p );
            return SS$_BADPARAM;
        }
        path = p;

        for ( vpp = &virt_device_list; *vpp != NULL; vpp = &(*vpp)->next ) {
            if( strcmp( (*vpp)->path, path ) == 0 ) {
                printf( "%%ODS2-E-MAPPED, %s is in use on virtual drive %s\n", path, (*vpp)->devnam );
                return SS$_DEVMOUNT;
            }
        }
    }
    devlen = strlen( devnam );

    for ( devsiz = 0; devsiz < devlen; devsiz++ ) {
        if ( devnam[devsiz] == ':' ) {
            break;
        }
    }
    if ( devsiz == 0 ) {
        free( path );
        return SS$_BADPARAM;
    }
    virt_remove( devnam, devsiz );
    if( vname != NULL ) {
        struct VDEV *vp;

        vp = virt_insert( devnam, devsiz, path );
        free( path );
        if( vp == NULL )
            return SS$_INSFMEM;

        *vname = vp->devnam;
    }
    return SS$_NORMAL;
}

/*********************************************************** virt_insert() */

static struct VDEV *virt_insert( const char *devnam, unsigned devsiz,
                                 const char *path ) {
    unsigned pathsiz;
    struct VDEV *vp, **vpp, **here;

    here = NULL;
    for ( vpp = &virt_device_list; *vpp != NULL; vpp = &(*vpp)->next ) {
        if ( virt_compare( devsiz, (char *) devnam, (*vpp)->devnam ) < 0 ) {
            here = vpp;
        }
    }
    if ( here == NULL ) {
        here = vpp;
    }
    vp = (struct VDEV *) malloc( sizeof( struct VDEV ) );
    if ( vp == NULL ) {
        return NULL;
    }
    vp->devnam = (char *) malloc( devsiz + 1 );
    vp->path = (char *) malloc( (pathsiz = strlen( path )) + 1 );
    if ( vp->devnam == NULL || vp->path == NULL ) {
        free( vp->devnam );
        free( vp->path );
        free( vp );
        return NULL;
    }
    memcpy( vp->devnam, devnam, devsiz );
    vp->devnam[devsiz] = '\0';
    memcpy( vp->path, path, pathsiz+1 );
    vp->next = *here;
    *here = vp;

    return vp;
}

/*********************************************************** virt_remove() */

static void virt_remove( const char *devnam, unsigned devsiz ) {

    struct VDEV *vp, **vpp;

    for ( vpp = &virt_device_list; *vpp != NULL; vpp = &(*vpp)->next ) {
        if ( virt_compare( devsiz, (char *) devnam, (*vpp)->devnam ) == 0 ) {
            vp = *vpp;
            *vpp = (*vpp)->next;
            free( vp->devnam );
            free( vp->path );
            free( vp );
            return;
        }
    }
}

/************************************************************* autodev() */

char *autodev( void ) {
    static char devnam[64];
    long d = 1;

#ifdef _WIN32
    /* Try for a drive letter that's unmapped &&
     * 1st choice: not a physical device and not A or B
     * 2nd choice: not a physical device and either A or B
     * 3rd choice: generate a 2 or more letter device name.
     */
    int l, L = -1, ffa = -1;

    memcpy( devnam, "A:", 3 );
    for( l = 0; l < 26; l++ ) {
        devnam[0] = 'A' + l;
        if( virt_lookup( devnam ) == NULL && L < 0) {
            char *pname;

            if( (pname = driveFromLetter( devnam )) != NULL ) {
                free( pname );
                continue;
            }
            if( devnam[0] >= 'C' ) {
                return devnam;
            } else {
                if( ffa < 0 )
                    ffa = l;
            }
        }
    }
    if( ffa >= 0 )
        L = ffa;
    if( L >= 0 ) {
        devnam[0] = 'A' + L;
        return devnam;
    }
    d = 27;
#endif

    /* Fabricate a name that's not in use, from the sequence:
     * A .. Z, AA .. AZ, ...
     */

    while( 1 ) {
        long n;
        int i = 0;
        char t[ sizeof(devnam) ];

        n = d;
        do {
            ldiv_t r;
            r = ldiv( --n, 26 );
            t[i++] = (char)r.rem + 'A';
            n = r.quot;
        } while( n != 0 );
        for( n = 0; i > 0; n++, i-- ) {
            devnam[n] = t[i-1];
        }
        devnam[n] ='\0';
        if( virt_lookup( devnam ) == NULL )
            return devnam;
        d++;
    }
}
