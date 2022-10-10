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

#ifndef _PHYVHD_H
#define _PHYVHD_H 1

#include "ods2.h"

struct DEV;

vmscond_t phyvhd_snapshot( const char *filename, const char *parent );
vmscond_t phyvhd_init( struct DEV *dev );
vmscond_t phyvhd_close( struct DEV *dev );
void phyvhd_show( struct DEV *dev, size_t column );

int phyvhd_available( int query );

#endif
