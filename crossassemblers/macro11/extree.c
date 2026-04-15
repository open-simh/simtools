#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "extree.h"                    /* my own definitions */

#include "util.h"
#include "assemble_globals.h"
#include "assemble_aux.h"
#include "listing.h"
#include "object.h"

#define DEBUG_REGEXPR   0

/* Diagnostic: print an expression tree.  I used this in various
   places to help me diagnose parse problems, by putting in calls to
   print_tree when I didn't understand why something wasn't working.
   This is currently dead code, nothing calls it; but I don't want it
   to go away. Hopefully the compiler will realize when it's dead, and
   eliminate it. */

void print_tree(
    FILE *printfile,
    EX_TREE *tp,
    int depth)
{
    SYMBOL         *sym;

    if (printfile == NULL) {
        return;
    }

    if (tp == NULL) {
        fprintf(printfile, "(null)");
        return;
    }

    switch (tp->type) {
    case EX_LIT:
        fprintf(printfile, "%o", tp->data.lit & 0177777);
        break;

    case EX_SYM:
    case EX_TEMP_SYM:
        sym = tp->data.symbol;
        fprintf(printfile, "%s{%s%o:%s}", tp->data.symbol->label, symflags(sym), sym->value,
                sym->section->label);
        break;

    case EX_UNDEFINED_SYM:
        fprintf(printfile, "%s{%o:undefined}", tp->data.symbol->label, tp->data.symbol->value);
        break;

    case EX_COM:
        fprintf(printfile, "^C<");
        print_tree(printfile, tp->data.child.left, depth + 4);
        fprintf(printfile, ">");
        break;

    case EX_NEG:
        fprintf(printfile, "-<");
        print_tree(printfile, tp->data.child.left, depth + 4);
        fputc('>', printfile);
        break;

    case EX_REG:
        fprintf(printfile, "%%<");
        print_tree(printfile, tp->data.child.left, depth + 4);
        fputc('>', printfile);
        break;

    case EX_ERR:
        fprintf(printfile, "{expression error}");
        if (tp->data.child.left) {
            fputc('<', printfile);
            print_tree(printfile, tp->data.child.left, depth + 4);
            fputc('>', printfile);
        }
        break;

    case EX_ADD:
        fputc('<', printfile);
        print_tree(printfile, tp->data.child.left, depth + 4);
        fputc('+', printfile);
        print_tree(printfile, tp->data.child.right, depth + 4);
        fputc('>', printfile);
        break;

    case EX_SUB:
        fputc('<', printfile);
        print_tree(printfile, tp->data.child.left, depth + 4);
        fputc('-', printfile);
        print_tree(printfile, tp->data.child.right, depth + 4);
        fputc('>', printfile);
        break;

    case EX_MUL:
        fputc('<', printfile);
        print_tree(printfile, tp->data.child.left, depth + 4);
        fputc('*', printfile);
        print_tree(printfile, tp->data.child.right, depth + 4);
        fputc('>', printfile);
        break;

    case EX_DIV:
        fputc('<', printfile);
        print_tree(printfile, tp->data.child.left, depth + 4);
        fputc('/', printfile);
        print_tree(printfile, tp->data.child.right, depth + 4);
        fputc('>', printfile);
        break;

    case EX_AND:
        fputc('<', printfile);
        print_tree(printfile, tp->data.child.left, depth + 4);
        fputc('&', printfile);
        print_tree(printfile, tp->data.child.right, depth + 4);
        fputc('>', printfile);
        break;

    case EX_OR:
        fputc('<', printfile);
        print_tree(printfile, tp->data.child.left, depth + 4);
        fputc('!', printfile);
        print_tree(printfile, tp->data.child.right, depth + 4);
        fputc('>', printfile);
        break;

    case EX_LSH:
        fputc('<', printfile);
        print_tree(printfile, tp->data.child.left, depth + 4);
        fputc('_', printfile);
        print_tree(printfile, tp->data.child.right, depth + 4);
        fputc('>', printfile);
        break;

    default:
        fprintf(printfile, "(node %d)", tp->type);
        break;
    }

    if (depth == 0)
        fputc('\n', printfile);
}

