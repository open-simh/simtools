/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
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

param_t dmopars[] = { {"drive_letter", REQ, STRING, NOPA, "Drive containing volume to dismount", },
                                  {NULL,           0,   0,      NOPA, NULL }
};

DECL_CMD(dismount) {
    struct DEV *dev;
    int sts;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    sts = device_lookup( strlen(argv[1]), argv[1], FALSE, &dev );
    if( sts & STS$M_SUCCESS ) {
        device_done( dev );
        if( dev->vcb != NULL ) {
            sts = dismount( dev->vcb );
        } else {
            sts = SS$_DEVNOTMOUNT;
        }
    }
    if( !(sts & STS$M_SUCCESS) )
        printf("%%DISMOUNT-E-STATUS Error: %s\n",getmsg(sts, MSG_TEXT));
    return sts;
}
