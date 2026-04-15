/*
 * Generic parsing routines.
 */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"                     /* my own definitions */

#include "assemble_globals.h"
#include "listing.h"
#include "rad50.h"
#include "util.h"

#define DEBUG_LSB       0  /* See also assemble.c */


/* skipwhite - used everywhere to advance a char pointer past spaces */

char           *skipwhite(
    char *cp)
{
    while (*cp == ' ' || *cp == '\t')
        cp++;
    return cp;
}


/* skipdelim - used everywhere to advance between tokens.  Whitespace
   and one comma are allowed delims. */

char           *skipdelim(
    char *cp)
{
    cp = skipwhite(cp);
    if (*cp == ',')
        cp = skipwhite(cp + 1);
    return cp;
}


/* skipdelim_comma - used to advance between tokens.  Whitespace
   and one comma are allowed delims.
   Set *comma depending on whether a comma was skipped. */

char           *skipdelim_comma(
    char *cp,
    int  *comma)
{
    cp = skipwhite(cp);
    *comma = (*cp == ',');
    if (*comma) {
        cp = skipwhite(cp + 1);
    }
    return cp;
}


/*
 * check_eol - check that we're at the end of a line.
 * Complain if not.
 */
int check_eol(
    STACK *stack,
    char *cp)
{
    cp = skipwhite(cp);

    if (EOL(*cp)) {
        return 1;
    }

    {
        int len = strcspn(cp, "\n");

        report_err(stack->top, "Junk at end of line ('%.*s')\n",
               (len > 20) ? 20 : len, cp);
    }

    return 0;
}


/* Parses a string from the input stream. */
/* If not bracketed by <...> or ^/.../, then */
/* the string is delimited by trailing comma or whitespace. */
/* Allows nested <>'s */

char           *getstring(
    char *cp,
    char **endp)
{
    int             len;
    int             start;
    char           *str;

    if (!brackrange(cp, &start, &len, endp)) {
        start = 0;
        len = strcspn(cp, " \t\n,;");
        if (endp)
            *endp = cp + len;
    }

    str = memcheck(malloc(len + 1));
    memcpy(str, cp + start, len);
    str[len] = 0;

    return str;
}


/* Parses a string from the input stream for .include and .library.
 * These have a special kind of delimiters. It likes
 * .include /name/ ?name? \name\ "name"
 * but not
 * .include ^/name/ <name> name =name= :name:
 * .include :name: seems to be silently ignored ... but ...
 * .include :name: is equivalent to creating two labels (.INCLUDE: NAME:)
 *
 * This should probably follow the similar rules to .ASCII
 * although that is not mentioned in the manual,
 * and MACRO-11 V05.05 does not support it.
 *
 * As an extension to MACRO-11 we allow for the device name and
 * directory name to be ignored.  This is so we can assemble the
 * same .MAC source-code on platforms which do not support these.
 * Command line options are '-nodev' and/or '-nodir'.
 */

char           *getstring_fn(
    char *cp,
    char **endp)
{
    char            endstr[4];
    int             len;

    if (*cp == '<')    /* MACRO-11 V05.05 exits with a ".INCLUDE directive file error" */
        return NULL;

    if (STRICT)  /* Disallow alpha-numeric quote characters */
        if (!ispunct((unsigned char) *cp))
            return NULL;

    endstr[0] = (char) toupper((unsigned char) *cp);  /* MACRO-11 treats upper- and ...  */
    endstr[1] = (char) tolower((unsigned char) *cp);  /* ... lower-case alike (see .REM) */
    endstr[2] = '\n';
    endstr[3] = '\0';
    cp++;

    len = strcspn(cp, endstr);

    if (!RELAXED)  /* Disallow [empty file name and] mismatched quote characters */
        if (len == 0 || cp[len] == '\n' || cp[len] == '\0')
            return NULL;

    if (endp)
        *endp = cp + len + 1;

    /* Parse -nodev and -nodir */
    {
        char           *str;
        int             lhs = 0;
        int             dirbeg = 0;
        int             nambeg;
        int             i;

        for (i = 0; i < len; i++) {
            if (!isalnum((unsigned char) cp[i]))
                break;
        }
        if (cp[i] == ':')
            dirbeg = i + 1;

        nambeg = dirbeg;
        if (cp[dirbeg] == '[') {
            i = strcspn(&cp[dirbeg], "]\n") + dirbeg;
            if (cp[i] == ']')
               nambeg = i + 1;
        }

        if (STRINGENT)  /* Disallow empty file name */
            if (cp[nambeg] == endstr[0] || cp[nambeg] == endstr[1] || cp[nambeg] == '\n' || cp[nambeg] == '\0')
                return NULL;

	if (ignore_fn_dev) {
            lhs  = dirbeg;
            len -= dirbeg;
        }

        str = memcheck(malloc(len + 1));  /* Always allocate space for the directory (even if -nodir) */
        if (ignore_fn_dir) {
            len -= nambeg - dirbeg;
            if (lhs < dirbeg) {  /* -nodir AND NOT -nodev, BUT we have a device */
                memcpy(str, cp, dirbeg);
                memcpy(&str[dirbeg], &cp[nambeg], len);
            } else {             /* -nodev and -nodir */
                memcpy(str, &cp[nambeg], len);
            }
        } else {                 /* Not -nodir (but possibly -nodev) */
            memcpy(str, &cp[lhs], len);
        }
        str[len] = '\0';
        return str;
    }
}


