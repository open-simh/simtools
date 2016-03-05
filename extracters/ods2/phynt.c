/* PHYNT.C  V2.1    Physical I/O module for Windows NT */

/* Win9X code now included with code to automatically determine
   if we are running under WinNT...

   W98 seems to have a performance problem where the
   INT25 call are VERY slow!!! */



/* This version built and tested under Visual C++ 6.0 and LCC.
   To support CD drives this version requires ASPI run-time
   support.. Windows 95/98 come with ASPI but NT/2000 require
   that a version of ASPI be install and started before running
   this program!!   (One verion is NTASPI available from
   www.symbios.com)

  Note: As of 2016, www.symbios.com is a financial site.  You
  may be able to obtain ASPI from someplace else, but the code
  to use it may be stale.  On more recent windows, you can simply
  use the NT drive letter, which will do physical I/O to the device.
  */


#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <winioctl.h>
#include <shlwapi.h>            /* PathSearchAndQualify() */

#include "ods2.h"
#include "device.h"
#include "phyio.h"
#include "ssdef.h"
#include "stsdef.h"
#include "compat.h"
#include "phyvirt.h"

#ifdef USE_ASPI
#include "scsidefs.h"
#include "wnaspi32.h"
#endif

/* Some NT definitions... */

/* #include "Ntddcdrm.h" */
#define IOCTL_CDROM_BASE         FILE_DEVICE_CD_ROM
#define IOCTL_CDROM_RAW_READ     CTL_CODE(IOCTL_CDROM_BASE, 0x000F, \
                                          METHOD_OUT_DIRECT, FILE_READ_ACCESS)
#define IOCTL_CDROM_GET_DRIVE_GEOMETRY \
                                 CTL_CODE(IOCTL_CDROM_BASE, 0x0013, \
                                          METHOD_BUFFERED,   FILE_READ_ACCESS)

/* Windows 9X I/O definitions... */
#define VWIN32_DIOC_DOS_IOCTL     1
#define VWIN32_DIOC_DOS_DRIVEINFO 6

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

typedef struct _DISKIO {
    DWORD  dwStartSector;   /* starting logical sector number */
    WORD   wSectors;        /* number of sectors              */
    DWORD  dwBuffer;        /* address of read/write buffer   */
} DISKIO, *PDISKIO;

#pragma pack()

static unsigned init_count = 0;        /* Some counters so we can report */
static unsigned read_count = 0;        /* How often we get called */
static unsigned write_count = 0;

static unsigned is_NT = 2;

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

#ifdef USE_ASPI
#define DEV_USES_ASPI( dev ) ( dev->drive > 2 )
#else
#define DEV_USES_ASPI( dev ) FALSE
#endif

#ifdef USE_ASPI
static unsigned ASPI_adapters = 0;     /* Count of ASPI adapters */
static unsigned ASPI_devices = 0;      /* Count of ASPI devices accessed */

static unsigned ASPI_status = 0;       /* Temporary status indicator */
static unsigned ASPI_HaStat = 0;       /* Temporary status indicator */
static unsigned ASPI_TargStat = 0;     /* Temporary status indicator */

static HANDLE ASPI_event;              /* Windows event for ASPI wait */
static SRB_ExecSCSICmd ASPI_srb;       /* SRB buffer for ASPI commands */

static unsigned aspi_execute( short bus, short id, BYTE Flags, DWORD BufLen,
                              BYTE *BufPointer, BYTE CDBLen, BYTE CDBByte );
static unsigned aspi_read( short bus, short id, unsigned sector,
                           unsigned bytespersector, char *buffer );
static unsigned aspi_write( short bus, short id, unsigned sector,
                            unsigned bytespersector, char *buffer );
static unsigned aspi_identify( short bus, short id, unsigned *sectors,
                               unsigned *bytespersector );
static unsigned aspi_initialize( short *dev_type, short *dev_bus,
                                 short *dev_id );
#endif

static BOOL GetDiskGeometry( struct DEV *dev, unsigned *sectors,
                             unsigned *bytespersector );

static BOOL LockVolume( struct DEV *dev );
static BOOL UnlockVolume( struct DEV *dev );
static void getsysversion( void );
static unsigned phy_getsect( struct DEV *dev, unsigned sector );
static unsigned phy_putsect( struct DEV *dev, unsigned sector );


#ifdef USE_ASPI
/************************************************************* aspi_execute() */

static unsigned aspi_execute( short bus, short id, BYTE Flags, DWORD BufLen,
                              BYTE *BufPointer, BYTE CDBLen, BYTE CDBByte ) {

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
    memset( ASPI_srb.SRB_Rsvd3, 0 ,sizeof( ASPI_srb.SRB_Rsvd3 ) );
    ASPI_srb.CDBByte[0] = CDBByte;
    SendASPI32Command( &ASPI_srb );       /* Perform the command... */
    while ( ASPI_srb.SRB_Status == SS_PENDING ) {
        WaitForSingleObject( ASPI_event, INFINITE );
    }
    ResetEvent( ASPI_event );
    ASPI_status = ASPI_srb.SRB_Status;
    ASPI_HaStat = ASPI_srb.SRB_HaStat;
    ASPI_TargStat = ASPI_srb.SRB_TargStat;
    if ( ASPI_srb.SRB_Status != SS_COMP ) {
        return SS$_PARITY;
    }
    return SS$_NORMAL;
}

/**************************************************************** aspi_read() */

/* Read a sector using ASPI... */

