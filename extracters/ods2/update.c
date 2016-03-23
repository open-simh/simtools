/* Update.c */

#if !defined( DEBUG ) && defined( DEBUG_UPDATE )
#define DEBUG DEBUG_UPDATE
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "access.h"
#include "device.h"
#include "ods2.h"
#include "phyio.h"
#include "ssdef.h"
#include "stsdef.h"

#ifdef VMS
#include <starlet.h>
#else
#include "vmstime.h"
#endif

unsigned deaccesshead(struct VIOC *vioc,struct HEAD *head,unsigned idxblk);
unsigned accesshead(struct VCB *vcb,struct fiddef *fid,unsigned seg_num,
                    struct VIOC **vioc,struct HEAD **headbuff,
                    unsigned *retidxblk,unsigned wrtflg);
unsigned getwindow(struct FCB * fcb,unsigned vbn,struct VCBDEV **devptr,
                   unsigned *phyblk,unsigned *phylen,struct fiddef *hdrfid,
                   unsigned *hdrseq);

/* Bitmaps get accesses in 'WORK_UNITs' which can be an integer
   on a little endian machine but must be a byte on a big endian system */

#ifdef ODS2_BIG_ENDIAN
#define WORK_UNIT unsigned char
#define WORK_MASK 0xff
#else
#define WORK_UNIT unsigned int
#define WORK_MASK ((WORK_UNIT)~0)
#endif
#define WORK_BITS (sizeof(WORK_UNIT) * 8)

/********************************************************* update_freecount() */

/* update_freecount() to read the device cluster bitmap and compute
   the number of un-used clusters */

unsigned update_freecount(struct VCBDEV *vcbdev,unsigned *retcount)
{
    register unsigned sts = SS$_NORMAL;
    register unsigned free_clusters = 0;
    register unsigned map_block, map_end;
    map_end = (vcbdev->max_cluster + 4095) / 4096 + 2;
    for (map_block = 2; map_block < map_end; ) {
        struct VIOC *vioc;
        unsigned blkcount;
        WORK_UNIT *bitmap,*work_ptr;
        register unsigned work_count;
        sts = accesschunk(vcbdev->mapfcb,map_block, &vioc,(char **) &bitmap,
                          &blkcount,0);
        if (!(sts & STS$M_SUCCESS)) return sts;
        if (blkcount > map_end - map_block) blkcount = map_end - map_block + 1;
        work_ptr = bitmap;
        work_count = blkcount * 512 / sizeof(WORK_UNIT);
        do {
            register WORK_UNIT work_val = *work_ptr++;
            if (work_val == WORK_MASK) {
                free_clusters += WORK_BITS;
            } else {
                while (work_val != 0) {
                    if (work_val & 1) free_clusters++;
                    work_val = work_val >> 1;
                }
            }
        } while (--work_count > 0);
        sts = deaccesschunk(vioc,0,0,TRUE);
        if (!(sts & STS$M_SUCCESS)) return sts;
        map_block += blkcount;
    }
    *retcount = free_clusters;
    return sts;
}

/************************************************************ bitmap_modify() */

/* bitmap_modify() will either set or release a block of bits in the
   device cluster bitmap */

