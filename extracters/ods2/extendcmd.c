/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_EXTENDCMD )
#define DEBUG DEBUG_EXTENDCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"


/***************************************************************** doextend() */

/* more test code... */

param_t extendpars[] = { {"ods-2_filespec", REQ, VMSFS, NOPA, "for file on ODS-2 volume."},
                                     { NULL, 0, 0, NOPA, NULL }
};

DECL_CMD(extend) {
    int sts;
    struct FAB fab = cc$rms_fab;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    fab.fab$b_fac = FAB$M_UPD;
    sts = sys_open( &fab );
    if ( sts & STS$M_SUCCESS ) {
        fab.fab$l_alq = 32;
        sts = sys_extend(&fab);
        sys_close(&fab);
    }
    if ( !( sts & STS$M_SUCCESS ) ) {
        printf("%%EXTEND-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}
