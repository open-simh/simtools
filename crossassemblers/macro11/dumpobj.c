/* Dump and interpret an object file. */
/* Optionally create a paper-tape (LDA) file. */

/*
Copyright (c) 2001, Richard Krehbiel
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

o Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

o Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

o Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

*/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifdef WIN32
#define strcasecmp stricmp 
#if !__STDC__
#define stricmp _stricmp
#endif
#endif

#include "rad50.h"
#include "util.h"

#ifndef SKIP_GIT_INFO
#include "git-info.h"
#endif

#define VERSION "February 13, 2023"     /* Also check the text above the "Usage:" section below */

#define NPSECTS 256
#define MAX_LDA_ADDR 0175777            /* Higher addresses will not be written to the LDA */
                                        /* TODO: If we 'variablize' MAX_LDA_ADDR force it to odd */

#ifndef DEFAULT_OBJECTFORMAT_RT11
#define DEFAULT_OBJECTFORMAT_RT11 -1    /* -1 = Auto-detect, 0 = RSX-11M/PLUS, 1 = RT-11 */
#endif

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT ' '           /* Default output text alignment: choose ' ' or '\t' */
#endif

#ifndef BADBIN_XFERAD
#define BADBIN_XFERAD 0                 /* xferad to use if badbin > 0: choose 0 or 1 */
#endif

#ifndef XFERAD_WHEN_ZERO
#define XFERAD_WHEN_ZERO 2              /* xferad to use if the <inputfile> provides XFER=0 */
#endif                                  /* Set to zero to disable this functionality (keep 0=0) */

#define WORD(cp) ((*(cp) & 0xff) + ((*((cp)+1) & 0xff) << 8))

int             psectid = 0;
char           *psects[NPSECTS];
FILE           *bin = NULL;
int             badfmt = 0;
int             badbin = 0;
int             xferad = 1;
unsigned        highest_addr = 0;
int             psects_with_data = 0;

int             loud = 1;                         /* Set to 0 by  '-q'                */
int             loudtxt = 1;                      /* Set to 0 by  '-qt'               */
char            alignment = DEFAULT_ALIGNMENT;    /* Set with the '-[no]align' option */
int             sortgsd = 1;                      /* Set to 0 by  '-nosort'           */
unsigned        origin = 0;                       /* Set with the '-o[f]' option      */
int             reloc_internal = 1;               /* Set to 0 by  '-of' & 1 by '-o'   */
char           *word_patch = NULL;                /* Set with the '-w' option         */
unsigned        new_xferad = MAX_LDA_ADDR+1;      /* Set with the '-x' option         */

/**** Summary [ '-s' option ] name strings and counters  ****/
/*    Strings starting with "*" may cause binary errors     */

char           *blkname[] = {
                              "*"  "Unused ",  /* 00 */
                              " "  "GSD    ",  /* 01 */  /* Must be first in OBJ */
                              " "  "ENDGSD ",  /* 02 */  /* Only one - must be before ENDMOD and no more GSDs follow */
                              " "  "TXT    ",  /* 03 */  /* At least one RLD must precede the first TXT (i.e. psect)*/
                              " "  "RLD    ",  /* 04 */
                              " "  "ISD    ",  /* 05 */  /* The RT-11 linker ignores this */
                              " "  "ENDMOD ",  /* 06 */  /* Only one - must be last in OBJ */
                              "*"  "LIBHDR ",  /* 07 */
                              "*"  "LIBEND "   /* 10 */
                            };

char           *gsdname[] = {
                              " "  "MODNAME",  /* 00 */
                              "*"  "CSECT  ",  /* 01 */
                              " "  "ISD    ",  /* 02 */  /* The RT-11 linker ignores this */
                              " "  "XFER   ",  /* 03 */
                              " "  "GLOBAL ",  /* 04 */  /* Each name ought to be unique */
                              " "  "PSECT  ",  /* 05 */  /* This seems to work if not unique */
                              " "  "IDENT  ",  /* 06 */
                              "*"  "VSECT  ",  /* 07 */
                              "*"  "COMPRTN"   /* 10 */
                            };

char           *rldname[] = {
                              "*"  "Not used (0)  ",  /* 00 */
                              " "  "Internal reloc",  /* 01 */
                              "*"  "Global   reloc",  /* 02 */
                              "*"  "Internal Disp ",  /* 03 */
                              "*"  "Global   Disp ",  /* 04 */
                              "*"  "Global + Off  ",  /* 05 */
                              "*"  "Gbl + Off Disp",  /* 06 */
                              " "  "Loc (.) Def   ",  /* 07 */
                              " "  "Loc (.) Mod   ",  /* 10 */
                              "*"  ".LIMIT        ",  /* 11 */
                              "*"  "Psect         ",  /* 12 */
                              "*"  "Not used (13) ",  /* 13 */
                              "*"  "Psect Disp    ",  /* 14 */
                              "*"  "Psect + Off   ",  /* 15 */
                              "*"  "Psect+Off Disp",  /* 16 */
                              "*"  "Complex       "   /* 17 */
                            };

