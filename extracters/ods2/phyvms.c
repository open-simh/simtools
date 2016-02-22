/* PHYVMS.c v1.3    Physical I/O module for VMS */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/*  How come VMS is so much easier to do physical I/O on than
    those other ^%$@*! systems?   I can't believe that they have
    different command sets for different device types, and can
    even have different command sets depending on what mode they
    are called from!  Sigh.                                  */

/* Modified by:
 *
 *   31-AUG-2001 01:04	Hunter Goatley <goathunter@goatley.com>
 *
 *	Added checks to be sure device we're mounting is a
 *	disk and that it's already physically mounted at the
 *	DCL level,
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <iodef.h>
#include <descrip.h>
#include <ssdef.h>
#include <dcdef.h>
#include <dvidef.h>
#include <mntdef.h>
#include <devdef.h>
#include <ssdef.h>

#ifdef __GNUC__
unsigned sys$assign();
unsigned sys$qiow();
unsigned sys$dassgn();
#else
#include <starlet.h>
#endif

#include "phyio.h"

#define chk(sts)  {register chksts; if (((chksts = sts) & 1) == 0) lib$stop(chksts);}

unsigned init_count = 0;
unsigned read_count = 0;
unsigned write_count = 0;

void phyio_show(void)
{
    printf("PHYIO_SHOW Initializations: %d Reads: %d Writes: %d\n",
           init_count,read_count,write_count);
}


unsigned phyio_init(int devlen,char *devnam,unsigned *handle,struct phyio_info *info)
{
    struct dsc$descriptor devdsc;
    unsigned long devclass, status;
    char devname[65];
    unsigned long devchar;
    unsigned long mntflags[2];
    struct ITMLST {
	unsigned short length;
	unsigned short itmcod;
	void *buffer;
	unsigned long *retlen;
	} dvi_itmlst[4] = {{sizeof(devclass), DVI$_DEVCLASS, &devclass, 0},
			   {sizeof(devname), DVI$_ALLDEVNAM, &devname, 0},
			   {sizeof(devchar), DVI$_DEVCHAR, &devchar, 0},
			   {0, 0, 0, 0}},
	  mnt_itmlst[3] = {{0, MNT$_DEVNAM, 0, 0},
			   {sizeof(mntflags), MNT$_FLAGS, &mntflags, 0},
			   {0, 0, 0, 0}};

    devdsc.dsc$w_length = devlen;
    devdsc.dsc$a_pointer = devnam;

    init_count++;
    info->status = 0;           /* We don't know anything about this device! */
    info->sectors = 0;
    info->sectorsize = 0;
    *handle = 0;

    /*  Get some device information: device name, class, and mount status */
    status = sys$getdviw(0,0,&devdsc, &dvi_itmlst, 0,0,0,0);
    if (status & 1)
	{
	if (devclass != DC$_DISK)	/* If not a disk, return an error */
	    return (SS$_IVDEVNAM);

	if (!(devchar & DEV$M_MNT))
	    return (SS$_DEVNOTMOUNT);
#if 0
/*
 *  This code will mount the disk if it's not already mounted.  However,
 *  there's no good way to ensure we dismount a disk we mounted (no
 *  easy way to walk the list of devices at exit), so it's been #ifdefed
 *  out.  If you enable it, the "mount" command will mount the disk, but
 *  it'll stay mounted by the process running ODS2, even after image exit.
 */
	    {
	    mnt_itmlst[0].length = strlen(devname);
	    mnt_itmlst[0].buffer = &devname;
	    mntflags[0] = MNT$M_FOREIGN | MNT$M_NOWRITE | MNT$M_NOASSIST |
			MNT$M_NOMNTVER;
	    mntflags[1] = 0;

	    status = sys$mount(&mnt_itmlst);
	    }
#endif
	}

    if (!(status & 1))
	return (status);

    return sys$assign(&devdsc,handle,0,0,0,0);
}


unsigned phyio_close(unsigned handle)
{
    return sys$dassgn(handle);
}


unsigned phyio_read(unsigned handle,unsigned block,unsigned length,char *buffer)
{
#ifdef DEBUG
    printf("Phyio read block: %d into %x (%d bytes)\n",block,buffer,length);
#endif
    read_count++;
    return sys$qiow(1,handle,IO$_READLBLK,NULL,0,0,buffer,length,block,0,0,0);
}


unsigned phyio_write(unsigned handle,unsigned block,unsigned length,char *buffer)
{
#ifdef DEBUG
    printf("Phyio write block: %d from %x (%d bytes)\n",block,buffer,length);
#endif
    write_count++;
    printf("Phyio write block: %d from %x (%d bytes)\n",block,buffer,length);
    return sys$qiow(1,handle,IO$_WRITELBLK,NULL,0,0,buffer,length,block,0,0,0);
}
