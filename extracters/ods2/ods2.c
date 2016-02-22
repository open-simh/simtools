#define MODULE_NAME	ODS2
#define MODULE_IDENT	"V1.3hb"

/*     Jul-2003, v1.3hb, some extensions by Hartmut Becker */

/*     Ods2.c v1.3   Mainline ODS2 program   */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.

        The modules in ODS2 are:-

                ACCESS.C        Routines for accessing ODS2 disks
                CACHE.C         Routines for managing memory cache
                DEVICE.C        Routines to maintain device information
                DIRECT.C        Routines for handling directories
                ODS2.C          The mainline program
                PHYVMS.C        Routine to perform physical I/O
                RMS.C           Routines to handle RMS structures
                VMSTIME.C       Routines to handle VMS times

        On non-VMS platforms PHYVMS.C should be replaced as follows:-

                OS/2            PHYOS2.C
                Windows 95/NT   PHYNT.C

        For example under OS/2 the program is compiled using the GCC
        compiler with the single command:-

                gcc -fdollars-in-identifiers ods2.c,rms.c,direct.c,
                      access.c,device.c,cache.c,phyos2.c,vmstime.c
*/

/* Modified by:
 *
 *   31-AUG-2001 01:04	Hunter Goatley <goathunter@goatley.com>
 *
 *	For VMS, added routine getcmd() to read commands with full
 *	command recall capabilities.
 *
 */

/*  This version will compile and run using normal VMS I/O by
    defining VMSIO
*/

/*  This is the top level set of routines. It is fairly
    simple minded asking the user for a command, doing some
    primitive command parsing, and then calling a set of routines
    to perform whatever function is required (for example COPY).
    Some routines are implemented in different ways to test the
    underlying routines - for example TYPE is implemented without
    a NAM block meaning that it cannot support wildcards...
    (sorry! - could be easily fixed though!)
*/

#ifdef VMS
#ifdef __DECC
#pragma module MODULE_NAME MODULE_IDENT
#else
#ifdef vaxc
#module MODULE_NAME MODULE_IDENT
#endif /* vaxc */
#endif /* __DECC */
#endif /* VMS */

#define DEBUGx on
#define VMSIOx on

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#ifdef VMSIO
#include <ssdef.h>
#include <descrip.h>
#include <starlet.h>
#include <rms.h>
#include <fiddef.h>
#define sys_parse       sys$parse
#define sys_search      sys$search
#define sys_open        sys$open
#define sys_close       sys$close
#define sys_connect     sys$connect
#define sys_disconnect  sys$disconnect
#define sys_get         sys$get
#define sys_put         sys$put
#define sys_create      sys$create
#define sys_erase       sys$erase
#define sys_extend      sys$extend
#define sys_asctim      sys$asctim
#define sys_setddir     sys$setddir
#define dsc_descriptor  dsc$descriptor
#define dsc_w_length    dsc$w_length
#define dsc_a_pointer   dsc$a_pointer

#else
#include "ssdef.h"
#include "descrip.h"
#include "access.h"
#include "rms.h"
#endif

#define PRINT_ATTR (FAB$M_CR | FAB$M_PRN | FAB$M_FTN)



/* keycomp: routine to compare parameter to a keyword - case insensitive! */

int keycomp(char *param,char *keywrd)
{
    while (*param != '\0') {
        if (tolower(*param++) != *keywrd++) return 0;
    }
    return 1;
}


/* checkquals: routine to find a qualifer in a list of possible values */

int checkquals(char *qualset[],int qualc,char *qualv[])
{
    int result = 0;
    while (qualc-- > 0) {
        int i = 0;
        while (qualset[i] != NULL) {
            if (keycomp(qualv[qualc],qualset[i])) {
                result |= 1 << i;
                i = -1;
                break;
            }
            i++;
        }
        if (i >= 0) printf("%%ODS2-W-ILLQUAL, Unknown qualifer '%s' ignored\n",qualv[qualc]);
    }
    return result;
}


/* dir: a directory routine */

char *dirquals[] = {"date","file","size",NULL};

