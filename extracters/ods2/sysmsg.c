/* Timothe Litt March 2016
 * Copyright (C) 2016 Timothe litt
 *   litt at acm dot org
 *
 * Free for use with the ODS2 package.  All other rights reserved.
 */

/*
 *       This is distributed as part of ODS2, originally  written by
 *       Paul Nankervis, email address:  Paulnank@au1.ibm.com
 *
 *       ODS2 is distributed freely for all members of the
 *       VMS community to use. However all derived works
 *       must maintain comments in their source to acknowledge
 *       the contributions of the original author and
 *       subsequent contributors.   This is free software; no
 *       warranty is offered,  and while we believe it to be useful,
 *       you use it at your own risk.
 */

/* VMS-ish message code translations */

#if !defined( DEBUG ) && defined( DEBUG_SYSMSG )
#define DEBUG DEBUG_SYSMSG
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* This is similar to, but not quite $PUTMSG.  We don't have VMS signals...
 */

#define SYSMSG_INTERNAL
#include "ods2.h"
#include "ssdef.h"
#include "rms.h"
#include "compat.h"
#include "stsdef.h"

#include "sysmsg.h"

#ifdef _WIN32
#undef sscanf
#define sscanf sscanf_s
#undef fscanf
#define fscanf fscanf_s
#endif

/* Facility name to text translations
 */

static
struct VMSFAC fac2textdef[] = {
#define GENERATE_FACTAB
#include "default.md"
}, /* Entry for unknown facility */
    nofac = { "NONAME", 0 };

/* Number of entries to search */
static size_t faclen =  (sizeof(fac2textdef)/sizeof(fac2textdef[0]));

/* Pointer to active table.
 * Special labels for facility code 0.
 */

static
struct VMSFAC *fac2text = fac2textdef,
    sysfac =  { "SYSTEM", 0 },
    shrfac =  { "SHR"   , 0 };

/* Pointer to memory containing dynamically-loaded message file data.
 */
static char *msgfile;
static char *msgfilename;

static const char sev2text[(STS$M_SEVERITY >> STS$V_SEVERITY)+1] =
    { 'W', 'S', 'E', 'I', 'F', '?', '?', '?', };
/*     0    1    2    3    4    5    6    7  */

/* Unknown facility or message code
 */
static char notext[256];

/* Message data
 */

static
struct VMSMSG vms2textdef[] = {
#define GENERATE_MSGTAB
#include "default.md"
},  /* Special item for Unknown message response */
    nomsg = { "NOMSG", notext, ODS2_NOMSG, 1 };
/* Number of messages */
static size_t msglen = sizeof(vms2textdef)/sizeof(vms2textdef[0]);

/* Pointer to active message table */
struct VMSMSG *vms2text = vms2textdef;

/* Default message display options
 */
static options_t message_format = MSG_FULL;

/* Expandable buffer for formatted messages */

static char *buf = NULL;
static size_t bufsize = 0;

/* Last few formatted messages for callers that hold
 * pointers across calls.
 */
static char *retbuf[10];
static size_t retidx = 0, lastidx = 0;

static int argcount( vmscond_t vmscond );

static int xpand( size_t used, size_t add );
static const char *returnbuf( void );
static int decstr( FILE *mf, size_t *len, char **pp, char **sp );

/******************************************************************* printmsg() */

vmscond_t printmsg( vmscond_t vmscond, unsigned int flags, ... ) {
    va_list ap;
    vmscond_t status;

    va_start( ap, flags );
    status = vprintmsg( vmscond, flags, ap );
    va_end( ap );

    return status;
}
/******************************************************************* vprintmsg() */

