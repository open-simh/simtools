/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_IMPORTCMD )
#define DEBUG DEBUG_IMPORTCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"


/***************************************************************** doimport() */

#define import_binary OPT_GENERIC_1
#define import_log    OPT_GENERIC_2

qual_t importquals[] = { {"ascii",  0,          import_binary, NV,
                          "Transfer file in ascii mode (default)"},
                         {"binary", import_binary, 0,          NV, "Transfer file in binary mode", },
                                     {NULL,     0,          0,          NV, NULL}
};
param_t importpars[] = { {"from_filespec", REQ, LCLFS, NOPA, "for source file."},
                         {"to_filespec",   REQ, VMSFS, NOPA, "for destination file."},
                         { NULL, 0, 0, NOPA, NULL }
};

DECL_CMD(import) {
    int options;
    unsigned sts;
    char *name;
    FILE *fromf;
    struct FAB fab = cc$rms_fab;
    struct RAB rab = cc$rms_rab;

    UNUSED(argc);

    options = checkquals( 0, importquals, qualc, qualv );
    if( options == -1 )
        return SS$_BADPARAM;

    sts = SS$_BADFILENAME;
    name = argv[1]; /*unquote( argv[1] );*/
    if ( *name == '\0' ) {
        return sts;
    }
    fromf = openf( name, (options & import_binary)? "rb": "r" );
    if( fromf == NULL ) {
        printf( "%%ODS2-E-OPENERR, Can't open %s\n", name );
        perror( "  - " );
        return sts;
    }
    fab.fab$b_fac = FAB$M_PUT;
    fab.fab$l_fop = FAB$M_OFP | FAB$M_TEF;
    fab.fab$b_org = FAB$C_SEQ;
    if( options & import_binary ) {
        fab.fab$w_mrs = 512;
        fab.fab$b_rfm = FAB$C_FIX;
    } else {
        fab.fab$b_rat = 0;
        fab.fab$b_rfm = FAB$C_STM;
    }

    fab.fab$l_fna = argv[2];
    fab.fab$b_fns = strlen( fab.fab$l_fna );
    sts = sys_create( &fab );
    if ( sts & STS$M_SUCCESS ) {
        rab.rab$l_fab = &fab;
        sts = sys_connect( &rab );
        if ( sts & STS$M_SUCCESS ) {
            if( options & import_binary ) {
                char *buf;
                size_t len;

                if( (buf = malloc( 512 )) == NULL )
                    sts = SS$_INSFMEM;
                else {
                    rab.rab$l_rbf = buf;
                    while( (len = fread( buf, 1, 512, fromf )) > 0 ) {
                        if( len != 512 )
                            memset( buf + len, 0, 512 - len );
                        rab.rab$w_rsz = len;
                        sts = sys_put( &rab );
                        if( !(sts & STS$M_SUCCESS) ) {
                            break;
                        }
                    }
                    free( buf );
                    buf = NULL;
                }
            } else {
                while ( (rab.rab$l_rbf = fgetline( fromf, TRUE )) != NULL ) {
                    rab.rab$w_rsz = strlen( rab.rab$l_rbf );
                    sts = sys_put( &rab );
                    free( rab.rab$l_rbf );
                    if ( !( sts & STS$M_SUCCESS ) ) {
                        break;
                    }
                }
            }
            sys_disconnect( &rab );
        }
        sys_close( &fab );
    }
    fclose( fromf );
    if ( !(sts & STS$M_SUCCESS) ) {
            printf("%%IMPORT-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }

    return sts;
}
