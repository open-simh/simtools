/* Timothe Litt March 2016
 * Copyright (C) 2016 Timothe litt
 *   litt at acm dot org
 * Distribution per Paul's license below.
 *
 * Revision history at bottom of this file.
 *
 */

/*
 *       This is distributed as part of ODS2, originally  written by
 *       Paul Nankervis, email address:  Paulnank@au1.ibm.com
 *
 *       ODS2 is distributed freely for all members of the
 *       VMS community to use. However all derived works
 *       must maintain comments in their source to acknowledge
 *       the contributions of the original author and
 *       subsequent contributors.   This is free software; no
 *       warranty is offered,  and while we believe it to be useful,
 *       you use it at your own risk.
 */


#ifndef VERSION_H
#define VERSION_H

#ifdef DEBUG_BUILD
#define MODULE_IDENT "Debug build"
#else
#define MODULE_IDENT "V3.0"
#endif

#define MODULE_NAME  ODS2

#define MNAMEX(n) #n
#define MNAME(n) MNAMEX(n)


/* Modified by:
 *   Oct 2022 Timothe Litt <litt@acm _ddot_ org>
 *      Remove readline due to license issues, editline instead.
 *      Deal with deprecated ftime.
 *   Feb 2016 Timothe Litt <litt at acm _ddot_ org>
 *      Bug fixes, readline support, build on NT without wnaspi32,
 *      Decode error messages, patch from vms2linux.de. VS project files.
 *      Rework command parsing and help.  Bugs, bugs & bugs.  Merge
 *      code from Larry Baker, USGS.  Add initialize volume, spawn,
 *      and more.  Make file creation/writes work. See git commit
 *      history for details.
 *
 *      V3.0 - Merge and adapt code from Larry Baker's V2.0
 *
*    8-JUN-2005		Larry Baker <baker@usgs.gov>
 *
 *  Add #include guards in .h files.
 *  Use named constants in place of literals.
 *  Replace BIG_ENDIAN with ODS2_BIG_ENDIAN (Linux always #defines
 *     BIG_ENDIAN).
 *  Add SUBST DRIVE: FILE to "mount" a file (vs. a disk).
 *  Implement quoted arguments (paired " or '; no escape characters)
 *     in cmdsplit(), e.g., to specify a Unix path or a null argument.
 *  Remove VMSIO conditional code (need to "mount" a file now).
 *
 *     Jul-2003, v1.3hb, some extensions by Hartmut Becker
 *
 *   31-AUG-2001 01:04	Hunter Goatley <goathunter@goatley.com>
 *
 *	For VMS, added routine getcmd() to read commands with full
 *	command recall capabilities.
 *
 */

#endif
