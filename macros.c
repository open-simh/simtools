#define MACROS__C

/*
 * Dealing with MACROs
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"                    /* My own definitions */

#include "assemble_aux.h"
#include "assemble_globals.h"
#include "listing.h"
#include "parse.h"
#include "rept_irpc.h"
#include "stream2.h"
#include "symbols.h"
#include "util.h"


/* Here's where I pretend I'm a C++ compiler. */

/* *** derive a MACRO_STREAM from a BUFFER_STREAM with a few other args */

/* macro_stream_delete is called when a macro expansion is
 * exhausted.  The unique behavior is to unwind any stacked
 * conditionals.  This allows a nested .MEXIT to work.  */

void macro_stream_delete(
    STREAM *str)
{
    MACRO_STREAM   *mstr = (MACRO_STREAM *) str;

    pop_cond(mstr->cond);
    buffer_stream_delete(str);
}


STREAM_VTBL     macro_stream_vtbl = {
    macro_stream_delete, buffer_stream_getline, buffer_stream_rewind
};


STREAM         *new_macro_stream(
    STREAM *refstr,
    BUFFER *buf,
    MACRO *mac,
    int nargs)
{
    MACRO_STREAM   *mstr = memcheck(malloc(sizeof(MACRO_STREAM)));
    {
        char           *name = memcheck(malloc(strlen(refstr->name) + SYMMAX_MAX + 10));

        sprintf(name, "%s:%d->%s", refstr->name, refstr->line, mac->sym.label);
        buffer_stream_construct(&mstr->bstr, buf, name);
        free(name);
    }

    mstr->bstr.stream.vtbl = &macro_stream_vtbl;
    mstr->nargs = nargs;  /* for .NARG directive: nonkeyword arguments in call*/
    mstr->cond = last_cond;
    return &mstr->bstr.stream;
}


/* read_body fetches the body of .MACRO, .REPT, .IRP, or .IRPC into a BUFFER. */

void read_body(
    STACK *stack,
    BUFFER *gb,
    char *name,
    int called)
{
    int             nest;
    char           *mac_name = memcheck(strdup(stack->top->name));
    int             mac_line = stack->top->line;

    /* Read the stream in until the end marker is hit */

    /* Note: "called" says that this body is being pulled from a macro
       library, and so under no circumstance should it be listed. */

    nest = 1;
    for (;;) {
        SYMBOL         *op;
        char           *nextline;
        char           *cp;

        if (!(called & CALLED_NOLIST) &&
                (list_level + LIST(MD)) > 0) {
            list_flush();
        }

        nextline = stack_getline(stack);  /* Now read the line */
        if (nextline == NULL) {        /* End of file. */
            report_fatal(stream_here(mac_name, mac_line), "Body of '%s' (here) has missing .ENDM\n", name);
#if TODO
            free(mac_name);
            return;
#else
            exit(EXIT_FAILURE);  /* TODO: remove this when .IRP/.IRPC/.REPT are not "fatal" */
#endif
        }

        if (!(called & CALLED_NOLIST) &&
                (list_level + LIST(MD)) > 0) {
            list_source(stack->top, nextline);
        }

        op = get_op(nextline, &cp);

        if (op == NULL) {              /* Not a pseudo-op */
            buffer_append_line(gb, nextline);
            continue;
        }

        if (op->section->type == SECTION_PSEUDO) {
            if (op->value == P_MACRO || op->value == P_REPT || op->value == P_IRP || op->value == P_IRPC) {
                nest++;
            }

            if (op->value == P_END) {
                if (STRICT) {
                    report_warn(stack->top, ".END seen in body of %s\n", name);
                /*  TODO: Would be nice to 'errcnt++' or similar  */
                }
            }

            if (op->value == P_ENDM || op->value == P_ENDR) {
                nest--;
                /* If there's a name on the .ENDM, then
                 * check if the name matches the one on .MACRO.
                 * See page 7-3.
                 * Since we don't keep a stack of nested
                 * .MACRO names, just check for the outer one.
                 */

                cp = skipwhite(cp);
                if (!EOL(*cp)) {
                    if (op->value == P_ENDR) {
                        if (STRICT)
                            check_eol(stack, cp);
                    } else {
                        char           *label = get_symbol(cp, &cp, NULL);

                        if (label) {
                            if (nest == 0 && name /* && op->value == P_ENDM */) {
                                if (strcmp(label, name) != 0) {
                                    report_err(stack->top, ".ENDM '%s' does not match .MACRO '%s'\n", label, name);
                                }
                            }
                            free(label);
                        } else {
                            if (STRICT)
                                check_eol(stack, cp);
                        }
                    }
                }
            }

            if (nest == 0) {
                free(mac_name);
                return;                /* All done. */
            }
        }
        buffer_append_line(gb, nextline);
    }
    /* NOT REACHED */
}