#define BLKSIZ (sizeof(blkname) / sizeof(blkname[0]))
#define GSDSIZ (sizeof(gsdname) / sizeof(gsdname[0]))
#define RLDSIZ (sizeof(rldname) / sizeof(rldname[0]))

int             blkcnt[BLKSIZ] = { 0 };
int             gsdcnt[GSDSIZ] = { 0 };
int             rldcnt[RLDSIZ] = { 0 };

/* memcheck - crash out if a pointer (returned from malloc) is NULL. */
/*          - this routine is already [unchanged] in util.c          */

#if 0
void           *memcheck(
    void *ptr)
{
    if (ptr == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(EXIT_FAILURE);
    }

    return ptr;
}
#endif

char           *readrec(
    FILE *fp,
    int *len,
    int rt11)
{
    int             c,
                    i;
    int             chksum;
    char           *buf;

    chksum = 0;

    if (rt11) {
        while (c = fgetc(fp), c != EOF && c == 0) ;

        if (c == EOF)
            return NULL;

        if (c != 1) {
            fprintf(stderr, "Improperly formatted OBJ file (1)\n");
            badfmt++;
            return NULL;               /* Not a properly formatted file. */
        }

        chksum -= c;

        c = fgetc(fp);
        if (c != 0) {
            fprintf(stderr, "Improperly formatted OBJ file (2)\n");
            badfmt++;
            return NULL;               /* Not properly formatted */
        }

        chksum -= c;                   /* even though for 0 the checksum isn't changed... */
    }

    c = fgetc(fp);
    if (c == EOF) {
        if (rt11) {
            fprintf(stderr, "Improperly formatted OBJ file (3)\n");
            badfmt++;
        }
        return NULL;
    }
    *len = c;

    chksum -= c;

    c = fgetc(fp);
    if (c == EOF) {
        fprintf(stderr, "Improperly formatted OBJ file (4)\n");
        badfmt++;
        return NULL;
    }

    *len += (c << 8);

    chksum -= c;

    if (rt11) {
        *len -= 4;                     /* Subtract header and length bytes from length */
    }

    if (*len < 0) {
        fprintf(stderr, "Improperly formatted OBJ file (5)\n");
        badfmt++;
        return NULL;
    }

    buf = malloc(*len);
    if (buf == NULL) {
        fprintf(stderr, "Out of memory allocating %d bytes\n", *len);
        badfmt++;
        return NULL;                   /* Bad alloc */
    }

    i = fread(buf, 1, *len, fp);
    if (i < *len) {
        free(buf);
        fprintf(stderr, "Improperly formatted OBJ file (6)\n");
        badfmt++;
        return NULL;
    }

    for (i = 0; i < *len; i++)
        chksum -= (buf[i] & 0xff);

    if (rt11) {
        c = fgetc(fp);
        c &= 0xff;
        chksum &= 0xff;

        if (c != chksum) {
            free(buf);
            fprintf(stderr, "Bad record checksum, calculated=$%04x, recorded=$%04x\n", chksum, c);
            badfmt++;
            return NULL;
        }
    } else {
        if (*len & 1) {
            /* skip 1 byte of padding */
            c = fgetc(fp);
            if (c == EOF) {
                free(buf);
                fprintf(stderr, "EOF where padding byte should be\n");
                badfmt++;
                return NULL;
            }
        }
    }

    return buf;
}

void dump_bytes(
    char *buf,
    int len)
{
    int             i,
                    j;

    if (!loud) return;  /* Nothing to do for -q[uiet] */

    for (i = 0; i < len; i += 8) {
        printf("\t%3.3o: ", i);
        for (j = i; j < len && j < i + 8; j++)
            printf("%3.3o ", buf[j] & 0xff);

        printf("%*s", (i + 8 - j) * 4, "");

        for (j = i; j < len && j < i + 8; j++) {
            int             c = buf[j] & 0xff;

            if (!isprint(c))
                c = '.';
            putchar(c);
        }

        putchar('\n');
    }
}

void dump_words(
    unsigned addr,
    char *buf,
    int len)
{
    int             i,
                    j;

    if (!loud) return;  /* Nothing to do for -q[uiet] */

    for (i = 0; i < len; i += 8) {
        printf("\t%6.6o: ", addr);

        for (j = i; j < len && j < i + 8; j += 2)
            if (len - j >= 2) {
                unsigned        word = WORD(buf + j);

                printf("%6.6o ", word);
            } else
                printf("%3.3o    ", buf[j] & 0xff);

        printf("%*s", (i + 8 - j) * 7 / 2, "");

        for (j = i; j < len && j < i + 8; j++) {
            int             c = buf[j] & 0xff;

            if (!isprint(c))
                c = '.';
            putchar(c);
        }

        putchar('\n');
        addr += 8;
    }
}

