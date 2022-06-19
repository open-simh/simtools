# /*
gcc float.c -o float -lm
./float
exit $?
*/
/*
 * Parsing PDP-11 floating point literals.
 * Testing ground for different implementations.
*/
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <ieeefp.h>

#define DEBUG_FLOAT     1

void
printflt(unsigned *flt, int size)
{
    printf("%06o: ",        flt[0]);
    printf("sign:  %d ",   (flt[0] & 0x8000) >> 15);
    printf("uexp:  %x ",   (flt[0] & 0x7F80) >>  7);
    printf("ufrac: %02x",   flt[0] & 0x007F);

    for (int i = 1; i < size; i++) {
        printf(" %04x", flt[i]);
    }

    printf("\n");
}

#if DEBUG_FLOAT
#define DF(...)   printf(__VA_ARGS__)
#else
#define DF(...)
#endif

/*
 * We need 56 bits of mantissa.
 *
 * Try to detect if it is needed, possible and useful to use
 * long double instead of double, when parsing floating point numbers.
 */

#if DBL_MANT_DIG >= 56
/* plain double seems big enough */
# define USE_LONG_DOUBLE        0
/* long double exists and seems big enough */
#elif LDBL_MANT_DIG >= 56
# define USE_LONG_DOUBLE        1
#elif defined(LDBL_MANT_DIG)
/* long double exists but is probably still too small */
# define USE_LONG_DOUBLE        1
#else
/* long double does not exist and plain double is too small */
# define USE_LONG_DOUBLE        0
#endif

#if USE_LONG_DOUBLE
# define DOUBLE                 long double
# define SCANF_FMT              "%Lf"
# define FREXP                  frexpl
#else
# define DOUBLE                 double
# define SCANF_FMT              "%lf"
# define FREXP                  frexp
#endif


/* Parse PDP-11 64-bit floating point format. */
/* Give a pointer to "size" words to receive the result. */
/* Note: there are probably degenerate cases that store incorrect
   results.  For example, I think rounding up a FLT2 might cause
   exponent overflow.  Sorry. */
/* Note also that the full 56 bits of precision probably aren't always
   available on the source platform, given the widespread application
   of IEEE floating point formats, so expect some differences.  Sorry
   again. */

