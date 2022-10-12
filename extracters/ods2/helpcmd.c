/* Copyright (c) 2022 Timothe Litt litt@acm.org
 */

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

#if !defined( DEBUG ) && defined( DEBUG_HELPCMD )
#define DEBUG DEBUG_HELPCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#ifndef _GNU_SOURCE
#define _XOPEN_SOURCE
#endif

#include "ods2.h"

#define HELPFILEDEFS
#include "cmddef.h"

#include "compat.h"
#include "version.h"

#ifndef TERM_MINWIDTH
#define TERM_MINWIDTH 132
#endif

static unsigned depth = 0;

extern CMDSET_t maincmds[];
extern void show_version( int full );

static void cmdhelp( CMDSETp_t cmdset );
static void parhelp( CMDSETp_t cmd, int argc, char **argv );
static void qualhelp( int par, qualp_t qtable );

static int cmdcmp( const void *qa, const void *qb );
static int qualcmp( const void *qa, const void *qb );

static vmscond_t topiclist( char *topic );
static vmscond_t findtopic( options_t options,
                            char *topic, hlptopic_t **rethlp );
static vmscond_t rdhelp( const char *filename );
static int reloc( hlproot_t *root, ptrdiff_t bufsize );
static int hlpcmp( const void *qa, const void *qb );
static int hlpcmpa( const void *key, const void *tgt );
static void helpexit( void );

static char *helpfile = NULL;
static char *helpdata = NULL;
#define hlproot ((hlproot_t *)(helpdata + sizeof( hlphdr_t )))

/* Local topics */
#if DEBUG
static DECL_CMD(hlpdata);
static vmscond_t printdata( unsigned level, hlproot_t *root );
#endif
static DECL_CMD(credits);
static DECL_CMD(invocation);
static DECL_CMD(syntax);

DECL_CMD(mounthelp); /* mountcmd.c */

static CMDSET_t helpitems[] = {
#if DEBUG
    { "hlpdata",       dohlpdata,    0, NULL, NULL, "" },
#endif
    { "credits",       docredits,    0, NULL, NULL, "" },
    { "syntax",        dosyntax,     0, NULL, NULL, "" },
    { "invocation",    doinvocation, 0, NULL, NULL, "" },
    { "specify_mount", domounthelp,  0, NULL, NULL, "" },
#ifdef USE_VHD
    { "VHD_image_files", NULL,  0, NULL, NULL, "vhd_image_files" },
#endif

    {  NULL,        NULL,      0, NULL, NULL, NULL }
};

/************************************************************** sethelpfile() */

vmscond_t sethelpfile( char *filename, char *env, char *pname ) {
    static int xitset = FALSE;

    if( filename != NULL ) {
        char *p;
        p = get_realpath( filename );
        if( p == NULL )
            return printmsg( HELP_OPENHLP, 0, filename );
        filename = p;
    } else {
        char *p;
        if( env != NULL && pname != NULL && (filename = get_env( env )) == NULL ) {
            size_t n;

            n = strlen( pname );
            filename = malloc( n + sizeof( ".hlb" ) );
            if( filename == NULL )
                return ODS2_INITIALFAIL;
            memcpy( filename, pname, n+1 );
#if defined( VMS ) || defined( _WIN32 )
#ifdef VMS
            p = strrchr( filename, ';' );
            if( p != NULL ) {
                *p = '\0';
                n = (size_t)(p - filename);
            }
#endif
            if( (p = strstr( filename+n-4, ".exe")) ||
                (p = strstr( filename+n-4, ".EXE")) ) {
                *p = '\0';
                n = (size_t)(p - filename);
            }
#endif
            memcpy( filename+n, ".hlb", sizeof( ".hlb" ) );
        }
        p = get_realpath( filename );
        if( p == NULL ) {
            vmscond_t sts;

            sts = printmsg( HELP_OPENHLP, 0, filename );
            free( filename );
            return sts;
        }
        free( filename );
        filename = p;
    }
    if( filename == NULL )
        return SS$_BADPARAM;

    if( helpdata != NULL ) {
        free(helpdata);
        helpdata = NULL;
    }
    if( helpfile != NULL )
        free( helpfile );
    helpfile = filename;

    if( !xitset ) {
        atexit( &helpexit );
        xitset = TRUE;
    }
    return SS$_NORMAL;
}

