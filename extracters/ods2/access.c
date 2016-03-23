/* Access.c */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/*
        This module implements 'accessing' files on an ODS2
        disk volume. It uses its own low level interface to support
        'higher level' APIs. For example it is called by the
        'RMS' routines.
*/

#if !defined( DEBUG ) && defined( DEBUG_ACCESS )
#define DEBUG DEBUG_ACCCESS
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssdef.h"
#include "access.h"
#include "device.h"
#include "initvol.h"
#include "ods2.h"
#include "phyvirt.h"
#include "stsdef.h"
#include "compat.h"
#include "sysmsg.h"
#include "vmstime.h"

extern int mountdef( const char *devnam );

struct VCB *vcb_list = NULL;

/* WCBKEY passes info to compare/create routines */

struct WCBKEY {
    unsigned vbn;
    struct FCB *fcb;
    struct WCB *prevwcb;
};

unsigned deallocfile(struct FCB *fcb);  /* Update.c */
#define DEBUGx

/***************************************************************** checksum() */

/* checksum() to produce header checksum values... */

f11word checksumn( f11word *block, int count ) {
    register unsigned result;
    register unsigned short *ptr;
    register unsigned data;

    ptr = block;
    result = 0;
    for ( ; count > 0; --count ) {
        data = *ptr++;
        result += F11WORD(data);
    }
    return (f11word)(result & 0xFFFF);
}

f11word checksum( f11word *block ) {
    return checksumn( block, 255 );
}

/***************************************************************** fid_copy() */

/* fid_copy() copy fid from file header with default rvn! */

void fid_copy(struct fiddef *dst,struct fiddef *src,unsigned rvn)
{
    dst->fid$w_num = F11WORD(src->fid$w_num);
    dst->fid$w_seq = F11WORD(src->fid$w_seq);
    dst->fid$b_rvn = src->fid$b_rvn == 0 ? rvn : src->fid$b_rvn;
    dst->fid$b_nmx = src->fid$b_nmx;
}

/************************************************************* deaccesshead() */

/* deaccesshead() release header from INDEXF... */

unsigned deaccesshead( struct VIOC *vioc, struct HEAD *head, unsigned idxblk ) {

    f11word check;

    if ( head && idxblk ) {
        check = checksum( (f11word *) head );
        head->fh2$w_checksum = F11WORD(check);
    }
    return deaccesschunk( vioc, idxblk, 1, TRUE );
}

/*************************************************************** accesshead() */

/* accesshead() find file or extension header from INDEXF... */

unsigned accesshead(struct VCB *vcb,struct fiddef *fid,unsigned seg_num,
                    struct VIOC **vioc,struct HEAD **headbuff,
                    unsigned *retidxblk,unsigned wrtflg)
{
    register unsigned sts;
    register struct VCBDEV *vcbdev;
    register unsigned idxblk;
    vcbdev = RVN_TO_DEV(vcb,fid->fid$b_rvn);
    if (vcbdev == NULL) return SS$_DEVNOTMOUNT;
    if( wrtflg && !(vcb->status & VCB_WRITE) )
        return SS$_WRITLCK;
    idxblk = fid->fid$w_num +
                 (fid->fid$b_nmx << 16) - 1 +
                 F11WORD(vcbdev->home.hm2$w_ibmapvbn) +
                 F11WORD(vcbdev->home.hm2$w_ibmapsize);
    if (vcbdev->idxfcb->head != NULL) {
        if (idxblk >=
            F11SWAP(vcbdev->idxfcb->head->fh2$w_recattr.fat$l_efblk)) {
#if DEBUG
            printf("%u,%u,%u, %u Not in index file\n",
                   fid->fid$w_num,fid->fid$w_seq,fid->fid$b_rvn,fid->fid$b_nmx);
#endif
            return SS$_NOSUCHFILE;
        }
    }
    sts = accesschunk(vcbdev->idxfcb,idxblk,vioc,(char **) headbuff,NULL,
                      wrtflg ? 1 : 0);
    if (sts & STS$M_SUCCESS) {
        register struct HEAD *head = *headbuff;
        if (retidxblk) {
            *retidxblk = wrtflg ? idxblk : 0;
        }
        if (F11WORD(head->fh2$w_fid.fid$w_num) != fid->fid$w_num ||
            head->fh2$w_fid.fid$b_nmx != fid->fid$b_nmx ||
            F11WORD(head->fh2$w_fid.fid$w_seq) != fid->fid$w_seq ||
            (head->fh2$w_fid.fid$b_rvn != fid->fid$b_rvn &&
             head->fh2$w_fid.fid$b_rvn != 0)) {
            /* lib$signal(SS$_NOSUCHFILE); */
            sts = SS$_NOSUCHFILE;
        } else {
            if (head->fh2$b_idoffset < 38 ||
                head->fh2$b_idoffset > head->fh2$b_mpoffset ||
                head->fh2$b_mpoffset > head->fh2$b_acoffset ||
                head->fh2$b_acoffset > head->fh2$b_rsoffset ||
                head->fh2$b_map_inuse >
                    head->fh2$b_acoffset - head->fh2$b_mpoffset ||
                checksum( (f11word *) head ) !=
                    F11WORD( head->fh2$w_checksum ) ) {
#if DEBUG
                    printf( "--->accesshead(): File header checksum failure:" );
                    printf( " FH2$W_CHECKSUM=%u, checksum()=%u\n",
                            F11WORD( head->fh2$w_checksum ),
                            checksum( (f11word *) head )
                          );
#endif
                sts = SS$_DATACHECK;
            } else {
                if (F11WORD(head->fh2$w_seg_num) != seg_num) {
                    sts = SS$_FILESEQCHK;
                }
            }
        }
        if (!(sts & STS$M_SUCCESS)) {
            deaccesschunk(*vioc,0,0,FALSE);
        }
    }
    return sts;
}

/************************************************************** wcb_compare() */

/* wcb_compare() compare two windows - return -1 for less, 0 for match */

/*    as a by product keep highest previous entry so that if a new window
      is required we don't have to go right back to the initial file header */

static int wcb_compare( unsigned hashval, void *keyval, void *thiswcb ) {

    register struct WCBKEY *wcbkey = (struct WCBKEY *) keyval;
    register struct WCB *wcb = (struct WCB *) thiswcb;

    UNUSED(hashval);

    if (wcbkey->vbn < wcb->loblk) {
        return -1;              /* Search key is less than this window maps */
    } else {
        if (wcbkey->vbn <= wcb->hiblk) {
            return 0;           /* Search key must be in this window */
        } else {
            if (wcbkey->prevwcb == NULL) {
                wcbkey->prevwcb = wcb;
            } else {
                if (wcb->loblk != 0 && wcb->hiblk > wcbkey->prevwcb->hiblk) {
                    wcbkey->prevwcb = wcb;
                }
            }
            return 1;           /* Search key is higher than this window... */
        }
    }
}

/************************************************************ premap_indexf() */

/* premap_indexf() called to physically read the header for indexf.sys
   so that indexf.sys can be mapped and read into virtual cache.. */

