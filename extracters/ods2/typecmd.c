/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_TYPECMD )
#define DEBUG DEBUG_TYPECMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

/******************************************************************* dotype() */

/* dotype: a file TYPE routine */

param_t typepars[] = { {"filespec", REQ, VMSFS, NOPA, "for file on ODS-2 volume."},
                       { NULL, 0, 0, NOPA, NULL }
};

DECL_CMD(type) {
    int sts;
    int records;
    struct FAB fab = cc$rms_fab;
    struct RAB rab = cc$rms_rab;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    records = 0;

    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    sts = sys_open( &fab );
    if ( sts & STS$M_SUCCESS ) {
        rab.rab$l_fab = &fab;
        sts = sys_connect( &rab );
        if ( sts & STS$M_SUCCESS ) {
            char *rec;

            if( (rec = malloc( MAXREC + 2 )) == NULL )
                sts = SS$_INSFMEM;
            else {
                rab.rab$l_ubf = rec;
                rab.rab$w_usz = MAXREC;
                while( (sts = sys_get( &rab )) & STS$M_SUCCESS ) {
                    unsigned rsz = rab.rab$w_rsz;
                    if( fab.fab$b_rat & PRINT_ATTR ) rec[rsz++] = '\n';
                    rec[rsz++] = '\0';
                    fputs( rec, stdout );
                    records++;
                }
                free( rec );
                rec = NULL;
            }
            sys_disconnect(&rab);
        }
        sys_close(&fab);
        if (sts == RMS$_EOF) sts = SS$_NORMAL;
    }
    if ( !( sts & STS$M_SUCCESS ) ) {
        printf("%%TYPE-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}


