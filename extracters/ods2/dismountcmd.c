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

#if !defined( DEBUG ) && defined( DEBUG_DISMOUNTCMD )
#define DEBUG DEBUG_DISMOUNTCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

#include "access.h"
#include "device.h"

/******************************************************************* dodismount() */

qual_t
dmoquals[] = { {"log",   MOU_LOG, 0,       NV,
                "-commands dismount qual_log"},
               {"nolog", 0,       MOU_LOG, NV, NULL },

               {NULL,    0,       0,       NV, NULL }
};

param_t
dmopars[] = { {"drive_name", REQ, STRING, NOPA,
               "commands dismount drive"},

              {NULL,           0,   0,      NOPA, NULL}
};

DECL_CMD(dismount) {
    vmscond_t sts;
    options_t options;
    struct DEV *dev;

    UNUSED(argc);

    if( $FAILS(sts = checkquals( &options, MOU_LOG, dmoquals, qualc, qualv )) ) {
        return sts;
    }
    if( $SUCCESSFUL(sts = device_lookup( (uint32_t)strlen(argv[1]),
                                         argv[1], FALSE, &dev )) ) {
        device_done( dev );
        if( dev->vcb != NULL ) {
            sts = dismount( dev->vcb, options );
        } else {
            sts = SS$_DEVNOTMOUNT;
        }
    }

    return sts;
}