/************************************************************ showthelpfile() */

vmscond_t showhelpfile( void ) {
    vmscond_t sts;
    hlphdr_t *hdr;
    char *ddate;

    if( (helpdata == NULL && (helpfile == NULL ||
                              $FAILS(sts = rdhelp( helpfile )))) ||
        helpdata == NULL )
        return printmsg( ODS2_HELPFILE, 0, "none" );
    hdr = (hlphdr_t *)helpdata;
    printmsg( ODS2_HELPFILE, 0, helpfile );
    if( (ddate = Ctime( &hdr->ddate )) == NULL ) {
        ddate = malloc( 3 );
        if( ddate == NULL ) exit( EXIT_FAILURE );
        memcpy( ddate, "??", 3 );
    }
    if( hdr->psize == hdr->ssize ) {
        sts = printmsg( ODS2_HELPFILEVER1, MSG_CONTINUE, ODS2_HELPFILE,
                        hdr->version, hdr->psize *8, ddate );
    } else {
        sts = printmsg( ODS2_HELPFILEVER1, MSG_CONTINUE, ODS2_HELPFILE,
                        hdr->version, hdr->ssize *8, hdr->psize *8, ddate );
    }
    free( ddate );
    return sts;
}

/******************************************************************* dohelp() */

/* help: Display help guided by command table. */

param_t
helppars[] = { {"topic",    OPT,       STRING, NOPA, "commands help topic"},
               {"subtopic", OPT|NOLIM, STRING, NOPA, "commands help subtopic"},

               {NULL,        0,        0,      NOPA, NULL}
};

DECL_CMD(help) {
    CMDSETp_t cmd;
    vmscond_t sts;
    int wid;
    static int widrpt = FALSE;

    UNUSED(qualc);
    UNUSED(qualv);

    termchar( &wid, NULL );
    if( wid < TERM_MINWIDTH ) {
        if( !widrpt ) {
            printmsg( HELP_TERMWID, 0, TERM_MINWIDTH );
            widrpt = TRUE;
        }
    } else
        widrpt = FALSE;

    if( argc <= 1 ) {
        show_version( 0 );
        printf( "\n" );
        printmsg( HELP_HEAD, MSG_TEXT );
        cmdhelp( maincmds );
        putchar( '\n' );
        printmsg( HELP_MORE, MSG_TEXT );
        cmdhelp( helpitems );
        putchar( '\n' );
        printmsg( HELP_EXPLAIN, MSG_TEXT );

        return SS$_NORMAL;
    }

    for( cmd = maincmds; cmd->name; cmd++ ) {
        if( keycomp(argv[1],cmd->name) ) {
            if( argc >= 3 ) {
                parhelp( cmd, argc-2, argv+2 );
                return SS$_NORMAL;
            }
            if( cmd->helpstr == NULL ) {
                sts = printmsg( HELP_NOCMDHELP, MSG_TEXT, cmd->name );
            } else {
                const char *p;

                for( p = cmd->name; *p; p++ )
                    putchar( toupper( *p ) );
                fputs( "\n  ", stdout );
                sts = helptopic( HLP_WRAP, cmd->helpstr, 2 );
            }
            if( cmd->validquals != NULL )
                qualhelp( 0, cmd->validquals );
            if( cmd->params != NULL )
                parhelp( cmd, 0, NULL );

            return sts;
        }
    }
    for( cmd = helpitems; cmd->name; cmd++ ) {
        if( keycomp(argv[1],cmd->name) ) {
            if( cmd->proc )
                return (*cmd->proc)(argc-2, argv+2, 0, &cmd->helpstr );
            else if( cmd->helpstr )
                return helptopic( 0, cmd->helpstr);
            else
                break;
        }
    }
    return printmsg( ODS2_UNKNOWN, 0, "topic", argv[1] );
}

/*********************************************************** cmdhelp() */

