/* check for cr - return terminator - update file length */
/* RMS.c V2.1  RMS components */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/*  Boy some cleanups are needed here - especially for
    error handling and deallocation of memory after errors..
    For now we have to settle for loosing memory left/right/center...

    This module implements file name parsing, file searches,
    file opens, gets, etc...       */


#define DEBUGx x

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RMS$INITIALIZE          /* used by rms.h to create templates */

#include "access.h"
#include "descrip.h"
#include "device.h"
#include "direct.h"
#include "fibdef.h"
#include "rms.h"
#include "ssdef.h"
#include "stsdef.h"
#include "compat.h"
#include "sysmsg.h"

#ifndef TRUE
#define TRUE ( 0 == 0 )
#endif
#ifndef FALSE
#define FALSE ( 0 != 0 )
#endif


/*      For file context info we use WCCDIR and WCCFILE structures...
        Each WCCFILE structure contains one WCCDIR structure for file
        context. Each level of directory has an additional WCCDIR record.
        For example DKA200:[PNANKERVIS.F11]RMS.C is loosley stored as:-
                        next         next
                WCCFILE  -->  WCCDIR  -->  WCCDIR
                RMS.C;       F11.DIR;1    PNANKERVIS.DIR;1

        WCCFILE is pointed to by fab->fab$l_nam->nam$l_wcc and if a
        file is open also by ifi_table[fab->fab$w_ifi]  (so that close
        can easily locate it).

        Most importantly WCCFILE contains a resulting filename field
        which stores the resulting file spec. Each WCCDIR has a prelen
        length to indicate how many characters in the specification
        exist before the bit contributed by this WCCDIR. (ie to store
        the device name and any previous directory name entries.)  */


#define STATUS_INIT 1
#define STATUS_TMPDIR  2
#define STATUS_WILDCARD 4

struct WCCDIR {
    struct WCCDIR *wcd_next;
    struct WCCDIR *wcd_prev;
#ifdef DEBUG
    size_t size;
#endif
    int wcd_status;
    int wcd_wcc;
    int wcd_prelen;
    unsigned short wcd_reslen;
    struct dsc_descriptor wcd_serdsc;
    struct fiddef wcd_dirid;
    char wcd_sernam[1];         /* Must be last in structure */
};                              /* Directory context */

#define STATUS_RECURSE 8
#define STATUS_TMPWCC  16
#define MAX_FILELEN 1024

/* Allocation sise for WCCFILE = block + max len wcd_sernam.
 *  WCCDIRs are allocated smaller on the chain..
 * Why 256?  The byte lengths?
 */
#define WCFALLOC_SIZE (sizeof(struct WCCFILE)+256)

struct WCCFILE {
    struct FAB *wcf_fab;
    struct VCB *wcf_vcb;
    struct FCB *wcf_fcb;
    int wcf_status;
    struct fiddef wcf_fid;
    char wcf_result[MAX_FILELEN];
    struct WCCDIR wcf_wcd;      /* Must be last..... (dynamic length). */
};                              /* File context */

/* Table of file name component delimeters... */