vmscond_t vprintmsg( vmscond_t vmscond, unsigned int flags, va_list ap ) {
    const char *msg, *pfxe;
    FILE *of = stdout;
    int nargs;
    vmscond_t primary;

    if( flags & MSG_TOFILE )
        of = va_arg( ap, FILE * );

    if( flags & MSG_CONTINUE ) {
        primary = va_arg( ap, vmscond_t );
        vmscond |= primary & STS$M_INHIB_MSG;
    } else
        primary = vmscond;

    if( (vmscond & STS$M_INHIB_MSG) && !(flags & MSG_ALWAYS) )
        return primary;

    if( (flags & (MSG_WITHARGS | MSG_NOARGS)) == 0 ) {
        nargs = argcount( vmscond );
        if( nargs > 0 )
            flags |= MSG_WITHARGS;
    }

    if( !(flags & MSG_FULL) ) {
        flags = (flags & ~MSG_FULL) | message_format;
    }

    if( (flags & (MSG_WITHARGS | MSG_TEXT)) != (MSG_WITHARGS | MSG_TEXT) ) {
        fprintf( of, "%s", getmsg_string( vmscond, flags ) );
        if( !(flags & MSG_NOCRLF) )
            fputc( '\n', of );
        return primary | STS$M_INHIB_MSG;
    }

    msg = getmsg_string( vmscond, flags );

    if( flags & (MSG_FACILITY | MSG_SEVERITY | MSG_NAME) ) {
        pfxe = strchr( msg, ',' );
        if( pfxe == NULL ) {
            abort();
        }
        pfxe += 2;
        fprintf( of, "%.*s", (int)(pfxe - msg), msg );
    } else {
        pfxe = msg;
    }

    vfprintf( of, pfxe, ap );

    if( !(flags & MSG_NOCRLF) )
        fputc( '\n', of );

    return primary | STS$M_INHIB_MSG;
}

/******************************************************************* getmsg() */

const char *getmsg( vmscond_t vmscond, unsigned int flags, ... ) {
    char *buf, *msg;
    va_list ap;
    size_t prelen, len, fmtlen;

    if( !(flags & MSG_WITHARGS) ) {
        return getmsg_string( vmscond, flags );
    }

    buf = retbuf[ lastidx ];

    prelen = 0;
    errno = 0;
    (void) getmsg_string( vmscond, flags );
    if( errno )
        return strerror( errno );

    fmtlen = strlen( buf );

    if( (msg = strchr( buf, ',')) != NULL ) {
        prelen = 2 + (size_t)( msg -buf );
    }

    va_start( ap, flags );
    len = vsnprintf( buf+fmtlen+1, 1, buf, ap );
    va_end( ap );

    if( (buf = realloc( buf, fmtlen + 1 + len + 1 )) == NULL )
        return strerror( errno );
    retbuf[lastidx] = buf;

    va_start( ap, flags );
    (void) vsnprintf( buf+fmtlen + 1, len+1, buf + prelen, ap );
    va_end( ap );

    memmove( buf, buf+fmtlen+1, len + 1 );

    return buf;
}

/******************************************************************* argcount() */
static int argcount( vmscond_t vmscond ) {
    struct VMSMSG *mp, key;

    memset( &key, 0, sizeof( key ) );
    key.code = vmscond;
    mp = bsearch( &key, vms2text, msglen,
                  sizeof(vms2text[0]), msgcmp );
    if( mp == NULL )
        return -1;

    return mp->nargs;
}

/******************************************************************* getmsg_string() */

