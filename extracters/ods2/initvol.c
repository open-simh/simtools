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
 *       the contibution of the original author.
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
#include "vmstime.h"

#define bootlbn 0

#define STRUCLEV      (0x0201)
#define FM2$M_FORMAT1 (1 << 14)
#define FM2$M_FORMAT2 (2 << 14)
#define FM2$M_FORMAT3 (3 << 14)
#define FM2$C_FMT1_MAXLBN ((1 << 22) - 1)
#define FM2$C_FMT1_MAXCNT (256)
#define FM2$C_FMT2_MAXLBN (0xFFFFFFFF)
#define FM2$C_FMT2_MAXCNT (1 << 14)
#define FM2$C_FMT3_MAXCNT (1 << 30)

#define CLUSTERS(lbns) ( ((lbns) + clustersize - 1) / clustersize )
#define CLU2LBN(cluster) ( (cluster) * clustersize )

#if DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

static unsigned add2dir( void *mfdp, unsigned *mfdlen, unsigned mfdblocks,
                         struct HEAD *head );

static unsigned add2map( struct NEWVOL *params, struct HEAD *fhd,
                         unsigned start, unsigned n );

static unsigned writeHEAD( void *dev, struct HEAD *head, unsigned hdrbase ) ;
static unsigned writeHDR( void *dev, struct HEAD *head, unsigned lbn );
static unsigned writeHOM( void *dev,
                          struct HOME *hom,
                          unsigned homlbn, unsigned homvbn,
                          unsigned ncopies );
static unsigned alloclbns( unsigned char *bitmap,
                           unsigned clustersize,
                           unsigned n_clusters,
                           unsigned startlbn, unsigned n,
                           unsigned *found );
static int tstlbnbit( struct NEWVOL *params, unsigned char *bitmap, unsigned lbn );
static void modlbnbits( struct NEWVOL *params, unsigned char *bitmap, int set, unsigned start, unsigned n );
static void modbits( unsigned char *bitmap, int set, unsigned start, unsigned n );
static f11word checksum( void *data, size_t length );
#if 0
static unsigned int delta_from_name( const char *diskname );
#endif
static unsigned int compute_delta( unsigned long sectorsize,
                                   unsigned long sectors,
                                   unsigned long tracks,
                                   unsigned long cylinders );

static void *Malloc( void ***list, size_t size );
static void *Calloc( void ***list, size_t nmemb, size_t n );
#define MALLOC(name, size) do {                          \
        if( (name = Malloc( size )) == NULL )            \
            return SS$_INSFMEM;                          \
    } while( 0 )
#define CALLOC(name, nmemb, n) do {                                     \
        if( (name = Calloc( nmemb, n )) == NULL )                       \
            return SS$_INSFMEM;                                         \
    } while( 0 )

unsigned eofmark = 0;

#define WRITE(dev, lbn, n, buf) do {                                    \
        unsigned status;                                                \
                                                                        \
        if( !((status = virt_write( dev, (lbn), ((n)*512), (char *)(buf) )) & STS$M_SUCCESS) ) \
            return status;                                              \
        if( ( (lbn) + (n) ) > eofmark )                                 \
            eofmark = (lbn) + (n)  -1;                                  \
    } while( 0 );

#define WRITEP(dev, pbn, n, buf) do {                                    \
        unsigned status;                                                \
                                                                        \
        if( !((status = virt_writep( dev, (pbn), ((n)*dp->sectorsize), (char *)(buf) )) & STS$M_SUCCESS) ) \
            return status;                                              \
    } while( 0 );

#define READ(dev, lbn, n, buf) do {                                    \
        unsigned status;                                                \
                                                                        \
        if( !((status = virt_read( dev, (lbn), ((n)*512), (char *)(buf) )) & STS$M_SUCCESS) ) \
            return status;                                              \
    } while( 0 );

#define READP(dev, pbn, n, buf) do {                                    \
        unsigned status;                                                \
                                                                        \
        if( !((status = virt_readp( dev, (pbn), ((n)*dp->sectorsize), (char *)(buf) )) & STS$M_SUCCESS) ) \
            return status;                                              \
    } while( 0 );

static unsigned doinit( void ***memlist, void *dev, struct NEWVOL *params );

