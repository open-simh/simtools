/* PHYVMS.c    Physical I/O module for VMS */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/*  How come VMS is so much easier to do physical I/O on than
    those other ^%$@*! systems?   I can't believe that they have
    different command sets for different device types, and can
    even have different command sets depending on what mode they
    are called from!  Sigh.                                  */

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

static unsigned phyio_read( struct DEV *dev, unsigned block, unsigned length,
                            char *buffer );
static unsigned phyio_write( struct DEV *dev, unsigned block, unsigned length,
                             const char *buffer );

struct ITMLST {
    unsigned short  length;
    unsigned short  itmcod;
    void           *buffer;
    unsigned short *retlen;
};

#define chk( sts )  { register chksts; \
                      if ( !( ( chksts = sts ) & STS$M_SUCCESS ) ) \
                          lib$stop( chksts ); \
                    }

#define MSGID(sts) ((sts) & (STS$M_COND_ID|STS$M_SEVERITY))

static unsigned init_count = 0;
static unsigned read_count = 0;
static unsigned write_count = 0;

/*************************************************************** showdevs() */
static void showdevs( void ) {
    unsigned long status;
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
        printf( "No devices found\n" );
    case SS$_NORMAL:
        break;
    default:
        printf( "%%ODS2-W-ERRDEV, Error scanning devices: %s\n",
                getmsg( status, MSG_TEXT ) );
        break;
    }
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
    fprintf( fp, "mount DUB0:\n" );
    fprintf( fp, "The device can be specified as a physical device or logical name\n" );
    fprintf( fp, "You will need the LOGIO privilege\n" );
    fprintf( fp, "Use show DEVICES for a list of available devices.\n" );
    fprintf( fp, "For a list of devices, use the DCL SHOW DEVICE command.\n\n" );

    return;
}


/*************************************************************** phyio_path() */

#define MAXPATH 255

char *phyio_path( const char *filnam ) {

    size_t n;
    unsigned short resultant_path[1 + ( ( MAXPATH + 1 ) / 2)];
    char *path;
    unsigned long context, sts, flags;
    struct dsc$descriptor filespec = {
        0, DSC$K_DTYPE_T, DSC$K_CLASS_S, NULL
    };
    struct dsc$descriptor resultant_filespec = {
        MAXPATH, DSC$K_DTYPE_T, DSC$K_CLASS_VS, (char *) resultant_path
    };

    path = NULL;
    if ( filnam != NULL ) {
        filespec.dsc$w_length = strlen( filnam );
        filespec.dsc$a_pointer = (char *) filnam;
        *resultant_path = 0;
        context = 0;
        flags = LIB$M_FIL_NOWILD;
        sts = lib$find_file( &filespec, &resultant_filespec, &context,
                             NULL, NULL, &sts, &flags );
        lib$find_file_end( &context );
        if ( sts & STS$M_SUCCESS ) {
            n = *resultant_path;
            path = (char *) malloc( n + 1 );
            if ( path != NULL ) {
                memcpy( path, resultant_path + 1, n );
                path[n] = '\0';
            }
        }
    }
    return path;
}

/*************************************************************** phyio_init() */