const char *getmsg_string( vmscond_t vmscond, unsigned int flags ) {
    const struct VMSFAC *fp = NULL;
    const struct VMSMSG *mp = NULL;
    struct VMSFAC fackey;
    struct VMSMSG msgkey;

    memset( &fackey, 0, sizeof( fackey ) );
    memset( &msgkey, 0, sizeof( msgkey ) );

    if( !(flags & MSG_FULL) )
        flags = (flags & ~MSG_FULL) | message_format;

    while( 1 ) {
        size_t i = 0, len;

        do {
            if( flags & (MSG_FACILITY|MSG_SEVERITY|MSG_NAME) ) {
                if( !xpand( i, 1 ) )
                    return strerror( errno );
                buf[i++] = (flags & MSG_CONTINUE)? '-': '%';

                if( fp == NULL ) {
                    uint32_t facno;

                    facno = $VMS_STATUS_FAC_NO(vmscond);

                    if( facno == 0  ) {
                        fp = &sysfac;
                        if( !$VMS_STATUS_FAC_SP(vmscond) ) {
                            uint32_t msgno;

                            msgno = $VMS_STATUS_CODE(vmscond);
                            if( msgno >= 4096 && msgno < 8192 )
                                fp = &shrfac;
                        }
                    } else {
                        fackey.code = facno;
                        fp = bsearch( &fackey, fac2text, faclen,
                                      sizeof(fac2text[0]), faccmp );
                        if( fp == NULL ) fp = &nofac;
                    }
                }

                if( fp->text == NULL )
                    break;
                if( flags & MSG_FACILITY ) {
                    len = strlen( fp->text );
                    if( !xpand( i, len +1 ) )
                        return strerror( errno );

                    memcpy( buf+i, fp->text, len );
                    i += len;
                    if( flags & (MSG_SEVERITY|MSG_NAME) )
                        buf[i++] = '-';
                }
                if( flags & MSG_SEVERITY ) {
                    if( !xpand( i, 2 ) )
                        return strerror( errno );
                    buf[i++] = sev2text[$VMS_STATUS_SEVERITY(vmscond)];
                    if( flags & MSG_NAME )
                        buf[i++] = '-';
                }
                if( flags & MSG_NAME ) {
                    if( mp == NULL ) {
                        msgkey.code = vmscond;
                        mp = bsearch( &msgkey, vms2text, msglen,
                                      sizeof(vms2text[0]), msgcmp );
                    }
                    if( mp == NULL )
                        break;
                    len = strlen(  mp->ident );
                    if( !xpand( i, len ) )
                        return strerror( errno );
                    memcpy( buf+i, mp->ident, len );
                    i += len;
                }
                if( flags & MSG_TEXT ) {
                    if( !xpand( i, 2 ) )
                        return strerror( errno );
                    buf[i++] = ',';
                    buf[i++] = ' ';
                }
            }
            if( flags & MSG_TEXT ) {
                if( mp == NULL ) {
                    msgkey.code = vmscond;
                    mp = bsearch( &msgkey, vms2text, msglen,
                                  sizeof(vms2text[0]), msgcmp );
                    if( mp == NULL )
                        break;
                }
                len = strlen( mp->text );
                if( !xpand( i, len ) )
                    return strerror( errno );

                memcpy( buf+i, mp->text, len );
                if( !(flags & (MSG_FACILITY|MSG_SEVERITY|MSG_NAME|MSG_NOUC)) )
                    buf[i] = toupper( buf[i] );

                i += len;
            }
            buf[i] = '\0';
            return returnbuf();
        } while( 0 );

        if( fp == NULL || fp->text == NULL )
            fp = &nofac;

        if( mp == NULL || mp->text == NULL ) {
            msgkey.code = vmscond;
            mp = bsearch( &msgkey, vms2text, msglen,
                          sizeof(vms2text[0]), msgcmp );
            if( mp == NULL || mp->text == NULL ) {
                msgkey.code = ODS2_NOMSG;
                mp = bsearch( &msgkey, vms2text, msglen,
                              sizeof(vms2text[0]), msgcmp );
                if( mp == NULL || mp->text == NULL ) {
                    (void) snprintf( notext, sizeof(notext),
                                     "message number %08X", vmscond );
                } else {
                    (void) snprintf( notext, sizeof(notext),
                                     mp->text, vmscond );
                    notext[sizeof(notext) -1] = '\0';
                }
                mp = &nomsg;
            }
        }
    }
}

/******************************************************************* xpand() */

static int xpand( size_t used, size_t add ) {
    char *nbuf;
    size_t need;

    need = used + add + 1;                           /* Always allow for \0 */
    if( need < bufsize ) {
        return 1;
    }
    nbuf = realloc( buf, need + 32 + 1);             /* 32 minimizes reallocs */
    if( nbuf == NULL ) {
        return 0;
    }
    buf = nbuf;
    bufsize = need + 32 + 1;
    return 1;
}

/******************************************************************* returnbuf() */

static const char *returnbuf( void ) {
    const char *rbuf;

    lastidx = retidx;
    free( retbuf[retidx] );
    retbuf[retidx++] = buf;
    retidx %= sizeof( retbuf ) / sizeof( retbuf[0] );
    rbuf = buf;
    buf = NULL;
    bufsize = 0;
    return rbuf;
}

/***************************************************************** set_message_format */
vmscond_t set_message_format( options_t new ) {

    message_format = new & (MSG_FACILITY|MSG_SEVERITY|MSG_NAME|MSG_TEXT);
    return SS$_NORMAL;
}

