/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com
 *
 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors.   This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
 */

/*

 *      Vmstime.c

 *      Author: Paul Nankervis

 *       1.4    Changed declarations NOT to define DOLLAR identifiers
 *              unless DOLLAR is defined (some compilers can't handle $'s!)
 *       1.4A   Changed default DOLLAR handling to include DOLLAR
 *              identifers unless NO_DOLLAR is defined
 *              (ie #ifdef DOLLAR to #ifndef NO_DOLLAR)
 *       1.5    Added 64 bit support and cvt_internal routines
 *       1.6    Change addx, subx and compare to use larger integers
 *       1.6A   Fixed endian problems in addx/subx

 *      Please send bug reports or requests for enhancement
 *      or improvement via email to:     PaulNank@au1.ibm.com


 *      This module contains versions of the VMS time routines
 *      sys$numtim(), sys$asctim() and friends... They are
 *      intended to be compatible with the routines of the same
 *      name on a VMS system (so descriptors feature regularly!)

 *      This code relies on being able to manipluate day numbers
 *      and times using 32 bit arithmetic to crack a VMS quadword
 *      byte by byte. If your C compiler doesn't have 32 bit unsigned
 *      integer types then give up now! On a 64 bit systems VMSTIME_64BIT
 *      can be defined to do 64 bit operations directly....

 *      One advantage of doing arithmetic byte by byte is that
 *      the code does not depend on what 'endian' the target
 *      machine is - it will always treat bytes in the same order!
 *      (Hopefully VMS time bytes will always be in the same order!)

 *      Note: VMS time quadwords are simply an eight byte integer
 *            (quadword) representing the number of 100 nanasecond
 *            units since 17-Nov-1858. Delta times (time differences)
 *            are represented by a negative quadword value. It is
 *            interesting that Windows UTC times are in the same units
 *            but are based on the year 1601.

 *     A couple of stupid questions to go on with:-
 *         o OK, I give up! What is the difference between a zero
 *           date and a zero delta time?
 *         o Anyone notice that the use of 16 bit words in
 *           sys$numtim restricts delta times to 65535 days?

 *                                      Paul Nankervis
 *                                      paulnank@au1.ibm.com

 *      Note: The routine are defined here with an underscore instead
 *            of a dollar symbol (ie sys_asctim instead of sys$asctim).
 *            This avoids problems on systems which don't handle
 *            the '$' well, and it avoids conflicts under VMS.

*/

#if !defined( DEBUG ) && defined( DEBUG_VMSTIME )
#define DEBUG DEBUG_VMSTIME
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>            /* Use windows routines to get currrent time */
#else

#ifndef NO_LOCALTIME
#if defined(VMS)
#include <timeb.h>              /* For VAXC and GCC include libraries on VMS */
#else
#ifdef USE_FTIME
#include <sys/timeb.h>          /* For ftime() */
#else
#include <sys/time.h>           /* gettimeofday() */
#endif
#endif
#endif
#endif

#include "vmstime.h"            /* Our header file! */
#include "ods2.h"

#define TIMEBASE 100000         /* 10 millisecond units in quadword */
#define TIMESIZE 8640000        /* Factor between dates & times */

#define QUAD_CENTURY_DAYS 146097
/*   (400*365) + (400/4) - (400/100) + (400/400)   */
#define CENTURY_DAYS    36524
/*   (100*365) + (100/4) - (100/100)    */
#define QUAD_YEAR_DAYS  1461
/*   (4*365) + (4/4)    */
#define YEAR_DAYS       365
/*   365        */
#define OFFSET_DAYS     94187
/*   ((1858-1601)*365) + ((1858-1601)/4) - ((1858-1601)/100)
   + ((1858-1601)/400) + 320
        OFFSET FROM 1/1/1601 TO 17/11/1858  */
#define BASE_YEAR       1601


/* lib$addx & lib$subx perform arithmetic using a small integer at
   a time. Compare can be done using a full size integer (if the endian
   is correct.Note that on a big endian machine the maximum unit of
   work is always one character!!!
*/ 

#ifdef VMSTIME_64BIT
#define LARGE_INT unsigned VMSTIME_64BIT
#define SMALL_INT unsigned int
#define WORK_INT unsigned VMSTIME_64BIT
#else

#define WORK_INT unsigned int
#if defined(VMS) || defined(_WIN32)
#define LARGE_INT unsigned int
#define SMALL_INT unsigned short
#else
#define SMALL_INT unsigned char
#endif

#endif

/***************************************************************** lib_addx() */

