/* Direct.h    Definitions for directory access routines */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

#ifndef _DIRECT_H
#define _DIRECT_H

#include "access.h"
#include "descrip.h"
#include "f11def.h"

void direct_show( void );
unsigned direct(struct VCB *vcb,struct dsc_descriptor *fibdsc,
                struct dsc_descriptor *filedsc,unsigned short *reslen,
                struct dsc_descriptor *resdsc,unsigned action);

#define DIRECT_FIND 0
#define DIRECT_DELETE 1
#define DIRECT_CREATE 2

#define MODIFIES(action) (action != DIRECT_FIND)

#endif /* #ifndef _DIRECT_H */
