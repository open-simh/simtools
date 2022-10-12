/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com
 *
 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors.   This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
 */

#if !defined( DEBUG ) && defined( DEBUG_SHOWCMD )
#define DEBUG DEBUG_SHOWCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

#ifdef USE_LIBEDIT
#include <histedit.h>
#else
extern FILE *histfile;
#endif

#include "access.h"
#include "cache.h"
#include "direct.h"
#include "f11def.h"
#include "phyio.h"
#ifdef USE_VHD
#include "phyvhd.h"
#endif
#include "phyvirt.h"
#include "version.h"

extern defqualsp_t copy_defopt;
extern qual_t copyquals[];

extern defqualsp_t dir_defopt;
extern qual_t dirquals[];

extern defqualsp_t search_defopt;
extern qual_t searchquals[];

static unsigned show_defq( defqualsp_t defs,
                           qualp_t qualset, const char *name );

static unsigned show_stats( void );

static vmscond_t show_history( int qualc, char **qualv );

void show_version( int full );

static char *get_cwd( void );


/******************************************************************* doshow() */

#define hist_max OPT_SHARED_20
static uint32_t histmax;
static
qual_t histquals[] = { {"max-entries",    hist_max, 0,  DV(&histmax),
                        "commands history qual_maxent"},
                       {NULL, 0, 0, NV, NULL }
};

static
qual_t showkwds[] = { {"cwd",             0, ~0u, NV,
                       "commands show copy"},
                      {"default",         1, ~0u, NV,
                       "commands show default"},
                      {"devices",         2, ~0u, NV,
                       "commands show devices"},
                      {"qualifier_style", 3, ~0u, NV,
                       "commands show qstyle" },
                      {"statistics",      4, ~0u, NV,
                       "commands show statistics"},
                      {"terminal",        5, ~0u, NV,
                       "commands show terminal"},
                      {"time",            6, ~0u, NV,
                       "commands show time"},
                      {"verify",          7, ~0u, NV,
                       "commands show verify" },
                      {"version",         8, ~0u, NV,
                       "commands show version"},
                      {"volumes",         9, ~0u, NV,
                       "commands show volumes"},
                      {"directory_qualifiers",
                                         10, ~0u, NV,
                       "commands show directory"},
                      {"copy_qualifiers",
                                         11, ~0u, NV,
                       "commands show copy"},
                      {"message",        12, ~0u, NV,
                       "commands show message"},
                      {"search_qualifiers",
                                         13, ~0u, NV,
                       "commands show search"},
                      {"history",        14, ~0u, NV,
                       "commands show history"},
                      {"help_file",      15, ~0u, NV,
                       "commands show helpfile"},

                      {NULL,              0, 0, NV, NULL }
};
param_t showpars[] = { {"item_name", REQ, KEYWD, PA(showkwds), "" },
                       {"selector", OPT|NOLIM, STRING, NOPA, NULL},

                       {NULL,        0,   0,     NOPA,         NULL }
};

/*********************************************************** doshow() */