unsigned bitmap_modify(struct VCBDEV *vcbdev,
                       unsigned cluster, unsigned count,
                       unsigned release_flag) {
    register unsigned sts;
    register unsigned clust_count;
    register unsigned map_block;
    register unsigned block_offset;

    clust_count = count;
    map_block = cluster / 4096 + 2;
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
        struct VIOC *vioc;
        unsigned blkcount;
        WORK_UNIT *bitmap;
        register WORK_UNIT *work_ptr;
        register unsigned work_count;
        register unsigned bit_no;

        sts = accesschunk( vcbdev->mapfcb, map_block, &vioc, (char **) &bitmap,
                           &blkcount, 1 );
        if( !(sts & STS$M_SUCCESS) )
            return sts;

        work_ptr = bitmap + block_offset / WORK_BITS;

        if( (bit_no = block_offset % WORK_BITS) != 0 ) {
            register WORK_UNIT bit_mask = WORK_MASK;

            if( bit_no + clust_count < WORK_BITS ) {
                bit_mask = bit_mask >> (WORK_BITS - clust_count);
                clust_count = 0;
            } else {
                clust_count -= WORK_BITS - bit_no;
            }
            bit_mask = bit_mask << bit_no;
            if (release_flag) {
#if  DEBUG
                if( !(*work_ptr & bit_mask) ) {
                    *work_ptr |= bit_mask;
                } /* else BUGCHECK( freeing unallocated cluster(s) ) */
                else abort();
#else
                *work_ptr |= bit_mask;
#endif
            } else {
#if DEBUG
                if( *work_ptr & bit_mask ) {
                    *work_ptr &= ~bit_mask;
                } /* else BUGCHECK( allocating allocated cluster(s) ) */
                else abort();
#else
                *work_ptr &= ~bit_mask;
#endif
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
        if (release_flag) {
            while( work_count-- > 0 ) {
#if DEBUG
                if( *work_ptr != 0 )
                    abort(); /* else BUGCHECK( freeing unallocated cluster(s) ) */
#endif
                *work_ptr++ = WORK_MASK;
            }
        } else {
            while( work_count-- > 0 ) {
#if DEBUG
                if( *work_ptr != WORK_MASK )
                    abort(); /* else BUGCHECK( allocating allocated cluster(s) ) */
#endif
                *work_ptr++ = 0;
            }
        }
        if( clust_count != 0 && block_offset ) {
            register WORK_UNIT bit_mask;

            bit_mask = WORK_MASK >> (WORK_BITS - clust_count);
            if (release_flag) {
#if DEBUG
                if( !(*work_ptr & bit_mask) ) {
                    *work_ptr |= bit_mask;
                } /* else BUGCHECK( freeing unallocated cluster(s) ) */
                else abort();
#else
                *work_ptr |= bit_mask;
#endif
            } else {
#if DEBUG
                if( *work_ptr & bit_mask ) {
                    *work_ptr &= ~bit_mask;
                } /* else BUGCHECK( allocating allocated cluster(s) ) */
                else abort();
#else
                *work_ptr &= ~bit_mask;
#endif
            }
            ++work_ptr;

            clust_count = 0;
        }
        sts = deaccesschunk( vioc, map_block, blkcount, TRUE );
        if( !(sts & STS$M_SUCCESS) )
            return sts;
        map_block += blkcount;
        block_offset = 0;
    } while( clust_count != 0 );

    return sts;
}

/************************************************************ bitmap_search() */

/* bitmap_search() is a routine to find a pool of free clusters in the
   device cluster bitmap */

unsigned bitmap_search(struct VCBDEV *vcbdev,unsigned *position,unsigned *count)
{
    register unsigned sts;
    register unsigned map_block,block_offset;
    register unsigned search_words,needed;
    register unsigned run = 0,cluster;
    register unsigned best_run = 0,best_cluster = 0;
    needed = *count;
    if (needed < 1 || needed > vcbdev->max_cluster + 1) return SS$_BADPARAM;
    cluster = *position;
    if (cluster + *count > vcbdev->max_cluster + 1) cluster = 0;
    map_block = cluster / 4096 + 2;
    block_offset = cluster % 4096;
    cluster = cluster - (cluster % WORK_BITS);
    search_words = vcbdev->max_cluster / WORK_BITS;
    do {
        struct VIOC *vioc;
        unsigned blkcount;
        WORK_UNIT *bitmap;
        register WORK_UNIT *work_ptr, work_val;
        register unsigned work_count;
        sts = accesschunk(vcbdev->mapfcb,map_block,&vioc,(char **) &bitmap,
                          &blkcount,1);
        if (!(sts & STS$M_SUCCESS)) return sts;
        work_ptr = bitmap + block_offset / WORK_BITS;
        work_val = *work_ptr++;
        if (block_offset % WORK_BITS) {
            work_val = work_val && (WORK_MASK << block_offset % WORK_BITS);
        }
        work_count = (blkcount * 4096 - block_offset) / WORK_BITS;
        if (work_count > search_words) work_count = search_words;
        search_words -= work_count;
        do {
            if (work_val == WORK_MASK) {
                run += WORK_BITS;
            } else {
                register unsigned bit_no = 0;
                while (work_val != 0) {
                    if (work_val & 1) {
                        run++;
                    } else {
                        if (run > best_run) {
                            best_run = run;
                            best_cluster = cluster + bit_no;
                        }
                        run = 0;
                    }
                    work_val = work_val >> 1;
                    bit_no++;
                }
                if (bit_no < WORK_BITS) {
                    if (run > best_run) {
                        best_run = run;
                        best_cluster = cluster + bit_no;
                    }
                    run = 0;
                }
            }
            cluster += WORK_BITS;
            if (work_count-- > 0) {
                work_val = *work_ptr++;
            } else {
                break;
            }
        } while (best_run < needed);
        sts = deaccesschunk(vioc,map_block,0,TRUE);
        if (!(sts & STS$M_SUCCESS)) break;
        map_block += blkcount;
        block_offset = 0;
    } while (best_run < needed && search_words != 0);
    if (best_run > needed) best_run = needed;
    *count = best_run;
    *position = best_cluster;
    return sts;
}

/************************************************************ headmap_clear() */

/* headmap_clear() will release a header from the indexf.sys file header
   bitmap */

unsigned headmap_clear(struct VCBDEV *vcbdev,unsigned head_no)
{
    WORK_UNIT *bitmap;
    struct VIOC *vioc;
    register unsigned sts;
    register unsigned map_block;
    map_block = head_no / 4096 + vcbdev->home.hm2$w_cluster * 4 + 1;
    if (head_no <= F11WORD(vcbdev->home.hm2$w_resfiles)) return SS$_NOPRIV; /* Protect reserved files */
    sts = accesschunk(vcbdev->idxfcb,map_block,&vioc,(char **) &bitmap,NULL,1);
    if (sts & STS$M_SUCCESS) {
        bitmap[(head_no % 4096) / WORK_BITS] &= ~(1 << (head_no % WORK_BITS));
        sts = deaccesschunk(vioc,map_block,1,TRUE);
    }
    return sts;
}

/********************************************************** update_findhead() */

/* update_findhead() will locate a free header from indexf.sys */

unsigned update_findhead(struct VCBDEV *vcbdev,unsigned *rethead_no,
                         struct VIOC **retvioc,struct HEAD **headbuff,
                         unsigned *retidxblk)
{
    unsigned head_no = 0;
    register unsigned sts;
    do {
        struct VIOC *vioc;
        int modify_flag = FALSE;
        unsigned blkcount;
        WORK_UNIT *bitmap,*work_ptr;
        register unsigned map_block,work_count;
        map_block = head_no / 4096 + vcbdev->home.hm2$w_cluster * 4 + 1;
        sts = accesschunk(vcbdev->idxfcb,map_block,
            &vioc,(char **) &bitmap,&blkcount,1);
        if (!(sts & STS$M_SUCCESS)) return sts;
        work_count = (head_no % 4096) / WORK_BITS;
        work_ptr = bitmap + work_count;
        work_count = blkcount * 512 / WORK_BITS - work_count;
        do {
            register WORK_UNIT work_val = *work_ptr;
            if (work_val == WORK_MASK) {
                head_no += WORK_BITS;
            } else {
                register unsigned bit_no = 0;
                for (bit_no = 0; bit_no < WORK_BITS; bit_no++) {
                    if ((work_val & (1 << bit_no)) == 0) {
                        register unsigned idxblk = head_no +
                            F11WORD(vcbdev->home.hm2$w_ibmapvbn) +
                            F11WORD(vcbdev->home.hm2$w_ibmapsize);
                        sts = accesschunk(vcbdev->idxfcb,idxblk,retvioc,
                                          (char **) headbuff,NULL,1);
                        if (sts & STS$M_SUCCESS) {
                            *work_ptr |= 1 << bit_no;
                            modify_flag = TRUE;
                            if( (*headbuff)->fh2$w_checksum != 0 ||
                                (*headbuff)->fh2$w_fid.fid$w_num != 0 ||
                                !(F11LONG((*headbuff)->fh2$l_filechar) &
                                   FH2$M_MARKDEL) ) {
                                sts = deaccesschunk(*retvioc,0,0,FALSE);
                            } else {
                                *rethead_no = head_no + 1;
                                *retidxblk = idxblk;
                                deaccesschunk(vioc,map_block,blkcount,
                                              modify_flag);
                                return SS$_NORMAL;
                            }
                        }
                    }
                    head_no++;
                }
            }
            work_ptr++;
        } while (--work_count != 0);
        deaccesschunk(vioc,map_block,blkcount,modify_flag);
        if (!(sts & STS$M_SUCCESS)) break;
    } while (head_no < F11LONG(vcbdev->home.hm2$l_maxfiles));
    return sts;
}

/*********************************************************** update_addhead() */

unsigned update_addhead(struct VCB *vcb,char *filename,struct fiddef *back,
                     unsigned seg_num,struct fiddef *fid,
                     struct VIOC **vioc,struct HEAD **rethead,
                     unsigned *idxblk)
{
    register unsigned free_space = 0;
    register unsigned device,rvn,sts;
    unsigned head_no;
    struct IDENT *id;
    struct HEAD *head;
    struct VCBDEV *vcbdev = NULL;

    if( rethead != NULL ) *rethead = NULL;

    for (device = 0; device < vcb->devices; device++) {
        if (vcb->vcbdev[device].dev != NULL) {
            if (vcb->vcbdev[device].free_clusters > free_space) {
                free_space = vcb->vcbdev[device].free_clusters;
                vcbdev = &vcb->vcbdev[device];
                rvn = device;
            }
        }
    }
    if (vcbdev == NULL) return SS$_DEVICEFULL;

    sts = update_findhead(vcbdev,&head_no,vioc,&head,idxblk);
    if (!(sts & STS$M_SUCCESS)) return sts;
    printf("Header %d index %u rvn %u\n",head_no,*idxblk,rvn);
    fid->fid$w_num = head_no;
    fid->fid$w_seq = ++head->fh2$w_fid.fid$w_seq;
    if (fid->fid$w_seq == 0) fid->fid$w_seq = 1;
    if (rvn > 0) {
        fid->fid$b_rvn = rvn + 1;
    } else {
        fid->fid$b_rvn = 0;
    }
    fid->fid$b_nmx = head_no >> 16;
    memset(head,0,512);
    head->fh2$b_idoffset = 40;
    head->fh2$b_mpoffset = 100;
    head->fh2$b_acoffset = 255;
    head->fh2$b_rsoffset = 255;
    head->fh2$w_seg_num = seg_num;
    head->fh2$w_struclev = 513;
    head->fh2$l_fileowner.uic$w_mem = 4;
    head->fh2$l_fileowner.uic$w_grp = 1;
    fid_copy(&head->fh2$w_fid,fid,0);
    if (back != NULL) fid_copy(&head->fh2$w_backlink,back,0);
    id = (struct IDENT *) ((unsigned short *) head + 40);
    memset(id->fi2$t_filenamext,' ',66);
    if (strlen(filename) < 20) {
        memset(id->fi2$t_filename,' ',20);
        memcpy(id->fi2$t_filename,filename,strlen(filename));
    } else {
        memcpy(id->fi2$t_filename,filename,20);
        memcpy(id->fi2$t_filenamext,filename+20,strlen(filename+20));
    }
    id->fi2$w_revision = 1;
    sys$gettim(id->fi2$q_credate);
    memcpy(id->fi2$q_revdate,id->fi2$q_credate,sizeof(id->fi2$q_credate));
    memcpy(id->fi2$q_expdate,id->fi2$q_credate,sizeof(id->fi2$q_credate));
    head->fh2$w_recattr.fat$l_efblk = F11SWAP(1);
    {
        unsigned short check = checksum((f11word *) head);
        head->fh2$w_checksum = F11WORD(check);
    }
    if( rethead != NULL ) *rethead = head;
    return SS$_NORMAL;
}

/************************************************************ update_create() */

/* update_create() will create a new file... */

unsigned update_create(struct VCB *vcb,struct fiddef *did,char *filename,
                       struct fiddef *fid,struct FCB **fcb)
{
    struct VIOC *vioc;
    struct HEAD *head;
    unsigned idxblk;
    register unsigned sts;
    sts = update_addhead(vcb,filename,did,0,fid,&vioc,&head,&idxblk);
    if (!(sts & STS$M_SUCCESS)) return sts;
    sts = deaccesshead(vioc,head,idxblk);
    if (sts & STS$M_SUCCESS && fcb != NULL) {
        sts = accessfile(vcb,fid,fcb,1);
    }
    printf("(%d,%d,%d) %d\n",fid->fid$w_num,fid->fid$w_seq,fid->fid$b_rvn,sts);
    return sts;
}

/************************************************************ update_extend() */

unsigned update_extend(struct FCB *fcb,unsigned blocks,unsigned contig)
{
    register unsigned sts;
    struct VCBDEV *vcbdev;
    struct VIOC *vioc;
    struct HEAD *head;
    unsigned headvbn;
    struct fiddef hdrfid;
    unsigned hdrseq;
    unsigned start_pos = 0;
    unsigned block_count = blocks;
    if (block_count < 1) return 0;
    if( !(fcb->status & FCB_WRITE) ) return SS$_WRITLCK;
    if (fcb->hiblock > 0) {
        unsigned mapblk,maplen;
        sts = getwindow(fcb,fcb->hiblock,&vcbdev,&mapblk,&maplen,&hdrfid,
                        &hdrseq);
        if (!(sts & STS$M_SUCCESS)) return sts;
        start_pos = mapblk + 1;
        if (hdrseq != 0) {
            sts = accesshead(fcb->vcb,&hdrfid,hdrseq,&vioc,&head,&headvbn,1);
            if (!(sts & STS$M_SUCCESS)) return sts;
        } else {
            head = fcb->head;
            vioc = NULL;
        }
    } else {
        head = fcb->head;
        vioc = NULL;
        start_pos = 0;          /* filenum * 3 /indexfsize * volumesize; */
    }
    if (vioc == NULL) {
		vcbdev = RVN_TO_DEV(fcb->vcb,fcb->rvn);
		if (vcbdev == NULL) return SS$_DEVNOTMOUNT;
	}
    if (vcbdev->free_clusters == 0 || head->fh2$b_map_inuse + 4 >=
                head->fh2$b_acoffset - head->fh2$b_mpoffset) {
        struct VIOC *nvioc;
        struct HEAD *nhead;
        unsigned nidxblk;
        sts = update_addhead(fcb->vcb,"",&head->fh2$w_fid,head->fh2$w_seg_num+1,
              &head->fh2$w_ext_fid,&nvioc,&nhead,&nidxblk);
        if (!(sts & STS$M_SUCCESS)) return sts;
        if (vioc != NULL) deaccesshead(vioc,head,headvbn);
        vioc = nvioc;
        head = nhead;
        headvbn = nidxblk;
        vcbdev = RVN_TO_DEV(fcb->vcb,head->fh2$w_fid.fid$b_rvn);
		if (vcbdev == NULL) return SS$_DEVNOTMOUNT;
    }
    sts = bitmap_search(vcbdev,&start_pos,&block_count);
    printf("Update_extend %d %d\n",start_pos,block_count);
    if (sts & STS$M_SUCCESS) {
        if( block_count < 1 ||
            (contig && block_count * vcbdev->clustersize < blocks) ) {
            sts = SS$_DEVICEFULL;
        } else {
            register unsigned short *mp;
            mp = (unsigned short *) head + head->fh2$b_mpoffset +
                 head->fh2$b_map_inuse;
            *mp++ = (3 << 14) | ((block_count *vcbdev->clustersize - 1) >> 16);
            *mp++ = (block_count * vcbdev->clustersize - 1) & 0xffff;
            *mp++ = (start_pos * vcbdev->clustersize) & 0xffff;
            *mp++ = (start_pos * vcbdev->clustersize) >> 16;
            head->fh2$b_map_inuse += 4;
            fcb->hiblock += block_count * vcbdev->clustersize;
            fcb->head->fh2$w_recattr.fat$l_hiblk =
                F11SWAP(fcb->hiblock * vcbdev->clustersize);
            sts = bitmap_modify(vcbdev,start_pos,block_count,0);
        }
    }
    if (vioc != NULL) deaccesshead(vioc,head,headvbn);
    return sts;
}

/************************************************************** deallocfile() */

unsigned deallocfile(struct FCB *fcb)
{
    register unsigned sts = SS$_NORMAL;
    /*
    First mark all file clusters as free in BITMAP.SYS
    */
    register unsigned vbn;
    for( vbn = 1; vbn <= fcb->hiblock; ) {
        register unsigned sts;
        unsigned phyblk,phylen;
        struct VCBDEV *vcbdev;

        sts = getwindow( fcb, vbn, &vcbdev, &phyblk, &phylen, NULL, NULL );
        if( !(sts & STS$M_SUCCESS) )
            break;
        vbn += phylen;
        phyblk /= vcbdev->clustersize;
        phylen = (phylen + vcbdev->clustersize -1)/vcbdev->clustersize;
        sts = bitmap_modify( vcbdev, phyblk, phylen, 1 );
        if( !(sts & STS$M_SUCCESS) )
            break;
    }
    /*
    Now reset file header bit map in INDEXF.SYS and
    update each of the file headers...
    */
    {
        unsigned rvn = fcb->rvn;
        unsigned headvbn = fcb->headvbn;
        struct HEAD *head = fcb->head;
        struct VIOC *headvioc = fcb->headvioc;

        while (TRUE) {
            unsigned ext_seg_num = 0;
            struct fiddef extfid;
            unsigned *bitmap;
            struct VIOC *vioc;
            register unsigned filenum = (head->fh2$w_fid.fid$b_nmx << 16) +
                head->fh2$w_fid.fid$w_num - 1;
            register struct VCBDEV *vcbdev = RVN_TO_DEV(fcb->vcb,rvn);
            register unsigned idxblk;
            if (vcbdev == NULL) {
				sts = SS$_DEVNOTMOUNT;
				break;
			}
            idxblk = filenum / 4096 + vcbdev->home.hm2$w_cluster * 4 + 1;
            sts = accesschunk(vcbdev->idxfcb,idxblk,&vioc,(char **) &bitmap,
                              NULL,1);
            if( !(sts & STS$M_SUCCESS) )
                break;
            bitmap[(filenum % 4096) / WORK_BITS] &=
                ~(1 << (filenum % WORK_BITS));
            sts = deaccesschunk(vioc,idxblk,1,TRUE);
            head->fh2$w_fid.fid$w_num = 0;
            head->fh2$w_fid.fid$b_rvn = 0;
            head->fh2$w_fid.fid$b_nmx = 0;
            head->fh2$w_checksum = 0;
            ext_seg_num++;
            memcpy(&extfid,&fcb->head->fh2$w_ext_fid,sizeof(struct fiddef));
            sts = deaccesshead(headvioc,NULL,headvbn);
            if( !(sts & STS$M_SUCCESS) )
                break;
            if (extfid.fid$b_rvn == 0) {
                extfid.fid$b_rvn = rvn;
            } else {
                rvn = extfid.fid$b_rvn;
            }
            if (extfid.fid$w_num != 0 || extfid.fid$b_nmx != 0) {
                sts = accesshead(fcb->vcb,&extfid,ext_seg_num,&headvioc,&head,
                                 &headvbn,1);
                if (!(sts & STS$M_SUCCESS)) break;
            } else {
                break;
            }
        }
        if (sts & STS$M_SUCCESS) {
            fcb->headvioc = NULL;
            cache_untouch(&fcb->cache,FALSE);
            cache_delete(&fcb->cache);
        }
    }
    return sts;
}

/************************************************************** accesserase() */

/* accesserase: delete a file... */

unsigned accesserase(struct VCB * vcb,struct fiddef * fid)
{
    struct FCB *fcb;
    register int sts;
    struct VCBDEV *vcbdev;

    vcbdev = RVN_TO_DEV(vcb,fid->fid$b_rvn);
    if (vcbdev == NULL)
        return SS$_DEVNOTMOUNT;
    if( (fid->fid$w_num | (fid->fid$b_nmx << 16)) <=
        F11WORD( vcbdev->home.hm2$w_resfiles ) ) {
        return SS$_NOPRIV;
    }

    sts = accessfile(vcb,fid,&fcb,1);
    if (sts & STS$M_SUCCESS) {
        fcb->head->fh2$l_filechar |= FH2$M_MARKDEL;
#if DEBUG
        printf("Accesserase ... \n");
#endif
        sts = deaccessfile(fcb);
    }
    return sts;
}

/******************************************************************* extend() */

#ifdef EXTEND

unsigned extend(struct FCB *fcb,unsigned blocks)
{
    register unsigned sts;
    struct VCBDEV *vcbdev;
    unsigned clusterno;
    unsigned extended = 0;
    if( !(fcb->status & FCB_WRITE) ) return SS$_WRITLCK;
    if (fcb->hiblock > 0) {
        unsigned phyblk,phylen;
        sts = getwindow(fcb,fcb->hiblock,&vcbdev,&phyblk,&phylen,NULL,NULL);
        clusterno = (phyblk + 1) / vcbdev->home.hm2$w_cluster;
        if (!(sts & STS$M_SUCCESS)) return sts;
    } else {
        vcbdev = fcb->vcb->vcbdev;
        clusterno = 0;          /* filenum * 3 /indexfsize * volumesize; */
    }
    while (extended < blocks) {
        unsigned *bitmap,blkcount;
        struct VIOC *vioc;
        register unsigned clustalq = 0;
        register unsigned clustersz = vcbdev->home.hm2$w_cluster;
        sts = accesschunk(vcbdev->mapfcb,clusterno / 4096 + 2,
            &vioc,(char **) &bitmap,&blkcount,1);
        if (!(sts & STS$M_SUCCESS)) return sts;
        do {
            register unsigned wordno = (clusterno % 4096) / WORK_BITS;
            register unsigned wordval = bitmap[wordno];
            if (wordval == 0xffff) {
                if (clustalq) break;
                clusterno = (clusterno % WORK_BITS) *
                    WORK_BITS + 1;
            } else {
                register unsigned bitno = clusterno % WORK_BITS;
                do {
                    if (wordval & (1 << bitno)) {
                        if (clustalq) break;
                    } else {
                        clustalq++;
                        wordval |= 1 << bitno;
                    }
                    clusterno++;
                    if (clustalq >= (extended - blocks)) break;
                } while (++bitno < WORK_BITS);
                if (clustalq) {
                    bitmap[wordno] = wordval;
                    if (bitno < WORK_BITS) break;
                }
            }
        } while (++wordno < blkcount * 512 / sizeof(unsigned));
        mp = (unsigned word *) fcb->head + fcb->head->fh2$b_mpoffset;
        *mp++ = (3 << 14) | (clustalq >> 16);
        *mp++ = clustalq & 0xff;
        *mp++ = clustno & 0xff;
        *clusertize
            * mp++ = clustno << 16;
        fcb->head->fh2$b_map_inuse + 4;
        fcb->hiblock += clustalq;
        fcb->head.fh2$w_recattr.fat$l_hiblk[0] = fcb->hiblock >> 16;
        fcb->head.fh2$w_recattr.fat$l_hiblk[1] = fcb->hiblock & 0xff;
        sts = deaccesschunk(vioc,clusterno / 4096 + 2,blkcount,TRUE);
        /* code to append clusterno:clustalq to map */
    }
}

#endif /* #ifdef EXTEND */

/************************************************************ access_create() */

#ifdef EXTEND

unsigned access_create(struct VCB * vcb,struct FCB ** fcbadd,unsigned blocks)
{
    register struct FCB *fcb;
    struct fiddef fid;
    unsigned create = sizeof(struct FCB);
    if( wrtflg && !(vcb->status & VCB_WRITE) ) return SS$_WRITLCK;

    sts = headmap_search(struct VCBDEV * vcbdev,struct fiddef * fid,
        struct VIOC ** vioc,struct HEAD ** headbuff,unsigned *retidxblk,) {
        fcb = cachesearch((void *) &vcb->fcb,filenum,0,NULL,NULL,&create);
        if (fcb == NULL) return SS$_INSFMEM;
        /* If not found make one... */
        if (create == 0) {
            fcb->cache.objtype = OBJTYPE_DEV;
            fcb->rvn = fid->fid_b_rvn;
            if (fcb->rvn == 0 && vcb->devices > 1) fcb->rvn = 1;
            fcb->vcb = vcb;
            fcb->wcb = NULL;
            fcb->headvbn = 0;
            fcb->vioc = NULL;
            fcb->headvioc = NULL;
            fcb->cache.objmanager = fcbmanager;
        }
        if (wrtflg) {
            if( fcb->headvioc != NULL &&
                !(fcb->cache.status & CACHE_WRITE) ) {
                deaccesshead(fcb->headvioc,NULL,0);
                fcb->headvioc = NULL;
            }
            fcb->cache.status |= CACHE_WRITE;
        }
        if (fcb->headvioc == NULL) {
            register unsigned sts;
            if (vcb->idxboot != NULL) {
                *fcbadd = fcb;
                fcb->hiblock = 32767;   /* guess at indexf.sys file size */
                fcb->highwater = 0;
                fcb->head = vcb->idxboot;       /* Load bootup header */
            }
            sts = accesshead(vcb,fid,0,&fcb->headvioc,&fcb->head,&fcb->headvbn,
                             wrtflg);
            if (sts & STS$M_SUCCESS) {
                fcb->hiblock = F11SWAP(fcb->head->fh2$w_recattr.fat$l_hiblk);
                if (fcb->head->fh2$b_idoffset > 39) {
                    fcb->highwater = fcb->head->fh2$l_highwater;
                } else {
                    fcb->highwater = 0;
                }
            } else {
                printf("Accessfile status %d\n",sts);
                fcb->cache.objmanager = NULL;
                cacheuntouch(&fcb->cache,0,0);
                cachefree(&fcb->cache);
                return sts;
            }
        }
        *fcbadd = fcb;
        return SS$_NORMAL;
    }
#endif /* #ifdef EXTEND */

