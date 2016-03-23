/* main() - command parsing and dispatch */

/*
 *      This is part of ODS2 written by Paul Nankervis,
 *      email address:  Paulnank@au1.ibm.com
 *
 *      ODS2 is distributed freely for all members of the
 *      VMS community to use. However all derived works
 *      must maintain comments in their source to acknowledge
 *      the contibution of the original author.
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
 *     See version.h for revision history.
 */

/*  This is the top level set of routines. It is fairly
 *  simple minded asking the user for a command, doing some
 *  primitive command parsing, and then calling a set of routines
 *  to perform whatever function is required (for example COPY).
 *  Some routines are implemented in different ways to test the
 *  underlying routines - for example TYPE is implemented without
 *  a NAM block meaning that it cannot support wildcards...
 *  (sorry! - could be easily fixed though!)
 */

#include "version.h"

#if !defined( DEBUG ) && defined( DEBUG_ODS2 )
#define DEBUG DEBUG_ODS2
#else
#ifndef DEBUG
#define DEBUG 0
#endif
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
#endif /* VMS */

#define DEBUGx on

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
#endif

#ifdef USE_READLINE
#ifndef _GNU_SOURCE
#define _XOPEN_SOURCE
#endif
#include <wordexp.h>
#include <readline/readline.h>
#include <readline/history.h>
#endif


static int disktypecmp( const void *da, const void *db );
static int cmdsplit( char *str );
static int cmdexecute( int argc, char **argv, int qualc, char **qualv );

DECL_CMD(copy);
DECL_CMD(create);
DECL_CMD(delete);
DECL_CMD(diff);
DECL_CMD(dir);
DECL_CMD(dismount);
DECL_CMD(extend);
DECL_CMD(help);
DECL_CMD(import);
DECL_CMD(initial);
DECL_CMD(mount);
DECL_CMD(search);
DECL_CMD(set);
DECL_CMD(show);
DECL_CMD(spawn);
DECL_CMD(type);


#ifdef VMS
static char *getcmd( char *inp, size_t max, char *prompt );
#endif

static void add_diskkwds( qualp_t qualset, const char *qname, qualp_t *disks );

/* Qualifier style = vms = /qualifier; else -option
 */

int vms_qual = 1;
int verify_cmd = 1;

/******************************************************************* Command table */

/* information about the commands we know... */
extern  qual_t copyquals[], delquals[], dirquals[], importquals[],
    iniquals[], mouquals[];

extern param_t copypars[], createpars[], delpars[], diffpars[], dirpars[],
    dmopars[], extendpars[], helppars[], importpars[], inipars[], moupars[],
    searchpars[], setpars[], showpars[], typepars[];

CMDSET_t maincmds[] = {
    { "copy",       docopy,    0,copyquals,  copypars,   "Copy a file from VMS to host file" },
    { "create",     docreate,  0,NULL,       createpars,  NULL },
    { "delete",     dodelete,  0,delquals,   delpars,    "Delete a VMS file" },
    { "difference", dodiff,    0,NULL,       diffpars,   "Compare VMS file to host file" },
    { "directory",  dodir,     0,dirquals,   dirpars,    "List directory of VMS files" },
    { "dismount",   dodismount,0,NULL,       dmopars,    "Dismount a VMS volume" },
    { "exit",       NULL,      2,NULL,       NULL,       "Exit ODS2" },
    { "extend",     doextend,  0,NULL,       extendpars, NULL },
    { "help",       dohelp,    0,NULL,       helppars,   "Obtain help on a command" },
    { "import",     doimport,  0,importquals,importpars, "Copy a file from host to VMS" },
    { "initialize", doinitial,-4, iniquals,  inipars,   "Create a new filesystem" },
    { "mount",      domount,   0,mouquals,   moupars,    "Mount a VMS volume" },
    { "quit",       NULL,      2,NULL,       NULL,       "Exit ODS2" },
    { "search",     dosearch,  0,NULL,       searchpars, "Search VMS file for a string" },
    { "set",        doset,     0,NULL,       setpars,    "Set PARAMETER - set HELP for list" },
    { "show",       doshow,    0,NULL,       showpars,   "Display state" },
    { "spawn",      dospawn,   0,NULL,       NULL,       "Open a command subprocess" },
    { "type",       dotype,    0,NULL,       typepars,   "Display a VMS file on the terminal" },

    { NULL,         NULL,      0,NULL,       NULL, NULL } /* ** END MARKER ** */
};