/* num_subtrees tells you how many subtrees this EX_TREE has. */

int num_subtrees(
    EX_TREE *tp)
{
    switch (tp->type) {
    case EX_UNDEFINED_SYM:
    case EX_TEMP_SYM:
    case EX_SYM:
    case EX_LIT:
        return 0;
        break;

    case EX_COM:
    case EX_NEG:
    case EX_REG:
    case EX_ERR:
        return 1;

    case EX_ADD:
    case EX_SUB:
    case EX_MUL:
    case EX_DIV:
    case EX_AND:
    case EX_OR:
    case EX_LSH:
        return 2;
    }

    return 0;
}

/* free_tree frees an expression tree. */

void free_tree(
    EX_TREE *tp)
{
    if (!tp)
        return;

    switch (num_subtrees(tp)) {
        case 0:
            switch (tp->type) {
            case EX_UNDEFINED_SYM:
            case EX_TEMP_SYM:
                free(tp->data.symbol->label);
                free(tp->data.symbol);
                break;

            case EX_SYM:
            case EX_LIT:
                break;

            default:
                assert(0);
                break;
            }
        break;
        case 2:
            free_tree(tp->data.child.right);
            /* FALLTHROUGH */
        case 1:
            free_tree(tp->data.child.left);
            break;
    }

    free(tp);
}

/* new_temp_sym allocates a new EX_TREE entry of type "TEMPORARY
   SYMBOL" (slight semantic difference from "UNDEFINED"). */

static EX_TREE *new_temp_sym(
    char *label,
    SECTION *section,
    unsigned value)
{
    SYMBOL         *sym;
    EX_TREE        *tp;

    sym = memcheck(malloc(sizeof(SYMBOL)));
    sym->label = memcheck(strdup(label));
    sym->flags = SYMBOLFLAG_DEFINITION;
    sym->stmtno = stmtno;
    sym->next = NULL;
    sym->section = section;
    sym->value = value;

    tp = new_ex_tree(EX_TEMP_SYM);
    tp->data.symbol = sym;

    return tp;
}

SYMBOL *dup_symbol(
    SYMBOL *sym)
{
    SYMBOL *res;

    if (sym == NULL) {
        return NULL;
    }

    res = memcheck(malloc(sizeof(SYMBOL)));
    res->label = memcheck(strdup(sym->label));
    res->flags = sym->flags;
    res->stmtno = sym->stmtno;
    res->next = NULL;
    res->section = sym->section;
    res->value = sym->value;

    return res;
}


EX_TREE *dup_tree(
    EX_TREE *tp)
{
    EX_TREE *res = NULL;

    if (tp == NULL) {
        return NULL;
    }

    res = new_ex_tree(tp->type);
    res->cp = tp->cp;

    switch (num_subtrees(tp)) {
        case 0:
            switch (tp->type) {
            case EX_UNDEFINED_SYM:
            case EX_TEMP_SYM:
                res->data.symbol = dup_symbol(tp->data.symbol);
                break;

            /* The symbol reference in EX_SYM is not freed in free_tree() */
            case EX_SYM:
                res->data.symbol = tp->data.symbol;
                break;

            case EX_LIT:
                res->data.lit = tp->data.lit;
                break;

            default:
                assert(0);
                break;
            }
        break;
        case 2:
            res->data.child.right = dup_tree(tp->data.child.right);
            /* FALLTHROUGH */
        case 1:
            res->data.child.left = dup_tree(tp->data.child.left);
            break;
    }

    return res;
}

#define RELTYPE(tp) (((tp)->type == EX_SYM || (tp)->type == EX_TEMP_SYM) && \
        (tp)->data.symbol->section->flags & PSECT_REL)

/* evaluate "evaluates" an EX_TREE, ideally trying to produce a
   constant value, else a symbol plus an offset.
   Leaves the input tree untouched and creates a new tree as result. */
