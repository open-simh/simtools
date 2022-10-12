/* Phyvirt.h    Definitions for virtual device routines */

/* Timothe Litt March 2016
 * Copyright (C) 2016 Timothe litt
 *   litt at acm dot org
 *
 * Free for use with the ODS2 package.  All other rights reserved.
 */

/*
 *       This is distributed as part of ODS2, originally  written by
 *       Paul Nankervis, email address:  Paulnank@au1.ibm.com
 *
 *       ODS2 is distributed freely for all members of the
 *       VMS community to use. However all derived works
 *       must maintain comments in their source to acknowledge
 *       the contributions of the original author and
 *       subsequent contributors.   This is free software; no
 *       warranty is offered,  and while we believe it to be useful,
 *       you use it at your own risk.
 */

#ifndef _PHYVIRT_H
#define _PHYVIRT_H

#include <stddef.h>

typedef struct disktype disktype_t;
typedef disktype_t *disktypep_t;

struct disktype {
    const char *name;
    uint32_t sectorsize, sectors, tracks, cylinders;
    uint32_t reserved, interleave, skew;
    uint32_t flags;
#define DISK_BAD144 1
#define DISK_BADSW  2
};

extern struct disktype disktype[];
extern size_t max_disktype;

struct DEV;

void virt_show( void );
vmscond_t virt_open( char **devname, uint32_t flags, struct DEV **dev );
char *virt_lookup( const char *devnam );
vmscond_t virt_close( struct DEV *dev );

vmscond_t virt_read( struct DEV *dev, uint32_t lbn, uint32_t length,
                    char *buffer );
vmscond_t virt_write( struct DEV *dev, uint32_t lbn, uint32_t length,
                     const char *buffer );

vmscond_t virt_readp( struct DEV *dev, uint32_t pbn, uint32_t length,
                    char *buffer );
vmscond_t virt_writep( struct DEV *dev, uint32_t pbn, uint32_t length,
                     char *buffer );

#ifdef USE_VHD
int virt_vhdavailable( int needed );
#endif

#endif /* # ifndef _PHYVIRT_H */
