/* check for cr - return terminator - update file length */
/* RMS.c  RMS components */

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

/*  Boy some cleanups are needed here - especially for
 *  error handling and deallocation of memory after errors..
 *  For now we have to settle for loosing memory left/right/center...
 *
 *  This module implements file name parsing, file searches,
 *  file opens, gets, etc...
 */


#if !defined( DEBUG ) && defined( DEBUG_RMS )
#define DEBUG DEBUG_RMS
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#define DEBUGx x

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RMS$INITIALIZE          /* used by rms.h to create templates */

#include "access.h"
#include "descrip.h"
#include "device.h"
#include "direct.h"
#include "fibdef.h"
#include "ods2.h"
#include "rms.h"
#include "ssdef.h"
#include "stsdef.h"
#include "compat.h"
#include "sysmsg.h"
#include "vmstime.h"


/*      For file context info we use WCCDIR and WCCFILE structures...
 *      Each WCCFILE structure contains one WCCDIR structure for file
 *      context. Each level of directory has an additional WCCDIR record.
 *      For example DKA200:[PNANKERVIS.F11]RMS.C is loosely stored as:-
 *                      next         next
 *              WCCFILE  -->  WCCDIR  -->  WCCDIR
 *              RMS.C;       F11.DIR;1    PNANKERVIS.DIR;1
 *
 *      WCCFILE is pointed to by fab->fab$l_nam->nam$l_wcc and if a
 *      file is open also by ifi_table[fab->fab$w_ifi]  (so that close
 *      can easily locate it).
 *
 *      Most importantly WCCFILE contains a resulting filename field
 *      which stores the resulting file spec. Each WCCDIR has a prelen
 *      length to indicate how many characters in the specification
 *      exist before the bit contributed by this WCCDIR. (ie to store
 *      the device name and any previous directory name entries.)
 */


#define STATUS_INIT      (1 << 0)
#define STATUS_TMPDIR    (1 << 1)

struct WCCDIR {
    struct WCCDIR *wcd_next;
    struct WCCDIR *wcd_prev;
#if DEBUG
    size_t size;
#endif
    int wcd_status;
    int wcd_wcc;
    int wcd_prelen;
    unsigned short wcd_reslen;
    struct dsc_descriptor wcd_serdsc;
    struct fiddef wcd_dirid;
    struct fiddef wcd_fid;
    char wcd_sernam[1];         /* Must be last in structure */
};                              /* Directory context */

#define STATUS_RECURSE   (1 << 2)
#define STATUS_TMPWCC    (1 << 3)
#define STATUS_BYFID     (1 << 4)

#define MAX_FILELEN 1024


/* Allocation sise for WCCFILE = block + max len wcd_sernam.
 *  WCCDIRs are allocated smaller on the chain..
 */
#define WCFALLOC_SIZE (sizeof(struct WCCFILE)+256)

struct WCCFILE {
    struct FAB *wcf_fab;
    struct VCB *wcf_vcb;
    struct FCB *wcf_fcb;
    int wcf_status;
    uint32_t wcf_fnb;
    struct fiddef wcf_fid;
    char wcf_result[MAX_FILELEN];
    struct WCCDIR wcf_wcd;      /* Must be last..... (dynamic length). */
};                              /* File context */

static vmscond_t do_parse( struct FAB *fab, struct WCCFILE **wccret );
static int rlf_dir( char **srcp, int *lenp, struct NAM *rlf );
static vmscond_t name2nam( struct NAM *nam, struct WCCFILE *wccfile, int rsl );

static const VMSTIME timenotset = VMSTIME_ZERO;

/* Table of file name component delimeters... */

