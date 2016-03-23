/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_INITIALCMD )
#define DEBUG DEBUG_INITIALCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

#include "access.h"
#include "device.h"
#include "initvol.h"
#include "phyvirt.h"

/******************************************************************* doinitial() */

static struct NEWVOL initpars;

#define init_beg OPT_GENERIC_1
#define init_mid OPT_GENERIC_2
#define init_blk OPT_GENERIC_3
#define init_mount OPT_GENERIC_4

qual_t idxkwds[] = { {"beginning",       init_beg, init_mid|init_blk, NV,
                                  "Place index file in lowest available LBNs"},
                                 {"block",           init_blk, init_beg|init_mid, DV(&initpars.indexlbn),
                                  "Place index file near specific LBN"},
                                 {"middle",         init_mid, init_beg|init_blk, NV,
                                  "Place index file near middle of volume (default)"},
                                 {NULL,             0,        0,                 NV, NULL}
};

qual_t iniquals[] = { {DT_NAME, 0, MOU_DEVTYPE, CV(NULL), "Drive type (DEC model name) "},
                                  {"cluster_size",   0,           0,  DV(&initpars.clustersize),
                                   "Cluster size (blocks)" },
                                  {"create",         PHY_CREATE|MOU_VIRTUAL,  0,  NV, "Create a new image file" },
                                  {"directories",    0,           0,  DV(&initpars.directories),
                                   "Directory entries to pre-allocate" },
                                  {"headers",        0,           0,  DV(&initpars.headers),
                                   "File headers to pre-allocate" },
                                  {"index",          0,           init_beg|init_mid|init_blk, KV(idxkwds),
                                   "Index file placement hint"},
                                  {"image",          PHY_CREATE|MOU_VIRTUAL, 0,  NV,
                                   "Initialize a disk image file", },
                                  {"log",            MOU_LOG,     0, NV, "-Show progress"},
                                  {"nolog",          0,     MOU_LOG, NV, NULL},
                                  {"replace",        MOU_VIRTUAL, PHY_CREATE,    NV,
                                   "Replace filesystem in existing image file" },
                                  {"mount",          init_mount, 0, NV, "-Mount volume after initializing it"},
                                  {"nomount",        0, init_mount, NV, NULL },
                                  {"virtual",        PHY_CREATE|MOU_VIRTUAL, 0,  NV, NULL, },
                                  {"maximum_files",  0,           0,  DV(&initpars.maxfiles),
                                   "Maximum number of files volume can (ever) contain" },
                                  {"owner_uic",       0,          0, SV(&initpars.useruic),
                                   "Volume owner's UIC, default [1,1]"},
                                  {"user_name",       0,          0, SV(&initpars.username),
                                   "Volume owner's username, default yours"},
                                  {NULL,       0,           0,        NV, NULL } };

param_t inipars[] = { {"device", REQ, LCLFS, NOPA,
                       "for device or disk image to be initialized.\n          "
                       "ALL EXISTING DATA WILL BE LOST."
    },
                      {"labels", REQ, LCLFS, NOPA, "volume label for new volume" },
                      { NULL,    0,   0,    NOPA, NULL }
};

DECL_CMD(initial) {
    int sts = SS$_NORMAL;
    char *devname = NULL;
    size_t len;
    struct DEV *dev;
    int options;

    UNUSED(argc);

    devname = argv[1];

    options = checkquals( PHY_CREATE | init_mid | init_mount, iniquals, qualc, qualv );
    do {

        if( options == -1 ) {
            sts = SS$_BADPARAM;
            break;
        }
        options |= MOU_WRITE;
        if( !(options & MOU_VIRTUAL) )
            options &= ~PHY_CREATE;

        if( options & init_mid )
            initpars.indexlbn = INIT_INDEX_MIDDLE;
        if( options & init_beg )
            initpars.indexlbn = 0;
        if( (options & init_blk) && initpars.indexlbn == INIT_INDEX_MIDDLE )
            initpars.indexlbn--;

        initpars.options = 0;
        if( options & MOU_LOG )
            initpars.options |= INIT_LOG;
        if( options & MOU_VIRTUAL )
            initpars.options |= INIT_VIRTUAL;

        initpars.devtype = disktype[(options & MOU_DEVTYPE) >> MOU_V_DEVTYPE].name;

        len = strlen( argv[2] );
        if( len > 12 || !*argv[2] ||
            strspn( argv[2], VOL_ALPHABET ) != len ) {
            printf( "Volume label must be between 1 and 12 alphanumeric characters long\n" );
            sts = SS$_BADPARAM;
            break;
        }
        initpars.label = argv[2];

        if( !((sts = virt_open( &devname, options, &dev )) & STS$M_SUCCESS) )
            break;

        if( !(dev->access & MOU_WRITE) ) {
            sts = SS$_WRITLCK;
            virt_close( dev );
            break;
        }
        initpars.devnam = devname;
        if( !((sts = initvol( dev, &initpars )) & STS$M_SUCCESS) ) {
            virt_close( dev );
            break;
        }
        if( !((sts = virt_close( dev )) & STS$M_SUCCESS) )
            break;

        if( options & init_mount ) {
            sts = mount( (options & (MOU_VIRTUAL|MOU_DEVTYPE|MOU_WRITE)) | MOU_LOG,
                         1, argv+1, argv+2 );
        }
    } while( 0 );

    initpars.username =
        initpars.useruic = NULL;

    if( sts & STS$M_SUCCESS )
        return sts;

    printf( "%%ODS2-E-FAILED, Initialize failed: %s\n", getmsg( sts, MSG_TEXT ) );

    if( (options & (MOU_VIRTUAL|PHY_CREATE)) == (MOU_VIRTUAL|PHY_CREATE) &&
        !( (sts & STS$M_COND_ID) == (SS$_DUPFILENAME & STS$M_COND_ID) ||
           (sts & STS$M_COND_ID) == (SS$_IVDEVNAM & STS$M_COND_ID) ) ) {
        (void) unlink( argv[1] );
    }

    return sts;
}
