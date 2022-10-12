/* Phyvirt.c Module to map, remember and find virtual devices */

/* Timothe Litt Feb 2016
 * Incorporates some code from Larry Baker of the USGS.
 */

/* Timothe Litt March 2016
 * Copyright (C) 2016 Timothe litt
 *   litt at acm dot org
 *
 * Free for use with the ODS2 package.  All other rights reserved.
 */

/*
 *       This is distributed as part of ODS2, originally  written by
 *       Paul Nankervis, email address:  Paulnank@au1.ibm.com
 *
 *       ODS2 is distributed freely for all members of the
 *       VMS community to use. However all derived works
 *       must maintain comments in their source to acknowledge
 *       the contributions of the original author and
 *       subsequent contributors.   This is free software; no
 *       warranty is offered,  and while we believe it to be useful,
 *       you use it at your own risk.
 */

/* A virtual device is used when normal file I/O is desired, e.g. to
 * a disk image (simulator, .iso).
 *
 * A virtual device is established on the command line by specifying
 * /image to the mount or initialize command, or by defaulting.
 *  If the device name is of the form dev:file, dev: becomes the
 *  virtual device name.  Otherwise, a suitable name is created.
 *
 * This module also virtualizes all I/O, living between the filesystem
 * and the platform-specific read/write LBN layer.  This enables
 * device specific transformations, such as interleave and skew.
 *
 * It also provides an access layer for VHD format image files, on
 * platforms where libvhd is available.
 */

#if !defined( DEBUG ) && defined( DEBUG_PHYVIRT )
#define DEBUG DEBUG_PHYVIRT
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "device.h"
#include "ods2.h"
#include "phyio.h"
#include "phyvirt.h"
#include "ssdef.h"
#include "sysmsg.h"

static struct VDEV {
    struct VDEV *next;
    char *devnam;
    char *path;
    struct DEV *dev;
} *virt_device_list = NULL;

#ifdef USE_VHD
#include "phyvhd.h"
#endif

static int virt_compare( uint32_t keylen, const char *keynam,
                         const char *devnam );
static struct VDEV *virt_insert( const char *devnam, uint32_t devsiz,
                                 const char *path );
static void virt_remove( const char *devnam, uint32_t devsiz );
static vmscond_t virt_makedev( char *devnam, char **vname );
static char *autodev( void );
static vmscond_t virt_assign( char *devnam, struct DEV *dev );
static vmscond_t maplbn( uint32_t lbn, disktypep_t dp, uint32_t sect );
static size_t get_devsiz( const char *devnam );


/************************************************************* virt_show() */

void virt_show( void ) {
    struct VDEV *vp;
    size_t maxd = sizeof( "Device" ) -1,
        maxp = sizeof( "File" ) -1,
        n;

    if( virt_device_list == NULL ) {
        printmsg( IO_VNODEVS, MSG_TEXT );
        return;
    }
    for( vp = virt_device_list; vp != NULL; vp = vp->next ) {
        n = strlen( vp->devnam );
        if( n > maxd )
            maxd = n;
        n = strlen( vp->path );
        if( n > maxp )
            maxp =n;
    }
    if( maxp > 64 )
        maxp = 64;

    printmsg( IO_VDEVHEADER, MSG_TEXT );
    printf( "  %-*s %s\n  ", (int)maxd, "Device", "File" );
    for( n = 0; n < maxd; n++ ) putchar( '-' );
    putchar( ' ');
    for( n = 0; n < maxp; n++ ) putchar( '-' );
    putchar( '\n' );

    for( vp = virt_device_list; vp != NULL; vp = vp->next ) {
        n = strlen( vp->path );
        printf( "  %-*s %-*.*s", (int)maxd, vp->devnam, (int)maxp,
                (int)maxp, (n > maxp)? vp->path+(n-maxp): vp->path );
#ifdef USE_VHD
        if( vp->dev->access & PHY_VHDDSK )
            phyvhd_show( vp->dev, 2 + maxd + 1 + maxp + 1 );
#endif
        putchar( '\n' );
    }

    return;
}

/********************************************************** virt_compare() */

static int virt_compare( uint32_t keylen, const char *keynam,
                         const char *devnam ) {
    register int cmp;

    cmp = 0;
    while( keylen-- > 0 ) {
        cmp = toupper( *keynam++ ) - toupper( *devnam++ );
        if( cmp != 0 ) {
            break;
        }
    }
    if( cmp == 0 && *devnam != '\0' && *devnam != ':' ) {
        cmp = -1;
    }
    return cmp;
}

