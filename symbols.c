#define SYMBOLS__C

/*
 * Symbol-related code.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "symbols.h"                   /* My own definitions */

#include "assemble_globals.h"
#include "listing.h"
#include "object.h"
#include "util.h"


/* GLOBALS */
int             symbol_len = SYMMAX_DEFAULT;    /* max. len of symbols. default = 6 */
int             symbol_allow_underscores = 0;   /* allow "_" in symbol names */
int             symbol_list_locals = 0;         /* list local symbols in the symbol table */
int             looked_up_local_sym;            /* TRUE if the current source line looked up a local symbol */

DIRARG          enabl_list_arg[ARGS__LAST];     /* .ENABL/.DSABL and .LIST/.NLIST arguments */

SYMBOL         *reg_sym[9];     /* Keep the register symbols in a handy array */

SYMBOL_TABLE    system_st;      /* System symbols (Instructions,
                                   pseudo-ops, registers) */

SYMBOL_TABLE    section_st;     /* Program sections */

SYMBOL_TABLE    symbol_st;      /* User symbols */

SYMBOL_TABLE    macro_st;       /* Macros */

SYMBOL_TABLE    implicit_st;    /* The symbols which may be implicit globals */

SYMBOL_TABLE    undefined_st;   /* The symbols which may be undefined */


void list_section(SECTION *sec);


void dump_sym(
    SYMBOL *sym)
{
    /* TODO: Replace THIS report_warn() with something like report_info() */
    report_warn(NULL, "'%s': %06o, stmt %d, flags %o:%s%s%s%s%s%s%s\n",
                sym->label,
                sym->value,
                sym->stmtno,
                sym->flags,
                ((sym->flags & SYMBOLFLAG_PERMANENT)? " PERMANENT" : ""),
                ((sym->flags & SYMBOLFLAG_GLOBAL)? " GLOBAL" : ""),
                ((sym->flags & SYMBOLFLAG_WEAK)? " WEAK" : ""),
                ((sym->flags & SYMBOLFLAG_DEFINITION)? " DEFINITION" : ""),
                ((sym->flags & SYMBOLFLAG_UNDEFINED)? " UNDEFINED" : ""),
                ((sym->flags & SYMBOLFLAG_LOCAL)? " LOCAL" : ""),
                ((sym->flags & SYMBOLFLAG_IMPLICIT_GLOBAL)? " IMPLICIT_GLOBAL" : ""));
}


void check_sym_invariants(
    SYMBOL *sym,
    char *file,
    int line,
    STREAM *stream)
{
    int dump = 0;

    if (sym->section == &instruction_section) {
        /* The instructions use the flags field differently */
        if ((sym->flags & ~OC_MASK) != 0) {
            report_fatal(stream, "%s %d: Instruction symbol %s has wrong flags\n", file, line, sym->label);
            dump_sym(sym);
        }
        return;
    }

    /*
     * A symbol is GLOBAL if it appears in the object file's symbol table.
     * It can be either exported (if defined) or imported (if not defined).
     *
     * A common test like this
     *
     *  if ((sym->flags & (SYMBOLFLAG_GLOBAL | SYMBOLFLAG_DEFINITION)) == SYMBOLFLAG_GLOBAL)
     *
     * tests if a symbol is imported.
     */

    switch (sym->flags & (SYMBOLFLAG_PERMANENT|SYMBOLFLAG_GLOBAL|SYMBOLFLAG_DEFINITION|SYMBOLFLAG_UNDEFINED)) {
        /* A DEFINITION can independently be PERMANENT and/or GLOBAL */
        case SYMBOLFLAG_PERMANENT|SYMBOLFLAG_GLOBAL|SYMBOLFLAG_DEFINITION:
        case                      SYMBOLFLAG_GLOBAL|SYMBOLFLAG_DEFINITION:
        case SYMBOLFLAG_PERMANENT|                  SYMBOLFLAG_DEFINITION:
        case                                        SYMBOLFLAG_DEFINITION:
            break;
        /* A GLOBAL can also be undefined, but then it's still usable */
        case                      SYMBOLFLAG_GLOBAL:
            break;
        /* A truly UNDEFINED symbol is an error to use */
        /* (this seems logically equivalent to all of these flags cleared) */
        case SYMBOLFLAG_UNDEFINED:
            break;
        default:
            report_fatal(stream, "%s %d: Symbol '%s' in section '%s' definedness is inconsistent\n",
                         file, line, sym->label, sym->section->label);
            dump++;
    }

    if ( (sym->flags & SYMBOLFLAG_IMPLICIT_GLOBAL) &&
        !(sym->flags & SYMBOLFLAG_GLOBAL)) {
        report_fatal(stream, "%s %d: Symbol '%s' globalness is inconsistent\n", file, line, sym->label);
        dump++;
    }

    if ( (sym->flags & SYMBOLFLAG_LOCAL) &&
         (sym->flags & SYMBOLFLAG_GLOBAL)) {
        report_fatal(stream, "%s %d: Symbol '%s' is local and global\n", file, line, sym->label);
        dump++;
    }

    if ( (sym->flags & SYMBOLFLAG_PERMANENT) &&
        !(sym->flags & SYMBOLFLAG_DEFINITION)) {
        report_fatal(stream, "%s %d: Symbol '%s' is permanent without definition\n", file, line, sym->label);
        dump++;
    }

    if ( (sym->flags & SYMBOLFLAG_WEAK) &&
        !(sym->flags & SYMBOLFLAG_GLOBAL)) {
        report_fatal(stream, "%s %d: Symbol '%s' weak/global is inconsistent\n", file, line, sym->label);
        dump++;
    }

    if (sym->value > 0xffff) {
            report_fatal(stream, "%s %d: Symbol '%s' [%o] takes more than 16-bits\n",
                         file, line, sym->label, sym->value);
            dump++;
    }

    if (enabl_debug && dump) {
        dump_sym(sym);
    }
}


/* hash_name hashes a name into a value from 0-HASH_SIZE */

int hash_name(
    char *label)
{
    unsigned        accum = 0;

    while (*label)
        accum = (accum << 1) ^ *label++;

    accum %= HASH_SIZE;

    return accum;
}


/* Diagnostic: symflags returns a char* which gives flags I can use to
   show the context of a symbol. */

char           *symflags(
    SYMBOL *sym)
{
    static char     temp[8];
    char           *fp = temp;

    if (sym->flags & SYMBOLFLAG_PERMANENT)
        *fp++ = 'P';
    if (sym->flags & SYMBOLFLAG_GLOBAL)
        *fp++ = 'G';
    if (sym->flags & SYMBOLFLAG_WEAK)
        *fp++ = 'W';
    if (sym->flags & SYMBOLFLAG_DEFINITION)
        *fp++ = 'D';
    if (sym->flags & SYMBOLFLAG_UNDEFINED)
        *fp++ = 'U';
    if (sym->flags & SYMBOLFLAG_LOCAL)
        *fp++ = 'L';
    if (sym->flags & SYMBOLFLAG_IMPLICIT_GLOBAL)
        *fp++ = 'I';

    *fp = '\0';
    return temp;
}


/* Allocate a new symbol.  Does not add it to any symbol table. */

static SYMBOL  *new_sym(
    char *label)
{
    SYMBOL         *sym = memcheck(malloc(sizeof(SYMBOL)));

    sym->label = memcheck(strdup(label));
    sym->section = NULL;
    sym->value = 0;
    sym->flags = 0;
    return sym;
}


/* Free a symbol. Does not remove it from any symbol table.  */

void free_sym(
    SYMBOL *sym)
{
    if (sym->label) {
        check_sym_invariants(sym, __FILE__, __LINE__, NULL);
        free(sym->label);
        sym->label = NULL;
    }
    free(sym);
}


/* remove_sym removes a symbol from its symbol table. */

