/* Timothe Litt litt _at_ acm _ddot_ org */

/* Message code translations for non-VMS systems */

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

static
const struct VMSMSG {
  unsigned int code;
  const char *const text;
} *mp, vms2text[] = {
{RMS$_BUG, "%RMS-F-BUG, fatal RMS condition detected, process deleted"},
{RMS$_DIR, "%RMS-F-DIR, error in directory name"},
{RMS$_DNF, "%RMS-E-DNF, directory not found"},
{RMS$_EOF, "%RMS-E-EOF, end of file detected"},
{RMS$_ESS, "%RMS-F-ESS, expanded string area too small"},
{RMS$_FNF, "%RMS-E-FNF, file not found"},
{RMS$_FNM, "%RMS-F-FNM, error in file name"},
{RMS$_IFI, "%RMS-F-IFI, invalid internal file identifier (IFI) value"},
{RMS$_NAM, "%RMS-F-NAM, invalid NAM block or NAM block not accessible"},
{RMS$_NMF, "%RMS-E-NMF, no more files found"},
{RMS$_RSS, "%RMS-F-RSS, invalid resultant string size"},
{RMS$_RSZ, "%RMS-F-RSZ, invalid record size"},
{RMS$_RTB, "%RMS-W-RTB, !UL byte record too large for user's buffer"},
{RMS$_WCC, "%RMS-E-WCC, invalid wild card context (WCC) value"},
{RMS$_WLD, "%RMS-F-WLD, invalid wildcard operation"},
{SS$_ABORT, "%SYSTEM-F-ABORT, abort"},
{SS$_BADFILENAME, "%SYSTEM-W-BADFILENAME, bad file name syntax"},
{SS$_BADIRECTORY, "%SYSTEM-W-BADIRECTORY, bad directory file format"},
{SS$_BADPARAM, "%SYSTEM-F-BADPARAM, bad parameter value"},
{SS$_BUGCHECK, "%SYSTEM-F-BUGCHECK, internal consistency failure"},
{SS$_DATACHECK, "%SYSTEM-F-DATACHECK, write check error"},
{SS$_DEVICEFULL, "%SYSTEM-W-DEVICEFULL, device full - allocation failure"},
{SS$_DEVMOUNT, "%SYSTEM-F-DEVMOUNT, device is already mounted"},
{SS$_DEVNOTALLOC, "%SYSTEM-W-DEVNOTALLOC, device not allocated"},
{SS$_DEVNOTDISM, "%SYSTEM-F-DEVNOTDISM, device not dismounted"},
{SS$_DEVNOTMOUNT, "%SYSTEM-F-DEVNOTMOUNT, device is not mounted"},
{SS$_DUPFILENAME, "%SYSTEM-W-DUPFILENAME, duplicate file name"},
{SS$_DUPLICATE, "%SYSTEM-F-DUPLNAM, duplicate name"},
{SS$_ENDOFFILE, "%SYSTEM-W-ENDOFFILE, end of file"},
{SS$_FILELOCKED, "%SYSTEM-W-FILELOCKED, file is deaccess locked"},
{SS$_FILESEQCHK, "%SYSTEM-W-FILESEQCHK, file identification sequence number check"},
{SS$_ILLEFC, "%SYSTEM-F-ILLEFC, illegal event flag cluster"},
{SS$_INSFMEM, "%SYSTEM-F-INSFMEM, insufficient dynamic memory"},
{SS$_ITEMNOTFOUND, "%SYSTEM-W-ITEMNOTFOUND, requested item cannot be returned"},
{SS$_IVCHAN, "%SYSTEM-F-IVCHAN, invalid I/O channel"},
{SS$_IVDEVNAM, "%SYSTEM-F-IVDEVNAM, invalid device name"},
{SS$_NOIOCHAN, "%SYSTEM-F-NOIOCHAN, no I/O channel available"},
{SS$_NOMOREFILES, "%SYSTEM-W-NOMOREFILES, no more files"},
{SS$_NORMAL, "%SYSTEM-S-NORMAL, normal successful completion"},
{SS$_NOSUCHDEV, "%SYSTEM-W-NOSUCHDEV, no such device available"},
{SS$_NOSUCHFILE, "%SYSTEM-W-NOSUCHFILE, no such file"},
{SS$_NOSUCHVOL, "%SYSTEM-E-NOSUCHVOL, No such volume"},
{SS$_NOTINSTALL, "%SYSTEM-F-NOTINSTALL, writable shareable images must be installed"},
{SS$_PARITY, "%SYSTEM-F-PARITY, parity error"},
{SS$_UNSUPVOLSET, "%SYSTEM-E-UNSUPVOLSET, Volume set not supported"},
{SS$_WASCLR, "%SYSTEM-S-NORMAL, normal successful completion"},
{SS$_WASSET, "%SYSTEM-S-WASSET, Event flag was set"},
{SS$_WRITLCK, "%SYSTEM-F-WRITLCK, write lock error"},
{0, NULL},
  };

const char *getmsg( unsigned int vmscode, unsigned int flags ) {
    const char fmt[] = "%%SYSTEM-E-NOSUCHMSG, Unknown message code %08X";
    static char buf[sizeof(fmt)+8+1];
    const char *txtp = NULL;

    for( mp =  vms2text; mp->text; mp++ ) {
        if( vmscode == mp-> code ) {
          txtp = mp->text;
          break;
        }
    }
    if( txtp == NULL ) {
        txtp = buf;
    }
    if( flags == MSG_TEXT ) {
        txtp = strchr( txtp, ',' );
        if( txtp == NULL ) abort();
        return txtp+2;
    }
    return txtp;
}

/*
#endif
*/
