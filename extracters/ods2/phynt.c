/* PHYNT.C  v1.2-2    Physical I/O module for Windows NT */

/* W95 code now included with code to automatically determine
   if we are running under NT...

   W98 seems to have a performance problem where the
   INT25 call are VERY slow!!! */



/* This version built and tested under Visual C++ 6.0 and LCC.
   To support CD drives this version requires ASPI run-time
   support.. Windows 95/98 come with ASPI but NT/2000 require
   that a version of ASPI be install and started before running
   this program!!   (One verion is NTASPI available from
   www.symbios.com)  */


#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winioctl.h>
#include <string.h>
#include <ctype.h>
#include <memory.h>

#include "ssdef.h"
#include "compat.h"


/*
        To read from CD ASPI run-time support is required (wnaspi32.dll).
        NT does not come with ASPI support so you need to install it first.
        (ASPI is also required for things like Digital Audio Extraction
        and was origionally loaded on my machine to make MP3 audio files.)
        One downside is that ASPI on NT does not have a way to match SCSI
        adapters and units to a system device. So this program works by
        finding the first CD like unit on the system and using it - that
        may NOT be what you want if you have several CD drives! And finally
        there are CD drives and there are CD drives. From what I can tell
        many simply won't work with this (or other!) code. Naturally if
        you find a better way please let me know... Paulnank@au1.ibm.com
*/

#ifdef USE_WNASPI32
#include "wnaspi32.h"
#include "scsidefs.h"

unsigned ASPI_status = 0;       /* Temporary status indicator */
unsigned ASPI_HaStat = 0;       /* Temporary status indicator */
unsigned ASPI_TargStat = 0;     /* Temporary status indicator */

HANDLE ASPI_event;              /* Windows event for ASPI wait */
SRB_ExecSCSICmd ASPI_srb;       /* SRB buffer for ASPI commands */

unsigned aspi_execute(short bus,short id,BYTE Flags,DWORD BufLen,
                      BYTE *BufPointer,BYTE CDBLen,BYTE CDBByte)
{
    ASPI_srb.SRB_Cmd = SC_EXEC_SCSI_CMD;
    ASPI_srb.SRB_Status = SS_PENDING;
    ASPI_srb.SRB_HaId = bus;
    ASPI_srb.SRB_Flags = Flags | SRB_EVENT_NOTIFY;
    ASPI_srb.SRB_Hdr_Rsvd = 0;
    ASPI_srb.SRB_Target = id;
    ASPI_srb.SRB_Lun = 0;
    ASPI_srb.SRB_Rsvd1 = 0;
    ASPI_srb.SRB_BufLen = BufLen;
    ASPI_srb.SRB_BufPointer = BufPointer;
    ASPI_srb.SRB_SenseLen = SENSE_LEN;
    ASPI_srb.SRB_CDBLen = CDBLen;
    ASPI_srb.SRB_HaStat = 0;
    ASPI_srb.SRB_TargStat = 0;
    ASPI_srb.SRB_PostProc = ASPI_event;
    ASPI_srb.SRB_Rsvd2 = NULL;
    memset(ASPI_srb.SRB_Rsvd3,0,sizeof(ASPI_srb.SRB_Rsvd3));
    ASPI_srb.CDBByte[0] = CDBByte;
    SendASPI32Command(&ASPI_srb);       /* Perform the command... */
    while (ASPI_srb.SRB_Status == SS_PENDING)
        WaitForSingleObject(ASPI_event,INFINITE);
    ResetEvent(ASPI_event);
    ASPI_status = ASPI_srb.SRB_Status;
    ASPI_HaStat = ASPI_srb.SRB_HaStat;
    ASPI_TargStat = ASPI_srb.SRB_TargStat;
    if (ASPI_srb.SRB_Status != SS_COMP) return SS$_PARITY;
    return 1;
}


/* Read a sector using ASPI... */

