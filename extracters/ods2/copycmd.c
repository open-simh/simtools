
/* This is part of ODS2, originally written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors.   This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
 */

#if !defined( DEBUG ) && defined( DEBUG_COPYCMD )
#define DEBUG DEBUG_COPYCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include "winfile.h"
#define basename winbasename
#undef fstat
#define fstat _fstat
#elif defined(VMS)
#include "vmsfile.h"
#define basename vmsbasename
#else
#include <glob.h>
#include <libgen.h>
#include <unistd.h>
#endif

#ifndef GLOB_BRACE
#define GLOB_BRACE 0
#endif
#ifndef GLOB_TILDE_CHECK
#define GLOB_TILDE_CHECK 0
#endif

#include "cmddef.h"

static vmscond_t copy_from( options_t options, int argc, char **argv );
static vmscond_t copy_1_from( options_t options, struct FAB *fab,
                             const char *to, size_t pause );
static vmscond_t put_prn( char ch, FILE *tof, pause_t *pause );

static vmscond_t copy_to( options_t options, int argc, char **argv );
static vmscond_t copy_1_to( options_t options, char *from, char *to );

char *output_file_name( const char *ofname, const char *ifname, int *flags );
#define OF_WILD 1

static int xpand( char **buf, size_t size[2], size_t add );

/******************************************************************* dotype() */

/* Display a file on the terminal */

#define type_page OPT_SHARED_20
#define type_one  OPT_SHARED_19
#define copy_vfc  OPT_SHARED_18

static uint32_t height;

qual_t typequals[] = { {"page",         type_page, 0,         VOPT(DV(&height)),
                        "-commands type qual_page"},
                       {"nopage",       0,         type_page, NV, NULL},
                       {"onefile",      type_one,  0,         NV, NULL},
                       {"vfc_header",   copy_vfc,  0,         NV,
                        "-commands type qual_vfc"},
                       {"novfc_header", 0,         copy_vfc,  NV, NULL},

                       {NULL, 0, 0, NV, NULL }
};

param_t typepars[] = { {"filespec", REQ | NOLIM, FSPEC, NOPA, "commands type filespec"},

                       { NULL, 0, 0, NOPA, NULL }
};

DECL_CMD(type) {
    vmscond_t sts;
    int theight;
    options_t options;
    struct FAB fab = cc$rms_fab;
    struct NAM nam = cc$rms_nam;
    char esa[NAM$C_MAXRSS + 1], rsa[NAM$C_MAXRSS + 1];

    (void) termchar( NULL, &theight );
    height = theight-1;

    if( $FAILS(sts = checkquals( &options, type_page, typequals, qualc, qualv )) ) {
        return sts;
    }
    if( !(options & type_page) ) {
        height = 0;
    }

    --argc;
    ++argv;
    while( $SUCCESSFUL(sts) && argc-- ) {
        nam.nam$l_esa = esa;
        nam.nam$b_ess = NAM$C_MAXRSS;

        fab.fab$l_fna = argv++[0];
        fab.fab$b_fns = (uint8_t)strlen(fab.fab$l_fna);

        if( options & type_one ) { /* RMS test/debug: implicit parse/search */
            if( $FAILS(sts = sys_open(&fab)) ) {
                nam.nam$l_rsa[nam.nam$b_rsl] = '\0';
                printmsg( TYPE_OPENIN, 0, nam.nam$l_rsa );
                sts = printmsg( sts, MSG_CONTINUE, TYPE_OPENIN );
                break;
            }
            sts = copy_1_from( options & copy_vfc, &fab, NULL, height );
            sys_close(&fab);
            continue;
        }
        fab.fab$l_nam = &nam;

        if( $FAILS(sts = sys_parse(&fab)) ) {
            fab.fab$l_fna[fab.fab$b_fns] = '\0';
            printmsg( TYPE_PARSEFAIL, 0, fab.fab$l_fna );
            sts = printmsg( sts, MSG_CONTINUE, TYPE_PARSEFAIL );
            break;
        }

        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = 0;

        while( $SUCCESSFUL(sts) ) {
            if( $FAILS(sts = sys_search( &fab )) ) {
                if( $MATCHCOND(sts, RMS$_NMF) )
                    sts = SS$_NORMAL;
                nam.nam$l_esa[nam.nam$b_esl] = '\0';
                printmsg( TYPE_SEARCHFAIL, 0, nam.nam$l_esa );
                sts = printmsg( sts, MSG_CONTINUE, TYPE_SEARCHFAIL );
                break;
            }
            fab.fab$b_fac = FAB$M_GET;

            if( nam.nam$l_fnb & NAM$M_WILDCARD )
                printf( "\n%.*s\n\n", nam.nam$b_rsl, rsa );

            if(  $FAILS(sts = sys_open(&fab)) ) {
                nam.nam$l_rsa[nam.nam$b_rsl] = '\0';
                printmsg( TYPE_OPENIN, 0, nam.nam$l_rsa );
                sts = printmsg( sts, MSG_CONTINUE, TYPE_OPENIN );
                break;
            }
            sts = copy_1_from( options & copy_vfc, &fab, NULL, height );

            sys_close(&fab);
        }
    }
    printf( "\n" );

    fab.fab$b_fns =
        nam.nam$b_ess =
        fab.fab$b_dns = 0;
    nam.nam$l_rlf = NULL;
    nam.nam$b_nop = NAM$M_SYNCHK;
    (void) sys_parse( &fab );                    /* Release context */

    if( $MATCHCOND( sts, RMS$_EOF ) )
        sts = SS$_NORMAL | STS$M_INHIB_MSG;

    return sts;
}

/******************************************************************* docopy() */

/* Copy files to and from Files-11 volume */

#define copy_binary   OPT_GENERIC_1
#define copy_log      OPT_GENERIC_2
#define copy_tof11    OPT_GENERIC_3
#define copy_confirm  OPT_GENERIC_4
#define copy_override OPT_GENERIC_5

#define copy_owner   OPT_GENERIC_6
#define copy_protect OPT_GENERIC_7
#define copy_volume  OPT_GENERIC_8
#define copy_verlim  OPT_GENERIC_9

#define copy_recfmt   (OPT_GENERIC_12|OPT_GENERIC_11|OPT_GENERIC_10)
#define copy_v_recfmt (OPT_V_GENERIC + 9)

/* GENERIC option space is full */

