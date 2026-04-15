#define LISTING__C

/* Listing and error-message related. */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "listing.h"                /* My own definitions */

#include "assemble_globals.h"
#include "macro11.h"
#include "macros.h"
#include "object.h"
#include "util.h"


/* GLOBAL VARIABLES */

char            title_string[32] = "";   /* .TITLE string (up to 31 characters) */
int             toc_shown = 0;           /* Flags that at least one .SBTTL was listed in the TOC */

int             page_break_ff = 0;       /* TRUE if -ff (use form-feed for listing page-breaks */
int             auto_page_break = 0;     /* TRUE if -apb (automatic-page-break) selected */
int             line_on_page;            /* Current line number on the listing page */

static char    *list_page_fmt = "\n\n";  /* Format to use for the page throw */
static char    *list_page_ff  = "\f\n";  /* Format to use if page_break_ff is TRUE */

int             list_page_top;           /* Are we at the top of a page? */

int             list_line_act;           /* Action to perform when listing the current line */
int             listing_forced = 0;      /* If set to LIST_FORCE_LISTING most lines will be listed */

static int      list_within_exp;         /* Flags the listing line is DIRECTLY from a macro/rept/irp/irpc expansion */

static char     binline[sizeof(LSTFORMAT) + 16] = "";  /* For octal expansion */

static char    *listline;                /* Source lines */

FILE           *lstfile = NULL;          /* Listing file */

int             list_pass_0 = 0;         /* Also list what happens during the first pass */

int             report_errcnt = 0;       /* Count the number of times report() has been called */

int             show_error_lines = 0;    /* Show the line with the error when reporting errors */
int             show_print_lines = 0;    /* Show .PRINT lines (similar to show_error_lines) */

int             exit_if_pass = PASS2+1;  /* Exit if a fatal error occurs on this pass [or higher] */
int             exit_requested = 0;      /* Set to TRUE if a 'fatal' exit is requested  (-fe only) */

static int      errline = 0;             /* Set if current line has an error */


/* can_list returns TRUE if listing may happen for this line. */

static int can_list(
    void)
{
    int             ok = lstfile != NULL &&
                         (pass > PASS1 || list_pass_0);

    return ok;
}


/* build_list returns TRUE if the listing line might be used later. */

static int build_list(
    void)
{
    int             ok = can_list() ||
                         (show_error_lines && pass > PASS1) /* || exit_if_pass >= pass */;

    return ok;
}


/* dolist returns TRUE if listing is enabled. */

static int dolist(
    void)
{
    int             ok = can_list() &&
                         (list_level >= 0 || errline || (list_line_act & LIST_FORCE_LISTING));

    return ok;
}


/* list_source saves a text line for later listing by list_flush */

void list_source(
    STREAM *str,
    char *cp)
{
    if (build_list()) {
        int             len = strcspn(cp, "\n");

        /* Not an error yet */
        errline = 0;

        /* We might see a local symbol later on */
        looked_up_local_sym = FALSE;

        /* Unless -yfl, we don't want to force the line to be listed */
        list_line_act &= ~LIST_FORCE_LISTING;
        list_line_act |= listing_forced;

        /* Flag whether this listing line is within a macro/rept/irp/irpc expansion */
        list_within_exp = within_macro_expansion(str);

        /* Construct the LHS of the listing line */

#if NODO
        if (!binline)
            binline = memcheck(malloc(sizeof(LSTFORMAT) + 16));
#endif

        sprintf(binline, "%*s%*d ", (int)SIZEOF_MEMBER(LSTFORMAT, flag), "",
                                    (int)SIZEOF_MEMBER(LSTFORMAT, line_number), str->line);

        /* Save the line-text away for later ... */
        if (listline)
            free(listline);
        listline = memcheck(malloc(len + 1));
        memcpy(listline, cp, len);
        listline[len] = '\0';
    }
}


/* list_title_line shows the title heading line(s) */
/* Heading will be printed prior to printing the first .SBTTL on pass 1 */
/* Or if a .TITLE was seen heading will be printed prior to starting pass 2 */
/* -e BMK will suppress version and date & time information for pass 1 */
/* -e BMK will suppress the whole heading on pass 2 (i.e. if no .SBTTL seen) */

