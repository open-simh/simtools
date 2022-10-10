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
/*
 *
 *     Vmstime.h
 *
 *     Author: Paul Nankervis
 *
 *     Please send bug reports to Paulnank@au1.ibm.com
 *
 *      1.4A Changed default DOLLAR handling to include DOLLAR
 *           identifers unless NO_DOLLAR is defined
 *           (ie #ifdef DOLLAR to #ifndef NO_DOLLAR)
 *      1.5  Moved more header info here and added cvt_internal support
 *      1.6A Fixed endian problems in addx/subx
 *
 */


#ifndef _VMSTIME_H
#define _VMSTIME_H

#include "descrip.h"
#include "ssdef.h"
#include "stsdef.h"

#ifdef __ALPHA
#define VMSTIME_64BIT __int64
#endif

/* Define quadword time type and test for delta times... */

#ifdef VMSTIME_64BIT
typedef VMSTIME_64BIT VMSTIME[1];
typedef VMSTIME_64BIT *pVMSTIME;
#define ISDELTA(x) (*x < 0)
#define VMSTIME_ZERO  {0}
#else
typedef unsigned char VMSTIME[8];
typedef unsigned char *pVMSTIME;
#define ISDELTA(x) (x[7] & 0x80)
#define VMSTIME_ZERO  {0,0,0,0,0,0,0,0}
#endif

/* Define status codes we require... note that if the real code values
 * are being included (SSDEF.H) then they should be included first!!!
 */

#ifndef SS__NORMAL
#define SS__NORMAL 1
#define SS__IVTIME 388
#define LIB__NORMAL 1409025
#define LIB__IVTIME 1410012
#define LIB__ONEDELTIM 1410020
#define LIB__ABSTIMREQ 1410044
#define LIB__DELTIMREQ 1410052
#define LIB__INVOPER 1410060
#define STS_M_SUCCESS 1

#ifndef NO_DOLLAR
#ifndef SS$_NORMAL
#define SS$_NORMAL     SS__NORMAL
#define SS$_IVTIME     SS__IVTIME
#define LIB$_NORMAL    LIB__NORMAL
#define LIB$_IVTIME    LIB__IVTIME
#define LIB$_ONEDELTIM LIB__ONEDELTIM
#define LIB$_ABSTIMREQ LIB__ABSTIMREQ
#define LIB$_DELTIMREQ LIB__DELTIMREQ
#define LIB$_INVOPER   LIB__INVOPER
#define STS$M_SUCCESS  STS_M_SUCCESS
#endif
#endif
#endif

/* Constants for lib$cvt_ routines */

#define LIB_K_MONTH_OF_YEAR 1
#define LIB_K_DAY_OF_YEAR 2
#define LIB_K_HOUR_OF_YEAR 3
#define LIB_K_MINUTE_OF_YEAR 4
#define LIB_K_SECOND_OF_YEAR 5
#define LIB_K_DAY_OF_MONTH 6
#define LIB_K_HOUR_OF_MONTH 7
#define LIB_K_MINUTE_OF_MONTH 8
#define LIB_K_SECOND_OF_MONTH 9
#define LIB_K_DAY_OF_WEEK 10
#define LIB_K_HOUR_OF_WEEK 11
#define LIB_K_MINUTE_OF_WEEK 12
#define LIB_K_SECOND_OF_WEEK 13
#define LIB_K_HOUR_OF_DAY 14
#define LIB_K_MINUTE_OF_DAY 15
#define LIB_K_SECOND_OF_DAY 16
#define LIB_K_MINUTE_OF_HOUR 17
#define LIB_K_SECOND_OF_HOUR 18
#define LIB_K_SECOND_OF_MINUTE 19
#define LIB_K_JULIAN_DATE 20
#define LIB_K_DELTA_WEEKS 21
#define LIB_K_DELTA_DAYS 22
#define LIB_K_DELTA_HOURS 23
#define LIB_K_DELTA_MINUTES 24
#define LIB_K_DELTA_SECONDS 25
#define LIB_K_DELTA_WEEKS_F 26
#define LIB_K_DELTA_DAYS_F 27
#define LIB_K_DELTA_HOURS_F 28
#define LIB_K_DELTA_MINUTES_F 29
#define LIB_K_DELTA_SECONDS_F 30
#define LIB_K_MAX_OPERATION 31

/* For system which can use a dollar symbol...*/

#ifndef NO_DOLLAR
#define sys$asctim          sys_asctim
#define sys$bintim          sys_bintim
#define sys$gettim          sys_gettim
#define sys$numtim          sys_numtim
#define lib$add_times       lib_add_times
#define lib$addx            lib_addx
#define lib$cvt_from_internal_time lib_cvt_from_internal_time
#define lib$cvt_to_internal_time lib_cvt_to_internal_time
#define lib$cvt_vectim      lib_cvt_vectim
#define lib$day             lib_day
#define lib$day_of_week     lib_day_of_week
#define lib$mult_delta_time lib_mult_delta_time
#define lib$sub_times       lib_sub_times
#define lib$subx            lib_subx

