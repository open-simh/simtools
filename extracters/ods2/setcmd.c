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

#if !defined( DEBUG ) && defined( DEBUG_SETCMD )
#define DEBUG DEBUG_SETCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

#ifdef USE_LIBEDIT
#include <histedit.h>
extern EditLine *editline;
extern History  *edithist;
#endif

static hlpfunc_t sethelp;

static vmscond_t setcwd( char *newdef );
static vmscond_t setdef( char *newdef );

static vmscond_t savequals( options_t defval, qual_t *qp,
                            int qualc, char **qualv,
                            defqualsp_t *saveloc );
static void set_exit( void );


/******************************************************************* doset() */

#define set_msgdef OPT_GENERIC_1

/* Indexes must match in doset() & sethelp() */

static uint32_t histmax;

static
qual_t setkwds[] = { {"cwd",                  0, 0, NV,
                      "commands set item_name item_cwd"},
                     {"directory_qualifiers", 1, 0, NV,
                      "commands set item_name item_dirquals"},
                     {"copy_qualifiers",      2, 0, NV,
                      "commands set item_name item_copyquals" },
                     {"default",              3, 0, NV,
                      "commands set item_name item_setdef"},
                     {"error_exit",           4, 0, NV,
                      "-commands set item_name item_seterrxit"},
                     {"noerror_exit",         5, 0, NV, NULL },
                     {"message",              6, 0, NV,
                      "commands set item_name item_setmsg"},
                     {"qualifier_style",      7, 0, NV,
                      "commands set item_name item_qualstyle"},
                     {"search_qualifiers",    8, 0, NV,
                      "commands set item_name item_searchquals"},
                     {"verify",               9, 0, NV,
                      "-commands set item_name item_verify"},
                     {"noverify",            10, 0, NV, NULL },
#ifdef USE_LIBEDIT
                     {"key_bindings",        11, 0, NV,
                      "commands set item_name item_key_bindings"},
#endif
                     {"history_limit",       12, 0, DV(&histmax),
                      "commands set item_name item_history_limit"},
                     {"help_file",           13, 0, NV,
                      "commands set item_name item_helpfile"},

                     {NULL,                   0, 0, NV, NULL }
};

static
qual_t setmsgkwdsp[] = {{"facility",         MSG_FACILITY,   0,            NV,
                         "commands set item_name item_msgfac"},
                        {"nofacility",       0,              MSG_FACILITY, NV, NULL},
                        {"severity",         MSG_SEVERITY,   0,            NV,
                         "commands set item_name item_msgsev"},
                        {"noseverity",       0,              MSG_SEVERITY, NV, NULL},
                        {"identification",   MSG_NAME,       0,            NV,
                         "commands set item_name item_msgid"},
                        {"noidentification", 0,              MSG_NAME,     NV, NULL},
                        {"text",             MSG_TEXT,       0,            NV,
                         "commands set item_name item_msgtext"},
                        {"notext",           0,              MSG_TEXT,     NV, NULL},
                        {"default",          set_msgdef,     0,            NV,
                         "commands set item_name item_default"},

                        {NULL,               0,              0,            NV, NULL }
};

static
qual_t setqskwds[] = {{"unix", 1, 0, NV, "commands set value value_unix"},
                      {"dcl",  2, 0, NV, "commands set value value_dcl"},
                      {"vms",  2, 0, NV, NULL },

                      {NULL,   0, 0, NV, NULL }
};
param_t setpars[] = { {"item_name", REQ, KEYWD, PA(setkwds),         "" },
                      {"value"    , CND, KEYWD, sethelp, setqskwds,  "" },

                      {NULL,        0,   0,      NOPA,               NULL },
};

/*********************************************************** doset() */

extern qual_t copyquals[];
extern const int copy_defaults;
extern defqualsp_t copy_defopt;

extern qual_t dirquals[];
extern const int dir_defaults;
extern defqualsp_t dir_defopt;

extern qual_t searchquals[];
extern const int search_defaults;
extern defqualsp_t search_defopt;

extern uint32_t cmd_histsize;

