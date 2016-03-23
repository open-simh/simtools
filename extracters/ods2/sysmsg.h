/* Timothe Litt March 2016
 * Copyright (C) 2016 Timothe litt
 *   litt at acm dot org
 *
 * Free for use with the ODS2 package.  All other rights reserved.
 */

/*
 *       This is distributed as part of ODS2, originally  written by
 *       Paul Nankervis, email address:  Paulnank@au1.ibm.com
 *
 *       ODS2 is distributed freely for all members of the
 *       VMS community to use. However all derived works
 *       must maintain comments in their source to acknowledge
 *       the contibution of the original author.
 */

#ifndef SYSMSG_H
#define SYSMSG_H

#include <stdarg.h>

#define MSG_FACILITY (1 << 0)
#define MSG_SEVERITY (1 << 1)
#define MSG_NAME     (1 << 2)
#define MSG_TEXT     (1 << 3)
#define MSG_WITHARGS (1 << 4)

#define MSG_FULL     (MSG_FACILITY|MSG_SEVERITY|MSG_NAME|MSG_TEXT)

/* Some random non-system codes from VMS */

#define COPY$_FACILITY 103
#define DELETE$_FACILITY 147

#define COPY$_OPENIN 0x109a
#define COPY$_
#define DELETE$_DELVER 0x120a

const char *getmsg( unsigned int vmscode, unsigned int flags, ... );

void sysmsg_rundown( void );

#endif
