/* Routines for reading from an RT-11 macro library (like SYSMAC.SML) */

/*
Copyright (c) 2001, Richard Krehbiel
Copyright (c) 2017, Olaf Seibert
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

#include "rad50.h"
#include "stream2.h"
#include "mlb.h"
#include "util.h"

#define MLBDEBUG_OPEN           0

static MLB     *mlb_rt11_open(
    char *name,
    int allow_object_library);
static BUFFER  *mlb_rt11_entry(
    MLB *mlb,
    char *name);
static void     mlb_rt11_close(
    MLB *mlb);
static void     mlb_rt11_extract(
    MLB *mlb);

struct mlb_vtbl mlb_rt11_vtbl = {
    .mlb_open    = mlb_rt11_open,
    .mlb_entry   = mlb_rt11_entry,
    .mlb_extract = mlb_rt11_extract,
    .mlb_close   = mlb_rt11_close,
    .mlb_is_rt11 = 1,
};

/*
 * Format description:
 * http://www.bitsavers.org/pdf/dec/pdp11/rt11/v5.6_Aug91/AA-PD6PA-TC_RT-11_Volume_and_File_Formats_Manual_Aug91.pdf
 * pages 2-27 ff.
 *
 * A MLB Macro Library Header differs a lot from an Object Library Header.
 */

#define BLOCKSIZE               512

#define WORD(cp) ((*(cp) & 0xff) + ((*((cp)+1) & 0xff) << 8))

/* BYTEPOS calculates the byte position within the macro libray file.
   I use this to sort the entries by their start position, in order to
   be able to calculate the entries' sizes, which isn't actually
   stored in the directory. */

#define BYTEPOS(rec) ((WORD((rec)+4) & 32767) * BLOCKSIZE + \
                      (WORD((rec)+6) & 511))

/* compare_position is the qsort callback function that compares byte
   locations within the macro library */
static int compare_position(
    const void *arg1,
    const void *arg2)
{
    const char     *c1 = arg1,
            *c2 = arg2;

    if (BYTEPOS(c1) < BYTEPOS(c2))
        return -1;
    if (BYTEPOS(c1) > BYTEPOS(c2))
        return 1;
    return 0;
}


/* trim removes trailing blanks from a string. */
static void trim(
    char *buf)
{
    char           *cp = buf + strlen(buf);

    while (--cp >= buf && *cp == ' ')
        *cp = 0;
}

/* mlb_rt11_open opens a file which is given to be a macro library. */
/* Returns NULL on failure. */

static
MLB            *mlb_rt11_open(
    char *name,
    int allow_object_library)
{
    MLB            *mlb = memcheck(malloc(sizeof(MLB)));
    char           *buff;
    unsigned        entsize;
    unsigned        nr_entries;
    unsigned        start_block;
    int             i;

#if MLBDEBUG_OPEN
    fprintf(stderr, "mlb_rt11_open('%s', %d)\n", name, allow_object_library);
#endif
    (void)allow_object_library;         /* This parameter is not supported */
    mlb->vtbl = &mlb_rt11_vtbl;
    mlb->directory = NULL;

    mlb->fp = fopen(name, "rb");
    if (mlb->fp == NULL) {
        mlb_rt11_close(mlb);
        return NULL;
    }

    buff = memcheck(malloc(044));      /* Size of MLB library header */

    if (fread(buff, 1, 044, mlb->fp) < 044) {
        goto error;                    /* Nope. */
    }

    if (WORD(buff) != 01001 ||
        WORD(buff + 2) != 0500) {      /* Is this really a macro library? */
        goto error;                    /* Nope. */
    }

    entsize = WORD(buff + 032);        /* The size of each macro directory
                                          entry */
    nr_entries = WORD(buff + 036);     /* The number of directory entries */
    start_block = WORD(buff + 034);    /* The start RT-11 block of the
                                          directory */

    if (entsize < 8) {                 /* Is this really a macro library? */
        fprintf(stderr, "mlb_rt11_open error: entsize too small: %d\n", entsize);
        goto error;                    /* Nope. */
    }

    if (start_block < 1) {             /* Is this really a macro library? */
        fprintf(stderr, "mlb_rt11_open error: start_block too small: %d\n", start_block);
        goto error;                    /* Nope. */
    }

#if MLBDEBUG_OPEN
    fprintf(stderr, "entsize=%d, nr_entries=%d, start_block=%d\n",
            entsize, nr_entries, start_block);
#endif
    free(buff);                        /* Done with that header. */

    /* Allocate a buffer for the disk directory */
    buff = memcheck(malloc(nr_entries * entsize));
    /* Go to the directory */
    if (fseek(mlb->fp, start_block * BLOCKSIZE, SEEK_SET) == -1) {
        fprintf(stderr, "mlb_rt11_open error: seek error: %d\n", start_block * BLOCKSIZE);
        goto error;
    }

    /* Read the disk directory */
    if (fread(buff, entsize, nr_entries, mlb->fp) < nr_entries) {
        fprintf(stderr, "mlb_rt11_open error: fread: not enough entries\n");
        goto error;                    /* Sorry, read error. */
    }

    /* Shift occupied directory entries to the front of the array
       before sorting */
    {
        int             j;

        for (i = 0, j = nr_entries; i < j; i++) {
            char           *ent1,
                           *ent2;
            int             w1, w2;

            ent1 = buff + (i * entsize);
            w1 = WORD(ent1);
            w2 = WORD(ent1 + 2);
            /* Unused entries have 0177777 0177777 for the RAD50 name,
               which is not legal RAD50. */
            if (w1 == 0177777 && w2 == 0177777) {
                while (--j > i
                       && (ent2 = buff + (j * entsize), WORD(ent2) == 0177777 && WORD(ent2 + 2) == 0177777)) ;
                if (j <= i)
                    break;             /* All done. */
                memcpy(ent1, ent2, entsize);    /* Move used entry
                                                   into unused entry's
                                                   space */
                memset(ent2, 0377, entsize);    /* Mark entry unused */
            } else {
                if (w1 == 0000000 && w2 == 0000000) {
                    fprintf(stderr, "mlb_rt11_open error: bad file, null name\n");
                    goto error;
                }
            }
        }

        /* Now i contains the actual number of entries. */

        mlb->nentries = i;
#if MLBDEBUG_OPEN
        fprintf(stderr, "mlb->nentries=%d\n",  mlb->nentries);
#endif
        if (mlb->nentries == 0) {
            fprintf(stderr, "mlb_rt11_open error: no entries\n");
            goto error;
        }

        /* Sort the array by file position */

        qsort(buff, i, entsize, compare_position);

        /* Now, allocate my in-memory directory */
        mlb->directory = memcheck(malloc(sizeof(MLBENT) * mlb->nentries));
        memset(mlb->directory, 0, sizeof(MLBENT) * mlb->nentries);

        unsigned long   max_filepos;

        fseek(mlb->fp, 0, SEEK_END);
        max_filepos = ftell(mlb->fp);

        /* Build in-memory directory */
        for (j = 0; j < i; j++) {
            char            radname[16];
            char           *ent;

            ent = buff + (j * entsize);

            unrad50(WORD(ent), radname);
            unrad50(WORD(ent + 2), radname + 3);
            radname[6] = 0;

            trim(radname);
            if (radname[0] == '\0' || strchr(radname, ' ')) {
                fprintf(stderr, "mlb_rt11_open error: entry with space in name\n");
                goto error;
            }

            mlb->directory[j].label = memcheck(strdup(radname));
            mlb->directory[j].position = BYTEPOS(ent);

            if (mlb->directory[j].position > max_filepos) {
                fprintf(stderr, "mlb_rt11_open error: entry past EOF\n");
                goto error;
            }

            if (j < i - 1) {
                mlb->directory[j].length = BYTEPOS(ent + entsize) - BYTEPOS(ent);
            } else {
                unsigned long   max = max_filepos;
                char            c;

                /* Look for last non-zero */
                do {
                    max--;
                    fseek(mlb->fp, max, SEEK_SET);
                    c = fgetc(mlb->fp);
                } while (max > 0 && c == 0);
                max++;
                mlb->directory[j].length = max - BYTEPOS(ent);
            }
#if MLBDEBUG_OPEN
            fprintf(stderr, "entry #%d '%s' %ld length %d\n",
                    j,
                    mlb->directory[j].label,
                    mlb->directory[j].position,
                    mlb->directory[j].length);
#endif
        }

        free(buff);
    }

    /* Done.  Return the struct that represents the opened MLB. */
    return mlb;

error:
#if MLBDEBUG_OPEN
    fprintf(stderr, "(mlb_rt11_open closing '%s' due to errors)\n", name);
#endif
    mlb_rt11_close(mlb);      /* Sorry, bad file. */
    free(buff);
    return NULL;
}

