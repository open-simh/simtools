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

#if !defined( DEBUG ) && defined( DEBUG_CREATECMD )
#define DEBUG DEBUG_CREATECMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

static vmscond_t create_directory( options_t options, int argc, char** argv );
static vmscond_t create_file( options_t options, int argc, char** argv );


/******************************************************************* docreate() */

#define create_log     OPT_GENERIC_1
#define create_dir     OPT_GENERIC_2

#define create_owner   OPT_GENERIC_3
#define create_protect OPT_GENERIC_4
#define create_volume  OPT_GENERIC_5
#define create_verlim  OPT_GENERIC_6

#define create_recfmt   (OPT_GENERIC_10|OPT_GENERIC_9|OPT_GENERIC_8)
#define create_v_recfmt (OPT_V_GENERIC + 7)

#define create_b4       (OPT_SHARED_2|OPT_SHARED_1)
#define create_v_b4     0

#define create_after    (OPT_SHARED_4|OPT_SHARED_3)
#define create_v_after  2

#define create_prn_none 0
#define create_prn_nl   1
#define create_prn_c0   2
#define create_prn_vfu  3

#define create_prn     OPT_SHARED_17
#define create_ftn     OPT_SHARED_18
#define create_cr      OPT_SHARED_19
#define create_span    OPT_SHARED_20

static uint32_t b4n, aftn;
static uint32_t vfclen;
static uint32_t reclen;
static uint32_t verlimit;
static uint32_t volume;
static uint32_t protection;
static uint32_t ownuic;

static qual_t
prnb4[] = { {"none", create_prn_none << create_v_b4,
                              create_b4, NV, "commands create qual_cc print b4 none"},
            {"nl",   create_prn_nl << create_v_b4,
                              create_b4, DV(&b4n), "commands create qual_cc print b4 nl"},
            {"ctl",  create_prn_c0, create_b4, DV(&b4n), "commands create qual_cc print b4 ctl"},
            {"vfu",  create_prn_vfu << create_v_b4,
                              create_b4, DV(&b4n), "commands create qual_cc print b4 vfu"},

            {NULL, 0, 0, NV, NULL}
};
static qual_t
prnafter[] = { {"none", create_prn_none << create_v_after,
                              create_after, NV, "commands create qual_cc print b4 none"},
               {"nl",   create_prn_nl << create_v_after,
                              create_after, DV(&aftn), "commands create qual_cc print b4 nl"},
               {"ctl",  create_prn_c0, create_b4, DV(&aftn), "commands create qual_cc print b4 ctl"},
               {"vfu",  create_prn_vfu << create_v_after,
                              create_after, DV(&aftn), "commands create qual_cc print b4 vfu"},

               {NULL, 0, 0, NV, NULL}
};

static qual_t
prnkwds[] = { {"before", 0, create_b4,    KV(prnb4),    "commands create qual_cc print b4"},
              {"after",  0, create_after, KV(prnafter), "commands create qual_cc print after"},

              {NULL, OPT_NOSORT, 0, NV, NULL}
};

static qual_t
cckwds[] = { {"fortran", create_ftn, create_cr|create_prn, NV,
              "commands create qual_cc fortran"},
             {"carriage_return", create_cr, create_ftn|create_prn, NV,
              "commands create qual_cc cr"},
             {"print", create_prn|(FAB$C_VFC<<create_v_recfmt),
              create_ftn|create_cr|create_recfmt, KV(prnkwds),
              "commands create qual_cc print"},
             {"none",  0, create_ftn|create_cr|create_prn, NV,
              "commands create qual_cc none"},

             {NULL,    0, 0, NV, NULL}
};

static qual_t
fmtkwds[] = { {"fixed",          (FAB$C_FIX +1) << create_v_recfmt,
               create_recfmt, DV(&reclen),
               "commands create qual_recfmt fixed"},
              {"variable",       (FAB$C_VAR +1) << create_v_recfmt,
               create_recfmt, NV,
               "commands create qual_recfmt variable"},
              {"vfc",            (FAB$C_VFC +1) << create_v_recfmt,
               create_recfmt, VOPT(DV(&vfclen)),
               "commands create qual_recfmt vfc"},
              {"stream",         (FAB$C_STM +1) << create_v_recfmt,
               create_recfmt, NV,
               "commands create qual_recfmt stream"},
              {"streamlf",       (FAB$C_STMLF +1) << create_v_recfmt,
               create_recfmt, NV,
               "commands create qual_recfmt streamlf"},
              {"streamcr",       (FAB$C_STMCR +1) << create_v_recfmt,
               create_recfmt, NV,
               "commands create qual_recfmt streamcr"},
              {"undefined",      (FAB$C_UDF +1) << create_v_recfmt,
               create_recfmt, NV,
               "commands create qual_recfmt undefined"},

              {NULL, 0, 0, NV, NULL}
};

