/* Direct.h    Definitions for directory access routines */

/*
 *      This is part of ODS2 written by Paul Nankervis,
 *      email address:  Paulnank@au1.ibm.com
 *
 *      ODS2 is distributed freely for all members of the
 *      VMS community to use. However all derived works
 *      must maintain comments in their source to acknowledge
 *      the contributions of the original author and
 *      subsequent contributors.   This is free software; no
 *      warranty is offered,  and while we believe it to be useful,
 *      you use it at your own risk.
 */

#ifndef _DIRECT_H
#define _DIRECT_H

#include <stdint.h>
#include "access.h"
#include "descrip.h"
#include "f11def.h"
#include "ods2.h"

void direct_show( void );
vmscond_t direct( struct VCB *vcb, struct dsc_descriptor *fibdsc,
                  struct dsc_descriptor *filedsc, uint16_t *reslen,
                  struct dsc_descriptor *resdsc, unsigned action );

#define DIRECT_FIND 0
#define DIRECT_DELETE 1
#define DIRECT_CREATE 2

#define MODIFIES(action) (action != DIRECT_FIND)

vmscond_t direct_dirid( struct VCB *vcb, struct dsc$descriptor *dirdsc,
                        struct fiddef *dirid, struct fiddef *fid );

#endif /* #ifndef _DIRECT_H */
