/* main() - command parsing and dispatch */

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
 *
 *      The modules in ODS2 are:-
 *
 *              ACCESS.C        Routines for accessing ODS2 disks
 *              CACHE.C         Routines for managing memory cache
 *              COMPAT.C        Routines for OS compatibility
 *              DEVICE.C        Routines to maintain device information
 *              DIRECT.C        Routines for handling directories
 *              ODS2.C          The mainline program
 *              *CMD.C          Routines to execute CLI commands
 *              PHYVMS.C        Routine to perform physical I/O
 *              PHYVHD.C        Interface to libvhd for VHD format virtual disks.
 *              PHYVIRT.C       Routines for managing virtual disks.
 *              RMS.C           Routines to handle RMS structures
 *              SYSMSG.C        Routines to convert status codes to text
 *              UPDATE.C        Routines for updating ODS2 structures
 *              VMSTIME.C       Routines to handle VMS times
 *
 *      On non-VMS platforms PHYVMS.C should be replaced as follows:-
 *
 *              Unix            PHYUNIX.C
 *              OS/2            PHYOS2.C
 *              Windows 95/NT   PHYNT.C
 *
 *     The included make/mms/com files do all this for you.
 *
 *     For accessing disk images (e.g. .ISO or simulator files),
 *     simply mount /image (or /virtual) filename.
 *
 *     See version.h and git for revision history.
 */

/*  This is the top level set of routines. It is fairly
 *  simple minded asking the user for a command, doing some
 *  primitive command parsing, and then calling a set of routines
 *  to perform whatever function is required (for example COPY).
 *  Some routines are implemented in different ways to test the
 *  underlying routines.
 */

#include "version.h"

#if !defined( DEBUG ) && defined( DEBUG_ODS2 )
#define DEBUG DEBUG_ODS2
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#ifndef MAXATDEPTH
#define MAXATDEPTH 16
#endif

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
#else
#include <locale.h>
#endif /* VMS */

#ifndef _WIN32
#include <sys/stat.h>
#endif

#define DEBUGx on

#include <signal.h>

#include "version.h"
#include "cmddef.h"

#include "cache.h"
#include "compat.h"
#include "descrip.h"
#include "ods2.h"
#include "phyvirt.h"
#include "ssdef.h"
#include "stsdef.h"
#include "sysmsg.h"

#ifdef VAXC
#ifdef EXIT_SUCCESS
#undef EXIT_SUCCESS
#endif
#define EXIT_SUCCESS SS$_NORMAL
#endif

#if defined( _WIN32 ) && defined(_MSC_VER) && defined( DEBUG_BUILD ) && defined( USE_VLD )
/* Normally done in the project file, but VLD is optional and I'd rather not provide
 * instructions as they vary by IDE version.  See http://vld.codeplex.com/ if interested.
 */
#include "C:\\Program Files (x86)\\Visual Leak Detector\\include\\vld.h"
#pragma comment (lib,"C:\\Program Files (x86)\\Visual Leak Detector\\lib\\Win32\\vld.lib")
int vld_present = 1;
#else
int vld_present = 0;
#endif

#ifndef CMD_HISTSIZE
#define CMD_HISTSIZE (200)
#endif
uint32_t cmd_histsize = CMD_HISTSIZE;

#ifdef USE_LIBEDIT
#include <histedit.h>
EditLine *editline;
History  *edithist;

static char *get_prompt( EditLine *el ) {
    UNUSED(el);

    return ( qstyle_c == '/' ?
             MNAME( MODULE_NAME ) "$> " : MNAME( MODULE_NAME ) "-> " );
}
#else
FILE *histfile = NULL;
#endif

typedef struct atfile {
    FILE *fp;
    uint32_t line;
} atfile_t;

int maxatdepth = MAXATDEPTH;

static void killat( int *atc, void **atfiles );
static int disktypecmp( const void *da, const void *db );
static vmscond_t cmdsplit( char *str );
static vmscond_t addarg( int *argc, void ***argv, void *arg );
static vmscond_t cmdexecute( int argc, char **argv, int qualc, char **qualv );

DECL_CMD(copy);
DECL_CMD(create);
DECL_CMD(delete);
DECL_CMD(diff);
DECL_CMD(dir);
DECL_CMD(dismount);
static DECL_CMD(echo);
DECL_CMD(extend);
DECL_CMD(help);
DECL_CMD(initial);
DECL_CMD(mount);
DECL_CMD(rename);
DECL_CMD(search);
DECL_CMD(set);
DECL_CMD(show);
DECL_CMD(spawn);
DECL_CMD(type);


#ifdef VMS
static char *getcmd( char *inp, size_t max, char *prompt );
#endif

static void add_diskkwds( qualp_t qualset, const char *qname, qualp_t *disks );

/* Qualifier style = vms => /qualifier; else -option
 */

const char *qstyle_s = "/";

int interactive = TRUE;
int verify_cmd  = FALSE;
int error_exit  = FALSE;
int exit_status = EXIT_SUCCESS;

/******************************************************************* Command table */

/* information about the commands we know... */
extern  qual_t copyquals[], createquals[], delquals[], diffquals[],
    dirquals[], dmoquals[], extendquals[], iniquals[], mouquals[],
    renamequals[], searchquals[], typequals[];

