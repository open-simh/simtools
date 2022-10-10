/* PHYVMS.c    Physical I/O module for VMS */

/*
 *      This is part of ODS2 written by Paul Nankervis,
 *      email address:  Paulnank@au1.ibm.com

 *      ODS2 is distributed freely for all members of the
 *      VMS community to use. However all derived works
 *      must maintain comments in their source to acknowledge
 *      the contributions of the original author and
 *      subsequent contributors.   This is free software; no
 *      warranty is offered,  and while we believe it to be useful,
 *      you use it at your own risk.
 */

/*  How come VMS is so much easier to do physical I/O on than
 *  those other ^%$@*! systems?   I can't believe that they have
 *  different command sets for different device types, and can
 *  even have different command sets depending on what mode they
 *  are called from!  Sigh.                                  */

/* Modified by:
 *
 *   31-AUG-2001 01:04  Hunter Goatley <goathunter@goatley.com>
 *
 *      Added checks to be sure device we're mounting is a
 *      disk and that it's already physically mounted at the
 *      DCL level,
 *
 *    7-JUN-2005        Larry Baker <baker@usgs.gov>
 *
 *      If Mount/Virtual, use normal Unix open()/close()/read()/write().
 *      Negative handle -> Unix I/O; positive handle -> SYS$QIOW().
 */

#if !defined( DEBUG ) && defined( DEBUG_PHYVMS )
#define DEBUG DEBUG_PHYVMS
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <dcdef.h>
#include <descrip.h>
#include <devdef.h>
#include <dvidef.h>
#include <iodef.h>
#include <lib$routines.h>
#include <mntdef.h>
#include <rms.h>                /* struct FAB */
#include <ssdef.h>
#include <stsdef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef VAXC
#define LIB$M_FIL_NOWILD 1
#include <file.h>
#include <unixio.h>
#else
#ifdef VAX
#define LIB$M_FIL_NOWILD 1
#else
#include <libfildef.h>          /* LIB$M_FIL_NOWILD */
#endif
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef __GNUC__
unsigned sys$assign();
unsigned sys$close();
unsigned sys$open();
unsigned sys$qiow();
unsigned sys$dassgn();
#else
#include <starlet.h>
#endif

#ifndef SS$_NOMOREDEV
#define SS$_NOMOREDEV 2648
#endif

#ifndef EFN$C_ENF
#define EFN$C_ENF 0 /* 128 in recent VMS */
#endif

#include "ods2.h"
#include "phyio.h"
#include "phyvirt.h"

static vmscond_t phyio_read( struct DEV *dev, uint32_t block,
                             uint32_t length, char *buffer );
static vmscond_t phyio_write( struct DEV *dev, uint32_t block,
                              uint32_t length, const char *buffer );

struct ITMLST {
    unsigned short  length;
    unsigned short  itmcod;
    void           *buffer;
    unsigned short *retlen;
};

#define MSGID(sts) ((sts) & (STS$M_COND_ID|STS$M_SEVERITY))

static unsigned init_count = 0;
static unsigned read_count = 0;
static unsigned write_count = 0;