void dump_bin(
    unsigned addr,
    char *buf,
    int len)
{
    int             chksum;     /* Checksum is negative sum of all
                                   bytes including header and length */
    int             FBR_LEAD1 = 1,
                    FBR_LEAD2 = 0;
    int             i;
    unsigned        hdrlen;

    if (addr + len - 1 > highest_addr)
        highest_addr = addr + len - 1;

    if (addr > MAX_LDA_ADDR) {
        /* We can't write anything at all */
    /*  badbin++;    // We do this ONCE later */
        return;
    }

    if (addr + len - 1 > MAX_LDA_ADDR) {
        len = MAX_LDA_ADDR - addr + 1;  /* Write as much as we can */
    /*  badbin++;    // We do this ONCE later */
    }

    if (!bin) return;  /* Nothing more to do if no <outputfile> */

    hdrlen = len + 6;

    for (i = 0; i < 8; i++)
        fputc(0, bin);
    chksum = 0;
    if (fputc(FBR_LEAD1, bin) == EOF)
        return;                        /* All recs begin with 1,0 */
    chksum -= FBR_LEAD1;
    if (fputc(FBR_LEAD2, bin) == EOF)
        return;
    chksum -= FBR_LEAD2;

    i = hdrlen & 0xff;                 /* length, lsb */
    chksum -= i;
    if (fputc(i, bin) == EOF)
        return;

    i = (hdrlen >> 8) & 0xff;          /* length, msb */
    chksum -= i;
    if (fputc(i, bin) == EOF)
        return;

    i = addr & 0xff;                   /* origin, msb */
    chksum -= i;
    if (fputc(i, bin) == EOF)
        return;

    i = (addr >> 8) & 0xff;            /* origin, lsb */
    chksum -= i;
    if (fputc(i, bin) == EOF)
        return;

    if ((len == 0) || (buf == NULL))
        return;                        /* end of tape block */

    i = fwrite(buf, 1, len, bin);
    if (i < len)
        return;

    while (len > 0) {                  /* All the data bytes */
        chksum -= *buf++ & 0xff;
        len--;
    }

    chksum &= 0xff;

    fputc(chksum, bin);                /* Followed by the checksum byte */

    return;                            /* Worked okay. */
}

void trim(
    char *buf)
{
    char           *cp;

    for (cp = buf + strlen(buf); cp > buf; cp--)
        if (cp[-1] != ' ')
            break;
    *cp = 0;
}

char          **all_gsds = NULL;
int             nr_gsds = 0;
int             gsdsize = 0;

void add_gsdline(
    char *line)
{
    if (nr_gsds >= gsdsize || all_gsds == NULL) {
        gsdsize += 128;
        all_gsds = memcheck(realloc(all_gsds, gsdsize * sizeof(char *)));
    }

    all_gsds[nr_gsds++] = line;
}

void got_gsd(
    char *cp,
    int len)
{
    int             i;
    char           *gsdline;

    for (i = 2; i < len; i += 8) {
        char            name[8];
        unsigned        value;
        unsigned        flags;
        char           *equals = (alignment == '\t') ? " = " : "=";

        gsdline = memcheck(malloc(256));

        unrad50(WORD(cp + i), name);
        unrad50(WORD(cp + i + 2), name + 3);
        name[6] = 0;

        value = WORD(cp + i + 6);
        flags = cp[i + 4] & 0xff;

        if ((cp[i + 5] & 0xff) < GSDSIZ) gsdcnt[cp[i + 5] & 0xff]++;
        switch (cp[i + 5] & 0xff) {
        case 0:
            sprintf(gsdline, "\tMODNAME%c%s%s%o%cflags=%o\n", alignment, name, equals, value, alignment, flags);
            break;
        case 1:
            sprintf(gsdline, "\tCSECT%c%s%s%o%cflags=%o\n", alignment, name, equals, value, alignment, flags);
            badbin++;
            break;
        case 2:
            sprintf(gsdline, "\tISD%c%s%s%o%cflags=%o\n", alignment, name, equals, value, alignment, flags);
            break;
        case 3:
            sprintf(gsdline, "\tXFER%c%s%s%o%cflags=%o\n", alignment, name, equals, value, alignment, flags);
            xferad = origin + value;
            break;
        case 4:
            sprintf(gsdline, "\tGLOBAL%c%s%s"
                    "%o%c%s%s%s %s flags=%o\n", alignment, name, equals, value, alignment,
                    flags &   01 ? "WEAK " : "",
                    flags &   04 ? "LIB "  : "",
                    flags &  010 ? "DEF"   : "REF",
                    flags &  040 ? "REL"   : "ABS",
                    flags);
            break;
        case 5:
            if (value) psects_with_data++;
            sprintf(gsdline, "\tPSECT%c%s%s"
                    "%o%c%s%s%s%s %s %s %s %s flags=%o\n", alignment, name, equals, value, alignment,
                    (alignment == '\t') ? "" : " ",    /* For backwards compatibility with original dumpobj */
                    flags &   01 ? "SAV " : "",
                    flags &   02 ? "LIB " : "",
                    flags &   04 ? "OVR"  : "CON",
                    flags &  020 ? "RO"   : "RW",
                    flags &  040 ? "REL"  : "ABS",
                    flags & 0100 ? "GBL"  : "LCL",
                    flags & 0200 ? "D"    : "I",
                    flags);
            psects[psectid] = memcheck(strdup(name));
            trim(psects[psectid++]);
            psectid %= NPSECTS;
            break;
        case 6:
            sprintf(gsdline, "\tIDENT%c%s%s%o%cflags=%o\n", alignment, name, equals, value, alignment, flags);
            break;
        case 7:
            sprintf(gsdline, "\tVSECT%c%s%s%o%cflags=%o\n", alignment, name, equals, value, alignment, flags);
            badbin++;
            break;
        case 010:
            sprintf(gsdline, "\t" "COMPRTN" /* "Completion Routine Name" */
                    "%c%s%s%o%cflags=%o\n", alignment, name, equals, value, alignment, flags);
            badbin++;
            break;
        default:
            sprintf(gsdline, "\t***Unknown GSD entry type %d flags=%o\n", cp[i + 5] & 0xff, flags);
            if (!loud) fprintf(stderr, "%s", gsdline+1);
            badfmt++;
            break;
        }

        gsdline = memcheck(realloc(gsdline, strlen(gsdline) + 1));
        add_gsdline(gsdline);
    }
}