void remove_sym(
    SYMBOL *sym,
    SYMBOL_TABLE *table)
{
    SYMBOL        **prevp,
                   *symp;
    int             hash;

    check_sym_invariants(sym, __FILE__, __LINE__, NULL);
    hash = hash_name(sym->label);
    prevp = &table->hash[hash];
    while (symp = *prevp, symp != NULL && symp != sym)
        prevp = &symp->next;

    if (symp)
        *prevp = sym->next;
}


/* lookup_sym finds a symbol in a table */

SYMBOL         *lookup_sym(
    char *label,
    SYMBOL_TABLE *table)
{
    unsigned        hash;
    SYMBOL         *sym;

    hash = hash_name(label);

    sym = table->hash[hash];
    while (sym && strcmp(sym->label, label) != 0)
        sym = sym->next;

    if (sym) {
        check_sym_invariants(sym, __FILE__, __LINE__, NULL);
        if (sym->flags & SYMBOLFLAG_LOCAL)
            looked_up_local_sym = TRUE;
    }
    return sym;
}


/* next_sym - returns the next symbol from a symbol table.  Must be
   preceeded by first_sym.  Returns NULL after the last symbol. */

SYMBOL         *next_sym(
    SYMBOL_TABLE *table,
    SYMBOL_ITER *iter)
{
    if (iter->current)
        iter->current = iter->current->next;

    while (iter->current == NULL) {
        if (iter->subscript >= HASH_SIZE)
            return NULL;               /* No more symbols. */
        iter->current = table->hash[iter->subscript];
        iter->subscript++;
    }

    return iter->current;              /* Got a symbol. */
}


/* first_sym - returns the first symbol from a symbol table. Symbols
   are stored in random order. */

SYMBOL         *first_sym(
    SYMBOL_TABLE *table,
    SYMBOL_ITER *iter)
{
    iter->subscript = 0;
    iter->current = NULL;
    return next_sym(table, iter);
}


/* add_table - add a symbol to a symbol table. */

void add_table(
    SYMBOL *sym,
    SYMBOL_TABLE *table)
{
    int             hash = hash_name(sym->label);

    sym->next = table->hash[hash];
    table->hash[hash] = sym;
    check_sym_invariants(sym, __FILE__, __LINE__, NULL);
}


/* add_sym - used throughout to add or update symbols in a symbol table. */

SYMBOL         *add_sym(
    char *labelraw,
    unsigned value,
    unsigned flags,
    SECTION *section,
    SYMBOL_TABLE *table,
    STREAM *stream)
{
    SYMBOL         *sym;
    char            label[SYMMAX_MAX + 1];
    int             is_local = isdigit((unsigned char)labelraw[0]);

    if (is_local) {
        // Don't truncate local labels
        strncpy(label, labelraw, SYMMAX_MAX);
        label[SYMMAX_MAX] = 0;
    } else {
        //JH: truncate symbol to SYMMAX
        strncpy(label, labelraw, symbol_len);
        label[symbol_len] = 0;
    }

    /* For directives with more than 6 characters ...
     * ... and -ysl >6 and not -stringent ...
     * ... add_sym() all of the shorter names ...
     * ... e.g. .INCLU & .INCLUD as well as .INCLUDE
     */

    if (section == &pseudo_section) {
        if (!STRINGENT && symbol_len > 6) {
            if (strlen(label) > 6) {
               label[strlen(label)-1] = '\0';
               if (add_sym(label, value, flags, section, table, stream) == NULL)
                   return NULL;  /* Should never happen */
                strncpy(label, labelraw, symbol_len);
                label[symbol_len] = '\0';
            }
        }
    }

    sym = lookup_sym(label, table);
    if (sym == NULL) {
        if (is_local && !(flags & SYMBOLFLAG_LOCAL)) {
            /* This would be an internal error and should never happen */
            report_warn(stream, "Symbol '%s' is local but not defined LOCAL\n", label);
            dump_sym(sym);
            flags |=  SYMBOLFLAG_LOCAL;   /* Force this to become a local symbol */
        }
    } else {
        // A symbol registered as "undefined" can be changed.
        //
        check_sym_invariants(sym, __FILE__, __LINE__, stream);

        if (sym->flags & SYMBOLFLAG_PERMANENT) {
            if (flags & SYMBOLFLAG_PERMANENT) {
                if (sym->value != value || sym->section != section) {
                    report_fatal(stream, "Phase error: '%s'\n", label);
                    return NULL;
                }
            } else {
                report_warn(stream, "Redefining permanent symbol '%s'\n", label);
                return NULL;
            }
        }

        if ((sym->flags & SYMBOLFLAG_UNDEFINED) && !(flags & SYMBOLFLAG_UNDEFINED)) {
            sym->flags &= ~(SYMBOLFLAG_PERMANENT | SYMBOLFLAG_UNDEFINED);
        }
        else if (!(sym->flags & SYMBOLFLAG_UNDEFINED) && (flags & SYMBOLFLAG_UNDEFINED)) {
            report_warn(stream, /* "INTERNAL ERROR: " */ "Turning defined symbol '%s' into undefined\n", label);
            return sym;
        }
        /* Check for compatible definition */
        else if (sym->section == section && sym->value == value) {
            sym->flags |= flags;       /* Merge flags quietly */
            check_sym_invariants(sym, __FILE__, __LINE__, stream);
            return sym;                /* 's okay */
        }

        if (!(sym->flags & SYMBOLFLAG_PERMANENT)) {
            /* permit redefinition */
            sym->value = value;
            sym->flags |= flags;
            sym->section = section;
            check_sym_invariants(sym, __FILE__, __LINE__, stream);
            return sym;
        }

        report_fatal(stream, "INTERNAL ERROR: Bad symbol '%s' redefinition\n", label);
        return NULL;                   /* Bad symbol redefinition */
    }

    sym = new_sym(label);
    sym->flags = flags;
    sym->stmtno = stmtno;
    sym->section = section;
    sym->value = value;

    add_table(sym, table);

    return sym;
}


/* add_dirarg adds a directive argument for .ENABL/.DSABL and .LIST/.NLIST */

static void add_dirarg(
    DIRARG     *arg,
    const char *name,
    int         def,
    unsigned    flags)
{
    arg->name[0] = name[0];
    arg->name[1] = name[1];
    arg->name[2] = name[2];  /* There are always at least two characters in the arg name */
    arg->name[3] = '\0';
    arg->curval  = -1;       /* Filled in by load_dirargs() */
    arg->defval  = def;
    arg->flags   = flags;
}


/* add_dirargs adds all directive arguments  */

