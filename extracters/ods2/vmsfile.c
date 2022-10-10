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
#include <starlet.h>
#include <rms.h>
#include <ssdef.h>
#include <stsdef.h>
#include <descrip.h>

#include "vmsfile.h"

#ifndef $SUCCESSFUL
#define $SUCCESSFUL(cond) (((cond) & STS$M_SUCCESS))
#endif

typedef struct globlist_ {
    struct globlist_ *next;
    char path[1];
} globlist_;

/******************************************************************* addfile() */

static int addfile( int flags, glob_t *pglob, dsc$descriptor_d *path ) {
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

    n = offsetof( globlist_, path ) + path->dsc$w_length;

    if( (fp = malloc( n + 1 )) == NULL )
        return GLOB_NOSPACE;

    memcpy( fp->path, path->dsc$a_pointer, n - offsetof( globlist_, path ) );
    fp->path[n] = '\0';

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
    struct dsc$descriptor_d retdsc = { 0, DSC$K_DTYPE_T, DSC$K_CLASS_D, NULL };
    unsigned long ctx = 0;
    unsigned long status;
    struct dsc$descriptor_s patdsc = { 0, DSC$K_TYPE_T, DSC$K_CLASS_S, NULL };
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

    ctx = 0;
    patdsc.dsc$a_pointer = pattern;
    patdsc.dsc$w_length = strlen( pattern );

    do {
        if( $SUCCESSFUL(status = lib$find_file( &patdsc, &retdsc, &ctx,
                                                NULL, NULL, NULL, 0 )) ) {
            err = addfile( flags, pglob, &retdsc );
            found++;
        }
    } while( !err && $SUCCESSFUL(status) );
    lib$find_file_end( &ctx );
    str$free1_dx(&retdsc );

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

/******************************************************************* vmsbasename() */

char *vmsbasename( char *path ) {
    static char buf[NAM$C_MAXRSS + 1];
    char *p, *d;
    if( path == NULL || !*path )
        return ".";
    strlcpy( buf, path, sizeof( buf ) );

    if( (p = strrchr( buf, ']' )) != NULL )
        return p+1;
    if( (p = strrchr( buf, ':' )) != NULL )
        return p+1;
    return buf;
}