unsigned dir(int argc,char *argv[],int qualc,char *qualv[])
{
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    int sts,options;
    int filecount = 0;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    struct XABDAT dat = cc$rms_xabdat;
    struct XABFHC fhc = cc$rms_xabfhc;
    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_xab = &dat;
    dat.xab$l_nxt = &fhc;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    fab.fab$l_dna = "*.*;*";
    fab.fab$b_dns = strlen(fab.fab$l_dna);
    options = checkquals(dirquals,qualc,qualv);
    sts = sys_parse(&fab);
    if (sts & 1) {
        char dir[NAM$C_MAXRSS + 1];
        int namelen;
        int dirlen = 0;
        int dirfiles = 0,dircount = 0;
        int dirblocks = 0,totblocks = 0;
        int printcol = 0;
#ifdef DEBUG
        res[nam.nam$b_esl] = '\0';
        printf("Parse: %s\n",res);
#endif
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
        while ((sts = sys_search(&fab)) & 1) {
            if (dirlen != nam.nam$b_dev + nam.nam$b_dir ||
                memcmp(rsa,dir,nam.nam$b_dev + nam.nam$b_dir) != 0) {
                if (dirfiles > 0) {
                    if (printcol > 0) printf("\n");
                    printf("\nTotal of %d file%s",dirfiles,(dirfiles == 1 ? "" : "s"));
                    if (options & 4) {
                        printf(", %d block%s.\n",dirblocks,(dirblocks == 1 ? "" : "s"));
                    } else {
                        fputs(".\n",stdout);
                    }
                }
                dirlen = nam.nam$b_dev + nam.nam$b_dir;
                memcpy(dir,rsa,dirlen);
                dir[dirlen] = '\0';
                printf("\nDirectory %s\n\n",dir);
                filecount += dirfiles;
                totblocks += dirblocks;
                dircount++;
                dirfiles = 0;
                dirblocks = 0;
                printcol = 0;
            }
            rsa[nam.nam$b_rsl] = '\0';
            namelen = nam.nam$b_name + nam.nam$b_type + nam.nam$b_ver;
            if (options == 0) {
                if (printcol > 0) {
                    int newcol = (printcol + 20) / 20 * 20;
                    if (newcol + namelen >= 80) {
                        fputs("\n",stdout);
                        printcol = 0;
                    } else {
                        printf("%*s",newcol - printcol," ");
                        printcol = newcol;
                    }
                }
                fputs(rsa + dirlen,stdout);
                printcol += namelen;
            } else {
                if (namelen > 18) {
                    printf("%s\n                   ",rsa + dirlen);
                } else {
                    printf("%-19s",rsa + dirlen);
                }
                sts = sys_open(&fab);
                if ((sts & 1) == 0) {
                    printf("Open error: %d\n",sts);
                } else {
                    sts = sys_close(&fab);
                    if (options & 2) {
                        char fileid[100];
                        sprintf(fileid,"(%d,%d,%d)",
                                (nam.nam$b_fid_nmx << 16) | nam.nam$w_fid_num,
                                nam.nam$w_fid_seq,nam.nam$b_fid_rvn);
                        printf("  %-22s",fileid);
                    }
                    if (options & 4) {
                        unsigned filesize = fhc.xab$l_ebk;
                        if (fhc.xab$w_ffb == 0) filesize--;
                        printf("%9d",filesize);
                        dirblocks += filesize;
                    }
                    if (options & 1) {
                        char tim[24];
                        struct dsc_descriptor timdsc;
                        timdsc.dsc_w_length = 23;
                        timdsc.dsc_a_pointer = tim;
                        sts = sys_asctim(0,&timdsc,dat.xab$q_cdt,0);
                        if ((sts & 1) == 0) printf("Asctim error: %d\n",sts);
                        tim[23] = '\0';
                        printf("  %s",tim);
                    }
                    printf("\n");
                }
            }
            dirfiles++;
        }
        if (sts == RMS$_NMF) sts = 1;
        if (printcol > 0) printf("\n");
        if (dirfiles > 0) {
            printf("\nTotal of %d file%s",dirfiles,(dirfiles == 1 ? "" : "s"));
            if (options & 4) {
                printf(", %d block%s.\n",dirblocks,(dirblocks == 1 ? "" : "s"));
            } else {
                fputs(".\n",stdout);
            }
            filecount += dirfiles;
            totblocks += dirblocks;
            if (dircount > 1) {
                printf("\nGrand total of %d director%s, %d file%s",
                       dircount,(dircount == 1 ? "y" : "ies"),
                       filecount,(filecount == 1 ? "" : "s"));
                if (options & 4) {
                    printf(", %d block%s.\n",totblocks,(totblocks == 1 ? "" : "s"));
                } else {
                    fputs(".\n",stdout);
                }
            }
        }
    }
    if (sts & 1) {
        if (filecount < 1) printf("%%DIRECT-W-NOFILES, no files found\n");
    } else {
        printf("%%DIR-E-ERROR Status: %d\n",sts);
    }
    return sts;
}


