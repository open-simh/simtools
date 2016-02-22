/* Header.h v1.0    Definitions for ODS2 file headers */

/* Small macro to swap words in a longword */

#define swapw(w) (w[0]<<16 | w[1])

/* File characteristic definitions */

#define FCH$M_NOBACKUP   0x2
#define FCH$M_CONTIGB    0x20
#define FCH$M_LOCKED     0x40
#define FCH$M_CONTIG     0x80
#define FCH$M_DIRECTORY  0x2000
#define FCH$M_MARKDEL    0x8000
#define FCH$M_ERASE      0x20000

/* File and record attribute definitions */

#define FAT$C_FIXED     0x1
#define FAT$C_VARIABLE  0x2
#define FAT$C_VFC       0x3
#define FAT$C_UNDEFINED 0x0
#define FAT$C_STREAM    0x4
#define FAT$C_STREAMLF  0x5
#define FAT$C_STREAMCR  0x6

#define FAT$C_DIRECT    0x3
#define FAT$C_INDEXED   0x2
#define FAT$C_RELATIVE  0x1
#define FAT$C_SEQUENTIAL 0x0

#define FAT$M_FORTRANCC 0x1
#define FAT$M_IMPLIEDCC 0x2
#define FAT$M_PRINTCC   0x4
#define FAT$M_NOSPAN    0x8
#define FAT$M_MSBRCW    0x10

/* Type definitions for basic data types */

typedef unsigned char u_byte;
typedef unsigned short u_word;
typedef unsigned int u_long;

/* Structure of time */

struct TIME {
    unsigned char time[8];
};

/* Definition of a UIC */

struct UIC {
    u_word uic$w_mem;
    u_word uic$w_grp;
};

/* Definition for a FID */

struct fiddef {
    u_word fid$w_num;
    u_word fid$w_seq;
    u_byte fid$b_rvn;
    u_byte fid$b_nmx;
};

/* RMS record definition */

struct RECATTR {
    u_byte fat$b_rtype;
    u_byte fat$b_rattrib;
    u_word fat$w_rsize;
    u_word fat$l_hiblk[2];
    u_word fat$l_efblk[2];
    u_word fat$w_ffbyte;
    u_byte fat$b_bktsize;
    u_byte fat$b_vfcsize;
    u_word fat$w_maxrec;
    u_word fat$w_defext;
    u_word fat$w_gbc;
    u_byte fat$_UU0[8];
    u_word fat$w_versions;
};

/* Layout of the volume home block... */

struct HOME {
    u_long hm2$l_homelbn;
    u_long hm2$l_alhomelbn;
    u_long hm2$l_altidxlbn;
    u_word hm2$w_struclev;
    u_word hm2$w_cluster;
    u_word hm2$w_homevbn;
    u_word hm2$w_alhomevbn;
    u_word hm2$w_altidxvbn;
    u_word hm2$w_ibmapvbn;
    u_long hm2$l_ibmaplbn;
    u_long hm2$l_maxfiles;
    u_word hm2$w_ibmapsize;
    u_word hm2$w_resfiles;
    u_word hm2$w_devtype;
    u_word hm2$w_rvn;
    u_word hm2$w_setcount;
    u_word hm2$w_volchar;
    struct UIC hm2$w_volowner;
    u_long hm2$l_reserved1;
    u_word hm2$w_protect;
    u_word hm2$w_fileprot;
    u_word hm2$w_reserved2;
    u_word hm2$w_checksum1;
    struct TIME hm2$q_credate;
    u_byte hm2$b_window;
    u_byte hm2$b_lru_lim;
    u_word hm2$w_extend;
    struct TIME hm2$q_retainmin;
    struct TIME hm2$q_retainmax;
    struct TIME hm2$q_revdate;
    u_byte hm2$r_min_class[20];
    u_byte hm2$r_max_class[20];
    u_byte hm2$t_reserved3[320];
    u_long hm2$l_serialnum;
    char hm2$t_strucname[12];
    char hm2$t_volname[12];
    char hm2$t_ownername[12];
    char hm2$t_format[12];
    u_word hm2$w_reserved4;
    u_word hm2$w_checksum2;
};

/* Structure of the header identification area */

struct IDENT {
    char fi2$t_filename[20];
    u_word fi2$w_revision;
    struct TIME fi2$q_credate;
    struct TIME fi2$q_revdate;
    struct TIME fi2$q_expdate;
    struct TIME fi2$q_bakdate;
    char fi2$t_filenamext[66];
};

/* File header layout */

struct HEAD {
    u_byte fh2$b_idoffset;
    u_byte fh2$b_mpoffset;
    u_byte fh2$b_acoffset;
    u_byte fh2$b_rsoffset;
    u_word fh2$w_seg_num;
    u_word fh2$w_struclev;
    struct fiddef fh2$w_fid;
    struct fiddef fh2$w_ext_fid;
    struct RECATTR fh2$w_recattr;
    u_long fh2$l_filechar;
    u_word fh2$w_reserved1;
    u_byte fh2$b_map_inuse;
    u_byte fh2$b_acc_mode;
    struct UIC fh2$l_fileowner;
    u_word fh2$w_fileprot;
    struct fiddef fh2$w_backlink;
    u_byte fh2$b_journal;
    u_byte fh2$b_ru_active;
    u_word fh2$w_reserved2;
    u_long fh2$l_highwater;
    u_byte fh2$b_reserved3[8];
    u_byte fh2$r_class_prot[20];
    u_byte fh2$r_restofit[402];
    u_word fh2$w_checksum;
};

/* Storage control block layout */

struct SCB {
    u_word scb$w_struclev;
    u_word scb$w_cluster;
    u_long scb$l_volsize;
    u_long scb$l_blksize;
    u_long scb$l_sectors;
    u_long scb$l_tracks;
    u_long scb$l_cylinders;
    u_long scb$l_status;
    u_long scb$l_status2;
    u_word scb$w_writecnt;
    char scb$t_volockname[12];
    struct TIME scb$q_mounttime;
    u_word scb$w_backrev;
    u_long scb$q_genernum[2];
    char scb$b_reserved[446];
    u_word scb$w_checksum;
};
