/* Initialize a FILES-11 volume */

/* Timothe Litt March 2016
 * Copyright (C) 2016 Timothe litt
 *   litt at acm dot org
 *
 * Free for use with the ODS2 package.  All other rights reserved.
 */

/*
 *       This is distributed as part of ODS2, originally  written by
 *       Paul Nankervis, email address:  Paulnank@au1.ibm.com
 *
 *       ODS2 is distributed freely for all members of the
 *       VMS community to use. However all derived works
 *       must maintain comments in their source to acknowledge
 *       the contributions of the original author and
 *       subsequent contributors.   This is free software; no
 *       warranty is offered,  and while we believe it to be useful,
 *       you use it at your own risk.   This is free software; no
 *       warranty is offered,  and while we believe it to be useful,
 *       you use it at your own risk.
 */

#if !defined( DEBUG ) && defined( DEBUG_INITVOL )
#define DEBUG DEBUG_INITVOL
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include "compat.h"

#include "f11def.h"
#include "initvol.h"
#include "ods2.h"
#include "phyvirt.h"
#include "rms.h"
#include "stsdef.h"
#include "sysmsg.h"
#include "vmstime.h"

#define CLUSTERS(lbns) ( ((lbns) + clustersize - 1) / clustersize )
#define CLU2LBN(cluster) ( (cluster) * clustersize )

#define RE
#if DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

static vmscond_t add2dir( void *mfdp, uint32_t *mfdlen, uint32_t mfdblocks,
                          struct HEAD *head );

static vmscond_t add2map( struct HEAD *fhd,
                          uint32_t start, uint32_t n, int sparse );

static vmscond_t writeHEAD( void *dev, struct HEAD *head, uint32_t hdrbase ) ;
static vmscond_t writeHDR( void *dev, struct HEAD *head, uint32_t lbn );
static vmscond_t writeHOM( void *dev,
                           struct HOME *hom,
                           uint32_t homlbn, uint32_t homvbn,
                           uint32_t ncopies );
static vmscond_t alloclbns( uint8_t *bitmap,
                            uint32_t clustersize,
                            uint32_t n_clusters,
                            uint32_t startlbn, uint32_t n,
                            uint32_t *found );
static int tstlbnbit( struct NEWVOL *params, uint8_t *bitmap,
                      uint32_t lbn );
static void modlbnbits( struct NEWVOL *params, uint8_t *bitmap,
                        int set, uint32_t start, uint32_t n );
static void modbits( uint8_t *bitmap, int set, uint32_t start, uint32_t n );
static f11word checksum( f11word *data, size_t length );
#if 0
static uint32_t delta_from_name( const char *diskname );
#endif
static uint32_t compute_delta( uint32_t sectorsize,
                               uint32_t sectors,
                               uint32_t tracks,
                               uint32_t cylinders );

static void *Malloc( void ***list, size_t size );
static void *Calloc( void ***list, size_t nmemb, size_t n );
#define MALLOC(name, size) do {                 \
        if( (name = Malloc( size )) == NULL )   \
            return SS$_INSFMEM;                 \
    } while( 0 )
#define CALLOC(name, nmemb, n) do {                     \
        if( (name = Calloc( nmemb, n )) == NULL )       \
            return SS$_INSFMEM;                         \
    } while( 0 )

static uint32_t eofmark = 0;

#define WRITE(dev, lbn, n, buf) do {                            \
        vmscond_t status;                                       \
                                                                \
        if( $FAILS(status = virt_write( dev, (lbn), ((n)*512),  \
                                        (char *)(buf) )) )      \
            return status;                                      \
        if( ( (lbn) + (n) ) > eofmark )                         \
            eofmark = (lbn) + (n)  -1;                          \
    } while( 0 )

#define WRITEP(dev, pbn, n, buf) do {                           \
        vmscond_t status;                                       \
                                                                \
        if( $FAILS(status = virt_writep( dev, (pbn),            \
                                         ((n)*dp->sectorsize),  \
                                         (char *)(buf) )) )     \
            return status;                                      \
    } while( 0 )

#define READ(dev, lbn, n, buf) do {                             \
        vmscond_t status;                                       \
                                                                \
        if( $FAILS(status = virt_read( dev, (lbn), ((n)*512),   \
                                       (char *)(buf) )) )       \
            return status;                                      \
    } while( 0 )

#define READP(dev, pbn, n, buf) do {                            \
        vmscond_t status;                                       \
                                                                \
        if( $FAILS(status = virt_readp( dev, (pbn),             \
                                        ((n)*dp->sectorsize),   \
                                        (char *)(buf) )) )      \
            return status;                                      \
    } while( 0 )

static vmscond_t doinit( void ***memlist, void *dev,
                        struct NEWVOL *params );

/************************************************************ BootBlock */

static uint16_t bootcode[] = { /* EHPLI M'T ARPPDEI SNDI E ADP-P11 */
    000240,  012706,  001000,  010700,  062700,  000036, 0112001,  001403,
    004767,  000006,  000773,  000005,  000000, 0110137, 0177566, 0105737,
   0177564, 0100375,  000207,  020040,  020040,  020040,  020040,  020040,
    020040,  020040,  020040,  071551,  067040,  072157,  060440,  071440,
    071571,  062564,  020155,  064544,  065563,  005015,  000012,  000000,
#define BOOT_LABEL_OFS 046
#define BOOT_LABEL_SIZE 15
};

/************************************************************ initvol() */

vmscond_t initvol( void *dev, struct NEWVOL *params ) {
    void **memlist, **mp;
    vmscond_t status;
    memlist = (void **)calloc( 1, sizeof( void * ) );

    if( params->options & INIT_LOG ) {
        printmsg( (params->options & INIT_VIRTUAL)? INIT_START_VIRT: INIT_START_PHY, 0,
                  params->label, params->devnam, params->devtype );
    }

    status = doinit( &memlist, dev, params );

    for( mp = memlist; *mp != NULL; mp++ )
        free( *mp );
    free( memlist );

    if( $SUCCESSFUL(status) && (params->options & INIT_LOG) ) {
        printmsg( INIT_VOLINIT, 0, params->label );
    }

    return status;
}

/************************************************************ Calloc() */

static void *Calloc( void ***list, size_t nmemb, size_t n ) {
    void *mp;

    n *= nmemb;
    mp = Malloc( list, n );
    if( mp == NULL )
        return NULL;
    memset( mp, 0, n );
    return mp;
}

/************************************************************ Malloc() */

static void *Malloc( void ***memlist, size_t size ) {
    void **mp;
    size_t n;
    for( n = 0, mp = *memlist; *mp != NULL; n++, mp++ )
        ;
    mp = realloc( *memlist, (n + 2) * sizeof( void * ) );
    if( mp == NULL )
        return NULL;
    *memlist = mp;
    mp[n] = malloc( size );
    if( mp[n] == NULL )
        return NULL;
    mp[n+1] = NULL;
    return mp[n];
}
#define Malloc( size ) Malloc( memlist, size )
#define Calloc( nmemb, size ) Calloc( memlist, nmemb, size )