int parse_float(
    char *cp,
    int size,
    unsigned *flt)
{
    DOUBLE          d;          /* value */
    DOUBLE          frac;       /* fractional value */
    uint64_t        ufrac;      /* fraction converted to 56 bit
                                   unsigned integer */
    uint64_t        onehalf;    /* one half of the smallest bit
                                   (used for rounding) */
    int             i;          /* Number of fields converted by sscanf */
    int             n;          /* Number of characters converted by sscanf */
    int             sexp;       /* Signed exponent */
    unsigned        uexp;       /* Unsigned excess-128 exponent */
    unsigned        sign = 0;   /* Sign mask */

    i = sscanf(cp, SCANF_FMT "%n", &d, &n);
    if (i == 0)
        return 0;                      /* Wasn't able to convert */
    DF("LDBL_MANT_DIG: %d\n", LDBL_MANT_DIG);
    DF("%Lf input: %s\n", d, cp);

    cp += n;

    if (d == 0.0) {
        for (i = 0; i < size; i++) {
            flt[i] = 0; /* All-bits-zero equals zero */
        }
        return 1;                      /* Good job. */
    }

    frac = FREXP(d, &sexp);            /* Separate into exponent and mantissa */
    DF("frac: %Lf %La sexp: %d\n", frac, frac, sexp);
    if (sexp < -127 || sexp > 127)
        return 0;                      /* Exponent out of range. */

    uexp = sexp + 128;                  /* Make excess-128 mode */
    uexp &= 0xff;                       /* express in 8 bits */
    DF("uexp: %02x\n", uexp);

    /*
     * frexp guarantees its fractional return value is
     *   abs(frac) >= 0.5    and  abs(frac) < 1.0
     * Another way to think of this is that:
     *   abs(frac) >= 2**-1  and  abs(frac) < 2**0
     */

    if (frac < 0) {
        sign = (1 << 15);              /* Negative sign */
        frac = -frac;                  /* fix the mantissa */
    }

    /*
     * For the PDP-11 floating point representation the
     *  fractional part is 7 bits (for 16-bit floating point
     *  literals), 23 bits (for 32-bit floating point values),
     *  or 55 bits (for 64-bit floating point values).
     * However the bit immediately above the MSB is always 1
     *  because the value is normalized.  So it's actually
     *  8 bits, 24 bits, or 56 bits.
     * We effectively multiply the fractional part of our value by
     *  2**56 to fully expose all of those bits (including
     *  the MSB which is 1).
     * However as an intermediate step, we really multiply by
     *  2**57, so we get one lsb for possible later rounding
     *  purposes. After that, we divide by 2 again.
     */

    /* The following big literal is 2 to the 57th power: */
    ufrac = (uint64_t) (frac * 144115188075855872.0); /* Align fraction bits */
    DF("ufrac: %016lx\n", ufrac);
    DF("56   : %016lx\n", (1UL<<57) - 2);

    /*
     * ufrac is now >= 2**56 and < 2**57.
     *  This means it's normalized: bit [56] is 1
     *  and all higher bits are 0.
     */

    /* Round from 57-bits to 56, 24, or 8.
     * We do this by:
     * + first adding a value equal to one half of the
     *   least significant bit (the value 'onehalf')
     * + (possibly) dealing with any carrying that
     *   causes the value to no longer be normalized
     *   (with bit [56] = 1 and all higher bits = 0)
     * + shifting right by 1 bit (which throws away
     *   the 0 bit).  Note this step could be rolled
     *   into the next step.
     * + taking the remaining highest order 8,
     *   24, or 56 bits.
     *
     * +--+--------+-------+ +--------+--------+
     * |15|14     7|6     0| |15      |       0|
     * +--+--------+-------+ +--------+--------+
     * | S|EEEEEEEE|MMMMMMM| |MMMMMMMM|MMMMMMMM| ...maybe 2 more words...
     * +--+--------+-------+ +--------+--------+
     *  Sign (1 bit)
     *     Exponent (8 bits)
     *              Mantissa (7 bits)
     */

    onehalf = 1ULL << (16 * (4-size));
    ufrac += onehalf;
    DF("onehalf=%016lx, ufrac+onehalf: %016lx\n", onehalf, ufrac);

    /* Did it roll up to a value 2**56? */
    if ((ufrac >> 57) > 0) {           /* Overflow? */
        if (uexp < 0xFF) {
            ufrac >>= 1;               /* Normalize */
            uexp++;
            DF("ufrac: %016lx  uexp: %02x (normalized)\n", ufrac, uexp);
        } else {
            /*
             * If rounding and then normalisation would cause the exponent to
             * overflow, just don't round: the cure is worse than the disease.
             * We could detect ahead of time but the conditions for all size
             * values may be a bit complicated, and so rare, that it is more
             * readable to just undo it here.
             */
            ufrac -= onehalf;
            DF("don't round: exponent overflow\n");
        }
    }

    ufrac >>= 1;                       /* Go from 57 bits to 56 */

    flt[0] = (unsigned) (sign | (uexp << 7) | ((ufrac >> 48) & 0x7F));
    if (size > 1) {
        flt[1] = (unsigned) ((ufrac >> 32) & 0xffff);
        if (size > 2) {
            flt[2] = (unsigned) ((ufrac >> 16) & 0xffff);
            flt[3] = (unsigned) ((ufrac >> 0) & 0xffff);
        }
    }

    return 1;
}

/*
 * Several of these functions assume that "unsigned" contains more
 * than 16 bits. Bit 16 is sometimes used to store a carry.
 */

#define DUMP DF("exp: %d. %d  %04x %04x %04x %04x\n", \
                    float_dec_exponent, float_bin_exponent, \
                    float_buf[0], float_buf[1], float_buf[2], float_buf[3])
#define DUMP4(b, t) DF("%04x %04x %04x %04x%s", \
                        b[0], b[1], b[2], b[3], t)

void float_copy(                /* XMIT4 at FLTGSV, more or less */
    unsigned *to,
    unsigned *from)
{
    to[0] = from[0] & 0xFFFF;
    to[1] = from[1] & 0xFFFF;
    to[2] = from[2] & 0xFFFF;
    to[3] = from[3] & 0xFFFF;
}

