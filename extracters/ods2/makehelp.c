/* Copyright (c) Timothe Litt <litt@acm.org>
 */

/* Help file compiler
 */

/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors.   This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "compat.h"

#define HELPFILEDEFS
#include "cmddef.h"

#define INEXT  ".hlp"
#define OUTEXT ".hlb"
#define LSTEXT ".lst"

#ifdef _WIN64
#define OUTPFX "win64-ods2_"
#define DDLIM '\\'
#elif defined( _WIN32 )
#define OUTPFX "win-ods2_"
#define DDLIM '\\'
#elif defined( VMS )
#define OUTPFX "ods2_"
#define DDLIM ']'
#else
#define OUTPFX "ods2_"
#define DDLIM '/'
#endif

/* Leaks are not an issue since this program exits after every command.
 * However, the leak detector is handy for debugging other issues.
 */
#if defined( _WIN32 ) && defined(_MSC_VER) && defined( DEBUG_BUILD ) && defined( USE_VLD )
/* Normally done in the project file, but VLD is optional and I'd rather not provide
 * instructions as they vary by IDE version.  See http://vld.codeplex.com/ if interested.
 */
#include "C:\\Program Files (x86)\\Visual Leak Detector\\include\\vld.h"
#pragma comment (lib,"C:\\Program Files (x86)\\Visual Leak Detector\\lib\\Win32\\vld.lib")
#endif

typedef struct hlpstat {
    size_t topics[9];
    size_t keymem[9];
    size_t txtmem[9];
    size_t ntopics;
    size_t keymemtot;
    size_t txtmemtot;
} hlpstat_t;

static void pstats( FILE *of, char *outname, hlpstat_t *stats, size_t memsize );
static size_t hlpsize( hlproot_t *root );
static int rdhelp( hlproot_t *root, const char *filename );
static void helpsort( hlproot_t *root );
static int hlpcmp( const void *qa, const void *qb );
static void savehlp( hlproot_t *oroot, hlproot_t *iroot,
                     char *base, char **sp, char **pp,
                     unsigned lvl, hlpstat_t *stats );
static int printdata( FILE *of, unsigned level, hlproot_t *root,
                      int text, hlpstat_t *stats );

/* ******************************************************************* main() */