struct HEAD *premap_indexf(struct FCB *fcb,unsigned *retsts)
{
    struct HEAD *head;
    struct VCBDEV *vcbdev = RVN_TO_DEV(fcb->vcb,fcb->rvn);
    if (vcbdev == NULL) {
        *retsts = SS$_DEVNOTMOUNT;
        return NULL;
    }
    head = (struct HEAD *) malloc(sizeof(struct HEAD));
    if (head == NULL) {
        *retsts = SS$_INSFMEM;
    } else {
        *retsts = virt_read( vcbdev->dev,
                              F11LONG( vcbdev->home.hm2$l_ibmaplbn ) +
                              F11WORD( vcbdev->home.hm2$w_ibmapsize ),
                              sizeof( struct HEAD ), (char *) head );
        if (!(*retsts & STS$M_SUCCESS)) {
            free(head);
            head = NULL;
        } else {
            if (F11WORD(head->fh2$w_fid.fid$w_num) != 1 ||
                head->fh2$w_fid.fid$b_nmx != 0 ||
                F11WORD(head->fh2$w_fid.fid$w_seq) != 1 ||
                F11WORD(head->fh2$w_checksum) !=
                   checksum( (f11word *) head ) ) {
#if DEBUG
                    printf( "--->premap_indexf(): Index file header checksum" );
                    printf( " failure: FH2$W_CHECKSUM=%u, checksum()=%u\n",
                            F11WORD( head->fh2$w_checksum ),
                            checksum( (f11word *) head )
                          );
#endif
                *retsts = SS$_DATACHECK;
                free(head);
                head = NULL;
            }
        }
    }
    return head;
}

/*************************************************************** wcb_create() */

/* wcb_create() creates a window control block by reading appropriate
   file headers... */

static void *wcb_create( unsigned hashval, void *keyval, unsigned *retsts ) {

    register struct WCB *wcb = (struct WCB *) malloc(sizeof(struct WCB));

    UNUSED(hashval);

    if (wcb == NULL) {
        *retsts = SS$_INSFMEM;
    } else {
        unsigned curvbn;
        unsigned extents = 0;
        struct HEAD *head;
        struct VIOC *vioc = NULL;
        register struct WCBKEY *wcbkey = (struct WCBKEY *) keyval;
        wcb->cache.objmanager = NULL;
        wcb->cache.objtype = OBJTYPE_WCB;
        if (wcbkey->prevwcb == NULL) {
            curvbn = wcb->loblk = 1;
            wcb->hd_seg_num = 0;
            head = wcbkey->fcb->head;
            if (head == NULL) {
                head = premap_indexf(wcbkey->fcb,retsts);
                if (head == NULL) return NULL;
                head->fh2$w_ext_fid.fid$w_num = 0;
                head->fh2$w_ext_fid.fid$b_nmx = 0;
            }
            fid_copy(&wcb->hd_fid,&head->fh2$w_fid,wcbkey->fcb->rvn);
        } else {
            wcb->loblk = wcbkey->prevwcb->hiblk + 1;
            curvbn = wcbkey->prevwcb->hd_basevbn;
            wcb->hd_seg_num = wcbkey->prevwcb->hd_seg_num;
            memcpy(&wcb->hd_fid,&wcbkey->prevwcb->hd_fid,sizeof(struct fiddef));
            head = wcbkey->fcb->head;
        }
        while (TRUE) {
            register unsigned short *mp;
            register unsigned short *me;
            wcb->hd_basevbn = curvbn;
            if (wcb->hd_seg_num != 0) {
                *retsts = accesshead(wcbkey->fcb->vcb,&wcb->hd_fid,
                                     wcb->hd_seg_num,&vioc,&head,NULL,0);
                if (!(*retsts & STS$M_SUCCESS)) {
                    free(wcb);
                    return NULL;
                }
            }
            mp = (unsigned short *) head + head->fh2$b_mpoffset;
            me = mp + head->fh2$b_map_inuse;
            while (mp < me) {
                register unsigned phylen,phyblk;
                register unsigned short mp0;
                switch ((mp0 = F11WORD(*mp)) >> 14) {
                    case 0:
                        phylen = 0;
                        mp++;
                        break;
                    case 1:
                        phylen =  (mp0 & 0x00ff) + 1;
                        phyblk = ((mp0 & 0x3f00) << 8) | F11WORD(mp[1]);
                        mp += 2;
                        break;
                    case 2:
                        phylen =  (mp0 & 0x3fff) + 1;
                        phyblk = (F11WORD(mp[2]) << 16) | F11WORD(mp[1]);
                        mp += 3;
                        break;
                    case 3:
                        phylen = ((mp0 & 0x3fff) << 16) + F11WORD(mp[1]) + 1;
                        phyblk = (F11WORD(mp[3]) << 16) | F11WORD(mp[2]);
                        mp += 4;
                        break;
                    default:
                        printf( "Unknown type %x\n", (F11WORD(*mp)>>14) );
                        abort();
                }
                curvbn += phylen;
                if (phylen != 0 && curvbn > wcb->loblk) {
                    wcb->phylen[extents] = phylen;
                    wcb->phyblk[extents] = phyblk;
                    wcb->rvn[extents] = wcb->hd_fid.fid$b_rvn;
                    if (++extents >= EXTMAX) {
                        if (curvbn > wcbkey->vbn) {
                            break;
                        } else {
                            extents = 0;
                            wcb->loblk = curvbn;
                        }
                    }
                }
            }
            if (extents >= EXTMAX ||
                (F11WORD(head->fh2$w_ext_fid.fid$w_num) == 0 &&
                head->fh2$w_ext_fid.fid$b_nmx == 0)) {
                break;
            } else {
                register unsigned rvn;
                wcb->hd_seg_num++;
                rvn = wcb->hd_fid.fid$b_rvn;
                fid_copy(&wcb->hd_fid,&head->fh2$w_ext_fid,rvn);
                if (vioc != NULL) deaccesshead(vioc,NULL,0);
            }
        }
        if (vioc != NULL) {
            deaccesshead(vioc,NULL,0);
        } else {
            if (wcbkey->fcb->head == NULL) free(head);
        }
        wcb->hiblk = curvbn - 1;
        wcb->extcount = extents;
        *retsts = SS$_NORMAL;
        if (curvbn <= wcbkey->vbn) {
            free(wcb);
#if DEBUG
            printf( "--->wcb_create(): curvbn (=%u) <= wcbkey->vbn (=%u)\n",
                    curvbn, wcbkey->vbn );
#endif
            *retsts = SS$_DATACHECK;
            wcb = NULL;
        }
    }
    return wcb;
}

/**************************************************************** getwindow() */

/* getwindow() find a window to map VBN to LBN ... */