extern param_t copypars[], createpars[], delpars[], diffpars[], dirpars[],
    dmopars[], extendpars[], helppars[], inipars[], moupars[],
    renamepars[], searchpars[], setpars[], showpars[], typepars[];

static param_t echopars[] = {
    {"string", REQ | NOLIM, STRING, NOPA, "String(s) to print"},

    {NULL,     0,           0,      NOPA, NULL}
};

extern char spawnhelp[];
CMDSET_t maincmds[] = {
    { "copy",       docopy,     0, copyquals,  copypars,    "commands copy" },
    { "create",     docreate,   0, createquals, createpars, "commands create" },
    { "delete",     dodelete,   0, delquals,   delpars,     "commands delete" },
    { "difference", dodiff,     0, diffquals,  diffpars,    "commands difference" },
    { "directory",  dodir,      0, dirquals,   dirpars,     "commands directory" },
    { "dismount",   dodismount, 0, dmoquals,   dmopars,     "commands dismount" },
    { "echo",       doecho,     0, NULL,       echopars,    "commands echo" },
    { "exit",       NULL,       2, NULL,       NULL,        "commands exit" },
    { "extend",     doextend,   0, NULL,       extendpars,  "commands extend" },
    { "help",       dohelp,     0, NULL,       helppars,    "commands help" },
    { "initialize", doinitial, -4, iniquals,  inipars,      "commands initialize" },
    { "mount",      domount,    0, mouquals,   moupars,     "commands mount" },
    { "quit",       NULL,       2, NULL,       NULL,        "commmands quit" },
    { "rename",     dorename,  -3, renamequals,renamepars,  "commands rename" },
    { "search",     dosearch,   0, searchquals,searchpars,  "commands search" },
    { "set",        doset,      0, NULL,       setpars,     "commands set" },
    { "show",       doshow,     0, NULL,       showpars,    "commands show" },
    { "spawn",      dospawn,    0, NULL,       NULL,        spawnhelp },
    { "type",       dotype,     0, typequals,  typepars,    "commands type" },

    { NULL,         NULL,       0, NULL,       NULL,        NULL } /* ** END MARKER ** */
};


/*********************************************************** main() */

