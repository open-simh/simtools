#define REPT_IRPC__C

/*
 *      .REPT
 *      .IRP
 *      .IRPC streams
 */

#include <stdlib.h>
#include <string.h>

#include "rept_irpc.h"                 /* my own definitions */

#include "assemble_aux.h"
#include "assemble_globals.h"
#include "listing.h"
#include "parse.h"
#include "macros.h"
#include "util.h"


/* *** implement REPT_STREAM */

typedef struct rept_stream{
    BUFFER_STREAM   bstr;
    int             count;      /* The current repeat countdown */
    int             savecond;   /* conditional stack level at time of
                                   expansion */
} REPT_STREAM;


/* rept_stream_getline gets a line from a repeat stream.  At the end of
 * each count, the coutdown is decreated and the stream is reset to
 * its beginning. */

char           *rept_stream_getline(
    STREAM *str)
{
    REPT_STREAM    *rstr = (REPT_STREAM *) str;
    char           *cp;

    for (;;) {
        if (rstr->count <= 0)
            return NULL;

        if ((cp = buffer_stream_getline(str)) != NULL)
            return cp;

        buffer_stream_rewind(str);
        rstr->count--;
    }
}


/* rept_stream_delete unwinds nested conditionals like .MEXIT does. */

void            rept_stream_delete(
    STREAM *str)
{
    REPT_STREAM    *rstr = (REPT_STREAM *) str;

    pop_cond(rstr->savecond);          /* complete unterminated
                                          conditionals */
    buffer_stream_delete(&rstr->bstr.stream);
}


/* The VTBL */

STREAM_VTBL     rept_stream_vtbl = {
    rept_stream_delete, rept_stream_getline, buffer_stream_rewind
};


/* expand_rept is called when a .REPT is encountered in the input. */

STREAM         *expand_rept(
    STACK *stack,
    char *cp)
{
    EX_TREE        *value;
    BUFFER         *gb;
    REPT_STREAM    *rstr;
    int             levelmod;

    value = parse_expr(cp, 0);
    if (value->type != EX_LIT) {
        report_err(stack->top, ".REPT value must be constant\n");
        free_tree(value);
        return NULL;
    }
    /*
     * Reading the next lines-to-be-repeated overwrites the line buffer
     * that the caller is using. So for junk-at-end-of-line checking we
     * need to do it here.
     */
    check_eol(stack, value->cp);

    list_value(stack->top, value->data.lit);

    gb = new_buffer();

    levelmod = 0;
    if (!LIST(MD)) {
        list_level--;
        levelmod = 1;
    }

    read_body(stack, gb, ".REPT", FALSE);

    list_level += levelmod;

    rstr = memcheck(malloc(sizeof(REPT_STREAM))); {
        char           *name = memcheck(malloc(strlen(stack->top->name) + 32));

        sprintf(name, "%s:%d->.REPT", stack->top->name, stack->top->line);
        buffer_stream_construct(&rstr->bstr, gb, name);
        free(name);
    }

    rstr->count = (value->data.lit & 0x8000) ? 0 : value->data.lit;  /* Negative count repeats zero times */
    rstr->bstr.stream.vtbl = &rept_stream_vtbl;
    rstr->savecond = last_cond;

    buffer_free(gb);
    free_tree(value);

    return &rstr->bstr.stream;
}


/* *** implement IRP_STREAM */

typedef struct irp_stream {
    BUFFER_STREAM   bstr;
    char           *label;      /* The substitution label */
    char           *items;      /* The substitution items (in source code
                                   format) */
    int             offset;     /* Current offset into "items" */
    BUFFER         *body;       /* Original body */
    int             savecond;   /* Saved conditional level */
} IRP_STREAM;


/* irp_stream_getline expands the IRP as the stream is read. */
/* Each time an iteration is exhausted, the next iteration is
 * generated. */

