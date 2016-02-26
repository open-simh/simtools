/* Access.h v1.3    Definitions for file access routines */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

#define NO_DOLLAR
#include "cache.h"
#include "vmstime.h"

#ifdef USE_BIG_ENDIAN
#define VMSLONG(l) ((l & 0xff) << 24 | (l & 0xff00) << 8 | (l & 0xff0000) >> 8 | l >> 24)
#define VMSWORD(w) ((w & 0xff) << 8 | w >> 8)
#define VMSSWAP(l) ((l & 0xff0000) << 8 | (l & 0xff000000) >> 8 |(l & 0xff) << 8 | (l & 0xff00) >> 8)
#else
#define VMSLONG(l) l
#define VMSWORD(w) w
#define VMSSWAP(l) ((l & 0xffff) << 16 | l >> 16)
#endif

typedef unsigned char vmsbyte;
typedef unsigned short vmsword;
typedef unsigned int vmsswap;
typedef unsigned int vmslong;

#define FH2$M_NOBACKUP   0x2
#define FH2$M_CONTIGB    0x20
#define FH2$M_CONTIG     0x80
#define FH2$M_DIRECTORY  0x2000
#define FH2$M_MARKDEL    0x8000
#define FH2$M_ERASE      0x20000

#ifdef __ALPHA
#pragma member_alignment save
#pragma nomember_alignment
#endif

struct UIC {
    vmsword uic$w_mem;
    vmsword uic$w_grp;
};


struct fiddef {
    vmsword fid$w_num;
    vmsword fid$w_seq;
    vmsbyte fid$b_rvn;
    vmsbyte fid$b_nmx;
};


struct RECATTR {
    vmsbyte fat$b_rtype;
    vmsbyte fat$b_rattrib;
    vmsword fat$w_rsize;
    vmsswap fat$l_hiblk;
    vmsswap fat$l_efblk;
    vmsword fat$w_ffbyte;
    vmsbyte fat$b_bktsize;
    vmsbyte fat$b_vfcsize;
    vmsword fat$w_maxrec;
    vmsword fat$w_defext;
    vmsword fat$w_gbc;
    vmsbyte fat$_UU0[8];
    vmsword fat$w_versions;
};


struct HOME {
    vmslong hm2$l_homelbn;
    vmslong hm2$l_alhomelbn;
    vmslong hm2$l_altidxlbn;
    vmsword hm2$w_struclev;
    vmsword hm2$w_cluster;
    vmsword hm2$w_homevbn;
    vmsword hm2$w_alhomevbn;
    vmsword hm2$w_altidxvbn;
    vmsword hm2$w_ibmapvbn;
    vmslong hm2$l_ibmaplbn;
    vmslong hm2$l_maxfiles;
    vmsword hm2$w_ibmapsize;
    vmsword hm2$w_resfiles;
    vmsword hm2$w_devtype;
    vmsword hm2$w_rvn;
    vmsword hm2$w_setcount;
    vmsword hm2$w_volchar;
    struct UIC hm2$w_volowner;
    vmslong hm2$l_reserved1;
    vmsword hm2$w_protect;
    vmsword hm2$w_fileprot;
    vmsword hm2$w_reserved2;
    vmsword hm2$w_checksum1;
    VMSTIME hm2$q_credate;
    vmsbyte hm2$b_window;
    vmsbyte hm2$b_lru_lim;
    vmsword hm2$w_extend;
    VMSTIME hm2$q_retainmin;
    VMSTIME hm2$q_retainmax;
    VMSTIME hm2$q_revdate;
    vmsbyte hm2$r_min_class[20];
    vmsbyte hm2$r_max_class[20];
    vmsbyte hm2$t_reserved3[320];
    vmslong hm2$l_serialnum;
    char hm2$t_strucname[12];
    char hm2$t_volname[12];
    char hm2$t_ownername[12];
    char hm2$t_format[12];
    vmsword hm2$w_reserved4;
    vmsword hm2$w_checksum2;
};


struct IDENT {
    char fi2$t_filename[20];
    vmsword fi2$w_revision;
    VMSTIME fi2$q_credate;
    VMSTIME fi2$q_revdate;
    VMSTIME fi2$q_expdate;
    VMSTIME fi2$q_bakdate;
    char fi2$t_filenamext[66];
};


struct HEAD {
    vmsbyte fh2$b_idoffset;
    vmsbyte fh2$b_mpoffset;
    vmsbyte fh2$b_acoffset;
    vmsbyte fh2$b_rsoffset;
    vmsword fh2$w_seg_num;
    vmsword fh2$w_struclev;
    struct fiddef fh2$w_fid;
    struct fiddef fh2$w_ext_fid;
    struct RECATTR fh2$w_recattr;
    vmslong fh2$l_filechar;
    vmsword fh2$w_reserved1;
    vmsbyte fh2$b_map_inuse;
    vmsbyte fh2$b_acc_mode;
    struct UIC fh2$l_fileowner;
    vmsword fh2$w_fileprot;
    struct fiddef fh2$w_backlink;
    vmsbyte fh2$b_journal;
    vmsbyte fh2$b_ru_active;
    vmsword fh2$w_reserved2;
    vmslong fh2$l_highwater;
    vmsbyte fh2$b_reserved3[8];
    vmsbyte fh2$r_class_prot[20];
    vmsbyte fh2$r_restofit[402];
    vmsword fh2$w_checksum;
};

