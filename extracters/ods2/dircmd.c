/* This is part of ODS2 written by Paul Nankervis,
 * email address:  Paulnank@au1.ibm.com

 * ODS2 is distributed freely for all members of the
 * VMS community to use. However all derived works
 * must maintain comments in their source to acknowledge
 * the contibution of the original author.
 */

#if !defined( DEBUG ) && defined( DEBUG_DIRCMD )
#define DEBUG DEBUG_DIRCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

static void dirtotal( int options, int size, int alloc );


/******************************************************************* dodir() */

#define dir_extra (dir_date | dir_fileid | dir_owner | dir_prot | dir_size)
#define  dir_date        (1 <<  0)
#define  dir_fileid      (1 <<  1)
#define  dir_owner       (1 <<  2)
#define  dir_prot        (1 <<  3)
#define  dir_size        (1 <<  4)

#define dir_grand        (1 <<  5)
#define dir_heading      (1 <<  6)
#define dir_names        (1 <<  7)
#define dir_trailing     (1 <<  8)
#define dir_full         (1 <<  9)
#define dir_total        (1 << 10)

#define dir_backup       (1 << 11)
#define dir_created      (1 << 12)
#define dir_expired      (1 << 13)
#define dir_modified     (1 << 14)
#define dir_dates        (dir_backup | dir_created | dir_expired | dir_modified)

#define  dir_allocated   (1 << 15)
#define  dir_used        (1 << 16)
#define dir_sizes        (dir_allocated | dir_used)

#define dir_default (dir_heading|dir_names|dir_trailing)

const int dir_defaults = dir_default;

static qual_t datekwds[] = { {"created",  dir_created,  0, NV, "Date file created (default)"},
                                  {"modified", dir_modified, 0, NV, "Date file modified"},
                                  {"expired",  dir_expired,  0, NV, "Date file expired"},
                                  {"backup",   dir_backup,   0, NV, "Date of last backup"},
                                  {NULL,       0,            0, NV, NULL}
};
static qual_t sizekwds[] = { {"both",       dir_used|dir_allocated, 0, NV, "Both used and allocated" },
                                  {"allocation", dir_allocated,          0, NV, "Blocks allocated to file" },
                                  {"used",       dir_used,               0, NV, "Blocks used in file" },
                                  {NULL,         0,                      0, NV, NULL}
};
qual_t dirquals[] = { {"brief",         dir_default,   ~dir_default,    NV,
                                   "Brief display - names with header/trailer (default)"},
                                  {"date",          dir_date,       dir_dates,      KV(datekwds),
                                   "-Include file date(s)", },
                                  {"nodate",        0,              dir_date,       NV, NULL, },
                                  {"file_id",       dir_fileid,     0,              NV, "-Include file ID", },
                                  {"nofile_id",     0,              dir_fileid,     NV, NULL },
                                  {"full",          dir_full|dir_heading|dir_trailing,
                                                            ~dir_full,       NV, "Include full details", },
                                  {"grand_total",   dir_grand,     ~dir_grand & ~(dir_size|dir_sizes),
                                                                             NV, "-Include only grand total",},
                                  {"nogrand_total", 0,              dir_grand,      NV, NULL},
                                  {"heading",       dir_heading,    0,              NV, "-Include heading", },
                                  {"noheading",     0,              dir_heading,    NV, NULL},
                                  {"owner",         dir_owner,      0,              NV, "-Include file owner", },
                                  {"noowner",       0,              dir_owner,      NV, NULL, },
                                  {"protection",    dir_prot,       0,              NV,
                                   "-Include file protection", },
                                  {"noprotection",  0,              dir_prot,       NV, NULL, },
                                  {"size",          dir_size,       dir_sizes,      KV(sizekwds),
                                   "-Include file size (blocks)", },
                                  {"nosize",        0,              dir_size|dir_sizes,
                                                                                    NV, NULL, },
                                  {"total",         dir_total|dir_heading,
                                                                   ~dir_total & ~(dir_size|dir_sizes),
                                                                                    NV,
                                   "Include only directory name and summary",},
                                  {"trailing",      dir_trailing,   0,              NV,
                                   "-Include trailing summary line",},
                                  {"notrailing",    0,              dir_trailing,   NV, NULL},
                                  {NULL,            0,              0,              NV, NULL} };
int dir_defopt = dir_default;

param_t dirpars[] = { {"filespec", OPT, VMSFS, NOPA, "for files to select. Wildcards are allowed."},
                                  { NULL,      0,   0,     NOPA, NULL }
};


/************************************************************ dodir() */

