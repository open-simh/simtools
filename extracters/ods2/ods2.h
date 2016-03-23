/*
 *  ods2.h
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

/* Option bits common to I/O, access, visible outside the ODS-2 parser */

#define MOU_WRITE    (1 << 0)
#define MOU_VIRTUAL  (1 << 1)
#define MOU_LOG      (1 << 2)
#define PHY_CREATE   (1 << 3)
#define PHY_VHDDSK   (1 << 4)

#define MOU_V_DEVTYPE (8)
#define MOU_DEVTYPE  (0xffff << MOU_V_DEVTYPE)
#define MOU_S_DEVTYPE (16)

/* Option bits that will not conflict with I/O */

#define OPT_GENERIC_1 (1 << (MOU_V_DEVTYPE + MOU_S_DEVTYPE + 0) )
#define OPT_GENERIC_2 (1 << (MOU_V_DEVTYPE + MOU_S_DEVTYPE + 1) )
#define OPT_GENERIC_3 (1 << (MOU_V_DEVTYPE + MOU_S_DEVTYPE + 2) )
#define OPT_GENERIC_4 (1 << (MOU_V_DEVTYPE + MOU_S_DEVTYPE + 3) )
#define OPT_GENERIC_5 (1 << (MOU_V_DEVTYPE + MOU_S_DEVTYPE + 4) )
#define OPT_GENERIC_6 (1 << (MOU_V_DEVTYPE + MOU_S_DEVTYPE + 5) )
#define OPT_GENERIC_7 (1 << (MOU_V_DEVTYPE + MOU_S_DEVTYPE + 6) )
#define OPT_GENERIC_8 (1 << (MOU_V_DEVTYPE + MOU_S_DEVTYPE + 7) )

#define OPT_NOSORT    (1 << 31)

#endif /* #ifndef _ODS2_H */