int float_right_shift(          /* FLTGRS */
    unsigned *buf)
{
    int carry1, carry2;

    carry1 = buf[0] & 0x0001; buf[0] >>= 1;
    carry2 = buf[1] & 0x0001; buf[1] >>= 1; buf[1] |= (carry1 << 15);
    carry1 = buf[2] & 0x0001; buf[2] >>= 1; buf[2] |= (carry2 << 15);
    carry2 = buf[3] & 0x0001; buf[3] >>= 1; buf[3] |= (carry1 << 15);

    return carry2;
}

int float_left_shift(           /* FLTGLS */
    unsigned *buf)
{
    int carry1, carry2;

    carry1 = buf[3] >> 15; buf[3] <<= 1;
    carry2 = buf[2] >> 15; buf[2] <<= 1; buf[2] |= (carry1 & 0x0001);
    carry1 = buf[1] >> 15; buf[1] <<= 1; buf[1] |= (carry2 & 0x0001);
    carry2 = buf[0] >> 15; buf[0] <<= 1; buf[0] |= (carry1 & 0x0001);

    float_copy(buf, buf);       /* Clean up carries */

    return carry2 & 0x01;
}

void float_add(                 /* FLTGAD */
    unsigned *to,
    unsigned *from)
{
    //DF("float_add: "); DUMP4(to, ""); DF(" += "); DUMP4(from, "\n");
    to[3] += from[3];
    to[2] += from[2] + ((to[3] >> 16) & 0x0001);
    to[1] += from[1] + ((to[2] >> 16) & 0x0001);
    to[0] += from[0] + ((to[1] >> 16) & 0x0001);
    //DF("         = "); DUMP4(to, "\n");

    float_copy(to, to);       /* Clean up carries */
}

void float_mult_by_5(           /* FLTM50 */
    unsigned *buf,
    unsigned *workspace)
{
    float_copy(workspace, buf); /* Save 1 x original value */
    float_left_shift(buf);      /* 2 x */
    float_left_shift(buf);      /* 4 x */
    float_add(buf, workspace);  /* 5 x */
}

void float_mult_by_5_4th(       /* FLTM54 */
    int *bin_exp,
    unsigned *buf,
    unsigned *workspace)
{
    if (buf[0] >= 0146314) {
        float_right_shift(buf);
        ++*bin_exp;
    }
    float_copy(workspace, buf);
    float_right_shift(buf);     /* half of the original */
    float_right_shift(buf);     /* a quarter */
    float_add(buf, workspace);  /* add the original to the quarter */
}