void list_title_line(
    const char *line2)
{
    if (line2 == NULL) {  /* Will not be NULL if called from P_SBTTL */
        if (!can_list())
            return;

        if (ENABL(BMK) || title_string[0] == '\0')
            return;
    }

    fprintf(lstfile, "%.31s  " PROGRAM_NAME, (title_string[0] == '\0') ? ".MAIN." : title_string);

    if (!ENABL(BMK)) {
        time_t          current_time = time(NULL);
        struct tm      *local_time   = localtime(&current_time);
        const char     *day_name[7]  = {
                            "Sunday",
                            "Monday",
                            "Tuesday",
                            "Wednesday",
                            "Thursday",
                            "Friday",
                            "Saturday"
                        };
        const char     *month_name[12] = {
                            "Jan", "Feb", "Mar", "Apr",
                            "May", "Jun", "Jul", "Aug",
                            "Sep", "Oct", "Nov", "Dec"
                        };

        fprintf(lstfile, " " BASE_VERSION);
        if (local_time != NULL && local_time->tm_year > 69)
            fprintf(lstfile, "  %s %02d-%3s-%4d %02d:%02d",
                             day_name[local_time->tm_wday],
                             local_time->tm_mday,
                             month_name[local_time->tm_mon],
                             local_time->tm_year + 1900,
                             local_time->tm_hour,
                             local_time->tm_min);
    } else {
#if TODO /* (?) */
        fprintf(lstfile, " " "Version#");
        fprintf(lstfile, "  Weekday dd-Mmm-yyyy hh:mm");
#endif
    }
/*  fprintf(lstfile, "\n");  // This is done below */

    if (line2 == NULL)
        fprintf(lstfile, "\n\n");
    else
        fprintf(lstfile, "\n%.*s\n\n", (STRINGENT) ? 80 /* DOC: J1.1.1 */ : 132, line2);
}


/* list_throw_page starts a new page on the listing */
/* Note: Unlike MACRO-11 blank lines at the top of the page will be suppressed.
 *       For example, all EMPTY lines following a .INCLUDE will not be listed.
 *       Or, indeed, all of the first EMPTY lines in a file.  */

void list_throw_page(
    int force_throw)
{
    if (force_throw || (dolist() && !list_page_top)) {
        if (page_break_ff)
            fputs(list_page_ff, lstfile);
        else
            fputs(list_page_fmt, lstfile);
    }
    list_page_top = 1;
    line_on_page = 0;
}


/* trunc_line truncates a string by replacing all ' ' and '\t' at the end with '\0' */
/* If the string is longer than the maximum allowed, it will truncate from there instead */

static void trunc_line(
    char *string)
{
    int             len = strlen(string);

    if (len > MAX_LISTING_LINE_LEN) {
        len = MAX_LISTING_LINE_LEN;
        string[len] = '\0';
    }

    while (--len >= 0)
        if (string[len] == ' ' || string[len] == '\t')
            string[len] = '\0';
        else
            break;

    return;
}


/* list_oct2hex converts all octal numbers in a string to hex */
/* We can handle octal numbers with 1-6 digits. */

static void list_oct2hex(
    char *cp)
{
    char           *cpe;
    int             len;
    int             octval;
    char            oldche;
    const char      format[6][6] = { /* Digits   Octal   Hexadec */
                        "%1.1X",     /*      1       7         7 */
                        "%2.2X",     /*      2      77        3F */
                        "%3.2X",     /*      3     777       1FF */  /* But usually one byte (0x00-0xFF) */
                        "%4.3X",     /*      4    7777       FFF */
                        "%5.4X",     /*      5   77777      7FFF */
                        "%6.4X"      /*      6  177777      FFFF */
                    };

    for (;;) {
        while (!isdigit((unsigned char) *cp))
            if (*cp++ == '\0')
                return;

        octval = strtol(cp, &cpe, 8);
        len = cpe - cp;
        assert(len > 0 && len <= 6);

        oldche = *cpe;
        sprintf(cp, format[len-1], octval);
        *cpe = oldche;

        cp = cpe;
    }
}


/* list_oct2dec converts all octal numbers in a string to decimal */
/* We can handle octal numbers with 1-6 digits. */

