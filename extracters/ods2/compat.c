/* Timothe Litt February 2016 */

/* This module contains compatibility code, currently just
 * to support Microsoft Windows.
 *
 * Microsoft deprecates sprintf, but doesn't supply standard
 * replacement until very recent IDEs.
 * Microsoft doesn't like fopen.
 * One needs to use a M$ call to translate system errors.
 *
 * Finding out about drive letter assignments is unique to windows.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "compat.h"

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900

int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}
#endif

#ifdef _MSC_VER

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

TCHAR *w32_errstr( DWORD eno, ... ) {
    va_list ap;
    TCHAR *msg;

    if( eno == 0 )
        eno = GetLastError();
    va_start(ap,eno);

    if( FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM, NULL, eno, 0,
        (LPSTR)&msg, 1, &ap ) == 0 ) {
        msg = (TCHAR *)malloc( 32 );
        snprintf( msg, 32, "(%u)", eno );
    }
    va_end(ap);
    return msg;
}

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

/* For ISO C compliance, ensure that there's something in this module */
void dummy_compat ( void ) { return; }