static unsigned aspi_read( short bus, short id, unsigned sector,
                           unsigned bytespersector, char *buffer ) {

    ASPI_srb.CDBByte[1] = 0;
    ASPI_srb.CDBByte[2] = ( sector >> 24 )       ;
    ASPI_srb.CDBByte[3] = ( sector >> 16 ) & 0xff;
    ASPI_srb.CDBByte[4] = ( sector >>  8 ) & 0xff;
    ASPI_srb.CDBByte[5] =   sector         & 0xff;
    ASPI_srb.CDBByte[6] = 0;
    ASPI_srb.CDBByte[7] = 0;
    ASPI_srb.CDBByte[8] = 1;    /* Read a single sector */
    ASPI_srb.CDBByte[9] = 0;
    return aspi_execute( bus, id, SRB_DIR_IN, bytespersector, buffer, 10,
                         SCSI_READ10 );
}

/*************************************************************** aspi_write() */

/* Write a sector using ASPI... */

static unsigned aspi_write( short bus, short id, unsigned sector,
                            unsigned bytespersector, char *buffer ) {

    ASPI_srb.CDBByte[1] = 0;
    ASPI_srb.CDBByte[2] = ( sector >> 24 )       ;
    ASPI_srb.CDBByte[3] = ( sector >> 16 ) & 0xff;
    ASPI_srb.CDBByte[4] = ( sector >>  8 ) & 0xff;
    ASPI_srb.CDBByte[5] =   sector         & 0xff;
    ASPI_srb.CDBByte[6] = 0;
    ASPI_srb.CDBByte[7] = 0;
    ASPI_srb.CDBByte[8] = 1;    /* Write a single sector */
    ASPI_srb.CDBByte[9] = 0;
    return aspi_execute( bus, id, SRB_DIR_OUT, bytespersector, buffer, 10,
                         SCSI_WRITE10 );
}

/************************************************************ aspi_identify() */

/* Routine to identify a device found by ASPI */

static unsigned aspi_identify( short bus, short id, unsigned *sectors,
                               unsigned *bytespersector ) {

    unsigned sts;
    struct SCSI_Inquiry {
        unsigned char device;
        unsigned char dev_qual2;
        unsigned char version;
        unsigned char response_format;
        unsigned char additional_length;
        unsigned char unused[2];
        unsigned char flags;
        char          vendor[8];
        char          product[16];
        char          revision[4];
        unsigned char extra[60];
    } InquiryBuffer;

    ASPI_srb.CDBByte[1] = 0;    /* SCSI Inquiry packet */
    ASPI_srb.CDBByte[2] = 0;
    ASPI_srb.CDBByte[3] = 0;
    ASPI_srb.CDBByte[4] = sizeof( InquiryBuffer );
    ASPI_srb.CDBByte[5] = 0;
    sts = aspi_execute( bus, id, SRB_DIR_IN, sizeof( InquiryBuffer ),
                        (BYTE *) &InquiryBuffer, 6, SCSI_INQUIRY );
    if ( sts & STS$M_SUCCESS ) {
        ASPI_srb.CDBByte[1] = 0;/* SCSI TEST if ready packet */
        ASPI_srb.CDBByte[2] = 0;
        ASPI_srb.CDBByte[3] = 0;
        ASPI_srb.CDBByte[4] = 0;
        ASPI_srb.CDBByte[5] = 0;
        sts = aspi_execute( bus, id, 0, 0, NULL, 6, SCSI_TST_U_RDY );
        if ( !( sts & STS$M_SUCCESS ) ) {
            ASPI_srb.CDBByte[1] = 0;    /* SCSI START STOP packet */
            ASPI_srb.CDBByte[2] = 0;
            ASPI_srb.CDBByte[3] = 0;
            ASPI_srb.CDBByte[4] = 1;    /* Start unit */
            ASPI_srb.CDBByte[5] = 0;
            sts = aspi_execute( bus, id, 0, 0, NULL, 6 ,SCSI_START_STP );
        }
        if ( sts & STS$M_SUCCESS ) {
            struct {
                unsigned char blocks[4];
                unsigned char sectsz[4];
            } CapBuffer;
            memset( ASPI_srb.CDBByte, 0, sizeof( ASPI_srb.CDBByte ) );
            sts = aspi_execute( bus, id, SRB_DIR_IN, sizeof( CapBuffer ),
                                (BYTE *) &CapBuffer, 10, SCSI_RD_CAPAC );
            if ( sts & STS$M_SUCCESS ) {
                *sectors        = ( CapBuffer.blocks[0] << 24 ) |
                                  ( CapBuffer.blocks[1] << 16 ) |
                                  ( CapBuffer.blocks[2] <<  8 ) |
                                    CapBuffer.blocks[3]         ;
                *bytespersector = ( CapBuffer.sectsz[0] << 24 ) |
                                  ( CapBuffer.sectsz[1] << 16 ) |
                                  ( CapBuffer.sectsz[2] <<  8 ) |
                                    CapBuffer.sectsz[3]         ;
            }
        }
        printf( "ASPI (Bus %d,ID %d) %8.8s %16.16s %4.4s %s %d\n",
                bus, id, InquiryBuffer.vendor,InquiryBuffer.product,
                InquiryBuffer.revision,
                ( InquiryBuffer.dev_qual2 & 0x80 ) ? "Removable" : "Fixed",
                *bytespersector );

    }
    if ( !( sts & STS$M_SUCCESS ) ) {
        int i;
        printf( "ASPI Error sense %x %x %x - ",
                ASPI_status, ASPI_HaStat, ASPI_TargStat );
        for ( i = 0; i < SENSE_LEN; i++ ) {
            printf( " %x", ASPI_srb.SenseArea[i] );
        }
        printf( "\n" );
    }
    return sts;
}

