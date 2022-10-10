/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com
 *
 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors.   This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
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
static uint32_t alq;

#define xtnd_truncate    OPT_GENERIC_1
#define xtnd_allocate    OPT_GENERIC_2

qual_t
extendquals[] = { {"allocate", xtnd_allocate, 0, DV(&alq),
                   "commands extend qual_allocate"},
                  {"truncate", xtnd_truncate, 0, NV,
                   "commands extend qual_truncate"},

                  {NULL,       0,             0, NV, NULL }
};

param_t
extendpars[] = { {"filespec", REQ, FSPEC, NOPA, "commands extend filespec"},

                 {NULL,       0,   0,     NOPA, NULL}
};

DECL_CMD(extend) {
    vmscond_t sts;
    options_t options;
    struct FAB fab;

    UNUSED(argc);

    fab = cc$rms_fab;

    alq = 0;
    if( $FAILS(sts = checkquals( &options, 0, extendquals, qualc, qualv )) ) {
        return sts;
    }

    if( !(~options & (xtnd_allocate|xtnd_truncate)) )
        return printmsg( EXTEND_CONFLQUAL, 0, qstyle_s, qstyle_s );

    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = (uint8_t)strlen(fab.fab$l_fna);
    fab.fab$b_fac = FAB$M_UPD;

    if( options & xtnd_truncate )
        fab.fab$l_fop |= FAB$M_TEF;

    if( $SUCCESSFUL(sts = sys_open( &fab )) ) {
        if( alq ) {
            fab.fab$l_alq = alq;
            sts = sys_extend(&fab);
        }
        sys_close( &fab );
        fab.fab$l_alq = 0;
        if( $SUCCESSFUL(sts) ) {
            sts = sys_open( &fab );
            sys_close( &fab );
        }
    }
    if( $FAILED(sts) ) {
        sts = printmsg(sts, 0);
    } else {
        sts = printmsg( EXTEND_NEWSIZE, 0, fab.fab$l_alq );
    }

    return sts;
}