unsigned getwindow(struct FCB * fcb,unsigned vbn,struct VCBDEV **devptr,
                   unsigned *phyblk,unsigned *phylen,struct fiddef *hdrfid,
                   unsigned *hdrseq)
{
    unsigned sts;
    struct WCB *wcb;
    struct WCBKEY wcbkey;
#if DEBUG
    printf("Accessing window for vbn %d, file (%x)\n",vbn,fcb->cache.hashval);
#endif
    wcbkey.vbn = vbn;
    wcbkey.fcb = fcb;
    wcbkey.prevwcb = NULL;
    wcb = cache_find((void *) &fcb->wcb,0,&wcbkey,&sts,wcb_compare,wcb_create);
    if (wcb == NULL) return sts;
    {
        register unsigned extent = 0;
        register unsigned togo = vbn - wcb->loblk;
        while (togo >= wcb->phylen[extent]) {
            togo -= wcb->phylen[extent];
            if (++extent > wcb->extcount) return SS$_BUGCHECK;
        }
        *devptr = RVN_TO_DEV(fcb->vcb,wcb->rvn[extent]);
        *phyblk = wcb->phyblk[extent] + togo;
        *phylen = wcb->phylen[extent] - togo;
        if (hdrfid != NULL) memcpy(hdrfid,&wcb->hd_fid,sizeof(struct fiddef));
        if (hdrseq != NULL) *hdrseq = wcb->hd_seg_num;
#if DEBUG
        printf("Mapping vbn %d to %d (%d -> %d)[%d] file (%x)\n",
               vbn,*phyblk,wcb->loblk,wcb->hiblk,wcb->hd_basevbn,
               fcb->cache.hashval);
#endif
        cache_untouch(&wcb->cache,TRUE);
    }
    if (*devptr == NULL) return SS$_DEVNOTMOUNT;
    return SS$_NORMAL;
}

/************************************************************* vioc_manager() */

/* Object manager for VIOC objects:- if the object has been
   modified then we need to flush it to disk before we let
   the cache routines do anything to it... */

void *vioc_manager(struct CACHE * cacheobj,int flushonly)
{
    register struct VIOC *vioc = (struct VIOC *) cacheobj;

    UNUSED(flushonly);

    if (vioc->modmask != 0) {
        register struct FCB *fcb = vioc->fcb;
        register unsigned int length = VIOC_CHUNKSIZE;
        register unsigned curvbn = vioc->cache.hashval + 1;
        register char *address = (char *) vioc->data;
        register unsigned modmask = vioc->modmask;
#if DEBUG
        printf("\nvioc_manager writing vbn %d\n",curvbn);
#endif
        do {
            register unsigned sts;
            unsigned int wrtlen = 0;
            unsigned phyblk,phylen;
            struct VCBDEV *vcbdev;
            while (length > 0 && !(1 & modmask) ) {
                length--;
                curvbn++;
                address += 512;
                modmask = modmask >> 1;
            }
            while (wrtlen < length && (1 & modmask) ) {
                wrtlen++;
                modmask = modmask >> 1;
            }
            length -= wrtlen;
            while (wrtlen > 0) {
                if (fcb->highwater != 0 && curvbn >= fcb->highwater) {
                    length = 0;
                    break;
                }
                sts = getwindow(fcb,curvbn,&vcbdev,&phyblk,&phylen,NULL,NULL);
                if (!(sts & STS$M_SUCCESS)) return NULL;
                if (phylen > wrtlen) phylen = wrtlen;
                if (fcb->highwater != 0 &&
                    curvbn + phylen > fcb->highwater) {
                    phylen = fcb->highwater - curvbn;
                }
                sts = virt_write( vcbdev->dev, phyblk, phylen * 512, address );
                if (!(sts & STS$M_SUCCESS)) return NULL;
                wrtlen -= phylen;
                curvbn += phylen;
                address += phylen * 512;
            }
        } while (length > 0 && modmask != 0);
        vioc->modmask = 0;
        vioc->cache.objmanager = NULL;
    }
    return cacheobj;
}

/************************************************************ deaccesschunk() */

/* deaccesschunk() to deaccess a VIOC (chunk of a file) */

unsigned deaccesschunk(struct VIOC *vioc,unsigned wrtvbn,
                       int wrtblks,int reuse)
{
#if DEBUG
    printf("Deaccess chunk %8x\n",vioc->cache.hashval);
#endif
    if (wrtvbn) {
        register unsigned modmask;
        if (wrtvbn <= vioc->cache.hashval ||
            wrtvbn + wrtblks > vioc->cache.hashval + VIOC_CHUNKSIZE + 1) {
            return SS$_BADPARAM;
        }
        modmask = 1 << (wrtvbn - vioc->cache.hashval - 1);
        while (--wrtblks > 0) modmask |= modmask << 1;
        if ((vioc->wrtmask | modmask) != vioc->wrtmask) return SS$_WRITLCK;
        vioc->modmask |= modmask;
        if (vioc->cache.refcount == 1) vioc->wrtmask = 0;
        vioc->cache.objmanager = vioc_manager;
    }
    cache_untouch(&vioc->cache,reuse);
    return SS$_NORMAL;
}

/************************************************************** vioc_create() */

static void *vioc_create( unsigned hashval, void *keyval, unsigned *retsts ) {

    register struct VIOC *vioc = (struct VIOC *) malloc(sizeof(struct VIOC));
    if (vioc == NULL) {
        *retsts = SS$_INSFMEM;
    } else {
        register unsigned int length;
        register unsigned curvbn = hashval + 1;
        register char *address;
        register struct FCB *fcb = (struct FCB *) keyval;
        vioc->cache.objmanager = NULL;
        vioc->cache.objtype = OBJTYPE_VIOC;
        vioc->fcb = fcb;
        vioc->wrtmask = 0;
        vioc->modmask = 0;
        length = fcb->hiblock - curvbn + 1;
        if (length > VIOC_CHUNKSIZE) length = VIOC_CHUNKSIZE;
        address = (char *) vioc->data;
        do {
            if (fcb->highwater != 0 && curvbn >= fcb->highwater) {
                memset(address,0,length * 512);
                break;
            } else {
                register unsigned sts;
                unsigned phyblk,phylen;
                struct VCBDEV *vcbdev;
                sts = getwindow(fcb,curvbn,&vcbdev,&phyblk,&phylen,NULL,NULL);
                if (sts & STS$M_SUCCESS) {
                    if (phylen > length) phylen = length;
                    if (fcb->highwater != 0 && curvbn + phylen > fcb->highwater) {
                        phylen = fcb->highwater - curvbn;
                    }
                    sts = virt_read( vcbdev->dev, phyblk, phylen * 512,
                                      address);
                }
                if (!(sts & STS$M_SUCCESS)) {
                    free(vioc);
                    *retsts = sts;
                    return NULL;
                }
                length -= phylen;
                curvbn += phylen;
                address += phylen * 512;
            }
        } while (length > 0);
        *retsts = SS$_NORMAL;
    }
    return vioc;
}

/************************************************************** accesschunk() */

/* accesschunk() return pointer to a 'chunk' of a file ... */