/* Diagnostic: dumpmacro dumps a macro definition to stdout.
   I used this for debugging; it's not called at all right now, but
   I hate to delete good code. */

#if DEBUG_MACROS
void dumpmacro(
    MACRO *mac,
    FILE *fp)
{
    ARG            *arg;

    fprintf(fp, ".MACRO %s ", mac->sym.label);

    for (arg = mac->args; arg != NULL; arg = arg->next) {
        fputs(arg->label, fp);
        if (arg->value) {
            fputc('=', fp);
            fputs(arg->value, fp);
        }
        fputc(' ', fp);
    }
    fputc('\n', fp);

    fputs(mac->text->buffer, fp);

    fputs(".ENDM\n", fp);
}
#endif


/* defmacro - define a macro. */
/* Also used by .MCALL to pull macro definitions from macro libraries */

MACRO          *defmacro(
    char *cp,
    STACK *stack,
    int called)
{
    MACRO          *mac;
    ARG            *arg,
                  **argtail;
    char           *label;

    cp = skipwhite(cp);

    if (*cp == '.' && !issym((unsigned char) cp[1]))
        label = NULL;
    else
        label = get_symbol(cp, &cp, NULL);

    if (label == NULL) {
        report_err(stack->top, "Invalid .MACRO definition\n");
        return NULL;
    }

    if (!(called & CALLED_NODEFINE)) {
        /* Allow redefinition of a macro; new definition replaces the old. */
        mac = (MACRO *) lookup_sym(label, &macro_st);
        if (mac) {
            /* Remove from the symbol table... */
            remove_sym(&mac->sym, &macro_st);
            free_macro(mac);
        }
    }

    mac = new_macro(label);

    if (!(called & CALLED_NODEFINE)) {
        add_table(&mac->sym, &macro_st);
    }

    argtail = &mac->args;
    cp = skipdelim(cp);

    while (!EOL(*cp)) {
        arg = new_arg();
        arg->locsym = *cp == '?';
        if (arg->locsym) /* special argument flag? */
            cp++;
        arg->label = get_symbol(cp, &cp, NULL);
        if (arg->label == NULL) {
            /* It turns out that I have code which is badly formatted
             * but which MACRO.SAV assembles.  Sigh.  */
            if (STRICT) {
                report_warn(stack->top, "Invalid .MACRO argument\n");
#if NODO
                remove_sym(&mac->sym, &macro_st);
                free_macro(mac);
                return NULL;
#endif
            }
            break;  /* So, just quit defining arguments. */
        }

        /* DOC: 7.1.1 - check for duplicate dummy argument names */
        {
            ARG *argp = mac->args;

            while (argp != NULL) {
                if (strcmp(arg->label, argp->label) == 0) {
                    report_err(stack->top, "Duplicate .MACRO argument '%s'\n", arg->label);
                    return NULL;
                }
                argp = argp->next;
            }
        }

        cp = skipwhite(cp);
        if (*cp == '=') {
            /* Default substitution given */
            arg->value = getstring(cp + 1, &cp);
            if (arg->value == NULL) {
                report_err(stack->top, "Invalid .MACRO argument\n");
                remove_sym(&mac->sym, &macro_st);
                free_macro(mac);
                return NULL;
            }
        }

        /* Append to list of arguments */
        arg->next = NULL;
        *argtail = arg;
        argtail = &arg->next;

        cp = skipdelim(cp);
    }

    /* Read the stream in until the end marker is hit */
    {
        BUFFER         *gb;
        int             levelmod = 0;

        gb = new_buffer();

        if ((called & CALLED_NOLIST) && !LIST(MD)) {
            list_level--;
            levelmod = 1;
        }

        read_body(stack, gb, mac->sym.label, called);

        list_level += levelmod;

        if (mac->text != NULL)         /* Discard old macro body */
            buffer_free(mac->text);

        mac->text = gb;
    }

    return mac;
}