int main( int argc, char **argv ) {
    vmscond_t sts;
    int atc;
    void **atfiles;
    int initfile;
    char *rl = NULL, *buf = NULL;
    size_t bufsize = 80;
    qualp_t disks = NULL;
    char *p, mname[] = { MNAME(MODULE_NAME) };
#ifdef VMS
#ifndef VMS_MAXCMDLEN
#define VMS_MAXCMDLEN 2048
#endif
    char *vmscmdbuf;
#endif
    char *hfname = NULL;
#ifdef USE_LIBEDIT
    struct HistEvent histevt;
#endif

    signal( SIGINT, SIG_IGN );

#ifndef VMS
    {   /* "Standard"s... */
        const char *const loc[] = {
#ifdef PREFERRED_LOCALE
            PREFERRED_LOCALE,
#endif
#ifdef _WIN32
            ".28591",
#else
            "ISO-8859-1",
            "iso-8859-1",
            "iso_8859_1",
            ".iso88591",
            ".ISO88591",
            ".iso8859-1",
            ".iso-8859-1",
            ".iso_8859-1",
            "en_US.ISO-8859-1",
            "en_US.ISO_8859-1",
            "en_US.iso88591",
            "en_US.ISO88591",
#endif
            "C",
            NULL }, *const*p;

        for( p = loc; *p ; p++ ) {
#if DEBUG_LOCALE
            char *q;
            if( (q = setlocale( LC_CTYPE, *p )) )
                printf( "%s => %s [OK]\n", *p, q );
            else
                printf( "%s => [Failed]\n", *p );
#else
            if( setlocale( LC_CTYPE, *p ) != NULL ) {
#ifdef USE_LIBEDIT
                setenv( "LC_CTYPE", *p, 1 );
                unsetenv( "LC_ALL" );
#endif
                break;
            }
#endif
        }
#if !DEBUG_LOCALE
        if( !*p ) {
            int err;

            err = errno;
            printmsg( ODS2_LOCALE, 0 );
            printmsg( $SETLEVEL(ODS2_OSERROR,WARNING), MSG_CONTINUE, ODS2_LOCALE, strerror( err ) );
        }
#endif
    }
#endif

    if( $FAILS( (sts = sethelpfile( NULL, "ODS2HELP", argv[0] )) ) ) {
        printmsg( sts, 0 );
    }

    atc = 0;
    if( (atfiles = (void **) calloc( 1, sizeof( void * ) )) == NULL ) {
        printmsg( ODS2_INITIALFAIL, 0 );
        exit( EXIT_FAILURE );
    }

    for( p = mname; *p; p++ )
        *p = tolower( *p );

#ifdef USE_LIBEDIT
    edithist = history_init();
    editline = el_init(mname,stdin, stdout, stderr);
    el_set( editline, EL_EDITOR, "emacs" );
    el_set( editline, EL_SIGNAL, 1 );
    el_set( editline, EL_PROMPT, get_prompt );
    el_set( editline, EL_HIST, history, edithist );
    history( edithist, &histevt, H_SETSIZE, cmd_histsize );

    el_source( editline, NULL );

    if( (hfname = homefile( 1, mname, NULL)) != NULL ) {
        history( edithist, &histevt, H_LOAD, hfname );
    }
#else
    hfname = homefile( 1, mname, ".history" );
    if( hfname) histfile = openf( hfname, "a+" );
    if( histfile && Chmod( hfname, S_IREAD | S_IWRITE ) )
        perror( "chmod history" );

    /* if it's possible, load previous session history into recall buffers, if any.
     * Haven't found a case where it can be done.
     * In any case, history can be viewed.  Or find a libedit for the host OS...
     */
#endif
    sts = SS$_NORMAL;
    {
        FILE *atfile = NULL;

        p = get_env( "ODS2INIT" );
        if( p != NULL ) {
            if( strcmp( p, "-" ) == 0 ) {
                interactive = FALSE;
            } else {
                char *rp = NULL;
                rp = get_realpath( p );
                atfile = openf( rp? rp : p, "r" );
                if( atfile == NULL ) {
                    int err = errno;
                    printmsg( ODS2_OPENIND, 0, rp? rp : p );
                    printmsg( ODS2_OSERROR, MSG_CONTINUE, ODS2_OPENIND, strerror( err ) );
                } else {
#ifdef USE_LIBEDIT
                    char *fn;
                    size_t len;
                    len = strlen( rp? rp : p ) + sizeof( ";ODS2INIT=" );
                    fn = malloc( len );
                    if( fn != NULL ) {
                        snprintf( fn, len, ";ODS2INIT=%s", rp? rp : p );
                        history( edithist, &histevt, H_ENTER, fn );
                        free( fn );
                    }
#else
                    if( histfile ) fprintf( histfile, ";ODS2INIT=%s\n", rp? rp : p );
#endif
                }
                if( rp != NULL ) free( rp );
            }
            free( p );
        }
        if( (p == NULL) && (atfile == NULL) && (p = homefile( 1, mname, ".ini" )) != NULL ) {
            char *rp = NULL;
            rp = get_realpath( p );
            atfile = openf( rp? rp : p, "r" );
            if( atfile == NULL ) {
                int err = errno;
                if( errno != ENOENT ) {
                    printmsg( ODS2_OPENIND, 0, rp? rp : p );
                    printmsg( ODS2_OSERROR, MSG_CONTINUE, ODS2_OPENIND, strerror( err ) );
                }
            } else {
#ifdef USE_LIBEDIT
                char *fn;
                size_t len;
                len = strlen( rp? rp : p ) + sizeof( ";initfile=" );
                fn = malloc( len );
                if( fn != NULL ) {
                    snprintf( fn, len, ";initfile=%s", rp? rp : p );
                    history( edithist, &histevt, H_ENTER, fn );
                    free( fn );
                }
#else
                if( histfile ) fprintf( histfile, ";initfile=%s\n", rp? rp : p );
#endif
            }
            if( rp != NULL ) free( rp );
            free( p );
        }

        if( (initfile = (atfile != NULL)) ) {
            atfile_t *atp;

            if( (atp = (atfile_t *) malloc( sizeof( atfile_t ) )) == NULL ) {
                fclose( atfile );
                sts = printmsg( ODS2_INITIALFAIL, 0 );
                exit( EXIT_FAILURE );
            }
            atp->fp = atfile;
            atp->line = 0;
            if( $FAILS(sts = addarg( &atc, &atfiles, atp )) ) {
                sts = printmsg( ODS2_INITIALFAIL, 0 );
                exit( EXIT_FAILURE );
            }
        }
    }

    if( $FAILS(sts = sys_initialize()) ) {
        printmsg( ODS2_INITIALFAIL, 0 );
        printmsg( sts, MSG_CONTINUE | MSG_NOARGS, ODS2_INITIALFAIL );
        exit( EXIT_FAILURE );
    }

    add_diskkwds( mouquals, DT_NAME, &disks );
    add_diskkwds( iniquals, DT_NAME, &disks );

#ifdef VMS
    if( (vmscmdbuf = malloc( VMS_MAXCMDLEN )) == NULL ) {
        sts = printmsg( ODS2_INITIALFAIL, 0 );
        exit( EXIT_FAILURE );
    }
#endif

    argc--;
    argv++;

    while( 1 ) {
        char *ptr = NULL;

        if( !initfile && argc > 0 ) {
            size_t n, al = 0;

            rl = NULL;

            for( n = 0; n < (unsigned)argc; n++ )
                if( !strcmp( "$", argv[n] ) )
                    break;
                else
                    al += 1 + strlen( argv[n] );
            if( al > 0 ) {
                size_t i;
                char *cp;

                if( buf == NULL || al > bufsize ) {
                    cp = realloc( buf, al );
                    if( cp == NULL ) {
                        perror( "malloc" );
                        exit( EXIT_FAILURE );
                    }
                    buf = cp;
                    bufsize = al;
                }
                cp =
                    rl = buf;
                for( i = 0; i < n; i++ ) {
                    size_t len;

                    len = strlen( *argv );
                    if( i != 0 )
                        *cp++ = ' ';
                    memcpy( cp, *argv++, len );
                    argc--;
                    cp += len;
                }
                *cp = '\0';

                ptr = rl;
#ifdef USE_LIBEDIT
                if( *ptr && interactive )
                    history( edithist, &histevt, H_ENTER, ptr );
#else
                if( *ptr && interactive && histfile )
                    fprintf( histfile, "%s\n", ptr );
#endif
            }
            if( argc > 0 ) {
                argc--;
                argv++;
            }
        }
        if( ptr == NULL ) {
            if( atc ) {
                atfile_t *atp;

                atp = (atfile_t *)(atfiles[atc - 1]);
                if( (rl = fgetline( atp->fp, FALSE, &buf, &bufsize )) == NULL ) {
                    fclose( atp->fp );
                    free( atfiles[--atc] );
                    atfiles[atc] = NULL;
                    initfile = FALSE;
                    continue;
                } else {
                    ++atp->line;
                    ptr = strchr( rl, '\r' );
                    if( ptr != NULL )
                        *ptr = '\0';

                    if( verify_cmd )
                        printf( "@%u:%u_%c> %s\n", atc, atp->line,
                        (qstyle_c == '/' ? '$' : '-'), rl );
                }
                ptr = rl;
            }
            if( ptr == NULL ) {
#ifdef VMS
                if( getcmd( vmscmdbuf, VMS_MAXCMDLEN,
                    (qstyle_c == '/' ? "$> " : "-> ") ) == NULL )
                    break;
                ptr = vmscmdbuf;
                if( histfile ) fprintf( histfile, "%s\n", ptr );
#else
#ifdef USE_LIBEDIT
                if( interactive ) {
                    int nread;

                    rl = (char *) el_gets( editline, &nread );
                    if( rl == NULL || nread < 1 ) {
                        break;
                    } else {
                        if( rl[nread-1] == '\n' )
                            --nread;
                        if( buf == NULL || (size_t)nread +1 > bufsize ) {
                            ptr = realloc( buf, nread +1 );
                            if( ptr == NULL ) {
                                perror( "malloc" );
                                exit( EXIT_FAILURE );
                            }
                            buf = ptr;
                            bufsize = nread +1;
                        }
                        memcpy( buf, rl, nread );
                        buf[nread] = '\0';
                        ptr = rl = buf;
                        if( *rl != '\0' ) {
                            history( edithist, &histevt, H_ENTER, rl );
                        }
                    }
                } else
#endif
                {
                    if( interactive )
                        printf( "%s%c> ", MNAME(MODULE_NAME), qstyle_c == '/' ? '$' : '-' );
                    if( (rl = fgetline( stdin, FALSE, &buf, &bufsize )) == NULL ) break;
                    ptr = rl;
#ifndef USE_LIBEDIT
                    if( interactive && histfile ) fprintf( histfile, "%s\n", ptr );
#endif
                }
#endif
            }
        }

        while( *ptr == ' ' || *ptr == '\t' )
            ptr++;
        if( !strlen( ptr ) || *ptr == '!' || *ptr == ';' )
            continue;

        if( *ptr == '@' ) {
            FILE *atfile;
            char *rp = NULL;

            if( atc >= maxatdepth ) {
                sts = printmsg( ODS2_MAXIND, 0, maxatdepth );
                argc = 0;
                killat( &atc, atfiles );
                continue;
            }
            rp = get_realpath( ptr + 1 );
            if( (atfile = openf( rp? rp : ptr + 1, "r" )) == NULL ) {
                int err;

                err = errno;
                printmsg( ODS2_OPENIND, 0, rp? rp : ptr + 1 );
                printmsg( ODS2_OSERROR, MSG_CONTINUE, ODS2_OPENIND, strerror( err ) );
                argc = 0;
                killat( &atc, atfiles );
            } else {
                atfile_t *atp;

                if( (atp = (atfile_t *)malloc( sizeof( atfile_t ) )) == NULL ) {
                    int err;

                    err = errno;
                    sts = printmsg( ODS2_OPENIND, 0, rp? rp : ptr + 1 );
                    printmsg( ODS2_OSERROR, MSG_CONTINUE, ODS2_OPENIND, strerror( err ) );
                    fclose( atfile );
                    argc = 0;
                    killat( &atc, atfiles );
                } else {
                    atp->fp = atfile;
                    atp->line = 0;

                    if( $FAILS( sts = addarg( &atc, &atfiles, atp ) ) ) {
                        sts = printmsg( sts, 0 );
                        if( rp != NULL ) free( rp );
                        break;
                    }
                }
            }
            if( rp != NULL ) free( rp );
            continue;
        }
#if DEBUG
        if( !isprint( '\347' & 0xFFul ) ) {
            static int once = 0;
            if( !once++ )
                printf( "***DEBUG: 8-bit characters are not printable, "
                    "probable locale issue - contact maintainer.\n" );
        }
#endif

        sts = cmdsplit( ptr );

        if( sts == ODS2_EXIT )
            break;
        if( $FAILED(sts) ) {
            printmsg( sts, MSG_NOARGS );

            killat( &atc, atfiles );
            initfile = FALSE;
            argc = 0;
            if( error_exit ) {
                exit_status = EXIT_FAILURE;
                break;
            }
        }
    } /* while 1 */

    /* cleanup for exit */

#ifdef USE_LIBEDIT
    if( hfname != NULL ) {
        history( edithist, &histevt, H_SAVE, hfname );
        history_end( edithist );
        el_end( editline );
    }
#else
    if( histfile ) { /* Limit history to cmd_histsize lines */
        int nent, limit;

        limit = cmd_histsize;
        fseek( histfile, 0, SEEK_SET );
        for( nent = 0; ; ) { /* Count history lines */
            int c;

            c = getc( histfile );
            if( c == EOF ) break;
            if( c == '\n' ) ++nent;
        }
        if( nent > limit ) { /* Over the retention limit, delete oldest lines */
            FILE *tmp;
            char *tmpfile;

            if( (tmpfile = malloc( strlen( hfname ) + sizeof( ".tmp" ) )) != NULL ) {
                memcpy( tmpfile, hfname, strlen( hfname ) );
                memcpy( tmpfile+strlen( hfname), ".tmp", sizeof( ".tmp" ) );
                if( (tmp = openf( tmpfile, "w" )) != NULL ) {
                    if( Chmod( tmpfile, S_IREAD | S_IWRITE ) )
                        perror( "chmod history" );

                    limit = nent - limit;
                    fseek( histfile, 0, SEEK_SET );
                    for( nent = 0; nent < limit; ) { /* Skip old lines */
                        int c;

                        c = getc( histfile );
                        if( c == EOF ) break;
                        if( c == '\n' ) ++nent;
                    }
                    for( ; ; nent++ ) {
                        char *line;
                        if( (line = fgetline( histfile, TRUE, &buf, &bufsize  )) == NULL )
                            break;
                        fprintf( tmp, "%s", line );
                    }
                    fclose( tmp );
#ifdef _WIN32
                    fclose( histfile ); /* No atomic rename(), newname must not exist. */
                    histfile = NULL;
                    (void) Unlink( hfname );
#endif
                    if( rename( tmpfile, hfname ) )
                        perror( "rename history" );
                }
                free( tmpfile );
            }
        }
        if( histfile ) fclose( histfile );
    }
#endif
    if( buf != NULL ) free( buf );
    if( hfname != NULL ) free(hfname);

    killat( &atc, atfiles );
    free( atfiles );

    free( disks );

    exit( exit_status );
}