/* copy: a file copy routine */

#define MAXREC 32767

char *copyquals[] = {"binary",NULL};

unsigned copy(int argc,char *argv[],int qualc,char *qualv[])
{
    int sts,options;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    int filecount = 0;
    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    options = checkquals(copyquals,qualc,qualv);
    sts = sys_parse(&fab);
    if (sts & 1) {
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
        while ((sts = sys_search(&fab)) & 1) {
            sts = sys_open(&fab);
            if ((sts & 1) == 0) {
                printf("%%COPY-F-OPENFAIL, Open error: %d\n",sts);
                perror("-COPY-F-ERR ");
            } else {
                struct RAB rab = cc$rms_rab;
                rab.rab$l_fab = &fab;
                if ((sts = sys_connect(&rab)) & 1) {
                    FILE *tof;
                    char name[NAM$C_MAXRSS + 1];
                    unsigned records = 0;
                    {
                        char *out = name,*inp = argv[2];
                        int dot = 0;
                        while (*inp != '\0') {
                            if (*inp == '*') {
                                inp++;
                                if (dot) {
                                    memcpy(out,nam.nam$l_type + 1,nam.nam$b_type - 1);
                                    out += nam.nam$b_type - 1;
                                } else {
                                    unsigned length = nam.nam$b_name;
                                    if (*inp == '\0') length += nam.nam$b_type;
                                    memcpy(out,nam.nam$l_name,length);
                                    out += length;
                                }
                            } else {
                                if (*inp == '.') {
                                    dot = 1;
                                } else {
                                    if (strchr(":]\\/",*inp)) dot = 0;
                                }
                                *out++ = *inp++;
                            }
                        }
                        *out++ = '\0';
                    }
#ifndef _WIN32
                    tof = fopen(name,"w");
#else
		    if ((options & 1) == 0 && fab.fab$b_rat & PRINT_ATTR) {
                        tof = fopen(name,"w");
		    } else {
		        tof = fopen(name,"wb");
		    }
#endif
                    if (tof == NULL) {
                        printf("%%COPY-F-OPENOUT, Could not open %s\n",name);
                        perror("-COPY-F-ERR ");
                    } else {
                        char rec[MAXREC + 2];
                        filecount++;
                        rab.rab$l_ubf = rec;
                        rab.rab$w_usz = MAXREC;
                        while ((sts = sys_get(&rab)) & 1) {
                            unsigned rsz = rab.rab$w_rsz;
                            if ((options & 1) == 0 &&
                                 fab.fab$b_rat & PRINT_ATTR) rec[rsz++] = '\n';
                            if (fwrite(rec,rsz,1,tof) == 1) {
                                records++;
                            } else {
                                printf("%%COPY-F- fwrite error!!\n");
                                perror("-COPY-F-ERR ");
                                break;
                            }
                        }
                        if (fclose(tof)) {
                            printf("%%COPY-F- fclose error!!\n");
                            perror("-COPY-F-ERR ");
                        }
                    }
                    sys_disconnect(&rab);
                    rsa[nam.nam$b_rsl] = '\0';
                    if (sts == RMS$_EOF) {
                        printf("%%COPY-S-COPIED, %s copied to %s (%d record%s)\n",
                               rsa,name,records,(records == 1 ? "" : "s"));
                    } else {
                        printf("%%COPY-F-ERROR Status: %d for %s\n",sts,rsa);
                        sts = 1;
                    }
                }
                sys_close(&fab);
            }
        }
        if (sts == RMS$_NMF) sts = 1;
    }
    if (sts & 1) {
        if (filecount > 0) printf("%%COPY-S-NEWFILES, %d file%s created\n",
                                  filecount,(filecount == 1 ? "" : "s"));
    } else {
        printf("%%COPY-F-ERROR Status: %d\n",sts);
    }
    return sts;
}

/* import: a file copy routine */


