/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_SETCMD )
#define DEBUG DEBUG_SETCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

static hlpfunc_t sethelp;

static unsigned setcwd( char *newdef );
static unsigned setdef( char *newdef );


/******************************************************************* doset() */

static qual_t setkwds[] = { {"cwd",                  0, 0, NV, "Working directory on local system"},
                                 {"directory_qualifiers", 1, 0, NV, "Default qualifiers for DIRECTORY command" },
                                 {"default",              2, 0, NV, "Default directory on VMS volume"},
                                 {"qualifier_style",      3, 0, NV, "Qualifier style (Unix, VMS)"},
                                 {"verify",               4, 0, NV, "-Display commands in indirect files"},
                                 {"noverify",             5, 0, NV, NULL },
                                 {NULL,                   0, 0, NV, NULL }
};
static qual_t setqskwds[] = {{"unix", 1, 0, NV, "Unix style options, '-option'"},
                                  {"vms",  2, 0, NV, "VMS style qualifiers, '/qualifier'"},
                                  {NULL,   0, 0, NV, NULL }
};
param_t setpars[] = { {"item_name", REQ, KEYWD, PA(setkwds),         "" },
                      {"value"    , CND, KEYWD, sethelp, setqskwds,  "" },
                      {NULL,        0,   0,      NOPA,               NULL },
};

/*********************************************************** doset() */

extern qual_t dirquals[];
extern const int dir_defaults;
extern int dir_defopt;

DECL_CMD(set) {
    int parnum;

    UNUSED(argc);

    parnum = checkquals( 0, setkwds, -1, argv+1 );
    switch( parnum ) {
    default:
        return SS$_BADPARAM;
    case 0: /* default */
    case 2: /* current working directory */
        if( qualc ) {
            printf( "%%ODS2-E-NOQUAL, No qualifiers are permitted\n" );
            return 0;
        }
        return (parnum == 0)? setcwd( argv[2] ) : setdef( argv[2] );
    case 1:{ /* directory_qualifiers */
        int options = checkquals(dir_defaults,dirquals,qualc,qualv);
        if( options == -1 )
            return SS$_BADPARAM;
        dir_defopt = options;
        return 1;
    }
    case 3: { /* qualifier_style */
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
    case 4:
        verify_cmd = 1;
        return 1;
    case 5:
        verify_cmd = 0;
        return 1;
    }
}

/************************************************************ sethelp() */

static const char * sethelp( CMDSETp_t cmd, paramp_t p, int argc, char **argv ) {
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
        p->helpstr = "working directory for local files";
        p->ptype = STRING;
        break;
    case 1:
        p->helpstr = "default directory on volume";
        p->ptype = VMSFS;
        break;
    case 2:
        p->helpstr = "directory qualifier name ";
        p->ptype = KEYWD;
        p->arg = dirquals;
        break;
    case 3:
        p->helpstr = "style ";
        p->ptype = KEYWD;
        p->arg = setqskwds;
        break;
    case 4:
    case 5:
        p->ptype = NONE;
        break;

    default:
        abort();
    }
    return NULL;
}

/*********************************************************** setcwd() */

static unsigned setcwd( char *newdef ) {
   if( chdir( newdef ) != 0 ) {
        printf( "%%ODS2-W-SETDEF, Error %s setting cwd to %s\n",
                strerror( errno ), newdef );
        return SS$_BADPARAM;
    }
    return SS$_NORMAL;
}

/*********************************************************** setdef() */

static int default_set = FALSE;

static unsigned setdef( char *newdef ) {
    register unsigned sts;
    struct dsc_descriptor defdsc;

    defdsc.dsc_a_pointer = (char *) newdef;
    defdsc.dsc_w_length = (unsigned short)strlen( defdsc.dsc_a_pointer );
    sts = sys_setddir( &defdsc, NULL, NULL );
    if ( sts & STS$M_SUCCESS ) {
        default_set = TRUE;
    } else {
        printf( "%%ODS2-E-SETDEF, Error %s setting default to %s\n", getmsg(sts, MSG_TEXT), newdef );
    }
    return sts;
}

/*********************************************************** mountdef() */

int mountdef( const char *devnam ) {
    char *colon, *buf;
    size_t len;

    if( default_set )
        return SS$_NORMAL;

    len = strlen( devnam );
    buf = (char *) malloc( len  + sizeof( ":[000000]" ));
    if( buf == NULL ) {
        perror( "malloc" );
        return SS$_INSFMEM;
    }

    colon = strchr( devnam, ':' );
    if( colon != NULL )
        len = (size_t)(colon - devnam);
    memcpy( buf, devnam, len );
    memcpy( buf+len, ":[000000]", sizeof( ":[000000]" ) );
    setdef(buf);
    free( buf );

    return SS$_NORMAL;
}
