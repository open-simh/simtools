/* Direct.c */

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

/*  This module does all directory file handling - mostly
 *  lookups of filenames in directory files...
 */

#if !defined( DEBUG ) && defined( DEBUG_DIRECT )
#define DEBUG DEBUG_DIRECT
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#define DEBUGx on

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "access.h"
#include "descrip.h"
#include "direct.h"
#include "fibdef.h"
#include "ods2.h"
#include "ssdef.h"
#include "stsdef.h"
#include "sysmsg.h"

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

static vmscond_t name_check( char *str, int len, int *retlen,
                             int *retver, int *wildflag ) {
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
            if ((name - name_start) > 40)
                return SS$_BADFILENAME;
            name_start = name;
            if (dots++ > 1)
                break;
        } else {
            if (ch == ';') {
                break;
            } else {
                if (ch == '*' || ch == '%') {
                    wildcard = TRUE;
                } else {
                    if (ch == '[' || ch == ']' || ch == ':' ||
                        !isprint(ch))
                        return SS$_BADFILENAME;
                }
            }
        }
    }
    if ((name - name_start) > 40)
        return SS$_BADFILENAME;

    /* Return the name length and start checking the version */

    *retlen = (int)(name - str - 1);
    dots = 0;
    if (name < name_end) {
        register char ch = *name;
        if (ch == '*') {
            if (++name < name_end)
                return SS$_BADFILENAME;
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
                if (!isdigit(ch))
                    return SS$_BADFILENAME;
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

int name_match(char *spec, int spec_len, char *dirent, int dirent_len) {
    int percent = MAT_GT;
    register char *name = spec, *entry = dirent;
    register char *name_end = name + spec_len, *entry_end = entry + dirent_len;
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
            if (percent == MAT_NE)
                return MAT_NE;
            if (entry < entry_end)
                if (toupper(*entry) > toupper(*name))
                    return MAT_GT;
            return MAT_LT;
        }
        /* Strip out wildcard(s) - if end then we match! */

        do {
            name++;
        } while (name < name_end && *name == '*');
        if (name >= name_end)
            return MAT_EQ;

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

            if (entry >= entry_end)
                return MAT_NE;
            entry++;

            /* See how much of the rest we can match... */

            while (name < name_end && entry < entry_end) {
                register char sch = *name, ech;
                if (sch == '*')
                    break;
                if (sch != (ech = *entry))
                    if (toupper(sch) != toupper(ech))
                        if (sch != '%')
                            break;
                name++;
                entry++;
                offset++;
            }

            /* If matching died because of a wildcard we are OK... */

            if (name < name_end && *name == '*') {
                do {
                    name++;
                } while (name < name_end && *name == '*');
                if (name >= name_end)
                    return MAT_EQ;

                /* Otherwise we finished OK or we need to try again... */

            } else {
                if (name >= name_end && entry >= entry_end)
                    return MAT_EQ;
                name -= offset;
                entry -= (size_t)offset - 1;
            }
        }
    }

    /* No more specification - match depends on remainder of entry... */

    if (entry < entry_end)
        return MAT_NE;
    return MAT_EQ;
}

/*************************************************************** insert_ent() */

/* insert_ent() - procedure to add a directory entry at record dr entry de */

