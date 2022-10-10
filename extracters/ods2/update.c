/* Update.c */

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

#if !defined( DEBUG ) && defined( DEBUG_UPDATE )
#define DEBUG DEBUG_UPDATE
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "access.h"
#include "device.h"
#include "initvol.h"
#include "ods2.h"
#include "phyio.h"
#include "ssdef.h"
#include "stsdef.h"

#ifdef VMS
#include <starlet.h>
#else
#include "vmstime.h"
#endif

/* Bitmaps are bitstrings stored little-endian; the LSB represents the first item
 * (0 or 1) in the map.  For storage allocation, a set bit indicates a free block.
 * For file header allocation, a set bit indicates in-use.
 *
 * Bitmaps are accessed in 'WORK_UNITs' which can be a wide integer
 * on a little endian machine but is always a byte on a big endian system.
 * (The cost of byte-swapping the bitmaps exceeds the benefit of larger
 * work units.)
 *
 * There is an implicit assumption that WORK_UNITs will not span a disk block
 * (512 byte) boundary.
 */

#ifndef WORK_UNIT
#if ODS2_BIG_ENDIAN
#define WORK_UNIT uint8_t
#define WORK_MASK UINT8_MAX
#define WORK_C(n) UINT8_C(n)
#define WORK_BITS 8
#elif defined( INT64_MAX )
#define WORK_UNIT uint64_t
#define WORK_MASK UINT64_MAX
#define WORK_C(n) UINT64_C(n)
#define WORK_BITS 64
#else /* uint32_t is required for many other structures; no point going smaller */
#define WORK_UNIT uint32_t
#define WORK_MASK UINT32_MAX
#define WORK_C(n) UINT32_C(n)
#define WORK_BITS 32
#endif
#endif

/********************************************************* update_freecount() */

/* Read a device's storage allocation bitmap and tally the unused clusters.
 */

