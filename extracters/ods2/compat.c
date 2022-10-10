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

/* This module contains compatibility code, currently just
 * to support Microsoft Windows.
 *
 * Microsoft deprecates sprintf, but doesn't supply standard
 * replacement until very recent IDEs.
 * Microsoft doesn't like fopen, or strerror, or getcwd, or getenv.
 * One needs to use a M$ call to translate system errors.
 *
 * Finding out about drive letter assignments is unique to windows.
 */

#if !defined( DEBUG ) && defined( DEBUG_COMPAT )
#define DEBUG DEBUG_COMPAT
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "descrip.h"
#include "stsdef.h"

#ifdef VMS
#include <jpidef.h>
#else
#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>            /* PathSearchAndQualify() */
#else
#include <unistd.h>
#include <sys/types.h>
#ifndef USER_FROM_ENV
#include <pwd.h>
#endif
#endif
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900

/******************************************************************* c99_vsnprintf() */

int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap) {
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

/******************************************************************* c99_snprintf() */

int c99_snprintf(char *outBuf, size_t size, const char *format, ...) {
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}
#endif

#ifdef _MSC_VER

/******************************************************************* openf() */

FILE *openf( const char *filename, const char *mode ) {
  errno_t err;
  FILE *fd = NULL;

  err = fopen_s( &fd, filename, mode );
  if( err == 0 ) {
    return fd;
  }
  return NULL;
}
#endif

#ifdef _WIN32

/******************************************************************* ods2_strerror() */

const char *ods2_strerror( int errn ) {
    static char buf[256];

    if( strerror_s( buf, sizeof( buf ), errn ) != 0 )
        (void) snprintf( buf, sizeof( buf ), "Untranslatable error %u", errn );

    return buf;
}

/******************************************************************* w32_errstr() */

TCHAR *w32_errstr( DWORD eno, ... ) {
    va_list ap;
    TCHAR *msg = NULL;

    if( eno == NO_ERROR )
        eno = GetLastError();
    va_start(ap, eno);

    if( FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM, NULL, eno, 0,
        (LPSTR)&msg, 1, &ap ) == 0 ) {
        msg = (TCHAR *)malloc( 64 );
        if( msg == NULL ) {
            printf( "Out of memory reporting Windows error(%u\n", eno );
        } else
            (void) snprintf( msg, 64, "(%u)", eno );
    }
    va_end(ap);
    return msg;
}

/******************************************************************* driveFromLetter() */

char *driveFromLetter( const char *letter ) {
    DWORD rv = ERROR_INSUFFICIENT_BUFFER;
    DWORD cs = 16;
    TCHAR *bufp = NULL;

    do {
        if( rv == ERROR_INSUFFICIENT_BUFFER ) {
            TCHAR *newp;
            cs *= 2;
            newp = (TCHAR *) realloc( bufp, cs );
            if( newp == NULL )
                break;
            bufp = newp;
        }
        rv = QueryDosDevice( letter, bufp, cs );
        if( rv == 0 ) {
            rv = GetLastError();
            continue;
        }
        return bufp;
    } while( rv == ERROR_INSUFFICIENT_BUFFER );

    free( bufp );
    return NULL;
}
#endif

/******************************************************************* get_env() */

char *get_env( const char *name ) {
    size_t i;
    char *r;

#ifdef _WIN32
    (void) getenv_s( &i, NULL, 0, name );
    if( i == 0 )
        return NULL;
    if ((r = malloc( i )) == NULL )
        return NULL;
    (void)getenv_s( &i, r, i, name );
    return r;
#else
    char *t;

    t = getenv( name );
    if( t == NULL )
        return NULL;
    i = strlen( t );
    r = malloc( i + 1 );
    if( r == NULL )
        return NULL;

    memcpy( r, t, i+1 );

    return r;
#endif
}

/******************************************************************* get_username() */

char *get_username( void ) {
    char *username;
    char *r;
    size_t i;
#ifdef VMS
    char uame[12 + 1] = "UNKNOWN     ";
    struct dsc$descriptor_s userdsc = { sizeof( uname ) - 1,
                                        DSC$K_DTYPE_T,
                                        DSC$K_CLASS_S, uname };

    r = "UNKNOWN";
    if( $SUCCESSFUL(lib$getjpi( &JPI$_USERNAME, 0, 0, 0, &userdsc, 0 )) ) {
        for( i = sizeof( uname ) - 1; i >= 0; i-- ) {
            if( uname[i] == ' ' )
                uname[i] = '\0';
            else
                break;
        }
        if( uname[0] )
            r = uname;
    }
#else
#ifdef _WIN32
    (void) getenv_s( &i, NULL, 0, "USERNAME" );
    if( i == 0 )
        r = "UNKNOWN";
    else {
        if ((r = malloc( i )) == NULL )
            return NULL;
        (void)getenv_s( &i, r, i, "USERNAME" );
        return r;
    }
#else
#ifdef USER_FROM_ENV
    if( (r = getenv( "USER" )) == NULL )
        r = getenv( "LOGNAME" );
    if( r == NULL )
        r = "UNKNOWN";
#else
    struct passwd *pw;

    pw = getpwuid(geteuid());
    r = pw->pw_name;
#endif
#endif
#endif
    i = strlen( r );
    if( (username = malloc( i + 1 )) == NULL )
        return NULL;
    memcpy( username, r, i + 1 );
    return username;
}

/******************************************************************* homefile () */

