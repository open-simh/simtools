/* Ssdef.h v1.3  System status definitions */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

#ifndef _SSDEF_H
#define _SSDEF_H

#define SYSTEM$_FACILITY 0

#define SS$_NORMAL 1
#define SS$_WASCLR 1
#define SS$_WASSET 9
#define SS$_BADPARAM 20
#define SS$_NOPRIV 36
#define SS$_ABORT 44
#define SS$_DATACHECK 92
#define SS$_DEVMOUNT 108
#define SS$_DEVNOTMOUNT 124
#define SS$_DEVOFFLINE 132
#define SS$_DUPLICATE 148
#define SS$_ILLBLKNUM 220
#define SS$_ILLEFC 236
#define SS$_INSFMEM 292
#define SS$_IVCHAN 316
#define SS$_IVDEVNAM 324
#define SS$_NOIOCHAN 436
#define SS$_PARITY 500
#define SS$_WRITLCK 604
#define SS$_BUGCHECK 676
#define SS$_DEVCMDERR 812
#define SS$_BADFILENAME 2072
#define SS$_BADIRECTORY 2088
#define SS$_DEVALLOC	2112
#define SS$_DEVASSIGN	2120
#define SS$_DEVICEFULL 2128
#define SS$_DEVNOTALLOC 2136
#define SS$_DUPFILENAME 2152
#define SS$_ENDOFFILE 2160
#define SS$_FILELOCKED 2216
#define SS$_FILESEQCHK 2232
#define SS$_NOSUCHDEV 2312
#define SS$_NOSUCHFILE 2320
#define SS$_NOMOREFILES 2352
#define SS$_ITEMNOTFOUND 2640
#define SS$_NOMOREDEV 2648
#define SS$_NOSUCHVOL 3882
#define SS$_NOTINSTALL 8212
#define SS$_DEVNOTDISM 8628
#define SS$_UNSUPVOLSET 9908
#define SS$_OFFSET_TOO_BIG 10036

#endif /* #ifndef _SSDEF_H */