vmscond_t update_freecount( struct VCBDEV *vcbdev, uint32_t *retcount ) {
    register vmscond_t sts = SS$_NORMAL;
    register uint32_t free_clusters = 0;
    register uint32_t map_block, map_end;

    *retcount = 0;

    map_end = (((size_t)vcbdev->max_cluster + 4095) / 4096) + 2; /* 1 VBN past bitmap end */

    for( map_block = 2; map_block < map_end; ) { /* VBN 1 is SCB, 2 starts bitmap */
        struct VIOC *vioc;
        uint32_t blkcount = 0;
        WORK_UNIT *bitmap = NULL, *work_ptr = NULL;
        register uint32_t work_count = 0;

        if( $FAILS(sts = accesschunk( vcbdev->mapfcb, map_block, &vioc,
                                      (char **) &bitmap, &blkcount, 0 )) )
            return sts;
        if( blkcount > map_end - map_block )
            blkcount = map_end - map_block + 1;

        work_ptr = bitmap;
        work_count = ((size_t)blkcount * 512) / sizeof(WORK_UNIT);
        do {
            register WORK_UNIT work_val;

            work_val = *work_ptr++;

            if( work_val == WORK_MASK ) {
                free_clusters += WORK_BITS;
                continue;
            }
            if( work_val == 0 )
                continue;
#if WORK_BITS == 64
            work_val = work_val - ((work_val >> 1) & UINT64_C(0x5555555555555555));
            work_val = (work_val & UINT64_C(0x3333333333333333)) +
                ((work_val >> 2) & UINT64_C(0x3333333333333333));
            free_clusters += (((work_val +
                                (work_val >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f)) *
                              UINT64_C(0x0101010101010101)) >> 56;
#elif WORK_BITS == 32
            work_val = work_val - ((work_val >> 1) & UINT32_C(0x55555555));
            work_val = (work_val & UINT32_C(0x33333333)) +
                ((work_val >> 2) & UINT32_C(0x33333333));
            free_clusters += (((work_val +
                                (work_val >> 4)) & UINT32_C(0x0f0f0f0f)) *
                              UINT32_C(0x01010101)) >> 24;
#elif WORK_BITS == 8
            work_val = work_val - ((work_val >> 1) & UINT8_C(0x55));
            work_val = (work_val & UINT8_C(0x33)) + ((work_val >> 2) & UINT8_C(0x33));
            free_clusters += ((work_val + (work_val >> 4)) & UINT8_C(0x0f));
#else
            while( work_val != 0 ) {
                if( work_val & WORK_C(1) )
                    free_clusters++;
                work_val = work_val >> 1;
            }
#endif
        } while( --work_count > 0 );

        if( $FAILS(sts = deaccesschunk( vioc, 0, 0, TRUE)) )
            return sts;
        map_block += blkcount;
    }

    *retcount = free_clusters;
    return sts;
}

/************************************************************ bitmap_modify() */

/* Set or clear a contiguous block of bits in the device storage allocation bitmap
 */

static vmscond_t bitmap_modify( struct VCBDEV *vcbdev,
                                uint32_t cluster, uint32_t count,
                                uint32_t release_flag ) {
    register vmscond_t sts;
    register uint32_t clust_count;
    register uint32_t map_block;
    register uint32_t block_offset;

    clust_count = count;
    map_block = (cluster / 4096) + 2; /* VBN 1 = SCB, 2 = bitmap start */
    block_offset = cluster % 4096;

   if( clust_count < 1 )
        return SS$_BADPARAM;

    if (cluster + clust_count > vcbdev->max_cluster + 1)
        return SS$_BADPARAM;

    if( release_flag )
        vcbdev->free_clusters += clust_count;
    else
        vcbdev->free_clusters -= clust_count;

    do {
        struct VIOC *vioc = NULL;
        uint32_t blkcount = 0;
        WORK_UNIT *bitmap = NULL;
        register WORK_UNIT *work_ptr = NULL;
        register uint32_t work_count = 0;
        register uint32_t bit_no = 0;

       if( $FAILS(sts = accesschunk( vcbdev->mapfcb, map_block , &vioc,
                                     (char **) &bitmap, &blkcount, 1 )) )
            return sts;

        work_ptr = bitmap + block_offset / WORK_BITS;

        if( (bit_no = block_offset % WORK_BITS) != 0 ) {
            register WORK_UNIT bit_mask;

            bit_mask = WORK_MASK;
            if( bit_no + clust_count < WORK_BITS ) {
                bit_mask = bit_mask >> (WORK_BITS - clust_count);
                clust_count = 0;
            } else {
                clust_count -= WORK_BITS - bit_no;
            }
            bit_mask = bit_mask << bit_no;
            if (release_flag) {
                *work_ptr |= bit_mask;
            } else {
               *work_ptr &= ~bit_mask;
            }
            ++work_ptr;
            block_offset += WORK_BITS - bit_no;
        }

        work_count = (blkcount * 4096 - block_offset) / WORK_BITS;
        if( work_count > clust_count / WORK_BITS ) {
            work_count = clust_count / WORK_BITS;
            block_offset = 1;
        } else {
            block_offset = 0;
        }
        clust_count -= work_count * WORK_BITS;
        if( release_flag ) {
            while( work_count-- > 0 ) {
                *work_ptr++ = WORK_MASK;
            }
        } else {
            while( work_count-- > 0 ) {
                *work_ptr++ = 0;
            }
        }
        if( clust_count != 0 && block_offset ) {
            register WORK_UNIT bit_mask;

            bit_mask = WORK_MASK >> (WORK_BITS - clust_count);
            if( release_flag ) {
                *work_ptr |= bit_mask;
            } else {
                *work_ptr &= ~bit_mask;
            }
            ++work_ptr;

            clust_count = 0;
        }

        if( $FAILS(sts = deaccesschunk( vioc, map_block, blkcount, TRUE )) )
            return sts;
        map_block += blkcount;
        block_offset = 0;
    } while( clust_count != 0 );

    return sts;
}

/************************************************************ bitmap_search() */

/* Find a pool of free clusters in the device's storage allocation bitmap */

static vmscond_t bitmap_search( struct VCBDEV *vcbdev,
                                uint32_t *position, uint32_t *count ) {
    register vmscond_t sts;
    register uint32_t map_block, block_offset;
    register uint32_t search_words, needed;
    register uint32_t run = 0, cluster;
    register uint32_t best_run = 0, best_cluster = 0;

    needed = *count;
    if( needed < 1 )
        return SS$_BADPARAM;
    if( needed > (vcbdev->max_cluster + 1) * vcbdev->clustersize)
        return SS$_DEVICEFULL;

    needed = (needed + vcbdev->clustersize -1) / vcbdev->clustersize;

    cluster = *position / vcbdev->clustersize;

    if( cluster + needed > vcbdev->max_cluster + 1 )
        cluster = 0;

    map_block = (cluster / 4096) + 2;
    block_offset = cluster % 4096;
    cluster = cluster - (cluster % WORK_BITS);
    search_words = (vcbdev->max_cluster +1 -cluster + (WORK_BITS -1)) / WORK_BITS;

    do {
        struct VIOC *vioc =  NULL;
        uint32_t blkcount = 0;
        WORK_UNIT *bitmap = NULL;
        register WORK_UNIT *work_ptr = NULL, work_val = 0;
        register uint32_t work_count = 0;

        if( $FAILS(sts = accesschunk( vcbdev->mapfcb, map_block, &vioc,
                                      (char **) &bitmap, &blkcount, 0 )) )
            return sts;
        work_ptr = bitmap + (block_offset / WORK_BITS);
        work_val = *work_ptr++;
        if (block_offset % WORK_BITS) {
            work_val = work_val & (WORK_MASK << (block_offset % WORK_BITS));
        }
        work_count = (((blkcount * 4096) + WORK_BITS -1) - block_offset) / WORK_BITS;
        if (work_count > search_words)
            work_count = search_words;
        search_words -= work_count;
        do {
            if( work_val == WORK_MASK ) {
                run += WORK_BITS;
                if( run >= needed ) {
                    best_run = run;
                    best_cluster = cluster + WORK_BITS - run;
                    break;
                }
            } else {
                register uint32_t bit_no = 0;

                while( work_val != 0 ) {
                    if( work_val & 1 ) {
                        run++;
                        if( run >= needed ) {
                            best_run = run;
                            best_cluster = cluster + bit_no + 1 - run;
                            bit_no = WORK_BITS + 1;
                            break;
                        }
                    } else {
                        if( run >= needed || run > best_run ) {
                            best_run = run;
                            best_cluster = cluster + bit_no - run;
                            if( run >= needed ) {
                                bit_no = WORK_BITS + 1;
                                break;
                            }
                        }
                        run = 0;
                    }
                    work_val = work_val >> 1;
                    bit_no++;
                }
                if( bit_no < WORK_BITS ) {
                    if( run >= needed || run > best_run ) {
                        best_run = run;
                        best_cluster = cluster + bit_no - run;
                        if( run >= needed )
                            break;
                    }
                    run = 0;
                }
            }
            cluster += WORK_BITS;
            if( work_count-- <= 0 )
                break;
            work_val = *work_ptr++;
        } while( best_run < needed );

        if( $FAILS(sts = deaccesschunk( vioc, 0, 0, TRUE )) )
            break;

        if( best_run >= needed )
            break;

        if( search_words == 0 ) {
            if( run > best_run ) {
                best_run = run;
                best_cluster = cluster - run;
            }
            if( *position == 0 )
                break;
            run = 0;
            search_words = ( (((*position + vcbdev->clustersize -1)
                               / vcbdev->clustersize) + (WORK_BITS -1))
                             / WORK_BITS );
            *position = 0;
            cluster = 0;
            map_block = 2;
        } else
            map_block += blkcount;

        block_offset = 0;
    } while( TRUE );

    if( best_run > needed )
        best_run = needed;
    *count = best_run * vcbdev->clustersize;
    *position = best_cluster * vcbdev->clustersize;
    return sts;
}

#if 0
/************************************************************ headmap_clear() */

/* Release a header from the indexf.sys file header bitmap */

static vmscond_t headmap_clear( struct VCBDEV *vcbdev, uint32_t head_no ) {
    WORK_UNIT *bitmap;
    struct VIOC *vioc;
    register vmscond_t sts;
    register uint32_t map_block;

    map_block = head_no / 4096 + vcbdev->home.hm2$w_cluster * 4 + 1;
    if (head_no <= F11WORD(vcbdev->home.hm2$w_resfiles))
        return SS$_NOPRIV; /* Protect reserved files */

    if( $SUCCESSFUL(sts = accesschunk( vcbdev->idxfcb, map_block,
                                       &vioc, (char **) &bitmap, NULL, 1)) ) {
        bitmap[(head_no % 4096) / WORK_BITS] &= ~(WORK_C(1) << (head_no % WORK_BITS));
        sts = deaccesschunk( vioc, map_block, 1, TRUE );
    }
    return sts;
}
#endif
/********************************************************** update_findhead() */

/* update_findhead() will locate or create a free header in indexf.sys
 * The header will be marked allocated.  indexf.sys is extended if necessary.
 */

static vmscond_t update_findhead( struct VCBDEV *vcbdev, uint32_t *rethead_no,
                                  struct VIOC **retvioc, struct HEAD **headbuff,
                                  uint32_t *retidxblk) {
    uint32_t head_no = 0;
    register vmscond_t sts = 0;
    struct HEAD *idxhead = NULL;
    uint32_t eofblk = 0;

    head_no = 0;

    idxhead = vcbdev->idxfcb->head;
    eofblk = F11SWAP(idxhead->fh2$w_recattr.fat$l_efblk);
    if( F11WORD(idxhead->fh2$w_recattr.fat$w_ffbyte) == 0 )
        --eofblk;

    sts = SS$_NORMAL;

    do {
        struct VIOC *vioc = NULL;
        int modify_flag = FALSE;
        uint32_t blkcount = 0;
        WORK_UNIT *bitmap = NULL, *work_ptr = NULL;
        register uint32_t map_block = 0, work_count = 0;

        map_block = head_no / 4096 + F11WORD( vcbdev->home.hm2$w_cluster ) * 4 + 1;

        if( $FAILS(sts = accesschunk( vcbdev->idxfcb, map_block,
                                      &vioc, (char **)&bitmap, &blkcount, 1 )) )
            return sts;
        work_count = (head_no % 4096) / WORK_BITS;
        work_ptr = bitmap + work_count;
        work_count = blkcount * 512 / WORK_BITS - work_count;
        do {
            register WORK_UNIT work_val;
            register uint32_t bit_no;

            work_val = *work_ptr++;
            if( work_val == WORK_MASK ) {
                head_no += WORK_BITS;
                continue;

            }

            bit_no = 0;
            for( bit_no = 0;
                 bit_no < WORK_BITS;
                 bit_no++, head_no++ ) {

                if( head_no < F11WORD(vcbdev->home.hm2$w_resfiles) )
                    continue;

                if( !(work_val & (WORK_C(1) << bit_no)) ) {
                    register uint32_t idxblk = head_no +
                        F11WORD( vcbdev->home.hm2$w_ibmapvbn ) +
                        F11WORD( vcbdev->home.hm2$w_ibmapsize );

                    if( idxblk > eofblk ) {
                        uint32_t neweof;

                        neweof = idxblk + 1;

                        if( idxblk > vcbdev->idxfcb->hiblock ) {
                            f11long alq;

                            alq = F11WORD(vcbdev->idxfcb->head->fh2$w_recattr.fat$w_defext);
                            if( alq == 0 )
                                alq = F11WORD(vcbdev->home.hm2$w_extend);
                            if( alq == 0 )
                                alq = 1;
                            if( alq < idxblk - vcbdev->idxfcb->hiblock )
                                alq = idxblk - vcbdev->idxfcb->hiblock;

                            if( $FAILS(sts = update_extend( vcbdev->idxfcb, alq, 0 )) ) {
                                if( $FAILS( deaccesschunk( vioc, map_block, blkcount,
                                                           modify_flag )) )
                                    abort();
                                return sts;
                            }
                        }

                        idxhead->fh2$w_recattr.fat$l_efblk = F11SWAP( neweof );
                        idxhead->fh2$w_recattr.fat$w_ffbyte = 0;

                        if( vcbdev->idxfcb->highwater != 0 && neweof > vcbdev->idxfcb->highwater ) {
                            vcbdev->idxfcb->highwater = neweof;
                            idxhead->fh2$l_highwater = F11LONG( neweof );
                        }

                        do {
                            if( $FAILS(sts = accesschunk( vcbdev->idxfcb, ++eofblk, retvioc,
                                                          (char **) headbuff, NULL, 1 )) ) {
                                deaccesschunk( vioc, map_block, blkcount, modify_flag );
                                return sts;
                            }
                            memset( *headbuff, 0, 512 );

                            if( eofblk != idxblk ) {
                                if( $FAILS(sts = deaccesschunk( *retvioc, eofblk, 1, 1 )) ) {
                                    deaccesschunk( vioc, map_block, blkcount, modify_flag );
                                    return sts;
                                }
                            }
                        } while( eofblk < idxblk );

                        work_ptr[-1] |= WORK_C( 1 ) << bit_no;
                        *rethead_no = head_no + 1;
                        *retidxblk = idxblk;
                        deaccesschunk( vioc, map_block, blkcount, TRUE );
                        return sts;
                    }

                    if( $SUCCESSFUL(sts = accesschunk( vcbdev->idxfcb, idxblk, retvioc,
                                                       (char **) headbuff, NULL, 1 )) ) {
                        if( (*headbuff)->fh2$w_checksum == 0 &&
                            (*headbuff)->fh2$w_fid.fid$w_num == 0 &&
                            (*headbuff)->fh2$w_fid.fid$b_nmx == 0 &&
                            (*headbuff)->fh2$w_fid.fid$b_rvn == 0 &&
                            ((*headbuff)->fh2$l_filechar & F11LONG(FH2$M_MARKDEL)) ) {

                            work_ptr[-1] |= WORK_C( 1 ) << bit_no;
                            modify_flag = TRUE;
                            *rethead_no = head_no + 1;
                            *retidxblk = idxblk;
                            return deaccesschunk( vioc, map_block, blkcount, TRUE );
                        }
                        sts = deaccesschunk( *retvioc, 0, 0, FALSE );
                    }
                }
            }
        } while( --work_count != 0 );
        {
            vmscond_t sts2;

            if( $SUCCESSFUL(sts2 = deaccesschunk( vioc, map_block, blkcount, modify_flag )) )
                sts = sts2;
        }
        if( $FAILED(sts) )
            break;
    } while( head_no < F11LONG(vcbdev->home.hm2$l_maxfiles) );

    if( $SUCCESSFUL(sts) && head_no > F11LONG(vcbdev->home.hm2$l_maxfiles) )
        return SS$_IDXFILEFULL;
    return sts;
}

/*********************************************************** update_addhead() */

static vmscond_t update_addhead( struct VCB *vcb, char *filename,
                                 struct fiddef *back,
                                 uint32_t seg_num, struct fiddef *fid,
                                 struct VIOC **vioc, struct HEAD **rethead,
                                 uint32_t *idxblk ) {
    register uint32_t free_space = 0;
    vmscond_t sts;
    register uint32_t device, rvn = 0;
    size_t len;
    uint32_t head_no = 0;
    struct IDENT *id;
    struct HEAD *head;
    struct VCBDEV *vcbdev = NULL;

    if( rethead != NULL )
        *rethead = NULL;

    for (device = 0; device < vcb->devices; device++) {
        if (vcb->vcbdev[device].dev != NULL) {
            if (vcb->vcbdev[device].free_clusters > free_space) {
                free_space = vcb->vcbdev[device].free_clusters;
                vcbdev = &vcb->vcbdev[device];
                rvn = device;
            }
        }
    }
    if( vcbdev == NULL )
        return SS$_DEVICEFULL;

    if( $FAILS(sts = update_findhead( vcbdev, &head_no, vioc, &head, idxblk )) )
        return sts;
#if DEBUG
    printf("Header %d index %u rvn %u\n", head_no, *idxblk, rvn);
#endif
    fid->fid$w_num = head_no;
    fid->fid$w_seq = F11WORD(head->fh2$w_fid.fid$w_seq) + 1;
    if( fid->fid$w_seq == 0 )
        fid->fid$w_seq = 1;
    if( rvn > 0 ) {
        fid->fid$b_rvn = rvn + 1;
    } else {
        fid->fid$b_rvn = 0;
    }
    fid->fid$b_nmx = head_no >> 16;

    memset( head, 0, 512 );

    head->fh2$w_struclev = F11WORD( STRUCLEV );

    head->fh2$w_seg_num = seg_num;
    head->fh2$l_fileowner = vcbdev->home.hm2$w_volowner;
    head->fh2$w_fileprot = vcbdev->home.hm2$w_fileprot;
    fid_copy( &head->fh2$w_fid , fid, 0 );
    head->fh2$w_fid.fid$b_rvn = 0;

    if( back != NULL )
        fid_copy( &head->fh2$w_backlink, back, 0 );
    head->fh2$w_recattr.fat$l_efblk = F11SWAP(1);
    head->fh2$l_highwater = F11LONG( 1 );

    head->fh2$b_idoffset = FH2$C_LENGTH / 2;
    id = (struct IDENT *) ((unsigned short *) head + head->fh2$b_idoffset);

#ifdef MINIMIZE_IDENT
    head->fh2$b_mpoffset = (head->fh2$b_idoffset +
                            (offsetof( struct IDENT, fi2$t_filenamext ) / 2));
#else /* VMS allocates maximum len filename, which simplifies rename.  Use same strategy */
    head->fh2$b_mpoffset = head->fh2$b_idoffset + ( sizeof( struct IDENT ) / 2 );
    memset( id->fi2$t_filenamext, ' ', sizeof( id->fi2$t_filenamext ) );
#endif

    len = strlen( filename );
    if( len <= sizeof( id->fi2$t_filename ) ) {
        memcpy( id->fi2$t_filename, filename, len );
        memset( id->fi2$t_filename + len, ' ', sizeof( id->fi2$t_filename ) - len );
    } else {
        memcpy( id->fi2$t_filename, filename, sizeof( id->fi2$t_filename ) );
        len -= sizeof( id->fi2$t_filename );
#ifdef MINIMIZE_IDENT
        memcpy( id->fi2$t_filenamext, filename + sizeof( id->fi2$t_filename ), len );
        if( len & 1 )
            id->fi2$t_filenamext[len] = ' ';
        head->fh2$b_mpoffset += (f11byte)((len + 1) / 2);
#else
        memcpy( id->fi2$t_filenamext, filename + sizeof( id->fi2$t_filename ), len );
#endif
    }
    id->fi2$w_revision = 0;
    sys_gettim( id->fi2$q_credate );
    memcpy( id->fi2$q_revdate, id->fi2$q_credate, sizeof(id->fi2$q_credate) );

    head->fh2$b_acoffset = offsetof( struct HEAD, fh2$w_checksum ) / 2;
    head->fh2$b_rsoffset = offsetof( struct HEAD, fh2$w_checksum ) / 2;

    if( rethead != NULL )
        *rethead = head;
    return SS$_NORMAL;
}

/************************************************************ update_create() */

/* Create a new file */

vmscond_t update_create( struct VCB *vcb, struct fiddef *did, char *filename,
                         struct fiddef *fid, struct NEWFILE *attrs, struct FCB **fcb ) {
    struct VIOC *vioc;
    struct HEAD *head;
    struct IDENT *id;
    uint32_t idxblk;
    register vmscond_t sts;
    struct UIC dirown;
    static const VMSTIME notset = VMSTIME_ZERO;

    if( !(vcb->status & VCB_WRITE) )
        return SS$_WRITLCK;

    if( $FAILS(sts = accessfile( vcb, did, fcb, FALSE )) )
        return sts;
    head = (*fcb)->head;
    dirown = head->fh2$l_fileowner;
    if( attrs->verlimit == 0 )
        attrs->verlimit = head->fh2$w_recattr.fat$w_versions;
    deaccessfile( *fcb );

    if( $FAILS(sts = update_addhead( vcb, filename, did, 0, fid, &vioc, &head, &idxblk )) )
        return sts;

    if( attrs != NULL ) {
        id = (struct IDENT *)( ((f11word *)head) + F11WORD(head->fh2$b_idoffset) );

        if( memcmp( attrs->credate, notset, sizeof( notset ) ) )
            memcpy( id->fi2$q_credate, attrs->credate, sizeof( VMSTIME ) );
        if( memcmp( attrs->revdate, notset, sizeof( notset ) ) )
            memcpy( id->fi2$q_revdate, attrs->revdate, sizeof( VMSTIME ) );
        if( memcmp( attrs->expdate, notset, sizeof( notset ) ) )
            memcpy( id->fi2$q_expdate, attrs->expdate, sizeof( VMSTIME ) );
        if( memcmp( attrs->bakdate, notset, sizeof( notset ) ) )
            memcpy( id->fi2$q_bakdate, attrs->bakdate, sizeof( VMSTIME ) );
        if( attrs->fileowner.uic$w_mem || attrs->fileowner.uic$w_grp )
            head->fh2$l_fileowner = attrs->fileowner;
        else
            head->fh2$l_fileowner = dirown;
        if( attrs->fileprot & (1u << 16) )
            head->fh2$w_fileprot = (f11word)(attrs->fileprot);

        head->fh2$w_recattr.fat$b_rtype = attrs->recattr.fat$b_rtype;
        head->fh2$w_recattr.fat$b_rattrib = attrs->recattr.fat$b_rattrib;
        head->fh2$w_recattr.fat$w_rsize = attrs->recattr.fat$w_rsize;
        head->fh2$w_recattr.fat$b_bktsize = attrs->recattr.fat$b_bktsize;
        head->fh2$w_recattr.fat$b_vfcsize = attrs->recattr.fat$b_vfcsize;
        head->fh2$w_recattr.fat$w_maxrec = attrs->recattr.fat$w_maxrec;
        head->fh2$w_recattr.fat$w_defext = attrs->recattr.fat$w_defext;
        head->fh2$w_recattr.fat$w_gbc = attrs->recattr.fat$w_gbc;
        memcpy(head->fh2$w_recattr.fat$_UU0, attrs->recattr.fat$_UU0,
               sizeof( head->fh2$w_recattr.fat$_UU0 ));
        head->fh2$w_recattr.fat$w_versions = attrs->recattr.fat$w_versions;
#define CRE_MODIFIABLE (                                                \
                        FH2$M_NOBACKUP | FH2$M_WRITEBACK | FH2$M_READCHECK | \
                        FH2$M_WRITECHECK | FH2$M_CONTIGB | FH2$M_LOCKED | \
                        FH2$M_CONTIG | FH2$M_SPOOL | FH2$M_DIRECTORY |  \
                        FH2$M_NOCHARGE | FH2$M_ERASE | FH2$M_NOMOVE )

        head->fh2$l_filechar = ( ( head->fh2$l_filechar & ~CRE_MODIFIABLE ) |
                                 ( attrs->filechar & CRE_MODIFIABLE ) );
#undef CRE_MODIFIABLE

        if( attrs->revision != 0 )
            id->fi2$w_revision = attrs->revision;
    }
    sts = deaccesshead( vioc, head, idxblk );
    cache_flush();
    if( $SUCCESSFUL(sts) && fcb != NULL) {
        sts = accessfile(vcb, fid, fcb, 1);
    }
#if DEBUG
    printf("(%d,%d,%d) %d\n", fid->fid$w_num, fid->fid$w_seq, fid->fid$b_rvn, sts);
#endif
    cache_flush();

    return sts;
}