void add_dirargs(
    void)
{

#define ADD_ENABL_ARG(arg, def, flags) add_dirarg(&enabl_list_arg[E_##arg], #arg, def, ARGS_DSABL_ENABL | (flags))
#define  ADD_LIST_ARG(arg, def, flags) add_dirarg(&enabl_list_arg[L_##arg], #arg, def, ARGS_NLIST_LIST  | (flags))

    /* TODO: Provide support for .DSABL PNC and .DSABL REG */

    ADD_ENABL_ARG(ABS, ARGS_DSABL, ARGS_NOT_IMPLEMENTED);  /* Output in .LDA format */
    ADD_ENABL_ARG(AMA, ARGS_DSABL, ARGS_NO_FLAGS       );  /* Convert mode 67 (@ADDR) to 37 (@#ADDR) */
    ADD_ENABL_ARG(BMK, ARGS_DSABL, ARGS_HIDDEN         );  /* MACRO regression benchmark [undocumented] */
    ADD_ENABL_ARG(CDR, ARGS_DSABL, ARGS_NO_FLAGS       );  /* Card-reader format (ignore columns 73 onwards) */
    ADD_ENABL_ARG(CRF, ARGS_ENABL, ARGS_NO_FUNCTION    );  /* Cross-reference output */
    ADD_ENABL_ARG(DBG, ARGS_DSABL, ARGS_NOT_IMPLEMENTED);  /* ISD Record support (DEBUG-16 & MpPASCAL) [undocumented] */
    ADD_ENABL_ARG(FPT, ARGS_DSABL, ARGS_NO_FLAGS       );  /* Floating-point truncation */
    ADD_ENABL_ARG(GBL, ARGS_ENABL, ARGS_NO_FLAGS       );  /* Undefined symbols are allowed and are global */
    ADD_ENABL_ARG(LC,  ARGS_ENABL, ARGS_NO_FLAGS       );  /* Lower-case */
    ADD_ENABL_ARG(LCM, ARGS_DSABL, ARGS_NO_FLAGS       );  /* .IF IDN/DIF are case-sensitive */
    ADD_ENABL_ARG(LSB, ARGS_DSABL, ARGS_NO_FLAGS       );  /* Start a new local-symbol block */
    ADD_ENABL_ARG(MCL, ARGS_DSABL, ARGS_NO_FLAGS       );  /* Lookup undefined op-codes in macro libraries */
    ADD_ENABL_ARG(PIC, ARGS_DSABL, ARGS_NOT_SUPPORTED  );  /* Generate PIC code -- m11 extension */
    ADD_ENABL_ARG(PNC, ARGS_ENABL, ARGS_NOT_IMPLEMENTED);  /* Generate binary code */
    ADD_ENABL_ARG(REG, ARGS_ENABL, ARGS_NOT_IMPLEMENTED);  /* Use default register definitions */

    ADD_LIST_ARG(LIS,          0,  ARGS_NO_FLAGS       );  /* Listing level */
    ADD_LIST_ARG(BEX,  ARGS_LIST,  ARGS_NO_FLAGS       );  /* Binary values which don't fit on the listing line */
    ADD_LIST_ARG(BIN,  ARGS_LIST,  ARGS_NO_FLAGS       );  /* Binary code -- SEQ,LOC,BIN,SRC = whole line */
    ADD_LIST_ARG(CND,  ARGS_LIST,  ARGS_NO_FLAGS       );  /* Unsatisfied conditional coding */
    ADD_LIST_ARG(COM,  ARGS_LIST,  ARGS_NO_FLAGS       );  /* Comments -- subset of SRC */
    ADD_LIST_ARG(HEX,  ARGS_NLIST, ARGS_NO_FLAGS       );  /* Use HEX radix */
    ADD_LIST_ARG(LD,   ARGS_NLIST, ARGS_HIDDEN         );  /* Seems to only be a flag (but not used) [undocumented] */
    ADD_LIST_ARG(LOC,  ARGS_LIST,  ARGS_NO_FLAGS       );  /* Location counter -- SEQ,LOC,BIN,SRC = whole line */
    ADD_LIST_ARG(MC,   ARGS_LIST,  ARGS_NO_FLAGS       );  /* Macro calls and repeat range expansions */
    ADD_LIST_ARG(MD,   ARGS_LIST,  ARGS_NO_FLAGS       );  /* Macro definitions and repeat range [expansions] */
    ADD_LIST_ARG(ME,   ARGS_NLIST, ARGS_NO_FLAGS       );  /* Macro expansions */
    ADD_LIST_ARG(MEB,  ARGS_NLIST, ARGS_NO_FLAGS       );  /* Macro expansion binary code -- subset of ME */
    ADD_LIST_ARG(SEQ,  ARGS_LIST,  ARGS_NO_FLAGS       );  /* Source line-number -- SEQ,LOC,BIN,SRC = whole line */
    ADD_LIST_ARG(SRC,  ARGS_LIST,  ARGS_NO_FLAGS       );  /* Source line -- SEQ,LOC,BIN,SRC = whole line */
    ADD_LIST_ARG(SYM,  ARGS_LIST,  ARGS_NO_FLAGS       );  /* Symbol table */
    ADD_LIST_ARG(TOC,  ARGS_LIST,  ARGS_NO_FLAGS       );  /* Table of contents */
    ADD_LIST_ARG(TTM,  ARGS_NLIST, ARGS_NO_FLAGS       );  /* Printer format (132 or 80 columns) */

#undef ADD_ENABL_ARG
#undef ADD_LIST_ARG

}


/* load_dirargs loads the default directive argument values into the current values */

void load_dirargs(
    void)
{
    int i;

    for (i = 0; i < ARGS__LAST; i++)
        enabl_list_arg[i].curval = enabl_list_arg[i].defval;
}


/* dump_dirargs dumps the current values of directive arguments (for debugging) */

void dump_dirargs(
    const char *prefix)
{
    int i;

#define ARG_FLAGS(arg) ((arg & ARGS_NOT_SUPPORTED  ) ? '*' : \
                        (arg & ARGS_NOT_IMPLEMENTED) ? '-' : \
                        (arg & ARGS_IGNORE_THIS    ) ? '#' : \
                        (arg & ARGS_HIDDEN         ) ? ';' : \
                        (arg & ARGS_NO_FUNCTION    ) ? '.' : \
                        (arg & ARGS_MUST_SUPPORT   ) ? '!' : ' ')

    fprintf(stderr, "%s.ENABL", (prefix == NULL) ? "" : prefix);
    for (i = 0; i < ARGS__LAST; i++)
        if (enabl_list_arg[i].flags & ARGS_DSABL_ENABL)
            fprintf(stderr, " %c%3.3s=%-2d", ARG_FLAGS(enabl_list_arg[i].flags),
                                             enabl_list_arg[i].name, enabl_list_arg[i].curval);

    fprintf(stderr, "\n%s.LIST ", (prefix == NULL) ? "" : prefix);
    for (i = 0; i < ARGS__LAST; i++)
        if (enabl_list_arg[i].flags & ARGS_NLIST_LIST)
            fprintf(stderr, " %c%3.3s=%-2d", ARG_FLAGS(enabl_list_arg[i].flags),
                                             enabl_list_arg[i].name, enabl_list_arg[i].curval);
    fprintf(stderr, "\n");

#undef ARG_FLAGS
}


/* usage_dirarg shows the usage for a single directive argument list */

static void usage_dirarg(
    const char *prefix,
    unsigned    type,
    int         tf_value)
{
    int shown = 0;
    int i;

    for (i = 0; i < ARGS__LAST; i++) {
       if (enabl_list_arg[i].flags & type) {
           if ((!enabl_list_arg[i].defval) == (!tf_value)) {
               if ((enabl_list_arg[i].flags & ~ARGS_VISIBLE) == 0) {
                    if (!shown) {
                        shown = 1;
                        fprintf(stderr, "   %s %s", prefix, enabl_list_arg[i].name);
                    } else {
                        fprintf(stderr, ",%s", enabl_list_arg[i].name);
                    }
                }
            }
        }
    }
    if (shown)
        fprintf(stderr, "\n");
}


/* usage_dirargs shows the usage for -e and -d */

void usage_dirargs(
    void)
{
    usage_dirarg("[.ENABL] -e", ARGS_DSABL_ENABL, ARGS_DSABL);
    usage_dirarg("[.DSABL] -d", ARGS_DSABL_ENABL, ARGS_ENABL);
    usage_dirarg("[.LIST ] -e", ARGS_NLIST_LIST,  ARGS_NLIST);
    usage_dirarg("[.NLIST] -d", ARGS_NLIST_LIST,  ARGS_LIST);
}


/* lookup_arg finds the enum of a .ENABL or .DSABL directive */

/* MACRO-11 V05.05 is a lot more flexible with arguments.
 * Appended characters to the name (if > 3 characters) are ignored.
 * E.g. .LIST BINARY is valid as is .NLIST HEXADECIMAL.
 * TODO: If not STRINGENT we could do this too.  This code already
 *       supports it - we need to do it when we call lookup_arg(). */

