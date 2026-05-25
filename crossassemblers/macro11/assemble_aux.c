/*
 * Smaller operators for assemble
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "assemble_aux.h"  /* My own definitions */

#include "assemble.h"
#include "assemble_globals.h"
#include "listing.h"
#include "macros.h"
#include "parse.h"
#include "symbols.h"
#include "util.h"


/* Allocate a new section */

SECTION        *new_section(
    void)
{
    SECTION        *sect = memcheck(malloc(sizeof(SECTION)));

    sect->flags = 0;
    sect->size = 0;
    sect->pc = 0;
    sect->type = 0;
    sect->sector = 0;
    sect->label = NULL;
    return sect;
}


/* This is called by places that are about to store some code, or
   which want to manually update DOT. */

void change_dot(
    TEXT_RLD *tr,
    int size)
{
    if (size > 0) {
        if (last_dot_section != current_pc->section) {
            text_define_location(tr, current_pc->section->label, &current_pc->value);
            last_dot_section = current_pc->section;
            last_dot_addr = current_pc->value;
        }
        if (last_dot_addr != current_pc->value) {
            text_modify_location(tr, &current_pc->value);
            last_dot_addr = current_pc->value;
        }

        /* Update for next time */
        last_dot_addr += size;
    }

    if (DOT + size > current_pc->section->size)
        current_pc->section->size = DOT + size;
}


/* store_word stores a word to the object file and lists it to the
   listing file */

int store_word(
    STREAM *str,
    TEXT_RLD *tr,
    int size,
    unsigned word)
{
    if (size == 1)
        if((word & 0x8000) ? word < 0xff00 : word > 0xff)
            report_warn(str, "Truncated BYTE %o to %o\n", word, word & 0xff);
    change_dot(tr, size);
    list_word(str, DOT, word, size, "");
    return text_word(tr, &DOT, size, word);
}


/* store_displaced_word stores a word to the object file and lists it to the
   listing file */

static int store_displaced_word(
    STREAM *str,
    TEXT_RLD *tr,
    int size,
    unsigned word)
{
    change_dot(tr, size);
    list_word(str, DOT, word, size, "'");
    return text_displaced_word(tr, &DOT, size, word);
}

static int store_global_displaced_offset_word(
    STREAM *str,
    TEXT_RLD *tr,
    int size,
    unsigned word,
    char *global)
{
    change_dot(tr, size);
    list_word(str, DOT, word, size, "G");
    return text_global_displaced_offset_word(tr, &DOT, size, word, global);
}

static int store_global_offset_word(
    STREAM *str,
    TEXT_RLD *tr,
    int size,
    unsigned word,
    char *global)
{
    change_dot(tr, size);
    list_word(str, DOT, word, size, "G");
    return text_global_offset_word(tr, &DOT, size, word, global);
}

static int store_internal_word(
    STREAM *str,
    TEXT_RLD *tr,
    int size,
    unsigned word)
{
    change_dot(tr, size);
    list_word(str, DOT, word, size, "'");
    return text_internal_word(tr, &DOT, size, word);
}

static int store_psect_displaced_offset_word(
    STREAM *str,
    TEXT_RLD *tr,
    int size,
    unsigned word,
    char *name)
{
    change_dot(tr, size);
    list_word(str, DOT, word, size, "'");
    return text_psect_displaced_offset_word(tr, &DOT, size, word, name);
}

static int store_psect_offset_word(
    STREAM *str,
    TEXT_RLD *tr,
    int size,
    unsigned word,
    char *name)
{
    change_dot(tr, size);
    list_word(str, DOT, word, size, "'");
    return text_psect_offset_word(tr, &DOT, size, word, name);
}

int store_limits(
    STREAM *str,
    TEXT_RLD *tr)
{
    change_dot(tr, 4);
    list_word(str, DOT, 0, 2, "");
    list_word(str, DOT + 2, 0, 2, "");
    return text_limits(tr, &DOT);
}


/* free_addr_mode frees the storage consumed by an addr_mode */

void free_addr_mode(
    ADDR_MODE *mode)
{
    if (mode->offset)
        free_tree(mode->offset);
    mode->offset = NULL;
}


/* Get the register indicated by the expression */

