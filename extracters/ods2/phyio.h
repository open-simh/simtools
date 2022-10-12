/* Phyio.h    Definition of Physical I/O routines */

/*
 *      This is part of ODS2 written by Paul Nankervis,
 *      email address:  Paulnank@au1.ibm.com
 *
 *      ODS2 is distributed freely for all members of the
 *      VMS community to use. However all derived works
 *      must maintain comments in their source to acknowledge
 *      the contributions of the original author and
 *      subsequent contributors.   This is free software; no
 *      warranty is offered,  and while we believe it to be useful,
 *      you use it at your own risk.
 */

/*      To set up physical I/O for a new system a group of phyio
 *      routines need to be set up. They are:-
 *           phyio_show() which doesn't need to do anything - but
 *                        it would generally print some statistics
 *                        about the other phyio calls.
 *           phyio_path() returns the full path of the supplied filnam.a  The
 *                        returned character string must be deallocated by the
 *                        caller.
 *           phyio_init() to prepare a device for use by future
 *                        read/write calls. The device name would usually
 *                        map to a local device - for example rra: to /dev/rra
 *                        on a Unix system. The call needs to return a handle
 *                        (channel, file handle, reference number...) for
 *                        future reference, and optionally some device
 *                        information.
 *          phyio_done()  makes a device unavailable.
 *          phyio_read()  will return a specified number of bytes into a
 *                        buffer from the start of a 512 byte block on the
 *                        device referred to by the handle.
 *          phyio_write() will write a number of bytes out to a 512 byte block
 *                        address on a device.
 */

#ifndef _PHYIO_H
#define _PHYIO_H

#include <stdint.h>
#include <stdio.h>

#include "ods2.h"

#define PHYIO_READONLY 1

typedef enum showtype {
    SHOW_STATS,
    SHOW_FILE64,
    SHOW_DEVICES
} showtype_t;

struct DEV;

void phyio_show( showtype_t type );
vmscond_t phyio_init( struct DEV *dev );
vmscond_t phyio_done( struct DEV *dev );

typedef vmscond_t( *phy_iord_t )( struct DEV *dev, uint32_t block,
                                  uint32_t length, char *buffer );
typedef vmscond_t( *phy_iowr_t )( struct DEV *dev, uint32_t block,
                                  uint32_t length, const char *buffer );

void phyio_help( void );

#endif /* #ifndef _PHYIO_H */