int parse_float_m2(
    char *cp,
    int size,
    unsigned *flt)
{
    int float_sign = 0;
    int float_dot = 0;
    int float_dec_exponent = 0;
    int float_bin_exponent = 0;
    unsigned float_buf[4] = { 0 };
    unsigned float_save[4];

    float_bin_exponent = 65;

    if (*cp == '+') {
        cp++;
    } else if (*cp == '-') {
        float_sign = 0100000;
        cp++;
    }
    DF("float_sign: %d\n", float_sign);

    for (;;) {
        if (isdigit(*cp)) {
            /* Can we multiply by 10? */
            DF("digit: %c\n", *cp);
            if (float_buf[0] & 0174000) {
                /* No, that would overflow */
                float_dec_exponent++; /* no, compensate for the snub */
                /*
                 * Explanation of the above comment:
                 * - after the decimal separator, we should ignore extra digits
                 *   completely. Since float_dot == -1, the exponent will be
                 *   decremented below, and compensate for that here.
                 * - before the decimal separator, we don't add the digit
                 *   (which would overflow) so we lose some lesser significant
                 *   bits. float_dot == 0 in this case, and we do want the
                 *   exponent increased to compensate for the ignored digit.
                 * So in both cases the right thing happens.
                 */
            } else {
                /* Multiply by 10 */
                float_mult_by_5(float_buf, float_save);
                float_left_shift(float_buf);

                /* Add digit */
                float_buf[3] += *cp - '0';
                /* Ripple carry */
                float_buf[2] += (float_buf[3] >> 16) & 0x0001;
                float_buf[1] += (float_buf[2] >> 16) & 0x0001;
                float_buf[0] += (float_buf[1] >> 16) & 0x0001;
                float_copy(float_buf, float_buf); /* Clean up carries */
            }
            float_dec_exponent += float_dot;
            DUMP;
            cp++;
        } else if (*cp == '.') {
            DF("dot: %c\n", *cp);
            if (float_dot < 0) {
                // error...
                printf("Error: repeated decimal separator\n");
                return 0;
            }
            float_dot = -1;
            cp++;
        } else {
            DF("Other char: %c\n", *cp);
            break;
        }
    }

    if (toupper(*cp) == 'E') {
        cp++;
        int exp = strtol(cp, &cp, 10);

        float_dec_exponent += exp;
        DF("E%d -> dec_exp %d\n", exp, float_dec_exponent);
    }

    /* FLTG3 */
    if (float_buf[0] | float_buf[1] | float_buf[2] | float_buf[3]) {
        /* Non-zero */
        DF("Non-zero: decimal exponent: %d\n", float_dec_exponent);
        while (float_dec_exponent > 0) { /* 31$ */
            DUMP;
            if (float_buf[0] <= 031426) {       /* 13078, 0x3316, 65390 / 5 */
                /* Multiply by 5 and 2 */
                DF("Multiply by 5 and 2\n");
                float_mult_by_5(float_buf, float_save);
                float_bin_exponent++;
                DUMP;
            } else {
                /* Multiply by 5/4 and 8 32$ */
                DF("Multiply by 5/4 and 8\n");
                float_mult_by_5_4th(&float_bin_exponent, float_buf, float_save);
                float_bin_exponent += 3;
                DUMP;
            }
            float_dec_exponent--;
        }

        while (float_dec_exponent < 0) { /* 41$ ish */
            DUMP;
            DF("Prepare for division by left-shifting the bits\n");
            /* Prepare for division by left-shifting the bits */
            while ((float_buf[0] & 0x8000) == 0) {     /* 41$ */
                float_bin_exponent--;                   /* 40$ */
                float_left_shift(float_buf);
                DUMP;
            }

            /* Divide by 2 */
            float_right_shift(float_buf);

            float_copy(float_save, float_buf);
            DUMP;
            DF("float_save: "); DUMP4(float_save, "\n");

            /* Divide by 5:
             * multiplying by (2**32 - 1) / 5 = 0x333333333
             * while dropping the 32 least significant bits.
             * This is nearly exact but exact enough?
             * 2**32 - 1 is a multiple of 5.
             */
            for (int i = 16 * 2; i > 0; i--) {
                if ((i & 1) == 0) {                    /* 42$ */
                    float_right_shift(float_buf);
                    float_right_shift(float_buf);
                }
                float_right_shift(float_buf);           /* 43$ */
                float_add(float_buf, float_save);
                DF("Loop i=%2d: ", i); DUMP;
            }

            float_bin_exponent -= 3;
            float_dec_exponent++;
            DUMP;
        }

        /* Normalize the mantissa: shift a single 1 out to the left */
        DF("Normalize the mantissa: shift a single 1 out to the left\n");
        int carry;
        do {
            /* FLTG5 */
            float_bin_exponent--;
            carry = float_left_shift(float_buf);
            DUMP;
        } while (carry == 0);

        /* Set excess 128. */
        DF("Set excess 128.\n");
        float_bin_exponent += 0200;
        DUMP;

        if (float_bin_exponent & 0xFF00) {
            /* Error N. Underflow. 2$ */
            printf("Error N (underflow)\n");
            return 0;
        }

        /* Shift right 9 positions to make room for sign and exponent 3$ */
        DF("Shift right 9 positions to make room for sign and exponent\n");
        int round = (float_buf[3] >> 8) & 0x0001;
        float_buf[3] >>= 9; float_buf[3] |= (float_buf[2] << (16-9)) & 0xff80;
        float_buf[2] >>= 9; float_buf[2] |= (float_buf[1] << (16-9)) & 0xff80;
        float_buf[1] >>= 9; float_buf[1] |= (float_buf[0] << (16-9)) & 0xff80;
        float_buf[0] >>= 9;
        float_buf[0] |= float_bin_exponent << 7;
        DUMP;
        /*
         * float_bin_exponent is included because of the location of
         * FLTBEX ;BINARY EXPONENT (MUST PRECEED FLTBUF)
         */

        /* Round (this is optional, really) */
        if (1) {
            if (size < 4) {
                round = float_buf[size] & 0x8000;
            }
            /* The rounding bit is the lesser significant bit that's just
             * outside the returned result. If we round, we add it to the
             * returned value.
             *
             * If there is a carry-out of the mantissa, it gets added to
             * the exponent (increasing it by 1).
             *
             * If that also overflows, we truely have overflow.
             */

            if (round) {
                for (int i = size - 1 ; round && i >= 0; i--) {
                    float_buf[i]++;     /* 5$ */
                    round = float_buf[i] & 0x10000;      /* carry */
                    DF("round, i=%d: ", i); DUMP;
                }
            }
            DF("After rounding\n");
            DUMP;
        }

        if (float_buf[0] & 0x8000) {
            // 6$ error T: exponent overflow
            printf("Error T\n");
            return 0;
        }

        /* 7$ */
        float_buf[0] |= float_sign;
        DF("Put in float_sign: "); DUMP;
    }

    /* Now put together the result from the parts */
    flt[0] = float_buf[0] & 0xFFFF;
    if (size > 1) {
        flt[1] = float_buf[1] & 0xFFFF;
        if (size > 2) {
            flt[2] = float_buf[2] & 0xFFFF;
            flt[3] = float_buf[3] & 0xFFFF;
        }
    }

    return 1;
}

