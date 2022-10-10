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

#if !defined( DEBUG ) && defined( DEBUG_SEARCHCMD )
#define DEBUG DEBUG_SEARCHCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmddef.h"

#define CSI "\033["

static const char *const C0[32] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS" , "TAB", "LF" , "VT" , "FF" , "CR" , "SO" , "SI" ,
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM" , "SUB", "ESC", "FS" , "GS" , "RS" , "US" };
static const char *const C1[33] = { /* 0x80 - 0xA0 */
    "80" , "81" , "BPH",  "NBH", "IND", "NEL", "SSA", "ESA",
    "HTS", "HTJ", "VTS",  "PLD", "PLU", "RI" , "SS2", "SS3",
    "DCS", "PU1", "PU2",  "STS", "CCH", "MW" , "SPA", "EPA",
    "SOS", "SGCI","DECID","CSI", "ST" , "OSC", "PM" , "APC",
    "NSP" };
static uint8_t CW[256], CWS;

#if MAXREC <= INT8_MAX
typedef int8_t mpos_t;
#elif MAXREC <= INT16_MAX
typedef int16_t mpos_t;
#else /* MAXREC <= INT32_MAX */
typedef int32_t mpos_t;
#endif

static int prep( int fc, char *pattern, int plen, void **ctx );
static int match( int fc, char *pattern, int plen,
                  char *data, int dlen, mpos_t *mpos, void *ctx );

/***************************************************************** dosearch() */

#define search_exact     OPT_GENERIC_1
#define search_number    OPT_GENERIC_2
#define search_confirm   OPT_GENERIC_3
#define search_heading   OPT_GENERIC_4
#define search_nohead    OPT_GENERIC_5
#define search_log       OPT_GENERIC_6
#define search_highlight OPT_GENERIC_7
#define search_nowindow  OPT_GENERIC_8
#define search_page      OPT_GENERIC_9
#define search_blink     OPT_SHARED_1
#define search_bold      OPT_SHARED_2
#define search_reverse   OPT_SHARED_3
#define search_underline OPT_SHARED_4
#define search_hardcopy  OPT_SHARED_5

#define search_match     (OPT_SHARED_7|OPT_SHARED_6)
#define search_v_match   (OPT_V_SHARED + 5)

#define MAT_AND    0
#define MAT_OR     1
#define MAT_NAND   2
#define MAT_NOR    3

#define search_mat_and   (MAT_AND  << search_v_match)
#define search_mat_or    (MAT_OR   << search_v_match)
#define search_mat_nand  (MAT_NAND << search_v_match)
#define search_mat_nor   (MAT_NOR  << search_v_match)

static qual_t
matchkwds[] = { {"and",  search_mat_and,  search_match, NV,
                 "commands search qual_match match_and"},
                {"or",   search_mat_or,   search_match, NV,
                 "commands search qual_match match_or"},
                {"nand", search_mat_nand, search_match, NV,
                 "commands search qual_match match_nand"},
                {"nor",  search_mat_nor,  search_match, NV,
                 "commands search qual_match match_nor"},

                {NULL,  0,               0,            NV, NULL}
};

static qual_t
highkwds[] = { {"blink",     search_blink,     0, NV,
                "commands search qual_highlight highlight_blink"},
               {"bold",      search_bold,      0, NV,
                "commands search qual_highlight highlight_bold"},
               {"hardcopy",  search_hardcopy,  0, NV,
                "commands search qual_highlight highlight_hardcopy"},
               {"reverse",   search_reverse,   0, NV,
                "commands search qual_highlight highlight_reverse"},
               {"underline", search_underline, 0, NV,
                "commands search qual_highlight highlight_underline"},

               {NULL,       0,                0, NV, NULL}
};

static uint32_t height;

const int search_defaults = search_highlight | search_mat_or;
defqualsp_t search_defopt = NULL;