#define IS_DELIM(ch) (((ch) < 0x60) && char_delim[ch])
static unsigned char char_delim[] = {
    /*1 2 3 4 5 7 7 8 9 A B C D E F  0 1 2 3 4 5 7 7 8 9 A B C D E F  */
    /*                                                                */ /* 00 - 1F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /*                          .                        : ; <   >    */ /* 20 - 3F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0, 0,0,0,0,0,0,0,0,0,0,1,1,1,0,1,0,
    /*                                                     [   ]      */ /* 40 - 5F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,
};

/*************************************************************** name_delim() */

/* Parse filename string and return size of each component.
 *    [0] => dev, [1] => dir, [2] => name, [3] => type, [4] => vers
 * Includes 'dev:'    '[dir]'       'name'      '.type'       ';nnn'
 */

static vmscond_t name_delim( char *str, int len, int size[5] ) {
    register unsigned char ch = 0;
    register char *curptr;
    register char *endptr;
    register char *begptr;

    begptr =
        curptr = str;
    endptr = begptr + len;
    memset( size, 0, 5 * sizeof( size[0] ) );

    while( curptr < endptr ) {
        ch = *curptr++;
        if( IS_DELIM(ch) )
            break;
    }
    if( curptr > begptr && ch == ':' ) {
        size[0] = (int)(curptr - begptr);
        if( !size[0] || size[0] > (int)NAM$S_DVI ) /* VMS: 15 physdevnam,64 lognam */
            return RMS$_FNM;
        begptr = curptr;
        while( curptr < endptr ) {
            ch = *curptr++;
            if( IS_DELIM(ch) )
                break;
        }
    }

    if( ch == '[' || ch == '<' ) {
        unsigned char closech;
        char *sp;

        if( curptr != begptr + 1 )
            return RMS$_FNM;
        closech = (ch == '[')? ']': '>';

        sp = curptr;
        while( curptr < endptr ) {
            ch = *curptr++;
            if( IS_DELIM(ch) ) {
                if( ch == '.' ) {
                    if( (curptr - sp) > 40 )
                        return RMS$_FNM;
                    sp = curptr;
                } else
                    break;
            }
        }
        if( curptr < begptr + 2 || (curptr - sp) > 40 || ch != closech )
            return RMS$_FNM;
        size[1] = (int)(curptr - begptr);
        begptr = curptr;
        while( curptr < endptr ) {
            ch = *curptr++;
            if( IS_DELIM(ch) )
                break;
        }
    }

    if( curptr > begptr && IS_DELIM(ch) ) {
        size[2] = (int)(curptr - begptr) - 1;
        begptr = curptr - 1;
    } else {
        size[2] = (int)(curptr - begptr);
        begptr = curptr;
    }
    if (curptr > begptr && ch == '.') {
        while (curptr < endptr) {
            ch = *curptr++;
            if( IS_DELIM(ch) )
                break;
        }
        if (curptr > begptr && IS_DELIM(ch)) {
            size[3] = (int)((curptr - begptr) - 1);
            begptr = curptr - 1;
        } else {
            size[3] = (int)(curptr - begptr);
            begptr = curptr;
        }
    }

    if( curptr > begptr && (ch == ';' || ch == '.') ) {
        while (curptr < endptr) {
            ch = *curptr++;
            if( IS_DELIM(ch) )
                break;
        }
        size[4] = (int)(curptr - begptr);
    }

    if( size[2] > 39 || size[3] > 40 )
        return RMS$_FNM;

#if DEBUG
    printf("Name delim %.*s = %d %d %d %d %d\n",
           len, str,
           size[0], size[1], size[2], size[3], size[4]);
#endif
    if (curptr < endptr)
        return RMS$_FNM;

    return SS$_NORMAL;
}

/************************************************************** cleanup_wcf() */
#define WCDALLOC_SIZE(size) (offsetof(struct WCCDIR, wcd_sernam)+(size))

#define WCFALLOC_SIZE (sizeof(struct WCCFILE)+256)


/* Function to remove WCCFILE and WCCDIR structures when not required */

static void cleanup_wcf( struct WCCFILE **wccfilep ) {
    struct WCCFILE *wccfile;
    struct WCCDIR *wcc;

    wccfile = *wccfilep;
    *wccfilep = NULL;

    if( wccfile == NULL || wccfile == (struct WCCFILE *)-1 )
        return;

    wcc = wccfile->wcf_wcd.wcd_next;
#if DEBUG
    wccfile->wcf_wcd.wcd_next = NULL;
    wccfile->wcf_wcd.wcd_prev = NULL;
#endif

    while( wcc != NULL ) {
        struct WCCDIR *next;

        next = wcc->wcd_next;
#if DEBUG
        wcc->wcd_next = NULL;
        wcc->wcd_prev = NULL;
        memset(wcc, 0, wcc->size);
#endif
        free(wcc);
        wcc = next;
    }
#if DEBUG
    memset(wccfile, 0, WCFALLOC_SIZE);
#endif
    free( wccfile );

    return;
}

/**************************************************************** do_search() */

/* Function to perform an RMS search... */

static vmscond_t do_search( struct FAB *fab, struct WCCFILE **wccfilep ) {
    struct WCCFILE *wccfile;
    vmscond_t sts;
    struct fibdef fibblk;
    struct WCCDIR *wcc;
    struct dsc_descriptor fibdsc, resdsc;
    struct NAM *nam;

    memset( &fibblk, 0, sizeof(fibblk) );
    memset( &fibdsc, 0, sizeof(fibdsc) );
    memset( &resdsc, 0, sizeof(resdsc) );

    wccfile = *wccfilep;

    if( wccfile == (struct WCCFILE *)-1 )
        return RMS$_NMF;

    nam = fab->fab$l_nam;
    wcc = &wccfile->wcf_wcd;

    if( fab->fab$w_ifi != 0 )
        return RMS$_IFI;

    /* if first time through position at top directory... WCCDIR */

    while( !(wcc->wcd_status & STATUS_INIT) && wcc->wcd_next != NULL) {
        wcc = wcc->wcd_next;
    }
    fibdsc.dsc_w_length = sizeof(struct fibdef);
    fibdsc.dsc_a_pointer = (char *) &fibblk;
    fab->fab__w_verlimit = 0;

    while (TRUE) {
        if( !(wcc->wcd_status & STATUS_INIT) || wcc->wcd_wcc != 0) {
            wcc->wcd_status |= STATUS_INIT;
            resdsc.dsc_w_length = 256 - wcc->wcd_prelen;
            resdsc.dsc_a_pointer = wccfile->wcf_result + wcc->wcd_prelen;

#if DEBUG
            wcc->wcd_sernam[wcc->wcd_serdsc.dsc_w_length] = '\0';
            wccfile->wcf_result[wcc->wcd_prelen + wcc->wcd_reslen] = '\0';
#endif
            memcpy(&fibblk.fib$w_did_num, &wcc->wcd_dirid, sizeof(struct fiddef));
            fibblk.fib$w_nmctl = 0;     /* FIB_M_WILD; */
            fibblk.fib$l_acctl = 0;
            fibblk.fib$w_fid_num = 0;
            fibblk.fib$w_fid_seq = 0;
            fibblk.fib$b_fid_rvn = 0;
            fibblk.fib$b_fid_nmx = 0;
            fibblk.fib$l_wcc = wcc->wcd_wcc;
#if DEBUG
            printf("Ser: '%s' (%d,%d,%d) WCC: %d Prelen: %d '%s'\n",
                   wcc->wcd_sernam,
                   fibblk.fib$w_did_num | (fibblk.fib$b_did_nmx << 16),
                   fibblk.fib$w_did_seq, fibblk.fib$b_did_rvn,
                   wcc->wcd_wcc, wcc->wcd_prelen,
                   wccfile->wcf_result + wcc->wcd_prelen);
#endif
            sts = direct( wccfile->wcf_vcb, &fibdsc, &wcc->wcd_serdsc,
                         &wcc->wcd_reslen, &resdsc, DIRECT_FIND );
        } else {
            sts = SS$_NOMOREFILES;
        }
        if( $SUCCESSFUL(sts) ) {
            fab->fab__w_verlimit = fibblk.fib$w_verlimit; /* Needs a better mechanism** */
#if DEBUG
            wccfile->wcf_result[wcc->wcd_prelen + wcc->wcd_reslen] = '\0';
            printf("Fnd: '%s'  (%d,%d,%d) WCC: %d\n",
                   wccfile->wcf_result + wcc->wcd_prelen,
                   fibblk.fib$w_fid_num | (fibblk.fib$b_fid_nmx << 16),
                   fibblk.fib$w_fid_seq, fibblk.fib$b_fid_rvn,
                   wcc->wcd_wcc);
#endif
            wcc->wcd_wcc = fibblk.fib$l_wcc;
            if( wcc->wcd_prev ) {/* go down directory */
                if (wcc->wcd_prev->wcd_next != wcc) {
                    printf("wcd_prev corruption\n");
                }
                if (fibblk.fib$w_fid_num != 4 || fibblk.fib$b_fid_nmx != 0 ||
                    wcc == &wccfile->wcf_wcd ||
                    memcmp(wcc->wcd_sernam, "000000.", 7) == 0) {
                    memcpy(&wcc->wcd_prev->wcd_dirid, &fibblk.fib$w_fid_num,
                           sizeof(struct fiddef));
                    if (wcc->wcd_next) {
                        wccfile->wcf_result[wcc->wcd_prelen - 1] = '.';
                    }
                    wcc->wcd_prev->wcd_prelen = wcc->wcd_prelen +
                                                wcc->wcd_reslen - 5;
                    wcc = wcc->wcd_prev;        /* go down one level */
                    if (wcc->wcd_prev == NULL) {
                        wccfile->wcf_result[wcc->wcd_prelen - 1] = ']';
                    }
                }
            } else {
                if( nam != NULL ) {
                    /* TODO: is RVN from the header?  If so, the value is zero.  We should return the actual volume of the primary header... */

                    memcpy( &nam->nam$w_fid, &fibblk.fib$w_fid_num,
                           sizeof(struct fiddef) );

                    if( $FAILS(sts = name2nam( nam, wccfile, TRUE)) &&
                        (nam->nam$l_fnb & NAM$M_WILDCARD) ) {

                        nam->nam$l_wcc = NULL;
                        cleanup_wcf( &wccfile );
                        return RMS$_RSS;
                    }
                }
                memcpy( &wccfile->wcf_fid, &fibblk.fib$w_fid_num,
                        sizeof(struct fiddef));
                return SS$_NORMAL;
            }
        } else {
#if DEBUG
            printf("Err: %s\n", getmsg( sts, MSG_TEXT ));
#endif
            if( $MATCHCOND(sts, SS$_BADIRECTORY) ) {
                if (wcc->wcd_next != NULL) {
                    if( wcc->wcd_next->wcd_status & STATUS_INIT ) {
                        sts = SS$_NOMOREFILES;
                    }
                }
            }
            if( $MATCHCOND(sts, SS$_NOMOREFILES) ) {
                wcc->wcd_status &= ~STATUS_INIT;
                wcc->wcd_wcc = 0;
                wcc->wcd_reslen = 0;
                if (wcc->wcd_status & STATUS_TMPDIR) {
                    struct WCCDIR *savwcc = wcc;
                    if (wcc->wcd_next != NULL) {
                        wcc->wcd_next->wcd_prev = wcc->wcd_prev;
                    }
                    if (wcc->wcd_prev != NULL) {
                        wcc->wcd_prev->wcd_next = wcc->wcd_next;
                    }
                    wcc = wcc->wcd_next;
                    if( wcc != NULL )
                        memcpy( wccfile->wcf_result + wcc->wcd_prelen +
                                wcc->wcd_reslen - 6, ".DIR;1", 6 );
                    else
                        abort();
#if DEBUG
                    memset(savwcc, 0, savwcc->size);
#endif
                    free(savwcc);
                } else {
                    if ((wccfile->wcf_status & STATUS_RECURSE) &&
                        wcc->wcd_prev == NULL) {
                        struct WCCDIR *newwcc;
                        newwcc = (struct WCCDIR *) calloc(1, WCDALLOC_SIZE(8));
                        if ( newwcc == NULL ) {
                            if( nam )
                                nam->nam$l_wcc = NULL;
                            cleanup_wcf( &wccfile );
                            return SS$_INSFMEM;
                        }
#if DEBUG
                        newwcc->size = WCDALLOC_SIZE(8);
#endif
                        newwcc->wcd_next = wcc->wcd_next;
                        newwcc->wcd_prev = wcc;
                        newwcc->wcd_wcc = 0;
                        newwcc->wcd_status = STATUS_TMPDIR;
                        newwcc->wcd_reslen = 0;
                        if (wcc->wcd_next != NULL) {
                            wcc->wcd_next->wcd_prev = newwcc;
                        }
                        wcc->wcd_next = newwcc;
                        memcpy(&newwcc->wcd_dirid, &wcc->wcd_dirid,
                               sizeof(struct fiddef));
                        newwcc->wcd_serdsc.dsc_w_length = 7;
                        newwcc->wcd_serdsc.dsc_a_pointer = newwcc->wcd_sernam;
                        memcpy(newwcc->wcd_sernam, "*.DIR;1", 7);
                        newwcc->wcd_prelen = wcc->wcd_prelen;
                        wcc = newwcc;
                    } else {
                        if (wcc->wcd_next != NULL) {
#if DEBUG
                            if (wcc->wcd_next->wcd_prev != wcc) {
                                printf("wcd_NEXT corruption\n");
                            }
#endif
                            wcc = wcc->wcd_next;        /* backup one level */
                            memcpy(wccfile->wcf_result + wcc->wcd_prelen +
                                       wcc->wcd_reslen - 6, ".DIR;1", 6);
                        } else {
                            sts = RMS$_NMF;
                            break;
                            if( (wccfile->wcf_status & STATUS_TMPWCC) ) {
                                cleanup_wcf(&wccfile);
                            } else {
                                cleanup_wcf(&wccfile);
                                wccfile = (struct WCCFILE *)-1;
                            }
                            break;      /* giveup */
                        }
                    }
                }
            } else {
                if( $MATCHCOND(sts, SS$_NOSUCHFILE) ) {
                    if (wcc->wcd_prev) {
                        sts = RMS$_DNF;
                    } else {
                        sts = RMS$_FNF;
                    }
                    break;
                    cleanup_wcf(&wccfile);
                }
                break;
            }
        }
    }
    if( $FAILED(sts) ) {
        cleanup_wcf(&wccfile);

        if( $MATCHCOND(sts, RMS$_NMF) )
            wccfile = (struct WCCFILE *)-1;
    }

    *wccfilep = wccfile;
    if (nam != NULL) {
        nam->nam$l_wcc = wccfile;
    }
    fab->fab$w_ifi = 0;         /* must dealloc memory blocks! */
    return sts;
}

/*************************************************************** sys_search() */

/* External entry for search function... */

vmscond_t sys_search( struct FAB *fab ) {
    struct NAM *nam;
    struct WCCFILE *wccfile;
    vmscond_t sts;

    errno = 0;

    if( fab == NULL )
        return RMS$_IAL;

    nam = fab->fab$l_nam;

    if( nam == NULL )
        return RMS$_NAM;

    wccfile = nam->nam$l_wcc;

    if( wccfile == NULL ) {
        if( $SUCCESSFUL(sts = do_parse( fab, &wccfile )) ) {
            if( wccfile == NULL )
                sts = RMS$_NOVALPRS;
            else {
                if( wccfile->wcf_fnb & NAM$M_WILDCARD ) {
                    cleanup_wcf( &wccfile );
                    return RMS$_NOVALPRS;;
                }
                wccfile->wcf_status |= STATUS_TMPWCC;
            }
        }
    }

    sts = do_search( fab, &wccfile );

    nam->nam$l_wcc = wccfile;

    return sts;
}

/***************************************************************** do_parse() */

#define DEFAULT_SIZE 120
static char  default_buffer[DEFAULT_SIZE];
static char *default_name = "DKA200:[000000].;";
static int   default_size[] = {7, 8, 0, 1, 1};

/* Function to perform RMS parse.... */

static vmscond_t do_parse( struct FAB *fab, struct WCCFILE **wccret ) {
    vmscond_t sts;
    struct WCCFILE *wccfile;
    char *fna;
    char *dna;
    struct NAM *nam;
    int fna_size[5], dna_size[5];
    int field, level, ess = MAX_FILELEN;
    char *esa, *def;
    int ofp;
    struct dsc$descriptor dirdsc;

    memset( &dirdsc, 0, sizeof(dirdsc) );

    if( fab == NULL || fab->fab$w_ifi != 0 ) {
        if( wccret != NULL )
            *wccret = NULL;
        return RMS$_IFI;
    }

    ofp = fab->fab$l_fop & FAB$M_OFP;
    fna = fab->fab$l_fna;
    dna = fab->fab$l_dna;
    nam = fab->fab$l_nam;

    if( nam != NULL && nam->nam$l_wcc != NULL ) {
        struct WCCFILE *wcc;

        wcc = nam->nam$l_wcc;
        cleanup_wcf( &wcc );
        nam->nam$l_wcc = NULL;
    }

    if( wccret != NULL )
        *wccret = NULL;

    /* Break up file specifications... */

    if( $FAILS(sts = name_delim(fna, fab->fab$b_fns, fna_size)) )
        return sts;
    if( dna ) {
        if( $FAILS(sts = name_delim(dna, fab->fab$b_dns, dna_size)) )
            return sts;
    } else {
        memset( dna_size, 0, sizeof( dna_size ) );
    }

    /* Make WCCFILE entry for rest of processing
     * N.B. calloc initializes many fields.
     */

    wccfile = (struct WCCFILE *) calloc(1, WCFALLOC_SIZE);
    if( wccfile == NULL )
        return SS$_INSFMEM;
    wccfile->wcf_fab = fab;

    /* Combine file specifications */
    def = default_name;

    esa = wccfile->wcf_result;
    for( field = 0; field < 5; field++ ) { /* DEV DIR NAM TYP VER */
        char *src;
        int len;

        len = fna_size[field];
        if( len > 0 ) { /* explicit */
            src = fna;
            switch( field ) {
            case 0:
                wccfile->wcf_fnb |= NAM$M_EXP_DEV; break;
            case 1:
                wccfile->wcf_fnb |= NAM$M_EXP_DIR; break;
            case 2:
                wccfile->wcf_fnb |= NAM$M_EXP_NAME; break;
            case 3:
                wccfile->wcf_fnb |= NAM$M_EXP_TYPE; break;
            case 4:
                wccfile->wcf_fnb |= NAM$M_EXP_VER; break;
            }
            if( ofp && nam != NULL ) { /* Process any RLF - only 1 for output */
                struct NAM *rlf;
                int found = FALSE;

                for( rlf = nam->nam$l_rlf;
                     !found && rlf != NULL; rlf = NULL /* rlf->nam$l_rlf */ ) {
                    if( rlf->nam$b_rsl == 0 )
                        continue;

                    switch( field ) {
                    default: /* 0 */
                        break;
                    case 1:
                        if( rlf->nam$b_dir != 0 )
                            found = rlf_dir( &src, &len, rlf );
                        break;
                    case 2:
                        if( len == 1 && src[0] == '*' && rlf->nam$b_name != 0 ) {
                            src = rlf->nam$l_name;
                            len = rlf->nam$b_name;
                            found = TRUE;
                        }
                        break;
                    case 3:
                        if( len == 2 && src[1] == '*' && rlf->nam$b_type != 0 ) {
                            src = rlf->nam$l_type;
                            len = rlf->nam$b_type;
                            found = TRUE;
                        }
                        break;
                    case 4:
                        if( len == 2 && src[1] == '*' && rlf->nam$b_ver != 0 ) {
                            src = rlf->nam$l_ver;
                            len = rlf->nam$b_ver;
                            found = TRUE;
                        }
                        break;
                    }
                }
            }
        } else { /* NULL */
            int found = FALSE;

            if( nam != NULL ) { /* Process any RLF list */
                struct NAM *rlf;

                for( rlf = nam->nam$l_rlf;
                     !found && rlf != NULL; rlf = rlf->nam$l_rlf ) {
                    if( rlf->nam$b_rsl == 0 )
                        continue;

                    switch( field ) {
                    case 0:
                        if( !ofp && (len = rlf->nam$b_dev) ) {
                            src = rlf->nam$l_dev;
                            found = TRUE;
                        }
                        break;
                    case 1:
                        if( !ofp && (len = rlf->nam$b_dir) ) {
                            src = rlf->nam$l_dir;
                            found = TRUE;
                        }
                        break;
                    case 2:
                        if( (len = rlf->nam$b_name) != 0 ) {
                            src = rlf->nam$l_name;
                            found = TRUE;
                        }
                        break;
                    case 3:
                        if( (len = rlf->nam$b_type) != 0 ) {
                            src = rlf->nam$l_type;
                            found = TRUE;
                        }
                        break;
                    default: /* case 4: */
                        break;
                    }
                }
            }
            if( !found ) {
                len = dna_size[field];
                if( len > 0 ) {
                    src = dna;
                } else {
                    len = default_size[field];
                    src = def;
                }
            }
        }
        fna += fna_size[field];
        if( field == 1 ) { /* DIR */
            int dirlen;

            dirlen = len;
            if( len < 3 ) { /* NULL */
                dirlen =
                    len = default_size[1];
                src = def;
            } else {
                char ch1, ch2;

                ch1 = src[1];
                ch2 = src[2];
                if( ch1 == '.' ||
                    (ch1 == '-' &&
                     (ch2 == '-' || ch2 == '.' || ch2 == ']')) ) {
                    char *dir;
                    int count;

                    dir = def;
                    count = default_size[1] - 1;
                    len--;
                    src++;
                    while( len >= 2 && *src == '-' ) { /* UP */
                        len--;
                        src++;
                        if( count < 2 ||
                            (count == 7 && memcmp(dir, "[000000", 7) == 0) ) {
                            cleanup_wcf(&wccfile);
                            return RMS$_DIR;
                        }
                        while( count > 1 ) {
                            if( dir[--count] == '.' )
                                break;
                        }
                    }
                    if( count < 2 && len < 2 ) {
                        src = "[000000]";
                        dirlen = len = 8;
                    } else {
                        if( *src != '.' && *src != ']' ) {
                            cleanup_wcf(&wccfile);
                            return RMS$_DIR;
                        }
                        if( *src == '.' && count < 2 ) {
                            src++;
                            len--;
                        }
                        dirlen = len + count;
                        if( (ess -= count) < 0 ) {
                            cleanup_wcf(&wccfile);
                            return RMS$_ESS;
                        }
                        memcpy(esa, dir, count);
                        esa += count;
                    }
                }
            }
            fna_size[field] = dirlen;
        } else { /* Not dir */
            fna_size[field] = len;
        }
        dna += dna_size[field];
        def += default_size[field];
        if( (ess -= len) < 0 ) {
            cleanup_wcf(&wccfile);
            return RMS$_ESS;
        }
        while( len-- > 0 ) {
            register char ch;

            *esa++ =
                ch = *src++;
            if( ch == '*' || ch == '%' ) {
                wccfile->wcf_fnb |= NAM$M_WILDCARD;
                switch( field ) {
                case 0:
                    cleanup_wcf(&wccfile);
                    return RMS$_DEV;
                case 1:
                    wccfile->wcf_fnb |= NAM$M_WILD_DIR; break;
                case 2:
                    wccfile->wcf_fnb |= NAM$M_WILD_NAME; break;
                case 3:
                    wccfile->wcf_fnb |= NAM$M_WILD_TYPE; break;
                case 4:
                    wccfile->wcf_fnb |= NAM$M_WILD_VER; break;
                }
            }
        }
    }

    sts = SS$_NORMAL;

    /* Now build up WCC structures as required */

    if( nam == NULL || !(nam->nam$b_nop & NAM$M_SYNCHK) ) {
        int dirlen, dirsiz;
        char *dirnam;
        struct WCCDIR *wcc;
        struct DEV *dev;

        if( $FAILS(sts = device_lookup(fna_size[0], wccfile->wcf_result, FALSE, &dev)) ) {
            cleanup_wcf(&wccfile);
            return sts;
        }
        wccfile->wcf_vcb = dev->vcb;
        device_done(dev);
        if( wccfile->wcf_vcb == NULL) {
            cleanup_wcf(&wccfile);
            return SS$_DEVNOTMOUNT;
        }
        wcc = &wccfile->wcf_wcd;

        /* find directory name - chop off '...' if found */

        dirnam = wccfile->wcf_result + fna_size[0] + 1;
        dirlen = fna_size[1] - 2;       /* Don't include [] */
        if (dirlen >= 3) {
            if (memcmp(dirnam + dirlen - 3, "...", 3) == 0) {
                wccfile->wcf_status |= STATUS_RECURSE;
                wccfile->wcf_fnb |= NAM$M_WILDCARD | NAM$M_WILD_DIR;
                dirlen -= 3;
            }
        }
        /* see if we can resolve the directory name in one pass.
         * still need to create all the wcds...
         */

        dirdsc.dsc$w_length = dirlen;
        dirdsc.dsc$a_pointer = dirnam;
        (void) direct_dirid( wccfile->wcf_vcb, &dirdsc, NULL, NULL );

        /* Create directory wcc blocks ... */

        level = 0;
        for( dirsiz = 0; dirsiz < dirlen; ) {
            int seglen = 0;
            char *dirptr;
            struct WCCDIR *wcd;
            int iswild = FALSE;

            dirptr = dirnam + dirsiz;
            do {
                unsigned char ch;

                ch = *dirptr++;
                if( ch == '*' || ch == '%' ) {
                    iswild = TRUE;
                    if( level <= 7 ) {
                        if( iswild )
                            wccfile->wcf_fnb |= NAM$M_WILD_UFD << level;
                        wccfile->wcf_fnb = ( wccfile->wcf_fnb & ~NAM$M_DIR_LVLS) |
                            (level << NAM$V_DIR_LVLS);
                    }
                }
                if( IS_DELIM(ch) ) {
                    if( ch == '.' )
                        ++level;
                    break;
                }
                seglen++;
            } while( dirsiz + seglen < dirlen );

            wcd = (struct WCCDIR *) calloc( 1, WCDALLOC_SIZE((size_t)seglen + 7) );
            if ( wcd == NULL ) {
                cleanup_wcf(&wccfile);
                return SS$_INSFMEM;
            }
#if DEBUG
            wcd->size = WCDALLOC_SIZE(seglen + 7);
#endif
            memcpy(wcd->wcd_sernam, dirnam + dirsiz, seglen);
            memcpy(wcd->wcd_sernam + seglen, ".DIR;1", 7);
            wcd->wcd_serdsc.dsc_w_length = seglen + 6;
            wcd->wcd_serdsc.dsc_a_pointer = wcd->wcd_sernam;
            wcd->wcd_prev = wcc;
            wcd->wcd_next = wcc->wcd_next;
            if (wcc->wcd_next != NULL)
                wcc->wcd_next->wcd_prev = wcd;
            wcc->wcd_next = wcd;
            wcd->wcd_prelen = fna_size[0] + dirsiz + 1; /* Include [ */

            dirdsc.dsc$w_length = dirsiz + seglen;

            if( iswild ) {
                static const struct fiddef mfdid = { FID$C_MFD, FID$C_MFD, 0, 0 };
                if( level == 0 ) {
                    wcd->wcd_dirid = mfdid;
                } else {
                    wcd->wcd_dirid = wcc->wcd_dirid;
                }
            } else {
                vmscond_t dsts;

                if( $FAILS(dsts = direct_dirid( wccfile->wcf_vcb, &dirdsc,
                                                &wcd->wcd_dirid, &wcd->wcd_fid )) &&
                    $SUCCESSFUL(sts) )
                    sts = dsts;
            }

            dirsiz += seglen + 1;
        }

        if( wcc->wcd_next != NULL )
            wcc->wcd_dirid = wcc->wcd_next->wcd_fid;

        wcc->wcd_serdsc.dsc_w_length = fna_size[2] + fna_size[3] + fna_size[4];
        wcc->wcd_serdsc.dsc_a_pointer = wcc->wcd_sernam;
        memcpy(wcc->wcd_sernam, wccfile->wcf_result + fna_size[0] + fna_size[1],
               wcc->wcd_serdsc.dsc_w_length);
#if DEBUG
        wcc->wcd_sernam[wcc->wcd_serdsc.dsc_w_length] = '\0';
        printf("Parse spec is %s\n", wccfile->wcf_wcd.wcd_sernam);
        for (dirsiz = 0; dirsiz < 5; dirsiz++)
            printf( "  %d", fna_size[dirsiz] );
        printf("\n");
#endif
    }

    /* Pass back results... */
    if( nam != NULL ) {
        size_t len;

        nam->nam$b_rsl = 0;
        if( nam->nam$l_esa == NULL ) {
            nam->nam$l_dev =
                nam->nam$l_dir =
                nam->nam$l_name =
                nam->nam$l_type =
                nam->nam$l_ver = NULL;
        } else {
            nam->nam$l_dev  = nam->nam$l_esa;
            nam->nam$l_dir  = nam->nam$l_dev  + fna_size[0];
            nam->nam$l_name = nam->nam$l_dir  + fna_size[1];
            nam->nam$l_type = nam->nam$l_name + fna_size[2];
            nam->nam$l_ver  = nam->nam$l_type + fna_size[3];
        }
        nam->nam$b_dev  = fna_size[0];
        nam->nam$b_dir  = fna_size[1];
        nam->nam$b_name = fna_size[2];
        nam->nam$b_type = fna_size[3];
        nam->nam$b_ver  = fna_size[4];
        len = (size_t)(esa - wccfile->wcf_result);
        if( len > NAM$C_MAXRSS )
            len = NAM$C_MAXRSS;
        nam->nam$b_esl = (uint8_t)len;

        nam->nam$l_fnb = wccfile->wcf_fnb;

        len = nam->nam$b_dev;
        if( len > NAM$S_DVI )
            len = NAM$S_DVI;
        memcpy( nam->nam$t_dvi+1, wccfile->wcf_result, len );
        nam->nam$t_dvi[0] = (uint8_t)len;

        if( (wccfile->wcf_fnb & NAM$M_WILDCARD) || wccfile->wcf_wcd.wcd_next == NULL )
            memset( &nam->nam$w_did, 0, sizeof( struct fiddef ) );
        else
            nam->nam$w_did = wccfile->wcf_wcd.wcd_next->wcd_fid;

        if( nam->nam$l_esa != NULL && nam->nam$b_esl <= nam->nam$b_ess ) {
            memcpy (nam->nam$l_esa, wccfile->wcf_result, nam->nam$b_esl );
        } else {
            if( wccfile->wcf_fnb & NAM$M_WILDCARD ) {
                cleanup_wcf( &wccfile );
                return RMS$_ESS;
            }
        }
    }

    if( nam != NULL ) {
        if( (nam->nam$b_nop & NAM$M_SYNCHK) &&
            !fab->fab$b_dns && !nam->nam$l_rlf )
            cleanup_wcf( &wccfile );

        nam->nam$l_wcc = wccfile;
    }
    if( wccret != NULL )
        *wccret = wccfile;
    return sts;
}

/**************************************************************** rlf_dir() */
static int rlf_dir( char **srcp, int *lenp, struct NAM *rlf ) {
    int len;
    char *src;

    src = *srcp;
    len = *lenp;

    /* OFP: Substitute wildcarded src directory with RLF field(s).
     *
     * src[0] and src[len-1] have delimiters - [] or <>
     * subdirectory components are known to be <= 39 characters.
     * rlf->nam$b_dir is != 0
     *
     * IFF a substitution is made, data will be copied to expanded spec
     * immediately upon return.  Thus, we can use a static buffer.
     *
     static char xpndir[MAX_FILELEN];
     * return TRUE if a substitution is made.
     */

    if( (len == 3 && src[1] == '*') ||
        (len == 6 && (src[1] == '*' &&
                      src[2] == '.' &&
                      src[3] == '.' &&
                      src[4] == '.')) ) {
        *srcp = rlf->nam$l_dir;
        *lenp = rlf->nam$b_dir;
        return TRUE;
    }
    /* TODO - rules for xxx..., xxx.*, and xxx.*.*
     *
     * The specification for this isn't clear & may need experiments under VMS.
     */
    /* From "Guide to OpenVMS File Applications"
      o Only the asterisk and the ellipsis are permitted in the output directory
      specification. In the case of a related file specification, you may
      choose either the asterisk or the ellipsis (but not both) in the output
      directory specification unless you use the following form:

          [*...]
      o A subdirectory specification that contains wildcard characters cannot be
      followed by a subdirectory specification that does not contain wildcard
      characters.

      o Specifications in the UIC directory format may receive defaults only from
      directories in the UIC directory format.

      o Specifications in the non-UIC directory format may receive defaults only
      from directories in the non-UIC directory format.

      o Specifications in the non-UIC directory format that consist entirely of
      wildcard characters may receive related file specification defaults from
      directories in UIC or non-UIC format.

      RMS processes wildcard characters in an output directory specification as follows:

      o If you specify an output directory using a specification that consists
      entirely of wildcard characters (only [*] and [*...] are allowed), RMS
      accepts the complete directory component from the related file
      specification. This permits you to duplicate an entire directory
      specification.

      o If you specify an output directory with a trailing asterisk (for
      example, [A.B.*]), RMS substitutes the first "wild" subdirectory from
      the related file specification. This substitution permits you to move
      files from one directory tree to another directory tree that is not as
      deep as the first one.

      o If you specify an output directory with a trailing ellipsis (for
      example, [A.B...]), RMS substitutes the entire "wild" subdirectory from
      the related file specification. This substitution permits you to move
      entire subdirectory trees.

      o The related name block must have the appropriate file name status bits
      set in the NAM$L_FNB or NAML$L_FNB field set according to the resultant
      string to allow RMS to identify the "wild" portion of the resultant string.

      ---
      Trailing '*' are replaced substituted from RLF starting with first wildcard.
      '...' replaced by input, starting with first RLF with wildcard
      ignored if no subdirectory in RLF.
    */

    return FALSE;
}

/**************************************************************** sys_parse() */

/* External entry for parse function... */

vmscond_t sys_parse( struct FAB *fab ) {
    struct NAM *nam;
    vmscond_t sts;

    errno = 0;

    if( fab == NULL )
        return RMS$_IAL;

    nam = fab->fab$l_nam;
    if( nam == NULL )
        return RMS$_NAM;

    sts = do_parse( fab, NULL );

    memset( &nam->nam$w_fid, 0, sizeof( nam->nam$w_fid ) );

    return sts;
}

/************************************************************** sys_setddir() */

/* Function to set default directory (heck we can sneak in the device...)  */

vmscond_t sys_setddir( struct dsc_descriptor *newdir,
                       uint16_t *oldlen, struct dsc_descriptor *olddir ) {
    vmscond_t sts = SS$_NORMAL;

    errno = 0;

    if( olddir != NULL ) {
        int retlen;

        retlen = default_size[0] + default_size[1];
        if( retlen > olddir->dsc_w_length ) {
            retlen = olddir->dsc_w_length;
            sts = $SHRMSG(SS$_RESULTOVF,RMS$,WARNING);
        }
        if( oldlen != NULL )
            *oldlen = retlen;
        memcpy( olddir->dsc_a_pointer, default_name, retlen );
    }
    if( newdir != NULL ) {
        struct FAB fab = cc$rms_fab;
        struct NAM nam = cc$rms_nam;

        fab.fab$l_nam = &nam;
        nam.nam$b_nop |= NAM$M_SYNCHK;
        nam.nam$b_ess = DEFAULT_SIZE;
        nam.nam$l_esa = default_buffer;
        fab.fab$b_fns = (uint8_t)newdir->dsc_w_length;
        fab.fab$l_fna = newdir->dsc_a_pointer;

        if( $SUCCESSFUL(sts = sys_parse(&fab)) ) {
            if( nam.nam$l_fnb & (NAM$M_EXP_NAME | NAM$M_EXP_TYPE | NAM$M_EXP_VER) ) {
                sts = RMS$_DIR;
            } else {
                if( nam.nam$l_fnb & NAM$M_WILDCARD )
                    sts = RMS$_WLD;
                else {
                    default_name = default_buffer;
                    default_size[0] = nam.nam$b_dev;
                    default_size[1] = nam.nam$b_dir;
                    memcpy( default_name + nam.nam$b_dev + nam.nam$b_dir, ".;", 3 );
                }
            }
        }
        fab.fab$b_fns =
            nam.nam$b_ess =
            fab.fab$b_dns = 0;
        nam.nam$l_rlf = NULL;
        nam.nam$b_nop = NAM$M_SYNCHK;
        (void) sys_parse( &fab );                    /* Release context */
    }
    return sts;
}

#define IFI_MAX 63
static struct WCCFILE *ifi_table[IFI_MAX+1] = {
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
};

/************************************************************** sys_connect() */

/* This version of connect only resets record pointer and does
 * the bookeeping to allow detection of various programming
 * errors.  It also allows close to find and disconnect all
 * active streams, which MUST be in scope.  This implementation
 * of isi differs from VMS in that it uses a uint32_t bitmask to identify
 * the stream.  This limits the implementation to 32 active streams per fab.
 * As we only support sequential files, that should be more than enough.
 * (1 is expected/normal...)
 */

vmscond_t sys_connect( struct RAB *rab ) {
    unsigned int ifi_no;
    struct FCB *fcb;
    struct FAB *fab;

    errno = 0;

    if( rab == NULL )
        return RMS$_IAL;

    if( (fab = rab->rab$l_fab) == NULL ||
        (ifi_no = fab->fab$w_ifi) == 0 || ifi_no > IFI_MAX )
        return RMS$_CCR;

    if( rab->rab$w_isi != 0 )
        return RMS$_BUSY;

    if( rab->rab$l_rop & RAB$M_EOF ) {
        uint32_t block;

        fcb = ifi_table[ifi_no]->wcf_fcb;

        block = F11SWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
        rab->rab$w_rfa[0] = block & 0xffff;
        rab->rab$w_rfa[1] = block >> 16;
        rab->rab$w_rfa[2] = F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte);
    } else {
        rab->rab$w_rfa[0] = 0;
        rab->rab$w_rfa[1] = 0;
        rab->rab$w_rfa[2] = 0;
    }
    rab->rab$w_rsz = 0;

    if( fab->fab$b_org == FAB$C_SEQ ) {
        uint32_t isi;

        isi = fab->fab$l_isi & (1+~fab->fab$l_isi);
        if( isi == 0 )
            return RMS$_ISI;
        rab->rab$w_isi = isi;
        fab->fab$l_isi &= ~isi;
        rab->rab$a__next = fab->fab$a__rab;
        fab->fab$a__rab = rab;
        return SS$_NORMAL;
    }
    return ODS2_BADORG;
}