unsigned get_register(
    EX_TREE *expr)
{
    unsigned        reg = 0xE;  /* Invalid register number */

    if (expr->type == EX_LIT)
        reg = expr->data.lit;
    else if (expr->type == EX_SYM && expr->data.symbol->section->type == SECTION_REGISTER)
        reg = expr->data.symbol->value;

    if (reg <= 7)
        return reg;

    return NO_REG;
}


/*
  implicit_gbl is a self-recursive routine that adds undefined symbols
  to the "implicit globals" symbol table, or alternatively adds the
  symbol as an UNDEFINED symbol.
*/

void implicit_gbl(
    EX_TREE *value)
{
    if (pass > PASS1 || !value)
        return;                        /* Only do this in first pass */

    switch (num_subtrees(value)) {
    case 0:
        switch (value->type) {
        case EX_UNDEFINED_SYM:
            {
                if (!(value->data.symbol->flags & SYMBOLFLAG_LOCAL) &&
                    !isdigit((unsigned char) value->data.symbol->label[0])) {
                    /* Unless it's a local symbol, */
                    if (ENABL(GBL)) {
                        /* either make the undefined symbol into an
                           implicit global */
                        add_sym(value->data.symbol->label, 0, SYMBOLFLAG_GLOBAL,
                                &absolute_section, &implicit_st, NULL);  /* TODO: Use abs_section_addr() ? */
                    } else {
                        /* or add it to the undefined symbol table,
                           purely for listing purposes.
                           It also works to add it to symbol_st,
                           all code is carefully made for that. */
#define ADD_UNDEFINED_SYMBOLS_TO_MAIN_SYMBOL_TABLE      0
#if ADD_UNDEFINED_SYMBOLS_TO_MAIN_SYMBOL_TABLE
                        add_sym(value->data.symbol->label, 0, SYMBOLFLAG_UNDEFINED,
                                &absolute_section, &symbol_st, stack);  /* TODO: Use abs_section_addr() ? */
#else
                        add_sym(value->data.symbol->label, 0, SYMBOLFLAG_UNDEFINED,
                                &absolute_section, &undefined_st, NULL);  /* TODO: Use abs_section_addr() ? */
#endif
                    }
                }
            }
            break;
        case EX_LIT:
        case EX_SYM:
        case EX_TEMP_SYM: // Impossible on this pass
            return;
        default:
            break;
        }
        break;
    case 2:
        implicit_gbl(value->data.child.right);
        /* FALLS THROUGH */
    case 1:
        implicit_gbl(value->data.child.left);
        break;
    }
}


/* Done between the first and second passes */
/* Migrates the symbols from the "implicit" table into the main table. */

void migrate_implicit(
    void)
{
    SYMBOL_ITER     iter;
    SYMBOL         *isym,
                   *sym;

    for (isym = first_sym(&implicit_st, &iter); isym != NULL; isym = next_sym(&implicit_st, &iter)) {
        sym = lookup_sym(isym->label, &symbol_st);
        if (sym) {
            continue;                  // It's already in there.  Great.
        }

        if (isym->flags & SYMBOLFLAG_LOCAL)
            continue;                  /* Do not attempt to migrate local symbols */
                                       /* These are noticed on pass 1 but they will ...
                                        * ... be reported as invalid expressions later */

        isym->flags |= SYMBOLFLAG_IMPLICIT_GLOBAL;
        sym = add_sym(isym->label, isym->value, isym->flags, isym->section,
                      &symbol_st, NULL);
        // Just one other thing - migrate the stmtno
        sym->stmtno = isym->stmtno;
    }
}


/* Done between second pass and listing */
/* Migrates the symbols from the "undefined" table into the main table. */

void migrate_undefined(
    void)
{
    SYMBOL_ITER     iter;
    SYMBOL         *isym,
                   *sym;

    for (isym = first_sym(&undefined_st, &iter); isym != NULL; isym = next_sym(&undefined_st, &iter)) {
        sym = lookup_sym(isym->label, &symbol_st);
        if (sym) {
            continue;                  /* It's already in there.  Great. */
        }
        isym->flags |= SYMBOLFLAG_UNDEFINED; /* Just in case */
        sym = add_sym(isym->label, isym->value, isym->flags, isym->section,
                      &symbol_st, NULL);
        /* Just one other thing - migrate the stmtno */
        sym->stmtno = isym->stmtno;
    }
}