static void
mult64to128(uint64_t op1, uint64_t op2, uint64_t *hi, uint64_t *lo)
{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ >= 16
    __uint128_t result = (__uint128_t)op1 * op2;
    *lo = result;
    *hi = result >> 64;
#else
    uint64_t u1 = (op1 & 0xffffffff);
    uint64_t v1 = (op2 & 0xffffffff);
    uint64_t t = (u1 * v1);
    uint64_t w3 = (t & 0xffffffff);
    uint64_t k = (t >> 32);

    op1 >>= 32;
    t = (op1 * v1) + k;
    k = (t & 0xffffffff);
    uint64_t w1 = (t >> 32);

    op2 >>= 32;
    t = (u1 * op2) + k;
    k = (t >> 32);

    *hi = (op1 * op2) + w1 + k;
    *lo = (t << 32) + w3;
#endif
}

#define DUMP3 DF("exp: %d. %d  %016llx\n", \
                    float_dec_exponent, float_bin_exponent, float_buf)

int parse_float_m3(
    char *cp,
    int size,
    unsigned *flt)
{
    int float_sign = 0;
    int float_dot = 0;
    int float_dec_exponent = 0;
    int float_bin_exponent = 0;
    uint64_t float_buf = 0;

    float_bin_exponent = 65;

    if (*cp == '+') {
        cp++;
    } else if (*cp == '-') {
        float_sign = 1;
        cp++;
    }
    DF("float_sign: %d\n", float_sign);

    for (;;) {
        if (isdigit(*cp)) {
            /* Can we multiply by 10? */
            DF("digit: %c\n", *cp);
            if (float_buf & 0xF800000000000000ULL) { /* [0] & 0174000, 0xF800 */
                /* No, that would overflow */
                float_dec_exponent++; /* no, compensate for the snub */
                /*
                 * Explanation of the above comment:
                 * - after the decimal separator, we should ignore extra digits
                 *   completely. Since float_dot == -1, the exponent will be
                 *   decremented below, and compensate for that here.
                 * - before the decimal separator, we don't add the digit
                 *   (which would overflow) so we lose some lesser significant
                 *   bits. float_dot == 0 in this case, and we do want the
                 *   exponent increased to compensate for the ignored digit.
                 * So in both cases the right thing happens.
                 */
            } else {
                /* Multiply by 10 */
                float_buf *= 10;
                /* Add digit */
                float_buf += *cp - '0';
            }
            float_dec_exponent -= float_dot;
            DUMP3;
            cp++;
        } else if (*cp == '.') {
            DF("dot: %c\n", *cp);
            if (float_dot) {
                // error...
                printf("Error: repeated decimal separator\n");
                return 0;
            }
            float_dot = 1;
            cp++;
        } else {
            DF("Other char: %c\n", *cp);
            break;
        }
    }

    if (toupper(*cp) == 'E') {
        cp++;
        int exp = strtol(cp, &cp, 10);

        float_dec_exponent += exp;
        DF("E%d -> dec_exp %d\n", exp, float_dec_exponent);
    }

    /* FLTG3 */
    if (float_buf) {
        /* Non-zero */
        DF("Non-zero: decimal exponent: %d\n", float_dec_exponent);
        while (float_dec_exponent > 0) { /* 31$ */
            DUMP3;
            if (float_buf <= 0x3316000000000000ULL) { /* 031426, 13078, 0x3316, 65390 / 5 */
                /* Multiply by 5 and 2 */
                DF("Multiply by 5 and 2\n");
                float_buf *= 5;
                float_bin_exponent++;
                DUMP3;
            } else {
                /* Multiply by 5/4 and 8 32$ */
                DF("Multiply by 5/4 and 8\n");
                if (float_buf >= 0xCCCC000000000000ULL) {
                    float_buf >>= 1;
                    float_bin_exponent++;
                }
                float_buf += (float_buf >> 2);
                float_bin_exponent += 3;
                DUMP3;
            }
            float_dec_exponent--;
        }

        while (float_dec_exponent < 0) { /* 41$ ish */
            DUMP3;
            DF("Prepare for division by left-shifting the bits\n");
            /* Prepare for division by left-shifting the bits */
            while (((float_buf >> 63) & 1) == 0) {     /* 41$ */
                float_bin_exponent--;                  /* 40$ */
                float_buf <<= 1;
                DUMP3;
            }

            /* Divide by 2 */
            float_buf >>= 1;

            uint64_t float_save = float_buf;
            DUMP3;
            DF("float_save: %016llx\n", float_save);

            /* Divide by 5:
             * multiplying by (2**66 - 4) / 5 = 0xCCCCCCCCCCCCCCCC
             * while dropping the 64 least significant bits.
             * This is nearly exact but exact enough?
             * Somehow there is another doubling in here, so the
             * final result = start * 8 / 5.
             */
            for (int i = 16 * 2; i > 0; i--) {
                if ((i & 1) == 0) {             /* 42$ */
                    float_buf >>= 2;
                }
                float_buf >>= 1;                /* 43$ */
                float_buf += float_save;
                DF("Loop i=%2d: ", i); DUMP3;
            }
#if 1
            { /* Method 3 */
                uint64_t hi, lo;
                mult64to128(float_save, 0x9999999999999999ULL, &hi, &lo);
                /*
                 * Really multiply with the 65-bit number
                 *                     0x19999999999999999
                 *                     1 1001 1001 1001 ...
                 * which is 8 *          0011 0011 0011 ... aka 0x333...
                 * which is (2**64 - 1) / 5.
                 */
                uint64_t result = hi + float_save;
                if (result == float_buf) {
                    printf("Same               333 loop and *9999: %016llx vs %016llx\n", float_buf, result);
                    printf("                              was    : %016llx\n", float_save);
                } else {
                    printf("Difference between 333 loop and *3333: %016llx vs %016llx\n", float_buf, result);
                    printf("                                     : %016llx    %016llx\n", hi, lo);
                    printf("                              was    : %016llx\n", float_save);
                }
            }
#endif
#if 1   /* Try other methods to calculate the same thing more directly */
            {
                uint64_t result = float_save / 5 * 8;
                /* Try to fill in some of the lesser significant bits */
                int round = float_save % 5;
                if (round) {
                    result += round * 8 / 5;
                } else {
                    //result--;
                }
# if 0
                if (result == float_buf) {
                    printf("Same               333 loop and /5 *8: %016llx vs %016llx\n", float_buf, result);
                } else {
                    printf("Difference between 333 loop and /5 *8: %016llx vs %016llx\n", float_buf, result);
                    printf("                             was     : %016llx    %d %d\n", float_buf, float_save % 20, (float_save % 20) * 2 / 5);
                }
# endif
                float_buf = result;
                printf("after / 5 * 8: ");
                DUMP3;
            }
#endif
#if 0
            {
                __uint128_t big = (__uint128_t)float_save << 3;

                uint64_t result = big / 5;

                if (float_save % 5 == 0)
                    result--;

                if (result == float_buf) {
                    printf("Same               333 loop and *8 /5: %016llx vs %016llx\n", float_buf, result);
                } else {
                    printf("Difference between 333 loop and *8 /5: %016llx vs %016llx\n", float_buf, result);
                }

                /*
                 * Rounding is slightly different, in particular for start
                 * values that are multiples of 5 but some other cases too.
                 */
            }
#endif

            /* It's not simply dividing by 5, it also multiplies by 8,
             * so we need to adjust the exponent here. */
            float_bin_exponent -= 3;
            float_dec_exponent++;
            DUMP3;
        }

        /* Normalize the mantissa: shift a single 1 out to the left */
        DF("Normalize the mantissa: shift a single 1 out to the left\n");
        int carry;
        do {
            /* FLTG5 */
            float_bin_exponent--;
            carry = (float_buf >> 63) & 1;
            float_buf <<= 1;
            DUMP3;
        } while (carry == 0);

        /* Set excess 128. */
        DF("Set excess 128.\n");
        float_bin_exponent += 0200;
        DUMP3;

        if (float_bin_exponent & 0xFF00) {
            /* Error N. Underflow. 2$ */
            printf("Error N (underflow)\n");
            return 0;
        }

        /* Shift right 9 positions to make room for sign and exponent 3$ */
        DF("Shift right 9 positions to make room for sign and exponent\n");
        int round = (float_buf >> 8) & 0x0001;
        float_buf >>= 9;
        float_buf |= (uint64_t)float_bin_exponent << (64-9);
        DUMP3;

        /*
         * This rounding step seems always needed to make the result the same
         * as the implementation with long doubles.
         * This may be because of the slight imprecision of the divisions by 10?
         * However there is something to be said for the argument that when you
         * want a 1 or 2 word result, rounding twice is wrong.
         * The reference implementation omits this rounding for size != 4.
         */
        float_buf += round;
        DF("round: size = 4, round = %d\n", round);

        /* Round (there is a truncation option to omit this step) */
        if (1) {
            if (size < 4) {
                /* 1 << 31 or 1 << 47 */
                uint64_t onehalf = 1ULL << ((16 * (4-size)) - 1);
                DUMP3;
                DF("round: size = %d, onehalf = %016llx\n", size, onehalf);
                float_buf += onehalf;
                DUMP3;

                /* The rounding bit is the lesser significant bit that's just
                 * outside the returned result. If we round, we add it to the
                 * returned value.
                 *
                 * If there is a carry-out of the mantissa, it gets added to
                 * the exponent (increasing it by 1).
                 *
                 * If that also overflows, we truely have overflow.
                 */
            }

            DF("After rounding\n");
            DUMP3;
        }

        if (float_buf & 0x8000000000000000ULL) {
            // 6$ error T: exponent overflow
            printf("Error T: exponent overflow\n");
            return 0;
        }

        /* 7$ */
        float_buf |= (uint64_t)float_sign << 63;
        DF("Put in float_sign: "); DUMP3;
    }

    /* Now put together the result from the parts */
    flt[0] = (float_buf >> 48) & 0xFFFF;
    if (size > 1) {
        flt[1] = (float_buf >> 32) & 0xFFFF;
        if (size > 2) {
            flt[2] = (float_buf >> 16) & 0xFFFF;
            flt[3] = (float_buf >>  0) & 0xFFFF;
        }
    }

    return 1;
}

