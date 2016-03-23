/* Definitions for command handlers */

/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */


#ifndef _CMDDEF_H
#define _CMDDEF_H

#define _BSD_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
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
#define PRINT_ATTR ( FAB$M_CR | FAB$M_PRN | FAB$M_FTN )

#define DT_NAME "drive_type"

#define DECL_CMD(x) unsigned do ## x(int argc,char **argv,int qualc,char **qualv)

typedef struct CMDSET CMDSET_t;
typedef struct param param_t;
typedef struct qual qual_t;

typedef CMDSET_t *CMDSETp_t;
typedef param_t *paramp_t;
typedef qual_t *qualp_t;

/* Command table entry */

struct CMDSET {
    char *name;
    unsigned (*proc) (int argc,char *argv[],int qualc,char *qualv[]);
    int uniq;
    qualp_t validquals;
    paramp_t params;
    char *helpstr;
};

/* set, clear: bits specified are cleared, then set in value
 * helpstr: if null, switch not listed in help.  If starts with -,
 *          listed as negatable in help
 */
#define NV NOVAL, NULL
#define KV(list) KEYVAL, list
#define CV(list) KEYCOL, list
#define DV(val)  DECVAL, val
#define SV(val)  STRVAL, ((void *)val)
struct qual {
    const char *name;
    int set;
    int clear;
    enum qualtype { NOVAL, KEYVAL, KEYCOL, DECVAL, STRVAL } qtype;
    void *arg;
    const char *helpstr;
};

typedef const char *(hlpfunc_t)( CMDSET_t *cmd, param_t *p, int argc, char **argv );

struct param {
    const char *name;
    enum parflags { REQ, OPT, CND } flags;
    enum partype { VMSFS, LCLFS, LIST, KEYWD, STRING, CMDNAM, NONE } ptype;
#define PA(arg) NULL, (arg)
#define NOPA PA(NULL)
    hlpfunc_t *hlpfnc;
    void *arg;
    const char *helpstr;
};

extern int vms_qual;
extern int verify_cmd;

int parselist( char ***items, size_t min, char *arg, const char *label );
int checkquals( int result, qual_t qualset[],int qualc,char *qualv[] );
int keycomp(const char *param, const char *keywrd);
char *fgetline( FILE *stream, int keepnl );

int prvmstime(VMSTIME vtime, const char *sfx);
void pwrap( int *pos, const char *fmt, ... );

#endif