unsigned aspi_read(short bus,short id,unsigned sector,unsigned sectorsize,char *buffer)
{

    ASPI_srb.CDBByte[1] = 0;
    ASPI_srb.CDBByte[2] = (sector >> 24);
    ASPI_srb.CDBByte[3] = (sector >> 16) & 0xff;
    ASPI_srb.CDBByte[4] = (sector >> 8) & 0xff;
    ASPI_srb.CDBByte[5] = sector & 0xff;
    ASPI_srb.CDBByte[6] = 0;
    ASPI_srb.CDBByte[7] = 0;
    ASPI_srb.CDBByte[8] = 1;    /* Read a single sector */
    ASPI_srb.CDBByte[9] = 0;
    return aspi_execute(bus,id,SRB_DIR_IN,sectorsize,buffer,10,SCSI_READ10);
}


/* Write a sector using ASPI... */

unsigned aspi_write(short bus,short id,unsigned sector,unsigned sectorsize,char *buffer)
{

    ASPI_srb.CDBByte[1] = 0;
    ASPI_srb.CDBByte[2] = (sector >> 24);
    ASPI_srb.CDBByte[3] = (sector >> 16) & 0xff;
    ASPI_srb.CDBByte[4] = (sector >> 8) & 0xff;
    ASPI_srb.CDBByte[5] = sector & 0xff;
    ASPI_srb.CDBByte[6] = 0;
    ASPI_srb.CDBByte[7] = 0;
    ASPI_srb.CDBByte[8] = 1;    /* Read a single sector */
    ASPI_srb.CDBByte[9] = 0;
    return aspi_execute(bus,id,SRB_DIR_OUT,sectorsize,buffer,10,SCSI_WRITE10);
}


/* Routine to identify a device found by ASPI */

unsigned aspi_identify(short bus,short id,unsigned *sectsize)
{
    struct SCSI_Inquiry {
        unsigned char device;
        unsigned char dev_qual2;
        unsigned char version;
        unsigned char response_format;
        unsigned char additional_length;
        unsigned char unused[2];
        unsigned char flags;
        char vendor[8];
        char product[16];
        char revision[4];
        unsigned char extra[60];
    } InquiryBuffer;
    unsigned sts;
    ASPI_srb.CDBByte[1] = 0;    /* SCSI Inquiry packet */
    ASPI_srb.CDBByte[2] = 0;
    ASPI_srb.CDBByte[3] = 0;
    ASPI_srb.CDBByte[4] = sizeof(InquiryBuffer);
    ASPI_srb.CDBByte[5] = 0;
    sts = aspi_execute(bus,id,SRB_DIR_IN,sizeof(InquiryBuffer),(BYTE *)&InquiryBuffer,6,SCSI_INQUIRY);
    if (sts & 1) {
        ASPI_srb.CDBByte[1] = 0;/* SCSI TEST if ready packet */
        ASPI_srb.CDBByte[2] = 0;
        ASPI_srb.CDBByte[3] = 0;
        ASPI_srb.CDBByte[4] = 0;
        ASPI_srb.CDBByte[5] = 0;
        sts = aspi_execute(bus,id,0,0,NULL,6,SCSI_TST_U_RDY);
        if ((sts & 1) == 0) {
            ASPI_srb.CDBByte[1] = 0;    /* SCSI START STOP packet */
            ASPI_srb.CDBByte[2] = 0;
            ASPI_srb.CDBByte[3] = 0;
            ASPI_srb.CDBByte[4] = 1;    /* Start unit */
            ASPI_srb.CDBByte[5] = 0;
            sts = aspi_execute(bus,id,0,0,NULL,6,SCSI_START_STP);
        }
        if (sts & 1) {
            struct {
                unsigned char blocks[4];
                unsigned char sectsz[4];
            } CapBuffer;
            memset(ASPI_srb.CDBByte,0,sizeof(ASPI_srb.CDBByte));
            sts = aspi_execute(bus,id,SRB_DIR_IN,sizeof(CapBuffer),(BYTE *)&CapBuffer,10,SCSI_RD_CAPAC);
            if (sts & 1) {
                *sectsize = (CapBuffer.sectsz[0] << 24) | (CapBuffer.sectsz[1] << 16) |
                      (CapBuffer.sectsz[2] << 8) | CapBuffer.sectsz[3];
            }
        }
        printf("ASPI (Bus %d,ID %d) %8.8s %16.16s %4.4s %s %d\n",bus,id,
               InquiryBuffer.vendor,InquiryBuffer.product,InquiryBuffer.revision,
               (InquiryBuffer.dev_qual2 & 0x80) ? "Removable" : "Fixed",*sectsize);

    }
    if ((sts & 1) == 0) {
        int i;
        printf("ASPI Error sense %x %x %x - ",ASPI_status,ASPI_HaStat,ASPI_TargStat);
        for (i = 0; i < SENSE_LEN; i++) printf(" %x",ASPI_srb.SenseArea[i]);
        printf("\n");
    }
    return sts;
}



