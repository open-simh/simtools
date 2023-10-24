#define ASSEMBLE__C

/*
 * Main assembly code.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "assemble.h"                  /* My own definitions */

#include "assemble_aux.h"
#include "assemble_globals.h"
#include "extree.h"
#include "listing.h"
#include "macros.h"
#include "mlb.h"
#include "object.h"
#include "parse.h"
#include "rad50.h"
#include "rept_irpc.h"
#include "symbols.h"
#include "util.h"


#define DEBUG_LSB       0  /* See also parse.c */

#define CHECK_EOL       check_eol(stack, cp)

#define STRICT_IF_DF_NDF_RECURSES 0    /* 0 = '< ... >' forbidden, else allowed if !STRICTER */

/* eval_ifdf_ifndf assembles a '.IF DF' or '.IF NDF' statement (or .IIF) */

//    .if df  x & y -> both  must be defined
//    .if df  x ! y -> either may be defined
//    .if ndf x & y -> both  must be undefined
//    .if ndf x ! y -> either may be undefined

static int eval_ifdf_ifndf(
    char  **cpp,
    int     if_df,
    STACK  *stack)
{
    char           *cp     = *cpp;
    int             ok     = TRUE;    /* 0 = false, 1 = true */
    int             op     = 0;       /* 0 = '&',   1 = '!'  */

    for (;;) {
        char           *ncp;
        int             islocal;
        int             found;

        cp = skipwhite(cp);

#if STRICT_IF_DF_NDF_RECURSES
        if (!STRICTER && *cp == '<') {
            ncp = skipwhite(++cp);
            found = eval_ifdf_ifndf(&ncp, if_df, stack);
            if (*ncp != '>') {
                report_err(stack->top, "Missing '>'\n");
                return 0;
            }
            ncp++;
        } else /* { ... Continues below */
#endif
        { /* ... This could be the 'else' part from above */
            char           *label;
            SYMBOL         *sym;

            if ((label = get_symbol(cp, &ncp, &islocal)) == NULL) {
#if DISABLE_EXTREE_IF_DF_NDF
                if (RELAXED)
                    return 0;  /* We allow -relaxed to have no parameter */
#endif
                if (support_m11)
                    if (!STRICTER)
                        return 0;  /* We allow -ym11 without -strict -strict to have no parameter */

                report_err(stack->top, "Missing symbol name\n");
                return 0;  /* Assume a missing symbol returns FALSE */
            }
            if (islocal) {
                report_err(stack->top, "Local symbols are not allowed: %s\n", label);
                free(label);
                return 0;  /* Assume a local symbol returns FALSE */
            }
            sym = lookup_sym(label, &symbol_st);
            found = (sym != NULL);
            if (found) {
                 if (SYM_IS_IMPORTED(sym)) {                              /* Unassigned global symbols are undefined */
                    found = 0;
                 } else if (sym->flags & SYMBOLFLAG_UNDEFINED) {          /* Symbols marked undefined are undefined */
                    found = 0;
                 } else if (sym->section->type == SECTION_INSTRUCTION) {  /* Opcodes are undefined */
                    found = 0;
                 } else if (sym->section->type == SECTION_PSEUDO) {       /* Directives are undefined */
                    found = 0;
                 } else {
                    ;  /* It must be truly "found" */
                 }
            }
            free(label);
        }

        if (!if_df) found = !found;

        if (op) { /* '!' = OR */
            ok |= found;  /* C has no '||=' */
        } else {  /* '&' = AND */
            ok &= found;  /* C has no '&&=' */
        }

        cp = skipwhite(ncp);
        op = -1;
        if (*cp == '&')
            op = 0;
        if (*cp == '!')
            op = 1;
        if (op < 0)
            break;
	cp++;
    }
    *cpp = cp;
    return ok;
}


/* abs_section_addr - returns the address of the relevant absolute section */
/* If the section is REL then the absolute_section address will be returned */
/* If -a0 is in effect and the section is OVR the parameter will be returned */
/* Else the absolute_section address will be returned */


static SECTION *abs_section_addr(
    SECTION *sect)
{
    if (sect->flags & PSECT_REL)
        return &absolute_section;

    if (abs_0_based)
        if (sect->flags & PSECT_COM)
            return sect;

    return &absolute_section;
}


/* assemble a rad50 value (some number of words). */
static unsigned *assemble_rad50(
    char *cp,
    int max,
    int *count,
    STACK *stack)
{
    char           *radstr;
    unsigned       *ret;
    int             i, len, wcnt;

    /* Allocate storage sufficient for the rest of the line. */
    radstr = memcheck(malloc(strlen(cp)));
    len = 0;

    cp = skipwhite(cp);
    do {
        if (*cp == '<') {
            EX_TREE        *value;
            /* A byte value */
            value = parse_unary_expr(cp, 0);
            cp = value->cp;
            if (value->type != EX_LIT) {
                report_err(stack->top, "Expression must be constant\n");
                radstr[len++] = '\0';
            } else if (value->data.lit >= 050) {
                report_err(stack->top, "Invalid character value %o\n",
                       value->data.lit & 0xFFFF);
                radstr[len++] = '\0';
            } else {
                radstr[len++] = (char) value->data.lit;
            }
            free_tree(value);
        } else {
            char            quote = (char) toupper((unsigned char) *cp++);  /* MACRO-11 quirk */

            while (*cp != '\0' && *cp != '\n' && *cp != quote) {
                int         ch = ascii2rad50(*cp++);

                if (ch < 0) {
                    report_err(stack->top, "Invalid character '%c'\n", cp[-1]);
                    radstr[len++] = '\0';
                } else {
                    radstr[len++] = (char) ch;
                }

            }
            if (*cp++ != quote) {
                if (STRICT) {
                    report_warn(stack->top, "Mismatched quote (%c)\n", quote);
                }
            }
        }
        cp = skipwhite(cp);
    } while (!EOL(*cp));

    wcnt = (len + 2) / 3;
    /* Return at most "max" words, if specified */
    if (max && max < wcnt)
        wcnt = max;
    if (count != NULL)
        *count = wcnt;

    /* Allocate space for actual or max words, whichever is larger */
    ret = memcheck (malloc (((wcnt < max) ? max : wcnt) * sizeof (int)));
    for (i = 0; i < wcnt; i++) {
        int word = packrad50word(radstr + i * 3, len - (i * 3));

        ret[i] = word;
    }

    /* If max is specified, zero fill */
    for (; i < max; i++)
        ret[i] = 0;

    free(radstr);
    return ret;
}


/* Get the mode and check the result */

int get_mode_check(
    char *cp,
    char **endp,
    ADDR_MODE *mode,
    char **error,
    STREAM *str,
    char *fmt)
{
    int             retval;

    *error = NULL;
    retval = get_mode(cp, endp, mode, error);

    if (*error)
        report_err(str, fmt, *error);

    return retval;
}


/* assemble - read a line from the input stack, assemble it. */

/* This function is way way too large, because I just coded most of
   the operation code and pseudo-op handling right in line.  */