static void cmdhelp( CMDSETp_t cmdset ) {
    CMDSETp_t cmd;
    int n = 0;
    size_t max = 0;

    for( cmd = cmdset; cmd->name != NULL; cmd++ ) {
        if( strlen(cmd->name) > max )
            max = strlen(cmd->name);
    }
    qsort( cmdset, (size_t)(cmd - cmdset), sizeof( CMDSET_t ), cmdcmp );

    for( cmd = cmdset; cmd->name; cmd++ ) {
        if( cmd->helpstr == NULL )
            continue;
        if( n++ % 4 == 0 )
            putchar( '\n' );
        printf( "    %-*s", (int)max,cmd->name );
    }
    putchar( '\n' );
}

/*********************************************************** parhelp() */

static void parhelp( CMDSETp_t cmd, int argc, char **argv ) {
    paramp_t p;
    paramp_t ptable;
    const char *footer = NULL;
    size_t max = 0;
    int col, one;

    ptable = cmd->params;

    if( ptable == NULL ) {
        printmsg( HELP_NOPARAMS, MSG_TEXT, cmd->name );
        return;
    }

    if( argc == 0 ) {
        int nolim = FALSE;

        col = 0;
        for( p = ptable; p->name != NULL; p++ ) {
            if( p->helpstr ) {
                size_t len;

                len = strlen(p->name);
                if( p->flags & NOLIM ) {
                    ++len;
                    nolim = TRUE;
                }
                if( len > max )
                    max = len;
            }
        }
        if( !max ) {
            printmsg( HELP_NOPARAMS, MSG_TEXT, cmd->name );
            return;
        }
        for( p = ptable; p->name != NULL; p++ ) {
            if( p->helpstr ) {
                size_t len;

                len = strlen(p->name);
                if( !col ) {
                    printmsg( HELP_PARHEADING, MSG_TEXT|MSG_NOCRLF );
                    col = 4;
                } else {
                    if( 1+(size_t)col + len > 80 ) {
                        printf( "\n    " );
                        col = 4;
                    } else {
                        printf ( " " );
                        col++;
                    }
                }
                printf( "%s%s%*s", p->name, (p->flags & NOLIM)? "*": "",
                        (int)max - (int)(strlen(p->name) + 
                                         (p->flags & NOLIM)? 1: 0),"" );
                col += (int)len;
            }
        }
        printf( "\n\n");
        if( nolim )
            printmsg( HELP_PARNOLIMIT, MSG_TEXT );
        printmsg( HELP_PAREXPLAIN, MSG_TEXT, cmd->name );
        return;
    }

    for( p = ptable; p->name != NULL; p++ ) {
        if( keycomp( argv[0], p->name ) )
            break;
    }
    if( p->name == NULL ) {
        if( keycomp( argv[0], "parameters" ) ) {
            for( p = ptable; p->name; p++ ) {
                size_t len;

                len = strlen( p->name );
                if( len > max )
                    max = len;
            }
            if( !max ) {
                printmsg( HELP_NOPARAMS, MSG_TEXT, cmd->name );
                return;
            }
            p = ptable;
            one = FALSE;
            printmsg( HELP_PARHEADING, MSG_TEXT );
        } else {
            printmsg( HELP_NOPARHELP, MSG_TEXT, argv[0] );
            return;
        }
    } else {
        max = strlen( p->name );
        one = TRUE;
        if( p->helpstr == NULL ) {
            printmsg( HELP_NOCMDHELP, 0, p->name );
            return;
        }
    }

    do {
        if( p->helpstr == NULL )
            continue;
        if( p->hlpfnc != NULL )
            footer = (*p->hlpfnc)(cmd, p, argc, argv);

        if( p->ptype == NONE ) {
            printmsg( HELP_PARNOTACT, MSG_TEXT );
            continue;
        }

        col = printf( "  %s:%*s ", p->name,
                      (int)(max - strlen( p->name )), "" );

        switch( p->ptype ) {
        case FSPEC:
            helptopic( HLP_WRAP, p->helpstr, col );
            break;
        case KEYWD:
            if( p->helpstr && *p->helpstr )
                helptopic( HLP_WRAP, p->helpstr, col );
            printmsg( (p->flags & NOLIM)? HELP_KEYWORDS: HELP_KEYWORD,
                      MSG_TEXT );
            qualhelp( 1, (qualp_t)p->arg );
            break;
        case QUALS:
            if( p->helpstr )
                helptopic( HLP_WRAP, p->helpstr, col );
            printmsg( (p->flags & NOLIM)? HELP_QUALIFIERS: HELP_QUALIFIER,
                      MSG_TEXT|MSG_NOCRLF );
            qualhelp( 0, (qualp_t )p->arg );
            break;
        case LIST:
            printmsg( (p->flags & NOLIM)? HELP_LIST: HELP_LISTS, MSG_TEXT,
                      p->helpstr );
            break;
        case STRING:
            helptopic( HLP_WRAP, p->helpstr, col );
            break;
        default:
            abort();
        }
        if( footer )
            printf( "%s\n", footer );
    } while( !one && (++p)->name );

    return;
}