/* Get what would be the operation code from the line.  */
/* Used to find the ends of streams without evaluating them, like
   finding the closing .ENDM on a macro definition */

SYMBOL         *get_op(
    char *cp,
    char **endp)
{
    int             local;
    char           *label;
    SYMBOL         *op;

    for (;;) {
        cp = skipwhite(cp);
        if (EOL(*cp))
            return NULL;

        label = get_symbol(cp, &cp, &local);
        if (label == NULL)
            return NULL;                   /* No operation code. */

        cp = skipwhite(cp);
        if (*cp != ':')
            break;

        /* It is a label definition - so skip it */

        cp = skipwhite(cp + 1);        /* We could be STRINGENT here */
        if (*cp == ':')
            cp++;                      /* Skip it */
        free(label);
    }

    op = lookup_sym(label, &system_st);
    free(label);

    if (endp)
        *endp = cp;

    return op;
}


/* get_mode - parse a general addressing mode. */

int get_mode(
    char *cp,
    char **endp,
    ADDR_MODE *mode,
    char **error)
{
    EX_TREE        *value;

    mode->offset = NULL;
    mode->pcrel = 0;
    mode->type = MODE_REG;

    cp = skipwhite(cp);

    /* @ means "indirect," sets bit 3 */
    if (*cp == '@') {
        cp = skipwhite(cp + 1);
        mode->type |= MODE_INDIRECT;
    }

    /* Immediate modes #imm and @#imm */
    if (*cp == '#') {
        cp = skipwhite(cp + 1);
        mode->type |= MODE_AUTO_INCR | MODE_PC;

        mode->offset = parse_expr(cp, 0);
        if (endp)
            *endp = mode->offset->cp;
        {
            int ok = expr_ok(mode->offset);

            if (!ok) {
                *error = "Invalid expression after '#'";
                free_tree(mode->offset);
                mode->offset = NULL;
            }
            return ok;
        }
    }

    /* Check for -(Rn) */

    if (*cp == '-') {
        char           *tcp = skipwhite(cp + 1);

        if (*tcp++ == '(') {
            unsigned        reg;

            /* It's -(Rn) */
            value = parse_expr(tcp, 0);
            reg = get_register(value);
            if (reg == NO_REG) {
                *error = "Register expected after '-('";
            }
            if (tcp = skipwhite(value->cp), *tcp++ != ')') {
                *error = "')' expected after register";
                free_tree(value);
                return FALSE;
            }
            mode->type |= MODE_AUTO_DECR | reg;
            if (endp)
                *endp = tcp;
            free_tree(value);
            return TRUE;
        }
    }

    /* Check for (Rn) */
    if (*cp == '(') {
        char           *tcp;
        unsigned        reg;

        value = parse_expr(cp + 1, 0);
        reg = get_register(value);

        if (reg == NO_REG) {
            *error = "Register expected after '('";
        }
        if (tcp = skipwhite(value->cp), *tcp++ != ')') {
            *error = "')' expected after register";
            free_tree(value);
            return FALSE;
        }

        tcp = skipwhite(tcp);
        if (*tcp == '+') {
            tcp++;                     /* It's (Rn)+ */
            if (endp)
                *endp = tcp;
            mode->type |= MODE_AUTO_INCR | reg;
            free_tree(value);
            return TRUE;
        }

        if (mode->type == MODE_INDIRECT) { /* For @(Rn) there's an
                                              implied 0 offset */
            mode->offset = new_ex_lit(0);
            mode->type |= MODE_OFFSET | reg;
            free_tree(value);
            if (endp)
                *endp = tcp;
            return TRUE;
        }

        mode->type |= MODE_INDIRECT | reg; /* Mode 10 is register indirect
                                              as in (Rn) */
        free_tree(value);
        if (endp)
            *endp = tcp;
        return TRUE;
    }

    /* Modes with an offset */

    mode->offset = parse_expr(cp, 0);

    if (!expr_ok(mode->offset)) {
        *error = "Invalid expression";
        free_tree(mode->offset);
        mode->offset = NULL;
        return FALSE;
    }

    cp = skipwhite(mode->offset->cp);

    if (*cp == '(') {
        unsigned        reg;

        /* indirect register plus offset */
        value = parse_expr(cp + 1, 0);
        reg = get_register(value);
        if (reg == NO_REG) {
            *error = "Register expected after 'offset('";
        }
        if (cp = skipwhite(value->cp), *cp++ != ')') {
            *error = "')' expected after 'offset(register'";
            free_tree(value);
            free_tree(mode->offset);
            mode->offset = NULL;
            return FALSE;              /* Syntax error in addressing mode */
        }

        mode->type |= MODE_OFFSET | reg;

        free_tree(value);

        if (endp)
            *endp = cp;
        return TRUE;
    }

    /* Plain old expression. */

    if (endp)
        *endp = cp;

    /* It might be a register, though. */
    if (mode->offset->type == EX_SYM) {
        SYMBOL         *sym = mode->offset->data.symbol;

        if (sym->section->type == SECTION_REGISTER) {
            free_tree(mode->offset);
            mode->offset = NULL;
            mode->type |= sym->value;
            if (sym->value == REG_ERR_VALUE) {
                *error = "Invalid register";
            }
            return TRUE;
        }
    }

    /* It's either 067 (PC-relative) or 037 (absolute) mode, depending */
    /* on user option. */

    if (mode->type & MODE_INDIRECT) {  /* Have already noted indirection? */
        mode->type |= MODE_OFFSET|MODE_PC;/* If so, then PC-relative is the only
                                          option */
        mode->pcrel = 1;               /* Note PC-relative */
    } else if (ENABL(AMA)) {           /* User asked for absolute adressing? */
        mode->type |= MODE_INDIRECT|MODE_AUTO_INCR|MODE_PC;
                                       /* Give it to him. */
    } else {
        mode->type |= MODE_OFFSET|MODE_PC; /* PC-relative */
        mode->pcrel = 1;               /* Note PC-relative */
    }

    return TRUE;
}