/* Allocate a new ARG */

ARG            *new_arg(
    void)
{
    ARG            *arg = memcheck(malloc(sizeof(ARG)));

    arg->locsym = 0;
    arg->value = NULL;
    arg->next = NULL;
    arg->label = NULL;
    return arg;
}


/* Free a list of args (as for a macro, or a macro expansion) */

static void free_args(
    ARG *arg)
{
    ARG            *next;

    while (arg) {
        next = arg->next;
        if (arg->label) {
            free(arg->label);
            arg->label = NULL;
        }
        if (arg->value) {
            free(arg->value);
            arg->value = NULL;
        }
        free(arg);
        arg = next;
    }
}


/* find_arg - looks for an arg with the given name in the given
   argument list */

static ARG     *find_arg(
    ARG *arg,
    char *name)
{
    for (; arg != NULL; arg = arg->next)
        if (strcmp(arg->label, name) == 0)
            return arg;

    return NULL;
}


/* subst_args - given a BUFFER and a list of args, generate a new
   BUFFER with argument replacement having taken place. */

BUFFER         *subst_args(
    BUFFER *text,
    ARG *args)
{
    char           *in;
    char           *begin;
    BUFFER         *gb;
    char           *label;
    ARG            *arg;

    gb = new_buffer();

    /* Blindly look for argument symbols in the input. */
    /* Don't worry about quotes or comments. */

    for (begin = in = text->buffer; in < text->buffer + text->length;) {
        char           *next;

        if (issym((unsigned char)*in)) {
            label = get_symbol(in, &next, NULL);
            if (label) {
                arg = find_arg(args, label);
                if (arg) {
                    /* An apostrophe may appear before or after the symbol. */
                    /* In either case, remove it from the expansion. */

                    if (in > begin && in[-1] == '\'')
                        in--;          /* Don't copy it. */
                    if (*next == '\'')
                        next++;

                    /* Copy prior characters */
                    buffer_appendn(gb, begin, (int) (in - begin));
                    /* Copy replacement string */
                    buffer_append_line(gb, arg->value);
                    in = begin = next;
                    --in;              /* prepare for subsequent increment */
                }
                free(label);
                in = next;
            } else
                in++;
        } else
            in++;
    }

    /* Append the rest of the text */
    buffer_appendn(gb, begin, (int) (in - begin));

    return gb;                         /* Done. */
}


/* eval_str - the language allows an argument expression to be given
   as "\expression" which means, evaluate the expression and
   substitute the numeric value in the current radix. */

char *eval_str(
    STREAM *refstr,
    char *arg)
{
    EX_TREE        *value = parse_expr(arg, 0);
    unsigned        word = 0;
    char            temp[10];

    if (value->type != EX_LIT) {
        report_err(refstr, "Constant value required\n");
    } else
        word = value->data.lit;

    free_tree(value);

    /* printf can't do base 2. */
    my_ultoa(word & 0177777, temp, radix);
    free(arg);
    arg = memcheck(strdup(temp));
    return arg;
}


/* getstring_macarg - parse a string that possibly starts with a backslash.
 * If so, performs expression evaluation.
 *
 * The current implementation over-accepts some input.
 *
 * MACRO V05.05:

        .list me
        .macro test x
        .blkb x
        .endm

        size = 10
        foo = 2

    ; likes:

        test size
        test \size
        test \<size>
        test \<size + foo>
        test ^/size + foo/

    ; dislikes:

        test <\size>          ; arg is \size which could be ok in other cases
        test size + foo       ; gets split at the space
        test /size + foo/     ; gets split at the space
        test \/size + foo/
        test \^/size + foo/   ; * accepted by this version

 */

