/* Fibdef.h    Definition of 'struct fibdef' */

/*
 *      This is part of ODS2 written by Paul Nankervis,
 *      email address:  Paulnank@au1.ibm.com

 *      ODS2 is distributed freely for all members of the
 *      VMS community to use. However all derived works
 *      must maintain comments in their source to acknowledge
 *      the contributions of the original author and
 *      subsequent contributors.   This is free software; no
 *      warranty is offered,  and while we believe it to be useful,
 *      you use it at your own risk.
 */

#ifndef _FIBDEF_H
#define _FIBDEF_H

#define FIB$M_WILD 0x100

struct fibdef {
    unsigned fib$l_acctl;
    unsigned short fib$w_fid_num;
    unsigned short fib$w_fid_seq;
    unsigned char fib$b_fid_rvn;
    unsigned char fib$b_fid_nmx;
    unsigned short fib$w_did_num;
    unsigned short fib$w_did_seq;
    unsigned char fib$b_did_rvn;
    unsigned char fib$b_did_nmx;
    unsigned fib$l_wcc;
    unsigned fib$w_nmctl;
    unsigned fib$l_exsz;
    unsigned fib$w_exctl;
    unsigned short fib$w_verlimit;
};

#endif /* #ifndef _FIBDEF_H */