qual_t
searchquals[] = { {"confirm",     search_confirm,   0,                NV,
                   "-commands search qual_confirm"},
                  {"noconfirm",   0,                search_confirm,   NV, NULL},
                  {"exact",       search_exact,     0,                NV,
                   "-commands search qual_exact"},
                  {"noexact",     0,                search_exact,     NV, NULL},
                  {"log",         search_log,       0,                NV,
                   "-commands search qual_log"},
                  {"nolog",       0,                search_log,       NV, NULL},
                  {"heading",     search_heading,   search_nohead,    NV,
                   "-commands search qual_heading"},
                  {"noheading",   search_nohead,    search_heading,   NV, NULL},
                  {"highlight",   search_highlight, 0,                VOPT(KV(highkwds)),
                   "-commands search qual_highlight"},
                  {"nohighlight", 0,                search_highlight, NV, NULL},
                  {"match_type",  0,                0,                KV(&matchkwds),
                   "commands search qual_match"},
                  {"nowindow",    search_nowindow|search_heading,      /* ~~/WINDOW=0 */
                                                    search_nohead,    NV,
                   "commands search qual_nowindow"},
                  {"numbers",     search_number,    0,                NV,
                   "-commands search qual_numbers"},

                  {"nonumbers",   0,                search_number,    NV, NULL},
                  {"page",        search_page,      0,                VOPT(DV(&height)),
                        "-commands search qual_page"},
                  {"nopage",      0,                search_page,      NV, NULL},
                  {NULL,          0,                0,                NV, NULL}
};

param_t
searchpars[] = { {"filespec", REQ, FSPEC,  NOPA, "commands search filespec"},
                 {"string",   REQ|NOLIM, STRING, NOPA, "commands search string"},
                 { NULL, 0, 0, NOPA, NULL }
};