unsigned accesschunk(struct FCB *fcb,unsigned vbn,struct VIOC **retvioc,
                     char **retbuff,unsigned *retblocks,unsigned wrtblks)
{
    unsigned sts;
    register unsigned int blocks;
    register struct VIOC *vioc;
#if DEBUG
    printf("Access chunk %d (%x)\n",vbn,fcb->cache.hashval);
#endif
    if (vbn < 1 || vbn > fcb->hiblock) return SS$_ENDOFFILE;
    blocks = (vbn - 1) / VIOC_CHUNKSIZE * VIOC_CHUNKSIZE;
    if (wrtblks) {
        if( !(fcb->status & FCB_WRITE) )
            return SS$_WRITLCK;
        if (vbn + wrtblks > blocks + VIOC_CHUNKSIZE + 1) {
            return SS$_BADPARAM;
        }
    }
    vioc = cache_find((void *) &fcb->vioc,blocks,fcb,&sts,NULL,vioc_create);
    if (vioc == NULL) return sts;
    /*
        Return result to caller...
    */
    *retvioc = vioc;
    blocks = vbn - blocks - 1;
    *retbuff = vioc->data[blocks];
    if (wrtblks || retblocks != NULL) {
        register unsigned modmask = 1 << blocks;
        blocks = VIOC_CHUNKSIZE - blocks;
        if (vbn + blocks > fcb->hiblock) blocks = fcb->hiblock - vbn + 1;
        if (wrtblks && blocks > wrtblks) blocks = wrtblks;
        if (retblocks != NULL) *retblocks = blocks;
        if (wrtblks && blocks) {
            while (--blocks > 0) modmask |= modmask << 1;
            vioc->wrtmask |= modmask;
            vioc->cache.objmanager = vioc_manager;
        }
    }
    return SS$_NORMAL;
}

/************************************************************* deaccessfile() */

/* deaccessfile() finish accessing a file.... */

unsigned deaccessfile(struct FCB *fcb)
{
#if DEBUG
    printf("Deaccessing file (%x) reference %d\n",fcb->cache.hashval,
           fcb->cache.refcount);
#endif
    if (fcb->cache.refcount == 1) {
        register unsigned refcount;
        refcount = cache_refcount((struct CACHE *) fcb->wcb) +
            cache_refcount((struct CACHE *) fcb->vioc);
        if (refcount != 0) {
#if DEBUG
            printf("File reference counts non-zero %d  (%d)\n",refcount,
            fcb->cache.hashval);
#endif
#if DEBUG
            printf("File reference counts non-zero %d %d\n",
                   cache_refcount((struct CACHE *) fcb->wcb),
                   cache_refcount((struct CACHE *) fcb->vioc));
#endif
            return SS$_BUGCHECK;
        }
        if( fcb->status & FCB_WRITE ) {
            if (F11LONG(fcb->head->fh2$l_filechar) & FH2$M_MARKDEL) {
                return deallocfile(fcb);
            }
        }
    }
    cache_untouch(&fcb->cache,TRUE);
    return SS$_NORMAL;
}

/************************************************************** fcb_manager() */

/* Object manager for FCB objects:- we point to one of our
   sub-objects (vioc or wcb) in preference to letting the
   cache routines get us!  But we when run out of excuses
   it is time to clean up the file header...  :-(   */

void *fcb_manager(struct CACHE *cacheobj,int flushonly)
{
    register struct FCB *fcb = (struct FCB *) cacheobj;
    if (fcb->vioc != NULL) return &fcb->vioc->cache;
    if (fcb->wcb != NULL) return &fcb->wcb->cache;
    if (fcb->cache.refcount != 0 || flushonly) return NULL;
    if (fcb->headvioc != NULL) {
        deaccesshead(fcb->headvioc,fcb->head,fcb->headvbn);
        fcb->headvioc = NULL;
    }
    return cacheobj;
}

/*************************************************************** fcb_create() */

static void *fcb_create( unsigned filenum, void *keyval, unsigned *retsts ) {

    register struct FCB *fcb = (struct FCB *) malloc(sizeof(struct FCB));

    UNUSED(filenum);
    UNUSED(keyval);

    if (fcb == NULL) {
        *retsts = SS$_INSFMEM;
    } else {
        fcb->cache.objmanager = fcb_manager;
        fcb->cache.objtype = OBJTYPE_FCB;
        fcb->vcb = NULL;
        fcb->headvioc = NULL;
        fcb->head = NULL;
        fcb->wcb = NULL;
        fcb->vioc = NULL;
        fcb->headvbn = 0;
        fcb->hiblock = 100000;
        fcb->highwater = 0;
        fcb->status = 0;
        fcb->rvn = 0;
    }
    return fcb;
}

/*************************************************************** accessfile() */

/* accessfile() open up file for access... */

unsigned accessfile(struct VCB * vcb,struct fiddef * fid,struct FCB **fcbadd,
                    unsigned wrtflg)
{
    unsigned sts;
    register struct FCB *fcb;
    register unsigned filenum = (fid->fid$b_nmx << 16) + fid->fid$w_num;
#if DEBUG
    printf("Accessing file (%d,%d,%d)\n",(fid->fid$b_nmx << 16) +
           fid->fid$w_num,fid->fid$w_seq,fid->fid$b_rvn);
#endif
    if (filenum < 1) return SS$_BADPARAM;
    if( wrtflg && !(vcb->status & VCB_WRITE) )
        return SS$_WRITLCK;
    if (fid->fid$b_rvn > 1) filenum |= fid->fid$b_rvn << 24;
    fcb = cache_find((void *) &vcb->fcb,filenum,NULL,&sts,NULL,fcb_create);
    if (fcb == NULL) return sts;
    /* If not found make one... */
    *fcbadd = fcb;
    if (fcb->vcb == NULL) {
        fcb->rvn = fid->fid$b_rvn;
        if (fcb->rvn == 0 && vcb->devices > 1) fcb->rvn = 1;
        fcb->vcb = vcb;
    }
    if (wrtflg) {
        if( fcb->headvioc != NULL && !(fcb->status & FCB_WRITE) ) {
            deaccesshead(fcb->headvioc,NULL,0);
            fcb->headvioc = NULL;
        }
        fcb->status |= FCB_WRITE;
    }
    if (fcb->headvioc == NULL) {
        register unsigned sts;
        sts = accesshead(vcb,fid,0,&fcb->headvioc,&fcb->head,&fcb->headvbn,
                         wrtflg);
        if (sts & STS$M_SUCCESS) {
            fcb->hiblock = F11SWAP(fcb->head->fh2$w_recattr.fat$l_hiblk);
            if (fcb->head->fh2$b_idoffset > 39) {
                fcb->highwater = F11LONG(fcb->head->fh2$l_highwater);
            } else {
                fcb->highwater = 0;
            }
        } else {
#if DEBUG
            printf("Accessfile status %d\n",sts);
#endif
            fcb->cache.objmanager = NULL;
            cache_untouch(&fcb->cache,FALSE);
            cache_delete(&fcb->cache);
            return sts;
        }
    }
    return SS$_NORMAL;
}

/***************************************************************** dismount() */

/* dismount() finish processing on a volume */