/************************************************************ update_extend() */

vmscond_t update_extend( struct FCB *fcb, uint32_t blocks, uint32_t contig ) {
    register vmscond_t sts;
    struct VCBDEV *vcbdev;
    struct VIOC *vioc;
    struct HEAD *head;
    uint32_t headvbn;
    struct fiddef hdrfid;
    uint32_t hdrseq;
    uint32_t start_pos = 0;
    uint32_t block_count;

    if (blocks < 1)
        return SS$_BADPARAM;
    if( !(fcb->status & FCB_WRITE) )
        return SS$_WRITLCK;

    if( fcb->wcb != NULL && cache_refcount( &fcb->wcb->cache ) != 0 ) {
        printf( "Extend while WCB has a reference\n" );
        abort();
    }

    block_count = F11WORD( fcb->head->fh2$w_recattr.fat$w_defext );
    if( block_count == 0 ) {
        vcbdev = RVN_TO_DEV(fcb->vcb, fcb->rvn);
        if( vcbdev == NULL )
            return SS$_DEVNOTMOUNT;
        block_count = F11WORD(vcbdev->home.hm2$w_extend);
    }
    if( blocks < block_count )
        blocks = block_count;

    sts = SS$_NORMAL;

    while( blocks > 0 && $SUCCESSFUL(sts) ) {
        block_count = blocks;

        if (fcb->hiblock > 0) {
            uint32_t mapblk, maplen;

            if( $FAILS(sts = getwindow( fcb, fcb->hiblock, &vcbdev, &mapblk,
                                        &maplen, &hdrfid, &hdrseq)) )
                return sts;
            start_pos = mapblk + 1;
            if( hdrseq != 0 ) {
                if( $FAILS(sts = accesshead( fcb->vcb, &hdrfid, hdrseq,
                                             &vioc, &head, &headvbn, 1 )) )
                    return sts;
            } else {
                head = fcb->head;
                vioc = NULL;
            }
        } else {
            head = fcb->head;
            vioc = NULL;
            start_pos = 0;
        }
        if (vioc == NULL) {
            vcbdev = RVN_TO_DEV(fcb->vcb, fcb->rvn);
            if( vcbdev == NULL )
                return SS$_DEVNOTMOUNT;
        }
        if( start_pos == 0 ) {
            start_pos = /* Randomize initial placement to help contig extends */
                ( ((head->fh2$w_fid.fid$w_num << 16) | head->fh2$w_fid.fid$w_seq) *
                  (head->fh2$w_fid.fid$b_rvn << 24) | 6781 | (head->fh2$w_fid.fid$b_nmx << 8))  %
                vcbdev->max_cluster;
            start_pos *= vcbdev->clustersize;
        }

        if( vcbdev->free_clusters == 0 || head->fh2$b_map_inuse + 4 >=
            head->fh2$b_acoffset - head->fh2$b_mpoffset ) {
            struct VIOC *nvioc;
            struct HEAD *nhead;
            uint32_t nidxblk;

            if( $FAILS(sts = update_addhead(fcb->vcb, "", &head->fh2$w_fid,
                                             head->fh2$w_seg_num+1, &head->fh2$w_ext_fid,
                                             &nvioc, &nhead, &nidxblk)) )
                return sts;
            if (vioc != NULL)
                deaccesshead(vioc, head, headvbn);
            vioc = nvioc;
            head = nhead;
            headvbn = nidxblk;
            vcbdev = RVN_TO_DEV(fcb->vcb, head->fh2$w_fid.fid$b_rvn);
            if (vcbdev == NULL)
                return SS$_DEVNOTMOUNT;
        }
        sts = bitmap_search( vcbdev, &start_pos, &block_count);

#if DEBUG
        printf("Update_extend %d %d %d %d\n", start_pos, block_count, fcb->hiblock, vcbdev->free_clusters);
#endif
        if( $SUCCESSFUL(sts) ) {
            if( block_count < 1 ||
                (contig && block_count < blocks) ) {
                sts = SS$_DEVICEFULL;
            } else {
                uint32_t alloc_start, alloc_count;

                alloc_start = start_pos;
                alloc_count = block_count;

                addmappointers( head, &start_pos, &block_count, 0 );

                if( fcb->wcb != NULL )
                    cache_delete( &fcb->wcb->cache );

                alloc_count -= block_count;

                fcb->hiblock += alloc_count;

                if( alloc_count >= blocks )
                    blocks = 0;
                else
                    blocks -= alloc_count;

                sts = bitmap_modify( vcbdev,
                                     alloc_start / vcbdev->clustersize,
                                     alloc_count / vcbdev->clustersize, 0 );
            }
        }
        if( vioc != NULL )
            deaccesshead(vioc, head, headvbn);
    }

    return sts;
}

