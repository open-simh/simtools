/*
 * Sector layout converter for single-sided floppy disk images
 * (Logical to Physical, and back)
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


inline
static size_t filesize(FILE* fp)
{
    return fseek(fp, 0, SEEK_END) != 0 ? -1L : ftell(fp);
}


static int tracks = 0;
static int sectors = 0;
static int sector_size = 0;

static int interleave = 0;
static int track_skew = 0;
static int track0 = 0;

static int round = 0;


static int gcd(int a, int b)
{
    while (a  &&  b) {
        if (a > b)
            a %= b;
        else
            b %= a;
    }
    return a + b;
}


inline
static int lcm(int a, int b)
{
    return (a * b) / gcd(a, b);
}


static int lbn2pbn(int lbn)
{
    int track;
    int sector;

    track   = lbn / sectors;

    /* interleave */
    sector  = lbn % sectors;
    sector *= interleave;
    sector += sector / round;

    /* track skew */
    sector += track * track_skew;

    /* PBN */
    sector %= sectors;
    if (!track0)
        ++track;
    track  %= tracks;

    return track * sectors + sector;
}


#ifdef __GNUC__
__attribute__((noreturn))
#endif
static void usage(const char* prog)
{
    fprintf(stderr, "%s -T tracks -S sectors -B sector_size"
                    " -i interleave -k track_skew [-0] [-r] infile outfile\n",
                    prog);

    fprintf(stderr, "\nTypical parameters:\n"
                    "RX01:                   -T 77 -S 26 -B 128 -i 2 -k 6\n"
                    "RX02 (single density):  -T 77 -S 26 -B 128 -i 2 -k 6\n"
                    "RX02 (double density):  -T 77 -S 26 -B 256 -i 2 -k 6\n"
                    "RX50:                   -T 80 -S 10 -B 512 -i 2 -k 2\n"
                    "\nSupported sector sizes (in bytes): 128, 256, 512\n\n");

    fprintf(stderr, "-0 = To leave track 0 in place (else, wrapped around)\n"
                    "-r = To do an inverse (PBN to LBN) conversion\n");
    exit(2);
}


#ifndef __CYGWIN__
extern int opterr, optind;
extern char* optarg;
#  ifndef _MSC_VER
#    define _stricmp  strcmp
#  endif
#else
#  define   _stricmp  strcasecmp
#endif


int main(int argc, char* argv[])
{
    int reverse = 0;
    const char* infile;
    const char* outfile;
    size_t max_size;
    size_t size;
    FILE* in;
    FILE* out;
    int p, q;

    p = optind;
    while ((q = getopt(argc, argv, "T:S:B:i:k:0r")) != EOF) {
        switch (q) {
        case 'T':
            tracks = atoi(optarg);
            break;
        case 'S':
            sectors = atoi(optarg);
            break;
        case 'B':
            sector_size = atoi(optarg);
            if ((sector_size & (sector_size - 1))
                ||  sector_size < 128  ||  sector_size > 512) {
                sector_size = 0;
            }
            break;
        case 'i':
            interleave = atoi(optarg);
            break;
        case 'k':
            track_skew = atoi(optarg);
            break;
        case '0':
            track0 = 1;
            break;
        case 'r':
            if (!reverse) {
                reverse = 1;
                break;
            }
            /*FALLTHRU*/
        default:
            usage(argv[0]);
        }
        p = optind;
    }
    if (argv[p]  &&  strcmp(argv[p], "--") == 0)
        ++p;
    if (tracks <= 0  ||  sectors <= 0  ||  interleave <= 0  ||  track_skew < 0
        ||  interleave >= sectors  ||  sector_size <= 0  ||  p != optind
        ||  !(infile = argv[p++])  ||  !(outfile = argv[p++])  ||  argv[p]
        ||  _stricmp(infile, outfile) == 0) {
        usage(argv[0]);
    }

    if (!(in = fopen(infile, "rb"))) {
        perror(infile);
        return 1;
    }

    size = filesize(in);
    q = tracks * sectors;
    max_size = q * sector_size;
    if (size > max_size) {
        fprintf(stderr, "%s: file size (%zd) may not exceed %zu\n", infile, size, max_size);
        return 1;
    }

    if (!(out = fopen(outfile, "wb"))) {
        perror(outfile);
        return 1;
    }

    rewind(in);
    round = lcm(sectors, interleave);
    for (p = 0;  p < q;  ++p) {
        char sector[512];
        int n = lbn2pbn(p);
#ifdef _DEBUG
        printf("LBN %4d (%2d / %2d)   =   PBN %4d (%2d / %2d)\n",
               p, p / sectors, p % sectors,
               n, n / sectors, n % sectors);
#endif
        if ( reverse  &&  fseek(in,  n * sector_size, SEEK_SET) != 0)
            abort();
        if (fread (sector, sector_size, 1, in)  != 1)
            memset(sector, 0, sector_size);
        if (!reverse  &&  fseek(out, n * sector_size, SEEK_SET) != 0)
            abort();
        if (fwrite(sector, sector_size, 1, out) != 1)
            abort();
    }

    fclose(out);
    fclose(in);
    return 0;
}
