/* PHYUNIX.c v1.3    Physical I/O module for Unix */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/*
	If the user mounts  cd0   we open up /dev/cd0 for access.
*/

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include "phyio.h"
#include "ssdef.h"
#include "compat.h"

#if defined(__digital__) && defined(__unix__)
#define DEV_PREFIX "/devices/rdisk/%s"
#else
#ifdef sun
#define DEV_PREFIX "/dev/dsk/%s"
#else
#define DEV_PREFIX "/dev/%s"
#endif
#endif

unsigned init_count = 0;
unsigned read_count = 0;
unsigned write_count = 0;

void phyio_show(void)
{
    printf("PHYIO_SHOW Initializations: %d Reads: %d Writes: %d\n",
           init_count,read_count,write_count);
}

void phyio_help(FILE *fp ) {
    fprintf( fp, "Specify the device to be mounted as using the format:\n" );
    fprintf( fp, " mount %s%s\n\n", DEV_PREFIX, "device" );

    fprintf( fp, "For example, if you are using " DEV_PREFIX "\n", "cdrom0" );
    fprintf( fp, "  ODS2$> mount cdrom0\n\n" );

    fprintf( fp, "Use ODS2-Image to work with disk images such as .ISO or simulator files.\n" );
    fprintf( fp, "Alternatively, you can mount the image on a loop device.\n" );
    return;
}

unsigned phyio_init(int devlen,char *devnam,unsigned *handle,struct phyio_info *info)
{
    int vmsfd;
    char *cp,devbuf[200];

    UNUSED(devlen);

    init_count++;
    info->status = 0;           /* We don't know anything about this device! */
    info->sectors = 0;
    info->sectorsize = 0;
    snprintf(devbuf,sizeof(devbuf),DEV_PREFIX,devnam);
    cp = strchr(devbuf,':');
    if (cp != NULL) *cp = '\0';
    vmsfd = open(devbuf,O_RDWR);
    if (vmsfd < 0) vmsfd = open(devbuf,O_RDONLY);
    if (vmsfd < 0) return SS$_NOSUCHDEV;
    *handle = vmsfd;
    return SS$_NORMAL;
}


unsigned phyio_close(unsigned handle)
{
    close(handle);
    return SS$_NORMAL;
}


unsigned phyio_read(unsigned handle,unsigned block,unsigned length,char *buffer)
{
    off_t res;
#ifdef DEBUG
    printf("Phyio read block: %d into %p (%d bytes)\n",block,buffer,length);
#endif
    read_count++;
    if ((res = lseek(handle,block*512,0)) < 0) {
        perror("lseek ");
        printf("lseek failed %" PRIuMAX "u\n",(uintmax_t)res);
        return SS$_PARITY;
    }
    if ((res = read(handle,buffer,length)) != (ssize_t)length) {
        perror("read ");
        printf("read failed %" PRIuMAX "u\n",(uintmax_t)res);
        return SS$_PARITY;
    }
    return SS$_NORMAL;
}


unsigned phyio_write(unsigned handle,unsigned block,unsigned length,char *buffer)
{
#ifdef DEBUG
    printf("Phyio write block: %d from %p (%d bytes)\n",block,buffer,length);
#endif
    write_count++;
    if (lseek(handle,block*512,0) < 0) return SS$_PARITY;
    if (write(handle,buffer,length) != length) return SS$_PARITY;
    return SS$_NORMAL;
}
