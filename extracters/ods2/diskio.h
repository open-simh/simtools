#ifndef DISKIO_H
#define DISKIO_H

/* diskio.h - ANSI C Disk I/O - for .ISO and simulator disks */

/* Timothe Litt Feb 2016
 *
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/*      To set up physical I/O for a new system a group of phyio
        routines need to be set up. They are:-
             phyio_show() which doesn't need to do anything - but
                          it would generally print some statistics
                          about the other phyio calls.
             phyio_init() to prepare a device for use by future
                          read/write calls. The device name would usually
                          map to a local device - for example rra: to /dev/rra
                          on a Unix system. The call needs to return a handle
                          (channel, file handle, reference number...) for
                          future reference, and optionally some device
                          information.
            phyio_read()  will return a specified number of bytes into a
                          buffer from the start of a 512 byte block on the
                          device referred to by the handle.
            phyio_write() will write a number of bytes out to a 512 byte block
                          address on a device.

*/

char *diskio_mapfile( const char *filename, int options );

int diskio_unmapdrive( const char *drive );

int diskio_showdrives( void );

#endif