/*********************************************************** sys_disconnect() */

vmscond_t sys_disconnect( struct RAB *rab ) {
    uint32_t isi;
    struct RAB **p;
    struct FAB *fab;

    errno = 0;

    if( rab == NULL || (fab = rab->rab$l_fab) == NULL )
        return RMS$_RAB;
    if( (isi = rab->rab$w_isi) == 0 )
        return RMS$_STR;

    for( p = &fab->fab$a__rab; *p != NULL; p = &(*p)->rab$a__next ) {

        if( *p == rab ) {
            *p = rab->rab$a__next;
            rab->rab$a__next = NULL;

            if( ((isi & (1+~isi)) ^ isi) || (fab->fab$l_isi & isi) )
                return RMS$_STR;

            fab->fab$l_isi |= isi;
            rab->rab$w_isi = 0;

            return SS$_NORMAL;
        }
    }
    return RMS$_STR;
}

/****************************************************************** sys_get() */

/* get for sequential files */

vmscond_t sys_get( struct RAB *rab ) {
    char *buffer, *recbuff;
    uint32_t block = 0, blocks = 0, offset = 0,
        cpylen = 0, reclen = 0, fullreclen = 0,
        delim = 0, rfm = 0, sts = 0;
    struct VIOC *vioc;
    struct FCB *fcb;
    struct FAB *fab;
    int span = 0, dellen = 0, rfmvar = 0;

    errno = 0;

    if( rab == NULL )
        return RMS$_RAB;
    if( (fab = rab->rab$l_fab) == NULL )
        return RMS$_IAL;

    if( fab->fab$w_ifi == 0 || fab->fab$w_ifi > IFI_MAX )
        return RMS$_IFI;

    fcb = ifi_table[fab->fab$w_ifi]->wcf_fcb;

    reclen = rab->rab$w_usz;
    recbuff = rab->rab$l_ubf;
    rab->rab$l_rbf = recbuff; /* Locate mode not implemented */
    rab->rab$l_stv = 0;
    offset = rab->rab$w_rfa[2] % 512;
    block = ((rab->rab$w_rfa[1] << 16) + rab->rab$w_rfa[0]) + (rab->rab$w_rfa[2] / 512);
    if (block == 0)
        block = 1;

    span = (fab->fab$b_rat & FAB$M_BLK) == 0;

    rfmvar = FALSE;
    delim = 0;
    fullreclen = 0;
    switch( (rfm = fab->fab$b_rfm) ) {
    case FAB$C_STMLF:
        delim = 1;
        span = 1;
        break;
    case FAB$C_STMCR:
        delim = 2;
        span = 1;
        break;
    case FAB$C_STM:
        delim = 3;
        span = 1;
        break;
    case FAB$C_VFC:
        reclen += fab->fab$b_fsz;
        rfmvar = TRUE;
        offset &= ~1;
        break;
    case FAB$C_VAR:
        rfmvar = TRUE;
        offset &= ~1;
        break;
    case FAB$C_FIX:
        if (reclen < fab->fab$w_mrs) {
            rab->rab$l_stv = fab->fab$w_mrs;
            return RMS$_RTB;
        }
        reclen = fab->fab$w_mrs;
        offset &= ~1;
        break;
    }

    rab->rab$w_rsz = 0;

    do {
        uint32_t eofblk, endblk, endofs;

        eofblk = F11SWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
        if( block > eofblk || (block == eofblk &&
                               offset >= F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte)) ) {
            return RMS$_EOF;
        }
        endofs = offset + reclen;
        endblk = block + (endofs / 512);
        endofs %= 512;
        if( endblk > eofblk || (endblk == eofblk &&
                                endofs > F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte)) ) {
            uint32_t excess;
            excess = (endblk - eofblk) * 512;
            if( endofs > F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte) )
                excess += endofs - F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte);
            else
                excess -= F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte) - endofs;
            reclen -= excess;
        }

        if( $FAILS(sts = accesschunk( fcb, block, &vioc, &buffer, &blocks, 0 )) ) {
            if( $MATCHCOND(sts, SS$_ENDOFFILE) )
                sts = RMS$_EOF;
            return sts;
        }

        if( rfmvar ) {
            do {
                f11word *lenptr = (f11word *) (buffer + offset);

                fullreclen =
                    reclen = F11WORD(*lenptr);
                offset += 2;
                if( !span && offset + reclen > 510 ) { /* N.B. Length words 0xFFFF are used for fill */
                    block++;
                    offset = 0;
                    blocks--;
                    buffer += 512;
                } else
                    span = 1;
            } while( !span && blocks > 0 &&
                     (block < eofblk ||
                      (block == eofblk && fcb->head->fh2$w_recattr.fat$w_ffbyte > 0))  );
            if( !span ) {
                sts = deaccesschunk( vioc, 0, 0, FALSE );
                continue;
            }
            if( reclen > rab->rab$w_usz ) {
                sts = deaccesschunk(vioc, 0, 0, FALSE);
                rab->rab$l_stv = fullreclen;
                return RMS$_RTB;
            }
        }
    } while( !span );

    cpylen = 0;
    dellen = 0;

    while( TRUE ) {
        uint32_t seglen;

        seglen = (blocks * 512) - offset;

        if( seglen > 0 ) {
            if (delim) {
                char *ptr;

                ptr = buffer + offset;
                if (delim >= 3) {
                    if (dellen == 1 && *ptr != '\n') {
                        if (cpylen >= reclen) {
                            seglen = 0;
                            fullreclen++;
                            sts = RMS$_RTB;
                        } else {
                            *recbuff++ = '\r';
                            cpylen++;
                        }
                    }
                    while (seglen > 0) {
                        char ch;
                        ch = *ptr++;
                        --seglen;
                        if( ch == '\n' || ch == '\f' || ch == '\v' || ch == '\033' ) {
                            if (ch == '\n') {
                                if( ptr - (buffer + offset) < ++dellen )
                                    dellen = (int)(ptr - (buffer + offset));
                            } else {
                                dellen = 0;
                            }
                            delim = 99;
                            break;
                        }
                        dellen = (ch == '\r')? 1: 0;
                    }
                    if( !(fab->fab$b_rat & (FAB$M_CR | FAB$M_FTN)) )
                        dellen = 0;

                    seglen = (int)((ptr - (buffer + offset)) - dellen);
                    fullreclen += seglen;
                } else {
                    char term ;

                    term = (delim == 1)? '\n': '\r';

                    while (seglen > 0) {
                        --seglen;
                        if( *ptr++ == term ) {
                            dellen = 1;
                            delim = 99;
                            break;
                        }
                    }
                    if( !(fab->fab$b_rat & (FAB$M_CR | FAB$M_FTN)) )
                        dellen = 0;
                    seglen = (int)((ptr - (buffer + offset)) - dellen);
                    fullreclen += seglen;
                }
            } else {
                if( seglen > reclen - cpylen )
                    seglen = reclen - cpylen;
                if( rfm == FAB$C_VFC && cpylen < fab->fab$b_fsz ) {
                    uint32_t fsz;

                    fsz = fab->fab$b_fsz - cpylen;
                    if( fsz > seglen )
                        fsz = seglen;
                    if (rab->rab$l_rhb) {
                        memcpy( rab->rab$l_rhb + cpylen, buffer + offset, fsz );
                    }
                    cpylen += fsz;
                    offset += fsz;
                    seglen -= fsz;
                }
            }
            if( seglen ) {
                if (cpylen + seglen > reclen) {
                    seglen = reclen - cpylen;
                    sts = RMS$_RTB;
                }
                memcpy(recbuff, buffer + offset, seglen);
                recbuff += seglen;
                cpylen += seglen;
            }
            offset += seglen + dellen;
            if( (offset & 1) && (rfmvar || rfm == FAB$C_FIX) )
                offset++;
        }

        { vmscond_t sts2;
            if( $FAILS(sts2 = deaccesschunk( vioc, 0, 0, TRUE )) )
                sts = sts2;
        }
        if( $FAILED(sts) )
            break;
        block += offset / 512;
        offset %= 512;
        if ((delim == 0 && cpylen >= reclen) || delim == 99) {
            break;
        } else {
            if( $FAILS(sts = accesschunk( fcb, block, &vioc, &buffer, &blocks, 0 ) ) ) {
                if( $MATCHCOND(sts, SS$_ENDOFFILE ) )
                    sts = RMS$_EOF;
                if( $MATCHCOND(sts, RMS$_EOF) && cpylen!= 0 ) {
                    sts = SS$_NORMAL;
                    break;
                }
                return sts;
            }
            offset = 0;
        }
    }
    if( rfm == FAB$C_VFC )
        cpylen -= fab->fab$b_fsz;
    rab->rab$w_rsz = cpylen;

    rab->rab$w_rfa[0] = block & 0xffff;
    rab->rab$w_rfa[1] = block >> 16;
    rab->rab$w_rfa[2] = offset;

    if( $MATCHCOND(sts, RMS$_RTB) ) {
        if( rfm == FAB$C_UDF )
            sts = SS$_NORMAL;
        else
            rab->rab$l_stv = fullreclen;
    }
    return sts;
}