unsigned lib_addx(void *addant,void *addee,void *result,int *lenadd)
{
    register int count;
    if (lenadd == NULL) {
        count = 8;
    } else {
        count = *lenadd * 4;
    }
#ifdef LARGE_INT
    if (count == sizeof(LARGE_INT)) {
        *(LARGE_INT *) result = *(LARGE_INT *) addant + *(LARGE_INT *) addee;
    } else {
#else
    {
#endif
        register SMALL_INT *ant = (SMALL_INT *) addant;
        register SMALL_INT *ee  = (SMALL_INT *) addee;
        register SMALL_INT *res = (SMALL_INT *) result;
        register WORK_INT carry = 0;
        count /= sizeof(SMALL_INT);
        do {
            carry = *ant++ + (carry + *ee++);
            *res++ = carry;
            carry = carry >> (sizeof(SMALL_INT) * 8);
        } while (--count > 0);
    }
    return LIB__NORMAL;
}

/***************************************************************** lib_subx() */

unsigned lib_subx(void *subant,void *subee,void *result,int *lenadd)
{
    register int count;
    if (lenadd == NULL) {
        count = 8;
    } else {
        count = *lenadd * 4;
    }
#ifdef LARGE_INT
    if (count == sizeof(LARGE_INT)) {
        *(LARGE_INT *) result = *(LARGE_INT *) subant + *(LARGE_INT *) subee;
    } else {
#else
    {
#endif
        register SMALL_INT *ant = (SMALL_INT *) subant;
        register SMALL_INT *ee  = (SMALL_INT *) subee;
        register SMALL_INT *res = (SMALL_INT *) result;
        register WORK_INT carry = 0;
        count /= sizeof(SMALL_INT);
        do {
            carry = *ant++ - (carry + *ee++);
            *res++ = carry;
            carry = (carry >> (sizeof(SMALL_INT) * 8)) & 1;
        } while (--count > 0);
    }
    return LIB__NORMAL;
}

/********************************************************** vmstime_compare() */

/* vmstime_compare() is used to compare times.
      returns -1 if time2 is bigger
               0 if times are equal
               1 if time1 is bigger
*/

int vmstime_compare(pVMSTIME time1,pVMSTIME time2)
{
#ifdef VMSTIME_64BIT
    if (*time1 > *time2) return 1;
    if (*time1 < *time2) return -1;
#else
#ifdef LARGE_INT
    register int count = 8 / sizeof(LARGE_INT);
    register LARGE_INT *t1 = (LARGE_INT *) time1 + 8 / sizeof(LARGE_INT) - 1;
    register LARGE_INT *t2 = (LARGE_INT *) time2 + 8 / sizeof(LARGE_INT) - 1;
#else
    register int count = 8 / sizeof(SMALL_INT);
    register SMALL_INT *t1 = (SMALL_INT *) time1 + 8 / sizeof(SMALL_INT) - 1;
    register SMALL_INT *t2 = (SMALL_INT *) time2 + 8 / sizeof(SMALL_INT) - 1;
#endif
    do {
        if (*t1 > *t2) return 1;
        if (*t1 < *t2) return -1;
        t1--; t2--;
    } while (--count > 0);
#endif
    return 0;
}

/******************************************************** vmstime_date_time() */

/* vmstime_date_time() is an internal routine assemble the date (day number)
   and time in a time quadword - basically the opposite of lib_day()
*/

unsigned vmstime_date_time(int days,pVMSTIME timadr,int day_time)
{
    /* Put date/time into VMS quadword timbuf... */

#ifdef VMSTIME_64BIT
    if (day_time <= -TIMESIZE || day_time >= TIMESIZE) return SS__IVTIME;
    *timadr = ((days * (VMSTIME_64BIT) TIMESIZE) + day_time) * TIMEBASE;
#else
    register unsigned time = day_time;
    register unsigned char *dstptr = timadr;
    register int count = 8,carry = 0,date = days;

    if (day_time <= -TIMESIZE || day_time >= TIMESIZE) return SS__IVTIME;

    if (date == 0 && day_time < 0) {
        do {
            carry += (time & 0xFF) * TIMEBASE;
            time = (time >> 8) | 0xFF000000;
            *dstptr++ = carry;
            carry = (carry >> 8);
        } while (--count > 0);
    } else{ 
        do {
            time += (date & 0xFF) * TIMESIZE;
            date = (date >> 8);
            carry += (time & 0xFF) * TIMEBASE;
            time = (time >> 8);
            *dstptr++ = carry;
            carry = (carry >> 8);
        } while (--count > 0);
    }
#endif
    return SS__NORMAL;
}

/*********************************************************** lib_cvt_vectim() */

/* lib_cvt_vectim() takes individual date/time fields from a
   seven word buffer and munges them into a time quadword...
*/

int vmstime_mthend[] = {0,31,60,91,121,152,182,213,244,274,305,335,366};

unsigned lib_cvt_vectim(unsigned short timvec[7],pVMSTIME timadr)
{
    register vmscond_t sts;
    register int year,month,days;

    /* lib_cvt_vectim packs the seven date/time components into a quadword... */

    if (timvec[3] > 23 || timvec[4] > 59 ||
        timvec[5] > 59 || timvec[6] > 99) return SS__IVTIME;

    year = timvec[0];
    month = timvec[1];
    days = timvec[2];

    if (year == 0 && month == 0) {

	/* Pass back delta result */

        sts = vmstime_date_time(-days,timadr,timvec[3] * -360000 - 
                                timvec[4] * 6000 - timvec[5] * 100 - timvec[6]);
    } else {

        /* Generate days since base date.. */

        int nonleap_adjust = 0;
        if (year < 1858 || year > 9999 ||
            month < 1 || month > 12 || days < 1) return SS__IVTIME;
        days += vmstime_mthend[month - 1];
        if (month > 1) {
            if (year % 4) {
                nonleap_adjust = 1;
            } else {
                if ((year % 100) == 0 && year % 400) nonleap_adjust = 1;
            }
        }
        if (days > vmstime_mthend[month] || (month == 2 &&
             days > vmstime_mthend[month] - nonleap_adjust)) return SS__IVTIME;
        if (month > 2) days -= nonleap_adjust;
        year -= BASE_YEAR;
        days += year * YEAR_DAYS + year / 4 - year / 100 + year / 400 -
                     OFFSET_DAYS - 1;
        sts = vmstime_date_time(days,timadr,timvec[3] * 360000 +
                                timvec[4] * 6000 + timvec[5] * 100 + timvec[6]);
    }
    return sts;
}

/*************************************************************** sys_gettim() */

/* sys_gettim() implemented here by getting C library time in seconds
   since 1-Jan-1970 and munging into a time quadword... Can use time()
   to just get seconds or ftime() to find seconds with milli-seconds.
   localtime() is used to find non-offset time. Note that some libraries
   don't do any of this particularly well! Under windows we can use
   a munged version of the internal UTC time
*/

unsigned sys_gettim(pVMSTIME timadr)
{
#ifdef _WIN32
    /* Get windows time adjust for timezone and convert to VMS... */
    GetSystemTimeAsFileTime((struct _FILETIME *)timadr);
    FileTimeToLocalFileTime((struct _FILETIME *)timadr,
                            (struct _FILETIME *)timadr);
    return vmstime_from_nt(timadr,timadr);
#else
#ifdef NO_LOCALTIME
    /* Use time straight from time() */
    time_t curtim = time(NULL);
    return vmstime_date_time(40587 + curtim / 86400,timadr,
                             (curtim % 86400) * 100);
#else
    /* Get time from ftime() and localtime() */
    struct tm *lclptr;
    unsigned short timvec[7];
#ifdef USE_FTIME
    struct timeb timval;

    timval.millitm = 0;         /* for broken versions of ftime() */
    ftime(&timval);
    lclptr = localtime(&timval.time);
    timvec[6] = timval.millitm / 10;
#else
    struct timeval timval;

    if( gettimeofday( &timval, NULL ) ) {
        return SS$_BADPARAM;
    }
    lclptr = localtime(&timval.tv_sec);
    timvec[6] = (timval.tv_usec + 500) / 10000;
#endif

    timvec[0] = lclptr->tm_year + 1900;
    timvec[1] = lclptr->tm_mon + 1;
    timvec[2] = lclptr->tm_mday;
    timvec[3] = lclptr->tm_hour;
    timvec[4] = lclptr->tm_min;
    timvec[5] = lclptr->tm_sec;
    return lib_cvt_vectim(timvec,timadr);
#endif
#endif
}

/************************************************************** vmstime_day() */

/* vmstime_day() is a routine to crack time quadwords into a day number
   and time. It is different to lib_day() in that delta times are returned
   as negative values to distinguish small delta values from dates
*/

unsigned vmstime_day(int *days,pVMSTIME timadr,int *day_time)
{
    VMSTIME wrktim;
#ifdef VMSTIME_64BIT
    register VMSTIME_64BIT timval;
#else
    register int delta = 0;
    register unsigned char *srcptr;
#endif

    /* If no time specified get current using gettim() */

    if (timadr == NULL) {
        register vmscond_t sts;
        sts = sys_gettim(wrktim);
        if (!(sts & STS_M_SUCCESS)) return sts;
#ifdef VMSTIME_64BIT
        timval = *wrktim;
    } else {
        timval = *timadr;
    }
    timval /= TIMEBASE;
    *days = (timval / TIMESIZE);
    if (day_time != NULL) *day_time = (timval % TIMESIZE);

#else
        srcptr = wrktim + 7;
    } else {

        /* Check specified time for delta... */

        if ((delta = ISDELTA(timadr))) {

            /* We have to 2's complement delta times - sigh!! */

            register int count = 8,carry = 1;
            register unsigned char *dstptr = wrktim;
            srcptr = timadr;
            do {
                carry += (~*srcptr++) & 0xFF;
                *dstptr++ = carry;
                carry = carry >> 8;
            } while (--count > 0);
            srcptr = wrktim + 7;

        } else {
            srcptr = timadr + 7;
        }
    }

    /* Extract the date and time from the quadword... */

    {
        register int count = 5,date = 0;
        register unsigned time = 0, carry;

        carry = *srcptr--;
        carry = (carry << 8) | *srcptr--;
        carry = (carry << 8) | *srcptr--;
        do {
            carry = (carry << 8) | *srcptr--;
            time = (time << 8) | (carry / TIMEBASE);
            date = (date << 8) | (time / TIMESIZE);
            carry %= TIMEBASE;
            time %= TIMESIZE;
        } while (--count > 0);

        /* Return results... */

        if (delta) {
            *days = -(int) date;
            if (day_time != NULL) *day_time = -(int) time;
        } else {
            *days = date;
            if (day_time != NULL) *day_time = time;
        }
    }
#endif
    return LIB__NORMAL;
}

/****************************************************************** lib_day() */

/* lib_day() is a routine to crack time quadwords into a day number
   and time.
*/

unsigned lib_day(int *days,pVMSTIME timadr,int *day_time)
{
    register vmscond_t sts;
    sts = vmstime_day(days,timadr,day_time);
    if (*day_time < 0) *day_time = -*day_time;
    return sts;
}

/*********************************************************** vmstime_numtim() */

/* vmstime_numtim() takes a time quadword and breaks it into a
   seven word time vector (year,month,day,hour...)
*/

unsigned vmstime_numtim(unsigned short timvec[7],pVMSTIME timadr,
                        int *days,int *day_time,int *day_of_year)
{
    register int date,time;

    /* Use vmstime_day to crack time into days/time... */

    {
        register vmscond_t sts;
        sts = vmstime_day(days,timadr,day_time);
        if (!(sts & STS_M_SUCCESS)) return sts;
        date = *days;
        time = *day_time;
    }

    /* Delta time or date? */

    if (date < 0 || time < 0) {
        timvec[0] = 0;          /* Year */
        timvec[1] = 0;          /* Month */
        timvec[2] = -date;      /* Days */
        time = -time;

    } else {

        /* Date - calculate years to quad century... */

        register int year;
        date += OFFSET_DAYS;
        year = BASE_YEAR + (date / QUAD_CENTURY_DAYS) * 400;
        date %= QUAD_CENTURY_DAYS;

        /* Add years to century - last century in quad is longer!! */

        {
            register int century = date / CENTURY_DAYS;
            if (century == 4) century = 3;
            if (century) {
                year += century * 100;
                date -= century * CENTURY_DAYS;
            }
        }

        /* Add years to quad year... */

        year += (date / QUAD_YEAR_DAYS) * 4;
        date %= QUAD_YEAR_DAYS;

        /* Finally to current year - last year in quad is longer!! */

        {
            register int yearno = date / YEAR_DAYS;
            if (yearno == 4) yearno = 3;
            if (yearno) {
                year += yearno;
                date -= yearno * YEAR_DAYS;
            }
        }
        if (day_of_year != NULL) *day_of_year = date;

        /* Non-leap adjustment for years which have no Feb 29th */

        if (date++ > 58) {
            if (year % 4) {
                date++;
            } else {
                if ((year % 100) == 0 && year % 400) date++;
            }
        }
        /* Figure out month and return results... */

        {
            register int month = 1;
            while (date > vmstime_mthend[month]) month++;

            timvec[0] = year;   /* Year */
            timvec[1] = month;  /* Month */
            timvec[2] = date - vmstime_mthend[month - 1];       /* Days */
        }
    }

    /* Return time information... */

    timvec[6] = time % 100;     /* Hundredths */
    time /= 100;
    timvec[5] = time % 60;      /* Seconds */
    time /= 60;
    timvec[4] = time % 60;      /* Minutes */
    timvec[3] = time / 60;      /* Hours */

    return SS__NORMAL;
}

/*************************************************************** sys_numtim() */

/* sys_numtim() takes a time quadword and breaks it into a
   seven word time vector (year,month,day,hour...)
*/

unsigned sys_numtim(unsigned short timvec[7],pVMSTIME timadr)
{
    int days,day_time;
    return vmstime_numtim(timvec,timadr,&days,&day_time,NULL);
}

/*********************************************************** vmstime_getnum() */

/* Define internal tables... */

static const char vmstime_punct[] = "::.";
static const char vmstime_digits[] = "0123456789";
static const char vmstime_months[] = "-JAN-FEB-MAR-APR-MAY-JUN-JUL-AUG-SEP-OCT-NOV-DEC-";

/* vmstime_getnum() internal routine to convert a printable number to binary */

char *vmstime_getnum(char *ptr,char *end,unsigned short *result,int *scale)
{
    register int numval = 0;
    register int scaleval = 1;
    register char *chrptr = ptr;
    do {
        register int binval = 0;
        register char digit = *chrptr++;
        do {
            if (digit == vmstime_digits[binval]) break;
        } while (++binval < 10);
        numval = numval * 10 + binval;
        scaleval *= 10;
    } while (chrptr < end && isdigit(*chrptr));
    *result = numval;
    *scale = scaleval;
    return chrptr;
}

/*************************************************************** sys_bintim() */

#define NOVALUE 0xFFFF

/* sys_bintim() takes a printable time and converts it to a time quadword */

unsigned sys_bintim(struct dsc_descriptor *timbuf,pVMSTIME timadr)
{
    int scale;
    unsigned short wrktim[7];
    register int field_count = 0;
    register char *chrptr = timbuf->dsc_a_pointer;
    register char *endptr = chrptr + timbuf->dsc_w_length;

    memset( wrktim, 0, sizeof(wrktim) );

    /* Skip any spaces... */

    while (chrptr < endptr && *chrptr == ' ') chrptr++;

    /* Get the day number or delta days... */

    if (chrptr < endptr && isdigit(*chrptr)) {
        chrptr = vmstime_getnum(chrptr,endptr,&wrktim[2],&scale);
        field_count++;
    } else {
        wrktim[2] = NOVALUE;
    }

    /* Check for month separator "-" - if found then we have a date! */

    if (chrptr < endptr && *chrptr == '-') {
        chrptr++;

        wrktim[0] = wrktim[1] = NOVALUE;
        wrktim[3] = wrktim[4] = wrktim[5] = wrktim[6] = NOVALUE;

        /* See if there is a month... */

        if (chrptr + 2 < endptr && *chrptr != '-') {
            register int month = 1;
            register char upmth0 = toupper(chrptr[0]);
            register char upmth1 = toupper(chrptr[1]);
            register char upmth2 = toupper(chrptr[2]);
            register const char *mthptr = vmstime_months + 1;
            do {
                if (upmth0 == mthptr[0] &&
                    upmth1 == mthptr[1] &&
                    upmth2 == mthptr[2]) break;
                mthptr += 4;
            } while (++month <= 12);
            chrptr += 3;
            wrktim[1] = month;
            field_count++;
        }
        /* Now look for year... */

        if (chrptr < endptr && *chrptr == '-') {
            chrptr++;
            if (chrptr < endptr && isdigit(*chrptr)) {
                chrptr = vmstime_getnum(chrptr,endptr,&wrktim[0],&scale);
                field_count++;
            }
        }
    } else {

        /* Set default values for delta time... */

        wrktim[0] = wrktim[1] = 0;
        wrktim[3] = wrktim[4] = wrktim[5] = wrktim[6] = 0;
        if (wrktim[2] == NOVALUE) wrktim[2] = 0;

        /* If no space then just a time value? */

        if (chrptr < endptr && *chrptr != ' ') {
            wrktim[3] = wrktim[2];
            wrktim[2] = 0;
        }
        field_count = 7;        /* Don't use current time! */
    }

    /* Skip any spaces... */

    while (chrptr < endptr && *chrptr == ' ') chrptr++;

    /* Extract time fields... Note: we don't round hundredths into seconds */

    {
        register int time_field;
        for (time_field = 0; time_field < 4 && chrptr < endptr; time_field++) {
            if (chrptr < endptr && isdigit(*chrptr)) {
                chrptr = vmstime_getnum(chrptr,endptr,&wrktim[time_field + 3],
                                        &scale);
                field_count++;
                if (time_field == 3) {
                    register unsigned hundredth = wrktim[6] * 1000 / scale;
                    if (hundredth < 995) hundredth += 5;
                    wrktim[6] = hundredth / 10;
                    break;
                }
            }
            if (chrptr < endptr && time_field < 3 &&
                *chrptr == vmstime_punct[time_field]) chrptr++;
        }
    }

    /* Skip any spaces... */

    while (chrptr < endptr && *chrptr == ' ') chrptr++;

    /* If anything left then we have a problem... */

    if (chrptr < endptr) return SS__IVTIME;

    /* If some fields not specified use current time...*/

    if (field_count < 7) {
        register int field;
        unsigned short curtim[7];
        register vmscond_t sts = sys_numtim(curtim,NULL);
        if (!(sts & STS_M_SUCCESS)) return sts;
        for (field = 0; field < 7; field++) {
            if (wrktim[field] == NOVALUE) wrktim[field] = curtim[field];
        }
    }
    return lib_cvt_vectim(wrktim,timadr);
}

/*************************************************************** sys_asctim() */

/* sys_asctim() converts a time quadword into printable form...  */

unsigned sys_asctim(unsigned short *timlen,struct dsc_descriptor *timbuf,
                    pVMSTIME timadr,unsigned cvtflg)
{
    unsigned short timvec[7];
    register int count,timval;
    register char *chrptr = timbuf->dsc_a_pointer;
    register char *endptr = chrptr + timbuf->dsc_w_length;

    /* First use sys_numtim to get the date/time fields... */

    {
        register vmscond_t sts;
        sts = sys_numtim(timvec,timadr);
        if (!(sts & STS_M_SUCCESS)) return sts;
    }

    /* See if we want delta days or date... */

    if (cvtflg == 0) {

        /* Check if date or delta time... */

        if (timvec[0]) {

            /* Generate two digit day... */

            if( chrptr < endptr ) {
                if( (timval = timvec[2]) / 10 == 0 ) {
                    *chrptr++ = ' ';
                } else {
                    *chrptr++ = vmstime_digits[timval / 10];
                }

                if( chrptr < endptr ) {
                    *chrptr++ = vmstime_digits[timval % 10];
                }
            }
            /* Add month name with hyphen separators... */

            if ((count = (int)(endptr - chrptr)) > 5) count = 5;
            memcpy(chrptr,vmstime_months + (((size_t)timvec[1] - 1) * 4),count);
            chrptr += count;

            /* Get year... */

            timval = timvec[0];

        } else {

            /* Get delta days... */

            timval = timvec[2];
        }

        /* Common code for year number and delta days! Spaces first... */

        count = 1000;
        if (timval < count && chrptr < endptr) {
            do {
                *chrptr++ = ' ';
                count /= 10;
            } while (timval < count && count > 1 && chrptr < endptr);
        } else {
            if (timval >= 10000) count = 10000;
        }
        /* Then digits... */

        if (chrptr < endptr) {
            do {
                *chrptr++ = vmstime_digits[timval / count];
                timval = timval % count;
                count /= 10;
            } while (count > 0 && chrptr < endptr);
        }

        /* Put a space between date and time... */

        if (chrptr < endptr) *chrptr++ = ' ';

    }
    /* Move onto time fields... hh:mm:ss.hh */

    if (chrptr < endptr) {
        count = 0;
        do {
            timval = timvec[count + 3];
            *chrptr++ = vmstime_digits[timval / 10];
            if (chrptr >= endptr) break;
            *chrptr++ = vmstime_digits[timval % 10];
            if (count >= 3 || chrptr >= endptr) break;
            *chrptr++ = vmstime_punct[count++];
        } while (chrptr < endptr);
    }

    /* We've done it - return length... */

    if (timlen != NULL) *timlen = (int)((chrptr - (char *) timbuf->dsc_a_pointer));

    return SS__NORMAL;
}

/********************************************************** lib_day_of_week() */

/* lib_day_of_week() computes day of week from time quadword... */

unsigned lib_day_of_week(pVMSTIME timadr,unsigned *weekday)
{
    int days;
    register vmscond_t sts;

    /* Use lib_day to crack quadword... */

    sts = lib_day(&days,timadr,NULL);
    if (sts & STS_M_SUCCESS) {
        *weekday = ((days + 2) % 7) + 1;
    }
    return sts;
}

/****************************************************** lib_mult_delta_time() */

/* lib_mult_delta_time multiplies a delta time by a scalar integer... */

unsigned lib_mult_delta_time(int *multiple,pVMSTIME timadr)
{
    register int factor = *multiple;

    /* Check for delta time... */

    if (!ISDELTA(timadr)) return SS__IVTIME;

    /* Use absolute factor... */

    if (factor < 0) factor = -factor;

#ifdef VMSTIME_64BIT
    *timadr *= factor;
#else
    {
        register int count = 8;
        register unsigned carry = 0;
        register unsigned char *ptr = timadr;
        do {
            carry += *ptr * factor;
            *ptr++ = carry;
            carry = (carry >> 8);
        } while (--count > 0);
    }
#endif
    return LIB__NORMAL;
}

/************************************************************ lib_add_times() */

/* lib_add_times() adds two quadword time values */

unsigned lib_add_times(pVMSTIME time1,pVMSTIME time2,pVMSTIME result)
{
    if (ISDELTA(time1)) {
        if (ISDELTA(time2)) {
#ifdef VMSTIME_64BIT
            *result = *time1 + *time2;
        } else {
            *result = *time2 - *time1;
#else
            lib_addx(time1,time2,result,NULL);
        } else {
            lib_subx(time2,time1,result,NULL);
#endif
        }
    } else {
        if (ISDELTA(time2)) {
#ifdef VMSTIME_64BIT
            *result = *time1 - *time2;
#else
            lib_subx(time1,time2,result,NULL);
#endif
        } else {
            return LIB__ONEDELTIM;
        }
    }
    return LIB__NORMAL;
}

/************************************************************ lib_sub_times() */

/* lib_sub_times() subtracts two quadword time values */

unsigned lib_sub_times(pVMSTIME time1,pVMSTIME time2,pVMSTIME result)
{
#ifdef VMSTIME_64BIT
    if (ISDELTA(time1) != ISDELTA(time2)) {
        *result = *time1 + *time2;
    } else {
        if (*time1 < *time2) {
            *result = *time1 - *time2;
        } else {
            *result = *time2 - *time1;
        }
    }
#else
    if (ISDELTA(time1) != ISDELTA(time2)) {
        lib_addx(time1,time2,result,NULL);
    } else {
        if (vmstime_compare(time1,time2) < 0) {
            lib_subx(time1,time2,result,NULL);
        } else {
            lib_subx(time2,time1,result,NULL);
        }
    }
#endif
    return LIB__NORMAL;
}

/*********************************************** lib_cvt_from_internal_time() */

/* lib_cvt_from_internal_time() extracts a time field from a time value */

unsigned lib_cvt_from_internal_time(unsigned *operation,
                                    unsigned *result,pVMSTIME input_time)
{
    register unsigned resval;
    unsigned short timvec[7];
    int days,day_time,day_of_year;
    if (*operation >= LIB$K_MAX_OPERATION) return LIB__INVOPER;
    {
        register vmscond_t sts;
        sts = vmstime_numtim(timvec,input_time,&days,&day_time,&day_of_year);
        if (!(sts & STS_M_SUCCESS)) return sts;
    }
    if (timvec[0] != 0) {
        switch (*operation) {
            case LIB$K_MONTH_OF_YEAR:
                resval = timvec[1];
                break;
            case LIB$K_DAY_OF_YEAR:
                resval = day_of_year + 1;
                break;
            case LIB$K_HOUR_OF_YEAR:
                resval = day_of_year * 24 + timvec[3];
                break;
            case LIB$K_MINUTE_OF_YEAR:
                resval = (day_of_year * 24 + timvec[3]) * 60 + timvec[4];
                break;
            case LIB$K_SECOND_OF_YEAR:
                resval =
                    ((day_of_year * 24 + timvec[3]) * 60 + timvec[4]) * 60 +
                    timvec[5];
                break;
            case LIB$K_DAY_OF_MONTH:
                resval = timvec[2];
                break;
            case LIB$K_HOUR_OF_MONTH:
                resval = (timvec[2] - 1) * 24 + timvec[3];
                break;
            case LIB$K_MINUTE_OF_MONTH:
                resval = ((timvec[2] - 1) * 24 + timvec[3]) * 60 + timvec[4];
                break;
            case LIB$K_SECOND_OF_MONTH:
                resval =
                    (((timvec[2] - 1) * 24 + timvec[3]) * 60 + timvec[4]) * 60 +
                    timvec[5];
                break;
            case LIB$K_DAY_OF_WEEK:
                resval = ((days + 2) % 7) + 1;
                break;
            case LIB$K_HOUR_OF_WEEK:
                resval = ((days + 2) % 7) * 24 + timvec[3];
                break;
            case LIB$K_MINUTE_OF_WEEK:
                resval = (((days + 2) % 7) * 24 + timvec[3]) * 60 + timvec[4];
                break;
            case LIB$K_SECOND_OF_WEEK:
                resval =
                    ((((days + 2) % 7) * 24 + timvec[3]) * 60 + timvec[4]) *
                    60 + timvec[5];
                break;
            case LIB$K_HOUR_OF_DAY:
                resval = timvec[3];
                break;
            case LIB$K_MINUTE_OF_DAY:
                resval = timvec[3] * 60 + timvec[4];
                break;
            case LIB$K_SECOND_OF_DAY:
                resval = (timvec[3] * 60 + timvec[4]) * 60 + timvec[5];
                break;
            case LIB$K_MINUTE_OF_HOUR:
                resval = timvec[4];
                break;
            case LIB$K_SECOND_OF_HOUR:
                resval = timvec[4] * 60 + timvec[5];
                break;
            case LIB$K_SECOND_OF_MINUTE:
                resval = timvec[5];
                break;
            case LIB$K_JULIAN_DATE:
                resval = days;
                break;
            default:
                return LIB__DELTIMREQ;
        }
    } else {
        switch (*operation) {
            case LIB$K_DELTA_WEEKS:
                resval = timvec[2] / 7;
                break;
            case LIB$K_DELTA_DAYS:
                resval = timvec[2];
                break;
            case LIB$K_DELTA_HOURS:
                resval = timvec[2] * 24 + timvec[3];
                break;
            case LIB$K_DELTA_MINUTES:
                resval = (timvec[2] * 24 + timvec[3]) * 60 + timvec[4];
                break;
            case LIB$K_DELTA_SECONDS:
                resval =
                    ((timvec[2] * 24 + timvec[3]) * 60 + timvec[4]) * 60 +
                    timvec[5];
                break;
            case 0:
            case LIB$K_DELTA_WEEKS_F:
            case LIB$K_DELTA_DAYS_F:
            case LIB$K_DELTA_HOURS_F:
            case LIB$K_DELTA_MINUTES_F:
            case LIB$K_DELTA_SECONDS_F:
                return LIB__INVOPER;
            default:
                return LIB__ABSTIMREQ;
        }
    }
    *result = resval;
    return LIB__NORMAL;
}

/************************************************* lib_cvt_to_internal_time() */

static const unsigned char vmstime_oneweek[] =   {0x00,0xc0,0x1b,0xd7,0x7f,0xfa,0xff,0xff};
static const unsigned char vmstime_oneday[] =    {0x00,0x40,0x96,0xd5,0x36,0xff,0xff,0xff};
static const unsigned char vmstime_onehour[] =   {0x00,0x98,0x3b,0x9e,0xf7,0xff,0xff,0xff};
static const unsigned char vmstime_oneminute[] = {0x00,0xba,0x3c,0xdc,0xff,0xff,0xff,0xff};
static const unsigned char vmstime_onesecond[] = {0x80,0x69,0x67,0xff,0xff,0xff,0xff,0xff};

/* lib_cvt_to_internal_time() converts a time field to a time value */

unsigned lib_cvt_to_internal_time(unsigned *operation,int *input,
                                  pVMSTIME result)
{
    unsigned const char *ptr;
    if (*input < 1) return LIB__IVTIME;
    switch (*operation) {
        case LIB$K_DELTA_WEEKS:
            ptr = vmstime_oneweek;
            break;
        case LIB$K_DELTA_DAYS:
            ptr = vmstime_oneday;
            break;
        case LIB$K_DELTA_HOURS:
            ptr = vmstime_onehour;
            break;
        case LIB$K_DELTA_MINUTES:
            ptr = vmstime_oneminute;
            break;
        case LIB$K_DELTA_SECONDS:
            ptr = vmstime_onesecond;
            break;
        default:
            return LIB__INVOPER;
    }
    memcpy(result,ptr,sizeof(VMSTIME));
    return lib_mult_delta_time(input,result);
}

/********************************************************** vmstime_from_nt() */

/* Difference between VMS and NT is 94187 days.... */

static const unsigned char vmstime_nt_offset[] = {0x00,0xc0,0xac,0x76,0x88,0xe3,0xde,0xfe};

/* vmstime_from_nt() convert Windows NT time to vmstime */

unsigned vmstime_from_nt(pVMSTIME nt_time,pVMSTIME vms_time)
{
    if (ISDELTA(nt_time)) return LIB__IVTIME;
    return lib_sub_times(nt_time,(pVMSTIME) vmstime_nt_offset,vms_time);
}

/************************************************************ vmstime_to_nt() */

/* vmstime_to_nt() convert vmstime to Windows NT time */

unsigned vmstime_to_nt(pVMSTIME vms_time,pVMSTIME nt_time)
{
    if (ISDELTA(vms_time)) return LIB__IVTIME;
    return lib_add_times(vms_time,(pVMSTIME) vmstime_nt_offset,nt_time);
}