int lookup_arg(
    const char argnam[4],
    unsigned   type)
{
    int i;

    for (i = 0; i < ARGS__LAST; i++)
        if (enabl_list_arg[i].flags & type)
            if (enabl_list_arg[i].name[0] == argnam[0] &&
                enabl_list_arg[i].name[1] == argnam[1] &&
                enabl_list_arg[i].name[2] == argnam[2])
                return i;
    return -1;
}


/* add_symbols adds all the internal symbols. */

void add_symbols(
    SECTION *current_section)
{
    current_pc = add_sym(".", 0, SYMBOLFLAG_DEFINITION, current_section, &symbol_st, NULL);

/**********************************************************************/

#define ADD_SYM_REGISTER(label, value) \
            add_sym(label, value, (SYMBOLFLAG_PERMANENT | SYMBOLFLAG_DEFINITION), \
                    &register_section, &system_st, NULL)

    reg_sym[0] = ADD_SYM_REGISTER("R0", 0);
    reg_sym[1] = ADD_SYM_REGISTER("R1", 1);
    reg_sym[2] = ADD_SYM_REGISTER("R2", 2);
    reg_sym[3] = ADD_SYM_REGISTER("R3", 3);
    reg_sym[4] = ADD_SYM_REGISTER("R4", 4);
    reg_sym[5] = ADD_SYM_REGISTER("R5", 5);
    reg_sym[6] = ADD_SYM_REGISTER("SP", 6);
    reg_sym[7] = ADD_SYM_REGISTER("PC", 7);
    REG_ERR    = ADD_SYM_REGISTER("%E", REG_ERR_VALUE);  /* Used for invalid register-number (%0-%7) usage */

/**********************************************************************/

#define ADD_SYM_PSEUDO(label, value) \
            add_sym(label, value, (SYMBOLFLAG_PERMANENT | SYMBOLFLAG_DEFINITION), \
                    &pseudo_section, &system_st, NULL)

    /* Symbols longer than current SYMMAX will be truncated. SYMMAX=6 is minimum! */

    ADD_SYM_PSEUDO(".ASCII",   P_ASCII);
    ADD_SYM_PSEUDO(".ASCIZ",   P_ASCIZ);
    ADD_SYM_PSEUDO(".ASECT",   P_ASECT);
    ADD_SYM_PSEUDO(".BLKB",    P_BLKB);
    ADD_SYM_PSEUDO(".BLKW",    P_BLKW);
    ADD_SYM_PSEUDO(".BYTE",    P_BYTE);
    ADD_SYM_PSEUDO(".CROSS",   P_CROSS);
    ADD_SYM_PSEUDO(".CSECT",   P_CSECT);
    ADD_SYM_PSEUDO(".DSABL",   P_DSABL);
    ADD_SYM_PSEUDO(".ENABL",   P_ENABL);
    ADD_SYM_PSEUDO(".END",     P_END);
    ADD_SYM_PSEUDO(".ENDC",    P_ENDC);
    ADD_SYM_PSEUDO(".ENDM",    P_ENDM);
    ADD_SYM_PSEUDO(".ENDR",    P_ENDR);
    ADD_SYM_PSEUDO(".EOT",     P_EOT);
    ADD_SYM_PSEUDO(".ERROR",   P_ERROR);
    ADD_SYM_PSEUDO(".EVEN",    P_EVEN);
    ADD_SYM_PSEUDO(".FLT2",    P_FLT2);
    ADD_SYM_PSEUDO(".FLT4",    P_FLT4);
    ADD_SYM_PSEUDO(".GLOBL",   P_GLOBL);
    ADD_SYM_PSEUDO(".IDENT",   P_IDENT);
    ADD_SYM_PSEUDO(".IF",      P_IF);
    ADD_SYM_PSEUDO(".IFF",     P_IFF);
    ADD_SYM_PSEUDO(".IFT",     P_IFT);
    ADD_SYM_PSEUDO(".IFTF",    P_IFTF);
    ADD_SYM_PSEUDO(".IFDF",    P_IFXX);
    ADD_SYM_PSEUDO(".IFNDF",   P_IFXX);
    ADD_SYM_PSEUDO(".IFZ",     P_IFXX);
    ADD_SYM_PSEUDO(".IFEQ",    P_IFXX);
    ADD_SYM_PSEUDO(".IFNZ",    P_IFXX);
    ADD_SYM_PSEUDO(".IFNE",    P_IFXX);
    ADD_SYM_PSEUDO(".IFL",     P_IFXX);
    ADD_SYM_PSEUDO(".IFLT",    P_IFXX);
    ADD_SYM_PSEUDO(".IFG",     P_IFXX);
    ADD_SYM_PSEUDO(".IFGT",    P_IFXX);
    ADD_SYM_PSEUDO(".IFLE",    P_IFXX);
    ADD_SYM_PSEUDO(".IFGE",    P_IFXX);
    ADD_SYM_PSEUDO(".IIF",     P_IIF);
    ADD_SYM_PSEUDO(".INCLUDE", P_INCLUDE);
    ADD_SYM_PSEUDO(".IRP",     P_IRP);
    ADD_SYM_PSEUDO(".IRPC",    P_IRPC);
    ADD_SYM_PSEUDO(".LIBRARY", P_LIBRARY);
    ADD_SYM_PSEUDO(".LIMIT",   P_LIMIT);
    ADD_SYM_PSEUDO(".LIST",    P_LIST);
    ADD_SYM_PSEUDO(".MACRO",   P_MACRO);
    ADD_SYM_PSEUDO(".MCALL",   P_MCALL);
    ADD_SYM_PSEUDO(".MDELETE", P_MDELETE);
    ADD_SYM_PSEUDO(".MEXIT",   P_MEXIT);
    ADD_SYM_PSEUDO(".NARG",    P_NARG);
    ADD_SYM_PSEUDO(".NCHR",    P_NCHR);
    ADD_SYM_PSEUDO(".NLIST",   P_NLIST);
    ADD_SYM_PSEUDO(".NOCROSS", P_NOCROSS);
    ADD_SYM_PSEUDO(".NTYPE",   P_NTYPE);
    ADD_SYM_PSEUDO(".ODD",     P_ODD);
    ADD_SYM_PSEUDO(".PACKED",  P_PACKED);
    ADD_SYM_PSEUDO(".PAGE",    P_PAGE);
    ADD_SYM_PSEUDO(".PRINT",   P_PRINT);
    ADD_SYM_PSEUDO(".PSECT",   P_PSECT);
    ADD_SYM_PSEUDO(".RAD50",   P_RAD50);
    ADD_SYM_PSEUDO(".RADIX",   P_RADIX);
    ADD_SYM_PSEUDO(".REM",     P_REM);
    ADD_SYM_PSEUDO(".REPT",    P_REPT);
    ADD_SYM_PSEUDO(".RESTORE", P_RESTORE);
    ADD_SYM_PSEUDO(".SAVE",    P_SAVE);
    ADD_SYM_PSEUDO(".SBTTL",   P_SBTTL);
    ADD_SYM_PSEUDO(".TITLE",   P_TITLE);
    ADD_SYM_PSEUDO(".WEAK",    P_WEAK);
    ADD_SYM_PSEUDO(".WORD",    P_WORD);

/**********************************************************************/

#define ADD_SYM_INST(label, value, operand) \
            add_sym(label, value, operand, \
                    &instruction_section, &system_st, NULL)

    ADD_SYM_INST("ADC",    I_ADC,    OC_1GEN);
    ADD_SYM_INST("ADCB",   I_ADCB,   OC_1GEN);
    ADD_SYM_INST("ADD",    I_ADD,    OC_2GEN);
    ADD_SYM_INST("ASH",    I_ASH,    OC_ASH);
    ADD_SYM_INST("ASHC",   I_ASHC,   OC_ASH);
    ADD_SYM_INST("ASL",    I_ASL,    OC_1GEN);
    ADD_SYM_INST("ASLB",   I_ASLB,   OC_1GEN);
    ADD_SYM_INST("ASR",    I_ASR,    OC_1GEN);
    ADD_SYM_INST("ASRB",   I_ASRB,   OC_1GEN);
    ADD_SYM_INST("BCC",    I_BCC,    OC_BR);
    ADD_SYM_INST("BCS",    I_BCS,    OC_BR);
    ADD_SYM_INST("BEQ",    I_BEQ,    OC_BR);
    ADD_SYM_INST("BGE",    I_BGE,    OC_BR);
    ADD_SYM_INST("BGT",    I_BGT,    OC_BR);
    ADD_SYM_INST("BHI",    I_BHI,    OC_BR);
    ADD_SYM_INST("BHIS",   I_BHIS,   OC_BR);
    ADD_SYM_INST("BIC",    I_BIC,    OC_2GEN);
    ADD_SYM_INST("BICB",   I_BICB,   OC_2GEN);
    ADD_SYM_INST("BIS",    I_BIS,    OC_2GEN);
    ADD_SYM_INST("BISB",   I_BISB,   OC_2GEN);
    ADD_SYM_INST("BIT",    I_BIT,    OC_2GEN);
    ADD_SYM_INST("BITB",   I_BITB,   OC_2GEN);
    ADD_SYM_INST("BLE",    I_BLE,    OC_BR);
    ADD_SYM_INST("BLO",    I_BLO,    OC_BR);
    ADD_SYM_INST("BLOS",   I_BLOS,   OC_BR);
    ADD_SYM_INST("BLT",    I_BLT,    OC_BR);
    ADD_SYM_INST("BMI",    I_BMI,    OC_BR);
    ADD_SYM_INST("BNE",    I_BNE,    OC_BR);
    ADD_SYM_INST("BPL",    I_BPL,    OC_BR);
    ADD_SYM_INST("BPT",    I_BPT,    OC_NONE);
    ADD_SYM_INST("BR",     I_BR,     OC_BR);
    ADD_SYM_INST("BVC",    I_BVC,    OC_BR);
    ADD_SYM_INST("BVS",    I_BVS,    OC_BR);
    ADD_SYM_INST("CALL",   I_CALL,   OC_1GEN);
    ADD_SYM_INST("CALLR",  I_CALLR,  OC_1GEN);
    ADD_SYM_INST("CCC",    I_CCC,    OC_NONE);
    ADD_SYM_INST("CLC",    I_CLC,    OC_NONE);
    ADD_SYM_INST("CLN",    I_CLN,    OC_NONE);
    ADD_SYM_INST("CLR",    I_CLR,    OC_1GEN);
    ADD_SYM_INST("CLRB",   I_CLRB,   OC_1GEN);
    ADD_SYM_INST("CLV",    I_CLV,    OC_NONE);
    ADD_SYM_INST("CLZ",    I_CLZ,    OC_NONE);
    ADD_SYM_INST("CMP",    I_CMP,    OC_2GEN);
    ADD_SYM_INST("CMPB",   I_CMPB,   OC_2GEN);
    ADD_SYM_INST("COM",    I_COM,    OC_1GEN);
    ADD_SYM_INST("COMB",   I_COMB,   OC_1GEN);
    ADD_SYM_INST("CSM",    I_CSM,    OC_1GEN);
    ADD_SYM_INST("DEC",    I_DEC,    OC_1GEN);
    ADD_SYM_INST("DECB",   I_DECB,   OC_1GEN);
    ADD_SYM_INST("DIV",    I_DIV,    OC_ASH);
    ADD_SYM_INST("EMT",    I_EMT,    OC_MARK);
    ADD_SYM_INST("FADD",   I_FADD,   OC_1REG);
    ADD_SYM_INST("FDIV",   I_FDIV,   OC_1REG);
    ADD_SYM_INST("FMUL",   I_FMUL,   OC_1REG);
    ADD_SYM_INST("FSUB",   I_FSUB,   OC_1REG);
    ADD_SYM_INST("HALT",   I_HALT,   OC_NONE);
    ADD_SYM_INST("INC",    I_INC,    OC_1GEN);
    ADD_SYM_INST("INCB",   I_INCB,   OC_1GEN);
    ADD_SYM_INST("IOT",    I_IOT,    OC_NONE);
    ADD_SYM_INST("JMP",    I_JMP,    OC_1GEN);
    ADD_SYM_INST("JSR",    I_JSR,    OC_JSR);
    ADD_SYM_INST("MARK",   I_MARK,   OC_MARK);
    ADD_SYM_INST("MED6X",  I_MED6X,  OC_NONE);  /* PDP-11/60 Maintenance */
    ADD_SYM_INST("MED74C", I_MED74C, OC_NONE);  /* PDP-11/74 CIS Maintenance */
    ADD_SYM_INST("MFPD",   I_MFPD,   OC_1GEN);
    ADD_SYM_INST("MFPI",   I_MFPI,   OC_1GEN);
    ADD_SYM_INST("MFPS",   I_MFPS,   OC_1GEN);
    ADD_SYM_INST("MFPT",   I_MFPT,   OC_NONE);
    ADD_SYM_INST("MOV",    I_MOV,    OC_2GEN);
    ADD_SYM_INST("MOVB",   I_MOVB,   OC_2GEN);
    ADD_SYM_INST("MTPD",   I_MTPD,   OC_1GEN);
    ADD_SYM_INST("MTPI",   I_MTPI,   OC_1GEN);
    ADD_SYM_INST("MTPS",   I_MTPS,   OC_1GEN);
    ADD_SYM_INST("MUL",    I_MUL,    OC_ASH);
    ADD_SYM_INST("NEG",    I_NEG,    OC_1GEN);
    ADD_SYM_INST("NEGB",   I_NEGB,   OC_1GEN);
    ADD_SYM_INST("NOP",    I_NOP,    OC_NONE);
    ADD_SYM_INST("RESET",  I_RESET,  OC_NONE);
    ADD_SYM_INST("RETURN", I_RETURN, OC_NONE);
    ADD_SYM_INST("ROL",    I_ROL,    OC_1GEN);
    ADD_SYM_INST("ROLB",   I_ROLB,   OC_1GEN);
    ADD_SYM_INST("ROR",    I_ROR,    OC_1GEN);
    ADD_SYM_INST("RORB",   I_RORB,   OC_1GEN);
    ADD_SYM_INST("RTI",    I_RTI,    OC_NONE);
    ADD_SYM_INST("RTS",    I_RTS,    OC_1REG);
    ADD_SYM_INST("RTT",    I_RTT,    OC_NONE);
    ADD_SYM_INST("SBC",    I_SBC,    OC_1GEN);
    ADD_SYM_INST("SBCB",   I_SBCB,   OC_1GEN);
    ADD_SYM_INST("SCC",    I_SCC,    OC_NONE);
    ADD_SYM_INST("SEC",    I_SEC,    OC_NONE);
    ADD_SYM_INST("SEN",    I_SEN,    OC_NONE);
    ADD_SYM_INST("SEV",    I_SEV,    OC_NONE);
    ADD_SYM_INST("SEZ",    I_SEZ,    OC_NONE);
    ADD_SYM_INST("SOB",    I_SOB,    OC_SOB);
    ADD_SYM_INST("SPL",    I_SPL,    OC_MARK);
    ADD_SYM_INST("SUB",    I_SUB,    OC_2GEN);
    ADD_SYM_INST("SWAB",   I_SWAB,   OC_1GEN);
    ADD_SYM_INST("SXT",    I_SXT,    OC_1GEN);
    ADD_SYM_INST("TRAP",   I_TRAP,   OC_MARK);
    ADD_SYM_INST("TST",    I_TST,    OC_1GEN);
    ADD_SYM_INST("TSTB",   I_TSTB,   OC_1GEN);
    ADD_SYM_INST("TSTSET", I_TSTSET, OC_1GEN);
    ADD_SYM_INST("WAIT",   I_WAIT,   OC_NONE);
    ADD_SYM_INST("WRTLCK", I_WRTLCK, OC_1GEN);
    ADD_SYM_INST("XFC",    I_XFC,    OC_NONE);  /* PDP-11/60 Extended Function Code (is not in V05.05) */
    ADD_SYM_INST("XOR",    I_XOR,    OC_JSR);

    /* FPP instructions */

    ADD_SYM_INST("ABSD",   I_ABSD,   OC_FPP_FDST);
    ADD_SYM_INST("ABSF",   I_ABSF,   OC_FPP_FDST);
    ADD_SYM_INST("ADDD",   I_ADDD,   OC_FPP_FSRCAC);
    ADD_SYM_INST("ADDF",   I_ADDF,   OC_FPP_FSRCAC);
    ADD_SYM_INST("CFCC",   I_CFCC,   OC_NONE);
    ADD_SYM_INST("CLRD",   I_CLRD,   OC_FPP_FDST);
    ADD_SYM_INST("CLRF",   I_CLRF,   OC_FPP_FDST);
    ADD_SYM_INST("CMPD",   I_CMPD,   OC_FPP_FSRCAC);
    ADD_SYM_INST("CMPF",   I_CMPF,   OC_FPP_FSRCAC);
    ADD_SYM_INST("DIVD",   I_DIVD,   OC_FPP_FSRCAC);
    ADD_SYM_INST("DIVF",   I_DIVF,   OC_FPP_FSRCAC);
    ADD_SYM_INST("LDCDF",  I_LDCDF,  OC_FPP_FSRCAC);
    ADD_SYM_INST("LDCFD",  I_LDCFD,  OC_FPP_FSRCAC);
    ADD_SYM_INST("LDCID",  I_LDCID,  OC_FPP_SRCAC);
    ADD_SYM_INST("LDCIF",  I_LDCIF,  OC_FPP_SRCAC);
    ADD_SYM_INST("LDCLD",  I_LDCLD,  OC_FPP_SRCAC);
    ADD_SYM_INST("LDCLF",  I_LDCLF,  OC_FPP_SRCAC);
    ADD_SYM_INST("LDD",    I_LDD,    OC_FPP_FSRCAC);
    ADD_SYM_INST("LDEXP",  I_LDEXP,  OC_FPP_SRCAC);
    ADD_SYM_INST("LDF",    I_LDF,    OC_FPP_FSRCAC);
    ADD_SYM_INST("LDFPS",  I_LDFPS,  OC_1GEN);
    ADD_SYM_INST("LDSC",   I_LDSC,   OC_NONE);        /* Load Step Counter (11/45) */
    ADD_SYM_INST("LDUB",   I_LDUB,   OC_NONE);        /* Load microbreak register (11/60, 11/45) */
    ADD_SYM_INST("MODD",   I_MODD,   OC_FPP_FSRCAC);
    ADD_SYM_INST("MODF",   I_MODF,   OC_FPP_FSRCAC);
    ADD_SYM_INST("MULD",   I_MULD,   OC_FPP_FSRCAC);
    ADD_SYM_INST("MULF",   I_MULF,   OC_FPP_FSRCAC);
    ADD_SYM_INST("NEGD",   I_NEGD,   OC_FPP_FDST);
    ADD_SYM_INST("NEGF",   I_NEGF,   OC_FPP_FDST);
    ADD_SYM_INST("SETD",   I_SETD,   OC_NONE);
    ADD_SYM_INST("SETF",   I_SETF,   OC_NONE);
    ADD_SYM_INST("SETI",   I_SETI,   OC_NONE);
    ADD_SYM_INST("SETL",   I_SETL,   OC_NONE);
    ADD_SYM_INST("STA0",   I_STA0,   OC_NONE);        /* Store AR in AC0 (11/45) */
    ADD_SYM_INST("STB0",   I_STB0,   OC_NONE);        /* Diagnostic Floating Point [Maintenance Right Shift] */
    ADD_SYM_INST("STQ0",   I_STQ0,   OC_NONE);        /* Store QR in AC0 (11/45) */
    ADD_SYM_INST("STCDF",  I_STCDF,  OC_FPP_ACFDST);
    ADD_SYM_INST("STCDI",  I_STCDI,  OC_FPP_ACFDST);
    ADD_SYM_INST("STCDL",  I_STCDL,  OC_FPP_ACFDST);
    ADD_SYM_INST("STCFD",  I_STCFD,  OC_FPP_ACFDST);
    ADD_SYM_INST("STCFI",  I_STCFI,  OC_FPP_ACFDST);
    ADD_SYM_INST("STCFL",  I_STCFL,  OC_FPP_ACFDST);
    ADD_SYM_INST("STD",    I_STD,    OC_FPP_ACFDST);
    ADD_SYM_INST("STEXP",  I_STEXP,  OC_FPP_ACDST);
    ADD_SYM_INST("STF",    I_STF,    OC_FPP_ACFDST);
    ADD_SYM_INST("STFPS",  I_STFPS,  OC_1GEN);
    ADD_SYM_INST("STST",   I_STST,   OC_1GEN);
    ADD_SYM_INST("SUBD",   I_SUBD,   OC_FPP_FSRCAC);
    ADD_SYM_INST("SUBF",   I_SUBF,   OC_FPP_FSRCAC);
    ADD_SYM_INST("TSTD",   I_TSTD,   OC_FPP_FDST);
    ADD_SYM_INST("TSTF",   I_TSTF,   OC_FPP_FDST);

    /* The CIS instructions */

    ADD_SYM_INST("ADDN",   I_ADDN,          OC_NONE);
    ADD_SYM_INST("ADDNI",  I_ADDN|I_CIS_I,  OC_CIS3);
    ADD_SYM_INST("ADDP",   I_ADDP,          OC_NONE);
    ADD_SYM_INST("ADDPI",  I_ADDP|I_CIS_I,  OC_CIS3);
    ADD_SYM_INST("ASHN",   I_ASHN,          OC_NONE);
    ADD_SYM_INST("ASHNI",  I_ASHN|I_CIS_I,  OC_CIS3);
    ADD_SYM_INST("ASHP",   I_ASHP,          OC_NONE);
    ADD_SYM_INST("ASHPI",  I_ASHP|I_CIS_I,  OC_CIS3);
    ADD_SYM_INST("CMPC",   I_CMPC,          OC_NONE);
    ADD_SYM_INST("CMPCI",  I_CMPC|I_CIS_I,  OC_CIS3);
    ADD_SYM_INST("CMPN",   I_CMPN,          OC_NONE);
    ADD_SYM_INST("CMPNI",  I_CMPN|I_CIS_I,  OC_CIS2);
    ADD_SYM_INST("CMPP",   I_CMPP,          OC_NONE);
    ADD_SYM_INST("CMPPI",  I_CMPP|I_CIS_I,  OC_CIS2);
    ADD_SYM_INST("CVTLN",  I_CVTLN,         OC_NONE);
    ADD_SYM_INST("CVTLNI", I_CVTLN|I_CIS_I, OC_CIS2);
    ADD_SYM_INST("CVTLP",  I_CVTPL,         OC_NONE);
    ADD_SYM_INST("CVTLPI", I_CVTLP|I_CIS_I, OC_CIS2);
    ADD_SYM_INST("CVTNL",  I_CVTNL,         OC_NONE);
    ADD_SYM_INST("CVTNLI", I_CVTNL|I_CIS_I, OC_CIS2);
    ADD_SYM_INST("CVTNP",  I_CVTNP,         OC_NONE);
    ADD_SYM_INST("CVTNPI", I_CVTNP|I_CIS_I, OC_CIS2);
    ADD_SYM_INST("CVTPL",  I_CVTPL,         OC_NONE);
    ADD_SYM_INST("CVTPLI", I_CVTPL|I_CIS_I, OC_CIS2);
    ADD_SYM_INST("CVTPN",  I_CVTPN,         OC_NONE);
    ADD_SYM_INST("CVTPNI", I_CVTPN|I_CIS_I, OC_CIS2);
    ADD_SYM_INST("DIVP",   I_DIVP,          OC_NONE);
    ADD_SYM_INST("DIVPI",  I_DIVP|I_CIS_I,  OC_CIS3);
    ADD_SYM_INST("L2D0",   I_L2Dr+0,        OC_NONE);
    ADD_SYM_INST("L2D1",   I_L2Dr+1,        OC_NONE);
    ADD_SYM_INST("L2D2",   I_L2Dr+2,        OC_NONE);
    ADD_SYM_INST("L2D3",   I_L2Dr+3,        OC_NONE);
    ADD_SYM_INST("L2D4",   I_L2Dr+4,        OC_NONE);
    ADD_SYM_INST("L2D5",   I_L2Dr+5,        OC_NONE);
    ADD_SYM_INST("L2D6",   I_L2Dr+6,        OC_NONE);
    ADD_SYM_INST("L2D7",   I_L2Dr+7,        OC_NONE);
    ADD_SYM_INST("L3D0",   I_L3Dr+0,        OC_NONE);
    ADD_SYM_INST("L3D1",   I_L3Dr+1,        OC_NONE);
    ADD_SYM_INST("L3D2",   I_L3Dr+2,        OC_NONE);
    ADD_SYM_INST("L3D3",   I_L3Dr+3,        OC_NONE);
    ADD_SYM_INST("L3D4",   I_L3Dr+4,        OC_NONE);
    ADD_SYM_INST("L3D5",   I_L3Dr+5,        OC_NONE);
    ADD_SYM_INST("L3D6",   I_L3Dr+6,        OC_NONE);
    ADD_SYM_INST("L3D7",   I_L3Dr+7,        OC_NONE);
    ADD_SYM_INST("LOCC",   I_LOCC,          OC_NONE);
    ADD_SYM_INST("LOCCI",  I_LOCC|I_CIS_I,  OC_CIS2);
    ADD_SYM_INST("MATC",   I_MATC,          OC_NONE);
    ADD_SYM_INST("MATCI",  I_MATC|I_CIS_I,  OC_CIS2);
    ADD_SYM_INST("MOVC",   I_MOVC,          OC_NONE);
    ADD_SYM_INST("MOVCI",  I_MOVC|I_CIS_I,  OC_CIS3);
    ADD_SYM_INST("MOVRC",  I_MOVRC,         OC_NONE);
    ADD_SYM_INST("MOVRCI", I_MOVRC|I_CIS_I, OC_CIS3);
    ADD_SYM_INST("MOVTC",  I_MOVTC,         OC_NONE);
    ADD_SYM_INST("MOVTCI", I_MOVTC|I_CIS_I, OC_CIS4);
    ADD_SYM_INST("MULP",   I_MULP,          OC_NONE);
    ADD_SYM_INST("MULPI",  I_MULP|I_CIS_I,  OC_CIS3);
    ADD_SYM_INST("SCANC",  I_SCANC,         OC_NONE);
    ADD_SYM_INST("SCANCI", I_SCANC|I_CIS_I, OC_CIS2);
    ADD_SYM_INST("SKPC",   I_SKPC,          OC_NONE);
    ADD_SYM_INST("SKPCI",  I_SKPC|I_CIS_I,  OC_CIS2);
    ADD_SYM_INST("SPANC",  I_SPANC,         OC_NONE);
    ADD_SYM_INST("SPANCI", I_SPANC|I_CIS_I, OC_CIS2);
    ADD_SYM_INST("SUBN",   I_SUBN,          OC_NONE);
    ADD_SYM_INST("SUBNI",  I_SUBN|I_CIS_I,  OC_CIS3);
    ADD_SYM_INST("SUBP",   I_SUBP,          OC_NONE);
    ADD_SYM_INST("SUBPI",  I_SUBP|I_CIS_I,  OC_CIS3);

/**********************************************************************/

#define UNSUPPORT_ARG(arg) \
        if (!(enabl_list_arg[arg].flags & ARGS_MUST_SUPPORT)) \
            enabl_list_arg[arg].flags |= ARGS_NOT_SUPPORTED

    if (STRINGENT) {
        UNSUPPORT_ARG(E_BMK);
        UNSUPPORT_ARG(E_DBG);
    }

    if (support_m11) {
        enabl_list_arg[E_PIC].flags &= ~ARGS_NOT_SUPPORTED;
        enabl_list_arg[E_PIC].flags |=  ARGS_NOT_IMPLEMENTED;  /* TODO: Add PIC checks like 'TST @#rel or TST abs' */
        ADD_SYM_PSEUDO(".MACR", P_MACRO);                      /* Probably never used anyway */
        ADD_SYM_INST("CNZ", I_CLN | I_CLZ, OC_NONE);           /* This neither */
    } else if (STRINGENT) {
        UNSUPPORT_ARG(L_LD);
    }

#undef UNSUPPORT_ARG

/**********************************************************************/

    /* TODO: Why are we doing this (it seems to work without) */

    if (!STRINGENT)
        add_sym(current_section->label, 0,
             /* 0, // This was the original flags value */
             /* (SYMBOLFLAG_PERMANENT | SYMBOLFLAG_DEFINITION), */
                SYMBOLFLAG_UNDEFINED,
                current_section, &section_st, NULL);

#undef ADD_SYM_REGISTER
#undef ADD_SYM_PSEUDO
#undef ADD_SYM_INST
}