qual_t
createquals[] = { {"log",           create_log,     0,          NV,
                   "-commands create qual_log"},
                  {"nolog",         0,              create_log, NV, NULL },
                  {"directory",     create_dir,     0,          NV,
                   "commands create qual_directory"},
                  {"version_limit", create_verlim,  0,          DV(&verlimit),
                   "commands create qual_verlim"},
#if 0
                  {"volume",        create_volume,  0,          DV(&volume),
                   "commands create qual_volume"},
#endif
                  {"owner_uic",     create_owner,   0,          UV(&ownuic),
                   "commands create qual_owner" },
                  {"protection",    create_protect, 0,          PV(&protection),
                   "commands create qual_protection"},
                  {"record_format", 0,              0,          KV(fmtkwds),
                   "commands create qual_recfmt"},
                  {"carriage_control", 0,           0,          KV(cckwds),
                   "commands create qual_cc"},
                  {"span",          create_span,    0,          NV, "commands create qual_span"},
                  {"nospan",        0,              create_span,NV, NULL},

                  { NULL,           0,              0,          NV, NULL }
};

param_t
createpars[] = { {"parameter", REQ, STRING, NOPA, "commands create filespec"},

                 { NULL, 0, 0, NOPA, NULL }
};

DECL_CMD(create) {
    vmscond_t sts;
    options_t options;

    vfclen = 0;
    reclen = 0;

    if( $FAILS(sts = checkquals( &options, create_span, createquals, qualc, qualv )) ) {
        return sts;
    }

    if( options & create_dir )
        return create_directory( options, argc, argv );

    return create_file( options, argc, argv );
}

/******************************************************************* create_directory() */

