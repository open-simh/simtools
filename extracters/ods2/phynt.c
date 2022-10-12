/* PHYNT.C     Physical I/O module for Windows NT */

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

/* Win9X code now included with code to automatically determine
 * if we are running under WinNT...
 *
 * W98 seems to have a performance problem where the
 * INT25 call are VERY slow!!!
 */



/* This version built and tested under Visual C++ 6.0 and LCC.
 * To support CD drives this version requires ASPI run-time
 * support.. Windows 95/98 come with ASPI but NT/2000 require
 * that a version of ASPI be install and started before running
 * this program!!   (One verion is NTASPI available from
 * www.symbios.com)
 *
 *Note: As of 2016, www.symbios.com is a financial site.  You
 *may be able to obtain ASPI from someplace else, but the code
 *to use it may be stale.  On more recent windows, you can simply
 *use the NT drive letter, which will do physical I/O to the device.
 */

#if !defined( DEBUG ) && defined( DEBUG_PHYNT )
#define DEBUG DEBUG_PHYNT
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <winioctl.h>

#include "ods2.h"
#include "cmddef.h"
#include "device.h"
#include "phyio.h"
#include "ssdef.h"
#include "stsdef.h"
#include "compat.h"
#include "phyvirt.h"
#include "sysmsg.h"

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
 *      To read from CD ASPI run-time support is required (wnaspi32.dll).
 *      NT does not come with ASPI support so you need to install it first.
 *      (ASPI is also required for things like Digital Audio Extraction
 *      and was origionally loaded on my machine to make MP3 audio files.)
 *      One downside is that ASPI on NT does not have a way to match SCSI
 *      adapters and units to a system device. So this program works by
 *      finding the first CD like unit on the system and using it - that
 *      may NOT be what you want if you have several CD drives! And finally
 *      there are CD drives and there are CD drives. From what I can tell
 *      many simply won't work with this (or other!) code. Naturally if
 *      you find a better way please let me know... Paulnank@au1.ibm.com
 */

#ifdef USE_ASPI
#define DEV_USES_ASPI( dev ) ( dev->drive > ('C' - 'A') )
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

static vmscond_t aspi_execute( short bus, short id, BYTE Flags, DWORD BufLen,
                               BYTE *BufPointer, BYTE CDBLen, BYTE CDBByte );
static vmscond_t aspi_read( short bus, short id, unsigned sector,
                            unsigned bytespersector, char *buffer );
static vmscond_t aspi_write( short bus, short id, unsigned sector,
                             unsigned bytespersector, char *buffer );
static vmscond_t aspi_identify( short bus, short id, unsigned *sectors,
                                unsigned *bytespersector );
static vmscond_t aspi_initialize( short *dev_type, short *dev_bus,
                                  short *dev_id );
#endif

static BOOL GetDiskGeometry( struct DEV *dev, unsigned *sectors,
                             unsigned *bytespersector );

static BOOL LockVolume( struct DEV *dev );
static BOOL UnlockVolume( struct DEV *dev );
static void getsysversion( void );
static vmscond_t phy_getsect( struct DEV *dev, unsigned sector );
static vmscond_t phy_putsect( struct DEV *dev, unsigned sector );

static vmscond_t phyio_read( struct DEV *dev, unsigned block, unsigned length,
                             char *buffer );
static vmscond_t phyio_write( struct DEV *dev, unsigned block, unsigned length,
                              const char *buffer );

#ifdef USE_ASPI
/************************************************************* aspi_execute() */