/***************************************************************** killat() */
static void killat( int *atc, void **atfiles ) {
    while( *atc ) {
        atfile_t *atp;

        atp = (atfile_t *)(atfiles[--*atc]);
        fclose( atp->fp );
        free( atp );
        atfiles[*atc] = NULL;
    }
    return;
}

/***************************************************************** add_diskkwds() */
/* Build parser keyword list from disk table entries */

static void add_diskkwds( qualp_t qualset, const char *qname, qualp_t *disks ) {
    qualp_t qp;

    if( *disks == NULL ) {
        disktypep_t  dp;
        size_t n = 0;

        for( dp = disktype; dp->name != NULL; dp++ )
            ;
        n = (size_t) (dp - disktype);
        if( (n << MOU_V_DEVTYPE) & ~MOU_DEVTYPE )
            abort(); /* MOU_DEVTYPE isn't wide enough for the max index */

        *disks = (qualp_t)calloc( n+1,  sizeof( qual_t ) );
        if( *disks == NULL ) {
            perror( "malloc" );
            exit( EXIT_FAILURE );
        }
        /* Do NOT sort 1st element (default, must have search seq 1)
         * or last - list terminator.
         */
        qsort( disktype+1, n - 1, sizeof( disktype_t ),
               disktypecmp );

        for( qp = *disks, dp = disktype; dp->name != NULL; dp++, qp++ ) {
            qp->name = dp->name;
            qp->set = ((options_t)(dp-disktype)) << MOU_V_DEVTYPE;
            qp->clear = MOU_DEVTYPE;
            qp->qtype = NOVAL;
            if( dp != disktype )
                qp->helpstr = "";
        }
        qp->set |= OPT_NOSORT; /* Mark table as non-sortable by help */
    }
    for( qp = qualset; qp->name != NULL; qp++ )
        if( !strcmp( qp->name, qname ) )
            break;
    if( qp->name == NULL )
        abort();

    qp->arg = *disks;

    return;
}

