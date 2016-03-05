#define MODULE_NAME  ODS2

/*     Feb-2016, v1.4 add readline support, dir/full etc
 *     See git commit messages for details.
 *     V2.1 - Merge and adapte code from Larry Baker's V2.0
 */

/*     Jul-2003, v1.3hb, some extensions by Hartmut Becker */

/*     ODS2.c V2.1   Mainline ODS2 program   */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.

        The modules in ODS2 are:-

                ACCESS.C        Routines for accessing ODS2 disks
                CACHE.C         Routines for managing memory cache
                COMPAT.C        Routines for OS compatibility
                DEVICE.C        Routines to maintain device information
                DIRECT.C        Routines for handling directories
                ODS2.C          The mainline program
                PHYVMS.C        Routine to perform physical I/O
                PHYVIRT.C       Routines for managing virtual disks.
                RMS.C           Routines to handle RMS structures
                SYSMSG.C        Routines to convert status codes to text
                UPDATE.C        Routines for updating ODS2 structures
                VMSTIME.C       Routines to handle VMS times

        On non-VMS platforms PHYVMS.C should be replaced as follows:-

                Unix            PHYUNIX.C
                OS/2            PHYOS2.C
                Windows 95/NT   PHYNT.C

        For example under OS/2 the program is compiled using the GCC
        compiler with the single command:-

                gcc -fdollars-in-identifiers ods2.c,rms.c,direct.c,
                      access.c,device.c,cache.c,phyos2.c,vmstime.c

       For accessing disk images (e.g. .ISO or simulator files),
       simply mount /image (or /virtual) filename.

       The included make/mms/com files do all this for you.
*/

/* Modified by:
 *   Feb 2016 Timothe Litt <litt at acm _ddot_ org>
 *      Bug fixes, readline support, build on NT without wnaspi32,
 *      Decode error messages, patch from vms2linux.de. VS project files.
 *      Rework command parsing and help.  Bugs, bugs & bugs.  Merge
 *      code from Larry Baker, USGS.  See git
 *      commit history for details.
 *
 *   31-AUG-2001 01:04	Hunter Goatley <goathunter@goatley.com>
 *
 *	For VMS, added routine getcmd() to read commands with full
 *	command recall capabilities.
 *
 *    8-JUN-2005		Larry Baker <baker@usgs.gov>
 *
 *  Add #include guards in .h files.
 *  Use named constants in place of literals.
 *  Replace BIG_ENDIAN with ODS2_BIG_ENDIAN (Linux always #defines
 *     BIG_ENDIAN).
 *  Add SUBST DRIVE: FILE to "mount" a file (vs. a disk).
 *  Implement quoted arguments (paired " or '; no escape characters)
 *     in cmdsplit(), e.g., to specify a Unix path or a null argument.
 *  Remove VMSIO conditional code (need to "mount" a file now).
 */

/*  This is the top level set of routines. It is fairly
    simple minded asking the user for a command, doing some
    primitive command parsing, and then calling a set of routines
    to perform whatever function is required (for example COPY).
    Some routines are implemented in different ways to test the
    underlying routines - for example TYPE is implemented without
    a NAM block meaning that it cannot support wildcards...
    (sorry! - could be easily fixed though!)
*/

#ifdef VMS
#ifdef __DECC
#pragma module MODULE_NAME MODULE_IDENT
#else
#ifdef VAXC
#module MODULE_NAME MODULE_IDENT
#endif /* VAXC */
#endif /* __DECC */
#include <smgdef.h>		/* For VMS version of getcmd()                */
#include <smgmsg.h>             /* SMG$_EOF */
#include <smg$routines.h>
#endif /* VMS */

#define DEBUGx on

#define _BSD_SOURCE
#include <ctype.h>		/* isalpha(), isspace(), tolower()            */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "version.h"
#include "compat.h"
#include "sysmsg.h"
#include "phyio.h"
#include "phyvirt.h"

#ifdef _WIN32
#include <Windows.h>
#endif
#include "access.h"
#include "descrip.h"
#include "device.h"
#include "direct.h"
#include "ods2.h"
#include "phyio.h"
#include "rms.h"
#include "ssdef.h"
#include "stsdef.h"

#ifdef VMS
#include <starlet.h>

#else
#include "vmstime.h"
#endif

#ifdef VAXC
#ifdef EXIT_SUCCESS
#undef EXIT_SUCCESS
#endif
#define EXIT_SUCCESS SS$_NORMAL
#endif

#ifndef TRUE
#define TRUE ( 0 == 0 )
#endif
#ifndef FALSE
#define FALSE ( 0 != 0 )
#endif

#define MAXREC 32767
#define PRINT_ATTR ( FAB$M_CR | FAB$M_PRN | FAB$M_FTN )

#ifdef USE_READLINE
#define _XOPEN_SOURCE
#include <wordexp.h>
#include <readline/readline.h>
#include <readline/history.h>
#endif

/* Read a line of input - unlimited length
 * Removes \n, returns NULL at EOF
 * Caller responsible for free()
 */
static char *fgetline( FILE *stream, int keepnl ) {
    size_t bufsize = 0,
           xpnsize = 80,
           idx = 0;
    char *buf = NULL;
    int c;

    while( (c = fgetc(stream)) != EOF && c != '\n' ) {
        if( idx + (keepnl != 0) +1 > bufsize ) { /* Allow space for char + (optional \n) + \0 */
            char *nbuf;
            bufsize += xpnsize;
            nbuf = (char *) realloc( buf, bufsize );
            if( nbuf == NULL ) {
                perror( "realloc" );
                abort();
            }
            buf = nbuf;
        }
        buf[idx++] = c;
    }
    if( c == '\n' ) {
        if( keepnl )
            buf[idx++] = '\n';
    } else {
        if( c == EOF && idx == 0 ) {
            free( buf );
            return NULL;
        }
    }
    if( bufsize == 0 ) {
        buf = (char *) malloc( 1 );
        if( buf == NULL ) {
            perror( "malloc" );
            abort();
        }
    }
    buf[idx] = '\0';
    return buf;
}

/* keycomp: routine to compare parameter to a keyword - case insensitive! */

static int keycomp(const char *param, const char *keywrd) {
    while (*param != '\0') {
        if (tolower(*param++) != *keywrd++) return 0;
    }
    return 1;
}

/* set, clear: bits specified are cleared, then set in value
 * helpstr: if null, switch not listed in help.  If starts with -,
 *          listed as negatable in help
 */
#define NV NOVAL, NULL
#define KV(list) KEYVAL, list
struct qual {
    const char *name;
    int set;
    int clear;
    enum qualtype { NOVAL, KEYVAL } qtype;
    void *arg;
    const char *helpstr;
};

struct param;
struct CMDSET;

typedef const char *(hlpfunc_t)( struct CMDSET *cmd, struct param *p, int argc, char **argv );

struct param {
    const char *name;
    enum parflags { REQ, OPT, CND } flags;
    enum partype { VMSFS, LCLFS, LIST, KEYWD, STRING, CMDNAM, NONE } ptype;
#define PA(arg) NULL, (arg)
#define NOPA PA(NULL)
    hlpfunc_t *hlpfnc;
    void *arg;
    const char *helpstr;
};

/* Command table entry */

struct CMDSET {
    char *name;
    unsigned (*proc) (int argc,char *argv[],int qualc,char *qualv[]);
    unsigned uniq;
    struct qual *validquals;
    struct param *params;
    char *helpstr;
};

static void qualhelp( int par, struct qual *qtable );

/* Qualifier style = vms = /qualifier; else -option
 */

static int vms_qual = 1;
static int verify_cmd = 1;

/* checkquals: Given valid qualifier definitions, process qualifer
 * list from a command left to right.  Also handles parameters and
 * qualifier values (/Qual=value).
 */

static int checkquals(int result, struct qual qualset[],int qualc,char *qualv[]) {
    int i;
    const char *type;

    type = (qualc < 0)? "parameter": "qualifier";
    qualc = abs( qualc );

    for( i = 0; i < qualc; i++ ) {
        char *qv;
        struct qual *qs, *qp = NULL;

        qv = strchr( qualv[i], '=' );
        if( qv == NULL )
            qv = strchr( qualv[i], ':' );
        if( qv != NULL )
            *qv++ = '\0';
        for( qs = qualset; qs->name != NULL; qs++) {
            if (keycomp(qualv[i],qs->name)) {
                if( qp != NULL ) {
                    printf ( "%%ODS2-W-AMBQUAL, %c%s '%s' is ambiguous\n",
                             type[0], type+1, qualv[i] );
                    return -1;
                }
                qp = qs;
            }
        }
        if (qp == NULL) {
            printf("%%ODS2-W-ILLQUAL, Unknown %s '%s'\n", type, qualv[i]);
            return -1;
        }
        result = (result & ~qp->clear) | qp->set;
        if( qv != NULL ) {
            char *nvp;

            if( qp->qtype == NOVAL ) {
                printf( "%%ODS2-W-NOQVAL, %c%s '%s' does not accept a value\n",
                        toupper( *type ), type+1, qualv[i] );
                return -1;
            }
            if( *qv == '(' ) {
                qv++;
                nvp = strchr( qv, ')' );
                if( nvp == NULL ) {
                    printf( "%%ODS2-W-NQP, %c%s %s is missing ')'\n",
                            toupper( *type ), type+1, qualv[i] );
                    return -1;
                }
                *nvp = '\0';
            }
            do {
                while( *qv == ' ' ) qv++;
                nvp = strchr( qv, ',' );
                if( nvp != NULL )
                    *nvp++ = '\0';
                switch( qp->qtype ) {
                case KEYVAL:
                    result = checkquals( result, (struct qual *)qp->arg, -1, &qv );
                    if( result == -1 )
                        return result;
                    break;
                default:
                    abort();
                }
                qv = nvp;
            } while( qv != NULL );
        }
    }
    return result;
}

