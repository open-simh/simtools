/* Access.h    Definitions for file access routines */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

#ifndef _ACCESS_H
#define _ACCESS_H

#include "cache.h"
#include "f11def.h"
#include "vmstime.h"

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
#define FCB_WRITE 1             /* FCB open for write... */
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
    struct VCB *next;           /* Next in mounted volume list (Must be first item) */
    unsigned status;            /* Volume status */
    unsigned devices;           /* Number of volumes in set */
    struct FCB *fcb;            /* File control block tree */
    struct DIRCACHE *dircache;  /* Directory cache tree */
    struct VCBDEV vcbdev[1];    /* List of volumes devices */
};                              /* Volume control block */

extern struct VCB *vcb_list;
void show_volumes( void );

/* RVN_TO_DEV( vcb, rvn ) - returns device from relative volume number */

/* returns NULL if RVN illegal or device not mounted */

#define RVN_TO_DEV( vcb, rvn ) ( ( rvn < 2 ) ? \
                                     vcb->vcbdev : \
                                     ( ( rvn <= vcb->devices ) ? \
                                           &vcb->vcbdev[rvn - 1] : \
                                           NULL \
                                     ) \
                               )

void fid_copy(struct fiddef *dst,struct fiddef *src,unsigned rvn);

unsigned dismount(struct VCB *vcb);
unsigned mount(unsigned flags,unsigned devices,char *devnam[],char *label[] );

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
f11word checksumn( f11word *block, int count );
f11word checksum( f11word *block );

void access_rundown( void );

#endif /* # ifndef _ACCESS_H */