unsigned import(int argc,char *argv[],int qualc,char *qualv[])
{
    int sts;
    FILE *fromf;
    fromf = fopen(argv[1],"r");
    if (fromf != NULL) {
        struct FAB fab = cc$rms_fab;
        fab.fab$l_fna = argv[2];
        fab.fab$b_fns = strlen(fab.fab$l_fna);
        if ((sts = sys_create(&fab)) & 1) {
            struct RAB rab = cc$rms_rab;
            rab.rab$l_fab = &fab;
            if ((sts = sys_connect(&rab)) & 1) {
                char rec[MAXREC + 2];
                rab.rab$l_rbf = rec;
                rab.rab$w_usz = MAXREC;
                while (fgets(rec,sizeof(rec),fromf) != NULL) {
                    rab.rab$w_rsz = strlen(rec);
                    sts = sys_put(&rab);
                    if ((sts & 1) == 0) break;
                }
                sys_disconnect(&rab);
            }
            sys_close(&fab);
        }
        fclose(fromf);
        if (!(sts & 1)) {
            printf("%%IMPORT-F-ERROR Status: %d\n",sts);
        }
    } else {
        printf("Can't open %s\n",argv[1]);
    }
    return sts;
}


/* diff: a simple file difference routine */

unsigned diff(int argc,char *argv[],int qualc,char *qualv[])
{
    int sts;
    struct FAB fab = cc$rms_fab;
    FILE *tof;
    int records = 0;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    tof = fopen(argv[2],"r");
    if (tof == NULL) {
        printf("Could not open file %s\n",argv[1]);
        sts = 0;
    } else {
        if ((sts = sys_open(&fab)) & 1) {
            struct RAB rab = cc$rms_rab;
            rab.rab$l_fab = &fab;
            if ((sts = sys_connect(&rab)) & 1) {
                char rec[MAXREC + 2],cpy[MAXREC + 1];
                rab.rab$l_ubf = rec;
                rab.rab$w_usz = MAXREC;
                while ((sts = sys_get(&rab)) & 1) {
                    strcpy(rec + rab.rab$w_rsz,"\n");
                    fgets(cpy,MAXREC,tof);
                    if (strcmp(rec,cpy) != 0) {
                        printf("%%DIFF-F-DIFFERENT Files are different!\n");
                        sts = 4;
                        break;
                    } else {
                        records++;
                    }
                }
                sys_disconnect(&rab);
            }
            sys_close(&fab);
        }
        fclose(tof);
        if (sts == RMS$_EOF) sts = 1;
    }
    if (sts & 1) {
        printf("%%DIFF-I-Compared %d records\n",records);
    } else {
        printf("%%DIFF-F-Error %d in difference\n",sts);
    }
    return sts;
}


/* typ: a file TYPE routine */

unsigned typ(int argc,char *argv[],int qualc,char *qualv[])
{
    int sts;
    int records = 0;
    struct FAB fab = cc$rms_fab;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    if ((sts = sys_open(&fab)) & 1) {
        struct RAB rab = cc$rms_rab;
        rab.rab$l_fab = &fab;
        if ((sts = sys_connect(&rab)) & 1) {
            char rec[MAXREC + 2];
            rab.rab$l_ubf = rec;
            rab.rab$w_usz = MAXREC;
            while ((sts = sys_get(&rab)) & 1) {
                unsigned rsz = rab.rab$w_rsz;
                if (fab.fab$b_rat & PRINT_ATTR) rec[rsz++] = '\n';
                rec[rsz++] = '\0';
                fputs(rec,stdout);
                records++;
            }
            sys_disconnect(&rab);
        }
        sys_close(&fab);
        if (sts == RMS$_EOF) sts = 1;
    }
    if ((sts & 1) == 0) {
        printf("%%TYPE-F-ERROR Status: %d\n",sts);
    }
    return sts;
}



/* search: a simple file search routine */