int main( int argc, char **argv ) {
    int opth = 0, optd = 0, optD = 0, optq = 0;
    char *inname = NULL, *outname = NULL, *pname;
    int outmalloced = FALSE;
    size_t memsize = 0;
    hlproot_t inroot;
    hlphdr_t *hdr;
    hlpstat_t stats;
    struct stat instat;
    char *base, *pp, *sp;
    FILE *of = NULL;

    memset( &inroot, 0, sizeof( inroot ) );
    inroot.topics.ptr = NULL;
    inroot.ntopics = 0;

    pname = argv[0];
    if( argc <= 1 ) opth = TRUE;
    for( ; argc > 1; --argc, ++argv ) {
        if( !strcmp( argv[1], "--" ) ) break;
        if( *argv[1] == '-' && argv[1][1] ) {
            char *p;

            for( p = argv[1]+1; *p; p++ ) {
                switch( *p ) {
                case 'h':
                    opth = TRUE;
                    break;
                case 'd':
                    optd = TRUE;
                    break;
                case 'D':
                    optD = TRUE;
                    break;
                case 'q':
                    optq = TRUE;
                    break;
                default:
                    fprintf( stderr, "Unknown option %c\n", argv[1][1] );
                    exit( EXIT_FAILURE );
                }
            }
        } else {
            break;
        }
    }
    if( opth ) {
        printf( "%s [opts] [in" INEXT " [out" OUTEXT "]]\n"
                " -h This help\n"
                " -d Dump structure of input\n"
                " -D Dump structure of input with topic text\n"
                " -q Suppress statistics report\n"
                " -- End of options\n"
                " -  is stdin/out\n"
                "If an output file is not specified, the input filename\n"
                "prefixed by " OUTPFX " is used.  " OUTEXT " replaces the "
                "input name's extension.\n"
                "With -d or -D, " LSTEXT " replaces the input name's extension.\n"
                "The " OUTEXT " files are architecture-dependent.\n"
                "Under Windows, the default output file is prefixed\n"
                "with 'win-' or 'win64-'.\n", pname? pname : "makehelp" );
        exit( EXIT_SUCCESS );
    }
    if( argc > 1 ) {
        if( strcmp( argv[1], "-" ) ) {
            inname = argv[1];
        }
        --argc; ++argv;
    }
    if( argc > 1 ) {
        if( strcmp( argv[1], "-" ) ) {
            outname = argv[1];
        }
        --argc; ++argv;
    } else {
        if( inname != NULL ) {
            char *p, *outext;
            size_t inlen, extsiz, addlen;
            ptrdiff_t plen = 0;

            inlen = strlen( inname );
            outext = ( optd || optD )? LSTEXT  : OUTEXT;
            addlen =
                extsiz = strlen( outext ) +1;
#ifdef OUTPFX
            addlen += sizeof( OUTPFX ) -1;
#endif
            outname = malloc( inlen + addlen );
            if( outname == NULL ) {
                perror( "malloc" );
                exit( EXIT_FAILURE );
            }
            outmalloced = TRUE;
#ifdef OUTPFX
            if( (p = strrchr( inname, DDLIM )) != NULL ) {
                plen = (p - inname) +1;
                p = outname + plen;
                memcpy( outname, inname, plen );
            } else
                p = outname;
            memcpy( p, OUTPFX, sizeof( OUTPFX ) -1 );
            memcpy( p + sizeof( OUTPFX ) -1,
                    inname + plen, strlen( inname + plen ) +1 );
#else
            memcpy( outname, inname + plen, inlen +1 ); /* plen == 0, ensures it's used */
#endif
            if( (p = strrchr( outname, '.' )) != NULL && strchr( p, DDLIM ) == NULL ) {
                *p = '\0';
            }
            memcpy( outname + strlen(outname), outext, extsiz );
        }
    }
    if( argc >1 ) {
        fprintf( stderr, "Too many command arguments\n" );
        exit( EXIT_FAILURE );
    }

    if( optd || optD ) {
        if( outname == NULL )
            of = stdout;
        else {
            of = openf( outname, "w" );
            if( !of ) {
                perror( outname );
                if( outmalloced) free( outname );
                exit( EXIT_FAILURE );
            }
        }
    }
    memset( &stats, 0, sizeof( stats ) );
    memset( &instat, 0, sizeof( instat ) );
    if( inname == NULL || stat( inname, &instat ) != 0 )
        instat.st_mtime = 0;

    if( !optq ) {
        time_t now;
        FILE *pf;
        char *rpi = NULL;

        if( inname != NULL )
            rpi = get_realpath( inname );
        pf = (optd || optD)? of : stderr; 
        fprintf( pf, "Topic structure of %s\n",
                 inname? (rpi? rpi : inname) : "<stdin>" );
        if( rpi ) free( rpi );
        if( instat.st_mtime != 0 ) {
            char *mt;
            mt = Ctime( &instat.st_mtime );
            if( mt != NULL ) {
                now = time( &now );
                fprintf( pf, "File: %s ", mt );
                free( mt );
                mt = Ctime( &now );
                if( mt != NULL ) {
                    fprintf( pf, "as of %s", mt );
                    free( mt );
                }
                fprintf( pf, "\n" );
            }
        }
        fprintf( pf,
                 "---------------------------------------------------------------\n" );
    }

    if( rdhelp( &inroot, inname ) ) {
        if( outmalloced) free( outname );
        perror( inname );
        if( of && of != stdout && of != stderr ) fclose( of );
        exit( EXIT_FAILURE );
    }
    memsize = sizeof( inroot) + hlpsize( &inroot );

    if( optd || optD ) {
        printdata( of, 1, &inroot, optD, &stats );
        if( !optq ) pstats( of, outname, &stats, memsize );
        if( of && of != stdout && of != stderr ) fclose( of );
        if( outmalloced) free( outname );
        exit( EXIT_SUCCESS );
    }

    base = malloc( sizeof( hlphdr_t ) + memsize );
    if( !base ) {
        perror("malloc");
        exit( EXIT_FAILURE );
    }
    hdr = (hlphdr_t *)base;
    sp  = base + sizeof( hlphdr_t );
    pp  = sp + memsize;

    memset( hdr, 0, sizeof( *hdr ) );
    memcpy( hdr->magic, HLP_MAGIC, sizeof( HLP_MAGIC ) );
    hdr->version = HLP_VERSION;
    hdr->psize = sizeof( void * );
    hdr->ssize = sizeof( size_t );
    hdr->tsize = sizeof( time_t );
    hdr->ddate = instat.st_mtime;
    hdr->size = memsize;

    savehlp( NULL, &inroot, base, &sp, &pp, 1, &stats );

    if( outname == NULL )
        of = stdout;
    else {
        of = openf( outname, "wb" );
        if( !of ) {
            perror( outname );
            if( outmalloced) free( outname );
            exit( EXIT_FAILURE );
        }
    }
    if( !optq) pstats( stderr, outname, &stats, memsize );
    fwrite( base, 1, memsize + sizeof( hlphdr_t ), of );
    if( of && of != stdout && of != stderr ) fclose( of );
    free( base );
    if( outmalloced) free( outname );
    exit( EXIT_SUCCESS );
}