EX_TREE        *evaluate_rec(
    EX_TREE *tp,
    int flags,
    int *outflags)
{
    EX_TREE        *res;
    char           *cp = tp->cp;

    switch (tp->type) {
    case EX_SYM:
        {
            SYMBOL         *sym = tp->data.symbol;

            /* Change some symbols to "undefined" */

#if !DISABLE_EXTREE_IF_DF_NDF
            if (flags & EVALUATE_DEFINEDNESS) {
                int             is_undefined = 0;

                /* I'd prefer this behavior, but MACRO.SAV is a bit too primitive. */
#if NODO
                /* A temporary symbol defined later is "undefined." */
                if (!(sym->flags & PERMANENT) && sym->stmtno > stmtno)
                    is_undefined = 1;
#endif

                /* A global symbol with no assignment is "undefined." */
                /* Go figure. */
                if (SYM_IS_IMPORTED(sym))
                    is_undefined = 1;

                /* A symbol marked as undefined is undefined */
                if (sym->flags & SYMBOLFLAG_UNDEFINED)
                    is_undefined = 1;

                if (is_undefined) {
                    res = new_temp_sym(tp->data.symbol->label, tp->data.symbol->section,
                                       tp->data.symbol->value);
                    res->type = EX_UNDEFINED_SYM;
                    break;
                }
            }
#endif

            /* Turn defined absolute symbol to a literal */
            if (!(sym->section->flags & PSECT_REL)
                && !SYM_IS_IMPORTED(sym)) {

#if DISABLE_EXTREE_IF_DF_NDF
                res = new_ex_lit(sym->value);
#else
                if (flags & EVALUATE_DEFINEDNESS && !sym->value)
                    res = new_ex_lit(1);
                else
                    res = new_ex_lit(sym->value);
#endif

                if (sym->section->type == SECTION_REGISTER) {
                    *outflags |= EVALUATE_OUT_IS_REGISTER;
                }

                break;
            }

            /* Make a temp copy of any reference to "." since it might
               change as complex relocatable expressions are written out
             */
            if (strcmp(sym->label, ".") == 0) {
                res = new_temp_sym(".", sym->section, sym->value);
                break;
            }

            /* Copy other symbol reference verbatim. */
            res = dup_tree(tp);
            break;
        }

    case EX_LIT:
        res = dup_tree(tp);
        break;

    case EX_TEMP_SYM:
    case EX_UNDEFINED_SYM:
        /* Copy temp and undefined symbols */
        res = new_temp_sym(tp->data.symbol->label, tp->data.symbol->section, tp->data.symbol->value);
        res->type = tp->type;
        break;

    case EX_COM:
        /* Complement */
        tp = evaluate_rec(tp->data.child.left, flags, outflags);
        if (tp->type == EX_LIT) {
            /* Complement the literal */
            res = new_ex_lit(~tp->data.lit);
            free_tree(tp);
        } else {
            /* Copy verbatim. */
            res = new_ex_una(EX_COM, tp);
        }

        break;

    case EX_NEG:
        tp = evaluate_rec(tp->data.child.left, flags, outflags);
        if (tp->type == EX_LIT) {
            /* negate literal */
            res = new_ex_lit((unsigned) -(int) tp->data.lit);
            free_tree(tp);
        } else if (tp->type == EX_TEMP_SYM ||
                   (tp->type == EX_SYM &&
                    (tp->data.symbol->flags & SYMBOLFLAG_DEFINITION))) {
            /* Make a temp sym with the negative value of the given
               sym (this works for symbols within relocatable sections
               too) */
            res = new_temp_sym("*NEG", tp->data.symbol->section, (unsigned) -(int) tp->data.symbol->value);
            res->cp = tp->cp;
            free_tree(tp);
        } else {
            /* Copy verbatim. */
            res = new_ex_una(EX_NEG, tp);
        }
        break;

    case EX_REG:
        res = evaluate_rec(tp->data.child.left, flags, outflags);
        *outflags |= EVALUATE_OUT_IS_REGISTER;
        break;

    case EX_ERR:
        /* Copy */
        res = dup_tree(tp);
        break;

    case EX_ADD:
        {
            EX_TREE        *left,
                           *right;

            left = evaluate_rec(tp->data.child.left, flags, outflags);
            right = evaluate_rec(tp->data.child.right, flags, outflags);

            /* Both literals?  Sum them and return result. */
            if (left->type == EX_LIT && right->type == EX_LIT) {
                res = new_ex_lit(left->data.lit + right->data.lit);
                free_tree(left);
                free_tree(right);
                break;
            }

            /* Commutative: A+x == x+A.
               Simplify by putting the literal on the right */
            if (left->type == EX_LIT) {
                EX_TREE        *temp = left;

                left = right;
                right = temp;
            }

            if (right->type == EX_LIT &&        /* Anything plus 0 == itself */
                right->data.lit == 0) {
                res = left;
                free_tree(right);
                break;
            }

            /* Relative symbol plus lit is replaced with a temp sym
               holding the sum */
            if (RELTYPE(left) && right->type == EX_LIT) {
                SYMBOL         *sym = left->data.symbol;

                res = new_temp_sym("*ADD", sym->section, sym->value + right->data.lit);
                free_tree(left);
                free_tree(right);
                break;
            }

            /* Associative:  <A+x>+y == A+<x+y> */
            /*  and if x+y is constant, I can do that math. */
            if (left->type == EX_ADD && right->type == EX_LIT) {
                EX_TREE        *leftright = left->data.child.right;

                if (leftright->type == EX_LIT) {
                    /* Do the shuffle */
                    res = left;
                    leftright->data.lit += right->data.lit;
                    free_tree(right);
                    break;
                }
            }

            /* Associative:  <A-x>+y == A+<y-x> */
            /*  and if y-x is constant, I can do that math. */
            if (left->type == EX_SUB && right->type == EX_LIT) {
                EX_TREE        *leftright = left->data.child.right;

                if (leftright->type == EX_LIT) {
                    /* Do the shuffle */
                    res = left;
                    leftright->data.lit = right->data.lit - leftright->data.lit;
                    free_tree(right);
                    break;
                }
            }

            /* Anything else returns verbatim */
            res = new_ex_bin(EX_ADD, left, right);
        }
        break;

    case EX_SUB:
        {
            EX_TREE        *left,
                           *right;

            left = evaluate_rec(tp->data.child.left, flags, outflags);
            right = evaluate_rec(tp->data.child.right, flags, outflags);

            /* Both literals?  Subtract them and return a lit. */
            if (left->type == EX_LIT && right->type == EX_LIT) {
                res = new_ex_lit(left->data.lit - right->data.lit);
                free_tree(left);
                free_tree(right);
                break;
            }

            if (right->type == EX_LIT &&        /* Symbol minus 0 == symbol */
                right->data.lit == 0) {
                res = left;
                free_tree(right);
                break;
            }

            /* A relocatable minus an absolute - make a new temp sym
               to represent that. */
            if (RELTYPE(left) && right->type == EX_LIT) {
                SYMBOL         *sym = left->data.symbol;

                res = new_temp_sym("*SUB", sym->section, sym->value - right->data.lit);
                free_tree(left);
                free_tree(right);
                break;
            }

            if (RELTYPE(left) && RELTYPE(right) && left->data.symbol->section == right->data.symbol->section) {
                /* Two defined symbols in the same psect.  Resolve
                   their difference as a literal. */
                res = new_ex_lit(left->data.symbol->value - right->data.symbol->value);
                free_tree(left);
                free_tree(right);
                break;
            }

            /* Associative:  <A+x>-y == A+<x-y> */
            /*  and if x-y is constant, I can do that math. */
            if (left->type == EX_ADD && right->type == EX_LIT) {
                EX_TREE        *leftright = left->data.child.right;

                if (leftright->type == EX_LIT) {
                    /* Do the shuffle */
                    res = left;
                    leftright->data.lit -= right->data.lit;
                    free_tree(right);
                    break;
                }
            }

            /* Associative:  <A-x>-y == A-<x+y> */
            /*  and if x+y is constant, I can do that math. */
            if (left->type == EX_SUB && right->type == EX_LIT) {
                EX_TREE        *leftright = left->data.child.right;

                if (leftright->type == EX_LIT) {
                    /* Do the shuffle */
                    res = left;
                    leftright->data.lit += right->data.lit;
                    free_tree(right);
                    break;
                }
            }

            /* Anything else returns verbatim */
            res = new_ex_bin(EX_SUB, left, right);
        }
        break;

    case EX_MUL:
        {
            EX_TREE        *left,
                           *right;

            left = evaluate_rec(tp->data.child.left, flags, outflags);
            right = evaluate_rec(tp->data.child.right, flags, outflags);

            /* Can only multiply if both are literals */
            if (left->type == EX_LIT && right->type == EX_LIT) {
                res = new_ex_lit(left->data.lit * right->data.lit);
                free_tree(left);
                free_tree(right);
                break;
            }

            /* Commutative: A*x == x*A.
               Simplify by putting the literal on the right */
            if (left->type == EX_LIT) {
                EX_TREE        *temp = left;

                left = right;
                right = temp;
            }

            if (right->type == EX_LIT &&        /* Symbol times 1 == symbol */
                right->data.lit == 1) {
                res = left;
                free_tree(right);
                break;
            }

            if (right->type == EX_LIT &&        /* Symbol times 0 == 0 */
                right->data.lit == 0) {
                res = right;
                free_tree(left);
                break;
            }

            if (right->type == EX_LIT &&        /* Symbol times -1 == -symbol */
                right->data.lit == 0xffff) {
                res = evaluate(new_ex_una(EX_NEG, left), 0);
                free_tree(left);
                free_tree(right);
                break;
            }

            /* Associative: <A*x>*y == A*<x*y> */
            /* If x*y is constant, I can do this math. */
            /* Is this safe?  I will potentially be doing it */
            /* with greater accuracy than the target platform. */
            /* Hmmm. */

            if (left->type == EX_MUL && right->type == EX_LIT) {
                EX_TREE        *leftright = left->data.child.right;

                if (leftright->type == EX_LIT) {
                    /* Do the shuffle */
                    res = left;
                    leftright->data.lit *= right->data.lit;
                    free_tree(right);
                    break;
                }
            }

            /* Anything else returns verbatim */
            res = new_ex_bin(EX_MUL, left, right);
        }
        break;

    case EX_DIV:
        {
            EX_TREE        *left,
                           *right;

            left  = evaluate_rec(tp->data.child.left,  flags, outflags);
            right = evaluate_rec(tp->data.child.right, flags, outflags);

            /* Optimize if right == -1 || 0 || +1 */
            if (right->type == EX_LIT) {
                if (right->data.lit == 0) {
                    /* Can't divide by zero - V05.05 returns 0 without error */

                    /* Note that the RSX-11M/PLUS task builder does not allow
                     * a divide-by-zero with the following error message ...
                     * TKB -- *DIAG*-Complex relocation error-divide by zero module x */

                    /* Handle divide by zero (= 0) */
                    if (!RELAXED)
                        res = ex_err(new_ex_lit(0), right->cp);
                    else
                        res = new_ex_lit(0);
                    free_tree(left);
                    free_tree(right);
                    break;
                }

                if (right->data.lit == 1) {
                    /* Handle divide by one (= left) */
                    res = left;
                    free_tree(right);
                    break;
                }

                if (right->type == EX_LIT &&
                    right->data.lit == 0xffff) {
                    /* Handle divide by minus-one (= -left) */
                    res = evaluate(new_ex_una(EX_NEG, left), 0);
                    free_tree(left);
                    free_tree(right);
                    break;
                }
            }

            /* Can only divide if both are literals */
            if (left->type == EX_LIT && right->type == EX_LIT) {
                int             dividend = (int) left->data.lit  & 0xffff;
                int             divisor  = (int) right->data.lit & 0xffff;

                if (dividend & 0x8000)
                    dividend =  dividend - 0x10000;

                if (divisor & 0x8000)
                    divisor = divisor - 0x10000;

                res = new_ex_lit(dividend / divisor);
                free_tree(left);
                free_tree(right);
                break;
            }

            /* Anything else returns verbatim */
            res = new_ex_bin(EX_DIV, left, right);
        }
        break;

    case EX_AND:
        {
            EX_TREE        *left,
                           *right;

            left = evaluate_rec(tp->data.child.left, flags, outflags);
            right = evaluate_rec(tp->data.child.right, flags, outflags);

            /* Operate if both are literals */
            if (left->type == EX_LIT && right->type == EX_LIT) {
                res = new_ex_lit(left->data.lit & right->data.lit);
                free_tree(left);
                free_tree(right);
                break;
            }

            /* Commutative: A&x == x&A.
               Simplify by putting the literal on the right */
            if (left->type == EX_LIT) {
                EX_TREE        *temp = left;

                left = right;
                right = temp;
            }

            if (right->type == EX_LIT &&        /* Symbol AND 0 == 0 */
                right->data.lit == 0) {
                res = new_ex_lit(0);
                free_tree(left);
                free_tree(right);
                break;
            }

            if (right->type == EX_LIT &&        /* Symbol AND 0177777 == symbol */
                right->data.lit == 0177777) {
                res = left;
                free_tree(right);
                break;
            }

            /* Anything else returns verbatim */
            res = new_ex_bin(EX_AND, left, right);
        }
        break;

    case EX_OR:
        {
            EX_TREE        *left,
                           *right;

            left = evaluate_rec(tp->data.child.left, flags, outflags);
            right = evaluate_rec(tp->data.child.right, flags, outflags);

            /* Operate if both are literals */
            if (left->type == EX_LIT && right->type == EX_LIT) {
                res = new_ex_lit(left->data.lit | right->data.lit);
                free_tree(left);
                free_tree(right);
                break;
            }

            /* Commutative: A!x == x!A.
               Simplify by putting the literal on the right */
            if (left->type == EX_LIT) {
                EX_TREE        *temp = left;

                left = right;
                right = temp;
            }

            if (right->type == EX_LIT &&        /* Symbol OR 0 == symbol */
                right->data.lit == 0) {
                res = left;
                free_tree(right);
                break;
            }

            if (right->type == EX_LIT &&        /* Symbol OR 0177777 == 0177777 */
                right->data.lit == 0177777) {
                res = new_ex_lit(0177777);
                free_tree(left);
                free_tree(right);
                break;
            }

            /* Anything else returns verbatim */
            res = new_ex_bin(EX_OR, left, right);
        }
        break;

    case EX_LSH:
        {
            EX_TREE        *left,
                           *right;

            left  = evaluate_rec(tp->data.child.left,  flags, outflags);
            right = evaluate_rec(tp->data.child.right, flags, outflags);

            /* Zero shifted by anything == 0 */
            if (left->type == EX_LIT &&
                left->data.lit == 0) {
                res = left;
                free_tree(right);
                break;
            }

            if (right->type == EX_LIT) {
                int             shift = right->data.lit;

                /* Symbol shifted 0 == symbol */
                if (shift == 0) {
                    res = left;
                    free_tree(right);
                    break;
                }

                if (shift & 0x8000)
                    shift = shift - 0x10000;

                /* Anything shifted 16 == 0 */
                if (shift > 15 || shift < -15) {
                    res = new_ex_lit(0);
                    free_tree(left);
                    free_tree(right);
                    break;
                }

                /* Operate if both are literals */
                if (left->type == EX_LIT) {
                    if (shift < 0)
                        res = new_ex_lit(left->data.lit >> -shift);
                    else
                        res = new_ex_lit(left->data.lit << shift);
                    free_tree(left);
                    free_tree(right);
                    break;
                }

                /* Other shifts become '*' or '/' */
                if (shift > 0)
                    res = new_ex_bin(EX_MUL, left, new_ex_lit(1 << shift));
                else {  /* (shift < 0) */
                    res = new_ex_bin(EX_AND, left, new_ex_lit(~(0x7fff >> (-shift-1))));
                    res = new_ex_bin(EX_DIV, res,  new_ex_lit(1 << -shift));
                    res = new_ex_bin(EX_AND, res,  new_ex_lit(0x7fff >> (-shift-1)));
                }
                free_tree(right);
                break;
            }

            /* Anything else returns verbatim */
            res = new_ex_bin(EX_LSH, left, right);
        }
        break;

    default:
        fprintf(stderr, "evaluate_rec: Invalid tree: ");
        print_tree(stderr, tp, 0);
        return NULL;
    }

    res->cp = cp;

    return res;
}