/************************************************************ doinit() */

static vmscond_t doinit( void ***memlist, void *dev, struct NEWVOL *params ) {
    vmscond_t status;
    struct disktype *dp = NULL;
    struct HOME *hom = NULL;
    struct SCB *scb = NULL;
    struct HEAD
        *idxhead = NULL,
        *badhead = NULL,
        *corhead = NULL,
        *volhead = NULL,
        *conthead = NULL,
        *bakhead = NULL,
        *loghead = NULL,
        *bmphead = NULL,
        *mfdhead = NULL;
    struct IDENT  *ident = NULL;
    struct BAD144 *mbad = NULL;
    struct SWBAD  *sbad = NULL;
    uint32_t tmp, volstart, clustersize;
    uint32_t pbs, pb_per_lb, lb_per_pb, npb, nlb, n_clusters;
    uint32_t homlbn, homvbn, hm2lbn, hm2cluster, fhlbn;
    uint32_t mfdlbn, bmplbn, idxlbn, aidxlbn;
    const char *p = NULL;
    uint8_t *bitmap = NULL, *indexmap = NULL;
    uint32_t bitmap_size, indexmap_size;
    uint16_t *bootblock = NULL;
    time_t serial;
    char tbuf[12+1];
    char *mfdp = NULL;
    uint32_t mfdlen = 0, mfdblocks = 3, mfdalloc;

    if( params->username && (!params->username[0] ||
                             (tmp = (uint32_t)strlen( params->username )) > 12 ||
                             strspn( params->username, VOL_ALPHABET ) != tmp) ) {
        return printmsg( INIT_BADUSERNAME, 0 );
    }

    for( dp = disktype; dp->name != NULL; dp++ ) {
        if( !strcmp( params->devtype, dp->name ) )
            break;

    }
    if( dp->name == NULL ) {
        return printmsg( INIT_BADMEDIUM, 0, params->devtype );
    }

    pbs = dp->sectorsize;
    pb_per_lb = (pbs < 512)? 512/pbs: 1;
    lb_per_pb = (pbs >512)? pbs/512: 1;

    npb = (dp->sectors * dp->tracks * dp->cylinders ) - dp->reserved;
#ifdef UINT64_C
    nlb = (uint32_t) ((((uint64_t)npb) * lb_per_pb)
                            / pb_per_lb);
#else
    nlb = (uint32_t) ((((double)npb) * lb_per_pb)
                            / pb_per_lb);
#endif
    if( params->clustersize == 0 ) {
        if( nlb < 50000 )
            params->clustersize = 1;
        else {
            uint32_t tmp;
#ifdef UINT64_C
            tmp = (uint32_t)((((uint64_t)nlb) + (UINT64_C(255) * 4096) - 1) / (UINT64_C(255) * 4096));
#else
            tmp = (uint32_t)((((double)nlb) + (255 * 4096) - 1) / (255 * 4096));
#endif
            params->clustersize = (tmp > 3? tmp: 3);
        }
    }
    tmp = nlb / 50;
    if( tmp > 16382 )
        tmp = 16382;
    if( params->clustersize > tmp )
        params->clustersize = tmp;

    clustersize = params->clustersize;

    if( npb < 100 || nlb < 100 || clustersize < 1 ) {
        return printmsg( INIT_TOOSMALL, 0 );
    }

    n_clusters = CLUSTERS( nlb );

    if( params->maxfiles < 16 ) {
        params->maxfiles = nlb / ((clustersize +1) * 2);
        if( params->maxfiles < 16 )
            params->maxfiles = 16;
    }
    tmp = nlb / (clustersize + 1 );
    if( params->maxfiles > tmp )
        params->maxfiles = tmp;

    if( params->maxfiles > ((1 << 24) - 1) )
        params->maxfiles = ((1 << 24) - 1);

    if( params->headers < 16 )
        params->headers = 16;
    if( params->headers > 65500 )
        params->headers = 65500;

    if( params->headers > params->maxfiles )
        params->headers = params->maxfiles;

    if( params->directories > 16000 )
        params->directories = 16000;
    if( params->directories > params->maxfiles )
        params->directories = params->maxfiles;
    if( params->directories < 16 )
        params->directories = 16;

    mfdalloc = params->directories / 16;
    mfdalloc = CLU2LBN( CLUSTERS( mfdalloc ) );
    if( mfdalloc < mfdblocks )
        mfdalloc = mfdblocks;

    if( params->windows < 7 )
        params->windows = 7;
    else if( params->windows > 80 )
        params->windows = 80;

    if( params->extension > 65535 )
        params->extension = 65535;

    (void) time( &serial );

    /* Create the bootblock.  (This is PDP-11 code. )
     */

    CALLOC( bootblock, 256, sizeof( uint16_t ) );
    memcpy( bootblock, bootcode, sizeof( bootcode ) );

    for( p = params->label, tmp = 0; *p && tmp < BOOT_LABEL_SIZE; tmp++, p++ )
        ((char *)bootblock)[BOOT_LABEL_OFS+tmp] = toupper(*p);
#if ODS2_BIG_ENDIAN
    for( tmp = 0; tmp < 256; tmp++ )
        bootblock[tmp] = F11WORD(bootblock[tmp]);
#endif

#define ALLHEAD(name)                           \
    CALLOC( name, 1, sizeof(struct HEAD) )

    ALLHEAD( idxhead );
    ALLHEAD( badhead );
    ALLHEAD( corhead );
    ALLHEAD( volhead );
    ALLHEAD( conthead );
    ALLHEAD( bakhead );
    ALLHEAD( loghead );
    ALLHEAD( bmphead );
    ALLHEAD( mfdhead );

    CALLOC(hom, 1, sizeof( struct HOME ) );

    hom->hm2$w_struclev = F11WORD( STRUCLEV );
    hom->hm2$w_cluster = F11WORD( clustersize );
    hom->hm2$w_resfiles = F11WORD( FID$C_MAXRES );
    hom->hm2$l_maxfiles = F11LONG( params->maxfiles );

    if( params->options & INIT_OWNUIC ) {
        hom->hm2$w_volowner.uic$w_grp = F11WORD((uint16_t)
                                                (params->useruic >> 16));
        hom->hm2$w_volowner.uic$w_mem = F11WORD((uint16_t)params->useruic);
    } else {
        hom->hm2$w_volowner.uic$w_grp = F11WORD(1u);
        hom->hm2$w_volowner.uic$w_mem = F11WORD(1u);
    }

    /* Protection values: Set default, then replace any field specifed per mask */
    hom->hm2$w_protect = (f11word)~ /* N.B. Permissions are inverted here: noxx => xx allowed */
        SETPROT(prot$m_none,prot$m_none,prot$m_none,prot$m_none); /* S:RWED,O:RWED,G:RWED,W:RWED */
    hom->hm2$w_protect = ((hom->hm2$w_protect & ~(params->volprot >> 16)) |
                          (uint16_t)(params->volprot));
    hom->hm2$w_protect = F11WORD(hom->hm2$w_protect);

    hom->hm2$w_fileprot = (f11word)~ /* S:RWED,O:RWED,G:RE,W: */
        SETPROT(prot$m_noread|prot$m_nowrite|prot$m_noexe|prot$m_nodel,
                prot$m_noread|prot$m_nowrite|prot$m_noexe|prot$m_nodel,
                prot$m_noread|prot$m_noexe,
                prot$m_norestrict);
    hom->hm2$w_fileprot = ((hom->hm2$w_fileprot & ~(params->fileprot >> 16)) |
                          (uint16_t)(params->fileprot));
    hom->hm2$w_fileprot = F11WORD(hom->hm2$w_fileprot);

    hom->hm2$b_window = (uint8_t)params->windows;
    hom->hm2$b_lru_lim = 16;
    hom->hm2$w_extend = (f11word)F11WORD( params->extension );

    CALLOC( scb, 1, 512 );
    scb->scb$w_cluster = F11WORD(clustersize);
    scb->scb$w_struclev = F11WORD(STRUCLEV);
    scb->scb$l_volsize = F11LONG(nlb);
    scb->scb$l_blksize = F11LONG(pb_per_lb);
    scb->scb$l_sectors = F11LONG(dp->sectors);
    scb->scb$l_tracks = F11LONG(dp->tracks);
    scb->scb$l_cylinders = F11LONG(dp->cylinders);
    scb->scb$l_status = F11LONG( 0 );
    scb->scb$l_status2 = F11LONG( 0 );
    scb->scb$w_writecnt = 0;

    /* If necessary, scb$t_volockname will be initialized by mount under
     * an OS.
     */

    if( $FAILS(status = sys_gettim( scb->scb$q_mounttime )) ) {
        return status;
    }
    scb->scb$w_checksum = F11WORD(checksum((f11word *)scb,
                                           offsetof( struct SCB,
                                                     scb$w_checksum ) / 2  ) );

    bitmap_size = (n_clusters + 4095) / 4096;

    indexmap_size = (params->maxfiles + 4095) / 4096;
    hom->hm2$l_maxfiles = F11LONG( params->maxfiles );

    memcpy( hom->hm2$q_credate, scb->scb$q_mounttime,
            sizeof(hom->hm2$q_credate) );
    memcpy( hom->hm2$q_revdate, scb->scb$q_mounttime,
            sizeof(hom->hm2$q_revdate) );

    DPRINTF(("Total LBNs: %u cluster size: %u total clusters: %u, "
             "bitmap size: %u blocks maxfiles %u index map size %u blocks\n",
             nlb, clustersize, n_clusters, bitmap_size, params->maxfiles,
             indexmap_size));

    hom->hm2$l_serialnum = (f11long) F11LONG( serial );
    memset( hom->hm2$t_strucname, ' ', sizeof( hom->hm2$t_strucname ) );
    (void) snprintf( tbuf, sizeof( tbuf), "%-12.12s", params->label );
    for( tmp = 0; tmp < sizeof( tbuf ); tmp++ )
        tbuf[tmp] = toupper( tbuf[tmp] );
    memcpy( hom->hm2$t_volname, tbuf, sizeof( hom->hm2$t_volname ) );
    p = get_username();
    (void) snprintf( tbuf, sizeof( tbuf), "%-12.12s",
                     (params->username? params->username: (p == NULL)? "": p) );
    free( (void *)p );
    for( tmp = 0; tmp < sizeof( tbuf ); tmp++ )
        tbuf[tmp] = toupper( tbuf[tmp] );
    memcpy( hom->hm2$t_ownername, tbuf, sizeof( hom->hm2$t_ownername ) );
    (void) snprintf( tbuf, sizeof( tbuf ), "%-12.12s", "DECFILE11B" );
    memcpy( hom->hm2$t_format, tbuf, sizeof( hom->hm2$t_format ) );

    CALLOC( indexmap, 1, (size_t)indexmap_size * 512 );

    hom->hm2$w_ibmapsize = F11WORD( (f11word)indexmap_size );

    /* Index header will be template for all other files.
     * Index-specific settings must come after copy.
     */

    idxhead->fh2$b_idoffset = FH2$C_LENGTH / 2;
    idxhead->fh2$b_acoffset = offsetof( struct HEAD, fh2$w_checksum ) / 2;
    idxhead->fh2$b_rsoffset = offsetof( struct HEAD, fh2$w_checksum ) / 2;

    idxhead->fh2$w_seg_num = F11WORD( 0 );
    idxhead->fh2$w_struclev = F11WORD( STRUCLEV );

    idxhead->fh2$w_fid.fid$b_rvn = 0;
    idxhead->fh2$w_fid.fid$b_nmx = 0;
    idxhead->fh2$w_ext_fid.fid$w_num = F11WORD( 0 );
    idxhead->fh2$w_ext_fid.fid$w_seq = F11WORD( 0 );
    idxhead->fh2$w_ext_fid.fid$b_rvn = 0;
    idxhead->fh2$w_ext_fid.fid$b_nmx = 0;

    idxhead->fh2$w_recattr.fat$l_hiblk = F11SWAP( 0 );
    idxhead->fh2$w_recattr.fat$l_efblk = F11SWAP( 1 );
    idxhead->fh2$w_recattr.fat$w_ffbyte = F11WORD( 0 );
    idxhead->fh2$w_recattr.fat$b_bktsize = 0;
    idxhead->fh2$w_recattr.fat$b_vfcsize = 0;
    idxhead->fh2$w_recattr.fat$w_defext = F11WORD( 0 );
    idxhead->fh2$w_recattr.fat$w_gbc = F11WORD( 0 );
    idxhead->fh2$w_recattr.fat$w_versions = F11WORD( 0 );
    idxhead->fh2$l_filechar = F11LONG( 0 );
    idxhead->fh2$b_map_inuse = 0;
    idxhead->fh2$b_acc_mode = 0;
    idxhead->fh2$l_fileowner.uic$w_mem = F11WORD( 1 );
    idxhead->fh2$l_fileowner.uic$w_grp = F11WORD( 1 );
    idxhead->fh2$w_fileprot = (f11word)~
        SETPROT(prot$m_noread|prot$m_nowrite|prot$m_noexe|prot$m_nodel,
                prot$m_noread|prot$m_nowrite|prot$m_noexe|prot$m_nodel,
                prot$m_noread|prot$m_noexe,
                prot$m_norestrict);
    idxhead->fh2$w_fileprot = F11WORD( idxhead->fh2$w_fileprot );

    idxhead->fh2$w_backlink.fid$w_num = F11WORD( FID$C_MFD );
    idxhead->fh2$w_backlink.fid$w_seq = F11WORD( FID$C_MFD );
    idxhead->fh2$w_backlink.fid$b_rvn = 0;
    idxhead->fh2$w_backlink.fid$b_nmx = 0;
    idxhead->fh2$l_highwater = F11LONG( 1 );

#define IDENTp(head) ((struct IDENT *) (((f11word *)head) +     \
                                        head->fh2$b_idoffset))
    ident = IDENTp(idxhead);

    ident->fi2$w_revision = F11WORD( 1 );
    memcpy( ident->fi2$q_credate, scb->scb$q_mounttime,
            sizeof(ident->fi2$q_credate) );
    memcpy( ident->fi2$q_revdate, scb->scb$q_mounttime,
            sizeof(ident->fi2$q_credate) );
    /* These files can never be renamed or deleted.  But maximum map area
     * is helpful and all names are short. So omit filenamext.
     */
    idxhead->fh2$b_mpoffset = idxhead->fh2$b_idoffset +
                                      (offsetof( struct IDENT,
                                                 fi2$t_filenamext ) / 2);

    memcpy( badhead,  idxhead,  sizeof( *badhead ) );
    memcpy( corhead,  idxhead,  sizeof( *corhead ) );
    memcpy( volhead,  idxhead,  sizeof( *volhead ) );
    memcpy( conthead, idxhead, sizeof( *conthead ) );
    memcpy( bakhead,  idxhead,  sizeof( *bakhead ) );
    memcpy( loghead,  idxhead,  sizeof( *loghead ) );
    memcpy( bmphead,  idxhead,  sizeof( *bmphead ) );
    memcpy( mfdhead,  idxhead,  sizeof( *mfdhead ) );

    mfdhead->fh2$l_filechar = F11LONG( FH2$M_DIRECTORY );
    mfdhead->fh2$w_fileprot &= ~F11WORD( /* W:E allowed for MFD */
                                        SETPROT(0,
                                                0,
                                                0,
                                                prot$m_noexe) );

    mfdblocks = CLU2LBN( CLUSTERS( mfdblocks ) );
    CALLOC( mfdp, 1, (size_t)mfdblocks * 512 );