static void list_oct2dec(
    char *cp)
{
    char           *cpe;
    int             len;
    int             octval;
    char            oldche;
    const char      format[6][4] = { /* Digits   Octal   Decimal */
                        "%1d",       /*      1       7         7 */
                        "%2d",       /*      2      77        63 */
                        "%3d",       /*      3     777       511 */
                        "%4d",       /*      4    7777      4095 */
                        "%5d",       /*      5   77777     32767 */
                        "%6d"        /*      6  177777     65535 */
                    };

    for (;;) {
        while (!isdigit((unsigned char) *cp))
            if (*cp++ == '\0')
                return;

        octval = strtol(cp, &cpe, 8);
        len = cpe - cp;
        assert(len > 0 && len <= 6);

        oldche = *cpe;
        sprintf(cp, format[len-1], octval);
        *cpe = oldche;

        cp = cpe;
    }
}


/* list_process processes the listing line prior to outputting it */
/* This handles .[N]LIST SEQ,LOC,BIN,BEX,SRC,COM,HEX (except LOC without either SEQ or BIN). */
/* .NLIST COM is handled differently to MACRO-11 -- Lines starting with ';' or blank lines are suppressed */

static void list_process(
    void)
{
    int             binstart = 0;
    int             nlist_loc_only = 0;

#define HAVE_SEQ   (isdigit((unsigned char) binline[offsetof(LSTFORMAT, gap_after_seq) - 1]))
#define HAVE_LOC   (isdigit((unsigned char) binline[offsetof(LSTFORMAT, pc)            + 5]))
#define HAVE_WORD1 (isdigit((unsigned char) binline[offsetof(LSTFORMAT, words)         + 0]) && \
                    /* Ignore .IF lines */ (binline[offsetof(LSTFORMAT, words)         + 6] != '='))

    if (LIST(TTM))
        padto(binline, offsetof(LSTFORMAT, words[1]));
    else
        padto(binline, offsetof(LSTFORMAT, source));

    trunc_line(listline);

    if (!errline) {  /* Never pre-process error lines */

        /* Handle .NLIST SEQ,LOC,BIN,SRC */

        if (!(LIST(SEQ) || LIST(LOC) || LIST(BIN) || LIST(SRC)))
            return;  /* Completely suppress all [non-error] lines */

        /* Handle NLIST BEX [BIN] and .NLIST COM */

        if (!HAVE_SEQ) {
            if (!LIST(BEX) || !LIST(BIN))
                return;  /* If no sequence number, suppress the BEX line */
        } else if (!LIST(COM)) {
#if NODO  /* Don't forget to #include "parse.h" */
            if (listline[0] == '\0' || *skipwhite(listline) == ';')
            /*  if (binline[0] == ' ')  */
                    return;  /* Completely suppress comment-only lines and blank lines */
#else
            if (listline[0] == ';' || listline[0] == '\0')
            /*  if (binline[0] == ' ')  */
                    return;  /* Completely suppress LHS-comment lines and blank lines */
#endif
        }

        /* Handle .NLIST ME,MEB */

        if (list_within_exp && !LIST(ME) && LIST(LIS) < 1) {  /* V05.05 treats list_level >= 1 as .LIST ME */
           if (!LIST(MEB) && !(list_line_act & LIST_FORCE_LISTING))  /* (Optional) -yfl forces .LIST MEB */
               return;  /* Suppress all source lines within a macro/rept/irp/irpc expansion */

#if TODO
           /* TODO: Future extension: for .LIST MEBX we do not suppress ME lines which have
            *       a 'valid' value in WORD1 (e.g. symbol definitions) */

        /* if (LIST(MEBX)) */ {
               if (!(HAVE_LOC || HAVE_WORD1))
                   return;  /* If no location and no 'valid; word1, suppress the ME line */
           }
#else
           if (!HAVE_LOC && binline[0] != '-')
               return;  /* If no location, suppress the ME line (unless forced) */
#endif
        }

        /* Handle .NLIST LOC (but .LIST SEQ,BIN) */

        nlist_loc_only = (!LIST(LOC) && LIST(SEQ) && LIST(BIN));

        /* Handle .NLIST BIN[,LOC] */

        if (!LIST(BIN)) {
            binline[offsetof(LSTFORMAT, words)] = '\0';
            if (!LIST(LOC)) {
                binline[offsetof(LSTFORMAT, pc)] = '\0';
            }
        }

        /* Handle .NLIST SRC */

        if (!LIST(SRC)) {
            listline[0] = '\0';
            trunc_line(binline);
            if ( /* (binline[0] == ' ' || binline[0] == '\0') && */
                    strlen(binline) < offsetof(LSTFORMAT, pc))
                return;  /* Completely suppress source lines with only a sequence number */
        }

        /* Handle .NLIST SEQ[,LOC] */

        if (!LIST(SEQ)) {
            /* Note that we lose the 'flag' field this way */
            binstart = offsetof(LSTFORMAT, pc);
            if (!LIST(LOC))
                binstart = offsetof(LSTFORMAT, words);
        }
    }

    if (listline[0] == '\0')
        trunc_line(binline);

    /* Handle .LIST LD (extension to MACRO-11) and .LIST HEX */
    /* Remember: if binline has been truncated it will be filled with '\0' */

    if (LIST(LD)) {
        list_oct2dec(&binline[offsetof(LSTFORMAT, words)]);  /* Keep the LOC (PC) octal */
    } else {
        if (LIST(HEX))
            list_oct2hex(&binline[offsetof(LSTFORMAT, pc)]);
    }

    /* Handle automatic-page-breaks (-apb) */

    if (auto_page_break && line_on_page >= PAGE_LENGTH)
        list_throw_page(FALSE);

    /* Output the line */

    line_on_page++;
    if (binline[binstart] != '\0') {
        if (symbol_list_locals) {
            if (looked_up_local_sym)
                fprintf(lstfile, "[$%5d]", lsb);
            else
                fputs("        ", lstfile);
        }
        if (nlist_loc_only) {
                binline[offsetof(LSTFORMAT, gap_after_seq) + 1] = '\0';
                fputs(binline, lstfile);
                binstart = offsetof(LSTFORMAT, words);
        }
        fputs(&binline[binstart], lstfile);
    }

    if (listline[0] != '\0') {
        fputs(listline, lstfile);
    }

    /* If both binline & listline are empty, we will still need an empty line */
    fputc('\n', lstfile);
    list_page_top = 0;

