
/* This is part of ODS2, originally written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors.   This is free software; no
 * warranty is offered, and while we believe it to be useful,
 * you use it at your own risk.
 */

#if !defined( DEBUG ) && defined( DEBUG_RENAMECMD )
#define DEBUG DEBUG_RENAMECMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"


/******************************************************************* dorename() */

/* rename a file */

#define rename_log      OPT_GENERIC_1
#define rename_confirm  OPT_GENERIC_2


qual_t
renamequals[] = { {"log",           rename_log,     0,              NV,
                   "-commands rename qual_log"},
                  {"nolog",         0,              rename_log,     NV, NULL},
                  {"confirm",       rename_confirm, 0,              NV,
                   "-commands rename qual_confirm"},
                  {"noconfirm",     0,              rename_confirm, NV, NULL},

                  {NULL,            0,              0,              NV, NULL}
};

param_t
renamepars[] = { {"from_filespec", REQ,         FSPEC, NOPA,
                  "commands rename fromspec"},
                 {"to_filespec",   REQ,         FSPEC, NOPA,
                  "commands rename tospec"},

                 { NULL,           0,           0,     NOPA, NULL }
};

DECL_CMD(rename) {
    options_t options;
    vmscond_t sts = 0;
    vmscond_t confirm = ODS2_CONFIRM_ALL;
    uint32_t renamecnt = 0;
    struct FAB ofab, nfab;
    struct NAM onam, nnam;
    char oes[NAM$C_MAXRSS+1], nes[NAM$C_MAXRSS+1],
        ors[NAM$C_MAXRSS+1], nrs[NAM$C_MAXRSS+1];

    UNUSED(argc);

    if( $FAILS(sts = checkquals( &options, 0, renamequals, qualc, qualv )) ) {
        return sts;
    }

    if( options & rename_confirm )
        confirm = RENAME_CONFIRM;

    /* Old spec, possibly wild */

    onam = cc$rms_nam;
    onam.nam$l_esa = oes;
    onam.nam$b_ess = sizeof( oes ) -1;
    onam.nam$l_rsa = ors;
    onam.nam$b_rss = sizeof( ors ) -1;

    ofab = cc$rms_fab;
    ofab.fab$l_nam = &onam;
    ofab.fab$l_fna = argv[1];
    ofab.fab$b_fns = (uint8_t)strlen(ofab.fab$l_fna);

    do {
        if( $FAILS(sts = sys_parse(&ofab)) ) {
            printmsg( RENAME_PARSEFAIL, 0, ofab.fab$l_fna );
            sts = printmsg( sts, MSG_CONTINUE, RENAME_PARSEFAIL );
            break;
        }

        oes[onam.nam$b_esl] = '\0';

        while( $SUCCESSFUL(sts) ) {
            struct FAB rfab;
            struct NAM rnam;
            char res[NAM$C_MAXRSS+1], rrs[NAM$C_MAXRSS+1];

            if( $FAILS(sts = sys_search( &ofab )) ) {
                if( $MATCHCOND(sts, RMS$_NMF) ) {
                    sts = SS$_NORMAL;
                    break;
                }
                printmsg( RENAME_SEARCHFAIL, 0, onam.nam$l_esa );
                sts = printmsg( sts, MSG_CONTINUE, RENAME_SEARCHFAIL );
                break;
            }
            ors[onam.nam$b_rsl] = '\0';

            /* New spec */

            nnam = cc$rms_nam;
            nnam.nam$l_esa = nes;
            nnam.nam$b_ess = sizeof( nes ) -1;
            nnam.nam$l_rsa = nrs;
            nnam.nam$b_rss = sizeof( nrs ) -1;
            nnam.nam$l_rlf = &onam;

            nfab = cc$rms_fab;
            nfab.fab$l_nam = &nnam;
            nfab.fab$l_fna = argv[2];
            nfab.fab$b_fns = (uint8_t)strlen(nfab.fab$l_fna);
            nfab.fab$l_fop = FAB$M_OFP;

            if( $FAILS(sts = sys_parse(&nfab)) ) {
                printmsg( RENAME_PARSEFAIL, 0, nfab.fab$l_fna );
                sts = printmsg( sts, MSG_CONTINUE, RENAME_PARSEFAIL );
                break;
            }
            nes[nnam.nam$b_esl] = '\0';

            /* Old resultant spec for rename */

            rnam = cc$rms_nam;
            rnam.nam$l_esa = res;
            rnam.nam$b_ess = sizeof( res ) -1;
            rnam.nam$l_rsa = rrs;
            rnam.nam$b_rss = sizeof( rrs ) -1;

            rfab = cc$rms_fab;
            rfab.fab$l_nam = &rnam;
            rfab.fab$l_fna = onam.nam$l_rsa;
            rfab.fab$b_fns = onam.nam$b_rsl;

            if( $MATCHCOND( confirm, RENAME_CONFIRM) )
                confirm = confirm_cmd( confirm, ors, nes );

            if( !$MATCHCOND( confirm, ODS2_CONFIRM_ALL ) ) {
                if( $MATCHCOND( confirm, ODS2_CONFIRM_YES ) ) {
                    confirm = RENAME_CONFIRM;
                } else if( $MATCHCOND( confirm, ODS2_CONFIRM_NO ) ) {
                    confirm = RENAME_CONFIRM;
                    continue;
                } else { /* ODS2_CONFIRM_QUIT */
                    sts = confirm;
                    break;
                }
            }

            sts = sys_rename( &rfab, NULL, NULL, &nfab );
            res[rnam.nam$b_esl] = '\0';
            rrs[rnam.nam$b_rsl] = '\0';
            nes[nnam.nam$b_esl] = '\0';
            nrs[nnam.nam$b_rsl] = '\0';

            if( $SUCCESSFUL(sts) ) {
                renamecnt++;
                if( options & rename_log )
                    sts = printmsg( RENAME_RENAMED, 0, rrs, nrs );
            } else {
                printmsg( RENAME_FAILED, 0,
                          rnam.nam$b_rsl? rrs: rnam.nam$b_esl? res: rfab.fab$l_fna,
                          nnam.nam$b_rsl? nrs: nnam.nam$b_rsl? nes: nfab.fab$l_fna );
                sts = printmsg( sts, MSG_CONTINUE, RENAME_FAILED );
            }

            rfab.fab$b_fns =
                rnam.nam$b_ess =
                rfab.fab$b_dns = 0;
            rnam.nam$b_nop = NAM$M_SYNCHK;
            (void) sys_parse( &rfab );

            if( $FAILED(sts) && $VMS_STATUS_SEVERITY(sts) > STS$K_WARNING )
                break;
        } /* search old files */
    } while( 0 );

    if( options & rename_log ) {
        vmscond_t st2 = 0;

        if( renamecnt > 1 ) {
            st2 = printmsg( RENAME_NRENAMED, 0, renamecnt );
        } else if( renamecnt == 0 && $SUCCESSFUL(sts) )
            st2 = printmsg( RENAME_NOFILES, 0 );
        if( $SUCCESSFUL(sts) )
            sts = st2;
    }

    ofab.fab$b_fns =
        onam.nam$b_ess =
        ofab.fab$b_dns = 0;
    onam.nam$l_rlf = NULL;
    onam.nam$b_nop = NAM$M_SYNCHK;
    (void) sys_parse( &ofab );

    nfab.fab$b_fns =
        nnam.nam$b_ess =
        nfab.fab$b_dns = 0;
    nnam.nam$l_rlf = NULL;
    nnam.nam$b_nop = NAM$M_SYNCHK;
    (void) sys_parse( &nfab );

    return sts;
}
