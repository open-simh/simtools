/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors.   This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
 */

#if !defined( DEBUG ) && defined( DEBUG_MOUNTCMD )
#define DEBUG DEBUG_MOUNTCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "compat.h"
#include "cmddef.h"

#include "access.h"
#include "device.h"
#include "phyio.h"
#include "phyvirt.h"

#ifdef USE_VHD
#include "phyvhd.h"

static char *parents = NULL;
#endif

void mounthelp(void);

#define mou_snapshot OPT_GENERIC_1
#define mou_dtype    OPT_GENERIC_2

qual_t
mouquals[] = { {DT_NAME,    0,           MOU_DEVTYPE, CV(NULL),
                "commands mount qual_media"},
               {"image",    mou_dtype | MOU_VIRTUAL,
                                         0,           NV,
                "commands mount qual_image"},
               {"log",      MOU_LOG,     0,           NV,
                "-commands mount qual_log"},
               {"nolog",    0,           MOU_LOG,     NV, NULL},
               {"readonly", 0,           MOU_WRITE,   NV,
                "commands mount qual_readonly"},
#ifdef USE_VHD
               {"snapshots_of",
                            MOU_VIRTUAL | MOU_WRITE | mou_snapshot,
                                         PHY_CREATE,  SV(&parents),
                "commands mount qual_snapshot"},
#endif
               {"device",   mou_dtype,   MOU_VIRTUAL, NV,
                "commands mount qual_device"},
               {"virtual",  mou_dtype | MOU_VIRTUAL, 0,
                                                      NV, NULL},
               {"write",    MOU_WRITE,   0,           NV,
                "-commands mount qual_write"},
               {"nowrite" , 0,           MOU_WRITE,   NV, NULL},

               {NULL,       0,           0,           NV, NULL} };

param_t
moupars[] = { {"volumes",   REQ, LIST, NOPA, "commands mount volumes"},
              {"labels",    OPT, LIST, NOPA, "commands mount labels"},

              { NULL,       0,   0,    NOPA, NULL }
};

/******************************************************************* domount() */

DECL_CMD(mount) {
    vmscond_t sts = 1;
    int devices = 0;
    size_t i;
    char **devs = NULL, **labs = NULL;
    options_t options;

#ifdef USE_VHD
    int nparents = 0;
    char  **parfiles = NULL;
#endif

    UNUSED(argc);

    if( $FAILS(sts = checkquals( &options, MOU_LOG, mouquals, qualc, qualv )) ) {
        return sts;
    }

    if( $FAILS(sts = parselist( &devices, &devs, 0, argv[1])) )
        return sts;
    if( $FAILS(sts = parselist( NULL, &labs, devices, argv[2])) ) {
        free( devs );
        return sts;
    }

    if( !(options & mou_dtype) ) {
        for( i = 0; i < (unsigned)devices; i++ ) {
            size_t len;
            char *p;

            if( (p = strchr( devs[i], '.' )) != NULL ) {
                len = strlen( p+1 );
                if( len >= 1 && len == strspn( p+1, "0123456789" ".;-_"
                                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                               "abcdefghijklmnopqrstuvwxyz" ) )
                    options |= MOU_VIRTUAL;
            }
        }
    }

#ifdef USE_VHD
    if( options & mou_snapshot ) {
        if( $FAILS(sts = parselist( &nparents, &parfiles, devices, parents )) ) {
            free( devs );
            free( labs );
            return sts;
        }
        if( nparents != devices ) {
            sts = printmsg( MOUNT_NOTSAME, 0, nparents, qstyle_c, devices );
            free( parfiles );
            free( devs );
            free( labs );
            return sts;
        }
        for( i = 0; i < (size_t)nparents; i++ ) {
            sts = phyvhd_snapshot( devs[i], parfiles[i] );
            if( $FAILS(sts = phyvhd_snapshot( devs[i], parfiles[i] )) ) {
                if( !$MATCHCOND(sts, SS$_DUPFILENAME) ) {
                    (void) Unlink( devs[i] );
                }
                while( i > 0 ) {
                    --i;
                    (void) Unlink( devs[i] );
                }
                free( parfiles );
                free( devs );
                free( labs );
                return sts;
            }
        }
        if( options & MOU_LOG ) {
            for( i = 0; i < (size_t)nparents; i++ )
                sts = printmsg( MOUNT_SNAPOK, 0, devs[i], parfiles[i] );
        }
        options |= MOU_WRITE;
        free( parfiles );
        parfiles = NULL;
    }
#endif

    if( devices > 0 )
        sts = mount( options, devices, devs, labs );

    free( devs );
    free( labs );
    return sts;
}

/*************************************************************** domounthelp() */

DECL_CMD(mounthelp) {
    UNUSED( argc );
    UNUSED( argv );
    UNUSED( qualc );
    UNUSED( qualv );

    (void) helptopic( 0, "MOUNT", qstyle_s, qstyle_s );

    phyio_help();

    (void) helptopic( 0, "MOUNT VOLSET" );

#ifdef USE_VHD
    (void) helptopic( 0, "MOUNT VHD", qstyle_s, qstyle_s );
#endif

    return SS$_NORMAL;
}
