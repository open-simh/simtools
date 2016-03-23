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
 *       the contibution of the original author.
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
    TCHAR *msg;

    if( eno == NO_ERROR )
        eno = GetLastError();
    va_start(ap,eno);

    if( FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM, NULL, eno, 0,
        (LPSTR)&msg, 1, &ap ) == 0 ) {
        msg = (TCHAR *)malloc( 32 );
        if( msg == NULL ) {
            perror( "malloc" );
            printf( "Original error number (%u)\n", eno );
        } else
        (void) snprintf( msg, 32, "(%u)", eno );
    }
    va_end(ap);
    return msg;
}

/******************************************************************* driveFromLetter() */

char *driveFromLetter( const char *letter ) {
    DWORD rv = ERROR_INSUFFICIENT_BUFFER;
    size_t cs = 16;
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
    (void)getenv_s( &i, r, i, "USERNAME" );
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
    struct dsc$descriptor_s userdsc = { sizeof( uname ) - 1,  DSC$K_DTYPE_T, DSC$K_CLASS_S, uname };

    r = "UNKNOWN";
    if( lib$getjpi( &JPI$_USERNAME, 0, 0, 0, &userdsc, 0 ) & STS$M_SUCCESS ) {
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