DECL_CMD(search) {
    vmscond_t sts = 0;
    vmscond_t confirm = ODS2_CONFIRM_ALL;
    options_t options;
    uint32_t filecount = 0;
    uint32_t findcount = 0;
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    char *rec = NULL;
    mpos_t *mpos = NULL;
    uint32_t *mind = NULL;
    int theight;
    pause_t *pause = NULL, pausectl;
    char highlight[sizeof(CSI ";1;4;5;7m")];
    struct sstr {
        void *ctx;
        int   slen;
        char *string;
    } *sstr, *ss;
    int nstrings;
    uint32_t match_type;
    struct NAM nam;
    struct FAB fab;
    struct RAB rab;

    memset( res, 0, sizeof(res) );
    memset( rsa, 0, sizeof(rsa) );
    memset( &pausectl, 0, sizeof(pausectl) );

    if( !CWS ) {
        unsigned int i;

        for( i = 0; i < 256; i++ ) {
            if( isprint( i ) )
                CW[i] = 1;
            else if( i < 32 )
                 CW[i] = 2 + (unsigned char)strlen( C0[i] );
            else if( i == '\177' )
                CW[i] = sizeof( "<DEL>" ) -1;
            else if( i >= 0x80ul && i <= 0xA0ul )
                CW[i] = 2 + (unsigned char)strlen( C1[i - 0x80ul] );
            else
                CW[i] = sizeof( "<xx>" ) -1;
        }
        CWS = TRUE;
    }

    (void) termchar( NULL, &theight );
    height = theight-1;

    if( search_defopt != NULL ) {
        if( $FAILS(sts = checkquals( &options, search_defaults, searchquals,
                                     search_defopt->qualc, search_defopt->qualv )) ) {
            return sts;
        }
    }
    else
        options = search_defaults;

    if( $FAILS(sts = checkquals( &options, options,
                                 searchquals, qualc, qualv )) )
        return sts;

    match_type = (options & search_match) >> search_v_match;

    if( options & search_page ) {
        pausectl.interval = height;
        pausectl.lineno = 0;
        pausectl.prompt = SEARCH_CONTINUE;
        pause = &pausectl;
    }

    if( options & search_highlight ) {
        if( !(options &
              (search_bold|search_blink|search_underline|search_reverse|search_hardcopy)) )
            memcpy( highlight, CSI "1m", sizeof( CSI "1m" ) );
        else if( options & (search_bold|search_blink|search_underline|search_reverse) ) {
            memcpy( highlight, CSI, sizeof( CSI ) );
            if( options & search_bold )
                memcpy( highlight+strlen(highlight), ";1", sizeof( ";1" ) );
            if( options & search_blink )
                memcpy( highlight+strlen(highlight), ";5", sizeof( ";5" ) );
            if( options & search_reverse )
                memcpy( highlight+strlen(highlight), ";7", sizeof( ";7" ) );
            if( options & search_underline )
                memcpy( highlight+strlen(highlight), ";4", sizeof( ";4" ) );
            memcpy( highlight+strlen(highlight), "m", sizeof( "m" ) );
        } else
            highlight[0] = '\0';
    }

    nstrings = argc - 2;
    if( (sstr = calloc( nstrings, sizeof( struct sstr ) )) == NULL ) {
        return printmsg( SS$_INSFMEM, 0 );
    }

    if( options & search_confirm )
        confirm = SEARCH_CONFIRM;

    nam = cc$rms_nam;
    fab = cc$rms_fab;
    rab = cc$rms_rab;
    rab.rab$l_fab = &fab;

    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;

    fab.fab$l_nam = &nam;
    fab.fab$b_fac = FAB$M_GET;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = (uint8_t)strlen(fab.fab$l_fna);

    do {
        int i;

        if( (mind = malloc( (size_t)4u * ((MAXREC + 2u + 31u) / 32u) )) == NULL ) {
            sts = printmsg( SS$_INSFMEM, 0 );
            break;
        }

        for( ss = sstr, i = 0; i < nstrings; ss++, i++ ) {
            ss->string = argv[2 + i];
            ss->slen = (int)strlen( ss->string);
            if( ss->slen < 1 ) {
                sts = printmsg( SEARCH_BADSTRING, 0, i + 1 );
                break;
            }
            if( !prep( !(options & search_exact),
                       ss->string, ss->slen, &ss->ctx ) ) {
                sts = printmsg( SS$_INSFMEM, 0 );
                break;
            }
        }

        if( $FAILS(sts = sys_parse(&fab)) ) {
            printmsg( SEARCH_PARSEFAIL, 0, fab.fab$l_fna );
            sts = printmsg( sts, MSG_CONTINUE, SEARCH_PARSEFAIL );
            break;
        }
        nam.nam$l_esa[nam.nam$b_esl] = '\0';
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = 0;

        if( (rec = malloc( MAXREC + 2 )) == NULL ) {
            sts = printmsg( SS$_INSFMEM, 0 );
            break;
        }
        if( (mpos = malloc( MAXREC * sizeof( mpos_t ) )) == NULL ) {
            sts = printmsg( SS$_INSFMEM, 0 );
            break;
        }

        while( TRUE ) {
            int32_t lineno, filefind;
            int first = TRUE;

            if( $FAILS(sts = sys_search( &fab )) ) {
                if( $MATCHCOND(sts, RMS$_NMF) ) {
                    sts = SS$_NORMAL;
                    break;
                }
                printmsg( SEARCH_SEARCHFAIL, 0, nam.nam$l_esa );
                sts = printmsg( sts, MSG_CONTINUE, SEARCH_SEARCHFAIL );
                break;
            }
            nam.nam$l_rsa[ nam.nam$b_rsl ] = '\0';

            if( $FAILS(sts = sys_open(&fab)) ) {
                printmsg( SEARCH_OPENIN, 0, nam.nam$l_rsa );
                sts = printmsg( sts, MSG_CONTINUE, SEARCH_OPENIN );
                continue;
            }
            if( $MATCHCOND( confirm, SEARCH_CONFIRM) )
                confirm = confirm_cmd( confirm,  nam.nam$l_rsa);

            if( !$MATCHCOND( confirm, ODS2_CONFIRM_ALL ) ) {
                if( $MATCHCOND( confirm, ODS2_CONFIRM_YES ) ) {
                    confirm = SEARCH_CONFIRM;
                } else if( $MATCHCOND( confirm, ODS2_CONFIRM_NO ) ) {
                    confirm = SEARCH_CONFIRM;
                    sys_close( &fab );
                    continue;
                } else { /* ODS2_CONFIRM_QUIT */
                    sys_close( &fab );
                    sts = confirm;
                    break;
                }
            }

            if( $FAILS(sts = sys_connect( &rab )) ) {
                printmsg( sts, 0 );
                sys_close( &fab );
                continue;
            }

            filecount++;
            lineno = 0;
            filefind = 0;
            rab.rab$l_ubf = rec;
            rab.rab$w_usz = MAXREC;
            while( $SUCCESSFUL(sts = sys_get( &rab )) ) {
                int indent = 0, matched = 0, hl = FALSE;
                unsigned m;
                char *p;

                ++lineno;
                for( ss = sstr; ss < sstr + nstrings; ss++ ) {
                    int nmat;

                    nmat = match( !(options & search_exact),
                                  ss->string, ss->slen,
                                  rec, rab.rab$w_rsz, mpos, ss->ctx );
                    if( nmat <= 0 )
                        continue;
                    if( !matched++ )
                        memset( mind, 0, 4u * (((size_t)rab.rab$w_rsz + 31u) / 32u) );
                    for( i = 0; i < nmat; i++ )
                        for( m = mpos[i]; m < (unsigned)(mpos[i] + ss->slen); m++ )
                            mind[ m >> 5 ] |= UINT32_C(1) << (m & 31u);
                }
                if( match_type == MAT_OR ) {
                    if( !(matched > 0) )
                        continue;
                } else if( match_type == MAT_AND ) {
                    if( !(matched == nstrings) )
                        continue;
                } else if( match_type == MAT_NOR ) {
                    if( !!(matched > 0) )
                        continue;
                } else { /* MAT_NAND */
                    if( !!(matched == nstrings) )
                        continue;
                }
                findcount++;
                filefind++;
                if( first && !(options & search_nohead) &&
                    ((options & search_heading) ||
                     (nam.nam$l_fnb & NAM$M_WILDCARD)) ) {
                    first = FALSE;
                    if( options & search_nowindow ) {
                        printf( "%.*s", nam.nam$b_rsl, nam.nam$l_rsa );
                        if( $FAILS(sts = put_c( '\n', stdout, pause )) )
                            break;
                        continue;
                    } else {
                        if( $FAILS(sts = put_str( "\n******************************\n",
                                                  stdout, pause )) )
                            break;
                        printf( "%.*s", nam.nam$b_rsl, nam.nam$l_rsa );
                        if( $FAILS(sts = put_strn( "\n\n", 2, stdout, pause )) )
                            break;
                    }
                } else if( options & search_nowindow )
                    continue;

                if( options & search_number )
                    indent = printf( "%u: ", lineno );

                for( m = 0, p = rec; p < rec + rab.rab$w_rsz; m++, p++ ) {
                    if( matched && (options & search_highlight) ) {
                        if( mind[ m >> 5 ] & (UINT32_C(1) << (m & 31u)) ) {
                            if( !hl && highlight[0] )
                                fputs( highlight, stdout );
                            hl = TRUE;
                        } else {
                            if( hl && highlight[0] )
                                fputs( CSI "m", stdout );
                            hl = FALSE;
                        }
                    }
                    if( isprint( *p & 0xFFul ) ) {
                        putc( (*p & 0xFFul), stdout );
                    } else if( (*p & 0xFFul) < 32 ) {
                        printf( "<%s>", C0[*p & 0x1Ful] );
                    } else if( *p == '\177' ) {
                        printf( "<DEL>" );
                    } else if( (*p & 0xFFul) >= 0x80ul &&
                               (*p & 0xFFul) <= 0xA0ul ) { /* Special case A0 for code analysis */
                        printf( "<%s>", C1[ ((*p & 0xFFul) == 0xA0ul)? 0x20ul :
                                           (((*p & 0xFFul) - 0x80ul) & 0x1Ful) ] );
                    } else
                        printf( "<%02lX>", (*p & 0xFFul) );
                }
                if( hl && highlight[0] )
                    fputs( CSI "m", stdout );
                hl = FALSE;

                if( $FAILS(sts = put_c( '\n', stdout, pause )) )
                    break;

                if( matched && (options & search_hardcopy) ) {
                    unsigned e;

                    for( e = rab.rab$w_rsz; e > 0; --e )
                        if( mind[ (e - 1) >> 5 ] & (UINT32_C(1) << ((e -1) & 31u)) )
                            break;
                    if( e > 0 ) {
                        printf( "%*s", indent, "" );
                        for( m = 0, p = rec; m < e; m++, p++ ) {
                            int i;

                            for( i = 0; i < CW[*p & 0xFFul]; i++ )
                                fputc( (mind[ m >> 5] & (UINT32_C(1) << (m & 31u)))?
                                       '^': ' ', stdout );
                        }
                        if( $FAILS(sts = put_c( '\n', stdout, pause)) )
                            break;
                    }
                }
            }
            sys_disconnect( &rab );
            sys_close( &fab );
            if( options & search_log ) {
                if( filefind > 0 ) {
                    printmsg( SEARCH_MATCHED, 0,
                              nam.nam$l_rsa, lineno, filefind );
                } else {
                    printmsg( SEARCH_FILE, 0, nam.nam$l_rsa, lineno );
                }
            }
            if( $MATCHCOND(sts, SS$_ABORT) )
                break;
        }
    } while( 0 );

    for( ss = sstr; ss < sstr + nstrings; ss++ )
        free( ss->ctx );
    free( sstr );
    free( mind );
    free( rec );
    free( mpos );

    fab.fab$b_fns =
        nam.nam$b_ess =
        fab.fab$b_dns = 0;
    nam.nam$l_rlf = NULL;
    nam.nam$b_nop = NAM$M_SYNCHK;
    (void) sys_parse( &fab );                    /* Release context */

    if( $SUCCESSFUL(sts) ||
        $MATCHCOND(sts, SS$_ABORT) || $MATCHCOND(sts, ODS2_CONFIRM_QUIT) ) {
        if( filecount < 1 ) {
            sts = printmsg( SEARCH_NOFILES, 0 );
        } else {
            if( findcount < 1 ) {
                sts = printmsg( SEARCH_NOMATCH, 0 );
            }
        }
    } else {
        sts = printmsg(sts, 0 );
    }
    return sts;
}