unsigned ASPI_adapters = 0;     /* Count of ASPI adapters */
unsigned ASPI_devices = 0;      /* Count of ASPI devices we have accessed */

/* Get ASPI ready and locate the next ASPI drive... */

unsigned aspi_initialize(short *dev_type,short *dev_bus,short *dev_id,unsigned *sectsize)
{
    short aspi_bus,aspi_id;
    unsigned aspi_devcount = 0;
    DWORD ASPIStatus;
    SRB_GDEVBlock Ssrb;
    if (ASPI_adapters == 0) {
        ASPIStatus = GetASPI32SupportInfo();
        ASPI_status = ASPIStatus;
        if (HIBYTE(LOWORD(ASPIStatus)) != SS_COMP) {
            printf("Could not initialize ASPI (%x)\n",ASPIStatus);
            printf("Please check that ASPI is installed and started properly\n");
            return 8;
        } else {
            ASPI_adapters = LOWORD(LOBYTE(ASPIStatus));
            if ((ASPI_event = CreateEvent(NULL,FALSE,FALSE,NULL)) == NULL) return 8;
        }
    }
    Ssrb.SRB_Cmd = SC_GET_DEV_TYPE;
    Ssrb.SRB_HaId = 0;
    Ssrb.SRB_Flags = SRB_EVENT_NOTIFY;
    Ssrb.SRB_Hdr_Rsvd = 0;
    Ssrb.SRB_Lun = 0;
    for (aspi_bus = 0; aspi_bus < ASPI_adapters; aspi_bus++) {
        for (aspi_id = 0; aspi_id <= 7; aspi_id++) {
            Ssrb.SRB_HaId = aspi_bus;
            Ssrb.SRB_Target = aspi_id;
            ASPIStatus = SendASPI32Command(&Ssrb);
            ASPI_status = ASPIStatus;
            if (ASPIStatus == SS_COMP) {
                if (Ssrb.SRB_DeviceType == DTYPE_CROM || Ssrb.SRB_DeviceType == DTYPE_OPTI) {
                    if (++aspi_devcount > ASPI_devices) {
                        unsigned sts = aspi_identify(aspi_bus,aspi_id,sectsize);
                        *dev_type = Ssrb.SRB_DeviceType;
                        *dev_bus = aspi_bus;
                        *dev_id = aspi_id;
                        ASPI_devices++;
                        return 1;
                    }
                }
            }
        }
    }
    printf("Could not find suitable ASPI device\n");
    return 8;
}
#else
unsigned aspi_read(short bus,short id,unsigned sector,unsigned sectorsize,char *buffer)
{
    fprintf( stderr, "wnaspi32 support not compiled in this version\n" );
    exit(1);
}
unsigned aspi_write(short bus,short id,unsigned sector,unsigned sectorsize,char *buffer)
{
    fprintf( stderr, "wnaspi32 support not compiled in this version\n" );
    exit(1);
}
unsigned aspi_initialize(short *dev_type,short *dev_bus,short *dev_id,unsigned *sectsize)
{
    fprintf( stderr, "wnaspi32 support not compiled in this version\n" );
    exit(1);
}
unsigned int ASPI_status = 0,ASPI_HaStat = 0,ASPI_TargStat = 0;