unsigned dismount(struct VCB * vcb)
{
    register unsigned sts,device;
    struct VCBDEV *vcbdev;
    int expectfiles = vcb->devices;
    int openfiles = cache_refcount(&vcb->fcb->cache);
    if( vcb->status & VCB_WRITE )
        expectfiles *= 2;
#if DEBUG
    printf("Dismounting disk %d\n",openfiles);
#endif
    sts = SS$_NORMAL;
    if (openfiles != expectfiles) {
        sts = SS$_DEVNOTDISM;
    } else {
        vcbdev = vcb->vcbdev;
        for (device = 0; device < vcb->devices; device++) {
#if DEBUG
        printf( "--->dismount(): vcbdev[%d] = \"%s\"\n", device,
                vcbdev->dev != NULL ? vcbdev->dev->devnam : "NULL" );
#endif
            if (vcbdev->dev != NULL) {
                if (vcb->status & VCB_WRITE && vcbdev->mapfcb != NULL) {
                    sts = deaccessfile(vcbdev->mapfcb);
#if DEBUG
                    printf( "--->dismount(): %d = deaccessfile()\n", sts );
#endif
                    if (!(sts & STS$M_SUCCESS)) break;
                    vcbdev->idxfcb->status &= ~FCB_WRITE;
                    vcbdev->mapfcb = NULL;
                }
                cache_remove(&vcb->fcb->cache);
#if DEBUG
                printf( "--->dismount(): cache_remove()\n" );
#endif
                sts = deaccesshead(vcbdev->idxfcb->headvioc,
                                   vcbdev->idxfcb->head,
                                   vcbdev->idxfcb->headvbn);
#if DEBUG
                printf( "--->dismount(): %d = deaccesshead()\n", sts );
#endif
                if (!(sts & STS$M_SUCCESS)) break;
                vcbdev->idxfcb->headvioc = NULL;
                cache_untouch(&vcbdev->idxfcb->cache,FALSE);
#if DEBUG
                printf( "--->dismount(): cache_untouch()\n" );
#endif
                vcbdev->dev->vcb = NULL;
                virt_close( vcbdev->dev );
#if DEBUG
                printf( "--->dismount(): virt_close()\n" );
#endif
            }
            vcbdev++;
        }
        if (sts & STS$M_SUCCESS) {
            struct VCB *lp;

            cache_remove(&vcb->fcb->cache);
#if DEBUG
            printf( "--->dismount(): cache_remove()\n" );
#endif
            while (vcb->dircache) {
                cache_delete((struct CACHE *) vcb->dircache);
#if DEBUG
                printf( "--->dismount(): cache_delete()\n" );
#endif
            }
            for( lp = (struct VCB *)&vcb_list; lp->next != NULL; lp = lp->next ) {
                if( lp->next == vcb ) {
                    lp->next = vcb->next;
                    break;
                }
            }
            free(vcb);
        }
    }
    return sts;
}

/******************************************************************** mount() */

#if DEBUG
#ifndef HOME_SKIP
#define HOME_SKIP 1
#endif
#ifndef HOME_LIMIT
#define HOME_LIMIT 3
#endif
#else
#ifndef HOME_SKIP
#define HOME_SKIP 0
#endif
#ifndef HOME_LIMIT
#define HOME_LIMIT 1000
#endif
#endif

/* mount() make disk volume available for processing... */

