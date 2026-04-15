/*
    Assembler compatible with MACRO-11.

Copyright (c) 2001, Richard Krehbiel
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

o Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

o Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

o Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

*/

/*
 *  The goal of this project is to provide a portable MACRO-11 assembler
 *  which, as far as possible, will assemble source code which MACRO-11 on
 *  RSX-11M/PLUS would assemble.  The resulting executable program should be
 *  the same as on the original platform.
 *
 *  Source code which would produce errors on the original platform may produce
 *  different results using this program.  However, some effort has been made
 *  to detect errors and handle them similarly to the original.
 *
 *  Reference version is MACRO-11 V05.05 for RSX-11M/PLUS and RT-11.
 *  Documentation is the PDP-11 MACRO-11 Language Reference Manual (May 88) ...
 *  ... Order Number AA-KX10A-TC including Update Notice 1, AD-KXIOA-Tl.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "macro11.h"

#include "assemble.h"
#include "assemble_aux.h"
#include "assemble_globals.h"
#include "listing.h"
#include "macros.h"
#include "object.h"
#include "parse.h"
#include "rad50.h"
#include "stream2.h"
#include "symbols.h"
#include "util.h"


#ifdef WIN32
#define strcasecmp stricmp
#if !__STDC__
#define stricmp _stricmp
#endif
#endif


/* enable_tf is called by command argument parsing to enable and
   disable named options.  If flags != 0 then flags are |'ed instead. */

static void enable_tf(
    char    *opt,
    int      tf,
    unsigned flags)
{
    char           *cp = opt;
    char            argnam[4];
    int             argnum;
    int             len;

    do {
        if (cp == opt)
            cp = skipwhite(cp);
        else
            cp = skipdelim(cp);

        for (len = 0; len < 4; len++)
            if (isupper((unsigned char) *cp))
                argnam[len] = *cp++;
            else
               break;

        if (len < 2 || len > 3) {
           fprintf(stderr, "Invalid -%s option: %s [ignored]\n", (flags) ? "-dc" : (tf) ? "e" : "d", (cp - len));
           break;
        }

        argnam[len] = '\0';

        argnum = lookup_arg(argnam, ARGS_VISIBLE);
        if (argnum < 0) {
            fprintf(stderr, "Unknown -%s option: %s [ignored]\n", (flags) ? "dc" : (tf) ? "e" : "d", argnam);
        /*  break;  */
        } else {
            if (flags == 0) {
                if (argnum == L_LIS) {
                    if (tf)
                        enabl_list_arg[L_LIS].defval++;   /* Increase list level */
                    else
                        enabl_list_arg[L_LIS].defval--;   /* Decrease list level */
                } else {
                    enabl_list_arg[argnum].defval = tf;
                }
                enabl_list_arg[argnum].flags &= ~ARGS_NOT_SUPPORTED;  /* -e and -d ...     */
                enabl_list_arg[argnum].flags |= ARGS_MUST_SUPPORT;    /* ... force support */
            } else {
            /*  enabl_list_arg[argnum].flags &= ~ARGS_NOT_SUPPORTED;  // If -dc xxx, pretend we support 'xxx' */
                enabl_list_arg[argnum].flags |= flags;
            }
        }
    } while (*cp != '\0');
}


/*JH:*/
static void print_version(
    FILE *strm)
{
    fprintf(strm, PROGRAM_NAME " - portable MACRO-11 assembler for DEC PDP-11\n");
    if (enabl_debug)
        fprintf(strm, "  Version " THIS_VERSION "\n");
    fprintf(strm, "  Version " VERSIONSTR "\n");
    fprintf(strm, "  Copyright 2001 Richard Krehbiel,\n");
    fprintf(strm, "  modified  2009 by Joerg Hoppe,\n");
    fprintf(strm, "  modified  2015-2017,2020-2023 by Olaf 'Rhialto' Seibert,\n");
    fprintf(strm, "  modified  2023 by Mike Hill.\n");
    fprintf(strm, "\n");
}