#endif


/* Some NT definitions... */

/* #include "Ntddcdrm.h" */
#define IOCTL_CDROM_BASE         FILE_DEVICE_CD_ROM
#define IOCTL_CDROM_RAW_READ     CTL_CODE(IOCTL_CDROM_BASE, 0x000F, METHOD_OUT_DIRECT,  FILE_READ_ACCESS)
#define IOCTL_CDROM_GET_DRIVE_GEOMETRY CTL_CODE(IOCTL_CDROM_BASE, 0x0013, METHOD_BUFFERED, FILE_READ_ACCESS)

/* NT Get disk or CD geometry... */

BOOL
    GetDiskGeometry(
                    HANDLE hDisk,
                    DISK_GEOMETRY *dGeometry
    )
{
    DWORD ReturnedByteCount;
    BOOL results = DeviceIoControl(
                                   hDisk,
                                   IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                   NULL,
                                   0,
                                   dGeometry,
                                   sizeof(*dGeometry),
                                   &ReturnedByteCount,
                                   NULL
                );
    if (!results) results = DeviceIoControl(
                                            hDisk,
                                            IOCTL_CDROM_GET_DRIVE_GEOMETRY,
                                            NULL,
                                            0,
                                            dGeometry,
                                            sizeof(*dGeometry),
                                            &ReturnedByteCount,
                                            NULL
                                    );
    return results;
}


/* NT drive lock - we don't want any interference... */

BOOL
    LockVolume(
               HANDLE hDisk
    )
{
    DWORD ReturnedByteCount;

    return DeviceIoControl(
                           hDisk,
                           FSCTL_LOCK_VOLUME,
                           NULL,
                           0,
                           NULL,
                           0,
                           &ReturnedByteCount,
                           NULL
                );
}



/* Windows 95 I/O definitions... */

#define VWIN32_DIOC_DOS_IOCTL 1
#define VWIN32_DIOC_DOS_INT25 2
#define VWIN32_DIOC_DOS_INT26 3

#pragma pack(1)
typedef struct _DIOC_REGISTERS {
    DWORD reg_EBX;
    DWORD reg_EDX;
    DWORD reg_ECX;
    DWORD reg_EAX;
    DWORD reg_EDI;
    DWORD reg_ESI;
    DWORD reg_Flags;
} DIOC_REGISTERS;

typedef struct _DPB {
    unsigned int dpb_sector;
    unsigned short dpb_count;
    char *dpb_buffer;
} DPB;

typedef struct _DEVICEPARAM {
    char junk1[7];
    unsigned short sec_size;
    char junk2[23];
} DEVICEPARAM;

#pragma pack()



/* This routine figures out whether this is an NT system or not... */

unsigned is_NT = 2;
OSVERSIONINFO sysver;

void getsysversion(void)
{
    memset(&sysver,0,sizeof(OSVERSIONINFO));
    sysver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&sysver);
    if (sysver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        is_NT = 1;
    } else {
        is_NT = 0;
    }
}

/* Each device we talk to has a channel entry so that we can
   remember its details... */

#define CHAN_MAX 32
unsigned chan_count = 0;
struct CHANTAB {
    HANDLE handle;              /* File handle for device */
    char *IoBuffer;             /* Pointer to a buffer for the device */
    unsigned sectorsize;        /* Device sector size */
    unsigned last_sector;       /* Last sector no read (still in buffer) */
    short device_status;        /* Device status bits */
    short device_name;          /* Drive letter (A, B, C, ...) */
    short device_dtype;         /* Type of disk... */
    short device_bus;           /* ASPI device bus */
    short device_id;            /* ASPI device id */
} chantab[CHAN_MAX];