int express_sym_offset(
    EX_TREE *value,
    SYMBOL **sym,
    unsigned *offset)
{
    implicit_gbl(value);               /* Translate tree's undefined syms
                                          into global syms */

    /* Internally relocatable symbols will have been summed down into
       EX_TEMP_SYM's. */

    if (value->type == EX_SYM || value->type == EX_TEMP_SYM) {
        *sym = value->data.symbol;
        *offset = 0;
        return 1;
    }

    /* What remains is external symbols. */

    if (value->type == EX_ADD) {
        EX_TREE        *left = value->data.child.left;
        EX_TREE        *right = value->data.child.right;

        if ((left->type != EX_SYM && left->type != EX_UNDEFINED_SYM) || right->type != EX_LIT)
            return 0;                  /* Failed. */
        *sym = left->data.symbol;
        *offset = right->data.lit;
        return 1;
    }

    if (value->type == EX_SUB) {
        EX_TREE        *left = value->data.child.left;
        EX_TREE        *right = value->data.child.right;

        if ((left->type != EX_SYM && left->type != EX_UNDEFINED_SYM) || right->type != EX_LIT)
            return 0;                  /* Failed. */
        *sym = left->data.symbol;
        *offset = (unsigned) -(int) (right->data.lit);
        return 1;
    }

    return 0;
}


/*
  Translate an EX_TREE into a TEXT_COMPLEX suitable for encoding
  into the object file. */

int complex_tree(
    TEXT_COMPLEX *tx,
    EX_TREE *tree)
{
    switch (tree->type) {
    case EX_LIT:
        text_complex_lit(tx, tree->data.lit);
        return 1;

    case EX_TEMP_SYM:
    case EX_SYM:
        {
            SYMBOL         *sym = tree->data.symbol;

            /* This check may not be needed; so far it made no difference. */
            if (sym->flags & SYMBOLFLAG_UNDEFINED) {
                return 0;
            }

            if (SYM_IS_IMPORTED(sym)) {
                text_complex_global(tx, sym->label);
            } else {
                text_complex_psect(tx, sym->section->sector, sym->value);
            }
        }
        return 1;

    case EX_COM:
        if (!complex_tree(tx, tree->data.child.left))
            return 0;
        text_complex_com(tx);
        return 1;

    case EX_NEG:
        if (!complex_tree(tx, tree->data.child.left))
            return 0;
        text_complex_neg(tx);
        return 1;

    case EX_ADD:
        if (!complex_tree(tx, tree->data.child.left))
            return 0;
        if (!complex_tree(tx, tree->data.child.right))
            return 0;
        text_complex_add(tx);
        return 1;

    case EX_SUB:
        if (!complex_tree(tx, tree->data.child.left))
            return 0;
        if (!complex_tree(tx, tree->data.child.right))
            return 0;
        text_complex_sub(tx);
        return 1;

    case EX_MUL:
        if (!complex_tree(tx, tree->data.child.left))
            return 0;
        if (!complex_tree(tx, tree->data.child.right))
            return 0;
        text_complex_mul(tx);
        return 1;

    case EX_DIV:
        if (!complex_tree(tx, tree->data.child.left))
            return 0;
        if (!complex_tree(tx, tree->data.child.right))
            return 0;
        text_complex_div(tx);
        return 1;

    case EX_AND:
        if (!complex_tree(tx, tree->data.child.left))
            return 0;
        if (!complex_tree(tx, tree->data.child.right))
            return 0;
        text_complex_and(tx);
        return 1;

    case EX_OR:
        if (!complex_tree(tx, tree->data.child.left))
            return 0;
        if (!complex_tree(tx, tree->data.child.right))
            return 0;
        text_complex_or(tx);
        return 1;

    default:
        return 0;
    }
}


/* store a word which is represented by a complex expression. */

static void store_complex(
    STREAM *refstr,
    TEXT_RLD *tr,
    int size,
    EX_TREE *value)
{
    TEXT_COMPLEX    tx;

    change_dot(tr, size);              /* About to store - update DOT */

    implicit_gbl(value);               /* Turn undefined symbols into globals */

    text_complex_begin(&tx);           /* Open complex expression */

    if (!complex_tree(&tx, value)) {   /* Translate */
        report_err(refstr, "Invalid expression (complex relocation)\n");
        store_word(refstr, tr, size, 0);
    } else {
        list_word(refstr, DOT, 0, size, "C");
        text_complex_commit(tr, &DOT, size, &tx, 0);
    }
}


/* store_complex_displaced is the same as store_complex but uses the
   "displaced" RLD code */

