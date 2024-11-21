/*
 * IMD -> DSK (pure data) file converter
 *
 * Copyright (c) 2024 Tony Lawrence
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above
 * copyright notice, this list of conditions and the following
 * disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3.  Neither the name of the copyright holder nor the names
 * of its contributors may be used to endorse or promote
 * products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_MSC_VER)
#  define PACKED(...)                           \
    __pragma(pack(push, 1))                     \
    __VA_ARGS__                                 \
    __pragma(pack(pop))
#elif defined(__GNUC__)
#  define PACKED(...)                           \
    __VA_ARGS__                                 \
    __attribute__((packed))
#endif


typedef unsigned char  uint1;
typedef   signed char   int1;
typedef unsigned short uint2;
typedef          short  int2;
typedef uint32_t       uint4;
typedef  int32_t        int4;


/* IMD track header */
PACKED(struct S_IMDTrkHdr {
    uint1       mode;               /* Track write mode (check only)    */
    uint1       cyl;                /* Cylinder #                       */
    uint1       head;               /* Head(side) 0|1 and map flags     */
    uint1       nsect;              /* Number of sectors to follow      */
    uint1       ssize;              /* Sector size or 0xFF for size tbl */
});

#define MODE_MAX        5           /* Maximal valid track write mode   */

#define SECTOR_CYL_MAP  0x80        /* Cylinder map for each sector     */
#define SECTOR_HEAD_MAP 0x40        /* Head map for each sector         */

#define SECTOR_SIZE_TBL 0xFF        /* Sector size table for each sector*/
#define SECTOR_SIZE_MAX 8192        /* Maximal sector size supported    */

#define IMD_HEADER_END  '\x1A'      /* Ctrl-Z (Text file EOF in MS-DOS) */


static int verbose = 0;

#define VERBOSE_SUMMARY 1
#define VERBOSE_HEADER  2
#define VERBOSE_TRACK   3
#define VERBOSE_SECTOR  4


