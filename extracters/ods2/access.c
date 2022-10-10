/* Access.c */

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

/*
 *      This module implements 'accessing' files on an ODS2
 *      disk volume. It uses its own low level interface to support
 *      'higher level' APIs. For example it is called by the
 *      'RMS' routines.
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

extern vmscond_t mountdef( const char *devnam );

struct VCB *vcb_list = NULL;

/* WCBKEY passes info to compare/create routines */

struct WCBKEY {
    unsigned vbn;
    struct FCB *fcb;
    struct WCB *prevwcb;
};

static vmscond_t get_scb( struct VCB *vcb, unsigned device, struct SCB *scb );

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

void fid_copy( struct fiddef *dst, struct fiddef *src, unsigned rvn ) {
    dst->fid$w_num = F11WORD(src->fid$w_num);
    dst->fid$w_seq = F11WORD(src->fid$w_seq);
    dst->fid$b_rvn = src->fid$b_rvn == 0 ? rvn : src->fid$b_rvn;
    dst->fid$b_nmx = src->fid$b_nmx;
}

/************************************************************* deaccesshead() */

/* deaccesshead() release header from INDEXF... */

vmscond_t deaccesshead( struct VIOC *vioc, struct HEAD *head, uint32_t idxblk ) {
    f11word check;

    if ( head && idxblk ) {
        check = checksum( (f11word *) head );
        head->fh2$w_checksum = F11WORD(check);
    }
    return deaccesschunk( vioc, idxblk, 1, TRUE );
}

/*************************************************************** accesshead() */

/* accesshead() find file or extension header from INDEXF... */

vmscond_t accesshead( struct VCB *vcb, struct fiddef *fid, unsigned seg_num,
                      struct VIOC **vioc, struct HEAD **headbuff,
                      uint32_t *retidxblk, unsigned wrtflg ) {
    register vmscond_t sts;
    register struct VCBDEV *vcbdev;
    register uint32_t idxblk;

    vcbdev = RVN_TO_DEV(vcb, fid->fid$b_rvn);
    if( vcbdev == NULL )
        return SS$_DEVNOTMOUNT;

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
                   fid->fid$w_num, fid->fid$w_seq, fid->fid$b_rvn, fid->fid$b_nmx);
#endif
            return SS$_NOSUCHFILE;
        }
    }

    if( $SUCCESSFUL(sts = accesschunk( vcbdev->idxfcb, idxblk, vioc,
                                      (char **) headbuff, NULL,
                                      wrtflg ? 1 : 0 )) ) {
        register struct HEAD *head = *headbuff;
        if( retidxblk ) {
            *retidxblk = wrtflg ? idxblk : 0;
        }
        if (F11WORD(head->fh2$w_fid.fid$w_num) != fid->fid$w_num ||
            head->fh2$w_fid.fid$b_nmx != fid->fid$b_nmx ||
            F11WORD(head->fh2$w_fid.fid$w_seq) != fid->fid$w_seq ||
            (head->fh2$w_fid.fid$b_rvn != fid->fid$b_rvn &&
             head->fh2$w_fid.fid$b_rvn != 0)) {
            sts = SS$_NOSUCHFILE;
        } else if( head->fh2$b_idoffset < 38 ||
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
            sts = $SETLEVEL(SS$_BADCHKSUM, SEVERE);
        } else if( F11WORD(head->fh2$w_seg_num) != seg_num ) {
            sts = SS$_FILESEQCHK;
        }

        if( $FAILED(sts) ) {
            deaccesschunk( *vioc, 0, 0, FALSE );
        }
    }
    return sts;
}

/************************************************************** wcb_compare() */

/* wcb_compare() compare two windows - return -1 for less, 0 for match */

/*    as a by product keep highest previous entry so that if a new window
 *    is required we don't have to go right back to the initial file header
 */

static int wcb_compare( uint32_t hashval, void *keyval, void *thiswcb ) {

    register struct WCBKEY *wcbkey;
    register struct WCB *wcb;

    UNUSED(hashval);

    wcbkey = (struct WCBKEY *) keyval;
    wcb = (struct WCB *) thiswcb;

    if( wcbkey->vbn < wcb->loblk ) {
        return -1;              /* Search key is less than this window maps */
    }

    if (wcbkey->vbn <= wcb->hiblk) {
        return 0;               /* Search key must be in this window */
    }
    if (wcbkey->prevwcb == NULL) {
        wcbkey->prevwcb = wcb;
    } else {
        if (wcb->loblk != 0 && wcb->hiblk > wcbkey->prevwcb->hiblk) {
            wcbkey->prevwcb = wcb;
        }
    }
    return 1;                   /* Search key is higher than this window... */
}

/************************************************************ premap_indexf() */

/* premap_indexf() called to physically read the header for indexf.sys
 * so that indexf.sys can be mapped and read into virtual cache..
 */

struct HEAD *premap_indexf( struct FCB *fcb, vmscond_t *retsts ) {
    struct HEAD *head;
    struct VCBDEV *vcbdev;
    int iderr;

    vcbdev = RVN_TO_DEV( fcb->vcb, fcb->rvn );
    if( vcbdev == NULL ) {
        *retsts = SS$_DEVNOTMOUNT;
        return NULL;
    }
    head = (struct HEAD *) malloc(sizeof(struct HEAD));
    if( head == NULL ) {
        *retsts = SS$_INSFMEM;
        return NULL;
    }

    if( $FAILS(*retsts = virt_read( vcbdev->dev,
                                     F11LONG( vcbdev->home.hm2$l_ibmaplbn ) +
                                     F11WORD( vcbdev->home.hm2$w_ibmapsize ),
                                     sizeof( struct HEAD ), (char *) head )) ) {
        free( head );
        return NULL;
    }
    if( (iderr = (F11WORD(head->fh2$w_fid.fid$w_num) != FID$C_INDEXF ||
                  head->fh2$w_fid.fid$b_nmx != 0 ||
                  F11WORD(head->fh2$w_fid.fid$w_seq) != FID$C_INDEXF)) ||
        F11WORD(head->fh2$w_checksum) != checksum( (f11word *) head ) ) {
#if DEBUG
        printf( "--->premap_indexf(): Index file header %s" );
        printf( " failure: FH2$W_CHECKSUM=%u, checksum()=%u\n",
                (iderr? "File ID match" : "checksum"),
                F11WORD( head->fh2$w_checksum ),
                checksum( (f11word *) head )
                );
#endif
        *retsts = $SETLEVEL((iderr? SS$_FILENUMCHK: SS$_BADCHKSUM), SEVERE);
        free( head );
        return NULL;
    }

    return head;
}

/*************************************************************** wcb_create() */

/* wcb_create() creates a window control block by reading appropriate
 * file headers...
 */