#define copy_b4       (OPT_SHARED_2|OPT_SHARED_1)
#define copy_v_b4     0

#define copy_after    (OPT_SHARED_4|OPT_SHARED_3)
#define copy_v_after  2

#define copy_prn_none 0
#define copy_prn_nl   1
#define copy_prn_c0   2
#define copy_prn_vfu  3

#define copy_prn     OPT_SHARED_5
#define copy_ftn     OPT_SHARED_6
#define copy_cr      OPT_SHARED_7
#define copy_span    OPT_SHARED_8
#define copy_append  OPT_SHARED_9

/* Type uses OPT_SHARED from the high end */

static uint32_t b4n, aftn;
static uint32_t vfclen;
static uint32_t reclen;
static uint32_t verlimit;
#if 0
static uint32_t volume;
#endif
static uint32_t protection;
static uint32_t ownuic;

/* Print CC */
static qual_t
prnb4[] = { {"none", copy_prn_none << copy_v_b4,
                              copy_b4, NV, "commands copy qual_cc print b4 none"},
            {"nl",   copy_prn_nl << copy_v_b4,
                              copy_b4, DV(&b4n), "commands copy qual_cc print b4 nl"},
            {"ctl",  copy_prn_c0, copy_b4, DV(&b4n), "commands copy qual_cc print b4 ctl"},
            {"vfu",  copy_prn_vfu << copy_v_b4,
                              copy_b4, DV(&b4n), "commands copy qual_cc print b4 vfu"},

            {NULL, 0, 0, NV, NULL}
};
static qual_t /* Same keywords as prnb4.  Use same help. */
prnafter[] = { {"none", copy_prn_none << copy_v_after,
                              copy_after, NV, "commands copy qual_cc print b4 none"},
               {"nl",   copy_prn_nl << copy_v_after,
                              copy_after, DV(&aftn), "commands copy qual_cc print b4 nl"},
               {"ctl",  copy_prn_c0, copy_b4, DV(&aftn), "commands copy qual_cc print b4 ctl"},
               {"vfu",  copy_prn_vfu << copy_v_after,
                              copy_after, DV(&aftn), "commands copy qual_cc print b4 vfu"},

               {NULL, 0, 0, NV, NULL}
};

static qual_t
prnkwds[] = { {"before", 0, copy_b4,    KV(prnb4),    "commands copy qual_cc print b4"},
              {"after",  0, copy_after, KV(prnafter), "commands copy qual_cc print after"},

              {NULL, OPT_NOSORT, 0, NV, NULL}
};

/* Carriage control record attributes */
static qual_t
cckwds[] = { {"fortran", copy_ftn, copy_cr|copy_prn, NV,
              "commands copy qual_cc fortran"},
             {"carriage_return", copy_cr, copy_ftn|copy_prn, NV,
              "commands copy qual_cc cr"},
             {"print", copy_prn|(FAB$C_VFC<<copy_v_recfmt),
              copy_ftn|copy_cr|copy_recfmt, KV(prnkwds),
              "commands copy qual_cc print"},
             {"none",  0, copy_ftn|copy_cr|copy_prn, NV,
              "commands copy qual_cc none"},

             {NULL, 0, 0, NV, NULL}
};

static qual_t
fmtkwds[] = { {"fixed",          (FAB$C_FIX +1) << copy_v_recfmt,
               copy_recfmt, DV(&reclen),
               "commands copy qual_recfmt fixed"},
              {"variable",       (FAB$C_VAR +1) << copy_v_recfmt,
               copy_recfmt, NV,
               "commands copy qual_recfmt variable"},
              {"vfc",            (FAB$C_VFC +1) << copy_v_recfmt,
               copy_recfmt, VOPT(DV(&vfclen)),
               "commands copy qual_recfmt vfc"},
              {"stream",         (FAB$C_STM +1) << copy_v_recfmt,
               copy_recfmt, NV,
               "commands copy qual_recfmt stream"},
              {"streamlf",       (FAB$C_STMLF +1) << copy_v_recfmt,
               copy_recfmt, NV,
               "commands copy qual_recfmt streamlf"},
              {"streamcr",       (FAB$C_STMCR +1) << copy_v_recfmt,
               copy_recfmt, NV,
               "commands copy qual_recfmt streamcr"},
              {"undefined",      (FAB$C_UDF +1) << copy_v_recfmt,
               copy_recfmt, NV,
               "commands copy qual_recfmt undefined"},

              {NULL, 0, 0, NV, NULL}
};

qual_t
copyquals[] = { {"ascii",         0,            copy_binary,   NV, "commands copy qual_ascii"},
                {"binary",        copy_binary,   0,            NV, "commands copy qual_binary"},
                {"append",        copy_append,   0,            NV, "commands copy qual_append"},
                {"from_files-11", 0,             copy_tof11,   NV, "commands copy qual_from" },
                {"to_files-11",   copy_tof11,    0,            NV, "commands copy qual_to" },
                {"log",           copy_log,      0,            NV, "-commands copy qual_log"},
                {"nolog",         0,             copy_log,     NV, NULL},
                {"override",      copy_override, 0,            NV, NULL},
                {"confirm",       copy_confirm,  0,            NV, "-commands copy qual_confirm"},
                {"noconfirm",     0,             copy_confirm, NV, NULL},
                {"version_limit", copy_verlim,   0,            DV(&verlimit),
                 "commands copy qual_verlim"},
#if 0
                {"volume",        copy_volume,   0,            DV(&volume),
                 "commands copy qual_volume"},
#endif
                {"owner_uic",     copy_owner,    0,            UV(&ownuic),
                 "commands copy qual_owner" },
                {"protection",    copy_protect,  0,            PV(&protection),
                 "commands copy qual_protection"},
                {"record_format", 0,             0,            KV(fmtkwds),
                 "commands copy qual_recfmt"},
                {"carriage_control", 0,          0,            KV(cckwds),
                 "commands copy qual_cc"},
                {"span",          copy_span,     0,            NV, "commands copy qual_span"},
                {"nospan",        0,             copy_span,    NV, NULL},
                {"vfc_header",    copy_vfc,      0,            NV,
                 "-commands type qual_vfc"},
                {"novfc_header",  0,             copy_vfc,     NV, NULL},

                {NULL,            0,             0,            NV, NULL}
};