/* sym_hist is a diagnostic function that prints a histogram of the
   hash table useage of a symbol table.  I used this to try to tune
   the hash function for better spread.  It's not used now. */

void sym_hist(
    SYMBOL_TABLE *st,
    char *name)
{
    int             i;
    SYMBOL         *sym;

    fprintf(lstfile, "Histogram for symbol table %s\n", name);
    for (i = 0; i < 1023; i++) {
        fprintf(lstfile, "%4d: ", i);
        for (sym = st->hash[i]; sym != NULL; sym = sym->next)
            fputc('#', lstfile);
        fputc('\n', lstfile);
    }
}


/* rad50order returns the order of a RAD50 character */
/* Control-characters return lower than RAD50 and ...
 * ... non-RAD50 characters return higher than RAD50 */

static int rad50order(
    int c)
{
    enum rad50value {
        R50_SPACE  = 000,  /* ' ' =  0. */
        R50_ALPHA  = 001,  /* 'A' =  1. */
                           /*  :        */
                           /* 'Z' = 26. */
        R50_DOLLAR = 033,  /* '$' = 27. */
        R50_PERIOD = 034,  /* '.' = 28. */
        R50_ULINE  = 035,  /* '_' = 29. */
        R50_DIGIT  = 036,  /* '0' = 30. */
                           /*  :        */
                           /* '9' = 39. */
        R50_NOTR50 = 050   /* ... = 40. */
    };

    if (isupper(c))
        return R50_ALPHA;

    if (isdigit(c))
        return R50_DIGIT;

    switch (c) {
    case ' ':
        return R50_SPACE;

    case '$':
        return R50_DOLLAR;

    case '.':
        return R50_PERIOD;

    case '_':
        return R50_ULINE;
    }

    if (!iscntrl(c))
        return R50_NOTR50;

    return -1;
}