/***************************************************************** disktypecmp() */

static int disktypecmp( const void *da, const void *db ) {

    return strcmp( ((disktypep_t )da)->name,
                   ((disktypep_t )db)->name );
}

/***************************************************************** cmdsplit() */

/* cmdsplit: break a command line into its components */

static vmscond_t cmdsplit( char *str ) {
    vmscond_t sts;
    int argc = 0, qualc = 0;
    char **argv = NULL, **qualv = NULL;
    char *sp = str;
    char q = qstyle_c;

    argv = (char **) calloc( 1, sizeof( char * ) );
    qualv = (char **) calloc( 1, sizeof( char * ) );

    if( argv == NULL || qualv == NULL ) {
        free( argv );
        free( qualv );
        return SS$_INSFMEM;
    }

    sts = SS$_NORMAL;

    while( *sp != '\0' ) {
        while( *sp == ' ' ) sp++;
        if( *sp == '\0' )
            break;

        if( *sp == q ) {                             /* Start of qualifier */
            *sp++ = '\0';                            /* Terminate previous word */
            if (*sp == q) {                          /* qq = end of qualifiers */
                sp++;
                q = '\0';
                continue;
            }
            if( $FAILS(sts = addarg( &qualc, (void ***)&qualv, (void *)sp)) )
                break;
        } else {                                     /* New argument */
            int qt;

            if( $FAILS(sts = addarg( &argc, (void ***)&argv, (void *)sp)) )
                break;
            if( (qt = *sp) == '"' || (qt = *sp) == '\'' ) {
                ++argv[argc-1];
                for( ++sp; *sp && (*sp != qt || sp[1] == qt); sp++ ) {
                    if( *sp == qt )                 /* Interior "" => " */
                        memmove( sp, sp+1, strlen(sp+1)+1 );
                }
                if( *sp == qt ) {                   /* Ending " of string */
                    *sp++ = '\0';
                    if( *sp && *sp != ' ' ) {        /* Something following */
                        sts = printmsg( ODS2_UNTERMSTR, 0 );
                        break;
                    }
                } else {
                    sts = printmsg( ODS2_UNTERMSTR, 0 );
                    break;
                }
                continue;
            } /* Quoted string */
        }

        /* Find end of atom */

        while( !(*sp == '\0' || *sp == ' ' || *sp == q) ) sp++;
        if (*sp == '\0')
            break;
        if( *sp == ' ' )
            *sp++ = '\0';
    }

    if( $SUCCESSFUL(sts) && argc > 0 )
        sts = cmdexecute( argc, argv, qualc, qualv );

    free( argv );
    free( qualv );

    return sts;
}

