#define ASSEMBLE_GLOBALS__C

#include "assemble_globals.h"          /* My own definitions */

#include "object.h"


/* GLOBAL VARIABLES */
int             pass;           /* The current assembly pass.  0 = first pass [PASS1] */
int             stmtno;         /* The current source line number */
int             radix;          /* The current input conversion radix */

int             lsb;            /* The current local symbol section identifier */
int             lsb_used;       /* Whether there was a local symbol using this lsb */
int             next_lsb;       /* The number of the next local symbol block */
int             last_macro_lsb;         /* The last block in which a macro
                                         * automatic label was created */

int             last_locsym;            /* The last local symbol number generated */

int             suppressed;             /* Assembly suppressed by failed conditional */

int             enabl_debug = 0;        /* Whether assembler debugging is enabled */

int             support_m11 = 0;        /* Do we want to support m11 extensions? */

int             abs_0_based = 0;        /* TRUE if all ABSolute sections are zero based (else only '. ABS.') */

int             strictness = 0;         /* Neither -strict nor -relaxed are in effect */

int             ignore_fn_dev = 0;      /* Ignore device names in .INCLUDE and .LIBRARY? */
int             ignore_fn_dir = 0;      /* Ignore directory names in .INCLUDE and .LIBRARY? */

MLB            *mlbs[MAX_MLBS]; /* macro libraries specified on the
                                 * command line */
int             nr_mlbs = 0;    /* Number of macro libraries */

COND            conds[MAX_CONDS];  /* Stack of recent conditions */
int             last_cond;         /* 0 means no stacked cond. */

SECTION        *sect_stack[SECT_STACK_SIZE]; /* 32 saved sections */
int             dot_stack[SECT_STACK_SIZE];  /* 32 saved sections */
int             sect_sp;        /* Stack pointer */

char           *module_name = NULL;     /* The module name (taken from the 'TITLE'); */

unsigned       *ident = NULL;   /* Encoded .IDENT name */

EX_TREE        *xfer_address = NULL;    /* The transfer address */

SYMBOL         *current_pc;     /* The current program counter */

unsigned        last_dot_addr;  /* Last coded PC... */
SECTION        *last_dot_section;       /* ...and its program section */

/* The following are dummy psects for symbols which have meaning to
the assembler: */

SECTION         register_section = {
    "*REGISTERS*", SECTION_REGISTER, 0, 0, 0, 0
};                                     /* The section containing the registers */

SECTION         pseudo_section = {
    "*PSEUDO*", SECTION_PSEUDO, 0, 0, 0, 0
};                                     /* The section containing the
                                        * pseudo-operations */

SECTION         instruction_section = {
    "*INSTR*", SECTION_INSTRUCTION, 0, 0, 0, 0
};                                     /* The section containing instructions */

SECTION         macro_section = {
    "*MACRO*", SECTION_SYSTEM, 0, 0, 0, 0
};                                     /* Section for macros */

/* These are real psects that get written out to the object file */

SECTION         absolute_section = {
    ". ABS.", SECTION_SYSTEM, PSECT_GBL | PSECT_COM, 0, 0, 0
};                                     /* The default
                                        * absolute section */

SECTION         blank_section = {
    "", SECTION_SYSTEM, PSECT_REL, 0, 0, 1
};                                     /* The default relocatable section */

SECTION        *sections[256] = {
    /* Array of sections in the order they were defined */
    &absolute_section, &blank_section,
};

int             sector = 2;     /* Number of such sections */