int compare_gsdlines(
    const void *p1,
    const void *p2)
{
    const char     *const *l1 = p1,
                   *const *l2 = p2;

    return strcmp(*l1, *l2);
}

void got_endgsd(
    char *cp,
    int len)
{
    int             i;

    (void)cp;
    (void)len;

    if (nr_gsds == 0) {
        return;
    }

    if (loud && sortgsd)
        qsort(all_gsds, nr_gsds, sizeof(char *), compare_gsdlines);

    if (loud) printf("GSD:\n");

    for (i = 0; i < nr_gsds; i++) {
        if (loud) fputs(all_gsds[i], stdout);
        free(all_gsds[i]);
    }

    if (loud) printf("ENDGSD\n");

    free(all_gsds);
    all_gsds = NULL;
    nr_gsds = 0;
    gsdsize = 0;
}

unsigned        last_text_addr = 0;

void got_text(
    char *cp,
    int len)
{
    unsigned        addr = WORD(cp + 2);

    last_text_addr = addr;

    if (loud) {
        printf("TEXT ADDR=%o LEN=%o\n", last_text_addr, len - 4);
        if (loudtxt)
            dump_words(last_text_addr, cp + 4, len - 4);
    }

    if (bin || word_patch)
        dump_bin(origin + last_text_addr, cp + 4, len - 4);
}

void rad50name(
    char *cp,
    char *name)
{
    unrad50(WORD(cp), name);
    unrad50(WORD(cp + 2), name + 3);
    name[6] = 0;
    trim(name);
}