unsigned mount( unsigned flags,
                unsigned devices,
                char *devnam[],
                char *label[] ) {
    register unsigned device,sts = 0;
    struct VCB *vcb;
    struct VCBDEV *vcbdev;
    struct VOLSETREC *volsetSYS = NULL;

#if DEBUG
    if (sizeof(struct HOME) != 512 || sizeof(struct HEAD) != 512) return SS$_NOTINSTALL;
#endif

    vcb = (struct VCB *) calloc( 1, sizeof(struct VCB) +
                                ((devices - 1) * sizeof(struct VCBDEV)) );
    if( vcb == NULL )
        return SS$_INSFMEM;

    vcb->status = 0;
    if( flags & MOU_WRITE )
        vcb->status |= VCB_WRITE;
    vcb->fcb = NULL;
    vcb->dircache = NULL;
    vcb->devices = 0;

    vcbdev = vcb->vcbdev;
    for( device = 0; device < devices; device++, vcbdev++ ) {
        char *dname;

        dname = devnam[device];
        sts = SS$_NOSUCHVOL;
        vcbdev->dev = NULL;
        if (strlen(dname)) { /* Really want to allow skipping volumes? */
            unsigned int hba, delta, homtry;

            if( label[device] != NULL && strlen(label[device]) >
                sizeof( volsetSYS[0].vsr$t_label ) ) {
                printf( "%%ODS2-E-BADPARAM, Label %s is too long\n",
                        label[device] );
                sts = SS$_BADPARAM;
                break;
            }
            sts = virt_open( &dname, flags, &vcbdev->dev );
            if( !(sts & STS$M_SUCCESS) )
                break;
            vcb->devices++;
            if (vcbdev->dev->vcb != NULL) {
                sts = SS$_DEVMOUNT;
                break;
            }
            delta = delta_from_index( (vcbdev->dev->access & MOU_DEVTYPE) >> MOU_V_DEVTYPE );

            for( hba = 1, homtry = 0; homtry < HOME_LIMIT; homtry++, hba += delta ) {
                struct HOME *hom;

#if HOME_SKIP > 100
                if( homtry < HOME_SKIP )
                    continue;
#endif
                sts = virt_read( vcbdev->dev, hba, sizeof( struct HOME ),
                                 (char *) &vcbdev->home );
                if (!(sts & STS$M_SUCCESS)) break;
                hom = &vcbdev->home;
#if defined( DEBUG ) || defined( HOME_LOG )
                printf( "--->mount(%u): LBA=%u, HM2$L_HOMELBN=%u, "
                        "HM2$L_ALHOMELBN=%u, "
                        "HM2$T_FORMAT=\"%12.12s\", memcmp()=%u\n",
                        homtry+1,hba, F11LONG( hom->hm2$l_homelbn ),
                        F11LONG( hom->hm2$l_alhomelbn ),
                        hom->hm2$t_format,
                        memcmp( hom->hm2$t_format, "DECFILE11B  ", 12 )
                      );
#endif
                if( (hba == F11LONG(hom->hm2$l_homelbn)) &&
                    (F11LONG(hom->hm2$l_alhomelbn) != 0) &&
                    (F11LONG(hom->hm2$l_altidxlbn) != 0) &&
                    (F11WORD(hom->hm2$w_homevbn)   != 0) &&
                    (F11LONG(hom->hm2$l_ibmaplbn)  != 0) &&
                    (F11WORD(hom->hm2$w_ibmapsize) != 0) &&
                    (F11WORD(hom->hm2$w_resfiles)  >= 5) &&
                    (F11WORD(hom->hm2$w_checksum1) ==
                     checksumn( (f11word *)hom,
                                (offsetof( struct HOME, hm2$w_checksum1 )/2) ))  &&
                    (F11WORD(hom->hm2$w_checksum2) ==
                     checksum( (f11word *)hom )) &&
                    (memcmp(hom->hm2$t_format,"DECFILE11B  ",12) == 0) ) {
                    break;
                }
#if defined( DEBUG ) || defined( HOME_LOG )
                printf( "--->mount(): Home block validation failure\n" );
                printf( "(F11LONG(hom->hm2$l_alhomelbn) != 0) %u\n", (F11LONG(hom->hm2$l_alhomelbn) != 0) );
                printf( "(F11LONG(hom->hm2$l_altidxlbn) != 0) %u\n", (F11LONG(hom->hm2$l_altidxlbn) != 0) );
                printf( "(F11WORD(hom->hm2$w_homevbn)   != 0) %u\n", (F11WORD(hom->hm2$w_homevbn)   != 0));
                printf( "(F11LONG(hom->hm2$l_ibmaplbn)  != 0) %u\n", (F11LONG(hom->hm2$l_ibmaplbn)  != 0) );
                printf( "(F11WORD(hom->hm2$w_ibmapsize) != 0) %u\n", (F11WORD(hom->hm2$w_ibmapsize) != 0));
                printf( "(F11WORD(hom->hm2$w_resfiles)  >= 5) %u\n", (F11WORD(hom->hm2$w_resfiles)  >= 5));
                printf( "(F11WORD(hom->hm2$w_checksum1) = %u %u\n", F11WORD(hom->hm2$w_checksum1),
                        checksumn( (f11word *)hom,
                                   (offsetof( struct HOME, hm2$w_checksum1 )/2) ) );
                printf( "(F11WORD(hom->hm2$w_checksum2) = %u %u\n", F11WORD(hom->hm2$w_checksum2),
                        checksum( (f11word *)hom ));
#endif
                sts = SS$_ENDOFFILE;
            }
            if( !(sts & STS$M_SUCCESS) )
                break;
            if( label[device] != NULL ) {
                int i;
                char lbl[12+1]; /* Pad CLI-supplied label to match ODS */
                (void) snprintf( lbl, sizeof(lbl), "%-12s", label[device] );
                for( i = 0; i < 12; i++ ) {
                    if( toupper( lbl[i] ) != toupper( vcbdev->home.hm2$t_volname[i] ) ) {
                        printf( "%%ODS2-W-WRONGVOL, Device %s contains volume '%12.12s', '%s' expected\n",
                                dname, vcbdev->home.hm2$t_volname, lbl );
                        sts = SS$_ITEMNOTFOUND;
                        break;
                    }
                }
                if( !(sts & STS$M_SUCCESS) )
                    break;
                if (flags & MOU_WRITE && !(vcbdev->dev->access & MOU_WRITE)) {
                    printf("%%MOUNT-W-WRITELOCK, %s is write locked\n",
                           dname);
                    vcb->status &= ~VCB_WRITE;
                }
            }
            if( (F11WORD(vcbdev->home.hm2$w_rvn) != device + 1) &&
                !(F11WORD(vcbdev->home.hm2$w_rvn) == 0 && device == 0) ) {
                    printf( "%%ODS2-E-WRONGVOL, Device %s contains RVN %u, RVN %u expected\n",
                             dname, F11WORD(vcbdev->home.hm2$w_rvn), device+1 );
                    sts = SS$_UNSUPVOLSET;
            }
            if( !(sts & STS$M_SUCCESS) )
                break;
        } /* for(each device) */
    }
    if (sts & STS$M_SUCCESS) {
        vcbdev = vcb->vcbdev;
        for( device = 0; device < devices; device++, vcbdev++ ) {
            char *dname;

            dname = devnam[device];
            sts = SS$_NOSUCHVOL;
            vcbdev->idxfcb = NULL;
            vcbdev->mapfcb = NULL;
            vcbdev->clustersize = 0;
            vcbdev->max_cluster = 0;
            vcbdev->free_clusters = 0;
            if (strlen(dname)) {
                struct fiddef idxfid = {1,1,0,0};
                idxfid.fid$b_rvn = device + 1;
                sts = accessfile( vcb, &idxfid, &vcbdev->idxfcb, (vcb->status & VCB_WRITE) != 0 );
                if (!(sts & STS$M_SUCCESS)) {
                    virt_close( vcbdev->dev );
                    vcbdev->dev = NULL;
                    continue;
                }
                vcbdev->dev->vcb = vcb;
                if( F11WORD(vcbdev->home.hm2$w_rvn) != 0 ) {
                    if( device == 0 ) {
                        struct fiddef vsfid = {6,6,1,0};
                        struct FCB *vsfcb = NULL;
                        struct VIOC *vioc = NULL;
                        unsigned recs = 0;
                        int rec;
                        unsigned int vbn = 1;
                        struct VOLSETREC *bufp;
                        int setcount = F11WORD(vcbdev->home.hm2$w_setcount);

                        if( setcount != (int)devices ) {
                            printf( "%%ODS2-E-VOLCOUNT, Volume set %12.12s has %u members, but %u specified\n",
                                    vcbdev->home.hm2$t_strucname, setcount, devices );
                            sts = SS$_DEVNOTMOUNT;
                            break;
                        }
                        /* Read VOLSET.SYS */
                        volsetSYS = (struct VOLSETREC *)malloc( (1+setcount) * sizeof( struct VOLSETREC ) );
                        sts = accessfile( vcb, &vsfid, &vsfcb, 0 );
                        if( !(sts & STS$M_SUCCESS) ) {
                            printf( "%%ODS2-E-NOVOLSET, Unable to access VOLSET.SYS: %s\n", getmsg(sts, MSG_TEXT) );
                            break;
                        }
                        for( rec = 0; rec <= setcount; rec++ ) {
                            if( recs == 0 ) {
                                if( vbn != 1 ) deaccesschunk(vioc,0,0,0);
                                sts = accesschunk(vsfcb,vbn, &vioc,(char **)&bufp, &recs, 0);
                                if( !(sts & STS$M_SUCCESS) )
                                    break;
                                vbn += recs;
                                recs *= 512 / sizeof( struct VOLSETREC );
                            }
                            memcpy(volsetSYS+rec, bufp++, sizeof( struct VOLSETREC ));
                        }
                        deaccesschunk(vioc,0,0,0);
                        { int st2;
                            st2 = deaccessfile(vsfcb);
                            if( sts & STS$M_SUCCESS ) sts = st2;
                        }
                        if( !(sts & STS$M_SUCCESS) )  {
                            printf( "%%ODS2-E-VOLIOERR, Error reading VOLSET.SYS: %s\n", getmsg(sts, MSG_TEXT) );
                            break;
                        }
                        if( memcmp(vcbdev->home.hm2$t_strucname, volsetSYS[0].vsr$t_label, 12 ) != 0 ) {
                            printf( "%%ODS2-E-INCONVOL, Volume set name is '%12.12s', but VOLSET.SYS is for '%12.12s'\n",
                                    vcbdev->home.hm2$t_strucname, volsetSYS[0].vsr$t_label );
                            sts = SS$_NOSUCHVOL;
                            break;
                        }
                    } else { /* device != 0 */
                        if( vcb->vcbdev[0].dev == NULL ) {
                            printf( "%%ODS2-F-NORVN1, RVN 1 must be mounted\n" );
                            sts = SS$_NOSUCHVOL;
                            break;
                        }
                        if( memcmp(vcbdev->home.hm2$t_strucname, vcb->vcbdev[0].home.hm2$t_strucname, 12) != 0 ) {
                            printf( "%%ODS2-E-INCONVOL, Volume '%12.12s' on %s is a member of '%12.12s', not a member of '%12.12s'\n",
                                    vcbdev->home.hm2$t_volname, dname, vcbdev->home.hm2$t_strucname,
                                    vcb->vcbdev[0].home.hm2$t_strucname );
                            sts = SS$_NOSUCHVOL;
                            break;
                        }
                    }

                    if( memcmp(vcbdev->home.hm2$t_volname, volsetSYS[device+1].vsr$t_label, 12 ) != 0 ) {
                        printf( "%%ODS2-E-WRONGVOL, RVN %u of '%12.12s' is '%12.12s'.  %s contains '%12.12s'\n",
                                device+1, vcb->vcbdev[0].home.hm2$t_strucname,
                                volsetSYS[device+1].vsr$t_label, dname, vcbdev->home.hm2$t_volname );
                        sts = SS$_NOSUCHVOL;
                        break;
                    }
                } /* rvn != 0 */

                if( vcb->status & VCB_WRITE ) {
                    struct fiddef mapfid = { 2, 2, 0, 0 };
                    mapfid.fid$b_rvn = device + 1;
                        sts = accessfile( vcb,&mapfid, &vcbdev->mapfcb, TRUE );
                        if( sts & STS$M_SUCCESS ) {
                            struct VIOC *vioc;
                            struct SCB *scb;

/* TODO *** scb->scb$w_checksum == checksum( (f11word *)scb */
                            sts = accesschunk( vcbdev->mapfcb, 1, &vioc,
                                              (char **) &scb, NULL ,0 );
                            if( sts & STS$M_SUCCESS ) {
                                if( scb->scb$w_cluster ==
                                    vcbdev->home.hm2$w_cluster ) {
                                    vcbdev->clustersize =
                                        F11WORD( vcbdev->home.hm2$w_cluster );
                                    vcbdev->max_cluster =
                                        (F11LONG(scb->scb$l_volsize) +
                                         F11WORD(scb->scb$w_cluster) - 1) /
                                        F11WORD(scb->scb$w_cluster);
                                    deaccesschunk(vioc,0,0,FALSE);
                                    sts = update_freecount(
                                                           vcbdev,&vcbdev->free_clusters
                                                           );
#if DEBUG
                                    printf( "%d of %d blocks are free on %12.12s\n",
                                            vcbdev->free_clusters * vcbdev->clustersize, scb->scb$l_volsize,
                                        vcbdev->home.hm2$t_volname );
#endif
                                } else {
                                    deaccesschunk(vioc,0,0,FALSE);
                                }
                        }
                } else {
                    printf( "%%ODS2-E-NOBITMAP, Unable to access BITMAP.SYS: %s\n", getmsg(sts, MSG_TEXT) );
                        vcbdev->mapfcb = NULL;
                        break;
                    }
                }
                if( (sts & STS$M_SUCCESS) && (flags & MOU_LOG) ) {
                    printf( "%%MOUNT-I-MOUNTED, Volume %12.12s mounted on %s\n",
                            vcbdev->home.hm2$t_volname, vcbdev->dev->devnam );
                }
            } /* device len */
        } /* for( each device ) */
        if( !(sts & STS$M_SUCCESS) ) {
            vcbdev = vcb->vcbdev;
            for( device = 0; device < devices; device++, vcbdev++ ) {
                if (vcbdev->dev == NULL) {
                    continue;
                }

                if( vcb->status & VCB_WRITE && vcbdev->mapfcb != NULL ) {
                    /* sts = */
                    deaccessfile(vcbdev->mapfcb);
                    /* if( !(sts & STS$M_SUCCESS) ) ??; */
                    vcbdev->idxfcb->status &= ~FCB_WRITE;
                    vcbdev->mapfcb = NULL;
                }
                cache_remove( &vcb->fcb->cache );
                /* sts = */
                deaccesshead(vcbdev->idxfcb->headvioc,vcbdev->idxfcb->head,vcbdev->idxfcb->headvbn);
                /* if (!(sts & STS$M_SUCCESS)) ??; */
                vcbdev->idxfcb->headvioc = NULL;
                cache_untouch(&vcbdev->idxfcb->cache,0);
                vcbdev->dev->vcb = NULL;
                sts = virt_close( vcbdev->dev );
            }
            cache_remove( &vcb->fcb->cache );
            while( vcb->dircache ) cache_delete( (struct CACHE *) vcb->dircache );
        }
    } else {
        vcbdev = vcb->vcbdev;
        for( device = 0; device < vcb->devices; device++, vcbdev++ ) {
            if (vcbdev->dev == NULL) {
                continue;
            }
            virt_close( vcbdev->dev );
        }
        free(vcb);
        vcb = NULL;
    }
    if( (sts & STS$M_SUCCESS) && (flags & MOU_LOG) && F11WORD(vcb->vcbdev[0].home.hm2$w_rvn) != 0 ) {
        printf ( "%%MOUNT-I-MOUNTEDVS, Volume set %12.12s mounted\n", vcb->vcbdev[0].home.hm2$t_strucname );
    }
    if( vcb != NULL ) {
        struct VCB *lp;

        for( lp = (struct VCB *)&vcb_list; lp->next != NULL; lp = lp->next )
            ;
        lp->next = vcb;
        vcb->next = NULL;
    }
    if( volsetSYS != NULL ) free( volsetSYS );

    if( sts & STS$M_SUCCESS )
        (void) mountdef( vcb->vcbdev[0].dev->devnam );

    return sts;
}

