/* Phyvhd.c Physical I/O for VHD format disks */

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

#if !defined( DEBUG ) && defined( DEBUG_PHYVHD )
#define DEBUG DEBUG_PHYVHD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#ifndef phyvhd_BUFSIZE
#define phyvhd_BUFSIZE (10 * 512)
#endif

#ifndef phyvhd_MINSIZE
#define phyvhd_MINSIZE (0)
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#define close _close
#else
#include <unistd.h>
#define _aligned_free free
#endif

#include "compat.h"
#include "device.h"
#include "ods2.h"
#include "phyvirt.h"
#include "ssdef.h"
#include "stsdef.h"
#include "sysmsg.h"

#include "libvhd.h"
#include "phyvhd.h"

typedef vhd_context_t *vhd_contextp;
typedef vhd_context_t  vhd_context;

#ifdef USE_LIBVHD
#include <dlfcn.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int (*vhd_open_p)( vhd_contextp ctx, const char *filename, int flags );
typedef void (*vhd_close_p)( vhd_contextp ctx );
typedef int (*vhd_create_p)(const char *file, uint64_t size,
                            int type, uint32_t flags );
typedef int vhd_header_decode_parent(vhd_contextp ctx,
                                     vhd_header_t *header, char **buf);
typedef int (*vhd_snapshot_p)( const char *filename, const char *parent, uint64_t size,
                               const char *parentfile, uint32_t flags );
typedef int (*vhd_io_read_p)(vhd_contextp ctx, void *buf, uint64_t pbn, uint32_t count );
typedef int (*vhd_io_write_p)(vhd_contextp ctx, void *buf, uint64_t pbn, uint32_t count);
typedef void (*libvhd_set_log_level_p)(int);
typedef uint32_t (*vhd_chs_p)(uint64_t size);
/* *** */

static void *libvhd = NULL;

#define vhddeclare(s) s ## _p s ## _fcn = NULL
    vhddeclare(vhd_open);
    vhddeclare(vhd_close);
    vhddeclare(vhd_create);
    vhddeclare(libvhd_set_log_level);
    vhddeclare(vhd_header_decode_parent);
    vhddeclare(vhd_snapshot);
#if 0
    vhddeclare(vhd_chs);
#endif
    vhddeclare(vhd_io_read);
    vhddeclare(vhd_io_write);

#define vhdresolve(s) do {                                              \
        if( libvhd != NULL &&                                           \
            (s ## _fcn = (s ## _p)dlsym( libvhd, #s )) == NULL ) {      \
            printmsg( IO_VHDNOSYM, 0, #s, LIBVHDSO(USE_LIBVHD) );       \
            dlclose( libvhd );                                          \
            libvhd = NULL;                                              \
        }                                                               \
} while( 0 )
#else

#define vhd_open_fcn vhd_open
#define vhd_close_fcn vhd_close
#define vhd_create_fcn vhd_create
#define libvhd_set_log_level_fcn libvhd_set_log_level
#define vhd_header_decode_parent_fcn vhd_header_decode_parent
#define vhd_snapshot_fcn vhd_snapshot
#if 0
#define vhd_chs_fcn vhd_chs
#endif
#define vhd_io_read_fcn vhd_io_read
#define vhd_io_write_fcn vhd_io_write

#endif

static vmscond_t phyvhd_read( struct DEV *dev, uint32_t lbn, uint32_t length,
                              char *buffer );
static vmscond_t phyvhd_write( struct DEV *dev, uint32_t lbn, uint32_t length,
                               const char *buffer );

/*********************************************************** phyvhd_available() */
int phyvhd_available( int query ) {
#ifdef USE_LIBVHD
#define LIBVHDSOx(x) #x
#define LIBVHDSO(n) LIBVHDSOx(n)

    if( libvhd == NULL ) {
        if( (libvhd = dlopen( LIBVHDSO(USE_LIBVHD), RTLD_LAZY )) == NULL ) {
            const char *err;

            if( query )
                printmsg( IO_VHDNOTAVAIL, MSG_TEXT );

            printmsg( IO_VHDNOLIB, 0, LIBVHDSO(USE_LIBVHD) );
            if( (err = dlerror()) != NULL )
                printmsg( ODS2_OSERROR, MSG_CONTINUE, O_VHDNOLIB, err );
            return FALSE;
        }

        vhdresolve( vhd_open );
        vhdresolve( vhd_close );
        vhdresolve( vhd_create );
        vhdresolve( vhd_snapshot );
        vhdresolve( libvhd_set_log_level );
        vhdresolve( vhd_header_decode_parent );

        vhdresolve( vhd_io_read );
        vhdresolve( vhd_io_write );

        if( libvhd == NULL ) {
            printmsg( IO_VHDNOTVALID, MSG_CONTINUE,
                      IO_VHDNOSYM, LIBVHDSO(USE_LIBVHD) );
            return FALSE;
        }
    }
#endif

    if( query )
        printmsg( IO_VHDAVAIL, MSG_TEXT );

    return TRUE;
}

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4996) /* MS complains about _open, which is open() */
#endif
/*********************************************************** phyvhd_snapshot() */
vmscond_t phyvhd_snapshot( const char *filename, const char *parent ) {
    int fd, err;
    char *p;

    if( !( (p = strrchr( filename, '.')) != NULL && ( !strcmp( p, ".vhd" ) ||
                                                      !strcmp( p, ".VHD" ) ) ) ) {
        return printmsg( IO_VHDONLY, 0, filename );
    }

#ifdef USE_LIBVHD
    if( libvhd == NULL && !phyvhd_available( FALSE ) )
        return IO_VHDNOTVALID | STS$M_INHIB_MSG;
#endif

#ifdef _WIN32
    fd = _open( filename, _O_BINARY | _O_RDWR | _O_CREAT | _O_EXCL, 0644 );
#else
    fd = open( filename, O_RDWR | O_CREAT | O_EXCL, 0644 );
#endif
    if( fd == -1 ) {
        int err;

        if( errno == EEXIST ) {
            printmsg( IO_EXISTS, 0, filename );
            return SS$_DUPFILENAME | STS$M_INHIB_MSG;
        }
        err = errno;
        printmsg( IO_OPENDEV, 0, filename );
        return printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_OPENDEV, strerror( err ) );
    }
    err = vhd_snapshot_fcn( filename, 0, parent, 0 );
    close( fd );

    if( err == 0 )
        return SS$_NORMAL;

    printmsg( IO_VHDSNAPFAIL, 0, parent );
    return printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_VHDSNAPFAIL, strerror( -err ) );
}

