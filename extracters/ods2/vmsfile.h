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

#ifndef _VMSFILE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct globlist_;
typedef struct {
    size_t gl_pathc;
    char **gl_pathv;
    size_t gl_offs;
    struct globlist_ *list_;
} glob_t;

#define _WINFILES_H
#define GLOB_MARK 0
#define GLOB_BRACE 0
#define GLOB_TILDE_CHECK 0

#define GLOB_APPEND 1
#define GLOB_DOOFFS 2
#define GLOB_NOCHECK 4
#define GLOB_NOSORT 8

#define GLOB_NOSPACE 1
#define GLOB_ABORTED 2
#define GLOB_NOMATCH 3

int glob( const char *pattern, int flags,
          int (*errfunc)( const char *epath, int eerrno ),
          glob_t *pglob );

void globfree( glob_t *pglob );

char *vmsbasename( char *path );

#endif
