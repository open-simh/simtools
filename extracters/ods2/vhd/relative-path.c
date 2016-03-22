/* Copyright (c) 2008, XenSource Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of XenSource Inc. nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* Modified March 2016 Timothe Litt to work under windows.
 * Copyright (C) 2016 Timothe Litt
 * Modifications subject to the same license terms as above,
 * substituting "Timothe Litt" for "XenSource Inc."
 */

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "relative-path.h"

#ifdef _WIN32
#include <stdio.h>
#include <windows.h>
#include <Shlwapi.h>
#define strdup _strdup
#else
#include <syslog.h>
#endif

#define DELIMITER    '/'

#define FN __func__

#define EPRINTF(a) rpath_log_error a

static void rpath_log_error( const char *func, const char *fmt, ... )
#ifdef __GNUC__
    __attribute__((format(printf,2,3)));
#else
    ;
#endif

#ifndef LIBVHD_HAS_SYSLOG
#ifdef _WIN32
#define LIBVHD_HAS_SYSLOG 0
#else
#define LIBVHD_HAS_SYSLOG 1
#endif
#endif

#define sfree(ptr)         \
do {                       \
	free(ptr);         \
	ptr = NULL;        \
} while (0)

#ifdef _WIN32
char *realpath( const char *path, char *resolved ) {
    char *p;
    DWORD len;

    p = resolved;
    if( resolved == NULL ) {
        resolved = malloc( 1+ MAX_PATH + 1 );
        if( resolved == NULL ) {
            return NULL;
        }
    }
    if( (len = GetFullPathName( path[0] == '/'? path+1:path, MAX_PATH + 1, resolved, NULL )) == 0 ) {
        if( p == NULL )
            free( resolved );
        return NULL;
    }
    if( len > MAX_PATH ) {
        if( p != NULL )
            return NULL;
        if( (p = realloc( resolved, len )) == NULL ) {
            free( resolved );
            return NULL;
        }
        resolved = p;
        len = GetFullPathName( path, MAX_PATH + 1, resolved, NULL );
        if( len > MAX_PATH || len == 0 ) {
            free( resolved );
            return NULL;
        }
    }

    for( p = resolved; *p; p++ ) {
        if( *p == '\\')
            *p = '/';
    }
    for( p = resolved; isalpha( *p ); p++ )
        ;
    if( *p == ':' ) { /* Hide drive under '/' for callers. */
        memmove( resolved + 1, resolved, strlen( resolved ) +1 );
        resolved[0] = '/'; /* Callers must skip '/' when touching filesystem. */
    }
    return resolved;
}
int asprintf( char **result, const char *fmt, ... ) {
    int len;
    va_list ap;
    char *np;

    /* It would be better to va_copy() the list and do a prescan,
     * but va_copy apparently has issues with versions of MS C
     * still in the field.  This approach is ugly and wasteful, but
     * should work.  (A KB just isn't what it used to be...)
     */
    if( (*result = malloc( 1 * 100 * 1000 + 1 )) == NULL )
        return -1;
    va_start( ap, fmt );
    len = vsprintf( *result, fmt, ap );
    np = realloc( *result, len + 1 );
    if( np != NULL )
        *result = np;
    return len;
}
#endif

/*
 * count number of tokens between DELIMITER characters
 */

int count_nodes(char *path)
{
	int i;
	char *tmp;

	if (!path)
		return 0;

	for (i = 0, tmp = path; *tmp != '\0'; tmp++)
		if (*tmp == DELIMITER)
			i++;

	return i;
}

/*
 * return copy of next node in @path, or NULL
 * @path is moved to the end of the next node
 * @err is set to -errno on failure
 * copy should be freed
 */
static char *
next_node(char **path, int *err)
{
	int ret;
	char *tmp, *start;

	if (!path || !*path) {
		*err = -EINVAL;
		return NULL;
	}

	*err  = 0;
	start = *path;

	for (tmp = *path; *tmp != '\0'; tmp++)
		if (*tmp == DELIMITER) {
			int size;
			char *node;

			size = tmp - start + 1;
			node = malloc(size);
			if (!node) {
				*err = -ENOMEM;
				return NULL;
			}

			ret = snprintf(node, size, "%s", start);
			if (ret < 0) {
				free(node);
				*err = -EINVAL;
				return NULL;
			}

			*path = tmp;
			return node;
		}

	return NULL;
}

/*
 * count number of nodes in common betwee @to and @from
 * returns number of common nodes, or -errno on failure
 */
static int
count_common_nodes(char *to, char *from)
{
	int err, common;
	char *to_node, *from_node;

	if (!to || !from)
		return -EINVAL;

	err       = 0;
	common    = 0;
	to_node   = NULL;
	from_node = NULL;

	do {
		to_node = next_node(&to, &err);
		if (err || !to_node)
			break;

		from_node = next_node(&from, &err);
		if (err || !from_node)
			break;

		if (strncmp(to_node, from_node, MAX_NAME_LEN))
			break;

		++to;
		++from;
		++common;
		sfree(to_node);
		sfree(from_node);

	} while (1);

	sfree(to_node);
	sfree(from_node);

	if (err)
		return err;

	return common;
}

