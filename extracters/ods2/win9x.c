#include <windows.h>

#include <stdio.h>
#include <string.h>

#define VWIN32_DIOC_DOS_IOCTL 1 
 
#pragma pack( 1 )

typedef struct _DEVIOCTL_REGISTERS 
{ 
    DWORD reg_EBX; 
    DWORD reg_EDX; 
    DWORD reg_ECX; 
    DWORD reg_EAX; 
    DWORD reg_EDI; 
    DWORD reg_ESI; 
    DWORD reg_Flags; 
} DEVIOCTL_REGISTERS, *PDEVIOCTL_REGISTERS; 
 
typedef struct _MID 
{ 
    WORD  midInfoLevel; 
    DWORD midSerialNum; 
    BYTE  midVolLabel[11]; 
    BYTE  midFileSysType[8]; 
} MID, *PMID; 

typedef struct _DPARAMS
{
    BYTE special_functions;
    BYTE device_type;
    WORD device_attributes;
    WORD no_of_cylinders;
    BYTE media_type;
    struct {
        WORD  bytes_per_sector;
        BYTE  sectors_per_cluster;
        WORD  reserved_sectors;
        BYTE  no_of_FATs;
        WORD  root_entries;
        WORD  small_sectors;    /* FAT12/16 iff < 65536, else =0 and number */
                                /*    of sectors is in large_sectors        */
        BYTE  media_descriptor;
        WORD  sectors_per_FAT;
        WORD  sectors_per_track;
        WORD  no_of_heads;
        DWORD hidden_sectors;
        DWORD large_sectors;    /* FAT32 */
        BYTE  reserved[6];
    } BPB;
} DPARAMS, *PDPARAMS;
 
#pragma pack( )

int main( ) {

    UINT nDrive;
    MID mid;
    DPARAMS dparams;
    char RootPathName[3] = "X:"; // root path
    DWORD SectorsPerCluster;     // sectors per cluster
    DWORD BytesPerSector;        // bytes per sector
    DWORD NumberOfFreeClusters;  // free clusters
    DWORD TotalNumberOfClusters; // total clusters

    for ( nDrive = 1; nDrive <= 26; nDrive++ ) {
        RootPathName[0] = nDrive - 1 + 'A';
        memset( &mid, 0, sizeof( mid ) );
        if ( GetMediaID( &mid, nDrive ) && mid.midFileSysType[0] != '\0' ) {
            printf( "Drive %s %8.8s %11.11s\n", RootPathName,
                    mid.midFileSysType, mid.midVolLabel );
            memset( &dparams, 0, sizeof( dparams ) );
            if ( GetDeviceParams( &dparams, nDrive ) ) {
                printf( "   Device Type:             0x%1.1x\n",
                        dparams.device_type );
                printf( "   No. of Cylinders         %d\n",
                        dparams.no_of_cylinders );
                printf( "   BPB: Bytes Per Sector:   %5d",
                        dparams.BPB.bytes_per_sector );
                printf( "  Sectors Per Track   %5d\n",
                        dparams.BPB.sectors_per_track );
                printf( "        Sectors Per Cluster %5d",
                        dparams.BPB.sectors_per_cluster );
                printf( "  No. of Heads        %5d\n",
                        dparams.BPB.no_of_heads );
                printf( "        Sectors Per Track   %5d",
                        dparams.BPB.sectors_per_track );
                printf( "  Large Sectors:      %8lu\n",
                        dparams.BPB.large_sectors );
                if ( GetDiskFreeSpace( RootPathName,         // root path
                                       &SectorsPerCluster,   // sects per clustr
                                       &BytesPerSector,      // bytes per sector
                                       &NumberOfFreeClusters,// free clusters
                                       &TotalNumberOfClusters// total clusters
                                     ) ) {
                    printf( "   Sectors Per Cluster:   %8lu",
                            SectorsPerCluster );
                    printf( "  BytesPerSector:        %8lu\n",
                            BytesPerSector );
                    printf( "   NumberOfFreeClusters:  %8lu",
                            NumberOfFreeClusters );
                    printf( "  TotalNumberOfClusters: %8lu\n",
                            TotalNumberOfClusters );
                } else {
                    printf( "GetDiskFreeSpace() error %d.\n", GetLastError() );
                }
            } else {
                printf( "GetDeviceParams() error %d.\n", GetLastError() );
            }
        }
    }
}

BOOL GetMediaID(PMID pmid, UINT nDrive) 
{ 
    DEVIOCTL_REGISTERS reg; 
 
    reg.reg_EAX = 0x440D;       // IOCTL for block devices 
    reg.reg_EBX = nDrive;       // zero-based drive ID 
    reg.reg_ECX = 0x0866;       // Get Media ID command 
    reg.reg_EDX = (DWORD) pmid; // receives media ID info 
 
    if (!DoIOCTL(&reg)) 
        return FALSE; 
 
    if (reg.reg_Flags & 0x8000) // error if carry flag set 
        return FALSE; 
 
    return TRUE; 
} 
 
BOOL GetDeviceParams(PDPARAMS pdparams, UINT nDrive)
{
    DEVIOCTL_REGISTERS reg;

    reg.reg_EAX = 0x440D;           // IOCTL for block devices
    reg.reg_EBX = nDrive;           // zero-based drive ID
    reg.reg_ECX = 0x0860;           // Get Device Parameters command
    reg.reg_EDX = (DWORD) pdparams; // receives media ID info

    if (!DoIOCTL(&reg))
        return FALSE;

    if (reg.reg_Flags & 0x8000) // error if carry flag set
        return FALSE;

    return TRUE;
}

BOOL DoIOCTL(PDEVIOCTL_REGISTERS preg) 
{ 
    HANDLE hDevice; 
 
    BOOL fResult; 
    DWORD cb; 
 
    preg->reg_Flags = 0x8000; // assume error (carry flag set) 
 
    hDevice = CreateFile("\\\\.\\vwin32", 
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
        (LPSECURITY_ATTRIBUTES) NULL, OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL, (HANDLE) NULL); 
 
    if (hDevice == (HANDLE) INVALID_HANDLE_VALUE) 
        return FALSE; 
    else 
    { 
        fResult = DeviceIoControl(hDevice, VWIN32_DIOC_DOS_IOCTL, 
            preg, sizeof(*preg), preg, sizeof(*preg), &cb, 0); 
    } 
 
    CloseHandle(hDevice); 
 
    return fResult;
}
