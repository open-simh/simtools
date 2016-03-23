/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_SEARCHCMD )
#define DEBUG DEBUG_SEARCHCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"


/***************************************************************** dosearch() */

/* dosearch: a simple file search routine */

param_t searchpars[] = { {"filespec", REQ, VMSFS,  NOPA, "for file to search. Wildcards are allowed."},
                                     {"string",   REQ, STRING, NOPA, "string to search for."},
                                     { NULL, 0, 0, NOPA, NULL }
};

DECL_CMD(search) {
    int sts = 0;
    int filecount = 0;
    int findcount = 0;
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    register char firstch, *searstr, *searend, *s;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    struct RAB rab = cc$rms_rab;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    searstr = argv[2]; /* unquote( argv[2] ); */

    searend = searstr + strlen( searstr );
    for ( s = searstr; s < searend; s++ ) {
        *s = tolower( *s );
    }
    firstch = *searstr++;
    filecount = 0;
    findcount = 0;
    nam = cc$rms_nam;
    fab = cc$rms_fab;
    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    fab.fab$l_dna = "";
    fab.fab$b_dns = strlen(fab.fab$l_dna);
    sts = sys_parse(&fab);
    if ( sts & STS$M_SUCCESS ) {
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
        while ( ( sts = sys_search( &fab ) ) & STS$M_SUCCESS ) {
            sts = sys_open(&fab);
            if ( !( sts & STS$M_SUCCESS ) ) {
                printf("%%SEARCH-F-OPENFAIL, Open error: %s\n",getmsg(sts, MSG_TEXT));
            } else {
                rab.rab$l_fab = &fab;
                sts = sys_connect( &rab );
                if ( sts & STS$M_SUCCESS ) {
                    int printname = 1;
                    char *rec;

                    if( (rec = malloc( MAXREC + 2 )) == NULL )
                        sts = SS$_INSFMEM;
                    else {
                        filecount++;
                        rab.rab$l_ubf = rec;
                        rab.rab$w_usz = MAXREC;
                        while( (sts = sys_get( &rab )) & STS$M_SUCCESS ) {
                            register char *strng = rec;
                            register char *strngend = strng + (rab.rab$w_rsz -
                                (searend - searstr));
                            while( strng < strngend ) {
                                register char ch = *strng++;
                                if( ch == firstch ||
                                    (ch >= 'A' && ch <= 'Z' &&
                                        ch + 32 == firstch) ) {
                                    register char *str = strng;
                                    register char *cmp = searstr;
                                    while( cmp < searend ) {
                                        register char ch2 = *str++;
                                        ch = *cmp;
                                        if( ch2 != ch &&
                                            (ch2 < 'A' || ch2 > 'Z' ||
                                                ch2 + 32 != ch) ) break;
                                        cmp++;
                                    }
                                    if( cmp >= searend ) {
                                        findcount++;
                                        rec[rab.rab$w_rsz] = '\0';
                                        if( printname ) {
                                            rsa[nam.nam$b_rsl] = '\0';
                                            printf(
                                                "\n******************************\n"
                                                );
                                            printf( "%s\n\n", rsa );
                                            printname = 0;
                                        }
                                        fputs( rec, stdout );
                                        if( fab.fab$b_rat & PRINT_ATTR ) {
                                            fputc( '\n', stdout );
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        free( rec );
                        rec = NULL;
                    }
                    sys_disconnect(&rab);
                }
                if (sts == SS$_NOTINSTALL) {
                    printf(
                        "%%SEARCH-W-NOIMPLEM, file operation not implemented\n"
                    );
                    sts = SS$_NORMAL;
                }
                sys_close(&fab);
            }
        }
        if (sts == RMS$_NMF || sts == RMS$_FNF) sts = SS$_NORMAL;
    }
    if ( sts & STS$M_SUCCESS ) {
        if (filecount < 1) {
            printf("%%SEARCH-W-NOFILES, no files found\n");
        } else {
            if (findcount < 1) {
                printf("%%SEARCH-I-NOMATCHES, no strings matched\n");
            }
        }
    } else {
        printf("%%SEARCH-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}


