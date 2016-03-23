/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_HELPCMD )
#define DEBUG DEBUG_HELPCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

extern void mounthelp(void);

extern CMDSET_t maincmds[];
extern void show_version( void );

static void cmdhelp( CMDSETp_t cmdset );
static void parhelp( CMDSETp_t cmd, int argc, char **argv );
static void qualhelp( int par, qualp_t qtable );

static int cmdcmp( const void *qa, const void *qb );
static int qualcmp( const void *qa, const void *qb );

/******************************************************************* dohelp() */

/* help: Display help guided by command table. */

param_t helppars[] = { {"command",   OPT, CMDNAM, PA(maincmds),       "" },
                       {"parameter", OPT, STRING, NOPA,               "" },
                       {"value",     OPT, STRING, NOPA,               "" },
                       {NULL,        0,   0,      NOPA,               NULL },
};

DECL_CMD(help) {
    CMDSETp_t cmd;

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
        cmdhelp( maincmds );
        printf( "\n Type HELP COMMAND for information on any command.\n" );
        return 1;
    }

    for( cmd = maincmds; cmd->name; cmd++ ) {
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
            if( strcmp(cmd->name, "mount") == 0 )
                mounthelp();
            return 1;
        }
    }
    printf( "%s: command not found\n", argv[1] );
    return 0;
}

/*********************************************************** cmdhelp() */

static void cmdhelp( CMDSETp_t cmdset ) {
    CMDSETp_t cmd;
    int n = 0;
    size_t max = 0;

    for( cmd = cmdset; cmd->name != NULL; cmd++ ) {
        if( strlen(cmd->name) > max )
            max = strlen(cmd->name);
    }
    qsort( cmdset, (size_t)(cmd - cmdset), sizeof( CMDSET_t ), cmdcmp );

    for( cmd = cmdset; cmd->name; cmd++ ) {
        if( cmd->helpstr == NULL )
            continue;
        if( n++ % 4 == 0 )
            printf( "\n" );
        printf("    %-*s", (int)max,cmd->name );
    }
    printf( "\n" );
}

/*********************************************************** parhelp() */

static void parhelp( CMDSETp_t cmd, int argc, char **argv ) {
    paramp_t p;
    paramp_t ptable;
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
            if( p->helpstr ) {
                size_t len = strlen(p->name);
                if( len > max )
                    max = len;
            }
        }
        for( p = ptable; p->name != NULL; p++ ) {
            if( p->helpstr ) {
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
        printf( "\n\n  Type help %s PARAMETER for more about each parameter.\n",
                cmd->name );
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
        qualhelp( 1, (qualp_t )p->arg );
        break;
    case LIST:
        printf( "list, %s\n", p->helpstr );
        break;
    case STRING:
        printf( "%s\n", p->helpstr );
        break;
    case CMDNAM:
        printf( "command name, one of the following:\n");
        cmdhelp( (CMDSETp_t )p->arg );
        break;
    default:
        abort();
    }
    if( footer )
        printf( "%s", footer );

    return;
}

/*********************************************************** qualhelp() */

static void qualhelp( int par, qualp_t qtable ) {
    qualp_t q;
    int n = 0;
    size_t max = 0, col = 4;

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
                ++len;
            if( q->qtype == DECVAL )
                ++len;
            if( len > max )
                max = len;
        }
    }

    if( !(q->set & OPT_NOSORT) ) {
        qsort( qtable, (size_t)(q - qtable), sizeof( qual_t ), qualcmp );
    }

    for( q = qtable; q->name; q++ ) {
        if( q->helpstr ) {
            if( !n++ && !par )
                printf( "  %s:\n", "Qualifiers"  );
            printf("    %s", par? par < 0? "   ": "": vms_qual? "/" : "-" );
            if( *q->helpstr == '-' )
                switch( q->qtype ) {
                case NOVAL:
                    if( *q->helpstr ) {
                        printf( NOSTR "%-*s - %s\n",
                                (int) (max-NOSTR_LEN), q->name, q->helpstr+1 );
                        break;
                    }
                    if( col + max > 50 ) {
                        printf( "\n        " );
                        col = 8;
                    } else {
                        while( col < 8 ) {
                            putchar( ' ' ); col++;
                        }
                    }
                    printf( "%-*s", (int) max, q->name );
                    col += max;
                    break;
                case KEYVAL:
                case KEYCOL:
                    printf( NOSTR "%s=%-*s - %s\n",
                            q->name, (int) (max-(NOSTR_LEN+strlen(q->name)+1)), "",
                            q->helpstr+1 );
                    qualhelp( q->qtype == KEYCOL? 1 : -(int)max, (qualp_t )q->arg );
                    break;
                case DECVAL:
                    printf( NOSTR "%s=n%-*s - %s\n",
                            q->name, (int) (max-(NOSTR_LEN+strlen(q->name)+2)), "",
                            q->helpstr+1 );
                    break;
                case STRVAL:
                    printf( NOSTR "%s=%-*s - %s\n",
                            q->name, (int) (max-(NOSTR_LEN+strlen(q->name)+2)), "",
                            q->helpstr+1 );
                    break;
                default:
                    abort();
                }
            else
                switch( q->qtype ) {
                case NOVAL:
                    if( *q->helpstr ) {
                        printf("%-*s - %s\n", (int) max, q->name, q->helpstr );
                        break;
                    }
                    if( col + max > 50 ) {
                        printf( "\n        " );
                        col = 8;
                    } else {
                        while( col < 8 ) {
                            putchar( ' ' ); col++;
                        }
                    }
                    printf( "%-*s", (int) max, q->name );
                    col += max;
                    break;
                case KEYVAL:
                case KEYCOL:
                    printf( "%s=%-*s - %s\n", q->name, (int)(max-(strlen(q->name)+1)), "",
                            q->helpstr );
                    qualhelp( q->qtype == KEYCOL? 1 : -(int)max, (qualp_t )q->arg );
                    break;
                case DECVAL:
                    printf( "%s=n%-*s - %s\n",
                            q->name, (int) (max-(strlen(q->name)+2)), "",
                            q->helpstr );
                    break;
                case STRVAL:
                    printf( "%s=%-*s - %s\n",
                            q->name, (int) (max-(strlen(q->name)+1)), "",
                            q->helpstr );
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

/***************************************************************** cmdcmp() */

static int cmdcmp( const void *qa, const void *qb ){

    return strcmp( ((CMDSETp_t )qa)[0].name,
                   ((CMDSETp_t )qb)[0].name );
}

/***************************************************************** qualcmp() */

static int qualcmp( const void *qa, const void *qb ) {

    return strcmp( ((qualp_t )qa)[0].name,
                   ((qualp_t )qb)[0].name );
}
