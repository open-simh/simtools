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

#if !defined( DEBUG ) && defined( DEBUG_TERMIO )
#define DEBUG DEBUG_TERMIO
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#include "cmddef.h"

extern int interactive;

#ifdef USE_LIBEDIT
#include <histedit.h>

#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#elif defined HAVE_TERM_H
#include <term.h>
#elif defined HAVE_CURSES_H
#include <curses.h>
#else
int   tgetent (char *buffer, char *termtype);
char *tgetstr (char *name, char **area);
int   tgetnum (char *name);
int   tgetflag (char *name);
#endif

extern EditLine *editline;

/* Max size of a terminal description - documented as ">1024, probably 2048"
 */
#define TERMENTMAX (4096)

#define TERM_CLR_EOL caps[0].val
#define TERM_CUR_UP  caps[1].val
#define TERM_CR      caps[2].val
static struct cap {
    char *id;
    char *val;
    char *def; } caps[] = {
    {"ce", NULL,"\r                                        "\
                "                                        \r" },
    {"up", NULL, ""},
    {"cr", NULL, "\r"},
    {NULL, NULL, NULL} };
static int have_caps = 0;

static void get_termattrs(void) {
    int ok;
    struct cap *cp;
    char *tentbuf, *term;

    if( have_caps ) {
        return;
    }
    have_caps = 1;

    tentbuf = (char *)calloc( 1, TERMENTMAX ); /* May be ignored, or have desc. */
    if( !tentbuf ) {
        perror( "calloc" );
        exit( EXIT_FAILURE );
    }

    term = getenv( "TERM" );

    if( !(term && *term) ) {
        term = "vt100";
    }

    ok = tgetent(tentbuf, term );
    if( ok <= 0 ) {
        if( ok < 0 )
            fprintf( stderr, "Can't access termcap database\n" );
        else
            fprintf( stderr, "Terminal '%s' is not defined in termcap\n", term );
        for( cp = caps; cp->id; cp++ )
            cp->val = cp->def;
    } else {
        char *parbuf;

        parbuf = calloc( 1, TERMENTMAX );
        if( !parbuf ) {
            perror("calloc");
            exit( EXIT_FAILURE );
        }
        for( cp = caps; cp->id; cp++ ) {
            char *area = parbuf;

            cp->val = tgetstr( cp->id, &area );
            if( cp->val ) {
                size_t vlen;

                vlen = strlen( cp->val ) + 1;
                area = malloc( vlen );
                if( !area ) {
                    perror( "malloc");
                    exit( EXIT_FAILURE );
                }
                memcpy( area, cp->val, vlen );
                cp->val = area;
            } else {
                fprintf( stderr, "No %s for %s terminal\n", cp->id, term );
                cp->val = cp->def;
            }
        }
        free( parbuf );
    }
    free( tentbuf );
}

#endif

/******************************************************************* confirm_cmd() */
vmscond_t confirm_cmd( vmscond_t status, ... ) {
    va_list ap;
    char *response, *mp, *buf = NULL;
    size_t bufsize = 16;
    vmscond_t rcode;

    do {
        va_start( ap, status );
        (void) vprintmsg( status,
                          MSG_TEXT | MSG_WITHARGS | MSG_NOCRLF,
                          ap );
        va_end( ap );
        fflush( stdout );
        mp =
            response = fgetline( stdin, FALSE, &buf, &bufsize );
        while( mp != NULL && (*mp == ' ' || *mp == '\t') )
            ++mp;

        if( mp != NULL && ( *mp == '\0' ||
                            keycomp( mp, "NO" ) ||
                            keycomp( mp, "FALSE" ) ||
                            keycomp( mp, "0" ) ) ) {
            rcode = ODS2_CONFIRM_NO;
            break;
        }

        if( mp == NULL || keycomp( mp, "ALL" ) ) {
            rcode = ODS2_CONFIRM_ALL;
            break;
        }

        if( keycomp( mp, "YES" ) ||
            keycomp( mp, "TRUE" ) ||
            keycomp( mp, "1" ) ) {
            rcode = ODS2_CONFIRM_YES;
            break;
        }

        if( keycomp( mp, "QUIT" ) ) {
            rcode = ODS2_CONFIRM_QUIT;
            break;
        }

        putchar( '\b' );
    } while( 1 );

    if( buf != NULL ) free( buf );
    return rcode | STS$M_INHIB_MSG;
}