#undef HAVE_SEQ
#undef HAVE_LOC
#undef HAVE_WORD1

}


/* show_error_line shows a source line on stderr with format:
 * .LIST SEQ,LOC,BIN,SRC,COM,TTM and .NLIST BEX,LD,HEX */

void show_error_line(  /* Synonym for show_print_line() */
    void)
{
    if (pass == PASS1)
        return;

    if (!build_list())
        return;

    if (lstfile == stdout)
        return;

    if (binline[0] == '\0')
        return;

    if (!isdigit((unsigned char) binline[offsetof(LSTFORMAT, gap_after_seq) - 1]))
        return;

    fprintf(stderr, "%-*s%.132s\n", (int)offsetof(LSTFORMAT, words), binline, listline);
}


/* list_flush produces a buffered list line. */

void list_flush(
    void)
{
    if (build_list()) {
        /* Remember: binline might be empty (but never NULL) */
        if (errline) {
            list_line_act &= ~LIST_SUPPRESS_LINE;      /* Error lines are never suppressed */
            if (show_error_lines || pass == PASS1) {   /* TODO: Do this on both passes (?) */
                int             i;

                /* TODO: Implement error-letters like MACRO-11 (not trivial) */
                for (i = 0; i < errline; i++) {
                    if (binline[i] != ' ')
                        break;
                    binline[i] = '*';
                }

                if (show_error_lines)
                    show_error_line();
            }
        }

        if (list_line_act & LIST_SUPPRESS_LINE) {
            if (list_line_act & LIST_FORCE_LISTING) {
                list_line_act &= ~LIST_SUPPRESS_LINE;  /* Forced-listing lines are never suppressed */
                if (binline[0] == ' ')
                    binline[0] = '-';                  /* Flag the line as 'suppressed' but 'forced' */
            }
        }
    }

    if (dolist()) {
        if (pass == PASS1)
            list_line_act &= ~LIST_SUPPRESS_LINE;  /* Never suppress lines on pass 1 (for -yl1) */

        if (list_line_act & LIST_SUPPRESS_LINE) {
            list_line_act &= ~LIST_SUPPRESS_LINE;  /* Defer a LIST_PAGE_BEFORE */
        } else {
            if (list_line_act & LIST_PAGE_BEFORE) {
                list_line_act &= ~LIST_PAGE_BEFORE;
                list_throw_page(FALSE);
            }
            if (binline[0])
                list_process();
        }

        if (list_line_act & LIST_PAGE_AFTER) {   /* If we want a page throw 'after' we've listed the line ... */
            list_line_act &= ~LIST_PAGE_AFTER;   /* ... turn it into ... */
            list_line_act |=  LIST_PAGE_BEFORE;  /* ... a LIST_PAGE_BEFORE */
        }
    }

    /* Show the line in error (if any) and exit if a fatal error occurred with -fe or -fe2 */
    if (exit_requested) {
        if (listline != NULL && *listline != '\0')
            fprintf(stderr, "%.132s\n", listline);
        exit(EXIT_FAILURE);
    }

    if (build_list()) {
        list_line_act &= ~(LIST_SUPPRESS_LINE | LIST_FORCE_LISTING);  /* TODO: We shouldn't need this - but we do (!) */
        binline[0]  = '\0';
        listline[0] = '\0';
    }
}


