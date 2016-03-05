#include <windows.h>

#include "scsidefs.h"
#include "ssdef.h"
#include "stsdef.h"
#include "wnaspi32.h"

static unsigned ASPI_adapters = 0;     /* Count of ASPI adapters */
static unsigned ASPI_devices = 0;      /* Count of ASPI devices accessed */

static unsigned ASPI_status = 0;       /* Temporary status indicator */
static unsigned ASPI_HaStat = 0;       /* Temporary status indicator */
static unsigned ASPI_TargStat = 0;     /* Temporary status indicator */

static HANDLE ASPI_event;              /* Windows event for ASPI wait */
static SRB_ExecSCSICmd ASPI_srb;       /* SRB buffer for ASPI commands */

static unsigned aspi_execute( short bus, short id, BYTE Flags, DWORD BufLen,
                              BYTE *BufPointer, BYTE CDBLen, BYTE CDBByte );
static unsigned aspi_identify( short bus, short id, unsigned *sectors,
                               unsigned *bytespersector );
static unsigned aspi_enumerate();

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
        if ( !( sts & STS$M_SUCCESS ) ) {
            printf( "Drive not ready.\n" );
            sts = SS$_DEVOFFLINE;
        }
    } else {
        printf( "SCSI Inquiry failed.\n" );
        sts = SS$_DEVCMDERR;
    }
    if ( !( sts & STS$M_SUCCESS ) ) {
        int i;
        printf( "ASPI (Bus %d,ID %d) Error sense %x %x %x - ",
                bus, id, ASPI_status, ASPI_HaStat, ASPI_TargStat );
        for ( i = 0; i < SENSE_LEN; i++ ) {
            printf( " %x", ASPI_srb.SenseArea[i] );
        }
        printf( "\n" );
    }
    return sts;
}

/*********************************************************** aspi_enumerate() */

/* Initialize ASPI and enumerate the ASPI drives */

static unsigned aspi_enumerate() {

    short aspi_bus, aspi_id;
    unsigned sectors, bytespersector;
    DWORD ASPIStatus;
    SRB_GDEVBlock Ssrb;

    if ( ASPI_adapters == 0 ) {
        ASPIStatus = GetASPI32SupportInfo();
        ASPI_status = ASPIStatus;
        if ( HIBYTE( LOWORD( ASPIStatus ) ) != SS_COMP ) {
            printf( "Could not initialize ASPI (%x).\n", ASPIStatus );
            printf( "Please check that ASPI is installed and started "
                    "properly.\n" );
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
    ASPI_devices = 0;
    for ( aspi_bus = 0; aspi_bus < ASPI_adapters; aspi_bus++ ) {
        for ( aspi_id = 0; aspi_id <= 15; aspi_id++ ) {
            Ssrb.SRB_HaId = aspi_bus;
            Ssrb.SRB_Target = aspi_id;
            ASPIStatus = SendASPI32Command( &Ssrb );
            ASPI_status = ASPIStatus;
            if ( ASPIStatus == SS_COMP ) {
                ASPI_devices++;
                sectors = 0;
                bytespersector = 0;
                aspi_identify( aspi_bus, aspi_id, &sectors, &bytespersector );
                printf( "Device Type: 0x%x%s, Sectors: %lu, Bytes Per Sector: %lu\n",
                        Ssrb.SRB_DeviceType,
                        ( ( Ssrb.SRB_DeviceType == DTYPE_CROM ) ? " (CD-ROM)" :
                          ( ( Ssrb.SRB_DeviceType == DTYPE_OPTI ) ? " (Optical)" :
                            "" ) ), sectors, bytespersector );
            }
        }
    }
    if ( ASPI_devices == 0 ) {
        printf( "Could not find a suitable ASPI device.\n" );
        return 8;
    }
    return SS$_NORMAL;
}

/********************************************************************* main() */

int main( ) {

    aspi_enumerate();

    return EXIT_SUCCESS;

}