static void append_env(
    char *envname,
    char *value)
{
    char           *env = getenv(envname);
    char           *temp;

    if (env == NULL)
        env = "";

    temp = memcheck(malloc(strlen(envname) +
                           1 +
                           strlen(env) +
                           1 +
                           strlen(value) +
                           1));
    strcpy(temp, envname);
    strcat(temp, "=");
    strcat(temp, env);
    strcat(temp, PATHSEP);
    strcat(temp, value);
    putenv(temp);
}


/*JH:*/
static void print_help(
    void)
{
    printf("\n");
    print_version(stdout);
    printf("Usage:\n");
    printf("  " PROGRAM_NAME " [-o <file>] [-l {- | <file>}] [-p1 | -p2] [-se] [-sp] [-fe[2]]\n");
    printf("          [-h] [-v] [-apb] [-ff] [-e <options>] [-d <options>] [-dc <options>]\n");
    printf("          [-a0] [-rsx | -rt11] [-stringent | -strict | -relaxed]\n");
    printf("          [-ym11] [-ysl <num>] [-yus] [-yfl] [-ylls] [-yl1] [-yd]\n");
    printf("          [-s [\"]<statement>[\"]] [-s ...]\n");
    printf("          [-nodev] [-nodir]\n");
    printf("          [-I <directory>]\n");
    printf("          [-m <file>] [-p <directory>] [-xl] [-xtract]\n");
    printf("          <inputfile> [<inputfile> ...]\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  <inputfile>  MACRO-11 source file(s) to assemble\n");
    printf("\n");
    printf("Options:\n");
    printf("-apb automatic page-break after %d listing lines (also with -xl).\n", PAGE_LENGTH);
    printf("-d   disable <options> (see below).\n");
    printf("-dc  disable changing of <options> (see below).\n");
    printf("-e   enable <options> (see below).\n");
    printf("-fe  fatal errors will abort assembly.\n");
/*  printf("-fe2 fatal errors will abort assembly on pass 2.\n");  */
    printf("-ff  use a form-feed <FF> between listing pages (also with -apb & -xl).\n");
    printf("-h   print this help.\n");
    printf("-I   gives the name of a directory in which .INCLUDEd files may be found.\n");
    printf("     Sets environment variable \"INCLUDE\".\n");
    printf("-l   gives the listing file name (.LST).\n");
    printf("     -l - enables listing to stdout.\n");
    printf("-o   gives the object file name (.OBJ).\n");
    printf("-p   gives the name of a directory in which .MCALLed macros may be found.\n");
    printf("     Sets environment variable \"MCALL\".\n");
    printf("-p1  only assemble pass 1 (not supported or recommended).\n");
    printf("-p2  only assemble pass 2 (not supported or recommended).\n");
    printf("-s   prepend a <statement> to the source (e.g. -s \"DEBUG\\t=\\t1\\t\\t; Enable\").\n");
    printf("-se  shows source line in error after the error message.\n");
    printf("-sp  shows .PRINT source lines on stderr (similar to -se).\n");
    printf("-v   print version.\n");
/*  printf("     Violates DEC standard, but sometimes needed.\n");  */
    printf("\n");
    printf("-a0        Use sections with ABS,OVR for their own local symbol definitions.\n");
    printf("-nodev     Ignore 'ddn:' style device names in .INCLUDE and .LIBRARY filenames.\n");
    printf("-nodir     Ignore '[directory]' style names in .INCLUDE and .LIBRARY filenames.\n");
    printf("-rsx       Generate RSX style object files%s.\n",  (!rt11 ? " (default)" : ""));
    printf("-rt11      Generate RT-11 style object files%s.\n", (rt11 ? " (default)" : ""));
    printf("\n");
    printf("-m         load RSX-11 or RT-11 compatible macro library from which\n");
    printf("           .MCALLed macros can be found.  Similar to .LIBRARY directive.\n");
    printf("           Multiple allowed.  Affected by any -rsx or -rt11 which come before.\n");
    printf("-xl        as -xtract but stores macros to '-l' file. If none, only lists names.\n");
    printf("-xtract    invokes " PROGRAM_NAME " to expand the contents of the registered\n");
    printf("           macro libraries (see -m) into individual .MAC files in the\n");
    printf("           current directory.  No assembly of input is done.\n");
    printf("           This must be the last command line option!\n");
    printf("\n");

    printf("-relaxed   Relax some of the usual rules for source code.\n");
    printf("-strict    Expect source code to closely match MACRO-11 V05.05.\n");
    printf("-stringent Expect source code to closely match MACRO-11 documentation.\n");
    printf("\n");

    printf("-yd        Extension: enable debugging.\n");
    printf("-yfl       Extension: force listing of suppressed lines.\n");
    printf("-yl1       Extension: list the first pass too, not only the second.\n");
    printf("-ylls      Extension: list local symbols when used and in the symbol table.\n");
    printf("-ym11      Syntax extension: support BSD m11 language extensions.\n");
    printf("-ysl       Syntax extension: change length of symbols (from %d up to %d).\n", SYMMAX_DEFAULT, SYMMAX_MAX);
    printf("-yus       Syntax extension: allow underscore \"_\" in symbols.\n");
    printf("\n");
    printf("Options for -e, -d, and -dc are:\n");
    usage_dirargs();
    printf("\n");
}