/*********************************************************** qualhelp() */

static void qualhelp( int par, qualp_t qtable ) {
    qualp_t q;
    int n = 0 ,wrap;
    size_t max = 0, col = 4;

    if( par < 0 )
        max = -par;

#define NOSTR "[no]"
#define NOSTR_LEN (sizeof(NOSTR)-1)
    for( q = qtable; q->name; q++ ) {
        if( q->helpstr ) {
            size_t len = strlen(q->name);
            if( *q->helpstr == '-' )
                len += NOSTR_LEN;
            if( q->qtype != NOVAL )
                ++len;
            if( q->qtype < 0 )
                len += 2;
            if( abs(q->qtype) == DECVAL )
                ++len;
            if( len > max )
                max = len;
        }
    }

    if( !(q->set & OPT_NOSORT) ) { /* Table end marker */
        qsort( qtable, (size_t)(q - qtable), sizeof( qual_t ), qualcmp );
        q->set |= OPT_NOSORT;
    }

    for( q = qtable; q->name; q++ ) {
        int qtype, optn, negn;
        const char *nos, *vals;
        char bullet;
        static const char bullets[] = { '-', 'o', '+' };

        if( !q->helpstr )
            continue;

        bullet = bullets[ depth % sizeof( bullets ) ];

        qtype = abs(q->qtype);

        if( q->qtype < 0 ) {
            if( qtype == DECVAL )
                vals = "[=n]";
            else
                vals = "[=]";
        } else if( q->qtype == 0 ) {
            vals = "";
        } else {
            if( qtype == DECVAL )
                vals = "=n";
            else
                vals = "=";
        }
        optn = (int)strlen( vals );

        if( *q->helpstr == '-' ) {
            negn = 1;
            optn += NOSTR_LEN;
            nos = NOSTR;
        } else {
            negn = 0;
            nos = "";
        }

        if( !n++ && !par )
            printf( "\n  %s:\n", getmsg( HELP_QUALHEAD, MSG_TEXT|MSG_NOCRLF )  );
        wrap = printf( "    " );
        if( par < 0 ) {
            unsigned i;

            for( i = 0; i < depth; i++ )
                wrap += printf( "   " );
        } else if( !par )
            wrap += printf( "%s", qstyle_s );

        switch( qtype ) {
        case NOVAL:
            if( *q->helpstr ) {
                wrap += printf( "%s%-*s %c ",
                                nos, (int) (max-optn), q->name, bullet );
                helptopic( HLP_WRAP, q->helpstr+negn, wrap );
                break;
            }
            if( col + max > 50 ) {
                printf( "\n        " );
                col = 8;
            } else {
                while( col < 8 ) {
                    putchar( ' ' ); col++;
                }
            }
            printf( "%-*s", (int) max, q->name );
            col += max;
            break;
        case KEYVAL:
        case KEYCOL:
            wrap += printf( "%s%s%s%-*s %c ",
                            nos, q->name, vals, (int) (max-(optn+strlen(q->name))), "", bullet );
            helptopic( HLP_WRAP, q->helpstr+negn, wrap );
            depth++;
            qualhelp( qtype == KEYCOL? 1 : -(int)max, (qualp_t )q->arg );
            depth--;
            break;
        case DECVAL:
            wrap += printf( "%s%s%s%-*s %c ",
                            nos, q->name, vals, (int) (max-(optn+strlen(q->name))), "", bullet );
            helptopic( HLP_WRAP, q->helpstr+negn, wrap );
            break;
        case STRVAL:
        case PROT:
        case UIC:
            wrap += printf( "%s%s%s%-*s %c ",
                            nos, q->name, vals, (int) (max-(optn+strlen(q->name))), "", bullet );
            helptopic( HLP_WRAP, q->helpstr+negn, wrap );
            break;
        default:
            abort();
        }
    }
    if( par >= 0 )
        printf( "\n" );
}
#undef NOSTR
#undef NOSTR_LEN