param_t copypars[] = { {"from_filespec", REQ | NOLIM, FSPEC, NOPA,
                        "commands copy fromspec"},
                       {"to_filespec",   REQ,         FSPEC, NOPA,
                        "commands copy tospec"},

                       { NULL,           0,           0,     NOPA, NULL }
};

const int copy_defaults = 0 | copy_span | copy_log;
defqualsp_t copy_defopt = NULL;

DECL_CMD(copy) {
    options_t options;
    vmscond_t sts;

    vfclen = 0;
    reclen = 0;

    if( copy_defopt != NULL ) {
        if( $FAILS(sts = checkquals( &options, copy_defaults, copyquals,
                                     copy_defopt->qualc, copy_defopt->qualv )) ) {
            return sts;
        }
    }
    else
        options = copy_defaults;

    if( $FAILS(sts = checkquals( &options, options, copyquals, qualc, qualv )) ) {
        return sts;
    }

    if( options & copy_tof11 )
        return copy_to( options, argc, argv );

    return copy_from( options, argc, argv );

}

/******************************************************************* copy_from() */

static vmscond_t copy_from( options_t options, int argc, char **argv ) {
    vmscond_t sts;
    struct NAM nam;
    struct FAB fab;
    char res[NAM$C_MAXRSS + 1], rsa[NAM$C_MAXRSS + 1];
    int i, filecount = 0;
    vmscond_t confirm = ODS2_CONFIRM_ALL;

    if( options & copy_confirm )
        confirm = COPY_CONFIRM;

    sts = SS$_NORMAL;

    nam = cc$rms_nam;
    fab = cc$rms_fab;

    for( i = 1; i < argc -1; i++ ) {
        nam.nam$l_esa = res;
        nam.nam$b_ess = NAM$C_MAXRSS;
        fab.fab$l_nam = &nam;
        fab.fab$l_fna = argv[1];
        fab.fab$b_fns = (uint8_t)strlen(fab.fab$l_fna);

        if( $FAILS(sts = sys_parse(&fab)) ) {
            printmsg( COPY_PARSEFAIL, 0, fab.fab$l_fna );
            sts = printmsg( sts, MSG_CONTINUE, COPY_PARSEFAIL );
            break;
        }
        res[nam.nam$b_esl] = '\0';

        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = 0;

        while( $SUCCESSFUL(sts) ) {
            char name[NAM$C_MAXRSS + 1];
            char *out, *inp;
            int dot = FALSE;
            uint32_t directory = 0;
            uint16_t retlen;
            struct XABPRO pro;
            struct XABDAT dat;
            struct XABITM itm;
            struct item_list xitems[] = {
                { XAB$_UCHAR_DIRECTORY, sizeof(directory), NULL, 0 },
                { 0, 0, NULL, 0 }
            };

            if( $FAILS(sts = sys_search( &fab )) ) {
                if( $MATCHCOND(sts, RMS$_NMF) ) {
                    sts = SS$_NORMAL;
                    break;
                }
                nam.nam$l_esa[nam.nam$b_esl] = '\0';
                printmsg( COPY_SEARCHFAIL, 0, nam.nam$l_esa );
                sts = printmsg( sts, MSG_CONTINUE, COPY_SEARCHFAIL );
                break;
            }
            rsa[nam.nam$b_rsl] = '\0';

            pro = cc$rms_xabpro;
            dat = cc$rms_xabdat;
            itm = cc$rms_xabitm;

            dat.xab$l_nxt = &pro;
            itm.xab$l_nxt = &dat;
            xitems[0].buffer = &directory;
            xitems[0].retlen = &retlen;
            itm.xab$b_mode = XAB$K_SENSEMODE;
            itm.xab$l_itemlist = xitems;
            fab.fab$l_xab = &itm;
            fab.fab$b_fac = FAB$M_GET;

            sts = sys_open(&fab);
            fab.fab$l_xab = NULL;
            rsa[nam.nam$b_rsl] = '\0';

            if( $FAILED(sts) ) {
                int err;
                err = errno;
                printmsg( COPY_OPENIN, 0, rsa );
                sts = printmsg( sts, MSG_CONTINUE, COPY_OPENIN );
                if( err )
                    printmsg( ODS2_OSERROR, MSG_CONTINUE, COPY_OPENIN,
                              strerror( err ) );
                break;
            }

            if( directory ) {
                if( !(options & copy_override) ) {
                    printmsg( COPY_DIRNOTCOPIED, 0, rsa );
                    sys_close( &fab );
                    continue;
                }
            }

            out = name;
            inp = argv[2];

            while (*inp != '\0') {
                if (*inp == '*') {
                    inp++;
                    if (dot) {
                        memcpy(out, nam.nam$l_type + 1,
                               nam.nam$b_type - 1);
                        out += nam.nam$b_type - 1;
                    } else {
                        size_t length;

                        length = nam.nam$b_name;

                        if (*inp == '\0')
                            length += nam.nam$b_type;
                        memcpy( out, nam.nam$l_name, length );
                        out += length;
                    }
                    continue;
                }
                if (*inp == '.') {
                    dot = TRUE;
                } else {
                    if (strchr(":]\\/", *inp))
                        dot = FALSE;
                }
                *out++ = *inp++;
            }
            *out++ = '\0';

            if( $MATCHCOND( confirm, COPY_CONFIRM) )
                confirm = confirm_cmd( confirm, rsa, name );

            if( !$MATCHCOND( confirm, ODS2_CONFIRM_ALL ) ) {
                if( $MATCHCOND( confirm, ODS2_CONFIRM_YES ) ) {
                    confirm = COPY_CONFIRM;
                } else if( $MATCHCOND( confirm, ODS2_CONFIRM_NO ) ) {
                    confirm = COPY_CONFIRM;
                    sys_close( &fab );
                    continue;
                } else { /* ODS2_CONFIRM_QUIT */
                    sys_close( &fab );
                    sts = confirm;
                    break;
                }
            }

            fab.fab$l_xab = &itm;
            if( $SUCCESSFUL(sts = copy_1_from( options, &fab, name, 0 )) )
                filecount++;
            fab.fab$l_xab = NULL;

            sys_close(&fab);
        } /* search */
        if( $VMS_STATUS_SEVERITY(sts) > STS$K_WARNING ||
            $MATCHCOND(sts, ODS2_CONFIRM_QUIT) )
            break;
    } /* arg */

    if( $SUCCESSFUL(sts) || $MATCHCOND(sts, ODS2_CONFIRM_QUIT) ) {
        if (filecount > 0) {
            if( options & copy_log )
                sts = printmsg( COPY_COPIED, 0, filecount );
        } else {
            sts = printmsg( COPY_NOFILES, 0 );
        }
    }
    return sts;
}

