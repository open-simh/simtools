#ifndef LISTING__H
#define LISTING__H

#include "stream2.h"
#include "symbols.h"

/*
 *  Format of a listing line.
 *  Interestingly, no instances of this struct are ever created.
 *  It lives to be a way to layout the format of a list line.
 *  I wonder if I should have bothered.
 *  Of course you should ... you use offsetof() etc.
 */

/*  0123456789012345678901234567890123456789
 *  fssssss llllll  aaaaaaaabbbbbbbbcccccccc...
 *  ?   123 001000  012737  052100  000776  ...
 */

typedef struct lstformat {
    char            flag[1];           /* Error flags            (ought to be [3]) */
    char            line_number[6];    /* Line [sequence] number (ought to be [4]) */
    char            gap_after_seq[1];  /* Space */
    char            pc[6];             /* Location */
    char            gap_after_pc[2];   /* Spaces */
    char            words[3][8];       /* Up to three instruction words or bytes */
    char            source[1];         /* Source line */
} LSTFORMAT;


/* GLOBAL VARIABLES */
#ifndef LISTING__C

extern char     title_string[32];  /* .TITLE string (up to 31 characters) */
extern int      toc_shown;         /* Flags that at least one .SBTTL was listed in the TOC */

extern int      page_break_ff;     /* TRUE if -ff (use form-feed for listing page-breaks */
extern int      auto_page_break;   /* TRUE if -apb (automatic-page-break) selected */
extern int      line_on_page;      /* Current line number on the listing page */

extern char    *list_page_fmt;     /* Format to use for the page throw */

extern int      list_page_top;     /* Are we at the top of a page? */

extern int      list_line_act;     /* Action to perform when listing the current line */
extern int      listing_forced;    /* If set to LIST_FORCE_LISTING all lines will be listed */

extern int      list_within_exp;   /* Flag if the listing line is DIRECTLY within a macro/rept/irp/irpc expansion */

extern FILE    *lstfile;           /* Listing file descriptor */

extern int      list_pass_0;       /* Also list what happens during the first pass */

extern int      report_errcnt;     /* Count the number of times report() has been called */

extern int      show_error_lines;  /* Show the line with the error when reporting errors */
extern int      show_print_lines;  /* Show .PRINT lines (similar to show_error_lines) */

extern int      exit_if_pass;      /* Exit if a fatal error occurs on this pass [or higher] */
extern int      exit_requested;    /* Set to TRUE if a 'fatal' exit is requested  (-fe only) */

#endif

#define PAGE_LENGTH 57             /* Number of listing lines on the page for -apb */

#define MAX_LISTING_LINE_LEN 148   /* V05.05 truncates after 120 characters */
                                   /* The 'binline' part (including LSB) can be up to 48 characters.  This means
                                    * that if 'listline' is 148 long, we can get a listing line 196 bytes long. */

#define list_level enabl_list_arg[L_LIS].curval  /* = LIST(LIS) // Directly access the .[N]LIST 'LIS' level (= value) */

#define LIST_SUPPRESS_LINE  1   /* Suppress the line itself (e.g. '.PAGE') */
#define LIST_FORCE_LISTING  2   /* Force the listing of the line (regardelss of LIST_SUPPRESS_LINE) */
#define LIST_PAGE_BEFORE    4   /* New page BEFORE listing the line */
#define LIST_PAGE_AFTER     8   /* New page AFTER listing the line */

#define DONT_LIST_THIS_LINE() (list_line_act |= LIST_SUPPRESS_LINE)
#define MUST_LIST_THIS_LINE() (list_line_act |= LIST_FORCE_LISTING)


void            list_source(
    STREAM *str,
    char *cp);

void            list_title_line(
    const char *line2);

void            list_throw_page(
    int force_throw);

void            list_symbol(
    STREAM *str,
    SYMBOL *sym);

void            list_value(
    STREAM *str,
    unsigned word);

void            list_value_if(
    STREAM *str,
    unsigned word);

void            list_3digit_value(
    STREAM *str,
    unsigned word);

void            list_signed_value(
    STREAM *str,
    unsigned word);

void            list_short_value_if(
    STREAM *str,
    unsigned word);

void            list_word(
    STREAM *str,
    unsigned addr,
    unsigned value,
    int size,
    char *flags);

void            list_location(
    STREAM *str,
    unsigned word);

#define show_print_line() show_error_line()

void            show_error_line(
    void);

void            list_flush(
    void);

#if 0 // TODO   /* TODO: Just in case ##__VA_ARGS__ doesn't work everywhere - see listing.c */

#define         report_warn  report
#define         report_err   report
#define         report_fatal report

void            report(
    STREAM *str,
    char *fmt,
    ...);

#endif

#define         REPORT_WARNING  1  /* Will [optionally] be displayed on pass 2 (and pass 1 if -yl1*2) */
#define         REPORT_ERROR    2  /* Will be displayed on pass 2 (and pass 1 if -yl1*2) */
#define         REPORT_FATAL    3  /* Will be displayed on pass 1 and pass 2 */

#define         report_warn(str, fmt, ...)  report_generic(REPORT_WARNING, str, fmt, ##__VA_ARGS__)
#define         report_err(str, fmt, ...)   report_generic(REPORT_ERROR,   str, fmt, ##__VA_ARGS__)
#define         report_fatal(str, fmt, ...) report_generic(REPORT_FATAL,   str, fmt, ##__VA_ARGS__)

void            report_generic(
    unsigned type,
    STREAM *str,
    char *fmt,
    ...);

#endif /* LISTING__H */