/***************************************************************** addarg() */
/* Add an item to a dynamic list of things, e.g. argv, qualv,...
 * Updates count.
 */

static vmscond_t addarg( int *argc, void ***argv, void *arg ) {
    void **list;

    list = (void **) realloc( *argv, ((size_t)*argc + 2) * sizeof( void * ) );
    if( list == NULL )
        return SS$_INSFMEM;
    list[*argc] = arg;
    list[++*argc] = NULL;
    *argv = list;
    return SS$_NORMAL;
}

/*************************************************************** cmdexecute() */
/* cmdexecute: identify and execute a command */

static vmscond_t cmdexecute( int argc, char **argv, int qualc, char **qualv ) {
    char *ptr;
    CMDSETp_t cmd, cp = NULL;
    size_t cmdsiz;
    int minpars, maxpars;
    paramp_t p;
    vmscond_t sts;

    ptr = argv[0];
    while (*ptr != '\0') {
        *ptr = tolower(*ptr);
        ptr++;
    }
    ptr = argv[0];
    cmdsiz = strlen(ptr);

    for( cmd = maincmds; cmd->name != NULL; cmd++ ) {
        if( cmdsiz <= strlen( cmd->name ) &&
            keycomp( ptr, cmd->name ) ) {
            if( cmd->uniq ) {
                if( cmd->uniq > 0 && (int) cmdsiz >= cmd->uniq ) {
                    cp = cmd; /* Unique in n */
                    break;
                }
                if( cmd->uniq < 0 && cmdsiz < (size_t) abs( cmd->uniq ) )
                    cp = cmd; /* Requires n even if fewer unique */
            }
            if( cp != NULL ) {
                return printmsg( ODS2_AMBIGUOUS, 0, "command", ptr );
            }
            cp = cmd;
        }
    }
    if( cp == NULL ) {
        return printmsg( ODS2_UNKNOWN, 0, "command", ptr );
    }
    cmd = cp;

    if( cmd->proc == NULL )
        return ODS2_EXIT;

    minpars =
        maxpars = 1;
    for( p = cmd->params; p && p->name != NULL; p++ ) {
        if( ((unsigned)maxpars) != ~0u )
            maxpars++;
        if( p->flags & REQ )
            minpars++;
        if( p->flags & NOLIM )
            maxpars = ~0u;
    }

    if (argc < minpars || (((unsigned)argc) != ~0u &&
                           ((unsigned)argc) > (unsigned)maxpars) ) {
        return printmsg(  (argc < minpars? ODS2_TOOFEW: ODS2_TOOMANY), 0, "parameters" );
    }

    sts = (*cmd->proc) (argc, argv, qualc, qualv);

    cache_flush();

    return sts;
}

/*********************************************************** doecho() */

static DECL_CMD(echo) {
    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    while( argv[1] ) {
        printf( "%s", argv++[1] );
        if( argv[1] )
            putchar( ' ' );
    }
    putchar( '\n' );
    fflush( stdout );

    return SS$_NORMAL;
}

/*********************************************************** parselist() */