/********************************************************** aspi_initialize() */

/* Get ASPI ready and locate the next ASPI drive... */

static unsigned aspi_initialize( short *dev_type, short *dev_bus,
                                 short *dev_id ) {

    short aspi_bus, aspi_id;
    unsigned aspi_devcount;
    DWORD ASPIStatus;
    SRB_GDEVBlock Ssrb;

    aspi_devcount = 0;
    if ( ASPI_adapters == 0 ) {
        ASPIStatus = GetASPI32SupportInfo();
        ASPI_status = ASPIStatus;
        if ( HIBYTE( LOWORD( ASPIStatus ) ) != SS_COMP ) {
            printf( "Could not initialize ASPI (%x)\n", ASPIStatus );
            printf( "Please check that ASPI is installed and started "
                    "properly\n" );
            return 8;
        } else {
            ASPI_adapters = LOWORD( LOBYTE( ASPIStatus ) );
            ASPI_event = CreateEvent( NULL, FALSE, FALSE, NULL );
            if ( ASPI_event == NULL ) {
                return 8;
            }
        }
    }
    Ssrb.SRB_Cmd = SC_GET_DEV_TYPE;
    Ssrb.SRB_HaId = 0;
    Ssrb.SRB_Flags = SRB_EVENT_NOTIFY;
    Ssrb.SRB_Hdr_Rsvd = 0;
    Ssrb.SRB_Lun = 0;
    for ( aspi_bus = 0; aspi_bus < ASPI_adapters; aspi_bus++ ) {
        for ( aspi_id = 0; aspi_id <= 7; aspi_id++ ) {
            Ssrb.SRB_HaId = aspi_bus;
            Ssrb.SRB_Target = aspi_id;
            ASPIStatus = SendASPI32Command( &Ssrb );
            ASPI_status = ASPIStatus;
            if ( ASPIStatus == SS_COMP ) {
                if ( Ssrb.SRB_DeviceType == DTYPE_CROM ||
                     Ssrb.SRB_DeviceType == DTYPE_OPTI ) {
                    if ( ++aspi_devcount > ASPI_devices ) {
                        *dev_type = Ssrb.SRB_DeviceType;
                        *dev_bus = aspi_bus;
                        *dev_id = aspi_id;
                        ASPI_devices++;
                        return SS$_NORMAL;
                    }
                }
            }
        }
    }
    printf( "Could not find a suitable ASPI device\n" );
    return 8;
}
#endif

/********************************************************** GetDiskGeometry() */

/* NT Get disk or CD geometry... */

static BOOL GetDiskGeometry( struct DEV *dev, unsigned *sectors,
                             unsigned *bytespersector ) {

    BOOL result;

#ifdef USE_ASPI
    if ( DEV_USES_ASPI( dev ) ) {
        unsigned sts;
        sts = aspi_identify( dev->API.ASPI.bus, dev->API.ASPI.id,
                             sectors, bytespersector );
        result = sts & STS$M_SUCCESS;
    } else
#endif
   {
     DWORD ReturnedByteCount;
        if ( is_NT ) {

            /* WinNT */

            DISK_GEOMETRY Geometry;
            result =
                DeviceIoControl( dev->API.Win32.handle,    /* hDevice         */
                                                           /* dwIoControlCode */
                                 IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                 NULL,                     /* lpInBuffer      */
                                 0,                        /* nInBufferSize   */
                                 &Geometry,                /* lpOutBuffer     */
                                 sizeof( Geometry ),       /* nOutBufferSize  */
                                 &ReturnedByteCount,       /* lpBytesReturned */
                                 NULL                      /* lpOverlapped    */
                               );
            if ( !result ) {
                result =
                    DeviceIoControl( dev->API.Win32.handle,/* hDevice         */
                                                           /* dwIoControlCode */
                                     IOCTL_CDROM_GET_DRIVE_GEOMETRY,
                                     NULL,                 /* lpInBuffer      */
                                     0,                    /* nInBufferSize   */
                                     &Geometry,            /* lpOutBuffer     */
                                     sizeof( Geometry ),   /* nOutBufferSize  */
                                     &ReturnedByteCount,   /* lpBytesReturned */
                                     NULL                  /* lpOverlapped    */
                                   );
            }
            if ( result ) {
                *sectors        = Geometry.Cylinders.QuadPart *
                                  Geometry.TracksPerCylinder *
                                  Geometry.SectorsPerTrack;
                *bytespersector = Geometry.BytesPerSector;
            }

        } else {

            /* Win9X */

            char RootPathName[4] = "X:\\";
            DWORD SectorsPerCluster, BytesPerSector, NumberOfFreeClusters,
                  TotalNumberOfClusters;
            ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes,
                           TotalNumberOfFreeBytes;
            RootPathName[0] = dev->drive + 'A';
            result =
                GetDiskFreeSpace( RootPathName,    /* lpRootPathName          */
                                                   /* lpSectorsPerCluster     */
                                  &SectorsPerCluster,
                                  &BytesPerSector, /* lpBytesPerSector        */
                                                   /* lpNumberOfFreeClusters  */
                                  &NumberOfFreeClusters,
                                                   /* lpTotalNumberOfClusters */
                                  &TotalNumberOfClusters
                                );
            if ( result ) {
                *sectors        = TotalNumberOfClusters * SectorsPerCluster;
                *bytespersector = BytesPerSector;
                /* Note: GetDiskFreeSpace() returns incorrect values for */
                /* volumes over 2 GB.  We don't really care, as long as  */
                /* the bytes per sector is correct.                      */
                if ( GetDiskFreeSpaceEx( RootPathName,
                                         &FreeBytesAvailable,
                                         &TotalNumberOfBytes,
                                         &TotalNumberOfFreeBytes
                                       ) ) {
                    *sectors = TotalNumberOfBytes.QuadPart / BytesPerSector;
                }
            }
        }
    }
    return result;
}

