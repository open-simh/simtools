/* Phyvirt.h V2.1    Definitions for virtual device routines */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

#ifndef _PHYVIRT_H
#define _PHYVIRT_H

void virt_show( const char *devnam );
char *virt_lookup( const char *devnam );
unsigned virt_device( char *devnam, char **vname );

#endif /* # ifndef _PHYVIRT_H */