/***************************************************************** cmdcmp() */

static int cmdcmp( const void *qa, const void *qb ){

    return strcmp( ((CMDSETp_t )qa)->name,
                   ((CMDSETp_t )qb)->name );
}

/***************************************************************** qualcmp() */

static int qualcmp( const void *qa, const void *qb ) {

    return strcmp( ((qualp_t )qa)->name,
                   ((qualp_t )qb)->name );
}

/*************************************************************** do_credits() */

static DECL_CMD(credits) {
    UNUSED( argc );
    UNUSED( argv );
    UNUSED( qualc );
    UNUSED( qualv );

    /* Do not move to help files (including en_us.hlp): the executable
     * must contain this text even if the hlp file contains a translation.
     */

    if( $SUCCESSFUL( helptopic( HLP_OPTIONAL, "CREDITS" ) ) )
        return SS$_NORMAL;

    printf( " The orginal version of ods2 was developed by Paul Nankervis\n" );
    printf( " <Paulnank@au1.ibm.com>\n" );
    printf( " This modified version was developed by Timothe Litt, and\n" );
    printf( " incorporates significant previous modifications by Larry\n" );
    printf( " Baker of the USGS and by Hunter Goatley\n" );
    printf("\n This code may be freely distributed within the VMS community\n" );
    printf( " providing that the contributions of the original author and\n" );
    printf( " subsequent contributors are acknowledged in the help text and\n" );
    printf( " source files.  This is free software; no warranty is offered,\n" );
    printf( " and while we believe it to be useful, you use it at your own risk.\n" );
    printf("\n Please send problem reports/comments to litt@acm.org\n\n");

    return SS$_NORMAL;
}

/*************************************************************** do_invocation() */

static DECL_CMD(invocation) {
#if defined(_WIN32)
    char *hd, *hp;

    hd = get_env("HOMEDRIVE");
    hp = get_env("HOMEPATH");
#elif !defined(VMS)
    char lcname[] = { MNAME(MODULE_NAME) }, *p;

    for( p = lcname; *p; p++ )
        *p = tolower( *p );
#endif

    UNUSED( argc );
    UNUSED( argv );
    UNUSED( qualc );
    UNUSED( qualv );

    (void) helptopic( 0, "INVOCATION",
                      MNAME(MODULE_NAME), MNAME(MODULE_NAME) );

#ifdef _WIN32
    (void) helptopic( 0, "INVOCATION NT",
                      (hd? hd: "%HOMEDRIVE%"),
                      (hp? hp: "%HOMEPATH%" ), MNAME(MODULE_NAME) );
    free( hd );
    free( hp );
#elif defined(VMS)
    (void) helptopic( 0, "INVOCATION VMS", MNAME(MODULE_NAME) );
#else
    (void) helptopic( 0, "INVOCATION UNIX", lcname );
#endif

#ifdef USE_LIBEDIT
    (void) helptopic( 0, "INVOCATION LIBEDIT", lcname );
#endif

    return SS$_NORMAL;
}

/*************************************************************** do_syntax() */

