#ifndef COMPAT_H
#define COMPAT_H

#if defined(_MSC_VER) && _MSC_VER < 1900

#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf

#include <stdarg.h>

int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap);
int c99_snprintf(char *outBuf, size_t size, const char *format, ...);

#endif

#ifdef _MSC_VER
FILE *openf( const char *filename, const char *mode );
#else
#define openf fopen
#endif

#ifdef _WIN32
#include <stdarg.h>
#include <windows.h>

TCHAR *w32_errstr( DWORD eno, ... );
char *driveFromLetter( const char *letter );
#endif

#define UNUSED(x) (void)(x)

#endif
