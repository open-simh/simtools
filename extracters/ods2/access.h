/* Access.h    Definitions for file access routines */

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

#ifndef _ACCESS_H
#define _ACCESS_H

#include "ods2.h"
#include "cache.h"
#include "f11def.h"
#include "vmstime.h"

#define EXTMAX 20

struct WCB {
    struct CACHE cache;
    uint32_t loblk, hiblk;      /* Range of window */
    uint32_t hd_basevbn;        /* File blocks prior to header */
    unsigned hd_seg_num;        /* Header segment number */
    struct fiddef hd_fid;       /* Header FID */
    unsigned short extcount;    /* Extents in use */
    uint32_t phylen[EXTMAX];
    uint32_t phyblk[EXTMAX];
    uint8_t rvn[EXTMAX];
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
    uint32_t headvbn;           /* vbn for file header */
    uint32_t hiblock;           /* Highest block mapped */
    uint32_t highwater;         /* First high water block */
    unsigned char status;       /* FCB status bits */
#define FCB_WRITE 1             /* FCB open for write... */
#define FCB_WRITTEN 2           /* Modified */
#define FCB_RDTSET 4            /* Revision date set by create */

    uint8_t rvn;                /* Initial file relative volume */
};                              /* File control block */

struct DIRCACHE {
    struct CACHE cache;
    struct fiddef parent;
    struct dir$r_ent entry;
    struct dir$r_rec record;
};

#define VCB_WRITE 1
struct VCBDEV {
    struct DEV *dev;            /* Pointer to device info */
    struct FCB *idxfcb;         /* Index file control block */
    struct FCB *mapfcb;         /* Bitmap file control block */
    uint32_t clustersize;       /* Cluster size of the device */
    uint32_t max_cluster;       /* Total clusters on the device */
    uint32_t free_clusters;     /* Free clusters on disk volume */
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

#define RVN_TO_DEV( vcb, rvn ) ( ( rvn < 2 ) ?                  \
                                 vcb->vcbdev :                  \
                                 ( ( rvn <= vcb->devices ) ?    \
                                   &vcb->vcbdev[rvn - 1] :      \
                                   NULL                         \
                                   )                            \
                                 )

struct NEWFILE {
    struct RECATTR recattr;
    VMSTIME credate;
    VMSTIME revdate;
    VMSTIME expdate;
    VMSTIME bakdate;
    f11word verlimit;
    f11word revision;
    uint32_t fileprot;
    f11long filechar;
    struct UIC fileowner;
};

void fid_copy( struct fiddef *dst, struct fiddef *src, unsigned rvn );

vmscond_t dismount( struct VCB *vcb, options_t options);
vmscond_t mount( options_t flags, unsigned devices, char *devnam[], char *label[] );

vmscond_t accesserase( struct VCB *vcb, struct fiddef *fid );
vmscond_t deaccessfile( struct FCB *fcb );
vmscond_t accessfile( struct VCB *vcb, struct fiddef *fid,
                      struct FCB **fcb, unsigned wrtflg );

vmscond_t deaccesschunk( struct VIOC *vioc, uint32_t wrtvbn, int wrtblks, int reuse );
vmscond_t accesschunk( struct FCB *fcb, uint32_t vbn, struct VIOC **retvioc,
                       char **retbuff, uint32_t *retblocks, uint32_t wrtblks );
vmscond_t access_extend( struct FCB *fcb, uint32_t blocks, unsigned contig );

vmscond_t deallocfile(struct FCB *fcb);

f11word checksumn( f11word *block, int count );
f11word checksum( f11word *block );

void access_rundown( void );

/* These live in update.c, which contains the write part of access */

vmscond_t update_freecount( struct VCBDEV *vcbdev, uint32_t *retcount );
vmscond_t update_create( struct VCB *vcb, struct fiddef *did, char *filename,
                         struct fiddef *fid, struct NEWFILE *attrs, struct FCB **fcb );
vmscond_t update_extend( struct FCB *fcb, uint32_t blocks, unsigned contig );

vmscond_t update_truncate( struct FCB *fcb, uint32_t newsize );

/* These are used by update.c */

vmscond_t deaccesshead( struct VIOC *vioc, struct HEAD *head, uint32_t idxblk );
vmscond_t accesshead( struct VCB *vcb, struct fiddef *fid, uint32_t seg_num,
                      struct VIOC **vioc, struct HEAD **headbuff,
                      uint32_t *retidxblk, unsigned wrtflg );
vmscond_t getwindow( struct FCB * fcb, uint32_t vbn, struct VCBDEV **devptr,
                     uint32_t *phyblk, uint32_t *phylen, struct fiddef *hdrfid,
                     uint32_t *hdrseq );

#endif /* # ifndef _ACCESS_H */
