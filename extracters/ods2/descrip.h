/* Descrip.h    Definitions for descriptors */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contributions of the original author and
 * subsequent contributors.

        1.4  Changed declarations NOT to define DOLLAR identifiers
             unless DOLLAR is defined (some compilers can't handle $'s!)
             Fixed definition of _DESCRIPTOR().
        1.4A Changed default DOLLAR handling to include DOLLAR
             identifers unless NO_DOLLAR is defined
             (ie #ifdef DOLLAR to #ifndef NO_DOLLAR)
*/

#ifndef _DESCRIP_H
#define _DESCRIP_H

#ifndef NO_DOLLAR
#ifndef $DESCRIPTOR
#define DSC$K_DTYPE_T   DSC_K_DTYPE_T
#define DSC$K_CLASS_S   DSC_K_CLASS_S
#define dsc$descriptor  dsc_descriptor
#define dsc$w_length    dsc_w_length
#define dsc$b_dtype     dsc_b_dtype
#define dsc$b_class     dsc_b_class
#define dsc$a_pointer   dsc_a_pointer
#define $DESCRIPTOR     _DESCRIPTOR
#endif
#endif

#define DSC_K_DTYPE_T 0
#define DSC_K_CLASS_S 0

struct dsc_descriptor {
    unsigned short dsc_w_length;
    unsigned char dsc_b_dtype;
    unsigned char dsc_b_class;
    char *dsc_a_pointer;
};

#define _DESCRIPTOR(symbol,value) \
        struct dsc_descriptor symbol = {sizeof(value)-1,0,0,value}

#endif /* #ifndef _DESCRIP_H */