DECL_CMD(show) {
    options_t parnum;
    vmscond_t sts;

    UNUSED(qualc);
    UNUSED(qualv);

   if( $FAILS(sts = checkquals( &parnum, 0, showkwds, -1, argv+1 )) ) {
        return sts;
    }

    switch( parnum ) {
    default:
        return SS$_BADPARAM;
    case 0: {
        char *cwd;

        cwd = get_cwd();
        if( cwd == NULL ) {
            return SS$_BADPARAM;
        }
        printmsg( HELP_SHOWCWD, MSG_TEXT, cwd );
        free( cwd );
        return SS$_NORMAL;
    }
    case 1: {
        unsigned short curlen;
        char curdir[NAM$C_MAXRSS + 1] = { "" };
        struct dsc_descriptor curdsc;
        struct FAB fab;
        struct NAM nam;

        memset( &curdsc, 0, sizeof( curdsc ) );
        curdsc.dsc_w_length = NAM$C_MAXRSS;
        curdsc.dsc_a_pointer = curdir;

        if( $FAILS(sts = sys_setddir( NULL, &curlen, &curdsc )) ) {
            return printmsg( sts, 0 );
        }

        fab = cc$rms_fab;
        nam = cc$rms_nam;

        nam.nam$b_ess = sizeof(curdir) -1;
        nam.nam$l_esa = curdir;
        fab.fab$l_nam = &nam;
        fab.fab$l_fna = curdir;
        fab.fab$b_fns = (uint8_t)curlen;

        if( $FAILS(sts = sys_parse(&fab)) ) {
            curdir[curlen] = '\0';
            sts = printmsg( ODS2_INVDEF, 0, curdir );
        } else {
            curdir[nam.nam$b_dev+nam.nam$b_dir] = '\0';
            printf( "%s\n", curdir );
        }
        fab.fab$b_fns =
            nam.nam$b_ess =
            fab.fab$b_dns = 0;
        nam.nam$b_nop = NAM$M_SYNCHK;
        nam.nam$l_rlf = NULL;
        (void) sys_parse( &fab );                    /* Release context */
        return sts;
    }
    case 2:
        phyio_show( SHOW_DEVICES );
        virt_show();
        return SS$_NORMAL;
    case 3:
        return printmsg( HELP_SHOWQSTYLE, MSG_TEXT, (qstyle_c == '/')? "/DCL": "-unix" );
    case 4:
        return show_stats();
    case 5: {
        int twid, th;
        termchar( &twid, &th );
        return printmsg( HELP_TERMSIZE, MSG_TEXT, twid, th );
    }
    case 6: {
        char timstr[24] = { "" };
        unsigned short timlen;
        struct dsc_descriptor timdsc;

        memset( &timdsc, 0, sizeof( timdsc ) );
        timdsc.dsc_w_length = 20;
        timdsc.dsc_a_pointer = timstr;
        if( $FAILS(sts = sys_asctim( &timlen, &timdsc, NULL, 0 )) ) {
            printmsg(ODS2_VMSTIME, 0 );
            return printmsg( sts, MSG_CONTINUE, ODS2_VMSTIME );
        }
        timstr[timlen] = '\0';
        printf("  %s\n",timstr);
        return SS$_NORMAL;
    }
    case 7:
        return printmsg( verify_cmd? HELP_SHOWVERON: HELP_SHOWVEROFF, MSG_TEXT );
    case 8:
        show_version( 1 );
        return SS$_NORMAL;
    case 9:
        show_volumes();
        return SS$_NORMAL;
    case 10:
        return show_defq( dir_defopt, dirquals,
                          "directory" );
    case 11:
        return show_defq( copy_defopt, copyquals, "copy" );
    case 12:
        return show_message( argc, argv );
    case 13:
        return show_defq( search_defopt, searchquals, "search" );
    case 14:
        return show_history( qualc, qualv );
    case 15:
        return showhelpfile();
    }
    return SS$_NORMAL;
}

/************************************************************ show_defq() */

static unsigned show_defq( defqualsp_t defs,
                           qualp_t qualset, const char *name ) {
    int i;

    printmsg( HELP_SHOWDEFQ, MSG_TEXT, name );

    if( defs == NULL || defs->qualc == 0 ) {
        printmsg( HELP_SHOWNODEFQ, MSG_TEXT );
        return SS$_NORMAL;
    }

    for( i = 0; i < defs->qualc; i++ ) {
        qualp_t qp;
        char *qk, *qv;
        size_t len;

        len = strlen( defs->qualv[i] ) +1;
        if( (qk = malloc( len )) == NULL )
            return SS$_INSFMEM;
        memcpy( qk, defs->qualv[i], len );
        if( (qv = strchr( qk, '=' )) == NULL )
            qv = strchr( qk, ':');
        if( qv != NULL )
            *qv++ = '\0';

        for( qp = qualset; qp->name != NULL; qp++ ) {
            if( keycomp( qk, qp->name ) ) {
                printf( "      %c%s", qstyle_c, qp->name );
                if( qp->qtype != NOVAL && qv != NULL && *qv )
                    printf( "=%s", qv );
                putchar( '\n' );
                break;
            }
        }
        free( qk );
    }

    return SS$_NORMAL;
}