/* Read a physical sector... */

unsigned phy_getsect(unsigned chan,unsigned sector)
{
    register unsigned sts = 1;
    if (sector != chantab[chan].last_sector) {
        if (chantab[chan].device_dtype >= 0) {
            sts = aspi_read(chantab[chan].device_bus,chantab[chan].device_id,sector,
                            chantab[chan].sectorsize,chantab[chan].IoBuffer);
        } else {
            if (is_NT) {
                DWORD BytesRead = -1;   /* NT Bytes read */
                SetFilePointer(chantab[chan].handle,
                               sector * chantab[chan].sectorsize,0,FILE_BEGIN);
                if (!ReadFile(chantab[chan].handle,chantab[chan].IoBuffer,
                              chantab[chan].sectorsize,&BytesRead,NULL)) sts = SS$_PARITY;
            } else {
                DIOC_REGISTERS reg;     /* W95 DIOC registers */
                DPB dpb;
                BOOL fResult;
                DWORD cb = 0;
                dpb.dpb_sector = sector;        /* sector number */
                dpb.dpb_count = 1;      /* sector count */
                dpb.dpb_buffer = chantab[chan].IoBuffer;        /* sector buffer */
                reg.reg_EAX = chantab[chan].device_name - 'A';  /* drive           */
                reg.reg_EBX = (DWORD) & dpb;    /* parameter block */
                reg.reg_ECX = -1;       /* use dpb    */
                reg.reg_EDX = 0;/* sector num      */
                reg.reg_EDI = 0;
                reg.reg_ESI = 0;
                reg.reg_Flags = 0x0001; /* set carry flag  */
                fResult = DeviceIoControl(chantab[chan].handle,
                                          VWIN32_DIOC_DOS_INT25,
                                          &reg,sizeof(reg),
                                          &reg,sizeof(reg),
                                          &cb,0);
                if (!fResult || (reg.reg_Flags & 0x0001)) {
                    TCHAR *msg = w32_errstr(0);
                    printf("Sector %d read failed %s\n",sector,msg);
                    LocalFree(msg);
                    sts = SS$_PARITY;
                }
            }
        }
    }
    if (sts & 1) {
        chantab[chan].last_sector = sector;
    }
    return sts;
}


/* Write a physical sector... */

unsigned phy_putsect(unsigned chan,unsigned sector)
{
    register unsigned sts = 1;
    chantab[chan].last_sector = sector;
    if (chantab[chan].device_dtype >= 0) {
        sts = aspi_write(chantab[chan].device_bus,chantab[chan].device_id,sector,
                         chantab[chan].sectorsize,chantab[chan].IoBuffer);
    } else {
        if (is_NT) {
            DWORD BytesRead = -1;       /* NT Bytes written */
            SetFilePointer(chantab[chan].handle,
                           sector * chantab[chan].sectorsize,0,FILE_BEGIN);
            if (!WriteFile(chantab[chan].handle,chantab[chan].IoBuffer,
                           chantab[chan].sectorsize,&BytesRead,NULL)) sts = SS$_PARITY;
        } else {
            sts = SS$_WRITLCK;  /* Not implemented yet!! */
        }
    }
    return sts;
}

#include "phyio.h"


unsigned init_count = 0;        /* Some counters so we can report */
unsigned read_count = 0;        /* How often we get called */
unsigned write_count = 0;

void phyio_show(void)
{
    printf("PHYIO_SHOW Initializations: %d Reads: %d Writes: %d\n",
           init_count,read_count,write_count);
}


/* Initialize device by opening it, locking it and getting it ready.. */