/****************************************************************** sys_put() */

/* put for sequential files */

vmscond_t sys_put( struct RAB *rab ) {
    char *buffer, *recbuff;
    uint32_t block = 0, blocks = 0, offset = 0, hiblock = 0, hioffset = 0;
    uint32_t cpylen = 0, reclen = 0, rfmvar = 0;
    uint32_t delim = 0, rfm = 0;
    vmscond_t sts = 0;
    struct VIOC *vioc;
    struct FCB *fcb;
    struct FAB *fab;

    errno = 0;

    if( rab == NULL )
        return RMS$_RAB;
    if( (fab = rab->rab$l_fab) == NULL )
        return RMS$_IAL;

    if( fab->fab$w_ifi == 0 || fab->fab$w_ifi > IFI_MAX )
        return RMS$_IFI;

    fcb = ifi_table[fab->fab$w_ifi]->wcf_fcb;

    if( fcb->head->fh2$l_filechar & F11LONG(FH2$M_DIRECTORY) )
        return ODS2_ILLDIRWRT;

    reclen = rab->rab$w_rsz;
    recbuff = rab->rab$l_rbf;
    delim = 0;
    blocks = reclen;

    block = F11SWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
    offset = F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte);

    rfmvar = FALSE;
    switch( rfm = fab->fab$b_rfm ) {
    case FAB$C_STMLF:
        if( reclen < 1 ) {
            delim = 1;
        } else {
            if (recbuff[reclen-1] != '\n')
                delim = 1;
        }
        break;
    case FAB$C_STMCR:
        if( reclen < 1 ) {
            delim = 2;
        } else {
            if( recbuff[reclen-1] != '\r' )
                delim = 2;
        }
        break;
    case FAB$C_STM:
        if( reclen < 2 ) {
            delim = 3;
        } else {
            if( recbuff[reclen-2] != '\r' || recbuff[reclen-1] != '\n' ) {
                delim = 3;
            }
        }
        break;
    case FAB$C_VAR:
        blocks += 2;
        rfmvar = TRUE;
        offset += offset & 1;
        break;
    case FAB$C_VFC:
        reclen += fab->fab$b_fsz;
        blocks = reclen + 2;
        rfmvar = TRUE;
        offset += offset & 1;
        break;
    case FAB$C_FIX:
        if (reclen != fab->fab$w_mrs)
            return RMS$_RSZ;
        offset += offset & 1;
        break;
    }
    if( delim && !(fab->fab$b_rat & (FAB$M_CR|FAB$M_FTN)) )
        delim = 0;

    block += offset / 512;
    offset %= 512;

    blocks += (delim >= 3)? 2: delim? 1 : 0;

    hiblock = block + ((offset + blocks) / 512);
    hioffset = ((offset + blocks) % 512);

    if( (hiblock > F11SWAP( fcb->head->fh2$w_recattr.fat$l_hiblk )) ||
        (hiblock == F11SWAP( fcb->head->fh2$w_recattr.fat$l_hiblk ) &&
         (hioffset != 0) ) ) {
        blocks = hiblock - F11SWAP( fcb->head->fh2$w_recattr.fat$l_hiblk );
        blocks += (hioffset != 0);

        if( $FAILS(sts = update_extend( fcb, blocks, 0 )) )
            return sts;
    }

    if( $FAILS(sts = accesschunk( fcb, block, &vioc, &buffer, &blocks, 1 )) )
        return sts;

    if( rfmvar ) {
        f11word *lenptr;

        lenptr = (f11word *) (buffer + offset);
        *lenptr = F11WORD(reclen);
        offset += 2;
    }

    cpylen = 0;
    while (TRUE) {
        uint32_t seglen;

        seglen = blocks * 512 - offset;

        if( seglen > reclen - cpylen )
            seglen = reclen - cpylen;
        if( rfm == FAB$C_VFC && cpylen < fab->fab$b_fsz ) {
            uint32_t fsz;

            fsz = fab->fab$b_fsz - cpylen;

            if( fsz > seglen )
                fsz = seglen;
            if( rab->rab$l_rhb ) {
                memcpy( buffer + offset, rab->rab$l_rhb + cpylen, fsz );
            } else {
                memset( buffer + offset, 0, fsz );
            }
            cpylen += fsz;
            offset += fsz;
            seglen -= fsz;
        }
        if( seglen ) {
            memcpy( buffer + offset, recbuff, seglen );
            recbuff += seglen;
            cpylen += seglen;
            offset += seglen;
        }
        if( offset < blocks * 512 ) {
            switch( delim ) {
            case 1:
                buffer[offset++] = '\n';
                delim = 0;
                break;
            case 2:
                buffer[offset++] = '\r';
                delim = 0;
                break;
            case 3:
                buffer[offset++] = '\r';
                if( offset < blocks * 512 ) {
                    buffer[offset++] = '\n';
                    delim = 0;
                } else {
                    delim = 1;
                }
                break;
            default:
                if( rfmvar &&
                    ( (blocks * 512) > (offset + 2 + (offset & 1)) ) ) {
                    buffer[(offset|1)+0] = -1;
                    buffer[(offset|1)+1] = -1;
                }
            }
        }

        if( $FAILS(sts = deaccesschunk( vioc, block, blocks, TRUE )) )
            break;
        fcb->status |= FCB_WRITTEN;

        if( cpylen >= reclen && delim == 0 ) {
            break;
        }
        block += blocks;
        offset = 0;

        if( $FAILS(sts = accesschunk( fcb, block, &vioc, &buffer, &blocks, 1 )) )
            break;
    }
    if( (offset & 1) && (rfmvar || rfm == FAB$C_FIX) )
        offset++;
    block += offset / 512;
    offset %= 512;
    hiblock = F11SWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
    hioffset = F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte);
    hiblock += hioffset / 512;
    hioffset %= 512;
    if( block > hiblock || (block == hiblock && offset > hioffset) ) {
        fcb->head->fh2$w_recattr.fat$l_efblk = F11SWAP(block);
        fcb->head->fh2$w_recattr.fat$w_ffbyte = F11WORD(offset);
    }
    if( rfmvar &&
        (reclen = rab->rab$w_rsz) > F11WORD(fcb->head->fh2$w_recattr.fat$w_rsize) )
        fcb->head->fh2$w_recattr.fat$w_rsize = F11WORD(reclen);
    rab->rab$w_rfa[0] = block & 0xffff;
    rab->rab$w_rfa[1] = block >> 16;
    rab->rab$w_rfa[2] = offset;

    return sts;
}