/* mlb_rt11_close discards MLB and closes the file. */
static
void mlb_rt11_close(
    MLB *mlb)
{
    if (mlb) {
        int             i;

        if (mlb->directory) {
            for (i = 0; i < mlb->nentries; i++)
                if (mlb->directory[i].label)
                    free(mlb->directory[i].label);
            free(mlb->directory);
        }
        if (mlb->fp)
            fclose(mlb->fp);

        free(mlb);
    }
}

/* mlb_rt11_entry returns a BUFFER containing the specified entry from the
   macro library, or NULL if not found. */

static
BUFFER         *mlb_rt11_entry(
    MLB *mlb,
    char *name)
{
    int             i;
    MLBENT         *ent = NULL;
    BUFFER         *buf;
    char           *bp;
    int             c;

    for (i = 0; i < mlb->nentries; i++) {
        ent = &mlb->directory[i];
        if (strcmp(mlb->directory[i].label, name) == 0)
            break;
    }

    if (i >= mlb->nentries)
        return NULL;

    /* Allocate a buffer to hold the text */
    buf = new_buffer();
    buffer_resize(buf, ent->length + 1);        /* Make it large enough */
    bp = buf->buffer;

    fseek(mlb->fp, ent->position, SEEK_SET);

    for (i = 0; i < ent->length; i++) {
        c = fgetc(mlb->fp);            /* Get macro byte */
        if (c == '\r' || c == 0)       /* If it's a carriage return or 0,
                                          discard it. */
            continue;
        *bp++ = c;
    }
    *bp++ = 0;                         /* Store trailing 0 delim */

    /* Now resize that buffer to the length actually read. */
    buffer_resize(buf, (int) (bp - buf->buffer));

    return buf;
}

/* mlb_rt11_extract - walk thru a macro library and store its contents
   into files in the current directory.

   See, I had decided not to bother writing macro library maintenance
   tools, since the user can call macros directly from the file
   system.  But if you've already got a macro library without the
   sources, you can use this to extract the entries and maintain them
   in the file system from thence forward.
*/

static
void mlb_rt11_extract(
    MLB *mlb)
{
    int             i;
    FILE           *fp;
    BUFFER         *buf;

    for (i = 0; i < mlb->nentries; i++) {
        char            name[32];

        buf = mlb_entry(mlb, mlb->directory[i].label);
        sprintf(name, "%s.MAC", mlb->directory[i].label);
        fp = fopen(name, "w");
        fwrite(buf->buffer, 1, buf->length, fp);
        fclose(fp);
        buffer_free(buf);
    }
}