/* Check IMD file header and return file pos where it ends */
static long x_skip_header(FILE* fp, const char* file)
{
    int major, minor, ch;
    struct tm tm, tmp;

    memset(&tm, 0, sizeof(tm));
    if (fscanf(fp, "IMD %d.%d: %d/%d/%d %d:%d:%d",
               &major, &minor,
               &tm.tm_mday, &tm.tm_mon, &tm.tm_year,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 8) {
        goto out;
    }
    if (major <= 0  ||  minor < 0)
        goto out;
    if (tm.tm_mday < 1  ||  tm.tm_mday > 31  ||
        tm.tm_mon  < 1  ||  tm.tm_mon  > 12  ||
        tm.tm_year < 1900  ||
        tm.tm_hour < 0  ||  tm.tm_hour > 23  ||
        tm.tm_min  < 0  ||  tm.tm_min  > 59  ||
        tm.tm_sec  < 0  ||  tm.tm_sec  > 60) {
        goto out;
    }
    tm.tm_mon--;
    tm.tm_year -= 1900;
    memcpy(&tmp, &tm, sizeof(tmp));
    if (mktime(&tmp) == (time_t)(-1L))
        goto out;
    if (tm.tm_mday != tmp.tm_mday  ||
        tm.tm_mon  != tmp.tm_mon   ||
        tm.tm_year != tmp.tm_year) {
        /* NB:  No check for time because
         * the ranges are already okay yet
         * DST can screw up the hours... */
        goto out;
    }

    ch = fgetc(fp);
    if (ch != IMD_HEADER_END) {
        int/*bool*/ space = 1/*true*/;
        if (!isspace(ch))
            goto out;
        if (verbose >= VERBOSE_HEADER) {
            char timebuf[80];
            strftime(timebuf, sizeof(timebuf), "%d/%m/%Y %T", &tm);
            printf("IMD %d.%d: %s\n", major, minor, timebuf);
        }
        for (;;) {
            ch = fgetc(fp);
            if (ch == IMD_HEADER_END)
                break;
            if (isspace(ch)) {
                if (space)
                    continue;
            } else {
                if (!isprint(ch))
                    goto out;
                space = 0/*false*/;
            }
            if (verbose >= VERBOSE_HEADER)
                putchar(ch);
        }
        if (!space  &&  verbose >= VERBOSE_HEADER)
            putchar('\n');
    }

    return ftell(fp);

out:
    fprintf(stderr, "%s: Invalid IMD file header\n", file);
    return 0;
}


inline
static int/*bool*/ x_check_mode(uint1 mode)
{
    return mode > MODE_MAX ? 0/*false*/ : 1/*true*/;
}


inline
static uint2 x_sector_size(uint1 ssize)
{
    return ssize > 6 ? SECTOR_SIZE_MAX + 1 : 1 << (7 + ssize);
}


static int/*bool*/ x_skip_sector(FILE* fp, uint2 ssize)
{
    int ch = fgetc(fp);
    if (ch == EOF)
        return 0/*false*/;
    if (ch < 0  ||  ch > 8)
        return 0/*false*/;
    if (ch == 0)
        return 1/*true*/;
    return fseek(fp, ch & 1 ? ssize : 1, SEEK_CUR) == 0 ? 1/*T*/ : 0/*F*/;
}


/* Disk parameters */
static int cyls = 0;
static int heads = 0;
static int sectors = 0;
static int sector_size = 0;

/* Sector numbering */
static int max_sect = 0;
static int zero_sect = 0;
static int one_based = 1;


/* Scan the file, set up disk params and return the number of tracks found */
static int x_scan_tracks(FILE* fp, const char* file)
{
    const char* problem = 0;
    uint1 smap[256];
    char buf[80];
    int track;

    for (track = 0; ; ++track) {
        struct S_IMDTrkHdr hdr;
        uint2* stable;
        uint2 ssize;
        long skip;
        int n;

        if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
            if (feof(fp)  &&  track)
                break;
            problem = "File read error";
            goto out;
        }
        if (!x_check_mode(hdr.mode)) {
            sprintf(buf, "Unrecognized track write mode %u", hdr.mode);
            problem = buf;
            goto out;
        }
        ssize = hdr.ssize;
        if (ssize != SECTOR_SIZE_TBL) {
            ssize  = x_sector_size(hdr.ssize);
            assert(ssize);
            if (ssize & (ssize - 1)) {
                sprintf(buf, "Invalid sector size %hu", ssize);
                problem = buf;
                goto out;
            }
            assert(ssize <= SECTOR_SIZE_MAX);
            if (!sector_size)
                sector_size = ssize;
            else if (sector_size != ssize) {
                sprintf(buf, "Sector size change %d -> %hu", sector_size, ssize);
                problem = buf;
                goto out;
            }
        }
        n = hdr.head & ~(SECTOR_CYL_MAP | SECTOR_HEAD_MAP);
        if (n & ~1) {
            sprintf(buf, "Invalid head number %d", n);
            problem = buf;
            goto out;
        }
        if (heads <= n)
            heads++;
        if (!hdr.nsect)
            continue;
        if (sectors < hdr.nsect)
            sectors = hdr.nsect;
        if (fread(smap, hdr.nsect, 1, fp) != 1) {
            problem = "Cannot read sector map";
            goto out;
        }
        for (n = 0;  n < hdr.nsect;  ++n) {
            if (!smap[n]) {
                ++zero_sect;
                continue;
            }
            if (max_sect < smap[n])
                max_sect = smap[n];
        }
        skip = 0;
        if (hdr.head & SECTOR_CYL_MAP)
            skip += hdr.nsect;
        if (hdr.head & SECTOR_HEAD_MAP)
            skip += hdr.nsect;
        if (skip  &&  fseek(fp, skip, SEEK_CUR) != 0) {
            problem = "File positioning error";
            goto out;
        }
        if (ssize == SECTOR_SIZE_TBL) {
            if (!(stable = (uint2*) malloc(sizeof(*stable) * hdr.nsect))) {
                problem = "Cannot allocate for sector size table";
                goto out;
            }
            if (fread(stable, sizeof(*stable), hdr.nsect, fp) != hdr.nsect) {
                problem = "Cannot read sector size table";
                goto out;
            }
        } else
            stable = 0;
        for (n = 0;  n < hdr.nsect;  ++n) {
            if (stable) {
                ssize = stable[n];
                if (!ssize  ||  (ssize & (ssize - 1))  ||  ssize > SECTOR_SIZE_MAX) {
                    sprintf(buf, "Invalid sector size %hu in sector %d",
                            ssize, n + 1);
                    problem = buf;
                    break;
                }
                if (!sector_size)
                    sector_size = ssize;
                else if (sector_size != ssize) {
                    sprintf(buf, "Sector size change %d -> %hu in sector %d",
                            sector_size, ssize, n + 1);
                    problem = buf;
                    break;
                }
            } else
                assert(ssize  &&  !(ssize & (ssize - 1))  &&  ssize <= SECTOR_SIZE_MAX);
            if (!x_skip_sector(fp, ssize)) {
                sprintf(buf, "Error skipping sector %d (size %hu)", n + 1, ssize);
                problem = buf;
                break;
            }
        }
        if (stable)
            free(stable);
        if (n < hdr.nsect) {
            assert(problem);
            goto out;
        }
    }

    return track;

out:
    assert(problem);
    fprintf(stderr, "%s (scanning track data %d): %s\n", file, track, problem);
    return 0/*error*/;
}


