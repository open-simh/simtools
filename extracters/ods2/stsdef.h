/*
 *  stsdef.h
 */

#ifndef _STSDEF_H
#define _STSDEF_H

#define STS$V_SEVERITY	0
#define STS$M_SEVERITY	0x00000007
#define STS$S_SEVERITY	3
#define STS$V_COND_ID	3
#define STS$M_COND_ID	0x0FFFFFF8
#define STS$S_COND_ID	25
#define STS$V_CONTROL	28
#define STS$M_CONTROL	0xF0000000
#define STS$S_CONTROL	4
#define STS$V_SUCCESS	0
#define STS$M_SUCCESS	0x01
#define STS$S_SUCCESS	1
#define STS$V_MSG_NO	3
#define STS$M_MSG_NO	0x0000FFF8
#define STS$S_MSG_NO	13
#define STS$V_CODE	3
#define STS$M_CODE	0x00007FF8
#define STS$S_CODE	12
#define STS$V_FAC_SP	15
#define STS$M_FAC_SP	0x00008000
#define STS$S_FAC_SP	1
#define STS$V_CUST_DEF	27
#define STS$M_CUST_DEF	0x08000000
#define STS$S_CUST_DEF	1
#define STS$V_INHIB_MSG 28
#define STS$M_INHIB_MSG 0x10000000
#define STS$S_INHIB_MSG 1
#define STS$V_FAC_NO	16
#define STS$M_FAC_NO	0x0FFF0000
#define STS$S_FAC_NO	12

#define STS$K_WARNING	0
#define STS$K_SUCCESS	1
#define STS$K_ERROR	2
#define STS$K_INFO	3
#define STS$K_SEVERE	4

#define $VMS_STATUS_CODE(code)          ( ( (code) & STS$M_CODE )       >> STS$V_CODE )
#define $VMS_STATUS_COND_ID(code)       ( ( (code) & STS$M_COND_ID )    >> STS$V_COND_ID )
#define $VMS_STATUS_CONTROL(code)       ( ( (code) & STS$M_CONTROL )    >> STS$V_CONTROL )
#define $VMS_STATUS_CUST_DEF(code)      ( ( (code) & STS$M_CUST_DEF )   >> STS$V_CUST_DEF )
#define $VMS_STATUS_FAC_NO(code)        ( ( (code) & STS$M_FAC_NO )     >> STS$V_FAC_NO )
#define $VMS_STATUS_FAC_SP(code)        ( ( (code) & STS$M_FAC_SP )     >> STS$V_FAC_SP )
#define $VMS_STATUS_INHIB_MSG(code)     ( ( (code) & STS$M_INHIB_MSG )  >> STS$V_INHIB_MSG )
#define $VMS_STATUS_MSG_NO(code)        ( ( (code) & STS$M_MSG_NO )     >> STS$V_MSG_NO )
#define $VMS_STATUS_SEVERITY(code)      ( ( (code) & STS$M_SEVERITY )   >> STS$V_SEVERITY )
#define $VMS_STATUS_SUCCESS(code)       ( ( (code) & STS$M_SUCCESS )    >> STS$V_SUCCESS )

#endif /* #ifndef _STSDEF_H */