/******************************************************************* copy_1_from() */

static vmscond_t copy_1_from( options_t options, struct FAB *fab,
                             const char *to, size_t dopause ) {
    vmscond_t sts;
    uint32_t records = 0;
    int stream = 0, var = 0;
    FILE *tof;
    char *rec;
    pause_t *pause = NULL, pausectl;
    struct RAB rab = cc$rms_rab;

    if( dopause ) {
        pausectl.interval = dopause;
        pausectl.lineno = 0;
        pausectl.prompt = TYPE_CONTINUE;
        pause = &pausectl;
    }

    rab.rab$l_fab = fab;

    if( $FAILS(sts = sys_connect(&rab)) ) {
        printmsg( COPY_OPENIN, 0, fab->fab$l_fna );
        sts = printmsg( sts, MSG_CONTINUE, COPY_OPENIN );
        return sts;
    }

    if( (rec = malloc( MAXREC + 2 )) == NULL ) {
        sys_disconnect( &rab );
        printmsg( COPY_OPENIN, 0, fab->fab$l_fna );
        sts = printmsg( SS$_INSFMEM, MSG_CONTINUE, COPY_OPENIN );
        return sts;
    }

    rab.rab$l_ubf = rec;
    rab.rab$w_usz = MAXREC;
    rab.rab$l_rhb = NULL;

    if( options & copy_binary ) {
        fab->fab$b_rfm = FAB$C_UDF;
        fab->fab$b_rat = 0;
    } else {
        switch( fab->fab$b_rfm ) {
        case FAB$C_STMLF:
            stream = '\n';
            break;
        case FAB$C_STMCR:
            stream = '\r';
            break;
        case FAB$C_STM:
            stream = EOF;
            break;
        case FAB$C_VFC:
            var = 1;
            if( fab-> fab$b_fsz != 0 ) {
                ++var;
                rab.rab$l_rhb = malloc( fab->fab$b_fsz );
                if( rab.rab$l_rhb == NULL ) {
                    free( rec );
                    sys_disconnect( &rab );
                    printmsg( COPY_OPENIN, 0, fab->fab$l_fna );
                    sts = printmsg( sts, MSG_CONTINUE, COPY_OPENIN );
                    return sts;
                }
            }
            break;
        case FAB$C_VAR:
            var = 1;
            break;
        case FAB$C_FIX:
            break;
        }
    }

    if( to == NULL )
        tof = stdout;
    else {
        if( (stream ||
             (fab->fab$b_rat & (FAB$M_CR | FAB$M_PRN | FAB$M_FTN))) &&
            !(options & copy_binary) ) {
            tof = openf( to, (options & copy_append)? "a": "w" );
        } else {
            tof = openf( to, (options & copy_append)? "ab": "wb" );
        }
    }
    if (tof == NULL) {
        int err;

        err = errno;
        free( rec );
        free( rab.rab$l_rhb );
        sys_disconnect( &rab );
        printmsg( COPY_OPENOUT, 0, to );
        return printmsg( ODS2_OSERROR, MSG_CONTINUE, COPY_OPENOUT,
                         strerror( err ) );
    }

    while( $SUCCESSFUL(sts) ) {
        char *rp;
        unsigned rsz;

        if( ferror( tof ) ) {
            int err;

            err = errno;
            printmsg( COPY_WRITEERR, 0, (to == NULL)? "tty": to );
            sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, COPY_WRITEERR,
                            strerror(err) );
            break;
        }
        if( $FAILS(sts = sys_get( &rab )) ) {
            if( $MATCHCOND(sts, RMS$_EOF) )
                sts |= STS$M_INHIB_MSG;
            else {
                printmsg( COPY_READERR, 0, fab->fab$l_fna );
                sts = printmsg( sts, MSG_CONTINUE, COPY_READERR, rab.rab$l_stv);
            }
            break;
        }
        rp = rec;
        rsz = rab.rab$w_rsz;

        if( (options & copy_binary) || fab->fab$b_rfm == FAB$C_UDF ) {
            if( rsz != 0 ) {
                if( $FAILS(sts = put_strn( rp, rsz, tof, pause )) )
                    break;
                if( fab->fab$b_rfm == FAB$C_UDF )
                    records += rsz;
                else
                    records++;
            }
            continue;
        }

        if( stream ) {
            if( $FAILS(sts = put_strn( rp, rsz, tof, pause )) )
                break;

            if( fab->fab$b_rat & (FAB$M_CR|FAB$M_FTN) ) {
                if( stream == EOF ) {
                    if( $FAILS(sts = put_strn( "\r\n", 2, tof, pause )) )
                        break;
                } else {
                    if( $FAILS(sts = put_c( stream, tof, pause )) )
                        break;
                }
            }
            ++records;
            continue;
        }

        if( fab->fab$b_rat & FAB$M_FTN ) {
            --rsz;
            switch( *rp++ ) { /* AA-D034E-TE Table 8-5 */
            case '+':  /* Overprint current line and return to left margin */
                if( $FAILS(sts = put_c( '\r', tof, pause )) )
                    break;
                if( $FAILS(sts = put_strn( rp, rsz, tof, pause )) )
                    break;
                if( $FAILS(sts = put_c( '\r', tof, pause )) )
                    break;
                break;
            case ' ':  /* Single space: output to beginning of next line */
            default:
                if(  $FAILS(sts = put_c( '\n', tof, pause )) )
                    break;
                if( $FAILS(sts = put_strn(rp, rsz, tof, pause )) )
                    break;
                if( $FAILS(sts = put_c( '\r', tof, pause )) )
                    break;
                break;
            case '0':  /* Double space: skip a line before starting output */
                if(  $FAILS(sts = put_strn( "\n\n", 2, tof, pause )) )
                    break;
                if( $FAILS(sts = put_strn( rp, rsz, tof, pause )) )
                    break;
                if( $FAILS(sts = put_c( '\r', tof, pause )) )
                    break;
                break;
            case '1':  /* Paging: start output at top of a new page */
                if( $FAILS(sts = put_c( '\f', tof, pause )) )
                    break;
                if( $FAILS(sts = put_strn( rp, rsz, tof, pause )) )
                    break;
                if( $FAILS(sts = put_c( '\r', tof, pause )) )
                    break;
                break;
            case '$':  /* Prompting: Start a beginning of next line and suppress \r */
                if(  $FAILS(sts = put_c( '\n', tof, pause )) )
                    break;
                if( $FAILS(sts = put_strn( rp, rsz, tof, pause )) )
                    break;
                break;
            case '\0': /* Overprinting with no advance - no \r at end */
                if( $FAILS(sts = put_c( '\r', tof, pause )) )
                    break;
                if( $FAILS(sts = put_strn( rp, rsz, tof, pause )) )
                    break;
                break;
            }
            if( $FAILED(sts) )
                break;
            ++records;
            continue;
        }

        if( (fab->fab$b_rat & FAB$M_CR) && $FAILS(sts = put_c( '\n', tof, pause )) )
            break;

        if( var == 2  ) {
            if( (fab->fab$b_rat & FAB$M_PRN) && fab->fab$b_fsz == 2 ) {
                if( $FAILS(sts = put_prn( rab.rab$l_rhb[0], tof, pause )) )
                    break;
                if( $FAILS(sts = put_strn( rp, rsz, tof, pause )) )
                    break;
                if( $FAILS(sts = put_prn( rab.rab$l_rhb[1], tof, pause )) )
                    break;
                ++records;
                continue;
            }

            if( (options & copy_vfc) &&
                $FAILS(sts = put_strn( rab.rab$l_rhb,
                                       fab->fab$b_fsz, tof, pause )) )
                break;
        }

        if( $FAILS(sts = put_strn( rp, rsz, tof, pause )) )
            break;

        if( fab->fab$b_rat & FAB$M_CR )
            fputc( '\r', tof );

        ++records;
    }

    sys_disconnect( &rab );

    if( var == 2 ) {
        free( rab.rab$l_rhb );
        rab.rab$l_rhb = NULL;
    }

    free( rec );
    rec = NULL;

    if( to == NULL ) { /* stdout */
        if( $FAILED(sts) )
            sts = printmsg( sts, 0, rab.rab$l_stv );
        return sts;
    }
    if( fclose( tof ) ) {
        int err;

        err = errno;
        printmsg( COPY_CLOSEOUT, 0, (to == NULL)? "tty": to );
        sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, COPY_CLOSEOUT, strerror( err ) );
        return sts;
    }

    do {
        struct XABDAT *dat;
        time_t posixtime;
        unsigned short timvec[7];
        char *tz;
        struct tm tm;
#ifdef _WIN32
        struct _utimbuf tv;
#else
        struct timeval tv[2];
#endif

        for( dat = (struct XABDAT *)fab->fab$l_xab;
             dat != NULL && dat->xab$b_cod != XAB$C_DAT; dat = dat->xab$l_nxt )
            ;

        if( dat == NULL )
            break;

        if( $FAILS(sys_numtim( timvec, dat->xab$q_rdt )) )
            break;

        tm.tm_sec = timvec[5];
        tm.tm_min = timvec[4];
        tm.tm_hour = timvec[3];
        tm.tm_mday = timvec[2];
        tm.tm_mon = timvec[1] -1;
        tm.tm_year = timvec[0] - 1900;

        tz = get_env( "TZ" );
        setenv( "TZ", "", 1 );
        tzset();
        posixtime = mktime( &tm );
        if( posixtime != (time_t)-1 ) {
#ifdef _WIN32
            tv.actime =
                tv.modtime = posixtime;
            (void)_utime( to, &tv );
#else
            tv[0].tv_sec = posixtime;
            tv[0].tv_usec = timvec[6] * 10000;
            tv[1] = tv[0]; /* Set mtime to atime */
            (void) utimes( to, tv );
#endif
        }
        if( tz != NULL )
            setenv( "TZ", tz, 1 );
        else
            unsetenv( "TZ" );
        tzset();
        free( tz );

    } while( 0 );