/************************************************************** deallocfile() */

vmscond_t deallocfile( struct FCB *fcb ) {
    register vmscond_t sts = SS$_NORMAL;
    /*
    First mark all file clusters as free in BITMAP.SYS
    */
    register uint32_t vbn = 0;
    for( vbn = 1; vbn <= fcb->hiblock; ) {
        register vmscond_t sts = 0;
        uint32_t phyblk = 0, phylen = 0;
        struct VCBDEV *vcbdev = NULL;

        if( $FAILS(sts = getwindow( fcb, vbn, &vcbdev, &phyblk, &phylen, NULL, NULL )) )
            break;
        vbn += phylen;
        phyblk /= vcbdev->clustersize;
        phylen = (phylen + vcbdev->clustersize -1)/vcbdev->clustersize;

        if( $FAILS(sts = bitmap_modify( vcbdev, phyblk, phylen, 1 )) )
            break;
    }
    /*
    Now reset file header bit map in INDEXF.SYS and
    update each of the file headers...
    */
    {
        uint32_t rvn = fcb->rvn;
        uint32_t headvbn = fcb->headvbn;
        struct HEAD *head = fcb->head;
        struct VIOC *headvioc = fcb->headvioc;

        while (TRUE) {
            uint32_t ext_seg_num = 0;
            struct fiddef extfid;
            WORK_UNIT *bitmap = NULL;
            struct VIOC *vioc = NULL;
            register uint32_t filenum = 0;
            register struct VCBDEV *vcbdev = NULL;
            register uint32_t idxblk = 0;

            filenum = ((size_t)head->fh2$w_fid.fid$b_nmx << 16) +
                (size_t)head->fh2$w_fid.fid$w_num - 1;

            vcbdev = RVN_TO_DEV(fcb->vcb, rvn);
            if (vcbdev == NULL) {
				sts = SS$_DEVNOTMOUNT;
				break;
			}
            idxblk = (size_t)filenum / 4096 + (size_t)vcbdev->home.hm2$w_cluster * 4 + 1;

            if( $FAILS(sts = accesschunk( vcbdev->idxfcb, idxblk, &vioc,
                                          (char **) &bitmap, NULL, 1 )) )
                break;

            bitmap[(filenum % 4096) / WORK_BITS] &=
                ~(WORK_C( 1 ) << (filenum % WORK_BITS));

            sts = deaccesschunk( vioc, idxblk, 1, TRUE );

            head->fh2$l_filechar |= F11LONG( FH2$M_MARKDEL );
            head->fh2$w_fid.fid$w_num = 0;
            head->fh2$w_fid.fid$b_rvn = 0;
            head->fh2$w_fid.fid$b_nmx = 0;
            head->fh2$w_checksum = 0;
            ext_seg_num++;
            memcpy(&extfid, &fcb->head->fh2$w_ext_fid, sizeof(struct fiddef));

            if( $FAILS(sts = deaccesshead(headvioc, NULL, headvbn)) )
                break;
            if (extfid.fid$b_rvn == 0) {
                extfid.fid$b_rvn = rvn;
            } else {
                rvn = extfid.fid$b_rvn;
            }
            if( extfid.fid$w_num != 0 || extfid.fid$b_nmx != 0) {
                if( $FAILS(sts = accesshead( fcb->vcb, &extfid , ext_seg_num,
                                             &headvioc, &head, &headvbn, 1 )) )
                    break;
            } else
                break;
        }
        if( $SUCCESSFUL(sts) ) {
            fcb->headvioc = NULL;
            cache_untouch( &fcb->cache, TRUE );
            cache_delete( &fcb->cache );
        }
    }
    return sts;
}