#define SETFILE(head, num, name, rtype, rattr, rsize )  do {    \
        vmscond_t status;                                       \
        struct IDENT *id;                                       \
        char fname[sizeof( id->fi2$t_filename )+1];             \
                                                                \
        id = IDENTp(head);                                      \
        (void) snprintf( fname, sizeof( fname ),                \
                         "%-20.20s", #name );                   \
        memcpy( id->fi2$t_filename, fname,                      \
                sizeof( id->fi2$t_filename ) );                 \
        head->fh2$w_fid.fid$w_num = F11WORD( FID$C_ ## num );   \
        head->fh2$w_fid.fid$w_seq = F11WORD( FID$C_ ## num );   \
        head->fh2$w_recattr.fat$b_rtype = ((FAB$C_SEQ << 4) |   \
                                           (FAB$C_ ## rtype));  \
        head->fh2$w_recattr.fat$b_rattrib = rattr;              \
        head->fh2$w_recattr.fat$w_rsize = F11WORD( rsize );     \
        head->fh2$w_recattr.fat$w_maxrec = F11WORD( rsize );    \
        if( $FAILS(status = add2dir( mfdp, &mfdlen,             \
                                     mfdblocks, head )) )       \
            return status;                                      \
        modbits( indexmap, 1, FID$C_ ## num, 1 );               \
    } while( 0 )

    /* Must be in alphabetical order by filename for MFD */
    SETFILE( mfdhead,  MFD,    000000.DIR;1,  VAR, FAB$M_BLK, 512 );
    SETFILE( bakhead,  BACKUP, BACKUP.SYS;1,  FIX, 0,          64 );
    SETFILE( badhead,  BADBLK, BADBLK.SYS;1,  FIX, 0,         512 );
    SETFILE( loghead,  BADLOG, BADLOG.SYS;1,  FIX, 0,          16 );
    SETFILE( bmphead,  BITMAP, BITMAP.SYS;1,  FIX, 0,         512 );
    SETFILE( conthead, CONTIN, CONTIN.SYS;1,  FIX, 0,         512 );
    SETFILE( corhead,  CORIMG, CORIMG.SYS;1,  FIX, 0,         512 );
    SETFILE( idxhead,  INDEXF, INDEXF.SYS;1,  FIX, 0,         512 );
    SETFILE( volhead,  VOLSET, VOLSET.SYS;1,  FIX, 0,          64 );
#if 0 /* Reserved for free space, but not defined */
    SETFILE( NULL,     FREFIL, FREFIL.SYS;1,  UDF, 0,           0 );
#endif

    MALLOC( bitmap, 512 * (size_t)bitmap_size );
    memset( bitmap, 0xFF, 512 * (size_t)bitmap_size );

    DPRINTF(("Non-existent bitmap bits %u / %u LBNs %u - %u\n",
             (bitmap_size *4096) - n_clusters, bitmap_size *4096,
             n_clusters * clustersize, bitmap_size * 4096 * clustersize));
    modbits( bitmap, 0, n_clusters, (bitmap_size * 4096) - n_clusters );

#define ADDMAPENTRY(head, start, n ) do {                               \
        vmscond_t status;                                               \
                                                                        \
        if( $FAILS(status = add2map( (head),                            \
                                     CLU2LBN( (start) / clustersize ),  \
                                     CLU2LBN( CLUSTERS( (n) ) ), 0)) )  \
            return status;                                              \
    } while( 0 )

    if( dp->flags & DISK_BAD144 ) {
        uint32_t start, count;

        tmp = 10 * dp->sectorsize;

        MALLOC( mbad, tmp );
        if( params->options &  INIT_VIRTUAL ) {
            memset( mbad->bbd$l_badblock, ~0,
                    dp->sectorsize - offsetof( struct BAD144, bbd$l_badblock) );
            mbad->bbd$l_serial = F11LONG( ((uint32_t) serial) & ~0x80000000 );
            mbad->bbd$w_reserved = 0;
            mbad->bbd$w_flags = 0;
            for( count = 1; count < 10; count++ )
                memcpy( ((char *)mbad)+(count * (size_t)dp->sectorsize),
                        mbad, dp->sectorsize );
        } else {
            ;/* Should read & mark bad */
        }

        /* Might have to deal with finding clusters more accurately
         * for disks with interleave/skew.
         */
        start = npb - 10;
        start = (uint32_t) ( ( ((off_t)tmp) * pb_per_lb ) / lb_per_pb );
        count = (uint32_t) ( ( ((off_t)10) * lb_per_pb ) / pb_per_lb );

        modlbnbits( params, bitmap, 0, start, count );
        ADDMAPENTRY( badhead, start, count );
        if( !(params->options &  INIT_VIRTUAL) ) {
            READP( dev, start, 10, mbad );
            /* *** TODO : Decode and add to bitmap/badblk.sys */
        }
    }
    if( dp->flags & DISK_BADSW ) {
        CALLOC( sbad, 1, sizeof( struct SWBAD ) );
        if( params->options &  INIT_VIRTUAL ) {
            sbad->bbm$b_countsize = 1;
            sbad->bbm$b_lbnsize = 3;
            sbad->bbm$b_inuse = 0;
            sbad->bbm$b_avail = (uint8_t)( (offsetof( struct SWBAD,
                                                       bbm$w_checksum ) -
                                             offsetof( struct SWBAD,
                                                       bbm$r_map )) /
                                           sizeof( uint8_t ) );
        }
        modlbnbits( params, bitmap, 0, nlb - 1, 1 );
        ADDMAPENTRY( badhead, nlb-1, 1 );
        if( !(params->options &  INIT_VIRTUAL) ) {
            READ( dev, nlb-1, 1, sbad );
            /* *** TODO : Decode and add to bitmap/badblk.sys */
        }
    }

    /* Make index file map entries (in increasing VBN order)
     */

    /* BOOT and HOM blocks.
     *
     * HOM1 and boot merge if clustersize >1 & primary HOM is good.
     */

    modlbnbits( params, bitmap, 0, params->bootlbn, 1 );

    homlbn = 1;
    tmp = delta_from_index( dp - disktype );

    do {
        if( tstlbnbit( params, bitmap, homlbn ) ||
            ( (homlbn != params->bootlbn ) &&
              (homlbn / clustersize == params->bootlbn / clustersize)) )
            break;

        /* Block is allocated, so it must already be in bitmap/badblk */

        homlbn += tmp;
    } while( homlbn < nlb );

    if( homlbn >= nlb ) {
        return printmsg( INIT_NOHOMLOC, 0 );
    }

    DPRINTF(("Boot block = %u, primary HOM block = %u\n", params->bootlbn, homlbn));

    if((homlbn == params->bootlbn + 1) && (params->bootlbn % clustersize == 0) &&
       params->bootlbn / clustersize == homlbn / clustersize ) {
        if( clustersize > 1 ) {
            modlbnbits( params, bitmap, 0, params->bootlbn, clustersize * 2 );
            ADDMAPENTRY( idxhead, params->bootlbn, clustersize * 2 );
        } else {
            modlbnbits( params, bitmap, 0, params->bootlbn, clustersize  );
            ADDMAPENTRY( idxhead, params->bootlbn, clustersize );
        }
    } else { /* This odd case is the exception to the rule that VBNS and
              * map pointers are always cluster aligned.
              */
        uint32_t offset;

        offset = homlbn % clustersize;
        if( offset != 0 ) {
            (void) add2map( badhead, homlbn, offset, 0 );
        }

        (void) add2map( idxhead, params->bootlbn, 1, 0 );
        if( clustersize > 1 ) {
            modlbnbits( params, bitmap, 0, homlbn, clustersize * 2 );
            (void) add2map( idxhead, homlbn, clustersize * 2 - offset, 0 );
        } else {
            modlbnbits( params, bitmap, 0, homlbn, clustersize );
            (void) add2map( idxhead, homlbn, clustersize - offset, 0 );
        }
    }

    /* Alternate HOM block
     */

    hm2lbn = homlbn;
    do {
        hm2lbn += tmp;

        if( tstlbnbit( params, bitmap, hm2lbn ) )
            break;
        /* Bad or allocated block */
    } while( hm2lbn < nlb );

    if( hm2lbn >= nlb ) {
        return printmsg( INIT_NOHOM2LOC, 0 );
    }

    modlbnbits( params, bitmap, 0, hm2lbn, 1 );

    hm2cluster = hm2lbn / clustersize;

    DPRINTF(("Alternate HOM block is %u\n", hm2lbn));

    ADDMAPENTRY( idxhead, CLU2LBN( hm2cluster ), 1 );

    hom->hm2$l_alhomelbn = F11LONG( hm2lbn );

    if( params->options & INIT_LOG ) {
        printmsg( INIT_SIZING1, 0,
                  nlb, clustersize, params->maxfiles );
        printmsg( INIT_SIZING2, MSG_CONTINUE, INIT_SIZING1,
                  params->headers, params->directories );
        printmsg( INIT_PLACED1, MSG_CONTINUE, INIT_SIZING1,
                  params->bootlbn, homlbn, hm2lbn );
    }

    /* Variable placement. */

    if( params->indexlbn == INIT_INDEX_MIDDLE )
        volstart = nlb / 2;
    else
        volstart = params->indexlbn;

#define ALLOCLBNS( start, number, result ) do {                         \
        vmscond_t status;                                               \
        if( $FAILS(status = alloclbns( bitmap, clustersize, n_clusters, \
                                       (start), (number), (result))) )  \
            return status;                                              \
    } while( 0 )

    /* This order is recommended for best placement */

    /* MFD */
    ALLOCLBNS( volstart, mfdalloc, &mfdlbn );

    /* SCB and storage bitmap */
    ALLOCLBNS( volstart, 1 + bitmap_size, &bmplbn );

    /* Index bitmap and initial file headers. */
    ALLOCLBNS( volstart, indexmap_size + params->headers, &idxlbn );

    /* Backup index header -- put it near the 2nd HOM block */
    ALLOCLBNS( hm2lbn, 1, &aidxlbn );

    hom->hm2$l_altidxlbn = F11LONG( aidxlbn );
    hom->hm2$w_altidxvbn = F11WORD( (clustersize * 3) +1 );

    DPRINTF(("Backup index header %u\n", aidxlbn));
    ADDMAPENTRY( idxhead, aidxlbn, 1 );

    fhlbn = idxlbn + indexmap_size;
    hom->hm2$l_ibmaplbn = F11LONG( idxlbn );

    DPRINTF(("Index bitmap LBN is %u, first extent %u\n",
             idxlbn, indexmap_size+params->headers));

    /* EOF is 1 block past the 9 used headers */

    tmp = (clustersize * 4) + indexmap_size + 9 +1;
    idxhead->fh2$w_recattr.fat$l_efblk = F11SWAP( tmp );
    idxhead->fh2$w_recattr.fat$w_ffbyte = F11WORD( 0 );
    idxhead->fh2$l_highwater = F11LONG( tmp );

    ADDMAPENTRY( idxhead, idxlbn, indexmap_size + params->headers );

    hom->hm2$w_ibmapvbn = F11WORD( (clustersize * 4) + 1 );

    /* Accurate EOF is not kept for directories, reclen = 32767 is
     * end marker in data.
     */
#if 0
    mfdhead->fh2$w_recattr.fat$l_efblk = F11SWAP( 1 + (mfdlen / 512) );
    mfdhead->fh2$w_recattr.fat$w_ffbyte = F11WORD( mfdlen % 512 );
#else
    mfdhead->fh2$w_recattr.fat$l_efblk = F11SWAP( 1 + ((mfdlen + 511) / 512) );
    mfdhead->fh2$w_recattr.fat$w_ffbyte = F11WORD( 0 );
#endif
    mfdhead->fh2$l_highwater = F11LONG( 1 + ((mfdlen + 511) / 512) );

    ADDMAPENTRY( mfdhead, mfdlbn, mfdalloc );

    /* Storage bitmap */

    bmphead->fh2$w_recattr.fat$l_efblk = F11SWAP( 1 + bitmap_size + 1);
    bmphead->fh2$w_recattr.fat$w_ffbyte = F11WORD( 0 );
    bmphead->fh2$l_highwater = F11LONG( 1 + bitmap_size + 1 );

    ADDMAPENTRY( bmphead, bmplbn, 1 + bitmap_size );

    if( params->options & INIT_LOG ) {
        printmsg( INIT_PLACED2, MSG_CONTINUE, INIT_SIZING1,
                  mfdlbn, idxlbn,
                  idxlbn + indexmap_size + params->headers -1 );
    }

    /* Let's try for a spiral write... */

    WRITE( dev, params->bootlbn, 1, bootblock );

    /* Write HOM blocks */

    homvbn = 2;
    hom->hm2$w_homevbn = F11WORD( homvbn );

    DPRINTF(("Primary HOM block lbn %u vbn %u\n", homlbn, homvbn ));

    homvbn = (clustersize * 2) + 1;
    hom->hm2$w_alhomevbn = F11WORD( homvbn );

    if( clustersize > 1 )                            /* Copies needed */
        tmp = ( clustersize * 2 ) - ( homlbn % clustersize );
    else
        tmp = 1;

    if( $FAILS(status = writeHOM( dev, hom, homlbn, 2, tmp )) )
        return status;

    DPRINTF(("Secondary HOM block lbn %u vbn %u\n", hm2lbn, homvbn));

    if( $FAILS(status = writeHOM( dev, hom, hm2cluster * clustersize,
                                  homvbn, clustersize )) )
        return status;

    /* MFD */

    DPRINTF(("MFD at %u, %u blocks, len %u\n", mfdlbn, mfdalloc, mfdlen));

    WRITE( dev, mfdlbn, mfdblocks, mfdp );

    DPRINTF(("Writing BITMAP.SYS at %u, %u blocks\n", bmplbn,
             1 + bitmap_size));

    WRITE( dev, bmplbn, 1, scb );
    WRITE( dev, bmplbn + 1, bitmap_size, bitmap );

    DPRINTF(("Index file bitmap\n"));
    WRITE( dev, idxlbn, indexmap_size, indexmap );

#define WRITEHEAD(head, name) do {                                      \
        vmscond_t status;                                               \
        head->fh2$w_checksum = checksum( (f11word *)(head),             \
                                         offsetof( struct HEAD,         \
                                                   fh2$w_checksum ) / 2 ); \
        DPRINTF(("Writing " #name " header\n"));                        \
        if( $FAILS(status = writeHEAD( dev, (head), fhlbn )) )          \
            return status;                                              \
    } while( 0 )

    WRITEHEAD( idxhead,  INDEXF.SYS;1 );
    WRITEHEAD( bmphead,  BITMAP.SYS;1 );
    WRITEHEAD( badhead,  BADBLK.SYS;1 );
    WRITEHEAD( mfdhead,  000000.DIR;1 );
    WRITEHEAD( corhead,  CORIMG.SYS;1 );
    WRITEHEAD( volhead,  VOLSET.SYS;1 );
    WRITEHEAD( conthead, CONTIN.SYS;1 );
    WRITEHEAD( bakhead,  BACKUP.SYS;1 );
    WRITEHEAD( loghead,  BADLOG.SYS;1 );

    if( $FAILS(status = writeHDR( dev, idxhead, aidxlbn )) )
        return status;

    if( params->options &  INIT_VIRTUAL ) {
        if( dp->flags & DISK_BAD144 ) {
            if( params->options & INIT_LOG ) {
                printmsg( INIT_MFGBAD, 0 );
            }
            WRITEP( dev, npb - 10, 10, mbad );
        }
        if( dp->flags & DISK_BADSW ) {
            if( params->options & INIT_LOG ) {
                printmsg( INIT_SWBAD, 0 );
            }
            sbad->bbm$w_checksum = checksum( (f11word *)sbad,
                                             offsetof( struct SWBAD,
                                                       bbm$w_checksum ) / 2 );
            WRITE( dev, nlb - 1, 1, sbad );
        }

        /*  Access last block so full size of file is allocated. (Hopefully
         * sparse.) To account for possible interleaving, do the last track.
         * Helps humans in various ways.
         */

        if( eofmark < nlb ) {
            for( tmp = nlb - ((dp->sectors * lb_per_pb) / pb_per_lb);
                 tmp < nlb; tmp++ ) {
                READ( dev, tmp, 1, idxhead );
                WRITE( dev, tmp, 1, idxhead );
            }
        }
    }

    return status;
}
#undef Malloc

/************************************************************ add2dir() */

static vmscond_t add2dir( void *dirp, uint32_t *dirlen, uint32_t dirblocks,
                         struct HEAD *head ) {
    struct dir$r_ent *de;
    struct dir$r_rec *dr;
    struct IDENT *ident;
    uint32_t block, offset;
    f11word reclen, paddednamelen, version, verlimit;
    size_t namelen, typlen;
    char fname[  sizeof( ident->fi2$t_filename ) +
                 sizeof( ident->fi2$t_filenamext ) + 1], *p;
    int extlen;

    ident = IDENTp(head);

    memcpy( fname, ident->fi2$t_filename, sizeof( ident->fi2$t_filename ) );

    extlen = (((size_t)head->fh2$b_mpoffset - head->fh2$b_idoffset) * sizeof( f11word )) -
              offsetof( struct IDENT, fi2$t_filenamext );
    if( extlen > 0 ) {
        if( extlen > (int) sizeof( ident->fi2$t_filenamext ) )
            extlen = sizeof( ident->fi2$t_filenamext );
        memcpy( fname + sizeof( ident->fi2$t_filename ),
                ident->fi2$t_filenamext, extlen );
    } else extlen = 0;

    namelen = sizeof( ident->fi2$t_filename ) + extlen;
    fname[namelen] = '\0';
    while( namelen > 1 && fname[namelen - 1] == ' ' )
        fname[--namelen] = '\0';

    if( (p = strrchr( fname, ';' )) != NULL ) {
        typlen = (size_t) ( fname + namelen - p );
        namelen = (size_t)(p -fname );
        *p++ = '\0';
        version = (f11word)strtoul( p, &p, 10 );
        if( p != fname + namelen + typlen || typlen < 1 )
            return SS$_BADFILENAME;
    } else
        version = 1;

    block = *dirlen / 512;
    offset = *dirlen % 512;

    verlimit = (head->fh2$l_filechar & F11LONG(FH2$M_DIRECTORY))? 1: 0;

    paddednamelen = (f11word) ( ((namelen + 1) / 2) * 2 );

    reclen = ( offsetof( struct dir$r_rec, dir$t_name ) + paddednamelen +
               sizeof( struct dir$r_ent ) );
    if( reclen + offset > 512 ) {
        block++;
        if( block >= dirblocks )
            return SS$_DEVICEFULL;
        if( offset <= 512 - sizeof(f11word) ) {
            ((f11word *)dirp)[offset / sizeof(f11word)] = F11WORD( -1 );
            offset += sizeof( f11word );
        }
        *dirlen += 512 - offset;
        offset = 0;
    }
    dr = (struct dir$r_rec *)( ((char *)dirp) + *dirlen );
    dr->dir$w_size = (f11word) F11WORD( reclen - sizeof( dr->dir$w_size ) );
    dr->dir$w_verlimit = F11WORD( verlimit );
    dr->dir$b_flags = 0; /* dir$c_fid */
    dr->dir$b_namecount = (uint8_t) namelen;
    memcpy( dr->dir$t_name, fname, paddednamelen );
    de = (struct dir$r_ent *) ( ((char *)dirp) + ( (size_t)block * 512 ) + offset +
                                reclen - sizeof( struct dir$r_ent ));
    de->dir$w_version = version;
    de->dir$w_fid = head->fh2$w_fid;
    offset += reclen;
    if( offset <= 512 - sizeof(f11word) )
        ((f11word *)dirp)[offset/sizeof(f11word)] = F11WORD( -1 );
    *dirlen += reclen;

    return SS$_NORMAL;
}

/************************************************************ add2map() */

static vmscond_t add2map( struct HEAD *fhd,
                         uint32_t start, uint32_t n, int sparse ) {

    return addmappointers( fhd, &start, &n, sparse );
}

/************************************************************ addmappointers() */

vmscond_t addmappointers( struct HEAD *fhd,
                         uint32_t *start, uint32_t *n, int sparse ) {
    f11word *mp, *ep;
    uint8_t mapsize, inuse, len;
    f11long hiblk;
    uint32_t pbn = 0, count = 0, msparse = 0, nextpbn = 0;
    int contig = -1;

    if( sparse )
        *start = ~0U;

    hiblk = F11SWAP( fhd->fh2$w_recattr.fat$l_hiblk );

    mapsize = fhd->fh2$b_acoffset - fhd->fh2$b_mpoffset;
    inuse = fhd->fh2$b_map_inuse;

    mp = ((f11word *)fhd) + fhd->fh2$b_mpoffset;
    ep = mp + inuse;

    len = 0;

    while( mp < ep ) {
        f11word e0;

        e0 = *mp++;
        e0 = F11WORD(e0);
        switch( e0 &  FM2$M_FORMAT3 ) {
        case  FM2$M_FORMAT1:
            len = 2;
            count = e0 & 0xff;
            pbn = F11WORD(*mp) | (((e0 & ~FM2$M_FORMAT3) >> 8) << 16);
            msparse = (pbn == FM2$C_FMT1_MAXLBN);
            mp++;
            break;
        case  FM2$M_FORMAT2:
            len = 3;
            count = e0 & ~FM2$M_FORMAT3;
            pbn = *mp++;
            pbn = F11WORD( pbn );
            pbn |= F11WORD (*mp ) << 16;
            msparse = (pbn == FM2$C_FMT2_MAXLBN);
            mp++;
            break;
        case FM2$M_FORMAT3:
            len = 4;
            count = ((e0 & ~FM2$M_FORMAT3) << 16);
            count |= F11WORD( *mp );
            mp++;
            pbn = *mp++;
            pbn = F11WORD( pbn );
            pbn |= F11WORD (*mp ) << 16;
            msparse = (pbn == FM2$C_FMT3_MAXLBN);
            mp++;
            break;
        default:
            len = 1;
            continue;
        }
        ++count;
        if( contig < 0 && !msparse ) {
            contig = 1;
            nextpbn = pbn + count;
        } else if( msparse || pbn != nextpbn )
            contig = 0;
        else
            nextpbn = pbn + count;
    }

    if( sparse || (contig == 1 && *n && nextpbn != *start) )
        contig = 0;
    if( contig )
        fhd->fh2$l_filechar |= F11LONG(FH2$M_CONTIG);
    else
        fhd->fh2$l_filechar &= ~F11LONG(FH2$M_CONTIG);

    if( len >= 2 && ((sparse && msparse) ||
                     (!sparse && *start == pbn + count) ) ) {
        mp -= len;
        inuse -= len;
        hiblk -= count;
        *n += count;
        *start = pbn;
    }

    while( *n > 0 ) {
        f11word freew =  mapsize - inuse;

        count = *n;
        if( *start < FM2$C_FMT1_MAXLBN || sparse ) {
            if( freew < 2 )
                break;
            if( count <= FM2$C_FMT1_MAXCNT ) {
                hiblk += count;
                *n -= count--;
                *mp++ = ( F11WORD( FM2$M_FORMAT1 |
                                   (((*start & FM2$C_FMT1_MAXLBN) >> 16) << 8) |
                                   count ) );
                *mp++ = F11WORD( (f11word)*start );
                inuse += 2;
                if( !sparse )
                    *start += count + 1;
                break;
            }
        }
        if( count <= FM2$C_FMT2_MAXCNT ) {
            if( freew < 3 )
                break;
            hiblk += count;
            *n -= count--;
            *mp++ = F11WORD( FM2$M_FORMAT2 | count );
            *mp++ = F11WORD( (f11word)*start );
            *mp++ = F11WORD( (f11word)(*start >> 16) );
            inuse += 3;
            if( !sparse )
                *start += count + 1;
            break;
        }
        if( freew < 4 )
            break;
        if( count > FM2$C_FMT3_MAXCNT )
            count = FM2$C_FMT3_MAXCNT;
        hiblk += count;
        *n -= count--;
        *mp++ = F11WORD( FM2$M_FORMAT3 | (f11word)(count >> 16) );
        *mp++ = F11WORD( (f11word)count );
        *mp++ = F11WORD( (f11word)*start );
        *mp++ = F11WORD( (f11word)(*start >> 16) );
        inuse += 4;
        if( !sparse )
            *start += count + 1;
    }

    fhd->fh2$w_recattr.fat$l_hiblk = F11SWAP( hiblk );
    fhd->fh2$b_map_inuse = inuse;

    if( *n != 0 )
        return SS$_DEVICEFULL;

    return SS$_NORMAL;
}

/************************************************************ writeHEAD() */

static vmscond_t writeHEAD( void *dev, struct HEAD *head, uint32_t hdrbase ) {
    hdrbase += head->fh2$w_fid.fid$w_num - 1;

    return writeHDR( dev, head, hdrbase );
}

/************************************************************ writeHDR() */

static vmscond_t writeHDR( void *dev, struct HEAD *head, uint32_t lbn ) {
    head->fh2$w_checksum = checksum( (f11word *)head, offsetof( struct HEAD, fh2$w_checksum ) / 2 );
    head->fh2$w_checksum = F11WORD( head->fh2$w_checksum );

    WRITE( dev, lbn, 1, head );
    return SS$_NORMAL;
}

/************************************************************ writeHOM() */

static vmscond_t writeHOM( void *dev, struct HOME *hom,
                          uint32_t homlbn, uint32_t homvbn,
                          uint32_t ncopies ) {
    uint32_t copy = 0;

    do {
        hom->hm2$l_homelbn = F11LONG( homlbn );
        hom->hm2$w_homevbn = F11WORD( homvbn );
        hom->hm2$w_checksum1 = checksum( (f11word *)hom,
                                         offsetof( struct HOME, hm2$w_checksum1 ) /2 );
        hom->hm2$w_checksum1 =  F11WORD( hom->hm2$w_checksum1 );
        hom->hm2$w_checksum2 = checksum( (f11word *)hom,
                                         offsetof( struct HOME, hm2$w_checksum2 ) /2 );
        hom->hm2$w_checksum2 =  F11WORD( hom->hm2$w_checksum2 );

        WRITE( dev, homlbn, 1, hom );

        homlbn++;
        homvbn++;
    } while( ++copy < ncopies );

    return SS$_NORMAL;
}

/************************************************************ alloclbns() */

static vmscond_t alloclbns( uint8_t *bitmap,
                            uint32_t clustersize,
                            uint32_t n_clusters,
                            uint32_t startlbn, uint32_t n,
                            uint32_t *found ) {
    uint32_t nfound;
    int pass;

    *found = ~0U;

    startlbn /= clustersize;
    n = CLUSTERS( n );

    for( pass = 0, nfound = 0; pass < 2 && nfound < n; pass++ ) {
        uint32_t lbn;

        if( pass > 0 )
            startlbn = 0;

        for( lbn = startlbn, nfound = 0;
             nfound < n && lbn + n < n_clusters; lbn++ ) {
            if( bitmap[ lbn >> 3 ] & (1 << (lbn & 0x7)) ) {
                if( nfound++ == 0 )
                    startlbn = lbn;
            } else
                nfound = 0;
        }
    }

    if( nfound < n )
        return SS$_DEVICEFULL;

    modbits( bitmap, 0, startlbn, n );

    *found = startlbn * clustersize;
    return SS$_NORMAL;
}

/************************************************************ modlbnbits() */

static void modlbnbits( struct NEWVOL *params, uint8_t *bitmap, int set, uint32_t start, uint32_t n ) {
    ldiv_t d;
    uint32_t first, clusters, clustersize;

    if( n == 0 )
        return;

    clustersize = params->clustersize;

    d = ldiv( start, clustersize );

    clusters = 1;

    if( (first = clustersize - d.rem) < n )
        clusters += CLUSTERS( n - first );

    modbits( bitmap, set, d.quot, clusters );
    return;
}

/************************************************************ tstlbnbit() */

static int tstlbnbit( struct NEWVOL *params, uint8_t *bitmap, uint32_t lbn ) {

    lbn = lbn / params->clustersize;

    return (bitmap[lbn >> 3] & (1 << (lbn & 0x7))) != 0;
}

/************************************************************ modbits() */

static void modbits( uint8_t *bitmap, int set, uint32_t start, uint32_t n ) {
    uint32_t q, bit, byte;

    if( set ) {
        while( n > 0 ) {
            bit = start & 0x7;
            byte = start >>3;

            if( bit == 0 && n > 8 ) {
                q = n >> 3;
                n &= 0x7;

                if( q == 1 )
                    bitmap[byte] = 0xff;
                else
                    memset( bitmap+byte, 0xff, q );
                start += q << 3;
            } else {
                bitmap[byte] |= 1 << bit;
                start++;
                n--;
            }
        }
        return;
    }
    while( n > 0 ) {
        bit = start & 0x7;
        byte = start >>3;

        if( bit == 0 && n > 8 ) {
            q = n >> 3;
            n &= 0x7;

            if( q == 1 )
                bitmap[byte] = 0;
            else
                memset( bitmap+byte, 0x00, q );
            start += q << 3;
        } else {
            bitmap[byte] &= ~(1 << bit);
            start++;
            n--;
        }
    }
    return;
}

/************************************************************ checksum() */

static f11word checksum( f11word *data, size_t length ) {
    f11word sum;
    size_t count;

    for( sum = 0, count = 0; count < length; count++ )
        sum += *data++;
    return sum;
}

/*************************************************************** compute_delta() */
/*
 * The search delta is computed from the
 * volume  geometry,  expressed  in  sectors,  tracks   (surfaces),   and
 * cylinders, according to the following rules, to handle the cases where
 * one or two dimensions of the volume have a size of 1.
 *
 *           Geometry:     Delta
 *
 *           s x 1 x 1:    1          Rule 1
 *           1 x t x 1:    1          Rule 2
 *           1 x 1 x c:    1          Rule 3
 *
 *           s x t x 1:    s+1        Rule 4
 *           s x 1 x c:    s+1        Rule 5
 *           1 x t x c:    t+1        Rule 6
 *
 *           s x t x c:    (t+1)*s+1  Rule 7
 */


static uint32_t compute_delta( uint32_t sectorsize,
                               uint32_t sectors,
                               uint32_t tracks,
                               uint32_t cylinders ) {
    if( sectorsize < 512 )
        sectors = (sectorsize * sectors) / 512;

    if( sectors > 1 && tracks > 1 && cylinders > 1 )        /* Rule 7 */
        return  (tracks + 1) * sectors +1;

    if( (sectors > 1 && tracks > 1 && cylinders == 1 ) ||   /* Rule 4 */
        (sectors > 1 && tracks == 1 && cylinders > 1 ) )    /* Rule 5 */
        return sectors + 1;

    if( sectors == 1 && tracks > 1 && cylinders > 1 )       /* Rule 6 */
        return tracks + 1;

    return 1;                                               /* Rules 1-3 */
}

/************************************************************ delta_from_name() */

#if 0
static uint32_t delta_from_name( const char *diskname ) {
    struct disktype *dp;

    for( dp = disktype; dp->name != NULL; dp++ ) {
        if( !strcmp( dp->name, diskname ) )
            return compute_delta( dp->sectorsize, dp->sectors, dp->tracks, dp->cylinders );
    }

    return ~0u;
}
#endif

/************************************************************ delta_from_index() */

uint32_t delta_from_index( size_t index ) {
    struct disktype *dp;
    uint32_t delta;

    if( index > max_disktype )
        abort();

    dp = disktype + index;
    delta = compute_delta( dp->sectorsize, dp->sectors, dp->tracks, dp->cylinders );

#if DEBUG  || HOME_SKIP > 0
    printf( "HOM search index for %s is %u\n", dp->name, delta );
#endif

    return delta;
}
