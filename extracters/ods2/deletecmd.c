/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_DELETECMD )
#define DEBUG DEBUG_DELETECMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

/***************************************************************** dodelete() */

#define delete_log OPT_GENERIC_1

qual_t delquals[] = { {"log",   delete_log, 0, NV, "-List name of each file deleted. (Default)"},
                      {"nolog", 0, delete_log, NV, NULL },
                      { NULL,   0, 0, NV, NULL }
};
param_t delpars[] = { {"filespec", REQ, VMSFS, NOPA,
                       "for files to be deleted from ODS-2 volume.  Wildcards are permitted.."},
                      { NULL,      0,   0,     NOPA, NULL }
};

DECL_CMD(delete) {
    int sts = 0;
    char res[NAM$C_MAXRSS + 1], rsa[NAM$C_MAXRSS + 1];
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    int options, filecount = 0;

    UNUSED(argc);

    options = checkquals( delete_log, delquals, qualc, qualv );
    if( options == -1 )
        return SS$_BADPARAM;

    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    sts = sys_parse(&fab);
    if ( sts & STS$M_SUCCESS ) {
        if (nam.nam$b_ver < 2) {
            printf( "%s\n", getmsg( DELETE$_DELVER, MSG_FULL) );
            return SS$_BADPARAM;
        }

        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
            while ( ( sts = sys_search( &fab ) ) & STS$M_SUCCESS ) {
            sts = sys_erase(&fab);
                if ( !( sts & STS$M_SUCCESS ) ) {
                printf("%%DELETE-F-DELERR, Delete error: %s\n",getmsg(sts, MSG_TEXT));
                break;
            } else {
                filecount++;
                if( options & delete_log ) {
                    rsa[nam.nam$b_rsl] = '\0';
                    printf("%%DELETE-I-DELETED, Deleted %s\n",rsa);
                }
            }
        }
        if( sts == RMS$_NMF ) sts = SS$_NORMAL;
    }
        if(  sts & STS$M_SUCCESS ) {
        if( filecount < 1 ) {
            printf( "%%DELETE-W-NOFILES, no files deleted\n" );
        }
    } else {
        printf( "%%DELETE-F-ERROR Status: %s\n", getmsg(sts, MSG_TEXT) );
    }

    return sts;
}