unsigned search(int argc,char *argv[],int qualc,char *qualv[])
{
    int sts = 0;
    int filecount = 0;
    int findcount = 0;
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    register char *searstr = argv[2];
    register char firstch = tolower(*searstr++);
    register char *searend = searstr + strlen(searstr);
    {
        char *str = searstr;
        while (str < searend) {
            *str = tolower(*str);
            str++;
        }
    }
    nam = cc$rms_nam;
    fab = cc$rms_fab;
    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    fab.fab$l_dna = "";
    fab.fab$b_dns = strlen(fab.fab$l_dna);
    sts = sys_parse(&fab);
    if (sts & 1) {
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
        while ((sts = sys_search(&fab)) & 1) {
            sts = sys_open(&fab);
            if ((sts & 1) == 0) {
                printf("%%SEARCH-F-OPENFAIL, Open error: %d\n",sts);
            } else {
                struct RAB rab = cc$rms_rab;
                rab.rab$l_fab = &fab;
                if ((sts = sys_connect(&rab)) & 1) {
                    int printname = 1;
                    char rec[MAXREC + 2];
                    filecount++;
                    rab.rab$l_ubf = rec;
                    rab.rab$w_usz = MAXREC;
                    while ((sts = sys_get(&rab)) & 1) {
                        register char *strng = rec;
                        register char *strngend = strng + (rab.rab$w_rsz - (searend - searstr));
                        while (strng < strngend) {
                            register char ch = *strng++;
                            if (ch == firstch || (ch >= 'A' && ch <= 'Z' && ch + 32 == firstch)) {
                                register char *str = strng;
                                register char *cmp = searstr;
                                while (cmp < searend) {
                                    register char ch2 = *str++;
                                    ch = *cmp;
                                    if (ch2 != ch && (ch2 < 'A' || ch2 > 'Z' || ch2 + 32 != ch)) break;
                                    cmp++;
                                }
                                if (cmp >= searend) {
                                    findcount++;
                                    rec[rab.rab$w_rsz] = '\0';
                                    if (printname) {
                                        rsa[nam.nam$b_rsl] = '\0';
                                        printf("\n******************************\n%s\n\n",rsa);
                                        printname = 0;
                                    }
                                    fputs(rec,stdout);
                                    if (fab.fab$b_rat & PRINT_ATTR) fputc('\n',stdout);
                                    break;
                                }
                            }
                        }
                    }
                    sys_disconnect(&rab);
                }
                if (sts == SS$_NOTINSTALL) {
                    printf("%%SEARCH-W-NOIMPLEM, file operation not implemented\n");
                    sts = 1;
                }
                sys_close(&fab);
            }
        }
        if (sts == RMS$_NMF || sts == RMS$_FNF) sts = 1;
    }
    if (sts & 1) {
        if (filecount < 1) {
            printf("%%SEARCH-W-NOFILES, no files found\n");
        } else {
            if (findcount < 1) printf("%%SEARCH-I-NOMATCHES, no strings matched\n");
        }
    } else {
        printf("%%SEARCH-F-ERROR Status: %d\n",sts);
    }
    return sts;
}


/* del: you don't want to know! */

unsigned del(int argc,char *argv[],int qualc,char *qualv[])
{
    int sts = 0;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    int filecount = 0;
    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    sts = sys_parse(&fab);
    if (sts & 1) {
        if (nam.nam$b_ver < 2) {
            printf("%%DELETE-F-NOVER, you must specify a version!!\n");
        } else {
            nam.nam$l_rsa = rsa;
            nam.nam$b_rss = NAM$C_MAXRSS;
            fab.fab$l_fop = FAB$M_NAM;
            while ((sts = sys_search(&fab)) & 1) {
                sts = sys_erase(&fab);
                if ((sts & 1) == 0) {
                    printf("%%DELETE-F-DELERR, Delete error: %d\n",sts);
                } else {
                    filecount++;
                    rsa[nam.nam$b_rsl] = '\0';
                    printf("%%DELETE-I-DELETED, Deleted %s\n",rsa);
                }
            }
            if (sts == RMS$_NMF) sts = 1;
        }
        if (sts & 1) {
            if (filecount < 1) {
                printf("%%DELETE-W-NOFILES, no files deleted\n");
            }
        } else {
            printf("%%DELETE-F-ERROR Status: %d\n",sts);
        }
    }
    return sts;
}

/* test: you don't want to know! */
struct VCB *test_vcb;

unsigned test(int argc,char *argv[],int qualc,char *qualv[])
{
    int sts = 0;
    struct fiddef fid;
    sts = update_create(test_vcb,NULL,"Test.File",&fid,NULL);
    printf("Test status of %d (%s)\n",sts,argv[1]);
    return sts;
}

/* more test code... */

unsigned extend(int argc,char *argv[],int qualc,char *qualv[])
{
    int sts;
    struct FAB fab = cc$rms_fab;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    fab.fab$b_fac = FAB$M_UPD;
    if ((sts = sys_open(&fab)) & 1) {
        fab.fab$l_alq = 32;
        sts = sys_extend(&fab);
        sys_close(&fab);
    }
    if ((sts & 1) == 0) {
        printf("%%EXTEND-F-ERROR Status: %d\n",sts);
    }
    return sts;
}




/* show: the show command */