/************************************************************** sys_display() */

/* display to fill fab & xabs with info from the file header... */

vmscond_t sys_display( struct FAB *fab ) {
    struct WCCFILE *wccfile;
    struct XABDAT *xab;
    struct NAM *nam;
    struct HEAD *head;
    uint16_t *pp;
    struct IDENT *id;
    unsigned int ifi_no;
    f11long fch;
    vmscond_t sts;

    errno = 0;

    if( fab == NULL )
        return RMS$_IAL;

    ifi_no = fab->fab$w_ifi;

    if( ifi_no == 0 || ifi_no > IFI_MAX )
        return RMS$_IFI;

    wccfile = ifi_table[ifi_no];

    sts = SS$_NORMAL;

    head = wccfile->wcf_fcb->head;
    pp = (uint16_t *) head;
    id = (struct IDENT *) (pp + head->fh2$b_idoffset);

    fab->fab$l_alq = F11SWAP(head->fh2$w_recattr.fat$l_hiblk);
    fab->fab$b_bks = head->fh2$w_recattr.fat$b_bktsize;
    fab->fab$w_deq = F11WORD(head->fh2$w_recattr.fat$w_defext);
    fab->fab$b_fsz = head->fh2$w_recattr.fat$b_vfcsize;
    fab->fab$w_gbc = F11WORD(head->fh2$w_recattr.fat$w_gbc);
    fab->fab$w_mrs = F11WORD(head->fh2$w_recattr.fat$w_maxrec);
    fab->fab$b_org = head->fh2$w_recattr.fat$b_rtype & 0xf0;
    fab->fab$b_rfm = head->fh2$w_recattr.fat$b_rtype & 0x0f;
    fab->fab$b_rat = head->fh2$w_recattr.fat$b_rattrib;

    fch = F11LONG(head->fh2$l_filechar);
    fab->fab$l_fop &= ~(FAB$M_CTG|FAB$M_CBT|FAB$M_RCK|FAB$M_WCK);
    if( fch & FH2$M_CONTIG )
        fab->fab$l_fop |= FAB$M_CTG;
    if( fch & FH2$M_CONTIGB )
        fab->fab$l_fop |= FAB$M_CBT;
    if( fch & FH2$M_READCHECK )
        fab->fab$l_fop |= FAB$M_RCK;
    if( fch & FH2$M_WRITECHECK )
        fab->fab$l_fop |= FAB$M_WCK;

    nam = fab->fab$l_nam;

    if( nam != NULL ) {
        nam->nam$w_did.fid$w_num = F11WORD(head->fh2$w_backlink.fid$w_num);
        nam->nam$w_did.fid$w_seq = F11WORD(head->fh2$w_backlink.fid$w_seq);
        nam->nam$w_did.fid$b_nmx = head->fh2$w_backlink.fid$b_rvn;
        nam->nam$w_did.fid$b_nmx = head->fh2$w_backlink.fid$b_nmx;
        sts = name2nam( nam, wccfile, TRUE );
        if( !(wccfile->wcf_status & STATUS_BYFID) && nam->nam$l_dev != NULL ) {
            size_t len;

            len = nam->nam$b_dev;
            if( len > NAM$S_DVI )
                len = NAM$S_DVI;
            memcpy( nam->nam$t_dvi+1, nam->nam$l_dev, len );
            nam->nam$t_dvi[0] = (uint8_t)len;
        }
    }

    for( xab = fab->fab$l_xab; xab != NULL; xab = xab->xab$l_nxt ) {
        switch (xab->xab$b_cod) {
        case XAB$C_ALL: {
                struct XABALL *all;

                all = (struct XABALL *) xab;

                all->xab$w_deq = fab->fab$w_deq;
                all->xab$l_alq = fab->fab$l_alq;
            }
                break;
        case XAB$C_DAT:
            memcpy(&xab->xab$q_bdt, id->fi2$q_bakdate,
                   sizeof(id->fi2$q_bakdate));
            memcpy(&xab->xab$q_cdt, id->fi2$q_credate,
                   sizeof(id->fi2$q_credate));
            memcpy(&xab->xab$q_edt, id->fi2$q_expdate,
                   sizeof(id->fi2$q_expdate));
            memcpy(&xab->xab$q_rdt, id->fi2$q_revdate,
                   sizeof(id->fi2$q_revdate));
            xab->xab$w_rvn = F11WORD( id->fi2$w_revision );
            break;
        case XAB$C_RDT: {
            struct XABRDT *rdt;

            rdt = (struct XABRDT *)xab;
            memcpy(&rdt->xab$q_rdt, id->fi2$q_revdate,
                   sizeof(id->fi2$q_revdate));
            xab->xab$w_rvn = F11WORD( id->fi2$w_revision );
        }
            break;
        case XAB$C_FHC:{
            struct XABFHC *fhc;

            fhc = (struct XABFHC *) xab;

            fhc->xab$b_atr = head->fh2$w_recattr.fat$b_rattrib;
            fhc->xab$b_bkz = head->fh2$w_recattr.fat$b_bktsize;
            fhc->xab$w_dxq = F11WORD(head->fh2$w_recattr.fat$w_defext);
            fhc->xab$l_ebk = F11SWAP(head->fh2$w_recattr.fat$l_efblk);
            fhc->xab$w_ffb = F11WORD(head->fh2$w_recattr.fat$w_ffbyte);
            fhc->xab$w_gbc = F11WORD(head->fh2$w_recattr.fat$w_gbc);
            fhc->xab$l_hbk = F11SWAP(head->fh2$w_recattr.fat$l_hiblk);
            fhc->xab$b_hsz = head->fh2$w_recattr.fat$b_vfcsize;
            fhc->xab$w_lrl = F11WORD(head->fh2$w_recattr.fat$w_rsize);
            fhc->xab$w_verlimit = (head->fh2$l_filechar & F11LONG( FH2$M_DIRECTORY ))?
                F11WORD(head->fh2$w_recattr.fat$w_versions): fab->fab__w_verlimit;
        }
            break;
        case XAB$C_PRO:{
            struct XABPRO *pro;

            pro = (struct XABPRO *) xab;

            pro->xab$w_pro = F11WORD(head->fh2$w_fileprot);
            memcpy( &pro->xab$l_uic, &head->fh2$l_fileowner, 4 );
        }
            break;
        case XAB$C_ITM:{
            struct XABITM *itm;
            struct item_list *list;

            itm = (struct XABITM *) xab;

            if( itm->xab$b_mode != XAB$K_SENSEMODE )
                break;

            for( list = itm->xab$l_itemlist;
                 list->code != 0 || list->length != 0; list++ ) {
                assert( list->retlen != NULL );
                *(list->retlen) = sizeof(int);
                switch( list->code ) {
                case XAB$_UCHAR_NOBACKUP:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_NOBACKUP) != 0; break;
                case XAB$_UCHAR_WRITEBACK:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_WRITEBACK) != 0;   break;
                case XAB$_UCHAR_READCHECK:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_READCHECK) != 0;   break;
                case XAB$_UCHAR_WRITECHECK:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_WRITECHECK) != 0;   break;
                case XAB$_UCHAR_CONTIGB:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_CONTIGB) != 0;   break;
                case XAB$_UCHAR_LOCKED:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_LOCKED) != 0;   break;
                case XAB$_UCHAR_CONTIG:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_CONTIG) != 0;    break;
                case XAB$_UCHAR_SPOOL:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_SPOOL) != 0;   break;
                case XAB$_UCHAR_DIRECTORY:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_DIRECTORY) != 0; break;
                case XAB$_UCHAR_BADBLOCK:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_BADBLOCK) != 0;   break;
                case XAB$_UCHAR_MARKDEL:
                    *((uint32_t *)list->buffer) = (fch & FH2$M_MARKDEL) != 0;   break;
                default:
                    list->retlen = 0;
                    break;
                }
            }
        }
            break;
        default:
            break;
        }
    }

    return sts;
}

/**************************************************************** sys_close() */

/* close a file */

vmscond_t sys_close( struct FAB *fab ) {
    vmscond_t sts;
    unsigned int ifi_no;
    struct FCB *fcb;
    struct WCCFILE *wccfile;
    struct XABRDT *rdt;
    struct IDENT *id = NULL;
    int rdtset = FALSE;
    struct RAB *rab;

    errno = 0;

    if( fab == NULL )
        return RMS$_IAL;

    ifi_no = fab->fab$w_ifi;
    if( ifi_no == 0 || ifi_no > IFI_MAX ||
        (wccfile = ifi_table[ifi_no]) == NULL )
        return RMS$_IFI;

    fcb = wccfile->wcf_fcb;

    if( fcb->head->fh2$b_mpoffset > fcb->head->fh2$b_idoffset &&
        ((size_t)(fcb->head->fh2$b_mpoffset - fcb->head->fh2$b_idoffset) >=
         offsetof( struct IDENT, fi2$t_filenamext)/2) ) {
        id = ((struct IDENT *) (((f11word *)fcb->head) +
                                fcb->head->fh2$b_idoffset));
    }

    sts = SS$_NORMAL;

    if( fcb->status & FCB_RDTSET )
        rdtset = TRUE;

    for( rdt = fab->fab$l_xab;
         (fcb->status & FCB_WRITE) && rdt != NULL; rdt = rdt->xab$l_nxt ) {
        switch( rdt->xab$b_cod ) {
        case XAB$C_RDT:
            if( id != NULL ) {
                if( memcmp( &rdt->xab$q_rdt, &timenotset, sizeof( VMSTIME ) ) ) {
                    memcpy( id->fi2$q_revdate, &rdt->xab$q_rdt,
                            sizeof( VMSTIME ) );
                    if( rdt->xab$w_rvn != 0 )
                        id->fi2$w_revision = F11WORD( rdt->xab$w_rvn );
                    rdtset = TRUE;
                } else {
                    memcpy( &rdt->xab$q_rdt, id->fi2$q_revdate,
                            sizeof( VMSTIME ) );
                }
                rdt->xab$w_rvn = F11WORD( id->fi2$w_revision );
            }
            break;
            case XAB$C_PRO: {
                struct XABPRO *pro;

                pro = (struct XABPRO *) rdt;
                fcb->head->fh2$w_fileprot = F11WORD( pro->xab$w_pro );
                memcpy( &fcb->head->fh2$l_fileowner, &pro->xab$l_uic, 4 );
            }
                break;
        }
    }

    if( id != NULL && !rdtset &&
        (fcb->status & (FCB_WRITE | FCB_WRITTEN)) == (FCB_WRITE | FCB_WRITTEN) ) {

        sys_gettim( id->fi2$q_revdate );
        if( id->fi2$w_revision != (f11word)~0u )
            ++id->fi2$w_revision;
    }
    /* At one point, I did this so that files initial creation would have revision
     * 1.  This turns out to be wrong in the real world. Further, this can corruption:
     * it can change the header of a file not open for write, and if cached, create
     * a checksum error since the header won't be re-written.  We could check for
     * FCB_WRITE, but since revison is allowed to be zero, I we'll allow it.
     * else if( id != NULL && id->fi2$w_revision == 0 )
     *  id->fi2$w_revision = 1;
     */
    fcb->status &= ~FCB_WRITTEN;

    if( (fab->fab$l_openfop | fab->fab$l_fop) & FAB$M_DLT ) {
        if( $SUCCESSFUL(sts = accesserase( fcb->vcb, &fcb->head->fh2$w_fid )) ) {
            struct fibdef fibblk;
            struct dsc_descriptor fibdsc, serdsc;

            memset( &fibdsc, 0, sizeof( fibdsc ) );
            memset( &serdsc, 0, sizeof( serdsc ) );
            memset( &fibblk, 0, sizeof( fibblk ) );
            fibdsc.dsc_w_length = sizeof(struct fibdef);
            fibdsc.dsc_a_pointer = (char *) &fibblk;

            serdsc.dsc_w_length = wccfile->wcf_wcd.wcd_reslen;
            serdsc.dsc_a_pointer = wccfile->wcf_result +
                wccfile->wcf_wcd.wcd_prelen;

            memcpy( &fibblk.fib$w_did_num, &wccfile->wcf_wcd.wcd_dirid,
                    sizeof(struct fiddef));
            sts = direct( wccfile->wcf_vcb, &fibdsc, &serdsc,
                          NULL, NULL, DIRECT_DELETE ) ;
        }
    }

    if( fab->fab$l_fop & FAB$M_TEF ) {
        uint32_t eofblk;

        eofblk = F11SWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
        if( eofblk == 0 && F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte) < 512 ) {
            eofblk = 0;
            fcb->head->fh2$w_recattr.fat$l_efblk = F11SWAP( 1 );
            fcb->head->fh2$w_recattr.fat$w_ffbyte = 0;
        } else {
            if( F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte) == 0 )
                --eofblk;
        }
        sts = update_truncate( fcb, eofblk );
    }

    if( $SUCCESSFUL(sts) )
        sts = deaccessfile(fcb);
    else
        deaccessfile(fcb);

    wccfile->wcf_fcb = NULL;
    if( wccfile->wcf_status & STATUS_TMPWCC )
        cleanup_wcf( &wccfile );
    else if( !(wccfile->wcf_fnb & NAM$M_WILDCARD) ) {
        cleanup_wcf( &wccfile );
        wccfile = (struct WCCFILE *)-1;
    }
    if( fab->fab$l_nam != NULL ) {
        fab->fab$l_nam->nam$l_wcc = wccfile;
    }

    for( rab = fab->fab$a__rab; rab != NULL; rab = fab->fab$a__rab ) {
        fab->fab$l_isi |= rab->rab$w_isi;
        rab->rab$w_isi = 0;
        fab->fab$a__rab = rab->rab$a__next;
        rab->rab$a__next = NULL;
    }
    if( fab->fab$l_isi != ~0u )
        abort();
    fab->fab$l_isi = 0;

    fab->fab$w_ifi = 0;
    ifi_table[ifi_no] = NULL;

    cache_flush(); /* Excessive, but seems safe... */

    return sts;
}
/***************************************************************** sys_open() */

