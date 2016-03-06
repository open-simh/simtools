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
#include <errno.h>
#include <stdarg.h>
#include <windows.h>
#include <direct.h>
#undef getcwd
#define getcwd _getcwd
#undef chdir
#define chdir _chdir

#undef strerror
#define strerror(n) ods2_strerror(n)
const char *ods2_strerror( errno_t errn );

TCHAR *w32_errstr( DWORD eno, ... );
char *driveFromLetter( const char *letter );

#else /* Not WIN32 */
#include <unistd.h>
#endif

#define UNUSED(x) (void)(x)

#endif