void phyio_help(FILE *fp ) {
    fprintf( fp, "Specify the device to be mounted as a drive letter.\n" );
    fprintf( fp, "E.g. mount D:\n" );
#ifdef USE_WNASPI32
    fprintf( fp, "If the drive letter is C: or higher, commands are\n" );
    fprintf( fp, "sent directly to the drive, bypassing system drivers.\n" );
    fprintf( fp, "The drive must be a SCSI device\n" );
#endif
    fprintf( fp, "The drive letter must be between A: and Z:.\n" );
    fprintf( fp, "The drive is accessed as a physical device, which may require privileges.\n" );
    fprintf( fp, "Use show DEVICES for a list of available devices.\n" );
    fprintf( fp, "Use ODS2-Image to work with disk images such as .ISO or simulator files.\n" );
    return;
}

unsigned phyio_init(int devlen,char *devnam,unsigned *chanptr,struct phyio_info *info)
{
    unsigned sts = 1;
    unsigned chan = chan_count;
    if (chan >= CHAN_MAX - 1)
        return SS$_IVCHAN;

    if( !(devlen == 2 &&
          toupper(*devnam) >= 'A' && toupper(*devnam) <= 'Z' && *(devnam + 1) == ':') )
          return SS$_NOSUCHDEV;

    chantab[chan].device_status = 0;
    chantab[chan].device_name = toupper(*devnam);
    chantab[chan].device_dtype = -1;

    do {
        HANDLE hDrive;

#ifdef USE_WNASPI32
        /* Use ASPI for devices past C... */

        if (toupper(*devnam) > 'C') {
            sts = aspi_initialize(&chantab[chan].device_dtype,
                                    &chantab[chan].device_bus,&chantab[chan].device_id,
                                    &chantab[chan].sectorsize);
            if ((sts & 1) == 0) return sts;
            break;
        }
#endif
        if (is_NT > 1) getsysversion();

        /* NT stuff */

        if (is_NT) {
            char ntname[20];
            DISK_GEOMETRY Geometry;
            snprintf(ntname,sizeof(ntname),"\\\\.\\%s",devnam);
            chantab[chan].handle = hDrive = CreateFile(ntname,
                                                        GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                        NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
            if (hDrive == INVALID_HANDLE_VALUE) {
                TCHAR *msg = w32_errstr(0);
                printf("Open %s failed %s\n",devnam,msg);
                LocalFree(msg);
                return SS$_NOSUCHDEV;
            }
            if (LockVolume(hDrive) == FALSE) {
                TCHAR *msg = w32_errstr(0);
                printf("LockVolume %s failed %s\n",devnam,msg);
                LocalFree(msg);
                return 72;
            }
            if (!GetDiskGeometry(hDrive,&Geometry)) {
                TCHAR *msg = w32_errstr(0);
                printf("GetDiskGeometry %s failed %s\n",devnam,msg);
                LocalFree(msg);
                return 80;
            }
            chantab[chan].sectorsize = Geometry.BytesPerSector;
            info->sectors = Geometry.Cylinders.QuadPart * Geometry.TracksPerCylinder *
                        Geometry.SectorsPerTrack;
            break;
        }

        /* W95 stuff */

        {
            DIOC_REGISTERS reg;
            DEVICEPARAM deviceparam;
            BOOL fResult;
            DWORD cb;
            chantab[chan].handle = hDrive = CreateFile("\\\\.\\vwin32",
                                                        0,0,NULL,0,FILE_FLAG_DELETE_ON_CLOSE,NULL);
            if (hDrive == INVALID_HANDLE_VALUE) {
                TCHAR *msg = w32_errstr(0);
                printf("Open %s failed %s\n",devnam,msg);
                LocalFree(msg);
                return SS$_NOSUCHDEV;
            }
            reg.reg_EAX = 0x440d;
            reg.reg_EBX = chantab[chan].device_name - 'A' + 1;
            reg.reg_ECX = 0x084a;   /* Lock volume */
            reg.reg_EDX = 0;
            reg.reg_Flags = 0x0000; /* Permission  */

            fResult = DeviceIoControl(hDrive,VWIN32_DIOC_DOS_IOCTL,
                                        &reg,sizeof(reg),&reg,sizeof(reg),&cb,0);

            if (!fResult || (reg.reg_Flags & 0x0001)) {
                TCHAR *msg = w32_errstr(0);
                printf("Volume lock failed (%s)\n",msg);
                LocalFree(msg);
                return SS$_DEVNOTALLOC;
            }
            reg.reg_EAX = 0x440d;
            reg.reg_EBX = chantab[chan].device_name - 'A' + 1;
            reg.reg_ECX = 0x0860;   /* Get device parameters */
            reg.reg_EDX = (DWORD) & deviceparam;
            reg.reg_Flags = 0x0001; /* set carry flag  */

            fResult = DeviceIoControl(hDrive,
                                        VWIN32_DIOC_DOS_IOCTL,
                                        &reg,sizeof(reg),
                                        &reg,sizeof(reg),
                                        &cb,0);

            if (!fResult || (reg.reg_Flags & 0x0001)) {
                TCHAR *msg = w32_errstr(0);
                printf("Volume get parameters failed (%s)\n",msg);
                LocalFree(msg);
                return 8;
            }
            chantab[chan].sectorsize = deviceparam.sec_size;
            info->sectors = 0;
            break;
        }
    } while(0);

    chantab[chan].IoBuffer = VirtualAlloc(NULL,chantab[chan].sectorsize,
                                            MEM_COMMIT,PAGE_READWRITE);
    chantab[chan].last_sector = ~0U;
    *chanptr = chan_count++;
    info->status = 0;
    info->sectorsize = chantab[chan].sectorsize;
    init_count++;
    return 1;
}




/* Handle a read request ... need to read the approriate sectors to
   complete the request... */

unsigned phyio_read(unsigned chan,unsigned block,unsigned length,char *buffer)
{
    register unsigned sts = 1;

    if (chan < chan_count) {
		register unsigned sectblks = chantab[chan].sectorsize / 512;
        register unsigned sectno = block / sectblks;
		register unsigned offset = block % sectblks;
        char *sectbuff = chantab[chan].IoBuffer;
        while (length > 0) {
            register unsigned transfer;
            transfer = (sectblks - offset) * 512;
            if (transfer > length) transfer = length;
            if (((sts = phy_getsect(chan,sectno)) & 1) == 0) break;
            memcpy(buffer,sectbuff + (offset * 512),transfer);
            buffer += transfer;
            length -= transfer;
            sectno++;
            offset = 0;
        }
        if (sts & 1) {
            read_count++;
        } else {
            printf("PHYIO Error %d Block %d Length %d (ASPI: %x %x %x)\n",
                   sts,block,length,ASPI_status,ASPI_HaStat,ASPI_TargStat);
        }

    } else {
        sts = SS$_IVCHAN;
    }
    return sts;
}

/* Handle a write request ... need to read the approriate sectors to
   complete the request... */

unsigned phyio_write(unsigned chan,unsigned block,unsigned length,char *buffer)
{
    register unsigned sts = 1;

    if (chan < chan_count) {
		register unsigned sectblks = chantab[chan].sectorsize / 512;
        register unsigned sectno = block / sectblks;
		register unsigned offset = block % sectblks;
        char *sectbuff = chantab[chan].IoBuffer;
        while (length > 0) {
            register unsigned transfer;
            transfer = (sectblks - offset) * 512;
            if (transfer > length) transfer = length;
            if (transfer != sectblks * 512) {
                if (((sts = phy_getsect(chan,sectno)) & 1) == 0) break;
            }
            memcpy(sectbuff + (offset * 512),buffer,transfer);
            if (((sts = phy_putsect(chan,sectno)) & 1) == 0) break;
            buffer += transfer;
            length -= transfer;
            sectno++;
            offset = 0;
        }
        if (sts & 1) {
            write_count++;
        } else {
            printf("PHYIO Error %d Block %d Length %d (ASPI: %x %x %x)\n",
                   sts,block,length,ASPI_status,ASPI_HaStat,ASPI_TargStat);
        }

    } else {
        sts = SS$_IVCHAN;
    }
    return sts;
}
