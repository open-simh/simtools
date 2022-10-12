#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>
#include <stdio.h>

void dumpblock( unsigned block, unsigned length, const char *buffer,
                const char *more, ... );
void vdumpblock( unsigned block, unsigned length, const char *buffer,
                 const char *more, va_list ap );
void dumpblockto( FILE *fp, unsigned block, unsigned length, const char *buffer,
                const char *more, ... );
void vdumpblockto( FILE *fp, unsigned block, unsigned length, const char *buffer,
                   const char *more, va_list ap );
#endif