char char_delim[] = {
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*                          .                        : ; <   >    */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0, 0,0,0,0,0,0,0,0,0,0,1,1,1,0,1,0,
/*                                                     [   ]      */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/*************************************************************** name_delim() */

/* Routine to find size of file name components */

unsigned name_delim(char *str,int len,int size[5])
{
    register unsigned ch = 0;
    register char *curptr = str;
    register char *endptr = curptr + len;
    register char *begptr = curptr;
    while (curptr < endptr) {
        ch = (*curptr++ & 127);
        if (char_delim[ch]) break;
    }
    if (curptr > begptr && ch == ':') {
        size[0] = (curptr - begptr);
        begptr = curptr;
        while (curptr < endptr) {
            ch = (*curptr++ & 127);
            if (char_delim[ch]) break;
        }
    } else {
        size[0] = 0;
    }
    if (ch == '[' || ch == '<') {
        if (curptr != begptr + 1) return RMS$_FNM;
        while (curptr < endptr) {
            ch = (*curptr++ & 127);
                if (char_delim[ch]) if (ch != '.') break;
        }
        if (curptr < begptr + 2 || (ch != ']' && ch != '>')) return RMS$_FNM;
        size[1] = (curptr - begptr);
        begptr = curptr;
        while (curptr < endptr) {
            ch = (*curptr++ & 127);
            if (char_delim[ch]) break;
        }
    } else {
        size[1] = 0;
    }
    if (curptr > begptr && char_delim[ch]) {
        size[2] = (curptr - begptr) - 1;
        begptr = curptr - 1;
    } else {
        size[2] = (curptr - begptr);
        begptr = curptr;
    }
    if (curptr > begptr && ch == '.') {
        while (curptr < endptr) {
            ch = (*curptr++ & 127);
            if (char_delim[ch]) break;
        }
        if (curptr > begptr && char_delim[ch]) {
            size[3] = (curptr - begptr) - 1;
            begptr = curptr - 1;
        } else {
            size[3] = (curptr - begptr);
            begptr = curptr;
        }
    } else {
        size[3] = 0;
    }
    if (curptr > begptr && (ch == ';' || ch == '.')) {
        while (curptr < endptr) {
            ch = (*curptr++ & 127);
            if (char_delim[ch]) break;
        }
        size[4] = (curptr - begptr);
    } else {
        size[4] = 0;
    }
#ifdef DEBUG
    printf("Name delim %d %d %d %d %d\n",
           size[0],size[1],size[2],size[3],size[4]);
#endif
    if (curptr >= endptr) {
        return SS$_NORMAL;
    } else {
        return RMS$_FNM;
    }
}

/******************************************************************* dircmp() */

/* Function to compare directory cache names - directory cache NOT IN USE */

static int dircmp( unsigned keylen, void *key, void *node ) {

    register struct DIRCACHE *dirnode = (struct DIRCACHE *) node;
    register int cmp = keylen - dirnode->dirlen;
    if (cmp == 0) {
        register int len = keylen;
        register const char *keynam = (const char *) key;
        register char *dirnam = dirnode->dirnam;
        while (len-- > 0) {
            cmp = toupper(*keynam++) - toupper(*dirnam++);
            if (cmp != 0) break;
        }
    }
    return cmp;
}

/***************************************************************** dircache() */

/* Routine to find directory name in cache... NOT CURRENTLY IN USE!!! */

unsigned dircache(struct VCB *vcb,char *dirnam,int dirlen,struct fiddef *dirid)
{
    register struct DIRCACHE *dir;
    if (dirlen < 1) {
        dirid->fid$w_num = 4;
        dirid->fid$w_seq = 4;
        dirid->fid$b_rvn = 0;
        dirid->fid$b_nmx = 0;
        return SS$_NORMAL;
    } else {
        unsigned sts;
        dir = cache_find((void *) &vcb->dircache,dirlen,dirnam,&sts,dircmp,
                         NULL);
        if (dir != NULL) {
            memcpy(dirid,&dir->dirid,sizeof(struct fiddef));
            return SS$_NORMAL;
        }
        return 0;
    }
}

/************************************************************** cleanup_wcf() */
#define WCDALLOC_SIZE(size) (sizeof(struct WCCDIR)+(size))
/* Allocation sise for WCCFILE = block + max len wcd_sernam.
 *  WCCDIRs are allocated smaller on the chain..
 * Why 256?  The byte lengths?
 */
#define WCFALLOC_SIZE (sizeof(struct WCCFILE)+256)


/* Function to remove WCCFILE and WCCDIR structures when not required */

void cleanup_wcf(struct WCCFILE **wccfilep)
{
    struct WCCFILE *wccfile;

    wccfile = *wccfilep;
    *wccfilep = NULL;
    if (wccfile != NULL) {
        struct WCCDIR *wcc = wccfile->wcf_wcd.wcd_next;
        wccfile->wcf_wcd.wcd_next = NULL;
        wccfile->wcf_wcd.wcd_prev = NULL;
        /* should deaccess volume */
        while (wcc != NULL) {
            struct WCCDIR *next = wcc->wcd_next;
            wcc->wcd_next = NULL;
            wcc->wcd_prev = NULL;
#ifdef DEBUG
            memset(wcc,0,wcc->size);
#endif
            free(wcc);
            wcc = next;
        }
#ifdef DEBUG
        memset(wccfile,0,WCFALLOC_SIZE);
#endif
        free(wccfile);
    }
}

/**************************************************************** do_search() */

/* Function to perform an RMS search... */

unsigned do_search(struct FAB *fab,struct WCCFILE *wccfile)
{
    int sts;
    struct fibdef fibblk;
    struct WCCDIR *wcc;
    struct dsc_descriptor fibdsc,resdsc;
    struct NAM *nam = fab->fab$l_nam;
    wcc = &wccfile->wcf_wcd;
    if (fab->fab$w_ifi != 0) return RMS$_IFI;

    /* if first time through position at top directory... WCCDIR */

    while ((wcc->wcd_status & STATUS_INIT) == 0 && wcc->wcd_next != NULL) {
        wcc = wcc->wcd_next;
    }
    fibdsc.dsc_w_length = sizeof(struct fibdef);
    fibdsc.dsc_a_pointer = (char *) &fibblk;
    while (TRUE) {
        if ((wcc->wcd_status & STATUS_INIT) == 0 || wcc->wcd_wcc != 0) {
            wcc->wcd_status |= STATUS_INIT;
            resdsc.dsc_w_length = 256 - wcc->wcd_prelen;
            resdsc.dsc_a_pointer = wccfile->wcf_result + wcc->wcd_prelen;
            memcpy(&fibblk.fib$w_did_num,&wcc->wcd_dirid,sizeof(struct fiddef));
            fibblk.fib$w_nmctl = 0;     /* FIB_M_WILD; */
            fibblk.fib$l_acctl = 0;
            fibblk.fib$w_fid_num = 0;
            fibblk.fib$w_fid_seq = 0;
            fibblk.fib$b_fid_rvn = 0;
            fibblk.fib$b_fid_nmx = 0;
            fibblk.fib$l_wcc = wcc->wcd_wcc;
#ifdef DEBUG
            wcc->wcd_sernam[wcc->wcd_serdsc.dsc_w_length] = '\0';
            wccfile->wcf_result[wcc->wcd_prelen + wcc->wcd_reslen] = '\0';
            printf("Ser: '%s' (%d,%d,%d) WCC: %d Prelen: %d '%s'\n",
                   wcc->wcd_sernam,
                   fibblk.fib$w_did_num | (fibblk.fib$b_did_nmx << 16),
                   fibblk.fib$w_did_seq,fibblk.fib$b_did_rvn,
                   wcc->wcd_wcc,wcc->wcd_prelen,
                   wccfile->wcf_result + wcc->wcd_prelen);
#endif
            sts = direct(wccfile->wcf_vcb,&fibdsc,&wcc->wcd_serdsc,
                         &wcc->wcd_reslen,&resdsc,0);
        } else {
            sts = SS$_NOMOREFILES;
        }
        if (sts & STS$M_SUCCESS) {
#ifdef DEBUG
            wccfile->wcf_result[wcc->wcd_prelen + wcc->wcd_reslen] = '\0';
            printf("Fnd: '%s'  (%d,%d,%d) WCC: %d\n",
                   wccfile->wcf_result + wcc->wcd_prelen,
                   fibblk.fib$w_fid_num | (fibblk.fib$b_fid_nmx << 16),
                   fibblk.fib$w_fid_seq,fibblk.fib$b_fid_rvn,
                   wcc->wcd_wcc);
#endif
            wcc->wcd_wcc = fibblk.fib$l_wcc;
            if (wcc->wcd_prev) {/* go down directory */
                if (wcc->wcd_prev->wcd_next != wcc) {
                    printf("wcd_PREV corruption\n");
                }
                if (fibblk.fib$w_fid_num != 4 || fibblk.fib$b_fid_nmx != 0 ||
                    wcc == &wccfile->wcf_wcd ||
                    memcmp(wcc->wcd_sernam,"000000.",7) == 0) {
                    memcpy(&wcc->wcd_prev->wcd_dirid,&fibblk.fib$w_fid_num,
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
                if (nam != NULL) {
                    int fna_size[5];
                    memcpy(&nam->nam$w_fid_num,&fibblk.fib$w_fid_num,
                           sizeof(struct fiddef));
                    nam->nam$b_rsl = wcc->wcd_prelen + wcc->wcd_reslen;
                    name_delim(wccfile->wcf_result,nam->nam$b_rsl,fna_size);
                    nam->nam$l_dev = nam->nam$l_rsa;
                    nam->nam$b_dev = fna_size[0];
                    nam->nam$l_dir = nam->nam$l_dev + fna_size[0];
                    nam->nam$b_dir = fna_size[1];
                    nam->nam$l_name = nam->nam$l_dir + fna_size[1];
                    nam->nam$b_name = fna_size[2];
                    nam->nam$l_type = nam->nam$l_name + fna_size[2];
                    nam->nam$b_type = fna_size[3];
                    nam->nam$l_ver = nam->nam$l_type + fna_size[3];
                    nam->nam$b_ver = fna_size[4];
                    if (nam->nam$b_rsl <= nam->nam$b_rss) {
                        memcpy(nam->nam$l_rsa,wccfile->wcf_result,
                               nam->nam$b_rsl);
                    } else {
                        return RMS$_RSS;
                    }
                }
                memcpy(&wccfile->wcf_fid,&fibblk.fib$w_fid_num,
                       sizeof(struct fiddef));
                return SS$_NORMAL;
            }
        } else {
#ifdef DEBUG
            printf("Err: %d\n",sts);
#endif
            if (sts == SS$_BADIRECTORY) {
                if (wcc->wcd_next != NULL) {
                    if (wcc->wcd_next->wcd_status & STATUS_INIT) {
                        sts = SS$_NOMOREFILES;
                    }
                }
            }
            if (sts == SS$_NOMOREFILES) {
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
                    memcpy(wccfile->wcf_result + wcc->wcd_prelen +
                               wcc->wcd_reslen - 6,".DIR;1",6);
#ifdef DEBUG
                    memset(savwcc, 0, savwcc->size);
#endif
                    free(savwcc);
                } else {
                    if ((wccfile->wcf_status & STATUS_RECURSE) &&
                        wcc->wcd_prev == NULL) {
                        struct WCCDIR *newwcc;
                        newwcc = (struct WCCDIR *) calloc(1,WCDALLOC_SIZE(8));
                        if ( newwcc == NULL ) {
                            return SS$_INSFMEM;
                        }
#ifdef DEBUG
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
                        memcpy(&newwcc->wcd_dirid,&wcc->wcd_dirid,
                               sizeof(struct fiddef));
                        newwcc->wcd_serdsc.dsc_w_length = 7;
                        newwcc->wcd_serdsc.dsc_a_pointer = newwcc->wcd_sernam;
                        memcpy(newwcc->wcd_sernam,"*.DIR;1",7);
                        newwcc->wcd_prelen = wcc->wcd_prelen;
                        wcc = newwcc;
                    } else {
                        if (wcc->wcd_next != NULL) {
#ifdef DEBUG
                            if (wcc->wcd_next->wcd_prev != wcc) {
                                printf("wcd_NEXT corruption\n");
                            }
#endif
                            wcc = wcc->wcd_next;        /* backup one level */
                            memcpy(wccfile->wcf_result + wcc->wcd_prelen +
                                       wcc->wcd_reslen - 6,".DIR;1",6);
                        } else {
                            sts = RMS$_NMF;
                            cleanup_wcf(&wccfile);
                            break;      /* giveup */
                        }
                    }
                }
            } else {
                if (sts == SS$_NOSUCHFILE) {
                    if (wcc->wcd_prev) {
                        sts = RMS$_DNF;
                    } else {
                        sts = RMS$_FNF;
                    }
                }
                break;          /* error - abort! */
            }
        }
    }
    if (nam != NULL) {
        nam->nam$l_wcc = NULL;
    }
    fab->fab$w_ifi = 0;         /* must dealloc memory blocks! */
    return sts;
}

/*************************************************************** sys_search() */

/* External entry for search function... */

unsigned sys_search(struct FAB *fab)
{
    struct NAM *nam = fab->fab$l_nam;
    struct WCCFILE *wccfile;
    if (nam == NULL)
        return RMS$_NAM;
    wccfile = (struct WCCFILE *) nam->nam$l_wcc;
    if (wccfile == NULL) return RMS$_WCC;
    return do_search(fab,wccfile);
}

/***************************************************************** do_parse() */

#define DEFAULT_SIZE 120
char default_buffer[DEFAULT_SIZE];
char *default_name = "DKA200:[000000].;";
int default_size[] = {7,8,0,1,1};

/* Function to perform RMS parse.... */

unsigned do_parse(struct FAB *fab,struct WCCFILE **wccret)
{
    struct WCCFILE *wccfile;
    char *fna = fab->fab$l_fna;
    char *dna = fab->fab$l_dna;
    struct NAM *nam = fab->fab$l_nam;
    int sts;
    int fna_size[5] = {0, 0, 0, 0, 0},
        dna_size[5] = {0, 0, 0, 0, 0};
    if (fab->fab$w_ifi != 0)
        return RMS$_IFI;
    if (nam != NULL && nam->nam$l_wcc == 0) {
        struct WCCFILE *wcc = (struct WCCFILE *)nam->nam$l_wcc;
        cleanup_wcf(&wcc);
        nam->nam$l_wcc = 0;
    }
    /* Break up file specifications... */

    sts = name_delim(fna,fab->fab$b_fns,fna_size);
    if (!(sts & STS$M_SUCCESS)) return sts;
    if (dna) {
        sts = name_delim(dna,fab->fab$b_dns,dna_size);
        if (!(sts & STS$M_SUCCESS)) return sts;
    }
    /* Make WCCFILE entry for rest of processing */

    {
        wccfile = (struct WCCFILE *) calloc(1,WCFALLOC_SIZE);
        if (wccfile == NULL)
            return SS$_INSFMEM;
        wccfile->wcf_fab = fab;
        wccfile->wcf_vcb = NULL;
        wccfile->wcf_fcb = NULL;
        wccfile->wcf_status = 0;
        wccfile->wcf_wcd.wcd_status = 0;
    }

    /* Combine file specifications */

    {
        int field,ess = MAX_FILELEN;
        char *esa,*def = default_name;
        esa = wccfile->wcf_result;
        for (field = 0; field < 5; field++) {
            char *src;
            int len = fna_size[field];
            if (len > 0) {
                src = fna;
            } else {
                len = dna_size[field];
                if (len > 0) {
                    src = dna;
                } else {
                    len = default_size[field];
                    src = def;
                }
            }
            fna += fna_size[field];
            if (field == 1) {
                int dirlen = len;
                if (len < 3) {
                    dirlen = len = default_size[field];
                    src = def;
                } else {
                    char ch1 = *(src + 1);
                    char ch2 = *(src + 2);
                    if (ch1 == '.' ||
                        (ch1 == '-' &&
                         (ch2 == '-' || ch2 == '.' || ch2 == ']'))) {
                        char *dir = def;
                        int count = default_size[1] - 1;
                        len--;
                        src++;
                        while (len >= 2 && *src == '-') {
                            len--;
                            src++;
                            if (count < 2 ||
                                (count == 7 && memcmp(dir,"[000000",7) == 0)) {
                                return RMS$_DIR;
                            }
                            while (count > 1) {
                                if (dir[--count] == '.') break;
                            }
                        }
                        if (count < 2 && len < 2) {
                            src = "[000000]";
                            dirlen = len = 8;
                        } else {
                            if (*src != '.' && *src != ']') {
                                /** cleanup_wcf(&wccfile); **/
                                return RMS$_DIR;
                            }
                            if (*src == '.' && count < 2) {
                                src++;
                                len--;
                            }
                            dirlen = len + count;
                            if ((ess -= count) < 0) {
                                /** cleanup_wcf(&wccfile); **/
                                return RMS$_ESS;
                            }
                            memcpy(esa,dir,count);
                            esa += count;
                        }
                    }
                }
                fna_size[field] = dirlen;
            } else {
                fna_size[field] = len;
            }
            dna += dna_size[field];
            def += default_size[field];
            if ((ess -= len) < 0) {
                /** cleanup_wcf(&wccfile); **/
                return RMS$_ESS;
            }
            while (len-- > 0) {
                register char ch;
                *esa++ = ch = *src++;
                if (ch == '*' || ch == '%')
                    wccfile->wcf_status |= STATUS_WILDCARD;
            }
        }
        /* Pass back results... */
        if (nam) {
            nam->nam$l_dev = nam->nam$l_esa;
            nam->nam$b_dev = fna_size[0];
            nam->nam$l_dir = nam->nam$l_dev + fna_size[0];
            nam->nam$b_dir = fna_size[1];
            nam->nam$l_name = nam->nam$l_dir + fna_size[1];
            nam->nam$b_name = fna_size[2];
            nam->nam$l_type = nam->nam$l_name + fna_size[2];
            nam->nam$b_type = fna_size[3];
            nam->nam$l_ver = nam->nam$l_type + fna_size[3];
            nam->nam$b_ver = fna_size[4];
            nam->nam$b_esl = esa - wccfile->wcf_result;
            nam->nam$l_fnb = 0;
            if (wccfile->wcf_status & STATUS_WILDCARD) {
                nam->nam$l_fnb = NAM$M_WILDCARD;
            }
            if (nam->nam$b_esl <= nam->nam$b_ess) {
                memcpy(nam->nam$l_esa,wccfile->wcf_result,nam->nam$b_esl);
            } else {
                /** cleanup_wcf(&wccfile); **/
                return RMS$_ESS;
            }
        }
    }
    sts = SS$_NORMAL;
    if (nam != NULL && (nam->nam$b_nop & NAM$M_SYNCHK))
        sts = 0;

    /* Now build up WCC structures as required */

    if (sts) {
        int dirlen,dirsiz;
        char *dirnam;
        struct WCCDIR *wcc;
        struct DEV *dev;
        sts = device_lookup(fna_size[0],wccfile->wcf_result,FALSE,&dev);
        if ((sts & STS$M_SUCCESS) == 0) {
            /** cleanup_wcf(&wccfile); **/
            return sts;
        }
        if ((wccfile->wcf_vcb = dev->vcb) == NULL) {
            /** cleanup_wcf(&wccfile); **/
            return SS$_DEVNOTMOUNT;
        }
        wcc = &wccfile->wcf_wcd;
        wcc->wcd_prev = NULL;
        wcc->wcd_next = NULL;

        /* find directory name - chop off ... if found */

        dirnam = wccfile->wcf_result + fna_size[0] + 1;
        dirlen = fna_size[1] - 2;       /* Don't include [] */
        if (dirlen >= 3) {
            if (memcmp(dirnam + dirlen - 3,"...",3) == 0) {
                wccfile->wcf_status |= STATUS_RECURSE;
                dirlen -= 3;
                wccfile->wcf_status |= STATUS_WILDCARD;
            }
        }
        /* see if we can find directory in cache... */

        dirsiz = dirlen;
        while (TRUE) {
            char *dirend = dirnam + dirsiz;
            if (dircache(wccfile->wcf_vcb,dirnam,dirsiz,&wcc->wcd_dirid)) break;
            while (dirsiz > 0) {
                dirsiz--;
                if (char_delim[*--dirend & 127]) break;
            }
        }

        /* Create directory wcc blocks for what's left ... */

        while (dirsiz < dirlen) {
            int seglen = 0;
            char *dirptr = dirnam + dirsiz;
            struct WCCDIR *wcd;
            do {
                if (char_delim[*dirptr++ & 127]) break;
                seglen++;
            } while (dirsiz + seglen < dirlen);
            wcd = (struct WCCDIR *) calloc(1,WCDALLOC_SIZE(seglen + 8));
            if ( wcd == NULL ) {
                return SS$_INSFMEM;
            }
#ifdef DEBUG
            wcd->size = WCDALLOC_SIZE(seglen + 8);
#endif
/* calloc()
            wcd->wcd_wcc = 0;
            wcd->wcd_status = 0;
            wcd->wcd_prelen = 0;
            wcd->wcd_reslen = 0;
*/
            memcpy(wcd->wcd_sernam,dirnam + dirsiz,seglen);
            memcpy(wcd->wcd_sernam + seglen,".DIR;1",7);
            wcd->wcd_serdsc.dsc_w_length = seglen + 6;
            wcd->wcd_serdsc.dsc_a_pointer = wcd->wcd_sernam;
            wcd->wcd_prev = wcc;
            wcd->wcd_next = wcc->wcd_next;
            if (wcc->wcd_next != NULL) wcc->wcd_next->wcd_prev = wcd;
            wcc->wcd_next = wcd;
            wcd->wcd_prelen = fna_size[0] + dirsiz + 1; /* Include [. */
            memcpy(&wcd->wcd_dirid,&wccfile->wcf_wcd.wcd_dirid,
                   sizeof(struct fiddef));
            dirsiz += seglen + 1;
        }
        wcc->wcd_wcc = 0;
        wcc->wcd_status = 0;
        wcc->wcd_reslen = 0;
        wcc->wcd_serdsc.dsc_w_length = fna_size[2] + fna_size[3] + fna_size[4];
        wcc->wcd_serdsc.dsc_a_pointer = wcc->wcd_sernam;
        if (wcc->wcd_serdsc.dsc_w_length >= 256) {
            printf( "serdsc length too long %u\n",  wcc->wcd_serdsc.dsc_w_length );
            abort();
        }
        memcpy(wcc->wcd_sernam,wccfile->wcf_result + fna_size[0] + fna_size[1],
               wcc->wcd_serdsc.dsc_w_length);
#ifdef DEBUG
        wcc->wcd_sernam[wcc->wcd_serdsc.dsc_w_length] = '\0';
        printf("Parse spec is %s\n",wccfile->wcf_wcd.wcd_sernam);
        for (dirsiz = 0; dirsiz < 5; dirsiz++) printf("  %d",fna_size[dirsiz]);
        printf("\n");
#endif
    }
    if (wccret != NULL) *wccret = wccfile;
    if (nam != NULL) {
        if( (nam->nam$b_nop & NAM$M_SYNCHK) &&
            !fab->fab$b_dns && !nam->nam$l_rlf ) {
            cleanup_wcf(&wccfile);
            if( wccret ) *wccret = NULL;
        } else {
            nam->nam$l_wcc = wccfile;
        }
    }
    return SS$_NORMAL;
}

/**************************************************************** sys_parse() */

/* External entry for parse function... */

unsigned sys_parse(struct FAB *fab)
{
    struct NAM *nam = fab->fab$l_nam;
    if (nam == NULL) return RMS$_NAM;
    return do_parse(fab,NULL);
}

/************************************************************** sys_setddir() */

/* Function to set default directory (heck we can sneak in the device...)  */

unsigned sys_setddir(struct dsc_descriptor *newdir,unsigned short *oldlen,
                     struct dsc_descriptor *olddir)
{
    unsigned sts = SS$_NORMAL;
    if (oldlen != NULL) {
        int retlen = default_size[0] + default_size[1];
        if (retlen > olddir->dsc_w_length) retlen = olddir->dsc_w_length;
        *oldlen = retlen;
        memcpy(olddir->dsc_a_pointer,default_name,retlen);
    }
    if (newdir != NULL) {
        struct FAB fab = cc$rms_fab;
        struct NAM nam = cc$rms_nam;
        fab.fab$l_nam = &nam;
        nam.nam$b_nop |= NAM$M_SYNCHK;
        nam.nam$b_ess = DEFAULT_SIZE;
        nam.nam$l_esa = default_buffer;
        fab.fab$b_fns = newdir->dsc_w_length;
        fab.fab$l_fna = newdir->dsc_a_pointer;
        sts = sys_parse(&fab);
        if (sts & STS$M_SUCCESS) {
            if (nam.nam$b_name + nam.nam$b_type + nam.nam$b_ver > 2) {
                sts = RMS$_DIR;
            } else {
                if (nam.nam$l_fnb & NAM$M_WILDCARD)
                    sts = RMS$_WLD;
                else {
                    default_name = default_buffer;
                    default_size[0] = nam.nam$b_dev;
                    default_size[1] = nam.nam$b_dir;
                    memcpy(default_name + nam.nam$b_dev + nam.nam$b_dir,".;",3);
                }
            }
        }
        fab.fab$b_dns = 0;
        nam.nam$l_rlf = 0;
        sys$parse( &fab );
    }
    return sts;
}

/************************************************************** sys_connect() */

/* This version of connect only resets record pointer */

unsigned sys_connect(struct RAB *rab)
{
    rab->rab$w_rfa[0] = 0;
    rab->rab$w_rfa[1] = 0;
    rab->rab$w_rfa[2] = 0;
    rab->rab$w_rsz = 0;
    if (rab->rab$l_fab->fab$b_org == FAB$C_SEQ) {
        return SS$_NORMAL;
    } else {
        return SS$_NOTINSTALL;
    }
}

/*********************************************************** sys_disconnect() */

/* Disconnect is even more boring */

unsigned sys_disconnect(struct RAB *rab)
{
    UNUSED(rab);

    return SS$_NORMAL;
}

#define IFI_MAX 64
struct WCCFILE *ifi_table[] = {
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
};

/****************************************************************** sys_get() */

/* get for sequential files */

unsigned sys_get(struct RAB *rab)
{
    char *buffer,*recbuff;
    unsigned block,blocks,offset;
    unsigned cpylen,reclen;
    unsigned delim,rfm,sts;
    struct VIOC *vioc;
    struct FCB *fcb = ifi_table[rab->rab$l_fab->fab$w_ifi]->wcf_fcb;

    reclen = rab->rab$w_usz;
    recbuff = rab->rab$l_ubf;
    delim = 0;
    switch (rfm = rab->rab$l_fab->fab$b_rfm) {
        case FAB$C_STMLF:
            delim = 1;
            break;
        case FAB$C_STMCR:
            delim = 2;
            break;
        case FAB$C_STM:
            delim = 3;
            break;
        case FAB$C_VFC:
            reclen += rab->rab$l_fab->fab$b_fsz;
            break;
        case FAB$C_FIX:
            if (reclen < rab->rab$l_fab->fab$w_mrs)
                return RMS$_RTB;
            reclen = rab->rab$l_fab->fab$w_mrs;
            break;
    }

    offset = rab->rab$w_rfa[2] % 512;
    block = (rab->rab$w_rfa[1] << 16) + rab->rab$w_rfa[0];
    if (block == 0)
        block = 1;
    {
        unsigned eofblk = VMSSWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
        if (block > eofblk || (block == eofblk &&
            offset >= VMSWORD(fcb->head->fh2$w_recattr.fat$w_ffbyte))) {
                return RMS$_EOF;
            }
    }

    sts = accesschunk(fcb,block,&vioc,&buffer,&blocks,0);
    if (!(sts & STS$M_SUCCESS)) {
        if (sts == SS$_ENDOFFILE) sts = RMS$_EOF;
        return sts;
    }

    if (rfm == FAB$C_VAR || rfm == FAB$C_VFC) {
        vmsword *lenptr = (vmsword *) (buffer + offset);
        reclen = VMSWORD(*lenptr);
        offset += 2;
        if (reclen > rab->rab$w_usz) {
            sts = deaccesschunk(vioc,0,0,FALSE);
            return RMS$_RTB;
        } 
    }

    cpylen = 0;
    while (TRUE) {
        int dellen = 0;
        unsigned int seglen = blocks * 512 - offset;
        if (delim) {
            if (delim >= 3) {
                char *ptr = buffer + offset;
                if (dellen == 1 && *ptr != '\n') {
                    if (cpylen >= reclen) {
                        seglen = 0;
                        sts = RMS$_RTB;
                    } else {
                        *recbuff++ = '\r';
                        cpylen++;
                    }
                }
                while (seglen-- > 0) {
                    char ch = *ptr++;
                    if (ch == '\n' || ch == '\f' || ch == '\v') {
                        if (ch == '\n') {
                            dellen++;
                        } else {
                            dellen = 0;
                        }
                        delim = 99;
                        break; 
                    }
                    dellen = 0;
                    if (ch == '\r') dellen = 1;
                }
                seglen = ptr - (buffer + offset) - dellen;;
            } else {
                char *ptr = buffer + offset;
                char term = '\r';
                if (delim == 1)
                    term = '\n';
                while (seglen-- > 0) {
                    if (*ptr++ == term) {
                        dellen = 1;
                        delim = 99;
                        break;
                    }
                }
                seglen = ptr - (buffer + offset) - dellen;;
            }
        } else {
            if (seglen > reclen - cpylen)
                seglen = reclen - cpylen;
            if (rfm == FAB$C_VFC && cpylen < rab->rab$l_fab->fab$b_fsz) {
                unsigned fsz = rab->rab$l_fab->fab$b_fsz - cpylen;
                if (fsz > seglen)
                if (rab->rab$l_rhb) {
                    memcpy(rab->rab$l_rhb + cpylen,buffer + offset,fsz);
                }
                cpylen += fsz;
                offset += fsz;
                seglen -= fsz;
            }
        }
        if (seglen) {
            if (cpylen + seglen > reclen) {
                seglen = reclen - cpylen;
                sts = RMS$_RTB;
            }
            memcpy(recbuff,buffer + offset,seglen);
            recbuff += seglen;
            cpylen += seglen;
        }
        offset += seglen + dellen;
        if ((offset & 1) && (rfm == FAB$C_VAR || rfm == FAB$C_VFC)) offset++;
/* ??? This was missing the "sts = ". Assumed to be an error. (LMB) ??? */
         { unsigned sts2;
            sts2 = deaccesschunk(vioc,0,0,TRUE);
            if( (sts & STS$M_SUCCESS) ) sts = sts2;
        }
        if (!(sts & STS$M_SUCCESS)) return sts;
        block += offset / 512;
        offset %= 512;
        if ((delim == 0 && cpylen >= reclen) || delim == 99) {
            break;
        } else {
            sts = accesschunk(fcb,block,&vioc,&buffer,&blocks,0);
            if (!(sts & STS$M_SUCCESS)) {
                if (sts == SS$_ENDOFFILE) sts = RMS$_EOF;
                return sts;
            }
            offset = 0;
        }
    }
    if (rfm == FAB$C_VFC)
        cpylen -= rab->rab$l_fab->fab$b_fsz;
    rab->rab$w_rsz = cpylen;

    rab->rab$w_rfa[0] = block & 0xffff;
    rab->rab$w_rfa[1] = block >> 16;
    rab->rab$w_rfa[2] = offset;
    return sts;
}

/****************************************************************** sys_put() */

/* put for sequential files */

unsigned sys_put(struct RAB *rab)
{
    char *buffer,*recbuff;
    unsigned block,blocks,offset;
    unsigned cpylen,reclen;
    unsigned delim,rfm,sts;
    struct VIOC *vioc;
    struct FCB *fcb = ifi_table[rab->rab$l_fab->fab$w_ifi]->wcf_fcb;

    reclen = rab->rab$w_rsz;
    recbuff = rab->rab$l_rbf;
    delim = 0;
    switch (rfm = rab->rab$l_fab->fab$b_rfm) {
    case FAB$C_STMLF:
        if (reclen < 1) {
            delim = 1;
        } else {
            if (recbuff[reclen] != '\n') delim = 1;
        }
        break;
    case FAB$C_STMCR:
        if (reclen < 1) {
            delim = 2;
        } else {
            if (recbuff[reclen] != '\r') delim = 2;
        }
        break;
    case FAB$C_STM:
        if (reclen < 2) {
            delim = 3;
        } else {
	        if (recbuff[reclen-1] != '\r' || recbuff[reclen] != '\n') {
                   delim = 3;
                }
        }
        break;
    case FAB$C_VFC:
        reclen += rab->rab$l_fab->fab$b_fsz;
        break;
    case FAB$C_FIX:
        if (reclen != rab->rab$l_fab->fab$w_mrs) return RMS$_RSZ;
        break;
    }

    block = VMSSWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
    offset = VMSWORD(fcb->head->fh2$w_recattr.fat$w_ffbyte);

    sts = accesschunk(fcb,block,&vioc,&buffer,&blocks,1);
    if (!(sts & STS$M_SUCCESS)) return sts;

    if (rfm == FAB$C_VAR || rfm == FAB$C_VFC) {
        vmsword *lenptr = (vmsword *) (buffer + offset);
        *lenptr = VMSWORD(reclen);
        offset += 2;
    }

    cpylen = 0;
    while (TRUE) {
        unsigned int seglen = blocks * 512 - offset;
        if (seglen > reclen - cpylen) seglen = reclen - cpylen;
        if (rfm == FAB$C_VFC && cpylen < rab->rab$l_fab->fab$b_fsz) {
            unsigned fsz = rab->rab$l_fab->fab$b_fsz - cpylen;

            if (fsz > seglen)
                fsz = seglen;
            if (rab->rab$l_rhb) {
                memcpy(buffer + offset,rab->rab$l_rhb + cpylen,fsz);
            } else {
                memset(buffer + offset,0,fsz);
            }
            cpylen += fsz;
            offset += fsz;
            seglen -= fsz;
        }
        if (seglen) {
            memcpy(buffer + offset,recbuff,seglen);
            recbuff += seglen;
            cpylen += seglen;
            offset += seglen;
        }
        if (delim && offset < blocks * 512) {
            offset++;
            switch (delim) {
            case 1:
                *buffer = '\n';
                delim = 0;
                break;	
            case 2:
                *buffer = '\r';
                delim = 0;
                break;	
            case 3:
                *buffer = '\r';
                if (offset < blocks * 512) {
                    offset++;
                    *buffer = '\n';
                    delim = 0;
                } else {
                    delim = 2;
                }
                break;
            }
        }
        sts = deaccesschunk(vioc,block,blocks,TRUE);
        if (!(sts & STS$M_SUCCESS))
            return sts;
        block += blocks;
        if (cpylen >= reclen && delim == 0) {
            break;
        } else {
            sts = accesschunk(fcb,block,&vioc,&buffer,&blocks,1);
            if (!(sts & STS$M_SUCCESS)) return sts;
            offset = 0;
        }
    }
    if ((offset & 1) && (rfm == FAB$C_VAR || rfm == FAB$C_VFC)) offset++;
    block += offset / 512;
    offset %= 512;
    fcb->head->fh2$w_recattr.fat$l_efblk = VMSSWAP(block);
    fcb->head->fh2$w_recattr.fat$w_ffbyte = VMSWORD(offset);
    rab->rab$w_rfa[0] = block & 0xffff;
    rab->rab$w_rfa[1] = block >> 16;
    rab->rab$w_rfa[2] = offset;
    return sts;
}

/************************************************************** sys_display() */

/* display to fill fab & xabs with info from the file header... */

unsigned sys_display(struct FAB *fab)
{
    struct XABDAT *xab = fab->fab$l_xab;

    struct HEAD *head = ifi_table[fab->fab$w_ifi]->wcf_fcb->head;
    unsigned short *pp = (unsigned short *) head;
    struct IDENT *id = (struct IDENT *) (pp + head->fh2$b_idoffset);

    int ifi_no = fab->fab$w_ifi;
    if (ifi_no == 0 || ifi_no >= IFI_MAX)
        return RMS$_IFI;
    fab->fab$l_alq = VMSSWAP(head->fh2$w_recattr.fat$l_hiblk);
    fab->fab$b_bks = head->fh2$w_recattr.fat$b_bktsize;
    fab->fab$w_deq = VMSWORD(head->fh2$w_recattr.fat$w_defext);
    fab->fab$b_fsz = head->fh2$w_recattr.fat$b_vfcsize;
    fab->fab$w_gbc = VMSWORD(head->fh2$w_recattr.fat$w_gbc);
    fab->fab$w_mrs = VMSWORD(head->fh2$w_recattr.fat$w_maxrec);
    fab->fab$b_org = head->fh2$w_recattr.fat$b_rtype & 0xf0;
    fab->fab$b_rfm = head->fh2$w_recattr.fat$b_rtype & 0x0f;
    fab->fab$b_rat = head->fh2$w_recattr.fat$b_rattrib;
    while (xab != NULL) {
        switch (xab->xab$b_cod) {
            case XAB$C_DAT:
                memcpy(&xab->xab$q_bdt,id->fi2$q_bakdate,
                       sizeof(id->fi2$q_bakdate));
                memcpy(&xab->xab$q_cdt,id->fi2$q_credate,
                       sizeof(id->fi2$q_credate));
                memcpy(&xab->xab$q_edt,id->fi2$q_expdate,
                       sizeof(id->fi2$q_expdate));
                memcpy(&xab->xab$q_rdt,id->fi2$q_revdate,
                       sizeof(id->fi2$q_revdate));
                xab->xab$w_rvn = id->fi2$w_revision;
                break;
        case XAB$C_FHC:{
            struct XABFHC *fhc = (struct XABFHC *) xab;
            fhc->xab$b_atr = head->fh2$w_recattr.fat$b_rattrib;
            fhc->xab$b_bkz = head->fh2$w_recattr.fat$b_bktsize;
            fhc->xab$w_dxq = VMSWORD(head->fh2$w_recattr.fat$w_defext);
            fhc->xab$l_ebk = VMSSWAP(head->fh2$w_recattr.fat$l_efblk);
            fhc->xab$w_ffb = VMSWORD(head->fh2$w_recattr.fat$w_ffbyte);
            if (fhc->xab$l_ebk == 0) {
                fhc->xab$l_ebk = fab->fab$l_alq;
                if (fhc->xab$w_ffb == 0) fhc->xab$l_ebk++;
            }
            fhc->xab$w_gbc = VMSWORD(head->fh2$w_recattr.fat$w_gbc);
            fhc->xab$l_hbk = VMSSWAP(head->fh2$w_recattr.fat$l_hiblk);
            fhc->xab$b_hsz = head->fh2$w_recattr.fat$b_vfcsize;
            fhc->xab$w_lrl = VMSWORD(head->fh2$w_recattr.fat$w_maxrec);
                    fhc->xab$w_verlimit =
                        VMSWORD(head->fh2$w_recattr.fat$w_versions);
                       }
                       break;
        case XAB$C_PRO:{
            struct XABPRO *pro = (struct XABPRO *) xab;
            pro->xab$w_pro = VMSWORD(head->fh2$w_fileprot);
            memcpy(&pro->xab$l_uic,&head->fh2$l_fileowner,4);
                       }
                       break;
        case XAB$C_ITM:{
            struct XABITM *itm = (struct XABITM *) xab;
            struct item_list *list;
            vmslong fch = VMSLONG( head->fh2$l_filechar );

            if( itm->xab$b_mode != XAB$K_SENSEMODE )
                break;
            for( list = itm->xab$l_itemlist; list->code != 0 || list->length != 0; list++ ) {
                list->retlen = sizeof(int);
                switch( list->code ) {
                case XAB$_UCHAR_NOBACKUP:
                    *((int *)list->buffer) = (fch & FH2$M_NOBACKUP) != 0; break;
                case XAB$_UCHAR_CONTIGB:
                    *((int *)list->buffer) = (fch & FH2$M_CONTIGB) != 0;   break;
                case XAB$_UCHAR_CONTIG:
                    *((int *)list->buffer) = (fch & FH2$M_CONTIG) != 0;    break;
                case XAB$_UCHAR_DIRECTORY:
                    *((int *)list->buffer) = (fch & FH2$M_DIRECTORY) != 0; break;
                case XAB$_UCHAR_MARKDEL:
                    *((int *)list->buffer) = (fch & FH2$M_MARKDEL) != 0;   break;
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
        xab = xab->xab$l_nxt;
    }

    return SS$_NORMAL;
}

/**************************************************************** sys_close() */

/* close a file */

unsigned sys_close(struct FAB *fab)
{
    int sts;
    int ifi_no = fab->fab$w_ifi;
    if (ifi_no < 1 || ifi_no >= IFI_MAX)
        return RMS$_IFI;
    sts = deaccessfile(ifi_table[ifi_no]->wcf_fcb);
    if (sts & STS$M_SUCCESS) {
        ifi_table[ifi_no]->wcf_fcb = NULL;
        if (ifi_table[ifi_no]->wcf_status & STATUS_TMPWCC) {
            cleanup_wcf(&ifi_table[ifi_no]);
            if ( fab->fab$l_nam != NULL ) {
                fab->fab$l_nam->nam$l_wcc = NULL;
            }
        }
        fab->fab$w_ifi = 0;
        ifi_table[ifi_no] = NULL;
    }
    return sts;
}

/***************************************************************** sys_open() */

/* open a file */

unsigned sys_open(struct FAB *fab)
{
    unsigned sts;
    int ifi_no = 1;
    int wcc_flag = FALSE;
    struct WCCFILE *wccfile = NULL;
    struct NAM *nam = fab->fab$l_nam;

    if (fab->fab$w_ifi != 0) return RMS$_IFI;

    while (ifi_table[ifi_no] != NULL && ifi_no < IFI_MAX)
        ifi_no++;

    if (ifi_no >= IFI_MAX)
        return RMS$_IFI;

    if (nam != NULL) {
        wccfile = (struct WCCFILE *) nam->nam$l_wcc;
    }
    if (wccfile == NULL) {
        sts = do_parse(fab,&wccfile);
        if (sts & STS$M_SUCCESS) {
            wcc_flag = TRUE;
            if (wccfile->wcf_status & STATUS_WILDCARD) {
                sts = RMS$_WLD;
            } else {
                sts = do_search(fab,wccfile);
                if ((sts & 1) == 0)
                    wcc_flag = 0;
            }
            wccfile->wcf_status |= STATUS_TMPWCC;
        }
    } else {
        sts = SS$_NORMAL;
    }
    if (sts & STS$M_SUCCESS) {
        sts = accessfile(wccfile->wcf_vcb,&wccfile->wcf_fid,&wccfile->wcf_fcb,
                         fab->fab$b_fac & (FAB$M_PUT | FAB$M_UPD));
    }
    if (sts & STS$M_SUCCESS) {
        struct HEAD *head = wccfile->wcf_fcb->head;
        ifi_table[ifi_no] = wccfile;
        fab->fab$w_ifi = ifi_no;
        if (head->fh2$w_recattr.fat$b_rtype == 0) {
            head->fh2$w_recattr.fat$b_rtype = FAB$C_STMLF;
        }
        sys_display(fab);
    }
    if (wcc_flag && (!(sts & STS$M_SUCCESS))) {
        cleanup_wcf(&wccfile);
        if (nam != NULL) {
            nam->nam$l_wcc = NULL;
        }
    }
    return sts;
}

/**************************************************************** sys_erase() */

/* blow away a file */

unsigned sys_erase(struct FAB *fab)
{
    unsigned sts;
    int ifi_no = 1;
    int wcc_flag = FALSE;
    struct WCCFILE *wccfile = NULL;
    struct NAM *nam = fab->fab$l_nam;
    if (fab->fab$w_ifi != 0) return RMS$_IFI;
    while (ifi_table[ifi_no] != NULL && ifi_no < IFI_MAX) ifi_no++;
    if (ifi_no >= IFI_MAX) return RMS$_IFI;
    if (nam != NULL) {
        wccfile = (struct WCCFILE *) fab->fab$l_nam->nam$l_wcc;
    }
    if (wccfile == NULL) {
        sts = do_parse(fab,&wccfile);
        if (sts & STS$M_SUCCESS) {
            wcc_flag = TRUE;
            if (wccfile->wcf_status & STATUS_WILDCARD) {
                sts = RMS$_WLD;
            } else {
                sts = do_search(fab,wccfile);
                if ((sts & 1) == 0)
                    wcc_flag = 0;
            }
        }
    } else {
        sts = SS$_NORMAL;
    }
    if (sts & STS$M_SUCCESS) {
        struct fibdef fibblk;
        struct dsc_descriptor fibdsc,serdsc;
        fibdsc.dsc_w_length = sizeof(struct fibdef);
        fibdsc.dsc_a_pointer = (char *) &fibblk;
        serdsc.dsc_w_length = wccfile->wcf_wcd.wcd_reslen;
        serdsc.dsc_a_pointer = wccfile->wcf_result +
                               wccfile->wcf_wcd.wcd_prelen;
        memcpy(&fibblk.fib$w_did_num,&wccfile->wcf_wcd.wcd_dirid,
               sizeof(struct fiddef));
        fibblk.fib$w_nmctl = 0;
        fibblk.fib$l_acctl = 0;
        fibblk.fib$w_fid_num = 0;
        fibblk.fib$w_fid_seq = 0;
        fibblk.fib$b_fid_rvn = 0;
        fibblk.fib$b_fid_nmx = 0;
        fibblk.fib$l_wcc = 0;
        sts = direct(wccfile->wcf_vcb,&fibdsc,&serdsc,NULL,NULL,TRUE);
        if (sts & STS$M_SUCCESS) {
            sts = accesserase(wccfile->wcf_vcb,&wccfile->wcf_fid);
	} else {
	    printf("Direct status is %d\n",sts);
	}
    }
    if (wcc_flag) {
        cleanup_wcf(&wccfile);
        if (nam != NULL) {
            nam->nam$l_wcc = NULL;
        }
    }
    return sts;
}

/*************************************************************** sys_create() */

unsigned sys_create(struct FAB *fab)
{
    unsigned sts;
    int ifi_no = 1;
    int wcc_flag = FALSE;

    struct WCCFILE *wccfile = NULL;
    struct NAM *nam = fab->fab$l_nam;
    if (fab->fab$w_ifi != 0) return RMS$_IFI;
    while (ifi_table[ifi_no] != NULL && ifi_no < IFI_MAX) ifi_no++;
    if (ifi_no >= IFI_MAX) return RMS$_IFI;
    if (nam != NULL) {
        wccfile = (struct WCCFILE *) fab->fab$l_nam->nam$l_wcc;
    }
    if (wccfile == NULL) {
        sts = do_parse(fab,&wccfile);
        if (sts & STS$M_SUCCESS) {
            wcc_flag = TRUE;
            if (wccfile->wcf_status & STATUS_WILDCARD) {
                sts = RMS$_WLD;
            } else {
                sts = do_search(fab,wccfile);
                if( !(sts & STS$M_SUCCESS) )
                    wcc_flag = 0;
                if (sts == RMS$_FNF) sts = SS$_NORMAL;
            }
        }
    } else {
        sts = SS$_NORMAL;
    }
    if (sts & STS$M_SUCCESS) {
        struct fibdef fibblk;
        struct dsc_descriptor fibdsc; /* ,serdsc; */
        fibdsc.dsc_w_length = sizeof(struct fibdef);
        fibdsc.dsc_a_pointer = (char *) &fibblk;
        /* Unused
        serdsc.dsc_w_length = wccfile->wcf_wcd.wcd_reslen;
        serdsc.dsc_a_pointer = wccfile->wcf_result +
                               wccfile->wcf_wcd.wcd_prelen;
        */
        memcpy(&fibblk.fib$w_did_num,&wccfile->wcf_wcd.wcd_dirid,
               sizeof(struct fiddef));
        fibblk.fib$w_nmctl = 0;
        fibblk.fib$l_acctl = 0;
        fibblk.fib$w_did_num = 11;
        fibblk.fib$w_did_seq = 1;
        fibblk.fib$b_did_rvn = 0;
        fibblk.fib$b_did_nmx = 0;
        fibblk.fib$l_wcc = 0;
        sts = update_create(wccfile->wcf_vcb,
                            (struct fiddef *)&fibblk.fib$w_did_num,
		            "TEST.FILE;1",
                            (struct fiddef *)&fibblk.fib$w_fid_num,
                            &wccfile->wcf_fcb);
        if (sts & STS$M_SUCCESS)
            sts = direct(wccfile->wcf_vcb,&fibdsc,
		&wccfile->wcf_wcd.wcd_serdsc,NULL,NULL,2);
            if (sts & STS$M_SUCCESS) {
                sts = update_extend(wccfile->wcf_fcb,100,0);
                ifi_table[ifi_no] = wccfile;
                fab->fab$w_ifi = ifi_no;
	    }
    }
    if( wcc_flag ) {
       cleanup_wcf(&wccfile);
        if( nam != NULL )
            nam->nam$l_wcc = NULL;
    }
    return sts;
}

/*************************************************************** sys_extend() */

unsigned sys_extend(struct FAB *fab)
{
    int sts;
    int ifi_no = fab->fab$w_ifi;
    if (ifi_no < 1 || ifi_no >= IFI_MAX) return RMS$_IFI;
    sts = update_extend(ifi_table[ifi_no]->wcf_fcb,
                        fab->fab$l_alq - ifi_table[ifi_no]->wcf_fcb->hiblock,0);
    return sts;
}

/*************************************************************** sys_initialize() */
static void sys_rundown( void ) {
    access_rundown();

    sysmsg_rundown();
}

/*************************************************************** sys_initialize() */
unsigned sys_initialize( void ) {
    if( atexit( sys_rundown ) != 0 )
        return SS$_INSFMEM;

    return SS$_NORMAL;
}
