/* Access.h    Definitions for file access routines */

/*
 *      This is part of ODS2 written by Paul Nankervis,
 *      email address:  Paulnank@au1.ibm.com

 *      ODS2 is distributed freely for all members of the
 *      VMS community to use. However all derived works
 *      must maintain comments in their source to acknowledge
 *      the contributions of the original author and
 *      subsequent contributors.   This is free software; no
 *      warranty is offered,  and while we believe it to be useful,
 *      you use it at your own risk.
 */

#ifndef _F11DEF_H
#define _F11DEF_H

#include <stdint.h>

#include "vmstime.h"


#ifndef ODS2_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ODS2_BIG_ENDIAN 1
#else
#define ODS2_BIG_ENDIAN 0
#endif
#elif !defined(__GNUC__) /* GLIB can define some of these with older GCC */
#if defined(__BIG_ENDIAN__) || defined(__BIG_ENDIAN) || defined(_BIG_ENDIAN)
#define ODS2_BIG_ENDIAN 1
#else
#define ODS2_BIG_ENDIAN 0
#endif
#else
#define ODS2_BIG_ENDIAN 0
#endif
#endif

#if ODS2_BIG_ENDIAN
#define F11LONG( l ) ((f11long)( ( (l) & 0x000000ff ) << 24 | \
                                 ( (l) & 0x0000ff00 ) <<  8 | \
                                 ( (l) & 0x00ff0000 ) >>  8 | \
                                   (l)                >> 24   ))
#define F11WORD( w ) ((f11word)( ( (w) & 0x00ff ) << 8 | \
                                   (w)            >> 8   ))
#define F11SWAP( l ) ((f11long)( ( (l) & 0x00ff0000 ) << 8 | \
                                 ( (l) & 0xff000000 ) >> 8 | \
                                 ( (l) & 0x000000ff ) << 8 | \
                                 ( (l) & 0x0000ff00 ) >> 8   ))
#else
#define F11LONG( l ) ((f11long)(l))
#define F11WORD( w ) ((f11word)(w))
#define F11SWAP( l ) ((f11long)( ( (l) & 0x0000ffff ) << 16 | \
                                   (l)                >> 16   ))
#endif

typedef uint8_t  f11byte;
typedef uint16_t f11word;
typedef uint32_t f11swap;
typedef uint32_t f11long;

#define FH2$M_WASCONTIG  0x00001
#define FH2$M_NOBACKUP   0x00002
#define FH2$M_WRITEBACK  0x00004
#define FH2$M_READCHECK  0x00008
#define FH2$M_WRITECHECK 0x00010
#define FH2$M_CONTIGB    0x00020
#define FH2$M_LOCKED     0x00040
#define FH2$M_CONTIG     0x00080

#define FH2$M_BADACL     0x000800
#define FH2$M_SPOOL      0x001000
#define FH2$M_DIRECTORY  0x002000
#define FH2$M_BADBLOCK   0x004000
#define FH2$M_MARKDEL    0x008000
#define FH2$M_NOCHARGE   0x010000
#define FH2$M_ERASE      0x020000
#define FH2$M_NOMOVE     0x200000

#ifdef __ALPHA
#pragma member_alignment save
#pragma nomember_alignment
#endif

#ifdef __GNUC__
#pragma pack(push,1)
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
#define NULL_FID(fidadr) ((fidadr)->fid$w_num == 0 &&                   \
                          (fidadr)->fid$w_seq == 0 &&                   \
                          (fidadr)->fid$b_rvn ==0 &&                    \
                          (fidadr)->fid$b_nmx == 0 )

#define FID_MATCH(fid1,fid2,rvn) ((fid1)->fid$w_num == (fid2)->fid$w_num && \
                                  (fid1)->fid$w_seq == (fid2)->fid$w_seq && \
                                  ((fid1)->fid$b_rvn? (fid1)->fid$b_rvn: rvn)  == \
                                  ((fid2)->fid$b_rvn? (fid2)->fid$b_rvn: rvn) && \
                                  (fid1)->fid$b_nmx == (fid1)->fid$b_nmx )

#define FID$C_INDEXF  1
#define FID$C_BITMAP  2
#define FID$C_BADBLK  3
#define FID$C_MFD     4
#define FID$C_CORIMG  5
#define FID$C_VOLSET  6
#define FID$C_CONTIN  7
#define FID$C_BACKUP  8
#define FID$C_BADLOG  9
#define FID$C_FREFIL 10

#define FID$C_MAXRES 10

#define prot$m_system 0x000F
#define prot$v_system  0
#define prot$m_owner  0x00F0
#define prot$v_owner   4
#define prot$m_group  0x0F00
#define prot$v_group   8
#define prot$m_world  0xF000
#define prot$v_world  12

#define prot$m_noread    0x1
#define prot$m_nowrite   0x2
#define prot$m_noexe     0x4
#define prot$m_nodel     0x8
#define prot$m_none    (prot$m_noread|prot$m_nowrite|prot$m_noexe|prot$m_nodel)
#define prot$m_norestrict 0x0
#define SETPROT( sys, own, grp, wld )                      \
    ( ((sys) << prot$v_system) | ((own) << prot$v_owner) | \
      ((grp) << prot$v_group) | ((wld) << prot$v_world) )

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
#define FH2$C_LENGTH 80 /* (offsetof( struct HEAD, fh2$l_highwater) +
                           sizeof( f11long ))
                        */
    f11byte fh2$b_reserved3[8];
    struct fh2$r_class_prot {
        f11byte cls$b_secur_lev;
        f11byte cls$b_integ_lev;
        f11word reserved4;
        f11byte cls$q_secur_cat[8];
        f11byte cls$q_integ_cat[8];
    } fh2$r_class_prot;
    f11byte fh2$r_vardata[402];
    f11word fh2$w_checksum;
};

#define STRUCLEV      (0x0201)
#define FM2$M_FORMAT1 (1 << 14)
#define FM2$M_FORMAT2 (2 << 14)
#define FM2$M_FORMAT3 (3 << 14)
#define FM2$C_FMT1_MAXLBN ((1 << 22) - 1)
#define FM2$C_FMT1_MAXCNT (256)
#define FM2$C_FMT2_MAXLBN (0xFFFFFFFF)
#define FM2$C_FMT2_MAXCNT (1 << 14)
#define FM2$C_FMT3_MAXLBN (0xFFFFFFFF)
#define FM2$C_FMT3_MAXCNT (1 << 30)

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

#ifdef __GNUC__
#pragma pack(pop)
#endif

#ifdef __ALPHA
#pragma member_alignment restore
#endif

#endif