void
test_one(char *input, unsigned expected0, unsigned expected1, unsigned expected2, unsigned expected3)
{
    unsigned result[4];
    unsigned result2[4];

    printf("------------------------\n");
    parse_float_m3(input, 4, result);

    if (result[0] != expected0 ||
        result[1] != expected1 ||
        result[2] != expected2 ||
        result[3] != expected3) {
        printf("Unexpected result: %04x %04x %04x %04x  from %s\n", result[0], result[1], result[2], result[3], input);
        printf("  expected       : %04x %04x %04x %04x\n", expected0, expected1, expected2, expected3);
        printflt(result, 4);
    }

#if 0
    parse_float_m2(input, 4, result2);
    if (result2[0] != expected0 ||
        result2[1] != expected1 ||
        result2[2] != expected2 ||
        result2[3] != expected3) {
        printf("Unexpected result2: %04x %04x %04x %04x from %s\n", result2[0], result2[1], result2[2], result2[3], input);
        printf("  expected        : %04x %04x %04x %04x\n", expected0, expected1, expected2, expected3);
        printflt(result2, 4);
    }
#endif
}

int
main(int argc, char **argv)
{
    /* Should print 64 */
    DF("LDBL_MANT_DIG: %d\n", LDBL_MANT_DIG);

    test_one("100E-2", 040200, 0000000, 0000000, 0000000);
#if 1
    test_one("10000000000E-10", 040200, 0000000, 0000000, 0000000);
    test_one("1.0", 040200, 0000000, 0000000, 0000000);
    test_one("0.1", 037314, 0146314, 0146314, 0146315);
    test_one("1.0E5",  0044303, 0050000, 0000000, 0000000);
    test_one("1.0E10", 0050425, 0001371, 0000000, 0000000);
    test_one("1.0E20", 0060655, 0074353, 0142654, 0061000);
    test_one("1.0E30", 0071111, 0171311, 0146404, 0063517);
    test_one("1.0E38", 0077626, 0073231, 0050265, 0006611);
    test_one("1.701411834604692307e+38", 077777, 0177777, 0177777, 0177777);
#endif
    test_one("0.994140625", 0040176, 0100000, 000000, 000000);
    //                         407e     8000    0000    0000
#if 1
    test_one("0.998046875", 0040177, 0100000, 000000, 000000);
    test_one("1.00390625",  0040200, 0100000, 000000, 000000);
    test_one("1.01171875",  0040201, 0100000, 000000, 000000);
    test_one("0.999999910593032836914062", 0040177, 0177776, 0100000, 0000000);
    test_one("0.999999970197677612304687", 0040177, 0177777, 0100000, 0000000);
    test_one("1.00000005960464477539062", 0040200, 0000000, 0100000, 0000000);
    test_one("1.000000178813934326171875", 0040200, 0000001, 0100000, 0000000);
    test_one("1.0000000000000000138777878078144567552953958511353", 0040200, 0000000, 0000000, 0000000);
    test_one("1.0000000000000000416333634234433702658861875534058", 0040200, 0000000, 0000000, 0000001);
    test_one("0.99999999999999997918331828827831486705690622329712", 0040177, 0177777, 0177777, 0177776);
    test_one("0.99999999999999999306110609609277162235230207443237", 0040177, 0177777, 0177777, 0177777);
    //test_one("", 0, 0, 0, 0);
    test_one("6.66666", 040725, 052507, 055061, 0122276);
    test_one("170141183460469230846243588177825628160", 0100000, 0000000, 0000000, 0000000);    // T error
    test_one("170141183460469230564930741053754966016", 0077777, 0177777, 0177777, 0177777);
    //test_one("3.1415926535897932384626433", 0, 0, 0, 0);
    //test_one("", 0, 0, 0, 0);
    //
    /* First a number that should just fit in a 56 bit mantissa. */
    /* 1 << 56 */
    test_one("72057594037927936", 056200, 000000, 000000, 000000);
    /* A bit more requires more precision, so they are rounded. */
    test_one("72057594037927937", 056200, 000000, 000000, 000001);
    test_one("72057594037927938", 056200, 000000, 000000, 000001);

    /* Lower numbers should all be represented exactly */
    test_one("72057594037927935", 056177, 0177777, 0177777, 0177777);
    test_one("72057594037927934", 056177, 0177777, 0177777, 0177776);
    test_one("72057594037927933", 056177, 0177777, 0177777, 0177775);

    /* 1 << 57 should also be exactly representable */
    test_one("144115188075855872", 056400, 000000, 000000, 000000);

    /* 1 less lacks one significant bit so will be rounded up */
    test_one("144115188075855871", 056400, 000000, 000000, 000000);

    /* Same for 1 more, rounded down */
    test_one("144115188075855873", 056400, 000000, 000000, 000000);

    /* but 2 more should show up in the lsb */
    /* This one seems most clearly problematic */
    test_one("144115188075855874", 056400, 000000, 000000, 000001);

    // Some numbers around some of the magic numbers in the parser
    test_one("3681129745421959167", 0057514, 0054000, 0, 0);
    test_one("3681129745421959168", 0057514, 0054000, 0, 0); // 0x3316000000000000
    test_one("3681129745421959169", 0057514, 0054000, 0, 0);
    test_one("3681129745421959170", 0057514, 0054000, 0, 0);

    test_one("14757170078986272767", 0060114, 0146000, 0000000, 0000000);
    test_one("14757170078986272768", 0060114, 0146000, 0000000, 0000000); // 0xCCCC000000000000
    test_one("14757170078986272769", 0060114, 0146000, 0000000, 0000000);
    test_one("14757170078986272780", 0060114, 0146000, 0000000, 0000000);

#endif

    return 0;
}
