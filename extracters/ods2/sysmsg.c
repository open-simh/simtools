/* Timothe Litt litt _at_ acm _ddot_ org */
/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/* Message code translations for non-VMS systems */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Should replace with lib$sys_getmsg under VMS

#ifndef VMS
*/

#include "ssdef.h"
#include "rms.h"
#include "compat.h"
#include "sysmsg.h"

#define MSG(code,text) { $VMS_STATUS_COND_ID(code), #code, text },

static
const struct VMSFAC {
    unsigned int code;
    const char *const text;
} fac2text[] = {
    { SYSTEM$_FACILITY, "SYSTEM" },
    { RMS$_FACILITY, "RMS" },
    { 0, NULL },
    { 0, "NONAME" }
};

static const char sev2text[(STS$M_SEVERITY>>STS$V_SEVERITY)+1] =
    { 'W', 'S', 'E', 'I', 'F', '?', '?', '?', };

/* Unknown facility or message code */

#ifndef SS$_NOMSG
#define SS$_NOMSG 0xFFFFFFFF
#endif

static const char nofmt[] = "Message number %08X";
static char notext[sizeof(nofmt)+8];

static
const struct VMSMSG {
    unsigned int code;
    const char *const txtcode;
    char *text;
} vms2text[] = {
    MSG(RMS$_BUG, "fatal RMS condition detected, process deleted")
    MSG(RMS$_DIR, "error in directory name")
    MSG(RMS$_DNF, "directory not found")
    MSG(RMS$_EOF, "end of file detected")
    MSG(RMS$_ESS, "expanded string area too small")
    MSG(RMS$_FNF, "file not found")
    MSG(RMS$_FNM, "error in file name")
    MSG(RMS$_IFI, "invalid internal file identifier (IFI) value")
    MSG(RMS$_NAM, "invalid NAM block or NAM block not accessible")
    MSG(RMS$_NMF, "no more files found")
    MSG(RMS$_RSS, "invalid resultant string size")
    MSG(RMS$_RSZ, "invalid record size")
    MSG(RMS$_RTB, "%u byte record too large for user's buffer") /* !UL byte */
    MSG(RMS$_WCC, "invalid wild card context (WCC) value")
    MSG(RMS$_WLD, "invalid wildcard operation")
    MSG(SS$_ABORT, "abort")
    MSG(SS$_BADFILENAME, "bad file name syntax")
    MSG(SS$_BADIRECTORY, "bad directory file format")
    MSG(SS$_BADPARAM, "bad parameter value")
    MSG(SS$_BUGCHECK, "internal consistency failure")
    MSG(SS$_DATACHECK, "write check error")
    MSG(SS$_DEVICEFULL, "device full - allocation failure")
    MSG(SS$_DEVMOUNT, "device is already mounted")
    MSG(SS$_DEVNOTALLOC, "device not allocated")
    MSG(SS$_DEVNOTDISM, "device not dismounted")
    MSG(SS$_DEVNOTMOUNT, "device is not mounted")
    MSG(SS$_DUPFILENAME, "duplicate file name")
    MSG(SS$_DUPLICATE, "duplicate name")
    MSG(SS$_ENDOFFILE, "end of file")
    MSG(SS$_FILELOCKED, "file is deaccess locked")
    MSG(SS$_FILESEQCHK, "file identification sequence number check")
    MSG(SS$_ILLEFC, "illegal event flag cluster")
    MSG(SS$_INSFMEM, "insufficient dynamic memory")
    MSG(SS$_ITEMNOTFOUND, "requested item cannot be returned")
    MSG(SS$_NOMOREDEV, "no more devices")
    MSG(SS$_IVCHAN, "invalid I/O channel")
    MSG(SS$_IVDEVNAM, "invalid device name")
    MSG(SS$_NOIOCHAN, "no I/O channel available")
    MSG(SS$_NOMOREFILES, "no more files")
    MSG(SS$_NORMAL, "normal successful completion")
    MSG(SS$_NOSUCHDEV, "no such device available")
    MSG(SS$_NOSUCHFILE, "no such file")
    MSG(SS$_NOSUCHVOL, "No such volume")
    MSG(SS$_NOTINSTALL, "writable shareable images must be installed")
    MSG(SS$_PARITY, "parity error")
    MSG(SS$_UNSUPVOLSET, "Volume set not supported")
    MSG(SS$_WASCLR, "normal successful completion")
    MSG(SS$_WASSET, "Event flag was set")
    MSG(SS$_WRITLCK, "write lock error")
    MSG(SS$_OFFSET_TOO_BIG, "Volume is too large for local file system: needs 64-bit I/O" )
    {0, NULL, NULL},
    { SS$_NOMSG, "SS$_NOMSG", notext }
};