/*********************************************************** virt_lookup() */

/* virt_lookup() finds virtual devices... */

char *virt_lookup( const char *devnam ) {
    uint32_t devsiz;
    struct VDEV *vp;

    devsiz = (uint32_t)get_devsiz( devnam );
    if( devsiz == 0 ) {
        return NULL;
    }
    for( vp = virt_device_list; vp != NULL; vp = vp->next ) {
        if( virt_compare( devsiz, (char *) devnam, vp->devnam ) == 0 ) {
            return vp->path;
        }
    }
    return NULL;
}

/*********************************************************** virt_insert() */

static struct VDEV *virt_insert( const char *devnam, uint32_t devsiz,
                                 const char *path ) {
    size_t pathsiz;
    struct VDEV *vp, **vpp, **here;

    here = NULL;
    for( vpp = &virt_device_list; *vpp != NULL; vpp = &(*vpp)->next ) {
        if( virt_compare( devsiz, (char *) devnam, (*vpp)->devnam ) < 0 ) {
            here = vpp;
        }
    }
    if( here == NULL ) {
        here = vpp;
    }
    vp = (struct VDEV *) malloc( sizeof( struct VDEV ) );
    if( vp == NULL ) {
        return NULL;
    }
    vp->devnam = (char *) malloc( (size_t)devsiz + 1 );

    vp->path = (char *) malloc( (pathsiz = strlen( path )) + 1 );

    if( vp->devnam == NULL || vp->path == NULL ) {
        free( vp->devnam );
        free( vp->path );
        free( vp );
        return NULL;
    }
    memcpy( vp->devnam, devnam, devsiz );
    vp->devnam[devsiz] = '\0';
    memcpy( vp->path, path, pathsiz+1 );
    vp->dev = NULL;
    vp->next = *here;
    *here = vp;

    return vp;
}

/*********************************************************** virt_remove() */

static void virt_remove( const char *devnam, uint32_t devsiz ) {

    struct VDEV *vp, **vpp;

    for( vpp = &virt_device_list; *vpp != NULL; vpp = &(*vpp)->next ) {
        if( virt_compare( devsiz, (char *) devnam, (*vpp)->devnam ) == 0 ) {
            vp = *vpp;
            *vpp = (*vpp)->next;
            free( vp->devnam );
            free( vp->path );
            free( vp );
            return;
        }
    }
}

/*********************************************************** virt_open() */
/* Wrapper for phyio that handles the mechanics of virtual
 * devices.  Calls phyio_init with everything straightend out.
 */

vmscond_t virt_open( char **devname, uint32_t flags, struct DEV **dev ) {
    vmscond_t sts;

    flags &= ~PHY_VHDDSK;

    if( flags & MOU_VIRTUAL ) {
        char *p;
        if( (p = strrchr( *devname, '.')) != NULL && ( !strcmp( p, ".vhd" ) ||
                                                       !strcmp( p, ".VHD" ) ) ) {
#ifdef USE_VHD
            flags |= PHY_VHDDSK;
#else
            return printmsg( IO_NOVHD, 0 );
#endif
        }
        if( $FAILS( sts = virt_makedev( *devname, devname )) )
            return sts;
    } else {
        if( virt_lookup( *devname ) != NULL ) {
            int devsiz;

            devsiz = (int)get_devsiz( *devname );
            return printmsg( IO_ISVIRTDEV, 0, devsiz, *devname );
        }
    }
    if( $FAILS(sts = device_lookup( strlen( *devname ), *devname, TRUE, dev )) ) {
        if( flags & MOU_VIRTUAL )
            virt_remove( *devname, (uint32_t)get_devsiz( *devname ) );
        return sts;
    }
    (*dev)->access = flags;                          /* Requested mount options */

    (*dev)->disktype = disktype+((flags & MOU_DEVTYPE) >> MOU_V_DEVTYPE);

    if( flags & PHY_VHDDSK ) {
#ifdef USE_VHD
        sts = phyvhd_init( *dev );
#else
        device_done( *dev );
        return printmsg( IO_NOVHD, 0 );
#endif
    } else {
        sts = phyio_init( *dev );
    }

    if( (flags & MOU_VIRTUAL) ) {
        if( $SUCCESSFUL(sts) ) {
            if( $FAILS(sts = virt_assign( *devname, *dev )) ) {
#ifdef USE_VHD
                if( flags & PHY_VHDDSK )
                    phyvhd_close( *dev );
                else
#endif
                    phyio_done( *dev );
                device_done( *dev );
            }
        } else {
            virt_remove( *devname, (uint32_t)get_devsiz( *devname ) );
            device_done( *dev );
        }
    }
    return sts;
}