unsigned show(int argc,char *argv[],int qualc,char *qualv[])
{
    unsigned sts = 1;
    if (keycomp(argv[1],"default")) {
        unsigned short curlen;
        char curdir[NAM$C_MAXRSS + 1];
        struct dsc_descriptor curdsc;
        curdsc.dsc_w_length = NAM$C_MAXRSS;
        curdsc.dsc_a_pointer = curdir;
        if ((sts = sys_setddir(NULL,&curlen,&curdsc)) & 1) {
            curdir[curlen] = '\0';
            printf(" %s\n",curdir);
        } else {
            printf("Error %d getting default\n",sts);
        }
    } else {
        if (keycomp(argv[1],"time")) {
            unsigned sts;
            char timstr[24];
            unsigned short timlen;
            struct dsc_descriptor timdsc;
            timdsc.dsc_w_length = 20;
            timdsc.dsc_a_pointer = timstr;
            sts = sys_asctim(&timlen,&timdsc,NULL,0);
            if (sts & 1) {
                timstr[timlen] = '\0';
                printf("  %s\n",timstr);
            } else {
                printf("%%SHOW-W-TIMERR error %d\n",sts);
            }
        } else {
            printf("%%SHOW-W-WHAT '%s'?\n",argv[1]);
        }
    }
    return sts;
}

unsigned setdef_count = 0;

void setdef(char *newdef)
{
    register unsigned sts;
    struct dsc_descriptor defdsc;
    defdsc.dsc_a_pointer = newdef;
    defdsc.dsc_w_length = strlen(defdsc.dsc_a_pointer);
    if ((sts = sys_setddir(&defdsc,NULL,NULL)) & 1) {
        setdef_count++;
    } else {
        printf("Error %d setting default to %s\n",sts,newdef);
    }
}

/* set: the set command */

unsigned set(int argc,char *argv[],int qualc,char *qualv[])
{
    unsigned sts = 1;
    if (keycomp(argv[1],"default")) {
        setdef(argv[2]);
    } else {
        printf("%%SET-W-WHAT '%s'?\n",argv[1]);
    }
    return sts;
}


#ifndef VMSIO

/* The bits we need when we don't have real VMS routines underneath... */

unsigned dodismount(int argc,char *argv[],int qualc,char *qualv[])
{
    struct DEV *dev;
    register int sts = device_lookup(strlen(argv[1]),argv[1],0,&dev);
    if (sts & 1) {
        if (dev->vcb != NULL) {
            sts = dismount(dev->vcb);
        } else {
            sts = SS$_DEVNOTMOUNT;
        }
    }
    if ((sts & 1) == 0) printf("%%DISMOUNT-E-STATUS Error: %d\n",sts);
    return sts;
}



char *mouquals[] = {"write",NULL};

unsigned domount(int argc,char *argv[],int qualc,char *qualv[])
{
    char *dev = argv[1];
    char *lab = argv[2];
    int sts = 1,devices = 0;
    char *devs[100],*labs[100];
    int options = checkquals(mouquals,qualc,qualv);
    while (*lab != '\0') {
        labs[devices++] = lab;
        while (*lab != ',' && *lab != '\0') lab++;
        if (*lab != '\0') {
            *lab++ = '\0';
        } else {
            break;
        }
    }
    devices = 0;
    while (*dev != '\0') {
        devs[devices++] = dev;
        while (*dev != ',' && *dev != '\0') dev++;
        if (*dev != '\0') {
            *dev++ = '\0';
        } else {
            break;
        }
    }
    if (devices > 0) {
        unsigned i;
        struct VCB *vcb;
        sts = mount(options,devices,devs,labs,&vcb);
        if (sts & 1) {
            for (i = 0; i < vcb->devices; i++)
                if (vcb->vcbdev[i].dev != NULL)
                    printf("%%MOUNT-I-MOUNTED, Volume %12.12s mounted on %s\n",
                           vcb->vcbdev[i].home.hm2$t_volname,vcb->vcbdev[i].dev->devnam);
            if (setdef_count == 0) {
                char *colon,defdir[256];
                strcpy(defdir,vcb->vcbdev[0].dev->devnam);
                colon = strchr(defdir,':');
                if (colon != NULL) *colon = '\0';
                strcpy(defdir + strlen(defdir),":[000000]");
                setdef(defdir);
                test_vcb = vcb;
            }
        } else {
            printf("Mount failed with %d\n",sts);
        }
    }
    return sts;
}


void direct_show(void);
void phyio_show(void);

/* statis: print some simple statistics */

unsigned statis(int argc,char *argv[],int qualc,char *qualv[])
{
    printf("Statistics:-\n");
    direct_show();
    cache_show();
    phyio_show();
    return 1;
}

#endif


/* help: a routine to print a pre-prepared help text... */