/* open a file */

vmscond_t sys_open( struct FAB *fab ) {
    vmscond_t sts;
    unsigned int ifi_no;
    struct WCCFILE *wccfile = NULL;
    struct NAM *nam;

    errno = 0;

    if( fab == NULL )
        return RMS$_IAL;

    if( fab->fab$w_ifi != 0 )
        return RMS$_IFI;

    for( ifi_no = 1;
         ifi_table[ifi_no] != NULL && ifi_no <= IFI_MAX; ifi_no++)
        ;

    if( ifi_no > IFI_MAX )
        return RMS$_IFI;

    nam = fab->fab$l_nam;
    fab->fab$l_openfop = 0;

    if( nam != NULL ) {
        wccfile = (struct WCCFILE *) nam->nam$l_wcc;
        if( wccfile != NULL &&
            (wccfile == (struct WCCFILE *)-1 || wccfile->wcf_fab != fab ))
            cleanup_wcf( &wccfile );
        nam->nam$l_wcc = wccfile;
    }

    do {
        struct DEV *dev;
        struct VCB *vcb;
        struct HEAD *head;
#if DEBUG
        unsigned short idsize, fnsize;
#endif

        if( !(fab->fab$l_fop & FAB$M_NAM) || nam == NULL ||
            nam->nam$t_dvi[0] == 0    ||
            NULL_FID(&nam->nam$w_fid) ||
            NULL_FID(&nam->nam$w_did) ) {
            sts = SS$_NORMAL;
            if( wccfile == NULL ) {
                if( $SUCCESSFUL(sts = do_parse( fab, &wccfile )) ) {
                    if( wccfile == NULL )
                        sts = RMS$_NOVALPRS;
                    else {
                        wccfile->wcf_status |= STATUS_TMPWCC;
                        if( wccfile->wcf_fnb & NAM$M_WILDCARD )
                            sts = RMS$_WLD;
                        else
                            sts = do_search( fab, &wccfile );
                    }
                }
            }
            if( $SUCCESSFUL(sts) ) {
                sts = accessfile( wccfile->wcf_vcb, &wccfile->wcf_fid,
                                  &wccfile->wcf_fcb,
                                  (fab->fab$b_fac &
                                   (FAB$M_PUT| FAB$M_DEL | FAB$M_TRN | FAB$M_UPD)) != 0 );
            }
            break;
        }

        if( wccfile == NULL ) {
            wccfile = (struct WCCFILE *) calloc( 1, WCFALLOC_SIZE );
            if (wccfile == NULL)
                return SS$_INSFMEM;
            wccfile->wcf_fab = fab;

            wccfile->wcf_status = STATUS_TMPWCC;
        }

        if( $FAILS(sts = device_lookup( nam->nam$t_dvi[0],
                                        nam->nam$t_dvi+1, FALSE, &dev)) ) {
            sts = $SETLEVEL(sts, ERROR);
            break;
        }
        vcb = dev->vcb;
        device_done(dev);

        if( vcb == NULL ) {
            sts = $SETLEVEL(SS$_DEVNOTMOUNT, ERROR);
            break;
        }

        wccfile->wcf_vcb = NULL;
        if( $FAILS( sts = accessfile( vcb, &nam->nam$w_fid,
                                      &wccfile->wcf_fcb,
                                      (fab->fab$b_fac & (FAB$M_PUT | FAB$M_UPD)) != 0) ) )
            break;

        head = wccfile->wcf_fcb->head;

        if( !(NULL_FID(&head->fh2$w_backlink) ||
              FID_MATCH(&head->fh2$w_backlink,
                        &nam->nam$w_did,
                        wccfile->wcf_fcb->rvn)) ) {
            deaccessfile( wccfile->wcf_fcb );
            wccfile->wcf_fcb = NULL;
            sts = RMS$_DNF;
            break;
        }
        wccfile->wcf_status |= STATUS_BYFID;

        memcpy( &wccfile->wcf_wcd.wcd_dirid, &nam->nam$w_did,
                sizeof( struct fiddef ) );
        memcpy( &wccfile->wcf_fid, &nam->nam$w_fid,
                sizeof( struct fiddef ) );

 #if DEBUG
        /* Return (possibly truncated) name from header as result string) */
        wccfile->wcf_wcd.wcd_prelen = nam->nam$t_dvi[0];
        wccfile->wcf_wcd.wcd_reslen = 0;

        memcpy( wccfile->wcf_result, nam->nam$t_dvi, nam->nam$t_dvi[0] );
        nam->nam$b_rsl = nam->nam$t_dvi[0];

        idsize = (head->fh2$b_mpoffset - head->fh2$b_idoffset) * 2;
        fnsize = 0;

        if( idsize >= offsetof( struct IDENT, fi2$t_filenamext ) ) {
            struct IDENT *id;
            char fn[sizeof(id->fi2$t_filename) + sizeof( id->fi2$t_filenamext) ], *p;

            id = (struct IDENT *) (((f11word *)head) + head->fh2$b_idoffset);
            memcpy( fn, id->fi2$t_filename, sizeof( id->fi2$t_filename ) );
            fnsize = sizeof( id->fi2$t_filename );
            idsize -= offsetof( struct IDENT, fi2$t_filenamext );

            if( idsize > 0 ) {
                memcpy( fn + sizeof( id->fi2$t_filename ),
                        id->fi2$t_filenamext, idsize );
                fnsize += idsize;
            }
            for( p = fn + fnsize -1; p >= fn && *p == ' '; --p )
                --fnsize;

            if( fnsize > (256u -wccfile->wcf_wcd.wcd_reslen  ) )
                fnsize = 256u - wccfile->wcf_wcd.wcd_reslen ;
            memcpy( wccfile->wcf_result + wccfile->wcf_wcd.wcd_prelen , fn, fnsize );
            wccfile->wcf_wcd.wcd_reslen = (unsigned short) fnsize;

            nam->nam$b_rsl = (uint8_t)(wccfile->wcf_wcd.wcd_prelen +
                                       wccfile->wcf_wcd.wcd_reslen);
        }
#endif
    } while ( 0 );

    if( $SUCCESSFUL(sts) ) {
        ifi_table[ifi_no] = wccfile;
        fab->fab$w_ifi = ifi_no;

        fab->fab$l_openfop = fab->fab$l_fop;
        fab->fab$l_isi = ~0u;

        if( $FAILS(sts = sys_display(fab)) ) {
            deaccessfile( wccfile->wcf_fcb );
            wccfile->wcf_fcb = NULL;
            fab->fab$l_isi = 0;
            fab->fab$w_ifi = 0;
            ifi_table[ifi_no] = NULL;
        }
    }
    if( $FAILED(sts) && (wccfile == NULL || wccfile == (struct WCCFILE *)-1 ||
                         (wccfile->wcf_status & STATUS_TMPWCC)) ) {
        cleanup_wcf(&wccfile);
        if (nam != NULL) {
            nam->nam$l_wcc = wccfile;
        }
    }
    return sts;
}

/**************************************************************** sys_rename() */

vmscond_t sys_rename( struct FAB *oldfab,
                      void *astrtn, void *astprm, struct FAB *newfab ) {
    vmscond_t sts;
    struct WCCFILE *owcc = NULL, *nwcc = NULL;
    struct NAM *onam, *nnam;
    char *fn = NULL;

    if( oldfab == NULL || newfab == NULL || astrtn != NULL || astprm != NULL )
        return RMS$_IAL;

    if( oldfab->fab$w_ifi != 0 || newfab->fab$w_ifi != 0 )
        return RMS$_IFI;

    onam = oldfab->fab$l_nam;

    if( onam != NULL ) {
        owcc = onam->nam$l_wcc;
        if( owcc != NULL &&
            (owcc == (struct WCCFILE *)-1 || owcc->wcf_fab != oldfab) )
            cleanup_wcf( &owcc );
    }

    nnam = newfab->fab$l_nam;

    if( nnam != NULL ) {
        nwcc = nnam->nam$l_wcc;
        if( nwcc != NULL &&
            (nwcc == (struct WCCFILE *)-1 || nwcc->wcf_fab != newfab) )
            cleanup_wcf( &nwcc );
    }

    do {
        struct HEAD *fhd;
        struct IDENT *id;
        size_t i;
        size_t len;
        uint16_t reslen = 0;
        struct fibdef fibblk;
        struct dsc_descriptor fibdsc = { 0, 0, 0, NULL };
        struct dsc$descriptor rd = { 0, 0, 0, NULL };
        struct dsc$descriptor nd = { 0, 0, 0, NULL };
        int fna_size[5];

        memset( &fibblk, 0, sizeof( fibdsc ) );

        sts = SS$_NORMAL;
        if( owcc == NULL ) {
            if( $SUCCESSFUL(sts = do_parse(oldfab, &owcc)) ) {
                if( owcc == NULL )
                    sts = RMS$_NOVALPRS;
                else {
                    owcc->wcf_status |= STATUS_TMPWCC;
                    if( owcc->wcf_fnb & NAM$M_WILDCARD )
                        sts = RMS$_WLD;
                    else
                        sts = do_search( oldfab, &owcc );
                }
            }
        }
        if( $FAILED(sts) )
            break;

        if( nwcc == NULL ) {
            if( $SUCCESSFUL(sts = do_parse(newfab, &nwcc)) ) {
                if( nwcc == NULL )
                    sts = RMS$_NOVALPRS;
                else {
                    nwcc->wcf_status |= STATUS_TMPWCC;
                    if( nwcc->wcf_fnb & NAM$M_WILDCARD )
                        sts = RMS$_WLD;
                }
            }
        }
        if( $FAILED(sts) )
            break;

        if( nwcc->wcf_vcb != owcc->wcf_vcb ||
            nwcc->wcf_wcd.wcd_dirid.fid$b_rvn !=
            owcc->wcf_wcd.wcd_dirid.fid$b_rvn ) {
            sts = ODS2_FCS2DV;
            break;
        }
        {
            struct VCBDEV *vcbdev;

            vcbdev = RVN_TO_DEV(owcc->wcf_vcb, owcc->wcf_fid.fid$b_rvn);
            if( vcbdev == NULL ) {
                sts = SS$_DEVNOTMOUNT;
                break;
            }
            if( (owcc->wcf_fid.fid$w_num | ((uint32_t)owcc->wcf_fid.fid$b_nmx << 16)) <=
                (uint32_t)F11WORD( vcbdev->home.hm2$w_resfiles ) ) {
                sts = SS$_NOPRIV;
                break;
            }
        }

        fibdsc.dsc_w_length = sizeof(struct fibdef);
        fibdsc.dsc_a_pointer = (char *) &fibblk;

        fn = malloc( (2 * (size_t)nwcc->wcf_wcd.wcd_serdsc.dsc$w_length) + sizeof( ";32767" ) );
        if( fn == NULL ) {
            sts = SS$_INSFMEM;
            break;
        }
        for( i = 0; i < nwcc->wcf_wcd.wcd_serdsc.dsc$w_length; i++ )
            fn[i] = toupper( nwcc->wcf_wcd.wcd_serdsc.dsc$a_pointer[i] );
        fn[i] = '\0';

        nwcc->wcf_fid = owcc->wcf_fid;
        nd.dsc$a_pointer = fn;
        nd.dsc$w_length = nwcc->wcf_wcd.wcd_serdsc.dsc$w_length;

        rd.dsc$a_pointer = fn + nwcc->wcf_wcd.wcd_serdsc.dsc$w_length;
        rd.dsc$w_length = (uint16_t)(nwcc->wcf_wcd.wcd_serdsc.dsc$w_length +
                                     sizeof( ";32767" ));

        memset( &fibblk, 0, sizeof( fibblk ) );
        memcpy( &fibblk.fib$w_did_num, &nwcc->wcf_wcd.wcd_dirid,
                sizeof(struct fiddef));
        memcpy( &fibblk.fib$w_fid_num, &nwcc->wcf_fid,
                sizeof(struct fiddef));

        fibblk.fib$w_verlimit = oldfab->fab__w_verlimit;

        sts = direct( nwcc->wcf_vcb, &fibdsc,
                      &nd, &reslen, &rd, DIRECT_CREATE );

        rd.dsc$w_length = reslen;
        if( nnam != NULL && nnam->nam$l_rsa != NULL ) {
            name_delim( nwcc->wcf_result, nnam->nam$b_esl, fna_size );
            len = 255u - ((size_t)fna_size[0] + fna_size[1]);
            if( reslen < len )
                len = reslen;
            memcpy( nwcc->wcf_result + fna_size[0] + fna_size[1],
                    rd.dsc$a_pointer, len );
            name_delim( nwcc->wcf_result, nnam->nam$b_esl, fna_size );

            len = (size_t)fna_size[0] + fna_size[1] +
                ($SUCCESSFUL(sts)? reslen : (fna_size[2] + fna_size[3] + fna_size[4]));
            nwcc->wcf_wcd.wcd_prelen = 0;
            nwcc->wcf_wcd.wcd_reslen = (unsigned short)len;
        }
        if( $FAILED(sts) )
            break;

        memcpy( &fibblk.fib$w_did_num, &owcc->wcf_wcd.wcd_dirid,
                sizeof(struct fiddef));
        fibblk.fib$l_wcc = 0;
        sts = direct( owcc->wcf_vcb, &fibdsc, &owcc->wcf_wcd.wcd_serdsc,
                      NULL, NULL, DIRECT_DELETE ) ;

        if( $FAILED(sts) )
            break;

        if( $FAILS(sts = accessfile( nwcc->wcf_vcb, &nwcc->wcf_fid,
                                     &nwcc->wcf_fcb, TRUE )) )
            break;
        fhd = nwcc->wcf_fcb->head;
        id = (struct IDENT *)(((f11word *)fhd) + fhd->fh2$b_idoffset);

        i = (size_t)(fhd->fh2$b_mpoffset - fhd->fh2$b_idoffset) * 2;
        if( i > offsetof( struct IDENT, fi2$t_filenamext ) )
            i = sizeof( id->fi2$t_filename ) +
                (i - offsetof( struct IDENT, fi2$t_filenamext ));
        else
            i = sizeof( id->fi2$t_filename );
        if( i >= reslen ) {
            memset( id->fi2$t_filenamext, ' ', sizeof( id->fi2$t_filenamext ) );
            if( reslen <= sizeof( id->fi2$t_filename ) ) {
                memcpy( id->fi2$t_filename, rd.dsc$a_pointer, reslen );
            } else {
                memcpy( id->fi2$t_filename, rd.dsc$a_pointer, sizeof( id->fi2$t_filename ) );
                reslen -= (uint16_t)sizeof( id->fi2$t_filename );
                rd.dsc$a_pointer += sizeof( id->fi2$t_filename );
                memcpy( id->fi2$t_filenamext, rd.dsc$a_pointer, reslen );
            }
        } /* otherwise, it's necessary to expand the IDENT area.
           * This might require moving map pointers, ACL & reserved
           * data to an extension header, which could roll forward
           * into subsequent headers.  Since VMS (at least) is known
           * to allocate the maximum filename length in the file
           * header, we'll just skip updating the header in this case.
           * The file is accessible via the directory; the header name
           * is almost never used. (Recovery programs, some analyzers.)
           */
#ifdef MINIMIZE_IDENT
#error "MINIMIZE_IDENT" will prevent sys_rename from working.
#endif

        sys_gettim( id->fi2$q_revdate );
        if( id->fi2$w_revision != (f11word)~0u )
            ++id->fi2$w_revision;

        sts = deaccessfile( nwcc->wcf_fcb );
    } while( 0 );
    {
        vmscond_t st2;

        st2 = name2nam( nnam, nwcc, $SUCCESSFUL(sts) );
        if( $SUCCESSFUL(sts ) )
            sts = st2;
    }

    free( fn );

    if( owcc == NULL ||
        owcc == (struct WCCFILE *)-1 ||
        (owcc->wcf_status & STATUS_TMPWCC) ||
        !(owcc->wcf_fnb & NAM$M_WILDCARD) ) {
        cleanup_wcf( &owcc );
        if (onam != NULL) {
            onam->nam$l_wcc = NULL;
        }
    }
    if( nwcc == NULL ||
        nwcc == (struct WCCFILE *)-1 ||
        (nwcc->wcf_status & STATUS_TMPWCC) ||
        !(nwcc->wcf_fnb & NAM$M_WILDCARD) ) {
        cleanup_wcf( &nwcc );
        if (nnam != NULL) {
            nnam->nam$l_wcc = NULL;
        }
    }
    return sts;
}

