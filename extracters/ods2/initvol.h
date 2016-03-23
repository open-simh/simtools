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

#ifndef _INITVOL_H
#define _INITVOL_H

#include <stdint.h>

struct NEWVOL {
    const char *label;
    const char *devtype;
    const char *devnam;
    const char *username;
    const char *useruic;
    unsigned options;
    unsigned clustersize;
    unsigned maxfiles;
    unsigned headers;
    unsigned indexlbn;
    unsigned directories;
};
#define INIT_INDEX_MIDDLE (~0U)
#define INIT_LOG (1 << 0)
#define INIT_VIRTUAL (1 << 1)

#define VOL_ALPHABET "abcdefghijklmnopqrstuvwxyz$-_"            \
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

#ifdef __ALPHA
#pragma member_alignment save
#pragma nomember_alignment
#endif

struct BAD144 {
    uint32_t bbd$l_serial;
    uint16_t bbd$w_reserved;
    uint16_t bbd$w_flags;
    uint32_t bbd$l_badblock[1];
#define BBD$M_TRACK    (0x7F   << BBD$V_TRACK)
#define BBD$V_TRACK    (24)
#define BBD$M_SECTOR   (0xFF   << BBD$V_SECTOR)
#define BBD$V_SECTOR   (16)
#define BBD$M_CYLINDER (0x7FFF << BBD$V_CYLINDER)
#define BBD$V_CYLINDER (0)
#define BBD$C_FILL     ((uint32_t)~0u)
};

struct SWBADENT {
    uint8_t bbm$b_highlbn;
    uint8_t bbm$b_count;
    uint16_t bbm$w_lowlbn;
};
struct SWBAD {
    uint8_t bbm$b_countsize;
    uint8_t bbm$b_lbnsize;
    uint8_t bbm$b_inuse;
    uint8_t bbm$b_avail;
    struct SWBADENT bbm$r_map[126];
    uint16_t bbm$w_reserved;
    uint16_t bbm$w_checksum;
};

#ifdef __ALPHA
#pragma member_alignment restore
#endif

unsigned initvol( void *dev, struct NEWVOL *params );
unsigned int delta_from_index( size_t index );

#endif