static vmscond_t create_directory( options_t options, int argc, char **argv ) {
    vmscond_t sts;
    char tdir[NAM$C_MAXRSS+1],
        rsname[NAM$C_MAXRSS+1];
    struct FAB fab = cc$rms_fab;
    struct NAM nam = cc$rms_nam;

    UNUSED(argc);

    do {
        int level;
        char newdir[NAM$C_MAXRSS + 1], *p;
        uint8_t newlen, devlen, newdirlen, tmplen, sfdlen;
        struct XABPRO parpro;
        struct XABFHC parfhc;

        parpro = cc$rms_xabpro;
        parfhc = cc$rms_xabfhc;

        nam.nam$b_ess = sizeof( newdir ) -1;
        nam.nam$l_esa = newdir;

        nam.nam$b_rss = sizeof( rsname ) -1;
        nam.nam$l_rsa = rsname;

        fab.fab$l_nam = &nam;

        fab.fab$b_fac = 0;
        fab.fab$l_fop = FAB$M_OFP;

        fab.fab$l_fna = argv[1];
        fab.fab$b_fns = (uint8_t)strlen(fab.fab$l_fna);

        /* Check syntax & see if specified directory exists */

        sts = sys_parse( &fab );

        tdir[nam.nam$b_rsl] = '\0';
        newdir[nam.nam$b_esl] = '\0';

        devlen = nam.nam$b_dev;
        newdirlen = nam.nam$b_dir;
        newlen = devlen + newdirlen;

        if( $SUCCESSFUL(sts) &&
            !(nam.nam$l_fnb & (NAM$M_EXP_NAME | NAM$M_EXP_TYPE |
                               NAM$M_EXP_VER  | NAM$M_WILDCARD)) ) {
            rsname[newlen] = '\0';
            sts = printmsg( CREATE_EXISTS, 0, rsname );
            break;
        }
        if( $FAILED(sts) && !$MATCHCOND(sts, RMS$_DNF)  ) {
            sts = printmsg( sts, 0 );
            break;
        }

        if( nam.nam$l_fnb & (NAM$M_EXP_NAME | NAM$M_EXP_TYPE |
                             NAM$M_EXP_VER  | NAM$M_WILDCARD) ) {
            sts = printmsg( RMS$_DIR, 0 );
            break;
        }

        /* Scan from MFD down to apply defaults and fill in missing
         * SFDs.
         */
        p = newdir+devlen+1;
        memcpy( tdir, newdir, devlen );
        memcpy( tdir+devlen, "[000000]000000.DIR;1",
                sizeof( "[000000]000000.DIR;1" ) -1 );
        tmplen = sizeof( "[000000]" ) -1;
        sfdlen = sizeof( "000000" ) -1;
        level = 0;

        fab.fab$b_fns =
            nam.nam$b_ess =
            fab.fab$b_dns = 0;
        nam.nam$l_rlf = NULL;
        nam.nam$b_nop = NAM$M_SYNCHK;
        (void) sys_parse( &fab );                    /* Release context */
        fab.fab$l_nam = NULL;

        fab.fab$l_fna = tdir;

        sts = SS$_NORMAL;

        while( TRUE ) {
            uint32_t directory;
            uint16_t retlen;
            struct XABFHC fhc;
            struct XABPRO pro;
            struct XABITM itm;
            struct item_list xitems[] = {
                { XAB$_UCHAR_DIRECTORY, sizeof(int), NULL, 0 },
                { 0, 0, NULL, 0 }
            };

            fhc = cc$rms_xabfhc;
            pro = cc$rms_xabpro;
            itm = cc$rms_xabitm;

            itm.xab$l_nxt = &fhc;
            xitems[0].buffer = &directory; xitems[0].retlen = &retlen;

            itm.xab$b_mode = XAB$K_SENSEMODE;
            itm.xab$l_itemlist = xitems;

            pro.xab$l_nxt = &itm;
            fab.fab$l_xab = &pro;

            fab.fab$b_fns = devlen+tmplen+sfdlen+6;

            tdir[fab.fab$b_fns] = '\0';
            if( $SUCCESSFUL(sys_open(&fab)) ) {
                fab.fab$l_xab = NULL;
                sys_close( &fab );
                if( !directory ) {
                    sts = printmsg( CREATE_NOTDIR, 0, tdir );
                    break;
                }
                parpro = pro;
                parfhc = fhc;
            } else {
                struct FAB   nfab = cc$rms_fab;
                struct NAM    nam = cc$rms_nam;
                struct XABALL all = cc$rms_xaball;

                all.xab$l_alq = 1;
                if( options & create_volume )
                    all.xab$w_vol = (uint16_t)volume;

                fhc = parfhc;
                if( options & create_verlim )
                    fhc.xab$w_verlimit = verlimit;

                fhc.xab$l_nxt = &all;

                pro = parpro;
                pro.xab$w_pro |= ( ((xab$m_nodel) << xab$v_system) |
                                   ((xab$m_nodel) << xab$v_owner)  |
                                   ((xab$m_nodel) << xab$v_group)  |
                                   ((xab$m_nodel) << xab$v_world) );

                if( options & create_protect ) {
                    pro.xab$w_pro = (uint16_t)
                        ((pro.xab$w_pro & ~(protection >> 16)) |
                         (uint16_t)protection);
                }

                if( options & create_owner ) {
                    pro.xab$l_uic = ownuic;
                }

                itm.xab$l_nxt = &fhc;

                directory = 1;
                itm.xab$b_mode = XAB$K_SETMODE;
                pro.xab$l_nxt = &itm;
                nfab.fab$l_xab = &pro;

                nam.nam$b_rss = sizeof( rsname ) -1;
                nam.nam$l_rsa = rsname;
                nfab.fab$l_nam = &nam;

                nfab.fab$b_fac = 0;
                nfab.fab$l_fop = FAB$M_OFP;
                nfab.fab$b_org = FAB$C_SEQ;
                nfab.fab$b_rat = FAB$M_BLK;
                nfab.fab$b_rfm = FAB$C_VAR;
                fhc.xab$w_lrl =
                    nfab.fab$w_mrs = 512;

                nfab.fab$b_fns = fab.fab$b_fns;
                nfab.fab$l_fna = fab.fab$l_fna;

                sts = sys_create( &nfab );

                rsname[nam.nam$b_rsl] = '\0';
                if( $FAILED(sts) ) {
                    printmsg( CREATE_CREDIRFAIL, 0, rsname );
                    sts = printmsg( sts, MSG_CONTINUE, CREATE_CREDIRFAIL );
                } else {
                    if( $FAILS(sts = sys_close(&nfab)) ) {
                        printmsg(CREATE_CLOSEOUT, 0, rsname);
                        sts = printmsg(sts, MSG_CONTINUE, CREATE_CLOSEOUT);
                    }
                }
                nfab.fab$b_fns =
                    nam.nam$b_ess = 0;
                    nfab.fab$b_dns = 0;
                nam.nam$b_nop = NAM$M_SYNCHK;
                (void) sys_parse( &nfab );

                if( $FAILED(sts) )
                    break;

                if( options & create_log )
                    sts = printmsg( CREATE_CREATED, 0, rsname );
            }

            if( *p == ']' )
                break;

            switch( level++ ) {
            case 0:
                break;
            case 1:
                memmove( tdir+devlen+1, tdir+devlen+tmplen, sfdlen );
                tmplen = 1+ sfdlen +1;
                tdir[devlen+tmplen-1] = ']';
                break;
            default:
                tdir[devlen+tmplen -1] = '.';
                tmplen += 1 + sfdlen;
                tdir[devlen+tmplen -1] = ']';
                break;
            }

            for( sfdlen = 0; *p != '.' && *p != ']'; sfdlen++ )
                tdir[devlen+tmplen+sfdlen] = *p++;

            memcpy( tdir+devlen+tmplen+sfdlen, ".DIR;1", 6 );
            if( *p == '.' )
                ++p;
        }

    } while( 0 );

    if( nam.nam$l_fnb & (NAM$M_EXP_NAME | NAM$M_EXP_TYPE |
                         NAM$M_EXP_VER  | NAM$M_WILDCARD) ) {
        sts = printmsg( RMS$_DIR, 0 );
    }

    fab.fab$b_fns =
        nam.nam$b_ess =
        fab.fab$b_dns = 0;
    nam.nam$b_nop = NAM$M_SYNCHK;
    (void) sys_parse( &fab );

    return sts;
}