/* list_fit checks to see if a word will fit in the current listing
   line.  If not, it flushes and prepares another line. */

static void list_fit(
    STREAM *str,
    unsigned addr)
{
    size_t          col1;
    size_t          col2 = offsetof(LSTFORMAT, pc);

    if (LIST(TTM))
        col1 = offsetof(LSTFORMAT, words[1]);
    else
        col1 = offsetof(LSTFORMAT, source);

    if (strlen(binline) >= col1) {
        list_flush();
        listline[0] = '\0';
        sprintf(binline, "%*s%6.6o", (int)offsetof(LSTFORMAT, pc), "", addr);
        padto(binline, offsetof(LSTFORMAT, words));
    } else if (strlen(binline) <= col2) {
        sprintf(binline, "%*s%*d %6.6o", (int)SIZEOF_MEMBER(LSTFORMAT, flag), "",
                (int)SIZEOF_MEMBER(LSTFORMAT, line_number), str->line, addr);
        padto(binline, offsetof(LSTFORMAT, words));
    }
}


/* list_format_value is used to format and show a computed value with associated flag */

static void list_format_value(
    STREAM *str,
    const char *format,
    unsigned word,
    char flag)
{
    (STREAM *) str;  /* This parameter is currently unused */

//  assert(build_list());  /* TODO: Remove sanity check */

/*  if (build_list())  */ {  /* TODO: Don't need this 'if' around this block - OR remove the checks from the callers */
        assert(strlen(binline) >= (int)offsetof(LSTFORMAT, gap_after_seq));                /* binline long enough? */
        assert(isdigit((unsigned char) binline[offsetof(LSTFORMAT, gap_after_seq) - 1]));  /* Has a sequence number? */
        padto(binline, offsetof(LSTFORMAT, words));
        sprintf(&binline[offsetof(LSTFORMAT, words)], format, word & 0177777);
        sprintf(binline + strlen(binline), "%c", flag);
    }
}


/* list_symbol is used to show a symbol value and the 'relative-flag' */

void list_symbol(
    STREAM *str,
    SYMBOL *sym)
{
    (STREAM *) str;  /* This parameter is currently unused */

    if (build_list()) {
        /* Print the symbol value and relative-flag */
        if (sym->section->flags & PSECT_REL)
            list_format_value(str, "%6.6o", sym->value, '\'');
        else
            list_format_value(str, "%6.6o", sym->value, '\0');
    }
}


/* list_value is used to show a computed value */

void list_value(
    STREAM *str,
    unsigned word)
{
    (STREAM *) str;  /* This parameter is currently unused */

    if (build_list()) {
        /* Print the value and go */
        list_format_value(str, "%6.6o", word, '\0');
    }
}


/* list_value_if is used to show a computed value for a .IF or .IFxx */

void list_value_if(
    STREAM *str,
    unsigned word)
{
    (STREAM *) str;  /* This parameter is currently unused */

    if (build_list()) {
        /* Print the value and go */
        list_format_value(str, "%6.6o", word, '=');
    }
}


/* list_3digit_value is used to show a '3-digit' computed value */
/* This is similar to list_value() except only the first 3 digits of value will be zero-filled. */
/* Also, if the value is < 0 it will be shown as a +ve value with a '-' flag. */

void list_3digit_value(
    STREAM *str,
    unsigned word)
{
    (STREAM *) str;  /* This parameter is currently unused */

    if (build_list()) {
        list_format_value(str, "%6.3o", word, '\0');
    }
}


