/* Definitions for command handlers */

/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com
 *
 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contributions of the original author and
 * subsequent contributors.   This is free software; no
 * warranty is offered,  and while we believe it to be useful,
 * you use it at your own risk.
 */


#ifndef _CMDDEF_H
#define _CMDDEF_H

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#ifdef _WIN32
#include <sys/utime.h>
#else
#include <sys/time.h>
#include <utime.h>
#endif

#include "compat.h"
#include "ods2.h"
#include "rms.h"
#include "ssdef.h"
#include "stsdef.h"
#include "sysmsg.h"

#define MAXREC 32767

#define DT_NAME "medium_type"

#define DECL_CMD(x) vmscond_t do ## x(int argc,char **argv,int qualc,char **qualv)

typedef struct CMDSET CMDSET_t;
typedef struct param param_t;
typedef struct qual qual_t;

typedef CMDSET_t *CMDSETp_t;
typedef param_t *paramp_t;
typedef qual_t *qualp_t;

typedef vmscond_t (*cmdproc_t) (int argc,char *argv[],int qualc,char *qualv[]);

/* Command table entry */

struct CMDSET {
    char *name;
    cmdproc_t proc;
    int uniq;
    qualp_t validquals;
    paramp_t params;
    char *helpstr;
};

/* set, clear: bits specified are cleared, then set in value
 * qtype = qualifier's value type.  + => value required, - optional, 0 none
 * Following macros simplify setting up the tables.
 * helpstr: if null, switch not listed in help.  If starts with -,
 *          listed as negatable in help. If null string (""), listed
 *          as value table. Otherwise, .hlp file lookup key.
 */
#define NV NOVAL, NULL
#define KV(list) KEYVAL, list
#define CV(list) KEYCOL, list
#define DV(val)  DECVAL, val
#define SV(val)  STRVAL, ((void *)val)
#define PV(val)  PROT, ((void *)val)
#define UV(val)  UIC, ((void *)val)

/* Use VOPT(XX()) for optional value */
#define VOPT(def) -def

struct qual {
    const char *name;
    options_t set;
    options_t clear;
    enum qualtype { opt_optional = -1, NOVAL = 0, /* enum must be signed */
                    KEYVAL, KEYCOL, DECVAL, STRVAL, PROT, UIC } qtype;
    void *arg;
    char *helpstr;
};

typedef const char *(hlpfunc_t)( CMDSET_t *cmd, param_t *p, int argc, char **argv );

struct param {
    const char *name;
    enum parflags { REQ = 1, OPT = 2, CND = 4, NOLIM = 8 } flags;
    enum partype { NONE = 0, FSPEC, LIST, KEYWD, QUALS, STRING } ptype;
#define PA(arg) NULL, (arg)
#define NOPA PA(NULL)
    hlpfunc_t *hlpfnc;
    void *arg;
    char *helpstr;
};

/* Default qualifer sets */

struct defquals {
    struct defquals *next;
    int qualc;
    char **qualv;
    char *data[1];
};

typedef struct defquals defquals_t;
typedef defquals_t *defqualsp_t;

extern const char *qstyle_s;
#define qstyle_c (qstyle_s[0])

extern int verify_cmd;
extern int error_exit;

vmscond_t parselist( int *nret, char ***items, size_t min, char *arg );
vmscond_t checkquals( options_t *result, options_t defval,
                      qualp_t qualset, int qualc,char **qualv );

int keycomp(const char *param, const char *keywrd);
vmscond_t confirm_cmd( vmscond_t status, ... );

vmscond_t sethelpfile( char *filename, char *env, char *pname );
vmscond_t showhelpfile( void );

vmscond_t helptopic( options_t options, char *topic, ... );
#define HLP_OPTIONAL 0x00000001
#define HLP_WRAP     0x00000002
#define HLP_RELOAD   0x00000004
#define HLP_ABBREV   0x00000008

typedef struct pause {
    size_t interval;
    size_t lineno;
    vmscond_t prompt;
} pause_t;

vmscond_t put_c( int c, FILE *outf, pause_t *pause );
vmscond_t put_str( const char *buf, FILE *outf, pause_t *pause );
vmscond_t put_strn( const char *buf, size_t len, FILE *outf, pause_t *pause );
void termchar( int *width, int *height );

vmscond_t prvmstime(FILE *of, VMSTIME vtime, const char *sfx);
void pwrap( FILE *of, int *pos, const char *fmt, ... );

#ifdef HELPFILEDEFS
/* File and index structures private to helpcmd and makehelp. */

/* Helpfile magic cookie, version & header */

#define HLP_MAGIC "ODS2HLP"
#define HLP_VERSION 1

typedef struct hlphdr {
    char magic[ sizeof(HLP_MAGIC) ];
    uint32_t version;
    uint32_t psize, ssize, tsize;
    time_t ddate;
    size_t size;
} hlphdr_t;

/* In-memory pointer / file offsets */

typedef union ptr {
    void *ptr;
    ptrdiff_t ofs;
} ptr_t;

/* Help tree topic nodes (including root) */

typedef struct hlproot {
    ptr_t  topics; /*struct hlptopic * */
    size_t ntopics;
} hlproot_t;

/* Help topic data under each node */

typedef struct hlptopic {
    ptr_t key;  /* char * */
    ptr_t text; /* char * */
    hlproot_t subtopics;
    uint32_t keylen;
} hlptopic_t;

#endif /* HELPFILEDEFS */
#endif