/******************************************************************* put_str() */

vmscond_t put_str( const char *buf, FILE *outf, pause_t *pause ) {
    return put_strn( buf, strlen( buf ), outf, pause );
}

/******************************************************************* put_str() */

vmscond_t put_strn( const char *buf, size_t len,
                    FILE *outf, pause_t *pause ) {
    vmscond_t sts;

    if( len == 0 )
        return SS$_NORMAL;

    if( pause == NULL || pause->interval == 0 ) {
        fwrite( buf, len, 1, outf );
        return SS$_NORMAL;
    }
    while( len-- )
        if( $FAILS(sts = put_c( *buf++, outf, pause )) )
            return sts;

    return SS$_NORMAL;
}

/******************************************************************* put_c() */
vmscond_t put_c( int c, FILE *outf, pause_t *pause ) {
    vmscond_t sts;

    fputc( c, outf );

    if( c != '\n' || pause == NULL || pause->interval == 0 ) {
        return SS$_NORMAL;
    }

    sts = SS$_NORMAL;

    if( ++pause->lineno < pause->interval )
        return SS$_NORMAL;

    printmsg( pause->prompt, MSG_TEXT | MSG_NOCRLF );
    fflush( stdout );
#ifdef USE_LIBEDIT
    if( interactive ) {
        get_termattrs();
        while( 1 ) {
            char ch;
            c = el_getc( editline, &ch );
            if( c > 0 ) {
                putchar( ch );
                fflush( stdout );
                if( ch == '\r' )
                    continue;
            }
            if( c == 0 ) {
                ch = '\n';
                sts = SS$_ABORT;
            } else if( tolower( ch ) == 'q' ) {
                sts = SS$_ABORT;
                continue;
            } else if( tolower( ch ) == 'f' ) {
                pause->interval = 0;
                continue;
            }
            if( ch == '\n' ) {
                pause->lineno = 0;
                printf( "%s%s%s", TERM_CR, TERM_CUR_UP, TERM_CLR_EOL );
                fflush( stdout );
                return sts | STS$M_INHIB_MSG;
            }
            el_beep(editline);
        }
    } else
#endif
    {
        while( 1 ) {
            c = getchar();
            if( c == '\r' ) {
                continue;
            }
            if( c == EOF ) {
                c = '\n';
                sts = SS$_ABORT;
            } else if( tolower( c ) == 'q' ) {
                sts = SS$_ABORT;
                continue;
            } else if( tolower( c ) == 'f' ) {
                pause->interval = 0;
                continue;
            }
            if( c == '\n' ) {
#ifdef _WIN32
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                size_t wid;

                fflush( stdout );

                GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ),
                    &csbi );
                wid = (size_t)csbi.srWindow.Right - csbi.srWindow.Left + 1;

                --csbi.dwCursorPosition.Y;
                csbi.dwCursorPosition.X = 0;
                SetConsoleCursorPosition( GetStdHandle( STD_OUTPUT_HANDLE ),
                    csbi.dwCursorPosition );
                while( wid-- )
                    putchar( ' ' );
                SetConsoleCursorPosition( GetStdHandle( STD_OUTPUT_HANDLE ),
                    csbi.dwCursorPosition );
#else
                putchar( '\r' );
#endif
                pause->lineno = 0;
                fflush( stdout );
                return sts | STS$M_INHIB_MSG;
            }
            putchar( '\b' );
        }
    }
}

/******************************************************************* termchar() */
void termchar( int *width, int *height ) {

    if( width != NULL )
        *width = 80;
    if( height != NULL )
        *height = 24;

    {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;

        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

        if( width != NULL )
            *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        if( height != NULL )
            *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
#ifdef TIOCGSIZE
        struct ttysize ts;

        ioctl(STDIN_FILENO, TIOCGSIZE, &ts);

        if( width != NULL )
            *width = ts.ts_cols;
        if( height != NULL )
            *height = ts.ts_lines;
#elif defined(TIOCGWINSZ)
        struct winsize ts;

        ioctl(STDIN_FILENO, TIOCGWINSZ, &ts);
        if( width != NULL )
            *width = ts.ws_col;
        if( height != NULL )
            *height = ts.ws_row;
#endif /* TIOCGSIZE */
#endif
    }

    return;
}