static int prvmstime(VMSTIME vtime, const char *sfx) {
    int sts = 0;
    char tim[24];
    static const VMSTIME nil;
    struct dsc_descriptor timdsc;

    if( memcmp( vtime, nil, sizeof(nil) ) ) {
        timdsc.dsc_w_length = 23;
        timdsc.dsc_a_pointer = tim;
        sts = sys_asctim(0,&timdsc,vtime,0);
        if ((sts & 1) == 0) printf("%%ODS2-W-TIMERR, SYS$ASCTIM error: %s\n",getmsg(sts, MSG_TEXT));
        tim[23] = '\0';
        printf("  %s",tim);
    } else {
        printf( "  %-23s", "     <Not recorded>" );
        sts = 1;
    }
    if (sfx != NULL)
        printf( "%s", sfx );
    return sts;
}

static void pwrap( int *pos, const char *fmt, ... ) {
    char pbuf[200], *p, *q;
    va_list ap;
    va_start(ap, fmt );
    vsnprintf( pbuf, sizeof(pbuf), fmt, ap );
    va_end(ap);
    p = pbuf;
    while( *p ) {
        int len;
        int eol = 0;

        q = strchr( p, '\n' );
        if( q != NULL ) {
            *q++ = '\0';
            eol = 1;
            len = strlen(p);
        } else {
            len = strlen(p);
            q = p + len;
        }
        if( *pos + len > 80 ) {
            static const char wrap[] = "                    ";
            printf( "\n%s", wrap );
            *pos = sizeof(wrap) -1;
            if( p[0] == ',' && p[1] == ' ' )
                p += 2;
        }
        *pos += strlen(p);
        printf( "%s%s", p, eol? "\n":"" );
        if( eol ) *pos = 0;
        p = q;
    }
}


#define dir_extra (dir_date | dir_fileid | dir_owner | dir_prot | dir_size)
#define  dir_date        (1 <<  0)
#define  dir_fileid      (1 <<  1)
#define  dir_owner       (1 <<  2)
#define  dir_prot        (1 <<  3)
#define  dir_size        (1 <<  4)

#define dir_grand        (1 <<  5)
#define dir_heading      (1 <<  6)
#define dir_names        (1 <<  7)
#define dir_trailing     (1 <<  8)
#define dir_full         (1 <<  9)
#define dir_total        (1 << 10)

#define dir_backup       (1 << 11)
#define dir_created      (1 << 12)
#define dir_expired      (1 << 13)
#define dir_modified     (1 << 14)
#define dir_dates        (dir_backup | dir_created | dir_expired | dir_modified)

#define  dir_allocated   (1 << 15)
#define  dir_used        (1 << 16)
#define dir_sizes        (dir_allocated | dir_used)

#define dir_default (dir_heading|dir_names|dir_trailing)

static struct qual datekwds[] = { {"created",  dir_created,  0, NV, "Date file created (default)"},
                                  {"modified", dir_modified, 0, NV, "Date file modified"},
                                  {"expired",  dir_expired,  0, NV, "Date file expired"},
                                  {"backup",   dir_backup,   0, NV, "Date of last backup"},
                                  {NULL,       0,            0, NV, NULL}
};
static struct qual sizekwds[] = { {"both",       dir_used|dir_allocated, 0, NV, "Both used and allocated" },
                                  {"allocation", dir_allocated,          0, NV, "Blocks allocated to file" },
                                  {"used",       dir_used,               0, NV, "Blocks used in file" },
                                  {NULL,         0,                      0, NV, NULL}
};
static struct qual dirquals[] = { {"brief",         dir_default,   ~dir_default,    NV,
                                   "Brief display - names with header/trailer (default)"},
                                  {"date",          dir_date,       dir_dates,      KV(datekwds),
                                   "-Include file date(s)", },
                                  {"nodate",        0,              dir_date,       NV, NULL, },
                                  {"file_id",       dir_fileid,     0,              NV, "-Include file ID", },
                                  {"nofile_id",     0,              dir_fileid,     NV, NULL },
                                  {"full",          dir_full|dir_heading|dir_trailing,
                                                            ~dir_full,       NV, "Include full details", },
                                  {"grand_total",   dir_grand,     ~dir_grand & ~(dir_size|dir_sizes),
                                                                             NV, "-Include only grand total",},
                                  {"nogrand_total", 0,              dir_grand,      NV, NULL},
                                  {"heading",       dir_heading,    0,              NV, "-Include heading", },
                                  {"noheading",     0,              dir_heading,    NV, NULL},
                                  {"owner",         dir_owner,      0,              NV, "-Include file owner", },
                                  {"noowner",       0,              dir_owner,      NV, NULL, },
                                  {"protection",    dir_prot,       0,              NV,
                                   "-Include file protection", },
                                  {"noprotection",  0,              dir_prot,       NV, NULL, },
                                  {"size",          dir_size,       dir_sizes,      KV(sizekwds),
                                   "-Include file size (blocks)", },
                                  {"nosize",        0,              dir_size|dir_sizes,
                                                                                    NV, NULL, },
                                  {"total",         dir_total|dir_heading,
                                                                   ~dir_total & ~(dir_size|dir_sizes),
                                                                                    NV,
                                   "Include only directory name and summary",},
                                  {"trailing",      dir_trailing,   0,              NV,
                                   "-Include trailing summary line",},
                                  {"notrailing",    0,              dir_trailing,   NV, NULL},
                                  {NULL,            0,              0,              NV, NULL} };
static int dir_defopt = dir_default;

static struct param dirpars[] = { {"filespec", OPT, VMSFS, NOPA, "for files to select. Wildcards are allowed."},
                                  { NULL,      0,   0,     NOPA, NULL }
};

static void dirtotal( int options, int size, int alloc ) {
    if ( !(options & dir_size) )
        return;
    fputs( ", ", stdout );

    if ( options & dir_used )
        printf( "%d", size );
    if ( options & dir_allocated ) {
        if (options & dir_used) printf( "/" );
        printf( "%d", alloc );
    }
    if ((options & dir_dates) == dir_dates)
        printf( " block%s",(size ==1 &&  alloc == 1 ? "" : "s"));
    else
        printf( " block%s",(((options & dir_used) && size == 1) ||
                            ((options & dir_allocated) && alloc == 1))? "" : "s");
    return;
}