/************************************************************** accesserase() */

/* accesserase: delete a file... */

 vmscond_t accesserase( struct VCB * vcb, struct fiddef * fid ) {
    struct FCB *fcb;
    register vmscond_t sts;
    struct VCBDEV *vcbdev;

    vcbdev = RVN_TO_DEV(vcb, fid->fid$b_rvn);
    if( vcbdev == NULL )
        return SS$_DEVNOTMOUNT;
    if( (fid->fid$w_num | ((uint32_t)fid->fid$b_nmx << 16)) <=
        (uint32_t)F11WORD( vcbdev->home.hm2$w_resfiles ) ) {
        return SS$_NOPRIV;
    }

    if( $SUCCESSFUL(sts = accessfile(vcb, fid, &fcb, 1)) ) {
        fcb->head->fh2$l_filechar |= F11LONG(FH2$M_MARKDEL);
#if DEBUG
        printf("Accesserase ... \n");
#endif
        sts = deaccessfile( fcb );
    }
    return sts;
}

/************************************************************** update_truncate() */

vmscond_t update_truncate( struct FCB *fcb, uint32_t newsize ) {
    struct HEAD *head;
    f11word *mp, *ep;
#if DEBUG
    vmscond_t sts;
#endif
    uint32_t clustersize, eofblk, vbn = 1;
    struct VCBDEV *vcbdev;
    int removed = 0;

    if( !(fcb->status & FCB_WRITE) )
        return SS$_WRITLCK;

    if( fcb->wcb != NULL && cache_refcount( &fcb->wcb->cache ) != 0 ) {
        printf( "Truncate while WCB has a reference\n" );
        abort();
    }

    head = fcb->head;

    if( (vcbdev = RVN_TO_DEV( fcb->vcb, head->fh2$w_fid.fid$b_rvn )) == NULL )
        return SS$_BUGCHECK;

#if DEBUG
#define release(lbn, count)                                             \
    if( (lbn) % clustersize || (count) % clustersize || !(count) )      \
        abort();                                                        \
    if( $FAILS(sts = bitmap_modify( vcbdev, ((lbn)/clustersize),        \
                                    (count)/clustersize,                \
                                    TRUE )) )                           \
        abort();
#else
#define release(lbn, count)                                             \
    (void) bitmap_modify( vcbdev, ((lbn)/clustersize),                  \
                          (count)/clustersize,                          \
                          TRUE )
#endif

    clustersize = F11WORD( vcbdev->home.hm2$w_cluster );

    newsize = ( (newsize + clustersize -1) / clustersize ) * clustersize;

    if( newsize == fcb->hiblock )
        return SS$_NORMAL;

    mp = ((f11word *)head) + head->fh2$b_mpoffset;
    ep = mp + head->fh2$b_map_inuse;

    while( mp < ep ) {
        f11word e0;
        uint32_t count, pbn, newcount;

        e0 = *mp++;
        e0 = F11WORD(e0);
        switch( e0 &  FM2$M_FORMAT3 ) {
        case  FM2$M_FORMAT1:
            count = (e0 & 0xff);
            pbn = F11WORD(*mp) | (((e0 & ~FM2$M_FORMAT3) >> 8) << 16);
            mp++;
            if( vbn + count <= newsize )
                break;
            if( vbn > newsize ) {
                removed += 2;
                release( pbn, count + 1 );
                mp[-2] = 0;
                mp[-1] = 0;
                break;
            }
            newcount = newsize + 1 - vbn;
            mp[-2] = F11WORD( (newcount -1) |
                              ((pbn >> 8) & 0x3f00) |
                              FM2$M_FORMAT1 );
            pbn += newcount;
            release( pbn,  (count + 1) - newcount );
            break;
        case  FM2$M_FORMAT2:
            count = (e0 & ~FM2$M_FORMAT3);
            pbn = *mp++;
            pbn = F11WORD( pbn );
            pbn |= F11WORD (*mp ) << 16;
            mp++;
            if( vbn + count <= newsize )
                break;
            if( vbn > newsize ) {
                removed += 3;
                release( pbn, count +1 );
                mp[-3] = 0;
                mp[-2] = 0;
                mp[-1] = 0;
                break;
            }
            newcount = newsize + 1 - vbn;
            mp[-3] = F11WORD( (newcount -1) | FM2$M_FORMAT2 );
            pbn += newcount;
            release( pbn,  (count + 1) - newcount );
            break;
        case FM2$M_FORMAT3:
            count = ((e0 & ~FM2$M_FORMAT3) << 16);
            count |= F11WORD( *mp );
            mp++;
            pbn = *mp++;
            pbn = F11WORD( pbn );
            pbn |= F11WORD (*mp ) << 16;
            mp++;
            if( vbn + count <= newsize )
                break;
            if( vbn > newsize ) {
                removed += 4;
                release( pbn, count + 1 );
                mp[-4] = 0;
                mp[-3] = 0;
                mp[-2] = 0;
                mp[-1] = 0;
                break;
            }
            newcount = newsize + 1 - vbn;
            mp[-4] = F11WORD( ((newcount -1) >> 16) | FM2$M_FORMAT3 );
            mp[-3] = F11WORD( (f11word)(newcount -1) );

            pbn += newcount;
            release( pbn,  (count + 1) - newcount );
            break;
        default:
            continue;
        }
        vbn += count + 1;
    }
#if DEBUG
    printf( "Truncate removed %u map words, end hiblk = %u, vbn = %u, newsze = %u", removed, fcb->hiblock, vbn, newsize );
#endif
    head->fh2$b_map_inuse -= removed;

    fcb->hiblock = newsize;
    fcb->head->fh2$w_recattr.fat$l_hiblk = F11SWAP( newsize );

    eofblk = F11SWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
    if( fcb->head->fh2$w_recattr.fat$w_ffbyte == 0 )
        --eofblk;
#if DEBUG
    printf( " Eofblk = %u\n", eofblk );
#endif
    if( eofblk > newsize ) {
        fcb->head->fh2$w_recattr.fat$l_efblk = F11SWAP( newsize + 1 );
        fcb->head->fh2$w_recattr.fat$w_ffbyte = 0;
    }

    if( fcb->head->fh2$b_idoffset > 39 &&
        fcb->head->fh2$l_highwater != 0 &&
        F11LONG(fcb->head->fh2$l_highwater) > newsize + 1 )
        fcb->head->fh2$l_highwater = F11LONG( newsize + 1 );

    /* Recompute CONTIG by adding 0 blocks */

    eofblk = 0;
    addmappointers( fcb->head, &eofblk, &eofblk, 0 );

    if( fcb->wcb != NULL )
        cache_delete( &fcb->wcb->cache );

    return SS$_NORMAL;
}