/*************************************************************** LockVolume() */

/* NT drive lock - we don't want any interference... */

static BOOL LockVolume( struct DEV *dev ) {

    DWORD ReturnedByteCount;
    BOOL result;

    if ( is_NT ) {

        /* WinNT */

        result =
            DeviceIoControl( dev->API.Win32.handle,        /* hDevice         */
                             FSCTL_LOCK_VOLUME,            /* dwIoControlCode */
                             NULL,                         /* lpInBuffer      */
                             0,                            /* nInBufferSize   */
                             NULL,                         /* lpOutBuffer     */
                             0,                            /* nOutBufferSize  */
                             &ReturnedByteCount,           /* lpBytesReturned */
                             NULL                          /* lpOverlapped    */
                           );
    } else {

        /* Win9X */

        DIOC_REGISTERS reg = { 0 };                /* Win9X DIOC registers    */
        reg.reg_EAX = 0x440d;                      /* IOCTL for block devices */
        reg.reg_EBX = dev->drive + 1;              /* 0=default, 1=A, ...     */
        reg.reg_ECX = 0x084b;                      /* Lock physical volume    */
                                                   /* Permission              */
        reg.reg_EDX = ( dev->access & MOU_WRITE ) ? 0x1 : 0x0 ;
        reg.reg_Flags = 0x0001;                    /* Set carry flag          */
        result =
            DeviceIoControl( dev->API.Win32.handle,        /* hDevice         */
                             VWIN32_DIOC_DOS_IOCTL,        /* dwIoControlCode */
                             &reg,                         /* lpInBuffer      */
                             sizeof( reg ),                /* nInBufferSize   */
                             &reg,                         /* lpOutBuffer     */
                             sizeof( reg ),                /* nOutBufferSize  */
                             &ReturnedByteCount,           /* lpBytesReturned */
                             NULL                          /* lpOverlapped    */
                           ) && !( reg.reg_Flags & 0x0001 );

    }

    return result;
}

/************************************************************* UnlockVolume() */

static BOOL UnlockVolume( struct DEV *dev ) {

    DWORD ReturnedByteCount;
    BOOL result;

    if ( is_NT ) {

        /* WinNT */

        result =
            DeviceIoControl( dev->API.Win32.handle,        /* hDevice         */
                             FSCTL_UNLOCK_VOLUME,          /* dwIoControlCode */
                             NULL,                         /* lpInBuffer      */
                             0,                            /* nInBufferSize   */
                             NULL,                         /* lpOutBuffer     */
                             0,                            /* nOutBufferSize  */
                             &ReturnedByteCount,           /* lpBytesReturned */
                             NULL                          /* lpOverlapped    */
                           );
    } else {

        /* Win9X */

        DIOC_REGISTERS reg = { 0 };                /* Win9X DIOC registers    */
        reg.reg_EAX = 0x440d;                      /* IOCTL for block devices */
        reg.reg_EBX = dev->drive + 1;              /* 0=default, 1=A, ...     */
        reg.reg_ECX = 0x086b;                      /* Unlock physical volume  */
        reg.reg_EDX = 0;
        reg.reg_Flags = 0x0001;                    /* Set carry flag          */
        result =
            DeviceIoControl( dev->API.Win32.handle,        /* hDevice         */
                             VWIN32_DIOC_DOS_IOCTL,        /* dwIoControlCode */
                             &reg,                         /* lpInBuffer      */
                             sizeof( reg ),                /* nInBufferSize   */
                             &reg,                         /* lpOutBuffer     */
                             sizeof( reg ),                /* nOutBufferSize  */
                             &ReturnedByteCount,           /* lpBytesReturned */
                             NULL                          /* lpOverlapped    */
                           ) && !( reg.reg_Flags & 0x0001 );

    }

    return result;
}

/************************************************************ getsysversion() */

/* This routine figures out whether this is an NT system or not... */

static void getsysversion( void ) {

    OSVERSIONINFO sysver;

    memset( &sysver, 0, sizeof( sysver ) );
    sysver.dwOSVersionInfoSize = sizeof( sysver );
    GetVersionEx( &sysver                                    /* lpVersionInfo */
                );
    is_NT = ( sysver.dwPlatformId == VER_PLATFORM_WIN32_NT ) ? 1 : 0 ;
}

/************************************************************** phy_getsect() */

/* Read a physical sector... */

