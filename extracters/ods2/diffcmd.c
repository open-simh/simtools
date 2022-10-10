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

#if !defined( DEBUG ) && defined( DEBUG_DIFFCMD )
#define DEBUG DEBUG_DIFFCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"
#include "phyio.h"

/******************************************************************* dodiff() */

/* dodiff: a simple file difference routine */

static uint32_t maxdiffs;

#define diff_maxdiff OPT_GENERIC_1
#define diff_binary  OPT_GENERIC_2
qual_t
diffquals[] = {{"maximum_differences", diff_maxdiff, 0, DV(&maxdiffs), "commands difference qual_maxdiff"},
               {"nomaximum_differences", 0, diff_maxdiff, NV, NULL },
               {"binary", diff_binary, 0, NV, "commands difference qual_binary"},

               { NULL, 0, 0, NV, NULL }
};

param_t
diffpars[] = { {"Files-11_filespec", REQ, FSPEC, NOPA, "commands difference Files-11_spec"},
               {"local_filespec", REQ, FSPEC, NOPA, "commands difference local_spec"},
               { NULL,            0,   0,     NOPA, NULL }
};

DECL_CMD(diff) {
    vmscond_t sts;
    options_t options;
    uint32_t records, diffs = 0;
    char *rec, *cpy = NULL, *buf = NULL;
    size_t bufsize = 80;
    char *name;
    FILE *tof;
    struct FAB fab = cc$rms_fab;
    struct RAB rab = cc$rms_rab;
    struct NAM nam = cc$rms_nam;
    char rsname[NAM$C_MAXRSS+1];

    UNUSED(argc);

    if( $FAILS(sts = checkquals( &options, 0, diffquals, qualc, qualv )) )
        return sts;

    name = get_realpath( argv[2] );
    records = 0;

    nam.nam$l_rsa = rsname;
    nam.nam$b_rss = (uint8_t)(sizeof( rsname) - 1);

    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = (uint8_t)strlen( fab.fab$l_fna );

    if ( (tof = openf( name? name: argv[2], "r" )) == NULL ) {
        int err;

        err = errno;
        printmsg( DIFF_OPENIN, 0, name? name: argv[2] );
        sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, DIFF_OPENIN, strerror(err) );
        free( name );
        return sts;
    }

    if( $SUCCESSFUL(sts = sys_open( &fab )) ) {
        rab.rab$l_fab = &fab;
        nam.nam$l_rsa[nam.nam$b_rsl] = '\0';
        if( $SUCCESSFUL(sts = sys_connect( &rab )) ) {
            if( (rec = malloc( MAXREC + 2 )) == NULL ) {
                int err;

                err = errno;
                sts = SS$_INSFMEM;
                printmsg( sts, 0 );
                sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, sts, strerror(err) );
            } else {
                rab.rab$l_ubf = rec;
                rab.rab$w_usz = MAXREC;
                while( $SUCCESSFUL(sts = sys_get( &rab )) ) {
                    rec[rab.rab$w_rsz] = '\0';
                    ++records;
                    cpy = fgetline( tof, FALSE, &buf, &bufsize );
                    if( cpy == NULL ||
                        rab.rab$w_rsz != strlen( cpy ) ||
                        memcmp( rec, cpy, rab.rab$w_rsz ) != 0 ) {

                        if( options & diff_binary )
                            sts = printmsg( DIFF_DIFFER, 0, records );
                        else
                            printf( "************\n"
                                    "File %s\n"
                                    "%5u %.*s\n"
                                    "******\n"
                                    "File %s\n"
                                    "%5u %s\n"
                                    "************\n",
                                    rsname,
                                    records, rab.rab$w_rsz, rec,
                                    name,
                                    records, cpy? cpy: "<EOF>" );
                        if( (++diffs > maxdiffs && (options & diff_maxdiff)) || cpy == NULL )
                            break;
                    }
                    cpy = NULL;
                }
                free( rec );
                rec = cpy = NULL;
            }
            sys_disconnect(&rab);
        }
        sys_close(&fab);
    } else {
        printmsg( DIFF_OPENIN, 0, argv[1] );
        sts = printmsg( sts, MSG_CONTINUE, DIFF_OPENIN );
    }
    if( sts == RMS$_EOF ) {
        if( (cpy = fgetline( tof, FALSE, &buf, &bufsize )) == NULL )
            sts = SS$_NORMAL;
        else {
            if( diffs < maxdiffs )
                sts = printmsg( DIFF_DIFFER, 0, records );
        }
    }

    if( buf != NULL ) free( buf );
    fclose(tof);

    if( $SUCCESSFUL(sts) ) {
        sts = printmsg( DIFF_COMPARED, 0, records );
    } else if( !$MATCHCOND(sts, DIFF_DIFFER) ) {
        sts = printmsg( sts, 0, rab.rab$l_stv );
    }
    free( name );
    return sts;
}