vmscond_t insert_ent( struct FCB * fcb, uint32_t eofblk, uint32_t curblk,
                      struct VIOC * vioc, char *buffer,
                      struct dir$r_rec * dr, struct dir$r_ent * de,
                      char *filename, uint32_t filelen,
                      uint32_t version, struct fiddef * fid,
                      uint16_t *reslen, struct dsc$descriptor *resdsc ) {
    register vmscond_t sts = SS$_NORMAL;
    register size_t inuse = 0;
    register uint16_t addlen;
    uint16_t verlimit;
    size_t avail = 0, len;
    char *p, vbuf[sizeof(";65535")];

    verlimit = version >> 16;
    version &= 0xffff;

    /* Return result */

    if( resdsc != NULL && resdsc->dsc$w_length != 0 ) {
        avail = resdsc->dsc$w_length;
        p = resdsc->dsc$a_pointer;
        len = filelen;
        if( len > avail )
            len = avail;
        memcpy( p, filename, len );
        avail -= len;
        p += len;
        len = snprintf( vbuf, sizeof( vbuf ), ";%u", version );
        if( len > avail )
            len = avail;
        memcpy( p, vbuf, len );
        p += len;
        *reslen = (unsigned short)(p - resdsc->dsc$a_pointer);
    }

    /* Compute space required... */

    addlen = sizeof(struct dir$r_ent);
    direct_inserts++;
    if (de == NULL)
        addlen += (((size_t)filelen + 1) & ~1) + offsetof(struct dir$r_rec, dir$t_name);

    /* Compute block space in use ... */

    {
        int invalid_dir = TRUE;
        while (TRUE) {
            register size_t reclen;
            register struct dir$r_rec *nr;

            nr = (struct dir$r_rec *) (buffer + inuse);

            if( dr == nr )
                invalid_dir = FALSE;
            reclen = F11WORD(nr->dir$w_size);
            if (reclen == 0xffff) /* End of data marker */
                break;
            reclen += 2;
            inuse += reclen;
            reclen -= (((size_t)nr->dir$b_namecount + 1) & ~1) + offsetof(struct dir$r_rec, dir$t_name);
            if (inuse > MAXREC || (inuse & 1) || reclen <= 0 ||
                reclen % sizeof(struct dir$r_ent) != 0) {
                deaccesschunk( vioc, 0, 0, FALSE );
                return SS$_BADIRECTORY;
            }
        }

        if (invalid_dir) {
            printf("BUGCHECK invalid dir\n");
            exit(EXIT_FAILURE);
        }
        if (de != NULL) {
            if (F11WORD(dr->dir$w_size) > MAXREC ||
                (char *) de < dr->dir$t_name + dr->dir$b_namecount ||
                (char *) de > (char *) dr + F11WORD(dr->dir$w_size) + 2) {
                printf("BUGCHECK invalid de\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    /* If not enough space free extend the directory... */

    if (addlen > MAXREC - inuse) {
        register struct dir$r_rec *nr;
        unsigned keep_new = 0;
        char *newbuf;
        struct VIOC *newvioc;
        uint32_t newblk = eofblk + 1;

        direct_splits++;
#if DEBUG
        printf( "Splitting directory record... %s", dr->dir$t_name );
        if( de != NULL )
            printf( " %u,%u,%u,%u\n", de->dir$w_fid.fid$w_num,
                de->dir$w_fid.fid$w_seq, de->dir$w_fid.fid$b_rvn, de->dir$w_fid.fid$b_nmx );
        else
            putchar( '\n' );
#endif
        if( newblk > fcb->hiblock ) {
#if DEBUG
            printf("Extending directory during split\n");
#endif
            if( $FAILS(sts = update_extend( fcb, newblk - fcb->hiblock, 0 )) ) {
#if DEBUG
                printf( "Failed to extend a directory during a split\n" );
#endif
                return $SETLEVEL(SS$_DIRALLOC, SEVERE);
            }
        }

        if( fcb->highwater != 0 && newblk > fcb->highwater -1 ) {
            fcb->highwater = newblk + 1;
            fcb->head->fh2$l_highwater = F11LONG( newblk + 1 );
        }

        if(  $SUCCESSFUL(sts = accesschunk( fcb, newblk, &newvioc, &newbuf, NULL, 1 )) ) {
            while( newblk > curblk + 1 ) {
                char *frombuf;
                struct VIOC *fromvioc;

                if( $FAILED(sts = accesschunk( fcb, newblk - 1, &fromvioc, &frombuf, NULL, 1 )) )
                    break;
                memcpy( newbuf, frombuf, BLOCKSIZE );
                sts = deaccesschunk( newvioc, newblk, 1, TRUE);
                newvioc = fromvioc;
                newbuf = frombuf;
                newblk--;
                if( $FAILED(sts) )
                    break;
            }
        } else {
            newvioc = NULL;
        }
        if( $FAILED(sts) ) {
            if( newvioc != NULL )
                deaccesschunk( newvioc, 0, 0, FALSE);
            deaccesschunk( vioc, 0, 0, FALSE );
            return sts;
        }
        memset( newbuf, 0, BLOCKSIZE );
        eofblk++;
        fcb->head->fh2$w_recattr.fat$l_efblk = F11SWAP( eofblk + 1 );

        /* First find where the next record is... */

        nr = dr;
        if (F11WORD(nr->dir$w_size) <= MAXREC)
            nr = (struct dir$r_rec *) ((char *) nr + F11WORD(nr->dir$w_size) + 2);

        /* Can we split between records? */

        if (de == NULL || (char *) dr != buffer ||
            F11WORD(nr->dir$w_size) <= MAXREC) {
            register struct dir$r_rec *sp;

            sp = dr;
            if ((char *) dr == buffer && de != NULL)
                sp = nr;
            memcpy(newbuf, sp, ((buffer + BLOCKSIZE) - (char *) sp));
            memset(sp, 0, ((buffer + BLOCKSIZE) - (char *) sp));
            sp->dir$w_size = F11WORD(0xffff);
            if (sp == dr &&
                (de != NULL || (char *) sp >= buffer + MAXREC - addlen)) {
                if (de != NULL)
                    de = (struct dir$r_ent *)
                        (newbuf + ((char *) de - (char *) sp));
                dr = (struct dir$r_rec *) (newbuf + ((char *) dr - (char *) sp));
                keep_new = 1;
            }
            /* OK, we have to split the record then.. */

        } else {
            register uint32_t reclen;
            register struct dir$r_rec *nbr;

            reclen = ( (((size_t)dr->dir$b_namecount + 1) & ~1) +
                       offsetof(struct dir$r_rec, dir$t_name) );
            nbr = (struct dir$r_rec *) newbuf;
#if DEBUG
            printf("Super split %s %u,%u,%u,%u\n", dr->dir$t_name, de->dir$w_fid.fid$w_num,
               de->dir$w_fid.fid$w_seq, de->dir$w_fid.fid$b_rvn, de->dir$w_fid.fid$b_nmx);
#endif
            memcpy(newbuf, buffer, reclen);
            memcpy(newbuf + reclen, de, ((char *) nr - (char *) de) + 2);
            nbr->dir$w_size = F11WORD(reclen + ((char *) nr - (char *) de) - 2);

            memset((char *) de + 2, 0, ((char *) nr - (char *) de));
            ((struct dir$r_rec *) de)->dir$w_size = F11WORD(0xffff);
            dr->dir$w_size = F11WORD(((char *) de - (char *) dr) - 2);
            if ((char *) de >= (char *) nr) {
                dr = (struct dir$r_rec *) newbuf;
                de = (struct dir$r_ent *) (newbuf + reclen);
                keep_new = 1;
            }
        }

        /* Need to decide which buffer we are going to keep (to write to) */

        if (keep_new) {
            sts = deaccesschunk( vioc, curblk, 1, TRUE);
            curblk = newblk;
            vioc = newvioc;
            buffer = newbuf;
        } else {
            sts = deaccesschunk( newvioc, newblk, 1, TRUE );
        }
        if( $FAILED(sts) )
            printf("I/O error splitting directory record: %s\n", getmsg(sts, 0));
    }
    /* After that we can just add the record or entry as appropriate... */

    if (de == NULL) {
        memmove((char *) dr + addlen, dr,
                BLOCKSIZE - (((char *) dr + addlen) - buffer));
        dr->dir$w_size = F11WORD(addlen - 2);
        dr->dir$w_verlimit = verlimit;
        dr->dir$b_flags = 0;
        dr->dir$b_namecount = filelen;
        memcpy(dr->dir$t_name, filename, filelen);
        de = (struct dir$r_ent *)
                 ((char *) dr + (addlen - sizeof(struct dir$r_ent)));
    } else {
        dr->dir$w_size = F11WORD(F11WORD(dr->dir$w_size) + addlen);
        memmove((char *) de + addlen, de,
                BLOCKSIZE - (((char *) de + addlen) - buffer));
    }

    /* Write the entry values are we are done! */

    de->dir$w_version = F11WORD(version);
    fid_copy( &de->dir$w_fid, fid, 0);
    return deaccesschunk( vioc, curblk, 1, TRUE );
}

/*************************************************************** delete_ent() */

/* delete_ent() - delete a directory entry */

vmscond_t delete_ent( struct FCB * fcb, struct VIOC * vioc, uint32_t curblk,
                      struct dir$r_rec * dr, struct dir$r_ent * de,
                      char *buffer, uint32_t eofblk ) {
    vmscond_t sts = SS$_NORMAL;
    uint32_t ent;

    direct_deletes++;
    ent = ((size_t)F11WORD(dr->dir$w_size) + 2 - offsetof(struct dir$r_rec, dir$t_name)
           - (((size_t)dr->dir$b_namecount + 1) & ~1)) / sizeof(struct dir$r_ent);
    if (ent > 1) {
        char *ne;

        ne = (char *) de + sizeof(struct dir$r_ent);
        memmove( de, ne, BLOCKSIZE - (ne - buffer) );
        dr->dir$w_size = F11WORD(F11WORD(dr->dir$w_size) - sizeof(struct dir$r_ent));
    } else {
        char *nr;

        nr = (char *) dr + F11WORD(dr->dir$w_size) + 2;
        if( eofblk == 1 || (char *) dr > buffer ||
            (nr <= buffer + MAXREC && (unsigned short) *nr < BLOCKSIZE) ) {
            memmove(dr, nr, BLOCKSIZE - (nr - buffer));
        } else {
            while (curblk < eofblk) {
                char *nxtbuffer;
                struct VIOC *nxtvioc;

                sts = accesschunk(fcb, curblk + 1, &nxtvioc, &nxtbuffer, NULL, 1);
                if( $FAILED(sts) )
                    break;
                memcpy(buffer, nxtbuffer, BLOCKSIZE);
                if( $FAILS(sts = deaccesschunk(vioc, curblk++, 1, TRUE)) )
                    break;
                buffer = nxtbuffer;
                vioc = nxtvioc;
            }
            if(  $SUCCESSFUL(sts) ) {
                fcb->head->fh2$w_recattr.fat$l_efblk = F11SWAP(eofblk);
                eofblk--;
            }
        }
    }
    {
        vmscond_t retsts;

        retsts = deaccesschunk(vioc, curblk, 1, TRUE);
        if(  $SUCCESSFUL(sts) )
            sts = retsts;
        return sts;
    }
}

/*************************************************************** return_ent() */

/* return_ent() - return information about a directory entry */

vmscond_t return_ent(struct FCB * fcb, struct VIOC * vioc, uint32_t curblk,
                     struct dir$r_rec * dr, struct dir$r_ent * de, struct fibdef * fib,
                     uint16_t *reslen, struct dsc_descriptor * resdsc,
                     int wildcard) {
    if( resdsc != NULL ) {
        register uint32_t scale = 10;
        register uint16_t version;
        register size_t length;
        register char *ptr;
        register size_t outlen;

        version = F11WORD(de->dir$w_version);
        length = dr->dir$b_namecount;
        ptr = resdsc->dsc_a_pointer;
        outlen = resdsc->dsc_w_length;

        if (length > outlen)
            length = outlen;
        memcpy(ptr, dr->dir$t_name, length);
        while( version >= scale )
            scale *= 10;
        ptr += length;
        if( length < outlen ) {
            *ptr++ = ';';
            length++;
            do {
                if( length >= outlen )
                    break;
                scale /= 10;
                *ptr++ = version / scale + '0';
                version %= scale;
                length++;
            } while( scale > 1 );
        }
        *reslen = (uint16_t)length;
    }
    fid_copy( (struct fiddef *)&fib->fib$w_fid_num, &de->dir$w_fid, 0 );

    if( fib->fib$b_fid_rvn == 0 )
        fib->fib$b_fid_rvn = fcb->rvn;

    fib->fib$w_verlimit = dr->dir$w_verlimit;

    if( wildcard || (fib->fib$w_nmctl & FIB$M_WILD) ) {
        fib->fib$l_wcc = curblk;
    } else {
        fib->fib$l_wcc = 0;
    }
    return deaccesschunk(vioc, 0, 0, TRUE);
}

/*************************************************************** search_ent() */

/* search_ent() - search for a directory entry */

vmscond_t search_ent( struct FCB *fcb,
                      struct dsc_descriptor *fibdsc,
                      struct dsc_descriptor *filedsc, uint16_t *reslen,
                      struct dsc_descriptor *resdsc, uint32_t eofblk,
                      unsigned action ) {
    register vmscond_t sts;
    uint32_t curblk;
    struct VIOC *vioc = NULL;
    char *searchspec, *buffer;
    int searchlen, version, wildcard, wcc_flag;
    struct fibdef *fib;

    fib = (struct fibdef *) fibdsc->dsc_a_pointer;
    direct_lookups++;

    /* 1) Generate start block (wcc gives start point)
     * 2) Search for start
     * 3) Scan until found or too big or end.
     */

    curblk = fib->fib$l_wcc;
    if (curblk != 0) {
        searchspec = resdsc->dsc_a_pointer;
        sts = name_check( searchspec, *reslen, &searchlen, &version, &wildcard );
        if( MODIFIES(action) || wildcard )
            sts = RMS$_WLD;
        wcc_flag = TRUE;
    } else {
        searchspec = filedsc->dsc_a_pointer;
        sts = name_check( searchspec, filedsc->dsc_w_length, &searchlen, &version,
                          &wildcard);
        if( (MODIFIES(action) && wildcard) || (action == DIRECT_CREATE && version < 0) ) {
            sts = RMS$_WLD;
        }
        wcc_flag = FALSE;
    }
    if( $FAILED(sts) )
        return sts;

    /* Identify starting block...*/

    if (*searchspec == '*' || *searchspec == '%') {
        curblk = 1;
    } else {
        register uint32_t loblk = 1, hiblk = eofblk;
        if( curblk < 1 || curblk > eofblk )
            curblk = (eofblk + 1) / 2;

        while (loblk < hiblk) {
            register int cmp;
            register uint32_t newblk;
            register struct dir$r_rec *dr;
            direct_searches++;

            if( $FAILS(sts = accesschunk( fcb, curblk, &vioc, &buffer, NULL, MODIFIES(action) )) )
                return sts;
            dr = (struct dir$r_rec *) buffer;
            if( F11WORD(dr->dir$w_size) > MAXREC ) {
                cmp = MAT_GT;
            } else {
                cmp = name_match( searchspec, searchlen, dr->dir$t_name,
                                  dr->dir$b_namecount );
                if (cmp == MAT_EQ) {
                    if (wildcard || version < 1 || version > 32767) {
                        cmp = MAT_NE;   /* no match - want to find start */
                    } else {
                        register struct dir$r_ent *de =
                            (struct dir$r_ent *)
                                (dr->dir$t_name + ((dr->dir$b_namecount + 1) & ~1));
                        if (F11WORD(de->dir$w_version) < version) {
                            cmp = MAT_GT;       /* too far... */
                        } else {
                            if (F11WORD(de->dir$w_version) > version) {
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
                if( $FAILS(sts = deaccesschunk( vioc, 0, 0, TRUE )) )
                    return sts;
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
        while( $SUCCESSFUL(sts) && curblk <= eofblk ) {
            register struct dir$r_rec *dr;
            register int cmp = MAT_LT;

            /* Access a directory block. Reset relative version if it starts
               with a record we haven't seen before... */

            if( vioc == NULL ) {
                if( $FAILS(sts = accesschunk( fcb, curblk, &vioc,
                                              &buffer, NULL, MODIFIES(action) )) )
                    return sts;
            }
            dr = (struct dir$r_rec *) buffer;
            if (last_len != dr->dir$b_namecount) {
                relver = 0;
            } else {
                if( name_match(last_name, last_len, dr->dir$t_name, last_len) !=
                    MAT_EQ ) {
                    relver = 0;
                }
            }

            /* Now loop through the records seeing which match our spec... */

            while (TRUE) {        /* dr records within block */
                register char *nr = (char *) dr + F11WORD(dr->dir$w_size) + 2;
                if (nr >= buffer + BLOCKSIZE)
                    break;
                if (dr->dir$t_name + dr->dir$b_namecount >= nr)
                    break;
                cmp = name_match(searchspec, searchlen, dr->dir$t_name,
                                 dr->dir$b_namecount);
                if (cmp == MAT_GT && wcc_flag) {
                    wcc_flag = FALSE;
                    searchspec = filedsc->dsc_a_pointer;
                    if( $FAILS(sts = name_check(searchspec, filedsc->dsc_w_length,
                                     &searchlen, &version, &wildcard)) )
                        break;
                } else {
                    if (cmp == MAT_EQ) {
                        register struct dir$r_ent *de;
                        de = (struct dir$r_ent *)
                                 (dr->dir$t_name +
                                  ((dr->dir$b_namecount + 1) & ~1));
                        if (version == 0 && action == DIRECT_CREATE) {
                            version = F11WORD(de->dir$w_version) + 1;
                            if (version > 32767) {
                                sts = RMS$_VER;
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
                                    (version < F11WORD(de->dir$w_version))) {
                                relver--;
                                de++;
                            } else {
                                if (version > 32767 || version == relver ||
                                    version == F11WORD(de->dir$w_version)) {
                                    cmp = MAT_EQ;
                                } else {
                                    cmp = MAT_GT;
                                }
                                if (!wcc_flag) {
                                    break;
                                } else {
                                    wcc_flag = FALSE;
                                    searchspec = filedsc->dsc_a_pointer;
                                    if( $FAILS(sts = name_check( searchspec,
                                                                 filedsc->dsc_w_length,
                                                                 &searchlen, &version,
                                                                 &wildcard )) )
                                        break;
                                    if (name_match(searchspec, searchlen,
                                                   dr->dir$t_name,
                                                   dr->dir$b_namecount) !=
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
                        if( $FAILED(sts) )
                            break;

                        /* Decide what to do with the entry we have found... */

                        if (cmp == MAT_EQ) {
                            switch (action) {
                            case DIRECT_FIND:
                                return return_ent( fcb, vioc, curblk, dr, de, fib,
                                                   reslen, resdsc, wildcard );
                            case DIRECT_DELETE:
                                return delete_ent( fcb, vioc, curblk, dr, de,
                                                   buffer, eofblk );
                            case DIRECT_CREATE:
                                sts = SS$_DUPFILENAME;
                                break;
                            default:
                                abort();
                            }
                        } else {
                            if( cmp != MAT_NE && action == DIRECT_CREATE ) {
                                 return insert_ent( fcb, eofblk, curblk, vioc, buffer,
                                                   dr, de, searchspec, searchlen,
                                                    (fib->fib$w_verlimit << 16) | version,
                                                    (struct fiddef *)&fib->fib$w_fid_num, reslen, resdsc );

                            }
                        }
                    }
                    /*  Finish unless we expect more... */

                    if (cmp == MAT_GT && !wildcard)
                        break;

                    /* If this is the last record in the block store the name
                       so that if it continues into the next block we can get
                       the relative version right! Sigh! */

                    if (F11WORD(((struct dir$r_rec *) nr)->dir$w_size) > MAXREC) {
                        last_len = dr->dir$b_namecount;
                        if (last_len > sizeof(last_name)) {
                            last_len = sizeof(last_name);
                        }
                        memcpy(last_name, dr->dir$t_name, last_len);
                        dr = (struct dir$r_rec *) nr;
                        break;
                    }
                    dr = (struct dir$r_rec *) nr;
                }
            }

            /* Release the buffer ready to get the next one - unless it is the
               last one in which case we can't defer an insert any longer!! */

            if( $FAILED(sts) || action != DIRECT_CREATE ||
                (cmp != MAT_GT && curblk < eofblk)) {
                register vmscond_t dests;

                if( $FAILS(dests = deaccesschunk( vioc, 0, 0, TRUE )) ) {
                    sts = dests;
                    break;
                }
                vioc = NULL;
                curblk++;
            } else {
                if( version == 0 )
                    version = 1;
                return insert_ent( fcb, eofblk, curblk, vioc, buffer, dr, NULL,
                                  searchspec, searchlen, (fib->fib$w_verlimit << 16) | version,
                                   (struct fiddef *)&fib->fib$w_fid_num,
                                   reslen, resdsc );
            }
        }                       /* curblk blocks within file */
    }

    /* We achieved nothing! Report the failure... */

    if( $SUCCESSFUL(sts) ) {
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
                2 - create an entry
  */

unsigned direct( struct VCB * vcb, struct dsc_descriptor * fibdsc,
                 struct dsc_descriptor * filedsc, uint16_t *reslen,
                 struct dsc_descriptor * resdsc, unsigned action ) {
    struct FCB *fcb;
    register vmscond_t sts;
    uint32_t eofblk;
    register struct fibdef *fib;

    fib = (struct fibdef *) fibdsc->dsc_a_pointer;

    if( $SUCCESSFUL(sts = accessfile( vcb, (struct fiddef *) &fib->fib$w_did_num,
                                      &fcb, MODIFIES(action) )) ) {
        if (F11LONG(fcb->head->fh2$l_filechar) & FH2$M_DIRECTORY) {
            eofblk = F11SWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
            if( F11WORD(fcb->head->fh2$w_recattr.fat$w_ffbyte) == 0 )
                --eofblk;
            if( eofblk == 0 ) {
                struct VIOC *vioc;
                char *buffer;
                uint32_t blocks;

                if( action != DIRECT_CREATE ) {
                    int searchlen, version, wildcard;

                    deaccessfile( fcb );
                    if( fib->fib$l_wcc ) {
                        if( $FAILS(sts = name_check( resdsc->dsc$a_pointer, *reslen,
                                                     &searchlen, &version, &wildcard )) )
                            return sts;
                        fib->fib$l_wcc = 0;
                        return SS$_NOMOREFILES;
                    }
                    if( $FAILS(sts = name_check( filedsc->dsc$a_pointer,
                                                 filedsc->dsc$w_length,
                                                 &searchlen, &version, &wildcard )) )
                        return sts;
                    if( wildcard )
                        return SS$_NOMOREFILES;
                    return SS$_NOSUCHFILE;
                }

                eofblk = F11SWAP(fcb->head->fh2$w_recattr.fat$l_hiblk);
                if( eofblk == 0 ) {
                    if( $FAILS(sts = update_extend( fcb, 1, 0 )) ) {
                        deaccessfile( fcb );
                        return sts;
                    }
                    eofblk = F11SWAP(fcb->head->fh2$w_recattr.fat$l_hiblk);
                    if( fcb->highwater != 0 ) {
                        fcb->highwater = 2;
                        fcb->head->fh2$l_highwater = F11SWAP( 2 );
                    }
                }
                fcb->head->fh2$w_recattr.fat$l_efblk = F11SWAP( 2 );
                fcb->head->fh2$w_recattr.fat$w_ffbyte = 0;
                eofblk = 1;
                if( $FAILS(sts = accesschunk( fcb, 1, &vioc, &buffer, &blocks, 1 )) ) {
                    deaccessfile( fcb );
                    return sts;
                }
                memset( buffer, 0, 512 );
                ((f11word *)buffer)[0] = 0xffff;
                deaccesschunk( vioc, 1, blocks, TRUE );
            }
            sts = search_ent( fcb, fibdsc, filedsc, reslen, resdsc, eofblk, action );
        } else {
            sts = SS$_BADIRECTORY;
        }
        {
            register uint32_t dests;

            dests = deaccessfile( fcb );
            if( $SUCCESSFUL(sts) )
                sts = dests;
        }
    }
    if( MODIFIES(action) ) {
        cache_flush();
        while( vcb->dircache ) /* Too conservative...but safe */
            cache_delete( (struct CACHE *)vcb->dircache );
    }

    return sts;
}

/******************************************************************* dircmp() */

/* Function to compare directory cache names */

static int dircmp( uint32_t keylen, void *key, void *node ) {
    register struct DIRCACHE *dirnode;
    register int cmp;
    unsigned dlen;
    const char *kp, *dp;
    int version, wild, namelen;
    vmscond_t sts;

    if( $FAILS(sts = name_check(key, keylen, &namelen, &version, &wild) ) ) {
        abort();
    }

    dirnode = (struct DIRCACHE *) node;

    if( version != F11WORD(dirnode->entry.dir$w_version) )
        return version - F11WORD(dirnode->entry.dir$w_version);

    dlen = dirnode->record.dir$b_namecount;

    kp = (const char *)key;
    dp = dirnode->record.dir$t_name;

    while( keylen > 0 && dlen > 0 ) {
        cmp = toupper(*kp++) - toupper(*dp++);
        if( cmp != 0 )
            return cmp;
        --keylen;
        --dlen;
    }
    return keylen - dlen;
}

/***************************************************************** dir_create() */
static void *dir_create( uint32_t keylen, void *key, vmscond_t *retsts ) {
    struct DIRCACHE *dir;
    vmscond_t sts;
    int version, wild, namelen;

    if( $FAILS(sts = name_check(key, keylen, &namelen, &version, &wild) ) ) {
        *retsts = RMS$_DIR;
        return NULL;
    }
    if( wild || version == 0 ) {
        *retsts = SS$_NOSUCHFILE;
        return NULL;
    }

    if( (dir = (struct DIRCACHE *)calloc( 1,
                                          offsetof( struct DIRCACHE, record.dir$t_name ) +
                                          namelen + (namelen & 1) )) == NULL ) {
        *retsts = SS$_INSFMEM;
        return NULL;
    }

    dir->cache.objtype = OBJTYPE_DIR;
    dir->record.dir$w_size = (f11word)F11WORD( offsetof( struct dir$r_rec,
                                                         dir$t_name ) + namelen - 2);
    dir->record.dir$b_namecount = (f11byte) namelen;
    memcpy( dir->record.dir$t_name, key, namelen );
    dir->entry.dir$w_version = (f11word)F11WORD((f11word)version);

    *retsts = SS$_CREATED;
    return dir;
}

/***************************************************************** direct_dirid() */

/* Find FID of directory/ies using a cache.
 * Used by sys$parse.  Can be generalized...
 * Input descriptor is a directory name string, including subdirectories.
 * Output is the fid of the lowest level directory.
 * Higher directories are searched (and cached).
 * Any wildcards/recursion stop the search (they'll be resolved when a
 * search matches them.)
 */

vmscond_t direct_dirid( struct VCB *vcb, struct dsc$descriptor *dirdsc,
                        struct fiddef *dirid, struct fiddef *fid ) {
    register struct DIRCACHE *dir = NULL;
    vmscond_t sts;
    struct fibdef fib;
    struct dsc$descriptor fibdsc;
    struct dsc$descriptor namdsc;
    struct fiddef srcdir;
    char *dirnam;
    int dirlen;
    size_t len;
    char *bp, *dp;
    char nambuf[256];

    memset( &fib, 0, sizeof( fib ) );
    memset( &fibdsc, 0, sizeof( fibdsc ) );
    memset( &namdsc, 0, sizeof( namdsc ) );
    memset( &srcdir, 0, sizeof( srcdir ) );

    dirlen = dirdsc->dsc$w_length;

    if( (size_t)dirlen > sizeof( nambuf ) -1 )
        abort();
    dirnam = dirdsc->dsc$a_pointer;

    srcdir.fid$w_num = FID$C_MFD;
    srcdir.fid$w_seq = FID$C_MFD;
    srcdir.fid$b_rvn = 0;
    srcdir.fid$b_nmx = 0;

    if( dirlen < 1 || (dirlen == 6 && !memcmp( dirnam, "000000", 6 )) ) {
        if( dirid )
            *dirid = srcdir;
        if( fid )
            *fid = srcdir;
        return SS$_NORMAL;
    }

    if( dirid != NULL )
        memset( dirid, 0, sizeof( *dirid ) );
    if( fid != NULL )
        memset( fid, 0, sizeof( *fid ) );

    fibdsc.dsc$w_length = sizeof( fibdsc );
    fibdsc.dsc$a_pointer = (char *)&fib;

    for( bp = dp = dirnam; dp <= dirnam + dirlen; dp++) {
        char ch;

        if( dp < dirnam + dirlen ) {
            ch = *dp;
            if( ch == '*' || ch == '%' ) {
                return RMS$_WLD;
            }
            if( ch != '.' )
                continue;
        }
        len = (size_t)(dp - bp);
        if( len == 0 )
            break;

        memcpy( nambuf, bp, len );
        namdsc.dsc$w_length = (uint16_t)(len + sizeof( ".DIR;1" ) -1);
        memcpy( nambuf+len, ".DIR;1", namdsc.dsc$w_length );

        namdsc.dsc$a_pointer = nambuf;

        dir = cache_find( (void *) &vcb->dircache,  namdsc.dsc$w_length, nambuf,
                          &sts, dircmp, dir_create );
        if( dir == NULL ) {
            return RMS$_DNF;
        }
        cache_untouch( &dir->cache, FALSE );

        if( $MATCHCOND( sts, SS$_CREATED ) ) {
            memset( &fib, 0, sizeof( fib ) );
            memcpy( &fib.fib$w_did_num, &srcdir, sizeof( struct fiddef ) );

            sts = direct( vcb, &fibdsc, &namdsc, NULL, NULL, DIRECT_FIND );
            if( $FAILED(sts) ) {
                cache_delete( &dir->cache );
                return RMS$_DNF;
            }
            dir->parent = srcdir;
            dir->record.dir$w_verlimit = fib.fib$w_verlimit;
            memcpy( &dir->entry.dir$w_fid, &fib.fib$w_fid_num, sizeof( struct fiddef ) );
        }
        if( (dp + 2 < dirnam + dirlen) && dp[1] == '.' && dp[2] == '.' ) {
            return RMS$_DNF;
        }
        srcdir = dir->entry.dir$w_fid;
        bp = dp +1;
    }
    if( dir != NULL ) {
        if( dirid )
            *dirid = dir->parent;
        if( fid )
            *fid = dir->entry.dir$w_fid;
        return SS$_NORMAL;
    }
    return RMS$_DNF;
}