static unsigned phy_getsect( struct DEV *dev, unsigned sector ) {

    register unsigned sts;

    sts = SS$_NORMAL;
    if ( sector != dev->last_sector || dev->last_sector == 0 ) {
#ifdef USE_ASPI
        if ( DEV_USES_ASPI( dev ) ) {
            sts = aspi_read( dev->API.ASPI.bus, dev->API.ASPI.id,
                             sector, dev->bytespersector, dev->IoBuffer );
        } else
#endif
               {
            if ( dev->access & MOU_VIRTUAL || is_NT ) {
                DWORD DistanceLow, DistanceHigh, BytesRead;
                DistanceLow  = ( sector * dev->blockspersector ) <<  9;
                DistanceHigh = ( sector * dev->blockspersector ) >> 23;
                SetLastError( 0 );
                SetFilePointer( dev->API.Win32.handle,/* hFile                */
                                DistanceLow,          /* lDistanceToMove      */
                                &DistanceHigh,        /* lpDistanceToMoveHigh */
                                FILE_BEGIN            /* dwMoveMethod         */
                              );
                if ( GetLastError() != NO_ERROR ) {
                    TCHAR *msg = w32_errstr(0);
                    printf( "PHYIO_READ: SetFilePointer() failed: %s\n",
                            msg );
                    LocalFree(msg);
                    sts = SS$_PARITY;
                }
                if ( sts & STS$M_SUCCESS ) {
                                                      /* hFile                */
                    if ( !ReadFile( dev->API.Win32.handle,
                                    dev->IoBuffer,    /* lpBuffer             */
                                                      /* nNumberOfBytesToRead */
                                    dev->bytespersector,
                                    &BytesRead,       /* lpNumberOfBytesRead  */
                                    NULL              /* lpOverlapped         */
                                  ) || BytesRead != dev->bytespersector ) {
                        TCHAR *msg = w32_errstr(0);
                        printf( "PHYIO_READ: ReadFile() failed: %s\n",
                                msg );
                        LocalFree(msg);
                        sts = SS$_PARITY;
                    }
                }
            } else {
                DIOC_REGISTERS reg = { 0 };           /* Win9X DIOC registers */
                DISKIO dio;
                BOOL result;
                DWORD cb = 0;
                dio.dwStartSector = sector;           /* sector number        */
                dio.wSectors = 1;                     /* sector count         */
                dio.dwBuffer = (DWORD) dev->IoBuffer; /* sector buffer        */
                reg.reg_EAX = 0x7305;                 /* Ext_ABSDiskReadWrite */
                reg.reg_EBX = (DWORD) &dio;           /* parameter block      */
                reg.reg_ECX = -1;                     /* use dio              */
                reg.reg_EDX = dev->drive + 1;         /* 0=default, 1=A, ...  */
                reg.reg_EDI = 0;
                reg.reg_ESI = 0x0;                    /* read                 */
                reg.reg_Flags = 0x0001;               /* Set carry flag       */
                result =
                    DeviceIoControl( dev->API.Win32.handle,/* hDevice         */
                                                           /* dwIoControlCode */
                                     VWIN32_DIOC_DOS_DRIVEINFO,
                                     &reg,                 /* lpInBuffer      */
                                     sizeof( reg ),        /* nInBufferSize   */
                                     &reg,                 /* lpOutBuffer     */
                                     sizeof( reg ),        /* nOutBufferSize  */
                                     &cb,                  /* lpBytesReturned */
                                     NULL                  /* lpOverlapped    */
                                   ) && !( reg.reg_Flags & 0x0001 );
                if ( !result ) {
                    TCHAR *msg = w32_errstr(0);
                    printf( "PHYIO_READ: Read sector %d failed: %s\n",
                            sector, msg );
                    LocalFree(msg);
                    sts = SS$_PARITY;
                }
            }
        }
        dev->last_sector = ( sts & STS$M_SUCCESS ) ? sector : 0 ;
    }
    return sts;
}

/************************************************************** phy_putsect() */

/* Write a physical sector... */