/* Summary */
static int n_restored = 0;
static int n_compressed = 0;

/* Storage map */
static uint1* map = 0;


static int/*bool*/ x_extract_sector(int C, int H, int S, int track,
                                    FILE* in,  const char* infile,
                                    FILE* out, const char* outfile)
{
    static uint1 sector[SECTOR_SIZE_MAX];
    int/*bool*/ duplicate = 0/*false*/;
    const char* problem = 0;
    const char* file = 0;
    int ch;

    /* Disk Address */
    int da = (C * heads + H) * sectors + S - one_based;
    assert(0 <= da  &&  da < cyls * heads * sectors);

    if (map[da >> 3] & (1 << (da & 7))) {
        fprintf(stderr, "%s: WARNING -- Duplicate sector %d/%d/%d in track data %d\n",
                infile, C, H, S, track);
        duplicate = 1/*true*/;
    } else
        map[da >> 3] |= 1 << (da & 7);

    ch = fgetc(in);
    if (ch == EOF) {
        problem = "File read error";
        file = infile;
        goto out;
    }

    switch (ch) {
    case 0:
        problem = "Sector data unavailable";
        break;
    case 1: case 2:
        /* normal sector data */
        assert(!problem);
        break;
    case 3: case 4:
        problem = "Deleted data";
        break;
    case 5: case 6:
        problem = "Sector data with error";
        break;
    case 7: case 8:
        problem = "Deleted data with error";
        break;
    default:
        /* NB: this should never happen per x_skip_sector() */
        abort();
    }
    if (problem)
        fprintf(stderr, "%s: WARNING -- %d/%d/%d: %s\n", infile, C, H, S, problem);

    assert(0 < sector_size  &&  sector_size <= (int) sizeof(sector));
    if (ch & 1) {
        if (fread(sector, sector_size, 1, in) != 1) {
            problem = "Sector data read error";
            file = infile;
            goto out;
        }
    } else {
        if (ch) {
            ch = fgetc(in);
            if (ch == EOF) {
                problem = "Compressed sector data read error";
                file = infile;
                goto out;
            }
        } /* else: technically it's still a "compressed" sector */
        memset(sector, ch, sector_size);
        if (!duplicate)
            ++n_compressed;
    }

    da *= sector_size;
    if (fseek(out, da, SEEK_SET) != 0) {
        problem = "File positioning error";
        file = outfile;
        goto out;
    }
    if (fwrite(sector, sector_size, 1, out) != 1) {
        problem = "File write error";
        file = outfile;
        goto out;
    }

    if (!duplicate)
        ++n_restored;
    return 1/*true*/;

out:
    assert(file  &&  problem);
    fprintf(stderr, "%s (extracting track data %d for %d/%d/%d): %s\n",
            file, track, C, H, S, problem);
    return 0/*false*/;
}