unsigned help(int argc,char *argv[],int qualc,char *qualv[])
{
    printf("\nODS2 %s\n", MODULE_IDENT);
    printf(" Please send problems/comments to Paulnank@au1.ibm.com\n");
    printf(" Commands are:\n");
    printf("  copy        difference      directory     exit\n");
    printf("  mount       show_default    show_time     search\n");
    printf("  set_default type\n");
    printf(" Example:-\n    $ mount e:\n");
    printf("    $ search e:[vms_common.decc*...]*.h rms$_wld\n");
    printf("    $ set default e:[sys0.sysmgr]\n");
    printf("    $ copy *.com;-1 c:\\*.*\n");
    printf("    $ directory/file/size/date [-.sys*...].%%\n");
    printf("    $ exit\n");
    return 1;
}


/* informaion about the commands we know... */

struct CMDSET {
    char *name;
    unsigned (*proc) (int argc,char *argv[],int qualc,char *qualv[]);
    int minlen;
    int minargs;
    int maxargs;
    int maxquals;
} cmdset[] = {
    {
        "copy",copy,3,3,3,1
},
    {
        "import",import,3,3,3,0
},
    {
        "delete",del,3,2,2,0
},
    {
        "difference",diff,3,3,3,0
},
    {
        "directory",dir,3,1,2,6
},
    {
        "exit",NULL,2,0,0,0
},
    {
        "extend",extend,3,2,2,0
},
    {
        "help",help,2,1,1,0
},
    {
        "quit",NULL,2,0,0,0
},
    {
        "show",show,2,2,2,0
},
    {
        "search",search,3,3,3,0
},
    {
        "set",set,3,2,3,0
},
#ifndef VMSIO
    {
        "dismount",dodismount,3,2,2,0
},
    {
        "mount",domount,3,2,3,2
},
    {
        "statistics",statis,3,1,1,0
},
#endif
    {
        "test",test,4,2,2,0
},
    {
        "type",typ,3,2,2,0
},
    {
        NULL,NULL,0,0,0,0
}
};


/* cmdexecute: identify and execute a command */

int cmdexecute(int argc,char *argv[],int qualc,char *qualv[])
{
    char *ptr = argv[0];
    struct CMDSET *cmd = cmdset;
    unsigned cmdsiz = strlen(ptr);
    while (*ptr != '\0') {
        *ptr = tolower(*ptr);
        ptr++;
    }
    while (cmd->name != NULL) {
        if (cmdsiz >= cmd->minlen && cmdsiz <= strlen(cmd->name)) {
            if (keycomp(argv[0],cmd->name)) {
                if (cmd->proc == NULL) {
                    return 0;
                } else {
                    if (argc < cmd->minargs || argc > cmd->maxargs) {
                        printf("%%ODS2-E-PARAMS, Incorrect number of command parameters\n");
                    } else {
                        if (qualc > cmd->maxquals) {
                            printf("%%ODS2-E-QUALS, Too many command qualifiers\n");
                        } else {
                            (*cmd->proc) (argc,argv,qualc,qualv);
#ifndef VMSIO
                            /* cache_flush();  */
#endif
                        }
                    }
                    return 1;
                }
            }
        }
        cmd++;
    }
    printf("%%ODS2-E-ILLCMD, Illegal or ambiguous command '%s'\n",argv[0]);
    return 1;
}

/* cmdsplit: break a command line into its components */

/*
 * New feature for Unix: '//' stops qualifier parsing (similar to '--'
 * for some Unix tools). This enables us to copy to Unix directories.
 *     copy /bin // *.com /tmp/*
 * is split into argv[0] -> "*.com" argv[1] -> "/tmp/*" qualv[0]-> "/bin"
 */

int cmdsplit(char *str)
{
    int argc = 0,qualc = 0;
    char *argv[32],*qualv[32];
    char *sp = str;
    int i;
    char q = '/';
    for (i = 0; i < 32; i++) argv[i] = qualv[i] = "";
    while (*sp != '\0') {
        while (*sp == ' ') sp++;
        if (*sp != '\0') {
            if (*sp == q) {
                *sp++ = '\0';
                if (*sp == q) {
                      sp++;
                      q = '\0';
                      continue;
              }
                qualv[qualc++] = sp;
            } else {
                argv[argc++] = sp;
            }
            while (*sp != ' ' && *sp != q && *sp != '\0') sp++;
            if (*sp == '\0') {
                break;
            } else {
                if (*sp != q) *sp++ = '\0';
            }
        }
    }
    if (argc > 0) return cmdexecute(argc,argv,qualc,qualv);
    return 1;
}