/*
 * construct path of @count '../', './' if @count is zero, or NULL on error
 * result should be freed
 */
static char *
up_nodes(int count)
{
	char *path, *tmp;
	int i, ret, len, size;

	if (!count)
		return strdup("./");

	len  = strlen("../");
	size = len * count;
	if (size >= MAX_NAME_LEN)
		return NULL;

	path = malloc(size + 1);
	if (!path)
		return NULL;

	tmp = path;
	for (i = 0; i < count; i++) {
		ret = sprintf(tmp, "../");
		if (ret < 0 || ret != len) {
			free(path);
			return NULL;
		}
		tmp += ret;
	}

	return path;
}

/*
 * return pointer to @offset'th node of path or NULL on error
 */
static char *
node_offset(char *from, int offset)
{
	char *path;

	if (!from || !offset)
		return NULL;

	for (path = from; *path != '\0'; path++) {
		if (*path == DELIMITER)
			if (--offset == 0)
				return path + 1;
	}

	return NULL;
}

/*
 * return a relative path from @from to @to
 * result should be freed
 */
char *
relative_path_to(char *from, char *to, int *err)
{
	int from_nodes, common;
	char *to_absolute, *from_absolute;
	char *up, *common_target_path, *relative_path;

	*err          = 0;
	up            = NULL;
	to_absolute   = NULL;
	from_absolute = NULL;
	relative_path = NULL;

	if (strnlen(to, MAX_NAME_LEN)   == MAX_NAME_LEN ||
	    strnlen(from, MAX_NAME_LEN) == MAX_NAME_LEN) {
		EPRINTF((FN,"invalid input; max path length is %d\n",
			MAX_NAME_LEN));
		*err = -ENAMETOOLONG;
		return NULL;
	}

	to_absolute = realpath(to, NULL);
	if (!to_absolute) {
		EPRINTF((FN,"failed to get absolute path of %s\n", to));
		*err = -errno;
		goto out;
	}

	from_absolute = realpath(from, NULL);
	if (!from_absolute) {
		EPRINTF((FN,"failed to get absolute path of %s\n", from));
		*err = -errno;
		goto out;
	}

	if (strnlen(to_absolute, MAX_NAME_LEN)   == MAX_NAME_LEN ||
	    strnlen(from_absolute, MAX_NAME_LEN) == MAX_NAME_LEN) {
		EPRINTF((FN,"invalid input; max path length is %d\n",
			MAX_NAME_LEN));
		*err = -ENAMETOOLONG;
		goto out;
	}

	/* count nodes in source path */
	from_nodes = count_nodes(from_absolute);

	/* count nodes in common */
	common = count_common_nodes(to_absolute + 1, from_absolute + 1);
	if (common < 0) {
		EPRINTF((FN,"failed to count common nodes of %s and %s: %d\n",
			to_absolute, from_absolute, common));
		*err = common;
		goto out;
	}

	/* move up to common node */
	up = up_nodes(from_nodes - common - 1);
	if (!up) {
		EPRINTF((FN,"failed to allocate relative path for %s: %d\n",
			from_absolute, -ENOMEM));
		*err = -ENOMEM;
		goto out;
	}

	/* get path from common node to target */
	common_target_path = node_offset(to_absolute, common + 1);
	if (!common_target_path) {
		EPRINTF((FN,"failed to find common target path to %s: %d\n",
			to_absolute, -EINVAL));
		*err = -EINVAL;
		goto out;
	}

	/* get relative path */
	if (asprintf(&relative_path, "%s%s", up, common_target_path) == -1) {
		EPRINTF((FN,"failed to construct final path %s%s: %d\n",
			up, common_target_path, -ENOMEM));
		relative_path = NULL;
		*err = -ENOMEM;
		goto out;
	}

out:
	sfree(up);
	sfree(to_absolute);
	sfree(from_absolute);

	return relative_path;
}
static void rpath_log_error( const char *func, const char *fmt, ... )
{
    char *buf, nilbuf;
    size_t ilen, len;
    va_list ap;

    ilen = sizeof( "tap-err:%s: " ) + strlen( func ) -2;
    va_start(ap, fmt );
    len = vsnprintf( &nilbuf, 1, fmt, ap );
    va_end( ap );

    if( (buf = malloc( ilen + len + 1 )) == NULL ) {
#if LIBVHD_HAS_SYSLOG
        syslog( LOG_INFO, "tap-err:%s: Out of memory", func );
#else
        fprintf( stderr, "tap-err%s: Out of memory for %s\n", func, fmt );
#endif
        return;
    }
    va_start(ap, fmt);
    (void) snprintf( buf, ilen, "tap-err:%s: ", func );
    (void) vsnprintf( buf + ilen -1, len+1, fmt, ap );
    va_end( ap );
    len += ilen -1;
    if( buf[ len -1 ] != '\n' )
        buf[len++] = '\n';
    buf[len] = '\0';
#if LIBVHD_HAS_SYSLOG
    syslog(LOG_INFO, buf);
#else
    fputs( buf, stderr );
#endif
    free( buf );

    return;
 }