static int/*bool*/ x_extract_track(int track, int/*bool*/ no_map,
                                   FILE* in,  const char* infile,
                                   FILE* out, const char* outfile)
{
    static uint1 cmap[256];
    static uint1 hmap[256];
    static uint1 smap[256];
    const char* problem = 0;
    struct S_IMDTrkHdr hdr;
    char buf[80];
    long skip;
    int n;

    if (verbose >= VERBOSE_TRACK)
        fprintf(stderr, "Track data %d\n", track);

    if (fread(&hdr, sizeof(hdr), 1, in) != 1) {
        problem = "File read error";
        goto out;
    }
    assert(x_check_mode(hdr.mode));
    if (!hdr.nsect)
        return 1/*true*/;

    if (fread(smap, hdr.nsect, 1, in) != 1) {
        problem = "Cannot read sector map";
        goto out;
    }
    skip = 0;
    if (no_map) {
        if (hdr.head & SECTOR_CYL_MAP)
            skip += hdr.nsect;
        if (hdr.head & SECTOR_HEAD_MAP)
            skip += hdr.nsect;
        hdr.head &= ~(SECTOR_CYL_MAP | SECTOR_HEAD_MAP);
    } else {
        if ((hdr.head & SECTOR_CYL_MAP)
            &&  fread(cmap, hdr.nsect, 1, in) != 1) {
            problem = "Cannot read cylinder map";
            goto out;
        }
        if ((hdr.head & SECTOR_HEAD_MAP)
            &&  fread(hmap, hdr.nsect, 1, in) != 1) {
            problem = "Cannot read head map";
            goto out;
        }
    }
    if (hdr.ssize == SECTOR_SIZE_TBL)
        skip += sizeof(uint2) * hdr.nsect;
    if (skip  &&  fseek(in, skip, SEEK_CUR) != 0) {
        problem = "File positioning error";
        goto out;
    }

    for (n = 0;  n < hdr.nsect;  ++n) {
        int C = hdr.head & SECTOR_CYL_MAP  ? cmap[n] : hdr.cyl;
        int H = hdr.head & SECTOR_HEAD_MAP ? hmap[n] : hdr.head & 0xF;
        int S = smap[n];
        if (verbose >= VERBOSE_SECTOR)
            fprintf(stderr, "Extracting: %d/%d/%d\n", C, H, S);
        if (C < 0  ||  C >= cyls) {
            sprintf(buf, "Cylinder %d out of range [0..%d]", C, cyls - 1);
            problem = buf;
            goto out;
        }
        if (H < 0  ||  H >= heads) {
            sprintf(buf, "Head %d out of range [0..%d]", H, heads - 1);
            problem = buf;
            goto out;
        }
        if (S < one_based  ||  S >= sectors + one_based) {
            sprintf(buf, "Sector %d out of range [%d..%d]", S,
                    one_based, sectors - !one_based);
            problem = buf;
            goto out;
        }
        if (!x_extract_sector(C, H, S, track, in, infile, out, outfile))
            return 0/*false*/;
    }

    return 1/*true*/;

out:
    assert(problem);
    fprintf(stderr, "%s (extracting track data %d): %s\n", infile, track, problem);
    return 0/*false*/;
}


#ifdef __GNUC__
__attribute__((noreturn))
#endif
static void usage(const char* prog)
{
    fprintf(stderr, "%s [-i] [-v...] infile outfile\n\n", prog);
    fprintf(stderr, "-i = Ignore cylinder / head maps\n"
                    "-v = Increase verbosity with each occurrence\n");
    exit(2);
}


#if    defined(__CYGWIN__)
#  define _stricmp  strcasecmp
#elif !defined(_MSC_VER)
#  define _stricmp  strcmp
#else
#  define  strcmp  _strcmp
#endif