char           *getstring_macarg(
    STREAM *refstr,
    char *cp,
    char **endp)
{
    if (cp[0] == '\\') {
        char *str = getstring(cp + 1, endp);
        return eval_str(refstr, str);         /* Perform expression evaluation */
    } else {
        return getstring(cp, endp);
    }
}


/* expandmacro - return a STREAM containing the expansion of a macro */

STREAM         *expandmacro(
    STREAM *refstr,
    MACRO *mac,
    char *cp)
{
    ARG            *arg,
                   *args,
                   *macarg;
    char           *label;
    STREAM         *str;
    BUFFER         *buf;
    int             nargs;
    int             onemore;

    args = NULL;
    arg = NULL;
    nargs = 0;      /* for the .NARG directive */
    onemore = 0;

    /* Parse the arguments */

    while (!EOL(*cp) || onemore) {
        char           *nextcp;

        /* Check for named argument */
        label = get_symbol(cp, &nextcp, NULL);
        macarg = NULL;
        if (label) {
            nextcp = skipwhite(nextcp);
            if (*nextcp == '=')
                macarg = find_arg(mac->args, label);
        }
        if (/* label && *nextcp && */ macarg) {
            /* Check if I've already got a value for it */
            if ((arg = find_arg(args, label)) != NULL) {
                /* Duplicate is legal; the last one wins. */
                if (arg->value) {
                    free (arg->value);
                    arg->value = NULL;
                }
            }
            else {
                arg = new_arg();
                arg->label = label;
                arg->next = args;
                args = arg;
            }
            nextcp = skipwhite(nextcp + 1);
            arg->value = getstring_macarg(refstr, nextcp, &nextcp);
        } else {
            if (label)
                free(label);

            /* Find correct positional argument */

            for (macarg = mac->args; macarg != NULL; macarg = macarg->next) {
                if (find_arg(args, macarg->label) == NULL)
                    break;             /* This is the next positional arg */
            }

            if (macarg == NULL)
                break;                 /* Don't pick up any more arguments. */

            nextcp = skipwhite (cp);
            arg = new_arg();
            arg->label = memcheck(strdup(macarg->label));       /* Copy the name */
            arg->next = args;
            args = arg;
            if (*nextcp != ',') {
                arg->value = getstring_macarg(refstr, cp, &nextcp);
            }
            else {
                arg->value = NULL;
            }
            nargs++;                   /* Count nonkeyword arguments only. */
        }

        /* If there is a trailing comma, there is an empty last argument */
        cp = skipdelim_comma(nextcp, &onemore);
    }

    /* Now go back and fill in defaults */  {
        int             locsym;

        if (last_macro_lsb != lsb)
            locsym = last_locsym /* = START_LOCSYM */;  /* MACRO-11 uses a continuous range of '?' local symbols */
        else
            locsym = last_locsym;
        last_macro_lsb = lsb;

        for (macarg = mac->args; macarg != NULL; macarg = macarg->next) {
            arg = find_arg(args, macarg->label);
            if (arg == NULL || arg->value == NULL) {
                int wasnull = 0;
                if (arg == NULL) {
                    wasnull = 1;
                    arg = new_arg();
                    arg->label = memcheck(strdup(macarg->label));
                }
                if (macarg->locsym) {
                    char            temp[32];

                    /* Here's where we generate local labels */
                    sprintf(temp, "%d$", locsym++);
                    arg->value = memcheck(strdup(temp));
                } else if (macarg->value) {
                    arg->value = memcheck(strdup(macarg->value));
                } else
                    arg->value = memcheck(strdup(""));

                if (wasnull) {
                    arg->next = args;
                    args = arg;
                }
            }
        }

        last_locsym = locsym;
    }

    buf = subst_args(mac->text, args);

    str = new_macro_stream(refstr, buf, mac, nargs);

    free_args(args);
    buffer_free(buf);

    return str;
}


/* dump_all_macros is a diagnostic function that's currently not
   used.  I used it while debugging, and I haven't removed it. */

