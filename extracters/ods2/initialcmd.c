/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com
 *
 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors. This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.   This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
 */

#if !defined( DEBUG ) && defined( DEBUG_INITIALCMD )
#define DEBUG DEBUG_INITIALCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "compat.h"
#include "cmddef.h"

#include "access.h"
#include "device.h"
#include "initvol.h"
#include "phyvirt.h"

/******************************************************************* doinitial() */

static struct NEWVOL initpars;

#define init_beg      OPT_GENERIC_1
#define init_mid      OPT_GENERIC_2
#define init_blk      OPT_GENERIC_3
#define init_mount    OPT_GENERIC_4
#define init_disp     OPT_GENERIC_5
#define init_confirm  OPT_GENERIC_6
#define init_uic      OPT_GENERIC_7
#define init_dtype    OPT_GENERIC_8

static qual_t
idxkwds[] =
    { {"beginning",       init_beg, init_mid|init_blk, NV,
       "commands initialize qual_index begin"},
      {"block",           init_blk, init_beg|init_mid, DV(&initpars.indexlbn),
       "commands initialize qual_index block"},
      {"middle",         init_mid, init_beg|init_blk, NV,
       "commands initialize qual_index middle"},
      {NULL,             0,        0,                 NV, NULL}
    };

qual_t
iniquals[] =
    { {DT_NAME,           0,           MOU_DEVTYPE, CV(NULL),
       "commands initialize qual_media"},
      {"bootblock",       0,           0, DV(&initpars.bootlbn),
       "commands initialize qual_bootblock"},
      {"cluster_size",    0,           0, DV(&initpars.clustersize),
       "commands initialize qual_clustersize" },
      {"confirm",         init_confirm, 0,  NV,
       "-commands initialize qual_confirm"},
      {"noconfirm",       0,           init_confirm,
       NV, NULL},
      {"create",          init_disp|PHY_CREATE,
                                       0,  NV,
       "commands initialize qual_create" },
      {"directories",     0,           0,  DV(&initpars.directories),
       "commands initialize qual_directories" },
      {"file_protection", 0,           0, PV(&initpars.fileprot),
       "commands initialize qual_fileprot"},
      {"headers",         0,           0,  DV(&initpars.headers),
       "commands initialize qual_headers" },
      {"extension",       0,           0, DV(&initpars.extension),
       "commands initialize qual_extension" },
      {"windows",         0,           0, DV(&initpars.windows),
       "commands initialize qual_windows" },
      {"index",           0,           init_beg|init_mid|init_blk, KV(idxkwds),
       "commands initialize qual_index"},
      {"image",           init_dtype | MOU_VIRTUAL,
                                       0, NV,
       "commands initialize qual_image", },
      {"device",          init_dtype,  MOU_VIRTUAL, NV,
       "commands initialize qual_device", },
      {"log",             MOU_LOG,     0, NV, "-commands initialize qual_log"},
      {"nolog",           0,           MOU_LOG, NV, NULL},
      {"replace",         init_disp,   PHY_CREATE,
                                          NV,
       "commands initialize qual_replace" },
      {"mount",           init_mount,  0,  NV,
       "-commands initialize qual_mount"},
      {"nomount",         0,           init_mount,  NV, NULL },
      {"virtual",         init_dtype | MOU_VIRTUAL, 0, NV, NULL, },
      {"maximum_files",   0,           0, DV(&initpars.maxfiles),
       "commands initialize qual_maxfiles" },
      {"owner_uic",      init_uic,     0, UV(&initpars.useruic),
       "commands initialize qual_owneruic"},
      {"protection",      0,           0, PV(&initpars.volprot),
       "commands initialize qual_protection"},
      {"user_name",       0,           0, SV(&initpars.username),
       "commands initialize qual_username"},

      { NULL,              0,          0, NV, NULL }
    };

param_t
inipars[] =
    { {"device", REQ, FSPEC, NOPA, "commands initialize devspec" },
      {"labels", REQ, STRING, NOPA, "commands initialize volspec" },

      { NULL,    0,   0,    NOPA, NULL }
    };

DECL_CMD(initial) {
    vmscond_t sts = SS$_NORMAL;
    options_t options;
    char *devname = NULL;

    UNUSED(argc);

    devname = argv[1];

    memset( &initpars, 0, sizeof( initpars ) );
    initpars.extension = 5;
    initpars.windows = 7;

    sts = checkquals( &options, init_confirm | init_mid | init_mount,
                      iniquals, qualc, qualv );

    do {
        size_t len;
        struct DEV *dev;
        char *p;

        if( $FAILED( sts ) ) {
            break;
        }
        options |= MOU_WRITE;
        if( !(options & init_dtype) && (p = strchr( devname, '.' )) ) {
            len = strlen( p+1 );
            if( len >= 1 && len == strspn( p+1, "0123456789" ".;-_"
                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "abcdefghijklmnopqrstuvwxyz" ) )
            options |= MOU_VIRTUAL;
        }
        if( options & MOU_VIRTUAL ) {
            if( !(options & init_disp) )             /* Disposition (/create/replace)? */
                options |= PHY_CREATE;
        } else {
            if( options & init_disp ) {
                printmsg( INIT_INVOPT, 0, qstyle_s, qstyle_s );
            }
        }

        if( options & init_mid )
            initpars.indexlbn = INIT_INDEX_MIDDLE;
        if( options & init_beg )
            initpars.indexlbn = 0;
        if( (options & init_blk) && initpars.indexlbn == INIT_INDEX_MIDDLE )
            initpars.indexlbn--;

        if( options & MOU_LOG )
            initpars.options |= INIT_LOG;
        if( options & MOU_VIRTUAL )
            initpars.options |= INIT_VIRTUAL;
        if( options & init_uic )
            initpars.options |= INIT_OWNUIC;

        initpars.devtype = disktype[(options & MOU_DEVTYPE) >> MOU_V_DEVTYPE].name;

        len = strlen( argv[2] );
        if( len > 12 || !*argv[2] ||
            strspn( argv[2], VOL_ALPHABET ) != len ) {
            sts = printmsg( INIT_BADVOLLABEL, 0 );
            break;
        }
        initpars.label = argv[2];

        if( (options & init_confirm) &&
            $FAILS(sts = confirm_cmd( (options & MOU_VIRTUAL)?
                                     INIT_CONFIRM_VIRT: INIT_CONFIRM_PHY,
                                     devname, initpars.devtype )) )
            break;

        if( $FAILS(sts = virt_open( &devname, options, &dev )) )
            break;

        if( !(dev->access & MOU_WRITE) ) {
            sts = SS$_WRITLCK;
            virt_close( dev );
            break;
        }
        initpars.devnam = devname;
        if( $FAILS(sts = initvol( dev, &initpars )) ) {
            virt_close( dev );
            break;
        }
        if( $FAILS(sts = virt_close( dev )) )
            break;

        if( options & init_mount ) {
            sts = mount( (options & (MOU_VIRTUAL|MOU_DEVTYPE|MOU_WRITE)) | MOU_LOG,
                         1, argv+1, argv+2 );
        }
    } while( 0 );

    if( $SUCCESSFUL( sts )  )
        return sts;

    sts = printmsg( sts, 0 );

    if( (options & (MOU_VIRTUAL|PHY_CREATE)) == (MOU_VIRTUAL|PHY_CREATE) &&
        !( $MATCHCOND(sts, SS$_DUPFILENAME) ||
           $MATCHCOND(sts, SS$_IVDEVNAM) ) ) {
        (void) Unlink( argv[1] );
    }

    return sts;
}
