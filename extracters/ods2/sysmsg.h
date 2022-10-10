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
 *       subsequent contributors. This is free software; no
 *       warranty is offered,  and while we believe it to be useful,
 *       you use it at your own risk.
 */

#ifndef SYSMSG_H
#define SYSMSG_H

#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include "ods2.h"

#ifdef SYSMSG_INTERNAL
/* Shared between sysmsg & genmsg, which produces loadable message files */

#define MDF_VERSION "003"

#define MSG_HEADER_REC "MSGV"MDF_VERSION"%08"PRIx32"%08"PRIx32
#define MSG_HEADER_DEC "MSGV"MDF_VERSION"%8"SCNx32"%8"SCNx32"%n"
#define MSG_HEADER_DECN (4+3+8+8)

#define MSG_FAC_REC "%08"PRIx32
#define MSG_FAC_DEC "%8"SCNx32"%n"
#define MSG_FAC_DECN (8)

#define MSG_MSG_REC "%08"PRIx32"%08"PRIx32
#define MSG_MSG_DEC "%8"SCNx32"%8"SCNx32"%n"
#define MSG_MSG_DECN (8+8)

#define MSG_EOF_REC "VGSM"MDF_VERSION
#define MSG_EOF_DEC "VGSM"MDF_VERSION"%n"
#define MSG_EOF_DECN (4+3)

struct VMSFAC {
    char     *text;
    vmscond_t code;
};

#define FAC(code, text) { #text, (code) },

struct VMSMSG {
    char     *ident;
    char     *text;
    vmscond_t code;
    uint32_t  nargs;
};
#define MSG(code, ident, text, nargs) { #ident, text,  (code), nargs },

/* Magic codes for formats */
static const char *const pristr[] = {
    PRIo8, PRIo16, PRIo32, PRIo64,  /* A - D */
    PRIu8, PRIu16, PRIu32, PRIu64,  /* E - H */
    PRIX8, PRIX16, PRIX32, PRIX64,  /* I - L */
    PRId8, PRId16, PRId32, PRId64,  /* M - P */
};
static const char *const prisym[] = {
    "PRIo8", "PRIo16", "PRIo32", "PRIo64",  /* A - D */
    "PRIu8", "PRIu16", "PRIu32", "PRIu64",  /* E - H */
    "PRIX8", "PRIX16", "PRIX32", "PRIX64",  /* I - L */
    "PRId8", "PRId16", "PRId32", "PRId64",  /* M - P */
};
#define PRI_CODEBASE 'A'
#define PRI_CODEEND  'p'

/* How data is sorted (by genmsg, for sysmsg) */

#include "stsdef.h"

/******************************************************************* faccmp() */
static int faccmp( const void *p1, const void *p2 ) {
    uint32_t c1, c2;

    c1 = ((struct VMSFAC *)p1)->code;
    c2 = ((struct VMSFAC *)p2)->code;

    if( c1 < c2 )
        return -1;
    if( c1 == c2 )
        return 0;
    return 1;
}

/******************************************************************* msgcmp() */
static int msgcmp( const void *p1, const void *p2 ) {
    uint32_t c1, c2;

    c1 = ((struct VMSMSG *)p1)->code & STS$M_COND_ID;
    c2 = ((struct VMSMSG *)p2)->code & STS$M_COND_ID;

    if( !(c1 & STS$M_FAC_SP) )
        c1 &= STS$M_CUST_DEF | ~STS$M_FAC_NO;
    if( !(c2 & STS$M_FAC_SP) )
        c2 &= STS$M_CUST_DEF | ~STS$M_FAC_NO;

    if( c1 < c2 )
        return -1;
    if( c1 == c2 )
        return 0;
    return 1;
}

#endif /* SYSMSG_INTERNAL */

/* Flags for controlling formatting of messages for display */

#define MSG_FACILITY (1 <<  0)
#define MSG_SEVERITY (1 <<  1)
#define MSG_NAME     (1 <<  2)
#define MSG_TEXT     (1 <<  3)
#define MSG_WITHARGS (1 <<  4)
#define MSG_ALWAYS   (1 <<  5)
#define MSG_CONTINUE (1 <<  6)
#define MSG_NOCRLF   (1 <<  7)
#define MSG_NOARGS   (1 <<  8)
#define MSG_NOUC     (1 <<  9)
#define MSG_TOFILE   (1 << 10)
#define MSG_ALWAYS_CONT (MSG_ALWAYS|MSG_CONTNUE)
/* Must not exceed OPT_V_GENERIC -1 in ods2.h (currently 19).
 * see setcmd.c.
 */

#define MSG_FULL     (MSG_FACILITY|MSG_SEVERITY|MSG_NAME|MSG_TEXT)

/* Public API */

vmscond_t printmsg( vmscond_t vmscond, unsigned int flags, ... );
vmscond_t vprintmsg( vmscond_t vmscond, unsigned int flags, va_list ap );

const char *getmsg( vmscond_t vmscond, unsigned int flags, ... );

const char *getmsg_string( vmscond_t vmscond, unsigned int flags );

vmscond_t set_message_file( const char *filename );
vmscond_t show_message( int argc, char **argv );
vmscond_t set_message_format( options_t new );
options_t get_message_format( void );

void sysmsg_rundown( void );

#endif