#ifndef _WIN32
    do {
        struct XABPRO *pro;
        mode_t mode;
        uint16_t prot;

        for( pro = (struct XABPRO *)fab->fab$l_xab;
             pro != NULL && pro->xab$b_cod != XAB$C_PRO; pro = pro->xab$l_nxt )
            ;

        if( pro == NULL )
            break;

        mode = (S_IRUSR|S_IWUSR|S_IXUSR|
                S_IRGRP|S_IWGRP|S_IXGRP|
                S_IROTH|S_IWOTH|S_IXOTH);

        prot =  pro->xab$w_pro;

        if( prot & ((xab$m_noread << xab$v_system) |
                    (xab$m_noread << xab$v_owner)) )
            mode &= ~S_IRUSR;
        if( prot & (((xab$m_nodel|xab$m_nowrite) << xab$v_system) |
                    ((xab$m_nodel|xab$m_nowrite) << xab$v_owner)) )
            mode &= ~S_IWUSR;
        if( prot & ( (xab$m_noexe << xab$v_system) |
                     (xab$m_noexe << xab$v_owner) ) )
            mode &= ~S_IXUSR;
        if( prot & (xab$m_noread << xab$v_group) )
            mode &= ~S_IRGRP;
        if( prot & (xab$m_nowrite << xab$v_group) )
            mode &= ~S_IWGRP;
        if( prot & (xab$m_noexe << xab$v_group) )
            mode &= ~S_IXGRP;
        if( prot & (xab$m_noread << xab$v_world) )
            mode &= ~S_IROTH;
        if( prot & (xab$m_nowrite << xab$v_world) )
            mode &= ~S_IWOTH;
        if( prot & (xab$m_noexe << xab$v_world) )
            mode &= ~S_IXOTH;
         if( chmod( to, mode ) != 0 ) {
             int err;

             err = errno;
             printmsg( COPY_PROTERR, 0, to );
             sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, COPY_PROTERR,
                             strerror( err ) );
         }
    } while( 0 );
#endif

    if( !$MATCHCOND(sts, RMS$_EOF) )
        return sts;

    if( options & copy_log ) {
        sts = (fab->fab$b_rfm == FAB$C_UDF)? COPY_COPIEDU: COPY_COPIEDR;

        sts = printmsg( sts, 0,
                        fab->fab$l_nam->nam$l_rsa,
                        (to == NULL)? "tty": to,
                        records );
    }
    return SS$_NORMAL;
}

