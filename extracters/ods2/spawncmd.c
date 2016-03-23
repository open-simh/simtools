/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_SPAWNCMD )
#define DEBUG DEBUG_SPAWNCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

#if !defined( _WIN32 ) && !defined( VMS )
#include <sys/wait.h>
#else
# ifdef _WIN32
# include <process.h>
# endif
# ifdef VMS
# include <starlet.h>
# endif
#endif


/******************************************************************* dospawn() */

DECL_CMD(spawn)  {
#ifdef VMS
    unsigned sts;

    UNUSED( argc );
    UNUSED( argv );
    UNUSED( qualc );
    UNUSED( qualv );

    sts = lib$spawn( 0,0,0,0,0,0,0,0,0,0,0,0,0 );
    return sts;
#else
# ifdef _WIN32
    UNUSED( argc );
    UNUSED( argv );
    UNUSED( qualc );
    UNUSED( qualv );

    if( system( "cmd" ) == -1 ) {
        perror( "cmd" );
        return SS$_NOSUCHFILE;
    }
    return SS$_NORMAL;
# else
    char *shell, *p;
    pid_t pid;

    UNUSED( argc );
    UNUSED( argv );
    UNUSED( qualc );
    UNUSED( qualv );

    if( (shell = getenv( "SHELL" )) == NULL )
        shell = "/bin/sh";
    if( (p = strrchr( shell, '/')) == NULL )
        p = shell;
    else
        p++;

    if( (pid = fork()) == 0 ) {
        execlp( shell, p, (char *)NULL );
        perror( "%s" );
        exit(EXIT_FAILURE);
    }
    if( pid == -1 ) {
        perror( shell );
        return SS$_NOSUCHFILE;
    }
    waitpid( pid, NULL, 0 );

    return SS$_NORMAL;
# endif
#endif

}