static vmscond_t aspi_execute( short bus, short id, BYTE Flags, DWORD BufLen,
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
    memset( ASPI_srb.SRB_Rsvd3, 0 , sizeof( ASPI_srb.SRB_Rsvd3 ) );
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

static vmscond_t aspi_read( short bus, short id, unsigned sector,
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

static vmscond_t aspi_write( short bus, short id, unsigned sector,
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

static vmscond_t aspi_identify( short bus, short id, unsigned *sectors,
                                unsigned *bytespersector ) {
    vmscond_t sts;
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
    if( $SUCCESSFUL(sts = aspi_execute( bus, id, SRB_DIR_IN, sizeof( InquiryBuffer ),
                                        (BYTE *) &InquiryBuffer, 6, SCSI_INQUIRY )) ) {
        ASPI_srb.CDBByte[1] = 0;/* SCSI TEST if ready packet */
        ASPI_srb.CDBByte[2] = 0;
        ASPI_srb.CDBByte[3] = 0;
        ASPI_srb.CDBByte[4] = 0;
        ASPI_srb.CDBByte[5] = 0;
        if( $FAILS(sts = aspi_execute( bus, id, 0, 0, NULL, 6, SCSI_TST_U_RDY )) ) {
            ASPI_srb.CDBByte[1] = 0;    /* SCSI START STOP packet */
            ASPI_srb.CDBByte[2] = 0;
            ASPI_srb.CDBByte[3] = 0;
            ASPI_srb.CDBByte[4] = 1;    /* Start unit */
            ASPI_srb.CDBByte[5] = 0;
            sts = aspi_execute( bus, id, 0, 0, NULL, 6 , SCSI_START_STP );
        }
        if( $SUCCESSFUL(sts) ) {
            struct {
                unsigned char blocks[4];
                unsigned char sectsz[4];
            } CapBuffer;
            memset( ASPI_srb.CDBByte, 0, sizeof( ASPI_srb.CDBByte ) );
            if( $SUCCESSFUL(sts = aspi_execute( bus, id, SRB_DIR_IN, sizeof( CapBuffer ),
                                                (BYTE *) &CapBuffer, 10, SCSI_RD_CAPAC )) ) {
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
                bus, id, InquiryBuffer.vendor, InquiryBuffer.product,
                InquiryBuffer.revision,
                ( InquiryBuffer.dev_qual2 & 0x80 ) ? "Removable" : "Fixed",
                *bytespersector );

    }
    if( $FAILED(sts) ) {
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

static vmscond_t aspi_initialize( short *dev_type, short *dev_bus,
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

static BOOL GetDiskGeometry( struct DEV *dev, uint32_t *sectors,
                             uint32_t *bytespersector ) {
    BOOL result;

#ifdef USE_ASPI
    if ( DEV_USES_ASPI( dev ) ) {
        vmscond_t sts;
        sts = aspi_identify( dev->API.ASPI.bus, dev->API.ASPI.id,
                             sectors, bytespersector );
        result = $SUCCESSFUL(sts);
    } else
#endif
   {
     DWORD ReturnedByteCount;
        if ( is_NT ) {

            /* WinNT */

            DISK_GEOMETRY Geometry;
            memset( &Geometry, 0, sizeof(Geometry) );
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
                *sectors = (uint32_t)(Geometry.Cylinders.QuadPart * /* In theory, this can overfolow */
                                      Geometry.TracksPerCylinder *
                                      Geometry.SectorsPerTrack);
                *bytespersector = (uint32_t) Geometry.BytesPerSector;
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
                    *sectors = (uint32_t)(TotalNumberOfBytes.QuadPart / BytesPerSector);
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
#pragma warning (disable : 4996 28159)
    sysver.dwOSVersionInfoSize = sizeof( sysver );
    GetVersionEx( &sysver                                    /* lpVersionInfo */
                );
#pragma warning (default : 4996 28159)
    is_NT = ( sysver.dwPlatformId == VER_PLATFORM_WIN32_NT ) ? 1 : 0 ;
}

/************************************************************** phy_getsect() */

/* Read a physical sector... */

static vmscond_t phy_getsect( struct DEV *dev, uint32_t sector ) {
    register vmscond_t sts;

    sts = SS$_NORMAL;
    if( sector != dev->last_sector || dev->last_sector == 0 ) {
#ifdef USE_ASPI
        if( DEV_USES_ASPI( dev ) ) {
            sts = aspi_read( dev->API.ASPI.bus, dev->API.ASPI.id,
                             sector, dev->bytespersector, dev->IoBuffer );
        } else
#endif
            {
            if( dev->access & MOU_VIRTUAL || is_NT ) {
                DWORD BytesRead;
                __int64 distance;
                LARGE_INTEGER li;
                DWORD err = 0;
                TCHAR *msg;

                memset( &li, 0, sizeof(li) );
                distance = (((__int64)sector) * dev->blockspersector) << 9;
                li.QuadPart = distance;
                if( SetFilePointer( dev->API.Win32.handle,/* hFile                */
                                    li.LowPart,           /* lDistanceToMove      */
                                    &li.HighPart,         /* lpDistanceToMoveHigh */
                                    FILE_BEGIN            /* dwMoveMethod         */
                                    ) == INVALID_SET_FILE_POINTER && (err = GetLastError()) != NO_ERROR ) {
                    msg = w32_errstr( err );
                    printmsg( IO_SEEKERR, 0, sector );
                    if( msg == NULL ) {
                        sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_SEEKERR, err );
                    } else {
                        sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_SEEKERR, msg );
                        LocalFree( msg );
                    }
                }
                if( $SUCCESSFUL(sts) ) {
                    if( !ReadFile( dev->API.Win32.handle,/* hFile                */
                                   dev->IoBuffer,        /* lpBuffer             */
                                   dev->bytespersector,  /* nNumberOfBytesToRead */
                                   &BytesRead,           /* lpNumberOfBytesRead  */
                                   NULL                  /* lpOverlapped         */
                                   ) || BytesRead != dev->bytespersector ) {
                        if( BytesRead == 0 )
                            return SS$_ENDOFFILE;

                        err = GetLastError();
                        msg = w32_errstr( err );
                        printmsg( IO_READERR, 0, sector );
                        if( msg == NULL ) {
                            sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_READERR, err );
                        } else {
                            sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_READERR, msg );
                            LocalFree( msg );
                        }
                    }
                }
            } else {
#ifdef _WIN64
                exit( EXIT_FAILURE );
#else /* Old interface can't deal with 64-bit addresses */
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
                    DWORD err;
                    TCHAR *msg;

                    err = GetLastError();
                    msg = w32_errstr(err);
                    printmsg( IO_READERR, 0, sector );
                    if( msg == NULL ) {
                        sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_READERR, err );
                    } else {
                        sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_READERR, msg );
                        LocalFree( msg );
                    }
                }
#endif
            }
        }
        dev->last_sector = $SUCCESSFUL(sts) ? sector : 0 ;
    }
    return sts;
}

/************************************************************** phy_putsect() */

/* Write a physical sector... */

static vmscond_t phy_putsect( struct DEV *dev, uint32_t sector ) {
    register vmscond_t sts;

    sts = SS$_NORMAL;
    dev->last_sector = sector;
#ifdef USE_ASPI
    if( DEV_USES_ASPI( dev ) ) {
        sts = aspi_write( dev->API.ASPI.bus, dev->API.ASPI.id,
                          sector, dev->bytespersector, dev->IoBuffer );
    } else
#endif
        {
        if( (dev->access & MOU_VIRTUAL) || is_NT ) {
            DWORD BytesWritten;
            __int64 distance;
            LARGE_INTEGER li;
            DWORD err = 0;
            TCHAR *msg;

            memset( &li, 0, sizeof( li ) );
            distance = (((__int64)sector) * dev->blockspersector) << 9;
            li.QuadPart = distance;
            if( SetFilePointer( dev->API.Win32.handle,/* hFile                */
                                li.LowPart,           /* lDistanceToMove      */
                                &li.HighPart,         /* lpDistanceToMoveHigh */
                                FILE_BEGIN            /* dwMoveMethod         */
                                ) == INVALID_SET_FILE_POINTER && (err = GetLastError()) != NO_ERROR ) {
                msg = w32_errstr( err );
                printmsg( IO_SEEKERR, 0, sector );
                if( msg == NULL ) {
                    sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_SEEKERR, err );
                } else {
                    sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_SEEKERR, msg );
                    LocalFree( msg );
                }
            }
            if( $SUCCESSFUL(sts) ) {
                if( !WriteFile( dev->API.Win32.handle, /* hFile                  */
                                dev->IoBuffer,         /* lpBuffer               */
                                dev->bytespersector,   /* nNumberOfBytesToWrite  */
                                &BytesWritten,         /* lpNumberOfBytesWritten */
                                NULL                   /* lpOverlapped           */
                                ) || BytesWritten != dev->bytespersector ) {
                    err = GetLastError();
                    msg = w32_errstr( err );
                    printmsg( IO_WRITEERR, 0, sector );
                    if( msg == NULL ) {
                        sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_WRITEERR, err );
                    } else {
                        sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_WRITEERR, msg );
                        LocalFree( msg );
                    }
                }
            }
        } else {
#ifdef _WIN64
            exit( EXIT_FAILURE );
#else /* Old interface can't deal with 64-bit addresses */
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
            if( !result ) {
                DWORD err;
                TCHAR *msg;

                err = GetLastError();
                msg = w32_errstr( err );
                printmsg( IO_WRITEERR, 0, sector );
                if( msg == NULL ) {
                    sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_WRITEERR, err );
                } else {
                    sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_WRITEERR, msg );
                    LocalFree( msg );
                }
            }