/**************************************************************** sys_erase() */

vmscond_t sys_erase( struct FAB *fab ) {
    vmscond_t sts;
    struct WCCFILE *wccfile = NULL;
    struct NAM *nam;
    struct fibdef fibblk;

    errno = 0;

    if( fab == NULL )
        return RMS$_IAL;

    if( fab->fab$w_ifi != 0 )
        return RMS$_IFI;

    nam = fab->fab$l_nam;

    if( nam != NULL ) {
        wccfile = nam->nam$l_wcc;
        if( wccfile != NULL &&
            (wccfile == (struct WCCFILE *)-1 || wccfile->wcf_fab != fab) )
            cleanup_wcf( &wccfile );
    }

    memset( &fibblk, 0, sizeof( fibblk ) );

    do {
        struct DEV *dev;
        struct VCB *vcb;

        if( !(fab->fab$l_fop & FAB$M_NAM) || nam == NULL ||
            nam->nam$t_dvi[0] == 0    ||
            NULL_FID(&nam->nam$w_fid) ||
            NULL_FID(&nam->nam$w_did) ) {
            sts = SS$_NORMAL;
            if( wccfile == NULL ) {
                if( $SUCCESSFUL(sts = do_parse(fab, &wccfile)) ) {
                    if( wccfile == NULL )
                        sts = RMS$_NOVALPRS;
                    else {
                        wccfile->wcf_status |= STATUS_TMPWCC;
                        if( wccfile->wcf_fnb & NAM$M_WILDCARD )
                            sts = RMS$_WLD;
                        else
                            sts = do_search( fab, &wccfile );
                    }
                }
            }
            if( $FAILED(sts) )
                break;

            if( $SUCCESSFUL(sts = accesserase( wccfile->wcf_vcb,
                                               &wccfile->wcf_fid )) ) {
                struct dsc_descriptor fibdsc = { 0, 0, 0, NULL },
                    serdsc = { 0, 0, 0, NULL };

                fibdsc.dsc_w_length = sizeof(struct fibdef);
                fibdsc.dsc_a_pointer = (char *) &fibblk;

                serdsc.dsc_w_length = wccfile->wcf_wcd.wcd_reslen;
                serdsc.dsc_a_pointer = wccfile->wcf_result +
                    wccfile->wcf_wcd.wcd_prelen;

                memcpy( &fibblk.fib$w_did_num, &wccfile->wcf_wcd.wcd_dirid,
                        sizeof(struct fiddef));
                sts = direct( wccfile->wcf_vcb, &fibdsc, &serdsc,
                              NULL, NULL, DIRECT_DELETE ) ;
            }
            break;
        } /* By name */

        /* By FID */
        if( wccfile == NULL ) {
            wccfile = (struct WCCFILE *) calloc( 1, WCFALLOC_SIZE );
            if (wccfile == NULL)
                return SS$_INSFMEM;
            wccfile->wcf_fab = fab;

            wccfile->wcf_status = STATUS_TMPWCC;
        }

        if( $FAILS(sts = device_lookup( nam->nam$t_dvi[0],
                                        nam->nam$t_dvi+1, FALSE, &dev)) ) {
            sts = $SETLEVEL(sts, ERROR);
            break;
        }
        vcb = dev->vcb;
        device_done(dev);

        if( vcb == NULL ) {
            sts = $SETLEVEL(SS$_DEVNOTMOUNT, ERROR);
            break;
        }

        wccfile->wcf_vcb = vcb;
#if 0
        memcpy( &wccfile->wcf_wcd.wcd_dirid, &nam->nam$w_did,
                sizeof(struct fiddef) );
#endif
        memcpy( &wccfile->wcf_fid, &nam->nam$w_fid,
                sizeof(struct fiddef) );

        sts = accesserase( wccfile->wcf_vcb, &wccfile->wcf_fid );
    } while( 0 );

    if( wccfile == NULL ||
        wccfile == (struct WCCFILE *)-1 ||
        (wccfile->wcf_status & STATUS_TMPWCC) ||
        !(wccfile->wcf_fnb & NAM$M_WILDCARD) ) {
        cleanup_wcf( &wccfile );
        if (nam != NULL) {
            nam->nam$l_wcc = NULL;
        }
    }
    return sts;
}

/*************************************************************** sys_create() */