static DECL_CMD(syntax) {
    vmscond_t sts;
    extern int maxatdepth;
    char *key;
    size_t len, keylen;
    int i;

    UNUSED( qualc );
    UNUSED( qualv );

    if( argc == 0 ) {
        if( $FAILS(sts = helptopic( 0, "SYNTAX", MNAME(MODULE_NAME),
                                    qstyle_s, qstyle_s, qstyle_s )) )
            return sts;
        return topiclist( "SYNTAX" );
    }

    for( keylen = sizeof( "SYNTAX" ), i = 0; i < argc; i++ )
        keylen += 1+ strlen( argv[i] );
    if( (key = malloc( keylen )) == NULL )
        return printmsg( SS$_INSFMEM, 0 );
    /* N.B. No buffer overrun */
    memcpy( key, "SYNTAX", sizeof( "SYNTAX" ) -1 );
    for( keylen = sizeof( "SYNTAX" ) -1, i = 0; i < argc; i++ ) {
        key[keylen++] = ' ';
        len = strlen( argv[i] );
        memcpy( key + keylen, argv[i], len );
        keylen += len;
    }
    key[keylen] = '\0';
    sts = helptopic( HLP_ABBREV, key, maxatdepth );
    free( key );
    return sts;
}

/*************************************************************** topiclist() */

static vmscond_t topiclist( char *topic ) {
    vmscond_t sts;
    hlptopic_t *hlp, *sub;
    size_t max, len;
    int n = 0;

    if( $FAILS(sts = findtopic( HLP_ABBREV, topic, &hlp )) )
        return sts;

    if( hlp->subtopics.ntopics == 0 )
        return SS$_NORMAL;

    for( max = 0, sub = (hlptopic_t *)hlp->subtopics.topics.ptr;
         sub < (hlptopic_t *)hlp->subtopics.topics.ptr + hlp->subtopics.ntopics;
         sub++ )
        if( (len = strlen( (char *)sub->key.ptr )) > max )
            max = len;

    putchar( '\n' );
    printmsg( HELP_MORE, MSG_TEXT );

    for( sub = (hlptopic_t *)hlp->subtopics.topics.ptr;
         sub < (hlptopic_t *)hlp->subtopics.topics.ptr + hlp->subtopics.ntopics;
         sub++ ) {
        if( n++ % 4 == 0 )
            putchar( '\n' );
        printf( "    %-*s", (int)max, (char *)sub->key.ptr );
    }
    putchar( '\n' );

    return SS$_NORMAL;
}

/*************************************************************** helptopic() */

vmscond_t helptopic( options_t options, char *topic, ... ) {
    vmscond_t sts;
    va_list ap;
    hlptopic_t *hlp = NULL;
    int wrapcol;
    char c;
    size_t len;
    char *s, *buf, *e;

    if( $FAILS(sts = findtopic( options, topic, &hlp )) )
        return sts;

    if( !(options & HLP_WRAP) ) {
        va_start(ap, topic );
        vfprintf( stdout, (char *)hlp->text.ptr, ap );
        va_end(ap);
        return SS$_NORMAL;
    }

    va_start(ap, topic);
    wrapcol = va_arg(ap, int);
    len = (size_t)vsnprintf( &c, 1, (char *)hlp->text.ptr, ap ) + 1;
    va_end(ap);

    if( (buf = malloc( len +1 )) == NULL )
        return printmsg( SS$_INSFMEM, 0 );

    va_start(ap, topic);
    wrapcol = va_arg(ap, int);
    (void) vsnprintf( buf, len, (char *)hlp->text.ptr, ap );

    s = buf;
    do {
        if( (e = strchr(s, '\n')) == NULL ) {
            fputs( s, stdout );
            break;
        }
        *e++ = '\0';
        fputs( s, stdout );
        fputc( '\n', stdout );
        if( !*e )
            break;
        fprintf( stdout, "%*s", wrapcol, "" );
        s = e;
    } while( 1 );

    free( buf );
    va_end(ap);

    return SS$_NORMAL;
}

/*************************************************************** findtopic() */