static uint16_t bootcode[] = { /* EHPLI M'T ARPPDEI SNDI E ADP-P11 */
    000240,  012706,  001000,  010700,  062700,  000036, 0112001,  001403,
    004767,  000006,  000773,  000005,  000000, 0110137, 0177566, 0105737,
   0177564, 0100375,  000207,  020040,  020040,  020040,  020040,  020040,
    020040,  020040,  020040,  071551,  067040,  072157,  060440,  071440,
    071571,  062564,  020155,  064544,  065563,  005015,  000012,  000000,
#define BOOT_LABEL_OFS 046
#define BOOT_LABEL_SIZE 15
};

unsigned initvol( void *dev, struct NEWVOL *params ) {
    void **memlist, **mp;
    unsigned status;
    memlist = (void **)calloc( 1, sizeof( void * ) );

    if( params->options & INIT_LOG ) {
        printf( "%%INIT-I-STARTING, Initializing %s on %s as %s %s\n",
                params->label, params->devnam, params->devtype,
                (params->options & INIT_VIRTUAL)? "disk image": "physical device" );
    }

    status = doinit( &memlist, dev, params );

    for( mp = memlist; *mp != NULL; mp++ )
        free( *mp );
    free( memlist );

    if((status & STS$M_SUCCESS) && (params->options & INIT_LOG) ) {
        printf( "%%INIT-I-VOLINIT, %s initialized successfully\n",
                params->label );
    }

    return status;
}

static void *Calloc( void ***list, size_t nmemb, size_t n ) {
    void *mp;

    n *= nmemb;
    mp = Malloc( list, n );
    if( mp == NULL )
        return NULL;
    memset( mp, 0, n );
    return mp;
}

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