/****************************************************************** show_stats() */

/* dostats: print some simple statistics */

static unsigned show_stats( void ) {
    printmsg( HELP_SHOWSTATS, MSG_TEXT );
    direct_show();
    cache_show();
    phyio_show(SHOW_STATS);

    return SS$_NORMAL;
}

/*********************************************************** show_history() */
static vmscond_t show_history( int qualc, char **qualv ) {
    vmscond_t sts;
    options_t options;
#ifdef USE_LIBEDIT
    extern History  *edithist;
    HistEvent ev;
#else
    extern uint32_t cmd_histsize;
    uint32_t excess = 0;
    char *buf = NULL;
    size_t bufsize = 80;
#endif
    int nent;

    if( $FAILS(sts = checkquals( &options, 0, histquals, qualc, qualv )) ) {
        return sts;
    }
    sts = SS$_NORMAL;

#ifdef USE_LIBEDIT
    history( edithist, &ev, H_GETSIZE );
    nent = ev.num;
    if( nent < 0 ) {
        fprintf( stderr, "GETSIZE error (%d) %s\n", ev.num, ev.str );
        return sts;
    }
    if( !(options & hist_max) || histmax > (uint32_t)nent ) {
        histmax = nent;
    }
    if( histmax == 0 )
        return printmsg( HELP_SHOWNOCMDHIST, 0 );
    if( history( edithist, &ev, H_FIRST ) < 0 ) { /* Most recent cmd */
        fprintf( stderr, "FIRST error (%d) %s\n", ev.num, ev.str );
        return sts;
    }
    for( nent = 1; (uint32_t)nent < histmax; nent++ ) { /* Find MAXth older cmd */
        if( history( edithist, &ev, H_NEXT ) < 0 ) {
            fprintf( stderr, "NEXT error (%d) %s\n", ev.num, ev.str );
            return sts;
        }
    }
    for( nent = 0; (uint32_t)nent < histmax; nent++ ) {
        printmsg( HELP_SHOWCMDHISTLN, MSG_TEXT, ev.num, ev.str );
        history( edithist, &ev, H_PREV );
    }
    return sts;
#else
    if( histfile == NULL ) {
        printmsg( HELP_SHOWNOCMDHIST, 0 );
        return sts;
    }
    fseek( histfile, 0, SEEK_SET );
    for( nent = 0; ; ) { /* Count history lines */
        int c;

        c = getc( histfile );
        if( c == EOF ) break;
        if( c == '\n' ) ++nent;
    }
    if( !(options & hist_max) || histmax > (uint32_t)nent ) {
        histmax = nent;
    }
    if( histmax == 0 )
        return printmsg( HELP_SHOWNOCMDHIST, 0 );
    if( (uint32_t)nent > cmd_histsize )  /* Hide excess file lines if size reduced */
        excess = nent - cmd_histsize;
    if( histmax > (uint32_t)nent - excess )
        histmax = (uint32_t)nent - excess; /* Reduce request to visible window */
    histmax = nent - histmax;
    fseek( histfile, 0, SEEK_SET );
    for( nent = 0; (uint32_t)nent < histmax; ) { /* Skip old lines */
        int c;

        c = getc( histfile );
        if( c == EOF ) break;
        if( c == '\n' ) ++nent;
    }
    for( ; ; nent++ ) { /* Get full lines for display */
        char *line;

        if( (line = fgetline( histfile, FALSE, &buf, &bufsize )) == NULL )
            break;
        printmsg( HELP_SHOWCMDHISTLN, MSG_TEXT, nent + 1 - excess, line );
    }
    if( buf != NULL ) free( buf );
    fseek( histfile, 0, SEEK_END );
    return sts;
#endif
}

/*********************************************************** show_version() */