/*************************************************************** showdevs() */
static void showdevs( void ) {
    vmscond_t status;
    size_t max = 0;
    int pass;

    max = 0
    for( pass = 0; pass < 2; pass++ ) {
        unsigned long ctx[2] = { 0, 0 };
        size_t col = 0;
        unsigned short retlen, iosb[4];
        char devnam[64+1],
             dspnam[64+1];
        struct dsc$descriptor_s retdsc = { sizeof( devnam ) -1,
                                           DSC$K_DTYPE_T,
                                           DSC$K_CLASS_S, devnam };
        $DESCRIPTOR( searchdsc, "*" );
        unsigned long devclass = DC$_DISK;
        struct ITMLST scanlist[] = {
            { sizeof( devclass ), DC$_DISK, &devclass, NULL },
            { 0,                  0,        NULL,     NULL }
        };
        struct ITMLST dvilist[] = {
            { sizeof( dspnam ) -1, DVI$_DISPLAY_DEVNAM, dspnam, &retlen },
            { 0,                   0,                   NULL,   NULL }
        };

        while( 1 ) {
            char *name;
            size_t len;

            status = sys$device_scan( &retdsc, &retlen,
                                      &searchdsc, scanlist, &ctx );
            if( MSGID(status) != SS$_NORMAL )
                break;
            name = devnam;
            name[retlen] = '\0';
            len = retlen;
            status = sys$getdviw( EFN$C_ENF, 0, &retdsc, dvilist, iosb,
                                  NULL, 0, NULL );
            if( MSGID(status) == SS$_NORMAL ) {
                name = dspnam;
                dspnam[retlen] = '\0';
                len = retlen;
            }
            if( pass == 0 ) {
                if( len > max )
                    max = len;
                continue;
            }
            if( col + max > 75 ) {
                putchar( '\n' );
                col = 0;
            }
            col += 1+ max;

            printf( " %-*s", (int)max, name );
        }
    }
    switch( MSGID(status) ) {
    case SS$_NOMOREDEV:
        break;
    case SS$_NOSUCHDEV:
        printmsg( IO_NODEVS, MSG_TEXT );
        break;
    case SS$_NORMAL:
        break;
    default:
        printmsg( IO_VMSDEVSCAN, 0 );
        printmsg( status, MSG_CONTINUE, IO_VMSDEVSCAN );
        break;
    }
    return;
}

/*************************************************************** phyio_show() */

void phyio_show( showtype_t type ) {
    switch( type ) {
    case SHOW_STATS:
        printmsg( IO_STATS, MSG_TEXT, init_count, read_count, write_count );
        return;
    case SHOW_FILE64:
        printmsg( (sizeof( off_t ) < 8)? IO_NOLARGEFILE: IO_LARGEFILE, MSG_TEXT );
        return;
    case SHOW_DEVICES:
        printf( " Physical devices\n" );
        showdevs();
        return;
    default:
        abort();
    }
}

/*************************************************************** phyio_help() */

void phyio_help(FILE *fp ) {
    (void) helptopic( 0, "MOUNT VMS" );

    return;
}

/*************************************************************** phyio_init() */

