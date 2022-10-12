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

#ifndef _RELATIVE_PATH_H_
#define _RELATIVE_PATH_H_

#define MAX_NAME_LEN 1000

#if defined(_WIN32) && defined(LIBVHD_DLL)
#ifdef LIBVHD_EXPORTS
#define LIBVHD_API __declspec(dllexport)
#else
#define LIBVHD_API __declspec(dllimport)
#endif
#else
#define LIBVHD_API
#endif

/*
 * returns a relative path from @src to @dest
 * result should be freed
 */
LIBVHD_API char *relative_path_to(char *src, char *dest, int *err);

#ifdef _WIN32
LIBVHD_API char *realpath( const char *path, char *resolved );
LIBVHD_API int asprintf( char **result, const char *fmt, ... );
#endif

#endif