/* list_signed_value is used to show a signed 'short' computed value */
/* This is similar to list_value() except the value will not be zero-filled. */
/* If the value is >=0 it will be shown with a '+' flag. */
/* Also, if the value is < 0 it will be shown as a +ve value with a '-' flag. */

void list_signed_value(
    STREAM *str,
    unsigned word)
{
    (STREAM *) str;  /* This parameter is currently unused */

    if (build_list()) {
        if (word & 0x8000)
            list_format_value(str, "%6o", 0x8000 - (word & 0x7fff), '-');
        else
            list_format_value(str, "%6o", word, '+');
    }
}


/* list_short_value_if is used to show a 'short' computed value for .IFT/.IFF/.IFTF and .IIF */
/* This is similar to list_value() except the value will not be zero-filled */
/* Also, the value flag will be set to '=' (as .IF) */

void list_short_value_if(
    STREAM *str,
    unsigned word)
{
    (STREAM *) str;  /* This parameter is currently unused */

    if (build_list()) {
        list_format_value(str, "%6o", word, '=');
    }
}


/* list_word - print a word to the listing file */

void list_word(
    STREAM *str,
    unsigned addr,
    unsigned value,
    int size,
    char *flags)
{
    if (build_list()) {
        list_fit(str, addr);
        if (size == 1)
            sprintf(binline + strlen(binline), "   %3.3o%1.1s ", value & 0377, flags);
        else
            sprintf(binline + strlen(binline), "%6.6o%1.1s ", value & 0177777, flags);
    }
}


/* list_location - print just a line with the address to the listing file */

void list_location(
    STREAM *str,
    unsigned addr)
{
    if (build_list()) {
        list_fit(str, addr);
    }
}


/* report_generic - reports warning, error, and fatal messages  */
/* Fatal messages are listed on both passes (if we didn't exit) */
void report_generic(
    unsigned type,
    STREAM *str,
    char *fmt,
    ...)
{
    va_list         ap;
    char           *name = "**";
    int             line = 0;
    const char     *mess;

    report_errcnt++;
    if (enabl_debug)
        UPD_DEBUG_SYM(DEBUG_SYM_ERRCNT, report_errcnt);

    errline++;
    switch (type) {

    case REPORT_WARNING:
        if (pass == PASS1 && list_pass_0 < 2)
            return;                    /* Don't report now */
        mess = "WARNING";
        break;

    case REPORT_ERROR:
        if (pass == PASS1 && list_pass_0 < 2)
            return;                    /* Don't report now */
        mess = "ERROR";
        break;

    case REPORT_FATAL:
        mess = "FATAL";
        if (pass >= exit_if_pass) {
            exit_requested = TRUE;
            show_error_lines = FALSE;  /* We don't want the message twice */
            if (str == NULL) {
                if (listline != NULL)
                    listline[0] = '\0';
            }
        }
        break;

    default:
        mess = "BADERR";               /* Should never happen */
    }

    if (str) {
        name = str->name;
        line = str->line;
    }

    if (list_line_act & LIST_PAGE_BEFORE)
        list_throw_page(FALSE);

    fprintf(stderr, "%s:%d: ***%s ", name, line, mess);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (lstfile && lstfile != stdout) {
        fprintf(lstfile, "%s:%d: ***%s ", name, line, mess);
        va_start(ap, fmt);
        vfprintf(lstfile, fmt, ap);
        va_end(ap);
    }
}


#if NODO  /* See listing.h */
/* reports errors */
void report(
    STREAM *str,
    char *fmt,
    ...)
{
    va_list         ap;
    char           *name = "**";
    int             line = 0;

    report_errcnt++;
    if (enabl_debug)
        UPD_DEBUG_SYM(DEBUG_SYM_ERRCNT, report_errcnt);

    errline++;
    if (pass > PASS1 && list_pass_0 < 2)
        return;                        /* Don't report now. */

    if (str) {
        name = str->name;
        line = str->line;
    }

    if (list_line_act & LIST_PAGE_BEFORE)
        list_throw_page(FALSE);

    fprintf(stderr, "%s:%d: ***ERROR ", name, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (lstfile && lstfile != stdout) {
        fprintf(lstfile, "%s:%d: ***ERROR ", name, line);
        va_start(ap, fmt);
        vfprintf(lstfile, fmt, ap);
        va_end(ap);
    }
}
#endif /* NODO */
