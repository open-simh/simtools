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

/* Write a message data file.
 *
 * Update/translate messages in cc.msg (en_us.msg)
 * The makefiles handle this:
 *
 * message -bt vms_messages.msg en_us.msg en_us.mt
 * cc genmsg '-DTABLEFILE="en_us.mt"' -o genmsg
 * genmsg en_us.mdf
 * rm -f genmsg
 * ods2 set message en_us
 *
 * For translations:
 * message -br vms_messages_fr.msg fr_ca.msg fr_ca.mt
 * cc genmsg '-DTABLEFILE="fr_ca.mt"' -o genmsg
 * genmsg fr_ca.mdf
 * rm -f genmsg
 * ods2 set message fr_ca
 */

#ifndef TABLEFILE
#define TABLEFILE "default.md"
#endif

#define _XOPEN_SOURCE

#ifdef _MSC_VER /* fopen*/
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define SYSMSG_INTERNAL
#include "sysmsg.h"

#include "ssdef.h"
#include "stsdef.h"

static const char
hdrrec[] = { MSG_HEADER_REC },
    facrec[] = { MSG_FAC_REC },
    msgrec[] = { MSG_MSG_REC },
    summary1[] = { "%s: %"PRIu32 " facilities, %"PRIu32 " messages" },
    summary2[] = { ", file size %"PRIuMAX " writtten to " };

static const char facdatafmt[] = { "{ \"%s\", UINT32_C(%"PRIu32")},\n" };
static const char msgdatafmt[] = { "{ \"%s\", \"%s\", UINT32_C(0x%08"PRIX32"), UINT32_C(%"PRIu32") },\n" };

/* For portability of .mdf files, need fixed length encodings for host-specific
 * format descriptors... This re-encodes them without source changes:
 */

#undef PRIo8
#undef PRIo16
#undef PRIo32
#undef PRIo64
#define PRIo8   "\001A"
#define PRIo16  "\001B"
#define PRIo32  "\001C"
#define PRIo64  "\001D"

#undef PRIu8
#undef PRIu16
#undef PRIu32
#undef PRIu64
#define PRIu8   "\001E"
#define PRIu16  "\001F"
#define PRIu32  "\001G"
#define PRIu64  "\001H"

#undef PRIX8
#undef PRIX16
#undef PRIX32
#undef PRIX64
#define PRIX8   "\001I"
#define PRIX16  "\001J"
#define PRIX32  "\001K"
#define PRIX64  "\001L"

#undef PRId8
#undef PRId16
#undef PRId32
#undef PRId64
#define PRId8   "\001M"
#define PRId16  "\001N"
#define PRId32  "\001O"
#define PRId64  "\001P"

/* Instantiate the tables from ssdef.h (whatever language; it's essentially an object file)
 */

static
struct VMSFAC fac2text[] = {
#define GENERATE_FACTAB
#include TABLEFILE
};

static
struct VMSMSG vms2text[] = {
#define GENERATE_MSGTAB
#include TABLEFILE
};

static void putbyte( char c, size_t idx, char **buf, size_t *size );

/******************************************************************* main() */