static vmscond_t findtopic( options_t options,
                            char *topic, hlptopic_t **rethlp ) {
    vmscond_t sts;
    hlptopic_t key, *hlp = NULL;
    hlproot_t *root;
    char *s;

    *rethlp = NULL;
    memset( &key, 0, sizeof( key ) );

    if( (helpdata == NULL || (options & HLP_RELOAD))
        && $FAILS(sts = rdhelp( helpfile )) ) {
        if( !(options & HLP_OPTIONAL) )
            sts = printmsg( sts, 0, helpfile );
        return sts;
    }
    root = hlproot;

    for( s = topic;
         *s && root->ntopics;
         s += key.keylen, root = &hlp->subtopics ) {
        s = s + strspn( s, " " );
        key.key.ptr = s;
        key.keylen = (uint32_t)strcspn( s, " " );

        if( options & HLP_ABBREV ) {
            if( (hlp = bsearch( &key, root->topics.ptr, root->ntopics,
                                sizeof( hlptopic_t ), &hlpcmpa )) == NULL ) {
                sts = HELP_NOHELP;
                if( !(options & HLP_OPTIONAL) )
                    sts = printmsg( sts, 0, topic, helpfile );
                return sts;
            }
            if( (hlp > (hlptopic_t *)root->topics.ptr && !hlpcmpa( &key, hlp -1)) ||
                ( (hlp + 1 < (hlptopic_t *)root->topics.ptr + root->ntopics) &&
                  !hlpcmpa( &key, hlp + 1 ) ) )
                    return printmsg( ODS2_AMBIGUOUS, 0, "topic", topic );
        } else if( (hlp = bsearch( &key, root->topics.ptr, root->ntopics,
                                   sizeof( hlptopic_t ), &hlpcmp )) == NULL ) {
            sts = HELP_NOHELP;
            if( !(options & HLP_OPTIONAL) )
                sts = printmsg( sts, 0, topic, helpfile );
            return sts;
        }
    }
    *rethlp = hlp;

    return SS$_NORMAL;
}

/*************************************************************** rdhelp() */
static
vmscond_t rdhelp( const char *filename ) {
    vmscond_t sts;
    FILE *fp;
    hlphdr_t header;
    size_t bufsize, nr;

    /* Load the compiled help file (.hlb), which is sorted and indexed.
     * The entire structure ends up in a single malloc'd block.
     * Besides validating the file format, the main job is to relocate
     * pointers (stored as offsets) into memory addresses.  This is
     * faster and safer than parsing the text (.hlp) file which requires
     * many mallocs/reallocs.  The compiler is "makehelp".
     */
    sts = SS$_NORMAL;

    if( helpdata != NULL ) {
        free( helpdata );
        helpdata = NULL;
    }

    if( helpfile == NULL ) return printmsg( HELP_OPENHLP, 0, "<none>" );
    if( (fp = openf( filename, "rb" )) == NULL ) {
        int err = errno;
        printmsg( HELP_OPENHLP, 0, filename );
        return printmsg( ODS2_OSERROR, MSG_CONTINUE, HELP_OPENHLP, strerror( err ) );
    }

    if( (nr = fread( &header, sizeof( header ), 1, fp )) != 1 ) {
        fclose(fp);
        return printmsg( HELP_HELPFMT, 0, filename );
    }
    if( strncmp( header.magic, HLP_MAGIC, sizeof( HLP_MAGIC ) ) ) {
        fclose(fp);
        return printmsg( HELP_HELPFMT, 0, filename );
    }
    if( header.version != HLP_VERSION ||
        header.psize != sizeof( void * ) ||
        header.ssize != sizeof( size_t ) ||
        header.tsize != sizeof( time_t ) ||
        header.size < sizeof( hlproot_t ) + sizeof( hlptopic_t ) ) {
        fclose(fp);
        return printmsg( HELP_HELPFMT, 0, filename );
    }
    bufsize = sizeof( hlphdr_t ) + header.size;
    if( (helpdata = malloc( bufsize )) == NULL ) {
        fclose(fp);
        return printmsg( HELP_HELPFMT, 0, filename );
    }
    memcpy( helpdata, &header, sizeof( header ) );

    if( (nr = fread( helpdata + sizeof( header ), header.size, 1, fp )) != 1) {
        sts = printmsg( HELP_HELPFMT, 0, filename );
    } else {
        if( !reloc( hlproot, (ptrdiff_t)bufsize ) )
            sts = printmsg( HELP_HELPFMT, 0, filename );
    }
    fclose(fp);
    if( $FAILS(sts) ) {
        free( helpdata );
        helpdata = NULL;
    }
    return sts;
}