#endif
        }
    }
    return sts;
}

/*************************************************************** phyio_show() */

void phyio_show( showtype_t type ) {
    switch( type ) {
    case SHOW_STATS:
        printmsg( IO_STATS, MSG_TEXT, init_count, read_count, write_count );
        return;
    case SHOW_FILE64:
        printmsg( IO_LARGEFILE, MSG_TEXT );
        return;
    case SHOW_DEVICES: {
        TCHAR *namep = NULL, *dname = NULL;
        TCHAR devname[sizeof("Z:")];
        int n = 0;
        TCHAR l;

        for( l = 'A'; l <= 'Z'; l++ ) {
            (void) snprintf( devname, sizeof( devname ), "%c:", l );
            dname =
                namep = driveFromLetter( devname );
            if( namep != NULL ) {
#define WINDEVPFX "\\Device\\"
                if( strncmp( dname, WINDEVPFX, sizeof(WINDEVPFX)-1 ) == 0 )
                    dname += sizeof( WINDEVPFX ) -1;
#undef WINDEVPFX
                if( !n++ )
                    printmsg( IO_NTDEVHEADER, MSG_TEXT );
                switch( GetDriveType( devname ) ) {
                case DRIVE_REMOVABLE:
                    printmsg( IO_NTDEVREMOVABLE, MSG_TEXT, devname, dname ); break;
                case DRIVE_FIXED:
                    printmsg( IO_NTDEVFIXED, MSG_TEXT, devname, dname );     break;
                case DRIVE_REMOTE:
                    printmsg( IO_NTDEVREMOTE, MSG_TEXT, devname );           break;
                case DRIVE_CDROM:
                    printmsg( IO_NTDEVCDROM, MSG_TEXT, devname, dname );     break;
                case DRIVE_RAMDISK:
                    printmsg( IO_NTDEVRAMDISK, MSG_TEXT, devname, dname );   break;
                default:
                    printmsg( IO_NTDEVOTHER, MSG_TEXT, devname );            break;
                }
                free(namep);
            }
        }
        return;
    }
    default:
        abort();
    }
}