static void store_complex_displaced(
    STREAM *refstr,
    TEXT_RLD *tr,
    int size,
    EX_TREE *value)
{
    TEXT_COMPLEX    tx;

    change_dot(tr, size);

    implicit_gbl(value);               /* Turn undefined symbols into globals */

    text_complex_begin(&tx);

    if (!complex_tree(&tx, value)) {
        report_err(refstr, "Invalid expression (complex displaced relocation)\n");
        store_word(refstr, tr, size, 0);
    } else {
        list_word(refstr, DOT, 0, size, "C");
        text_complex_commit_displaced(tr, &DOT, size, &tx, 0);
    }
}


/*
  mode_extension - writes the extension word required by an addressing
  mode */

void mode_extension(
    TEXT_RLD *tr,
    ADDR_MODE *mode,
    STREAM *str)
{
    EX_TREE        *value = mode->offset;
    SYMBOL         *sym;
    unsigned        offset;

    /* Also frees the mode. */

    if (value == NULL) {
        free_addr_mode(mode);
        return;
    }

    if (value->type == EX_LIT) {
        if (mode->pcrel) {            /* PC-relative? */
            if (current_pc->section->flags & PSECT_REL) {
                store_displaced_word(str, tr, 2, value->data.lit);
            } else {
                /* I can compute this myself. */
                store_word(str, tr, 2, value->data.lit - DOT - 2);
            }
        } else {
            store_word(str, tr, 2, value->data.lit);    /* Just a
                                                           known
                                                           value. */
        }
    } else if (express_sym_offset(value, &sym, &offset)) {
        if (SYM_IS_IMPORTED(sym)) {
            /* Reference to a global symbol. */
            /* Global symbol plus offset */
            if (mode->pcrel)
                store_global_displaced_offset_word(str, tr, 2, offset, sym->label);
            else
                store_global_offset_word(str, tr, 2, offset, sym->label);
        } else if (sym->section->type == SECTION_REGISTER) {
            /* Delayed action: evaluate() excludes SECTION_REGISTER when
             * turning symbols into EX_LIT. Do it here now. */
            store_word(str, tr, 2, sym->value + offset);
        } else {
            /* Relative to non-external symbol. */
            if (current_pc->section == sym->section) {
                /* In the same section */
                if (mode->pcrel) {
                    /* I can compute this myself. */
                    store_word(str, tr, 2, sym->value + offset - DOT - 2);
                } else
                    store_internal_word(str, tr, 2, sym->value + offset);
            } else {
                /* In a different section */
                if (mode->pcrel)
                    store_psect_displaced_offset_word(str, tr, 2, sym->value + offset, sym->section->label);
                else
                    store_psect_offset_word(str, tr, 2, sym->value + offset, sym->section->label);
            }
        }
    } else {
        /* Complex relocation */

        if (mode->pcrel)
            store_complex_displaced(str, tr, 2, mode->offset);
        else
            store_complex(str, tr, 2, mode->offset);
    }

    free_addr_mode(mode);
}


/* eval_defined - take an EX_TREE and returns TRUE if the tree
   represents "defined" symbols. */

int eval_defined(
    EX_TREE *value)
{
    switch (value->type) {
    case EX_LIT:
        return 1;
    case EX_SYM:
        return 1;
    case EX_UNDEFINED_SYM:
        return 0;
    case EX_AND:
        return eval_defined(value->data.child.left) && eval_defined(value->data.child.right);
    case EX_OR:
        return eval_defined(value->data.child.left) || eval_defined(value->data.child.right);
    default:
        return 0;
    }
}


/* eval_undefined - take an EX_TREE and returns TRUE if it represents
   "undefined" symbols. */

int eval_undefined(
    EX_TREE *value)
{
    switch (value->type) {
    case EX_UNDEFINED_SYM:
        return 1;
    case EX_SYM:
        return 0;
    case EX_AND:
        return eval_undefined(value->data.child.left) && eval_undefined(value->data.child.right);
    case EX_OR:
        return eval_undefined(value->data.child.left) || eval_undefined(value->data.child.right);
    default:
        return 0;
    }
}


/* push_cond - a new conditional (.IF) block has been activated.  Push
   its context. */

void push_cond(
    int ok,
    STREAM *str)
{
    last_cond++;
    assert(last_cond < MAX_CONDS);
    conds[last_cond].ok = ok;
    conds[last_cond].file = memcheck(strdup(str->name));
    conds[last_cond].line = str->line;
}