/*********************************************************** phyvhd_init() */
vmscond_t phyvhd_init( struct DEV *dev ) {
    int err;
    char *fname;
    unsigned status;
    long align;
#ifdef _WIN32
    SYSTEM_INFO inf;

    GetSystemInfo( &inf );
    align = inf.dwPageSize;
#elif defined( _SC_PAGESIZE)
    align = sysconf( _SC_PAGESIZE );
#else
    align = getpagesize();
#endif

#ifdef USE_LIBVHD
    if( libvhd == NULL && !phyvhd_available( FALSE ) )
        return IO_VHDNOTVALID | STS$M_INHIB_MSG;
#endif
    libvhd_set_log_level_fcn( -1 );

    fname = virt_lookup( dev->devnam );
    if( fname == NULL )
        return SS$_NOSUCHDEV;

    dev->devread = phyvhd_read;
    dev->devwrite = phyvhd_write;

#ifdef _WIN32
    if( (dev->IoBuffer = _aligned_malloc( phyvhd_BUFSIZE, align )) == NULL ) {
        return printmsg( IO_VHDMEM, 0 );
    }

    if( (dev->context = _aligned_malloc( sizeof( vhd_context ), align )) == NULL ) {
        _aligned_free(dev->IoBuffer);
        dev->IoBuffer = NULL;
        return printmsg( IO_VHDMEM, 0 );
    }
#else
    if( posix_memalign( (void**)&dev->IoBuffer, align, phyvhd_BUFSIZE ) != 0 ) {
        return printmsg( IO_VHDMEM, 0 );
    }
    if( posix_memalign( (void**)&dev->context, align,
                        sizeof( vhd_context ) ) != 0 ) {
        free(dev->IoBuffer);
        dev->IoBuffer = NULL;
        return printmsg( IO_VHDMEM, 0 );
    }
#endif

    status = SS$_NORMAL;

    do {
        if( dev->access & PHY_CREATE ) {
            disktypep_t dp;
            uint64_t size;
            int fd;

#ifdef _WIN32
            fd = _open( fname, _O_BINARY | _O_RDWR | _O_CREAT | _O_EXCL, 0644 );
#else
            fd = open( fname, O_RDWR | O_CREAT | O_EXCL, 0644 );
#endif
            if( fd == -1 ) {
                status = SS$_DUPFILENAME;
                break;
            }
            if( dev->access & MOU_LOG )
                printmsg( IO_VHDFMTSTART, 0, fname );

            dp = dev->disktype;
            size = ((uint64_t)dp->sectors) * dp->tracks * dp->cylinders * dp->sectorsize;
            size = (size + 511) / 512;
            size *= 512;

#if phyvhd_MINSIZE == 0
            err = vhd_create_fcn( fname, size, HD_TYPE_DYNAMIC, 0 );
#else
            err = vhd_create_fcn( fname, size, (size >= phyvhd_MINSIZE?
                                                HD_TYPE_DYNAMIC: HD_TYPE_FIXED),
                                  0 );
#endif
            close( fd );
            if( err != 0 ) {
                printmsg( IO_VHDCREATEERR, 0, fname );
                status = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_VHDCREATEERR,
                                   strerror( -err ) );
                break;
            }
            if( dev->access & MOU_LOG )
                printmsg( IO_VHDFMTEND, 0 );
        }
        err = vhd_open_fcn( dev->context, fname,
                            (dev->access & MOU_WRITE)?
                            VHD_OPEN_RDWR | VHD_OPEN_STRICT :
                            VHD_OPEN_RDONLY | VHD_OPEN_STRICT );
        if( err != 0 && (dev->access & (MOU_WRITE|PHY_CREATE)) == MOU_WRITE ) {
            dev->access &= ~MOU_WRITE;
            err = vhd_open_fcn( dev->context, fname, VHD_OPEN_RDONLY | VHD_OPEN_STRICT );
        }
        if( err != 0 ) {
            printmsg( IO_OPENDEV, 0, fname );
            status = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_OPENDEV,
                               strerror( -err ) );
            break;
        }
        if( (dev->access & MOU_WRITE) && ((vhd_contextp)dev->context)->footer.saved ) {
            printmsg( IO_VHDSAVED, 0, fname );
        }
        dev->eofptr = (off_t)(((vhd_contextp)dev->context)->footer.curr_size);
    } while( 0 );

    if( $FAILED(status) ) {
        _aligned_free( dev->context );
        _aligned_free( dev->IoBuffer );
        dev->context =
            dev->IoBuffer = NULL;
    }
    return status;
}
#ifdef _WIN32
#pragma warning(pop)
#endif
/*********************************************************** phyvhd_show() */
void phyvhd_show( struct DEV *dev, size_t column ) {
    vhd_contextp ctx;
    const char *type, *p;
    size_t i, n;

    if( (ctx = dev->context) == NULL )
        return;
    i = ctx->footer.type;
    if( i > HD_TYPE_MAX )
        type = "Unknown";
    else
        type = HD_TYPE_STR[i];

    n = strlen( type );
    if( (p = strchr( type, ' ' )) == NULL )
        p = type + n;
    n = (size_t) (p -type);

    printf( " VHD %*.*s disk", (int)n, (int)n, type );
    if( i == HD_TYPE_DIFF ) {
        char *name;
#define PLABEL "parent: "
        if( vhd_header_decode_parent_fcn( ctx, &ctx->header, &name ) == 0 ) {
            printf( "\n%*sparent: %s", (int)(column < sizeof( PLABEL ) -1?
                                             (sizeof( PLABEL )-1)-column :
                                             column - (sizeof( PLABEL ) -1)),
                    "", name );
            free( name );
        }
    }

    return;
}
/*********************************************************** phyvhd_read() */
static vmscond_t phyvhd_read( struct DEV *dev, uint32_t lbn,
                              uint32_t length, char *buffer ) {
    vmscond_t status;
    int err;

    status = SS$_NORMAL;
    while( length > 0 ) {
        uint32_t rdsize;

        rdsize = (length + 511) / 512;
        rdsize = ( (rdsize > phyvhd_BUFSIZE/512)?
                   phyvhd_BUFSIZE/512 : rdsize );

        if( (err = vhd_io_read_fcn(dev->context, dev->IoBuffer, lbn, rdsize)) != 0 ) {
            if( err == -ERANGE )
                return  printmsg( IO_RDBLK2BIG, 0, lbn );
            printmsg( IO_READERR, 0, lbn );
            return printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_READERR,
                             strerror( -err ) );
        }
        lbn += rdsize;
        rdsize *= 512;
        if( rdsize > length )
            rdsize = length;
        memcpy( buffer, dev->IoBuffer, rdsize );
        buffer += rdsize;
        length -= rdsize;
    }
    return status;
}