static void *wcb_create( uint32_t hashval, void *keyval, vmscond_t *retsts ) {
    register struct WCB *wcb;
    uint32_t curvbn;
    unsigned extents = 0;
    struct HEAD *head;
    struct VIOC *vioc = NULL;
    register struct WCBKEY *wcbkey;

    UNUSED(hashval);

    wcb = (struct WCB *) calloc( sizeof(struct WCB), 1 );

    if( wcb == NULL ) {
        *retsts = SS$_INSFMEM;
        return NULL;
    }
    wcbkey = (struct WCBKEY *) keyval;
    wcb->cache.objmanager = NULL;
    wcb->cache.objtype = OBJTYPE_WCB;

    if( wcbkey->prevwcb == NULL ) {
        curvbn =
            wcb->loblk = 1;
        wcb->hd_seg_num = 0;
        head = wcbkey->fcb->head;
        if( head == NULL ) {
            head = premap_indexf( wcbkey->fcb, retsts );
            if (head == NULL) {
                free( wcb );
                return NULL;
            }
            head->fh2$w_ext_fid.fid$w_num = 0;
            head->fh2$w_ext_fid.fid$b_nmx = 0;
        }
        fid_copy(&wcb->hd_fid, &head->fh2$w_fid, wcbkey->fcb->rvn);
    } else {
        wcb->loblk = wcbkey->prevwcb->hiblk + 1;
        curvbn = wcbkey->prevwcb->hd_basevbn;
        wcb->hd_seg_num = wcbkey->prevwcb->hd_seg_num;
        memcpy( &wcb->hd_fid, &wcbkey->prevwcb->hd_fid, sizeof(struct fiddef) );
        head = wcbkey->fcb->head;
    }
    while( TRUE ) {
        register f11word *mp;
        register f11word *me;

        wcb->hd_basevbn = curvbn;

        if( wcb->hd_seg_num != 0 ) {
            if( $FAILS(*retsts = accesshead(wcbkey->fcb->vcb, &wcb->hd_fid,
                                            wcb->hd_seg_num, &vioc, &head, NULL, 0)) ) {
                free(wcb);
                return NULL;
            }
        }
        mp = (f11word *) head + head->fh2$b_mpoffset;
        me = mp + head->fh2$b_map_inuse;
        while( mp < me ) {
            register uint32_t phylen, phyblk;
            register f11word mp0 = 0;

            switch( ((mp0 = F11WORD(*mp)) >> 14) & 0x3 ) {
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
            case 3: default:
                phylen = ((mp0 & 0x3fff) << 16) + F11WORD(mp[1]) + 1;
                phyblk = (F11WORD(mp[3]) << 16) | F11WORD(mp[2]);
                mp += 4;
                break;
            }
            curvbn += phylen;
            if( phylen != 0 && curvbn > wcb->loblk ) {
                wcb->phylen[extents] = phylen;
                wcb->phyblk[extents] = phyblk;
                wcb->rvn[extents] = wcb->hd_fid.fid$b_rvn;
                if( ++extents >= EXTMAX) {
                    if( curvbn > wcbkey->vbn ) {
                        break;
                    } else {
                        extents = 0;
                        wcb->loblk = curvbn;
                    }
                }
            }
        }
        if( extents >= EXTMAX ||
            (F11WORD(head->fh2$w_ext_fid.fid$w_num) == 0 &&
             head->fh2$w_ext_fid.fid$b_nmx == 0) ) {
            break;
        }
        {
            register unsigned rvn;
            wcb->hd_seg_num++;
            rvn = wcb->hd_fid.fid$b_rvn;
            fid_copy(&wcb->hd_fid, &head->fh2$w_ext_fid, rvn);
            if( vioc != NULL ) {
                deaccesshead(vioc, NULL, 0);
                vioc = NULL;
            }
        }
    }
    if( vioc != NULL ) {
        deaccesshead(vioc, NULL, 0);
        vioc = NULL;
    } else {
        if( wcbkey->fcb->head == NULL )
            free(head);
    }
    wcb->hiblk = curvbn - 1;
    wcb->extcount = extents;
    *retsts = SS$_NORMAL;
    if( curvbn <= wcbkey->vbn ) {
        free(wcb);
        wcb = NULL;
#if DEBUG
        printf( "--->wcb_create(): curvbn (=%u) <= wcbkey->vbn (=%u)\n",
                curvbn, wcbkey->vbn );
#endif
        *retsts = SS$_DATACHECK;
    }

    return wcb;
}

/**************************************************************** getwindow() */

/* getwindow() find a window to map VBN to LBN ... */

vmscond_t getwindow( struct FCB * fcb, uint32_t vbn, struct VCBDEV **devptr,
                     uint32_t *phyblk, uint32_t *phylen, struct fiddef *hdrfid,
                     unsigned *hdrseq ) {
    vmscond_t sts;
    struct WCB *wcb;
    struct WCBKEY wcbkey;
    register unsigned extent = 0;
    register unsigned togo;

#if DEBUG
    printf("Accessing window for vbn %u, file (%u)\n", vbn, fcb->cache.hashval);
#endif
    memset( &wcbkey, 0, sizeof( wcbkey ) );
    wcbkey.vbn = vbn;
    wcbkey.fcb = fcb;
    wcbkey.prevwcb = NULL;
    wcb = cache_find( (void *) &fcb->wcb, 0, &wcbkey, &sts, wcb_compare, wcb_create );
    if( wcb == NULL )
        return sts;

    togo = vbn - wcb->loblk;
    while( togo >= wcb->phylen[extent] ) {
        togo -= wcb->phylen[extent];
        if( ++extent > wcb->extcount )
            return SS$_BUGCHECK;
    }
    *devptr = RVN_TO_DEV(fcb->vcb, wcb->rvn[extent]);
    *phyblk = wcb->phyblk[extent] + togo;
    *phylen = wcb->phylen[extent] - togo;

    if( hdrfid != NULL )
        memcpy(hdrfid, &wcb->hd_fid, sizeof(struct fiddef));

    if( hdrseq != NULL )
        *hdrseq = wcb->hd_seg_num;
#if DEBUG
    printf("Mapping vbn %u to %u (%u -> %u)[%u] file (%u)\n",
           vbn, *phyblk, wcb->loblk, wcb->hiblk, wcb->hd_basevbn,
           fcb->cache.hashval);
#endif
    cache_untouch( &wcb->cache, TRUE );

    if( *devptr == NULL )
        return SS$_DEVNOTMOUNT;

    return SS$_NORMAL;
}

/************************************************************* vioc_manager() */

/* Object manager for VIOC objects:- if the object has been
 * modified then we need to flush it to disk before we let
 * the cache routines do anything to it...
 */