/* ***************************************************************** pstats() */
static void pstats( FILE *of, char *outname, hlpstat_t *stats, size_t memsize ) {
    size_t i;
    char *p = NULL;

    if( outname != NULL && (p = get_realpath( outname )) != NULL )
        outname = p;
    fprintf( of,
             "\n---------------------------------------------------------------\n"
             "Summary of: %s\n", outname == NULL? "<stdout>" : outname );
    if( p != NULL ) free( p );
    for( i = 0; i < 9; i++ ) {
        if( stats->topics[i] ) {
            fprintf( of,
                     "Level %u topics: %4u Key memory: %5u bytes Text memory: %5u\n",
                     (unsigned)i+1, (unsigned)stats->topics[i],
                     (unsigned)stats->keymem[i],
                     (unsigned)stats->txtmem[i] );
            stats->ntopics++;
            stats->keymemtot += stats->keymem[i];
            stats->txtmemtot += stats->txtmem[i];
        }
    }
    fprintf( of, "  Total topics: %4u Key memory: %5u bytes Text memory: %5u\n",
             (unsigned)stats->ntopics, (unsigned)stats->keymemtot,
             (unsigned)stats->txtmemtot );
    fprintf( of, "                   Index memory: %5u bytes Data memory: %5u\n",
             (unsigned)(memsize - (stats->keymemtot + stats->txtmemtot)),
             (unsigned)(stats->keymemtot + stats->txtmemtot));
    return;
}

/* **************************************************************** hlpsize() */

static size_t hlpsize( hlproot_t *root ) {
    hlptopic_t *hlp;
    size_t size = 0;

    if( root->ntopics == 0 ) {
        fprintf( stderr, "Encountered an empty root\n" );
        return size;
    }
    for( hlp = root->topics.ptr;
         hlp < (hlptopic_t *)root->topics.ptr + root->ntopics; hlp++ ) {
        size += strlen( hlp->key.ptr ) +1;
        size += strlen( hlp->text.ptr ) +1;
        if( hlp->subtopics.ntopics )
            size += hlpsize( &hlp->subtopics );
    }
    size += root->ntopics * sizeof( hlptopic_t );
    return size;
}

