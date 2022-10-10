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

#if !defined( DEBUG ) && defined( DEBUG_DELETECMD )
#define DEBUG DEBUG_DELETECMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

#ifdef _WIN32
#undef DELETE
#endif

/***************************************************************** dodelete() */

#define delete_log     OPT_GENERIC_1
#define delete_confirm OPT_GENERIC_2

qual_t delquals[] = { {"log",        delete_log,    0,              NV,
                       "-commands delete qual_log"},
                      {"nolog",     0,              delete_log,     NV, NULL },
                      {"confirm",   delete_confirm, 0,              NV,
                       "-commands delete qual_confirm"},
                      {"noconfirm", 0,              delete_confirm, NV, NULL},
                      { NULL,       0,              0,              NV, NULL }
};
param_t delpars[] = { {"filespec", REQ | NOLIM, FSPEC, NOPA,
                       "commands delete filespec"},
                      { NULL,      0,   0,     NOPA, NULL }
};

DECL_CMD(delete) {
    vmscond_t sts = 0;
    char esa[NAM$C_MAXRSS + 1], rsa[NAM$C_MAXRSS + 1];
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    options_t options, filecount = 0;
    vmscond_t confirm = ODS2_CONFIRM_ALL;

    if( $FAILS(sts = checkquals( &options, delete_log, delquals, qualc, qualv )) ) {
        return sts;
    }

    if( options & delete_confirm )
        confirm = DELETE_CONFIRM;

    --argc;
    ++argv;
    while( ($SUCCESSFUL(sts) ||
            $VMS_STATUS_SEVERITY(sts) <= STS$K_WARNING ) && argc-- ) {
        nam.nam$l_esa = esa;
        nam.nam$b_ess = sizeof(esa) -1;
        fab.fab$l_nam = &nam;
        fab.fab$l_fna = argv++[0];
        fab.fab$b_fns = (uint8_t)strlen(fab.fab$l_fna);

        if( $FAILS(sts = sys_parse( &fab )) ) {
            printmsg( DELETE_NOTDELETED, 0, fab.fab$l_fna );
            sts = printmsg( sts,MSG_CONTINUE, DELETE_NOTDELETED );
            break;
        }
        esa[nam.nam$b_esl] = '\0';
        if( !(nam.nam$l_fnb & NAM$M_EXP_VER) ) {
            sts = printmsg( $SETFAC(SHR$_DELVER,DELETE), 0 );
            break;
        }
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = sizeof(rsa) -1;
        fab.fab$l_fop = 0;

        while( TRUE ) {
            uint32_t directory;
            uint16_t retlen;
            struct FAB ffab = cc$rms_fab;
            struct RAB rab = cc$rms_rab;
            struct XABITM itm = cc$rms_xabitm;
            struct item_list xitems[] = {
                { XAB$_UCHAR_DIRECTORY,   sizeof(directory), NULL, 0 },
                { 0, 0, NULL, 0 }
            };

            if( $FAILS(sts = sys_search( &fab )) ) {
                if( $MATCHCOND(sts, RMS$_NMF) )
                    break;
                printmsg( DELETE_NOTDELETED, 0,
                          (nam.nam$b_esl? esa: fab.fab$l_fna) );
                sts = printmsg( sts, MSG_CONTINUE, DELETE_NOTDELETED );
                break;
            }
            rsa[nam.nam$b_rsl] = '\0';

            if( $MATCHCOND( confirm, DELETE_CONFIRM) )
                confirm = confirm_cmd( confirm,  rsa );

            if( !$MATCHCOND( confirm, ODS2_CONFIRM_ALL ) ) {
                if( $MATCHCOND( confirm, ODS2_CONFIRM_YES ) ) {
                    confirm = DELETE_CONFIRM;
                } else if( $MATCHCOND( confirm, ODS2_CONFIRM_NO ) ) {
                    confirm = DELETE_CONFIRM;
                    continue;
                } else { /* ODS2_CONFIRM_QUIT */
                    sts = confirm;
                    break;
                }
            }

            xitems[0].buffer  = &directory;  xitems[0].retlen = &retlen;
            itm.xab$b_mode = XAB$K_SENSEMODE;
            itm.xab$l_itemlist = xitems;
            ffab.fab$l_xab = &itm;
            ffab.fab$l_fna = rsa;
            ffab.fab$b_fns = nam.nam$b_rsl;
            ffab.fab$b_fac = FAB$M_GET;

            if( $FAILS(sys_open( &ffab )) ) {
                sts = printmsg( DELETE_OPENIN, 0, rsa );
                continue;
            }
            if( directory ) {
                rab.rab$l_fab = &ffab;
                if( $FAILS(sts = sys_connect( &rab )) ) {
                    printmsg( DELETE_OPENIN, 0, rsa );
                    sts = printmsg( sts, MSG_CONTINUE, DELETE_OPENIN );
                } else {
                    sts = sys_get( &rab );
                    if( $MATCHCOND( sts, RMS$_EOF ) ) {
                        sts = SS$_NORMAL;
                    } else { /* RMS$_RTB if a record exists */
                        printmsg( DELETE_NOTDELETED, 0, rsa );
                        sts = printmsg( DELETE_DIRNOTEMPTY, MSG_CONTINUE, DELETE_NOTDELETED );
                    }
                    sys_disconnect( &rab );
                }
            }
            sys_close( &ffab );
            if( $FAILED(sts) ) {
                continue;
            }
            if( $FAILS(sts = sys_erase(&ffab)) ) {
                printmsg( DELETE_NOTDELETED, 0, rsa );
                sts = printmsg( sts, MSG_CONTINUE, DELETE_NOTDELETED );
                continue;
            }
            filecount++;
            if( options & delete_log )
                sts = printmsg( DELETE_DELETED, 0, rsa);
        } /* search */
        if( $MATCHCOND( sts, ODS2_CONFIRM_QUIT ) )
            break;
        if( $MATCHCOND(sts, RMS$_NMF) )
            sts = SS$_NORMAL;
    } /* arg */

    if(  $SUCCESSFUL(sts) || $MATCHCOND( sts, ODS2_CONFIRM_QUIT ) ) {
        if( filecount < 1 )
            sts = printmsg( DELETE_NOFILES, 0 );
        else if( filecount != 1 )
            sts = printmsg( DELETE_NFILES, 0, filecount );
    }
    fab.fab$b_fns =
        fab.fab$b_dns = 0;
    nam.nam$b_nop = NAM$M_SYNCHK;
    (void) sys_parse( &fab );                    /* Discard context */

    return sts;
}