static unsigned phy_putsect( struct DEV *dev, unsigned sector ) {

    register unsigned sts;

    sts = SS$_NORMAL;
    dev->last_sector = sector;
#ifdef USE_ASPI
    if ( DEV_USES_ASPI( dev ) ) {
        sts = aspi_write( dev->API.ASPI.bus, dev->API.ASPI.id,
                          sector, dev->bytespersector, dev->IoBuffer );
    } else
#endif
          {
        if ( dev->access & MOU_VIRTUAL || is_NT ) {
            DWORD DistanceLow, DistanceHigh, BytesWritten;
            DistanceLow  = ( sector * dev->blockspersector ) <<  9;
            DistanceHigh = ( sector * dev->blockspersector ) >> 23;
            SetLastError( 0 );
            SetFilePointer( dev->API.Win32.handle,    /* hFile                */
                            DistanceLow,              /* lDistanceToMove      */
                            &DistanceHigh,            /* lpDistanceToMoveHigh */
                            FILE_BEGIN                /* dwMoveMethod         */
                          );
            if ( GetLastError() != NO_ERROR ) {
                TCHAR *msg = w32_errstr(0);
                printf( "PHYIO_WRITE: SetFilePointer() failed: %s\n",
                        msg );
                LocalFree(msg);
                sts = SS$_PARITY;
            }
            if ( sts & STS$M_SUCCESS ) {
                                                    /* hFile                  */
                if ( !WriteFile( dev->API.Win32.handle,
                                 dev->IoBuffer,     /* lpBuffer               */
                                                    /* nNumberOfBytesToWrite  */
                                 dev->bytespersector,
                                 &BytesWritten,     /* lpNumberOfBytesWritten */
                                 NULL               /* lpOverlapped           */
                               ) || BytesWritten != dev->bytespersector ) {
                    TCHAR *msg = w32_errstr(0);
                    printf( "PHYIO_WRITE: WriteFile() failed: %s\n",
                            msg );
                    LocalFree(msg);
                    sts = SS$_PARITY;
                }
            }
        } else {
            DIOC_REGISTERS reg = { 0 };               /* Win9X DIOC registers */
            DISKIO dio;
            BOOL result;
            DWORD cb = 0;
            dio.dwStartSector = sector;               /* sector number        */
            dio.wSectors = 1;                         /* sector count         */
            dio.dwBuffer = (DWORD) dev->IoBuffer;     /* sector buffer        */
            reg.reg_EAX = 0x7305;                     /* Ext_ABSDiskReadWrite */
            reg.reg_EBX = (DWORD) &dio;               /* parameter block      */
            reg.reg_ECX = -1;                         /* use dio              */
            reg.reg_EDX = dev->drive + 1;             /* 0=default, 1=A, ...  */
            reg.reg_EDI = 0;
            reg.reg_ESI = 0x1;                        /* write                */
            reg.reg_Flags = 0x0001;                   /* Set carry flag       */
            result =
                DeviceIoControl( dev->API.Win32.handle,    /* hDevice         */
                                 VWIN32_DIOC_DOS_DRIVEINFO,/* dwIoControlCode */
                                 &reg,                     /* lpInBuffer      */
                                 sizeof( reg ),            /* nInBufferSize   */
                                 &reg,                     /* lpOutBuffer     */
                                 sizeof( reg ),            /* nOutBufferSize  */
                                 &cb,                      /* lpBytesReturned */
                                 NULL                      /* lpOverlapped    */
                               ) && !( reg.reg_Flags & 0x0001 );
            if ( !result ) {
                TCHAR *msg = w32_errstr(0);
                printf( "PHYIO_WRITE: Write sector %d failed: %s\n",
                        sector, msg );
                LocalFree(msg);
                sts = SS$_PARITY;
            }
        }
    }
    return sts;
}

/*************************************************************** phyio_show() */

void phyio_show( showtype_t type ) {

    switch( type ) {
    case SHOW_STATS:
        printf( "PHYIO_SHOW Initializations: %d Reads: %d Writes: %d\n",
                init_count, read_count, write_count );
        return;
    case SHOW_FILE64:
        printf( "\nLarge ODS-2 image files (>2GB) are supported.\n" );
        return;
    case SHOW_DEVICES: {
        TCHAR *namep = NULL, *dname = NULL;
        TCHAR devname[sizeof("Z:")];
        int n = 0;
        TCHAR l;

        for( l = 'A'; l <= 'Z'; l++ ) {
            snprintf( devname, sizeof( devname ), "%c:", l );
            dname = namep = driveFromLetter( devname );
            if( namep != NULL ) {
                const char *type = NULL;
#define WINDEVPFX "\\Device\\"
                if( strncmp( dname, WINDEVPFX, sizeof(WINDEVPFX)-1 ) == 0 )
                    dname += sizeof( WINDEVPFX ) -1;
#undef WINDEVPFX
                if( !n++ )
                    printf( " Physical devices\n"
                            "  Drv   Type    PhysicalName\n"
                            "  --- --------- ------------\n" );
                printf( "   %s ", devname );
                switch( GetDriveType( devname ) ) {
                case DRIVE_REMOVABLE:
                    type = "Removable"; break;
                case DRIVE_FIXED:
                    type = "Fixed";     break;
                case DRIVE_REMOTE:
                    type = "Network";
                    dname = NULL;       break;
                case DRIVE_CDROM:
                    type = "CDROM";     break;
                case DRIVE_RAMDISK:
                    type = "RAMdisk";   break;
                default:
                    type = "Other";
                    dname = NULL;       break;
                }
                printf( "%-9s %s\n", type, (dname? dname: "") );
                free(namep);
            }
        }
    }
        return;
    default:
        abort();
    }
}

/*************************************************************** phyio_help() */