#if DEBUG_MACROS
void dump_all_macros(
    void)
{
    MACRO          *mac;
    SYMBOL_ITER     iter;

    for (mac = (MACRO *) first_sym(&macro_st, &iter); mac != NULL; mac = (MACRO *) next_sym(&macro_st, &iter)) {
        dumpmacro(mac, lstfile);

        fprintf(lstfile, "\n\n");
    }
}
#endif


/* Allocate a new macro */

MACRO          *new_macro(
    char *label)
{
    MACRO          *mac = memcheck(malloc(sizeof(MACRO)));

    mac->sym.flags = SYMBOLFLAG_DEFINITION;
    mac->sym.label = label;
    mac->sym.stmtno = stmtno;
    mac->sym.next = NULL;
    mac->sym.section = &macro_section;
    mac->sym.value = 0;
    mac->args = NULL;
    mac->text = NULL;

    return mac;
}

/* free a macro, its args, its text, etc. */
void free_macro(
    MACRO *mac)
{
    if (mac->text) {
        free(mac->text);
    }
    free_args(mac->args);
    free_sym(&mac->sym);
}

#ifdef WIN32
#define stpncpy my_stpncpy
char *my_stpncpy(char *dest, const char *src, size_t n)
{
    size_t size = strnlen (src, n);

    memcpy(dest, src, size);
    dest += size;
    if (size == n)
        return dest;

    return memset(dest, '\0', n - size);
}
#endif


int do_mcall(
    char *label,
    STACK *stack)
{
    SYMBOL         *op;         /* The operation SYMBOL */
    STREAM         *macstr;
    BUFFER         *macbuf;
    char           *maccp;
    int             saveline;
    MACRO          *mac;
    int             i;
    char            macfile[FILENAME_MAX];
    char            hitfile[FILENAME_MAX];

    /* Find the macro in the list of included macro libraries (search LAST to FIRST) */
    macbuf = NULL;
    for (i = nr_mlbs - 1; i >= 0; i--)
        if ((macbuf = mlb_entry(mlbs[i], label)) != NULL)
            break;
    if (macbuf != NULL) {
        macstr = new_buffer_stream(macbuf, label);
        buffer_free(macbuf);
    } else {
        char           *bufend = &macfile[sizeof(macfile)];
        char           *end;

        end = stpncpy(macfile, label, sizeof(macfile) - 5);
        if (end >= bufend - 5) {
            report_err(stack->top, ".MCALL: name too long: '%s'\n", label);
            return 0;
        }
        stpncpy(end, ".MAC", bufend - end);
        my_searchenv(macfile, "MCALL", hitfile, sizeof(hitfile));
        if (hitfile[0])
            macstr = new_file_stream(hitfile);
        else
            macstr = NULL;
    }

    if (macstr != NULL) {
        for (;;) {
            char           *mlabel;

            maccp = macstr->vtbl->getline(macstr);
            if (maccp == NULL)
                break;
            mlabel = get_symbol(maccp, &maccp, NULL);
            if (mlabel == NULL)
                continue;
            op = lookup_sym(mlabel, &system_st);
            free(mlabel);
            if (op == NULL)
                continue;
            if (op->value == P_MACRO)
                break;
        }

        if (maccp != NULL) {
            STACK           macstack;
            int             savelist = list_level;

            macstack.top = macstr;
            saveline = stmtno;
            list_level = -1;
            mac = defmacro(maccp, &macstack, CALLED_NOLIST);
            if (mac == NULL) {
                report_err(stack->top, "Failed to define macro called %s\n", label);
            }

            stmtno = saveline;
            list_level = savelist;
        }

        macstr->vtbl->delete(macstr);
        return 1;
    }
    return 0;
}


/* within_macro_expansion - returns TRUE if 'str' is directly within a macro expansion (including .REPT, .IRP, .IRPC) */

int within_macro_expansion(
    STREAM *str)
{
    int             ok = ((str->vtbl == &macro_stream_vtbl) ||
                          (str->vtbl ==  &rept_stream_vtbl) ||
                          (str->vtbl ==   &irp_stream_vtbl) ||
                          (str->vtbl ==  &irpc_stream_vtbl));

    return ok;
}