void got_rld(
    char *cp,
    int len)
{
    int             i;

    if (loud) printf("RLD\n");

    for (i = 2; i < len;) {
        unsigned        addr;
        unsigned        word;
        unsigned        disp = cp[i + 1] & 0xff;
        char            name[8];
        char           *byte;

        addr = last_text_addr + disp - 4;

        byte = "";
        if (cp[i] & 0200)
            byte = " byte";

        if ((cp[i] & 0x7f) < RLDSIZ) rldcnt[cp[i] & 0x7f]++;
        switch (cp[i] & 0x7f) {
        case 01:
            word = WORD(cp + i + 2);
            if (loud) printf("\tInternal%s %o=%o\n", byte, addr, word);
            if (bin || word_patch) {
                char bytes[2];

                if (reloc_internal)
                    word += origin;

                bytes[0] = word & 0xff;
                bytes[1] = (word >> 8) & 0xff;
                dump_bin(origin + addr, bytes, 2);
            }
            i += 4;
            break;
        case 02:
            rad50name(cp + i + 2, name);
            if (loud) printf("\tGlobal%s %o=%s\n", byte, addr, name);
            i += 6;
            badbin++;
            break;
        case 03:
            if (loud) printf("\tInternal displaced%s %o=%o\n", byte, addr, WORD(cp + i + 2));
            i += 4;
            badbin++;
            break;
        case 04:
            rad50name(cp + i + 2, name);
            if (loud) printf("\tGlobal displaced%s %o=%s\n", byte, addr, name);
            i += 6;
            badbin++;
            break;
        case 05:
            rad50name(cp + i + 2, name);
            word = WORD(cp + i + 6);
            if (loud) printf("\tGlobal plus offset%s %o=%s+%o\n", byte, addr, name, word);
            i += 8;
            badbin++;
            break;
        case 06:
            rad50name(cp + i + 2, name);
            word = WORD(cp + i + 6);
            if (loud) printf("\tGlobal plus offset displaced%s %o=%s+%o\n", byte, addr, name, word);
            i += 8;
            badbin++;
            break;
        case 07:
            rad50name(cp + i + 2, name);
            word = WORD(cp + i + 6);
            if (loud) printf("\tLocation counter definition %s+%o\n", name, word);
            i += 8;

            last_text_addr = word;
            break;
        case 010:
            word = WORD(cp + i + 2);
            if (loud) printf("\tLocation counter modification %o\n", word);
            i += 4;

            last_text_addr = word;
            break;
        case 011:
            if (loud) printf("\t.LIMIT %o\n", addr);
            i += 2;
            badbin++;  /* .LIMIT will not be filled in by us */
            break;

        case 012:
            rad50name(cp + i + 2, name);
            if (loud) printf("\tPSECT%s %o=%s\n", byte, addr, name);
            i += 6;
            badbin++;
            break;
        case 014:
            rad50name(cp + i + 2, name);

            if (loud) printf("\tPSECT displaced%s %o=%s\n", byte, addr, name);
            i += 6;
            badbin++;
            break;
        case 015:
            rad50name(cp + i + 2, name);
            word = WORD(cp + i + 6);
            if (loud) printf("\tPSECT plus offset%s %o=%s+%o\n", byte, addr, name, word);
            i += 8;
            badbin++;
            break;
        case 016:
            rad50name(cp + i + 2, name);
            word = WORD(cp + i + 6);
            if (loud) printf("\tPSECT plus offset displaced%s %o=%s+%o\n", byte, addr, name, word);
            i += 8;
            badbin++;
            break;

        case 017:
            badbin++;
            if (loud) printf("\tComplex%s %o=", byte, addr);
            i += 2; {
                char           *xp = cp + i;
                int             size;

                for (;;) {
                    size = 1;
                    switch (*xp) {
                    case 000:
                        if (loud) fputs("nop ", stdout);
                        break;
                    case 001:
                        if (loud) fputs("+ ", stdout);
                        break;
                    case 002:
                        if (loud) fputs("- ", stdout);
                        break;
                    case 003:
                        if (loud) fputs("* ", stdout);
                        break;
                    case 004:
                        if (loud) fputs("/ ", stdout);
                        break;
                    case 005:
                        if (loud) fputs("& ", stdout);
                        break;
                    case 006:
                        if (loud) fputs("! ", stdout);
                        break;
                    case 010:
                        if (loud) fputs("neg ", stdout);
                        break;
                    case 011:
                        if (loud) fputs("^C ", stdout);
                        break;
                    case 012:
                        if (loud) fputs("store ", stdout);
                        break;
                    case 013:
                        if (loud) fputs("store{disp} ", stdout);
                        break;

                    case 016:
                        rad50name(xp + 1, name);
                        if (loud) printf("%s ", name);
                        size = 5;
                        break;

                    case 017:
                        assert((xp[1] & 0377) < psectid);
                        if (loud) printf("%s:%o ", psects[xp[1] & 0377], WORD(xp + 2));
                        size = 4;
                        break;

                    case 020:
                        if (loud) printf("%o ", WORD(xp + 1));
                        size = 3;
                        break;

                    default:
                        fprintf(stderr, "***UNKNOWN COMPLEX CODE** %o\n", *xp & 0377);
                        badfmt++;
                        return;
                    }
                    i += size;
                    if (*xp == 012 || *xp == 013)
                        break;
                    xp += size;
                }
                if (loud) fputc('\n', stdout);
                break;
            }

        default:
            fprintf(stderr, "%s***Unknown RLD code %o\n", (loud) ? "\t" : "", cp[i] & 0xff);
            badfmt++;
            return;
        }
    }
}

void got_isd(
    char *cp,
    int len)
{
    (void)cp;
    if (loud) printf("ISD len=%o\n", len);
}

void got_endmod(
    char *cp,
    int len)
{
    (void)cp;
    (void)len;
    if (loud) printf("ENDMOD\n");
}

void got_libhdr(
    char *cp,
    int len)
{
    (void)cp;
    (void)len;
    if (loud) printf("LIBHDR\n");
}

void got_libend(
    char *cp,
    int len)
{
    (void)cp;
    (void)len;
    if (loud) printf("LIBEND\n");
}