vmscond_t phyio_init( struct DEV *dev ) {

    vmscond_t sts;
    const char *virtual;

    init_count++;

    dev->handle = -1;
    dev->devread = phyio_read;
    dev->devwrite = phyio_write;
    dev->sectors = 0;

    if( dev->access & MOU_VIRTUAL ) {
        int vmsfd;
        struct FAB fab;

        virtual = virt_lookup( dev->devnam );
        if ( virtual == NULL ) {
            return SS$_NOSUCHDEV;
        }
#define opnopts "ctx=stm", "rfm=udf", "shr=get,put"
        vmsfd = open( virtual,
                      ((dev->access & MOU_WRITE )? O_RDWR : O_RDONLY) |
                      ((dev->access & (MOU_VIRTUAL|PHY_CREATE)) == (MOU_VIRTUAL|PHY_CREATE)?
                       O_CREAT | O_EXCL : 0),
                      0666, opnopts );

        if ( vmsfd < 0 && (dev->access & (MOU_WRITE|PHY_CREATE)) == MOU_WRITE ) {
            dev->access &= ~MOU_WRITE;
            vmsfd = open( virtual, O_RDONLY, 0444, opnopts );
        }
        if ( vmsfd < 0 ) {
            int err;

            switch( errno ) {
            case EEXIST:
                printmsg( IO_EXISTS, 0, virtual );
                return SS$_DUPFILENAME | STS$M_INHIB_MSG;
            default:
                err = errno;
                printmsg( IO_OPENDEV, 0, virtual );
                return printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_OPENDEV,
                                 strerror( err ) );
            }
        }

        dev->handle = vmsfd;

        fab = cc$rms_fab;                       /* Make this a real FAB (bid and bln) */
        fab.fab$l_fna = (char *) virtual;
        fab.fab$b_fns = strlen( virtual );
        if( $SUCCESSFUL(sts = sys$open( &fab )) {/* Lookup file, get file size */
            dev->sectors = fab.fab$l_alq;
            sys$close( &fab );
            if ( sizeof( off_t ) < 8 && dev->sectors > 0x3FFFFF ) {
                close( vmsfd );
                dev->handle = -1;
                return printmsg( IO_OFFSET_TOO_BIG, 0 ); /* Need 64-bit off_t */
            }
        }
    } else { /* Physical */
        struct dsc$descriptor devdsc;
        unsigned long devclass, cylinders, tracks, sectors;
        char devname[65];
        unsigned long devchar;
        unsigned short iosb[4];
        struct ITMLST dvi_itmlst[7] = {
            { sizeof( devclass ),  DVI$_DEVCLASS,  &devclass,  NULL },
            { sizeof( devname ),   DVI$_ALLDEVNAM, &devname,   NULL },
            { sizeof( devchar ),   DVI$_DEVCHAR,   &devchar,   NULL },
            { sizeof( cylinders ), DVI$_CYLINDERS, &cylinders, NULL },
            { sizeof( tracks ),    DVI$_TRACKS,    &tracks,    NULL },
            { sizeof( sectors ),   DVI$_SECTORS,   &sectors,   NULL },
            { 0, 0, 0, 0 }
        };

        /*  Get some device information: device name, class, and mount status */
        memset( devname, 0, sizeof( devname ) );
        devdsc.dsc$w_length  = strlen( dev->devnam );
        devdsc.dsc$a_pointer = dev->devnam;
        if( $FAILS(sts = sys$getdviw( EFN$C_ENF, 0, &devdsc, &dvi_itmlst,
                                      iosb, 0, 0, 0 )) ) {
            return sts;
        }
        if( devclass != DC$_DISK ) {   /* If not a disk, return an error */
            return SS$_IVDEVNAM;
        }
        dev->sectors = cylinders * tracks * sectors;
        if( !(devchar & DEV$M_MNT) ) {
#if 1
/*
 *  This code will mount the disk if it's not already mounted.
 *  If you enable it, the "mount" command will mount the disk, but
 *  it'll stay mounted by the process running ODS2 if ODS2 exits abnormally.
 */
            unsigned long mntflags[2];
            struct ITMLST mnt_itmlst[3] = {
                { 0,                  MNT$_DEVNAM,    &devname,  NULL },
                { sizeof( mntflags ), MNT$_FLAGS,     &mntflags, NULL },
                { 0, 0, 0, 0 }
            };
            mnt_itmlst[0].length = strlen( devname );
            mntflags[0] = MNT$M_FOREIGN | MNT$M_NOASSIST | MNT$M_NOMNTVER;
            if( !(dev->access & MOU_WRITE) ) {
                mntflags[0] |= MNT$M_NOWRITE;
            }
            mntflags[1] = 0;
            sts = sys$mount( &mnt_itmlst );
            if( $FAILED(sts) && (dev->access & MOU_WRITE) ) {
                dev->access &= ~MOU_WRITE;
                mntflags[0] |= MNT$M_NOWRITE;
                sts = sys$mount( &mnt_itmlst );
            }
            if( $SUCCESSFUL(sts) ) {
                dev->access |= OS_MOUNTED;
                printmsg( IO_VMSMOUNT, 0, devname );
            } else {
                printmsg( IO_VMSNOMOUNT, 0, devname );
            }
#else
            sts = SS$_DEVNOTMOUNT;
#endif
        }
        if( $SUCCESSFUL(sts) ) {
            sts = sys$assign( &devdsc, &dev->handle, 0, 0, 0, 0 );
        }
    }

    return sts;
}

/*************************************************************** phyio_done() */

