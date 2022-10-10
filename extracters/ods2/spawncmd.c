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

#ifdef VMS
char spawnhelp[] = { "commands spawnvms" };

DECL_CMD(spawn)  {
    vmscond_t sts;

    UNUSED( argc );
    UNUSED( argv );
    UNUSED( qualc );
    UNUSED( qualv );

    /* command, input, output, flags, process_name, pid, exit_sts, efn,
     * astaddr, astprm, prompt, cli, table
     */
    if( $FAILS(sts = lib$spawn( 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )) ) {
        printmsg( ODS2_NOSHELL, 0, "command interpreter" );
        sts = printmsg( sts, MSG_CONTINUE, ODS2_NOSHELL );
    }
    return sts;
}
#elif defined( _WIN32 )
char spawnhelp[] = { "commands spawnnt" };

DECL_CMD(spawn)  {
    UNUSED( argc );
    UNUSED( argv );
    UNUSED( qualc );
    UNUSED( qualv );

    if( system( "cmd" ) == -1 ) {
        int err;

        err = errno;
        printmsg( ODS2_NOSHELL, 0, "cmd" );
        return printmsg( ODS2_OSERROR, MSG_CONTINUE, strerror( err ) );
    }
    return SS$_NORMAL;
}
#else
char spawnhelp[] = { "commands spawnunix" };

DECL_CMD(spawn)  {
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
        int err;

        execlp( shell, p, (char *)NULL );

        err = errno;
        printmsg( ODS2_NOSHELL, 0, shell );
        printmsg( ODS2_OSERROR, MSG_CONTINUE, strerror( err ) );
        exit(EXIT_FAILURE);
    }
    if( pid == -1 ) {
        int err = errno;

        printmsg( ODS2_NOSHELL, 0, shell );
        return printmsg( ODS2_OSERROR, MSG_CONTINUE, strerror( err ) );
    }
    waitpid( pid, NULL, 0 );

    return SS$_NORMAL;
}
#endif

