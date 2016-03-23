/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_CREATECMD )
#define DEBUG DEBUG_CREATECMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

#include "f11def.h"

/******************************************************************* dotest() */

param_t createpars[] = { {"parameter", REQ, STRING, NOPA, "for create."},
                       { NULL, 0, 0, NOPA, NULL }
};

#if 0
extern struct VCB *test_vcb;
#endif

/* docreate: you don't want to know! */

DECL_CMD(create) {
    unsigned sts = 0;
    struct fiddef fid;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

#if 0 /* Needs to call RMS, etc */
    sts = update_create( test_vcb, NULL, "Test.File", &fid, NULL );
#else
    memset( &fid, 0, sizeof( struct fiddef ) );
#endif
    printf( "Create status of %d (%s)\n", sts, argv[1] );
    return sts;
}