vmscond_t phyio_done( struct DEV *dev ) {
    vmscond_t status;

    status = SS$_NORMAL;

    if ( dev->access & MOU_VIRTUAL ) {
        close( dev->handle );
        virt_device( dev->devnam, NULL );
    } else {
#if 1
        if( dev->access & OS_MOUNTED ) {
            struct dsc$descriptor devdsc;

            devdsc.dsc$b_dtype = DSC$K_DTYPE_T;
            devdsc.dsc$b_class = DSC$K_CLASS_S;
            devdsc.dsc$w_length = strlen( dev->devnam );
            devdsc.dsc$a_pointer = dev->devnam;

            if( $SUCCESSFUL(status = sys$dismou( &devdsc, 0 )) ) {
                printmsg( IO_VMSDMT, 0, dev->devnam );
            } else {
                printmsg( IO_VMSNODMT, 0, dev->devnam );
            }

            dev->access &= ~OS_MOUNTED;
        }
#endif

        sys$dassgn( dev->handle );
    }
    dev->handle = 0;

    return status;
}

/*************************************************************** phyio_read() */

static vmscond_t phyio_read( struct DEV *dev, uint32_t block, uint32_t length,
                             char *buffer ) {
    vmscond_t sts;

#if DEBUG
    printf("Phyio read block: %d into %x (%d bytes)\n",block,buffer,length);
#endif
    read_count++;
    if( dev->access & MOU_VIRTUAL ) {
        int res;
        if( (res = lseek( dev->handle, block * 512, SEEK_SET )) < 0 ) {
            int err;

            err = errno;
            printmsg( IO_SEEKERR, block );
            sts = printmsg( ODS2_OSERROR, MSG_CONTINUE,
                            IO_SEEKERR, strerror(err) );
            return sts;
        }
        if( (res = read( dev->handle, buffer, length )) != length ) {
            int err;

            if( res == 0 ) {
                return SS$_ENDOFFILE;
            }
            err = errno;
            printmsg( IO_READERR, 0, block );
            if( res == (off_t)-1 ) {
                sts = printmsg( ODS2_OSERROR, MSG_CONTINUE,
                                IO_READERR, strerror(err) );
            } else {
                sts = printmsg( IO_READLEN, MSG_CONTINUE, IO_READERR,
                                block, (uint32_t)res, length );
            }
            return sts;
        }
        return SS$_NORMAL;
    } else {
        unsigned short int iosb[4];

        return sys$qiow( EFN$C_ENF, dev->handle, IO$_READLBLK, iosb, 0, 0,
                         buffer, length, block, 0, 0, 0 );
    }
}

/************************************************************** phyio_write() */

static vmscond_t phyio_write( struct DEV *dev, uint32_t block, uint32_t length,
                              const char *buffer ) {

    vmscond_t sts;

#if DEBUG
    printf("Phyio write block: %d from %x (%d bytes)\n",block,buffer,length);
#endif
    write_count++;
    if( dev->access & MOU_VIRTUAL ) {
        int res;
        if( (res = lseek( dev->handle, block * 512, 0 )) < 0 ) {
            int err;

            err = errno;
            printmsg( IO_SEEKERR, block );
            sts = printmsg( ODS2_OSERROR, MSG_CONTINUE,
                            IO_SEEKERR, strerror(err) );
            return sts;
        }
        if( (res = write( dev->handle, buffer, length )) != length ) {
            int err;

            err = errno;
            printmsg( IO_WRITEERR, 0, block );
            if( res == (off_t)-1 ) {
                sts = printmsg( ODS2_OSERROR, MSG_CONTINUE,
                                IO_WRITEERR, strerror(err) );
            } else {
                sts = printmsg( IO_WRITELEN, MSG_CONTINUE, IO_WRITEERR,
                                block, (uint32_t)res, length );
            }
            return sts;
        }
        return SS$_NORMAL;
    } else {
        unsigned short int iosb[4];

        return sys$qiow( EFN$C_ENF, dev->handle, IO$_WRITELBLK, iosb, 0, 0,
                         buffer, length, block, 0, 0, 0 );
    }
}