EX_TREE        *evaluate(
    EX_TREE *tp,
    int flags)
{
    EX_TREE        *res;
    int             outflags = 0;

#if DEBUG_REGEXPR
    fprintf(stderr, "evaluate: ");
    print_tree(stderr, tp, 0);
#endif /* DEBUG_REGEXPR */
    /*
     * Check the common simple case for register references: R0...PC.
     * Copy those directly, without going into the recursion.
     * Therefore, in the recursion it can be assumed that the expression
     * is more complex, and it is worth the overhead of converting them
     * to an EX_LIT plus the EVALUATE_OUT_IS_REGISTER flag
     * (and back again, up here).
     */
    if (tp->type == EX_SYM &&
            tp->data.symbol->section->type == SECTION_REGISTER &&
            !SYM_IS_IMPORTED(tp->data.symbol)) {
        res = dup_tree(tp);
    } else {
        res = evaluate_rec(tp, flags, &outflags);

        if (outflags & EVALUATE_OUT_IS_REGISTER) {
            int             regno = get_register(res);

            if (regno == NO_REG) {
                report_err(NULL, "Register expression out of range\n");
#if DEBUG_REGEXPR
                print_tree(stderr, tp, 0);
                print_tree(stderr, res, 0);
#endif /* DEBUG_REGEXPR */
                /* TODO: maybe make this a EX_TEMP_SYM? */
                {
                    EX_TREE        *newresult = new_ex_tree(EX_SYM);

                    newresult->cp = res->cp;
                    newresult->data.symbol = REG_ERR;
                    free_tree(res);
                    res = newresult;
                }
            } else {
                EX_TREE        *newresult = new_ex_tree(EX_SYM);

                newresult->cp = res->cp;
                newresult->data.symbol = reg_sym[regno];
                free_tree(res);
                res = newresult;
            }
        }
    }

    return res;
}