static char *buf = NULL;
static size_t bufsize = 0;

/******************************************************************* xpand() */

static int xpand( size_t used, size_t add ) {
    char *nbuf;
    size_t need;

    need = used + add + 1;                           /* Always allow for \0 */
    if( need < bufsize ) {
        return 1;
    }
    nbuf = realloc( buf, need + 16 + 1);             /* 16 minimizes reallocs */
    if( nbuf == NULL ) {
        return 0;
    }
    buf = nbuf;
    bufsize = need + 16 + 1;
    return 1;
}

/******************************************************************* getmsg() */

const char *getmsg( unsigned int vmscode, unsigned int flags, ... ) {
    const struct VMSMSG *mp = NULL;
    const struct VMSFAC *fp = NULL;

    if( !(flags & MSG_FULL) )
        flags = (flags & ~MSG_FULL) | MSG_FULL;

    while( 1 ) {
        size_t i = 0, len;

        do {
            if( flags & (MSG_FACILITY|MSG_SEVERITY|MSG_NAME) ) {
                if( !xpand( i, 1 ) )
                    return strerror( errno );
                buf[i++] = '%';

                if( fp == NULL ) {
                    for( fp = fac2text; fp->text != NULL; fp++ ) {
                        if( fp->code == $VMS_STATUS_FAC_NO(vmscode) )
                            break;
                    }
                }
                if( fp->text == NULL )
                    break;
                if( flags & MSG_FACILITY ) {
                    len = strlen( fp->text );
                    if( !xpand( i, len +1 ) )
                        return strerror( errno );

                    memcpy( buf+i, fp->text, len );
                    i += len;
                    if( flags & (MSG_SEVERITY|MSG_NAME) )
                        buf[i++] = '-';
                }
                if( flags & MSG_SEVERITY ) {
                    if( !xpand( i, 2 ) )
                        return strerror( errno );
                    buf[i++] = sev2text[$VMS_STATUS_SEVERITY(vmscode)];
                    if( flags & MSG_NAME )
                        buf[i++] = '-';
                }
                if( flags & MSG_NAME ) {
                    char *p;

                    if( mp == NULL ) {
                        for( mp = vms2text; mp->text != NULL; mp++ ) {
                            if( $VMS_STATUS_COND_ID(vmscode) == mp-> code )
                                break;
                        }
                    }
                    if( mp->text == NULL )
                        break;
                    p = strchr( mp->txtcode, '_' );
                    if( p == NULL )
                        abort();
                    len = strlen( ++p );
                    if( !xpand( i, len ) )
                        return strerror( errno );
                    memcpy( buf+i, p, len );
                    i += len;
                }
                if( flags & MSG_TEXT ) {
                    if( !xpand( i, 2 ) )
                        return strerror( errno );
                    buf[i++] = ',';
                    buf[i++] = ' ';
                }
            }
            if( flags & MSG_TEXT ) {
                va_list ap;
                if( mp == NULL ) {
                    for( mp = vms2text; mp->text; mp++ ) {
                        if( $VMS_STATUS_COND_ID(vmscode) == mp-> code )
                            break;
                    }
                    if( mp->text == NULL )
                        break;
                }
                if( flags & MSG_WITHARGS ) {
                    va_start( ap, flags );
                    len = vsnprintf( buf+i, 1, mp->text, ap );
                    va_end( ap );
                } else
                    len = strlen( mp->text );
                if( !xpand( i, len ) )
                    return strerror( errno );
                if( flags & MSG_WITHARGS ) {
                    va_start( ap, flags );
                    (void) vsnprintf( buf+i, len+1, mp->text, ap );
                    va_end( ap );
                } else
                    memcpy( buf+i, mp->text, len );
                i += len;
            }
            buf[i] = '\0';
            return buf;
        } while( 0 );

        if( fp == NULL || fp->text == NULL )
            fp = fac2text + (sizeof(fac2text)/sizeof(fac2text[0])) -1;

        mp = vms2text + (sizeof(vms2text)/sizeof(vms2text[0])) -1;
        snprintf( notext, sizeof(notext), nofmt, vmscode );
    }
}

/******************************************************************* sysmsg_rundown() */

void sysmsg_rundown( void ) {
    free( buf );
    buf = NULL;
}

/*
#endif
*/
