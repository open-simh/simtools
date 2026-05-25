#ifndef EXTREE__H
#define EXTREE__H

#include "symbols.h"

typedef struct ex_tree {
    enum ex_type {
        EX_LIT = 1,
        /* Expression is a literal value */
        EX_SYM = 2,
        /* Expression has a symbol reference
         * (symbol from symbol table, so not freed) */
        EX_UNDEFINED_SYM = 3,
        /* Expression is undefined sym reference */
        EX_TEMP_SYM = 4,
        /* Expression is temp sym reference */

        EX_COM = 5,
        /* One's complement */
        EX_NEG = 6,
        /* Negate */
        EX_REG = 7,
        /* register value (%) */
        EX_ERR = 8,
        /* Expression with an error */

        EX_ADD = 9,
        /* Add */
        EX_SUB = 10,
        /* Subtract */
        EX_MUL = 11,
        /* Multiply */
        EX_DIV = 12,
        /* Divide */
        EX_AND = 13,
        /* bitwise and */
        EX_OR = 14,
        /* bitwise or */
        EX_LSH = 15,
        /* left shift */
    } type;

    char           *cp;         /* points to end of parsed expression */

    union {
        struct {
            struct ex_tree *left,
                           *right;      /* Left, right children */
        } child;
        unsigned        lit;    /* Literal value */
        SYMBOL         *symbol; /* Symbol reference */
    } data;
} EX_TREE;


EX_TREE        *new_ex_tree(
    int type);

void            free_tree(
    EX_TREE *tp);

EX_TREE        *new_ex_lit(
    unsigned value);

EX_TREE        *ex_err(
    EX_TREE *tp,
    char *cp);

EX_TREE        *new_ex_bin(
    int type,
    EX_TREE *left,
    EX_TREE *right);

EX_TREE        *new_ex_una(
    int type,
    EX_TREE *left);

int num_subtrees(
    EX_TREE *tp);

EX_TREE        *evaluate(
    EX_TREE *tp,
    int flags);

#define DISABLE_EXTREE_IF_DF_NDF        1  /* 0=extree may be used to evaluate definedness, 1=extree NEVER used */
                                           /* TODO: If we want to permanently disable this, we could remove the
                                            *       'flags' from parse_expr() and evaluate() etc. */

#if !DISABLE_EXTREE_IF_DF_NDF
#define EVALUATE_DEFINEDNESS            1  /* Flag used by parse_expr() to evaluate definedness */
#endif

#define EVALUATE_OUT_IS_REGISTER        1

#endif /* EXTREE__H */