/***************************************************************** prep() */

static int prep( int fc, char *pattern, int plen, void **ctx ) {
    int i, j, k = 0;
    int *bm, *gm, *sx;

    /* BM1977 */

    *ctx = NULL;

#define AS (UCHAR_MAX+1)
#define ALLOC(plen) ( (AS + plen) * sizeof( int ) )

    if( (bm = (int *)malloc( ALLOC((size_t)plen) )) == NULL ) {
        return FALSE;
    }
    if( (sx = (int *)malloc( plen * sizeof( int ) )) == NULL ) {
        free( bm );
        return FALSE;
    }

    *ctx = bm;
    gm = bm + AS;

    for( i = 0; i < AS; i++ ) {
        bm[i] = (int)plen;
    }
    j = plen - 1;
    if( fc ) {
        for( i = 0; i < j; i++ ) {
            pattern[i] = tolower( pattern[i] );
            bm[ (unsigned)(pattern[i] & 0xFFul) ] = j - i;
        }
        pattern[i] = tolower( pattern[i] & 0xFFul );
    } else
        for( i = 0; i < j; i++ ) {
            bm[ (unsigned)(pattern[i] & 0xFFul) ] = j - i;
        }
    sx[j] = plen;
    for( i = plen - 2; i >= 0; i-- ) {
        if( i > j && sx[ i + plen -1 - k ] < i - j ) {
            sx[i] = sx[ i + plen - 1 -k ];
        } else {
            if( i < j ) {
                j = i;
            }
            k = i;
            while( j >= 0 && pattern[j] == pattern[ j + plen - 1 - k ] ) {
                --j;
            }
            sx[i] = k - j;
        }
    }

    for( i = 0; i < plen; i++ ) {
        gm[i] = plen;
    }

    k = plen - 1;
    for( j = 0, i = k; i >= 0; i-- ) {
        if( sx[i] == i + 1 ) {
            for( ; j < k - i; j++ ) {
                if( gm[j] == plen ) {
                    gm[j] = k - i;
                }
            }
        }
    }
    for( i = 0; i <= plen - 2; i++ ) {
        gm[ k - sx[i] ] = k - i;
    }
    free( sx );

    return TRUE;
}