/*
  pop_cond - pop stacked conditionals. */

void pop_cond(
    int to)
{
    while (last_cond > to) {
        free(conds[last_cond].file);
        last_cond--;
    }
}


/* go_section - sets current_pc to a new program section */

void go_section(
    TEXT_RLD *tr,
    SECTION *sect)
{
    (void)tr;

    if (current_pc->section == sect)
        return;                        /* This is too easy */

    /* save current PC value for old section */
    current_pc->section->pc = DOT;

    /* Set current section and PC value */
    current_pc->section = sect;
    DOT = sect->pc;
}


/*
  store_value - used to store a value represented by an expression
  tree into the object file.  Used by do_word and .ASCII/.ASCIZ.
*/

void store_value(
    STACK *stack,
    TEXT_RLD *tr,
    int size,
    EX_TREE *value)
{
    SYMBOL         *sym;
    unsigned        offset;

    implicit_gbl(value);               /* turn undefined symbols into globals */

    if (value->type == EX_LIT) {
        store_word(stack->top, tr, size, value->data.lit);
    } else if (!express_sym_offset(value, &sym, &offset)) {
        store_complex(stack->top, tr, size, value);
    } else {
        if (SYM_IS_IMPORTED(sym)) {
            store_global_offset_word(stack->top, tr, size, sym->value + offset, sym->label);
        } else if (sym->section->type == SECTION_REGISTER) {
            /* Delayed action: evaluate() excludes SECTION_REGISTER when
             * turning symbols into EX_LIT. Do it here now. */
            store_word(stack->top, tr, size, sym->value + offset);
        } else if (sym->section != current_pc->section) {
            store_psect_offset_word(stack->top, tr, size, sym->value + offset, sym->section->label);
        } else {
            store_internal_word(stack->top, tr, size, sym->value + offset);
        }
    }
}


/* do_word - used by .WORD, .BYTE, and implied .WORD. */

int do_word(
    STACK *stack,
    TEXT_RLD *tr,
    char *cp,
    int size)
{
    int comma;

    if (size == 2 && (DOT & 1)) {
        report_warn(stack->top, ".WORD on odd boundary [.EVEN implied]\n");
        store_word(stack->top, tr, 1, 0);       /* Align it */
    }

    cp = skipwhite(cp);

    do {
        if (cp[0] == ',') {
            /* Empty expressions count as 0 */
            store_word(stack->top, tr, size, 0);
        } else {
            EX_TREE        *value = parse_expr(cp, 0);

            if (value->type != EX_ERR && value->cp > cp) {
                store_value(stack, tr, size, value);
                cp = value->cp;
            } else {
                char *byteword[] = { "BYTE", "WORD" };

                if (value->type == EX_ERR &&
                    value->cp != NULL &&
                    value->data.child.right == NULL &&
                    value->data.child.left != NULL &&
                    value->data.child.left->type == EX_LIT) {
                    report_warn(stack->top, "Invalid expression stored in .%s\n", byteword[size - 1]);

                    store_value(stack, tr, size, value->data.child.left);
                    cp = value->cp;
                } else {
                    report_err(stack->top, "Invalid expression in .%s\n", byteword[size - 1]);
                    cp = "";                /* force loop to end */
                }
            }

            free_tree(value);
        }
    } while (cp = skipdelim_comma(cp, &comma), !EOL(*cp));

    if (comma) {
        /* Trailing empty expressions count as 0 */
        store_word(stack->top, tr, size, 0);
    }

    return 1;
}


/*
  check_branch - check branch distance.
*/

int check_branch(
    STACK *stack,
    unsigned offset,
    int min,
    int max)
{
    int             s_offset;

    if (offset & 1) {
        report_err(stack->top, "Bad branch target (odd address)\n");
    }

    /* Sign-extend */
    if (offset & 0100000)
        s_offset = offset | ~0177777;
    else
        s_offset = offset & 077777;
    if (s_offset > max || s_offset < min) {
        char            temp[16];

        /* printf can't do signed octal. */
        my_ltoa(s_offset, temp, 8);
        report_err(stack->top, "Branch target out of range (distance=%s)\n", temp);
        return 0;
    }
    return 1;
}


/* write_psect_globals writes out the psect header and globals for each psect */
/* if gsd == NULL we only test the globals are unique and update the psect info */