/***************************************************************** set_message_format */
options_t get_message_format( void ) {

    return message_format;
}

/******************************************************************* set_message_file */

vmscond_t set_message_file( const char *filename ) {
    vmscond_t sts;
    char *newmsg = NULL;
    char *newfilename = NULL;
    FILE *mf = NULL;

    if( filename == NULL ) { /* Reset to built-in default tables */
        fac2text = fac2textdef;
        vms2text = vms2textdef;
        faclen = (sizeof(fac2textdef)/sizeof(fac2textdef[0]));
        msglen = sizeof(vms2textdef)/sizeof(vms2textdef[0]);

        if( msgfile != NULL ) {
            free( msgfile );
            msgfile = NULL;
        }
        if( msgfilename ) {
            free( msgfilename );
            msgfilename = NULL;
        }

        return SS$_NORMAL;
    }

    /* Load a file (built by genmsg.c) */

    sts = ODS2_OPENIN;
    errno = 0;
    do {
        size_t size, i;
        uint32_t facn, msgn;
        uint32_t code, nargs;
        int c, n;
        char *np, *sp;
        off_t pos;

        if( (newfilename = get_realpath( filename )) == NULL ) {
            size = strlen( filename ) +1;
            if( (newfilename = malloc( size)) == NULL ) {
                sts = SS$_INSFMEM;
                break;
            }
            memcpy( newfilename, filename, size );
        }

        if( (mf = openf( newfilename, "rb" )) == NULL ) {
            break;
        }

        sts = ODS2_BADMSGFILE;

        /* File header record */

        n = 0;
        if( (c = fscanf( mf, MSG_HEADER_DEC, &facn, &msgn, &n )) == EOF ||
            c < 2 ||
            n != MSG_HEADER_DECN )
            break;

        size = ( (facn * sizeof( struct VMSFAC ) ) +
                 (msgn * sizeof( struct VMSMSG ) ) );

        if( (c = getc( mf )) != '\0' )
            break;

        pos = ftell( mf );

        /* Compute string pool size */

        /* Facility records */

        for( i = 0; i < facn; i++ ) {
            n = 0;
            if( (c = fscanf( mf, MSG_FAC_DEC, &code, &n )) == EOF ||
                c < 1 ||
                n != MSG_FAC_DECN )
            break;

            if( (c = decstr( mf, &size, NULL, NULL )) == EOF )
                break;
        }
        if( c == EOF || i < facn )
            break;

        /* Message description records */

        for( i = 0; i < msgn; i++ ) {
            n = 0;
            if( (c = fscanf( mf, MSG_MSG_DEC, &code, &nargs, &n )) == EOF ||
                c < 2 ||
                n != MSG_MSG_DECN )
            break;

            if( (c = decstr( mf, &size, NULL, NULL )) == EOF )
                break;
            if( (c = decstr( mf, &size, NULL, NULL )) == EOF )
                break;
        }
        if( c == EOF || i < msgn )
            break;

        /* Allocate enough memory for the tables and the string pool */

        if( fseek( mf, pos, SEEK_SET ) == -1 )
            break;

        if( (newmsg = malloc( size )) == NULL ) {
            sts = SS$_INSFMEM;
            break;
        }

        np = newmsg;
        sp = np + ((facn * sizeof( struct VMSFAC )) +
                   (msgn * sizeof( struct VMSMSG )));

        /* Create each structure along with any strings. */

        /* Facility records */

        for( i = 0; i < facn; i++ ) {
            struct VMSFAC *fp = (struct VMSFAC *)np;

            np += sizeof( struct VMSFAC );

            n = 0;
            if( (c = fscanf( mf, MSG_FAC_DEC, &code, &n )) == EOF ||
                c < 1 ||
                n != MSG_FAC_DECN )
            break;

            fp->code = code;
            if( (c = decstr( mf, &size, &sp, &fp->text )) == EOF )
                break;
        }
        if( c == EOF || i < facn )
            break;

        /* Message description records */

        for( i = 0; i < msgn; i++ ) {
            struct VMSMSG *mp = (struct VMSMSG *)np;

            np += sizeof( struct VMSMSG );

            n = 0;
            if( (c = fscanf( mf, MSG_MSG_DEC, &code, &nargs, &n )) == EOF ||
                c < 2 ||
                n != MSG_MSG_DECN )
            break;

            mp->code = code;
            mp->nargs = nargs;

            if( (c = decstr( mf, &size, &sp, &mp->ident )) == EOF )
                break;
            if( (c = decstr( mf, &size, &sp, &mp->text )) == EOF )
                break;
        }
        if( c == EOF || i < msgn )
            break;

        /* EOF */
        n = 0;
        if( (c = fscanf( mf, MSG_EOF_DEC, &n )) == EOF ||
            c < 0 ||
            n != MSG_EOF_DECN || (c = getc( mf )) != 0 )
            break;

        /* Success.  Install new file. */

        free( msgfile );
        msgfile = newmsg;

        free( msgfilename );
        msgfilename = newfilename;
        newfilename = NULL;

        fac2text = (struct VMSFAC *)newmsg;
        vms2text = (struct VMSMSG *)(newmsg +
                                     (facn * sizeof( struct VMSFAC )));
        msglen = msgn;
        faclen = (size_t)facn;

        newmsg = NULL;

        sts = SS$_NORMAL;
    } while( 0 );

    if( mf != NULL ) {
        int err;

        err = errno;
        fclose( mf );
        if( err )
            errno = err;
    }

    if( $FAILED(sts) ) {
        if( errno ) {
            int err;

            err = errno;
            printmsg( sts, 0, newfilename );
            sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, sts,
                            strerror( err ) );
        } else
            sts = printmsg( sts, 0, newfilename );

        if( newmsg != NULL ) free( newmsg );
        if( newfilename != NULL ) free( newfilename );
    }

    return sts;
}

