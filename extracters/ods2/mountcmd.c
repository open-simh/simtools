/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_MOUNTCMD )
#define DEBUG DEBUG_MOUNTCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

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


qual_t mouquals[] = { {DT_NAME, 0, MOU_DEVTYPE, CV(NULL), "Drive type (DEC model name) "},
                      {"image",    MOU_VIRTUAL, 0,         NV, "Mount a disk image file", },
                      {"log",            MOU_LOG,     0, NV, "-Show progress"},
                      {"nolog",          0,     MOU_LOG, NV, NULL},
                      {"readonly", 0,           MOU_WRITE, NV, "Only allow reading from volume"},
#ifdef USE_VHD
                      {"snapshots_of",       MOU_VIRTUAL | MOU_WRITE | mou_snapshot, PHY_CREATE, SV(&parents),
                       "Create snapshot(s) of the (existing) VHD disk(s) specified by this qualifier." },
#endif
                      {"virtual",  MOU_VIRTUAL, 0,         NV, NULL, },
                      {"write",    MOU_WRITE,   0,         NV, "Allow writing to volume", },
                      {NULL,       0,           0,         NV, NULL } };

param_t moupars[] = { {"volumes", REQ, LIST, NOPA,
                       "devices or disk image(s) of volume set in RVN order separated by comma"
    },
                      {"labels", OPT, LIST, NOPA, "volume labels in RVN order separated by comma" },
                      { NULL,    0,   0,    NOPA, NULL }
};

/******************************************************************* domount() */

DECL_CMD(mount) {
    int sts = 1, devices = 0;
    char **devs = NULL, **labs = NULL;
    int options;

#ifdef USE_VHD
    int nparents = 0;
    size_t i;
    char  **parfiles = NULL;
#endif

    options = checkquals( 0, mouquals, qualc, qualv );
    if( options == -1 )
        return SS$_BADPARAM;

    UNUSED(argc);

    if( (devices = parselist( &devs, 0, argv[1], "devices")) < 0 )
        return SS$_BADPARAM;
    if( parselist( &labs, devices, argv[2], "labels") < 0 ) {
        free( devs );
        return SS$_BADPARAM;
    }
#ifdef USE_VHD
    if( options & mou_snapshot ) {
        nparents = parselist( &parfiles, devices, parents, "parent VHD files " );
        if( nparents != devices ) {
            printf( "%%ODS2-E-NOTSAME, You must specify the same number of existing files (%u) with %cSNAPSHOT as filenames to create (%u)\n",
                    nparents, vms_qual? '/': '-', devices );
            free( parfiles );
            return SS$_BADPARAM;
        }
        for( i = 0; i < (size_t)nparents; i++ ) {
            sts = phyvhd_snapshot( devs[i], parfiles[i] );
            if( !(sts & STS$M_SUCCESS) ) {
                if( (sts & STS$M_COND_ID) != (SS$_DUPFILENAME & STS$M_COND_ID) ) {
                    (void) unlink( devs[i] );
                }
                while( i > 0 ) {
                    --i;
                    (void) unlink( devs[i] );
                }
                free( parfiles );
                return SS$_BADPARAM;
            }
        }
        if( options & MOU_LOG ) {
            for( i = 0; i < (size_t)nparents; i++ )
                printf( "%%ODS2-I-SNAPOK, %s created from %s\n",
                        devs[i], parfiles[i] );
        }
        options |= MOU_WRITE;
        free( parfiles );
        parfiles = NULL;
    }
#endif

    if (devices > 0) {
        sts = mount( options | MOU_LOG, devices, devs, labs );
        if( !(sts & STS$M_SUCCESS) )
            printf("%%ODS2-E-MOUNTERR, Mount failed with %s\n", getmsg(sts, MSG_TEXT));
    }

    free( devs );
    free( labs );
    return sts;
}

void mounthelp(void) {
    printf( "\n" );
    printf( "You can mount a volume(-set) from either physical devices\n" );
    printf( "such a CDROM or hard disk drive, or files containing an\n" );
    printf( "image of a volume, such as a .ISO file or simulator disk\n\n" );
    printf( "To mount a disk image, use the %cimage qualifier and\n",
            (vms_qual? '/': '-') );
    printf( "specify the filename as the parameter.\n\n" );
    printf( "If the filename contains %c, specify it in double quotes\n\n",
            (vms_qual? '/': '-') );
    printf( "Mount will assign a virtual device name to each volume.\n" );
    printf( "You can select a virtual device name using the format\n" );
    printf( "  dka100=my_files.iso\n\n" );
    printf( "To mount a physical device, use the format:\n" );
    phyio_help(stdout);
    printf( "To mount a volume set, specify all the members in RVN order\n" );
    printf( "as a comma-separated list.\n\n" );
    printf( "If you specify a list of volume labels, they must be in\n" );
    printf( "the same order as the volumes, and each must match the label\n" );
    printf( "stored in the data\n" );
#ifdef USE_VHD
    printf( "\nTo create and mount a snapshot of a VHD-based volume set, use the %csnapshot_of qualifier.\n", vms_qual? '/': '-' );
    printf( "Specify the existing volumes' filenames as an argument to %csnapshot, and\n", vms_qual? '/': '-' );
    printf( "list the corresponding filenames to be created as the mount parameter.\n" );
#endif

    return;
}
