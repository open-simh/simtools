/* PHYOS2.C   v1.2   Physical I/O module for OS2 */

/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com
 *
 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors.   This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
 */

/*  This version implemented to read from Floppy or CD by
 *  guessing that A: or B: will be a floppy and anything
 *  else is probably a CD?
 */

/* This module has not been compiled in quite some time, and
 * while some effort has been made to keep up with API changes,
 * it requires quite a bit of work to bring it up to date.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_NOPMAPI
#include <os2.h>
#include "ods2.h"
#include "phyio.h"
#include "ssdef.h"
#include "sysmsg.h"

static vmscond_t phyio_read( struct DEV *dev, uint32_t block, uint32_t length,
                             char *buffer );
static vmscond_t phyio_write( struct DEV *dev, uint32_t block, uint32_t length,
                              const char *buffer );

unsigned init_count = 0;
unsigned read_count = 0;
unsigned write_count = 0;

void phyio_show( showtype_t type ) {
    switch( type ) {
    case SHOW_STATS:
        printmsg( IO_STATS, MSG_TEXT, init_count, read_count, write_count );
        return;
    case SHOW_FILE64:
        printmsg( IO_NOLARGEFILE, MSG_TEXT );
        return;
    case SHOW_DEVICES:
        return;
   default:
        abort();
    }
}


#pragma pack(1)
/*------------------------------------------------*
 * Cat 0x80, Func 0x72: Read Long                 *
 *------------------------------------------------*/
struct ReadLong {
    ULONG ID_code;
    UCHAR address_mode;
    USHORT transfer_count;
    ULONG start_sector;
    UCHAR reserved;
    UCHAR interleave_size;
    UCHAR interleave_skip_factor;
};
#pragma pack()

#define SECTORSIZE 2048
struct ReadLong rl = {
    0x31304443L,    /* "CD01" */
    0,                 /* Address mode */
    1,                 /* Transfer count */
    0,                 /* Start Sector */
    0,                 /* Reserved */
    0,                 /* Interleave size */
    0/* Skip factor */
};

#define HANDLE_MAX 32
int hand_count = 0;
struct HANDLE {
    HFILE hand_hfile;
    int hand_drive;
} handle[HANDLE_MAX];

void phyio_help( void ) {
    (void) helptopic( 0, "MOUNT OS2" );

  return;
}

NOTE: This will not compile.  The API for phyio_init has changed to
taking a DEV struct.  I'm leaving it this way becauses I don't have the
ability to test under OS2.  This was left undone when the API was changed...
before my time. /TL


vmscond_t phyio_init(int devlen,char *devnam,unsigned *hand,struct phyio_info *info) {
    dev->devread = phyio_read;
    dev->devwrite = phyio_write;

    if (hand_count < HANDLE_MAX - 1) {
        ULONG usAction;
        ULONG open_mode;
        ULONG bCmd;
        ULONG stat;
        char device[40];
        memcpy(device,devnam,devlen);
        device[devlen] = '\0';
        handle[hand_count].hand_drive = toupper(*device);
        open_mode = OPEN_FLAGS_DASD | OPEN_SHARE_DENYREADWRITE
             | OPEN_FLAGS_WRITE_THROUGH | OPEN_FLAGS_FAIL_ON_ERROR
             | OPEN_FLAGS_NOINHERIT | OPEN_ACCESS_READWRITE;
        stat = DosOpen(device,  /* filename to open              */
                       &handle[hand_count].hand_hfile,
         /* address of file handle        */
                       &usAction,
         /* address to store action taken */
                       0L,
         /* size of new file              */
                       FILE_NORMAL,
         /* file's attribute              */
                       FILE_OPEN,
         /* action to take if file exists */
                       open_mode,
         /* file's open mode              */
                       0L);
        /* Reserved                      */
        if (stat) {
            switch (stat) {
                case 15:
                    printf("Drive %s is invalid.\n",device);
                    break;
                case 21:
                    printf("Drive %s is not ready.\n",device);
                    break;
                case 32:
                    printf("Drive %s is currently locked by another process.\n",device);
                    break;
                case 65:
                    printf("Drive %s is a network drive.\n",device);
                    break;
                default:
                    printf("error %d opening drive %s\n",stat,device);
            }
            return SS$_PARITY;
        }
        bCmd = 0;
        stat = DosDevIOCtl(handle[hand_count].hand_hfile,IOCTL_DISK,DSK_LOCKDRIVE,&bCmd,1,NULL,NULL,0,NULL);
        if (stat) {
            printf("Error %d locking drive\n",stat);
            return SS$_DEVNOTALLOC;
        }
        if (handle[hand_count].hand_drive > 'C') {
            info->status = PHYIO_READONLY;
        } else {
            info->status = 0;
        }
        *hand = hand_count++;
        init_count++;
        return 1;
    } else {
        return SS$_IVCHAN;
    }
}

static vmscond_t phy_getsect(HFILE hfile, uint32_t sector,char *buffer) {
    ULONG ulPinout,ulDinout;
    ULONG stat;
    char rawsect[SECTORSIZE + 304];
    ulPinout = sizeof(rl);
    ulDinout = sizeof(rawsect);
    rl.start_sector = sector;
    stat = DosDevIOCtl(hfile,
                       IOCTL_CDROMDISK,
                       CDROMDISK_READLONG,
                       &rl,sizeof(rl),&ulPinout,
                       rawsect,sizeof(rawsect),&ulDinout);
    if (stat) {
        printf("sys%04u: CDROMDISK_READLONG error\n",stat);
        return SS$_PARITY;
    }
    memcpy(buffer,rawsect + 16,SECTORSIZE);
    return 1;
}

static vmscond_t phyio_read(unsigned handno, uint32_t block, uint32_t length, char *buffer) {
    register vmscond_t sts = SS$_NORMAL;
#if DEBUG
    printf("PHYIO_READ block %d length %d\n",block,length);
#endif
    if (handno >= hand_count) {
        sts = SS$_IVCHAN;
    } else {
        if (handle[handno].hand_drive > 'C') {
            register unsigned sect = block * 512 / SECTORSIZE;
            register unsigned offset = (block - sect * SECTORSIZE / 512) * 512;
            while (length > 0) {
                register unsigned transfer;
                if (offset == 0 && length >= SECTORSIZE) {
                    transfer = SECTORSIZE;
                    if (((sts = phy_getsect(handle[handno].hand_hfile,sect,buffer)) & 1) == 0) break;
                } else {
                    char sector[SECTORSIZE];
                    if (((sts = phy_getsect(handle[handno].hand_hfile,sect,sector)) & 1) == 0) break;
                    transfer = SECTORSIZE - offset;
                    if (transfer > length) transfer = length;
                    memcpy(buffer,sector + offset,transfer);
                }
                buffer += transfer;
                length -= transfer;
                sect++;
                offset = 0;
            }
        } else {
            USHORT rc;
            ULONG oldpos,lenread;
            sts = SS$_PARITY;
            rc = DosSetFilePtr(handle[handno].hand_hfile,block * 512,0,&oldpos);
            if (rc == 0) {
                rc = DosRead(handle[handno].hand_hfile,buffer,length,&lenread);
                if (rc == 0 && lenread == length) sts = 1;
            }
        }
    }
    if( $SUCCESSFUL(sts) ) {
        read_count++;
    } else {
        printf("PHYOS2 Error %d Block %d Length %d\n",sts,block,length);
    }
    return sts;
}

static vmscond_t phyio_write(unsigned handle, uint32_t block, uint32_t length, char *buffer) {
    write_count++;
    return SS$_WRITLCK;
}