/*************************************************************** rdhelp() */
static
int rdhelp( hlproot_t *toproot, const char *filename ) {
    FILE *fp;
    size_t bufsize = 80;
    char *buf = NULL, *rec;
    hlproot_t *root = NULL;
    hlptopic_t *hlp = NULL,
        *tpath[9];
    unsigned int level = 0, n, lineno = 0;

    if( filename == NULL )
        fp = stdin;
    else {
        if( (fp = openf( filename, "r" )) == NULL )
            return errno;
    }
    memset( tpath, 0, sizeof( tpath ) );

    for( rec = NULL; ; ) {
        char *p, *t;
        size_t txtlen = 0, len;

        /* Read initial search key */
        if( rec == NULL ) {
            ++lineno;
            if( (rec = fgetline( fp, FALSE, &buf, &bufsize )) == NULL )
                break;  /* EOF */
        }
        if( (p = strchr( rec, '\r' )) != NULL ) {
            *p = '\0';
        }
        p = rec + 2;
        if( rec[0] == '!' ||
            !(strlen(rec) >= 3 &&
              rec[0] >= '1' && rec[0] <= '9' &&
              rec[1] == ' ' && isprint( p[ (len = strspn( p, " " )) ]) ) ) {
            rec = NULL;
            continue;  /* Comment or not a search key record */
        }
        p += len; /* Search key starts after level + 1 or more spaces */

        /* Start new topic */

        n = rec[0] - '1';
        if( n <= level ) {
            if( n )
                root = &tpath[n-1]->subtopics;
            else
                root = toproot;
        } else if( n != level +1 ) {
            free( buf );
            if( filename != NULL) fclose( fp );
            fprintf( stderr, "Level error at line %u\n", lineno );
            exit( EXIT_FAILURE );
        } else { /* hlp can't be NULL, for code analyzer */
            if( hlp ) root = &hlp->subtopics;
        }
        level = n;

        if( !root ) exit(EXIT_FAILURE); /* More code analyzer */
        if( (hlp = realloc( root->topics.ptr,
                            (root->ntopics + 1) * sizeof( hlptopic_t ) )) == NULL ) {
            free( buf );
            if( filename != NULL) fclose( fp );
            return errno;
        }
        root->topics.ptr = hlp;
        hlp += root->ntopics++;
        tpath[level] = hlp;
        len = strlen( p ) + 1;
        if( (hlp->key.ptr = malloc( len )) == NULL ) {
            free( buf );
            if( filename != NULL) fclose( fp );
            return errno;
        }
        memcpy( hlp->key.ptr, p, len );
        hlp->keylen = (uint32_t)(len -1);
        hlp->text.ptr = NULL;
        hlp->subtopics.topics.ptr = NULL;
        hlp->subtopics.ntopics = 0;

        /* Add text */

        while( (rec = fgetline( fp, TRUE, &buf, &bufsize )) != NULL ) {
            ++lineno;
            if( (p = strchr( rec, '\r' )) != NULL ) {
                *p++ = '\n';
                *p = '\0';
            }
            if( rec[0] == '!' ) { /* Comment */
                continue;
            }
            p = rec;
            if( strlen(p) >= 3 &&
                (rec[0] >= '1' && rec[0] <= '9') &&
                rec[1] == ' ' && isprint( p[ strspn( p + 2, " " ) ] ) ) {
                if( (p = strchr( p, '\n' )) )
                    *p = '\0';
                break; /* New search key, rescan this record */
            }

            len = strlen( p );
            if( len >= 2 && p[len-2] == '$' && p[len-1] == '\n' ) {
                p[len-2] = '\0';  /* Strip \n for lines ending in $ */
                --len;
            }
            ++len;
            if( hlp->text.ptr ) {
                if(txtlen != strlen(hlp->text.ptr)) printf( "len mismatch\n");
            } else if( txtlen ) printf( "Missing text\n");
            if( (t = realloc( hlp->text.ptr, txtlen + len)) == NULL ) {
                free( buf );
                if( filename != NULL) fclose( fp );
                return errno;
            }
            hlp->text.ptr = t;
            memcpy( (char *)hlp->text.ptr + txtlen, p, len );
            txtlen += --len;
            rec = NULL;
        } /* Add topic text */
        if( hlp->text.ptr == NULL ) {
            fprintf( stderr, "No text for %s at line %u\n",
                     (char *)hlp->key.ptr, lineno );
            free( rec );
            if( filename != NULL) fclose( fp );
            exit( EXIT_FAILURE );
        }
    } /* Add topic */
    free( buf );
    if( filename != NULL) fclose( fp );
    if( toproot->ntopics == 0 ) {
        fprintf( stderr, "No topics found at line %u\n", lineno );
        exit( EXIT_FAILURE );
    }
    helpsort( toproot );

    return 0;
}

/*************************************************************** helpsort() */

static void helpsort( hlproot_t *root ) {
    hlptopic_t *hlp;

    if( root->topics.ptr == NULL ) {
        fprintf( stderr, "Node has no topics\n" );
        exit( EXIT_FAILURE );
    }

    for( hlp = root->topics.ptr;
         hlp < (hlptopic_t *)root->topics.ptr + root->ntopics; hlp++ ) {
        if( hlp->text.ptr == NULL ) {
            fprintf( stderr, "No text for %s\n", (char *)hlp->key.ptr );
            exit( EXIT_FAILURE );
        }
        if( hlp->subtopics.topics.ptr )
            helpsort( &hlp->subtopics );
    }

    qsort( root->topics.ptr, root->ntopics, sizeof( hlptopic_t ), &hlpcmp );

    return;
}

/***************************************************************** hlpcmp() */