/******************************************************************* put_prn() */
static vmscond_t  put_prn( char ch, FILE *tof, pause_t *pause ) {
    vmscond_t sts;
    size_t n;
                              /* 7 6 5 4 */
    if( !(ch & 0x80) ) {      /* 0 x x x */
        n = ch & 0x7F;
        while( n-- )
            if( $FAILS(sts = put_c( '\n', tof, pause )) )
                return sts;
        return SS$_NORMAL;
    }                         /* 1 x x x */
    if( !(ch & 0x40) ) {      /* 1 0 x x */
        if( !(ch & 0x20) )    /* 1 0 0 x */
            return put_c( ch & 0x1f, tof, pause );      /* C0 */
                              /* 1 0 1 x */
        return put_c( (ch & 0x1f) | 0x80, tof, pause ); /* C1/reserved */
    }                         /* 1 1 x x */
    if( !(ch & 0x30) ) {      /* 1 1 0 0 */
        static const char vfuch[16] = {
            '\f',   '\020', '\021', '\022', /* CH  1 -  4 */
            '\023', '\024', '\v',   '\n',   /* CH  5 -  8 */
            '\n',   '\n',   '\n',   '\n',   /* CH  9 - 12 */
            '\n',   '\n',   '\n',   '\n' }; /* CH 13 - 16 */
        if( tof == stdout )
            return put_c( '\n', tof, pause ); /* type: 1 LF */
        return put_c( vfuch[ch & 0xf], tof, pause );
                                            /* VFU channel 1+<3:0> */
    }                         /* 1 1 y y : yy == 01, 10, 11 */
    return put_c( ch, tof, pause ); /* Device-specific/reserved -- Agrave - yumlaut */
}

/******************************************************************* copy_to() */

static vmscond_t copy_to( options_t options, int argc, char **argv ) {
    glob_t globs;
    int i, err, gflags = GLOB_MARK | GLOB_BRACE | GLOB_TILDE_CHECK;
    size_t n, filecount = 0;
    vmscond_t sts;
    vmscond_t confirm = ODS2_CONFIRM_ALL;

    memset( &globs, 0, sizeof( globs ) );

    if( options & copy_confirm )
        confirm = COPY_CONFIRM;

    sts = SS$_NORMAL;

    for( i = 1; i < argc -1; i++ ) {
        err = glob( argv[i], gflags, NULL, &globs );
        gflags |= GLOB_APPEND;
        switch( err ) {
        case 0:
            sts = SS$_NORMAL;
            break;
        case GLOB_NOSPACE:
           sts = printmsg( COPY_NOMEM, 0, argv[i] );
           break;
        case GLOB_ABORTED:
            sts = printmsg( COPY_GLOBABT, 0, argv[i] );
            break;
        case GLOB_NOMATCH:
            sts = printmsg( COPY_NOMATCH, 0, argv[i] );
            break;
        default:
            sts = printmsg( COPY_GLOBERR, 0, err, argv[i] );
            break;
        }
        if( $FAILED(sts) ) {
            globfree( &globs );
            return sts;
        }
    }
    for( n = 0; n < globs.gl_pathc; n++ ) {
        char *to;

        if( (to = output_file_name( argv[ argc - 1],
                                    globs.gl_pathv[n], NULL )) == NULL ) {
            sts = printmsg( COPY_NOMEM, 0, globs.gl_pathv[n] );
            globfree( &globs );
            return sts;
        }

        if( $MATCHCOND( confirm, COPY_CONFIRM) )
            confirm = confirm_cmd( confirm, globs.gl_pathv[n], to );

        if( !$MATCHCOND( confirm, ODS2_CONFIRM_ALL ) ) {
            if( $MATCHCOND( confirm, ODS2_CONFIRM_YES ) ) {
                confirm = COPY_CONFIRM;
            } else if( $MATCHCOND( confirm, ODS2_CONFIRM_NO ) ) {
                confirm = COPY_CONFIRM;
                free( to );
                continue;
            } else { /* ODS2_CONFIRM_QUIT */
                sts = confirm;
                free( to );
                break;
            }
        }
        sts = copy_1_to( options, globs.gl_pathv[n], to );

        free( to );
        if( $SUCCESSFUL(sts ) )
            ++filecount;
        else if( $VMS_STATUS_SEVERITY(sts) > STS$K_WARNING ||
                 $MATCHCOND(sts, ODS2_CONFIRM_QUIT)  )
            break;
    } /* glob */

    if( $SUCCESSFUL(sts) || $MATCHCOND(sts, ODS2_CONFIRM_QUIT) ) {
        if( filecount > 0 ) {
            if( options & copy_log )
                sts = printmsg( COPY_COPIED, 0, filecount );
        } else {
            sts = printmsg( COPY_NOFILES, 0 );
        }
    }
    globfree( &globs );
    return sts;
}