void show_version( int full ) {
    char *detail, *version;
#ifdef DEBUG_BUILD
    extern int vld_present;
#endif

#ifdef USE_LIBEDIT
    printmsg( HELP_SHOWVERSRL, MSG_TEXT, MNAME(MODULE_NAME), MODULE_IDENT,
              __DATE__, __TIME__, LIBEDIT_MAJOR, LIBEDIT_MINOR );
#else
    printmsg( HELP_SHOWVERS, MSG_TEXT, MNAME(MODULE_NAME), MODULE_IDENT,
              __DATE__, __TIME__ );
#endif

    if( !full ) return;

    /* Determine what platform and compiler were targeted for this image */

#ifdef _MSC_VER
#define strcat( _dst, _src ) strcat_s( (_dst), MAXSTRL, (_src) )
#endif
#define MAXSTRL 512

    if( (detail = malloc( MAXSTRL * 2 )) == NULL ) {
        return;
    }
    detail[0] = '\0';
    version = detail + MAXSTRL;
    version[0] = '\0';

    /* N.B. The order of detection is intended to deal with ambiguities,
     * and does not inidcate any particular preference or judgement.
     *
     * The detection scheme is an amalgam of tests from various sources.
     * It has not been run in all these environments by the author,
     * (or perhaps by anyone), so errors may exist and should be reported.
     * Additions are also welcome.
     */

#if defined __ANDROID__
    strcat( detail, "Android" );
#elif defined __linux__
    strcat( detail, "Linux" );
#elif defined __APPLE__
  #include "TargetConditionals.h"
  #if TARGET_IPHONE_SIMULATOR
    strcat( detail, "iOS Simulator" );
  #elif TARGET_OS_IPHONE
    strcat( detail, "iOS device" );
  #elif TARGET_OS_MAC
    strcat( detail, "OS X" );
  #else
    strcat( detail, "Unknown Apple platform" );
  #endif
#elif defined __CYGWIN__ && !defined _WIN32
    strcat( detail, "Cygwin" );
#elif defined _WIN64
    strcat( detail, "Windows (64-bit)" );
#elif defined _WIN32
    strcat( detail, "Windows (32-bit)" );
#elif defined VMS || defined __VMS
  #if defined __ia64__ || defined __ia64 || defined _M_IA64
    strcat( detail, "VMS (Itanium)" );
  #elif defined __ALPHA || defined __ALPHA_AXP
    strcat( detail, "VMS (Alpha)" );
  #elif defined VAX || defined __VAX
    strcat( detail, "VMS (VAX)" );
  #else
    strcat( detail, "VMS" );
  #endif
  #if defined __VMS_VER
    snprintf( detail + strlen( detail ), MAXSTRL - strlen( detail ),
              "%c%u.%u-%u%c",
             (__VMS_VER % 100u),
             (__VMS_VER / 10000000u),
             (__VMS_VER / 100000u) % 100u,
             (__VMS_VER / 10000u) % 10u,
             (__VMS_VER / 100u) % 100u );
  #endif
#elif defined __osf__
    strcat( detail, "DEC OSF/1 (Tru64)" );
#elif defined ultrix || defined __ultrix || defined __ultrix__
    strcat( detail, "Ultrix" );
#elif defined UNICOS
    strcat( detail, "UNICOS" );
#elif defined _CRAY || defined __crayx1
    strcat( detail, "UNICOSD/mp" );
#elif defined _AIX
  #if defined __64BIT__
    strcat( detail, "AIX (64-bit)" );
  #else
    strcat( detail, "AIX" );
  #endif
#elif defined(__sun) && defined(__SVR4)
    strcat( detail, "Solaris" );
#elif defined __hpux
    strcat( detail, "HP-UX" );
#elif defined __NetBSD__
    strcat( detail, "NetBSD" );
#elif defined __OpenBSD__
    strcat( detail, "OpenBSD" );

    /* Generic */
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
  #include <sys/param.h>
  #if defined(BSD)
    strcat( detail, "BSD Unix" );
  #endif
#elif defined_POSIX_VERSION
    strcat( detail, "POSIX" );
  #if(   _POSIX_VERSION >= 200809L )
    strcat( detail, ".1-2008" );
  #elif( _POSIX_VERSION >= 200112L )
    strcat( detail, ".1-2001" );
  #elif( _POSIX_VERSION >= 199506L )
    strcat( detail, ".1-1995" );
  #elif( _POSIX_VERSION >= 199309L )
    strcat( detail, ".1-1993" );
  #elif( _POSIX_VERSION >= 199009L )
    strcat( detail,".1-1990" );
  #elif( _POSIX_VERSION >= 198808L )
    strcat( detail,".1-1988" );
  #endif
#else
    strcat( detail, "Unknown platform" );
#endif

    /* Hardware */

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
  #if defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || \
      defined(__64BIT__) || defined(_LP64) || defined(__LP64__)
    strcat( detail, " PowerPC (64-bit)" );
  #else
    strcat( detail, " PowerPC (32-bit)" );
  #endif
#elif defined(__ia64) || defined(__itanium__) || defined(_M_IA64)
    #if !defined VMS && !defined __VMS
      strcat( detail, " Itanium " );
    #endif
#elif defined(__sparc)
      strcat( detail, " SPARC" );
#elif defined(__x86_64__) || defined(_M_X64)
      strcat( detail, " x86-64" );
#elif defined(__i386) || defined(_M_IX86)
      strcat( detail, " x86-32" );
#elif defined __alpha || defined __alpha || defined _M_ALPHA
  #if !defined VMS
      strcat( detail, " Alpha" );
  #endif
#elif defined VAX || defined __VAX
      strcat( detail, " VAX" );
#elif defined __aarch64__
      strcat( detail, " ARM64" );
#elif defined __thumb__
      strcat( detail, " ARM (Thumb)" );
#elif defined __arm__
      strcat( detail, " ARM " )
#elif defined __hppa__ || defined __HPPA__ || defined _hppa
      strcat( detail, " HP/PA RISC" );
#elif defined __m68k__ || defined M68000 || defined __MC68K__
      strcat( detail, " Motorola 68000" );
#elif defined __mips__ || defined __mips
      strcat( detail, " MIPS" );
#elif defined __THW_RS6000__
    strcat( detail, " RS6000" );
#elif defined __370__ || defined __THW_370__ || defined __s390__ || defined __s390x || \
      defined __zarch__ || defined __SYSC_ZARCH__
    strcat( detail, " System Z (370)" );
#endif

#if defined __STDC_VERSION__
  #if( __STDC_VERSION__ >= 201112L )
    strcat( detail, " C11" );
  #elif( __STDC_VERSION__ >= 199901L )
    strcat( detail, " C99" );
  #elif( __STDC_VERSION__ >= 199409L )
    strcat( detail, " C94" );
  #else
    strcat( detail, " C89/90" );
  #endif
#elif defined __STDC__
    strcat( detail, " C89/90" );
#else
    strcat( detail, " C" );
#endif

#if defined __LSB_VERSION__
    snprintf( detail + strlen( detail ), MAXSTRL - strlen( detail ),
              " LSB V%u.%u",
              (__LSB_VERSION__ / 10), (__LSB_VERSION__%10) );
#endif

#if ODS2_BIG_ENDIAN
    strcat( detail, " Big-Endian" );
#endif
    printmsg( HELP_SHOWVERPLATFM, MSG_TEXT, detail );

    /* Compiler */

    detail[0] = '\0';
#if defined __clang__
    strcat( detail, "Clang/LLVM" );
    snprintf( version, MAXSTRL, "%d.%d.%d",
              __clang_major__, __clang_minor__, __clang_patchlevel__ );
#elif defined(__ICC) || defined(__INTEL_COMPILER)
    strcat( detail, "ICC" );
    #if defined __VERSION__
        strcat( version, __VERSION__ );
    #endif
#elif defined(__HP_cc) || defined(__HP_aCC)
    strcat( detail, "HP C" );
    #if defined __HP_aCC && __HP_aCC > 1
        snprintf( version, MAXSTRL, "%02u.%02u.%02u",
                 (__HP_aCC / 10000u ),
                 (__HP_aCC / 100u ) % 100u,
                 (__HP_aCC % 100u ) % 100u );
    #elif defined __HP_cc && __HP_cc > 1
        snprintf( version, MAXSTRL, "%02u.%02u.%02u",
                 (__HP_cc / 10000u ),
                 (__HP_cc / 100u ) % 100u,
                 (__HP_cc % 100u ) % 100u );
    #endif
#elif defined(__IBMC__) || defined(__IBMCPP__)
    strcat( detail, "IBM XL C" );
    #if defined __COMPILER_VER__
        strcat( version, __COMPILER_VER__ );
    #elif defined __xlxc__
        strcat( version, __xlc__ );
    #else
        strcat( version, __IBMC__ );
    #endif
#elif defined __LCC__
    strcat( detail, "LCC" );
#elif defined ___llvm
    strcat( detail, "LLVM" );
#elif defined __MINGW64__
    strcat( detail, "MinGW-w64" );
    #if defined __MINGW64_VERSION_MAJOR
        snprintf( version, MAXSTRL, "%u.%u",
                  __MINGW64_VERSION_MAJOR, __MINGW64_VERSION_MINOR );
    #endif
#elif defined __MINGW32__
    strcat( detail, "MinGW" );
    #if defined __MINGW32_MAJOR_VERSIONR
        snprintf( version, MAXSTRL, "%u.%u",
                  __MINGW32_MAJOR_VERSION, __MINGW32_MINOR_VERSION );
    #endif
#elif defined(__PGI)
    strcat( detail, "PGCC" );
    snprintf( version, MAXSTRL, "%u.%u.%u",
              __PGIC__, __PGIC_MINOR, __PGIC_PATCHLEVEL__ );
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
    strcat( detail, "Solaris Studio" );
    #if defined __SUNPRO_C
        snprintf( version, MAXSTRL, "%u.%u.%u",
                 (__SUNPRO_C / 0x100 ),
                 (__SUNPRO_C / 0x10 ) % 0x100,
                 (__SUNPRO_C % 0x10 ) );
    #elif defined __SUNPRO_CC
        snprintf( version, MAXSTRL, "%u.%u.%u",
                 (__SUNPRO_CC / 0x100 ),
                 (__SUNPRO_CC / 0x10 ) % 0x100,
                 (__SUNPRO_CC % 0x10 ) );
    #endif
#elif defined __sgi || defined sgi
    strcat( detail, "MIPSpro" );
    #if defined _SGI_COMPILER_VERSION
        snprintf( version, MAXSTRL, "%u.%u.%u",
                 (_SGI_COMPILER_VERSION / 100u),
                 (_SGI_COMPILER_VERSION / 10u) % 10u,
                 (_SGI_COMPILER_VERSION % 10u) );
    #elif defined _COMPILER_VERSION
        snprintf( version, MAXSTRL, "%u.%u.%u",
                 (_COMPILER_VERSION / 100u),
                 (_COMPILER_VERSION / 10u) % 10u,
                 (_COMPILER_VERSION % 10u) );
    #endif
#elif defined _MSC_FULL_VER
    strcat( detail, "Visual C++" );
    snprintf( version, MAXSTRL, "%d.%02d.%05d.%02d",
              (_MSC_FULL_VER / 10000000u),
              (_MSC_FULL_VER / 100000u ) % 100u,
              (_MSC_FULL_VER % 100000u ),
              _MSC_BUILD);
    #ifdef _MSC_VER
    { /* https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros */
        struct {
            char *ide; unsigned ver; } ides[] = {
            {"Visual Studio 6.0", 1200},
            {"Visual Studio .NET 2002 (7.0)",           1300},
            {"Visual Studio .NET 2003 (7.1)",           1310},
            {"Visual Studio 2005 (8.0)",                1400},
            {"Visual Studio 2008 (9.0)",                1500},
            {"Visual Studio 2010 (10.0)",               1600},
            {"Visual Studio 2012 (11.0)",               1700},
            {"Visual Studio 2013 (12.0)",               1800},
            {"Visual Studio 2015 (14.0)",               1900},
            {"Visual Studio 2017 RTW (15.0)",           1910},
            {"Visual Studio 2017 version 15.3",         1911},
            {"Visual Studio 2017 version 15.5",         1912},
            {"Visual Studio 2017 version 15.6",         1913},
            {"Visual Studio 2017 version 15.7",         1914},
            {"Visual Studio 2017 version 15.8",         1915},
            {"Visual Studio 2017 version 15.9",         1916},
            {"Visual Studio 2019 RTW (16.0)",           1920},
            {"Visual Studio 2019 version 16.1",         1921},
            {"Visual Studio 2019 version 16.2",         1922},
            {"Visual Studio 2019 version 16.3",         1923},
            {"Visual Studio 2019 version 16.4",         1924},
            {"Visual Studio 2019 version 16.5",         1925},
            {"Visual Studio 2019 version 16.6",         1926},
            {"Visual Studio 2019 version 16.7",         1927},
            {"Visual Studio 2019 version 16.8, 16.9",   1928},
            {"Visual Studio 2019 version 16.10, 16.11", 1929},
            {"Visual Studio 2022 RTW (17.0)",           1930},
            {"Visual Studio 2022 version 17.1",         1931},
            {"Visual Studio 2022 version 17.2",         1932},
            {"Visual Studio 2022 version 17.3",         1933},
            {"Visual Studio 2022+", 100000}, }, *ip;
        for( ip = ides; ip->ver < _MSC_VER; ip++ )
            ;
        strcat( version, ", " );
        strcat( version, ip->ide );
    }
    #endif
#elif defined _MSC_VER
    strcat( detail, "Visual Studio" );
    snprintf( version, MAXSTRL, "%d", _MSC_VER );
#elif defined __CC_ARM
    strcat( version, "ARM Compiler" );
    snprintf( version, MAXSTRL, "%u.%u.%u build %u",
             (__ARMCC_VERSION / 100000u ),
             (__ARMCC_VERSION / 10000u ) % 10u,
             (__ARMCC_VERSION / 1000u ) % 10u,
             (__ARMCC_VERSION % 1000u ) );
#elif defined __GNUC__
    strcat( detail, "gcc" );
    #if defined __VERSION__
        strcat( version, __VERSION__ );
    #else
        snprintf( version, MAXSTRL, "%d.%d", __GNUC__, __GNUC_MINOR__ );
    #endif
#elif defined __DECC__ || defined _DECC
    strcat( detail, "DEC C" );
    {
        char ct;
        switch( (__DECC_VER / 10000u) % 10u ) {
        case 6:
            ct = 'T'; break;
        case 8:
            ct = 'S'; break;
        case 9:
            ct = 'V'; break;
        default:
            ct = 'X'; break;
        }
        snprintf( version, MAXSTRL, "%c%u.%u-%03u", ct,
                 (__DECC_VER / 10000000u),
                 (__DECC_VER / 100000u) % 100u,
                 (__DECC_VER % 10000u) );
    }
#elif defined VAXC || defined VAX11C || defined __VAXC || defined __VAX11C
    strcat( detail, "VAX C" );
#endif
    if( !version[0] )
        strcat( version, "unknown" );
#undef strcat
#undef MAXSTRL

    printmsg( HELP_SHOWVERCOMPLR, MSG_TEXT, detail, version );

    free( detail ); /* includes version */
    detail = version = NULL;

#ifdef DEBUG_BUILD
    if( vld_present )
        printmsg( HELP_SHOWVLD, MSG_TEXT );
#endif
#ifdef USE_WNASPI32
    printmsg( HELP_SHOWVERSCSI, MSG_TEXT );
#endif
    putchar( ' ');
    phyio_show( SHOW_FILE64 );
#ifdef USE_VHD
    putchar( ' ');
    (void) phyvhd_available( TRUE );
#else
    printmsg( HELP_SHOWVERNOVHD, MSG_TEXT );
#endif
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
            printmsg( ODS2_OSERROR, 0, strerror( errno ) );
            return NULL;
        }
        size *= 2;
    }
    return buf;
}
