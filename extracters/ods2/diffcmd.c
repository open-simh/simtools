/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_DIFFCMD )
#define DEBUG DEBUG_DIFFCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

/******************************************************************* dodiff() */

/* dodiff: a simple file difference routine */

param_t diffpars[] = { {"ods-2_filespec", REQ, VMSFS, NOPA, "for file on ODS-2 volume."},
                                   {"local_filespec", REQ, LCLFS, NOPA, "for file on local filesystem."},
                                   { NULL,            0,   0,     NOPA, NULL }
};

DECL_CMD(diff) {
    int sts, records = 0;
    char *rec, *cpy = NULL;
    char *name;
    FILE *tof;
    struct FAB fab = cc$rms_fab;
    struct RAB rab = cc$rms_rab;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    name = argv[1];
    sts = SS$_BADFILENAME;
    if ( *name == '\0' ) {
        return sts;
    }
    records = 0;
    fab.fab$l_fna = name;
    fab.fab$b_fns = strlen( fab.fab$l_fna );
    tof = openf( argv[2], "r" );
    if ( tof == NULL ) {
        printf("%%ODS2-E-OPENERR, Could not open file %s\n",name);
        perror( "  - " );
        return SS$_NOSUCHFILE;
    }
    sts = sys_open( &fab );
    if ( sts & STS$M_SUCCESS ) {
        rab.rab$l_fab = &fab;
        sts = sys_connect( &rab );
        if( sts & STS$M_SUCCESS ) {
            if( (rec = malloc( MAXREC + 2 )) == NULL ) {
                perror( "malloc" );
            } else {
                rab.rab$l_ubf = rec;
                rab.rab$w_usz = MAXREC;
                while( (sts = sys_get( &rab )) & STS$M_SUCCESS ) {
                    rec[rab.rab$w_rsz] = '\0';
                    cpy = fgetline( tof, FALSE );
                    if( cpy == NULL ||
                        rab.rab$w_rsz != strlen( cpy ) ||
                        memcmp( rec, cpy, rab.rab$w_rsz ) != 0 ) {

                        printf( "%%DIFF-F-DIFFERENT Files are different!\n" );
                        sts = 4;
                        break;
                    }
                    free( cpy );
                    cpy = NULL;
                    records++;
                }
                if( cpy != NULL ) free( cpy );
                free( rec );
                rec = cpy = NULL;
            }
            sys_disconnect(&rab);
        }
        sys_close(&fab);
    }
    fclose(tof);
    if (sts == RMS$_EOF) sts = SS$_NORMAL;
    if ( sts & STS$M_SUCCESS ) {
        printf( "%%DIFF-I-Compared %d records\n", records );
    } else if ( sts != 4 ) {
        printf("%%DIFF-F-Error %s in difference\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}