/******************************************************************* copy_1_to() */
static vmscond_t copy_1_to( options_t options, char *from, char *to ) {
    vmscond_t sts;
    size_t records = 0, bufsize = 80;
    int keepnl = 0;
    char *buf = NULL;
    FILE *fromf;
    struct FAB fab;
    struct RAB rab;
    struct NAM nam;
    struct XABDAT dat;
    struct XABPRO pro;
    char vfc[2], rsbuf[NAM$C_MAXRSS+1];
#ifdef _WIN32
    struct _stat statbuf;
#else
    struct stat statbuf;
#endif

    fab = cc$rms_fab;
    rab = cc$rms_rab;
    nam = cc$rms_nam;
    dat = cc$rms_xabdat;
    pro = cc$rms_xabpro;

    if ( *from == '\0' ) {
        return printmsg( COPY_NONAME, 0 );
    }
    fromf = openf( from, (options & copy_binary)? "rb": "r" );
    if( fromf == NULL ) {
        int err;

        err = errno;
        sts = printmsg( COPY_OPENIN, 0, from );
        if( err )
            sts = printmsg( ODS2_OSERROR, MSG_CONTINUE,
                            COPY_OPENIN, strerror(err) );
        return sts;
    }
    if( fstat( Fileno( fromf ), &statbuf ) != 0 )
        statbuf.st_size = 0;
    else {
        unsigned short tvec[7];

#ifdef _WIN32
        struct tm tmd;
        struct tm *tm = &tmd;
        (void) gmtime_s( &tmd, &statbuf.st_mtime );
#else
        struct tm *tm;

        tm = gmtime( &statbuf.st_mtime );
#endif
        tvec[0] = tm->tm_year + 1900;
        tvec[1] = tm->tm_mon + 1;
        tvec[2] = tm->tm_mday;
        tvec[3] = tm->tm_hour;
        tvec[4] = tm->tm_min;
        tvec[5] = tm->tm_sec;
        tvec[6] = 0;

        if( $SUCCESSFUL(lib_cvt_vectim( tvec, dat.xab$q_rdt )) ) {
#ifdef _WIN32
            (void) gmtime_s( &tmd, &statbuf.st_ctime );
            tvec[0] = tm->tm_year + 1900;
            tvec[1] = tm->tm_mon + 1;
            tvec[2] = tm->tm_mday;
            tvec[3] = tm->tm_hour;
            tvec[4] = tm->tm_min;
            tvec[5] = tm->tm_sec;
            tvec[6] = 0;

            (void) lib_cvt_vectim( tvec, dat.xab$q_cdt );
#else /* No local create time */
            memcpy( &dat.xab$q_cdt, dat.xab$q_rdt, sizeof( dat.xab$q_cdt ) );
#endif

            fab.fab$l_xab = &dat;
        }
    }

    nam.nam$l_rsa = rsbuf;
    nam.nam$b_rss = (uint8_t)(sizeof( rsbuf ) -1);

    if( options & (copy_protect|copy_owner) ) {
        pro.xab$l_nxt = fab.fab$l_xab;
        fab.fab$l_xab = &pro;

        if( options & copy_protect ) {
            pro.xab$w_pro = (uint16_t)
                ((pro.xab$w_pro & ~(protection >> 16)) |
                 (uint16_t)protection);
        }

        if( options & copy_owner ) {
            pro.xab$l_uic = ownuic;
        }
    }

    fab.fab$l_nam = &nam;

    fab.fab$b_fac = FAB$M_PUT;
    fab.fab$l_fop = FAB$M_OFP | FAB$M_TEF;
    fab.fab$b_org = FAB$C_SEQ;

#if 0
    if( options & copy_binary ) {
        fab.fab$w_mrs = 512;
        fab.fab$b_rfm = FAB$C_FIX;
    } else {
        fab.fab$b_rat = 0;
        fab.fab$b_rfm = FAB$C_STMLF;
    }
#endif

    if( !(options & copy_recfmt) ) {
        options |= (FAB$C_VAR +1) << copy_v_recfmt;
        if( !(options & (copy_prn|copy_ftn|copy_cr)) )
            options |= copy_cr;
    }

    fab.fab$b_rfm = ((options & copy_recfmt) >> copy_v_recfmt) -1;

    switch( fab.fab$b_rfm ) {
    case FAB$C_STM:
    case FAB$C_STMLF:
    case FAB$C_STMCR:
        fab.fab$b_rat |= FAB$M_CR;
        keepnl = 1;
        break;
    case FAB$C_UDF:
        keepnl = 1;
        break;
    default:
        keepnl - 0;
        break;
    }

    if( !(options & copy_span) )
        fab.fab$b_rat |= FAB$M_BLK;
    if( options & copy_ftn )
        fab.fab$b_rat |= FAB$M_FTN;
    if( options & copy_cr )
        fab.fab$b_rat |= FAB$M_CR;
    if( options & copy_prn ) {
        fab.fab$b_rat |= FAB$M_PRN;
        fab.fab$b_fsz = 2;
        fab.fab$b_rfm = FAB$C_VFC;

        switch( (options & copy_b4) >> copy_v_b4 ) {
        case copy_prn_none:
            vfc[0] = 0;
            break;
        case copy_prn_nl:
            if( b4n < 1 || b4n > 127 )
                return printmsg( COPY_INVALIDNL, 0 );
            vfc[0] = b4n & 0x7f;
            break;
        case copy_prn_c0:
            if( b4n > 31 )
                return printmsg( COPY_INVALIDC0, 0 );
            vfc[0] = 0x80 | b4n;
            break;
        case copy_prn_vfu:
            if( b4n < 1 || b4n > 16 )
                return printmsg( COPY_INVALIDVFU, 0 );
            vfc[0] = (0xC0 | (b4n -1)) & 0xff;
        }
        switch( (options & copy_after) >> copy_v_after ) {
        case copy_prn_none:
            vfc[1] = 0;
            break;
        case copy_prn_nl:
            if( aftn < 1 || aftn > 127 )
                return printmsg( COPY_INVALIDNL, 0 );
            vfc[1] = aftn & 0x7f;
            break;
        case copy_prn_c0:
            if( aftn > 31 )
                return printmsg( COPY_INVALIDC0, 0 );
            vfc[1] = 0x80 | aftn;
            break;
        case copy_prn_vfu:
            if( aftn < 1 || aftn > 16 )
                return printmsg( COPY_INVALIDVFU, 0 );
            vfc[1] = (0xC0 | (aftn -1)) & 0xff;
        }
    } /* prn */

    fab.fab$l_alq = (statbuf.st_size + 511) / 512;
    fab.fab$w_mrs = (uint16_t)reclen;

    fab.fab$l_fna = to;
    fab.fab$b_fns = (uint8_t)strlen( fab.fab$l_fna );

    do {
        if( options & copy_append ) {
            sts = sys_open( &fab );
            rab.rab$l_rop = RAB$M_EOF;
        } else
            sts = sys_create( &fab );
        rsbuf[nam.nam$b_rsl] = '\0';
        if( $FAILED(sts) ) {
            fclose( fromf );
            printmsg( COPY_OPENOUT, 0, (nam.nam$b_rsl? rsbuf:to) );
            return printmsg( sts, MSG_CONTINUE, COPY_OPENOUT );
        }

        rab.rab$l_fab = &fab;
        rab.rab$b_rac = RAB$C_SEQ;

        if(  $FAILS(sts = sys_connect( &rab )) ) {
            sts = printmsg( sts, 0 );
            break;
        }

        if( fab.fab$b_rfm == FAB$C_VFC ) {
            if( options & copy_prn )
                rab.rab$l_rhb = vfc;
        } else
            options &= ~(copy_prn|copy_vfc);

        if( options & copy_binary ) {
            char *buf;
            uint16_t len;

            if( (buf = malloc( 512 )) == NULL ) {
                sts = printmsg( COPY_NOMEM, 0, from );
                break;
            }
            rab.rab$l_rbf = buf;
            while( (len = (uint16_t)fread( buf, 1, 512, fromf )) > 0 ) {
                if( len != 512 )
                    memset( buf + len, 0, 512 - len );
                rab.rab$w_rsz = 512;

                if( $FAILS(sts = sys_put( &rab )) ) {
                    printmsg( COPY_WRITEERR, 0, rsbuf );
                    sts = printmsg( sts, MSG_CONTINUE, COPY_WRITEERR );
                    break;
                }
                ++records;
            }
            free( buf );
            buf = NULL;
            break;
        }
        while( (rab.rab$l_rbf = fgetline( fromf, keepnl, &buf, &bufsize )) != NULL ) {
            size_t len;
            char * rp;

            rp = rab.rab$l_rbf;
            len = strlen( rp );
            if( (options & (copy_prn | copy_vfc)) == copy_vfc ) {
                if( len < fab.fab$b_fsz ) {
                    sts = RMS$_RSZ;
                    break;
                }
                rab.rab$l_rhb = rp;
                rab.rab$l_rbf = rp + fab.fab$b_fsz;
                len -= fab.fab$b_fsz;
            }

            do {
                rab.rab$w_rsz = (uint16_t) (len % (MAXREC + 1));
                sts = sys_put( &rab );
                rab.rab$l_rbf += rab.rab$w_rsz;
                len -= rab.rab$w_rsz;
            } while( len > 0 && $SUCCESSFUL( sts ) );
            ++records;
            if( $FAILED(sts) ) {
                printmsg( COPY_WRITEERR, 0, rsbuf );
                sts = printmsg( sts, MSG_CONTINUE, COPY_WRITEERR );
                break;
            }
        }
    } while( 0 );

    if( buf != NULL ) free( buf );
    sys_disconnect( &rab );
    {
        vmscond_t clsts;

        if( $FAILS(clsts = sys_close( &fab )) && $SUCCESSFUL(sts) ) {
            printmsg( COPY_CLOSEOUT, 0, rsbuf );
            clsts = printmsg( clsts, MSG_CONTINUE, COPY_CLOSEOUT );
        }
        if( $SUCCESSFUL(sts) )
            sts = clsts;
    }
    fclose( fromf );

    if( options & copy_log ) {
        sts = (options & copy_binary)? COPY_COPIEDB: COPY_COPIEDR;

        sts = printmsg( sts, 0,
                        from, fab.fab$l_nam->nam$l_rsa,
                        records );
    }

    return sts;
}