/*************************************************************** phyio_help() */

void phyio_help( void ) {

#ifdef USE_ASPI
    (void) helptopic( 0, "MOUNT NTASPI" );
#else
    (void) helptopic( 0, "MOUNT NT" );
#endif

    return;
}

/*************************************************************** phyio_init() */

/* Initialize device by opening it, locking it and getting it ready.. */

vmscond_t phyio_init( struct DEV *dev ) {
    if( is_NT > 1 ) {
        getsysversion();
    }

    init_count++;


    dev->API.Win32.handle = INVALID_HANDLE_VALUE;
    dev->devread = phyio_read;
    dev->devwrite = phyio_write;
    dev->drive = -1;
    dev->bytespersector = 0;
    dev->blockspersector = 0;
    dev->last_sector = 0;
    dev->IoBuffer = NULL;

    if( dev->access & MOU_VIRTUAL ) {
        const char *filenam;
        DWORD FileSizeLow, FileSizeHigh;

        filenam = virt_lookup( dev->devnam );
        if ( filenam == NULL ) {
            return SS$_NOSUCHDEV;
        }
        dev->API.Win32.handle =
            CreateFile( filenam,                      /* lpFileName            */
                        GENERIC_READ |                /* dwDesiredAccess       */
                        ((dev->access & MOU_WRITE)? GENERIC_WRITE : 0 ),
                        ((dev->access & MOU_WRITE) ? 0 : FILE_SHARE_READ),
                                                      /* dwShareMode           */
                        NULL,                         /* lpSecurityAttributes  */
                        ((dev->access & PHY_CREATE)?
                         CREATE_NEW: OPEN_EXISTING),  /* dwCreationDisposition */
                        FILE_FLAG_RANDOM_ACCESS |
                        (dev->access & MOU_WRITE ?
                         FILE_FLAG_WRITE_THROUGH : 0),/* dwFlagsAndAttributes  */
                        NULL                          /* hTemplateFile         */
                      );
        if( dev->API.Win32.handle == INVALID_HANDLE_VALUE &&
            (dev->access & (MOU_WRITE|PHY_CREATE)) == MOU_WRITE ) {
            dev->access &= ~MOU_WRITE;
            dev->API.Win32.handle =
                CreateFile( filenam,                 /* lpFileName            */
                            GENERIC_READ,            /* dwDesiredAccess       */
                            FILE_SHARE_READ,         /* dwShareMode           */
                            NULL,                    /* lpSecurityAttributes  */
                            OPEN_EXISTING,           /* dwCreationDisposition */
                            FILE_FLAG_RANDOM_ACCESS, /* dwFlagsAndAttributes  */
                            NULL                     /* hTemplateFile         */
                          );
        }
        if( dev->API.Win32.handle == INVALID_HANDLE_VALUE ) {
            vmscond_t sts;
            DWORD err;
            TCHAR *msg;

            err = GetLastError();
            msg = w32_errstr( err );

            printmsg( IO_OPENDEV, 0, filenam );
            if( msg == NULL ) {
                sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_OPENDEV, err );
            } else {
                sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_OPENDEV, msg );
                LocalFree( msg );
            }
            switch( err ) {
            case ERROR_FILE_EXISTS:
            case ERROR_ALREADY_EXISTS:
            case ERROR_ACCESS_DENIED:
            case ERROR_SHARING_VIOLATION:
                return SS$_DUPFILENAME | STS$M_INHIB_MSG;
            case ERROR_FILE_NOT_FOUND:
                return SS$_NOSUCHFILE | STS$M_INHIB_MSG;
            default:
                break;
            }
            return SS$_NOSUCHFILE | STS$M_INHIB_MSG;
        }
        SetLastError( 0 );
        FileSizeLow = GetFileSize( dev->API.Win32.handle,   /* hFile          */
                                   &FileSizeHigh            /* lpFileSizeHigh */
                                 );
        if( GetLastError() == NO_ERROR ) {
#if 0
            dev->sectors = (     FileSizeHigh        << 23 )                |
                           ( ( ( FileSizeLow + 511 ) >>  9 ) & 0x007FFFFF ) ;
#endif
            dev->eofptr = (off_t)(( ((__int64)FileSizeHigh) << 32 ) | FileSizeLow);
            dev->bytespersector = 512;
            dev->blockspersector = 1;
        }
    } else { /* Physical */
        uint32_t sectors;

        if( strlen( dev->devnam ) != 2 ||
            !isalpha( dev->devnam[0] ) || dev->devnam[1] != ':' ) {
            return SS$_IVDEVNAM;
        }
        dev->drive = toupper( dev->devnam[0] ) - 'A';

        /* Use ASPI for devices past C... */

#ifdef USE_ASPI
        if( DEV_USES_ASPI( dev ) ) {
            dev->API.ASPI.dtype = -1;
            dev->API.ASPI.bus = -1;
            dev->API.ASPI.id = -1;
            if( $FAILS(sts = aspi_initialize( &dev->API.ASPI.dtype,
                                              &dev->API.ASPI.bus,
                                              &dev->API.ASPI.id )) ) {
                return sts;
            }
        } else
#endif
               {

            /* WinNT stuff */

            if( is_NT ) {
                char ntname[7] = { '\\', '\\', '.', '\\', 'x', ':', '\0' };
                /*                   0     1    2     3    4    5     6 */
                ntname[4] = toupper( dev->devnam[0] );
                dev->API.Win32.handle =
                    CreateFile( ntname,              /* lpFileName            */
                                GENERIC_READ |       /* dwDesiredAccess       */
                                ( (dev->access & MOU_WRITE) ? GENERIC_WRITE : 0 ),
                                                     /* dwShareMode           */
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,                /* lpSecurityAttributes  */
                                OPEN_EXISTING,       /* dwCreationDisposition */
                                                     /* dwFlagsAndAttributes  */
                                FILE_FLAG_NO_BUFFERING,
                                NULL                 /* hTemplateFile         */
                              );
                if( dev->API.Win32.handle == INVALID_HANDLE_VALUE &&
                    (dev->access & MOU_WRITE) ) {
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
            if( dev->API.Win32.handle == INVALID_HANDLE_VALUE ) {
                vmscond_t sts;
                DWORD err;
                TCHAR *msg;

                err = GetLastError();
                msg = w32_errstr( err );

                printmsg( IO_OPENDEV, 0, dev->devnam );
                if( msg == NULL ) {
                    sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_OPENDEV, err );
                } else {
                    sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_OPENDEV, msg );
                    LocalFree( msg );
                }
               return sts;
            }
            if( !LockVolume( dev ) ) {
                vmscond_t sts;
                DWORD err;
                TCHAR *msg;

                err = GetLastError();
                msg = w32_errstr( err );

                printmsg( IO_LOCKVOL, 0, dev->devnam );
                if( msg == NULL ) {
                    sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_LOCKVOL, err );
                } else {
                    sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_LOCKVOL, msg );
                    LocalFree( msg );
                }
                phyio_done( dev );
                return sts;
            }
        }

        if( !GetDiskGeometry( dev, &sectors, &dev->bytespersector ) ) {
            vmscond_t sts;
            DWORD err;
            TCHAR *msg;

            err = GetLastError();
            msg = w32_errstr( err );
            printmsg( IO_NOGEO, 0, dev->devnam );
            if( msg == NULL ) {
                sts = printmsg( ODS2_OSERRORNO, MSG_CONTINUE, IO_NOGEO, err );
            } else {
                sts = printmsg( ODS2_OSERROR, MSG_CONTINUE, IO_NOGEO, msg );
                LocalFree( msg );
            }
            phyio_done( dev );
            return sts;
        }
        dev->blockspersector = dev->bytespersector / 512;
        dev->eofptr = ((off_t)sectors) * dev->bytespersector;
    }

    /* Common exit for virtual and physical */

    dev->IoBuffer = VirtualAlloc( NULL,                   /* lpAddress        */
                                  dev->bytespersector,    /* dwSize           */
                                  MEM_COMMIT,             /* flAllocationType */
                                  PAGE_READWRITE          /* flProtect        */
                                );
    if( dev->IoBuffer == NULL ) {
        phyio_done( dev );
        return SS$_INSFMEM;
    }