static unsigned doinit( void ***memlist, void *dev, struct NEWVOL *params ) {
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
    unsigned status, tmp, volstart, clustersize;
    unsigned pbs, pb_per_lb, lb_per_pb, npb, nlb, n_clusters;
    unsigned homlbn, homvbn, hm2lbn, hm2cluster, fhlbn;
    unsigned mfdlbn, bmplbn, idxlbn, aidxlbn;
    const char *p = NULL;
    unsigned char *bitmap = NULL, *indexmap = NULL;
    unsigned bitmap_size, indexmap_size;
    uint16_t *bootblock = NULL;
    time_t serial;
    char tbuf[12+1];
    char *mfdp = NULL;
    unsigned mfdlen = 0, mfdblocks = 3, mfdalloc;
    struct UIC owneruic = { 1, 1 };

    if( params->username && (!params->username[0] ||
                             (tmp = strlen( params->username )) > 12 ||
                             strspn( params->username, VOL_ALPHABET ) != tmp) )
        return SS$_BADPARAM;

    if( params->useruic &&
#ifdef _WIN32
        sscanf_s
#else
        sscanf
#endif
        ( params->useruic, " [%ho,%ho]", &owneruic.uic$w_grp, &owneruic.uic$w_mem ) != 2 )
        return SS$_BADPARAM;

    for( dp = disktype; dp->name != NULL; dp++ ) {
        if( !strcmp( params->devtype, dp->name ) )
            break;

    }
    if( dp->name == NULL )
        return SS$_BADPARAM;

    pbs = dp->sectorsize;
    pb_per_lb = (pbs < 512)? 512/pbs: 1;
    lb_per_pb = (pbs >512)? pbs/512: 1;

    npb = (dp->sectors * dp->tracks * dp->cylinders ) - dp->reserved;
    nlb = (unsigned) ((((unsigned long long)npb) * lb_per_pb)
                      / pb_per_lb);

    if( params->clustersize == 0 ) {
        if( nlb < 50000 )
            params->clustersize = 1;
        else {
            unsigned long long tmp;
            tmp = (nlb + (255 * 4096) - 1) / (255 * 4096);

            params->clustersize = (unsigned) (tmp > 3? tmp: 3);
        }
    }
    tmp = nlb / 50;
    if( tmp > 16382 )
        tmp = 16382;
    if( params->clustersize > tmp )
        params->clustersize = tmp;

    clustersize = params->clustersize;

    if( npb < 100 || nlb < 100 || clustersize < 1 ) {
        printf( "%%INIT-E-TOOSMALL, Device is too small to hold a useful Files-11 volume\n" );
        return SS$_DEVICEFULL;
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

    (void) time( &serial );

    /* Create the bootblock.  (This is PDP-11 code. )
     */

    CALLOC( bootblock, 256, sizeof( uint16_t ) );
    memcpy( bootblock, bootcode, sizeof( bootcode ) );

    for( p = params->label, tmp = 0; *p && tmp < BOOT_LABEL_SIZE; tmp++, p++ )
        ((char *)bootblock)[BOOT_LABEL_OFS+tmp] = toupper(*p);
#ifdef ODS2_BIG_ENDIAN
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
    hom->hm2$w_resfiles = F11WORD( 9 );
    hom->hm2$l_maxfiles = F11LONG( params->maxfiles );
    hom->hm2$w_volowner.uic$w_grp = F11WORD( owneruic.uic$w_grp );
    hom->hm2$w_volowner.uic$w_mem = F11WORD( owneruic.uic$w_mem );
    hom->hm2$w_protect = F11WORD( 0 );
    hom->hm2$w_fileprot = F11WORD( 0xFA00 );
    hom->hm2$b_window = 7;
    hom->hm2$b_lru_lim = 16;
    hom->hm2$w_extend = F11WORD( 5 );

    CALLOC( scb, 1, 512 );
    scb->scb$w_cluster = F11WORD(clustersize);
    scb->scb$w_struclev = F11WORD(STRUCLEV);
    scb->scb$l_volsize = F11LONG(nlb);
    scb->scb$l_blksize = F11LONG(pb_per_lb);
    scb->scb$l_sectors = F11LONG(dp->sectors);
    scb->scb$l_tracks = F11LONG(dp->tracks);
    scb->scb$l_cylinders = F11LONG(dp->cylinders);
    scb->scb$l_status = F11LONG( 0 );
    scb->scb$l_status = F11LONG( 0 );
    scb->scb$w_writecnt = 0;

    /* If necessary, scb$t_volockname will be initialized by mount under
     * an OS.
     */

    if( !((status = sys$gettim( scb->scb$q_mounttime )) & STS$M_SUCCESS) ) {
        return status;
    }
    scb->scb$w_checksum = F11WORD(checksum((f11word *)&scb, offsetof( struct SCB, scb$w_checksum ) / 2  ) );

    bitmap_size = (n_clusters + 4095) / 4096;

    indexmap_size = (params->maxfiles + 4095) / 4096;
    hom->hm2$l_maxfiles = F11LONG( params->maxfiles );
    memcpy( hom->hm2$q_credate, scb->scb$q_mounttime, sizeof(hom->hm2$q_credate) );
    memcpy( hom->hm2$q_revdate, scb->scb$q_mounttime, sizeof(hom->hm2$q_revdate) );

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

    CALLOC( indexmap, 1, indexmap_size * 512 );

    hom->hm2$w_ibmapsize = F11WORD( (f11word)indexmap_size );

    /* Index header will be template for all other files.
     * Index-specific settings must come after copy.
     */

    idxhead->fh2$b_idoffset = offsetof( struct HEAD, fh2$r_restofit ) / 2;
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

    idxhead->fh2$w_recattr.fat$l_hiblk = F11LONG( 0 );
    idxhead->fh2$w_recattr.fat$l_efblk = F11LONG( 0 );
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
    idxhead->fh2$w_fileprot = F11WORD( 0xFA00 );
    idxhead->fh2$w_backlink.fid$w_num = F11WORD( 4 );
    idxhead->fh2$w_backlink.fid$w_seq = F11WORD( 4 );
    idxhead->fh2$w_backlink.fid$b_rvn = 0;
    idxhead->fh2$w_backlink.fid$b_nmx = 0;
    idxhead->fh2$l_highwater = F11LONG( 1 );

#define IDENTp(head) ((struct IDENT *) (((f11word *)head) + F11WORD(head->fh2$b_idoffset)))
    ident = IDENTp(idxhead);

    ident->fi2$w_revision = F11WORD( 1 );
    memcpy( ident->fi2$q_credate, scb->scb$q_mounttime, sizeof(ident->fi2$q_credate) );
    memcpy( ident->fi2$q_revdate, scb->scb$q_mounttime, sizeof(ident->fi2$q_credate) );
    idxhead->fh2$b_mpoffset =F11WORD(  F11WORD(idxhead->fh2$b_idoffset) +
                                      (offsetof( struct IDENT, fi2$t_filenamext ) / 2) );

    memcpy( badhead, idxhead, sizeof( *badhead ) );
    memcpy( corhead, idxhead, sizeof( *corhead ) );
    memcpy( volhead, idxhead, sizeof( *volhead ) );
    memcpy( conthead, idxhead, sizeof( *conthead ) );
    memcpy( bakhead, idxhead, sizeof( *bakhead ) );
    memcpy( loghead, idxhead, sizeof( *loghead ) );
    memcpy( bmphead, idxhead, sizeof( *bmphead ) );
    memcpy( mfdhead, idxhead, sizeof( *mfdhead ) );

    bmphead->fh2$l_filechar = F11LONG( FH2$M_CONTIG );
    mfdhead->fh2$l_filechar = F11LONG( FH2$M_DIRECTORY | FH2$M_CONTIG );
    mfdhead->fh2$w_fileprot &= ~F11WORD( 0x4000 ); /* W:E allowed for MFD */

    mfdblocks = CLU2LBN( CLUSTERS( mfdblocks ) );
    CALLOC( mfdp, 1, mfdblocks * 512 );

#define SETFILE(head, num, seq, name, rtype, rattr, rsize )  do {       \
        unsigned status;                                                \
        struct IDENT *id;                                               \
        char fname[sizeof( id->fi2$t_filename )+1];                     \
        id = IDENTp(head);                                              \
        (void) snprintf( fname, sizeof( fname ), "%-20.20s", #name );          \
        memcpy( id->fi2$t_filename, fname, sizeof( id->fi2$t_filename ) ); \
        head->fh2$w_fid.fid$w_num = F11WORD( num );                     \
        head->fh2$w_fid.fid$w_seq = F11WORD( seq );                     \
        head->fh2$w_recattr.fat$b_rtype = (FAB$C_SEQ << 4) | (rtype);   \
        head->fh2$w_recattr.fat$b_rattrib = rattr;                      \
        head->fh2$w_recattr.fat$w_rsize = F11WORD( rsize );             \
        head->fh2$w_recattr.fat$w_maxrec = F11WORD( rsize );            \
        if( !((status = add2dir( mfdp, &mfdlen, mfdblocks, head )) & STS$M_SUCCESS) ) \
            return status;                                              \
    } while( 0 )

    /* Must be in alphabetical order for MFD */
    SETFILE( mfdhead,  4,4, 000000.DIR;1,  FAB$C_VAR, FAB$M_BLK, 512 );
    SETFILE( bakhead,  8,8, BACKUP.SYS;1,  FAB$C_FIX, 0,          64 );
    SETFILE( badhead,  3,3, BADBLK.SYS;1,  FAB$C_FIX, 0,         512 );
    SETFILE( loghead,  9,9, BADLOG.SYS;1,  FAB$C_FIX, 0,          16 );
    SETFILE( bmphead,  2,2, BITMAP.SYS;1,  FAB$C_FIX, 0,         512 );
    SETFILE( conthead, 7,7, CONTIN.SYS;1,  FAB$C_FIX, 0,         512 );
    SETFILE( corhead,  5,5, CORIMG.SYS;1,  FAB$C_FIX, 0,         512 );
    SETFILE( idxhead,  1,1, INDEXF.SYS;1,  FAB$C_FIX, 0,         512 );
    SETFILE( volhead,  6,6, VOLSET.SYS;1,  FAB$C_FIX, 0,          64 );

    modbits( indexmap, 1, 0, 9 );

    MALLOC( bitmap, 512 * bitmap_size );
    memset( bitmap, 0xFF, 512 * bitmap_size );

    DPRINTF(("Non-existent bitmap bits %u / %u LBNs %u - %u\n",
             (bitmap_size *4096) - n_clusters, bitmap_size *4096,
             n_clusters * clustersize, bitmap_size * 4096 * clustersize));
    modbits( bitmap, 0, n_clusters, (bitmap_size * 4096) - n_clusters );

#define ADDMAPENTRY(head, start, n ) do {                               \
        unsigned status;                                                \
                                                                        \
        if( !((status = add2map( params, (head), start, n)) & STS$M_SUCCESS ) ) \
            return status;                                              \
    } while( 0 )

    if( dp->flags & DISK_BAD144 ) {
        unsigned start, count;

        tmp = dp->sectors * dp->sectorsize;

        MALLOC( mbad, tmp * 10 );
        if( params->options &  INIT_VIRTUAL ) {
            memset( mbad->bbd$l_badblock, ~0, tmp );
            mbad->bbd$l_serial = (uint32_t) serial;
            mbad->bbd$w_reserved = 0;
            mbad->bbd$w_flags = 0;
            for( count = 0; count < 10; count++ )
                memcpy( ((char *)mbad)+(count * dp->sectorsize),
                        mbad, dp->sectorsize );
        }

        /* Might have to deal with finding clusters more accurately
         * for disks with interleave/skew.
         */
        start = npb - 10;
        start = (unsigned) ( ( ((off_t)tmp) * pb_per_lb ) / lb_per_pb );
        count = (unsigned) ( ( ((off_t)10) * lb_per_pb ) / pb_per_lb );

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
            sbad->bbm$b_avail = (uint8_t)( ( offsetof( struct SWBAD, bbm$w_checksum ) -
                                             offsetof( struct SWBAD, bbm$r_map ) ) / sizeof( uint8_t ) );
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

    homlbn = 1;
    tmp = 0;

    if( !tstlbnbit( params, bitmap, homlbn ) ) {
        tmp = delta_from_index( dp - disktype );
        do {
            int freelbn;

            freelbn = tstlbnbit( params, bitmap, homlbn );
            modlbnbits( params, bitmap, 0, homlbn, 1 );
            modlbnbits( params, bitmap, 0, bootlbn, 1 );

            if( freelbn )
                break;

            /* add to bad blocks */

            homlbn += tmp;
        } while( homlbn < nlb );
    }

    if( homlbn >= nlb ) {
        printf( "No good block found for HOM block\n" );
        return SS$_DEVICEFULL;
    }

    modlbnbits( params, bitmap, 0, bootlbn, 1 );


    DPRINTF(("Boot block = %u, primary HOM block = %u\n", bootlbn, homlbn));

    if( bootlbn == 0 && homlbn < clustersize * 2 ) {
        modlbnbits( params, bitmap, 0, bootlbn, clustersize * 2);
        ADDMAPENTRY( idxhead, bootlbn,  clustersize * 2 );
    } else {
        modlbnbits( params, bitmap, 0, bootlbn, clustersize );
        modlbnbits( params, bitmap, 0, homlbn, clustersize *2 );

        ADDMAPENTRY( idxhead, bootlbn, clustersize );
        ADDMAPENTRY( idxhead, homlbn,  clustersize * 2 );
    }

    /* Alternate HOM block
     */

    if( tmp == 0 )
        tmp = delta_from_index( dp - disktype );

    hm2lbn = homlbn;
    do {
        int freelbn;

        hm2lbn += tmp;
        freelbn = tstlbnbit( params, bitmap, hm2lbn );

        modlbnbits( params, bitmap, 0, hm2lbn, 1 );
        if( freelbn && hm2lbn >= params->clustersize * 2 )
            break;
        /* Add to bad blocks */
    } while( hm2lbn < nlb );

    if( hm2lbn >= nlb ) {
        printf( "No good block found for HOM block\n" );
        return SS$_DEVICEFULL;
    }

    hm2cluster = hm2lbn / clustersize;

    DPRINTF(("Alternate HOM block is %u\n", hm2lbn));

    ADDMAPENTRY( idxhead, CLU2LBN( hm2cluster ), 1 );

    hom->hm2$l_alhomelbn = F11LONG( hm2lbn );

    if( params->options & INIT_LOG ) {
        printf( "%%INIT-I-SIZING, Volume size %u block, cluster size %us, maximum files %u\n",
                nlb, clustersize, params->maxfiles );
        printf(  "     -I-SIZING, Preallocated %u file headers, %u directory entries\n",
                 params->headers, params->directories );
        printf( "%%INIT-I-PLACED, Boot block at LBN %u, HOM blocks at LBNs %u and %u\n",
                bootlbn, homlbn, hm2lbn );
    }

    /* Variable placement. */

    if( params->indexlbn == INIT_INDEX_MIDDLE )
        volstart = nlb / 2;
    else
        volstart = params->indexlbn;

#define ALLOCLBNS( start, number, result ) do {                         \
        unsigned status;                                                \
        if( !((status = alloclbns( bitmap, clustersize, n_clusters,     \
                                   (start), (number), (result))) & STS$M_SUCCESS) ) \
            return status;                                              \
    } while( 0 )

    /* This order is recommended for best placement */

    /* MFD */
    ALLOCLBNS( volstart, mfdalloc, &mfdlbn );

    /* Storage bitmap */
    ALLOCLBNS( volstart, 1 + bitmap_size, &bmplbn );

    /* Index bitmap and initial file headers. */
    ALLOCLBNS( volstart, indexmap_size + params->headers, &idxlbn );

    /* Backup index header -- Could be up around +nlb /4... */
    ALLOCLBNS( volstart, 1, &aidxlbn );

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
        printf( "%%INIT-I-PLACED, MFD at LBN %u, index file at LBNs %u thru %u\n",
                mfdlbn, idxlbn, idxlbn + indexmap_size + params->headers -1 );
    }

    /* Let's try for a spiral write... */

    WRITE( dev, 0, 1, bootblock );

    /* Write HOM blocks */

    homvbn = 2;
    hom->hm2$w_homevbn = F11WORD( homvbn );

    DPRINTF(("Primary HOM block lbn %u vbn %u\n", homlbn, homvbn ));

    homvbn = (clustersize * 2) + 1;
    hom->hm2$w_alhomevbn = F11WORD( homvbn );

    tmp = (clustersize > 2)?                         /* Copies needed */
                (clustersize * 2) - 1: 1;

    if( !((status = writeHOM( dev, hom, homlbn, 2, tmp )) & STS$M_SUCCESS) )
        return status;

    DPRINTF(("Secondary HOM block lbn %u vbn %u\n", hm2lbn, homvbn));

    if( !((status = writeHOM( dev, hom, hm2cluster * clustersize, homvbn, clustersize )) & STS$M_SUCCESS) )
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