void write_psect_globals(
    GSD *gsd)
{
    SYMBOL         *sym;
    SECTION        *psect;
    SYMBOL_ITER     sym_iter;
    int             isect;
    unsigned        nsyms = 0;
    SYMBOL        **symbols = NULL;

    /* TODO: Warnings if global symbols or PSECTs have '_' in them (?) */

    /* Count the global symbols in the table */
    for (sym = first_sym(&symbol_st, &sym_iter); sym != NULL; sym = next_sym(&symbol_st, &sym_iter))
        if (sym->flags & SYMBOLFLAG_GLOBAL)
            nsyms++;

    /* Sort them by name */
    if (nsyms) {
        SYMBOL      **symbolp = symbols = memcheck(malloc(nsyms * sizeof (SYMBOL *)));

        for (sym = first_sym(&symbol_st, &sym_iter); sym != NULL; sym = next_sym(&symbol_st, &sym_iter))
            if (sym->flags & SYMBOLFLAG_GLOBAL)
                *symbolp++ = sym;

        qsort(symbols, nsyms, sizeof(SYMBOL *), symbol_compar);

        if (nsyms > 1) {
            unsigned      i = 0,
                          j = 0;

            while (++j < nsyms) {
                if (symbols[i] && strncmp(symbols[i]->label, symbols[j]->label, 6) == 0) {
                    if (strncmp(symbols[i]->label, symbols[j]->label, 6) == 0) {
                        report_fatal(NULL, "Global symbol %s (in %s) causes %s (in %s) to be ignored\n",
                            symbols[i]->label,
                                (symbols[i]->section->label[0] == '\0') ? ". BLK." : symbols[i]->section->label,
                            symbols[j]->label,
                                (symbols[j]->section->label[0] == '\0') ? ". BLK." : symbols[j]->section->label);

                        symbols[j] = NULL;
                   }
               } else {
                   i = j;
               }
            }
        }
    }

    /* write out each PSECT with its global stuff */
    /* Sections must be written out in the order that they
     * appear in the assembly file.  */
    for (isect = 0; isect < sector; isect++) {
        psect = sections[isect];

        if (gsd)
            gsd_psect(gsd, psect->label, psect->flags, psect->size);
        psect->sector = isect;         /* Assign it a sector */
        psect->pc = 0;                 /* Reset its PC for second pass */

        if (gsd && nsyms) {
            unsigned      i;

            for (i = 0; i < nsyms; i++) {
                if (symbols[i] && symbols[i]->section == psect) {
                    gsd_global(gsd, symbols[i]->label,
                               ((symbols[i]->flags & SYMBOLFLAG_DEFINITION) ? GLOBAL_DEF  : 0) |
                               ((symbols[i]->flags & SYMBOLFLAG_WEAK)       ? GLOBAL_WEAK : 0) |
                               ((symbols[i]->section->flags & PSECT_REL)    ? GLOBAL_REL  : 0) |
                               0100,
                               /* Looks undefined, but add it in anyway */
                               symbols[i]->value);
                }
            }
        }
    }

    if (nsyms)
        free(symbols);
}


/* write_globals writes out the GSD prior to the second assembly pass */

void write_globals(
    FILE *obj)
{
    GSD             gsd;

    if (obj == NULL) {
        write_psect_globals(NULL);  /* Test the globals and fix up the psect if we don't have an object file */
        return;
    }

    gsd_init(&gsd, obj);

    gsd_mod(&gsd, module_name);

    if (ident)
        gsd_ident(&gsd, ident);

    write_psect_globals(&gsd);

    /* Finally write out the transfer address */
    if (xfer_address == NULL) {
        gsd_xfer(&gsd, ". ABS.", 1);
    } else {
        if (xfer_address->type == EX_LIT) {
            gsd_xfer(&gsd, ". ABS.", xfer_address->data.lit);
        } else {
            SYMBOL         *lsym;
            unsigned        offset;

            if (!express_sym_offset(xfer_address, &lsym, &offset)) {
                report_err(NULL, "Invalid program transfer address" /* " [set to 1]" */ "\n");
            /*  gsd_xfer(&gsd, ". ABS.", 1);  */
            } else {
                gsd_xfer(&gsd, lsym->section->label, lsym->value + offset);
            }
        }
    }

    gsd_flush(&gsd);
    gsd_end(&gsd);
}