char           *irp_stream_getline(
    STREAM *str)
{
    IRP_STREAM     *istr = (IRP_STREAM *) str;
    char           *cp;
    BUFFER         *buf;
    ARG            *arg;

    for (;;) {
        if ((cp = buffer_stream_getline(str)) != NULL)
            return cp;

        cp = istr->items + istr->offset;

        if (!*cp)
            return NULL;               /* No more items.  EOF. */

        arg = new_arg();
        arg->next = NULL;
        arg->locsym = 0;
        arg->label = istr->label;
        arg->value = getstring_macarg(str, cp, &cp);
        cp = skipdelim(cp);
        istr->offset = (int) (cp - istr->items);

        buf = subst_args(istr->body, arg);

        free(arg->value);
        free(arg);
        buffer_stream_set_buffer(&istr->bstr, buf);
        buffer_free(buf);
    }
}


/* irp_stream_delete - also pops the conditional stack */

void irp_stream_delete(
    STREAM *str)
{
    IRP_STREAM     *istr = (IRP_STREAM *) str;

    pop_cond(istr->savecond);          /* complete unterminated
                                          conditionals */

    buffer_free(istr->body);
    free(istr->items);
    free(istr->label);
    buffer_stream_delete(str);
}


STREAM_VTBL     irp_stream_vtbl = {
    irp_stream_delete, irp_stream_getline, buffer_stream_rewind
};


/* We occasionally see .IRP with the formal name in angle brackets.  I
 * have no idea why, but it appears to be legal.  So allow that.  Not
 * sure if it should be allowed elsewhere, e.g., in .MACRO.  For now,
 * don't.  */

static char *get_irp_sym (char *cp, char **endcp, int *islocal)
{
    char *ret = NULL;

    cp = skipwhite(cp);
    if (*cp == '<') {
        ret = get_symbol (cp + 1, &cp, islocal);
        if (*cp++ != '>') {
            *endcp = cp;
            return NULL;
        }
    }
    else {
        ret = get_symbol (cp, &cp, islocal);
    }
    *endcp = cp;
    return ret;
}


/* expand_irp is called when a .IRP is encountered in the input. */

STREAM         *expand_irp(
    STACK *stack,
    char *cp)
{
    char           *label,
                   *items;
    BUFFER         *gb;
    int             levelmod = 0;
    IRP_STREAM     *str;
    int             found_comma;

    label = get_irp_sym(cp, &cp, NULL);
    if (!label) {
        report_err(stack->top, "Invalid .IRP syntax (symbol or <symbol> expected)\n");
        return NULL;
    }

    cp = skipdelim_comma(cp, &found_comma);
    if (STRINGENT) {
        if (*cp != '<')
            report_warn(stack->top, ".IRP strictly needs '< ... >'\n");
    } else {
        if (STRICT)
            if (!found_comma && EOL(*cp))
                report_warn(stack->top, ".IRP strictly needs a parameter\n");
    }

    items = getstring(cp, &cp);
    if (!items) {
        report_err(stack->top, "Invalid .IRP syntax (string expected)\n");
        free(label);
        return NULL;
    }

    check_eol(stack, cp);

    gb = new_buffer();

    levelmod = 0;
    if (!LIST(MD)) {
        list_level--;
        levelmod++;
    }

    read_body(stack, gb, ".IRP", FALSE);

    list_level += levelmod;

    str = memcheck(malloc(sizeof(IRP_STREAM))); {
        char           *name = memcheck(malloc(strlen(stack->top->name) + 32));

        sprintf(name, "%s:%d->.IRP", stack->top->name, stack->top->line);
        buffer_stream_construct(&str->bstr, NULL, name);
        free(name);
    }

    str->bstr.stream.vtbl = &irp_stream_vtbl;

    str->body = gb;
    str->items = items;
    str->offset = 0;
    str->label = label;
    str->savecond = last_cond;

    return &str->bstr.stream;
}


/* *** implement IRPC_STREAM */