/******************************************************************* create_file() */

static vmscond_t create_file( options_t options, int argc, char** argv ) {
    vmscond_t sts;
    char *buf = NULL;
    size_t bufsize = 80;
    char vfc[2], rsbuf[NAM$C_MAXRSS + 1];
    struct NAM nam;
    struct FAB fab;
    struct RAB rab;
    struct XABPRO pro;

    UNUSED( argc );

    nam = cc$rms_nam;
    fab = cc$rms_fab;
    rab = cc$rms_rab;
    pro = cc$rms_xabpro;

    nam.nam$l_esa = NULL;
    nam.nam$b_ess = 0;

    nam.nam$b_rss = sizeof( rsbuf ) -1;
    nam.nam$l_rsa = rsbuf;

    if( options & (create_protect|create_owner) ) {
        fab.fab$l_xab = &pro;

        if( options & create_protect ) {
            pro.xab$w_pro = (uint16_t)
                ((pro.xab$w_pro & ~(protection >> 16)) |
                 (uint16_t)protection);
        }

        if( options & create_owner ) {
            pro.xab$l_uic = ownuic;
        }
    }

    fab.fab$l_nam = &nam;

    fab.fab$b_fac = FAB$M_PUT;
    fab.fab$l_fop = FAB$M_OFP | FAB$M_TEF;
    fab.fab$b_org = FAB$C_SEQ;

    if( !(options & create_recfmt) ) {
        options |= (FAB$C_VAR +1) << create_v_recfmt;
        if( !(options & (create_prn|create_ftn|create_cr)) )
            options |= create_cr;
    }

    fab.fab$b_rfm = ((options & create_recfmt) >> create_v_recfmt) -1;

    switch( fab.fab$b_rfm ) {
    case FAB$C_STM:
    case FAB$C_STMLF:
    case FAB$C_STMCR:
        fab.fab$b_rat |= FAB$M_CR;
        break;
    case FAB$C_VFC:
        fab.fab$b_fsz = vfclen;
        break;
    }

    if( !(options & create_span) )
        fab.fab$b_rat |= FAB$M_BLK;
    if( options & create_ftn )
        fab.fab$b_rat |= FAB$M_FTN;
    if( options & create_cr )
        fab.fab$b_rat |= FAB$M_CR;
    if( options & create_prn ) {
        fab.fab$b_rat |= FAB$M_PRN;
        fab.fab$b_fsz = 2;
        fab.fab$b_rfm = FAB$C_VFC;

        switch( (options & create_b4) >> create_v_b4 ) {
        case create_prn_none:
            vfc[0] = 0;
            break;
        case create_prn_nl:
            if( b4n < 1 || b4n > 127 )
                return printmsg( CREATE_INVALIDNL, 0 );
            vfc[0] = b4n & 0x7f;
            break;
        case create_prn_c0:
            if( b4n > 31 )
                return printmsg( CREATE_INVALIDC0, 0 );
            vfc[0] = 0x80 | b4n;
            break;
        case create_prn_vfu:
            if( b4n < 1 || b4n > 16 )
                return printmsg( CREATE_INVALIDVFU, 0 );
            vfc[0] = (0xC0 | (b4n -1)) & 0xff;
        }
        switch( (options & create_after) >> create_v_after ) {
        case create_prn_none:
            vfc[1] = 0;
            break;
        case create_prn_nl:
            if( aftn < 1 || aftn > 127 )
                return printmsg( CREATE_INVALIDNL, 0 );
            vfc[1] = aftn & 0x7f;
            break;
        case create_prn_c0:
            if( aftn > 31 )
                return printmsg( CREATE_INVALIDC0, 0 );
            vfc[1] = 0x80 | aftn;
            break;
        case create_prn_vfu:
            if( aftn < 1 || aftn > 16 )
                return printmsg( CREATE_INVALIDVFU, 0 );
            vfc[1] = (0xC0 | (aftn -1)) & 0xff;
        }
    } /* prn */

    /*
    fab.fab$w_deq = 5;
    fab.fab$l_alq = 100;
    */
    fab.fab$w_mrs = (uint16_t)reclen;

    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = (uint8_t)strlen(fab.fab$l_fna);

    do {
        char *input;

        sts = sys_create( &fab );
        rsbuf[nam.nam$b_rsl] = '\0';

        if( $FAILED(sts) )
            break;

        rab.rab$l_fab = &fab;
        if( $FAILS(sts = sys_connect( &rab )) )
            break;
        if( options & create_prn )
            rab.rab$l_rhb = vfc;
        if( options & create_log ) {
            printmsg( CREATE_READY, 0, rsbuf );
            printmsg( CREATE_TERMEOF, MSG_CONTINUE, CREATE_READY,
#if defined( VMS ) || defined( _WIN32 )
                      "Control-Z"
#ifdef _WIN32
                      " Enter"
#endif
#else
                      "Control-D"
#endif
                      );
        }

        while( (input = fgetline( stdin, FALSE, &buf, &bufsize )) != NULL ) {
            size_t len;

            len = strlen( input );
            sts = SS$_NORMAL;

#if 0
            if( len && input[len -1] == '\r' )
                len--;
#endif
            rab.rab$l_rbf = input;

            switch( fab.fab$b_rfm ) {
            case FAB$C_FIX:
                if( len > (size_t)fab.fab$w_mrs )
                    len = fab.fab$w_mrs;
                else if( len < (size_t)fab.fab$w_mrs ) {
                    char *p = input;

                    if( bufsize < fab.fab$w_mrs ) {
                        p = realloc( buf, fab.fab$w_mrs );
                        if( p == NULL ) {
                            sts = SS$_INSFMEM;
                            break;
                        }
                        bufsize = fab.fab$w_mrs;
                        rab.rab$l_rbf =
                            buf =
                            input = p;
                    }
                    p += len;
                    len = fab.fab$w_mrs - len;
                    while( len-- )
                        *p++ = ' ';
                    len = fab.fab$w_mrs;
                }
                break;
            case FAB$C_VFC:
                if( !(options & create_prn) ) {
                    if( len < fab.fab$b_fsz ) {
                        sts = RMS$_RFM;
                        break;
                    }
                    rab.rab$l_rhb = input;
                    rab.rab$l_rbf = input + fab.fab$b_fsz;
                    len -= fab.fab$b_fsz;
                }
                break;
            }
            if( $SUCCESSFUL(sts) ) {
                rab.rab$w_rsz = (uint16_t)len;

                sts = sys_put(&rab);
            }

            if( $FAILED(sts) )
                break;
        }
        break;
    } while( 0 );

    if( buf != NULL ) free( buf );

    if( $FAILED(sts) ) {
        vmscond_t rsts;
        int err;

        err = errno;
        rsts = printmsg( sts, 0, nam.nam$b_rsl? rsbuf: argv[1] );
        if( err )
            printmsg( ODS2_OSERROR, MSG_CONTINUE, sts, strerror( err ) );
        sts = rsts;
    }
    sys_disconnect( &rab );
    if( $SUCCESSFUL( sts ) ) {
        if( $FAILS(sts = sys_close( &fab )) ) {
            vmscond_t rsts;
            int err;

            err = errno;
            rsts = printmsg( CREATE_CLOSEOUT, 0,
                            nam.nam$b_rsl? rsbuf: argv[1] );
            printmsg( sts, MSG_CONTINUE, rsts, nam.nam$b_rsl? rsbuf: argv[1] );
            if( err )
                printmsg( ODS2_OSERROR, MSG_CONTINUE, sts, strerror( err ) );
            sts = rsts;
        }
    } else {
        sys_close( &fab );
    }

    if( $SUCCESSFUL( sts ) && (options & create_log) )
        sts = printmsg( CREATE_CREATED, 0, rsbuf );

    return sts;
}