vmscond_t parselist( int *nret, char ***items, size_t min, char *arg ) {
    size_t n = 0, i;
    char **list = NULL;

    if( nret != NULL )
        *nret = 0;
    *items = NULL;

    while( arg != NULL && *arg != '\0' ) {
        char **nl;

        while( *arg == ' ' || *arg == '\t' )
            arg++;
        if( *arg == '\0' )
            break;

        nl = (char **) realloc( list, (n < min? min +1: n +2) * sizeof( char * ) );
        if( nl == NULL ) {
            free( list );
            return printmsg( SS$_INSFMEM, 0 );
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
        n = 0;
        list = (char **) malloc( (min + 1) * sizeof( char * ) );
        if( list == NULL ) {
            return printmsg( SS$_INSFMEM, 0 );
        }
    }
    for( i = n; i < min; i++ )
        list[i] = NULL;

#ifdef _MSC_VER
    /* NOT a buffer overflow */
#pragma warning( push )
#pragma warning( disable: 6386 )
#endif
    list[i] = NULL;
#ifdef _MSC_VER
#pragma warning( pop )
#endif
    *items = list;

    if( nret != NULL )
        *nret = (int)n;

    return SS$_NORMAL;
}

/******************************************************************* checkquals() */
/* checkquals: Given valid qualifier definitions, process qualifer
 * list from a command left to right.  Also handles parameters and
 * qualifier values (/Qual=value).  May recurse for keyword values.
 * May not modify caller's arglist as it's reused (e.g. with saved
 * default qualifier lists.)
 */

vmscond_t checkquals( options_t *result, options_t defval,
                      qualp_t qualset, int qualc, char **qualv ) {
    vmscond_t sts;
    int i;
    const char *type;

#ifdef _WIN32 /* Code analysis thinks qualset can be NULL, resulting in a false positive. */
    if( qualset == NULL )
        abort();
#endif

    *result = defval;

    type = (qualc < 0)? "parameter": "qualifier";
    qualc = abs( qualc );

    sts = SS$_NORMAL;

    for( i = 0; i < qualc; i++ ) {
        char *qc;
        static char *topv = NULL;
        static unsigned level = 0;

        do {
            char *qv, *nvp;
            qualp_t qs, qp = NULL;
            int qtype;
            size_t len;

            if( level++ ) {
                qc = qualv[i];
            } else {
                len = strlen( qualv[i] ) + 1;
                if( (qc = malloc( len )) == NULL ) {
                    --level;
                    topv = NULL;
                    return printmsg( SS$_INSFMEM, 0 );
                }
                memcpy( qc, qualv[i], len );
                topv = qualv[i];
            }
            if( (qv = strchr( qc, '=' )) == NULL )
                qv = strchr( qc, ':' );
            if( qv != NULL )
                *qv++ = '\0';
            for( qs = qualset; qs->name != NULL; qs++) {
                int matched;
                if( (matched = keycomp( qc, qs->name ))!= 0 ) {
                    if( matched == (int)strlen( qs->name ) ) {
                        qp = qs;
                        break;
                    }
                    if( qp != NULL ) {
                        sts = printmsg( ODS2_AMBIGUOUS, 0, type, qc );
                        break;
                    }
                    qp = qs;
                }
            }
            if( $FAILED(sts) )
                break;
            if( qp == NULL ) {
                sts = printmsg( ODS2_UNKNOWN, 0, type, qc );
                break;
            }
            *result = (*result & ~qp->clear) | qp->set;
            qtype = qp->qtype;
            if( qv == NULL || *qv == '\0' ) {
                if( qtype > 0 ) {
                    sts = printmsg( ODS2_VALUEREQ, 0, type, qp->name );
                }
                break;
            }
            if( qtype == NOVAL ) {
                sts = printmsg( ODS2_NOVALUE, 0, type, qp->name );
                break;
            }
            qtype = abs( qtype );
            if( qtype == STRVAL ) {
                *((char **)qp->arg) = topv + (qv - qc);
                break;
            }
            if( *qv == '(' ) {
                qv++;
                nvp = strchr( qv, ')' );
                if( nvp == NULL ) {
                    sts = printmsg( ODS2_NOPAREN, 0, type, qc );
                    break;
                }
                *nvp = '\0';
            }
            if( qtype == PROT ) {
                int err = FALSE;
                uint32_t prot = 0;
                unsigned class = 0;

                while( *qv != '\0' && !err ) {
                    nvp = strchr( qv, ':' );
                    if( nvp == NULL ) {
                        nvp = qv + strlen( qv );
                    } else {
                        *nvp++ = '\0';
                    }
                    if( keycomp(qv, "SYSTEM" ) )
                        class = (prot$m_system << 16) | prot$v_system;
                    else if( keycomp(qv, "OWNER" ) )
                        class = (prot$m_owner << 16) | prot$v_owner;
                    else if( keycomp( qv, "GROUP" ) )
                        class = (prot$m_group << 16) | prot$v_group;
                    else if( keycomp( qv, "WORLD" ) )
                        class = (prot$m_world << 16) | prot$v_world;
                    else {
                        err = TRUE;
                        break;
                    }
                    prot |= class & 0xFFFF0000;
                    qv = nvp;
                    nvp = strchr( qv, ',' );
                    if( nvp == NULL )
                        nvp = qv + strlen( qv );
                    else
                        *nvp++ = '\0';
                    while( *qv != '\0' && !err ) {
                        switch( toupper(*qv++) ) {
                        case 'R':
                            prot |= prot$m_noread << (uint16_t)class;
                            break;
                        case 'W':
                            prot |= prot$m_nowrite << (uint16_t)class;
                            break;
                        case 'E':
                            prot |= prot$m_noexe << (uint16_t)class;
                            break;
                        case 'D':
                            prot |= prot$m_nodel << (uint16_t)class;
                            break;
                        default:
                            err = TRUE;
                            break;
                        }
                    }
                    qv = nvp;
                }
                if( err || !(prot & 0xFFFF0000) ) {
                    sts = printmsg( ODS2_BADPRO, 0 );
                    break;
                }
                *((uint32_t *)qp->arg) = ( (prot & 0xFFFF0000) |
                                           (uint16_t)((prot >> 16) & ~prot) );
                break;
            }
            if( qtype == UIC ) {
                uint16_t grp, mem;
                int n;
                if(
#ifdef _WIN32
                   sscanf_s
#else
                   sscanf
#endif
                   (qv, " [%ho,%ho]%n", &grp, &mem, &n ) < 2 ||
                   (size_t)n != strlen(qv) ) {
                    sts = printmsg( ODS2_BADUIC, 0 );
                    break;
                }
                *((uint32_t*)qp->arg) = (grp << 16) | mem;
                break;
            }

            do {
                while( *qv == ' ' )
                    qv++;
                nvp = strchr( qv, ',' );
                if( nvp != NULL )
                    *nvp++ = '\0';
                switch( qtype ) {
                case DECVAL: {
                    unsigned long int val;
                    char *ep;

                    errno = 0;
                    val = strtoul( qv, &ep, 10 );
                    if( !*qv || *ep || errno != 0 ) {
                        sts = printmsg( ODS2_INVNUMBER, 0, qv );
                        break;
                    }
                    *((uint32_t *)qp->arg) = (uint32_t) val;
                    break;
                }
                case KEYVAL:
                case KEYCOL:
                    sts = checkquals( result, *result, (qualp_t )qp->arg, -1, &qv );
                    break;
                default:
                    abort();
                }
                if( $FAILED(sts) )
                    break;
                qv = nvp;
            } while( qv != NULL );
        } while( 0 );
        if( !--level ) {
            free( qc );
            topv = NULL;
        }

        if( $FAILED(sts) )
            return sts;
    }
    return sts;
}

/******************************************************************* keycomp() */
/* keycomp: routine to compare parameter to a keyword - case insensitive
 */

int keycomp(const char *param, const char *keywrd) {
    const char *p;

    for( p = param; *p != '\0'; ) {
        if( tolower(*p++) != tolower(*keywrd++) )
            return 0;
    }
    return (int) (p - param);
}

/*********************************************************** getcmd() */

#ifdef VMS
static char *getcmd( char *inp, size_t max, char *prompt ) {
    struct dsc_descriptor prompt_d = { strlen(prompt), DSC$K_DTYPE_T,
                                       DSC$K_CLASS_S, prompt };
    struct dsc_descriptor input_d = { max -1, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, inp };
    vmscond_t status;
    char *retstat;
    static unsigned long key_table_id = 0;
    static unsigned long keyboard_id = 0;

    if (key_table_id == 0) {
        if( $SUCCESSFUL(status = smg$create_key_table( &key_table_id )) )
            status = smg$create_virtual_keyboard( &keyboard_id );
        if( $FAILED(status) )
            return (NULL);
    }

    status = smg$read_composed_line( &keyboard_id, &key_table_id,
                                     &input_d, &prompt_d, &input_d, 0, 0, 0, 0, 0, 0, 0 );

    if( $MATCHCOND(status, SMG$_EOF) )
        retstat = NULL;
    else {
        inp[input_d.dsc_w_length] = '\0';
        retstat = inp;
    }

    return(retstat);
}
#endif /* VMS */

/*********************************************************** prvmstime() */

vmscond_t prvmstime( FILE *of, VMSTIME vtime, const char *sfx ) {
    vmscond_t sts = 0;
    char tim[24] = { "" };
    static const VMSTIME nil;
    struct dsc_descriptor timdsc;

    memset( &timdsc, 0, sizeof( timdsc ) );
    if( memcmp( vtime, nil, sizeof(nil) ) ) {
        timdsc.dsc_w_length = 23;
        timdsc.dsc_a_pointer = tim;
        if( $FAILS( sts = sys_asctim( 0, &timdsc, vtime, 0 ) ) ) {
            printmsg( ODS2_VMSTIME, MSG_TOFILE, of );
            sts = printmsg( sts, MSG_CONTINUE|MSG_TOFILE, of, ODS2_VMSTIME );
        }
        tim[23] = '\0';
        fprintf( of, "  %s", tim );
    } else {
        fprintf( of, "  %-23s", "     <Not recorded>" );
        sts = SS$_NORMAL;
    }
    if( sfx != NULL )
        fprintf( of, "%s", sfx );
    return sts;
}

/*********************************************************** pwrap() */

void pwrap( FILE *of, int *pos, const char *fmt, ... ) {
    char pbuf[200], *p, *q;
    va_list ap;

    va_start(ap, fmt );
    vsnprintf( pbuf, sizeof(pbuf), fmt, ap );
    va_end(ap);

    for( p = pbuf;  *p; ) {
        int len;
        int eol = 0;

        q = strchr( p, '\n' );
        if( q != NULL ) {
            *q++ = '\0';
            eol = 1;
            len = (int)strlen(p);
        } else {
            len = (int)strlen(p);
            q = p + len;
        }
        if( *pos + len > 80 ) {
            static const char wrap[] = "                    ";
            fprintf( of, "\n%s", wrap );
            *pos = sizeof(wrap) -1;
            if( p[0] == ',' && p[1] == ' ' )
                p += 2;
        }
        *pos += (int)strlen(p);
        fprintf( of, "%s%s", p, eol? "\n":"" );
        if( eol ) *pos = 0;
        p = q;
    }
}