void phyio_help(FILE *fp ) {
    fprintf( fp, "Specify the device to be mounted as a drive letter.\n" );
    fprintf( fp, "E.g. mount D:\n" );
#ifdef USE_ASPI
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

/*************************************************************** phyio_path() */

char *phyio_path( const char *filnam ) {

    size_t n;
    char *path, resultant_path[MAX_PATH];

    if ( filnam == NULL || strpbrk( filnam, "*?" ) != NULL ||
         !PathSearchAndQualify( filnam,              /* pcszPath              */
                                resultant_path,      /* pszFullyQualifiedPath */
                                                     /* cchFullyQualifiedPath */
                                sizeof( resultant_path )
                              ) ) {
        return NULL;
    }
    n = strlen( resultant_path );
    path = (char *) malloc( n + 1 );
    if ( path != NULL ) {
        memcpy( path, resultant_path, n + 1 );
    }
    return path;
}

/*************************************************************** phyio_init() */

/* Initialize device by opening it, locking it and getting it ready.. */

unsigned phyio_init( struct DEV *dev ) {
    const char *virtual;

    if ( is_NT > 1 ) {
        getsysversion();
    }

    init_count++;

    dev->access &= ~MOU_VIRTUAL;
    dev->API.Win32.handle = INVALID_HANDLE_VALUE;
    dev->drive = -1;
    dev->sectors = 0;
    dev->bytespersector = 0;
    dev->blockspersector = 0;
    dev->last_sector = 0;
    dev->IoBuffer = NULL;

    virtual = virt_lookup( dev->devnam );
    if ( virtual == NULL ) {
        if ( strlen( dev->devnam ) != 2 ||
            !isalpha( dev->devnam[0] ) || dev->devnam[1] != ':' ) {
            return SS$_IVDEVNAM;
        }
        dev->drive = toupper( dev->devnam[0] ) - 'A';

        /* Use ASPI for devices past C... */

#ifdef USE_ASPI
        if ( DEV_USES_ASPI( dev ) ) {
            dev->API.ASPI.dtype = -1;
            dev->API.ASPI.bus = -1;
            dev->API.ASPI.id = -1;
            sts = aspi_initialize( &dev->API.ASPI.dtype, &dev->API.ASPI.bus,
                                   &dev->API.ASPI.id );
            if ( !( sts & STS$M_SUCCESS ) ) {
                return sts;
            }
        } else
#endif
               {

            /* WinNT stuff */

            if ( is_NT ) {
                char ntname[7] = "\\\\.\\";          /* 7 = sizeof \\.\a:\0 */
                memcpy( ntname+4, dev->devnam, 3 );  /* Copy a:\0 */
                ntname[4] = toupper( ntname[4] );
                dev->API.Win32.handle =
                    CreateFile( ntname,              /* lpFileName            */
                                GENERIC_READ |       /* dwDesiredAccess       */
                                ( dev->access & MOU_WRITE ? GENERIC_WRITE : 0 ),
                                                     /* dwShareMode           */
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,                /* lpSecurityAttributes  */
                                OPEN_EXISTING,       /* dwCreationDisposition */
                                                     /* dwFlagsAndAttributes  */
                                FILE_FLAG_NO_BUFFERING,
                                NULL                 /* hTemplateFile         */
                              );
                if ( dev->API.Win32.handle == INVALID_HANDLE_VALUE &&
                     dev->access & MOU_WRITE ) {
                    dev->access &= ~MOU_WRITE;
                    dev->API.Win32.handle =
                        CreateFile( ntname,          /* lpFileName            */
                                    GENERIC_READ,    /* dwDesiredAccess       */
                                                     /* dwShareMode           */
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,            /* lpSecurityAttributes  */
                                    OPEN_EXISTING,   /* dwCreationDisposition */
                                                     /* dwFlagsAndAttributes  */
                                    FILE_FLAG_NO_BUFFERING,
                                    NULL             /* hTemplateFile         */
                                  );
                }

            } else {

                /* Win9X stuff */

                dev->API.Win32.handle =
                    CreateFile( "\\\\.\\vwin32",     /* lpFileName            */
                                0,                   /* dwDesiredAccess       */
                                0,                   /* dwShareMode           */
                                NULL,                /* lpSecurityAttributes  */
                                0,                   /* dwCreationDisposition */
                                                     /* dwFlagsAnd Attributes */
                                FILE_FLAG_DELETE_ON_CLOSE,
                                NULL                 /* hTemplateFile         */
                              );
            }
            if ( dev->API.Win32.handle == INVALID_HANDLE_VALUE ) {
                TCHAR *msg = w32_errstr(0);
                printf( "PHYIO_INIT: Open( \"%s\" ) failed: %s\n",
                        dev->devnam, msg );
                LocalFree(msg);
                return SS$_NOSUCHDEV;
            }
            if ( !LockVolume( dev ) ) {
                TCHAR *msg = w32_errstr(0);
                printf( "PHYIO_INIT: LockVolume( \"%s\" ) failed: %a\n",
                        dev->devnam, msg );
                LocalFree(msg);
                phyio_done( dev );
                return SS$_DEVNOTALLOC;
            }
        }
        if ( !GetDiskGeometry( dev, &dev->sectors, &dev->bytespersector ) ) {
            TCHAR *msg = w32_errstr(0);
            printf( "PHYIO_INIT: GetDiskGeometry( \"%s\" ) failed: %s\n",
                    dev->devnam, msg );
            LocalFree(msg);
            phyio_done( dev );
            return 80;
        }
        dev->blockspersector = dev->bytespersector / 512;
    } else { /* Virtual device */
        DWORD FileSizeLow, FileSizeHigh;
        dev->access |= MOU_VIRTUAL;
        dev->API.Win32.handle =
            CreateFile( virtual,                     /* lpFileName            */
                        GENERIC_READ |               /* dwDesiredAccess       */
                        ( dev->access & MOU_WRITE ? GENERIC_WRITE : 0 ),
                        0,                           /* dwShareMode           */
                        NULL,                        /* lpSecurityAttributes  */
                        OPEN_EXISTING,               /* dwCreationDisposition */
                                                     /* dwFlagsAndAttributes  */
                        FILE_FLAG_RANDOM_ACCESS |
                        ( dev->access & MOU_WRITE ?
                          FILE_FLAG_WRITE_THROUGH : 0 ),
                        NULL                         /* hTemplateFile         */
                      );
        if ( dev->API.Win32.handle == INVALID_HANDLE_VALUE &&
             dev->access & MOU_WRITE ) {
            dev->access &= ~MOU_WRITE;
            dev->API.Win32.handle =
                CreateFile( virtual,                 /* lpFileName            */
                            GENERIC_READ,            /* dwDesiredAccess       */
                            0,                       /* dwShareMode           */
                            NULL,                    /* lpSecurityAttributes  */
                            OPEN_EXISTING,           /* dwCreationDisposition */
                                                     /* dwFlagsAndAttributes  */
                            FILE_FLAG_RANDOM_ACCESS,
                            NULL                     /* hTemplateFile         */
                          );
        }
        if ( dev->API.Win32.handle == INVALID_HANDLE_VALUE ) {
            return SS$_NOSUCHFILE;
        }
        SetLastError( 0 );
        FileSizeLow = GetFileSize( dev->API.Win32.handle,   /* hFile          */
                                   &FileSizeHigh            /* lpFileSizeHigh */
                                 );
        if ( GetLastError() == NO_ERROR ) {
            dev->sectors = (     FileSizeHigh        << 23 )                |
                           ( ( ( FileSizeLow + 511 ) >>  9 ) & 0x007FFFFF ) ;
            dev->bytespersector = 512;
            dev->blockspersector = 1;
        }
    }
    dev->IoBuffer = VirtualAlloc( NULL,                   /* lpAddress        */
                                  dev->bytespersector,    /* dwSize           */
                                  MEM_COMMIT,             /* flAllocationType */
                                  PAGE_READWRITE          /* flProtect        */
                                );
    if ( dev->IoBuffer == NULL ) {
        phyio_done( dev );
        return SS$_INSFMEM;
    }
#ifdef DEBUG
    printf( "--->phyio_init(): sectors = %u, bytespersector = %u\n",
            dev->sectors, dev->bytespersector );
#endif
    return SS$_NORMAL;
}

/*************************************************************** phyio_done() */

unsigned phyio_done( struct DEV *dev ) {

    if ( !DEV_USES_ASPI( dev ) &&
         dev->API.Win32.handle != INVALID_HANDLE_VALUE ) {
        UnlockVolume( dev );
        CloseHandle( dev->API.Win32.handle                      /* hObject    */
                   );
        dev->API.Win32.handle = INVALID_HANDLE_VALUE;
    }
    if( dev->access & MOU_VIRTUAL )
        virt_device( dev->devnam, NULL );
    if ( dev->IoBuffer != NULL ) {
        VirtualFree( dev->IoBuffer,                             /* lpAddress  */
                     dev->bytespersector,                       /* dwSize     */
                     MEM_RELEASE                                /* dwFreeType */
                   );
        dev->IoBuffer = NULL;
    }
    return SS$_NORMAL;
}

/*************************************************************** phyio_read() */

/* Handle a read request ... need to read the approriate sectors to
   complete the request... */

unsigned phyio_read( struct DEV *dev, unsigned block, unsigned length,
                     char *buffer ) {

    register unsigned sts, transfer, sectno, offset;
    char *sectbuff;

#ifdef DEBUG
    printf( "Phyio read block: %d into %x (%d bytes)\n",
            block, buffer, length );
#endif

    sts = SS$_NORMAL;
    read_count++;
    sectno = block / dev->blockspersector;
    offset = block % dev->blockspersector;
    sectbuff = dev->IoBuffer;
    while ( length > 0 ) {
        transfer = ( dev->blockspersector - offset ) * 512;
        if ( transfer > length ) {
            transfer = length;
        }
        sts = phy_getsect( dev, sectno );
        if ( !( sts & STS$M_SUCCESS ) ) {
            break;
        }
        memcpy( buffer, sectbuff + ( offset * 512 ), transfer );
        buffer += transfer;
        length -= transfer;
        sectno++;
        offset = 0;
    }
    if ( !( sts & STS$M_SUCCESS ) ) {
        printf( "PHYIO_READ Error %d Block %d Length %d",
                sts, block, length );
#ifdef USE_ASPI
        printf( " (ASPI: %x %x %x)",
                ASPI_status, ASPI_HaStat, ASPI_TargStat );
#endif
        printf( "\n" );
    }
    return sts;
}

/************************************************************** phyio_write() */

/* Handle a write request ... need to read the approriate sectors to
   complete the request... */

unsigned phyio_write( struct DEV *dev, unsigned block, unsigned length,
                      const char *buffer) {

    register unsigned sts, transfer, sectno, offset;
    char *sectbuff;

#ifdef DEBUG
    printf( "Phyio write block: %d from %x (%d bytes)\n",
            block, buffer, length );
#endif

    sts = SS$_NORMAL;
    write_count++;
    sectno = block / dev->blockspersector;
    offset = block % dev->blockspersector;
    sectbuff = dev->IoBuffer;
    while ( length > 0 ) {
        transfer = ( dev->blockspersector - offset ) * 512;
        if ( transfer > length ) {
            transfer = length;
        }
        if ( transfer != dev->blockspersector * 512 ) {
            sts = phy_getsect( dev, sectno );
            if ( !( sts & STS$M_SUCCESS ) ) {
                break;
            }
        }
        memcpy( sectbuff + ( offset * 512 ), buffer, transfer );
        sts = phy_putsect( dev, sectno );
        if ( !( sts & STS$M_SUCCESS ) ) {
            break;
        }
        buffer += transfer;
        length -= transfer;
        sectno++;
        offset = 0;
    }
    if ( !( sts & STS$M_SUCCESS ) ) {
        printf( "PHYIO_WRITE Error %d Block %d Length %d",
                sts, block, length );
#ifdef USE_ASPI
        printf( " (ASPI: %x %x %x)",
                ASPI_status, ASPI_HaStat, ASPI_TargStat );
#endif
        printf( "\n" );
    }
    return sts;
}