char *homefile( int dotfile, const char *filename, const char *ext ) {
    char * prefix, *delim, *name;
    size_t pfxlen, dellen, namlen, extlen;

#ifdef _WIN32
    char *drive;

    prefix = drive = get_env( "HOMEDRIVE" );
    if( drive != NULL ) {
        prefix = get_env( "HOMEPATH" );
        if( prefix != NULL ) {
            dellen = strlen( drive );
            pfxlen = strlen( prefix );
            if( (name = malloc( dellen + pfxlen + 1 )) == NULL ) {
                free( prefix );
                free( drive );
                return NULL;
            }
            memcpy( name, drive, dellen );
            memcpy( name+dellen, prefix, pfxlen +1 );
            free( prefix );
            prefix = name;
        }
        free( drive );
    }
    delim = dotfile? "\\.": "\\";
#elif defined( VMS )
    prefix = get_env( "SYS$LOGIN" );
    delim = dotfile? "_": "";
#else
    prefix = get_env( "HOME" );
    delim = dotfile? "/." : "/";
#endif
    pfxlen = prefix? strlen( prefix ) : 0;
    dellen = strlen( delim );
    namlen = strlen( filename );
    extlen = ext? strlen( ext ) : 0;

    if( (name = malloc( pfxlen+dellen+namlen+extlen+1)) == NULL )
        return NULL;
    /* N.B. There is no buffer overrun here */
    if( pfxlen )
        memcpy( name, prefix, pfxlen );
    memcpy( name+pfxlen, delim, dellen );
    memcpy( name+pfxlen+dellen, filename, namlen+1 );
    if( ext )
        memcpy( name+pfxlen+dellen+namlen, ext, extlen+1 );
    free( prefix );

    return name;
}

/************************************************************* get_realpath() */

char *get_realpath( const char *filnam ) {
#ifdef _WIN32
    size_t n;
    char *path, resultant_path[MAX_PATH];

    if ( filnam == NULL || strpbrk( filnam, "*?" ) != NULL ||
         !PathSearchAndQualify( filnam,              /* pcszPath              */
                                resultant_path,      /* pszFullyQualifiedPath */
                                                     /* cchFullyQualifiedPath */
                                sizeof( resultant_path )
                              ) ) {
        return NULL;
    }
    n = strlen( resultant_path );
    path = (char *) malloc( n + 1 );
    if ( path != NULL ) {
        memcpy( path, resultant_path, n + 1 );
    }
    return path;
#elif defined VMS
#define MAXPATH 255
    size_t n;
    unsigned short resultant_path[1 + ( ( MAXPATH + 1 ) / 2)];
    char *path;
    unsigned long context, sts, flags;
    struct dsc$descriptor filespec = {
        0, DSC$K_DTYPE_T, DSC$K_CLASS_S, NULL
    };
    struct dsc$descriptor resultant_filespec = {
        MAXPATH, DSC$K_DTYPE_T, DSC$K_CLASS_VS, (char *) resultant_path
    };

    path = NULL;
    if ( filnam != NULL ) {
        filespec.dsc$w_length = strlen( filnam );
        filespec.dsc$a_pointer = (char *) filnam;
        *resultant_path = 0;
        context = 0;
        flags = LIB$M_FIL_NOWILD;
        sts = lib$find_file( &filespec, &resultant_filespec, &context,
                             NULL, NULL, &sts, &flags );
        lib$find_file_end( &context );
        if( $SUCCESSFUL(sts) ) {
            n = *resultant_path;
            path = (char *) malloc( n + 1 );
            if ( path != NULL ) {
                memcpy( path, resultant_path + 1, n );
                path[n] = '\0';
            }
        }
    }
    return path;
#elif defined unix || 1
    size_t n;
    char *path, resolved_path[PATH_MAX+1];

    if ( filnam == NULL || realpath( filnam, resolved_path ) == NULL ) {
        return NULL;
    }
    n = strlen( resolved_path );
    path = (char *) malloc( n + 1 );
    if ( path != NULL ) {
        memcpy( path, resolved_path, n + 1 );
    }
    return path;
#endif
}

/******************************************************************** Ctime() */

char *Ctime( time_t *tval ) {
    char *buf;

#ifdef _WIN32
    if( (buf = malloc( 26 )) == NULL ) return NULL;
    if( ctime_s( buf, 26, tval ) != 0 ) {
        free( buf );
        return NULL;
    }
#else
    char *cbuf;

    if( (buf = malloc( 25 )) == NULL ) return NULL;
    if( (cbuf = ctime( tval )) == NULL ) {
        free( buf );
        return NULL;
    }
    memcpy( buf, cbuf, 24 );
#endif
    buf[24] = '\0';
    return buf;
}

/******************************************************************* fgetline() */
/* Read a line of input - unlimited length
 * Removes \n, returns NULL at EOF
 * If *buf is NULL, one is allocated of size *bufsize.
 * Thereafter, it is reused.  Expanded if necessary.
 * Caller responsible for free()
 */
char *fgetline( FILE *stream, int keepnl, char **buf, size_t *bufsize ) {
    int c;
    size_t idx = 0;

    if( *buf == NULL && (*buf = malloc( *bufsize )) == NULL ) {
        perror( "malloc" );
        exit(EXIT_FAILURE);
    }

    while( (c = getc( stream )) != EOF && c != '\n' ) {
        if( idx + (keepnl != 0) +2 > *bufsize ) { /* Now + char + (? \n) + \0 */
            char *nbuf;
            *bufsize += 32;
            nbuf = (char *) realloc( *buf, *bufsize );
            if( nbuf == NULL ) {
                perror( "realloc" );
                exit(EXIT_FAILURE);
            }
            *buf = nbuf;
        }
        buf[0][idx++] = c;
    }
    if( c == '\n' ) {
        if( keepnl )
            buf[0][idx++] = '\n';
    } else {
        if( c == EOF && idx == 0 ) {
            buf[0][0] = '\0';
            return NULL;
        }
    }
    buf[0][idx] = '\0';
    return *buf;
}