/***************************************************************** match() */

static int match( int fc, char *pattern, int plen,
                  char *data, int dlen, mpos_t *mpos, void *ctx ) {
    int i, j, n = 0;
    int *bm, *gm;

    bm = ctx;
    gm = bm + AS;

    if( fc ) {
        for( j = 0; j <= dlen - plen; ) {
            for( i = plen -1;
                 i >= 0 && pattern[i] == tolower( data[i + j] & 0xFFul ); i-- )
                ;
            if( i < 0 ) {
                if( mpos != NULL )
                    mpos[ n ] = j;
                ++n;
                j += gm[0];
            } else {
                int g, b;

                g = gm[i];
                b =  bm[ (unsigned)(tolower( data[i + j] & 0xFFul )
                                    & 0xFFul) ] - plen + 1 + i;
                j += (g > b)? g: b;
            }
        }

        return n;
    }

    for( j = 0; j <= dlen - plen; ) {
        for( i = plen -1; i >= 0 && pattern[i] == data[i + j]; i-- )
                ;
        if( i < 0 ) {
            if( mpos != NULL )
                mpos[ n ] = j;
            ++n;
            j += gm[0];
        } else {
            int g, b;

            g = gm[i];
            b =  bm[ (unsigned)(data[i + j] & 0xFFul) ] - plen + 1 + i;
            j += (g > b)? g: b;
        }
    }

    return n;
}
