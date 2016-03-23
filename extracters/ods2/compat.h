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
#include <stdio.h>
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
#undef tzset
#define tzset _tzset
#undef setenv
#define setenv(name, value, overwrite) _putenv_s(name, value)
#undef unsetenv
#define unsetenv(name) _putenv_s(name,"")
#undef unlink
#define unlink _unlink

#undef strerror
#define strerror(n) ods2_strerror(n)
const char *ods2_strerror( errno_t errn );

TCHAR *w32_errstr( DWORD eno, ... );
char *driveFromLetter( const char *letter );

#else /* Not WIN32 */
#include <unistd.h>
#endif

char *get_username( void );
char *get_env( const char *name );

#define UNUSED(x) (void)(x)

#endif
