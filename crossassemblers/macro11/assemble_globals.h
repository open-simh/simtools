#ifndef ASSEMBLE_GLOBALS__H
#define ASSEMBLE_GLOBALS__H


#include "extree.h"
#include "mlb.h"
#include "symbols.h"


#define MAX_FILE_LINE_LENGTH 132       /* DOC: 2.2 - Max length of an input source line from a file */
#define START_LOCSYM 30000             /* DOC: J.1.6 - Start locally generated symbols at 30000$ */
#define MAX_LOCSYM 65535               /* DOC: 3.5 - Strictly, local symbols are 1$ to 65535$ */
#define BAD_LOCSYM 999999              /* No local symbol may ever be higher than this */
#define MAX_MLBS 32                    /* DOC: 6.10.1 - Number of macro libraries (RT-11 max <= 12) */
                                       /* TODO: DOC: 6.10.2 - max nesting of .INCLUDE directives is five */
                                       /* TODO: Add other limits which we have coded directly (!) */


#define MAX_CONDS 256
typedef struct cond {
    int             ok;         /* What the condition evaluated to */
    char           *file;       /* What file and line it occurred */
    int             line;
} COND;

#define SECT_STACK_SIZE 32

#define PASS1   0               /* Value of 'pass' for pass 1 */
#define PASS2   1               /* Value of 'pass' for pass 2 */

#ifndef ASSEMBLE_GLOBALS__C
/* GLOBAL VARIABLES */
extern int      pass;           /* The current assembly pass.  0 = first pass [PASS1] */
extern int      stmtno;         /* The current source line number */
extern int      radix;          /* The current input conversion radix */
extern int      lsb;            /* The current local symbol section identifier */
extern int      lsb_used;       /* Whether there was a local symbol using this lsb */
extern int      next_lsb;       /* The number of the next local symbol block */
extern int      last_macro_lsb; /* The last block in which a macro
                                   automatic label was created */

extern int      last_locsym;    /* The last local symbol number generated */

extern int      enabl_debug;    /* Whether assembler debugging is enabled */

extern int      support_m11;    /* Do we want to support m11 extensions? */

extern int      abs_0_based;    /* TRUE if all ABSolute sections are zero based (else only '. ABS.') */

extern int      strictness;     /* How strict (relaxed) do we want to be? */
                                /* <0 = relaxed, 0 = normal, >0 = strict */

#ifndef STRINGENTNESS
#define STRINGENTNESS 4         /* Strictness level we consider to be STRINGENT (0-4) */
#endif
#define STRINGENT     (strictness >= STRINGENTNESS)  /* As STRICTEST but also follow the MACRO-11 documentation */

#if (STRINGENTNESS > 3)
#define STRICT        (strictness >  0)  /* Close to MACRO-11 V05.05 syntax */
#define STRICTER      (strictness >  1)  /* As close as we like or even more */
#define STRICTEST     (strictness >  2)  /* Really mega-strict (e.g. .END required) */
#elif (STRINGENTNESS > 2)
#define STRICT        (strictness >= 0)  /* Close to MACRO-11 V05.05 syntax */
#define STRICTER      (strictness >  0)  /* As close as we like or even more */
#define STRICTEST     (strictness >  1)  /* Really mega-strict (e.g. .END required) */
#elif (STRINGENTNESS > 1)
#define STRICT        (strictness >= 0)  /* Close to MACRO-11 V05.05 syntax */
#define STRICTER      (strictness >= 0)  /* As close as we like or even more */
#define STRICTEST     (strictness >  0)  /* Really mega-strict (e.g. .END required) */
#else
#define STRICT        (strictness >= 0)  /* Close to MACRO-11 V05.05 syntax */
#define STRICTER      (strictness >= 0)  /* As close as we like or even more */
#define STRICTEST     (strictness >= 0)  /* Really mega-strict (e.g. .END required) */
#endif

#define RELAXED       (strictness <  0)  /* Relax the rules as much as we like */
#define VERY_RELAXED  (strictness < -1)  /* Relax the rules so much that even .END isn't the end */

extern int      suppressed;              /* Assembly suppressed by failed conditional */

extern int      ignore_fn_dev;           /* Ignore device names in .INCLUDE and .LIBRARY? */
extern int      ignore_fn_dir;           /* Ignore directory names in .INCLUDE and .LIBRARY? */

extern MLB     *mlbs[MAX_MLBS];          /* macro libraries specified on the command line */
extern int      nr_mlbs;                 /* Number of macro libraries */

extern COND     conds[MAX_CONDS];        /* Stack of recent conditions */
extern int      last_cond;               /* 0 means no stacked cond. */

extern SECTION *sect_stack[SECT_STACK_SIZE]; /* Saved sections: ^psect */
extern int      dot_stack[SECT_STACK_SIZE];  /* Saved sections: '.'    */
extern int      sect_sp;        /* Stack pointer */

extern char    *module_name;    /* The module name (taken from the '.TITLE') */

extern unsigned *ident;         /* .IDENT name (encoded RAD50 value) */

extern EX_TREE *xfer_address;   /* The transfer address */

extern SYMBOL  *current_pc;     /* The current program counter */

extern unsigned last_dot_addr;  /* Last coded PC... */
extern SECTION *last_dot_section;       /* ...and its program section */

/* The following are dummy psects for symbols which have meaning to
   the assembler: */
extern SECTION  register_section;
extern SECTION  pseudo_section; /* the section containing the  pseudo-operations */
extern SECTION  instruction_section;    /* the section containing instructions */
extern SECTION  macro_section;  /* Section for macros */

/* These are real psects that get written out to the object file */
extern SECTION  absolute_section;       /* The default  absolute section */
extern SECTION  blank_section;
extern SECTION *sections[256];  /* Array of sections in the order they were defined */
extern int      sector;         /* number of such sections */

#endif  /* ASSEMBLE_GLOBALS__C */

#endif  /* ASSEMBLE_GLOBALS__H */