struct SCB {
    vmsword scb$w_struclev;
    vmsword scb$w_cluster;
    vmslong scb$l_volsize;
    vmslong scb$l_blksize;
    vmslong scb$l_sectors;
    vmslong scb$l_tracks;
    vmslong scb$l_cylinders;
    vmslong scb$l_status;
    vmslong scb$l_status2;
    vmsword scb$w_writecnt;
    char scb$t_volockname[12];
    VMSTIME scb$q_mounttime;
    vmsword scb$w_backrev;
    vmslong scb$q_genernum[2];
    char scb$b_reserved[446];
    vmsword scb$w_checksum;
};

#ifdef __ALPHA
#pragma member_alignment restore
#endif

#define EXTMAX 20

struct WCB {
    struct CACHE cache;
    unsigned loblk,hiblk;       /* Range of window */
    unsigned hd_basevbn;        /* File blocks prior to header */
    unsigned hd_seg_num;        /* Header segment number */
    struct fiddef hd_fid;       /* Header FID */
    unsigned short extcount;    /* Extents in use */
    unsigned phylen[EXTMAX];
    unsigned phyblk[EXTMAX];
    unsigned char rvn[EXTMAX];
};                              /* Window control block */


#define VIOC_CHUNKSIZE 4

struct VIOC {
    struct CACHE cache;
    struct FCB *fcb;            /* File this chunk is for */
    unsigned wrtmask;           /* Bit mask for writable blocks */
    unsigned modmask;           /* Bit mask for modified blocks */
    char data[VIOC_CHUNKSIZE][512];     /* Chunk data */
};                              /* Chunk of a file */


#define FCB_WRITE 1             /* FCB open for write... */

struct FCB {
    struct CACHE cache;
    struct VCB *vcb;            /* Volume this file is for */
    struct VIOC *headvioc;      /* Index file chunk for file header */
    struct HEAD *head;          /* Pointer to header block */
    struct WCB *wcb;            /* Window control block tree */
    struct VIOC *vioc;          /* Virtual I/O chunk tree */
    unsigned headvbn;           /* vbn for file header */
    unsigned hiblock;           /* Highest block mapped */
    unsigned highwater;         /* First high water block */
    unsigned char status;       /* FCB status bits */
    unsigned char rvn;          /* Initial file relative volume */
};                              /* File control block */


struct DIRCACHE {
    struct CACHE cache;
    int dirlen;                 /* Length of directory name */
    struct fiddef dirid;        /* File ID of directory */
    char dirnam[1];             /* Directory name */
};                              /* Directory cache entry */


#define VCB_WRITE 1

struct VCBDEV {
    struct DEV *dev;            /* Pointer to device info */
    struct FCB *idxfcb;         /* Index file control block */
    struct FCB *mapfcb;         /* Bitmap file control block */
    unsigned clustersize;       /* Cluster size of the device */
    unsigned max_cluster;       /* Total clusters on the device */
    unsigned free_clusters;     /* Free clusters on disk volume */
    struct HOME home;           /* Volume home block */
};
struct VCB {
    unsigned status;            /* Volume status */
    unsigned devices;           /* Number of volumes in set */
    struct FCB *fcb;            /* File control block tree */
    struct DIRCACHE *dircache;  /* Directory cache tree */
    struct VCBDEV vcbdev[1];    /* List of volumes devices */
};                              /* Volume control block */


struct DEV {
    struct CACHE cache;
    struct VCB *vcb;            /* Pointer to volume (if mounted) */
    unsigned handle;            /* Device physical I/O handle */
    unsigned status;            /* Device physical status */
    unsigned long long sectors;           /* Device physical sectors */
    unsigned sectorsize;        /* Device physical sectorsize */
    char devnam[1];             /* Device name */
};                              /* Device information */

void fid_copy(struct fiddef *dst,struct fiddef *src,unsigned rvn);
unsigned device_lookup(unsigned devlen,char *devnam,int create,struct DEV **retdev);

unsigned dismount(struct VCB *vcb);
unsigned mount(unsigned flags,unsigned devices,char *devnam[],char *label[],struct VCB **vcb);

unsigned accesserase(struct VCB *vcb,struct fiddef *fid);
unsigned deaccessfile(struct FCB *fcb);
unsigned accessfile(struct VCB *vcb,struct fiddef *fid,
                    struct FCB **fcb,unsigned wrtflg);

unsigned deaccesschunk(struct VIOC *vioc,unsigned wrtvbn,int wrtblks,int reuse);
unsigned accesschunk(struct FCB *fcb,unsigned vbn,struct VIOC **retvioc,
                     char **retbuff,unsigned *retblocks,unsigned wrtblks);
unsigned access_extend(struct FCB *fcb,unsigned blocks,unsigned contig);
unsigned update_freecount(struct VCBDEV *vcbdev,unsigned *retcount);
unsigned update_create(struct VCB *vcb,struct fiddef *did,char *filename,
                       struct fiddef *fid,struct FCB **fcb);
unsigned update_extend(struct FCB *fcb,unsigned blocks,unsigned contig);
unsigned short checksum(vmsword *block);