static int assemble(
    STACK *stack,
    TEXT_RLD *tr)
{
    char           *cp;         /* Parse character pointer */
    char           *opcp;       /* Points to operation mnemonic text */
    char           *ncp;        /* "next" cp */
    char           *label;      /* A label */
    char           *line;       /* The whole line */
    SYMBOL         *op;         /* The operation SYMBOL */
    int             local;      /* Whether a label is a local label or not */

    line = stack_getline(stack);
    if (line == NULL)
        return -1;                     /* Return code for EOF. */

    if (!ENABL(LC)) {                  /* If lower case disabled, */
        upcase(line);                  /* turn it into upper case. */
    }

    cp = line;

    stmtno++;                          /* Increment statement number */

    list_source(stack->top, line);     /* List source */

    if (STRICT || ENABL(CDR)) {
        if (from_file_stream(stack->top)) {
            int             len = strlen(line) - 1;  /* Line will be terminated by "\n") */

            if (STRICT)
                if (len > MAX_FILE_LINE_LENGTH)
                    report_warn(stack->top, "Line length of %d is longer than the allowed %d\n",
                                            len, MAX_FILE_LINE_LENGTH);

            if (ENABL(CDR))
                if (len > 72)
                    line[72] = '\0';
        } else {
            if (STRINGENT) {
                int             len = strcspn(line, "\n");

                if (len >= MAX_FILE_LINE_LENGTH)
                    report_warn(stack->top, "Processed line length of %d is longer than the allowed %d\n",
                                            len + 1, MAX_FILE_LINE_LENGTH);
            }
        }
    }

    /* Frankly, I don't need to keep "line."  But I found it quite
       handy during debugging, to see what the whole operation was,
       when I'm down to parsing the second operand and things aren't
       going right. */

    if (suppressed) {
        /* Assembly is suppressed by unsatisfied conditional.  Look
           for ending and enabling statements. */

        if (!LIST(CND))
            DONT_LIST_THIS_LINE();

        op = get_op(cp, &cp);          /* Look at operation code */

        /* No need to FIX ME: this code will blindly look into .REM commentary
           and find operation codes.  Incidentally, so will read_body().

           That doesn't really matter, though, since the original also
           did that (line 72 ends the suppressed conditional block):

     69                                         .if NE,0
     70                                         .rem    &
     71                                 junk
     72                                         .endc
A    73 000144  000000G 000000G         more junk
A    74 000150  000000G 000000G 000000G line that ends the comments with &
        000156  000000G 000000G 000000C
O    75                                         .endc
         */

        if (op == NULL)
            return 1;                  /* Not found.  Don't care. */
        if (op->section->type != SECTION_PSEUDO)
            return 1;                  /* Not a pseudo-op. */
        switch (op->value) {
        /* MACRO-11 does not list .IIF (only the second part) or .IF or .IFT or .IFF or .IFTF or .ENDC */
        case P_IF:
        case P_IFXX:  /* .IFxx where xx=DF NDF Z EQ NZ NE L LT G GT LE GE */
            suppressed++;              /* Nested.  Suppressed. */
            break;
        case P_IFTF:
            if (suppressed == 1)       /* Reduce suppression from 1 to 0. */
                suppressed = 0;
            list_short_value_if(stack->top, suppressed == 0);  /* Extension to MACRO-11 */
            break;
        case P_IFF:
            if (suppressed == 1) {     /* Can reduce suppression from 1 to 0. */
                if (!conds[last_cond].ok)
                    suppressed = 0;
            }
            list_short_value_if(stack->top, suppressed == 0);  /* Extension to MACRO-11 */
            break;
        case P_IFT:
            if (suppressed == 1) {     /* Can reduce suppression from 1 to 0. */
                if (conds[last_cond].ok)
                    suppressed = 0;
            }
            list_short_value_if(stack->top, suppressed == 0);  /* Extension to MACRO-11 */
            break;
        case P_ENDC:
            suppressed--;              /* Un-nested. */
            if (suppressed == 0)
                pop_cond(last_cond - 1);        /* Re-enabled. */
            break;
        }
        return 1;
    }

    /* The line may begin with "label<ws>:[:]", possibly repeated. */

    /* PSEUDO P_IIF jumps here.  */
  reassemble:
    opcp = cp;
    while ((label = get_symbol(cp, &ncp, &local)) != NULL) {
        int             flag = SYMBOLFLAG_PERMANENT | SYMBOLFLAG_DEFINITION | local;
        SYMBOL         *sym;

        ncp = skipwhite(ncp);
        if (*ncp == ':') {             /* Colon, for symbol definition? */
            ncp++;
            if (!STRINGENT)
                ncp = skipwhite(ncp);
            /* maybe it's a global definition */
            if (*ncp == ':') {
                if (local) {
                    report_warn(stack->top, "Local symbol may not be set global '%s'\n", label);
                } else {
                    flag |= SYMBOLFLAG_GLOBAL;      /* Yes, include global flag */
                }
                ncp++;
            }
            if (label[0] == '.' && label[1] == '\0')
                sym = NULL;
            else
                sym = add_sym(label, DOT, flag, current_pc->section, &symbol_st,
                              stack->top);
            cp = ncp;

            if (sym == NULL)
                report_err(stack->top, "Invalid label definition '%s'\n", label);

            free(label);

            list_location(stack->top, DOT);

            /* See if local symbol block should be incremented */
            if (!ENABL(LSB) && !local) {
                lsb = get_next_lsb();
            }

            cp = skipwhite(ncp);
            opcp = cp;
            label = get_symbol(cp, &ncp, NULL); /* Now, get what follows */
        }
        else {
            /* No more labels */
            break;
        }
    }

    cp = skipwhite(cp);

    if (EOL(*cp))
        return 1;                      /* It's commentary.  All done. */

    if (label) {                       /* Something looks like a label. */
        /* detect assignment */

        ncp = skipwhite(ncp);          /* The pointer to the text that
                                          follows the symbol */

        if (*ncp == '=') {
            unsigned        flags;
            EX_TREE        *value;
            SYMBOL         *sym;

            cp = ncp;

            /* Symbol assignment. */

            if (STRICT && local) {
                report_warn(stack->top, "Local symbol may not assigned a value '%s'\n", label);
             /* return 0; // But do it anyway */
            }

            flags = SYMBOLFLAG_DEFINITION | local;
            cp++;
            if (!STRINGENT)
                cp = skipwhite(cp);
            if (*cp == '=') {
                if (local) {
                   report_warn(stack->top, "Local symbol may not be set global '%s'\n", label);
                } else {
                    flags |= SYMBOLFLAG_GLOBAL;     /* Global definition */
                }
                cp++;
            }
            if (!STRINGENT)
                cp = skipwhite(cp);
            if (*cp == ':') {
                flags |= SYMBOLFLAG_PERMANENT;
                cp++;
            }

            cp = skipwhite(cp);

            /* V05.03 (DOC: J.1.3) fixed a bug where '.' is on the RHS of an equation:
             *   .PSECT TESTPS,ABS
             *    TESTPC == .  ; Should be in section 'TESTPS' not '. ABS.'
             *    .END
             * This block fixes the specific case of assigning '.' -- not '.+n' etc.
             * See also below for processing with the '-a0' command line option. */

            if (/* !abs_0_based && */ *cp == '.') {
                ncp = skipwhite(cp + 1);
                if (EOL(*ncp)) {
                    value = new_ex_tree(EX_SYM);
                    value->cp = ncp;
                    value->data.symbol = current_pc;
                } else {
                    value = parse_expr(cp, 0);
                }
            } else {
                value = parse_expr(cp, 0);
            }
            cp = value->cp;

            /* Special code: if the symbol is the '.' program counter,
             * this is harder. */

            if (strcmp(label, ".") == 0) {
                if (current_pc->section->flags & PSECT_REL) {
                    SYMBOL         *symb;
                    unsigned        offset;

                    /* Express the given expression as a symbol and an
                       offset. The symbol must not be global, the
                       section must = current. */

                    if (!express_sym_offset(value, &symb, &offset)) {
                        report_err(stack->top, "Invalid ORG (for relocatable section)\n");
                    } else if (SYM_IS_IMPORTED(symb)) {
                        report_err(stack->top, "Can't ORG to external location\n");
                    } else if (symb->flags & SYMBOLFLAG_UNDEFINED) {
                        report_err(stack->top, "Can't ORG to undefined sym\n");
                    } else if (symb->section != current_pc->section) {
                        report_err(stack->top, "Can't ORG to alternate section (use .PSECT)\n");
                    } else {
                        DOT = symb->value + offset;
                        list_value(stack->top, DOT);
                        change_dot(tr, 0);
                    }
                } else {
                    /* If the current section is absolute, the value
                       must be a literal */
                    if (value->type != EX_LIT) {
                        report_err(stack->top, "Can't ORG to non-absolute location\n");
                        free_tree(value);
                        free(label);
                        return 0;
                    }
                    DOT = value->data.lit;
                    list_value(stack->top, DOT);
                    change_dot(tr, 0);
                }
                free_tree(value);
                free(label);
                return CHECK_EOL;
            }

            /* regular symbols */
            if (value->type == EX_LIT) {

                /* V05.03 (DOC: J.1.3) fixed a bug where '.' is on the RHS of an equation:
                 *   .PSECT TESTPS,ABS
                 *    TESTPC == .  ; Should be in section 'TESTPS' not '. ABS.'
                 *    .END
                 *
                 * I think MACRO-11 should have gone one further:
                 *   If a symbol with an absolute value is assigned in
                 *   a PSECT with the ABS attribute then it should belong
                 *   to that PSECT and not '. ABS.'.  Of course, this is if we
                 *   KNOW that this section will not be relocated somewhere else.
                 *
                 * This is what we do here for '-a0'.  If an absolute symbol is
                 * assigned within a REL PSECT, it is assigned to '. ABS.' else it
                 * will be assigned to the current ABS PSECT (which may be '. ABS.').
                 *
                 * Although you can set the attributes ABS,CON it is always
                 * treated as ABS,OVR.  There is no concatination of ABS sections.
                 * We use ABS,CON to override the effect of '-a0' for a given section.
                 *
                 * TODO: Provide a fool-proof solution (?) */

                sym = add_sym(label, value->data.lit, flags,
                              abs_section_addr(current_pc->section), &symbol_st, stack->top);

            } else if (value->type == EX_SYM || value->type == EX_TEMP_SYM) {
                sym = add_sym(label, value->data.symbol->value, flags,
                              value->data.symbol->section, &symbol_st, stack->top);
            } else {
                if (value->type == EX_ERR &&
                    value->cp != NULL &&
                    value->data.child.right == NULL &&
                    value->data.child.left != NULL &&
                    value->data.child.left->type == EX_LIT) {
                    report_warn(stack->top, "Complex expression has been assigned to a symbol\n");
                    sym = add_sym(label, value->data.child.left->data.lit, flags,
                                  abs_section_addr(current_pc->section), &symbol_st, stack->top);
                } else {
                    report_err(stack->top, "Complex expression cannot be assigned to a symbol\n");
                    if (pass == PASS1) {
                        /* This may work better in pass 2 - something in
                           RT-11 monitor needs the symbol to apear to be
                           defined even if I can't resolve its value. */
                        sym = add_sym(label, 0, SYMBOLFLAG_UNDEFINED,
                                      &absolute_section, &symbol_st, stack->top);  /* TODO: Use abs_section_addr() ? */
                    } else {
                        sym = NULL;
                    }
                }
            }

            if (sym != NULL)
                list_symbol(stack->top, sym);

            free_tree(value);
            free(label);

            return sym != NULL && CHECK_EOL;
        }

        /* Try to resolve macro */

do_mcalled_macro:
        op = lookup_sym(label, &macro_st);
        if (op /*&& op->stmtno < stmtno*/) {
            STREAM         *macstr;

            free(label);

            list_location(stack->top, DOT);

            macstr = expandmacro(stack->top, (MACRO *) op, ncp);
            if (macstr == NULL) {
                /* Error in expanding the macro, stop now. */
                return 0;
            }

            stack_push(stack, macstr); /* Push macro expansion
                                          onto input stream */

            if (!LIST(MC))
                DONT_LIST_THIS_LINE();

            return 1;
        }

        /* Try to resolve instruction or pseudo */
        op = lookup_sym(label, &system_st);
        if (op) {
            cp = ncp;

            free(label);               /* Don't need this hanging around anymore */

            switch (op->section->type) {
            case SECTION_PSEUDO:

                /* Handle all directives which require DOT to be even */

                switch (op->value) {
                case P_BLKW:
                case P_FLT2:
                case P_FLT4:
                case P_LIMIT:
                case P_RAD50:
                case P_WORD:
                    if (DOT & 1) {
                        report_warn(stack->top, "%s on odd boundary [.EVEN implied]\n", op->label);
                        DOT++;  /* And fix it */
                   }
                }

                /* Parse the basic syntax of the directive (and its stringentness):-
                 *
                 *     0) Skip any white-space
                 *     1) Ignore directives which do NOTHING if they have no arguments
                 *     2) Report an error for directives which do NOT accept arguments
                 *     3) Report an error for directives which REQUIRE arguments but have none
                 *     4) Pass any directives which allow optional arguments unchanged
                 *     5) Report an error if we forgot any directives (all must appear here!)
                 */

                cp = skipwhite(cp);
                switch (op->value) {

                /* 1) Ignore directives which do NOTHING if they have no arguments */

                case P_DSABL:    /* -stringent */
                case P_ENABL:    /* -stringent */
                case P_FLT2:     /* -stringent */
                case P_FLT4:     /* -stringent */
                case P_GLOBL:    /* -stringent */
                case P_MCALL:    /* -stringent */
                case P_MDELETE:  /* -stringent */
                case P_WEAK:     /* -stringent */

                    if (!EOL(*cp))
                        break;
                    if (!STRINGENT)
                        return 1;
                    report_warn(stack->top, "Directive '%s' should have arguments\n", op->label);
                    return 1;  /* Not strictly an error */

                /* 2) Report an error for directives which do NOT accept arguments */

                case P_PAGE:

                    if (list_level >= 0) {                  /* Only do .PAGE if .LIST is in force */
                        list_line_act |= LIST_PAGE_BEFORE;  /* Special case for .PAGE in case of error */
                    }

                    /* FALL THROUGH */

                case P_ASECT:
                case P_ENDC:
                case P_ENDR:    /* Undocumented: in MACRO-11 .ENDR is a synonym for .ENDM */
                case P_EOT:     /* -stringent (MACRO-11 ignores this depricated directive) */
                case P_EVEN:
                case P_IFF:
                case P_IFT:
                case P_IFTF:
                case P_LIMIT:
                case P_MEXIT:
                case P_ODD:
                case P_RESTORE:
                case P_SAVE:

                    if (!EOL(*cp)) {
                        if (STRICT || (op-> value != P_ENDR))
                            report_warn(stack->top, "Directive '%s' does not accept arguments [ignored]\n", op->label);
                        if (op-> value != P_ENDR)
                            while (!EOL(*cp))
                                cp++;  /* Remove all the arguments (just in case we CHECK_EOL etc.) */
                    }
                    if (!STRINGENT)
                        break;       /* Regardless of the message (if any) */
                    if (op->value != P_EOT)
                        break;
                    report_warn(stack->top, "Directive '%s' is depricated\n", op->label);
                    return 1;  /* Not strictly an error and there's nothing to do */

                /* 3) Report an error for directives which REQUIRE arguments but have none */

                case P_ASCII:        /* Note that MACRO-11 uppercases the first character (bug) */
                case P_ASCIZ:        /* Note that MACRO-11 uppercases the first character (bug) */
                case P_REM:          /* Note that MACRO-11 uppercases the first character (bug) */

                    if (*cp == ';')  /* Documented behaviour */
                        break;

                    /* FALL THROUGH */

                case P_IDENT:        /* Note that MACRO-11 uppercases the first character (bug) */
                case P_INCLUDE:
                case P_LIBRARY:
                case P_RAD50:        /* Note that MACRO-11 uppercases the first character (bug) */

                    if (!STRINGENT && *cp == ';')
                        break;       /* We will treat ';' as an error (below) if -stringent */

                    /* FALL THROUGH */

                case P_IRP:
                case P_IRPC:
                case P_IF:           /* MACRO-11 treats this as TRUE and expects a .ENDC etc. */
                case P_IFXX:         /* -stringent (but the same action as P_IF) */
                case P_IIF:
                case P_MACRO:
                case P_NARG:
                case P_NCHR:
                case P_NTYPE:
                case P_REPT:
                case P_TITLE:        /* Note that MACRO-11 uppercases the first character (bug) */

                    if (STRINGENT && op->value == P_IFXX)
                        report_warn(stack->top, "Directive '%s' is depricated\n", op->label);
                    if (!EOL(*cp))
                        break;
                    report_err(stack->top, "Directive '%s' requires arguments\n", op->label);
                    if (op->value == P_IF || op->value == P_IFXX)
                        push_cond(TRUE, stack->top);  /* Despite the error, we act on the .IF */
                    return 0;

                /* 4) Pass any directives which allow optional arguments unchanged */

                case P_BLKB:     /* -stringent */
                case P_BLKW:     /* -stringent */
                case P_BYTE:     /* -stringent */
                case P_CROSS:
                case P_CSECT:
                case P_END:
                case P_ENDM:
                case P_ERROR:
                case P_LIST:
                case P_NLIST:
                case P_NOCROSS:
                case P_PACKED:   /* -stringent */
                case P_PRINT:
                case P_PSECT:
                case P_RADIX:
                case P_SBTTL:
                case P_WORD:     /* -stringent */

                    if (!EOL(*cp))
                        break;
                    if (!STRINGENT)
                        break;
                    if (op->value != P_BLKB  && op->value != P_BLKW   &&
                        op->value != P_BYTE  && op->value != P_PACKED &&
                        op->value != P_WORD)
                        break;
                    report_warn(stack->top, "Directive '%s' should have arguments\n", op->label);
                    break;  /* Not strictly an error */

                /* 5) Report an error if we forgot any directives (all MUST appear here!) */

                default:
                    report_err(stack->top, "Directive '%s' is not yet supported\n", op->label);
                    return 0;
                }

                /* At this point, we only have directives which need to do something
                 * cp will either point to a character (skipwhite done) or eol.
                 * If the directive should be on an even boundary, it will be. */

                switch (op->value) {

                case P_EOT:
                   /* MACRO-11 ignores this depricated directive */
                   return 1;

                case P_PAGE:
                    /* Note that V05.05 even suppresses lines with labels on them (!) */

                    if (list_level < 0)
                        return 1;    /* Ignore .PAGE if .NLIST is in force */

                    if (*cp == ';' && LIST(COM))            /* Choose if .NLIST COM should always suppress .PAGE */
                        list_line_act |= LIST_PAGE_BEFORE;  /* Extension: list the .PAGE if it has a comment */
                    else
                        list_line_act |= LIST_PAGE_BEFORE | LIST_SUPPRESS_LINE;  /* TODO: Check for labels (?) */
                    return 1;

                case P_TITLE:
                    /* accquire module name and title string */
                    {
                        int             len = 0;

                        while (ascii2rad50(cp[len]) > 0 && len < symbol_len)
                            len++;
                        if (!len) {
                            report_err(stack->top, "Invalid .TITLE module-name\n");
                            return 0;
                        }

                        if (module_name != NULL) {
                            free(module_name);
                        }
                        module_name = memcheck(malloc(len + 1));
                        memcpy(module_name, cp, len);
                        module_name[len] = '\0';
                        upcase(module_name);

                        strncpy(title_string, cp, 31);
                        title_string[strcspn(title_string, "\n")] = '\0';

                        /* Remove trailing spaces and/or tabs */
                        len = strlen(title_string);
                        while (--len >= 0) {
                            if (title_string[len] == ' ' || title_string[len] == '\t')
                               title_string[len] = '\0';
                            else
                               break;
                        }

#if NODO
                        /* MACRO-11 upper-cases the first character - but it's probably a bug */
                        if (islower((unsigned char) title_string[0]))
                            title_string[0] = (char) toupper((unsigned char) title_string[0]);
#endif
                    }
                    return 1;

                case P_SBTTL:
                    if (lstfile == NULL)
                        return 1;  /* Ignore if we are not listing */

                    if (pass == PASS2 || (list_pass_0 && LIST(LIS) >= 0))
                        return 1;  /* Ignore if on pass 2 or listing pass 1 itself */

                    if (!LIST(TOC))
                        return 1;  /* Ignore for .NLIST TOC */

                    if (!toc_shown) {
                        list_title_line("Table of contents");
                        toc_shown = 1;
                    }

                    cp[strcspn(cp, "\n")] = '\0';

                    /* Remove trailing spaces and/or tabs */
                    {
                        int             len = strlen(cp);

                        while (--len >= 0) {
                            if (cp[len] == ' ' || cp[len] == '\t')
                               cp[len] = '\0';
                            else
                               break;
                        }
                    }

                    /* TODO: Instead of suppressing the sequence number within macro calls ...
                     *       ... we could go back up the chain until we find a 'useful' one */

                    fprintf(lstfile, "%*s%*.0d%*s- %.119s\n",
                                     (int) SIZEOF_MEMBER(LSTFORMAT, flag), "",
                                     (int) SIZEOF_MEMBER(LSTFORMAT, line_number),
                                     (int) (within_macro_expansion(stack->top)) ? 0 : stack->top->line,
                                     (int) SIZEOF_MEMBER(LSTFORMAT, gap_after_seq), "",
                                     cp);
                    return 1;

                case P_IDENT:
                    {
                        if (ident)          /* An existing ident? */
                            free(ident);    /* Discard it. */

                        ident = assemble_rad50(cp, 2, NULL, stack);
                        return 1;
                    }

                case P_RADIX:
                    {
                        int             old_radix = radix;
                        EX_TREE        *value;
                        int             ok = 1;

                        if (EOL(*cp)) {
                            /* If no argument, assume 8 */
                            radix = 8;
                            list_value(stack->top, radix);
                            return 1;
                        }

                        /* Parse the argument in decimal radix */
                        radix = 10;
                        value = parse_expr(cp, 0);
                        cp = value->cp;

                        if (value->type != EX_LIT) {
                            report_err(stack->top, "Argument to .RADIX must be constant\n");
                            radix = old_radix;
                            ok = 0;
                        } else {
                            radix = value->data.lit;
                            list_value(stack->top, radix);
                            if (radix != 8 && radix != 10 &&
                                radix != 2 && radix != 16) {
                                report_err(stack->top, "Argument to .RADIX (%d.) must be 2, 8, 10, or 16\n", radix);
                                radix = old_radix;
                                ok = 0;
                            }
                        }
                        free_tree(value);
                        return ok && CHECK_EOL;
                    }

                case P_FLT4:
                case P_FLT2:
                    {
                        int             ok = 1;

                        while (ok && !EOL(*cp)) {
                            unsigned        flt[4];

                            if (parse_float(cp, &cp, (op->value == P_FLT4 ? 4 : 2), flt)) {
                                /* All is well */
                            } else {
                                report_err(stack->top, "Bad floating point format\n");
                                ok = 0;         /* Don't try to parse the rest of the line */
                                flt[0] = flt[1] /* Store zeroes */
                                       = flt[2]
                                       = flt[3] = 0;
                            }
                            /* Store the word values */
                            store_word(stack->top, tr, 2, flt[0]);
                            store_word(stack->top, tr, 2, flt[1]);
                            if (op->value == P_FLT4) {
                                store_word(stack->top, tr, 2, flt[2]);
                                store_word(stack->top, tr, 2, flt[3]);
                            }
                            cp = skipdelim(cp);
                        }
                        return ok && CHECK_EOL;
                    }

                case P_ERROR:
                    report_err(stack->top, "%.*s\n", strcspn(cp, "\n"), cp);

                    /* FALL THROUGH */

                case P_PRINT:
                    {
                        int             ok = (op->value == P_PRINT);

                        list_location(stack->top, DOT);

                        if (!EOL(*cp)) {
                            EX_TREE        *value = parse_expr(cp, 0);

                            cp = value->cp;
                            ok = ok && CHECK_EOL;

                            if (value->type == EX_LIT) {
                                list_value(stack->top, value->data.lit);
                            } else if (value->type == EX_SYM || value->type == EX_TEMP_SYM) {
                                list_symbol(stack->top, value->data.symbol);
                            } else {
                                report_warn(stack->top, "Complex expression to %s is not supported\n", op->label);
                                ok = FALSE;
                            }
                            free_tree(value);
                        }
                        if (ok) {
                            if (op->value == P_PRINT) {
                                if (within_macro_expansion(stack->top))
                                    MUST_LIST_THIS_LINE();  /* Always list the .PRINT line if in macro expansion */
                                if (show_print_lines) {
                                    MUST_LIST_THIS_LINE();  /* Always list the .PRINT line for -sp */
                                    show_print_line();      /* If not listing, show it instead */
                                }
                            }
                        }
                        return ok;
                    }

                case P_SAVE:
                    if (sect_sp >= SECT_STACK_SIZE - 1) {
                        report_err(stack->top, "Too many saved sections for .SAVE\n");
                        return 0;
                    }

                    if (STRINGENT)
                        if (sect_sp >= 16 - 1)
                            report_warn(stack->top,
                                "The program section context stack can strictly only handle 16 .SAVEs\n");

                    /* Contrary to the documentation, V05.05 does not appear to save
                     * and restore the maximum section size */

                    sect_sp++;
                    sect_stack[sect_sp] = current_pc->section;
                    dot_stack[sect_sp] = DOT;
                /*  list_location(stack->top, DOT);  // V05.05 does not list the location */
                    list_3digit_value(stack->top, current_pc->section->sector);
                    return 1;

                case P_RESTORE:
                    if (sect_sp < 0) {
                        report_err(stack->top, "No saved section for .RESTORE\n");
                        return 0;
                    } else {
                        go_section(tr, sect_stack[sect_sp]);
                        DOT = dot_stack[sect_sp];
                        list_location(stack->top, DOT);
                        list_3digit_value(stack->top, current_pc->section->sector);
                        if (!ENABL(LSB)) {
                            lsb = get_next_lsb();
                        }
                        sect_sp--;
                    }
                    return 1;

                case P_NARG:
                    {
                        STREAM         *str;
                        MACRO_STREAM   *mstr;
                        int             islocal;

                        label = get_symbol(cp, &cp, &islocal);

                        if (label == NULL) {
                            report_err(stack->top, "Bad .NARG syntax (symbol expected)\n");
                            return 0;
                        }

                        /* Walk up the stream stack to find the
                           topmost macro stream */
                        for (str = stack->top; str != NULL && str->vtbl != &macro_stream_vtbl;
                             str = str->next) ;

                        if (!str) {
                            report_err(str, ".NARG not within macro expansion\n");
                            free(label);
                            return 0;
                        }

                        mstr = (MACRO_STREAM *) str;

                        add_sym(label, mstr->nargs, SYMBOLFLAG_DEFINITION | islocal,
                                abs_section_addr(current_pc->section), &symbol_st, stack->top);
                        free(label);
                        list_value(stack->top, mstr->nargs);
                        return CHECK_EOL;
                    }

                case P_NCHR:
                    {
                        char           *string;
                        int             islocal;
                        int             nchars;

                        label = get_symbol(cp, &cp, &islocal);

                        if (label == NULL) {
                            report_err(stack->top, "Bad .NCHR syntax (symbol expected)\n");
                            return 0;
                        }

                        cp = skipdelim(cp);

                        string = getstring(cp, &cp);
                        nchars = strlen(string);
                        free(string);

                        add_sym(label, nchars, SYMBOLFLAG_DEFINITION | islocal,
                                abs_section_addr(current_pc->section), &symbol_st, stack->top);
                        free(label);

                        list_value(stack->top, nchars);
                        return CHECK_EOL;
                    }

                case P_NTYPE:
                    {
                        ADDR_MODE       mode;
                        int             islocal;
                        char           *error;

                        label = get_symbol(cp, &cp, &islocal);
                        if (label == NULL) {
                            report_err(stack->top, "Bad .NTYPE syntax (symbol expected)\n");
                            return 0;
                        }

                        /* MACRO-11 does NOT create the symbol if .NTYPE is at the top-level (OQ error)  */
                        /* However, it DOES create the symbol within .REPT, .IRP, .IRPC */
                        /* If you want .NTYPE outside a macro, you could use .REPT 1; .NTYPE ...; .ENDR */

                        if (STRICTEST)
                            if (!within_macro_expansion(stack->top))
                                report_warn(stack->top, ".NTYPE should strictly be within a macro expansion\n");

                        cp = skipdelim(cp);
                        if (!get_mode_check(cp, &cp, &mode, &error, stack->top,
                                            "Bad .NTYPE addressing mode (%s)\n")) {
                            free(label);
                            return 0;
                        }

                        add_sym(label, mode.type, SYMBOLFLAG_DEFINITION | islocal,
                                abs_section_addr(current_pc->section), &symbol_st, stack->top);
                        list_value(stack->top, mode.type);
                        free_addr_mode(&mode);
                        free(label);

                        return CHECK_EOL;
                    }

                case P_INCLUDE:
                    {
                        char           *name = getstring_fn(cp, &cp);
                        char            hitfile[FILENAME_MAX];
                        STREAM         *incl;

                        if (name == NULL) {
                            report_fatal(stack->top, "Bad .INCLUDE file name\n");
                            return 0;
                        }

                        if (!CHECK_EOL) {
                            free(name);
                            return 0;  /* V05.05 does not attempt to .INCLUDE for syntax errors */
                        }

                        name = defext(name, "MAC");
                        my_searchenv(name, "INCLUDE", hitfile, sizeof(hitfile));

                        /* If we can't .INCLUDE the file ...
                         * ... MACRO-11 throws ".INCLUDE directive file error" and exits */

                        if (hitfile[0] == '\0') {
                            report_fatal(stack->top, "Unable to find .INCLUDE file \"%s\"\n", name);
                            free(name);
                            return 0;
                        }
                        free(name);

                        incl = new_file_stream(hitfile);
                        if (incl == NULL) {
                            report_fatal(stack->top, "Unable to open .INCLUDE file \"%s\"\n", hitfile);
                            return 0;
                        }

                        stack_push(stack, incl);
                        list_line_act |= LIST_PAGE_AFTER;  /* Throw a page after listing the '.INCLUDE' */
                        return 1;
                    }

                case P_LIBRARY:
                    {
                        char           *name = getstring_fn(cp, &cp);
                        char            hitfile[FILENAME_MAX];

                        if (name == NULL) {
                            report_fatal(stack->top, "Bad .LIBRARY file name\n");
                            return 0;
                        }

                        if (!CHECK_EOL) {
                            free(name);
                            return 0;  /* V05.05 does not attempt to add the .LIBRARY for syntax errors */
                        }

                        name = defext(name, "MLB");
                        my_searchenv(name, "MCALL", hitfile, sizeof(hitfile));

                        /* If we can't open the .LIBRARY file ...
                         * ... MACRO-11 throws ".LIBRARY directive file error" and exits */

                        if (hitfile[0] == '\0') {
                            report_fatal(stack->top, "Unable to find .LIBRARY file \"%s\"\n", name);
                            free(name);
                            return 0;
                        }
                        free(name);

                        /* Only open a specific library ONCE */
                        {
                            int i;

                            for (i = 0; i < nr_mlbs; i++) {
                                if (strcmp(hitfile, mlbs[i]->name) == 0) {
                                    return 1;
                                }
                            }
                        }

                        if (nr_mlbs < MAX_MLBS) {
                            mlbs[nr_mlbs] = mlb_open(hitfile, 0);
                        }
                        if (nr_mlbs >= MAX_MLBS || mlbs[nr_mlbs] == NULL) {
                            report_fatal(stack->top, "Unable to register macro library \"%s\"\n", hitfile);
                        } else {
                            nr_mlbs++;
                        }
                    }
                    return 1;

                case P_REM:
                    /* Read and list [or discard] lines until one with a closing quote */
                    {
                        char            quote[4];
                        char            quote_ch = *cp++;
                        char           *rem_name;
                        int             rem_line;

                        if (quote_ch == '\0' || quote_ch == '\n') {  /* ';' is allowed */
                            report_err(stack->top, "Unexpected end-of-line (quote character expected)\n");
                            return 0;
                        }

                        /* Remember where the .REM started in case we need to report_err() it */
                        rem_name = memcheck(strdup(stack->top->name));
                        rem_line = stack->top->line;

                        /* V05.05: .REM x ... X works (it is case insensitive) */
                        quote[0] = (char) toupper((unsigned char) quote_ch);
                        quote[1] = (char) tolower((unsigned char) quote_ch);
                        quote[2] = '\n';
                        quote[3] = 0;

                        if (!LIST(COM)) {
                            /* Just in case the closing quote is on the same line as .REM */
                            cp += strcspn(cp, quote);
                            if (*cp == quote[0] || *cp == quote[1]) {
                                cp++;
                                return CHECK_EOL;  /* MACRO-11 does not allow further non-comment stuff on line */
                            }
                            list_flush();
                        }

                        for (;;) {
                            cp += strcspn(cp, quote);
                            if (*cp == quote[0] || *cp == quote[1])
                                break;

                            if (LIST(COM))
                                list_flush();

                            line = stack_getline(stack);     /* Read next input line */
                            if (line == NULL) {
                            /* list_line_act &=  ~LIST_PAGE_BEFORE;  // Suppress the EOF page throw */
                                report_err(stream_here(rem_name, rem_line),
                                           "Closing quote to .REM %c (here) is missing\n", quote_ch);
                                free(rem_name);
                            /*  exit(EXIT_FAILURE);  */
                                return 0;  /* EOF */
                            }
                            if (!ENABL(LC)) {                  /* If lower case disabled, */
                                upcase(line);                  /* turn it into upper case. */
                            }
                            cp = line;
                            list_source(stack->top, line);     /* List source */
                        }
                        free(rem_name);
                    }
                    cp++;              /* Skip the closing quote */
                    return CHECK_EOL;  /* MACRO-11 does not allow further non-comment stuff on line */

                case P_IRP:
                    {
                        STREAM         *str = expand_irp(stack, cp);

                        if (str)
                            stack_push(stack, str);
                        return str != NULL;
                    }

                case P_IRPC:
                    {
                        STREAM         *str = expand_irpc(stack, cp);

                        if (str)
                            stack_push(stack, str);
                        return str != NULL;
                    }

                case P_MCALL:
                    {
                        for (;;) {
                            cp = skipdelim(cp);

                            if (EOL(*cp))
                                return 1;

                            /* (lib)macro syntax. Ignore (lib) for now. */
                            if (*cp == '(') {
                                char *close = strchr(cp + 1, ')');

                                if (close != NULL) {
                                    char *libname = cp + 1;
                                    (void)libname;
                                    *close = '\0';
                                    cp = close + 1;
                                }
                            }

                            label = get_symbol(cp, &cp, NULL);
                            if (!label) {
                                report_err(stack->top, "Invalid .MCALL syntax (symbol expected)\n");
                                return 0;
                            }

                            /* See if that macro's already defined */
                            if (lookup_sym(label, &macro_st)) {
                                free(label);    /* Macro already
                                                   registered.  No
                                                   prob. */
                                cp = skipdelim(cp);
                                continue;
                            }

                            /* Do the actual macro library search */
                            if (!do_mcall (label, stack))
                                report_err(stack->top, "MACRO '%s' not found\n", label);

                            free(label);
                        }
                        /* NOTREACHED */
                    }
                    /* NOTREACHED */

                case P_MACRO:
                    {
                        MACRO          *mac;

                        if (!LIST(MD))
                            DONT_LIST_THIS_LINE();

                        mac = defmacro(cp, stack, CALLED_NORMAL);
                        return mac != NULL;
                    }

                case P_MDELETE:
                    /* MACRO-11 only really uses this to save assembler memory */
                    /* TODO: After a macro has been .MDELETEd its use should cause an error (OQ)
                     *       For the moment, we simply ignore the directive (without syntax checking). */
                    return 1;

                case P_MEXIT:
                    {
                        STREAM         *macstr;

                        /* Pop a stream from the input. */
                        /* It must be the first stream, and it must be */
                        /* a macro, rept, irp, or irpc. */
                        macstr = stack->top;
                        if (macstr->vtbl != &macro_stream_vtbl && macstr->vtbl != &rept_stream_vtbl
                            && macstr->vtbl != &irp_stream_vtbl && macstr->vtbl != &irpc_stream_vtbl) {
                            report_err(stack->top, ".MEXIT not within a macro\n");
                            return 0;
                        }

                        /* and finally, pop the macro */
                        stack_pop(stack);

                        return 1;
                    }

                case P_REPT:
                    {
                        STREAM         *reptstr = expand_rept(stack, cp);

                        if (reptstr)
                            stack_push(stack, reptstr);
                        return reptstr != NULL;
                    }

                case P_NLIST:
                case P_LIST:
                    if (pass == PASS1)
                        return 1;  /* Ignore .NLIST [*] and .LIST [*] on pass 1 */

                    /* TODO: Documentation (6.1.1) says list level < 0 suppresses all lines
                     *       except errors and = 0 uses the .LIST/.NLIST settings else,
                     *       if > 0 always lists the line. */

                    if (EOL(*cp)) {
                        if (!ARGS_IGNORE(L_LIS)) {
                            /* Don't change the list-level if we disabled the 'LIS' dirarg */
                            /* TODO: We _could_ limit the list_level to a 'reasonable' value (?) */
                            list_level += (op->value == P_LIST ? 1 : -1);
                        }

                        /* Note that V05.05 does not list '.LIST' and '.NLIST' lines ...
                         * ... even if they start with a label e.g. 'LABEL: .LIST' */

#if NODO  /* Set to DODO if you want to always suppress .[N]LIST lines like MACRO-11 V05.05 */
                        DONT_LIST_THIS_LINE();  /* Don't list .[N]LIST if within a macro expansion */
#else
                        if (within_macro_expansion(stack->top)) {
                            DONT_LIST_THIS_LINE();  /* Don't list .[N]LIST if within a macro expansion */
                        } else {
                            if (list_level < -1)
                                MUST_LIST_THIS_LINE();  /* Extension: if transitioning outside the 'expected' levels */
                            else if (list_level <= 0)
                                DONT_LIST_THIS_LINE();  /* Extension: if transitioning outside the 'expected' levels */
                        }
#endif

                        if (enabl_debug) {
                            MUST_LIST_THIS_LINE();  /* Always list the line for -yd */
                        }

                        /* Could use list_value() or [ list_short_value() ] instead -- Extension to MACRO-11 */
                        list_signed_value(stack->top, (list_level >= 0) ?
                                                      (unsigned) list_level :
                                                      (unsigned) 0x8000 | (list_level & 0x7fff));
                        return 1;
                    }

                    /* FALLS THROUGH */

                case P_DSABL:
                case P_ENABL:
                    {
                        int arg_type;
                        int arg_value;

                        if (op->value == P_NLIST || op->value == P_LIST) {
                            arg_type  = ARGS_NLIST_LIST;
                            arg_value = (op->value == P_NLIST) ? ARGS_NLIST : ARGS_LIST;
                        } else {
                            arg_type  = ARGS_DSABL_ENABL;
                            arg_value = (op->value == P_DSABL) ? ARGS_DSABL : ARGS_ENABL;
                        }

                        while (!EOL(*cp)) {
                            int argnum;

                            label = get_symbol(cp, &cp, NULL);
                            if (!label) {
                                report_err(stack->top, "Missing %s option\n", op->label);
                                return 0;
                            }

                            if (strlen(label) > 3)
                                argnum = -1;
                            else
                                argnum = lookup_arg(label, arg_type);

                            if (argnum < 0) {
                                report_warn(stack->top, "Invalid %s option '%s' [ignored]\n", op->label, label);
                                free(label);
                                cp = skipdelim(cp);
                                continue;
                            }

                            if (!ARGS_IGNORE(argnum)) {
                                if (enabl_list_arg[argnum].flags & ARGS_NOT_SUPPORTED)
                                    report_warn(stack->top, "Unsupported %s option '%s'\n", op->label, label);

                                enabl_list_arg[argnum].curval = arg_value;

                                if (argnum == E_LSB) {
                                    looked_up_local_sym = TRUE;  /* So we see it in the listing if -ylls */
                                    if (op->value == P_ENABL || RELAXED)
                                       lsb = get_next_lsb();
                                }
                            }

                            free(label);
                            cp = skipdelim(cp);
                        }
                        return 1;
                    }

                case P_LIMIT:
                    store_limits(stack->top, tr);
                    return 1;

                case P_END:
                    /* Accquire transfer address */
                    {
                        int             retval = 1;

                        if (xfer_address)
                            free_tree(xfer_address);

                        if (EOL(*cp)) {
                            xfer_address = new_ex_lit(1);
                        } else {
                            xfer_address = parse_expr(cp, 0);
                            cp = xfer_address->cp;
                            if (xfer_address->type != EX_LIT &&
                                xfer_address->type != EX_SYM &&
                                xfer_address->type != EX_TEMP_SYM) {
                                report_err(stack->top, "Complex expression to .END is not supported\n");
                                free_tree(xfer_address);
                                xfer_address = new_ex_lit(1);
                                retval = 0;
                            }
                        }

                        if (xfer_address->type == EX_LIT)
                            list_value(stack->top, xfer_address->data.lit);
                        else {  /* Can only be either EX_SYM or EX_TEMP_SYM */
                            list_symbol(stack->top, xfer_address->data.symbol);
                        }

                        retval = (retval && CHECK_EOL);

                        if (VERY_RELAXED)  /* Continue processing without error for -relaxed*2 */
                            return retval;

                        /* TODO: Check if we are within a MACRO call or .REPT / .IRP / .IRPC block */
                        /*       At the moment, we report references to .END in read_body() - which is not ideal */
                        /*       We could [mis-]use list_within_exp to do this */

                        /* Check if we are within a .IF block (unless -relaxed) */
                        if (!RELAXED && last_cond >= 0) {
                            if (STRICT) {  /* If -strict simply report the error */
                                report_warn(stack->top, ".END within .IF will not end assembly\n");
                                return 0;
                            }
                            report_err(stack->top, ".END within .IF has been executed\n");
                            retval = 0;  /* Else honour it */
                        }

                        /* Before flushing the whole stack ... flush the topmost (current) entry */
                        /* TODO: Put this in stream2.c: stack_flush(stack); */

                        do
                            cp = stack->top->vtbl->getline(stack->top);
                        while (cp != NULL);
                        list_line_act &= ~(LIST_PAGE_BEFORE | LIST_SUPPRESS_LINE);  /* Suppress the EOF page throw */

                        /* If not -strict we only flush the current .INCLUDE etc. */

                        if (!STRICTER)
                            return retval;    /* TODO: Provide better m11 support (using -ym11 ?) */

                        /* We stop all further assembly here.
                         * This means ignoring any further files
                         * on the command line. */

                        /* Read and discard any lines following the .END */
                        /* TODO: Count them and if -yd write the count out (?) */
                        /* TODO: Put this in stream2.c: stack_flush_all(stack); */
                        do
                            cp = stack_getline(stack);
                        while (cp != NULL);
                        list_line_act &= ~(LIST_PAGE_BEFORE | LIST_SUPPRESS_LINE);  /* Suppress the EOF page throw */
                        return retval;
                    }

                case P_IFXX:  /* .IFxx where xx=DF NDF Z EQ NZ NE L LT G GT LE GE */
                    opcp = skipwhite(opcp);
                    cp = opcp + 3;     /* Point cp at the "DF" or "NDF" part etc. */
                    /* FALLS THROUGH */

                case P_IIF:
                case P_IF:
                    {
                        int             ok = -1;         /* Will be set to FALSE or TRUE if 'ok' has been changed */
                        unsigned        value_word = 0;  /* Value to be shown in the value field */
                        int             show_value = 0;  /* < 0 = ok flag, 0 = no value, > 0 = comparison value */

                        label = get_symbol(cp, &cp, NULL);      /* Get condition */
                        cp = skipdelim(cp);

                        if (!label) {
                            report_err(stack->top, "Missing %s condition\n", op->label);
                        } else if (strcmp(label, "DF") == 0) {

#if DISABLE_EXTREE_IF_DF_NDF
                            ok = eval_ifdf_ifndf(&cp, 1, stack);
#else  /* Optionally use extree to evaluate definedness */
                            if (STRICT) {
                                ok = eval_ifdf_ifndf(&cp, 1, stack);
                            } else {
                                EX_TREE        *value;

                                value = parse_expr(cp, EVALUATE_DEFINEDNESS);
                                cp = value->cp;
                                ok = eval_defined(value);
                                free_tree(value);
                            }
#endif

                        } else if (strcmp(label, "NDF") == 0) {

#if DISABLE_EXTREE_IF_DF_NDF
                            ok = eval_ifdf_ifndf(&cp, 0, stack);
#else  /* Optionally use extree to evaluate definedness */
                            if (STRICT) {
                                ok = eval_ifdf_ifndf(&cp, 0, stack);
                            } else {
                                EX_TREE        *value;

                                value = parse_expr(cp, EVALUATE_DEFINEDNESS);
                                cp = value->cp;
                                ok = eval_undefined(value);
                                free_tree(value);
                            }
#endif

                        } else if (strcmp(label, "B") == 0 ||
                                   strcmp(label, "NB") == 0) {
                            /*
                             * Page 6-46 footnote 1 says
                             * "A macro argument (a form of symbolic argument)
                             * is enclosed within angle brackets or delimited
                             * by the circumflex construction, as described in
                             * section 7.3. For example,
                             *   <A,B,C>
                             *   ^/124/"
                             * but we don't enforce that here (yet) by using
                             * simply getstring().
                             */
                            cp = skipwhite(cp);
                            if (EOL(*cp)) {
                                ok = 1;
                            } else {
                                char           *thing, *end;

                                thing = getstring(cp, &cp);
                                end = skipwhite(thing);
                                ok = (*end == 0);
                                free(thing);
                            }
                            if (label[0] == 'N') {
                                ok = !ok;
                            }
                        } else if (strcmp(label, "IDN") == 0 ||
                                   strcmp(label, "DIF") == 0) {
                            char           *thing1,
                                           *thing2;

                            thing1 = getstring(cp, &cp);
                            cp = skipdelim(cp);
                            if (!EOL(*cp))
                                thing2 = getstring(cp, &cp);
                            else
                                thing2 = memcheck(strdup(""));

                            if (!ENABL(LCM)) {
                                upcase(thing1);
                                upcase(thing2);
                            }

                            ok = (strcmp(thing1, thing2) == 0);
                            if (label[0] == 'D') {
                                ok = !ok;
                            }
                            free(thing1);
                            free(thing2);
                        } else if (strcmp(label, "P1") == 0) {
                            ok = (pass == PASS1);
                        } else if (strcmp(label, "P2") == 0) {
                            ok = (pass == PASS2);
                        }

                        if (ok >= 0) {
                            show_value = -1;  /* We have successfully parsed a TRUE/FALSE .IF [or .IIF] */
                        } else if (label) {
                            int             sword;
                            unsigned        uword;
                            EX_TREE        *tvalue = parse_expr(cp, 0);

                            cp = tvalue->cp;

                            if (tvalue->type != EX_LIT) {
                                report_err(stack->top, "Bad %s expression\n", op->label);
                                free_tree(tvalue);

                                /* Undefined symbols are treated as zero for '.IF XX'
                                 * V05.05 does strange things when XX is undefined for ...
                                 * ... '.IIF XX' (I'm not quite sure what) */

                                /* If the condition is also invalid, we'll see two errors */
                                tvalue = new_ex_lit(0);
                            }

                            /* Convert to signed and unsigned words */
                            sword = tvalue->data.lit & 0x7fff;

                            /* No need to FIX ME:
                             * I don't know if the following
                             * is portable enough.  It should be but
                             * the replacement below surely is.
                             * if (tvalue->data.lit & 0x8000)
                             *     sword |= ~0x7FFF;   // Render negative */

                            if (tvalue->data.lit & 0x8000)
                                sword = sword - 0x8000;   /* Render negative */

                            /* Reduce unsigned value to 16 bits */
                            uword = tvalue->data.lit & 0xffff;

                            if (strcmp(label, "EQ") == 0 || strcmp(label, "Z") == 0)
                                ok = (uword == 0), value_word = uword;
                            else if (strcmp(label, "NE") == 0 || strcmp(label, "NZ") == 0)
                                ok = (uword != 0), value_word = uword;
                            else if (strcmp(label, "GT") == 0 || strcmp(label, "G") == 0)
                                ok = (sword > 0), value_word = sword;
                            else if (strcmp(label, "GE") == 0)
                                ok = (sword >= 0), value_word = sword;
                            else if (strcmp(label, "LT") == 0 || strcmp(label, "L") == 0)
                                ok = (sword < 0), value_word = sword;
                            else if (strcmp(label, "LE") == 0)
                                ok = (sword <= 0), value_word = sword;
                            else {
                                report_err(stack->top, "Invalid %s condition '%s'\n", op->label, label);
                            }

                            free_tree(tvalue);
                            if (ok >= 0)
                                show_value = 1;  /* We want to show the actual value we compared against */
                        }

                        if (label)
                            free(label);

                        if (ok < 0) {
                            ok = FALSE;  /* Pick TRUE or FALSE if we failed to parse the line */
                        } else {
                            if (op->value == P_IIF) {
                                if (LIST(CND)) {
                                    if (!ok)
                                        list_short_value_if(stack->top, ok);    /* Extension to MACRO-11 */
                                } else {
                                    if (ok)
                                        list_source(stack->top, skipdelim(cp));  /* Skip to AFTER the .IIF part */
                                    else
                                        DONT_LIST_THIS_LINE();
                                }
                            } else {
                                if (show_value < 0)
                                    list_short_value_if(stack->top, ok);    /* Extension to MACRO-11 */
                                else if (show_value > 0)
                                    list_value_if(stack->top, value_word);  /* Extension to MACRO-11 */
                                if (!LIST(CND))
                                    DONT_LIST_THIS_LINE();
                            }
                        }

                        if (op->value == P_IIF) {
                            stmtno++;  /* the second half is a
                                          separate statement */
                            if (ok) {
                                /* The "immediate if" */
                                /* Only slightly tricky. */
                                cp = skipdelim(cp);
                                goto reassemble;
                            }
                            return 1;           /* Ignore rest of line if
                                                   condition is false */
                        }

                        push_cond(ok, stack->top);

                        if (!ok)
                            suppressed++;       /* Assembly
                                                   suppressed
                                                   until .ENDC */
                    }
                    return CHECK_EOL;

                case P_IFF:
                    if (!LIST(CND))
                        DONT_LIST_THIS_LINE();

                    if (last_cond < 0) {
                        report_err(stack->top, "No conditional block active\n");
                        return 0;
                    }
                    if (conds[last_cond].ok)    /* Suppress if last cond is true */
                        suppressed++;
                    list_short_value_if(stack->top, suppressed == 0);  /* Extension to MACRO-11 */
                    return 1;

                case P_IFT:
                    if (!LIST(CND))
                        DONT_LIST_THIS_LINE();

                    if (last_cond < 0) {
                        report_err(stack->top, "No conditional block active\n");
                        return 0;
                    }
                    if (!conds[last_cond].ok)   /* Suppress if last cond is false */
                        suppressed++;
                    list_short_value_if(stack->top, suppressed == 0);  /* Extension to MACRO-11 */
                    return 1;

                case P_IFTF:
                    if (!LIST(CND))
                        DONT_LIST_THIS_LINE();

                    if (last_cond < 0) {
                        report_err(stack->top, "No conditional block active\n");
                        return 0;
                    }
                    list_short_value_if(stack->top, suppressed == 0);  /* Extension to MACRO-11 */
                    return 1;           /* Don't suppress. */

                case P_ENDC:
                    if (!LIST(CND))
                        DONT_LIST_THIS_LINE();

                    if (last_cond < 0) {
                        report_err(stack->top, "No conditional block active\n");
                        return 0;
                    }
                    pop_cond(last_cond - 1);
                    return 1;

                case P_ENDM:
                    report_err(stack->top, "No .MACRO definition block active\n");
                    return 0;

                case P_ENDR:
                    report_err(stack->top, "No repeat block active\n");
                    return 0;

                case P_EVEN:
                    if (DOT & 1) {
                        list_word(stack->top, DOT, 0, 1, "");
                        DOT++;
                        change_dot(tr, 0);
                    }
                    return 1;

                case P_ODD:
                    if (!(DOT & 1)) {
                        list_word(stack->top, DOT, 0, 1, "");
                        DOT++;
                        change_dot(tr, 0);
                    }
                    return 1;

                case P_ASECT:
                    if (!ENABL(LSB)) {
                        lsb = get_next_lsb();
                    }
                    go_section(tr, &absolute_section);
                    list_location(stack->top, DOT);
                    list_3digit_value(stack->top, current_pc->section->sector);
                    return 1;

                case P_CSECT:
                case P_PSECT:
                    {
                        SYMBOL         *sectsym;
                        SECTION        *sect;
                        unsigned int    old_flags;

                        label = get_symbol(cp, &cp, NULL);
                        if (label == NULL) {
                            sect = &blank_section;
                            cp = skipwhite(cp);
                            if (*cp != ',' && !EOL(*cp)) {
                                report_err(stack->top, "Invalid %s name: %.*s\n",
                                                       op->label, strcspn(cp, "\n"), cp);
                                return 0;
                            }
                        } else {
#if NODO
                            if (label[0] == '.' && label[1] == '\0') {
                                report_err(stack->top, "%s '.' is not permitted\n", op->label);
                                           /* Actually, RSX DOES permit this */
                                free(label);
                                return 0;
                            }
#endif
                            if (strlen(label) > 6) {  /* Only possible if -ysl > 6 */
                                if (STRINGENT)
                                    report_warn(stack->top, "%s name '%s' truncated to '%.6s'\n",
                                                            op->label, label, label);
                                label[6] = '\0';
                            }
                            sectsym = lookup_sym(label, &section_st);
                            if (sectsym) {
                                sect = sectsym->section;
                                free(label);
                            } else {
                                sect = new_section();
                                sect->label = label;
                                sect->flags = 0;
                                sect->pc = 0;
                                sect->size = 0;
                                sect->type = SECTION_USER;
                                sections[sector++] = sect;
                                sectsym = add_sym(label, 0,
                                        SYMBOLFLAG_DEFINITION, sect,
                                        &section_st, stack->top);

                                /* page 6-41 table 6-5 */
                                if (op->value == P_PSECT) {
                                    sect->flags |= PSECT_REL;
                                } else {
                                    sect->flags |= PSECT_REL | PSECT_COM | PSECT_GBL;
                                }
                            }
                        }

                        old_flags = sect->flags;

                        if (op->value == P_CSECT) {
                            cp = skipwhite(cp);
                            if (*cp == ',')
                                report_warn(stack->top, "Unexpected flag(s) to .CSECT directive: %.*s\n",
                                                        strcspn(cp, "\n"), cp);
                                /* Process them anyway and use them as best we can */
                        }

                        cp = skipdelim(cp);
                        if (!EOL(*cp)) {
                            while (cp = skipdelim(cp), !EOL(*cp)) {
                                /* Parse section options */
                                label = get_symbol(cp, &cp, NULL);
                                if (!label) {
                                    report_err(stack->top, "Unknown flag(s) to %s directive: %s",
                                                           op->label, cp);
                                    return 0;
                                }

                                /* MACRO-11 V05.05 is a lot more flexible with arguments.
                                 * Appended characters to the name are ignored.
                                 * E.g. NOSAVE is valid as is ABSOLUTE.
                                 * TODO: If not STRINGENT we could do this here. */

                                if (strcmp(label, "ABS") == 0) {
                                    sect->flags &= ~PSECT_REL;      /* Not relative */
                                    sect->flags |= PSECT_COM;       /* implies common */
                                } else if (strcmp(label, "REL") == 0) {
                                    sect->flags |= PSECT_REL;       /* Is relative */
                                } else if (strcmp(label, "SAV") == 0) {
                                    sect->flags |= PSECT_SAV;       /* Is root */
                                } else if (strcmp(label, "NOSAV") == 0) {
                                    sect->flags &= ~PSECT_SAV;      /* Is not root */
                                } else if (strcmp(label, "OVR") == 0) {
                                    sect->flags |= PSECT_COM;       /* Is common */
                                } else if (strcmp(label, "CON") == 0) {
                                    sect->flags &= ~PSECT_COM;      /* Concatenated */
                                } else if (strcmp(label, "RW") == 0 || (support_m11 &&
                                           strcmp(label, "PRV") == 0)) {
                                    sect->flags &= ~PSECT_RO;       /* Not read-only */
                                } else if (strcmp(label, "RO") == 0 || (support_m11 &&
                                           strcmp(label, "SHR") == 0)) {
                                    sect->flags |= PSECT_RO;        /* Is read-only */
                                } else if (strcmp(label, "I") == 0 || (support_m11 &&
                                           strcmp(label, "INS") == 0)) {
                                    sect->flags &= ~PSECT_DATA;     /* Not data */
                                } else if (strcmp(label, "D") == 0 || (support_m11 &&
                                           strcmp(label, "DAT") == 0)) {
                                    sect->flags |= PSECT_DATA;      /* Is data */
                                } else if (strcmp(label, "GBL") == 0) {
                                    sect->flags |= PSECT_GBL;       /* Global */
                                } else if (strcmp(label, "LCL") == 0) {
                                    sect->flags &= ~PSECT_GBL;      /* Local */
                                } else if (support_m11 && (strcmp(label, "BSS") == 0 ||
                                                           strcmp(label, "B")   == 0)) {
                                    sect->flags &= ~PSECT_RO;       /* Not read-only */
                                    sect->flags |= PSECT_DATA;      /* Is data */
                                    /* TODO: We could initialize all BSS sections with zeros
                                     *       We simply need to keep a 'ZER' PSECT flag and ...
                                     *       ... for each 'ZER' PSECT, write an RLD followed by ...
                                     *       ... a TEXT of zeros (size of ZER PSECT) to the object file ...
                                     *       ... at the end of pass 1 just after ENDGSD */
                                } else if (support_m11 && strcmp(label, "HGH") == 0) {
                                    /* For compatability with DEC assemblers (?) */
                                } else if (support_m11 && strcmp(label, "LOW") == 0) {
                                    /* For compatability with DEC assemblers (?) */
                                } else {
                                    report_warn(stack->top, "Unknown %s attribute '%s' [ignored]\n",
                                                            op->label, label);
                                /*  free(label);
                                 *  return 0;  // Continue parsing anyway */
                                }
                                free(label);
                            }
                            /* If a section is declared a second time, and flags
                             * are given, then they must be identical to the
                             * first time.
                             * See page 6-38 of AA-KX10A-TC_PDP-11_MACRO-11_Reference_Manual_May88.pdf .
                             */
                            if (sect->flags != old_flags) {
                                /* The manual also says that any different
                                 * flags are ignored, and an error issued.
                                 * Apparently, that isn't true.
                                 * MACRO-11 silently replaces the old flags
                                 * with the new.  This seems to be a bug.
                                 * Kermit seems to do this in k11cmd.mac:
                                 *      .psect  $pdata          ; line 16
                                 *      .psect  $pdata  ,ro,d,lcl,rel,con
                                 *               ; k11mac.mac, first pass only
                                 *      .psect  $PDATA  ,D      ; line 1083
                                 * and ends up with
                                 * $PDATA  001074    003   (RO,D,LCL,REL,CON)
                                 */

                                if (STRICTEST) {
                                    report_warn(stack->top, "Program section flags not identical [ignored]\n");
                                /*  sect->flags = old_flags;  //  Do not restore the flags */
                                }
                            }
                        }

                        if (!ENABL(LSB)) {
                            lsb = get_next_lsb();
                        }
                        go_section(tr, sect);
                        list_location(stack->top, DOT);
                        list_3digit_value(stack->top, current_pc->section->sector);
                        return CHECK_EOL;
                    }                  /* end PSECT code */
                    break;

                case P_CROSS:
                case P_NOCROSS:
                    /* The .CROSS directive with no symbol list is equivalent to .ENABL CRF and
                     * the .NOCROSS directive with no symbol list is equivalent to .DSABL CRF */

                    if (EOL(*cp)) {
                        if (!ARGS_IGNORE(E_CRF))
                            enabl_list_arg[E_CRF].curval = (op->value == P_CROSS);
                        return 1;
                    }

                    /* V05.05 .[NO]CROSS X where X is undefined creates a GX reference */
                    /* Fall through to P_GLOBL (.[NO]CROSS can both create global undefined symbols) */
                    /* FALL THROUGH */

                case P_GLOBL:
                case P_WEAK:
                    /* V05.05 .GLOBL / .WEAK X where X is undefined creates a G reference */
                    {
                        SYMBOL         *sym;
                        int             islocal = 0;
                        int             retval = 1;

                        SYMBOL_TABLE   *add_table = NULL;
                        int             add_flags = SYMBOLFLAG_NONE,
                                        upd_flags = SYMBOLFLAG_NONE;

                        switch (op->value) {
                        case P_CROSS:
                        case P_NOCROSS:
                            add_table = &implicit_st;
                            add_flags = SYMBOLFLAG_GLOBAL;
                            upd_flags = SYMBOLFLAG_NONE;
                            break;

                       case P_GLOBL:
                           add_table = &symbol_st;
                           add_flags = SYMBOLFLAG_GLOBAL;
                           upd_flags = add_flags;
                           break;

                        case P_WEAK:
                            add_table = &symbol_st;
                            add_flags = SYMBOLFLAG_GLOBAL | SYMBOLFLAG_WEAK;  /* TODO: Is this BAD for RSX? */
                            upd_flags = add_flags;
                            break;
                        }

                        do {
                            /* Loop and make definitions for comma-separated symbols */
                            label = get_symbol(cp, &ncp, &islocal);
                            if (label != NULL && label[0] == '.' && label[1] == '\0') {
                                free(label);
                                label = NULL;
                            }

                            if (label == NULL) {
                                int len = strcspn(cp, "\n");

                                report_err(stack->top, "Invalid syntax [symbol expected] ('%.*s')\n",
                                           (len > 20) ? 20 : len, cp);
                                return 0;
                            }

                            if (islocal) {
                                report_err(stack->top, "Local label '%s' invalid in %s\n", label, op->label);
                                retval = 0;
                            } else {
                                sym = lookup_sym(label, &symbol_st);
                                if (sym) {
                                    sym->flags |= upd_flags;
                                } else {
                                    sym = add_sym(label, 0, add_flags,
                                                  &absolute_section,  /* TODO: Use abs_section_addr() ? */
                                                  add_table, stack->top);
                                }
                            }
                            free(label);
                            cp = skipwhite(ncp);
                            if (*cp == ',' && !EOL(cp[1]))
                                cp = skipdelim(cp);
                        } while (!EOL(*cp));
                        return retval;
                    }

                case P_BYTE:
                case P_WORD:
                    {
                        int             size = (op->value == P_WORD ? 2 : 1);

                        /* .BYTE or .WORD might be followed by nothing, which
                         * is an implicit '.BYTE 0' or '.WORD 0' */
                        if (EOL(*cp)) {
                            store_word(stack->top, tr, size, 0);
                            return 1;
                        } else
                            return do_word(stack, tr, cp, size);
                    }

                case P_BLKW:
                case P_BLKB:
                    {
                        EX_TREE        *value;
                        int             ok = 1;

                        if (EOL(*cp)) {
                            /* If no argument, assume 1. Documented but
                             * discouraged. Par 6.5.3, page 6-32. */
                            value = new_ex_lit(1);
                        } else {
                            value = parse_expr(cp, 0);
                            cp = value->cp;
                        }

                        if (value->type != EX_LIT) {
                            report_err(stack->top, "Argument to .BLKB/.BLKW must be constant\n");
                            ok = 0;
                        } else {
                            if ((op->value == P_BLKW) && (DOT & 1))
                                DOT++; /* Force .BLKW to word boundary */
                            list_location(stack->top, DOT);
                            list_value(stack->top, value->data.lit);  /* Extension to MACRO-11 */
                            DOT += value->data.lit * (op->value == P_BLKW ? 2 : 1);
                            change_dot(tr, 0);
                        }
                        free_tree(value);
                        return ok && CHECK_EOL;
                    }

                case P_ASCII:
                case P_ASCIZ:
                    {
                        cp = skipwhite(cp);
                        do {
                            if (*cp == '<') {
                                EX_TREE        *value;
                                /* A byte value */
                                value = parse_unary_expr(cp, 0);
                                cp = value->cp;
                                store_value(stack, tr, 1, value);
                                free_tree(value);
                            } else {
                                char            quote = (char) toupper((unsigned char) *cp++);  /* MACRO-11 quirk */

                                while (*cp != '\0' && *cp != '\n' && *cp != quote)
                                    store_word(stack->top, tr, 1, *cp++);

                                if (*cp++ != quote) {
                                    if (STRICT) {
                                        report_warn(stack->top, "Mismatched quote (%c)\n", quote);
                                    }
                                }
                            }
                            cp = skipwhite(cp);
                        } while (!EOL(*cp));

                        if (op->value == P_ASCIZ) {
                            store_word(stack->top, tr, 1, 0);
                        }

                        return 1;
                    }

                case P_RAD50:
                    {
                        int             i,
                                        count;
                        unsigned       *rad50;

                        /* Now assemble the argument */
                        rad50 = assemble_rad50(cp, 0, &count, stack);
                        for (i = 0; i < count; i++) {
                            store_word(stack->top, tr, 2, rad50[i]);
                        }
                        free(rad50);
                    }
                    return 1;

                case P_PACKED:
#define PACKED_POSITIVE 0x0C  /* 1100(2) */
#define PACKED_NEGATIVE 0x0D  /* 1101(2) */
#define PACKED_UNSIGNED 0x0F  /* 1111(2) */
                    {
                        int sign;       /* The sign comes at the end */
                        int ndigits,
                             nybbles,
                             byte;
                        char *tmp;

                        cp = skipwhite(cp);

                        if (*cp == '+') {
                            sign = PACKED_POSITIVE;
                            cp++;
                        } else if (*cp == '-') {
                            sign = PACKED_NEGATIVE;
                            cp++;
                        } else {
                            sign = PACKED_UNSIGNED;
                        }

                        /* Count number of digits */
                        ndigits = 0;
                        for (tmp = cp;
                                isdigit((unsigned char )*tmp);
                                tmp++) {
                            ndigits++;
                        }

                        if (ndigits > 31) {
                            report_err(stack->top, "Too many packed decimal digits\n");
                            return 0;
                        }

                        /* If the number of digits is even,
                         * prefix an imaginary zero. */
                        nybbles = !(ndigits % 2);
                        byte = 0;

                        while (isdigit((unsigned char)*cp)) {
                            int value = *cp - '0';
                            byte = (byte << 4) + value;
                            nybbles++;
                            if ((nybbles % 2) == 0) {
                                store_word(stack->top, tr, 1, byte);
                                byte = 0;
                            }
                            cp++;
                        }

                        /* Append the sign, making an even number of nybbles. */
                        byte = (byte << 4) + sign;
                        store_word(stack->top, tr, 1, byte);

                        /* Maybe store the number of digits into a symbol. */
                        cp = skipdelim(cp);
                        if (!EOL(*cp)) {
                            int islocal;

                            label = get_symbol(cp, &cp, &islocal);

                            if (label == NULL) {
                                report_err(stack->top, "Bad .PACKED syntax (symbol expected)\n");
                                return 0;
                            }

                            add_sym(label, ndigits, SYMBOLFLAG_DEFINITION | islocal,
                                    abs_section_addr(current_pc->section), &symbol_st, stack->top);
                            free(label);
                        }

                    }
                    return CHECK_EOL;

                default:
                    report_err(stack->top, "Unimplemented directive %s\n", op->label);
                    return 0;
                }                      /* end switch (PSEUDO operation) */

            case SECTION_INSTRUCTION:
                {
                    /* The PC must always be even. */
                    if (DOT & 1) {
                        report_warn(stack->top, "Instruction on odd address [.EVEN implied]\n");
                        DOT++;         /* And fix it */
                    }

                    switch (op->flags & OC_MASK) {
                    case OC_NONE:
                        /* No operands. */
                        store_word(stack->top, tr, 2, op->value);
                        return CHECK_EOL;

                    case OC_MARK:
                        /* MARK, EMT, TRAP, SPL */  /* TODO: XFC (?) */
                        {
                            EX_TREE        *value;

                            cp = skipwhite(cp);
                            if (*cp == '#') {
                                if (STRICT)
                                    report_warn(stack->top,
                                                "Use of immediate mode (%s #n) is not strictly allowed\n",
                                                op->label);
                                cp = skipwhite(++cp);  /* Allow the hash, but don't require it */
                            }

                            if (EOL (*cp)) {
                                if (STRICT && (op->value == I_MARK || op->value == I_SPL))
                                    report_warn(stack->top, "%s requires an operand [assumed 0]\n", op->label);
                                /* Default argument is 0 */
                                store_word(stack->top, tr, 2, op->value);
                            } else {
                                value = parse_expr(cp, 0);
                                cp = value->cp;
                                if (value->type != EX_LIT) {
                                    if (op->value == I_MARK || op->value == I_SPL) {
                                        report_err(stack->top,
                                                   "%s requires a simple literal operand\n", op->label);
                                        store_word(stack->top, tr, 2, op->value);
                                    } else {
                                        /* EMT, TRAP: handle as two bytes */
                                        store_value(stack, tr, 1, value);
                                        store_word(stack->top, tr, 1, op->value >> 8);
                                    }
                                } else {
                                    if (op->value == I_MARK || op->value == I_SPL) {
                                        unsigned        mask = (op->value == I_MARK) ? 077 /* MARK */ : 07 /* SPL */;

                                        if (value->data.lit & ~mask)
                                            report_err(stack->top, "%s %d. is out of range [0:%d.]\n",
                                                                   op->label, value->data.lit, mask);
                                        store_word(stack->top, tr, 2, op->value | (value->data.lit & mask));
                                    } else {  /* I_EMT || I_TRAP */
                                        if((value->data.lit & 0x8000) ?
                                           (value->data.lit < 0xff00) : (value->data.lit > 0xff))
                                            report_err(stack->top, "%s ^O%o is out of range [-400:377]\n",
                                                                   op->label, value->data.lit);
                                        store_word(stack->top, tr, 2, op->value | (value->data.lit & 0377));
                                    }
                                }
                                free_tree(value);
                            }
                        }
                        return CHECK_EOL;

                    case OC_1GEN:
                        /* One general addressing mode */  {
                            ADDR_MODE       mode;
                            unsigned        word;
                            char           *error;

                            if (!get_mode_check(cp, &cp, &mode, &error, stack->top,
                                                "Invalid addressing mode (%s)\n")) {
                                return 0;
                            }

                            if ((mode.type & 070) == MODE_REG &&
                                (op->value == I_JMP || op->value == I_CALL ||
                                 op->value == I_TSTSET || op->value == I_WRTLCK)) {
                            /*  if (!STRICTER)  */
                                    report_warn(stack->top, "%s Rn is impossible\n", op->label);
                                /* But encode it anyway... */
                            }

                            if ((mode.type & 070) == MODE_AUTO_INCR &&
                                (op->value == I_JMP || op->value == I_CALL)) {
                                if (!RELAXED)
                                    report_warn(stack->top, "%s (Rn)+ is architecture dependent\n", op->label);
                                /* But encode it anyway... */
                            }

                            /* Build instruction word */
                            word = op->value | mode.type;
                            store_word(stack->top, tr, 2, word);
                            mode_extension(tr, &mode, stack->top);
                        }
                        return CHECK_EOL;

                    case OC_2GEN:
                        /* Two general addressing modes */  {
                            ADDR_MODE       left,
                                            right;
                            unsigned        word;
                            char           *error;

                            if (!get_mode_check(cp, &cp, &left, &error, stack->top,
                                                "Invalid addressing mode (1st operand: %s)\n")) {
                                return 0;
                            }

                            cp = skipwhite(cp);
                            if (*cp++ != ',') {
                                report_err(stack->top, "Invalid syntax (comma expected)\n");
                                free_addr_mode(&left);
                                return 0;
                            }

                            if (!get_mode_check(cp, &cp, &right, &error, stack->top,
                                                "Invalid addressing mode (2nd operand: %s)\n")) {
                                free_addr_mode(&left);
                                return 0;
                            }

                            if ((left.type  & 070) == MODE_REG       &&
                                (right.type & 070) >= MODE_AUTO_INCR &&
                                (right.type & 070) <  MODE_OFFSET    &&
                                (right.type & 007) == (left.type & 007)) {
                                if (!RELAXED)
                                    report_warn(stack->top,
                                                "%s Rn,%s%s(Rn)%s is architecture dependent\n",
                                                op->label,
                                                ((right.type & 010) == 010) ? "@" : "",
                                                ((right.type & 060) == 040) ? "-" : "",
                                                ((right.type & 060) == 020) ? "+" : "");
                                /* But encode it anyway... */
                            }

                            /* Build instruction word */
                            word = op->value | left.type << 6 | right.type;
                            store_word(stack->top, tr, 2, word);
                            mode_extension(tr, &left, stack->top);
                            mode_extension(tr, &right, stack->top);
                        }
                        return CHECK_EOL;

                    case OC_BR:
                        /* branches */  {
                            EX_TREE        *value;
                            unsigned        offset;

                            value = parse_expr(cp, 0);
                            cp = value->cp;

                            /* Relative PSECT or absolute? */
                            if (current_pc->section->flags & PSECT_REL) {
                                SYMBOL         *sym = NULL;

                                /* Can't branch unless I can
                                   calculate the offset. */

                                /* You know, I *could* branch
                                   between sections if I feed the
                                   linker a complex relocation
                                   expression to calculate the
                                   offset.  But I won't. */

                                if (!express_sym_offset(value, &sym, &offset)
                                    || sym->section != current_pc->section) {
                                    report_err(stack->top, "Bad branch target (%s)\n",
                                               sym ? "not same section"
                                                   : "can't express offset");
                                    store_word(stack->top, tr, 2, op->value);
                                    free_tree(value);
                                    return 0;
                                }

                                /* Compute the branch offset and
                                   check for addressability */
                                offset += sym->value;
                                offset -= DOT + 2;
                            } else {
                                if (value->type != EX_LIT) {
                                    report_err(stack->top, "Bad branch target (not literal; ABS section)\n");
                                    store_word(stack->top, tr, 2, op->value);
                                    free_tree(value);
                                    return 0;
                                }

                                offset = value->data.lit - (DOT + 2);
                            }

                            if (!check_branch(stack, offset, -256, 255))
                                offset = 0;

                            /* Emit the branch code */
                            offset &= 0777;     /* Reduce to 9 bits */
                            offset >>= 1;       /* Shift to become
                                                   word offset */

                            store_word(stack->top, tr, 2, op->value | offset);

                            free_tree(value);
                        }
                        return CHECK_EOL;

                    case OC_SOB:
                        {
                            EX_TREE        *value;
                            unsigned        reg;
                            unsigned        offset;

                            value = parse_expr(cp, 0);
                            cp = value->cp;

                            reg = get_register(value);
                            free_tree(value);
                            if (reg == NO_REG) {
                                report_err(stack->top, "Invalid addressing mode (register expected)\n");
                                return 0;
                            }

                            cp = skipwhite(cp);
                            if (*cp++ != ',') {
                                report_err(stack->top, "Invalid syntax (comma expected)\n");
                                return 0;
                            }

                            value = parse_expr(cp, 0);
                            cp = value->cp;

                            /* Relative PSECT or absolute? */
                            if (current_pc->section->flags & PSECT_REL) {
                                SYMBOL         *sym;

                                if (!express_sym_offset(value, &sym, &offset)) {
                                    report_err(stack->top, "Bad branch target (can't express offset)\n");
                                    free_tree(value);
                                    return 0;
                                }
                                /* Must be same section */
                                if (sym->section != current_pc->section) {
                                    report_err(stack->top, "Bad branch target (different section)\n");
                                    free_tree(value);
                                    offset = 0;
                                } else {
                                    /* Calculate byte offset */
                                    offset += DOT + 2;
                                    offset -= sym->value;
                                }
                            } else {
                                if (value->type != EX_LIT) {
                                    report_err(stack->top, "Bad branch target (not a literal)\n");
                                    offset = 0;
                                } else {
                                    offset = DOT + 2 - value->data.lit;
                                }
                            }

                            if (!check_branch(stack, offset, 0, 126))
                                offset = 0;

                            offset &= 0177;     /* Reduce to 7 bits */
                            offset >>= 1;       /* Shift to become word offset */
                            store_word(stack->top, tr, 2, op->value | offset | (reg << 6));

                            free_tree(value);
                        }
                        return CHECK_EOL;

                    case OC_ASH:
                        /* First op is gen, second is register. */  {
                            ADDR_MODE       mode;
                            EX_TREE        *value;
                            unsigned        reg;
                            unsigned        word;
                            char           *error;

                            if (!get_mode_check(cp, &cp, &mode, &error, stack->top,
                                                "Invalid addressing mode (1st operand: %s)\n")) {
                                return 0;
                            }

                            cp = skipwhite(cp);
                            if (*cp++ != ',') {
                                report_err(stack->top, "Invalid syntax (comma expected)\n");
                                free_addr_mode(&mode);
                                return 0;
                            }
                            value = parse_expr(cp, 0);
                            cp = value->cp;

                            reg = get_register(value);
                            if (reg == NO_REG) {
                                report_err(stack->top, "Invalid addressing mode (2nd operand: register expected)\n");
                                free_tree(value);
                                free_addr_mode(&mode);
                                return 0;
                            }

                            /* Instruction word */
                            word = op->value | mode.type | (reg << 6);
                            store_word(stack->top, tr, 2, word);
                            mode_extension(tr, &mode, stack->top);
                            free_tree(value);
                        }
                        return CHECK_EOL;

                    case OC_JSR:
                        /* For JSR and XOR, first op is register, second is gen. */  {
                            ADDR_MODE       mode;
                            EX_TREE        *value;
                            unsigned        reg;
                            unsigned        word;
                            char           *error;

                            value = parse_expr(cp, 0);
                            cp = value->cp;

                            reg = get_register(value);
                            if (reg == NO_REG) {
                                report_err(stack->top, "Invalid addressing mode (1st operand: register exected)\n");
                                free_tree(value);
                                return 0;
                            }

                            cp = skipwhite(cp);
                            if (*cp++ != ',') {
                                report_err(stack->top, "Invalid syntax (comma expected)\n");
                                return 0;
                            }

                            if (!get_mode_check(cp, &cp, &mode, &error, stack->top,
                                           "Invalid addressing mode (2nd operand: %s)\n")) {
                                free_tree(value);
                                return 0;
                            }

                            if (op->value == I_JSR && (mode.type & 070) == 0) {
                            /*  if (!STRICTER)  */
                                    report_warn(stack->top, "JSR Rn,Rm is impossible\n");
                                /* But encode it anyway... */
                            }

                            if (op->value == I_JSR && (mode.type & 070) == MODE_AUTO_INCR) {
                                if (!RELAXED)
                                    report_warn(stack->top, "JSR Rn,(Rm)+ is architecture dependent\n");
                                /* But encode it anyway... */
                            }

                            if (op->value == I_XOR && (mode.type & 070) >= MODE_AUTO_INCR &&
                                                      (mode.type & 070) <  MODE_OFFSET    &&
                                                      (mode.type & 007) == reg) {
                                if (!RELAXED)
                                    report_warn(stack->top,
                                                "%s Rn,%s%s(Rn)%s is architecture dependent\n",
                                                op->label,
                                                ((mode.type & 010) == 010) ? "@" : "",
                                                ((mode.type & 060) == 040) ? "-" : "",
                                                ((mode.type & 060) == 020) ? "+" : "");
                                /* But encode it anyway... */
                            }

                            word = op->value | mode.type | (reg << 6);
                            store_word(stack->top, tr, 2, word);
                            mode_extension(tr, &mode, stack->top);
                            free_tree(value);
                        }
                        return CHECK_EOL;

                    case OC_1REG:
                        /* One register (RTS, FADD, FSUB, FMUL, FDIV) */  {
                            EX_TREE        *value;
                            unsigned        reg;

                            value = parse_expr(cp, 0);
                            cp = value->cp;
                            reg = get_register(value);
                            if (reg == NO_REG) {
                                report_err(stack->top, "Invalid addressing mode (register expected)\n");
                                reg = 0;
                            }

                            store_word(stack->top, tr, 2, op->value | reg);
                            free_tree(value);
                        }
                        return CHECK_EOL;

#if NODO
/*
 * Although it is arguable that the FPP TSTF/TSTD instruction has 1
 * operand which is a floating point source, the PDP11 Architecture
 * Handbook describes it as a destination, and MACRO11 V05.05 doesn't
 * allow a FP literal argument.
 */
                    case OC_FPP_FSRC:
                        /* One fp immediate or a general addressing mode */  {
                            ADDR_MODE       mode;
                            unsigned        word;

                            error = NULL;
                            get_fp_src_mode(cp, &cp, &mode, &error);
                            if (error) {
                                report_err(stack->top,
                                           "Invalid addressing mode (1st operand, fsrc: %s)\n",
                                           error);
                                return 0;
                            }

                            /* Build instruction word */
                            word = op->value | mode.type;
                            store_word(stack->top, tr, 2, word);
                            mode_extension(tr, &mode, stack->top);
                        }
                        return CHECK_EOL;
#endif

                    case OC_FPP_SRCAC:
                    case OC_FPP_FSRCAC:
                        /* One gen and one reg 0-3 */  {
                            ADDR_MODE       mode;
                            EX_TREE        *value;
                            unsigned        reg;
                            unsigned        word;
                            char           *error;

                            if ((op->flags & OC_MASK) == OC_FPP_FSRCAC) {
                                error = NULL;
                                get_fp_src_mode(cp, &cp, &mode, &error);
                                if (error) {
                                    report_err(stack->top,
                                               "Invalid addressing mode (1st operand, fsrc: %s)\n",
                                               error);
                                    return 0;
                                }
                            } else {
                                if (!get_mode_check(cp, &cp, &mode, &error, stack->top,
                                                    "Invalid addressing mode (1st operand: %s)\n")) {
                                    return 0;
                                }
                            }

                            cp = skipwhite(cp);
                            if (*cp++ != ',') {
                                report_err(stack->top, "Invalid syntax (comma expected)\n");
                                free_addr_mode(&mode);
                                return 0;
                            }

                            value = parse_expr(cp, 0);
                            cp = value->cp;

                            reg = get_register(value);
                            if (reg == NO_REG || reg > 3) {
                                report_err(stack->top, "Invalid destination fp register\n");
                                reg = 0;
                            }

                            /*
                             * We could check here that the general mode
                             * is not AC6 or AC7, but the original Macro11
                             * doesn't do that either.
                             */
                            word = op->value | mode.type | (reg << 6);
                            store_word(stack->top, tr, 2, word);
                            mode_extension(tr, &mode, stack->top);
                            free_tree(value);
                        }
                        return CHECK_EOL;

                    case OC_FPP_ACFDST:
                        /* One reg 0-3 and one fdst */  {
                            ADDR_MODE       mode;
                            EX_TREE        *value;
                            unsigned        reg;
                            unsigned        word;
                            char           *error;

                            value = parse_expr(cp, 0);
                            cp = value->cp;

                            reg = get_register(value);
                            if (reg == NO_REG || reg > 3) {
                                report_err(stack->top, "Invalid source fp register\n");
                                reg = 0;
                            }

                            cp = skipwhite(cp);
                            if (*cp++ != ',') {
                                report_err(stack->top, "Invalid syntax (comma expected)\n");
                                free_tree(value);
                                return 0;
                            }

                            if (!get_mode_check(cp, &cp, &mode, &error, stack->top,
                                                "Invalid addressing mode (2nd operand: %s)\n")) {
                                free_tree(value);
                                return 0;
                            }

                            /*
                             * We could check here that the general mode
                             * is not AC6 or AC7, but the original Macro11
                             * doesn't do that either.
                             *
                             * For some (mostly STore instructions) the
                             * destination isn't a FDST but a plain DST.
                             */
                            word = op->value | mode.type | (reg << 6);
                            store_word(stack->top, tr, 2, word);
                            mode_extension(tr, &mode, stack->top);
                            free_tree(value);
                        }
                        return CHECK_EOL;

                        {   int nwords;
                            EX_TREE *expr[4];
                    case OC_CIS2:
                        /* Either no operands or 2 (mostly) address operand words
                         * (extension) */
                            nwords = 2;
                            goto cis_common;
                    case OC_CIS3:
                        /* Either no operands or 3 (mostly) address operand words
                         * (extension) */
                            nwords = 3;
                            goto cis_common;
                    case OC_CIS4:
                        /* Either no operands or 4 (mostly) address operand words
                         * (extension) */
                            nwords = 4;
                    cis_common:
                            if (!EOL(*cp)) {
                                int i;

                                for (i = 0; i < nwords; i++) {
                                    if (i > 0) {
                                        cp = skipwhite(cp);
                                        if (*cp++ != ',') {
                                            report_err(stack->top,
                                                       "Invalid syntax (operand %d: comma expected)\n", i+1);
                                            cp--;
                                        }
                                    }
                                    {
                                        EX_TREE *ex = parse_expr(cp, 0);

                                        if (!expr_ok(ex)) {
                                            report_err(stack->top, "Invalid expression (operand %d)\n", i+1);
                                        }
                                        cp = ex->cp;
                                        expr[i] = ex;
                                    }
                                }
                            } else {
                                expr[0] = NULL;
                            }

                            store_word(stack->top, tr, 2, op->value);

                            if (expr[0]) {
                                int i;

                                for (i = 0; i < nwords; i++) {
                                    store_value(stack, tr, 2, expr[i]);
                                }
                            }
                        }
                        return CHECK_EOL;

                    default:
                        report_err(stack->top, "Unimplemented instruction format\n");
                        return 0;
                    }                  /* end(handle an instruction) */
                }
                break;
            }                          /* end switch(section type) */
        }                              /* end if (op is a symbol) */
    }

    /* If .ENABL MCL is in effect, try to define the symbol as a
     * library macro if it is not a defined symbol. */
    if (ENABL(MCL)) {
        if (lookup_sym(label, &symbol_st) == NULL) {
            if (do_mcall (label, stack)) {
                goto do_mcalled_macro;
            }
        }
    }

    /* Only thing left is an implied .WORD directive */
    /*JH: fall through in case of illegal opcode, illegal label! */
    free(label);

    return do_word(stack, tr, cp, 2);
}

int get_next_lsb(
    void)
{
    if (lsb_used) {
        lsb_used = 0;
#if DEBUG_LSB
        if (enabl_debug > 1 && lstfile) {
            fprintf(lstfile, "get_next_lsb: lsb: %d becomes %d (= next_lsb)\n", lsb, next_lsb);
        }
#endif
        return next_lsb++;
    } else {
#if DEBUG_LSB
        if (enabl_debug > 1 && lstfile) {
            fprintf(lstfile, "get_next_lsb: lsb: stays %d\n", lsb);
        }
#endif
        return lsb;
    }
}

/* assemble_stack assembles the input stack.  It returns the error count. */

int assemble_stack(
    STACK *stack,
    TEXT_RLD *tr)
{
    int             res;
    int             errcount = 0;

    while ((res = assemble(stack, tr)) >= 0) {
        list_flush();
        if (res == 0)
            errcount++;                   /* Count an error */
    }

    return errcount;
}