/***************************************************************** show_volumes */
/* First, some private helper routines */

/********************************************************** print_volhdr */
static void print_volhdr( int volset, size_t devwid ) {
    size_t i;

    if( volset )
        printf( "    RVN " );
    else
        printf( "  " );
    printf(
"%-*s Volume/User  Lvl Clust Owner/CreateDate  VolProt/Default FileProt\n",
            (int)devwid, "Dev" );
    if( volset )
        printf( "    --- " );
    else
        printf( "  " );

    for( i= 0; i < devwid; i++ )
        putchar( '-' );
    printf(
" ------------ --- ----- ----------------- ---------------------------\n" );

    return;
}

/********************************************************** print_prot() */
static void print_prot( f11word code, int vol ) {
    static const char grp[4] = { "SOGW" };
    static const char acc[2][4] = {{"RWED"}, {"RWCD"}};
    int g, b, w;

    w = 27;

    for( g = 0; g < 4; g++ ) {
        w -= printf( "%c:", grp[g] );
        for( b = 0; b < 4; b++ ) {
            if( !(code & (1<<(b + (4*g)))) ) {
                putchar( acc[vol][b] );
                w--;
            }
        }
        if( g < 3 ) {
            putchar( ',' );
            w--;
        }
    }
    while( w-- ) putchar( ' ' );
}