/* get_fp_src_mode - parse an immediate fp literal or a general mode */

int get_fp_src_mode(
    char *cp,
    char **endp,
    ADDR_MODE *mode,
    char **error)
{
    char *savecp = (cp = skipwhite(cp));

    if (cp[0] == '#') {
        unsigned flt[1];
        char *fltendp = NULL;
        int ret = parse_float((cp = skipwhite(cp + 1)), &fltendp, 1, flt);

        if (ret) {
            mode->type = MODE_AUTO_INCR | MODE_PC;
            mode->pcrel = 0;
            mode->offset = new_ex_lit(flt[0]);
            mode->offset->cp = fltendp;

            if (endp)
                *endp = mode->offset->cp;

            return TRUE;
        } else if (fltendp) {
            /* it looked like a fp number but something was wrong with it */
        }
    }

    return get_mode(savecp, endp, mode, error);
}

#define DEBUG_FLOAT     0

#if DEBUG_FLOAT
void printflt(
    unsigned *flt,
    int size)
{
    int i;

    printf("%06o: ",        flt[0]);
    printf("sign:  %d ",   (flt[0] & 0x8000) >> 15);
    printf("uexp:  %x ",   (flt[0] & 0x7F80) >>  7);
    printf("ufrac: %02x",   flt[0] & 0x007F);

    for (i = 1; i < size; i++) {
        printf(" %04x", flt[i]);
    }

    printf("\n");
}
#endif

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

#define PARSE_FLOAT_WITH_FLOATS         0
#define PARSE_FLOAT_WITH_INTS           1
#define PARSE_FLOAT_DIVIDE_BY_MULT_LOOP 0

/* Parse PDP-11 64-bit floating point format. */
/* Give a pointer to "size" words to receive the result. */
/* Note: there are probably degenerate cases that store incorrect
   results.  For example, I think rounding up a FLT2 might cause
   exponent overflow.  Sorry. */

#if PARSE_FLOAT_WITH_FLOATS

/* Note also that the full 56 bits of precision probably aren't always
   available on the source platform, given the widespread application
   of IEEE floating point formats, so expect some differences.  Sorry
   again. */

int parse_float(
    char *cp,
    char **endp,
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
    DF("%Lf input: %s", d, cp);

    cp += n;
    if (endp)
        *endp = cp;

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

#elif PARSE_FLOAT_WITH_INTS

#define DUMP3 DF("exp: %d. %d  %016llx\n", \
                    float_dec_exponent, float_bin_exponent, float_buf)
/*
 * Parse floating point denotations using integer operations only.
 * Follows the reference version's implementation fairly closely.
 */