#if DEBUG
    printf( "--->phyio_init(): sectors = %u, bytespersector = %u\n",
            dev->sectors, dev->bytespersector );
#endif
    return SS$_NORMAL;
}

/*************************************************************** phyio_done() */

vmscond_t phyio_done( struct DEV *dev ) {
    if( !DEV_USES_ASPI( dev ) &&
        dev->API.Win32.handle != INVALID_HANDLE_VALUE ) {
        UnlockVolume( dev );
        CloseHandle( dev->API.Win32.handle );                   /* hObject    */

        dev->API.Win32.handle = INVALID_HANDLE_VALUE;
    }
    if( dev->IoBuffer != NULL ) {
        VirtualFree( dev->IoBuffer,                             /* lpAddress  */
                     0,                                         /* dwSize     */
                     MEM_RELEASE                                /* dwFreeType */
                     );
        dev->IoBuffer = NULL;
    }
    return SS$_NORMAL;
}

/*************************************************************** phyio_read() */

/* Handle a read request ... need to read the approriate sectors to
 * complete the request...
 */

static vmscond_t phyio_read( struct DEV *dev, uint32_t block,
                             uint32_t length, char *buffer ) {
    register vmscond_t sts;
    uint32_t transfer, sectno, offset;
    char *sectbuff;

#if DEBUG
    printf( "Phyio read block: %d into %x (%d bytes)\n",
            block, buffer, length );
#endif

    sts = SS$_NORMAL;
    read_count++;
    sectno = block / dev->blockspersector;
    offset = block % dev->blockspersector;
    sectbuff = dev->IoBuffer;
    while( length > 0 ) {
        transfer = ( dev->blockspersector - offset ) * 512;
        if ( transfer > length ) {
            transfer = length;
        }
        if( $FAILS(sts = phy_getsect( dev, sectno )) ) {
            break;
        }
        memcpy( buffer, sectbuff + ( (size_t)offset * 512 ), transfer );
        buffer += transfer;
        length -= transfer;
        sectno++;
        offset = 0;
    }
#ifdef USE_ASPI
    if( $FAILED(sts) ) {
        printf( "(ASPI: %x %x %x)\n",
                ASPI_status, ASPI_HaStat, ASPI_TargStat );
    }
#endif
    return sts;
}