/**************************************************************i*reloc() */
static int reloc( hlproot_t *root, ptrdiff_t bufsize ) {
    size_t i;
    hlptopic_t *hlp;

    if( root->topics.ofs >= (ptrdiff_t)bufsize ||
        root->topics.ofs < (ptrdiff_t)sizeof( hlphdr_t ) )
        return 0;

    root->topics.ptr = (hlptopic_t *)(root->topics.ofs + helpdata);
    for( i = 0, hlp = root->topics.ptr; i < root->ntopics; i++, hlp++ ) {
        ptrdiff_t ofs;
        ofs = hlp->key.ofs;
        if( ofs >= (ptrdiff_t)bufsize ||
            ofs < (ptrdiff_t)sizeof( hlphdr_t ) )
            return 0;
        hlp->key.ptr = helpdata + ofs;
        ofs = hlp->text.ofs;
        if( ofs >= (ptrdiff_t)bufsize ||
            ofs < (ptrdiff_t)sizeof( hlphdr_t ) )
            return 0;
        hlp->text.ptr = helpdata + ofs;
        if( hlp->subtopics.ntopics &&
            !reloc( &hlp->subtopics, bufsize ) )
            return 0;
    }
    return 1;
}

/***************************************************************** hlpcmp() */

static int hlpcmp( const void *ta, const void *tb ) {
    const char *a, *b;
    size_t alen, blen;

    a = ((hlptopic_t *)ta)->key.ptr;
    b = ((hlptopic_t *)tb)->key.ptr;
    alen = ((hlptopic_t *)ta)->keylen;
    blen = ((hlptopic_t *)tb)->keylen;

    while( alen && blen ) {
        int c;

        c = tolower( (unsigned char)*a ) - tolower( (unsigned char)*b );
        if( c != 0 )
            return c;
        ++a; ++b; --alen; --blen;
    }

    return (int)(alen - blen);
}

/***************************************************************** hlpcmpa() */

static int hlpcmpa( const void *key, const void *tgt ) {
    const char *k, *t;
    size_t klen, tlen;

    k = ((hlptopic_t *)key)->key.ptr;
    t = ((hlptopic_t *)tgt)->key.ptr;
    klen = ((hlptopic_t *)key)->keylen;
    tlen = ((hlptopic_t *)tgt)->keylen;

    while( klen && tlen ) {
        int c;

        c = tolower( (unsigned char)*k ) - tolower( (unsigned char)*t );
        if( c != 0 )
            return c;
        ++k; ++t; --klen; --tlen;
    }
    if( klen == 0 )
        return 0;

    return (int)(klen - tlen);
}

/*************************************************************** helpexit() */

static void helpexit( void ) {
    if( helpdata != NULL ) {
        free(helpdata);
        helpdata = NULL;
    }
    if( helpfile != NULL ) {
        free( helpfile );
        helpfile = NULL;
    }
}

/*************************************************************** dohlpdata() */
#if DEBUG
static DECL_CMD(hlpdata) {
    vmscond_t sts;

    UNUSED( argc );
    UNUSED( argv );
    UNUSED( qualc );
    UNUSED( qualv );

    if( helpdata == NULL && $FAILS(sts = rdhelp( helpfile )) )
        return printmsg( sts, 0, helpfile );

    return printdata( 1, hlproot );
}

/*************************************************************** printdata() */

static vmscond_t printdata( unsigned level, hlproot_t *root ) {
    vmscond_t sts;
    hlptopic_t *hlp;

    if( root->topics.ptr == NULL )
        return SS$_NORMAL;

    for( hlp = root->topics.ptr; hlp < (hlptopic_t *)root->topics.ptr + root->ntopics; hlp++ ) {
        printf( "%*s %u %s\n", level, "", level, (char *)hlp->key.ptr );
        if( hlp->text.ptr == NULL )
            return HELP_HELPFMT;
        if( hlp->subtopics.topics.ptr &&
            $FAILS(sts = printdata( level +1, &hlp->subtopics )) )
            return sts;
    }
    return SS$_NORMAL;
}
#endif