/******************************************************************* output_file_name() */

char *output_file_name( const char *ofname, const char *ifname, int *flags ) {
    char *ofn, *from, *fname;
    const char *p, *q, *e;
    size_t len, bufsize[2];

    if( flags != NULL )
        *flags = 0;

    ofn= NULL;
    bufsize[0] =
        bufsize[1] = 0;

    len = strlen( ifname ) +1;
    if( (from = malloc( len )) == NULL ) {
        return NULL;
    }
    memcpy( from, ifname, len );
#ifdef _WIN32
    for( fname = from; *fname != '\0'; fname++ )
        if( *fname == '/' )
            *fname = '\\';
#endif

    fname = basename( from );
    e = strrchr( fname, '.' );

    p = strrchr( ofname, ']' );
    if( p == NULL )
        p = strrchr( ofname, ':' );
    if( p == NULL )
        len = 0;
    else
        len = 1 + (size_t)( p - ofname );

    if( !xpand( &ofn, bufsize, len ) ) {
        free( from );
        return NULL;
    }
    memcpy( ofn, ofname, len );
    if( p == NULL )
        p = ofname;
    else
        ++p;

    if( p[0] == '*' && (!p[1] || p[1] == '.' || p[1] == ';') ) {
        if( flags )
            *flags |= OF_WILD;
        len = (e == NULL)? strlen(fname) : (size_t)(e - fname);
        if( !xpand( &ofn, bufsize, len ) ) {
            free( from );
            return NULL;
        }
        memcpy( ofn+ bufsize[1] -len, fname, len );
        ++p;
    } else {
        q = strchr( p, '.' );
        if( q == NULL )
            q = strrchr( p, ';' );
        if( q == NULL )
            q = p + strlen( p );
        len = (size_t)(q - p);

        if( !xpand( &ofn, bufsize, len ) ) {
            free( from );
            return NULL;
        }
        memcpy( ofn + bufsize[1] - len, p, len );
        p += len;
    }

    if( p[0] == '.' ) {
        q = strrchr( p+1, ';' );
        if( q == NULL )
            q = strrchr( p+1, '.' );
        if( q == NULL || q <= p )
            q = p + strlen( p );
        len = (size_t)(q - p );
        if( len == 2 && p[1] == '*' ) {
            if( flags )
                *flags |= OF_WILD;
            if( e != NULL ) {
                len = strlen( e );
                if( !xpand( &ofn, bufsize, len ) ) {
                    free( from );
                    return NULL;
                }
                memcpy( ofn+ bufsize[1] - len, e, len );
            }
            p += 2;
        } else {
            if( !xpand( &ofn, bufsize, len ) ) {
                free( from );
                return NULL;
            }
            memcpy( ofn+  bufsize[1] - len, p, len );
            p += len;
        }
    }
    if( *p == ';' || *p == '.' ) {
        len = strlen( p );
        if( !xpand( &ofn, bufsize, len ) ) {
            free( from );
            return NULL;
        }
        ofn[bufsize[1] - len] = ';';
        memcpy( ofn+ bufsize[1] + 1 - len, p+1, len -1 );
        p += len;
    }
    if( *p )
        abort();

    ofn[bufsize[1]] = '\0';
    free( from );

    return ofn;
}

/******************************************************************* xpand() */

static int xpand( char **buf, size_t size[2], size_t add ) {
    char *nbuf;
    size_t need;

    need = (size[1] += add) + 1;
    if( need < size[0] )
        return 1;

    nbuf = realloc( *buf, need + 32 + 1 );
    if( nbuf == NULL ) {
        free( *buf );
        *buf = NULL;
        return 0;
    }
    *buf = nbuf;
    size[0] = need + 32 + 1;
    return 1;
}