#ifdef VMS
#include <smgdef.h>
#include <smg$routines.h>

char *getcmd(char *inp, char *prompt)
{
    struct dsc_descriptor prompt_d = {strlen(prompt),DSC$K_DTYPE_T,
					DSC$K_CLASS_S, prompt};
    struct dsc_descriptor input_d = {1024,DSC$K_DTYPE_T,
					DSC$K_CLASS_S, inp};
    int status;
    char *retstat;
    static unsigned long key_table_id = 0;
    static unsigned long keyboard_id = 0;
    
    if (key_table_id == 0)
	{
	status = smg$create_key_table (&key_table_id);
	if (status & 1)
	    status = smg$create_virtual_keyboard (&keyboard_id);
	if (!(status & 1)) return (NULL);
	}

    status = smg$read_composed_line (&keyboard_id, &key_table_id,
		&input_d, &prompt_d, &input_d, 0,0,0,0,0,0,0);

    if (status == SMG$_EOF)
	retstat = NULL;
    else
	{
	inp[input_d.dsc_w_length] = '\0';
	retstat = inp;
	}

    return(retstat);
}
#endif /* VMS */


/* main: the simple mainline of this puppy... */

/*
 * Parse the command line to read and execute commands:
 *     ./ods2 mount scd1 $ set def [hartmut] $ copy *.com $ exit
 * '$' is used as command delimiter because it is a familiar character
 * in the VMS world. However, it should be used surounded by white spaces;
 * otherwise, a token '$copy' is interpreted by the Unix shell as a shell
 * variable. Quoting the whole command string might help:
 *     ./ods2 'mount scd1 $set def [hartmut] $copy *.com $exit'
 * If the ods2 reader should use any switches '-c' should be used for
 * the command strings, then quoting will be required:
 *     ./ods2 -c 'mount scd1 $ set def [hartmut] $ copy *.com $ exit'
 *
 * The same command concatenation can be implemented for the prompted input.
 * As for indirect command files (@), it isn't checked if one of the chained
 * commands fails. The user has to be careful, all the subsequent commands
 * are executed!
 */

int main(int argc,char *argv[])
{
    char str[2048];
    char *command_line = NULL;
    FILE *atfile = NULL;
    char *p;
    printf(" ODS2 %s\n", MODULE_IDENT);
    if (argc>1) {
           int i, l=0;
           for (i=1; i<argc; i++) {
                  if (command_line==NULL) {
                          command_line=malloc(1);
                          *command_line = '\0';
                          l= 1;
                  }
                  l+= strlen(argv[i]);
                  command_line= realloc(command_line,l+1);
                  strcat (command_line, argv[i]);
                  command_line[l-1]= ' ';
                  command_line[l++]= '\0';
           }
           if (l>1)
                  command_line[l-2]= '\0';
    } else command_line==NULL;
    while (1) {
        char *ptr;
       if (command_line) {
              static int i=0;
              int j=0;
              for (; j<sizeof str && command_line[i]; i++)
                      if (command_line[i]=='$') {
                              ++i;
                              break;
                      } else str[j++]= command_line[i];
              if (j<sizeof str)
                      str[j]= '\0';
              else str[j-1]= '\0';
              if (command_line[i]=='\0')
                      command_line= realloc(command_line,0);
       } else
{
        if (atfile != NULL) {
            if (fgets(str,sizeof(str),atfile) == NULL) {
                fclose(atfile);
                atfile = NULL;
                *str = '\0';
            } else {
                ptr = strchr(str,'\n');
                if (ptr != NULL) *ptr = '\0';
                printf("$> %s\n",str);
            }
        } else {
#ifdef VMS
	    if (getcmd (str, "$> ") == NULL) break;
#else
            printf("$> ");
            if (gets(str) == NULL) break;
#endif
        }
}
       ptr = str;
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (strlen(ptr) && *ptr != '!') {
            if (*ptr == '@') {
                if (atfile != NULL) {
                    printf("%%ODS2-W-INDIRECT, indirect indirection not permitted\n");
                } else {
                    if ((atfile = fopen(ptr + 1,"r")) == NULL) {
                        perror("%%Indirection failed");
                        printf("\n");
                    }
                }
            } else {
                if ((cmdsplit(ptr) & 1) == 0) break;
            }
        }
    }
    if (atfile != NULL) fclose(atfile);
    return 1;
}
