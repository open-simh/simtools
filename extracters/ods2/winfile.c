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

/* Copyright (C) 2016 Timothe Litt, litt at acm dot org.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <windows.h>

#include "winfile.h"

typedef struct globlist_ {
    struct globlist_ *next;
    char path[1];
} globlist_;

/******************************************************************* addfile() */
static int addfile( int flags, glob_t *pglob, const char *path ) {
    size_t n, ofs;
    char ** nl;
    globlist_ *fp;

    ofs = pglob->gl_pathc;
    if( flags & GLOB_DOOFFS )
        ofs += pglob->gl_offs;

    if( (nl = (char **)realloc( pglob->gl_pathv,
                                (ofs + 2) * sizeof( char **) )) == NULL ) {
        return GLOB_NOSPACE;
    }
    pglob->gl_pathv = nl;

    n = offsetof( globlist_, path ) + strlen( path ) + 1;

    if( (fp = malloc( n )) == NULL )
        return GLOB_NOSPACE;

    memcpy( fp->path, path, n - offsetof( globlist_, path ) );

    pglob->gl_pathv[ofs++] = fp->path;
    pglob->gl_pathv[ofs] = NULL;

    ++pglob->gl_pathc;

    fp->next = pglob->list_;
    pglob->list_ = fp;

    return 0;
}

/******************************************************************* globfree() */

void globfree( glob_t *pglob ) {
    globlist_ *fp;

    while( (fp = pglob->list_) != NULL ) {
        pglob->list_ = fp->next;
        free( fp );
    }
    free(  pglob->gl_pathv );
    pglob->gl_pathv = NULL;
    pglob->gl_pathc = 0;
}

/******************************************************************* cmpglb() */

static int cmpglb( const void *p1, const void *p2 ) {

    return _stricmp( *((const char *const *)p1),
                    *((const char *const *)p2) );
}

/******************************************************************* glob() */

int glob( const char *pattern, int flags,
          int (*errfunc)( const char *epath, int eerrno ),
          glob_t *pglob ) {
    HANDLE hfindfile;
    WIN32_FIND_DATAA finddata;
    int err;
    size_t found;

    if( errfunc != NULL )
        abort();

    found =
        err = 0;

    if( !(flags & GLOB_APPEND) ) {
        pglob->gl_pathc = 0;
        pglob->list_ = NULL;
        pglob->gl_pathv = NULL;
    }

    hfindfile = FindFirstFileA( pattern, &finddata );
    if( hfindfile == INVALID_HANDLE_VALUE ) {
        if( flags & GLOB_NOCHECK ) {
            err = addfile( flags, pglob, pattern );
            found ++;
        }
    } else {
        do {
            err = addfile( flags, pglob, finddata.cFileName );
            found++;
        } while( !err && FindNextFile( hfindfile, &finddata ) );
        FindClose( hfindfile );
    }

    if( !found )
        err = GLOB_NOMATCH;

    if( err ) {
        globfree( pglob );
        errno = err;
    } else {
        if( !(flags & GLOB_NOSORT) ) {
            qsort( pglob->gl_pathv, pglob->gl_pathc,
                   sizeof( char **), cmpglb );
        }
    }
    return err;
}

/******************************************************************* strlcpy() */

static size_t strlcpy( char *dst, const char *src,  size_t size ) {
    size_t srclen;

    size--;
    srclen = strlen( src );

    if( srclen > size )
        srclen = size;

    memcpy( dst, src, srclen );
    dst[srclen] = '\0';

    return (srclen);
}

/******************************************************************* winbasename() */

char *winbasename( char *path ) {
    static char buf[MAX_PATH + 1];
    char *p, *d;
    if( path == NULL || !*path )
        return ".";
    strlcpy( buf, path, sizeof( buf ) );
    for( d = buf; *d && *d != ':' && *d != '\\'; d++ )
        ;
    if( !*d )
        return buf;
    if( *d == ':' ) {
        d++;
        if( !strcmp( d, "." ) )
            return buf;
    }
    for( p = d + strlen( d ) - 1; p > d; p-- )
        if( *p == '\\' )
            *p = '\0';
        else
            break;
    if( p <= d ) {
        if( !*++d )
            return "\\";
        return d;
    }
    if( (p = strrchr( d, '\\' )) == NULL ) {
        return d;
    }
    return ++p;
}
