/* Debug routines */

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


#include <stddef.h>
#include <stdio.h>

#include "debug.h"

/*********************************************************** dumpblock() */
void dumpblock( unsigned block, unsigned length, const char *buffer,
                const char *more, ... ) {
    va_list ap;

    va_start( ap, more );
    vdumpblockto( stdout, block, length, buffer, more, ap );
    va_end( ap );
}

/*********************************************************** vdumpblock() */

void vdumpblock( unsigned block, unsigned length, const char *buffer,
                const char *more, va_list ap ) {

    vdumpblockto( stdout, block, length, buffer, more, ap );
}

/*********************************************************** dumpblockto() */

void dumpblockto( FILE *fp, unsigned block, unsigned length, const char *buffer,
                const char *more, ... ) {
    va_list ap;

    va_start( ap, more );
    vdumpblockto( fp, block, length, buffer, more, ap );
    va_end( ap );
}

/*********************************************************** vdumpblockto() */

void vdumpblockto( FILE *fp, unsigned block, unsigned length, const char *buffer,
                   const char *more, va_list ap ) {
#ifndef DEBUG_DISKIO
    (void) fp;
    (void) block;
    (void) length;
    (void) buffer;
    (void) more;
    (void) ap;
#else
    const char *p;

    size_t n, wid = 32, off = 0;

    fprintf( fp, "\n****************************************\n"
            "Block %u Length %u", block, length );
    if( more != NULL ) {
        fputc( ' ', fp );
        vfprintf( fp, more, ap );
    }
    fputc( '\n', fp );
    if( length == 0 )
        return;

    fprintf( fp, "     " );
    for( n = 0; n < wid; n++ ) {
        if( n % 2 == 0 )
            fprintf( fp, " %02x", (int)n );
        else
            fprintf( fp, "  " );
    }
    fprintf( fp, "  " );
    for( n = 0; n < wid; n++ ) {
        if( n % 4 == 0 )
            fprintf( fp, " %02x  ", (int)n );
    }
    fputc( '\n', fp );

    while( length ) {
        fprintf( fp, "%04x:", (unsigned) off );
        p = buffer;
        for( n = 0; n < wid; n++ ) {
            if( n % 2 == 0 )
                fputc( ' ', fp );
            if( n < length ) {
                fprintf( fp, "%02x", 0xff & (unsigned)*p++ );
            } else
                fprintf( fp, "  " );
        }

        fprintf( fp, "  " );
        p = buffer;
        for( n = 0; n < wid; n++, p++ ) {
            if( n % 4 == 0 )
                fputc( ' ', fp );
            fprintf( fp, "%c", (n >= length)? ' ': (*p < 040 || *p > 0176)? '.': *p );
        }
        length -= wid;
        buffer += wid;
        off += wid;
        fputc( '\n', fp );
    }
#endif

    return;
}