/* usage - prints the usage message */
void usage(
    char *message)
{
    fputs(message, stderr);
    exit(EXIT_FAILURE);
}


/* prepare_pass - prepares for pass 1 or pass 2 */
void prepare_pass(
    int this_pass,
    STACK *stack,
    int nr_files,
    char **fnames,
    int  nr_slines,
    char **slines)
{
    int i;

    pass = this_pass;
    assert(pass == PASS1 || pass == PASS2);

    stack_init(stack);

    /* Push the files onto the input stream in reverse order */
    for (i = nr_files - 1; i >= 0; --i) {
        STREAM         *str = new_file_stream(fnames[i]);

        if (str == NULL) {
            if (pass == PASS1) {
            /* fprintf(stderr, "Unable to open source file %s\n", fnames[i]); */
                perror(fnames[i]);
            } else {
                report_fatal(NULL, "Unable to reopen source file %s\n", fnames[i]);
            }
            exit(EXIT_FAILURE);
        }
        stack_push(stack, str);
    }

    /* Push the -s lines onto the input stream */
    for (i = 0; i < nr_slines; i++) {
        inject_source("%s", slines[i]);
    }
    stack_injected_source(stack, "-s");

    if (enabl_debug || list_pass_0)
        fprintf(stderr, "******** Starting pass %d ********\n", pass + 1);

    if (list_pass_0 && lstfile && lstfile != stdout)
        fprintf(lstfile, "******** Starting pass %d ********\n\n", pass + 1);

    load_dirargs();
    if (enabl_debug && pass == PASS2)
        dump_dirargs("Start  pass 2: ");  /* Show the directive argunents before starting pass 2 */

    DOT = 0;
    current_pc->section = &blank_section;
    last_dot_section = NULL;

    if (toc_shown)
        list_page_top = 0;
    else
        list_page_top = 1;  /* TODO: If we implement true pagination, set this to 0 */

    line_on_page = 0;

    list_line_act = LIST_PAGE_BEFORE;
    report_errcnt = 0;

    stmtno = 0;
    radix = 8;
    lsb = 0;
    lsb_used = 0;
    next_lsb = 1;
    last_macro_lsb = -1;
    last_locsym = START_LOCSYM;
    suppressed = 0;
    last_cond = -1;
    sect_sp = -1;

    if (pass == PASS2 && !toc_shown)
        list_title_line(NULL);
    title_string[0] = '\0';

    if (enabl_debug) {
        if (lookup_sym(DEBUG_SYM_ERRCNT, &symbol_st) == NULL) {
            ADD_DEBUG_SYM(DEBUG_SYM_DLEVEL, enabl_debug);  /* const: Debug level (>0) */
            ADD_DEBUG_SYM(DEBUG_SYM_ERRCNT, 0);            /* var:   Error count (>=0) */
            ADD_DEBUG_SYM(DEBUG_SYM_STRICT, strictness);   /* const: -relaxed = <0, -strict = >0, else neither */
        } else {
            UPD_DEBUG_SYM(DEBUG_SYM_ERRCNT, report_errcnt);
        }

        /* Push the -yd lines onto the input stream */

        inject_source(DEBUG_SYM_DLEVEL "\t=:\t%d.\t\t; Debugging enabled at level %d\n",
                      enabl_debug, enabl_debug);

        inject_source(DEBUG_SYM_ERRCNT "\t=:\t%d.\t\t; Reported error count (updated for errors)\n", 0);

        if (strictness == STRINGENTNESS)
            inject_source(DEBUG_SYM_STRICT "\t=:\t%d.\t\t; -stringent\n", strictness);
        else
            inject_source(DEBUG_SYM_STRICT "\t=:\t%d.\t\t; -strict*%d, -relaxed*%d\n",
                          strictness, (strictness > 0) ? +strictness : 0,
                                      (strictness < 0) ? -strictness : 0);

        stack_injected_source(stack, "-yd");
    }
}


