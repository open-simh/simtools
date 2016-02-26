/* Timothe Litt February 2016 */

/* This module contains compatibility code, currently just
 * to support Microsoft Windows.
 *
 * Microsoft deprecates sprintf, but doesn't supply standard
 * replacement until very recent IDEs.
 * Microsoft doesn't like fopen.
 * One needs to use a M$ call to translate system errors.
 */

#if defined(_MSC_VER) && _MSC_VER < 1900

#include <stdio.h>
#include "compat.h"

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

#ifdef _WIN32
#include <errno.h>
#include <windows.h>

FILE *openf( const char *filename, const char *mode ) {
  errno_t err;
  FILE *fd = NULL;

  err = fopen_s( &fd, filename, mode );
  if( err == 0 ) {
    return fd;
  }
  return NULL;
}


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
#endif
#endif

/* For ISO C compliance, ensure that there's something in this module */
void dummy_compat ( void ) { return; }
