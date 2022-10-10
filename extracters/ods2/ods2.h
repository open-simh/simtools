/*
 *  ods2.h
 */

/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com
 *
 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors. This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
 */

#ifndef _ODS2_H
#define _ODS2_H

#ifndef TRUE
#define TRUE ( 0 == 0 )
#endif
#ifndef FALSE
#define FALSE ( 0 != 0 )
#endif

#if DEBUG
#include "debug.h"
#endif

#include <stdint.h>

typedef uint32_t vmscond_t;

#define $MATCHCOND( c1, c2 ) ( ((c1) & STS$M_FAC_SP)?                    \
                              ((c1) & STS$M_COND_ID) == ((c2) & STS$M_COND_ID): \
                              ((c1) & STS$M_MSG_NO) == ((c2) & STS$M_MSG_NO) )

#define $SETLEVEL(cond, LEVEL) (((cond) & ~STS$M_SEVERITY) | STS$K_ ## LEVEL)
#define $SETFAC(cond,fac) (((cond) & ~STS$M_FAC_NO) | (fac ## _FACILITY << STS$V_FAC_NO))
#define $SHRMSG(cond,fac,LEVEL) $SETLEVEL((((cond) & STS$M_FAC_SP)? (cond): $SETFAC(cond,fac)),LEVEL)

#define $FAILED(cond) !((cond) & STS$M_SUCCESS)
#define $FAILS(cond)  !((cond) & STS$M_SUCCESS)
#define $SUCCESSFUL(cond) (((cond) & STS$M_SUCCESS))
#define $PRINTED(cond) (((cond) & STS$M_INHIB_MSG) != 0)

/* Option bits common to I/O, access, visible outside the ODS-2 parser */

#define MOU_WRITE    OPT_SHARED_1
#define MOU_VIRTUAL  OPT_SHARED_2
#define MOU_LOG      OPT_SHARED_3
#define PHY_CREATE   OPT_SHARED_4
#define PHY_VHDDSK   OPT_SHARED_5
#define OS_MOUNTED   OPT_SHARED_6

/* OPT_SHARED_7 is free */

/* DEVTYPE can probably be shrunk by several bits */
#define MOU_V_DEVTYPE (OPT_V_SHARED + 8)
#define MOU_DEVTYPE  ((((options_t)(1u) << MOU_S_DEVTYPE) -1) << MOU_V_DEVTYPE)
#define MOU_S_DEVTYPE (12)

/* I/O option bits overlaid for use by non-I/O commands */

#define OPT_V_SHARED  (0)
#define OPT_SHARED_1  (((options_t)1u) << (OPT_V_SHARED +  0))
#define OPT_SHARED_2  (((options_t)1u) << (OPT_V_SHARED +  1))
#define OPT_SHARED_3  (((options_t)1u) << (OPT_V_SHARED +  2))
#define OPT_SHARED_4  (((options_t)1u) << (OPT_V_SHARED +  3))
#define OPT_SHARED_5  (((options_t)1u) << (OPT_V_SHARED +  4))
#define OPT_SHARED_6  (((options_t)1u) << (OPT_V_SHARED +  5))
#define OPT_SHARED_7  (((options_t)1u) << (OPT_V_SHARED +  6))
#define OPT_SHARED_8  (((options_t)1u) << (OPT_V_SHARED +  7))
#define OPT_SHARED_9  (((options_t)1u) << (OPT_V_SHARED +  8))
#define OPT_SHARED_10 (((options_t)1u) << (OPT_V_SHARED +  9))
#define OPT_SHARED_11 (((options_t)1u) << (OPT_V_SHARED + 10))
#define OPT_SHARED_12 (((options_t)1u) << (OPT_V_SHARED + 11))
#define OPT_SHARED_13 (((options_t)1u) << (OPT_V_SHARED + 12))
#define OPT_SHARED_14 (((options_t)1u) << (OPT_V_SHARED + 13))
#define OPT_SHARED_15 (((options_t)1u) << (OPT_V_SHARED + 14))
#define OPT_SHARED_16 (((options_t)1u) << (OPT_V_SHARED + 15))
#define OPT_SHARED_17 (((options_t)1u) << (OPT_V_SHARED + 16))
#define OPT_SHARED_18 (((options_t)1u) << (OPT_V_SHARED + 17))
#define OPT_SHARED_19 (((options_t)1u) << (OPT_V_SHARED + 18))
#define OPT_SHARED_20 (((options_t)1u) << (OPT_V_SHARED + 19))


/* Option bits that will not conflict with I/O */

#define OPT_V_GENERIC  (MOU_V_DEVTYPE + MOU_S_DEVTYPE)
#define OPT_GENERIC_1  ((options_t)(1u << (OPT_V_GENERIC + 0)))
#define OPT_GENERIC_2  ((options_t)(1u << (OPT_V_GENERIC + 1)))
#define OPT_GENERIC_3  ((options_t)(1u << (OPT_V_GENERIC + 2)))
#define OPT_GENERIC_4  ((options_t)(1u << (OPT_V_GENERIC + 3)))
#define OPT_GENERIC_5  ((options_t)(1u << (OPT_V_GENERIC + 4)))
#define OPT_GENERIC_6  ((options_t)(1u << (OPT_V_GENERIC + 5)))
#define OPT_GENERIC_7  ((options_t)(1u << (OPT_V_GENERIC + 6)))
#define OPT_GENERIC_8  ((options_t)(1u << (OPT_V_GENERIC + 7)))
#define OPT_GENERIC_9  ((options_t)(1u << (OPT_V_GENERIC + 8)))
#define OPT_GENERIC_10 ((options_t)(1u << (OPT_V_GENERIC + 9)))
#define OPT_GENERIC_11 ((options_t)(1u << (OPT_V_GENERIC + 10)))
#define OPT_GENERIC_12 ((options_t)(1u << (OPT_V_GENERIC + 11)))

#if (OPT_V_GENERIC+11 > 31)
#error options_t overflow, change size or definitions in ods2.h
#endif

/* This bit is set in the table end marker and does not overlap the
 * data bits.  The decision to sort is made after a scan of the entire
 * table that computes some statistics that do not depend on order.
 */

#define OPT_NOSORT    ((options_t)(1u << 31))

typedef int32_t options_t;

#endif /* #ifndef _ODS2_H */
