/* Direct.c V2.1 */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

/*  This module does all directory file handling - mostly
    lookups of filenames in directory files... */

#define DEBUGx on

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "access.h"
#include "descrip.h"
#include "direct.h"
#include "fibdef.h"
#include "ssdef.h"
#include "stsdef.h"

#ifndef TRUE
#define TRUE ( 0 == 0 )
#endif
#ifndef FALSE
#define FALSE ( 0 != 0 )
#endif

#define BLOCKSIZE 512
#define MAXREC (BLOCKSIZE - 2)


/* Some statistical counters... */

static int direct_lookups = 0;
static int direct_searches = 0;
static int direct_deletes = 0;
static int direct_inserts = 0;
static int direct_splits = 0;
static int direct_checks = 0;
static int direct_matches = 0;

/************************************************************** direct_show() */

/* direct_show - to print directory statistics */

void direct_show( void ) {

    printf( "DIRECT_SHOW Lookups: %d ", direct_lookups );
    printf( "Searches: %d ", direct_searches );
    printf( "Deletes: %d ", direct_deletes );
    printf( "Inserts: %d ", direct_inserts );
    printf( "Splits: %d\n", direct_splits );
}

/*************************************************************** name_check() */

/* name_check() - take a name specification and return name length without
           the version number, an integer version number, and a wildcard flag */

unsigned name_check(char *str,int len,int *retlen,int *retver,int *wildflag)
{
    int wildcard = FALSE;
    char *name_start = str;
    register int dots = 0;
    register char *name = name_start;
    register char *name_end = name + len;
    direct_checks++;

    /* Go through the specification checking for illegal characters */

    while (name < name_end) {
        register char ch = *name++;
        if (ch == '.') {
            if ((name - name_start) > 40) return SS$_BADFILENAME;
            name_start = name;
            if (dots++ > 1) break;
        } else {
            if (ch == ';') {
                break;
            } else {
                if (ch == '*' || ch == '%') {
                    wildcard = TRUE;
                } else {
                    if (ch == '[' || ch == ']' || ch == ':' ||
                        !isprint(ch)) return SS$_BADFILENAME;
                }
            }
        }
    }
    if ((name - name_start) > 40) return SS$_BADFILENAME;

    /* Return the name length and start checking the version */

    *retlen = name - str - 1;
    dots = 0;
    if (name < name_end) {
        register char ch = *name;
        if (ch == '*') {
            if (++name < name_end) return SS$_BADFILENAME;
            dots = 32768;       /* Wildcard representation of version! */
            wildcard = TRUE;
        } else {
            register int sign = 1;
            if (ch == '-') {
                name++;
                sign = -1;
            }
            while (name < name_end) {
                ch = *name++;
                if (!isdigit(ch)) return SS$_BADFILENAME;
                dots = dots * 10 + (ch - '0');
            }
            dots *= sign;
        }
    }
    *retver = dots;
    *wildflag = wildcard;
    return SS$_NORMAL;
}

/*************************************************************** name_match() */

#define MAT_LT 0
#define MAT_EQ 1
#define MAT_GT 2
#define MAT_NE 3

/* name_match() - compare a name specification with a directory entry
               and determine if there is a match, too big, too small... */

