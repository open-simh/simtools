/* Access.h    Definitions for file access routines */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

#ifndef _F11DEF_H
#define _F11DEF_H

#include <stdint.h>

#include "vmstime.h"

#ifndef ODS2_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ODS2_BIG_ENDIAN 1
#endif
#else
#if defined(__BIG_ENDIAN__) || defined(__BIG_ENDIAN) || defined(_BIG_ENDIAN)
#define ODS2_BIG_ENDIAN 1
#endif
#endif
#endif

#ifdef ODS2_BIG_ENDIAN
#define F11LONG( l ) ( ( (l) & 0x000000ff ) << 24 | \
                       ( (l) & 0x0000ff00 ) <<  8 | \
                       ( (l) & 0x00ff0000 ) >>  8 | \
                         (l)                >> 24   )
#define F11WORD( w ) ( ( (w) & 0x00ff ) << 8 | \
                         (w)            >> 8   )
#define F11SWAP( l ) ( ( (l) & 0x00ff0000 ) << 8 | \
                       ( (l) & 0xff000000 ) >> 8 | \
                       ( (l) & 0x000000ff ) << 8 | \
                       ( (l) & 0x0000ff00 ) >> 8   )
#else
#define F11LONG( l ) (l)
#define F11WORD( w ) (w)
#define F11SWAP( l ) ( ( (l) & 0x0000ffff ) << 16 | \
                         (l)                >> 16   )
#endif

typedef uint8_t f11byte;
typedef uint16_t f11word;
typedef uint32_t f11swap;
typedef uint32_t f11long;

#define FH2$M_NOBACKUP   0x00002
#define FH2$M_CONTIGB    0x00020
#define FH2$M_CONTIG     0x00080
#define FH2$M_DIRECTORY  0x02000
#define FH2$M_MARKDEL    0x08000
#define FH2$M_ERASE      0x20000

#ifdef __ALPHA
#pragma member_alignment save
#pragma nomember_alignment
#endif

struct UIC {
    f11word uic$w_mem;
    f11word uic$w_grp;
};

struct fiddef {
    f11word fid$w_num;
    f11word fid$w_seq;
    f11byte fid$b_rvn;
    f11byte fid$b_nmx;
};

struct RECATTR {
    f11byte fat$b_rtype;
    f11byte fat$b_rattrib;
    f11word fat$w_rsize;
    f11swap fat$l_hiblk;
    f11swap fat$l_efblk;
    f11word fat$w_ffbyte;
    f11byte fat$b_bktsize;
    f11byte fat$b_vfcsize;
    f11word fat$w_maxrec;
    f11word fat$w_defext;
    f11word fat$w_gbc;
    f11byte fat$_UU0[8];
    f11word fat$w_versions;
};

struct HOME {
    f11long hm2$l_homelbn;
    f11long hm2$l_alhomelbn;
    f11long hm2$l_altidxlbn;
    f11word hm2$w_struclev;
    f11word hm2$w_cluster;
    f11word hm2$w_homevbn;
    f11word hm2$w_alhomevbn;
    f11word hm2$w_altidxvbn;
    f11word hm2$w_ibmapvbn;
    f11long hm2$l_ibmaplbn;
    f11long hm2$l_maxfiles;
    f11word hm2$w_ibmapsize;
    f11word hm2$w_resfiles;
    f11word hm2$w_devtype;
    f11word hm2$w_rvn;
    f11word hm2$w_setcount;
    f11word hm2$w_volchar;
    struct UIC hm2$w_volowner;
    f11long hm2$l_reserved1;
    f11word hm2$w_protect;
    f11word hm2$w_fileprot;
    f11word hm2$w_reserved2;
    f11word hm2$w_checksum1;
    VMSTIME hm2$q_credate;
    f11byte hm2$b_window;
    f11byte hm2$b_lru_lim;
    f11word hm2$w_extend;
    VMSTIME hm2$q_retainmin;
    VMSTIME hm2$q_retainmax;
    VMSTIME hm2$q_revdate;
    f11byte hm2$r_min_class[20];
    f11byte hm2$r_max_class[20];
    f11byte hm2$t_reserved3[320];
    f11long hm2$l_serialnum;
    char hm2$t_strucname[12];
    char hm2$t_volname[12];
    char hm2$t_ownername[12];
    char hm2$t_format[12];
    f11word hm2$w_reserved4;
    f11word hm2$w_checksum2;
};

struct IDENT {
    char fi2$t_filename[20];
    f11word fi2$w_revision;
    VMSTIME fi2$q_credate;
    VMSTIME fi2$q_revdate;
    VMSTIME fi2$q_expdate;
    VMSTIME fi2$q_bakdate;
    char fi2$t_filenamext[66];
};

struct HEAD {
    f11byte fh2$b_idoffset;
    f11byte fh2$b_mpoffset;
    f11byte fh2$b_acoffset;
    f11byte fh2$b_rsoffset;
    f11word fh2$w_seg_num;
    f11word fh2$w_struclev;
    struct fiddef fh2$w_fid;
    struct fiddef fh2$w_ext_fid;
    struct RECATTR fh2$w_recattr;
    f11long fh2$l_filechar;
    f11word fh2$w_reserved1;
    f11byte fh2$b_map_inuse;
    f11byte fh2$b_acc_mode;
    struct UIC fh2$l_fileowner;
    f11word fh2$w_fileprot;
    struct fiddef fh2$w_backlink;
    f11byte fh2$b_journal;
    f11byte fh2$b_ru_active;
    f11word fh2$w_reserved2;
    f11long fh2$l_highwater;
    f11byte fh2$b_reserved3[8];
    f11byte fh2$r_class_prot[20];
    f11byte fh2$r_restofit[402];
    f11word fh2$w_checksum;
};

struct SCB {
    f11word scb$w_struclev;
    f11word scb$w_cluster;
    f11long scb$l_volsize;
    f11long scb$l_blksize;
    f11long scb$l_sectors;
    f11long scb$l_tracks;
    f11long scb$l_cylinders;
    f11long scb$l_status;
    f11long scb$l_status2;
    f11word scb$w_writecnt;
    char scb$t_volockname[12];
    VMSTIME scb$q_mounttime;
    f11word scb$w_backrev;
    f11long scb$q_genernum[2];
    char scb$b_reserved[446];
    f11word scb$w_checksum;
};

struct VOLSETREC {
    char vsr$t_label[12];
    char vsr$b_reserved[52];
};

struct dir$r_rec {
    f11word dir$w_size;
    f11word dir$w_verlimit;
    f11byte dir$b_flags;
    f11byte dir$b_namecount;
    char dir$t_name[1];
};

struct dir$r_ent {
    f11word dir$w_version;
    struct fiddef dir$w_fid;
};


#ifdef __ALPHA
#pragma member_alignment restore
#endif

#endif