/************************************************************* virt_virt_makedev() */

/* First part of device creation:
 * assign a virtual device to whatever path we have.
 * We can't resolve the path or see if it's a duplicate because the
 * file might not exist yet. (initialize)
 */

static vmscond_t virt_makedev( char *devnam, char **vname ) {
    struct VDEV *vp;
    uint32_t devsiz;
    char *path = NULL;
    char *p;

    if( (p = strchr( devnam, '=' )) != NULL ) {
        *p = '\0';
        path = ++p;
        if( (p = virt_lookup( devnam)) != NULL ) {
            return printmsg( IO_VIRTDEVINUSE, 0, devnam, p );
        }
    } else {
        path = devnam;
        devnam = autodev();
    }

    devsiz = (uint32_t)get_devsiz( devnam );
    if( devsiz == 0 ) {
        return SS$_BADPARAM;
    }

    vp = virt_insert( devnam, devsiz, path );
    if( vp == NULL )
        return SS$_INSFMEM;

    *vname = vp->devnam;

    return SS$_NORMAL;
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

        memset( t, 0, sizeof(t) );
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

/************************************************************* virt_assign() */
/* Part two:
 * Resolve the pathname and check for duplicate use.
 * Delete the device if we have issues.  Otherwise, resolve
 * and record the full path.
 *
 * virt_open will close the physical device if we have problems.
 */

static vmscond_t virt_assign( char *devnam, struct DEV *dev ) {
    vmscond_t sts;
    uint32_t devsiz;
    char *p;
    struct VDEV *vp, **vpp;

    devsiz = (uint32_t)get_devsiz( devnam );
    if( devsiz == 0 ) {
        return SS$_BADPARAM;
    }

    for( vp = virt_device_list; vp != NULL; vp = vp->next ) {
        if( virt_compare( devsiz, (char *) devnam, vp->devnam ) == 0 )
            break;
    }
    if( vp == NULL )
        return SS$_BADPARAM;

    if( (p = get_realpath( vp->path )) == NULL || strlen( p ) == 0 ) {
        sts = printmsg( IO_BADPATH, 0, vp->path );
        free( p );
        virt_remove( devnam, devsiz );
        return sts;
    }

    for( vpp = &virt_device_list; *vpp != NULL; vpp = &(*vpp)->next ) {
        if( (*vpp != vp) && strcmp( (*vpp)->path, p ) == 0 ) {
            sts = printmsg( IO_VIRTFILEINUSE, 0, p, (*vpp)->devnam );
            free( p );
            virt_remove( devnam, devsiz );
            return sts;
        }
    }

    free( vp->path );
    vp->path = p;
    vp->dev = dev;

    return SS$_NORMAL;
}

/*********************************************************** virt_close() */

/* Destroy a (virtual) device.
 * Wraps phyio_done;
 */

vmscond_t virt_close( struct DEV *dev ) {
    uint32_t devsiz;
    vmscond_t sts;

    cache_flush();

    if( dev->access & MOU_VIRTUAL ) {
        devsiz = (uint32_t)get_devsiz( dev->devnam );
        virt_remove( dev->devnam, devsiz );
    }
#ifdef USE_VHD
    if( dev->access & PHY_VHDDSK )
        sts = phyvhd_close( dev );
    else
#endif
        sts = phyio_done( dev );
    device_done( dev );
    return sts;
}

/*********************************************************** maplbn() */
/* Map an LBN as viewed by the OS to each of its physical sectors.
 */

static vmscond_t maplbn( uint32_t lbn, disktypep_t dp, uint32_t sect ) {
    uint32_t c, t, s;
    ldiv_t r;

    lbn = (lbn * (512 / dp->sectorsize)) + sect;

    r = ldiv( lbn, dp->sectors * dp->tracks );
    c = r.quot;
    r = ldiv( r.rem, dp->sectors );
    t = r.quot;
    s = r.rem;

    if( s >= dp->sectors / dp->interleave )
        s = (s * dp->interleave) - (dp->sectors -1);
    else
        s = s * dp->interleave;

    s = (s + (dp->skew * c)) % dp->sectors;

    return (c * dp->sectors * dp->tracks) + (t * dp->sectors) + dp->reserved + s;
}

/*********************************************************** virt_read() */

vmscond_t virt_read( struct DEV *dev, uint32_t lbn, uint32_t length,
                    char *buffer ) {
    disktypep_t dp;
    char *lbnbuf = NULL;
    vmscond_t status;
    uint32_t secblk;
    off_t devlimit, end, eofptr;
#ifdef DEBUG_DISKIO
    uint32_t inlbn = lbn, inlen = length;
    char *inbuf = buffer;
#endif

    dp = dev->disktype;

    /* First pass - inefficient but should function.
     * First bit of ugliness is that simulator files may be smaller than the
     * device size (not sparse, EOF isn't set).  The filesystem will happily
     * read never-written data (not a security issue), so we have to emulate
     * sparse file semantics up to the device size.  (For write, the file will
     * extend.)
     *
     * Second is that interleaved devices cause lots of seeks.  This could be
     * a lot smarter - but then the filesystem has a cache and those devices
     * tend to be small.  So there's no rush to do better.
     *
     * Third, disk ID by the user isn't reliable; especially for images of
     * physical disks.  This can cause file system accesses to valid data
     * to exceed the limit established by (alleged) geometry.
     *
     * Fourth, the (virtual) size of VHD files is not necessarily what was
     * requested; it may have been rounded up to a page boundary.
     */

    devlimit = ((off_t)dp->sectors) * dp->tracks * dp->cylinders * dp->sectorsize;
    eofptr = dev->eofptr;

    if( eofptr > devlimit )
        devlimit = eofptr;

    status = SS$_NORMAL;

    if( dp->reserved == 0 && dp->interleave == 0 && dp->skew == 0 ) {
        if( (dev->access & MOU_VIRTUAL) &&
            (end = ((off_t)lbn * 512 + length)) > eofptr ) {
            uint32_t avail;
            off_t start;

            if( end > devlimit ) {
                return printmsg( IO_RDBLK2BIG, 0,
                                 ((end + dp->sectorsize -1)/dp->sectorsize) );
            }
            start = (off_t)lbn * 512;

            if( start <  eofptr ) {
                avail = (uint32_t)( eofptr - start );

                if( $FAILS(status = dev->devread( dev, lbn, avail, buffer )) )
                    return status;

                length -= avail;
                buffer += avail;
            }
            memset( buffer, 0, length );
#ifdef DEBUG_DISKIO
            dumpblock( inlbn, inlen, inbuf, "Virt Read with %u bytes of fill status %08X",
                       length, status );
#endif
            return status;
        }
        status = dev->devread( dev, lbn, length, buffer );
#ifdef DEBUG_DISKIO
        dumpblock( inlbn, inlen, inbuf, "Virt Read status %08X", status );
#endif
        return status;
    }

    status = SS$_NORMAL;

    if( dp->sectorsize > 512 )
        abort(); /* TODO *** Handle large sectors, e.g. CDROM 2K where multiple LBNs fit into one. */

    secblk = 512 / dp->sectorsize;
    if( (lbnbuf = malloc( (size_t)512 * 2 )) == NULL )
        return SS$_INSFMEM;

    while( length > 0 && $SUCCESSFUL(status) ) {
        uint32_t pbn;
        uint32_t s;

        for( s = 0; s < secblk && length > 0; s++ ) {
            uint32_t n;
            ldiv_t r;

            pbn = maplbn( lbn, dp, s );

            r = ldiv( (pbn * dp->sectorsize), 512 );

            n = r.rem + (length > dp->sectorsize? dp->sectorsize: length);

            if( (dev->access & MOU_VIRTUAL) &&
                (end = ((off_t)r.quot * 512 + n)) > eofptr ) {
                uint32_t avail;
                off_t start;

                if( ((off_t)pbn * (long)dp->sectorsize) + (long)n - r.rem > devlimit ) {
                    status =  printmsg( IO_RDBLK2BIG, 0,
                            (((pbn * dp->sectorsize + n) + dp->sectorsize -1) /
                             dp->sectorsize) -1 );
                    break;
                }

                start = (off_t)r.quot * 512;

                if( start <  eofptr ) {
                    avail = (uint32_t)( eofptr - start );

                    if( $FAILS(status = dev->devread( dev, r.quot, avail, lbnbuf )) )
                        return status;

                    memset( lbnbuf+avail, 0, (size_t)n - avail );
                } else
                    memset( lbnbuf, 0, n );
            } else {
                if( $FAILS(status = dev->devread( dev, r.quot, n, lbnbuf )) )
                    break;
            }
            n = (dp->sectorsize > length)? length: dp->sectorsize;
            memcpy( buffer, lbnbuf+r.rem, n );
            buffer += n;
            length -= n;
        }
        ++lbn;
    }
    free( lbnbuf );
#ifdef DEBUG_DISKIO
    dumpblock( inlbn, inlen, inbuf, "Virt Read interleaved status %08X", status );
#endif
    return status;
}

/*********************************************************** virt_write() */

vmscond_t virt_write( struct DEV *dev, uint32_t lbn, uint32_t length,
                     const char *buffer ) {
    disktypep_t dp;
    char *lbnbuf = NULL;
    vmscond_t status;
    uint32_t secblk;
    off_t devlimit, start, end;
#ifdef DEBUG_DISKIO
    uint32_t inlbn = lbn, inlen = length;
    const char *inbuf = buffer;
#endif

    dp = dev->disktype;

    devlimit = ((off_t)dp->sectors) * dp->tracks * dp->cylinders * dp->sectorsize;
    if( dev->eofptr > devlimit )
        devlimit = dev->eofptr;

    end = ((off_t)lbn * 512) + length;

    if( dp->reserved == 0 && dp->interleave == 0 && dp->skew == 0 ) {
        if( (end + 511)/512 > devlimit ) {
            return printmsg( IO_WRBLK2BIG, 0,
                             ((end + dp->sectorsize -1)/dp->sectorsize) );
        }
        status = dev->devwrite( dev, lbn, length, buffer );
#ifdef DEBUG_DISKIO
        dumpblock( inlbn, inlen, inbuf, "Virt Write, status %08X", status );
#endif
        if( $SUCCESSFUL(status) &&
            (dev->access & MOU_VIRTUAL) && (end > dev->eofptr) ) {
            dev->eofptr = end;
        }
        return status;
    }

    status = SS$_NORMAL;

    if( dp->sectorsize > 512 )
        abort(); /* TODO *** Handle large sectors, e.g. CDROM 2K where multiple LBNs fit into one. */

    secblk = 512 / dp->sectorsize;
    if( (lbnbuf = malloc( (size_t)512 * 2 )) == NULL )
        return SS$_INSFMEM;

    while( length > 0 && $SUCCESSFUL(status) ) {
        uint32_t pbn;
        uint32_t s;

        for( s = 0; s < secblk && length > 0; s++ ) {
            uint32_t n, m;
            ldiv_t r;

            pbn = maplbn( lbn, dp, s );

            r = ldiv( (pbn * dp->sectorsize), 512 );

            n = r.rem + (length > dp->sectorsize? dp->sectorsize: length);

            start = (off_t)r.quot * 512;
            m = ((n + 511) / 512) * 512;
            if( start < dev->eofptr && (off_t)(start + m) > dev->eofptr )
                m = (uint32_t)(dev->eofptr - start);

            if( dev->access & MOU_VIRTUAL ) {
                if( ((off_t)pbn * (long)dp->sectorsize) + ((long)n - r.rem)  > devlimit ) {
                    status = printmsg( IO_WRBLK2BIG, 0,
                                       (((pbn * dp->sectorsize) + (n - r.rem) +
                                         dp->sectorsize -1) / dp->sectorsize ) -1 );
                    break;
                }
            }
            memset( lbnbuf, 0, (size_t)512 * 2 );
            if( start < dev->eofptr ) {
                if( $FAILS(status = dev->devread( dev, r.quot, m, lbnbuf )) )
                    break;
                if( m < n )
                    memset( lbnbuf+m, 0, (size_t)n - m );
            }
            memcpy( lbnbuf+r.rem, buffer, (size_t)n - r.rem );
            end = start + n;
            if( end > dev->eofptr ) {
                status = dev->devwrite( dev, r.quot, n, lbnbuf );
                dev->eofptr = end;
            } else
                status = dev->devwrite( dev, r.quot, m, lbnbuf );

            if( $FAILED(status) )
                break;
            n -= r.rem;
            buffer += n;
            length -= n;
        }
        ++lbn;
    }
#ifdef DEBUG_DISKIO
    dumpblock( inlbn, inlen, inbuf, "Virt Write (interleaved) status %08X", status );
#endif

    free( lbnbuf );
    return status;
}

/*********************************************************** virt_readp() */

vmscond_t virt_readp( struct DEV *dev, uint32_t pbn, uint32_t length,
                    char *buffer ) {
    disktypep_t dp;
    vmscond_t status;
    off_t devlimit, eofptr;
    off_t fp, offset;
    char *buf;
    uint32_t avail;

    dp = dev->disktype;

    devlimit = ((off_t)dp->sectors) * dp->tracks * dp->cylinders * dp->sectorsize;
    if( dev->eofptr > devlimit )
        devlimit = dev->eofptr;

    status = SS$_NORMAL;
    fp = ((off_t)pbn) * dp->sectorsize;

    if( fp + (off_t)length > devlimit ) {
        return printmsg( IO_RDBLK2BIG, 0,
                ((fp + length + dp->sectorsize -1)/dp->sectorsize) );
    }

    if( dev->access & MOU_VIRTUAL ) {
        eofptr = dev->eofptr;

        if( fp >= eofptr ) {
            memset( buffer, 0, length );
#ifdef DEBUG_DISKIO
            dumpblock( pbn, length, buffer, "Phys Read of unallocated block status %08X",
                       status );
#endif
            return status;
        }
    } else {
        eofptr = devlimit;
    }

    offset = fp % 512;

    avail = (uint32_t)(eofptr - fp);
    if( avail > length )
        avail = length;

    if( offset == 0 ) {
        if( $FAILS(status = dev->devread( dev, (uint32_t) (fp / 512), avail, buffer )) )
            return status;

        length -= avail;
        buffer += avail;

        if( length > 0 )
            memset( buffer, 0, length );
#ifdef DEBUG_DISKIO
        dumpblock( pbn, avail+length, buffer-avail, "Phys Read with %u bytes of fill status %08X",
                   length, status );
#endif
        return status;
    }
    if( (buf = malloc( ((size_t)offset + avail) )) == NULL )
        return SS$_INSFMEM;
    status = dev->devread( dev, (uint32_t) (fp / 512),
                           (uint32_t) (offset + avail), buf );

    memcpy( buffer, buf+offset, avail );
    free( buf );
    length -= avail;
    buffer += avail;
    if( length > 0 )
        memset( buffer, 0, length );
#ifdef DEBUG_DISKIO
    dumpblock( pbn, avail+length, buffer-avail, "Phys Read with %u bytes of fill status %08X",
               length, status );
#endif
    return status;
}

/*********************************************************** virt_writep() */

vmscond_t virt_writep( struct DEV *dev, uint32_t pbn, uint32_t length,
                      char *buffer ) {
    disktypep_t dp;
    vmscond_t status;
    off_t devlimit;
    off_t fp, offset;
    char *buf = NULL;
    uint32_t avail, n, m;
#ifdef DEBUG_DISKIO
    uint32_t inpbn = pbn, inlen = length;
    const char *inbuf = buffer;
#endif

    dp = dev->disktype;

    devlimit = ((off_t)dp->sectors) * dp->tracks * dp->cylinders * dp->sectorsize;
    if( dev->eofptr > devlimit )
        devlimit = dev->eofptr;

    status = SS$_NORMAL;
    fp = ((off_t)pbn) * dp->sectorsize;

    if( fp + (off_t)length > devlimit ) {
        return printmsg( IO_WRBLK2BIG, 0,
                ((fp + length + dp->sectorsize -1)/dp->sectorsize) );
    }

    offset = fp % 512;

    if( offset == 0 && length % 512 == 0 ) {
        status = dev->devwrite( dev, (uint32_t)(fp / 512), length, buffer );
#ifdef DEBUG_DISKIO
        dumpblock( inpbn, inlen, inbuf, "Phys Write, status %08X", status );
#endif
        if( dev->access & MOU_VIRTUAL ) {
            fp += length;
            if( fp > dev->eofptr )
                dev->eofptr = fp;
        }
        return status;
    }

    if( (buf = malloc( 512 )) == NULL )
        return SS$_INSFMEM;

    do {
        if( offset != 0 ) {
            avail = (uint32_t) ( devlimit - (fp -offset) );

            n = (avail > 512)? 512: avail;

            if( $FAILS(status = dev->devread( dev, (uint32_t) (fp / 512), n, buf )) )
                break;

            m = (n > length)? length: n;

            memcpy( buf+offset, buffer, m );
            buffer += m;
            length -= m;

            if( $FAILS(status = dev->devwrite( dev, (uint32_t) (fp / 512), n, buf )) )
                break;
            fp += n;
        }

        if( (n = length / 512) > 0 ) {
            n *= 512;
            if( $FAILS(status = dev->devwrite( dev, (uint32_t) (fp / 512), n, buffer )) )
                break;

            buffer += n;
            length -= n;
            fp += n;
        }

        if( length > 0 ) {
            avail = (uint32_t) ( devlimit - fp );

            n = (avail > 512)? 512: avail;

            if( $FAILS(status = dev->devread( dev, (uint32_t) (fp / 512), n, buf )) )
                break;

            m = (n > length)? length: n;

            memcpy( buf, buffer, m );
            buffer += m;
            length -= m;

            if( $FAILS(status = dev->devwrite( dev, (uint32_t) (fp / 512), n, buf )) )
                break;

            fp += n;
        }
        if( length != 0 )
            status = SS$_BUGCHECK;
    } while( 0 );

    if( (dev->access & MOU_VIRTUAL) && fp > dev->eofptr )
        dev->eofptr = fp;

#ifdef DEBUG_DISKIO
        dumpblock( inpbn, inlen, inbuf, "Phys Write, status %08X", status );
#endif
        free( buf );
        return status;
}

/*********************************************************** vget_devsiz() */

static size_t get_devsiz( const char *devnam ) {
    const char *p;

    if( (p = strchr( devnam, ':' )) == NULL )
        return strlen( devnam );

    return (size_t)( p - devnam );
}

/*********************************************************** disktype */

#define DISK( name, sectors, tracks, cylinders, flags )                 \
    {#name,       512  ## ul, sectors ## ul, tracks ## ul, cylinders ## ul, \
            0 ## ul, 0 ## ul, 0 ## ul, (flags)},
#define DISKS( name, sectorsize, sectors, tracks, cylinders, reserved, flags ) \
    {#name, sectorsize ## ul, sectors ## ul, tracks ## ul, cylinders ## ul, \
            reserved ## ul, 0 ## ul, 0 ## ul, (flags)},
#define DISKI(  name, sectorsize, sectors, tracks, cylinders, reserved, \
                interleave, skew, flags )                                     \
    {#name, sectorsize ## ul, sectors ## ul, tracks ## ul, cylinders ## ul, \
            reserved ## ul, interleave ## ul, skew ## ul, (flags)},

disktype_t disktype[] = {
    DISK(UNKNOWN, 574,  1,    1,DISK_BADSW)    /* First = short sequence delta = 1 */

#if 0
    DISK(RB02, 20,  2,  512, DISK_BAD144)
    DISK(RB80, 31, 14,  559, DISK_BAD144)
#endif

    DISK(RK05, 12,  2,  203, DISK_BAD144)
    DISK(RK06, 22,  3,  411, DISK_BAD144)
    DISK(RK07, 22,  3,  815, DISK_BAD144)
    DISK(RK11, 12,  2,  203, DISK_BAD144)

    DISKS(RL01, 256, 40,  2,  256, 0, DISK_BAD144)
    DISKS(RL02, 256, 40,  2,  512, 0, DISK_BAD144)

    DISK(RM02, 32,  5,  823, DISK_BAD144)
    DISK(RM03, 32,  5,  823, DISK_BAD144)
    DISK(RP02, 10, 20,  203, DISK_BAD144)
    DISK(RP03, 10, 20,  406, DISK_BAD144)
    DISK(RP04, 22, 19,  411, DISK_BAD144)
    DISK(RP05, 22, 19,  411, DISK_BAD144)
    DISK(RM80, 31, 14,  559, DISK_BAD144)
    DISK(RP06, 22, 19,  815, DISK_BAD144)
    DISK(RM05, 32, 19,  823, DISK_BAD144)
    DISK(RP07, 50, 32,  630, DISK_BAD144)

    DISKS(RS03, 128, 64, 1, 64, 0, DISK_BAD144)
    DISKS(RS04, 256, 64, 1, 64, 0, DISK_BAD144)

#if 0 /* Not useful now as RSX20-F used ODS-1 */
    DISKS(RM02-T, 576, 30,  5,  823, 0, DISK_BAD144)
    DISKS(RM03-T, 576, 30,  5,  823, 0, DISK_BAD144)
    DISKS(RP04-T, 576, 20, 19,  411, 0, DISK_BAD144)
    DISKS(RP05-T, 576, 20, 19,  411, 0, DISK_BAD144)
    DISKS(RM80-T, 576, 30, 14,  559, 0, DISK_BAD144)
    DISKS(RP06-T, 576, 20, 19,  815, 0, DISK_BAD144)
    DISKS(RM05-T, 576, 30, 19,  823, 0, DISK_BAD144)
    DISKS(RP07-T, 576, 43, 32,  630, 0, DISK_BAD144)
#endif

    DISK(RX50, 10, 1, 80, DISK_BADSW)
    DISK(RX33, 15, 2, 80, DISK_BADSW)

#if 0
    DISK(RD50, 99, 99, 9999,0)
#endif
    DISK(RD51, 18, 4, 306,0)
    DISK(RD31, 17, 4, 615,0)
    DISK(RD52, 17, 8, 512,0)
    DISK(RD53, 17, 7, 1024,0)
    DISK(RD54, 17, 15, 1225,0)

    DISK(RA72, 51, 20, 1921,0)

#if 0
    DISK(RA80, 99, 99, 9999,0)

#endif
    DISK(RA81, 51, 14, 1258,0)
    DISK(RA82, 57, 15, 1435,0)

    DISK(RA90, 69, 13, 2649,0)
    DISK(RA92, 73, 13, 3099,0)

    DISK(RRD40,128, 1, 10400,0)
    DISK(RRD50,128, 1, 10400,0)
    DISKI(RX01, 128, 26, 1, 77, 26, 2, 6, DISK_BADSW)
    DISKI(RX02, 256, 26, 1, 77, 26, 2, 6, DISK_BADSW)

#if 0
    DISK(RX23-SD, 99, 99, 9999, DISK_BADSW)
    DISK(RX23-DD, 99, 99, 9999, DISK_BADSW)

    DISK(RX33-SD, 10, 1, 80, DISK_BADSW)
    DISK(RX33-DD, 99, 99, 9999, DISK_BADSW)
#endif

    DISK(RX50, 10, 1, 80, DISK_BADSW)
#if 0
    DISK(RC25, 99, 99, 9999, 0)

    DISK(RF30, 99, 99, 9999, 0)
    DISK(RF31, 99, 99, 9999, 0)
    DISK(RF35, 99, 99, 9999, 0)
    DISK(RF36, 99, 99, 9999, 0)
    DISK(RF71, 99, 99, 9999, 0)
    DISK(RF72, 99, 99, 9999, 0)
    DISK(RF73, 99, 99, 9999, 0)
    DISK(RF74, 99, 99, 9999, 0)

    DISK(RZ22, 99, 99, 9999, 0)
    DISK(RZ23, 99, 99, 9999, 0)
    DISK(RZ24, 99, 99, 9999, 0)
    DISK(RZ25, 99, 99, 9999, 0)
    DISK(RZ26, 99, 99, 9999, 0)
    DISK(RZ27, 99, 99, 9999, 0)
    DISK(RZ28, 99, 99, 9999, 0)
    DISK(RZ29, 99, 99, 9999, 0)
    DISK(RZ31, 99, 99, 9999, 0)
    DISK(RZ33, 99, 99, 9999, 0)
    DISK(RZ35, 99, 99, 9999, 0)
    DISK(RZ55, 99, 99, 9999, 0)
    DISK(RZ56, 99, 99, 9999, 0)
    DISK(RZ57, 99, 99, 9999, 0)
    DISK(RZ58, 99, 99, 9999, 0)
    DISK(RZ59, 99, 99, 9999, 0)

    DISK(RZ72, 99, 99, 9999, 0)
    DISK(RZ73, 99, 99, 9999, 0)
    DISK(RZ74, 99, 99, 9999, 0)
#endif

    { NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
};
size_t max_disktype = (sizeof( disktype ) / sizeof( disktype[0] )) - 2;