/*********************************************************** main() */

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

int main( int argc,char **argv ) {
    int sts;
    FILE *atfile = NULL;
    char *rl = NULL;
    qualp_t disks = NULL;

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
    }
#endif
    sts = sys_initialize();
    if( !(sts & STS$M_SUCCESS) ) {
        printf( "Unable to initialize library: %s\n", getmsg( sts, MSG_TEXT ) );
        exit(EXIT_FAILURE);
    }

    add_diskkwds( mouquals, DT_NAME, &disks );
    add_diskkwds( iniquals, DT_NAME, &disks );

    argc--;
    argv++;

    while( 1 ) {
        char *ptr = NULL;

        if( argc > 0 ) {
            size_t n, al = 0;

            free( rl );
            rl = NULL;

            for( n = 0; n < (unsigned)argc; n++ )
                if( !strcmp( "$", argv[n] ) )
                    break;
                else
                    al += 1 + strlen( argv[n] );
            if( al > 0 ) {
                size_t i;
                char *cp;

                cp =
                    rl = (char *) malloc( al );
                if( rl == NULL ) {
                    perror( "malloc" );
                    exit( EXIT_FAILURE );
                }
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
#ifdef USE_READLINE
            if( *ptr )
                add_history( ptr );
#endif
            }
            if( argc > 0 ) {
                argc--;
                argv++;
            }
        }
        if( ptr == NULL ) {
            if (atfile != NULL) {
                free( rl );
                if( (rl = fgetline( atfile, FALSE )) == NULL) {
                    fclose(atfile);
                    atfile = NULL;
                } else {
#ifndef _WIN32
                    ptr = strchr( rl, '\r' );
                    if( ptr != NULL )
                        *ptr = '\0';
#endif
                    if( verify_cmd )
                        printf("%c> %s\n", (vms_qual? '$': '-'), rl );
                }
                ptr = rl;
            }
            if( ptr == NULL ) {
#ifdef VMS
                if( getcmd( str, sizeof(str),
                            (vms_qual? "$> ": "-> ") ) == NULL )
                    break;
                ptr = str;
#else
                free( rl );
#ifdef USE_READLINE
                rl =
                    ptr = readline( vms_qual?
                                    MNAME(MODULE_NAME) "$> ": MNAME(MODULE_NAME) "-> " );
                if (rl == NULL) {
                    break;
                } else {
                    if( *rl != '\0' ) {
                        add_history( rl );
                    }
                }
#else
                printf( "%s", vms_qual? "$> ": "-> " );
                if( (rl = fgetline(stdin, FALSE)) == NULL ) break;
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
                if( (atfile = openf( ptr + 1, "r" )) == NULL ) {
                    perror("%%ODS2-E-INDERR, Failed to open indirect command file");
                    putchar( '\n' );
                    argc = 0;
                }
            }
            continue;
        }

        sts = cmdsplit(ptr);
        if( sts == -1 )
            break;
        if( !(sts & STS$M_SUCCESS) ) {
            if( atfile != NULL ) {
                fclose(atfile);
                atfile = NULL;
            }
            argc = 0;
        }
    } /* while 1 */

    free( rl );

#ifdef USE_READLINE
    if( hfname != NULL ) {
        write_history( hfname );
        clear_history();
    }
    wordfree( &wex );  /* hfname points into wex and should not be free()d */