/* main - main program */

#define MAX_FILES 32    /* Maximum number of input (source) files allowed on the command line */
#define MAX_SLINES 32   /* Maximum number of '-s' source statements allowed */

int main(
    int argc,
    char *argv[])
{
    char           *fnames[MAX_FILES];
    char           *slines[MAX_SLINES];
    int             nr_files = 0;
    int             nr_slines = 0;
    FILE           *obj = NULL;
    TEXT_RLD        tr;
    char           *objname = NULL;
    char           *lstname = NULL;
    int             arg;
    int             i;
    STACK           stack;
    int             errcount = 0;
    int             report_errcnt_p1 = 0;
    int             list_version = enabl_debug;
    int             strict  = 0;
    int             relaxed = 0;
    int             execute_p1 = TRUE;
    int             execute_p2 = TRUE;

    add_dirargs();

    if (argc <= 1) {
        print_help();
        return /* exit */ EXIT_FAILURE;
    }

    for (arg = 1; arg < argc; arg++) {
        if (*argv[arg] == '-') {
            char           *cp;

            cp = argv[arg] + 1;

            if (!strcasecmp(cp, "h")) {
                print_help();
            } else if (!strcasecmp(cp, "v")) {
                list_version = enabl_debug;    /* -yd before -v will LIST the version too */
                print_version(stderr);
            } else if (!strcasecmp(cp, "apb")) {
                auto_page_break = TRUE;
            } else if (!strcasecmp(cp, "ff")) {
                page_break_ff = TRUE;
            } else if (!strcasecmp(cp, "p1")) {
                 execute_p1 = TRUE;
                 execute_p2 = FALSE;
            } else if (!strcasecmp(cp, "p2")) {
                 execute_p1 = FALSE;
                 execute_p2 = TRUE;
            } else if (!strcasecmp(cp, "fe")) {
                exit_if_pass = PASS1;  /* or PASS2 */
            } else if (!strcasecmp(cp, "fe2")) {
                exit_if_pass = PASS2;
            } else if (!strcasecmp(cp, "se")) {
                show_error_lines = TRUE;
            } else if (!strcasecmp(cp, "sp")) {
                show_print_lines = 1;
            } else if (!strcasecmp(cp, "e")) {
                /* Followed by option(s) to enable */
                /* Since .LIST (/SHOW) and .ENABL (/ENABL) argument names don't overlap,
                   I consolidate. */
                if(arg >= argc-1 || !isalpha((unsigned char)*argv[arg+1])) {
                    usage("-e must be followed by an option to enable\n");
                }
                upcase(argv[++arg]);
                enable_tf(argv[arg], ARGS_ENABL, ARGS_NO_FLAGS);
            } else if (!strcasecmp(cp, "d")) {
                /* Followed by option(s) to disable */
                if(arg >= argc-1 || !isalpha((unsigned char)*argv[arg+1])) {
                    usage("-d must be followed by an option to disable\n");
                }
                upcase(argv[++arg]);
                enable_tf(argv[arg], ARGS_DSABL, ARGS_NO_FLAGS);
            } else if (!strcasecmp(cp, "dc")) {
                /* Followed by option(s) to disable changes */
                if(arg >= argc-1 || !isalpha((unsigned char)*argv[arg+1])) {
                    usage("-dc must be followed by an option to disable changes\n");
                }
                upcase(argv[++arg]);
                enable_tf(argv[arg], -1, ARGS_IGNORE_THIS);
            } else if (!strcasecmp(cp, "m")) {
                /* Macro library */
                /* This option gives the name of an RT-11 or RSX compatible
                 * macro library from which .MCALLed macros can be found.
                 */
                if(arg >= argc-1 || *argv[arg+1] == '-') {
                    usage("-m must be followed by a macro library file name\n");
                }
                arg++;
                if (nr_mlbs < MAX_MLBS) {
                    int allow_olb = (/* strcmp(argv[argc-1], "-x") == */ 0);  /* -x before -m is not supported (yet) */

                    mlbs[nr_mlbs] = mlb_open(argv[arg], allow_olb);
                }
                if (nr_mlbs >= MAX_MLBS || mlbs[nr_mlbs] == NULL) {
                    fprintf(stderr, "Unable to register macro library %s\n", argv[arg]);
                    return /* exit */ EXIT_FAILURE;
                }
                nr_mlbs++;
            } else if (!strcasecmp(cp, "p")) {
                /* P for search path */
                /* The -p option gives the name of a directory in
                   which .MCALLed macros may be found.  */  {

                    if(arg >= argc-1 || *argv[arg+1] == '-') {
                        usage("-p must be followed by a macro search directory\n");
                    }

                    append_env("MCALL", argv[arg+1]);
                    arg++;
                }
            } else if (!strcasecmp(cp, "I")) {
                /* I for include path */
                /* The -I option gives the name of a directory in
                   which .included files may be found.  */  {

                    if(arg >= argc-1 || *argv[arg+1] == '-') {
                        usage("-I must be followed by a include file search directory\n");
                    }
                    append_env("INCLUDE", argv[arg+1]);

                    arg++;
                }
            } else if (!strcasecmp(cp, "s")) {
                /* The -s option defines a statement to be prepended to the source */
                if(arg >= argc-1 || *argv[arg+1] == '-') {
                    usage("-s must be followed by a valid MACRO-11 statement\n");
                }
                if (nr_slines >= MAX_SLINES) {
                    fprintf(stderr, "Too many -s lines (max %d)\n", MAX_SLINES);
                    return /* exit */ EXIT_FAILURE;
                }
                slines[nr_slines++] = argv[++arg];
            } else if (!strcasecmp(cp, "o")) {
                /* The -o option gives the object file name (.OBJ) */
                if (objname)
                    usage("-o is only allowed once\n");
                if(arg >= argc-1 || *argv[arg+1] == '-') {
                    usage("-o must be followed by the object file name\n");
                }
                ++arg;
                objname = argv[arg];
            } else if (!strcasecmp(cp, "l")) {
                /* The option -l gives the listing file name (.LST) */
                /* -l - enables listing to stdout. */
                if (lstname)
                    usage("-l is only allowed once\n");
                if (arg >= argc-1 ||
                        (argv[arg+1][0] == '-' && argv[arg+1][1] != '\0')) {
                    usage("-l must be followed by the listing file name (- for standard output)\n");
                }
                lstname = argv[++arg];
                if (strcmp(lstname, "-") == 0)
                    lstfile = stdout;
                else
                    lstfile = fopen(lstname, "w");
                if (lstfile == NULL) {
                    fprintf(stderr, "Unable to create list file %s\n", lstname);
                    return /* exit */ EXIT_FAILURE;
                }
            } else if (!strcasecmp(cp, "xtract")) {
                /* The -xtract option invokes macro11 to expand the
                 * contents of the registered macro libraries (see -m)
                 * into individual .MAC files in the current
                 * directory.  No assembly of input is done.  This
                 * must be the last command line option (and no source code).
                 */
                int             m;

                if(arg != argc-1 || nr_files + nr_slines != 0) {
                    usage("-xtract must be the last option\n");
                }
                if (nr_mlbs > 0) {
                    for (m = 0; m < nr_mlbs; m++)
                        mlb_extract(mlbs[m]);
                    return /* exit */ EXIT_SUCCESS;
                }
            } else if (!strcasecmp(cp, "xl")) {
                /* The -xl option is like -xtract except all files are
                 * expanded to the listing-file.  If no listing-file
                 * is provided, the macro names will be listed to stdout.
                 * -xl will be ignored if no -m files are provided.
                 * -xl will allows no source code and will exit in that case.
                 */
                if (nr_mlbs > 0) {
                    int             m;

                    for (m = 0; m < nr_mlbs; m++) {
                        if (lstfile == NULL) {
                            printf("%s:-\n", mlbs[m]->name);  /* Heading is the filename */
                        }
                        mlb_list(mlbs[m], lstfile);           /* List the macro names or contents */
                    }
                    if (arg == argc-1)
                        if (nr_files + nr_slines == 0)
                            return /* exit */ EXIT_SUCCESS;   /* Allow -xl to be the last thing on the command line */
                }
            } else if (!strcasecmp(cp, "ym11")) {
                   support_m11 = 1;
            } else if (!strcasecmp(cp, "ysl")) {
                /* set symbol_len */
                if (arg >= argc-1) {
                    usage("-ysl must be followed by a number\n");
                } else {
                    char           *s = argv[++arg];
                    char           *endp;
                    int             sl = strtol(s, &endp, 10);

                    if (*endp || sl < SYMMAX_DEFAULT || sl > SYMMAX_MAX) {
                        usage("-ysl must be followed by a valid symbol length\n");
                    }
                    symbol_len = sl;
                }
            } else if (!strcasecmp(cp, "yus")) {
                /* allow underscores */
                symbol_allow_underscores = 1;
                rad50_enable_underscore();
            } else if (!strcasecmp(cp, "yfl")) {
                listing_forced = LIST_FORCE_LISTING;
            } else if (!strcasecmp(cp, "ylls")) {
                symbol_list_locals = 1;
            } else if (!strcasecmp(cp, "yl1")) {
                /* list the first pass, in addition to the second */
                list_pass_0++;  /* Repeat -yl1 to also show report_xxx() errors during pass 1 */
            } else if (!strcasecmp(cp, "yd")) {
                enabl_debug++;  /* Repeat -yd to increase the debug level */
            } else if (!strcasecmp(cp, "a0")) {
                abs_0_based = 1;
            } else if (!strcasecmp(cp, "nodev")) {
                ignore_fn_dev = 1;
            } else if (!strcasecmp(cp, "nodir")) {
                ignore_fn_dir = 1;
            } else if (!strcasecmp(cp, "rt11")) {
                rt11 = 1;
            } else if (!strcasecmp(cp, "rsx")) {
                rt11 = 0;
            } else if (!strcasecmp(cp, "stringent")) {
                strict = STRINGENTNESS, relaxed = 0;
                strictness = strict - relaxed;
            } else if (!strcasecmp(cp, "strict")) {
                strict++, relaxed = 0;
                strictness = strict - relaxed;
            } else if (!strcasecmp(cp, "relaxed")) {
                relaxed++, strict = 0;
                strictness = strict - relaxed;
            } else {
                fprintf(stderr, "Unknown option %s\n", argv[arg]);
                print_help();
                return /* exit */ EXIT_FAILURE;
            }
        } else {
            if (nr_files >= MAX_FILES) {
                fprintf(stderr, "Too many files (max %d)\n", MAX_FILES);
                return /* exit */ EXIT_FAILURE;
            }
            fnames[nr_files++] = argv[arg];
        }
    }

    if (objname) {
        obj = fopen(objname, "wb");
        if (obj == NULL) {
            fprintf(stderr, "Unable to create object file %s\n", objname);
            return /* exit */ EXIT_FAILURE;
        }
    }

    if (nr_files + nr_slines == 0) {
        fprintf(stderr, "No source files provided\n");
        return /* exit */ EXIT_FAILURE;
    }

    if (enabl_debug && (!lstfile || lstfile != stdout))
        fprintf(stderr, "Debugging enabled (level = %d)\n", enabl_debug);

#if NODO
    /* RSX blank PSECT = ". BLK. is documented but not true ...   */
    /* ... actually, TKB renames the blank ("") PSECT to ". BLK." */
    if (!rt11) blank_section.label = ". BLK.";
#endif

    add_symbols(&blank_section);
    module_name = memcheck(strdup(".MAIN."));
    if (!STRICTEST)
        xfer_address = new_ex_lit(1);      /* The undefined transfer address */

    if (list_version && lstfile && lstfile != stdout)
        print_version(lstfile);

    text_init(&tr, NULL, 0);

    if (execute_p1) {
        prepare_pass(PASS1, &stack, nr_files, fnames, nr_slines, slines);
        assemble_stack(&stack, &tr);
        report_errcnt_p1 = report_errcnt;
        assert(stack.top == NULL);

        if (list_pass_0 && lstfile)
            list_symbol_table();

#if DEBUG_MACROS
        if (enabl_debug > 1)
            dump_all_macros();
#endif

    }

    migrate_implicit();                /* Migrate the implicit globals */
    write_globals(obj);                /* Write the global symbol dictionary */

#if NODO
    sym_hist(&symbol_st, "symbol_st"); /* Draw a symbol table histogram */
#endif

    if (enabl_debug && list_pass_0 && pass == PASS2)
        if (execute_p1)
            dump_dirargs("End of pass 1: ");  /* Show the directive arguments at the end of pass 1 */

    pop_cond(-1);
    text_init(&tr, obj, 0);

    if (execute_p2) {
        prepare_pass(PASS2, &stack, nr_files, fnames, nr_slines, slines);
        errcount = assemble_stack(&stack, &tr);
        assert(stack.top == NULL);
    }

    text_flush(&tr);

    if (STRICTEST && xfer_address == NULL) {
        report_warn(NULL, ".END was not supplied\n");
        errcount++;
    }

    while (last_cond >= 0) {
        report_err(stream_here(conds[last_cond].file, conds[last_cond].line),
                   "Unterminated conditional (here)\n");
        pop_cond(last_cond - 1);
        errcount++;
    }

    for (i = 0; i < nr_mlbs; i++)
        mlb_close(mlbs[i]);

    write_endmod(obj);

    if (obj != NULL)
        fclose(obj);

    if (enabl_debug) {
        if (execute_p1 && execute_p2)
            dump_dirargs("End of pass 2: ");
        else
            dump_dirargs("End of pass:   ");
    }

    if (lstfile) {
        migrate_undefined();           /* Migrate the undefined symbols */
        list_symbol_table();
    }

    /* TODO: Write the error count(s) to the listing file */

    if (errcount > 0)
        fprintf(stderr, "%d Errors\n", errcount);
/* TODO:            ... "Errors detected:  %d\n", errcount); */

    if (list_pass_0) {
        if (report_errcnt_p1 != report_errcnt)
            fprintf(stderr, "%d Error%s %s during pass 1\n", report_errcnt_p1,
                    (report_errcnt_p1 == 1) ? " was" : "s were",
                    (list_pass_0 > 1) ? "reported" : "found");
        if (report_errcnt != errcount)
            fprintf(stderr, "%d Error%s reported during pass 2\n",
                    report_errcnt,
                    (report_errcnt == 1) ? " was" : "s were");
    } else if (enabl_debug) {
        if (report_errcnt != errcount)
            fprintf(stderr, "%d Error%s actually reported\n",
                    report_errcnt,
                    (report_errcnt == 1) ? " was" : "s were");
    }

/* TODO: Add this to the end of the listing file (?)

    if (!ENABL(BMK)) { ... }

    *** Assembler statistics


x   Work  file  reads: 0
x   Work  file writes: 0
x   Size of work file: 29 Words  ( 1 Pages)
x   Size of core pool: 9344 Words  ( 35 Pages)
    Operating  system: RSX-11M/M-PLUS

Elapsed time: 00:00:00.22

*/
    if (lstfile && strcmp(lstname, "-") != 0)
        fclose(lstfile);

    return /* exit */ errcount > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