unsigned phyio_init( struct DEV *dev ) {

    unsigned long sts;
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
            switch( errno ) {
            case EEXIST:
                printf( "%%ODS2-E-EXISTS, File %s already exists, not superseded\n",
                        virtual );
                sts = SS$_DUPFILENAME;
                break;
            default:
                printf( "%%ODS2-E-OPENERR, Open %s: %s\n",
                        virtual, strerror( errno ) );
            }
            return SS$_NOSUCHFILE;
        }

        dev->handle = vmsfd;

        fab = cc$rms_fab;                       /* Make this a real FAB (bid and bln) */
        fab.fab$l_fna = (char *) virtual;
        fab.fab$b_fns = strlen( virtual );
        sts = sys$open( &fab );                 /* Lookup file, get file size */
        if ( sts & STS$M_SUCCESS ) {
            dev->sectors = fab.fab$l_alq;
            sys$close( &fab );
            if ( sizeof( off_t ) < 8 && dev->sectors > 0x3FFFFF ) {
                close( vmsfd );
                dev->handle = -1;
                return SS$_OFFSET_TOO_BIG;      /* Sorry, need 64-bit off_t */
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
        devdsc.dsc$w_length  = strlen( dev->devnam );
        devdsc.dsc$a_pointer = dev->devnam;
        sts = sys$getdviw( EFN$C_ENF, 0, &devdsc, &dvi_itmlst, iosb, 0, 0, 0 );
        if ( !( sts & STS$M_SUCCESS ) ) {
            return sts;
        }
        if ( devclass != DC$_DISK ) {   /* If not a disk, return an error */
            return SS$_IVDEVNAM;
        }
        dev->sectors = cylinders * tracks * sectors;
        if ( !( devchar & DEV$M_MNT ) ) {
#if 0
/*
 *  This code will mount the disk if it's not already mounted.  However,
 *  there's no good way to ensure we dismount a disk we mounted (no
 *  easy way to walk the list of devices at exit), so it's been #ifdefed
 *  out.  If you enable it, the "mount" command will mount the disk, but
 *  it'll stay mounted by the process running ODS2, even after image exit.
 */
            unsigned long mntflags[2];
            struct ITMLST mnt_itmlst[3] = {
                { 0,                  MNT$_DEVNAM,    &devname,  NULL },
                { sizeof( mntflags ), MNT$_FLAGS,     &mntflags, NULL },
                { 0, 0, 0, 0 }
            };
            mnt_itmlst[0].length = strlen( devname );
            mntflags[0] = MNT$M_FOREIGN | MNT$M_NOASSIST | MNT$M_NOMNTVER;
            if ( !( dev->access & MOU_WRITE ) ) {
                mntflags[0] |= MNT$M_NOWRITE;
            }
            mntflags[1] = 0;
            sts = sys$mount( &mnt_itmlst );
            if ( !( sts & STS$M_SUCCESS ) && ( dev->access & MOU_WRITE ) ) {
                dev->access &= ~MOU_WRITE;
                mntflags[0] |= MNT$M_NOWRITE;
                sts = sys$mount( &mnt_itmlst );
            }
#else
            sts = SS$_DEVNOTMOUNT;
#endif
        }
        if ( sts & STS$M_SUCCESS ) {
            sts = sys$assign( &devdsc, &dev->handle, 0, 0, 0, 0 );
        }
    }

    return sts;
}

/*************************************************************** phyio_done() */

unsigned phyio_done( struct DEV *dev ) {

    if ( dev->access & MOU_VIRTUAL ) {
        close( dev->handle );
        virt_device( dev->devnam, NULL );
    } else {
        sys$dassgn( dev->handle );
    }
    dev->handle = 0;
    return SS$_NORMAL;
}

/*************************************************************** phyio_read() */

static unsigned phyio_read( struct DEV *dev, unsigned block, unsigned length,
                            char *buffer ) {

#if DEBUG
    printf("Phyio read block: %d into %x (%d bytes)\n",block,buffer,length);
#endif
    read_count++;
    if ( dev->access & MOU_VIRTUAL ) {
        int res;
        if ( ( res = lseek( dev->handle, block * 512, SEEK_SET ) ) < 0 ) {
            perror( "lseek " );
            printf( "lseek failed %d\n", res );
            return SS$_PARITY;
        }
        if ( ( res = read( dev->handle, buffer, length ) ) != length ) {
            if( res == 0 ) {
                return SS$_ENDOFFILE;
            }
            perror( "read " );
            printf( "read failed %d\n", res );
            return SS$_PARITY;
        }
        return SS$_NORMAL;
    } else {
        return sys$qiow( 1, dev->handle, IO$_READLBLK, NULL, 0, 0,
                         buffer, length, block, 0, 0, 0 );
    }
}

/************************************************************** phyio_write() */

static unsigned phyio_write( struct DEV *dev, unsigned block, unsigned length,
                             const char *buffer ) {

#if DEBUG
    printf("Phyio write block: %d from %x (%d bytes)\n",block,buffer,length);
#endif
    write_count++;
    if ( dev->access & MOU_VIRTUAL ) {
        int res;
        if ( ( res = lseek( dev->handle, block * 512, 0 ) ) < 0 ) {
            perror( "lseek " );
            printf( "lseek failed %d\n", res );
            return SS$_PARITY;
        }
        if ( ( res = write( dev->handle, buffer, length ) ) != length ) {
            perror( "write " );
            printf( "write failed %d\n", res );
            return SS$_PARITY;
        }
        return SS$_NORMAL;
    } else {
        return sys$qiow( 1, dev->handle, IO$_WRITELBLK, NULL, 0, 0,
                         buffer, length, block, 0, 0, 0 );
    }
}