int name_match(char *spec,int spec_len,char *dirent,int dirent_len)
{
    int percent = MAT_GT;
    register char *name = spec,*entry = dirent;
    register char *name_end = name + spec_len,*entry_end = entry + dirent_len;
    direct_matches++;

    /* See how much name matches without wildcards... */

    while (name < name_end && entry < entry_end) {
        register char sch = *name;
        if (sch != '*') {
            register char ech = *entry;
            if (sch != ech) {
              if (toupper(sch) != toupper(ech)) {
                if (sch == '%') {
                  percent = MAT_NE;
                } else {
                  break;
                }
              }
            }
        } else {
            break;
        }
        name++;
        entry++;
    }

    /* Mismatch - return result unless wildcard... */

    if (name >= name_end) {
        if (entry >= entry_end) {
            return MAT_EQ;
        } else {
            return percent;
        }
    } else {

        /* See if we can find a match with wildcards */

        if (*name != '*') {
            if (percent == MAT_NE) return MAT_NE;
            if (entry < entry_end)
                if (toupper(*entry) > toupper(*name)) return MAT_GT;
            return MAT_LT;
        }
        /* Strip out wildcard(s) - if end then we match! */

        do {
            name++;
        } while (name < name_end && *name == '*');
        if (name >= name_end) return MAT_EQ;

        /* Proceed to examine the specification past the wildcard... */

        while (name < name_end) {
            register int offset = 1;
            register char fch = toupper(*name++);

            /* See if can can find a match for the first character... */

            if (fch != '%') {
                while (entry < entry_end) {
                    if (toupper(*entry) != fch) {
                        entry++;
                    } else {
                        break;
                    }
                }
            }
            /* Give up if we can't find that one lousy character... */

            if (entry >= entry_end) return MAT_NE;
            entry++;

            /* See how much of the rest we can match... */

            while (name < name_end && entry < entry_end) {
                register char sch = *name,ech;
                if (sch == '*') break;
                if (sch != (ech = *entry))
                    if (toupper(sch) != toupper(ech))
                        if (sch != '%') break;
                name++;
                entry++;
                offset++;
            }

            /* If matching died because of a wildcard we are OK... */

            if (name < name_end && *name == '*') {
                do {
                    name++;
                } while (name < name_end && *name == '*');
                if (name >= name_end) return MAT_EQ;

                /* Otherwise we finished OK or we need to try again... */

            } else {
                if (name >= name_end && entry >= entry_end) return MAT_EQ;
                name -= offset;
                entry -= offset - 1;
            }
        }
    }

    /* No more specification - match depends on remainder of entry... */

    if (entry < entry_end) return MAT_NE;
    return MAT_EQ;
}

/*************************************************************** insert_ent() */

/* insert_ent() - procedure to add a directory entry at record dr entry de */