#define LIB$K_MONTH_OF_YEAR     LIB_K_MONTH_OF_YEAR
#define LIB$K_DAY_OF_YEAR       LIB_K_DAY_OF_YEAR
#define LIB$K_HOUR_OF_YEAR      LIB_K_HOUR_OF_YEAR
#define LIB$K_MINUTE_OF_YEAR    LIB_K_MINUTE_OF_YEAR
#define LIB$K_SECOND_OF_YEAR    LIB_K_SECOND_OF_YEAR
#define LIB$K_DAY_OF_MONTH      LIB_K_DAY_OF_MONTH
#define LIB$K_HOUR_OF_MONTH     LIB_K_HOUR_OF_MONTH
#define LIB$K_MINUTE_OF_MONTH   LIB_K_MINUTE_OF_MONTH
#define LIB$K_SECOND_OF_MONTH   LIB_K_SECOND_OF_MONTH
#define LIB$K_DAY_OF_WEEK       LIB_K_DAY_OF_WEEK
#define LIB$K_HOUR_OF_WEEK      LIB_K_HOUR_OF_WEEK
#define LIB$K_MINUTE_OF_WEEK    LIB_K_MINUTE_OF_WEEK
#define LIB$K_SECOND_OF_WEEK    LIB_K_SECOND_OF_WEEK
#define LIB$K_HOUR_OF_DAY       LIB_K_HOUR_OF_DAY
#define LIB$K_MINUTE_OF_DAY     LIB_K_MINUTE_OF_DAY
#define LIB$K_SECOND_OF_DAY     LIB_K_SECOND_OF_DAY
#define LIB$K_MINUTE_OF_HOUR    LIB_K_MINUTE_OF_HOUR
#define LIB$K_SECOND_OF_HOUR    LIB_K_SECOND_OF_HOUR
#define LIB$K_SECOND_OF_MINUTE  LIB_K_SECOND_OF_MINUTE
#define LIB$K_JULIAN_DATE       LIB_K_JULIAN_DATE
#define LIB$K_DELTA_WEEKS       LIB_K_DELTA_WEEKS
#define LIB$K_DELTA_DAYS        LIB_K_DELTA_DAYS
#define LIB$K_DELTA_HOURS       LIB_K_DELTA_HOURS
#define LIB$K_DELTA_MINUTES     LIB_K_DELTA_MINUTES
#define LIB$K_DELTA_SECONDS     LIB_K_DELTA_SECONDS
#define LIB$K_DELTA_WEEKS_F     LIB_K_DELTA_WEEKS_F
#define LIB$K_DELTA_DAYS_F      LIB_K_DELTA_DAYS_F
#define LIB$K_DELTA_HOURS_F     LIB_K_DELTA_HOURS_F
#define LIB$K_DELTA_MINUTES_F   LIB_K_DELTA_MINUTES_F
#define LIB$K_DELTA_SECONDS_F   LIB_K_DELTA_SECONDS_F
#define LIB$K_MAX_OPERATION     LIB_K_MAX_OPERATION
#endif


unsigned sys_gettim(pVMSTIME timadr);
unsigned sys_numtim(unsigned short timvec[7],pVMSTIME timadr);
unsigned sys_bintim(struct dsc_descriptor *timbuf,pVMSTIME timadr);
unsigned sys_asctim(unsigned short *timlen,struct dsc_descriptor *timbuf,
                    pVMSTIME timadr,unsigned cvtflg);
unsigned lib_add_times(pVMSTIME time1,pVMSTIME time2,pVMSTIME result);
unsigned lib_addx(void *addant,void *addee,void *result,int *lenadd);
unsigned lib_cvt_from_internal_time(unsigned *operation,
                                    unsigned *result,pVMSTIME input_time);
unsigned lib_cvt_to_internal_time(unsigned *operation,int *input,
                                  pVMSTIME result);
unsigned lib_cvt_vectim(unsigned short timbuf[7],pVMSTIME timadr);
unsigned lib_day(int *days,pVMSTIME timadr,int *day_time);
unsigned lib_day_of_week(pVMSTIME timadr,unsigned *weekday);
unsigned lib_mult_delta_time(int *multiple,pVMSTIME timadr);
unsigned lib_subx(void *subant,void *subee,void *result,int *lenadd);
unsigned lib_sub_times(pVMSTIME time1,pVMSTIME time2,pVMSTIME result);
unsigned vmstime_from_nt(pVMSTIME nt_time,pVMSTIME vms_time);
unsigned vmstime_to_nt(pVMSTIME vms_time,pVMSTIME nt_time);
int vmstime_compare(pVMSTIME time1,pVMSTIME time2);

#endif /* #ifndef _VMSTIME_H */