/* Allocate an EX_TREE */

EX_TREE        *new_ex_tree(
    int type)
{
    EX_TREE        *tr = memcheck(calloc(1, sizeof(EX_TREE)));

    tr->type = type;

    return tr;
}


/* Create an EX_TREE representing a parse error */

EX_TREE        *ex_err(
    EX_TREE *tp,
    char *cp)
{
    EX_TREE        *errtp;

    errtp = new_ex_tree(EX_ERR);
    errtp->cp = cp;
    errtp->data.child.left = tp;

    return errtp;
}

/* Create an EX_TREE representing a literal value */

EX_TREE        *new_ex_lit(
    unsigned value)
{
    EX_TREE        *tp;

    tp = new_ex_tree(EX_LIT);
    tp->data.lit = value & 0xffff;  /* Force to 16-bits */

    return tp;
}

/* Create an EX_TREE representing a binary expression */

EX_TREE        *new_ex_bin(
    int type,
    EX_TREE *left,
    EX_TREE *right)
{
    EX_TREE        *tp;

    tp = new_ex_tree(type);
    tp->data.child.left = left;
    tp->data.child.right = right;

    tp->cp = right->cp;

    return tp;
}

/* Create an EX_TREE representing a unary expression */

EX_TREE        *new_ex_una(
    int type,
    EX_TREE *left)
{
    EX_TREE        *tp;

    tp = new_ex_tree(type);
    tp->data.child.left = left;
    tp->data.child.right = NULL;

    tp->cp = left->cp;

    return tp;
}