DECL_CMD(set) {
    options_t parnum;
    vmscond_t sts;

    if( $FAILS(sts = checkquals( &parnum, 0, setkwds, -1, argv+1 )) ) {
        return sts;
    }
    switch( parnum ) {
    default:
        return SS$_BADPARAM;
    case 0: /* default */
    case 3: /* current working directory */
        if( qualc )
            return printmsg( ODS2_NOQUALSOK, 0 );
        return (parnum == 0)? setcwd( argv[2] ) : setdef( argv[2] );
    case 1:{ /* directory_qualifiers */
        if( qualc == 0 )
            return printmsg( ODS2_NOQUALS, 0 );
        return savequals( dir_defaults, dirquals,
                          qualc, qualv, &dir_defopt );
    }
    case 2:{ /* copy_qualifiers */
        if( qualc == 0 )
            return printmsg( ODS2_NOQUALS, 0 );
        return savequals( copy_defaults, copyquals,
                          qualc, qualv, &copy_defopt );
    }
    case 4:
        if( qualc )
            return printmsg( ODS2_NOQUALSOK, 0 );
        error_exit = TRUE;
        return SS$_NORMAL;
    case 5:
        if( qualc )
            return printmsg( ODS2_NOQUALSOK, 0 );
        error_exit = FALSE;
        return SS$_NORMAL;
    case 6: { /* message file */
        options_t options;

        if( qualc != 0 ) {
            if( $FAILS(sts = checkquals( &options, get_message_format(),
                       setmsgkwdsp, qualc, qualv )) )
                return sts;
            if( argc > 2 )
                return printmsg( ODS2_NOPAROK, 0 );
            if( options & set_msgdef ) {
                if( (options & ~set_msgdef) != get_message_format() )
                    return printmsg( ODS2_INCONQUAL, 0 );
                return set_message_file( NULL );
            } 
            if( $FAILS(sts = set_message_format( options )) )
                return printmsg( sts, 0 );
            return SS$_NORMAL;
        }
        if( argc > 3 )
            return printmsg( ODS2_TOOMANY, 0, "parameters" );
        if( argc < 3 )
            return printmsg( ODS2_TOOFEW, 0, "parameters" );
       if( $FAILS(sts = set_message_file( argv[2] )) )
            return printmsg(sts, 0 );
        return SS$_NORMAL;
    }
    case 7: { /* qualifier_style */
        options_t par;

        if( qualc )
            return printmsg( ODS2_NOQUALSOK, 0 );
        if( $FAILS(sts = checkquals( &par, 0, setqskwds, 1, argv+2 )) ) {
            return sts;
        }
        switch(par) {
        default:
            abort();
        case 2:
            qstyle_s = "/";
            break;
        case 1:
            qstyle_s = "-";
            break;
        }
        return SS$_NORMAL;
    }
    case 8:{ /* search_qualifiers */
        if( qualc == 0 )
            return printmsg( ODS2_NOQUALS, 0 );
        return savequals( search_defaults, searchquals,
                          qualc, qualv, &search_defopt );
    }
    case 9:
        if( qualc )
            return printmsg( ODS2_NOQUALSOK, 0 );
        verify_cmd = TRUE;
        return SS$_NORMAL;
    case 10:
        if( qualc )
            return printmsg( ODS2_NOQUALSOK, 0 );
        verify_cmd = FALSE;
        return SS$_NORMAL;
#ifdef USE_LIBEDIT
    case 11: {
        char *av[20];
        int i, ac;

        ac = argc -2;
        for( i = 0; i < 20; i++ ) {
            if( i < ac )
                av[i] = argv[i+2];
            else
                av[i] = NULL;
        }

        el_set( editline, EL_BIND,
                av[0],  av[1],  av[2],  av[3],  av[4],  av[5],  av[6],  av[7],
                av[8],  av[9],  av[10], av[11], av[12], av[13], av[14], av[15],
                av[16], av[17], av[18], NULL );

        return SS$_NORMAL;
    }
#endif
    case 12: {
#ifdef USE_LIBEDIT
        HistEvent ev;

        if( qualc )
            return printmsg( ODS2_NOQUALSOK, 0 );
        if( histmax < 1 )
            return printmsg( HELP_SETCMDHISTVAL, 0 );
        if( history( edithist, &ev, H_SETSIZE, histmax ) >= 0 ) {
            cmd_histsize = histmax;
        }
#else
        if( qualc )
            return printmsg( ODS2_NOQUALSOK, 0 );
        if( histmax < 1 )
            return printmsg( HELP_SETCMDHISTVAL, 0 );
        cmd_histsize = histmax;
#endif
        return SS$_NORMAL;
    }
    case 13:
        if( qualc )
            return printmsg( ODS2_NOQUALSOK, 0 );
        if( argc > 3 )
            return printmsg( ODS2_TOOMANY, 0, "parameters" );
        if( argc < 3 )
            return printmsg( ODS2_TOOFEW, 0, "parameters" );
        if( $FAILS(sts = sethelpfile( argv[2], NULL, NULL )) )
            return printmsg( sts, 0 );
        return sts;
    } /* switch */
}

/************************************************************ sethelp() */