vmscond_t sys_create( struct FAB *fab ) {
    vmscond_t sts;
    unsigned int ifi_no;
    int rdtset = FALSE;
    char *fn = NULL;
    struct NAM *nam;
    struct WCCFILE *wccfile = NULL;
    struct dsc$descriptor rd = { 0, 0, 0, NULL };

    errno = 0;

    if( fab == NULL )
        return RMS$_IAL;

    if( fab->fab$w_ifi != 0 )
        return RMS$_IFI;

    for( ifi_no = 1;
         ifi_table[ifi_no] != NULL && ifi_no <= IFI_MAX; ifi_no++ )
        ;

    if( ifi_no > IFI_MAX )
        return RMS$_IFI;

    fab->fab$b_fac |= FAB$M_PUT;

    nam = fab->fab$l_nam;

    if( nam != NULL ) {
        wccfile = nam->nam$l_wcc;
        if( wccfile != NULL && wccfile->wcf_fab != fab ) {
            cleanup_wcf( &wccfile );
            nam->nam$l_wcc = NULL;
        }
    }
    /* TODO create from DVI & DID
     */
    if (wccfile == NULL) {
        sts = do_parse( fab, &wccfile );
        if( $SUCCESSFUL(sts) ) {
            if( wccfile == NULL )
                sts = RMS$_NOVALPRS;
            else {
                wccfile->wcf_status |= STATUS_TMPWCC;

                if( wccfile->wcf_fnb & NAM$M_WILDCARD ) {
                    sts = RMS$_WLD;
                } else {
                    if( $FAILS(sts = do_search( fab, &wccfile)) && sts == RMS$_FNF ) {
                        sts = do_parse( fab, &wccfile );
                        if( wccfile != NULL && wccfile != (struct WCCFILE *)-1 )
                            wccfile->wcf_status |= STATUS_TMPWCC;
                    }
                }
            }
        }
    } else {
        sts = SS$_NORMAL;
    }
    do {
        int fna_size[5];
        size_t len;
        uint16_t reslen = 0;
        size_t i;
        uint32_t alq;
        struct XABDAT *xab;
        struct HEAD *fhd;
        struct IDENT *id;
        struct fibdef fibblk;
        struct dsc_descriptor fibdsc = { 0, 0, 0, NULL };
        struct dsc$descriptor nd = { 0, 0, 0, NULL };
        struct NEWFILE attrs;

        memset( &fibblk, 0, sizeof(fibblk) );
        if( $FAILED(sts) )
            break;

        fibdsc.dsc_w_length = sizeof(struct fibdef);
        fibdsc.dsc_a_pointer = (char *) &fibblk;

        fn = malloc( (2 * (size_t)wccfile->wcf_wcd.wcd_serdsc.dsc$w_length) + sizeof( ";32767" ) );
        if( fn == NULL ) {
            sts = SS$_INSFMEM;
            break;
        }
        for( i = 0; i < wccfile->wcf_wcd.wcd_serdsc.dsc$w_length; i++ )
            fn[i] = toupper( wccfile->wcf_wcd.wcd_serdsc.dsc$a_pointer[i] );
        fn[i] = '\0';

        memset( &attrs, 0, sizeof( attrs ) );

        switch( fab->fab$b_rfm ) {
        case FAB$C_FIX:
            if( fab->fab$w_mrs > 32767u || fab->fab$w_mrs == 0 )
                sts = RMS$_RSZ;
            else if( fab->fab$b_fsz )
                sts = RMS$_FSZ;
            break;
        case FAB$C_VAR:
            /* Directories use MRS of 512 but are also NOSPAN */
            if (i >= sizeof("A.DIR;1") - 1 &&
                !strcmp(fn + i - (sizeof(".DIR;1") - 1), ".DIR;1")) {
                if (fab->fab$w_mrs != 512 || !(fab->fab$b_rat & FAB$M_BLK)) {
                    sts = RMS$_RSZ;
                    break;
                }
                break;
            }
            if( fab->fab$w_mrs > ((fab->fab$b_rat & FAB$M_BLK)? 510: 32767) )
                sts = RMS$_RSZ;
            else if( fab->fab$b_fsz )
                sts = RMS$_FSZ;
            break;
        case FAB$C_VFC:
            if( fab->fab$b_fsz == 0 )
                fab->fab$b_fsz = 2;
            if( fab->fab$w_mrs >
                 (((fab->fab$b_rat & FAB$M_BLK)? 510: 32767) - fab->fab$b_fsz) )
                sts = RMS$_RSZ;
            break;
        case FAB$C_STM:
        case FAB$C_STMLF:
        case FAB$C_STMCR:
            if( fab->fab$w_mrs > 32767 )
                sts = RMS$_RSZ;
            else if( fab->fab$b_fsz )
                sts = RMS$_FSZ;
            else if( fab->fab$b_rat & (FAB$M_PRN | FAB$M_BLK) )
                sts = RMS$_RAT;
        case FAB$C_UDF:
            break;
        default:
            sts = RMS$_RFM;
        }
        if( $FAILED(sts) )
            break;

        switch( fab->fab$b_rat & (FAB$M_CR|FAB$M_FTN|FAB$M_PRN) ) {
        case 0:
        case FAB$M_CR:
        case FAB$M_FTN:
        case FAB$M_PRN:
            break;
        default:
            sts = RMS$_RAT;
            break;
        }
        if( $FAILED(sts) )
            break;

        attrs.recattr.fat$b_bktsize =  fab->fab$b_bks;
        attrs.recattr.fat$w_defext =  F11WORD( fab->fab$w_deq );
        attrs.recattr.fat$b_vfcsize =  fab->fab$b_fsz;
        attrs.recattr.fat$w_gbc = F11WORD( fab->fab$w_gbc );
        attrs.recattr.fat$w_maxrec = F11WORD( fab->fab$w_mrs );
        attrs.recattr.fat$b_rtype = ( (fab->fab$b_org & 0xf0 ) |
                                      (fab->fab$b_rfm  & 0x0f) );
        attrs.recattr.fat$b_rattrib = fab->fab$b_rat;

        if( fab->fab$b_rfm == FAB$C_STM ||
            fab->fab$b_rfm == FAB$C_STMLF ||
            fab->fab$b_rfm == FAB$C_STMCR )
            attrs.recattr.fat$w_rsize = 32767;

        alq = fab->fab$l_alq;

        for( xab = fab->fab$l_xab; xab != NULL; xab = xab->xab$l_nxt ) {
            switch( ((struct XABDAT *)xab)->xab$b_cod ) {
            case XAB$C_ALL: {
                struct XABALL *all;

                all = (struct XABALL *) xab;

                attrs.recattr.fat$w_defext = F11WORD( all->xab$w_deq );
                alq = all->xab$l_alq;
            }
                break;
            case XAB$C_DAT: {
                struct XABDAT *dat;

                dat = (struct XABDAT *) xab;
                memcpy( attrs.bakdate, dat->xab$q_bdt, sizeof( VMSTIME ) );
                memcpy( attrs.credate, dat->xab$q_cdt, sizeof( VMSTIME ) );
                memcpy( attrs.expdate, dat->xab$q_edt, sizeof( VMSTIME ) );
                memcpy( attrs.revdate, dat->xab$q_rdt, sizeof( VMSTIME ) );
                attrs.revision = F11WORD( dat->xab$w_rvn );
                rdtset = TRUE;
            }
                break;
            case XAB$C_RDT: {
                struct XABDAT *rdt;

                rdt = (struct XABDAT *) xab;
                if( memcmp( &rdt->xab$q_rdt, &timenotset, sizeof( VMSTIME ) ) ) {
                    memcpy(attrs.revdate, &rdt->xab$q_rdt,
                           sizeof( VMSTIME ));
                    rdtset = TRUE;
                    if( xab->xab$w_rvn != 0 ) {
                        attrs.revision = F11WORD( xab->xab$w_rvn );
                        rdtset = TRUE;
                    }
                }
            }
                break;
            case XAB$C_FHC:{
                struct XABFHC *fhc;

                fhc = (struct XABFHC *) xab;
                attrs.recattr.fat$w_rsize  = F11WORD( fhc->xab$w_lrl );
                attrs.verlimit             = F11WORD( fhc->xab$w_verlimit ); /* not set by VMS */
            }
                break;
            case XAB$C_PRO:{
                struct XABPRO *pro;

                pro = (struct XABPRO *) xab;
                attrs.fileprot = (1 << 16) | F11WORD( pro->xab$w_pro );
                memcpy( &attrs.fileowner, &pro->xab$l_uic, 4 );
            }
                break;
            case XAB$C_ITM:{
                struct XABITM *itm;
                struct item_list *list;

                itm = (struct XABITM *) xab;

                if( itm->xab$b_mode != XAB$K_SETMODE )
                    break;

                for( list = itm->xab$l_itemlist;
                     list->code != 0 || list->length != 0; list++ ) {
                    switch( list->code ) {
                    case XAB$_UCHAR_DIRECTORY:
                        i = strlen( fn );
                        if( i < sizeof( "A.DIR;1" ) -1 ||
                            strcmp( fn + i - (sizeof( ".DIR;1" ) -1), ".DIR;1" ) )
                            sts = SS$_NOPRIV;
                        if( *((uint32_t *)list->buffer) )
                            attrs.filechar |= F11LONG( FH2$M_DIRECTORY );
                        else
                            attrs.filechar &= ~F11LONG( FH2$M_DIRECTORY );
                        break;
                    case XAB$_UCHAR_NOBACKUP:
                        if( *((uint32_t *)list->buffer) )
                            attrs.filechar |= F11LONG( FH2$M_NOBACKUP );
                        else
                            attrs.filechar &= ~F11LONG( FH2$M_NOBACKUP );
                        break;
                    case XAB$_UCHAR_READCHECK:
                        if( *((uint32_t *)list->buffer) )
                            attrs.filechar |= F11LONG( FH2$M_READCHECK );
                        else
                            attrs.filechar &= ~F11LONG( FH2$M_READCHECK );
                        break;
                    case XAB$_UCHAR_WRITECHECK:
                        if( *((uint32_t *)list->buffer) )
                            attrs.filechar |= F11LONG( FH2$M_WRITECHECK );
                        else
                            attrs.filechar &= ~F11LONG( FH2$M_WRITECHECK );
                        break;
                    case XAB$_UCHAR_CONTIGB:
                        if( *((uint32_t *)list->buffer) )
                            attrs.filechar |= F11LONG( FH2$M_CONTIGB );
                        else
                            attrs.filechar &= ~F11LONG( FH2$M_CONTIGB );
                        break;
                    case XAB$_UCHAR_LOCKED:
                        if( *((uint32_t *)list->buffer) )
                            attrs.filechar |= F11LONG( FH2$M_LOCKED );
                        else
                            attrs.filechar &= ~F11LONG( FH2$M_LOCKED );
                        break;
                    default:
                        break;
                    }
                }
            }
                break;
            default:
                break;
            }
        }
#if 0 /* RMS does this, but alq is actually unsigned...alloc will fail */
        if( alq < 0 )
            sts = RMS$_ALQ;
#endif
        if( (fab->fab$b_rat & FAB$M_PRN)  &&
            !(fab->fab$b_rfm == FAB$C_VFC && fab->fab$b_fsz == 2) ) {
                sts = RMS$_RAT;
                break;
        }
        if( $FAILED(sts) )
            break;

        memcpy( &fibblk.fib$w_did_num, &wccfile->wcf_wcd.wcd_dirid,
                sizeof(struct fiddef));
        fibblk.fib$w_nmctl = 0;
        fibblk.fib$l_acctl = 0;
        fibblk.fib$l_wcc = 0;

        sts = update_create( wccfile->wcf_vcb,
                             (struct fiddef *)&fibblk.fib$w_did_num,
                             fn,
                             (struct fiddef *)&fibblk.fib$w_fid_num,
                             &attrs,
                             &wccfile->wcf_fcb );
        if( $FAILED(sts) )
            break;

        wccfile->wcf_fcb->status |= FCB_WRITTEN;

        if( rdtset )
            wccfile->wcf_fcb->status |= FCB_RDTSET;

        nd.dsc$a_pointer = fn;
        nd.dsc$w_length = wccfile->wcf_wcd.wcd_serdsc.dsc$w_length;

        rd.dsc$a_pointer = fn + wccfile->wcf_wcd.wcd_serdsc.dsc$w_length;
        rd.dsc$w_length = (uint16_t)(wccfile->wcf_wcd.wcd_serdsc.dsc$w_length +
                                     sizeof( ";32767" ));

        fibblk.fib$w_verlimit = attrs.verlimit;

        sts = direct( wccfile->wcf_vcb, &fibdsc,
                      &nd, &reslen, &rd, DIRECT_CREATE );

        rd.dsc$w_length = reslen;
        rd.dsc$a_pointer[reslen] = '\0';
        if( nam != NULL && nam->nam$l_rsa != NULL ) {
            name_delim( wccfile->wcf_result, nam->nam$b_esl, fna_size );
            len = 255u - ((size_t)fna_size[0] + fna_size[1]);
            if( reslen < len )
                len = reslen;
            memcpy( wccfile->wcf_result + fna_size[0] + fna_size[1],
                    rd.dsc$a_pointer, len );
            name_delim( wccfile->wcf_result, nam->nam$b_esl, fna_size );

            len = (size_t)fna_size[0] + fna_size[1] +
                ($SUCCESSFUL(sts)? reslen : (fna_size[2] + fna_size[3] + fna_size[4]));

            wccfile->wcf_wcd.wcd_prelen = 0;
            wccfile->wcf_wcd.wcd_reslen = (unsigned short)len;
        }
        if( $FAILED(sts) ) {
            deaccessfile( wccfile->wcf_fcb );
            accesserase( wccfile->wcf_vcb, (struct fiddef *)&fibblk.fib$w_fid_num );
            wccfile->wcf_fcb = NULL;
            break;
        }
        /* Update filename in header with ;version
         * Logically this belongs in access, but that will require
         * reworking the APIs.  The issue is that the version can't
         * be known until the directory entry is made, but making the
         * directory entry requires the FID.  It also needs to be possible
         * to create a file without a directory entry.  For now, this two-step
         * approach will do.
         */

        fhd = wccfile->wcf_fcb->head;
        id = (struct IDENT *)(((f11word *)fhd) + fhd->fh2$b_idoffset);
#ifdef MINIMIZE_IDENT
        i = (size_t)(fhd->fh2$b_mpoffset - fhd->fh2$b_idoffset) * 2;
        if( i > offsetof( struct IDENT, fi2$t_filenamext ) )
            i = sizeof( id->fi2$t_filename ) +
                (i - offsetof( struct IDENT, fi2$t_filenamext ));
        else
            i = sizeof( id->fi2$t_filename );
        if( i < reslen && reslen > sizeof( id->fi2$t_filename ) ) {
            size_t idlen;

            /* filenamext too small, must expand.
             * New file.  No map pointers to move/overflow. No acl/rsv alloc to adj.
             */
            idlen = ((reslen - sizeof( id->fi2$t_filename )) +
                      offsetof( struct IDENT, fi2$t_filenamext) + 1) / 2;
            fhd->fh2$b_mpoffset = fhd->fh2$b_idoffset + (uint8_t)idlen;
        }
#endif
        if( reslen <= sizeof( id->fi2$t_filename ) ) {
            memcpy( id->fi2$t_filename, rd.dsc$a_pointer, reslen );
        } else {
            memcpy( id->fi2$t_filename, rd.dsc$a_pointer, sizeof( id->fi2$t_filename ) );
            reslen -= (uint16_t)sizeof( id->fi2$t_filename );
            rd.dsc$a_pointer += sizeof( id->fi2$t_filename );
            memcpy( id->fi2$t_filenamext, rd.dsc$a_pointer, reslen );
 #ifdef MINIMIZE_IDENT
           if( reslen & 1 )
                id->fi2$t_filenamext[reslen] = ' ';
#endif
        }

        if( alq != 0 ) {
            sts = update_extend( wccfile->wcf_fcb, alq, 0 );
            if( $MATCHCOND(sts, SS$_DEVICEFULL) )
                sts = RMS$_FUL;
            if( $FAILED(sts) ) {
                deaccessfile( wccfile->wcf_fcb );
                accesserase( wccfile->wcf_vcb,
                             (struct fiddef *)&fibblk.fib$w_fid_num );
                wccfile->wcf_fcb = NULL;
                direct( wccfile->wcf_vcb, &fibdsc,
                        &rd, NULL, NULL, DIRECT_DELETE );
                break;
            }
        }
        ifi_table[ifi_no] = wccfile;
        fab->fab$w_ifi = ifi_no;
        fab->fab$l_isi = ~0u;

        if( $FAILS(sts = sys_display(fab)) ) {
            deaccessfile( wccfile->wcf_fcb );
            accesserase( wccfile->wcf_vcb,
                         (struct fiddef *)&fibblk.fib$w_fid_num );
            wccfile->wcf_fcb = NULL;
            direct( wccfile->wcf_vcb, &fibdsc,
                    &rd, NULL, NULL, DIRECT_DELETE );
            fab->fab$l_isi = 0;
            fab->fab$w_ifi = 0;
            ifi_table[ifi_no] = NULL;
        }
    } while( 0 );

    free( fn );
    if( $FAILED(sts) && (wccfile == NULL ||
                         wccfile == (struct WCCFILE *)-1 ||
                         (wccfile->wcf_status & STATUS_TMPWCC)) ) {
       cleanup_wcf( &wccfile );
        if( nam != NULL )
            nam->nam$l_wcc = NULL;
    }
    return sts;
}

/*************************************************************** sys_extend() */

vmscond_t sys_extend( struct FAB *fab ) {
    vmscond_t sts;
    unsigned int ifi_no;
    struct FCB *fcb;
    struct XABALL *xab;
    unsigned long int alq;

    if( fab == NULL )
        return RMS$_IAL;

    ifi_no = fab->fab$w_ifi;

    errno = 0;

    if( ifi_no == 0 || ifi_no > IFI_MAX )
        return RMS$_IFI;

    fcb = ifi_table[ifi_no]->wcf_fcb;

    alq = fab->fab$l_alq;

    for( xab = fab->fab$l_xab; xab != NULL; xab = xab->xab$l_nxt ) {
        if( xab->xab$b_cod == XAB$C_ALL ) {
            alq = xab->xab$l_alq;
            break;
        }
    }

    sts = update_extend( fcb, alq, 0 );

    return sts;
}

/*************************************************************** name2nam() */

 static vmscond_t name2nam( struct NAM *nam, struct WCCFILE *wccfile, int rsl ) {
    int fna_size[5];
    vmscond_t sts;
    uint8_t outlen, *lenp;
    uint16_t reslen;

    if( nam == NULL )
        return SS$_NORMAL;

    if( wccfile->wcf_status & STATUS_BYFID ) {
        /* ?? clear rsl? */
        return SS$_NORMAL;
    }

    reslen = wccfile->wcf_wcd.wcd_prelen + wccfile->wcf_wcd.wcd_reslen;

    if( $FAILS(sts = name_delim( wccfile->wcf_result, reslen, fna_size) ) )
        return sts;

    if( rsl ) {
        sts = RMS$_RSS;
        nam->nam$l_dev = nam->nam$l_rsa;
        lenp = &nam->nam$b_rsl;
        outlen = nam->nam$b_rss;
    } else {
        sts = RMS$_ESS;
        nam->nam$l_dev = nam->nam$l_esa;
        lenp = &nam->nam$b_esl;
        outlen = nam->nam$b_ess;
        nam->nam$b_rsl = 0;
    }


    if( nam->nam$l_dev == NULL || ((uint16_t)outlen) < reslen )
        return sts;

    memcpy( nam->nam$l_dev, wccfile->wcf_result, reslen );
    *lenp = (uint8_t)reslen;

    nam->nam$b_dev  = fna_size[0];
    nam->nam$l_dir  = nam->nam$l_dev + fna_size[0];
    nam->nam$b_dir  = fna_size[1];
    nam->nam$l_name = nam->nam$l_dir + fna_size[1];
    nam->nam$b_name = fna_size[2];
    nam->nam$l_type = nam->nam$l_name + fna_size[2];
    nam->nam$b_type = fna_size[3];
    nam->nam$l_ver  = nam->nam$l_type + fna_size[3];
    nam->nam$b_ver  = fna_size[4];

    return SS$_NORMAL;
}

/*************************************************************** sys_rundown() */
static void sys_rundown( void ) {
    access_rundown();

    sysmsg_rundown();

    cache_purge(TRUE);
}

/*************************************************************** sys_initialize() */
vmscond_t sys_initialize( void ) {
    static int exit_set = FALSE;

    errno = 0;

    if( !exit_set ) {
        if( atexit( sys_rundown ) != 0 )
            return SS$_INSFMEM;
        exit_set = TRUE;
    }

    return SS$_NORMAL;
}