int main( int argc, char **argv ) {
    struct VMSFAC *fp;
    struct VMSMSG *mp;
    FILE *ofp;
    int ac, quiet = 0, help = 0, newh = 0;
    char **av, *pname, *outname = NULL;

    ac = argc; av = argv;
    pname = argv[0];
    if( argc <= 1 ) help = 1;
    for( ; argc > 1; --argc, ++argv ) {
        if( !strcmp( argv[1], "--" ) ) break;
        if( *argv[1] == '-' && argv[1][1] ) {
            char *p;

            for( p = argv[1]+1; *p; p++ ) {
                switch( *p ) {
                case 'h':
                    help = 1;
                    break;
                case 'd':
                    newh = 1;
                    break;
                case 'q':
                    quiet = 1;
                    break;
                default:
                    fprintf( stderr, "Unknown option %c\n", argv[1][1] );
                    exit( EXIT_FAILURE );
                }
            }
        } else {
            break;
        }
    }

    if( help ) {
        fprintf( stderr, "%s - Generate ODS2 message file\n\n"
                 " Options: -q -- suppress informational messages\n"
                 "          -d -- generate a data file for built-in table\n"
                 "./message -bt vms_messages.msg ods2_en.msg en_us.mt\n"
                 "cc genmsg '-DTABLEFILE=\"en_us.mt\"'\n"
                 "./genmsg en_us.mdf\n"
                 "./ods2 set message en_us\n"
                 " ./genmsg -d default.md\n", pname
                 );
        exit( EXIT_SUCCESS );
    }

    if( argc > 1 ) {
        if( strcmp( argv[1], "-" ) ) {
            outname = argv[1];
        }
        --argc; ++argv;
    }

    qsort( fac2text, (uint32_t)(sizeof(fac2text)/sizeof(fac2text[0]))-1,
           sizeof(fac2text[0]), &faccmp );
    qsort( vms2text, (uint32_t)(sizeof(vms2text)/sizeof(vms2text[0])),
           sizeof(vms2text[0]), &msgcmp );

    if( outname == NULL )
        ofp = stdout;
    else if( (ofp = fopen( outname, "wb" )) == NULL ) {
        perror( outname );
        exit( EXIT_FAILURE );
    }

    if( !quiet ) {
        fprintf( stderr, summary1, TABLEFILE,
                 (uint32_t)(sizeof(fac2text)/sizeof(fac2text[0])) -1,
                 (uint32_t)(sizeof(vms2text)/sizeof(vms2text[0])) );
    }

    if( newh ) { /* Generate a new .h file for built-in defaults */
        size_t i, bufsize = 80;
        char *buf = NULL;

        fprintf( ofp,
                 "/* This is an automatically-generated file.\n"
                 " * Command:" );
        for( ; ac > 0; --ac, ++av )
            fprintf( ofp, " %s", *av );
        fprintf( ofp,
                 "\n"
                 " * Compiled-in version of a message table\n"
                 " * This technlology was developed by Timothe Litt, and is\n"
                 " * free to use with the ODS2 package, originally developed by\n"
                 " * Paul Nankervis <Paulnank@au1.ibm.com>.\n"
                 " * ODS2 may be freely distributed within the VMS community\n"
                 " * providing that the contributions of the original author and\n"
                 " * subsequent contributors are acknowledged in the help text and\n"
                 " * source files.  This is free software; no warranty is offered,\n"
                 " * and while we believe it to be useful, you use it at your own risk.\n"
                 " */\n\n" );

        fprintf( ofp,
                 "#ifdef GENERATE_FACTAB\n"
                 "#undef GENERATE_FACTAB\n" );
        for( i = 0; i < (uint32_t)(sizeof(fac2text)/sizeof(fac2text[0]))-1; i++ ) {
            fprintf( ofp, facdatafmt, fac2text[i].text, fac2text[i].code );
        }
        fprintf( ofp,
                 "#endif\n"
                 "#ifdef GENERATE_MSGTAB\n"
                 "#undef GENERATE_MSGTAB\n" );
        for( i = 0; i < (uint32_t)(sizeof(vms2text)/sizeof(vms2text[0])); i++ ) {
            char *p;
            size_t idx;

            p = vms2text[i].text;

            for( idx = 0; *p; p++ ) {
                char c = *p;
                switch( c ) {
                case '"':
                case '\\':
                    putbyte( '\\', idx++, &buf, &bufsize );
                    break;
                case '\n':
                    putbyte( '\\', idx++, &buf, &bufsize );
                    c = 'n';
                    break;
                case '\001': {
                    const char *xpn;

                    putbyte( '"', idx++, &buf, &bufsize );
                    c = *++p;
                    xpn = prisym[ c - PRI_CODEBASE ];
                    while( *xpn ) {
                        putbyte( *xpn++, idx++, &buf, &bufsize );
                    }
                    putbyte( '"', idx++, &buf, &bufsize );
                    continue;
                }
                default:
                    break;
                }
                putbyte( c, idx++, &buf, &bufsize );
            }
            putbyte( '\0', idx++, &buf, &bufsize );


            fprintf( ofp, msgdatafmt,
                     vms2text[i].ident, buf,
                     vms2text[i].code, vms2text[i].nargs );
        }
        fprintf( ofp,
                 "#endif\n" );
        if( ofp != stdout )
            fclose( ofp );

        if( !quiet ) {
            if( ofp == stdout )
                fputc( '\n', stderr );
            else
                fprintf( stderr, ", written to %s\n", outname );
        }
        exit( EXIT_SUCCESS );
    }

    /* File header record.  Version + number of facilities + number of message definitions
     */

    fprintf( ofp, hdrrec,
             (uint32_t)(sizeof(fac2text)/sizeof(fac2text[0])) -1,
             (uint32_t)(sizeof(vms2text)/sizeof(vms2text[0])) );
    fputc( '\0', ofp );

    /* Output each facility with its associated string.
     * Always produce an end-marker for the string.
     */

    for( fp = fac2text; fp < fac2text + sizeof(fac2text)/sizeof(fac2text[0]) -1; fp++ ) {
        fprintf( ofp, facrec, fp->code );
        if( fp->text )
            fprintf( ofp, "%s", fp->text );
        fputc( '\0', ofp );
    }

    /* Same for each message definition, but two strings.
     */

    for( mp = vms2text; mp < vms2text + sizeof(vms2text)/sizeof(vms2text[0]); mp++ ) {
        fprintf( ofp, msgrec,
                 mp->code, mp->nargs );
        if( mp->ident )
            fprintf( ofp, "%s", mp->ident );
        fputc( '\0', ofp );
        if( mp->text )
            fprintf( ofp, "%s", mp->text );
        fputc( '\0', ofp );
    }

    /* wrap up */

    fprintf( ofp, MSG_EOF_REC );
    fputc( '\0', ofp );

    if( ferror(ofp) ) {
        perror( "write" );
        exit( EXIT_FAILURE );
    }

    if( !quiet && ofp != stdout )
        fprintf( stderr, summary2, (uintmax_t)ftell(ofp) );

    if( ofp != stdout )
        fclose( ofp );

    if( !quiet ) {
        if( ofp == stdout )
            fputc( '\n', stderr );
        else
            fprintf( stderr, "%s\n", outname );
    }

    exit( EXIT_SUCCESS );
}

static void putbyte( char c, size_t idx, char **buf, size_t *size ) {
    if( *buf == NULL || idx+1 > *size ) {
        *buf = realloc( *buf, *size += 32 );
        if( *buf == NULL ) {
            perror( "realloc" );
            exit( EXIT_FAILURE );
        }
    }
    buf[0][idx++] = c;
    return;
}
