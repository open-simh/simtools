
/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_COPYCMD )
#define DEBUG DEBUG_COPYCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"


/******************************************************************* docopy() */

/* copy: a file copy routine */

#define copy_binary OPT_GENERIC_1
#define copy_log    OPT_GENERIC_2

qual_t copyquals[] = { {"ascii",  0,           copy_binary, NV, "Copy file in ascii mode (default)"},
                       {"binary", copy_binary, 0,           NV, "Copy file in binary mode", },
                       {"log",    copy_log,    0,           NV, "-Log files copied"},
                       {"nolog",  0,           copy_log,    NV, NULL},
                       {NULL,     0,           0,           NV, NULL}
};

param_t copypars[] = { {"from_filespec", REQ, VMSFS, NOPA,
                             "for source file. Wildcards are allowed."},
                            {"to_filespec",   REQ, LCLFS, NOPA,
                             "for destination file. Wildcards are replaced from source file name."},
                            { NULL, 0, 0, NOPA, NULL }
};

DECL_CMD(copy) {
    int sts,options;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    int filecount = 0;

    UNUSED(argc);

    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;
    fab.fab$l_nam = &nam;
    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    options = checkquals(0,copyquals,qualc,qualv);
    if( options == -1 )
        return SS$_BADPARAM;
    sts = sys_parse(&fab);
    if ( sts & STS$M_SUCCESS ) {
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;

        while( (sts = sys_search( &fab )) & STS$M_SUCCESS ) {
            int directory = 0;
            struct RAB rab = cc$rms_rab;
            struct XABDAT dat = cc$rms_xabdat;
            struct XABITM itm = cc$rms_xabitm;
            struct item_list xitems[] = {
                { XAB$_UCHAR_DIRECTORY,   sizeof(int), NULL, 0 },
                { 0, 0, NULL, 0 }
            };

            itm.xab$l_nxt = &dat;
            xitems[0].buffer = &directory;
            itm.xab$b_mode = XAB$K_SENSEMODE;
            itm.xab$l_itemlist = xitems;
            fab.fab$l_xab = &itm;

            sts = sys_open(&fab);
            fab.fab$l_xab = NULL;

            if ( !(sts & STS$M_SUCCESS) ) {
                printf("%%COPY-F-OPENFAIL, Open error: %s\n",getmsg(sts, MSG_TEXT));
                perror("-COPY-F-ERR ");
                continue;
            }
            if( directory ) {
                rsa[nam.nam$b_rsl] = '\0';
                printf( "%%COPY-I-NOTDIR, %s is directory, not copied\n", rsa );
                sys_close( &fab );
                continue;
            }

            rab.rab$l_fab = &fab;
            if( (sts = sys_connect(&rab)) & STS_M_SUCCESS ) {
                FILE *tof;
                char name[NAM$C_MAXRSS + 1];
                unsigned records = 0;
                char *out = name,*inp = argv[2]; /* unquote(argv[2]) */
                int dot = FALSE;
                while (*inp != '\0') {
                    if (*inp == '*') {
                        inp++;
                        if (dot) {
                            memcpy(out,nam.nam$l_type + 1,
                                   nam.nam$b_type - 1);
                            out += nam.nam$b_type - 1;
                        } else {
                            unsigned length = nam.nam$b_name;
                            if (*inp == '\0') length += nam.nam$b_type;
                            memcpy(out,nam.nam$l_name,length);
                            out += length;
                        }
                    } else {
                        if (*inp == '.') {
                            dot = TRUE;
                        } else {
                            if (strchr(":]\\/",*inp)) dot = FALSE;
                        }
                        *out++ = *inp++;
                    }
                }
                *out++ = '\0';
#ifndef _WIN32
                tof = openf( name,"w" );
#else
                if( !(options & copy_binary) && (fab.fab$b_rat & PRINT_ATTR) ) {
                    tof = openf(name,"w");
                } else {
                    tof = openf( name, "wb" );
                }
#endif
                if (tof == NULL) {
                    printf("%%COPY-F-OPENOUT, Could not open %s\n",name);
                    perror("-COPY-F-ERR ");
                } else {
                    char *rec;

                    if( (rec = malloc( MAXREC + 2 )) == NULL )
                        sts = SS$_INSFMEM;
                    else {
                        filecount++;
                        rab.rab$l_ubf = rec;
                        rab.rab$w_usz = MAXREC;
                        while( (sts = sys_get( &rab )) & STS$M_SUCCESS ) {
                            unsigned rsz = rab.rab$w_rsz;
                            if( !(options & copy_binary) &&
                                (fab.fab$b_rat & PRINT_ATTR) ) rec[rsz++] = '\n';
                            if( fwrite( rec, rsz, 1, tof ) == 1 ) {
                                records++;
                            } else {
                                printf( "%%COPY-F- fwrite error!!\n" );
                                perror( "-COPY-F-ERR " );
                                break;
                            }
                        }
                        free( rec );
                        rec = NULL;
                    }
                    if( fclose( tof ) ) {
                        printf( "%%COPY-F- fclose error!!\n" );
                        perror( "-COPY-F-ERR " );
                    } else {
                        time_t posixtime;
                        unsigned short timvec[7];
                        char *tz;
                        struct tm tm;
#ifdef _WIN32
                        struct _utimbuf tv;
#else
                        struct timeval tv[2];
#endif
                        if( sys$numtim( timvec, dat.xab$q_rdt ) & STS$M_SUCCESS ) {
                            tm.tm_sec = timvec[5];
                            tm.tm_min = timvec[4];
                            tm.tm_hour = timvec[3];
                            tm.tm_mday = timvec[2];
                            tm.tm_mon = timvec[1] -1;
                            tm.tm_year = timvec[0] - 1900;

                            tz = get_env( "TZ" );
                            setenv( "TZ", "", 1 );
                            tzset();
                            posixtime = mktime( &tm );
                            if( posixtime != (time_t)-1 ) {
#ifdef _WIN32
                                tv.actime =
                                    tv.modtime = posixtime;
                                (void)_utime( name, &tv );
#else
                                tv[0].tv_sec = posixtime;
                                tv[0].tv_usec = timvec[6] * 10000;
                                tv[1] = tv[0]; /* Set mtime to atime */
                                (void) utimes( name, tv );
#endif
                            }
                            if( tz != NULL )
                                setenv( "TZ", tz, 1 );
                            else
                                unsetenv( "TZ" );
                            tzset();
                            free( tz );
                        }
                    }
                }

                sys_disconnect( &rab );
                rsa[nam.nam$b_rsl] = '\0';
                if (sts == RMS$_EOF) {
                    if( options & copy_log )
                        printf(
                               "%%COPY-S-COPIED, %s copied to %s (%d record%s)\n",
                               rsa, name, records, (records == 1 ? "" : "s")
                               );
                } else {
                    printf("%%COPY-F-ERROR Status: %s for %s\n",getmsg(sts, MSG_TEXT),rsa);
                    sts = SS$_NORMAL;
                }
            }
            sys_close(&fab);
        }
        if (sts == RMS$_NMF) sts = SS$_NORMAL;
    }
    if ( sts & STS$M_SUCCESS ) {
        if (filecount > 0) {
            if( options & copy_log )
                printf("%%COPY-S-NEWFILES, %d file%s copied\n",
                       filecount,(filecount == 1 ? "" : "s"));
        } else {
            printf( "%%COPY-I-NOFILES, no files found\n" );
        }
    } else {
        printf("%%COPY-F-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}