int main(int argc, char* argv[])
{
    /* whether to ignore cyl / head maps */
    int/*bool*/ no_map = 0/*false*/;
    const char* infile;
    const char* outfile;
    int p, q, tracks;
    FILE* in;
    FILE* out;
    long pos;

    p = 1;
    if (argc > 1) {
        do {
            if (strcmp(argv[p], "-v") == 0) {
                ++verbose;
                continue;
            }
            if (strcmp(argv[p], "-i") != 0)
                break;
            if (no_map)
                usage(argv[0]);
            no_map = 1/*ignore map*/;
        } while (argv[++p]);
    }
    if (p < argc  &&  strcmp(argv[p], "--") == 0)
        ++p;
    if (argc < 3
        ||  !(infile = argv[p++])  ||  !(outfile = argv[p++])  ||  argv[p]
        ||  _stricmp(infile, outfile) == 0) {
        usage(argv[0]);
    }

    if (!(in = fopen(infile, "rb"))) {
        perror(infile);
        return EXIT_FAILURE;
    }

    if (!(pos = x_skip_header(in, infile)))
        return EXIT_FAILURE;
    if (pos == -1L) {
        fprintf(stderr, "%s: File positioning error", infile);
        return EXIT_FAILURE;
    }

    if (!(tracks = x_scan_tracks(in, infile)))
        return EXIT_FAILURE;
    assert(heads <= 2  &&  sectors <= 255  &&  sector_size <= SECTOR_SIZE_MAX);

    if (heads <= 0  ||  sectors  <= 0  ||  sector_size <= 0
        ||  (cyls = (tracks + heads - 1) / heads) <= 0  ||  255 < cyls) {
        fprintf(stderr, "%s: Failed to determine disk geometry, sorry\n", infile);
        return EXIT_FAILURE;
    }

    if (zero_sect > (tracks >> 2)  &&  max_sect < sectors)
        one_based = 0;
    if (verbose >= VERBOSE_SUMMARY) {
        fprintf(stderr, "%s: CHS = %d/%d/%d; Sector size = %d%s\n",
                infile, cyls, heads, sectors, sector_size,
                one_based ? "" : "; 0-based sector numbering");
    }

    if (fseek(in, pos, SEEK_SET) != 0) {
        perror(infile);
        return EXIT_FAILURE;
    }

    q = cyls * heads * sectors;
    assert(0 < q  &&  q <= 255 * 2 * 255 /*130050*/);
    if (!(map = (uint1*) calloc((q + 7) / 8, sizeof(*map)))) {
        perror(outfile);
        return EXIT_FAILURE;
    }

    if (!(out = fopen(outfile, "wb"))) {
        perror(outfile);
        return EXIT_FAILURE;
    }

    for (p = 0;  p < tracks;  ++p) {
        if (!x_extract_track(p, no_map, in, infile, out, outfile))
            return EXIT_FAILURE;
    }

    for (p = 0;  p < q;  p += 8) {
        int k, n = p >> 3;
        if (map[n] == (1 << 8) - 1)
            continue;
        for (k = 0;  k < 8;  ++k) {
            int C, H, S = p | k;
            if (S >= q)
                break;
            if (map[n] & (1 << k))
                continue;
            H  = S / sectors;
            S %= sectors;
            C  = H / heads;
            H %= heads;
            fprintf(stderr, "%s: WARNING -- Sector not stored: %d/%d/%d\n",
                    outfile, C, H, S + one_based);
        }
    }

    free(map);

    if (fclose(out) != 0) {
        fprintf(stderr, "%s: Closing error\n", outfile);
        return EXIT_FAILURE;
    }
    if (verbose >= VERBOSE_SUMMARY) {
        printf("%s: Total sectors restored: %d", outfile, n_restored);
        if (n_compressed)
            printf(", of those compressed: %d\n", n_compressed);
        else
            putchar('\n');
    }
    fclose(in);
    return EXIT_SUCCESS;
}