DECL_CMD(dir) {
    char res[NAM$C_MAXRSS + 1],rsa[NAM$C_MAXRSS + 1];
    int options;
    unsigned sts;
    int filecount = 0, nobackup = 0, contigb = 0, contig = 0, directory = 0;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    struct XABDAT dat = cc$rms_xabdat;
    struct XABFHC fhc = cc$rms_xabfhc;
    struct XABPRO pro = cc$rms_xabpro;
    struct XABITM itm = cc$rms_xabitm;
    struct item_list xitems[] = {
        { XAB$_UCHAR_NOBACKUP,    sizeof(int), NULL, 0 },
        { XAB$_UCHAR_CONTIG,      sizeof(int), NULL, 0 },
        { XAB$_UCHAR_CONTIGB,     sizeof(int), NULL, 0 },
        { XAB$_UCHAR_DIRECTORY,   sizeof(int), NULL, 0 },
        { 0, 0, NULL, 0 }
    };

    UNUSED(argc);

    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;

    fab.fab$l_nam = &nam;
    fab.fab$l_xab = &dat;

    dat.xab$l_nxt = &fhc;

    fhc.xab$l_nxt = &pro;

    pro.xab$l_nxt = &itm;
    xitems[0].buffer = &nobackup;
    xitems[1].buffer = &contig;
    xitems[2].buffer = &contigb;
    xitems[3].buffer = &directory;

    itm.xab$b_mode = XAB$K_SENSEMODE;
    itm.xab$l_itemlist = xitems;

    fab.fab$l_fna = argv[1];
    fab.fab$b_fns = strlen(fab.fab$l_fna);
    fab.fab$l_dna = "*.*;*";
    fab.fab$b_dns = strlen(fab.fab$l_dna);

    options = checkquals(dir_defopt,dirquals,qualc,qualv);
    if( options == -1 )
        return SS$_BADPARAM;

    if (options & dir_full)
        options |= dir_extra;
    if (options & (dir_total | dir_grand)) {
        options |= dir_trailing;
        if (options & (dir_extra & ~dir_size))
            options |= dir_names;
    } else {
        if (options & dir_extra)
            options |= dir_names;
    }
    if( (options & dir_size) && !(options & dir_sizes) )
        options |= dir_used;
    if( (options & dir_date) && !(options & dir_dates) )
        options |= dir_created;

    sts = sys_parse(&fab);
    if ( sts & STS$M_SUCCESS ) {
        char dir[NAM$C_MAXRSS + 1];
        int namelen;
        int dirlen = 0;
        int dirfiles = 0, dircount = 0;
        int dirblocks = 0, diralloc = 0, totblocks = 0, totalloc = 0;
        int printcol = 0;
#if DEBUG
        res[nam.nam$b_esl] = '\0';
        printf("Parse: %s\n",res);
#endif
        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = FAB$M_NAM;
        while ( ( sts = sys_search( &fab ) ) & STS$M_SUCCESS ) {

            if (dirlen != nam.nam$b_dev + nam.nam$b_dir ||
                memcmp(rsa, dir, nam.nam$b_dev + nam.nam$b_dir) != 0) {
                if (dirfiles > 0 && (options & dir_trailing)) {
                    if (printcol > 0) printf("\n");
                    printf("\nTotal of %d file%s",dirfiles,(dirfiles == 1 ? "" : "s"));
                    dirtotal( options, dirblocks, diralloc );
                    fputs(".\n",stdout);
                }
                dirlen = nam.nam$b_dev + nam.nam$b_dir;
                memcpy(dir,rsa,dirlen);
                dir[dirlen] = '\0';
                if( options & dir_heading) printf("\nDirectory %s\n\n",dir);
                filecount += dirfiles;
                totblocks += dirblocks;
                totalloc += diralloc;
                dircount++;
                dirfiles = 0;
                dirblocks = 0;
                diralloc = 0;
                printcol = 0;
            }
            rsa[nam.nam$b_rsl] = '\0';
            namelen = nam.nam$b_name + nam.nam$b_type + nam.nam$b_ver;
            if ((options & (dir_heading|dir_extra|dir_full)) == dir_heading) {
                if( options & dir_names ) {
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
                }
            } else {
                if (options & dir_names) {
                    if ( (options & dir_heading) == 0 ) printf( "%s",dir );
                    if (namelen > 18) {
                        printf("%s",rsa + dirlen);
                        if( options & dir_extra )
                            printf( "\n                   " );
                    } else {
                        printf("%-19s",rsa + dirlen);
                    }
                }
                sts = sys_open(&fab);
                if ( !( sts & STS$M_SUCCESS ) ) {
                    printf("%%ODS2-E-OPENERR, Open error: %s\n", getmsg(sts, MSG_TEXT));
                } else {
                    sts = sys_close(&fab);
                    if (options & dir_fileid) {
                        char fileid[30];  /* 24 bits, 16 bits. 8 bits = 8 + 5 + 3 digits = 16 + (,,)\0 => 21 */
                        if( options & dir_full) printf( "         File ID:" );
                        (void) snprintf(fileid,sizeof(fileid),"(%d,%d,%d)",
                            (nam.nam$b_fid_nmx << 16) | nam.nam$w_fid_num,
                            nam.nam$w_fid_seq, nam.nam$b_fid_rvn );
                        printf("  %-22s",fileid);
                    }
                    diralloc += fab.fab$l_alq;
                    if (options & dir_used) {
                        unsigned filesize = fhc.xab$l_ebk;
                        if (fhc.xab$w_ffb == 0) filesize--;
                        dirblocks += filesize;

                        if (options & dir_names) { /* Don't print w/o names (e.g. totals) */
                            if( options & dir_full) printf( "\nSize: " );
                            printf("%9d",filesize);
                            if( options & (dir_allocated|dir_full)) printf( "/%-9d   ",fab.fab$l_alq );
                        }
                    } else {
                        if ( (options & (dir_allocated|dir_names)) == (dir_allocated|dir_names)) {
                            printf( "%9d", fab.fab$l_alq );
                        }
                    }
#define pprot(val,pos,del) {\
    unsigned int v = ~(((val) >> (pos)) & xab$m_prot); \
    if( v & xab$m_noread )  printf( "R" ); \
    if( v & xab$m_nowrite ) printf( "W" ); \
    if( v & xab$m_noexe )   printf( "E" ); \
    if( v & xab$m_nodel )   printf( "D" ); \
    printf( del );                         \
                        }
                    if (options & dir_full) {
                        int pos = 0;

                        printf( "Owner:    [%o,%o]\n", ((pro.xab$l_uic>>16)&0xFFFF), pro.xab$l_uic&0xFFFF);
                        printf( "Created:  " ); prvmstime( dat.xab$q_cdt, "\n" );
                        printf( "Revised:  " ); prvmstime( dat.xab$q_rdt, " (" ); printf( "%u)\n", dat.xab$w_rvn );
                        printf( "Expires:  " ); prvmstime( dat.xab$q_edt, "\n" );
                        printf( "Backup:   " ); prvmstime( dat.xab$q_bdt, "\n" );
                        pwrap(  &pos, "File organization:  " );
                        switch( fab.fab$b_org ) {
                        case FAB$C_SEQ:
                            pwrap(  &pos, "Sequential" ); break;
                        case FAB$C_REL:
                            pwrap(  &pos, "Relative" /*, Maximum record number %u", fab.fab$l_mrn*/ ); break;
                        case FAB$C_IDX:
                            pwrap(  &pos, "Indexed" ); break; /*, Prolog: 3, Using 4 keys\nIn 3 areas */
                        default:
                            pwrap(  &pos, "Unknown (%u)", fab.fab$b_org ); break;
                        }
                        /* File attributes:    Allocation: 372, Extend: 3, Maximum bucket size: 3, Global buffer count: 0, No version limit
                                               Contiguous best try */
                        pwrap(  &pos, "\nFile attributes:    " );
                        pwrap(  &pos, "Allocation: %u", fab.fab$l_alq );
                        pwrap(  &pos, ", Extend: %u", fab.fab$w_deq );
                        /* Missing: , Maximum bucket size: n*/
                        pwrap(  &pos, ", Global buffer count: %u", fab.fab$w_gbc );
                        if( fhc.xab$w_verlimit == 0 || fhc.xab$w_verlimit == 32767 )
                            pwrap(  &pos, ", No %sversion limit", directory? "default ": "" );
                        else
                            pwrap(  &pos, ", %sersion limit: %u", (directory? "Default v": "V"), fhc.xab$w_verlimit );
                        if( contig )
                            pwrap(  &pos, ", Contiguous" );
                        if( contigb )
                            pwrap(  &pos, ", Contiguous best try" );
                        if( nobackup )
                            pwrap( &pos, ", Backups disabled" );
                        if( directory )
                            pwrap( &pos, ", Directory file" );
                        pwrap(  &pos, "\n" );

                        pwrap(  &pos, "Record format:      " );
                        switch( fab.fab$b_rfm ) {
                        default:
                        case FAB$C_UDF:
                            pwrap(  &pos, "Undefined" ); break;
                        case FAB$C_FIX:
                            pwrap(  &pos, "Fixed length %u byte records", fab.fab$w_mrs ); break;
                        case FAB$C_VAR:
                            pwrap(  &pos, "Variable length, maximum %u bytes", fab.fab$w_mrs ); break;
                        case FAB$C_VFC:
                            pwrap(  &pos, "Variable length, fixed carriage control %u, maximum %u bytes", (fab.fab$b_fsz? fab.fab$b_fsz: 2), fab.fab$w_mrs ); break;
                        case FAB$C_STM:
                            pwrap(  &pos, "Stream" ); break;
                        case FAB$C_STMLF:
                            pwrap(  &pos, "Stream-LF" ); break;
                        case FAB$C_STMCR:
                            pwrap(  &pos, "Stream-CR" ); break;
                        }
                        pwrap(  &pos, "\n" );

                        pwrap(  &pos, "Record attributes:  " );
                        if( fab.fab$b_rat == 0 )
                            pwrap(  &pos, "None" );
                        else {
                            const char *more = "";
                            if( fab.fab$b_rat & FAB$M_FTN ) {
                                pwrap(  &pos, "%sFortran carriage control", more );
                                more = ", ";
                            }
                            if( fab.fab$b_rat & FAB$M_CR ) {
                                pwrap(  &pos, "%sCarriage return carriage control", more );
                                more = ", ";
                            }
                            if( fab.fab$b_rat & FAB$M_PRN ) {
                                pwrap(  &pos, "%sPrinter control", more );
                                more = ", ";
                            }
                            if( fab.fab$b_rat & FAB$M_BLK ) {
                                pwrap(  &pos, "%sNon-spanned", more );
                            }
                        }
                        printf( "\n" );
                        /*
RMS attributes:     None
Journaling enabled: None
*/
                        printf( "File protection:    System:" );
                        pprot(pro.xab$w_pro,xab$v_system,", Owner:")
                        pprot(pro.xab$w_pro,xab$v_owner,", Group:")
                        pprot(pro.xab$w_pro,xab$v_group,", World:")
                        pprot(pro.xab$w_pro,xab$v_world,"\n");
                    } else { /* !full */
                        if (options & dir_date) {
                            if( options & dir_created )
                                sts = prvmstime( dat.xab$q_cdt, NULL );
                            if( options & dir_modified )
                                sts = prvmstime( dat.xab$q_rdt, NULL );
                            if( options & dir_expired )
                                sts = prvmstime( dat.xab$q_edt, NULL );
                            if( options & dir_backup )
                                sts = prvmstime( dat.xab$q_bdt, NULL );
                        }
                        if (options & dir_owner) {
                            printf("  [%o,%o]", ((pro.xab$l_uic>>16)&0xFFFF), pro.xab$l_uic&0xFFFF);
                        }
                        if (options & dir_prot) {
                            printf( "  (" );
                        pprot(pro.xab$w_pro,xab$v_system,",")
                        pprot(pro.xab$w_pro,xab$v_owner,",")
                        pprot(pro.xab$w_pro,xab$v_group,",")
                        pprot(pro.xab$w_pro,xab$v_world,")")
                        }
                    }
#undef pprot
                    if (options & dir_names)
                        printf("\n");
                }
            }
            dirfiles++;
        }
        if (sts == RMS$_NMF) sts = SS$_NORMAL;
        if (printcol > 0) printf("\n");
        if (options & dir_trailing) {
            printf("\nTotal of %d file%s",dirfiles,(dirfiles == 1 ? "" : "s"));
            dirtotal( options, dirblocks, diralloc );
           fputs(".\n",stdout);
        }
        filecount += dirfiles;
        totblocks += dirblocks;
        totalloc += diralloc;
        if (options & dir_grand) {
            printf("\nGrand total of %d director%s, %d file%s",
                   dircount,(dircount == 1 ? "y" : "ies"),
                   filecount,(filecount == 1 ? "" : "s"));
            dirtotal( options, totblocks, totalloc );
            fputs(".\n",stdout);
        }
    }
    if ( sts & STS$M_SUCCESS ) {
        if (filecount < 1) printf("%%DIRECT-W-NOFILES, no files found\n");
    } else {
        printf("%%DIR-E-ERROR Status: %s\n",getmsg(sts, MSG_TEXT));
    }
    return sts;
}

/*********************************************************** dirtotal() */

static void dirtotal( int options, int size, int alloc ) {
    if ( !(options & dir_size) )
        return;
    fputs( ", ", stdout );

    if ( options & dir_used )
        printf( "%d", size );
    if ( options & dir_allocated ) {
        if (options & dir_used) printf( "/" );
        printf( "%d", alloc );
    }
    if ((options & dir_dates) == dir_dates)
        printf( " block%s",(size ==1 &&  alloc == 1 ? "" : "s"));
    else
        printf( " block%s",(((options & dir_used) && size == 1) ||
                            ((options & dir_allocated) && alloc == 1))? "" : "s");
    return;
}