/************************************************************** phyio_write() */

/* Handle a write request ... need to read the approriate sectors to
   complete the request... */

static vmscond_t phyio_write( struct DEV *dev, uint32_t block, uint32_t length,
                              const char *buffer) {
    register vmscond_t sts;
    uint32_t transfer, sectno, offset;
    char *sectbuff;

#if DEBUG
    printf( "Phyio write block: %d from %x (%d bytes)\n",
            block, buffer, length );
#endif

    sts = SS$_NORMAL;
    write_count++;
    sectno = block / dev->blockspersector;
    offset = block % dev->blockspersector;
    sectbuff = dev->IoBuffer;
    while( length > 0 ) {
        transfer = ( dev->blockspersector - offset ) * 512;
        if ( transfer > length ) {
            transfer = length;
        }
        if ( transfer != dev->blockspersector * 512 ) {
            sts = phy_getsect( dev, sectno );
            if( $FAILS(sts = phy_getsect( dev, sectno )) &&
                !$MATCHCOND(sts, SS$_ENDOFFILE) ) {
                break;
            }
        }
        memcpy( sectbuff + ( (size_t)offset * 512 ), buffer, transfer );
        if( $FAILS(sts = phy_putsect( dev, sectno )) ) {
            break;
        }
        buffer += transfer;
        length -= transfer;
        sectno++;
        offset = 0;
    }
#ifdef USE_ASPI
    if( $FAILED(sts) ) {
        printf( "(ASPI: %x %x %x)\n",
                ASPI_status, ASPI_HaStat, ASPI_TargStat );
    }
#endif
    return sts;
}
