/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_SHOWCMD )
#define DEBUG DEBUG_SHOWCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

#ifdef USE_READLINE
#ifndef _GNU_SOURCE
#define _XOPEN_SOURCE
#endif
#include <readline/readline.h>
#endif

#include "access.h"
#include "cache.h"
#include "direct.h"
#include "phyio.h"
#ifdef USE_VHD
#include "phyvhd.h"
#endif
#include "phyvirt.h"
#include "version.h"

static unsigned show_stats( void );

void show_version( void );

static char *get_cwd( void );


/******************************************************************* doshow() */

static qual_t showkwds[] = { {"cwd",             0, 0, NV, "Working directory on local system"},
                             {"default",         1, 0, NV, "Default directory on VMS volume"},
                             {"devices",         2, 0, NV, "Devices"},
                             {"qualifier_style", 3, 0, NV, "Qualifier style (Unix, VMS)" },
                             {"statistics",      4, 0, NV, "Debugging statistics"},
                             {"time",            5, 0, NV, "Time"},
                             {"verify",          6, 0, NV, "Command file echo" },
                             {"version",         7, 0, NV, "Version"},
                             {"volumes",         8, 0, NV, "Mounted volume information" },
                             {NULL,              0, 0, NV, NULL }
};
param_t showpars[] = { {"item_name", REQ, KEYWD, PA(showkwds), "" },
                       {NULL,        0,   0,     NOPA,         NULL }
};

/*********************************************************** doshow() */

DECL_CMD(show) {
    int parnum;

    UNUSED(argc);
    UNUSED(qualc);
    UNUSED(qualv);

    parnum = checkquals( 0, showkwds, -1, argv+1 );
    switch( parnum ) {
    default:
        return SS$_BADPARAM;
    case 0: {
        char *cwd;

        cwd = get_cwd();
        if( cwd == NULL ) {
            return SS$_BADPARAM;
        }
        printf( " Current working directory is %s\n", cwd );
        free( cwd );
        return SS$_NORMAL;
    }
    case 1: {
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
    case 2:
        phyio_show( SHOW_DEVICES );
        virt_show( NULL );
        return SS$_NORMAL;
    case 3:
        printf ( "  Qualifier style: %s\n", vms_qual? "/VMS": "-unix" );
        return SS$_NORMAL;
    case 4:
        return show_stats();
    case 5: {
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
    case 6:
        printf( "Command file verification is %s\n", (verify_cmd? "on": "off") );
        return SS$_NORMAL;
    case 7:
        show_version();
        return SS$_NORMAL;
    case 8:
        show_volumes();
        return SS$_NORMAL;
    }
    return SS$_NORMAL;
}


/****************************************************************** show_stats() */

/* dostats: print some simple statistics */

static unsigned show_stats( void ) {
    printf("Statistics:-\n");
    direct_show();
    cache_show();
    phyio_show(SHOW_STATS);

    return SS$_NORMAL;
}

/*********************************************************** show_version() */


void show_version( void ) {
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
    printf( "\n " );
    phyio_show( SHOW_FILE64 );
#ifdef USE_VHD
    putchar( ' ');
    (void) phyvhd_available( TRUE );
#else
    printf( " VHD format image files are not supported\n" );
#endif

    putchar( '\n' );
    return;
}

/*********************************************************** get_cwd() */

static char *get_cwd( void ) {
    size_t size = 32;
    char *buf = NULL;
    while( 1 ) {
        char *nbuf;
        nbuf = (char *)realloc( buf, size );
        if( nbuf == NULL ) {
            free( buf );
            return NULL;
        }
        buf = nbuf;
        if( getcwd( buf, size ) != NULL )
            break;
        if( errno != ERANGE ) {
            perror( "getcwd" );
            return NULL;
        }
        size *= 2;
    }
    return buf;
}