typedef struct irpc_stream {
    BUFFER_STREAM   bstr;
    char           *label;      /* The substitution label */
    char           *items;      /* The substitution items (in source code
                                   format) */
    int             offset;     /* Current offset in "items" */
    BUFFER         *body;       /* Original body */
    int             savecond;   /* conditional stack at invocation */
} IRPC_STREAM;


/* irpc_stream_getline - same comments apply as with irp_stream_getline, but
   the substitution is character-by-character */

char           *irpc_stream_getline(
    STREAM *str)
{
    IRPC_STREAM    *istr = (IRPC_STREAM *) str;
    char           *cp;
    BUFFER         *buf;
    ARG            *arg;

    for (;;) {
        if ((cp = buffer_stream_getline(str)) != NULL)
            return cp;

        cp = istr->items + istr->offset;

        if (!*cp)
            return NULL;               /* No more items.  EOF. */

        arg = new_arg();
        arg->next = NULL;
        arg->locsym = 0;
        arg->label = istr->label;
        arg->value = memcheck(malloc(2));
        arg->value[0] = *cp++;
        arg->value[1] = 0;
        istr->offset = (int) (cp - istr->items);

        buf = subst_args(istr->body, arg);

        free(arg->value);
        free(arg);
        buffer_stream_set_buffer(&istr->bstr, buf);
        buffer_free(buf);
    }
}


/* irpc_stream_delete - also pops contidionals */

void irpc_stream_delete(
    STREAM *str)
{
    IRPC_STREAM    *istr = (IRPC_STREAM *) str;

    pop_cond(istr->savecond);          /* complete unterminated
                                          conditionals */
    buffer_free(istr->body);
    free(istr->items);
    free(istr->label);
    buffer_stream_delete(str);
}


STREAM_VTBL     irpc_stream_vtbl = {
    irpc_stream_delete, irpc_stream_getline, buffer_stream_rewind
};


/* expand_irpc - called when .IRPC is encountered in the input */

STREAM         *expand_irpc(
    STACK *stack,
    char *cp)
{
    char           *label,
                   *items;
    BUFFER         *gb;
    int             levelmod = 0;
    IRPC_STREAM    *str;
    int             len;
    int             found_comma;

    label = get_irp_sym(cp, &cp, NULL);
    if (!label) {
        report_err(stack->top, "Invalid .IRPC syntax (symbol or <symbol> expected)\n");
        return NULL;
    }

    cp = skipdelim_comma(cp, &found_comma);
    if (STRINGENT) {
        if (*cp != '<')
            report_warn(stack->top, ".IRPC recommends using '< ... >'\n");
    } else {
        if (STRICT)
            if (!found_comma && EOL(*cp))
                report_warn(stack->top, ".IRPC strictly needs a parameter\n");
    }

    items = getstring(cp, &cp);
    if (!items) {
        report_err(stack->top, "Invalid .IRPC syntax (string expected)\n");
        free(label);
        return NULL;
    }

    len = strlen(items);
    list_value(stack->top, len);

    if (STRINGENT) {
        if (len > 124 /* DOC: J.1.1 */)
            report_warn(stack->top, ".IRPC string length of %d is longer than the allowed 124\n", len);
    }

    check_eol(stack, cp);

    gb = new_buffer();

    levelmod = 0;
    if (!LIST(MD)) {
        list_level--;
        levelmod++;
    }

    read_body(stack, gb, ".IRPC", FALSE);

    list_level += levelmod;

    str = memcheck(malloc(sizeof(IRPC_STREAM))); {
        char           *name = memcheck(malloc(strlen(stack->top->name) + 32));

        sprintf(name, "%s:%d->.IRPC", stack->top->name, stack->top->line);
        buffer_stream_construct(&str->bstr, NULL, name);
        free(name);
    }

    str->bstr.stream.vtbl = &irpc_stream_vtbl;
    str->body = gb;
    str->items = items;
    str->offset = 0;
    str->label = label;
    str->savecond = last_cond;

    return &str->bstr.stream;
}
