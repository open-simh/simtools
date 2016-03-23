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
 *       the contibution of the original author.
 */

#ifndef _PHYVIRT_H
#define _PHYVIRT_H

#include <stddef.h>

typedef struct disktype disktype_t;
typedef disktype_t *disktypep_t;

struct disktype {
    const char *name;
    unsigned long sectorsize, sectors, tracks, cylinders;
    unsigned long reserved, interleave, skew;
    unsigned int flags;
#define DISK_BAD144 1
#define DISK_BADSW  2
};

extern struct disktype disktype[];
extern size_t max_disktype;

struct DEV;

void virt_show( const char *devnam );
unsigned virt_open( char **devname, unsigned flags, struct DEV **dev );
char *virt_lookup( const char *devnam );
unsigned virt_close( struct DEV *dev );

unsigned virt_read( struct DEV *dev, unsigned lbn, unsigned length,
                    char *buffer );
unsigned virt_write( struct DEV *dev, unsigned lbn, unsigned length,
                     const char *buffer );

unsigned virt_readp( struct DEV *dev, unsigned pbn, unsigned length,
                    char *buffer );
unsigned virt_writep( struct DEV *dev, unsigned pbn, unsigned length,
                     char *buffer );

#ifdef USE_VHD
int virt_vhdavailable( int needed );
#endif

#endif /* # ifndef _PHYVIRT_H */