static int hlpcmp( const void *ta, const void *tb ) {
    const char *a, *b;
    size_t alen, blen;

    a = ((hlptopic_t *)ta)->key.ptr;
    b = ((hlptopic_t *)tb)->key.ptr;
    alen = ((hlptopic_t *)ta)->keylen;
    blen = ((hlptopic_t *)tb)->keylen;

    while( alen && blen ) {
        int c;

        c = tolower( (unsigned char)*a ) - tolower( (unsigned char)*b );
        if( c != 0 )
            return c;
        ++a; ++b; --alen; --blen;
    }

    return (int)(alen - blen);
}

/* **************************************************************** savehlp() */

static void savehlp( hlproot_t *oroot, hlproot_t *iroot,
                     char *base,char **sp, char **pp,
                     unsigned lvl, hlpstat_t *stats ) {
    size_t t;
    hlptopic_t *ihlp, *ohlp;
    hlproot_t  *rp;

    /* sp allocates structures toward increasing addresses.
     * pp allocates strings toward decreasing addresses.
     * This ensures that alignment is accounted for.
     * All pointers in the input tree are offsets in the output.
     */
    if( oroot == NULL ) { /* Top root */
        rp = (hlproot_t *)*sp;
        *sp = (char *)(rp + 1);
        memset( rp, 0, sizeof( *rp ) );
    } else
        rp = oroot;       /* Subtopic root */

    ohlp = (hlptopic_t *)*sp;  /* Topic list for this root */
    rp->topics.ofs = (char *)ohlp - base;
    t =
        rp->ntopics = iroot->ntopics;
    t *= sizeof( hlptopic_t );
    *sp += t;
    memset( ohlp, 0, t );

    for( ihlp = iroot->topics.ptr, t = 0; t < iroot->ntopics; ihlp++, t++ ) {
        size_t len;

        stats->topics[lvl-1]++;

        memset( ohlp, 0, sizeof( *ohlp ) );
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning (disable: 6001)
#endif
        /* N.B. Pointers ARE initialized */
        len = strlen( ihlp->key.ptr ) +1;
        stats->keymem[lvl-1] += len;
        *pp = (char *)*pp - len;
        memcpy( *pp, ihlp->key.ptr, len );
        free( (void *)ihlp->key.ptr );
        ohlp->key.ofs = *pp - base;
        ohlp->keylen = (uint32_t)len -1;

        len = strlen( ihlp->text.ptr ) +1;
        stats->txtmem[lvl-1] += len;
        *pp = (char *)*pp - len;
        memcpy( *pp, ihlp->text.ptr, len );
        free( (void *)ihlp->text.ptr );
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        ohlp->text.ofs = *pp - base;

        ohlp->subtopics.ntopics = ihlp->subtopics.ntopics;
        if( ihlp->subtopics.ntopics ) {
            ohlp->subtopics.topics.ofs = *sp - base;
            savehlp( &ohlp->subtopics, &ihlp->subtopics, base, sp, pp,
                     lvl+1, stats );
        } else {
            ohlp->subtopics.topics.ofs = 0;
        }
        ++ohlp;
    }
    free( (void *)iroot->topics.ptr );
    return;
}

/*************************************************************** printdata() */

static int printdata( FILE *of, unsigned level, hlproot_t *root,
                      int text, hlpstat_t *stats ) {
    int sts;
    hlptopic_t *hlp;

    if( root->topics.ptr == NULL )
        return 1;

    for( hlp = root->topics.ptr;
         hlp < (hlptopic_t *)root->topics.ptr + root->ntopics; hlp++ ) {
        stats->topics[level-1]++;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning (disable: 6001)
#endif
        /* N.B. Pointers ARE initialized */
        fprintf( of, "%*s %u %s\n", level, "", level, (char *)hlp->key.ptr );
        free( hlp->key.ptr );
        if( hlp->text.ptr == NULL )
            return 0;
        stats->keymem[level-1] += (size_t)hlp->keylen+1;
        stats->txtmem[level-1] += strlen( (char *)hlp->text.ptr ) +1;
        if( text )
            printf( "%s\n", (char *)hlp->text.ptr );
        free( hlp->text.ptr );
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        if( hlp->subtopics.topics.ptr != NULL &&
            !(sts = printdata( of, level +1, &hlp->subtopics, text, stats )) )
            return sts;
    }
    free( root->topics.ptr );
    root->topics.ptr = NULL;
    root->ntopics = 0;
    return 1;
}