static const char *sethelp( CMDSETp_t cmd, paramp_t p,
                            int argc, char **argv ) {
    options_t par;
    vmscond_t sts;

    UNUSED( cmd );
    UNUSED( argc );

    if( argc == 1 ) {
        p->helpstr = "";
        p->ptype = KEYWD;
        p->arg = setkwds;
        return getmsg( HELP_SETVALUE, MSG_TEXT );
    }

    if( $FAILS(sts = checkquals( &par, 0, setkwds, -1, argv+1 )) ) {
        return NULL;
    }

    switch( par ) {
    case 0:
        p->helpstr = "commands set item_name type_cwd";
        p->ptype = STRING;
        break;
    case 1:
        p->helpstr = "commands set item_name type_dirquals";
        p->ptype = QUALS;
        p->arg = dirquals;
        break;
    case 2:
        p->helpstr = "commands set item_name type_copyquals";
        p->ptype = QUALS;
        p->arg = copyquals;
        break;
    case 3:
        p->helpstr = "commands set item_name type_setdef";
        p->ptype = FSPEC;
        break;
    case 4:
    case 5:
        p->helpstr = "commands set item_name type_seterrxit";
        p->ptype = NONE;
        break;
    case 6:
        p->helpstr = "commands set item_name type_setmsg";
        p->ptype = QUALS;
        p->arg = setmsgkwdsp;
        break;
    case 7:
        p->helpstr = "commands set item_name type_qualstyle";
        p->ptype = KEYWD;
        p->arg = setqskwds;
        break;
    case 8:
        p->helpstr = "commands set item_name type_searchquals";
        p->ptype = QUALS;
        p->arg = searchquals;
        break;
    case 9:
    case 10:
        p->helpstr = "commands set item_name item_verify";
        p->ptype = NONE;
        break;
#ifdef USE_LIBEDIT
    case 11:
        p->helpstr = "commands set item_name item_key_bindings";
        p->ptype = STRING;
        break;
#endif
    case 12:
        p->helpstr = "commands set item_name item_history_limit";
        p->ptype = STRING;
        break;
    case 13:
        p->helpstr = "commands set item_name item_helpfile";
        p->ptype = NONE;
        break;
    default:
        fprintf( stderr, "Missing help key for param %d\n", par );
        abort();
    }
    return NULL;
}

/*********************************************************** setcwd() */

static vmscond_t setcwd( char *newdef ) {
   if( chdir( newdef ) != 0 ) {
       int err;

       err = errno;
       printmsg( ODS2_SETCWD, 0 );
       return printmsg( ODS2_OSERROR, MSG_CONTINUE, ODS2_SETCWD,
                        strerror( err ) );
   }
   return SS$_NORMAL;
}

/*********************************************************** setdef() */

static int default_set = FALSE;

static vmscond_t setdef( char *newdef ) {
    register vmscond_t sts;
    struct dsc_descriptor defdsc = { 0, 0, 0, NULL };

    defdsc.dsc_a_pointer = (char *) newdef;
    defdsc.dsc_w_length = (unsigned short)strlen( defdsc.dsc_a_pointer );
    if( $SUCCESSFUL(sts = sys_setddir( &defdsc, NULL, NULL )) ) {
        default_set = TRUE;
    } else {
        sts = printmsg( sts, 0 );
    }
    return sts;
}

/*********************************************************** mountdef() */

vmscond_t mountdef( const char *devnam ) {
    char *colon, *buf;
    size_t len;

    if( default_set )
        return SS$_NORMAL;

    len = strlen( devnam );
    buf = (char *) malloc( len  + sizeof( ":[000000]" ) );
    if( buf == NULL ) {
        return printmsg( ODS2_OSERROR, 0, strerror( errno ) );
    }

    colon = strchr( devnam, ':' );
    if( colon != NULL )
        len = (size_t)(colon - devnam);
    memcpy( buf, devnam, len );
    memcpy( buf+len, ":[000000]", sizeof( ":[000000]" ) );
    setdef( buf );
    free( buf );

    return SS$_NORMAL;
}

/************************************************************ savequals() */

static defqualsp_t savedquals = NULL;

static vmscond_t savequals( options_t defval, qual_t *qp,
                            int qualc, char **qualv,
                            defqualsp_t *saveloc ) {
    vmscond_t sts;
    int i;
    size_t len;
    defqualsp_t newq, *old;
    char *dp;

    len = ( offsetof( defquals_t, data ) +
            ( sizeof( char** ) * ((size_t)qualc +1) ) );

    for( i = 0; i < qualc; i++ )
        len += strlen( qualv[i] ) + 1;

    if( (newq = (defqualsp_t) malloc( len )) == NULL )
        return SS$_INSFMEM;

    newq->qualc = qualc;
    newq->qualv = newq->data;

    dp = (char *)(newq->data + qualc + 1);

    for( i = 0; i < qualc; i++ ) {
        newq->qualv[i] = dp;
        len = strlen( qualv[i] ) +1;
        memcpy( dp, qualv[i], len );
        dp += len;
    }
    newq->qualv[i] = NULL;

    if( $FAILS(sts = checkquals( &defval, defval, qp, qualc, qualv )) ) {
        free( newq );
        return sts;
    }

    for( old = &savedquals; *old != NULL; old = &(*old)->next ) {
        defqualsp_t t;

        if( (t = *old) == *saveloc ) {
            *old = t->next;
            free( t );
            break;
        }
    }

    newq->next = savedquals;

    if( savedquals == NULL )
        atexit( set_exit );

    *saveloc =
        savedquals = newq;

    return SS$_NORMAL;
}

/************************************************************ set_exit() */
static void set_exit( void ) {
    defqualsp_t sq;

    while( (sq = savedquals) != NULL ) {
        savedquals = sq->next;
        free( sq );
    }

    return;
}