/* rad50cmp has the same function as strcmp() but with the RAD50 sort order. */
/* The parameters are expected to contain only characters from the RAD50 set.
 * The sort-order is for the first non-matching character in the two strings.
 * '\nnn' sorts lowest and non-RAD50 characters (including lower-case) sort highest. */

static int rad50cmp(
    const char *p1,
    const char *p2)
{
    const unsigned char *s1 = (const unsigned char *) p1;
    const unsigned char *s2 = (const unsigned char *) p2;
          unsigned char  c1,
                         c2;

    do {
        c1 = *s1++;
        c2 = *s2++;
        if (c1 == '\0')
	    return -c2;
    } while (c1 == c2);

    {
       int r1 = rad50order((int) c1);
       int r2 = rad50order((int) c2);

       if (r1 != r2)
           return r1 - r2;
    }
    return c1 - c2;
}


/* #define rad50cmp strcmp  // Uncomment this line to disable sorting in RADIX50 order */

int symbol_compar(
    const void *a,
    const void *b)
{
    SYMBOL *sa = *(SYMBOL **)a;
    SYMBOL *sb = *(SYMBOL **)b;

    return rad50cmp(sa->label, sb->label);
}


void list_symbol_table(
    void)
{
    SYMBOL_ITER     iter;
    SYMBOL         *sym;
    int             longest_symbol = 6;
    int             nsyms = 0;

    /* Count the symbols in the table */
    for (sym = first_sym(&symbol_st, &iter); sym != NULL; sym = next_sym(&symbol_st, &iter)) {
        int             len;

        check_sym_invariants(sym, __FILE__, __LINE__, NULL);
        if (!symbol_list_locals) {
            if (sym->flags & SYMBOLFLAG_LOCAL)
                continue;
            if (sym->label[0] == '.' && sym->label[1] == '\0')
                continue;
        }
        nsyms++;

        len = strlen(sym->label);
        if (len > longest_symbol)
            longest_symbol = len;
    }

    /* Only list the symbol table if .LIST SYM */
    if (LIST(SYM)) {
        /* Sort the symbols by name */
        if (nsyms) {
            SYMBOL **symbols = memcheck(malloc(nsyms * sizeof (SYMBOL *)));
            SYMBOL **symbolp = symbols;

            for (sym = first_sym(&symbol_st, &iter); sym != NULL; sym = next_sym(&symbol_st, &iter)) {
                if (!symbol_list_locals) {
                    if (sym->flags & SYMBOLFLAG_LOCAL)
                        continue;
                    if (sym->label[0] == '.' && sym->label[1] == '\0')
                        continue;
                }
                *symbolp++ = sym;
            }

            qsort(symbols, nsyms, sizeof(SYMBOL *), symbol_compar);

            symbolp = symbols;

            fprintf(lstfile,"\n\nSymbol table\n\n");

            /* Print the listing in NCOLS columns. */
            {
                int ncols;
                int nlines;
                int line;

                ncols = (((LIST(TTM) ? 80 : 132) + 1 /* gap_len[] */) / (longest_symbol + 19 /* info. */));
                if (ncols == 0)
                    ncols = 1;  /* Always fit at least one sym on a line */
                nlines = (nsyms + ncols - 1) / ncols;

                /*
                 * .NLIST HEX ->
                 * <Symb>1234567890123456789 = info.
                 * DIRER$=%004562RGX    006
                 * ^     ^^^     ^      ^-- for R symbols: program segment number (unless ". ABS." or ". BLK.")
                 * |     |||     +-- Flags: R = relocatable
                 * |     |||                G = global
                 * |     |||                X = implicit global
                 * |     |||                L = local
                 * |     |||                W = weak
                 * |     ||+- value, ****** for if it was not a definition
                 * |     |+-- % for a register
                 * |     +--- = for an absolute value, blank for relocatable
                 * +- label name
                 *
                 * .LIST HEX ->
                 * LABEL1=%1234RGX    01....
                 */

                for (line = 0; line < nlines; line++) {
                    int             i;
                    int             format_num = (LIST(HEX) != 0);
                    const char     *format3[3] = {  /* 3-digit numbers (section number) */
                                        "%5.3o",    /* .NLIST HEX */
                                        "%3.2X",    /* .LIST  HEX */
                                        "%4.3u"     /* .LIST  LD  */
                                    };
                    int             length3[3] = {  /* Length of format3 */
                                        5,          /* .NLIST HEX */
                                        3,          /* .LIST  HEX */
                                        4           /* .LIST  LD  */
                                    };
                    const char     *format6[3] = {  /* 6-digit numbers (address or value) */
                                        "%06o",     /* .NLIST HEX */
                                        "%04X",     /* .LIST  HEX */
                                        "%5u"       /* .LIST  LD  */

                                    };
                    int             length6[3] = {  /* Length of format6 */
                                        6,          /* .NLIST HEX */
                                        4,          /* .LIST  HEX */
                                        5           /* .LIST  LD  */
                                    };

                    int             gap_len[3] = {  /* Length of gap after symbol */
                                        1,          /* .NLIST HEX */
                                        5,          /* .LIST  HEX */
                                        3           /* .LIST  LD  */
                                    };

                    if (LIST(LD))
                        format_num = 2;

                    for (i = line; i < nsyms; i += nlines) {
                        sym = symbols[i];

                        fprintf(lstfile, "%-*s", longest_symbol, sym->label);
                        fprintf(lstfile, "%c", (sym->section->flags & PSECT_REL) ? ' ' : '=');
                        fprintf(lstfile, "%c", (sym->section->type == SECTION_REGISTER) ? '%' : ' ');

                        if (!(sym->flags & SYMBOLFLAG_DEFINITION)) {
                            fprintf(lstfile, "%.*s", length6[format_num], "******");
                        } else {
                            fprintf(lstfile, format6[format_num], sym->value & 0177777);
                        }

                        fprintf(lstfile, "%c", (sym->section->flags & PSECT_REL) ? 'R' : ' ');
                        fprintf(lstfile, "%c", (sym->flags & SYMBOLFLAG_GLOBAL) ?  'G' : ' ');
                        fprintf(lstfile, "%c", (sym->flags & SYMBOLFLAG_IMPLICIT_GLOBAL) ? 'X' : ' ');
                        fprintf(lstfile, "%c", (sym->flags & SYMBOLFLAG_LOCAL) ?   'L' : ' ');
                        fprintf(lstfile, "%c", (sym->flags & SYMBOLFLAG_WEAK) ?    'W' : ' ');


                        if (sym->section->sector != 0 &&
                            (sym->section != &blank_section || symbol_list_locals)) {
                            fprintf(lstfile, format3[format_num], sym->section->sector);
                        } else {
                        /*  if (i + nlines < nsyms)  // The right-hand edge lines up when commented out */
                                fprintf(lstfile, "%*s", length3[format_num], "");
                        }

                        if (i + nlines < nsyms)
                            fprintf(lstfile, "%*s", gap_len[format_num], "");
                    }
                    fprintf(lstfile,"\n");
                }
            }
            free(symbols);
        }

        /* List sections */

        fprintf(lstfile,"\n\nProgram sections" /* ":" */ "\n\n");

        {
            int i;

            for (i = 0; i < sector; i++) {
                list_section(sections[i]);
            }
        }
    }
}


void list_section(
    SECTION *sec)
{
    int             flags      = sec->flags;
    int             format_num = (LIST(HEX) != 0);
    const char     *format[3]  = {
                        "%-6s  %06o  %5.3o   ",      /* .NLIST HEX */
                        "%-6s  %04X   %3.2X      ",  /* .LIST  HEX */
                        "%-6s  %5u   %4.3u    "      /* .LIST  LD  */
                    };

    if (sec == NULL) {
        fprintf(lstfile, "(null)\n");  /* Should never happen */
        return;
    }

    if (LIST(LD))
        format_num = 2;

    fprintf(lstfile, format[format_num],
            sec->label, sec->size & 0177777, sec->sector);

    fprintf(lstfile, "(%s,%s,%s,%s,%s%s)\n",
           (flags & PSECT_RO)   ?  "RO"  : "RW",
           (flags & PSECT_DATA) ?  "D"   : "I",
           (flags & PSECT_GBL)  ?  "GBL" : "LCL",
           (flags & PSECT_REL)  ?  "REL" : "ABS",
           (flags & PSECT_COM)  ?  "OVR" : "CON",
           (flags & PSECT_SAV)  ? ",SAV" : (STRINGENT && !rt11) ? "" : ",NOSAV");
}