int main(
    int argc,
    char *argv[])
{
    int             len;
    FILE           *fp;
    int             arg;
    int             summary = 0;    /* Set to 1 by '-s' option */
    int             rt11 = DEFAULT_OBJECTFORMAT_RT11;
    char           *infile = NULL;
    char           *outfile = NULL;
    char           *cp;

    for (arg = 1; arg < argc; arg++) {
        if (*argv[arg] == '-') {
            cp = argv[arg] + 1;
            if (!strcasecmp(cp, "h")) {
                infile = NULL;
                break;
            } else if (!strcasecmp(cp, "q")) {
                loud = 0;
            } else if (!strcasecmp(cp, "qt")) {
                loudtxt = 0;
            } else if (!strcasecmp(cp, "align")) {
                alignment = '\t';
            } else if (!strcasecmp(cp, "noalign")) {
                alignment = ' ';
            } else if (!strcasecmp(cp, "nosort")) {
                sortgsd = 0;
            } else if (!strcasecmp(cp, "s")) {
                summary = 1;
            } else if (!strcasecmp(cp, "rt11")) {
                rt11 = 1;
            } else if (!strcasecmp(cp, "rsx")) {
                rt11 = 0;
            } else if (!strcasecmp(cp, "o") || !strcasecmp(cp, "of")) {
                char           *endptr;

                if (++arg >= argc) {
                    fprintf(stderr, "Missing <origin> to '-%s'\n", cp);
                    return /* exit */ (EXIT_FAILURE);
                }

                if (cp[1] == 'f') {
                    reloc_internal = 0;    /* Don't relocate Internal RLD addresses */
                } else {
                    reloc_internal = 1;    /* Relocate Internal RLD addresses */
                }

                origin = strtoul(argv[arg], &endptr, 8);
                /* strtoul() accepts "-" but, unless "-0", this creates a very big unsigned number */
                if (*endptr != '\0' || origin > MAX_LDA_ADDR || (origin & 1)) {
                    fprintf(stderr, "Invalid <origin> '%s'\n", argv[arg]);
                    return /* exit */ (EXIT_FAILURE);
                }
            } else if (!strcasecmp(cp, "w")) {
                if (++arg >= argc) {
                    fprintf(stderr, "Missing parameter to '-w'\n");
                    return /* exit */ (EXIT_FAILURE);
                }
                if (word_patch)    /* Note: we replace invalid '-w' entries without parsing them  */
                    fprintf(stderr, "'-w %s' replaced by '-w %s'\n", word_patch, argv[arg]);
                word_patch = argv[arg];
            } else if (!strcasecmp(cp, "x")) {
                char           *endptr;

                if (++arg >= argc) {
                    fprintf(stderr, "Missing <xfer> address to '-x'\n");
                    return /* exit */ (EXIT_FAILURE);
                }

                new_xferad = strtoul(argv[arg], &endptr, 8);
                if (*endptr != '\0' || new_xferad > MAX_LDA_ADDR) {
                    fprintf(stderr, "Invalid <xfer> address '%s'\n", argv[arg]);
                    return /* exit */ (EXIT_FAILURE);
                }
            } else {
                fprintf(stderr, "Unknown option '%s'\n", argv[arg]);
                return /* exit */ (EXIT_FAILURE);
            }
        }
        else if (infile == NULL) {
            infile = argv[arg];
        }
        else if (outfile == NULL) {
            outfile = argv[arg];
        }
        else {
            fprintf(stderr, "Extra parameter '%s'\n", argv[arg]);
            return /* exit */ (EXIT_FAILURE);
        }
    }

    /**** Display the usage ****/  

    if (infile == NULL) {
        fprintf(stderr, "\n"
                        "dumpobj - portable OBJECT file dumper for DEC PDP-11\n");
        fprintf(stderr, "  Version from " VERSION " compiled on " __DATE__ " at " __TIME__ "\n");
#ifdef GIT_VERSION
        fprintf(stderr, "      (git version: " GIT_VERSION ",\n"
                        "       git author date: " GIT_AUTHOR_DATE ")\n");
#endif
        fprintf(stderr, "  Copyright 2001 by Richard Krehbiel,\n");
        fprintf(stderr, "  modified  2013,2015,2020,2021 by Olaf 'Rhialto' Seibert,\n");
        fprintf(stderr, "  modified  2023 by Mike Hill.\n");
        fprintf(stderr, "\n"
                        "Usage:\n");
        fprintf(stderr, "  dumpobj [-h] [-q|-qt] [-[no]align] [-nosort] [-s] [-rt11|-rsx] <inputfile>\n");
        fprintf(stderr, "          [ [-o[f] <origin>] [" /*-b|*/ "-w [<addr>=]<data>] [-x <xfer>] <outputfile> ]\n");
        fprintf(stderr, "\n"
                        "Arguments:\n");
        fprintf(stderr, "  -h            show this help text%s\n", (argc <= 1) ? " with added examples" : "");
        fprintf(stderr, "  -q            quiet output - only errors will be shown\n");
        fprintf(stderr, "  -qt           quiet-text - omit the contents of TXT records\n");
#if (DEFAULT_ALIGNMENT != '\t')
        fprintf(stderr, "  -align        align fields when dumping GSD entries\n");
#else
        fprintf(stderr, "  -noalign      do not align fields when dumping GSD entries\n");
#endif
        fprintf(stderr, "  -nosort       do not sort the GSD entries (dump them in input order)\n");
        fprintf(stderr, "  -s            show a summary of all record types seen in the object file\n");
        fprintf(stderr, "  -rt11         expects object file to be in RT-11 format\n");
        fprintf(stderr, "  -rsx          expects object file to be in RSX-11M/PLUS format\n");
#if DEFAULT_OBJECTFORMAT_RT11 < 0
        fprintf(stderr, "  <inputfile>   required .OBJ input file (format can be auto-detected)\n");
#elif DEFAULT_OBJECTFORMAT_RT11 > 0
        fprintf(stderr, "  <inputfile>   required .OBJ input file (default is RT-11)\n");
#else
        fprintf(stderr, "  <inputfile>   required .OBJ input file (default is RSX-11M/PLUS)\n");
#endif
        fprintf(stderr, "\n"
                        "  -o <origin>   sets the octal origin for relocatable LDA code (default is 0)\n");
        fprintf(stderr, "  -of <origin>  as -o but keep 'internal relocations' fixed not origin based\n");
/*      fprintf(stderr, "  -b <a>=<d>,.. write bytes to the <outputfile> - [<addr>=]<data>[,...]]\n");  */
        fprintf(stderr, "  -w <a>=<d>,.. write words to the <outputfile> - [<addr>=]<data>[,...]]\n");
        fprintf(stderr, "  -x <xfer>     writes the octal transfer address to the <outputfile>\n");
        fprintf(stderr, "  <outputfile>  optional .LDA output file (in PDP-11 punched-tape format)\n");

        /* Only display the examples if at least one argument was on the command line */

        if (argc <= 1)
            return /* exit */ (EXIT_FAILURE);

        fprintf(stderr, "\n"
                        "Examples:\n");
        fprintf(stderr, "  dumpobj OBJECT.OBJ                            Dump contents of OBJECT.OBJ\n");
        fprintf(stderr, "  dumpobj -qt -align -nosort -s OBJECT.OBJ      Now dump in object file order\n");
        fprintf(stderr, "  dumpobj -q -s OBJECT.OBJ                      Only show a summary of records\n");
        fprintf(stderr, "  dumpobj -q -s OBJECT.OBJ -w 0                 Show errors ready for LDA o/p\n");
        fprintf(stderr, "  dumpobj -q OBJECT.OBJ PTAPE.LDA               Only create an LDA file\n");
        fprintf(stderr, "  dumpobj -q OBJECT.OBJ -o 1000 PTAPE.LDA       Relocate code to address 1000\n");
        fprintf(stderr, "  dumpobj EMPTY.OBJ -w 0,40=0,5237,0,774 -x 40  0:0,40:HALT,42:INC @#0,46:BR 40\n");
        fprintf(stderr, "  dumpobj EMPTY.OBJ -w ... -x ... PTAPE.LDA     Create an LDA file with data\n");

        return /* exit */ (EXIT_FAILURE);
    }

    /**** Start dumping the object file ****/

    fp = fopen(infile, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open %s\n", infile);
        return EXIT_FAILURE;
    }

    /* Auto-detect the format of the object file (if enabled)
     * If the first non-zero byte in <inputfile> > 1 assume RSX-11M/PLUS */

#if DEFAULT_OBJECTFORMAT_RT11 < 0
    if (rt11 < 0) {
        int             c;

        rt11 = 1;    /* Assume RT-11 (including empty or zero-filled file) */

        while (c = fgetc(fp), /* c != EOF && */ c == 0) /* continue */;
        if (c > 1) rt11 = 0;  /* Force to RSX-11M/PLUS */

        if (/* rewind */ fseek(fp, 0L, SEEK_SET)) {
            fprintf(stderr, "Unable to rewind %s\n", infile);
            return EXIT_FAILURE;
        }
#endif

    /*  if (loud) printf("Detected %s formatted object file\n", (rt11) ? "RT-11" : "RSX-11M/PLUS");  */
    }

    if (outfile != NULL) {
        bin = fopen(outfile, "wb");
        if (bin == NULL) {
            fprintf(stderr, "Unable to open %s\n", outfile);
            return EXIT_FAILURE;
        }
    }

    while ((cp = readrec(fp, &len, rt11)) != NULL) {
        if ((cp[0] & 0xff) < BLKSIZ) blkcnt[cp[0] & 0xff]++;
        switch (cp[0] & 0xff) {
        case 1:
            got_gsd(cp, len);
            break;
        case 2:
            got_endgsd(cp, len);
            break;
        case 3:
            got_text(cp, len);
            break;
        case 4:
            got_rld(cp, len);
            break;
        case 5:
            got_isd(cp, len);
            break;
        case 6:
            got_endmod(cp, len);
            break;
        case 7:
            got_libhdr(cp, len);
            badbin++;
            break;
        case 010:
            got_libend(cp, len);
            badbin++;
            break;
        default:
            fprintf(stderr, "***Unknown record type %o\n", cp[0] & 0xff);
            badfmt++;
            break;
        }

        free(cp);
    }

    fclose(fp);  /* We don't need the <inputfile> any more */

    /**** Display the summary ****/

    if (summary) {
        unsigned        i;
        int             stars = 0;
        int             showbin = (bin || word_patch);  /* Show LDA errors */

        for (i=0; i<BLKSIZ; i++) {
            if (blkcnt[i]) {
                printf("\nBLK:\n");
                break;
            }
        }
        for (i=0; i<BLKSIZ; i++) {
           if (blkcnt[i]) {
               if (*blkname[i] == '*') stars += blkcnt[i];
               printf("   %c%2.2o: %s = %-d\n", (showbin) ? *blkname[i] : ' ', i, &blkname[i][1], blkcnt[i]);
           }
        }

        if (blkcnt[1])
            printf("\nGSD:\n");
        for (i=0; i<GSDSIZ; i++) {
           if (gsdcnt[i]) {
               if (*gsdname[i] == '*') stars += gsdcnt[i];
               printf("   %c%2.2o: %s = %-d\n", (showbin) ? *gsdname[i] : ' ', i, &gsdname[i][1], gsdcnt[i]);
           }
        }

        if (blkcnt[4])
            printf("\nRLD:\n");
        for (i=0; i<RLDSIZ; i++) {
           if (rldcnt[i]) {
               if (*rldname[i] == '*') stars += rldcnt[i];
               printf("   %c%2.2o: %s = %-d\n", (showbin) ? *rldname[i] : ' ', i, &rldname[i][1], rldcnt[i]);
           }
        }

        if (showbin && stars) {
            printf("\n   *Lines above starting with an asterisk may cause binary output errors\n");
            if (!badbin) badbin = stars;
        }

#if 0
        if (psectid > 0) {
            printf("\nPSECTs:\n");
            for (i=0; i<psectid; i++) {
                printf("   %3d: %-6s\n", i, psects[i]);
                /* TODO: If we store more info about PSECTS, output it here */
            }
        }
#endif
    }

    /**** Warn about possible command line mistakes ****/

    if (summary || (loud && badfmt) || (loud && bin))
        printf("\n");

    if (!bin) {
        if (origin || !reloc_internal)
            fprintf(stderr, "With no <outputfile> '-o%s %o' is ignored\n", (reloc_internal) ? "" : "f", origin);

        if (new_xferad <= MAX_LDA_ADDR)
            fprintf(stderr, "With no <outputfile> '-x %o' is ignored\n", new_xferad);

        if (word_patch)
            fprintf(stderr, "With no <outputfile> '-w %s' is ignored\n", word_patch);
    }

    /**** Process the '-w' parameter ****/

    if (word_patch) {
        char           *endptr;
        unsigned        address = 0;
        unsigned        data;
        int             wantdata = 0;

        cp = word_patch;
        do {
            while (*cp == ' ') cp++;    /* Skip leading spaces if -w "addr=data" is used */

            data = strtoul(cp, &endptr, 8);    /* This accepts leading "+" or "-" but we catch "-n" below */
            if (endptr == cp) {
                if (*endptr)
                    fprintf(stderr, "Unexpected character in -w at '%s'\n", endptr);
                else
                    fprintf(stderr, "Missing -w <data> at end of '%s'\n", word_patch);
                badbin++;
                break;
            }
            if (*endptr == '=') {
                if (wantdata) {
                    fprintf(stderr, "Missing -w <data> or unexpected '=' at '%s'\n", --cp);
                    badbin++;
                    break;
                }
                wantdata++;
                address = data;
                cp = endptr + 1;
                if (*cp != '\0') endptr++;
                continue;
            }
            if (address >= MAX_LDA_ADDR || address & 1) {
                fprintf(stderr, "Invalid -w <addr> '%o' at '%s'\n", address, cp);
                badbin++;
                break;
            }
            if (data > 0xffff) {
                fprintf(stderr, "Invalid -w <data> '%o' at '%s'\n", data, cp);
                badbin++;
                break;
            }
            if (*endptr != ',' && *endptr != '\0') {
                fprintf(stderr, "Unexpected character in -w at '%s'\n", endptr);
                badbin++;
                break;
            }
         /* if (showbin) */ {
                char bytes[2];

                bytes[0] = data & 0xff;
                bytes[1] = (data >> 8) & 0xff;
                dump_bin(address, bytes, 2);
            }

            if (!bin)
                printf("-w would have patched %6.6o to %6.6o\n", address, data);

            wantdata = 0;
            address += 2;
            cp = endptr + 1;
        } while (*endptr != '\0');
    }

    /**** Process and report actual and potential errors ****/

    if (!bin)
        outfile = "<none>";

    if (bin || word_patch) {
        if (psects_with_data > 1) {
            fprintf(stderr, "More than one (%d) PSECT contains data\n", psects_with_data);
            badbin++;
        }

        if (new_xferad <= MAX_LDA_ADDR) {
            xferad = new_xferad;
        } else {
#if XFERAD_WHEN_ZERO
            if (xferad == 0) {
                fprintf(stderr, "Transfer address 0 has been modified to %d\n", XFERAD_WHEN_ZERO);
                xferad = XFERAD_WHEN_ZERO;
            }
#endif
        }

        if (highest_addr == 0)  /* If -w is specified, there will always be data */
            fprintf(stderr, "No data bytes written to binary file '%s'\n", outfile);

        if (origin < 0160000 && highest_addr >= 0160000)
            fprintf(stderr, "Binary file '%s' overlaps I/O space (160000-%o)\n",
                    outfile, (highest_addr > MAX_LDA_ADDR) ? MAX_LDA_ADDR : highest_addr);

        if (highest_addr > MAX_LDA_ADDR) {
            fprintf(stderr, "Binary file '%s' skipped data from %o-%o\n",
                    outfile, MAX_LDA_ADDR + 1, highest_addr);
            badbin++;
        }
    }

    if (badfmt) {
        fprintf(stderr, "Detected %d errors in the object file '%s'\n", badfmt, infile);
        xferad = BADBIN_XFERAD;
    }

    if (badbin && (bin || word_patch)) {
        fprintf(stderr, "Possibly %d mistake%s in the binary file '%s'\n",
                badbin, (badbin == 1) ? "" : "s", outfile);
        if (!summary)
            fprintf(stderr, "Try 'dumpobj -q -s ...' to see a summary of records with mistakes\n");
        xferad = BADBIN_XFERAD;
    }

    /* Write the transfer address and close the LDA file */

    if (bin) {
        dump_bin(xferad, NULL, 0);
        fclose(bin);

        if (badbin)
            return EXIT_FAILURE;
    }

    if (badfmt)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