void *vioc_manager( struct CACHE * cacheobj, int flushonly ) {
    register struct VIOC *vioc;

    UNUSED(flushonly);

    vioc = (struct VIOC *) cacheobj;

    if( vioc->modmask != 0 ) {
        register struct FCB *fcb = NULL;
        register unsigned int length = VIOC_CHUNKSIZE;
        register uint32_t curvbn = 0;
        register char *address = NULL;
        register unsigned modmask = 0;

        fcb = vioc->fcb;
        curvbn = (size_t)vioc->cache.hashval + 1;
        address = (char *) vioc->data;
        modmask = vioc->modmask;

#if DEBUG
        printf("\nvioc_manager writing vbn %d\n", curvbn);
#endif
        do {
            register vmscond_t sts;
            unsigned int wrtlen = 0;
            uint32_t phyblk = 0, phylen = 0, idxlbn;
            struct VCBDEV *vcbdev = NULL;

            while( length > 0 && !(1 & modmask) ) {
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
            while( wrtlen > 0 ) {
                if( fcb->highwater != 0 && curvbn >= fcb->highwater ) {
                    length = 0;
                    break;
                }
                if( $FAILS(sts = getwindow(fcb, curvbn, &vcbdev,
                                           &phyblk, &phylen, NULL, NULL)) )
                    return NULL;
                if (phylen > wrtlen)
                    phylen = wrtlen;
                if (fcb->highwater != 0 &&
                    curvbn + phylen > fcb->highwater) {
                    phylen = fcb->highwater - curvbn;
                }
                if( $FAILS(sts = virt_write( vcbdev->dev, phyblk,
                                             phylen * 512, address )) )
                    return NULL;

                idxlbn = (F11LONG(vcbdev->home.hm2$l_ibmaplbn)  +
                          F11WORD(vcbdev->home.hm2$w_ibmapsize));

                if( idxlbn >= phyblk && idxlbn < phyblk + phylen ) {
                    if( $FAILS(sts = virt_write( vcbdev->dev,
                                      F11LONG(vcbdev->home.hm2$l_altidxlbn),
                                                 512, address +
                                                 (512 * ((size_t)idxlbn - phyblk)) )) )
                        return NULL;
                }
                wrtlen -= phylen;
                curvbn += phylen;
                address += (size_t)phylen * 512;
            }
        } while( length > 0 && modmask != 0 );
        vioc->modmask = 0;
        vioc->cache.objmanager = NULL;
    }
    return cacheobj;
}

/************************************************************ deaccesschunk() */

/* deaccesschunk() to deaccess a VIOC (chunk of a file) */

vmscond_t deaccesschunk( struct VIOC *vioc, uint32_t wrtvbn,
                         int wrtblks, int reuse ) {
#if DEBUG
    printf( "Deaccess chunk %u (%08x) wrtvbn: %u blks: %u ru: %u\n",
            vioc->cache.hashval, vioc->cache.hashval, wrtvbn, wrtblks, reuse );
#endif

    if( wrtvbn ) {
        register unsigned modmask;

        if( wrtvbn <= vioc->cache.hashval ||
            wrtvbn + wrtblks > vioc->cache.hashval + VIOC_CHUNKSIZE + 1 ) {
#if DEBUG
            printf( "VBN out of reserved range\n" );
#endif
            return SS$_BADPARAM;
        }

        modmask = 1 << (wrtvbn - vioc->cache.hashval - 1);

        while( --wrtblks > 0 )
            modmask |= modmask << 1;

        if( (vioc->wrtmask | modmask) != vioc->wrtmask ) {
#if DEBUG
            printf( "Write mask error %08x %08x\n",
                    (vioc->wrtmask | modmask), vioc->wrtmask );
#endif
            return SS$_WRITLCK;
        }

        vioc->modmask |= modmask;
        if( vioc->cache.refcount == 1 )
           vioc->wrtmask = 0;
        vioc->cache.objmanager = vioc_manager;
    }

    cache_untouch( &vioc->cache, reuse );
    return SS$_NORMAL;
}

/************************************************************** vioc_create() */

static void *vioc_create( uint32_t hashval, void *keyval, vmscond_t *retsts ) {

    register struct VIOC *vioc;
    register uint32_t length;
    register uint32_t curvbn;
    register char *address;
    register struct FCB *fcb;

    vioc = (struct VIOC *) calloc( 1, sizeof(struct VIOC) );
    if( vioc == NULL ) {
        *retsts = SS$_INSFMEM;
        return NULL;
    }

    curvbn = hashval + 1;
    fcb = (struct FCB *) keyval;

    vioc->cache.objmanager = NULL;
    vioc->cache.objtype = OBJTYPE_VIOC;
    vioc->fcb = fcb;
    vioc->wrtmask = 0;
    vioc->modmask = 0;
    length = fcb->hiblock - curvbn + 1;
    if( length > VIOC_CHUNKSIZE )
        length = VIOC_CHUNKSIZE;
    address = (char *) vioc->data;
    do {
        register vmscond_t sts;
        uint32_t phyblk = 0, phylen = 0;
        struct VCBDEV *vcbdev = NULL;

        if (fcb->highwater != 0 && curvbn >= fcb->highwater) {
            memset( address, 0, (size_t)length * 512 );
            break;
        }

        if( $SUCCESSFUL(sts = getwindow( fcb, curvbn, &vcbdev,
                                        &phyblk, &phylen, NULL, NULL )) ) {
            if( phylen > length )
                phylen = length;
            if( fcb->highwater != 0 && curvbn + phylen > fcb->highwater ) {
                phylen = fcb->highwater - curvbn;
            }
            sts = virt_read( vcbdev->dev, phyblk, phylen * 512,
                             address );
        }
        if( $FAILED(sts) ) {
            free(vioc);
            *retsts = sts;
            return NULL;
        }
        length -= phylen;
        curvbn += phylen;
        address += (size_t)phylen * 512;
    } while( length > 0 );
    *retsts = SS$_NORMAL;

    return vioc;
}

/************************************************************** accesschunk() */

/* accesschunk() return pointer to a 'chunk' of a file ... */

vmscond_t accesschunk( struct FCB *fcb, uint32_t vbn, struct VIOC **retvioc,
                     char **retbuff, uint32_t *retblocks, unsigned wrtblks ) {
    vmscond_t sts;
    register uint32_t blocks;
    register struct VIOC *vioc;

#if DEBUG
    printf("Access chunk %u (%u)\n", vbn, fcb->cache.hashval);
#endif
    if (vbn < 1 || vbn > fcb->hiblock)
        return SS$_ENDOFFILE;
    blocks = (vbn - 1) / VIOC_CHUNKSIZE * VIOC_CHUNKSIZE;
    if (wrtblks) {
        if( !(fcb->status & FCB_WRITE) )
            return SS$_WRITLCK;
        if (vbn + wrtblks > blocks + VIOC_CHUNKSIZE + 1) {
            return SS$_BADPARAM;
        }
    }
    vioc = cache_find((void *) &fcb->vioc, blocks, fcb, &sts, NULL, vioc_create);
    if (vioc == NULL)
        return sts;
    /*
        Return result to caller...
    */
    *retvioc = vioc;
    blocks = vbn - blocks - 1;
    *retbuff = vioc->data[blocks];
    if (wrtblks || retblocks != NULL) {
        register unsigned modmask = 1 << blocks;

        blocks = VIOC_CHUNKSIZE - blocks;
        if (vbn + blocks > fcb->hiblock)
            blocks = fcb->hiblock - vbn + 1;
        if (wrtblks && blocks > wrtblks)
            blocks = wrtblks;
        if (retblocks != NULL)
            *retblocks = blocks;
        if (wrtblks && blocks) {
            if( fcb->highwater != 0 && vbn + blocks > fcb->highwater ) {
                fcb->highwater = vbn + blocks;
                fcb->head->fh2$l_highwater = F11LONG( fcb->highwater );
            }
            while( --blocks > 0 )
                modmask |= modmask << 1;
            vioc->wrtmask |= modmask;
            vioc->cache.objmanager = vioc_manager;
        }
    }
    return SS$_NORMAL;
}

/************************************************************* deaccessfile() */

/* deaccessfile() finish accessing a file.... */

vmscond_t deaccessfile( struct FCB *fcb ) {
#if DEBUG
    printf("Deaccessing file (%u) reference %d\n", fcb->cache.hashval,
           fcb->cache.refcount);
#endif
    if( fcb->cache.refcount == 1 ) {
        register unsigned refcount;

        refcount = cache_refcount((struct CACHE *) fcb->vioc);
        if( refcount && fcb->vioc == fcb->headvioc ) {
            /* Index file header and data may share a VIOC.
             * In this case, there's no way to know how
             * many of the refs are from each source.  Assume 1 from header.
             */
            --refcount;
        }
        refcount += cache_refcount((struct CACHE *) fcb->wcb);

        if( refcount != 0 ) {
#if DEBUG
            printf("File reference counts non-zero %d  (%d)\n", refcount,
            fcb->cache.hashval);
#endif
#if DEBUG
            printf("File reference counts non-zero %d %d\n",
                   cache_refcount((struct CACHE *) fcb->wcb),
                   cache_refcount((struct CACHE *) fcb->vioc));
#endif
            return SS$_BUGCHECK;
        }
        if( (fcb->status & FCB_WRITE ) &&
            (fcb->head->fh2$l_filechar & F11LONG(FH2$M_MARKDEL)) ) {
                return deallocfile(fcb);
        }
    }
    cache_untouch(&fcb->cache, TRUE);
    return SS$_NORMAL;
}

/************************************************************** fcb_manager() */

/* Object manager for FCB objects:- we point to one of our
   sub-objects (vioc or wcb) in preference to letting the
   cache routines get us!  But we when run out of excuses
   it is time to clean up the file header...  :-(   */

void *fcb_manager(struct CACHE *cacheobj, int flushonly) {
    register struct FCB *fcb;

    fcb = (struct FCB *) cacheobj;
    if( fcb->vioc != NULL )
        return &fcb->vioc->cache;
    if( fcb->wcb != NULL )
        return &fcb->wcb->cache;
    if( fcb->cache.refcount != 0 || flushonly)
        return NULL;
    if( fcb->headvioc != NULL ) {
        deaccesshead(fcb->headvioc, fcb->head, fcb->headvbn);
        fcb->headvioc = NULL;
    }
    return cacheobj;
}

/*************************************************************** fcb_create() */

static void *fcb_create( uint32_t filenum, void *keyval, vmscond_t *retsts ) {

    register struct FCB *fcb;

    fcb = (struct FCB *) calloc( 1, sizeof(struct FCB) );

    UNUSED(filenum);
    UNUSED(keyval);

    if (fcb == NULL) {
        *retsts = SS$_INSFMEM;
    } else {
        fcb->cache.objmanager = fcb_manager;
        fcb->cache.objtype = OBJTYPE_FCB;
        fcb->hiblock = 100000;
    }
    return fcb;
}

/*************************************************************** accessfile() */

/* accessfile() open up file for access... */

vmscond_t accessfile( struct VCB * vcb, struct fiddef * fid, struct FCB **fcbadd,
                     unsigned wrtflg ) {
    vmscond_t sts;
    register struct FCB *fcb;
    register uint32_t filenum;

    filenum = (fid->fid$b_nmx << 16) + fid->fid$w_num;
#if DEBUG
    printf("Accessing file (%d,%d,%d) wrt %u\n", (fid->fid$b_nmx << 16) +
           fid->fid$w_num, fid->fid$w_seq, fid->fid$b_rvn, wrtflg);
#endif
    if( filenum < 1 )
        return SS$_BADPARAM;
    if( wrtflg && !(vcb->status & VCB_WRITE) )
        return SS$_WRITLCK;
    if( fid->fid$b_rvn > 1 )
        filenum |= fid->fid$b_rvn << 24;
    fcb = cache_find( (void *) &vcb->fcb, filenum, NULL, &sts, NULL, fcb_create );
    if( fcb == NULL )
        return sts;
    /* If not found make one... */
    *fcbadd = fcb;
    if( fcb->vcb == NULL ) {
        fcb->rvn = fid->fid$b_rvn;
        if( fcb->rvn == 0 && vcb->devices > 1 )
            fcb->rvn = 1;
        fcb->vcb = vcb;
    }
    if( wrtflg ) {
        if( fcb->headvioc != NULL && !(fcb->status & FCB_WRITE) ) {
            deaccesshead(fcb->headvioc, NULL, 0);
            fcb->headvioc = NULL;
        }
        fcb->status |= FCB_WRITE;
    }
    if( fcb->headvioc == NULL ) {
        register vmscond_t sts;

        if( $SUCCESSFUL(sts = accesshead(vcb, fid, 0, &fcb->headvioc,
                                         &fcb->head, &fcb->headvbn,
                                         wrtflg)) ) {
            fcb->hiblock = F11SWAP(fcb->head->fh2$w_recattr.fat$l_hiblk);
            if (fcb->head->fh2$b_idoffset > 39) {
                fcb->highwater = F11LONG(fcb->head->fh2$l_highwater);
            } else {
                fcb->highwater = 0;
            }
        } else {
#if DEBUG
            printf("Accessfile fail status %08x\n", sts);
#endif
            fcb->cache.objmanager = NULL;
            cache_untouch(&fcb->cache, TRUE);
            cache_delete(&fcb->cache);
            return sts;
        }
    }
    return SS$_NORMAL;
}

/***************************************************************** dismount() */

/* dismount() finish processing on a volume */

vmscond_t dismount (struct VCB * vcb, options_t options ) {
    register vmscond_t sts, device;
    int expectfiles;
    int openfiles;
    struct VCBDEV *vcbdev;

    expectfiles = vcb->devices;
    openfiles = cache_refcount( &vcb->fcb->cache );

    if( vcb->status & VCB_WRITE )
        expectfiles *= 2;
#if DEBUG
    printf("Dismounting disk %d\n", openfiles);
#endif
    sts = SS$_NORMAL;
    if( openfiles != expectfiles ) {
        printmsg( DISM_CANNOTDMT, 0,
                  12, ( (vcb->vcbdev[0].home.hm2$w_rvn?
                         vcb->vcbdev[0].home.hm2$t_strucname:
                         vcb->vcbdev[0].home.hm2$t_volname) ) );
        return printmsg( DISM_USERFILES, MSG_CONTINUE,
                         DISM_CANNOTDMT, openfiles - expectfiles );
    }

    vcbdev = vcb->vcbdev;
    for( device = 0; device < vcb->devices; device++, vcbdev++ ) {
#if DEBUG
        printf( "--->dismount(): vcbdev[%d] = \"%s\"\n", device,
                vcbdev->dev != NULL ? vcbdev->dev->devnam : "NULL" );
#endif
        if( vcbdev->dev != NULL ) {
            if( (vcb->status & VCB_WRITE) && vcbdev->mapfcb != NULL ) {
                sts = deaccessfile( vcbdev->mapfcb );
#if DEBUG
                printf( "--->dismount(): %s = deaccessfile(bitmap)\n", getmsg(sts, 0) );
#endif
                if( $FAILED(sts) ) {
                    printmsg( DISM_CANNOTDMT, 0, 12, vcbdev->home.hm2$t_volname );
                    sts = printmsg( sts, MSG_CONTINUE, DISM_CANNOTDMT );
                    break;
                }
                vcbdev->mapfcb->status &= ~FCB_WRITE;
                vcbdev->mapfcb = NULL;
            }

            while (vcb->dircache) {
                cache_delete((struct CACHE *) vcb->dircache);
#if DEBUG
                printf( "--->dismount(): cache_delete(dircache)\n" );
#endif
            }
            /* Remove all unreferenced fcbs - the LRU list still has references
             * on file headers. This reduces the references to just the index file itself.
             */
            cache_remove( &vcb->fcb->cache );
#if DEBUG
            printf( "--->dismount(): cache_remove()\n" );
#endif

            /* Index file data & header.  Special handling due to header also being
             * file data for the index file.
             */
            deaccesshead( vcbdev->idxfcb->headvioc,
                          vcbdev->idxfcb->head, vcbdev->idxfcb->headvbn );
            /* Check status? */
            vcbdev->idxfcb->headvioc = NULL;
            cache_untouch(&vcbdev->idxfcb->cache, FALSE);
            vcbdev->idxfcb->status &= ~FCB_WRITE;
            vcbdev->idxfcb = NULL;
            /* Remove index fcb, which should be the last in the FCB cache. */

            cache_remove( &vcb->fcb->cache );
#if DEBUG
            printf( "--->dismount(): %s = deaccess(index)\n", getmsg(sts, 0) );
#endif
            if( $FAILED(sts) )
                break;
        }
    }
    if( $SUCCESSFUL(sts) ) {
        struct VCB *lp;

        vcbdev = vcb->vcbdev;
        for (device = 0; device < vcb->devices; device++, vcbdev++) {
            if( vcbdev->dev != NULL ) {
#if DEBUG
                printf( "--->dismount(): virt_close(%s)\n",
                        vcbdev->dev->devnam );
#endif
                virt_close( vcbdev->dev );
                vcbdev->dev = NULL;
                if( options & MOU_LOG )
                    sts = printmsg( (vcb->devices > 1)?
                                    DISM_DISMOUNTD: DISM_DISMOUNTDV,
                                    0,
                                    12, vcbdev->home.hm2$t_volname,
                                    12, ( (vcb->vcbdev[0].home.hm2$w_rvn?
                                           vcb->vcbdev[0].home.hm2$t_strucname:
                                           vcb->vcbdev[0].home.hm2$t_volname)  ) );
            }
        }
        for( lp = (struct VCB *)&vcb_list; lp->next != NULL; lp = lp->next ) {
            if( lp->next == vcb ) {
                lp->next = vcb->next;
                break;
            }
        }
        if( vcb->devices > 1 )
            sts = printmsg( DISM_DISMAL, 0, 12, vcb->vcbdev[0].home.hm2$t_strucname );
        free(vcb);
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

vmscond_t mount( options_t flags,
                 unsigned devices,
                 char *devnam[],
                 char *label[] ) {
    register unsigned device;
    vmscond_t sts = 0;
    struct VCB *vcb;
    struct VCBDEV *vcbdev;
    struct VOLSETREC *volsets = NULL;

#if DEBUG
    if (sizeof(struct HOME) != 512 || sizeof(struct HEAD) != 512)
        return SS$_BUGCHECK;
#endif

    /* calloc() initializes most of the structure */

    vcb = (struct VCB *) calloc( 1, sizeof(struct VCB) +
                                 (((size_t)devices - 1) * sizeof(struct VCBDEV)) );
    if( vcb == NULL )
        return SS$_INSFMEM;

    if( flags & MOU_WRITE )
        vcb->status |= VCB_WRITE;

    /* Pass 1: validate device name & open each for I/O.
     *         Read HOM blocks & verify labels.
     */
    vcbdev = vcb->vcbdev;
    for( device = 0; device < devices; device++, vcbdev++ ) {
        char *dname;
        uint32_t errcnt = 0;

        dname = devnam[device];
        sts = MOUNT_FAILED;
        vcbdev->dev = NULL;
        if( *dname != '\0' ) { /* Really want to allow skipping volumes? */
            unsigned int hba, delta, homtry;

            if( label[device] != NULL && strlen(label[device]) >
                sizeof( volsets[0].vsr$t_label ) ) {
                sts = printmsg( MOUNT_BADLABEL, 0, label[device] );
                break;
            }
            if( $FAILS(sts = virt_open( &dname, flags, &vcbdev->dev )) )
                break;
            vcb->devices++;
            if (vcbdev->dev->vcb != NULL) {
                sts = printmsg( SS$_DEVMOUNT, 0 );
                break;
            }
            if( (flags & MOU_WRITE ) && !(vcbdev->dev->access & MOU_WRITE) ) {
                sts = printmsg( MOUNT_WRITELCK, 0, devnam[device] );
                vcb->status &= ~VCB_WRITE;
            }

            delta = delta_from_index( (vcbdev->dev->access & MOU_DEVTYPE) >> MOU_V_DEVTYPE );

            for( hba = 1, homtry = 0;
                 homtry < HOME_LIMIT; homtry++, hba += delta ) {
                struct HOME *hom;

#if HOME_SKIP > 100
                if( homtry < HOME_SKIP )
                    continue;
#endif
                if( $FAILS(sts = virt_read( vcbdev->dev, hba,
                                            sizeof( struct HOME ),
                                            (char *) &vcbdev->home )) )
                    break;
                hom = &vcbdev->home;
#if DEBUG || defined( HOME_LOG )
                printf( "--->mount(%u): LBA=%u, HM2$L_HOMELBN=%u, "
                        "HM2$L_ALHOMELBN=%u, "
                        "HM2$T_FORMAT=\"%12.12s\", memcmp()=%d,%d\n",
                        homtry+1, hba, F11LONG( hom->hm2$l_homelbn ),
                        F11LONG( hom->hm2$l_alhomelbn ),
                        hom->hm2$t_format,
                        memcmp( hom->hm2$t_format, "DECFILE11B  ", 12 ),
                        memcmp( hom->hm2$t_format, "FILES11B_L0 ", 12 )
                );
#endif
                if( (hba == F11LONG(hom->hm2$l_homelbn)) &&
                    ((F11LONG(hom->hm2$l_alhomelbn) != 0) ||
                     (memcmp( hom->hm2$t_format, "FILES11B_L0 ", 12 ) == 0)) &&
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
                    ((memcmp(hom->hm2$t_format, "DECFILE11B  ", 12) == 0) ||
                    (memcmp( hom->hm2$t_format, "FILES11B_L0 ", 12 ) == 0)) ) {
                    break;
                }
                errcnt++;
                sts = MOUNT_HOMBLKCHK;
#if DEBUG || defined( HOME_LOG )
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
            }
            if( $FAILED(sts) ) {
                sts = printmsg( sts, 0, errcnt );
                break;
            }
            if( (F11WORD(vcbdev->home.hm2$w_struclev) >> 8) != 2 ||
                (F11WORD(vcbdev->home.hm2$w_struclev) & 0xFFu) < 1 ) {
                sts = printmsg( sts, 0,
                                F11WORD(vcbdev->home.hm2$w_struclev) >> 8,
                                F11WORD(vcbdev->home.hm2$w_struclev) & 0XFFu );
                break;
            }
            if( errcnt && (flags & MOU_LOG) ) /* Suppress warning if /nolog for scripted mounts */
                printmsg( $SETLEVEL(MOUNT_HOMBLKCHK, WARNING), 0, errcnt );

            if( label[device] != NULL ) {
                int i;
                char lbl[12+1]; /* Pad CLI-supplied label to match ODS */

                (void) snprintf( lbl, sizeof(lbl), "%-12s", label[device] );

                for( i = 0; i < 12; i++ ) {
                    if( toupper( lbl[i] ) !=
                        toupper( vcbdev->home.hm2$t_volname[i] ) ) {
                        sts = printmsg( MOUNT_WRONGVOL, 0, dname,
                                        12, vcbdev->home.hm2$t_volname, lbl );
                        break;
                    }
                }
                if( $FAILED(sts) )
                    break;
            }
            if( (F11WORD(vcbdev->home.hm2$w_rvn) != device + 1) &&
                !(F11WORD(vcbdev->home.hm2$w_rvn) == 0 && device == 0) ) {
                if( vcbdev->home.hm2$w_rvn == 0 ) {
                    sts = printmsg( MOUNT_NOTVSMEM, 0, dname,
                                    12, vcbdev->home.hm2$t_volname,
                                    device+1,
                                    12, vcb->vcbdev[0].home.hm2$t_strucname );
                } else {
                    sts = printmsg( MOUNT_WRONGRVN, 0, dname,
                                    12, vcbdev->home.hm2$t_volname,
                                    F11WORD(vcbdev->home.hm2$w_rvn),
                                    12, vcbdev->home.hm2$t_strucname,
                                    device + 1,
                                    12, vcb->vcbdev[0].home.hm2$t_strucname );
                }
            }
            if( $FAILED(sts) )
                break;
        } /* for(each device) */
    }
    if( $FAILED(sts) ) {
        vcbdev = vcb->vcbdev;
        for( device = 0; device < vcb->devices; device++, vcbdev++ ) {
            if (vcbdev->dev == NULL) {
                continue;
            }
            virt_close( vcbdev->dev );
        }
        free( vcb );
        vcb = NULL;
    } else {
        /* Pass 2: Open the index file.
         *         For RVN 1 of a volume set, read VOLSET.SYS.
         *         For writable volumes, open and validate BITMAP.SYS.
         */
        vcbdev = vcb->vcbdev;
        for( device = 0; device < devices; device++, vcbdev++ ) {
            char *dname;

            dname = devnam[device];
            sts = MOUNT_FAILED;
            vcbdev->idxfcb = NULL;
            vcbdev->mapfcb = NULL;
            vcbdev->clustersize = 0;
            vcbdev->max_cluster = 0;
            vcbdev->free_clusters = 0;

            if( *dname != '\0' ) {
                struct fiddef idxfid = { FID$C_INDEXF, FID$C_INDEXF, 0, 0 };
                idxfid.fid$b_rvn = device + 1;

                if( !(vcb->status & ~VCB_WRITE) )
                    vcbdev->dev->access &= ~MOU_WRITE;

                if( $FAILS(sts = accessfile( vcb, &idxfid, &vcbdev->idxfcb,
                                             (vcb->status & VCB_WRITE) != 0 )) ) {
                    virt_close( vcbdev->dev );
                    vcbdev->dev = NULL;
                    break;
                }
                vcbdev->dev->vcb = vcb;
                if( F11WORD(vcbdev->home.hm2$w_rvn) != 0 ) {
                    if( device == 0 ) {
                        struct fiddef vsfid = { FID$C_VOLSET,
                                                FID$C_VOLSET, 1, 0 } ;
                        struct FCB *vsfcb = NULL;
                        struct VIOC *vioc = NULL;
                        unsigned rec;
                        uint32_t vbn = 1;
                        struct VOLSETREC *bufp = NULL;

                        unsigned setcount;

                        setcount = F11WORD(vcbdev->home.hm2$w_setcount);
                        if( setcount != devices ) {
                            sts = printmsg( MOUNT_WRONGVOLCNT, 0,
                                            12, vcbdev->home.hm2$t_strucname,
                                            setcount, devices );
                            break;
                        }
                        /* Read VOLSET.SYS
                         * 1 record for volset name + 1 for each member's label.
                         */
                        volsets =
                            (struct VOLSETREC *)malloc( (1+(size_t)setcount) *
                                                        sizeof( struct VOLSETREC ) );
                        if( volsets == NULL ) {
                            printmsg( MOUNT_VOLSETSYS, 0, dname );
                            sts = printmsg( SS$_INSFMEM, MSG_CONTINUE, MOUNT_VOLSETSYS );
                            break;
                        }
                        if( $FAILS(sts = accessfile( vcb, &vsfid, &vsfcb, 0 )) ) {
                            printmsg( MOUNT_VOLSETSYS, 0, dname );
                            sts = printmsg( sts, MSG_CONTINUE, MOUNT_VOLSETSYS );
                            break;
                        }
                        for( rec = 0; rec <= setcount; ) {
                            uint32_t blocks = 0, recs;

                            if( $FAILS(sts = accesschunk( vsfcb, vbn, &vioc,
                                                          (char **)&bufp,
                                                          &blocks, 0 )) )
                                break;
                            vbn += blocks;

                            recs = (size_t)blocks * 512 / sizeof( struct VOLSETREC );
                            if( rec + recs > setcount + 1 )
                                recs = setcount + 1 - rec;

                            memcpy(volsets+rec, bufp, recs *
                                   sizeof( struct VOLSETREC ));
                            rec += recs;

                            deaccesschunk( vioc, 0, 0, 0 );
                        }

                        { vmscond_t st2;
                            st2 = deaccessfile( vsfcb );
                            if( $SUCCESSFUL(sts) )
                                sts = st2;
                        }
                        if( $FAILED(sts) )  {
                            printmsg( MOUNT_VOLSETSYS, 0, dname );
                            sts = printmsg( sts, MSG_CONTINUE, MOUNT_VOLSETSYS );
                            break;
                        }
                        if( memcmp(vcbdev->home.hm2$t_strucname,
                                   volsets[0].vsr$t_label, 12 ) != 0 ) {

                            sts = printmsg( MOUNT_WRONGVOLMEM, 0,
                                            dname,
                                            12, vcbdev->home.hm2$t_strucname,
                                            12, volsets[0].vsr$t_label );
                            break;
                        }
                    } else { /* device != 0 */
                        if( vcb->vcbdev[0].dev == NULL ) {
                            sts = printmsg( MOUNT_RVN1NOTMNT, 0 );
                            break;
                        }
                        if( memcmp(vcbdev->home.hm2$t_strucname,
                                   vcb->vcbdev[0].home.hm2$t_strucname,
                                   12) != 0 ) {
                            sts = printmsg( MOUNT_INCONVOL, 0,
                                            12, vcbdev->home.hm2$t_volname,
                                            dname,
                                            12, vcbdev->home.hm2$t_strucname,
                                            12, vcb->vcbdev[0].home.hm2$t_strucname );
                            break;
                        }
                    }
                    /* All volset members */

                    if( memcmp(vcbdev->home.hm2$t_volname,
                               volsets[device+1].vsr$t_label, 12 ) != 0 ) {
                        sts = printmsg( MOUNT_VOLOOO, 0, device+1,
                                        12, vcb->vcbdev[0].home.hm2$t_strucname,
                                        12,  volsets[device+1].vsr$t_label,
                                        dname,
                                        12, vcbdev->home.hm2$t_volname );
                        break;
                    }
                } /* rvn != 0 */

                /* All volumes */

                if( vcb->status & VCB_WRITE ) {
#if DEBUG
                    struct SCB scb;
                    if( $FAILS(sts = get_scb( vcb, device, &scb )) )
                        break;

                    printf( "%d of %d blocks are free on %12.12s\n",
                            vcbdev->free_clusters * vcbdev->clustersize,
                            scb.scb$l_volsize,
                            vcbdev->home.hm2$t_volname );
#else
                    if( $FAILS(sts = get_scb( vcb, device, NULL )) )
                        break;
#endif
                }
                if( $SUCCESSFUL(sts) && (flags & MOU_LOG) ) {
                    sts = printmsg( MOUNT_MOUNTED, 0,
                                    12, vcbdev->home.hm2$t_volname, vcbdev->dev->devnam );
                }
            } /* device len */
        } /* for( each device ) */
        if( $FAILED(sts) ) { /* Dismount any volumes prior to the error */
            vcbdev = vcb->vcbdev;
            for( device = 0; device < devices; device++, vcbdev++ ) {
                if (vcbdev->dev == NULL) {
                    continue;
                }

                if( vcb->status & VCB_WRITE && vcbdev->mapfcb != NULL ) {
                    /* check status? */
                    deaccessfile(vcbdev->mapfcb);
                    vcbdev->idxfcb->status &= ~FCB_WRITE;
                    vcbdev->mapfcb = NULL;
                }
                while( vcb->dircache )
                    cache_delete( (struct CACHE *) vcb->dircache );
                cache_remove( &vcb->fcb->cache );
                if( vcbdev->idxfcb != NULL ) {
                    /* check status? = */
                    deaccesshead( vcbdev->idxfcb->headvioc,
                                  vcbdev->idxfcb->head,
                                  vcbdev->idxfcb->headvbn );
                    vcbdev->idxfcb->headvioc = NULL;
                    cache_untouch( &vcbdev->idxfcb->cache, FALSE );
                    vcbdev->idxfcb->status &= ~FCB_WRITE;
                    vcbdev->idxfcb = NULL;
                }
                vcbdev->dev->vcb = NULL;
                virt_close( vcbdev->dev );
            }
            cache_remove( &vcb->fcb->cache );
        }
    }

    /* Wrap up */

    if( $SUCCESSFUL(sts) && (flags & MOU_LOG) &&
        F11WORD(vcb->vcbdev[0].home.hm2$w_rvn) != 0 ) {
        sts = printmsg ( MOUNT_VSMOUNTED, 0, 12, vcb->vcbdev[0].home.hm2$t_strucname );
    }
    if( vcb != NULL ) {
        struct VCB *lp;

        if( $SUCCESSFUL(sts) ) {
            for( lp = (struct VCB *)&vcb_list;
                lp->next != NULL; lp = lp->next )
                ;
            lp->next = vcb;
            vcb->next = NULL;
        } else {
            free( vcb );
            vcb = NULL;
        }
    }
    if( volsets != NULL )
        free( volsets );

    if( $SUCCESSFUL(sts) )
        (void) mountdef( vcb->vcbdev[0].dev->devnam );

    return sts;
}

/***************************************************************** get_scb() */
static vmscond_t get_scb( struct VCB *vcb, unsigned device, struct SCB *retscb ) {
    vmscond_t sts;
    int mapopen = 1;
    struct fiddef mapfid = { FID$C_BITMAP, FID$C_BITMAP, 0, 0 };
    struct VCBDEV *vcbdev = NULL;
    struct VIOC *vioc = NULL;
    struct SCB *scb = NULL;
    int volwrt;

    if( retscb != NULL )
        memset( retscb, 0, sizeof( struct SCB ) );

    vcbdev = vcb->vcbdev + device;

    mapfid.fid$b_rvn = (f11byte) F11WORD(vcbdev->home.hm2$w_rvn);

    volwrt = (vcb->status & VCB_WRITE) != 0;

    if( vcbdev->mapfcb == NULL ) {
        mapopen = 0;

        if( $FAILS(sts = accessfile( vcb, &mapfid,
                                     &vcbdev->mapfcb, volwrt )) ) {
            vcb->status &= ~VCB_WRITE;
            vcbdev->mapfcb = NULL;
            printmsg( MOUNT_BITMAPSYS, 0, vcbdev->dev->devnam );
            return printmsg( sts, MSG_CONTINUE, MOUNT_BITMAPSYS );
        }
    }

    if( $FAILS(sts = accesschunk( vcbdev->mapfcb, 1, &vioc,
                              (char **) &scb, NULL, 0 )) ) {
        if( !mapopen ) {
            deaccessfile( vcbdev->mapfcb );
            vcbdev->mapfcb = NULL;
            vcb->status &= ~VCB_WRITE;
        }
        printmsg( MOUNT_BITMAPSYS, 0, vcbdev->dev->devnam );
        return printmsg( sts, MSG_CONTINUE, MOUNT_BITMAPSYS );
    }

    if( retscb != NULL )
        memcpy( retscb, scb, sizeof( struct SCB ) );

    if( (F11WORD(scb->scb$w_checksum) != checksum( (f11word *)scb )) ||
        (scb->scb$w_cluster != vcbdev->home.hm2$w_cluster) ) {
        deaccesschunk(vioc, 0, 0, FALSE);
        if( !mapopen ) {
            deaccessfile( vcbdev->mapfcb );
            vcbdev->mapfcb = NULL;
            vcb->status &= ~VCB_WRITE;
        }
        printmsg( MOUNT_BITMAPSYS, 0, vcbdev->dev->devnam );
        return printmsg( MOUNT_BADSCB, MSG_CONTINUE, MOUNT_BITMAPSYS );
    }

    vcbdev->clustersize = F11WORD( vcbdev->home.hm2$w_cluster );
    vcbdev->max_cluster = ( (F11LONG(scb->scb$l_volsize) +
                             F11WORD(scb->scb$w_cluster) - 1) /
                            F11WORD(scb->scb$w_cluster) ) -1;
    deaccesschunk(vioc, 0, 0, FALSE);

    if( !volwrt || !mapopen ) {
        if( $FAILS(sts = update_freecount( vcbdev,
                                           &vcbdev->free_clusters )) ) {
            printmsg( MOUNT_BITUPDFAIL, 0, vcbdev->dev->devnam );
            sts = printmsg( sts, MSG_CONTINUE, MOUNT_BITUPDFAIL );
        }
        if( !(mapopen || volwrt) ) {
            deaccessfile( vcbdev->mapfcb );
            vcbdev->mapfcb = NULL;
        }
    }

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
    struct SCB scb;
    vmscond_t sts;
    size_t n;
    f11word timbuf[7];
    char uicbuf[ 1+6+1+6+1+1], *p;

    vcbdev = vcb->vcbdev+device;

    if( $FAILS(sts = get_scb( vcb, device, &scb )) ) {
        if( $MATCHCOND(sts, SS$_BADPARAM) )
            printf(" SCB is invalid\n" );
        else
            printf( " *Unable to read SCB: %s\n", getmsg( sts, MSG_TEXT ) );
    }

    for( n = 0; n < strlen( vcbdev->dev->devnam ); n++ ) {
        if( vcbdev->dev->devnam[n] == ':' )
            break;
        putchar( vcbdev->dev->devnam[n] );
    }
    printf( "%*s ", (int)(devwid-n), "" );

    printf( "%12.12s", vcbdev->home.hm2$t_volname );

    (void) snprintf(uicbuf, sizeof(uicbuf), "[%6o,%6o]",
                    F11WORD( vcbdev->home.hm2$w_volowner.uic$w_grp ),
                    F11WORD( vcbdev->home.hm2$w_volowner.uic$w_mem ));
    for( p = uicbuf; p[1] == ' '; p++ ) {
        p[0] = ' '; p[1] = '[';
    }
    p = strchr( uicbuf, ',' );
    n = strspn( ++p, " " );
    memmove( p, p + n, 7 - n );
    memset(  p + 7 - n, ' ', n );

    printf( " %u.%u %5u %s",
            F11WORD( vcbdev->home.hm2$w_struclev ) >> 8,
            F11WORD( vcbdev->home.hm2$w_struclev ) & 0xFF,
            F11WORD( vcbdev->home.hm2$w_cluster ),
            uicbuf
            );
    printf( "   " );
    print_prot( F11WORD( vcbdev->home.hm2$w_protect ), 1 );
    if( !(vcb->status & VCB_WRITE) )
        printf( " write-locked" );

    if( $SUCCESSFUL(sts) ) {
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
            if( scb.scb$l_volsize == size &&
                scb.scb$l_blksize == blksize &&
                scb.scb$l_sectors == dp->sectors &&
                scb.scb$l_tracks == dp->tracks &&
                scb.scb$l_cylinders == dp->cylinders ) {
                if( first ) {
                    printf( " type: " );
                    first = 0;
                } else
                    printf( "|" );
                printf( "%s", dp->name );
            }
        }
        if( first )
            printf( " Unrecognized type" );
    }

    printf( "\n%*s %12.12s           ", (int)(wrap+devwid), "",
            vcbdev->home.hm2$t_ownername );

    if( $SUCCESSFUL(sys_numtim( timbuf, vcbdev->home.hm2$q_credate )) ) {
        static const char *months =
            "-JAN-FEB-MAR-APR-MAY-JUN-JUL-AUG-SEP-OCT-NOV-DEC-";

        printf( "%2u%.5s%4u %2u:%2u ", timbuf[2],
                months+(4*((size_t)timbuf[1]-1)), timbuf[0],
                timbuf[3], timbuf[4]);
    } else
        printf( "%*s", 41-23, "" );

    print_prot( F11WORD( vcbdev->home.hm2$w_fileprot ), 0 );

    if( $SUCCESSFUL(sts) ) {
        printf( " Free: %u/%u", vcbdev->free_clusters * vcbdev->clustersize,
                scb.scb$l_volsize );
        printf( " Geo: %u/%u/%u", F11LONG(scb.scb$l_sectors),
                F11LONG(scb.scb$l_tracks), F11LONG(scb.scb$l_cylinders) );
        if( F11LONG(scb.scb$l_blksize) != 1 )
            printf( " %u phys blks/LBN", F11LONG(scb.scb$l_blksize) );
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
    struct VCB *vcb, *next = NULL;
    vmscond_t sts;

    for( vcb = vcb_list; vcb != NULL; vcb = next ) {
        next = vcb->next;

        if( $FAILS(sts = dismount( vcb, 0 )) ) {
            printf( "Dismount failed in rundown: %s\n", getmsg(sts, MSG_TEXT) );
        }
    }
}