/********************************************************** showmessagefile() */
vmscond_t show_message( int argc, char **argv ) {
    unsigned long n;

    if( argc > 2 ) { /* SHOW MESSAGE numeric [numeric...] */
        for( ; argc > 2; --argc, ++argv ) {
            n = strtoul( argv[2], NULL, 0 );
            if( $PRINTED((vmscond_t)n) ){
                printmsg( HELP_SHOWMSGPRINTED, MSG_TEXT | MSG_NOCRLF );
                printmsg( (vmscond_t)n, MSG_CONTINUE | MSG_ALWAYS, HELP_SHOWMSGPRINTED );
            } else
                printmsg( (vmscond_t)n, MSG_NOARGS | MSG_ALWAYS );
        }
        return SS$_NORMAL;
    }
    return printmsg( ODS2_MSGFILE, 0, (msgfilename? msgfilename: "/DEFAULT") );
}

/******************************************************************* decstr() */

static int decstr( FILE *mf, size_t *size, char **pp, char **sp ) {
    int c;
    size_t len = 0;

    if( sp != NULL )
        *sp = NULL;

    while( (c = getc( mf )) != EOF && c != '\0' ) {
        if( len == 0 ) {
            len = 1;
            if( sp != NULL )
                *sp = *pp;
        }
        if( c == '\001' ) {
            const char *s;
            size_t slen;

            c = getc( mf );
            if( c == EOF )
                return c;

            if( c < PRI_CODEBASE || c > PRI_CODEEND )
                return EOF;

            s = pristr[ c - PRI_CODEBASE ];
            len +=
                slen = strlen( s );
            if( sp != NULL ) {
                memcpy( *pp, s, slen );
                *pp += slen;
            }
        } else {
            if( sp != NULL )
                *(*pp)++ = c;
            ++len;
        }
    }
    if( c == EOF )
        return c;

    *size += len;
    if( sp != NULL && len != 0 )
        *(*pp)++ = '\0';
    return 0;
}

/******************************************************************* sysmsg_rundown() */

void sysmsg_rundown( void ) {
    size_t i;

    for( i = 0; i < sizeof( retbuf ) / sizeof( retbuf[0] ); i++ )
        if( retbuf[i] != buf ) {
            free( retbuf[i] );
            retbuf[i] = NULL;
        }
    free( buf );
    buf = NULL;
    free( msgfile );
    msgfile = NULL;
    free( msgfilename );
    msgfilename = NULL;
}