unsigned insert_ent(struct FCB * fcb,unsigned eofblk,unsigned curblk,
                    struct VIOC * vioc,char *buffer,
                    struct dir$rec * dr,struct dir$ent * de,
                    char *filename,unsigned filelen,
                    unsigned version,struct fiddef * fid)
{
    register unsigned sts = SS$_NORMAL;
    register int inuse = 0;

    /* Compute space required... */

    register int addlen = sizeof(struct dir$ent);
    direct_inserts++;
    if (de == NULL)
        addlen += (filelen + sizeof(struct dir$rec)) & ~1;

    /* Compute block space in use ... */

    {
        int invalid_dir = TRUE;
        while (TRUE) {
            register int sizecheck;
            register struct dir$rec *nr = (struct dir$rec *) (buffer + inuse);
            if (dr == nr) invalid_dir = FALSE;
            sizecheck = VMSWORD(nr->dir$size);
            if (sizecheck == 0xffff)
                break;
            sizecheck += 2;
            inuse += sizecheck;
            sizecheck -= (nr->dir$namecount + sizeof(struct dir$rec)) & ~1;
            if (inuse > MAXREC || (inuse & 1) || sizecheck <= 0 ||
                sizecheck % sizeof(struct dir$ent) != 0) {
                deaccesschunk(vioc,0,0,FALSE);
                return SS$_BADIRECTORY;
            }
        };

        if (invalid_dir) {
            printf("BUGCHECK invalid dir\n");
            exit(EXIT_FAILURE);
        }
        if (de != NULL) {
            if (VMSWORD(dr->dir$size) > MAXREC ||
                (char *) de < dr->dir$name + dr->dir$namecount ||
                (char *) de > (char *) dr + VMSWORD(dr->dir$size) + 2) {
                printf("BUGCHECK invalid de\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    /* If not enough space free extend the directory... */

    if (addlen > MAXREC - inuse) {
        register struct dir$rec *nr;
        unsigned keep_new = 0;
        char *newbuf;
        struct VIOC *newvioc;
        unsigned newblk = eofblk + 1;
        direct_splits++;
        printf("Splitting record... %s %u,%u,%u,%u\n", dr->dir$name,de->dir$fid.fid$w_num,
               de->dir$fid.fid$w_seq,de->dir$fid.fid$b_rvn,de->dir$fid.fid$b_nmx);
        if (newblk > fcb->hiblock) {
            printf("I can't extend a directory yet!!\n");
            exit(EXIT_FAILURE);
        }
        fcb->highwater = 0;
        sts = accesschunk(fcb,newblk,&newvioc,&newbuf,NULL,1);
        if (sts & STS$M_SUCCESS) {
            while (newblk > curblk + 1) {
                char *frombuf;
                struct VIOC *fromvioc;
                sts = accesschunk(fcb,newblk - 1,&fromvioc,&frombuf,NULL,1);
                if (!(sts & STS$M_SUCCESS)) break;
                memcpy(newbuf,frombuf,BLOCKSIZE);;
                sts = deaccesschunk(newvioc,newblk,1,TRUE);
                newvioc = fromvioc;
                newbuf = frombuf;
                newblk--;
                if (!(sts & STS$M_SUCCESS)) break;
            }
        } else {
            newvioc = NULL;
        }
        if (!(sts & STS$M_SUCCESS)) {
            if (newvioc != NULL) deaccesschunk(newvioc,0,0,FALSE);
            deaccesschunk(vioc,0,0,FALSE);
            return sts;
        }
        memset(newbuf,0,BLOCKSIZE);
        eofblk++;
        fcb->head->fh2$w_recattr.fat$l_efblk = VMSWORD(eofblk + 1);

        /* First find where the next record is... */

        nr = dr;
        if (VMSWORD(nr->dir$size) <= MAXREC)
            nr = (struct dir$rec *) ((char *) nr + VMSWORD(nr->dir$size) + 2);

        /* Can we split between records? */

        if (de == NULL || (char *) dr != buffer ||
            VMSWORD(nr->dir$size) <= MAXREC) {
            register struct dir$rec *sp = dr;
            if ((char *) dr == buffer && de != NULL) sp = nr;
            memcpy(newbuf,sp,((buffer + BLOCKSIZE) - (char *) sp));
            memset(sp,0,((buffer + BLOCKSIZE) - (char *) sp));
            sp->dir$size = VMSWORD(0xffff);
            if (sp == dr &&
                (de != NULL || (char *) sp >= buffer + MAXREC - addlen)) {
                if (de != NULL)
                    de = (struct dir$ent *)
                        (newbuf + ((char *) de - (char *) sp));
                dr = (struct dir$rec *) (newbuf + ((char *) dr - (char *) sp));
                keep_new = 1;
            }
            /* OK, we have to split the record then.. */

        } else {
            register unsigned reclen = (dr->dir$namecount +
                                        sizeof(struct dir$rec)) & ~1;
            register struct dir$rec *nbr = (struct dir$rec *) newbuf;
            printf("Super split %s %u,%u,%u,%u\n", dr->dir$name,de->dir$fid.fid$w_num,
               de->dir$fid.fid$w_seq,de->dir$fid.fid$b_rvn,de->dir$fid.fid$b_nmx);
            memcpy(newbuf,buffer,reclen);
            memcpy(newbuf + reclen,de,((char *) nr - (char *) de) + 2);
            nbr->dir$size = VMSWORD(reclen + ((char *) nr - (char *) de) - 2);

            memset((char *) de + 2,0,((char *) nr - (char *) de));
            ((struct dir$rec *) de)->dir$size = VMSWORD(0xffff);
            dr->dir$size = VMSWORD(((char *) de - (char *) dr) - 2);
            if ((char *) de >= (char *) nr) {
                dr = (struct dir$rec *) newbuf;
                de = (struct dir$ent *) (newbuf + reclen);
                keep_new = 1;
            }
        }

        /* Need to decide which buffer we are going to keep (to write to) */

        if (keep_new) {
            sts = deaccesschunk(vioc,curblk,1,TRUE);
            curblk = newblk;
            vioc = newvioc;
            buffer = newbuf;
        } else {
            sts = deaccesschunk(newvioc,newblk,1,TRUE);
        }
        if (!(sts & STS$M_SUCCESS)) printf("Bad status %d\n",sts);
    }
    /* After that we can just add the record or entry as appropriate... */

    if (de == NULL) {
        memmove((char *) dr + addlen,dr,
                BLOCKSIZE - (((char *) dr + addlen) - buffer));
        dr->dir$size = VMSWORD(addlen - 2);
        dr->dir$verlimit = 0;
        dr->dir$flags = 0;
        dr->dir$namecount = filelen;
        memcpy(dr->dir$name,filename,filelen);
        de = (struct dir$ent *)
                 ((char *) dr + (addlen - sizeof(struct dir$ent)));
    } else {
        dr->dir$size = VMSWORD(VMSWORD(dr->dir$size) + addlen);
        memmove((char *) de + addlen,de,
                BLOCKSIZE - (((char *) de + addlen) - buffer));
    }

    /* Write the entry values are we are done! */

    de->dir$version = VMSWORD(version);
    fid_copy(&de->dir$fid,fid,0);
    return deaccesschunk(vioc,curblk,1,TRUE);
}

/*************************************************************** delete_ent() */

/* delete_ent() - delete a directory entry */

unsigned delete_ent(struct FCB * fcb,struct VIOC * vioc,unsigned curblk,
                    struct dir$rec * dr,struct dir$ent * de,
                    char *buffer,unsigned eofblk)
{
    unsigned sts = SS$_NORMAL;
    unsigned ent;
    direct_deletes++;
    ent = (VMSWORD(dr->dir$size) - sizeof(struct dir$rec)
           - dr->dir$namecount + 3) / sizeof(struct dir$ent);
    if (ent > 1) {
        char *ne = (char *) de + sizeof(struct dir$ent);
        memcpy(de,ne,BLOCKSIZE - (ne - buffer));
        dr->dir$size = VMSWORD(VMSWORD(dr->dir$size) - sizeof(struct dir$ent));
    } else {
        char *nr = (char *) dr + VMSWORD(dr->dir$size) + 2;
        if (eofblk == 1 || (char *) dr > buffer ||
            (nr <= buffer + MAXREC && (unsigned short) *nr < BLOCKSIZE)) {
            memcpy(dr,nr,BLOCKSIZE - (nr - buffer));
        } else {
            while (curblk < eofblk) {
                char *nxtbuffer;
                struct VIOC *nxtvioc;
                sts = accesschunk(fcb,curblk + 1,&nxtvioc,&nxtbuffer,NULL,1);
                if (!(sts & STS$M_SUCCESS)) break;
                memcpy(buffer,nxtbuffer,BLOCKSIZE);
                sts = deaccesschunk(vioc,curblk++,1,TRUE);
                if (!(sts & STS$M_SUCCESS)) break;
                buffer = nxtbuffer;
                vioc = nxtvioc;
            }
            if (sts & STS$M_SUCCESS) {
                fcb->head->fh2$w_recattr.fat$l_efblk = VMSSWAP(eofblk);
                eofblk--;
            }
        }
    }
    {
        unsigned retsts = deaccesschunk(vioc,curblk,1,TRUE);
        if (sts & STS$M_SUCCESS) sts = retsts;
        return sts;
    }
}

/*************************************************************** return_ent() */

/* return_ent() - return information about a directory entry */

unsigned return_ent(struct FCB * fcb,struct VIOC * vioc,unsigned curblk,
                    struct dir$rec * dr,struct dir$ent * de,struct fibdef * fib,
                    unsigned short *reslen,struct dsc_descriptor * resdsc,
                    int wildcard)
{
    register int scale = 10;
    register int version = VMSWORD(de->dir$version);
    register int length = dr->dir$namecount;
    register char *ptr = resdsc->dsc_a_pointer;
    register int outlen = resdsc->dsc_w_length;
    if (length > outlen) length = outlen;
    memcpy(ptr,dr->dir$name,length);
    while (version >= scale) scale *= 10;
    ptr += length;
    if (length < outlen) {
        *ptr++ = ';';
        length++;
        do {
            if (length >= outlen) break;
            scale /= 10;
            *ptr++ = version / scale + '0';
            version %= scale;
            length++;
        } while (scale > 1);
    }
    *reslen = length;
    fid_copy((struct fiddef *)&fib->fib$w_fid_num,&de->dir$fid,0);
    if (fib->fib$b_fid_rvn == 0) fib->fib$b_fid_rvn = fcb->rvn;
    if (wildcard || (fib->fib$w_nmctl & FIB$M_WILD)) {
        fib->fib$l_wcc = curblk;
    } else {
        fib->fib$l_wcc = 0;
    }
    return deaccesschunk(vioc,0,0,TRUE);
}

/*************************************************************** search_ent() */

/* search_ent() - search for a directory entry */

unsigned search_ent(struct FCB *fcb,
                    struct dsc_descriptor *fibdsc,
                    struct dsc_descriptor *filedsc,unsigned short *reslen,
                    struct dsc_descriptor *resdsc,unsigned eofblk,
                    unsigned action)
{
    register unsigned sts,curblk;
    struct VIOC *vioc = NULL;
    char *searchspec,*buffer;
    int searchlen,version,wildcard,wcc_flag;
    struct fibdef *fib = (struct fibdef *) fibdsc->dsc_a_pointer;
    direct_lookups++;

    /* 1) Generate start block (wcc gives start point)
       2) Search for start
       3) Scan until found or too big or end   */

    curblk = fib->fib$l_wcc;
    if (curblk != 0) {
        searchspec = resdsc->dsc_a_pointer;
        sts = name_check(searchspec,*reslen,&searchlen,&version,&wildcard);
        if (action || wildcard) sts = SS$_BADFILENAME;
        wcc_flag = TRUE;
    } else {
        searchspec = filedsc->dsc_a_pointer;
        sts = name_check(searchspec,filedsc->dsc_w_length,&searchlen,&version,
                         &wildcard);
        if ((action && wildcard) || (action > 1 && version < 0)) {
            sts = SS$_BADFILENAME;
        }
        wcc_flag = FALSE;
    }
    if (!(sts & STS$M_SUCCESS)) return sts;

    /* Identify starting block...*/

    if (*searchspec == '*' || *searchspec == '%') {
        curblk = 1;
    } else {
        register unsigned loblk = 1,hiblk = eofblk;
        if (curblk < 1 || curblk > eofblk) curblk = (eofblk + 1) / 2;
        while (loblk < hiblk) {
            register int cmp;
            register unsigned newblk;
            register struct dir$rec *dr;
            direct_searches++;
            sts = accesschunk(fcb,curblk,&vioc,&buffer,NULL,action ? 1 : 0);
            if (!(sts & STS$M_SUCCESS)) return sts;
            dr = (struct dir$rec *) buffer;
            if (VMSWORD(dr->dir$size) > MAXREC) {
                cmp = MAT_GT;
            } else {
                cmp = name_match(searchspec,searchlen,dr->dir$name,
                                 dr->dir$namecount);
                if (cmp == MAT_EQ) {
                    if (wildcard || version < 1 || version > 32767) {
                        cmp = MAT_NE;   /* no match - want to find start */
                    } else {
                        register struct dir$ent *de =
                            (struct dir$ent *)
                                (dr->dir$name + ((dr->dir$namecount + 1) & ~1));
                        if (VMSWORD(de->dir$version) < version) {
                            cmp = MAT_GT;       /* too far... */
                        } else {
                            if (VMSWORD(de->dir$version) > version) {
                                cmp = MAT_LT;   /* further ahead... */
                            }
                        }
                    }
                }
            }
            switch (cmp) {
                case MAT_LT:
                    if (curblk == fib->fib$l_wcc) {
                        newblk = hiblk = loblk = curblk;
                    } else {
                        loblk = curblk;
                        newblk = (loblk + hiblk + 1) / 2;
                    }
                    break;
                case MAT_GT:
                case MAT_NE:
                    newblk = (loblk + curblk) / 2;
                    hiblk = curblk - 1;
                    break;
                default:
                    newblk = hiblk = loblk = curblk;
            }
            if (newblk != curblk) {
                sts = deaccesschunk(vioc,0,0,TRUE);
                if (!(sts & STS$M_SUCCESS)) return sts;
                vioc = NULL;
                curblk = newblk;
            }
        }
    }

    /* Now to read sequentially to find entry... */

    {
        char last_name[80];
        unsigned last_len = 0;
        register int relver = 0;
        while ((sts & STS$M_SUCCESS) && curblk <= eofblk) {
            register struct dir$rec *dr;
            register int cmp = MAT_LT;

            /* Access a directory block. Reset relative version if it starts
               with a record we haven't seen before... */

            if (vioc == NULL) {
                sts = accesschunk(fcb,curblk,&vioc,&buffer,NULL,action ? 1 : 0);
                if (!(sts & STS$M_SUCCESS)) return sts;
            }
            dr = (struct dir$rec *) buffer;
            if (last_len != dr->dir$namecount) {
                relver = 0;
            } else {
                if (name_match(last_name,last_len,dr->dir$name,last_len) !=
                    MAT_EQ) {
                    relver = 0;
                }
            }

            /* Now loop through the records seeing which match our spec... */

            while (TRUE) {        /* dr records within block */
                register char *nr = (char *) dr + VMSWORD(dr->dir$size) + 2;
                if (nr >= buffer + BLOCKSIZE) break;
                if (dr->dir$name + dr->dir$namecount >= nr) break;
                cmp = name_match(searchspec,searchlen,dr->dir$name,
                                 dr->dir$namecount);
                if (cmp == MAT_GT && wcc_flag) {
                    wcc_flag = FALSE;
                    searchspec = filedsc->dsc_a_pointer;
                    sts = name_check(searchspec,filedsc->dsc_w_length,
                                     &searchlen,&version,&wildcard);
                    if (!(sts & STS$M_SUCCESS)) break;
                } else {
                    if (cmp == MAT_EQ) {
                        register struct dir$ent *de;
                        de = (struct dir$ent *)
                                 (dr->dir$name +
                                  ((dr->dir$namecount + 1) & ~1));
                        if (version == 0 && action == 2) {
                            version = VMSWORD(de->dir$version) + 1;
                            if (version > 32767) {
                                sts = SS$_BADFILENAME;
                                break;
                            }
                        }
                        /* Look at each directory entry to see
                           if it is what we want...    */

                        if ((char *) dr != buffer) relver = 0;
                        cmp = MAT_LT;
                        while ((char *) de < nr) {
                            if ((version < 1) ?
                                    (relver > version) :
                                    (version < VMSWORD(de->dir$version))) {
                                relver--;
                                de++;
                            } else {
                                if (version > 32767 || version == relver ||
                                    version == VMSWORD(de->dir$version)) {
                                    cmp = MAT_EQ;
                                } else {
                                    cmp = MAT_GT;
                                }
                                if (!wcc_flag) {
                                    break;
                                } else {
                                    wcc_flag = FALSE;
                                    searchspec = filedsc->dsc_a_pointer;
                                    sts = name_check(searchspec,
                                                     filedsc->dsc_w_length,
                                                     &searchlen,&version,
                                                     &wildcard);
                                    if (!(sts & STS$M_SUCCESS)) break;
                                    if (name_match(searchspec,searchlen,
                                                   dr->dir$name,
                                                   dr->dir$namecount) !=
                                        MAT_EQ) {
                                        cmp = MAT_NE;
                                        break;
                                    }
                                    if (version < 0) {
                                        relver = -32768;
                                        cmp = MAT_GT;
                                        break;
                                    }
                                    if (cmp == MAT_EQ) {
                                        relver--;
                                        de++;
                                    }
                                    cmp = MAT_LT;
                                }
                            }
                        }
                        if (!(sts & STS$M_SUCCESS)) break;

                        /* Decide what to do with the entry we have found... */

                        if (cmp == MAT_EQ) {
                            switch (action) {
                                case 0:
                                    return return_ent(fcb,vioc,curblk,dr,de,fib,
                                                      reslen,resdsc,wildcard);
                                case 1:
                                    return delete_ent(fcb,vioc,curblk,dr,de,
                                                      buffer,eofblk);
                                default:
                                    sts = SS$_DUPFILENAME;
                                    break;
                            }
                        } else {
                            if (cmp != MAT_NE && action == 2) {
                                return insert_ent(fcb,eofblk,curblk,vioc,buffer,
                                                  dr,de,searchspec,searchlen,
                                                  version,
                                                  (struct fiddef *)
                                                      &fib->fib$w_fid_num);
                            }
                        }
                    }
                    /*  Finish unless we expect more... */

                    if (cmp == MAT_GT && !wildcard) break;

                    /* If this is the last record in the block store the name
                       so that if it continues into the next block we can get
                       the relative version right! Sigh! */

                    if (VMSWORD(((struct dir$rec *) nr)->dir$size) > MAXREC) {
                        last_len = dr->dir$namecount;
                        if (last_len > sizeof(last_name)) {
                            last_len = sizeof(last_name);
                        }
                        memcpy(last_name,dr->dir$name,last_len);
                        dr = (struct dir$rec *) nr;
                        break;
                    }
                    dr = (struct dir$rec *) nr;
                }
            }

            /* Release the buffer ready to get the next one - unless it is the
               last one in which case we can't defer an insert any longer!! */

            if (!(sts & STS$M_SUCCESS) || action != 2 ||
                (cmp != MAT_GT && curblk < eofblk)) {
                register unsigned dests = deaccesschunk(vioc,0,0,TRUE);
                if (!(dests & STS$M_SUCCESS)) {
                    sts = dests;
                    break;
                }
                vioc = NULL;
                curblk++;
            } else {
                if (version == 0) version = 1;
                return insert_ent(fcb,eofblk,curblk,vioc,buffer,dr,NULL,
                                  searchspec,searchlen,version,
                                  (struct fiddef *) & fib->fib$w_fid_num);
            }
        }                       /* curblk blocks within file */
    }

    /* We achieved nothing! Report the failure... */

    if (sts & STS$M_SUCCESS) {
        fib->fib$l_wcc = 0;
        if (wcc_flag || wildcard) {
            sts = SS$_NOMOREFILES;
        } else {
            sts = SS$_NOSUCHFILE;
        }
    }
    return sts;
}

/******************************************************************* direct() */

/* direct() - this routine handles all directory manipulations:-
         action 0 - find directory entry
                1 - delete entry
                2 - create an entry   */

unsigned direct(struct VCB * vcb,struct dsc_descriptor * fibdsc,
                struct dsc_descriptor * filedsc,unsigned short *reslen,
                struct dsc_descriptor * resdsc,unsigned action)
{
    struct FCB *fcb;
    register unsigned sts,eofblk;
    register struct fibdef *fib = (struct fibdef *) fibdsc->dsc_a_pointer;
    sts = accessfile(vcb,(struct fiddef *) & fib->fib$w_did_num,&fcb,action);
    if (sts & STS$M_SUCCESS) {
        if (VMSLONG(fcb->head->fh2$l_filechar) & FH2$M_DIRECTORY) {
            eofblk = VMSSWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
            if (VMSWORD(fcb->head->fh2$w_recattr.fat$w_ffbyte) == 0) --eofblk;
            sts = search_ent(fcb,fibdsc,filedsc,reslen,resdsc,eofblk,action);
        } else {
            sts = SS$_BADIRECTORY;
        }
        {
            register unsigned dests = deaccessfile(fcb);
            if (sts & STS$M_SUCCESS) sts = dests;
        }
    }
    return sts;
}