#define WRITEHEAD(head,name) do {                               \
        unsigned status;                                        \
        head->fh2$w_checksum = checksum( (f11word *)(head),    \
                                         offsetof( struct HEAD, fh2$w_checksum ) / 2 ); \
        DPRINTF(("Writing " #name " header\n"));                \
        if( !((status = writeHEAD( dev, (head), fhlbn )) & STS$M_SUCCESS) ) \
            return status;                                              \
    } while( 0 )

    WRITEHEAD( idxhead, INDEXF.SYS;1 );
    WRITEHEAD( bmphead,  BITMAP.SYS;1 );
    WRITEHEAD( badhead,  BADBLK.SYS;1 );
    WRITEHEAD( mfdhead,  000000.DIR;1 );
    WRITEHEAD( corhead,  CORIMG.SYS;1 );
    WRITEHEAD( volhead,  VOLSET.SYS;1 );
    WRITEHEAD( conthead, CONTIN.SYS;1 );
    WRITEHEAD( bakhead,  BACKUP.SYS;1 );
    WRITEHEAD( loghead,  BADLOG.SYS;1 );

    if( !((status = writeHDR( dev, idxhead, aidxlbn )) & STS$M_SUCCESS) )
        return status;

    if( params->options &  INIT_VIRTUAL ) {
        if( dp->flags & DISK_BAD144 ) {
            if( params->options & INIT_LOG ) {
                printf( "%%INIT-I-MFGBAD, writing manufacturer's bad block index\n" );
            }
            WRITEP( dev, npb - 10, 10, mbad );
        }
        if( dp->flags & DISK_BADSW ) {
            if( params->options & INIT_LOG ) {
                printf( "%%INIT-I-SWBAD, writing software bad block descriptor\n" );
            }
            sbad->bbm$w_checksum = checksum( sbad, offsetof( struct SWBAD, bbm$w_checksum ) / 2 );
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

static unsigned add2dir( void *dirp, unsigned *dirlen, unsigned dirblocks,
                         struct HEAD *head ) {
    struct dir$r_ent *de;
    struct dir$r_rec *dr;
    struct IDENT *ident;
    unsigned block, offset;
    f11word reclen, paddednamelen, version, verlimit;
    size_t namelen, typlen;
    char fname[  sizeof( ident->fi2$t_filename ) +
                 sizeof( ident->fi2$t_filenamext ) + 1], *p;
    int extlen;

    ident = IDENTp(head);

    memcpy( fname, ident->fi2$t_filename, sizeof( ident->fi2$t_filename ) );

    extlen = ((head->fh2$b_mpoffset - head->fh2$b_idoffset) * sizeof( f11word )) -
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

    verlimit = (head->fh2$l_filechar & FH2$M_DIRECTORY)? 1: 0;

    paddednamelen = (f11word) ( ((namelen + 1) / 2) * 2 );

    reclen = offsetof( struct dir$r_rec, dir$t_name ) + paddednamelen + sizeof( struct dir$r_ent );
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
    de = (struct dir$r_ent *) ( ((char *)dirp) + ( block * 512 ) + offset + reclen - sizeof( struct dir$r_ent ));
    de->dir$w_version = version;
    de->dir$w_fid = head->fh2$w_fid;
    offset += reclen;
    if( offset <= 512 - sizeof(f11word) )
        ((f11word *)dirp)[offset/sizeof(f11word)] = F11WORD( -1 );
    *dirlen += reclen;

    return SS$_NORMAL;
}

static unsigned add2map( struct NEWVOL *params, struct HEAD *fhd,
                         unsigned start, unsigned n ) {
    f11word *mpe;
    uint8_t mapsize, inuse;
    f11long hiblk;
    unsigned clustersize;

    clustersize = params->clustersize;

    hiblk = F11SWAP( fhd->fh2$w_recattr.fat$l_hiblk );

#if DEBUG
    printf( "Adding %u blocks at lbn %u, clu %u to %-20.20s",
            n, start, start / clustersize,  IDENTp(fhd)->fi2$t_filename );
#endif

    n = CLU2LBN( CLUSTERS( n ) );
    start = CLU2LBN( start / clustersize );

    mapsize = fhd->fh2$b_acoffset - fhd->fh2$b_mpoffset;
    inuse = fhd->fh2$b_map_inuse;

    mpe = ((f11word *)fhd) + F11WORD( fhd->fh2$b_mpoffset ) + inuse;

#if DEBUG
    printf( "Alloc: %u\n", n );
#endif

    while( n > 0 ) {
        f11word freew =  mapsize - inuse;
        unsigned count;

        count = n;
        if( start <= FM2$C_FMT1_MAXLBN ) {
            if( freew < 2 )
                return SS$_DEVICEFULL;
            if( count <= FM2$C_FMT1_MAXCNT ) {
                hiblk += count;
                n -= count--;
                *mpe++ = F11WORD( FM2$M_FORMAT1 | ((start >> 16) << 8) | count );
                *mpe++ = F11WORD( (f11word)start );
                inuse += 2;
                start += count + 1;
                continue;
            }
        }
        if( count <= FM2$C_FMT2_MAXCNT ) {
            if( freew < 3 )
                return SS$_DEVICEFULL;
            hiblk += count--;
            *mpe++ = F11WORD( FM2$M_FORMAT2 | count );
            *mpe++ = F11WORD( (f11word)start );
            *mpe++ = F11WORD( (f11word)(start >> 16) );
            inuse += 3;
            break;
        }
        if( freew < 4 )
            return SS$_DEVICEFULL;
        if( count > FM2$C_FMT3_MAXCNT )
            count = FM2$C_FMT3_MAXCNT;
        hiblk += count;
        n -= count--;
        *mpe++ = F11WORD( FM2$M_FORMAT3 | (f11word)(count >> 16) );
        *mpe++ = F11WORD( (f11word)count );
        *mpe++ = F11WORD( (f11word)start );
        *mpe++ = F11WORD( (f11word)(start >> 16) );
        inuse += 4;
        start += count + 1;
    }

    fhd->fh2$w_recattr.fat$l_hiblk = F11SWAP( hiblk );

#if DEBUG
    {
        unsigned vbn = 1;
        unsigned i;
        f11word *mp;

        mp = ((f11word *)fhd) + F11WORD( fhd->fh2$b_mpoffset );

        for( i = 0; i < inuse; i++ ) {
            f11word e0;
            unsigned count, pbn;

            e0 = *mp++;
            e0 = F11WORD(e0);
            switch( e0 &  FM2$M_FORMAT3 ) {
            case  FM2$M_FORMAT1:
                count = e0 & 0xff;
                pbn = F11WORD(*mp) | (((e0 & ~FM2$M_FORMAT3) >> 8) << 16);
                mp++;
                break;
            case  FM2$M_FORMAT2:
                count = e0 & ~FM2$M_FORMAT3;
                pbn = *mp++;
                pbn = F11WORD( pbn );
                pbn |= F11WORD (*mp ) << 16;
                mp++;
                break;
            case FM2$M_FORMAT3:
                count = ((e0 & ~FM2$M_FORMAT3) << 16);
                count |= F11WORD( *mp );
                mp++;
                pbn = *mp++;
                pbn = F11WORD( pbn );
                pbn |= F11WORD (*mp ) << 16;
                mp++;
                break;
            default:
                continue;
            }
            ++count;
            printf(" VBN %u, FMT %u PBN %u, Count %u\n", vbn, (e0 >> 14), pbn, count );
            vbn += count;
        }
        printf( " File EOF VBN %u FFB %u, Alloc: %u\n",
                F11SWAP( fhd->fh2$w_recattr.fat$l_efblk ),
                fhd->fh2$w_recattr.fat$w_ffbyte,
                F11SWAP(  fhd->fh2$w_recattr.fat$l_hiblk ) );
    }
#endif

    fhd->fh2$b_map_inuse = inuse;
    return SS$_NORMAL;
}

static unsigned writeHEAD( void *dev, struct HEAD *head, unsigned hdrbase ) {
    hdrbase += head->fh2$w_fid.fid$w_num - 1;

    return writeHDR( dev, head, hdrbase );
}

static unsigned writeHDR( void *dev, struct HEAD *head, unsigned lbn ) {
    head->fh2$w_checksum = checksum( (f11word *)head, offsetof( struct HEAD, fh2$w_checksum ) / 2 );
    head->fh2$w_checksum = F11WORD( head->fh2$w_checksum );

    WRITE( dev, lbn, 1, head );
    return SS$_NORMAL;
}
static unsigned writeHOM( void *dev, struct HOME *hom,
                          unsigned homlbn, unsigned homvbn,
                          unsigned ncopies ) {
    unsigned copy = 0;

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

static unsigned alloclbns( unsigned char *bitmap,
                           unsigned clustersize,
                           unsigned n_clusters,
                           unsigned startlbn, unsigned n,
                           unsigned *found ) {
    unsigned nfound;
    int pass;

    *found = ~0U;

    startlbn /= clustersize;
    n = CLUSTERS( n );

    for( pass = 0, nfound = 0; pass < 2 && nfound < n; pass++ ) {
        unsigned lbn;

        if( pass > 0 ) startlbn = 0;

        for( lbn = startlbn, nfound = 0; nfound < n && lbn + n < n_clusters; lbn++ ) {
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

static void modlbnbits( struct NEWVOL *params, unsigned char *bitmap, int set, unsigned start, unsigned n ) {
    ldiv_t d;
    unsigned first, clusters, clustersize;

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

static int tstlbnbit( struct NEWVOL *params, unsigned char *bitmap, unsigned lbn ) {

    lbn = lbn / params->clustersize;

    return (bitmap[lbn >> 3] & (1 << (lbn & 0x7))) != 0;
}

static void modbits( unsigned char *bitmap, int set, unsigned start, unsigned n ) {
    unsigned q, bit, byte;

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

static f11word checksum( void *data, size_t length ) {
    f11word sum, *dp;
    size_t count;

    for( sum = 0, dp = (f11word *)data,
             count = 0; count < length; count++ )
        sum += *dp++;
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


static unsigned int compute_delta( unsigned long sectorsize,
                                   unsigned long sectors,
                                   unsigned long tracks,
                                   unsigned long cylinders ) {

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

#if 0
static unsigned int delta_from_name( const char *diskname ) {
    struct disktype *dp;

    for( dp = disktype; dp->name != NULL; dp++ ) {
        if( !strcmp( dp->name, diskname ) )
            return compute_delta( dp->sectorsize, dp->sectors, dp->tracks, dp->cylinders );
    }

    return ~0u;
}
#endif
unsigned int delta_from_index( size_t index ) {
    struct disktype *dp;
    unsigned int delta;

    if( index > max_disktype )
        abort();

    dp = disktype + index;
    delta = compute_delta( dp->sectorsize, dp->sectors, dp->tracks, dp->cylinders );

#if defined( DEBUG )  || HOME_SKIP > 0
    printf( "HOM search index for %s is %u\n", dp->name, delta );
#endif

    return delta;
}