/********************************************************** print_volinf() */
static void print_volinf( struct VCB *vcb,
                          size_t devwid,
                          unsigned device,
                          size_t wrap ) {
    struct VCBDEV *vcbdev;
    struct SCB *scb = NULL;
    struct VIOC *vioc = NULL;
    unsigned sts;
    size_t n;
    f11word timbuf[7];

    vcbdev = vcb->vcbdev+device;

    if( !(vcb->status & VCB_WRITE) ) {
        struct fiddef mapfid = { 2, 2, 0, 0 };

        if( !(( sts = accessfile( vcb, &mapfid, &vcbdev->mapfcb, 0 )) & STS$M_SUCCESS) ) {
            printf( " *Can't access BITMAP.SYS %s\n", getmsg( sts, MSG_TEXT ) );
            return;
        }
    }
    if( (sts = accesschunk( vcbdev->mapfcb, 1, &vioc, (char **)&scb, NULL, 0 )) & STS$M_SUCCESS ) {
        if( scb->scb$w_cluster == vcbdev->home.hm2$w_cluster ) {
            vcbdev->clustersize = F11WORD(vcbdev->home.hm2$w_cluster);
            vcbdev->max_cluster = (F11LONG(scb->scb$l_volsize) +
                                   F11WORD(scb->scb$w_cluster) - 1) /
                F11WORD(scb->scb$w_cluster);
            deaccesschunk(vioc,0,0,FALSE);
            if( !(vcb->status & VCB_WRITE) )
                sts = update_freecount( vcbdev, &vcbdev->free_clusters );
        } else {
            printf(" SCB data doesn't match HOM %u / %u ",
                   F11WORD(scb->scb$w_cluster), F11WORD(vcbdev->home.hm2$w_cluster) );
            deaccesschunk(vioc,0,0,FALSE);
        }
    } else {
        printf( " *Can't read SCB %s", getmsg( sts, MSG_TEXT ) );
    }
    if( !(vcb->status & VCB_WRITE) )
        deaccessfile( vcbdev->mapfcb );

    for( n = 0; n < strlen( vcbdev->dev->devnam ); n++ ) {
        if( vcbdev->dev->devnam[n] == ':' )
            break;
        putchar( vcbdev->dev->devnam[n] );
    }
    printf( "%*s ", (int)(devwid-n), "" );

    printf( "%12.12s", vcbdev->home.hm2$t_volname );

    printf( " %u.%u %5u [%6o,%6o]",
            F11WORD( vcbdev->home.hm2$w_struclev ) >> 8,
            F11WORD( vcbdev->home.hm2$w_struclev ) & 0xFF,
            F11WORD( vcbdev->home.hm2$w_cluster ),
            F11WORD( vcbdev->home.hm2$w_volowner.uic$w_grp ),
            F11WORD( vcbdev->home.hm2$w_volowner.uic$w_mem )
            );
    printf( "   " );
    print_prot( F11WORD( vcbdev->home.hm2$w_protect ), 1 );
    if( !(vcb->status & VCB_WRITE) )
        printf( " write-locked" );

    if( sts & STS$M_SUCCESS ) {
        disktypep_t dp;
        int first = 1;

        for( dp = disktype; dp->name != NULL; dp++ ) {
            unsigned size;
            unsigned blksize = 1;

            size = dp->sectors * dp->tracks * dp->cylinders - dp->reserved;
            if( dp->sectorsize > 512 )
                size = size * dp->sectorsize / 512;
            else {
                if( dp->sectorsize < 512 ) {
                    blksize = (512 / dp->sectorsize);
                    size = size / blksize;
                }
            }
            if( scb->scb$l_volsize == size &&
                scb->scb$l_blksize == blksize &&
                scb->scb$l_sectors == dp->sectors &&
                scb->scb$l_tracks == dp->tracks &&
                scb->scb$l_cylinders == dp->cylinders ) {
                if( first ) {
                    printf( " type:" );
                    first = 0;
                } else
                    printf( "|" );
                printf( " %s", dp->name );
            }
        }
        if( first )
            printf( " Unrecognized type" );
    }

    printf( "\n%*s %12.12s           ", (int)(wrap+devwid), "",
            vcbdev->home.hm2$t_ownername );

    if( sys$numtim( timbuf, vcbdev->home.hm2$q_credate ) & STS$M_SUCCESS ) {
        static const char *months =
            "-JAN-FEB-MAR-APR-MAY-JUN-JUL-AUG-SEP-OCT-NOV-DEC-";

        printf( "%2u%.5s%4u %2u:%2u ", timbuf[2],
                months+(4*(timbuf[1]-1)), timbuf[0],
                timbuf[3], timbuf[4]);
    } else
        printf( "%*s", 41-23, "" );

    print_prot( F11WORD( vcbdev->home.hm2$w_fileprot ), 0 );

    if( sts & STS$M_SUCCESS ) {
        printf( " Free: %u/%u", vcbdev->free_clusters * vcbdev->clustersize,
                scb->scb$l_volsize );
        printf( " Geo: %u/%u/%u", F11LONG(scb->scb$l_sectors),
                F11LONG(scb->scb$l_tracks), F11LONG(scb->scb$l_cylinders) );
        if( F11LONG(scb->scb$l_blksize) != 1 )
            printf( " %u phys blks/LBN", F11LONG(scb->scb$l_blksize) );
    }
    putchar( '\n' );
}

/********************************************************** show_volumes() */
void show_volumes( void ) {
    struct VCB *vcb;
    size_t maxd =  sizeof( "Dev" ) -1;
    int nvol = 0;
    unsigned device;

    if( vcb_list == NULL ) {
        printf( " No volumes mounted\n" );
        return;
    }

    for( vcb = vcb_list; vcb != NULL; vcb = vcb->next ) {
        struct VCBDEV *vcbdev;

        if( vcb->devices == 0 ) {
            printf( "No devices for volume\n" );
            abort();
        }
        nvol++;
        vcbdev = vcb->vcbdev;
        for( device = 0; device < vcb->devices; device++, vcbdev++ ) {
            size_t n;
            for( n = 0; n < strlen( vcbdev->dev->devnam ); n++ )
                if( vcbdev->dev->devnam[n] == ':' )
                    break;
            if( n > maxd )
                maxd = n;
        }
    }
    for( vcb = vcb_list; vcb != NULL; vcb = vcb->next ) {
        struct VCBDEV *vcbdev;
        unsigned device;

        vcbdev = vcb->vcbdev;
        if( F11WORD(vcbdev->home.hm2$w_rvn) == 0 )
            continue;

        nvol--;
        printf( "  Volume set %12.12s\n", vcbdev->home.hm2$t_strucname );

        print_volhdr( TRUE, maxd );

        for( device = 0; device < vcb->devices; device++, vcbdev++ ) {
            printf( "    %3d ", F11WORD(vcbdev->home.hm2$w_rvn) );
            print_volinf( vcb, maxd, device, 8 );
        }
    }
    if( nvol == 0 )
        return;

    printf( "\n" );

    print_volhdr( FALSE, maxd );
    for( vcb = vcb_list; vcb != NULL; vcb = vcb->next ) {
        struct VCBDEV *vcbdev;
        unsigned device;

        vcbdev = vcb->vcbdev;
        if( F11WORD(vcbdev->home.hm2$w_rvn) != 0 )
            continue;

        vcbdev = vcb->vcbdev;
        for( device = 0; device < vcb->devices; device++, vcbdev++ ) {
            printf( "  " );
            print_volinf( vcb, maxd, device, 2 );
        }
    }

    return;
}

/*************************************************************** acccess_rundown() */
void access_rundown( void ) {
    struct VCB *vcb, *next;
    unsigned sts;

    for( vcb = vcb_list; vcb != NULL; vcb = next ) {
        next = vcb->next;

        sts = dismount( vcb );
        if( !(sts & STS$M_SUCCESS) ) {
            printf( "Dismount failed in rundown: %s\n", getmsg(sts, MSG_TEXT) );
        }
    }
}