static unsigned dodir(int argc,char *argv[],int qualc,char *qualv[]) {
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    int options;
    unsigned sts;
    int filecount = 0, nobackup = 0, contigb = 0, contig = 0, directory = 0;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    struct XABDAT dat = cc$rms_xabdat;
    struct XABFHC fhc = cc$rms_xabfhc;
    struct XABPRO pro = cc$rms_xabpro;
    struct XABITM itm = cc$rms_xabitm;
    struct item_list xitems[] = {
        { XAB$_UCHAR_NOBACKUP,    sizeof(int), NULL, 0 },
        { XAB$_UCHAR_CONTIG,      sizeof(int), NULL, 0 },
        { XAB$_UCHAR_CONTIGB,     sizeof(int), NULL, 0 },
        { XAB$_UCHAR_DIRECTORY,   sizeof(int), NULL, 0 },
        { 0, 0, NULL, 0 }
    };

    UNUSED(argc);

    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;

    fab.fab$l_nam = &nam;
    fab.fab$l_xab = &dat;

    dat.xab$l_nxt = &fhc;

    fhc.xab$l_nxt = &pro;

    pro.xab$l_nxt = &itm;
    xitems[0].buffer = &nobackup;
    xitems[1].buffer = &contig;
    xitems[2].buffer = &contigb;
    xitems[3].buffer = &directory;

    itm.xab$b_mode = XAB$K_SENSEMODE;
    itm.xab$l_itemlist = xitems;

    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    fab.fab$l_dna = "*.*;*";
    fab.fab$b_dns = strlen(fab.fab$l_dna);

    options = checkquals(dir_defopt,dirquals,qualc,qualv);
    if( options == -1 )
        return SS$_BADPARAM;

    if (options & dir_full)
        options |= dir_extra;
    if (options & (dir_total | dir_grand)) {
        options |= dir_trailing;
        if (options & (dir_extra & ~dir_size))
            options |= dir_names;
    } else {
        if (options & dir_extra)
            options |= dir_names;
    }
    if( (options & dir_size) && !(options & dir_sizes) )
        options |= dir_used;
    if( (options & dir_date) && !(options & dir_dates) )
        options |= dir_created;

    sts = sys_parse(&fab);
    if ( sts & STS$M_SUCCESS ) {
        char dir[NAM$C_MAXRSS + 1];
        int namelen;
        int dirlen = 0;
        int dirfiles = 0, dircount = 0;
        int dirblocks = 0, diralloc = 0, totblocks = 0, totalloc = 0;
        int printcol = 0;
#ifdef DEBUG
        res[nam.nam$b_esl] = '\0';
        printf("Parse: %s\n",res);
#endif
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
        while ( ( sts = sys_search( &fab ) ) & STS$M_SUCCESS ) {

            if (dirlen != nam.nam$b_dev + nam.nam$b_dir ||
                memcmp(rsa, dir, nam.nam$b_dev + nam.nam$b_dir) != 0) {
                if (dirfiles > 0 && (options & dir_trailing)) {
                    if (printcol > 0) printf("\n");
                    printf("\nTotal of %d file%s",dirfiles,(dirfiles == 1 ? "" : "s"));
                    dirtotal( options, dirblocks, diralloc );
                    fputs(".\n",stdout);
                }
                dirlen = nam.nam$b_dev + nam.nam$b_dir;
                memcpy(dir,rsa,dirlen);
                dir[dirlen] = '\0';
                if( options & dir_heading) printf("\nDirectory %s\n\n",dir);
                filecount += dirfiles;
                totblocks += dirblocks;
                totalloc += diralloc;
                dircount++;
                dirfiles = 0;
                dirblocks = 0;
                diralloc = 0;
                printcol = 0;
            }
            rsa[nam.nam$b_rsl] = '\0';
            namelen = nam.nam$b_name + nam.nam$b_type + nam.nam$b_ver;
            if ((options & (dir_heading|dir_extra|dir_full)) == dir_heading) {
                if( options & dir_names ) {
                    if (printcol > 0) {
                        int newcol = (printcol + 20) / 20 * 20;
                        if (newcol + namelen >= 80) {
                            fputs("\n",stdout);
                            printcol = 0;
                        } else {
                            printf("%*s",newcol - printcol," ");
                            printcol = newcol;
                        }
                    }
                    fputs(rsa + dirlen,stdout);
                    printcol += namelen;
                }
            } else {
                if (options & dir_names) {
                    if ( (options & dir_heading) == 0 ) printf( "%s",dir );
                    if (namelen > 18) {
                        printf("%s",rsa + dirlen);
                        if( options & dir_extra )
                            printf( "\n                   " );
                    } else {
                        printf("%-19s",rsa + dirlen);
                    }
                }
                sts = sys_open(&fab);
                if ( !( sts & STS$M_SUCCESS ) ) {
                    printf("%%ODS2-E-OPENERR, Open error: %s\n", getmsg(sts, MSG_TEXT));
                } else {
                    sts = sys_close(&fab);
                    if (options & dir_fileid) {
                        char fileid[30];  /* 24 bits, 16 bits. 8 bits = 8 + 5 + 3 digits = 16 + (,,)\0 => 21 */
                        if( options & dir_full) printf( "         File ID:" );
                        snprintf(fileid,sizeof(fileid),"(%d,%d,%d)",
                                (nam.nam$b_fid_nmx << 16) | nam.nam$w_fid_num,
                                nam.nam$w_fid_seq,nam.nam$b_fid_rvn);
                        printf("  %-22s",fileid);
                    }
                    diralloc += fab.fab$l_alq;
                    if (options & dir_used) {
                        unsigned filesize = fhc.xab$l_ebk;
                        if (fhc.xab$w_ffb == 0) filesize--;
                        dirblocks += filesize;

                        if (options & dir_names) { /* Don't print w/o names (e.g. totals) */
                            if( options & dir_full) printf( "\nSize: " );
                            printf("%9d",filesize);
                            if( options & (dir_allocated|dir_full)) printf( "/%-9d   ",fab.fab$l_alq );
                        }
                    } else {
                        if ( (options & (dir_allocated|dir_names)) == (dir_allocated|dir_names)) {
                            printf( "%9d", fab.fab$l_alq );
                        }
                    }
#define pprot(val,pos,del) {\
    unsigned int v = ~(((val) >> (pos)) & xab$m_prot); \
    if( v & xab$m_noread )  printf( "R" ); \
    if( v & xab$m_nowrite ) printf( "W" ); \
    if( v & xab$m_noexe )   printf( "E" ); \
    if( v & xab$m_nodel )   printf( "D" ); \
    printf( del );                         \
                        }
                    if (options & dir_full) {
                        int pos = 0;

                        printf( "Owner:    [%o,%o]\n", ((pro.xab$l_uic>>16)&0xFFFF), pro.xab$l_uic&0xFFFF);
                        printf( "Created:  " ); prvmstime( dat.xab$q_cdt, "\n" );
                        printf( "Revised:  " ); prvmstime( dat.xab$q_rdt, " (" ); printf( "%u)\n", dat.xab$w_rvn );
                        printf( "Expires:  " ); prvmstime( dat.xab$q_edt, "\n" );
                        printf( "Backup:   " ); prvmstime( dat.xab$q_bdt, "\n" );
                        pwrap(  &pos, "File organization:  " );
                        switch( fab.fab$b_org ) {
                        case FAB$C_SEQ:
                            pwrap(  &pos, "Sequential" ); break;
                        case FAB$C_REL:
                            pwrap(  &pos, "Relative" /*, Maximum record number %u", fab.fab$l_mrn*/ ); break;
                        case FAB$C_IDX:
                            pwrap(  &pos, "Indexed" ); break; /*, Prolog: 3, Using 4 keys\nIn 3 areas */
                        default:
                            pwrap(  &pos, "Unknown (%u)", fab.fab$b_org ); break;
                        }
                        /* File attributes:    Allocation: 372, Extend: 3, Maximum bucket size: 3, Global buffer count: 0, No version limit
                                               Contiguous best try */
                        pwrap(  &pos, "\nFile attributes:    " );
                        pwrap(  &pos, "Allocation: %u", fab.fab$l_alq );
                        pwrap(  &pos, ", Extend: %u", fab.fab$w_deq );
                        /* Missing: , Maximum bucket size: n*/
                        pwrap(  &pos, ", Global buffer count: %u", fab.fab$w_gbc );
                        if( fhc.xab$w_verlimit == 0 || fhc.xab$w_verlimit == 32767 )
                            pwrap(  &pos, ", No %sversion limit", directory? "default ": "" );
                        else
                            pwrap(  &pos, ", %sersion limit: %u", (directory? "Default v": "V"), fhc.xab$w_verlimit );
                        if( contig )
                            pwrap(  &pos, ", Contiguous" );
                        if( contigb )
                            pwrap(  &pos, ", Contiguous best try" );
                        if( nobackup )
                            pwrap( &pos, ", Backups disabled" );
                        if( directory )
                            pwrap( &pos, ", Directory file" );
                        pwrap(  &pos, "\n" );

                        pwrap(  &pos, "Record format:      " );
                        switch( fab.fab$b_rfm ) {
                        default:
                        case FAB$C_UDF:
                            pwrap(  &pos, "Undefined" ); break;
                        case FAB$C_FIX:
                            pwrap(  &pos, "Fixed length %u byte records", fab.fab$w_mrs ); break;
                        case FAB$C_VAR:
                            pwrap(  &pos, "Variable length, maximum %u bytes", fab.fab$w_mrs ); break;
                        case FAB$C_VFC:
                            pwrap(  &pos, "Variable length, fixed carriage control %u, maximum %u bytes", (fab.fab$b_fsz? fab.fab$b_fsz: 2), fab.fab$w_mrs ); break;
                        case FAB$C_STM:
                            pwrap(  &pos, "Stream" ); break;
                        case FAB$C_STMLF:
                            pwrap(  &pos, "Stream-LF" ); break;
                        case FAB$C_STMCR:
                            pwrap(  &pos, "Stream-CR" ); break;
                        }
                        pwrap(  &pos, "\n" );

                        pwrap(  &pos, "Record attributes:  " );
                        if( fab.fab$b_rat == 0 )
                            pwrap(  &pos, "None" );
                        else {
                            const char *more = "";
                            if( fab.fab$b_rat & FAB$M_FTN ) {
                                pwrap(  &pos, "%sFortran carriage control", more );
                                more = ", ";
                            }
                            if( fab.fab$b_rat & FAB$M_CR ) {
                                pwrap(  &pos, "%sCarriage return carriage control", more );
                                more = ", ";
                            }
                            if( fab.fab$b_rat & FAB$M_PRN ) {
                                pwrap(  &pos, "%sPrinter control", more );
                                more = ", ";
                            }
                            if( fab.fab$b_rat & FAB$M_BLK ) {
                                pwrap(  &pos, "%sNon-spanned", more );
                            }
                        }
                        printf( "\n" );
                        /*
RMS attributes:     None
Journaling enabled: None
*/
                        printf( "File protection:    System:" );
                        pprot(pro.xab$w_pro,xab$v_system,", Owner:")
                        pprot(pro.xab$w_pro,xab$v_owner,", Group:")
                        pprot(pro.xab$w_pro,xab$v_group,", World:")
                        pprot(pro.xab$w_pro,xab$v_world,"\n");
                    } else { /* !full */
                        if (options & dir_date) {
                            if( options & dir_created )
                                sts = prvmstime( dat.xab$q_cdt, NULL );
                            if( options & dir_modified )
                                sts = prvmstime( dat.xab$q_rdt, NULL );
                            if( options & dir_expired )
                                sts = prvmstime( dat.xab$q_edt, NULL );
                            if( options & dir_backup )
                                sts = prvmstime( dat.xab$q_bdt, NULL );
                        }
                        if (options & dir_owner) {
                            printf("  [%o,%o]", ((pro.xab$l_uic>>16)&0xFFFF), pro.xab$l_uic&0xFFFF);
                        }
                        if (options & dir_prot) {
                            printf( "  (" );
                        pprot(pro.xab$w_pro,xab$v_system,",")
                        pprot(pro.xab$w_pro,xab$v_owner,",")
                        pprot(pro.xab$w_pro,xab$v_group,",")
                        pprot(pro.xab$w_pro,xab$v_world,")")
                        }
                    }
#undef pprot
                    if (options & dir_names)
                        printf("\n");
                }
            }
            dirfiles++;
        }
        if (sts == RMS$_NMF) sts = SS$_NORMAL;
        if (printcol > 0) printf("\n");
        if (options & dir_trailing) {
            printf("\nTotal of %d file%s",dirfiles,(dirfiles == 1 ? "" : "s"));
            dirtotal( options, dirblocks, diralloc );
           fputs(".\n",stdout);
        }
        filecount += dirfiles;
        totblocks += dirblocks;
        totalloc += diralloc;
        if (options & dir_grand) {
            printf("\nGrand total of %d director%s, %d file%s",
                   dircount,(dircount == 1 ? "y" : "ies"),
                   filecount,(filecount == 1 ? "" : "s"));
            dirtotal( options, totblocks, totalloc );
            fputs(".\n",stdout);
        }
    }
    if ( sts & STS$M_SUCCESS ) {
        if (filecount < 1) printf("%%DIRECT-W-NOFILES, no files found\n");
    } else {
        printf("%%DIR-E-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}

/******************************************************************* docopy() */

/* copy: a file copy routine */

#define COP_BINARY 1

static struct qual copyquals[] = { {"ascii",  0,          COP_BINARY, NV, "Copy file in ascii mode (default)"},
                                   {"binary", COP_BINARY, 0,          NV, "Copy file in binary mode", },
                                   {NULL,     0,          0,          NV, NULL}
};

static struct param copypars[] = { {"from_filespec", REQ, VMSFS, NOPA,
                                    "for source file. Wildcards are allowed."},
                                   {"to_filespec",   REQ, LCLFS, NOPA,
                                    "for destination file. Wildcards are replaced from source file name."},
                                   { NULL, 0, 0, NOPA, NULL }
};

static unsigned docopy(int argc,char *argv[],int qualc,char *qualv[]) {
    int sts,options;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    int filecount = 0;

    UNUSED(argc);

    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    options = checkquals(0,copyquals,qualc,qualv);
    if( options == -1 )
        return SS$_BADPARAM;
    sts = sys_parse(&fab);
    if ( sts & STS$M_SUCCESS ) {
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
        while ( ( sts = sys_search( &fab ) ) & STS$M_SUCCESS ) {
            sts = sys_open(&fab);
            if ( !( sts & STS$M_SUCCESS ) ) {
                printf("%%COPY-F-OPENFAIL, Open error: %s\n",getmsg(sts, MSG_TEXT));
                perror("-COPY-F-ERR ");
            } else {
                struct RAB rab = cc$rms_rab;
                rab.rab$l_fab = &fab;
                if ((sts = sys_connect(&rab)) & STS_M_SUCCESS) {
                    FILE *tof;
                    char name[NAM$C_MAXRSS + 1];
                    unsigned records = 0;
                    {
                        char *out = name,*inp = argv[2]; /* unquote(argv[2]) */
                        int dot = FALSE;
                        while (*inp != '\0') {
                            if (*inp == '*') {
                                inp++;
                                if (dot) {
                                    memcpy(out,nam.nam$l_type + 1,
                                           nam.nam$b_type - 1);
                                    out += nam.nam$b_type - 1;
                                } else {
                                    unsigned length = nam.nam$b_name;
                                    if (*inp == '\0') length += nam.nam$b_type;
                                    memcpy(out,nam.nam$l_name,length);
                                    out += length;
                                }
                            } else {
                                if (*inp == '.') {
                                    dot = TRUE;
                                } else {
                                    if (strchr(":]\\/",*inp)) dot = FALSE;
                                }
                                *out++ = *inp++;
                            }
                        }
                        *out++ = '\0';
                    }
#ifndef _WIN32
                    tof = openf(name,"w");
#else
		    if ((options & 1) == 0 && fab.fab$b_rat & PRINT_ATTR) {
                        tof = openf(name,"w");
		    } else {
		        tof = openf(name,"wb");
		    }
#endif
                    if (tof == NULL) {
                        printf("%%COPY-F-OPENOUT, Could not open %s\n",name);
                        perror("-COPY-F-ERR ");
                    } else {
                        char rec[MAXREC + 2];
                        filecount++;
                        rab.rab$l_ubf = rec;
                        rab.rab$w_usz = MAXREC;
                        while ( ( sts = sys_get( &rab ) ) & STS$M_SUCCESS ) {
                            unsigned rsz = rab.rab$w_rsz;
                            if ((options & 1) == 0 &&
                                 fab.fab$b_rat & PRINT_ATTR) rec[rsz++] = '\n';
                            if (fwrite(rec,rsz,1,tof) == 1) {
                                records++;
                            } else {
                                printf("%%COPY-F- fwrite error!!\n");
                                perror("-COPY-F-ERR ");
                                break;
                            }
                        }
                        if (fclose(tof)) {
                            printf("%%COPY-F- fclose error!!\n");
                            perror("-COPY-F-ERR ");
                        }
                    }
                    sys_disconnect(&rab);
                    rsa[nam.nam$b_rsl] = '\0';
                    if (sts == RMS$_EOF) {
                        printf(
                            "%%COPY-S-COPIED, %s copied to %s (%d record%s)\n",
                            rsa,name,records,(records == 1 ? "" : "s")
                        );
                    } else {
                        printf("%%COPY-F-ERROR Status: %s for %s\n",getmsg(sts, MSG_TEXT),rsa);
                        sts = SS$_NORMAL;
                    }
                }
                sys_close(&fab);
            }
        }
        if (sts == RMS$_NMF) sts = SS$_NORMAL;
    }
    if ( sts & STS$M_SUCCESS ) {
        if (filecount > 0) printf("%%COPY-S-NEWFILES, %d file%s created\n",
                                  filecount,(filecount == 1 ? "" : "s"));
    } else {
        printf("%%COPY-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}

/***************************************************************** doimport() */

static struct qual importquals[] = { {"ascii",  0,          COP_BINARY, NV,
                                      "Transfer file in ascii mode (default)"},
                                     {"binary", COP_BINARY, 0,          NV, "Transfer file in binary mode", },
                                     {NULL,     0,          0,          NV, NULL}
};
static struct param importpars[] = { {"from_filespec", REQ, LCLFS, NOPA, "for source file."},
                                     {"to_filespec",   REQ, VMSFS, NOPA, "for destination file."},
                                     { NULL, 0, 0, NOPA, NULL }
};

static unsigned doimport( int argc, char *argv[], int qualc, char *qualv[] ) {
    int options;
    unsigned sts;
    char *name;
    FILE *fromf;
    struct FAB fab = cc$rms_fab;
    struct RAB rab = cc$rms_rab;

    UNUSED(argc);

    options = checkquals( 0, importquals, qualc, qualv );
    if( options == -1 )
        return SS$_BADPARAM;

    sts = SS$_BADFILENAME;
    name = argv[1]; /*unquote( argv[1] );*/
    if ( *name == '\0' ) {
        return sts;
    }
    fromf = openf( name, (options & COP_BINARY)? "rb": "r" );
    if( fromf == NULL ) {
        printf( "%%ODS2-E-OPENERR, Can't open %s\n", name );
        perror( "  - " );
        return sts;
    }
    fab.fab$b_fac = FAB$M_PUT;
    fab.fab$l_fop = FAB$M_OFP | FAB$M_TEF;
    fab.fab$b_org = FAB$C_SEQ;
    if( options & COP_BINARY ) {
        fab.fab$w_mrs = 512;
        fab.fab$b_rfm = FAB$C_FIX;
    } else {
        fab.fab$b_rat = 0;
        fab.fab$b_rfm = FAB$C_STM;
    }

    fab.fab$l_fna = argv[2];
    fab.fab$b_fns = strlen( fab.fab$l_fna );
    sts = sys_create( &fab );
    if ( sts & STS$M_SUCCESS ) {
        rab.rab$l_fab = &fab;
        sts = sys_connect( &rab );
        if ( sts & STS$M_SUCCESS ) {
            if( options & COP_BINARY ) {
                char buf[512];
                size_t len;

                rab.rab$l_rbf = buf;
                while( (len = fread( buf, 1, 512, fromf )) > 0 ) {
                    if( len != 512 )
                        memset( buf+len, 0, 512-len );
                    rab.rab$w_rsz = len;
                    sts = sys_put( &rab );
                    if ( !( sts & STS$M_SUCCESS ) ) {
                        break;
                    }
                }
            } else {
                while ( (rab.rab$l_rbf = fgetline( fromf, TRUE )) != NULL ) {
                    rab.rab$w_rsz = strlen( rab.rab$l_rbf );
                    sts = sys_put( &rab );
                    free( rab.rab$l_rbf );
                    if ( !( sts & STS$M_SUCCESS ) ) {
                        break;
                    }
                }
            }
            sys_disconnect( &rab );
        }
        sys_close( &fab );
    }
    fclose( fromf );
    if ( !(sts & STS$M_SUCCESS) ) {
            printf("%%IMPORT-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }

    return sts;
}

/******************************************************************* dodiff() */

/* dodiff: a simple file difference routine */

static struct param diffpars[] = { {"ods-2_filespec", REQ, VMSFS, NOPA, "for file on ODS-2 volume."},
                                   {"local_filespec", REQ, LCLFS, NOPA, "for file on local filesystem."},
                                   { NULL,            0,   0,     NOPA, NULL }
};

unsigned dodiff(int argc,char *argv[],int qualc,char *qualv[]) {
    int sts, records = 0;
    char rec[MAXREC + 2], *cpy = NULL;
    char *name;
    FILE *tof;
    struct FAB fab = cc$rms_fab;
    struct RAB rab = cc$rms_rab;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    name = argv[1];
    sts = SS$_BADFILENAME;
    if ( *name == '\0' ) {
        return sts;
    }
    records = 0;
    fab.fab$l_fna = name;
    fab.fab$b_fns = strlen( fab.fab$l_fna );
    tof = openf( argv[2], "r" );
    if ( tof == NULL ) {
        printf("%%ODS2-E-OPENERR, Could not open file %s\n",name);
        perror( "  - " );
        return SS$_NOSUCHFILE;
    }
    sts = sys_open( &fab );
    if ( sts & STS$M_SUCCESS ) {
        rab.rab$l_fab = &fab;
        sts = sys_connect( &rab );
        if( sts & STS$M_SUCCESS ) {
            rab.rab$l_ubf = rec;
            rab.rab$w_usz = MAXREC;
            while( (sts = sys_get( &rab )) & STS$M_SUCCESS ) {
                rec[rab.rab$w_rsz] = '\0';
                cpy = fgetline( tof, FALSE );
                if( cpy == NULL ||
                    rab.rab$w_rsz != strlen( cpy ) ||
                    memcmp(rec,cpy,rab.rab$w_rsz) != 0 ) {

                    printf("%%DIFF-F-DIFFERENT Files are different!\n");
                    sts = 4;
                    break;
                }
                free(cpy);
                cpy = NULL;
                records++;
            }
            if( cpy != NULL ) free(cpy);
            sys_disconnect(&rab);
        }
        sys_close(&fab);
    }
    fclose(tof);
    if (sts == RMS$_EOF) sts = SS$_NORMAL;
    if ( sts & STS$M_SUCCESS ) {
        printf( "%%DIFF-I-Compared %d records\n", records );
    } else if ( sts != 4 ) {
        printf("%%DIFF-F-Error %s in difference\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}

/******************************************************************* dotype() */

/* dotype: a file TYPE routine */

static struct param typepars[] = { {"filespec", REQ, VMSFS, NOPA, "for file on ODS-2 volume."},
                                   { NULL, 0, 0, NOPA, NULL }
};

static unsigned dotype(int argc,char *argv[],int qualc,char *qualv[]) {
    int sts;
    int records;
    struct FAB fab = cc$rms_fab;
    struct RAB rab = cc$rms_rab;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    records = 0;

    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    sts = sys_open( &fab );
    if ( sts & STS$M_SUCCESS ) {
        rab.rab$l_fab = &fab;
        sts = sys_connect( &rab );
        if ( sts & STS$M_SUCCESS ) {
            char rec[MAXREC + 2];
            rab.rab$l_ubf = rec;
            rab.rab$w_usz = MAXREC;
            while ( ( sts = sys_get( &rab ) ) & STS$M_SUCCESS ) {
                unsigned rsz = rab.rab$w_rsz;
                if (fab.fab$b_rat & PRINT_ATTR) rec[rsz++] = '\n';
                rec[rsz++] = '\0';
                fputs(rec,stdout);
                records++;
            }
            sys_disconnect(&rab);
        }
        sys_close(&fab);
        if (sts == RMS$_EOF) sts = SS$_NORMAL;
    }
    if ( !( sts & STS$M_SUCCESS ) ) {
        printf("%%TYPE-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}

/***************************************************************** dosearch() */

/* dosearch: a simple file search routine */

static struct param searchpars[] = { {"filespec", REQ, VMSFS,  NOPA, "for file to search. Wildcards are allowed."},
                                     {"string",   REQ, STRING, NOPA, "string to search for."},
                                     { NULL, 0, 0, NOPA, NULL }
};

static unsigned dosearch(int argc,char *argv[],int qualc,char *qualv[]) {
    int sts = 0;
    int filecount = 0;
    int findcount = 0;
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    register char firstch, *searstr, *searend, *s;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    struct RAB rab = cc$rms_rab;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    searstr = argv[2]; /* unquote( argv[2] ); */

    searend = searstr + strlen( searstr );
    for ( s = searstr; s < searend; s++ ) {
        *s = tolower( *s );
    }
    firstch = *searstr++;
    filecount = 0;
    findcount = 0;
    nam = cc$rms_nam;
    fab = cc$rms_fab;
    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    fab.fab$l_dna = "";
    fab.fab$b_dns = strlen(fab.fab$l_dna);
    sts = sys_parse(&fab);
    if ( sts & STS$M_SUCCESS ) {
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
        while ( ( sts = sys_search( &fab ) ) & STS$M_SUCCESS ) {
            sts = sys_open(&fab);
            if ( !( sts & STS$M_SUCCESS ) ) {
                printf("%%SEARCH-F-OPENFAIL, Open error: %s\n",getmsg(sts, MSG_TEXT));
            } else {
                rab.rab$l_fab = &fab;
                sts = sys_connect( &rab );
                if ( sts & STS$M_SUCCESS ) {
                    int printname = 1;
                    char rec[MAXREC + 2];
                    filecount++;
                    rab.rab$l_ubf = rec;
                    rab.rab$w_usz = MAXREC;
                    while ( ( sts = sys_get( &rab ) ) & STS$M_SUCCESS ) {
                        register char *strng = rec;
                        register char *strngend = strng + (rab.rab$w_rsz -
                                                  (searend - searstr));
                        while (strng < strngend) {
                            register char ch = *strng++;
                            if (ch == firstch ||
                                (ch >= 'A' && ch <= 'Z' &&
                                 ch + 32 == firstch)) {
                                register char *str = strng;
                                register char *cmp = searstr;
                                while (cmp < searend) {
                                    register char ch2 = *str++;
                                    ch = *cmp;
                                    if (ch2 != ch &&
                                        (ch2 < 'A' || ch2 > 'Z' ||
                                         ch2 + 32 != ch)) break;
                                    cmp++;
                                }
                                if (cmp >= searend) {
                                    findcount++;
                                    rec[rab.rab$w_rsz] = '\0';
                                    if (printname) {
                                        rsa[nam.nam$b_rsl] = '\0';
                                        printf(
                                            "\n******************************\n"
                                        );
                                        printf("%s\n\n",rsa);
                                        printname = 0;
                                    }
                                    fputs(rec,stdout);
                                    if (fab.fab$b_rat & PRINT_ATTR) {
                                        fputc('\n',stdout);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    sys_disconnect(&rab);
                }
                if (sts == SS$_NOTINSTALL) {
                    printf(
                        "%%SEARCH-W-NOIMPLEM, file operation not implemented\n"
                    );
                    sts = SS$_NORMAL;
                }
                sys_close(&fab);
            }
        }
        if (sts == RMS$_NMF || sts == RMS$_FNF) sts = SS$_NORMAL;
    }
    if ( sts & STS$M_SUCCESS ) {
        if (filecount < 1) {
            printf("%%SEARCH-W-NOFILES, no files found\n");
        } else {
            if (findcount < 1) {
                printf("%%SEARCH-I-NOMATCHES, no strings matched\n");
            }
        }
    } else {
        printf("%%SEARCH-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}

/***************************************************************** dodelete() */

/* dodelete: you don't want to know! */

static struct qual delquals[] = { {"log",   1, 0, NV, "-List name of each file deleted. (Default)"},
                                  {"nolog", 0, 1, NV, NULL },
                                  { NULL,   0, 0, NV, NULL }
};
static struct param delpars[] = { {"filespec", REQ, VMSFS, NOPA,
                                   "for files to be deleted from ODS-2 volume.  Wildcards are permitted.."},
                                  { NULL,      0,   0,     NOPA, NULL }
};

static unsigned dodelete(int argc,char *argv[],int qualc,char *qualv[]) {
    int sts = 0;
    char res[NAM$C_MAXRSS + 1], rsa[NAM$C_MAXRSS + 1];
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    int options, filecount = 0;

    UNUSED(argc);

    options = checkquals(1,delquals,qualc,qualv);
    if( options == -1 )
        return SS$_BADPARAM;

    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    sts = sys_parse(&fab);
    if ( sts & STS$M_SUCCESS ) {
        if (nam.nam$b_ver < 2) {
            printf("%%DELETE-F-NOVER, you must specify a version!!\n");
            return SS$_BADPARAM;
        }

        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
            while ( ( sts = sys_search( &fab ) ) & STS$M_SUCCESS ) {
            sts = sys_erase(&fab);
                if ( !( sts & STS$M_SUCCESS ) ) {
                printf("%%DELETE-F-DELERR, Delete error: %s\n",getmsg(sts, MSG_TEXT));
                break;
            } else {
                filecount++;
                if( options & 1 ) {
                    rsa[nam.nam$b_rsl] = '\0';
                    if( options & 1 )
                        printf("%%DELETE-I-DELETED, Deleted %s\n",rsa);
                }
            }
        }
        if (sts == RMS$_NMF) sts = SS$_NORMAL;
    }
        if ( sts & STS$M_SUCCESS ) {
        if (filecount < 1) {
            printf("%%DELETE-W-NOFILES, no files deleted\n");
        }
    } else {
        printf("%%DELETE-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }

    return sts;
}

/******************************************************************* dotest() */

static struct param testpars[] = { {"parameter", REQ, STRING, NOPA, "for test."},
                                   { NULL, 0, 0, NOPA, NULL }
};

struct VCB *test_vcb;

/* dotest: you don't want to know! */

static unsigned dotest( int argc, char *argv[], int qualc, char *qualv[] ) {
    unsigned sts = 0;
    struct fiddef fid;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    sts = update_create( test_vcb, NULL, "Test.File", &fid, NULL );
    printf( "Test status of %d (%s)\n", sts, argv[1] );
    return sts;
}

/***************************************************************** doextend() */

/* more test code... */

static struct param extendpars[] = { {"ods-2_filespec", REQ, VMSFS, NOPA, "for file on ODS-2 volume."},
                                     { NULL, 0, 0, NOPA, NULL }
};

static unsigned doextend(int argc,char *argv[],int qualc,char *qualv[]) {
    int sts;
    struct FAB fab = cc$rms_fab;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    fab.fab$b_fac = FAB$M_UPD;
    sts = sys_open( &fab );
    if ( sts & STS$M_SUCCESS ) {
        fab.fab$l_alq = 32;
        sts = sys_extend(&fab);
        sys_close(&fab);
    }
    if ( !( sts & STS$M_SUCCESS ) ) {
        printf("%%EXTEND-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}

/****************************************************************** dostats() */

/* dostats: print some simple statistics */

static unsigned dostats( void ) {

    printf("Statistics:-\n");
    direct_show();
    cache_show();
    phyio_show(SHOW_STATS);

    return SS$_NORMAL;
}

/******************************************************************* doshow() */

/* show: version */

#define MNAMEX(n) #n
#define MNAME(n) MNAMEX(n)

static void show_version( void ) {
    printf(" %s %s", MNAME(MODULE_NAME), MODULE_IDENT );
    printf( " built %s %s", __DATE__, __TIME__ );
#ifdef USE_READLINE
    printf(" with readline version %s", rl_library_version );
#endif
#ifdef USE_WNASPI32
# ifdef USE_READLINE
    printf( " and" );
# else
    printf( " with" );
# endif
    printf( " direct SCSI access support");
#endif
    printf( "\n\n" );
    return;
}

/* show: the show command */

static struct qual showkwds[] = { {"default",         0, 0, NV, "Default directory"},
                                  {"devices",         1, 0, NV, "Devices"},
                                  {"qualifier_style", 2, 0, NV, "Qualifier style (Unix, VMS)" },
                                  {"statistics",      3, 0, NV, "Debugging statistics", },
                                  {"time",            4, 0, NV, "Time"},
                                  {"verify",          5, 0, NV, "Command file echo" },
                                  {"version",         6, 0, NV, "Version"},
                                  {"volumes",         7, 0, NV, "Mounted volume information" },
                                  {NULL,              0, 0, NV, NULL }
};
static struct param showpars[] = { {"item_name", REQ, KEYWD, PA(showkwds), NULL },
                                   {NULL,        0,   0,     NOPA,         NULL }
};

unsigned doshow(int argc,char *argv[],int qualc,char *qualv[]) {
    int parnum;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    parnum = checkquals( 0, showkwds, -1, argv+1 );
    switch( parnum ) {
    default:
        return SS$_BADPARAM;
    case 0: {
        int sts;
        unsigned short curlen;
        char curdir[NAM$C_MAXRSS + 1];
        struct dsc_descriptor curdsc;
        curdsc.dsc_w_length = NAM$C_MAXRSS;
        curdsc.dsc_a_pointer = curdir;
        sts = sys_setddir( NULL, &curlen, &curdsc );
        if ( sts & STS$M_SUCCESS ) {
            curdir[curlen] = '\0';
            puts( curdir );
        } else {
            printf("%%ODS2-E-GETDEF, Error %s getting default\n",getmsg(sts, MSG_TEXT));
        }
        return sts;
    }
    case 1:
        phyio_show( SHOW_DEVICES );
        virt_show( NULL );
        return SS$_NORMAL;
    case 2:
        printf ( "  Qualifier style: %s\n", vms_qual? "/VMS": "-unix" );
        return SS$_NORMAL;
    case 3:
        return dostats();
    case 4: {
        unsigned sts;
        char timstr[24];
        unsigned short timlen;
        struct dsc_descriptor timdsc;

        timdsc.dsc_w_length = 20;
        timdsc.dsc_a_pointer = timstr;
        sts = sys$asctim( &timlen, &timdsc, NULL, 0 );
        if ( sts & STS$M_SUCCESS ) {
            timstr[timlen] = '\0';
            printf("  %s\n",timstr);
        } else {
            printf("%%SHOW-W-TIMERR error %s\n",getmsg(sts, MSG_TEXT));
        }
    }
        return SS$_NORMAL;
    case 5:
        printf( "Command file verification is %s\n", (verify_cmd? "on": "off") );
        return SS$_NORMAL;
    case 6:
        show_version();
        return SS$_NORMAL;
    case 7:
        show_volumes();
        return SS$_NORMAL;
    }
    return SS$_NORMAL;
}

/******************************************************************* setdef() */

static unsigned setdef_count = 0;

static unsigned setdef( char *newdef )
{
    register unsigned sts;
    struct dsc_descriptor defdsc;

    defdsc.dsc_a_pointer = (char *) newdef;
    defdsc.dsc_w_length = strlen( defdsc.dsc_a_pointer );
    sts = sys_setddir( &defdsc, NULL, NULL );
    if ( sts & STS$M_SUCCESS ) {
        setdef_count++;
    } else {
        printf( "%%ODS2-E-SETDEF, Error %s setting default to %s\n", getmsg(sts, MSG_TEXT), newdef );
    }
    return sts;
}

/******************************************************************** doset() */

static hlpfunc_t sethelp;

static struct qual setkwds[] = { {"default",              0, 0, NV, "Default directory"},
                                 {"directory_qualifiers", 1, 0, NV, "Default qualifiers for DIRECTORY command" },
                                 {"qualifier_style",      2, 0, NV, "Qualifier style (Unix, VMS)" },
                                 {"verify",               3, 0, NV, "-Display commands in indirect files" },
                                 {"noverify",             4, 0, NV, NULL },
                                 {NULL,                   0, 0, NV, NULL }
};
static struct qual setqskwds[] = {{"unix", 1, 0, NV, "Unix style options, '-option'"},
                                  {"vms",  2, 0, NV, "VMS style qualifiers, '/qualifier'"},
                                  {NULL,   0, 0, NV, NULL }
};
static struct param setpars[] = { {"item_name", REQ, KEYWD, PA(setkwds),         NULL },
                                  {"value"    , CND, KEYWD, sethelp, setqskwds,  NULL },
                                  {NULL,        0,   0,      NOPA,               NULL },
};

static const char * sethelp( struct CMDSET *cmd, struct param *p, int argc, char **argv ) {
    int par;

    UNUSED( cmd );
    UNUSED( argc );

    if( argc == 1 ) {
        p->helpstr = "";
        p->ptype = KEYWD;
        p->arg = setkwds;
        return "Type 'help set value ITEM' for item details\n";
    }

    par = checkquals( 0, setkwds, -1, argv+1 );
    if( par == -1 )
        return NULL;
    switch( par ) {
    case 0:
        p->helpstr = "default directory on volume - ";
        p->ptype = VMSFS;
        break;
    case 1:
        p->helpstr = "directory qualifier name ";
        p->ptype = KEYWD;
        p->arg = dirquals;
        break;
    case 2:
        p->helpstr = "style ";
        p->ptype = KEYWD;
        p->arg = setqskwds;
        break;
    case 3:
    case 4:
        p->ptype = NONE;
        break;

    default:
        abort();
    }
    return NULL;
}

static unsigned doset(int argc,char *argv[],int qualc,char *qualv[]) {
    int parnum;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    parnum = checkquals( 0, setkwds, -1, argv+1 );
    switch( parnum ) {
    default:
        return SS$_BADPARAM;
    case 0: /* default */
        if( qualc ) {
            printf( "%%ODS2-E-NOQUAL, No qualifiers are permitted\n" );
            return 0;
        }
        return setdef( argv[2] );
    case 1:{ /* directory_qualifiers */
        int options = checkquals(dir_default,dirquals,qualc,qualv);
        if( options == -1 )
            return SS$_BADPARAM;
        dir_defopt = options;
        return 1;
    }
    case 2: { /* qualifier_style */
        int par = checkquals (0,setqskwds,1,argv+2);
        if( par == -1 )
            return SS$_BADPARAM;
        switch(par) {
        default:
            abort();
        case 2:
            vms_qual = 1;
            break;
        case 1:
            vms_qual = 0;
            break;
        }
        return 1;
    }
    case 3:
        verify_cmd = 1;
        return 1;
    case 4:
        verify_cmd = 0;
        return 1;
    }
}

/* The bits we need when we don't have real VMS routines underneath... */

static struct param dmopars[] = { {"drive_letter", REQ, STRING, NOPA, "Drive containing volume to dismount", },
                                  {NULL,           0,   0,      NOPA, NULL }
};

unsigned dodismount(int argc,char *argv[],int qualc,char *qualv[]) {
    struct DEV *dev;
    int sts;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    sts = device_lookup(strlen(argv[1]),argv[1],FALSE,&dev);
    if (sts & 1) {
        if (dev->vcb != NULL) {
            sts = dismount(dev->vcb);
        } else {
            sts = SS$_DEVNOTMOUNT;
        }
    }
    if (!(sts & STS$M_SUCCESS)) printf("%%DISMOUNT-E-STATUS Error: %s\n",getmsg(sts, MSG_TEXT));
    return sts;
}

#if MOU_WRITE != 1
#error MOU_WRITE != 1
#endif

static struct qual mouquals[] = { {"image",    MOU_VIRTUAL, 0,         NV, "Mount a disk image file", },
                                  {"readonly", 0,           MOU_WRITE, NV, "Only allow reading from volume"},
                                  {"virtual",  MOU_VIRTUAL, 0,         NV, NULL, },
                                  {"write",    MOU_WRITE,   0,         NV, "Allow writing to volume", },
                                  {NULL,       0,           0,         NV, NULL } };

static struct param moupars[] = { {"volumes", REQ, LIST, NOPA,
                                   "devices or disk image(s) of volume set in RVN order separated by comma"
                                  },
                                  {"labels", OPT, LIST, NOPA, "volume labels in RVN order separated by comma" },
                                  { NULL,    0,   0,    NOPA, NULL }
};

static int parselist( char ***items, size_t min, char *arg, const char *label ) {
    size_t n = 0, i;
    char **list = NULL;

    *items = NULL;

    while( *arg != '\0' ) {
        char **nl;

        while( *arg == ' ' || *arg == '\t' )
            arg++;
        if( *arg == '\0' )
            break;

        nl = (char **) realloc( list, (n < min? min +1: n +2) * sizeof( char * ) );
        if( nl == NULL ) {
            free( list );
            printf( "%%ODS2-E-NOMEM, Not enough memory for %s\n", label );
            return -1;
        }
        list = nl;

        list[n++] = arg;
        while( !(*arg == ',' || *arg == '\0') )
            arg++;
        if( *arg == '\0' )
            break;
        *arg++ = '\0';
    }

    if( list == NULL ) {
        list = (char **) malloc( (min + 1) * sizeof( char * ) );
        if( list == NULL ) {
            printf( "%%ODS2-E-NOMEM, Not enough memory for %s\n", label );
            return -1;
        }
    }
    for( i = n; i < min; i++ )
        list[i] = NULL;

    list[i] = NULL;

    *items = list;
    return (int)n;
}

static unsigned domount(int argc,char *argv[],int qualc,char *qualv[]) {
    int sts = 1,devices = 0;
    char **devs = NULL, **labs = NULL;

    int options;

    options = checkquals( 0, mouquals, qualc, qualv );
    if( options == -1 )
        return SS$_BADPARAM;

    UNUSED(argc);

    if( (devices = parselist( &devs, 0, argv[1], "devices")) < 0 )
        return SS$_BADPARAM;
    if( parselist( &labs, devices, argv[2], "labels") < 0 ) {
        free( devs );
        return SS$_BADPARAM;
    }

    if (devices > 0) {
        struct VCB *vcb;
        sts = mount( options | MOU_LOG, devices, devs, labs, &vcb );
        if (sts & STS$M_SUCCESS) {
            if (setdef_count == 0) {
                char *colon, *buf;
                size_t len;

                len = strlen( vcb->vcbdev[0].dev->devnam );
                buf = (char *) malloc( len  + sizeof( ":[000000]" ));
                if( buf == NULL ) {
                    perror( "malloc" );
                } else {
                    colon = strchr( vcb->vcbdev[0].dev->devnam, ':' );
                    if( colon != NULL ) len = (size_t)(colon - vcb->vcbdev[0].dev->devnam);
                    memcpy( buf, vcb->vcbdev[0].dev->devnam, len );
                    memcpy( buf+len, ":[000000]", sizeof( ":[000000]" ) );
                    setdef(buf);
                    free( buf );
                }
            }
            test_vcb = vcb;
        } else {
            printf("%%ODS2-E-MOUNTERR, Mount failed with %s\n", getmsg(sts, MSG_TEXT));
        }
    }

    free( devs );
    free( labs );
    return sts;
}

/******************************************************************* dohelp() */

static void cmdhelp( struct CMDSET *cmdset ) {
    struct CMDSET *cmd;
    int n = 0;
    size_t max = 0;

    for( cmd = cmdset; cmd->name; cmd++ ) {
        if( strlen(cmd->name) > max )
            max = strlen(cmd->name);
    }
    for( cmd = cmdset; cmd->name; cmd++ ) {
        if( cmd->helpstr == NULL )
            continue;
        if( n++ % 4 == 0 )
            printf( "\n" );
        printf("    %-*s", (int)max,cmd->name );
    }
    printf( "\n" );
}

static void qualhelp( int par, struct qual *qtable ) {
    struct qual *q;
    int n = 0;
    size_t max = 0;

    if( par < 0 )
        max = -par;

#define NOSTR "[no]"
#define NOSTR_LEN (sizeof(NOSTR)-1)
    for( q = qtable; q->name; q++ ) {
        if( q->helpstr ) {
            size_t len = strlen(q->name);
            if( *q->helpstr == '-' )
                len += NOSTR_LEN;
            if( q->qtype != NOVAL )
                len++;
            if( len > max )
                max = len;
        }
    }
    for( q = qtable; q->name; q++ ) {
        if( q->helpstr ) {
            if( !n++ && !par )
                printf( "  %s:\n", "Qualifiers"  );
            printf("    %s", par? par<0? "   ": "": vms_qual? "/" : "-" );
            if( *q->helpstr == '-' )
                switch( q->qtype ) {
                case NOVAL:
                    printf( NOSTR "%-*s - %s\n",
                            (int) (max-NOSTR_LEN), q->name, q->helpstr+1 );
                    break;
                case KEYVAL:
                    printf( NOSTR "%s=%-*s - %s\n",
                            q->name, (int) (max-(NOSTR_LEN+strlen(q->name)+1)), "",
                            q->helpstr+1 );
                    qualhelp( -(int)max, (struct qual *)q->arg );
                    break;
                default:
                    abort();
                }
            else
                switch( q->qtype ) {
                case NOVAL:
                    printf("%-*s - %s\n", (int) max, q->name, q->helpstr );
                    break;
                case KEYVAL:
                    printf( "%s=%-*s - %s\n", q->name, (int)(max-strlen(q->name)+1), "",
                            q->helpstr );
                    qualhelp( -(int)max, (struct qual *)q->arg );
                    break;
                default:
                    abort();
                }
        }
    }
    if( par >= 0 )
        printf( "\n" );
}
#undef NOSTR
#undef NOSTR_LEN

static void parhelp( struct CMDSET *cmd, int argc, char **argv) {
    struct param *p;
    struct param *ptable;
    const char *footer = NULL;

    ptable = cmd->params;

    if( ptable == NULL ) {
        printf( "%s has no parameters\n", cmd->name );
        return;
    }

    if( argc == 0 ) {
        int col = 0;
        size_t max = 0;

        for( p = ptable; p->name != NULL; p++ ) {
            if( 1||p->helpstr ) {
                size_t len = strlen(p->name);
                if( len > max )
                    max = len;
            }
        }
        for( p = ptable; p->name != NULL; p++ ) {
            if(1|| p->helpstr ) {
                size_t len = strlen(p->name);
                if( !col ) {
                    printf( "  Parameters:\n    " );
                    col = 4;
                } else {
                    if( 1+col + len > 80 ) {
                        printf( "\n    " );
                        col = 4;
                    } else {
                        printf ( " " );
                        col++;
                    }
                }
                printf( "%-*s", (int)max, p->name );
                col += len;
            }
        }
        printf( "\n\n  Type help PARAMETER for more about each parameter.\n" );
        return;
    }

    for( p = ptable; p->name != NULL; p++ ) {
        if( keycomp( argv[0], p->name ) )
            break;
    }
    if( p->name == NULL ) {
        printf( "No parameter '%s' found\n",  argv[0] );
        return;
    }
    if( p->hlpfnc != NULL )
        footer = (*p->hlpfnc)(cmd, p, argc, argv);
    if( p->ptype == NONE ) {
        printf( "Parameter is not used for this command\n" );
        return;
    }

    printf( "  %s: ", p->name );

    if( p->flags == OPT )
        printf( "optional " );

    switch( p->ptype ) {
    case VMSFS:
        printf( "VMS file specification %s\n", p->helpstr );
        break;
    case LCLFS:
        printf( "local file specification %s\n", p->helpstr );
        break;
    case KEYWD:
        printf( "%skeyword, one of the following:\n",
                (p->helpstr == NULL? "": p->helpstr) );
        qualhelp( 1, (struct qual *)p->arg );
        break;
    case LIST:
        printf( "list, %s\n", p->helpstr );
        break;
    case STRING:
        printf( "%s\n", p->helpstr );
        break;
    case CMDNAM:
        printf( "command name, one of the following:\n");
        cmdhelp( (struct CMDSET *)p->arg );
        break;
    default:
        abort();
    }
    if( footer )
        printf( "%s", footer );

    return;
}

/* help: Display help guided by command table. */
#define NCMD 16
static struct CMDSET cmdset[NCMD+1];

static struct param helppars[] = { {"command",   OPT, CMDNAM, PA(cmdset),         NULL },
                                   {"parameter", OPT, STRING, NOPA,               NULL },
                                   {"value",     OPT, STRING, NOPA,               NULL },
                                   {NULL,        0,   0,      NOPA,               NULL },
};

/* information about the commands we know... */
static unsigned dohelp(int argc,char *argv[],int qualc,char *qualv[]);

static struct CMDSET cmdset[NCMD+1] = {
    { "copy",      docopy,    0,copyquals,copypars,   "Copy a file from VMS to host file", },
    { "import",    doimport,  0,NULL,     importpars, "Copy a file from host to VMS", },
    { "delete",    dodelete,  0,delquals, delpars,    "Delete a VMS file", },
    { "difference",dodiff,    0,NULL,     diffpars,   "Compare VMS file to host file", },
    { "directory", dodir,     0,dirquals,dirpars,     "List directory of VMS files", },
    { "exit",      NULL,      2,NULL,     NULL,       "Exit ODS2", },
    { "extend",    doextend,  0,NULL,     extendpars, NULL },
    { "help",      dohelp,    0,NULL,     helppars,   "Obtain help on a command", },
    { "quit",      NULL,      2,NULL,     NULL,       "Exit ODS-2", },
    { "show",      doshow,    0,NULL,     showpars,   "Display state", },
    { "search",    dosearch,  0,NULL,     searchpars, "Search VMS file for a string", },
    { "set",       doset,     0,NULL,    setpars,     "Set PARAMETER - set HELP for list", },
    { "dismount",  dodismount,0,NULL,     dmopars,    "Dismount a VMS volume", },
    { "mount",     domount,   0,mouquals, moupars,    "Mount a VMS volume", },
    { "test",      dotest,    0,NULL,     testpars,   NULL },
    { "type",      dotype,    0,NULL,     typepars,   "Display a VMS file on the terminal", },
    { NULL,        NULL,      0,NULL,     NULL, NULL } /* ** END MARKER ** */
};

static unsigned dohelp(int argc,char *argv[],int qualc,char *qualv[]) {
    struct CMDSET *cmd;

    UNUSED(qualc);
    UNUSED(qualv);

    if( argc <= 1 ) {
        show_version();

        printf( " The orginal version of ods2 was developed by Paul Nankervis\n" );
        printf( " <Paulnank@au1.ibm.com>\n" );
        printf( " This modified version was developed by Timothe Litt, and\n" );
        printf( " incorporates significant previous modifications by Larry \n" );
        printf( " Baker of the USGS and by Hunter Goatley\n" );
        printf("\n Please send problems/comments to litt@ieee.org\n\n");

        printf(" Commands are:\n");
        cmdhelp( cmdset );
        printf( "\n Type HELP COMMAND for information on any command.\n" );
        return 1;
    }

    for( cmd = cmdset; cmd->name; cmd++ ) {
        if( keycomp(argv[1],cmd->name) ) {
            if( argc >= 3 ) {
                parhelp( cmd, argc-2, argv+2 );
                return 1;
            }
            if( cmd->helpstr == NULL ) {
                printf( "%s: No help available\n",cmd->name );
            } else {
                printf( "%s: %s\n",cmd->name,cmd->helpstr );
            }
            if( cmd->validquals != NULL )
                qualhelp( 0, cmd->validquals );
            if( cmd->params != NULL )
                parhelp( cmd, 0, NULL );
            if( strcmp(cmd->name, "mount") == 0 ) {
                printf( "\n" );
                printf( "You can mount a volume(-set) from either physical devices\n" );
                printf( "such a CDROM or hard disk drive, or files containing an\n" );
                printf( "image of a volume, such as a .ISO file or simulator disk\n\n" );
                printf( "To mount a disk image, use the %cimage qualifier and\n",
                        (vms_qual? '/': '-') );
                printf( "specify the filename as the parameter.\n\n" );
                printf( "If the filename contains %c, specify it in double quotes\n\n",
                        (vms_qual? '/': '-') );
                printf( "Mount will assign a virtual device name to each volume.\n" );
                printf( "You can select a virtual device name using the format\n" );
                printf( "  dka100=my_files.iso\n\n" );
                printf( "To mount a physical device, use the format:\n" );
                phyio_help(stdout);
                printf( "To mount a volume set, specify all the members in RVN order\n" );
                printf( "as a comma-separated list.\n\n" );
                printf( "If you specify a list of volume labels, they must be in\n" );
                printf( "the same order as the volumes, and each must match the label\n" );
                printf( "stored in the data\n" );
            }
            return 1;
        }
    }
    printf( "%s: command not found\n", argv[1] );
    return 0;

    printf("  set_default type\n");
    printf(" Example:-\n    $ mount e:\n");
    printf("    $ search e:[vms_common.decc*...]*.h rms$_wld\n");
    printf("    $ set default e:[sys0.sysmgr]\n");
    printf("    $ copy *.com;-1 c:\\*.*\n");
    printf("    $ directory/file/size/date [-.sys*...].%%\n");
    printf("    $ exit\n");
    return 1;
}

/*************************************************************** cmdexecute() */
/* cmdexecute: identify and execute a command */

static int cmdexecute( int argc, char *argv[], int qualc, char *qualv[] ) {
    char *ptr;
    struct CMDSET *cmd, *cp = NULL;
    unsigned cmdsiz;
    int minpars, maxpars;
    struct param *p;
    int sts;

    ptr = argv[0];
    while (*ptr != '\0') {
        *ptr = tolower(*ptr);
        ptr++;
    }
    ptr = argv[0];
    cmdsiz = strlen(ptr);

    for( cmd = cmdset; cmd->name != NULL; cmd++ ) {
        if( cmdsiz <= strlen( cmd->name ) &&
            keycomp( ptr, cmd->name ) ) {
            if( (cmd->uniq && cmdsiz >= cmd->uniq) ) {
                cp = cmd;
                break;
            }
            if( cp != NULL ) {
                printf("%%ODS2-E-AMBCMD, '%s' is ambiguous\n", ptr );
                return 0;
            }
            cp = cmd;
        }
    }
    if( cp == NULL ) {
        printf("%%ODS2-E-ILLCMD, Unknown command '%s'\n", ptr );
        return 0;
    }
    cmd = cp;

    if (cmd->proc == NULL)
        return -1;

    minpars =
        maxpars = 1;
    for( p = cmd->params; p && p->name != NULL; p++ ) {
        maxpars++;
        if( p->flags == REQ )
            minpars++;
    }

    if (argc < minpars|| argc > maxpars) {
        printf( "%%ODS2-E-PARAMS, Too %s parameters specified\n", (argc < minpars? "few": "many") );
        return 0;
    }
    sts = (*cmd->proc) (argc,argv,qualc,qualv);
#ifndef VMSIO
    /* cache_flush();  */
#endif

    return sts;
}

/***************************************************************** cmdsplit() */

/* cmdsplit: break a command line into its components */

/*
 * New feature for Unix: '//' or '--' stops qualifier parsing.
 * This enables us to copy to Unix directories with VMS style /qualifiers.
 *     copy /bin // *.com /tmp/ *
 * is split into argv[0] -> "*.com" argv[1] -> "/tmp/ *" qualv[0]-> "/bin"
 *
 * Of course, one can just "" the string (VMS double quote rules)...
 */
#define MAXITEMS 32
static int cmdsplit(char *str) {
    int argc = 0,qualc = 0;
    char *argv[MAXITEMS],*qualv[MAXITEMS];
    char *sp = str;
    int i;
    char q = vms_qual? '/': '-';
    for (i = 0; i < MAXITEMS; i++) argv[i] = qualv[i] = "";
    while (*sp != '\0') {
        while (*sp == ' ') sp++;
        if (*sp != '\0') {
            if (*sp == q) {
                *sp++ = '\0';
                if (*sp == q) {
                      sp++;
                      q = '\0';
                      continue;
              }
                if( qualc >= MAXITEMS ) {
                    printf( "%%ODS2-E-CMDERR, Too many qualifiers specified\n" );
                    return 0;
                }
                qualv[qualc++] = sp;
            } else {
                if( argc >= MAXITEMS ) {
                    printf( "%%ODS2-E-CMDERR, Too many arguments specified\n" );
                    return 0;
                }
                argv[argc++] = sp;
                if( *sp == '"' ) {
                    ++argv[argc-1];
                    for( ++sp; *sp && (*sp != '"' || sp[1] == '"'); sp++ ) {
                        if( *sp == '"' )
                            memmove( sp, sp+1, strlen(sp+1)+1 );
                    }
                    if( *sp == '"' ) {
                        *sp++ = '\0';
                        if( *sp && *sp != ' ' ) {
                            printf( "%%ODS2-E-CMDERR, Unterminated string\n" );
                            return 0;
                        }
                    } else {
                        printf( "%%ODS2-E-CMDERR, Unterminated string\n" );
                        return 0;
                    }
                    continue;
                }
            }
            while (*sp != ' ' && *sp != q && *sp != '\0') sp++;
            if (*sp == '\0') {
                break;
            } else {
                if (*sp != q) *sp++ = '\0';
            }
        }
    }

    if (argc > 0) return cmdexecute(argc,argv,qualc,qualv);
    return 1;
}

#ifdef VMS
static char *getcmd( char *inp, size_t max, char *prompt ) {
    struct dsc_descriptor prompt_d = { strlen(prompt),DSC$K_DTYPE_T,
                                       DSC$K_CLASS_S, prompt };
    struct dsc_descriptor input_d = { max -1,DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, inp };
    int status;
    char *retstat;
    static unsigned long key_table_id = 0;
    static unsigned long keyboard_id = 0;

    if (key_table_id == 0) {
        status = smg$create_key_table( &key_table_id );
        if (status & 1)
            status = smg$create_virtual_keyboard( &keyboard_id );
        if (!(status & 1)) return (NULL);
    }

    status = smg$read_composed_line (&keyboard_id, &key_table_id,
                    &input_d, &prompt_d, &input_d, 0,0,0,0,0,0,0);

    if (status == SMG$_EOF)
        retstat = NULL;
    else {
        inp[input_d.dsc_w_length] = '\0';
        retstat = inp;
    }

    return(retstat);
}
#endif /* VMS */

/* main: the simple mainline of this puppy... */

/*
 * Parse the command line to read and execute commands:
 *     ./ods2 mount scd1 $ set def [hartmut] $ copy *.com $ exit
 * '$' is used as command delimiter because it is a familiar character
 * in the VMS world. However, it should be used surounded by white spaces;
 * otherwise, a token '$copy' is interpreted by the Unix shell as a shell
 * variable. Quoting the whole command string might help:
 *     ./ods2 'mount scd1 $set def [hartmut] $copy *.com $exit'
 * If the ods2 reader should use any switches '-c' should be used for
 * the command strings, then quoting will be required:
 *     ./ods2 -c 'mount scd1 $ set def [hartmut] $ copy *.com $ exit'
 *
 * The same command concatenation can be implemented for the prompted input.
 */

int main( int argc,char *argv[] ) {
    int sts;
    char *command_line = NULL;
    FILE *atfile = NULL;
    char *rl = NULL;
#ifdef VMS
    char str[2048];
#endif
#ifdef USE_READLINE
    char *p;
    wordexp_t wex;
    char *hfname = NULL;
    char mname[3+sizeof( MNAME(MODULE_NAME) )];

    snprintf( mname, sizeof(mname ), "~/.%s", MNAME(MODULE_NAME) );
    rl_readline_name =
      p = mname+3;
    do {
        *p = tolower( *p );
    } while( *p++ );
    memset( &wex, 0, sizeof( wordexp_t ) );
    if( wordexp( mname, &wex, WRDE_NOCMD ) == 0 && wex.we_wordc == 1 ) {
        hfname = wex.we_wordv[0];
        history_truncate_file( hfname, 200 );
        stifle_history( 200 );
        read_history( hfname );
    } else {
        hfname = NULL;
    }
#endif

    if( argc > 1 ) {
        int i, l = 0;
        for( i = 1; i < argc; i++ ) {
            int al;
            char *newp;

            al = strlen(argv[i]);
            newp = (char *)realloc( command_line, l+(l != 0)+al+1 );
            if( newp == NULL ) {
                perror( "realloc" );
                exit (1);
            }
            command_line = newp;
            if( l != 0 ) command_line[l++] = ' ';
            memcpy( command_line+l, argv[i], al+1 );
            l += al;
        }
    }
    while( 1 ) {
        char *ptr = NULL;
        if( command_line ) {
            static int i = 0;
            char *p;

            ptr = command_line + i;
            if( (p = strchr( ptr, '$' )) == NULL ) {
                if( *ptr == '\0' ) {
                    free( command_line );
                    command_line = NULL;
                    ptr = NULL;
                } else {
                    i += strlen( ptr );
                }
            } else {
                *p = '\0';
                i += strlen( ptr ) +1;
            }
        }
        if( ptr == NULL ) {
            if (atfile != NULL) {
                if( rl != NULL ) free( rl );
                if( (rl = fgetline(atfile, FALSE)) == NULL) {
                    fclose(atfile);
                    atfile = NULL;
                } else {
#ifndef _WIN32
                    ptr = strchr( rl, '\r' );
                    if( ptr != NULL )
                        *ptr = '\0';
#endif
                    if( verify_cmd )
                        printf("$> %s\n", rl);
                }
                ptr = rl;
            }
            if( ptr == NULL ) {
#ifdef VMS
                if( getcmd( str, sizeof(str), "$> " ) == NULL ) break;
                ptr = str;
#else
                if( rl != NULL ) {
                    free( rl );
                }
#ifdef USE_READLINE
                rl =
                    ptr = readline( MNAME(MODULE_NAME) "$> " );
                if (rl == NULL) {
                    break;
                } else {
                    if( *rl != '\0' ) {
                        add_history( rl );
                    }
                }
#else
                printf("$> ");
                if( (rl = fgetline(stdin, FALSE)) == NULL) break;
                ptr = rl;
#endif
#endif
            }
        }

        while( *ptr == ' ' || *ptr == '\t' )
            ptr++;
        if( !strlen(ptr) || *ptr == '!' || *ptr == ';' )
            continue;

        if (*ptr == '@') {
            if (atfile != NULL) {
                printf("%%ODS2-W-INDIRECT, nested indirect command files not supported\n");
            } else {
                if ((atfile = openf(ptr + 1,"r")) == NULL) {
                    perror("%%ODS2-E-INDERR, Failed to open indirect command file");
                    printf("\n");
                    free(command_line);
                    command_line = NULL;
                }
            }
            continue;
        }

        sts = cmdsplit(ptr);
        if( sts == -1 )
            break;
        if ((sts & 1) == 0) {
            if( atfile != NULL ) {
                fclose(atfile);
                atfile = NULL;
            }
            free(command_line);
            command_line = NULL;
        }
    } /* while 1 */

    if( rl != NULL ) {
        free( rl );
    }
#ifdef USE_READLINE
    if( hfname != NULL ) {
        write_history( hfname );
    }
#endif
    if (atfile != NULL) fclose(atfile);
#ifdef VMS
    return 1;
#else
    return 0;
#endif
}