#endif
    if (atfile != NULL)
        fclose(atfile);

    free( disks );

    exit( EXIT_SUCCESS );
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
            qp->set = OPT_NOSORT | (int)((dp-disktype) << MOU_V_DEVTYPE);
            qp->clear = MOU_DEVTYPE;
            qp->qtype = NOVAL;
            if( dp != disktype )
                qp->helpstr = "";
        }

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

    return strcmp( ((disktypep_t )da)[0].name,
                   ((disktypep_t )db)[0].name );
}

/***************************************************************** cmdsplit() */

/* cmdsplit: break a command line into its components */

/*
 * Feature for Unix: '//' or '--' stops qualifier parsing.
 * This enables us to copy to Unix directories with VMS style /qualifiers.
 *     copy /bin // *.com /tmp/ *
 * is split into argv[0] -> "*.com" argv[1] -> "/tmp/ *" qualv[0]-> "/bin"
 *
 * Of course, one can just "" the string (VMS double quote rules)...
 */
#define MAXITEMS 32
static int cmdsplit( char *str ) {
    int argc = 0,qualc = 0;
    char *argv[MAXITEMS],*qualv[MAXITEMS];
    char *sp = str;
    int i;
    char q = vms_qual? '/': '-';

    for( i = 0; i < MAXITEMS; i++ )
        argv[i] = qualv[i] = "";

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
            if( qualc >= MAXITEMS ) {
                printf( "%%ODS2-E-CMDERR, Too many qualifiers specified\n" );
                return 0;
            }
            qualv[qualc++] = sp;
        } else {                                     /* New argument */
            int qt;

            if( argc >= MAXITEMS ) {
                printf( "%%ODS2-E-CMDERR, Too many arguments specified\n" );
                return 0;
            }
            argv[argc++] = sp;
            if( (qt = *sp) == '"' || (qt = *sp) == '\'' ) {
                ++argv[argc-1];
                for( ++sp; *sp && (*sp != qt || sp[1] == qt); sp++ ) {
                    if( *sp == qt )                 /* Interior "" => " */
                        memmove( sp, sp+1, strlen(sp+1)+1 );
                }
                if( *sp == qt ) {                   /* Ending " of string */
                    *sp++ = '\0';
                    if( *sp && *sp != ' ' ) {        /* Something following */
                        printf( "%%ODS2-E-CMDERR, Unterminated string\n" );
                        return 0;
                    }
                } else {
                    printf( "%%ODS2-E-CMDERR, Unterminated string\n" );
                    return 0;
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

    if( argc > 0 )
        return cmdexecute( argc, argv, qualc, qualv );

    return 1;
}


/*************************************************************** cmdexecute() */
/* cmdexecute: identify and execute a command */

static int cmdexecute( int argc, char **argv, int qualc, char **qualv ) {
    char *ptr;
    CMDSETp_t cmd, cp = NULL;
    unsigned cmdsiz;
    int minpars, maxpars;
    paramp_t p;
    int sts;

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

    if( cmd->proc == NULL )
        return -1;

    minpars =
        maxpars = 1;
    for( p = cmd->params; p && p->name != NULL; p++ ) {
        maxpars++;
        if( p->flags == REQ )
            minpars++;
    }

    if (argc < minpars || argc > maxpars) {
        printf( "%%ODS2-E-PARAMS, Too %s parameters specified\n",
                (argc < minpars? "few": "many") );
        return 0;
    }
    sts = (*cmd->proc) (argc,argv,qualc,qualv);

    cache_flush();

    return sts;
}

/*********************************************************** parselist() */

int parselist( char ***items, size_t min, char *arg, const char *label ) {
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
        n = 0;
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

/******************************************************************* checkquals() */
/* checkquals: Given valid qualifier definitions, process qualifer
 * list from a command left to right.  Also handles parameters and
 * qualifier values (/Qual=value).
 */

int checkquals(int result, qualp_t qualset,int qualc,char **qualv) {
    int i;
    const char *type;

#ifdef _WIN32 /* Code analysis thinks qualset can be NULL, resulting in a false positive. */
    if( qualset == NULL )
        abort();
#endif

    type = (qualc < 0)? "parameter": "qualifier";
    qualc = abs( qualc );

    for( i = 0; i < qualc; i++ ) {
        char *qv;
        qualp_t qs, qp = NULL;

        qv = strchr( qualv[i], '=' );
        if( qv == NULL )
            qv = strchr( qualv[i], ':' );
        if( qv != NULL )
            *qv++ = '\0';
        for( qs = qualset; qs->name != NULL; qs++) {
            if( keycomp( qualv[i], qs->name ) ) {
                if( qp != NULL ) {
                    printf ( "%%ODS2-W-AMBQUAL, %c%s '%s' is ambiguous\n",
                             type[0], type+1, qualv[i] );
                    return -1;
                }
                qp = qs;
            }
        }
        if( qp == NULL ) {
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
            if( qp->qtype == STRVAL ) {
                *((char **)qp->arg) = qv;
                continue;
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
                case DECVAL: {
                    unsigned long int val;
                    char *ep;

                    errno = 0;
                    val = strtoul( qv, &ep, 10 );
                    if( !*qv || *ep || errno != 0 ) {
                        printf( "%%ODS2-BADVAL, %s is not a valid number\n", qv );
                        return -1;
                    }
                    *((unsigned *)qp->arg) = (unsigned)val;
                    break;
                }
                case KEYVAL:
                case KEYCOL:
                    result = checkquals( result, (qualp_t )qp->arg, -1, &qv );
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

/******************************************************************* keycomp() */
/* keycomp: routine to compare parameter to a keyword - case insensitive! */

int keycomp(const char *param, const char *keywrd) {
    while (*param != '\0') {
        if( tolower(*param++) != tolower(*keywrd++) )
            return 0;
    }
    return 1;
}

/******************************************************************* fgetline() */
/* Read a line of input - unlimited length
 * Removes \n, returns NULL at EOF
 * Caller responsible for free()
 */
char *fgetline( FILE *stream, int keepnl ) {
    size_t bufsize = 0,
           xpnsize = 80,
           idx = 0;
    char *buf = NULL;
    int c;

    if( (buf = malloc( (bufsize = xpnsize) )) == NULL ) {
        perror( "malloc" );
        abort();
    }

    while( (c = fgetc( stream )) != EOF && c != '\n' ) {
        if( idx + (keepnl != 0) +2 > bufsize ) { /* Now + char + (? \n) + \0 */
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
    buf[idx] = '\0';
    return buf;
}

/*********************************************************** getcmd() */

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
        if( status & STS$M_SUCCESS )
            status = smg$create_virtual_keyboard( &keyboard_id );
        if( !(status & STS$M_SUCCESS) )
            return (NULL);
    }

    status = smg$read_composed_line( &keyboard_id, &key_table_id,
                    &input_d, &prompt_d, &input_d, 0,0,0,0,0,0,0 );

    if( status == SMG$_EOF )
        retstat = NULL;
    else {
        inp[input_d.dsc_w_length] = '\0';
        retstat = inp;
    }

    return(retstat);
}
#endif /* VMS */

/*********************************************************** prvmstime() */

int prvmstime(VMSTIME vtime, const char *sfx) {
    int sts = 0;
    char tim[24];
    static const VMSTIME nil;
    struct dsc_descriptor timdsc;

    if( memcmp( vtime, nil, sizeof(nil) ) ) {
        timdsc.dsc_w_length = 23;
        timdsc.dsc_a_pointer = tim;
        sts = sys_asctim(0,&timdsc,vtime,0);
        if( !(sts & STS$M_SUCCESS) )
            printf("%%ODS2-W-TIMERR, SYS$ASCTIM error: %s\n",getmsg(sts, MSG_TEXT));
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

/*********************************************************** pwrap() */

void pwrap( int *pos, const char *fmt, ... ) {
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