/*********************************************************** phyvhd_write() */

static vmscond_t phyvhd_write( struct DEV *dev, uint32_t lbn,
                               uint32_t length, const char *buffer ) {
    vmscond_t status;
    int err;

    status = SS$_NORMAL;
    while( length > 0 ) {
        uint32_t wrsize, iosize;

        iosize = (length + 511) / 512;
        iosize = ( (iosize > phyvhd_BUFSIZE/512)?
                   phyvhd_BUFSIZE/512 : iosize );

        wrsize = iosize * 512;
        if( wrsize > length )
            wrsize = length;
        memcpy( dev->IoBuffer, buffer, wrsize );
        buffer += wrsize;
        length -= wrsize;

        if( (err = vhd_io_write_fcn(dev->context, dev->IoBuffer, lbn, iosize)) != 0 ) {
            if( err == -ERANGE )
                return  printmsg( IO_WRBLK2BIG, 0, lbn );
            printmsg( IO_WRITEERR, 0, lbn );
            return printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_WRITEERR,
                             strerror( -err ) );
        }
        lbn += iosize;
    }
    return status;
}

/*********************************************************** phyvhd_close() */
vmscond_t phyvhd_close( struct DEV *dev ) {
    if( dev->context != NULL ) {
        vhd_close_fcn( dev->context );
        _aligned_free( dev->context );
        dev->context = NULL;
    }
    _aligned_free( dev->IoBuffer );
   dev->IoBuffer = NULL;
    return SS$_NORMAL;
}