int parse_float(
    char *cp,
    char **endp,
    int size,
    unsigned *flt)
{
    int float_sign = 0;
    int float_dot = 0;
    int float_dec_exponent = 0;
    int float_bin_exponent = 0;
    int ok_chars = 0;
    uint64_t float_buf = 0;

    float_bin_exponent = 65;

    cp = skipwhite(cp);

    if (*cp == '+') {
        cp++;
    } else if (*cp == '-') {
        float_sign = 1;
        cp++;
    }
    DF("float_sign: %d\n", float_sign);

    while (!EOL(*cp)) {
        if (isdigit((unsigned char)*cp)) {
            /* Can we multiply by 10? */
            DF("digit: %c\n", *cp);
            ok_chars++;
            if (float_buf & 0xF800000000000000ULL) { /* [0] & 0174000, 0xF800 */
                /* No, that would overflow */
                float_dec_exponent++; /* no, compensate for the snub */
                /*
                 * Explanation of the above comment:
                 * - after the decimal separator, we should ignore extra digits
                 *   completely. Since float_dot == 1, the exponent will be
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
            ok_chars++;
            if (float_dot) {
                return 0;       /* Error: repeated decimal separator */
            }
            float_dot = 1;
            cp++;
        } else {
            DF("Other char: %c\n", *cp);
            if (ok_chars == 0) {
                return 0;       /* No valid number found */
            }
            break;
        }
    }

    if (toupper((unsigned char)*cp) == 'E') {
        cp++;
        {
            int exp = strtol(cp, &cp, 10);

            float_dec_exponent += exp;
            DF("E%d -> dec_exp %d\n", exp, float_dec_exponent);
        }
    }

    if (endp)
        *endp = cp;

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

#if PARSE_FLOAT_DIVIDE_BY_MULT_LOOP
            {
                uint64_t float_save = float_buf;
                DUMP3;
                DF("float_save: %016llx\n", float_save);

                /*
                 * Divide by 5: this is done by the trick of "dividing by
                 * multiplying". In order to keep as many significant bits as
                 * possible, we multiply by 8/5, and adjust the binary exponent to
                 * compensate for the factor of 8.
                 * The result is found when we drop the 64 low bits.
                 *
                 * So we multiply with the 65-bit number
                 *                     0x19999999999999999
                 *                     1 1001 1001 1001 ...
                 * which is 8 *          0011 0011 0011 ... aka 0x333...
                 * which is (2**64 - 1) / 5 aka 0xFFF... / 5.
                 *
                 * The rightmost (1 * float_save << 0) is contributed to the total
                 * because float_buf already contains that value.
                 * In loop i=32, (float_save << 3) is added:
                 *               due to the two extra conditional shifts.
                 * In loop i=31, (float_save << 4) is added.
                 * In loop i=30, (float_save << 7) is added.
                 * etc, etc,
                 * so forming the repeating bit-pattern 1100 of the multiplier.
                 *
                 * Instead of shifting float_save left, we shift float_buf right,
                 * which over time drops precisely the desired 64 low bits.
                 *
                 * This is nearly exact but exact enough.
                 *
                 * The final result = start * 8 / 5.
                 */
                for (int i = 16 * 2; i > 0; i--) {
                    if ((i & 1) == 0) {             /* 42$ */
                        float_buf >>= 2;
                    }
                    float_buf >>= 1;                /* 43$ */
                    float_buf += float_save;
                    DF("Loop i=%2d: ", i); DUMP3;
                }
            }
#else
            {
                int round = float_buf % 5;
                float_buf = float_buf / 5 * 8;

                /*
                 * Try to fill in some of the lesser significant bits.
                 * This is not always bitwise identical to the original method
                 * but probably more accurate.
                 */
                if (round) {
                    float_buf += round * 8 / 5;
                }
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
        {
            int carry;

            do {
                /* FLTG5 */
                float_bin_exponent--;
                carry = (float_buf >> 63) & 1;
                float_buf <<= 1;
                DUMP3;
            } while (carry == 0);
        }

        /* Set excess 128. */
        DF("Set excess 128.\n");
        float_bin_exponent += 0200;
        DUMP3;

        if (float_bin_exponent & 0xFF00) {
            /* Error N. Underflow. 2$ */
            report_err(NULL, "Error N (underflow)\n");
            return 0;
        }

        /* Shift right 9 positions to make room for sign and exponent 3$ */
        DF("Shift right 9 positions to make room for sign and exponent\n");
        {
            int round = (float_buf >> 8) & 0x0001;

            float_buf >>= 9;
            float_buf |= (uint64_t)float_bin_exponent << (64-9);
            DUMP3;

            /*
             * This rounding step seems always needed to make the result the same
             * as the implementation with long doubles.
             *
             * This may be because of the slight imprecision of the divisions by 10?
             *
             * It is needed to get some exact results for values that are indeed
             * exactly representable. Examples:
             *
             * (2**9-3)/2**9 = 0.994140625 = 0,11111101
             *                 407e 7fff ffff ffff -> 407e 8000 0000 0000 (correct)
             * 1.00 (or 100E-2) divides 100 by 100 and gets
             *                 407f ffff ffff ffff -> 4080 0000 0000 0000 (correct)
             *
             * The reference implementation omits this rounding for size != 4:
             * it has only one rounding step, which always depends on the size.
             */
            float_buf += round;
            DF("round: size = 4, round = %d\n", round);

            /* If .DSABL FPT (default), round the floating-point value */
            if (!ENABL(FPT)) {
                uint64_t onehalf;

                if (size < 4) {
                    /* 1 << 31 or 1 << 47 */
                    onehalf = 1ULL << ((16 * (4-size)) - 1);
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
                report_err(NULL, "error T: exponent overflow\n");
                return 0;
            }

            /* 7$ */
            float_buf |= (uint64_t)float_sign << 63;
            DF("Put in float_sign: "); DUMP3;
        }
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
#else
# error How are you going to parse floats?
#endif


/* The recursive-descent expression parser parse_expr. */

/* This parser was designed for expressions with operator precedence.
   However, MACRO-11 doesn't observe any sort of operator precedence.
   If you feel your source deserves better, give the operators
   appropriate precedence values right here. */

#define ADD_PREC 1
#define MUL_PREC 1
#define AND_PREC 1
#define OR_PREC 1
#define LSH_PREC 1

EX_TREE        *parse_unary(
    char *cp);                  /* Prototype for forward calls */

EX_TREE        *parse_binary(
    char *cp,
    char term,
    int depth)
{
    EX_TREE        *leftp,
                   *rightp,
                   *tp;

    leftp = parse_unary(cp);

    while (leftp->type != EX_ERR) {
        cp = skipwhite(leftp->cp);

        if (*cp == term)
            return leftp;

        switch (*cp) {
        case '+':
            if (depth >= ADD_PREC)
                return leftp;

            rightp = parse_binary(cp + 1, term, ADD_PREC);
            tp = new_ex_bin(EX_ADD, leftp, rightp);
            leftp = tp;
            break;

        case '-':
            if (depth >= ADD_PREC)
                return leftp;

            rightp = parse_binary(cp + 1, term, ADD_PREC);
            tp = new_ex_bin(EX_SUB, leftp, rightp);
            leftp = tp;
            break;

        case '*':
            if (depth >= MUL_PREC)
                return leftp;

            rightp = parse_binary(cp + 1, term, MUL_PREC);
            tp = new_ex_bin(EX_MUL, leftp, rightp);
            leftp = tp;
            break;

        case '/':
            if (depth >= MUL_PREC)
                return leftp;

            rightp = parse_binary(cp + 1, term, MUL_PREC);
            tp = new_ex_bin(EX_DIV, leftp, rightp);
            leftp = tp;
            break;

        case '!':
            if (depth >= OR_PREC)
                return leftp;

            rightp = parse_binary(cp + 1, term, OR_PREC);
            tp = new_ex_bin(EX_OR, leftp, rightp);
            leftp = tp;
            break;

        case '&':
            if (depth >= AND_PREC)
                return leftp;

            rightp = parse_binary(cp + 1, term, AND_PREC);
            tp = new_ex_bin(EX_AND, leftp, rightp);
            leftp = tp;
            break;

        case '_':
            if (STRICTEST || symbol_allow_underscores || depth >= LSH_PREC) {
                report_warn(NULL, "'_' is not strictly allowed\n");
            /*  return leftp;  */
            }

            rightp = parse_binary(cp + 1, term, LSH_PREC);
            tp = new_ex_bin(EX_LSH, leftp, rightp);
            leftp = tp;
            break;

        default:
            /* Some unknown character.  Let caller decide if it's okay. */

            return leftp;
        }                              /* end switch */
    }                                  /* end while */

    /* Can't be reached except by detected error. */
    return leftp;
}


/* get_symbol is used all over the place to pull a symbol out of the
   text.  */

char           *get_symbol(
    char *cp,
    char **endp,
    int *islocal)
{
    int             len;
    char           *symcp;
    int             start_digit = 0;
    int             not_digits = 0;

    cp = skipwhite(cp);                /* Skip leading whitespace */

    if (!issym((unsigned char)*cp))
        return NULL;

    if (isdigit((unsigned char)*cp))
        start_digit = 1;

    for (symcp = cp + 1; issym((unsigned char)*symcp); symcp++) {
        if (!isdigit((unsigned char)*symcp)) /* Not a digit? */
            not_digits++;                    /* Make a note. */
    }

    if (start_digit && not_digits == 0)
        return NULL;                   /* Not a symbol, it's a digit string */

    if (endp)
        *endp = symcp;

    len = (int) (symcp - cp);

    /* Now limit length */
    if (start_digit) {
        if (len > SYMMAX_MAX)
            len = SYMMAX_MAX;
    } else {
        if (len > symbol_len)
            len = symbol_len;
    }

    symcp = memcheck(malloc(len + 1));

    memcpy(symcp, cp, len);
    symcp[len] = 0;
    upcase(symcp);

    if (islocal) {
        *islocal = 0;

        /* Check if local label format */
        if (start_digit) {
            if (not_digits == 1 && symcp[len - 1] == '$') {
                char           *newsym = memcheck(malloc(32));  /* Overkill */
                long            symnum = strtol(symcp, NULL, 10);

                if (symnum <= 0 || symnum > MAX_LOCSYM) {
                    if (STRICT || symnum < 0 || symnum > BAD_LOCSYM) {
                        report_err(NULL, "Local symbol '%s' is out of range\n", symcp);
                        if (symnum < 0 || symnum > BAD_LOCSYM)
                            symnum = BAD_LOCSYM;  /* This may cause duplicates etc. */
                    }
                    /* 0$ and > MAX_LOCSYM$ will be used unchanged else BAD_LOCSYM$ */
                }

                sprintf(newsym, "%ld$%d", symnum, lsb);
#if DEBUG_LSB
                if (enabl_debug > 1 && lstfile) {
                    fprintf(lstfile, "lsb %d: %s -> %s\n",
                            lsb, symcp, newsym);
                }
#endif
                free(symcp);
                symcp = newsym;
                *islocal = SYMBOLFLAG_LOCAL;
                lsb_used++;
            } else {
                free(symcp);
                return NULL;
            }
        }
    } else {
        /* Disallow local label format */
        if (start_digit) {
            free(symcp);
            return NULL;
        }
    }

    return symcp;
}


/*
  brackrange is used to find a range of text which may or may not be
  bracketed.
  If the brackets are <>, then nested brackets are detected.
  If the brackets are of the form ^/.../ no detection of nesting is
  attempted.
  Using brackets ^<...< will mess this routine up.  What in the world
  are you thinking?
*/

int brackrange(
    char *cp,
    int *start,
    int *length,
    char **endp)
{
    char            endstr[] = "x\n";
    int             len = 0;
    int             nest;

    switch (*cp) {
    case '^':                   /* ^/text/ */
        endstr[0] = cp[1];
        *start = 2;
        cp += *start;
        len = strcspn(cp, endstr);
        break;
    case '<':                   /* <may<be>nested> */
        *start = 1;
        cp += *start;
        nest = 1;

        while (nest > 0) {
            int             sublen;

            sublen = strcspn(cp + len, "<>\n");
            if (cp[len + sublen] == '<') {
                nest++;
                sublen++;       /* include nested starting delimiter */
            } else {
                nest--;
                if (nest > 0 && cp[len + sublen] == '>')
                    sublen++;   /* include nested ending delimiter */
            }
            len += sublen;
            if (sublen == 0)
                break;
        }
        break;
    default:
        return FALSE;
    }

    /*
     * If we see a newline here, the proper terminator must be missing.
     * Don't use EOL() to check: it recognizes ';' too.
     * Unfortunately we can't issue a diagnostic here.
     */
    if (!cp[len] || cp[len] == '\n') {
        return FALSE;
    }

    *length = len;
    if (endp) {
        *endp = cp + len + 1;   /* skip over ending delimiter */
    }

    return 1;
}


/* parse_unary parses out a unary operator or leaf expression.  */

EX_TREE        *parse_unary(
    char *cp)
{
    EX_TREE        *tp;

    /* Skip leading whitespace */
    cp = skipwhite(cp);

    if (*cp == '%') {                  /* Register notation */
        tp = new_ex_una(EX_REG, parse_unary(cp + 1));
        return tp;
    }

    /* Unary negate */
    if (*cp == '-') {
        tp = new_ex_una(EX_NEG, parse_unary(cp + 1));
        return tp;
    }

    /* Unary + I can ignore. */
    if (*cp == '+')
        return parse_unary(cp + 1);

    if (*cp == '^') {
        int             save_radix;

        if (!STRINGENT)
            cp = skipwhite(cp + 1) - 1;

        switch (tolower((unsigned char)cp[1])) {
        case 'c':
            /* ^C, ones complement */
            tp = new_ex_una(EX_COM, parse_unary(cp + 2));
            return tp;
        case 'b':
            /* ^B, binary radix modifier */
            save_radix = radix;
            radix = 2;
            tp = parse_unary(cp + 2);
            radix = save_radix;
            return tp;
        case 'o':
            /* ^O, octal radix modifier */
            save_radix = radix;
            radix = 8;
            tp = parse_unary(cp + 2);
            radix = save_radix;
            return tp;
        case 'd':
            /* ^D, decimal radix modifier */
            save_radix = radix;
            radix = 10;
            tp = parse_unary(cp + 2);
            radix = save_radix;
            return tp;
        case 'x':
            /* ^X, hexadecimal radix modifier */
            save_radix = radix;
            radix = 16;
            tp = parse_unary(cp + 2);
            radix = save_radix;
            return tp;
        case 'r':
            /* ^R, RAD50 literal */  {
                int             start,
                                len;
                char           *endcp;
                unsigned        value;

                cp += 2; /* bracketed range is an extension */
                if (brackrange(cp, &start, &len, &endcp)) {
                   value = rad50(cp + start, NULL);
                    if (STRICTER)
                        report_warn(NULL, "^R<...> is not strictly allowed\n");
                 } else {
                    value = rad50(cp, &endcp);
                    /* It turns out that ^R allows extra characters;
                     * it will stop consuming input at the first
                     * non-RAD50 character.  It is actually documented. */
                    while (ascii2rad50 (*endcp) != -1)
                        endcp++;
                }
                tp = new_ex_lit(value);
                tp->cp = endcp;
                return tp;
            }
        case 'f':
            /* ^F, single-word floating point literal indicator */  {
                unsigned        flt[1];
                char           *endcp;

                if (!parse_float(cp + 2, &endcp, 1, flt)) {
                    tp = ex_err(NULL, cp + 2);
                } else {
                    tp = new_ex_lit(flt[0]);
                    tp->cp = endcp;
                }
                return tp;
            }
        case 'p':
            /* psect limits, low or high */
            {
                char            bound;
                int             islocal = 0;
                char           *endcp = NULL;
                char           *psectname;
                SYMBOL         *sectsym;

                cp += 2;
                bound = (char) tolower((unsigned char) *cp);
                cp = skipwhite(cp + 1);

                if (bound != 'l' && bound != 'h') {
                /*  report_err(NULL, "^p?: '%c' not recognized\n", bound);  */
                    return ex_err(NULL, cp);
                }

                psectname = get_symbol(cp, &endcp, &islocal);
                if (!psectname) {
                /*  report_err(NULL, "^p%c: Invalid psect name: %.*s\n", bound, strcspn(cp, "\n"), cp);  */
                    return ex_err(NULL, endcp);
                }

                sectsym = lookup_sym(psectname, &section_st);
                if (sectsym && !islocal) {
                    SECTION *psect = sectsym->section;

                    tp = new_ex_tree(EX_SYM);
                    tp->data.symbol = sectsym;
                    tp->cp = cp;

                    if (bound == 'l') {
                        ; /* that's it */
                    } else  /* if (bound == 'h') */ {
                        EX_TREE *rightp = new_ex_lit(psect->size);
                        tp = new_ex_bin(EX_ADD, tp, rightp);
                        /* TODO: Find out why 'SYM = ^phPSECT*2' is complex */
                    }
                } else {
                /*  report_err(NULL, "%^p%c: psect name %s not found\n", bound, psectname);  */
                    if (!endcp) {
                        endcp = cp;
                    }
                    /* During the first pass it is allowed that the psect is not found. */
                    tp = ex_err(new_ex_lit(0), endcp);
                }
                free(psectname);

                if (STRICTEST && tp->type != EX_ERR) {
                    report_warn(NULL, "^p%C is not strictly allowed\n", bound);
                /*  return ex_err(tp, endcp);  */
                }

                tp->cp = endcp;
                return tp;
            }
        }

        if (ispunct((unsigned char)cp[1])) {
            char           *ecp;

            /* oddly-bracketed expression like this: ^/expression/ */
            tp = parse_binary(cp + 2, cp[1], 0);
            ecp = skipwhite(tp->cp);

            if (*ecp != cp[1])
                return ex_err(tp, ecp);

            tp->cp = ecp + 1;
            return tp;
        }
    }

    /* Bracketed subexpression */
    if (*cp == '<') {
        char           *ecp;

        tp = parse_binary(cp + 1, '>', 0);
        ecp = skipwhite(tp->cp);
        if (*ecp != '>')
            return ex_err(tp, ecp);

        tp->cp = ecp + 1;
        return tp;
    }

    /* Check for ASCII constants */

    if (*cp == '\'') {
        /* 'x single ASCII character */
        cp++;
        if (*cp == '\n') {
           tp = ex_err(new_ex_lit(0), cp);
        } else {
            tp = new_ex_lit(*cp & 0xff);
            tp->cp = ++cp;
        }
        return tp;
    }

    if (*cp == '\"') {
        /* "xx ASCII character pair */
        cp++;
        if (cp[0] == '\n')
            tp = ex_err(new_ex_lit(0), cp);
        else if (cp[1] == '\n')
            tp = ex_err(new_ex_lit(cp[0] & 0xff), cp + 1);
        else {
            tp = new_ex_lit((cp[0] & 0xff) | ((cp[1] & 0xff) << 8));
            tp->cp = cp + 2;
        }
        return tp;
    }

    /* Numeric constants are trickier than they need to be, */
    /* since local labels start with a digit too. */
    if (isdigit((unsigned char)*cp)) {
        char           *label;
        int             local;

        if ((label = get_symbol(cp, NULL, &local)) == NULL) {
            char           *endcp;
            unsigned long   value;
            int             rad = radix;

            /* get_symbol returning NULL assures me that it's not a
               local label.  */

            /* Look for a trailing period, to indicate decimal... */
            for (endcp = cp; isdigit((unsigned char)*endcp); endcp++) ;
            if (*endcp == '.')
                rad = 10;

            value = strtoul(cp, &endcp, rad);
            if (*endcp == '.')
                endcp++;

            if (STRICTEST && value > 0xffff) {
                tp = ex_err(new_ex_lit(value /* & 0xffff */), endcp);
            } else {
                tp = new_ex_lit(value);
                tp->cp = endcp;
            }
            return tp;
        }

        free(label);
    }

    /* Now check for a symbol */
    {
        char           *label;
        int             local;
        SYMBOL         *sym;

        /* Optimization opportunity: I don't really need to call
           get_symbol a second time. */

        label = get_symbol(cp, &cp, &local);
        if (!label) {
            tp = ex_err(NULL, cp);     /* Not a valid label. */
            return tp;
        }

        sym = lookup_sym(label, &symbol_st);
        if (sym == NULL) {
            /* A symbol from the "PST", which means an instruction
               code. */
            sym = lookup_sym(label, &system_st);
        }

        if (sym != NULL && !(sym->flags & SYMBOLFLAG_UNDEFINED)) {
            tp = new_ex_tree(EX_SYM);
            tp->cp = cp;
            tp->data.symbol = sym;

            free(label);
            return tp;
        }

        /* The symbol was not found. Create an "undefined symbol"
           reference. These symbols are freed in free_tree(),
           in contrast to the symbol used in EX_SYM.
           implicit_gbl() will either make it an implicit global,
           or an undefined non-global symbol.
         */
        sym = memcheck(malloc(sizeof(SYMBOL)));
        sym->label = label;
        sym->flags = SYMBOLFLAG_UNDEFINED | local;
        sym->stmtno = stmtno;
        sym->next = NULL;
        sym->section = &absolute_section;  /* TODO: Decide if we want to use abs_section_addr() */
        sym->value = 0;

        tp = new_ex_tree(EX_UNDEFINED_SYM);
        tp->cp = cp;
        tp->data.symbol = sym;

        return tp;
    }
}


/*
  parse_expr - this gets called everywhere.  It parses and evaluates
  an arithmetic expression.
*/

EX_TREE        *parse_expr(
    char *cp,
    int flags)
{
    EX_TREE        *expr;
    EX_TREE        *value;

    expr = parse_binary(cp, 0, 0);     /* Parse into a tree */
    value = evaluate(expr, flags);     /* Perform the arithmetic */
    value->cp = expr->cp;              /* Pointer to end of text is part of
                                          the rootmost node  */
    free_tree(expr);                   /* Discard parse in favor of
                                          evaluation */

    return value;
}


/*
  parse_unary_expr It parses and evaluates
  an arithmetic expression but only a unary: (op)value or <expr>.
  In particular, it doesn't try to divide in <lf>/SOH/.
*/

EX_TREE        *parse_unary_expr(
    char *cp,
    int flags)
{
    EX_TREE        *expr;
    EX_TREE        *value;

    expr = parse_unary(cp);            /* Parse into a tree */
    value = evaluate(expr, flags);     /* Perform the arithmetic */
    value->cp = expr->cp;              /* Pointer to end of text is part of
                                          the rootmost node  */
    free_tree(expr);                   /* Discard parse in favor of
                                          evaluation */

    return value;
}


/*
 * expr_ok  Returns TRUE if there was a valid expression parsed.
 */

int             expr_ok(
    EX_TREE *expr)
{
    return (expr != NULL && expr->type != EX_ERR);
}
