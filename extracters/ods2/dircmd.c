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

#if !defined( DEBUG ) && defined( DEBUG_DIRCMD )
#define DEBUG DEBUG_DIRCMD
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

#include "cmddef.h"

#include "f11def.h"

struct dirstats {
    uint32_t filecount;
    uint32_t totblocks, totalloc, dircount;
};

static vmscond_t dir_1_arg( FILE *of, options_t options, struct dirstats *stats,
                            char *filespec );
static void dirtotal( FILE *of, options_t options, uint32_t size, uint32_t alloc );

static vmscond_t get_fileheader( FILE *of, struct dsc$descriptor *devname,
                                struct fiddef *fid, uint32_t *clustersize, struct HEAD **buf );
static void print_fileheader( FILE *of, struct HEAD *fhd, uint32_t *vbn, uint32_t clustersize );

/******************************************************************* dodir() */

#define dir_extra (dir_date | dir_fileid | dir_owner | dir_prot | dir_size)
#define  dir_date        OPT_SHARED_1
#define  dir_fileid      OPT_SHARED_2
#define  dir_owner       OPT_SHARED_3
#define  dir_prot        OPT_SHARED_4
#define  dir_size        OPT_SHARED_5

#define dir_grand        OPT_SHARED_6
#define dir_heading      OPT_SHARED_7
#define dir_names        OPT_SHARED_8
#define dir_trailing     OPT_SHARED_9
#define dir_full         OPT_SHARED_10
#define dir_total        OPT_SHARED_11

#define dir_backup       OPT_SHARED_12
#define dir_created      OPT_SHARED_13
#define dir_expired      OPT_SHARED_14
#define dir_modified     OPT_SHARED_15
#define dir_dates        (dir_backup | dir_created | dir_expired | dir_modified)

#define  dir_allocated   OPT_SHARED_16
#define  dir_used        OPT_SHARED_17
#define dir_sizes        (dir_allocated | dir_used)

#define dir_mappings     OPT_SHARED_19
#define dir_pack         OPT_SHARED_20

#define dir_default (dir_heading|dir_names|dir_trailing|dir_pack)

const int dir_defaults = dir_default;
static char *outfile;

static qual_t
datekwds[] = { {"created",  dir_created,  0, NV,
                "commands directory qual_date date_created"},
               {"modified", dir_modified, 0, NV,
                "commands directory qual_date date_modified"},
               {"expired",  dir_expired,  0, NV,
                "commands directory qual_date date_expired"},
               {"backup",   dir_backup,   0, NV,
                "commands directory qual_date date_backup"},

               {NULL,       0,            0, NV, NULL}
};
static qual_t
sizekwds[] = { {"all",        dir_used|dir_allocated, 0, NV,
                "commands directory qual_size size_both" },
               {"allocation", dir_allocated,          0, NV,
                "commands directory qual_size size_alloc" },
               {"used",       dir_used,               0, NV,
                "commands directory qual_size size_used" },

               {NULL,         0,                      0, NV, NULL}
};
qual_t
dirquals[] = { {"brief",         dir_default,   ~dir_default,    NV,
                "commands directory qual_brief"},
               {"date",          dir_date,       dir_dates,      VOPT(KV(datekwds)),
                "-commands directory qual_date", },
               {"nodate",        0,              dir_date,       NV, NULL, },
               {"file_id",       dir_fileid,     0,              NV,
                "-commands directory qual_fileid"},
               {"nofile_id",     0,              dir_fileid,     NV, NULL },
               {"full",          dir_full|dir_heading|dir_trailing,
                                          ~(dir_full|dir_pack),  NV,
                "commands directory qual_full"},
               {"grand_total",   dir_grand,     ~dir_grand & ~(dir_size|dir_sizes),
                NV, "-commands directory qual_grand"},
               {"nogrand_total", 0,              dir_grand,      NV, NULL},
               {"heading",       dir_heading,    0,              NV,
                "-commands directory qual_heading"},
               {"noheading",     0,              dir_heading,    NV, NULL},
               {"owner",         dir_owner,      0,              NV,
                "-commands directory qual_owner"},
               {"noowner",       0,              dir_owner,      NV, NULL, },
               {"output",        0,              0,              SV(&outfile),
                "commands directory qual_output"},
               {"pack",          dir_pack,       0,              NV,
                "-commands directory qual_pack"},
               {"nopack",        dir_full,       dir_pack|dir_heading|dir_trailing|dir_grand,
                                                                 NV, NULL, },
               {"protection",    dir_prot,       0,              NV,
                "-commands directory qual_protection"},
               {"noprotection",  0,              dir_prot,       NV, NULL, },
               {"size",          dir_size,       dir_sizes,      VOPT(KV(sizekwds)),
                "-commands directory qual_size"},
               {"nosize",        0,              dir_size|dir_sizes,
                NV, NULL, },
               {"total",         dir_total|dir_heading,
                ~dir_total & ~(dir_size|dir_sizes),
                NV,
                "commands directory qual_total",},
               {"trailing",      dir_trailing,   0,              NV,
                "-commands directory qual_trailing",},
               {"notrailing",    0,              dir_trailing,   NV, NULL},
               {"map_area",      dir_mappings|dir_full|dir_heading|dir_trailing,
                0,              NV, "-commands directory qual_map"},
               {"nomap_area",    0,              dir_mappings,   NV, NULL},

               {NULL,            0,              0,              NV, NULL} };

defqualsp_t dir_defopt = NULL;

param_t
dirpars[] = { {"filespec", OPT | NOLIM, FSPEC, NOPA, "commands directory filespec"},

              { NULL,      0,           0,     NOPA, NULL }
};


/************************************************************ dodir() */

DECL_CMD(dir) {
    FILE *of = stdout;
    options_t options;
    vmscond_t status;
    struct dirstats stats;

    memset( &stats, 0, sizeof( stats ) );
    outfile = NULL;

    if( dir_defopt != NULL ) {
        if( $FAILS(status = checkquals( &options, dir_defaults, dirquals,
                                        dir_defopt->qualc, dir_defopt->qualv )) ) {
            return status;
        }
    }
    else
        options = dir_defaults;

    if( $FAILS(status = checkquals( &options, options, dirquals, qualc, qualv )) ) {
        return status;
    }

    if( outfile != NULL ) {
        of = openf( outfile, "w" );
        if( of == NULL ) {
            int err;

            err = errno;
            printmsg( DIRECT_OPENOUT, 0, outfile );
            return printmsg( ODS2_OSERROR, MSG_CONTINUE, DIRECT_OPENOUT,
                strerror( err ) );
        }
    }

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

    --argc;
    ++argv;
    do {
        status = dir_1_arg( of, options, &stats, argv++[0] );
    } while( argc && --argc );

    if( (options & dir_grand) ||
        (stats.dircount > 1 && (options & dir_trailing)) ) {
        printmsg( DIRECT_GRANDTOT, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of, stats.dircount, stats.filecount );
        dirtotal( of, options, stats.totblocks, stats.totalloc );
        fputs( ".\n", of );
    }
    if( options & dir_pack ) {
        if( $SUCCESSFUL( status ) ) {
            if( stats.filecount < 1 )
                status = printmsg( DIRECT_NOFILES, MSG_TOFILE, of );
        } else {
            status = printmsg( status, MSG_TOFILE, of );
        }
    }

    if( outfile != NULL )
        fclose( of );
    return status;
}

/***********************************************************dir_1_arg() */

static vmscond_t dir_1_arg( FILE *of, options_t options, struct dirstats *stats,
                            char *filespec ) {
    char res[NAM$C_MAXRSS + 1] = { "" }, rsa[NAM$C_MAXRSS + 1] = { "" };
    vmscond_t status;
    uint32_t nobackup = 0, contigb = 0, contig = 0, directory = 0,
        writeback = 0, readcheck = 0, writecheck = 0, locked = 0, spool = 0,
        badblock = 0, markdel = 0;
    uint16_t retlen = 0;
    struct NAM nam = cc$rms_nam;
    struct FAB fab = cc$rms_fab;
    struct XABDAT dat = cc$rms_xabdat;
    struct XABFHC fhc = cc$rms_xabfhc;
    struct XABPRO pro = cc$rms_xabpro;
    struct XABITM itm = cc$rms_xabitm;
    struct item_list xitems[] = {
        { XAB$_UCHAR_NOBACKUP,    sizeof(nobackup), NULL, 0 },
        { XAB$_UCHAR_CONTIG,      sizeof(contig), NULL, 0 },
        { XAB$_UCHAR_CONTIGB,     sizeof(contigb), NULL, 0 },
        { XAB$_UCHAR_DIRECTORY,   sizeof(directory), NULL, 0 },
        { XAB$_UCHAR_WRITEBACK,   sizeof(writeback), NULL, 0 },
        { XAB$_UCHAR_READCHECK,   sizeof(readcheck), NULL, 0 },
        { XAB$_UCHAR_WRITECHECK,  sizeof(writecheck), NULL, 0 },
        { XAB$_UCHAR_LOCKED,      sizeof(locked), NULL, 0 },
        { XAB$_UCHAR_SPOOL,       sizeof(spool), NULL, 0 },
        { XAB$_UCHAR_MARKDEL,     sizeof(markdel), NULL, 0 },
        { XAB$_UCHAR_BADBLOCK,    sizeof(badblock), NULL, 0 },
        { 0, 0, NULL, 0 }
    };

    nam.nam$l_esa = res;
    nam.nam$b_ess = NAM$C_MAXRSS;

    fab.fab$l_nam = &nam;
    fab.fab$l_xab = &dat;

    dat.xab$l_nxt = &fhc;

    fhc.xab$l_nxt = &pro;

    pro.xab$l_nxt = &itm;
    xitems[0].buffer  = &nobackup;   xitems[0].retlen = &retlen;
    xitems[1].buffer  = &contig;     xitems[1].retlen = &retlen;
    xitems[2].buffer  = &contigb;    xitems[2].retlen = &retlen;
    xitems[3].buffer  = &directory;  xitems[3].retlen = &retlen;
    xitems[4].buffer  = &writeback;  xitems[4].retlen = &retlen;
    xitems[5].buffer  = &readcheck;  xitems[5].retlen = &retlen;
    xitems[6].buffer  = &writecheck; xitems[6].retlen = &retlen;
    xitems[7].buffer  = &locked;     xitems[7].retlen = &retlen;
    xitems[8].buffer  = &spool;      xitems[8].retlen = &retlen;
    xitems[9].buffer  = &markdel;    xitems[9].retlen = &retlen;
    xitems[10].buffer = &badblock;  xitems[10].retlen = &retlen;

    itm.xab$b_mode = XAB$K_SENSEMODE;
    itm.xab$l_itemlist = xitems;

    fab.fab$l_fna = filespec;
    fab.fab$b_fns = filespec? (uint8_t)strlen(fab.fab$l_fna): 0;
    fab.fab$l_dna = "*.*;*";
    fab.fab$b_dns = (uint8_t)strlen(fab.fab$l_dna);

    if( $SUCCESSFUL(status = sys_parse(&fab)) ) {
        char dir[NAM$C_MAXRSS + 1] = { "" };
        size_t namelen;
        size_t dirlen = 0;
        uint32_t dirfiles = 0;
        uint32_t dirblocks = 0, diralloc = 0;
        size_t printcol = 0;

        nam.nam$l_rsa = rsa;
        nam.nam$b_rss = NAM$C_MAXRSS;
        fab.fab$l_fop = 0;
        while( $SUCCESSFUL(status) && $SUCCESSFUL(status = sys_search( &fab )) ) {

            if (dirlen != (size_t)nam.nam$b_dev + nam.nam$b_dir ||
                memcmp(rsa, dir, (size_t)nam.nam$b_dev + nam.nam$b_dir) != 0) {
                if (dirfiles > 0 && (options & dir_trailing)) {
                    if (printcol > 0)
                        fputc( '\n', of );
                    printmsg( DIRECT_FILETOTAL, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of, dirfiles );
                    dirtotal( of, options, dirblocks, diralloc );
                    fputs(".\n", of);
                }
                dirlen = (size_t)nam.nam$b_dev + nam.nam$b_dir;
                memcpy(dir, rsa, dirlen);
                dir[dirlen] = '\0';
                if( options & dir_heading)
                    printmsg( DIRECT_DIRHEAD, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of, dir );
                stats->filecount += dirfiles;
                stats->totblocks += dirblocks;
                stats->totalloc += diralloc;
                stats->dircount++;
                dirfiles = 0;
                dirblocks = 0;
                diralloc = 0;
                printcol = 0;
            }
            rsa[nam.nam$b_rsl] = '\0';
            namelen = (size_t)nam.nam$b_name + nam.nam$b_type + nam.nam$b_ver;
            if ((options & (dir_heading|dir_extra|dir_full)) == dir_heading) {
                if( options & dir_names ) {
                    if (printcol > 0) {
                        size_t newcol = (printcol + 20) / 20 * 20;
                        if (newcol + namelen >= 80) {
                            fputs( "\n", of );
                            printcol = 0;
                        } else {
                            fprintf( of, "%*s", (int)(newcol - printcol), " " );
                            printcol = newcol;
                        }
                    }
                    fputs( rsa + dirlen, of );
                    printcol += namelen;
                }
            } else {
                if (options & dir_names) {
                    if ( (options & dir_heading) == 0 )
                        fprintf( of, "%s", dir );
                    if( options & dir_pack ) {
                        if( namelen > 18 ) {
                            fprintf( of, "%s", rsa + dirlen );
                            if( options & dir_extra )
                                fprintf( of, "\n                   " );
                        } else {
                            fprintf( of, "%-19s", rsa + dirlen );
                        }
                    } else
                        fprintf( of, "%s\n", rsa + dirlen );
                }
                if( $FAILS(status = sys_open(&fab)) ) {
                    printmsg( status, MSG_TOFILE, of );
                    status = SS$_NORMAL;
                } else {
                    status = sys_close(&fab);
                    if (options & dir_fileid) {
                        char fileid[30];  /* 24 bits, 16 bits. 8 bits = 8 + 5 + 3 digits = 16 + (,,)\0 => 21 */
                        if( options & dir_full)
                            fprintf( of, "%sFile ID:", (options & dir_pack)? "         ": "" );
                        (void) snprintf(fileid, sizeof(fileid), "(%u,%u,%u)",
                                        (nam.nam$w_fid.fid$b_nmx << 16) | nam.nam$w_fid.fid$w_num,
                                        nam.nam$w_fid.fid$w_seq, nam.nam$w_fid.fid$b_rvn );
                        fprintf( of, "  %-22s", fileid );
                    }
                    diralloc += fab.fab$l_alq;
                    if (options & dir_used) {
                        uint32_t filesize;

                        filesize = fhc.xab$l_ebk;  /* VBN of eof */
                        if( filesize == 0 )        /* none, use allocated size */
                            filesize = fab.fab$l_alq;
                        else {
                            if( fhc.xab$w_ffb == 0 )
                                filesize--;
                        }
                        dirblocks += filesize;

                        if (options & dir_names) { /* Don't print w/o names (e.g. totals) */
                            if( options & dir_full)
                                printmsg( DIRECT_FULLSIZE, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of, filesize );
                            else
                                fprintf( of, "%9u", filesize );
                            if( options & (dir_allocated|dir_full))
                                fprintf( of, "/%-9u", fab.fab$l_alq );
                            if( options & (dir_allocated | dir_full) ) {
                                if( options & dir_pack )
                                    fputs( "   ", of );
                                else
                                    fputc( '\n', of );
                            }
                        }
                    } else {
                        if ( (options & (dir_allocated|dir_names)) == (dir_allocated|dir_names)) {
                            fprintf( of, "%9u", fab.fab$l_alq );
                        }
                    }
#define pprot(val, pos, del) do {                                         \
                        unsigned int v = ~(((val) >> (pos)) & xab$m_prot); \
                        if( v & xab$m_noread )  fprintf( of, "R" );          \
                        if( v & xab$m_nowrite ) fprintf( of, "W" );          \
                        if( v & xab$m_noexe )   fprintf( of, "E" );          \
                        if( v & xab$m_nodel )   fprintf( of, "D" );          \
                        fprintf( of, del );                                  \
                    } while( 0 )

                    if (options & dir_full) {
                        int pos = 0;

                        printmsg( DIRECT_OWNER, MSG_TEXT|MSG_TOFILE, of,
                                  ((pro.xab$l_uic>>16)&0xFFFF), pro.xab$l_uic&0xFFFF);
                        printmsg( DIRECT_CREATED, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of );
                        prvmstime( of, dat.xab$q_cdt, "\n" );
                        printmsg( DIRECT_REVISED, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of );
                        prvmstime( of, dat.xab$q_rdt, " (" ); fprintf( of, "%u)\n", dat.xab$w_rvn );
                        printmsg( DIRECT_EXPIRES, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of );
                        prvmstime( of, dat.xab$q_edt, "\n" );
                        printmsg( DIRECT_BACKUP, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of );
                        prvmstime( of, dat.xab$q_bdt, "\n" );

#define MSG(x) getmsg( DIRECT_ ## x, MSG_TEXT|MSG_NOCRLF )
                        pwrap( of, &pos, MSG( FILEORG ) );
                        switch( fab.fab$b_org ) {
                        case FAB$C_SEQ:
                            pwrap( of, &pos, MSG( SEQORG) ); break;
                        case FAB$C_REL:
                            pwrap( of, &pos, MSG( RELORG ) /*, Maximum record number %u", fab.fab$l_mrn*/ ); break;
                        case FAB$C_IDX:
                            pwrap( of, &pos, MSG( IDXORG ) ); break; /*, Prolog: 3, Using 4 keys\nIn 3 areas */
                        default:
                            pwrap( of, &pos, MSG( UNKORG ), fab.fab$b_org ); break;
                        }

                        pwrap( of, &pos, MSG( FILEATT ) );
                        pwrap( of, &pos, MSG( ALLOC ), fab.fab$l_alq );
                        pwrap( of, &pos, MSG( EXTEND ), fab.fab$w_deq );
                        /* Missing: , Maximum bucket size: n*/
                        pwrap( of, &pos, MSG( GBC ), fab.fab$w_gbc );

                        if( fhc.xab$w_verlimit == 0 || fhc.xab$w_verlimit == 32767 ) {
                            if( directory )
                                pwrap( of, &pos, MSG(NODIRLIMIT) );
                            else
                                pwrap( of, &pos, MSG(NOVERLIMIT ) );
                        } else {
                            if( directory )
                                pwrap( of, &pos, MSG(DIRLIMIT), fhc.xab$w_verlimit );
                            else
                                pwrap( of, &pos, MSG(VERLIMIT), fhc.xab$w_verlimit );
                        }
#define pattr(bit, text)                                \
                        if( bit )                       \
                            pwrap( of, &pos, MSG(text) )
                        pattr( contig, CONTIG );
                        pattr( contigb, CONTIGB );
                        pattr( nobackup, NOBACKUP );
                        pattr( directory, DIRECTORY );
                        pattr( writeback, WRITEBACK );
                        pattr( readcheck, READCHECK );
                        pattr( writecheck, WRITECHECK );
                        pattr( locked, LOCKED );
                        pattr( spool, SPOOL );
                        pattr( markdel, MARKDEL );
                        pattr( badblock, BADBLOCK );
                        pwrap( of, &pos, "\n" );
#undef pattr
                        pwrap( of, &pos, MSG(EOFPOS), fhc.xab$l_ebk, fhc.xab$w_ffb );

                        pwrap( of, &pos, MSG(RECFMT) );
                        switch( fab.fab$b_rfm ) {
                        default:
                        case FAB$C_UDF:
                            pwrap( of, &pos, MSG(RECUDF) ); break;
                        case FAB$C_FIX:
                            pwrap( of, &pos, MSG(RECFIX), fab.fab$w_mrs ); break;
                        case FAB$C_VAR:
                            pwrap( of, &pos, MSG(RECVAR), fhc.xab$w_lrl ); break;
                        case FAB$C_VFC:
                            pwrap( of, &pos, MSG(RECVFC),
                                    (fab.fab$b_fsz? fab.fab$b_fsz: 2),
                                    fhc.xab$w_lrl ); break;
                        case FAB$C_STM:
                            pwrap( of, &pos, MSG(RECSTM) ); break;
                        case FAB$C_STMLF:
                            pwrap( of, &pos, MSG(RECSTMLF) ); break;
                        case FAB$C_STMCR:
                            pwrap( of, &pos, MSG(RECSTMCR) ); break;
                        }
                        pwrap( of, &pos, "\n" );

                        pwrap( of, &pos, MSG(RECATT) );
                        if( fab.fab$b_rat == 0 )
                            pwrap( of, &pos, MSG(RECATT_NONE) );
                        else {
                            const char *more = "";
                            if( fab.fab$b_rat & FAB$M_FTN ) {
                                pwrap( of, &pos, MSG(RECATT_FTN), more );
                                more = ", ";
                            }
                            if( fab.fab$b_rat & FAB$M_CR ) {
                                pwrap( of, &pos, MSG(RECATT_CR), more );
                                more = ", ";
                            }
                            if( fab.fab$b_rat & FAB$M_PRN ) {
                                pwrap( of, &pos, MSG(RECATT_PRN), more );
                                more = ", ";
                            }
                            if( fab.fab$b_rat & FAB$M_BLK ) {
                                pwrap( of, &pos, MSG(RECATT_BLK), more );
                            }
                        }
                        fprintf( of, "\n" );
                        /*
                         * RMS attributes:     None
                         * Journaling enabled: None
                         */
                        fprintf( of, MSG(PROT_SYS) );
                        pprot(pro.xab$w_pro, xab$v_system, MSG(PROT_OWNER) );
                        pprot(pro.xab$w_pro, xab$v_owner, MSG(PROT_GROUP) );
                        pprot(pro.xab$w_pro, xab$v_group, MSG(PROT_WORLD) );
                        pprot(pro.xab$w_pro, xab$v_world, "\n");

                        if( options & dir_mappings ) {
                            struct dsc$descriptor devnam;
                            unsigned clustersize = 0;
                            struct HEAD *buf = NULL;
                            struct fiddef lastfid, fid;
                            unsigned vbn = 1;

                            memset( &devnam, 0, sizeof( devnam ) );
                            devnam.dsc$b_dtype = DSC$K_DTYPE_T;
                            devnam.dsc$b_class = DSC$K_CLASS_S;
                            devnam.dsc$w_length = nam.nam$b_dev;
                            devnam.dsc$a_pointer = nam.nam$l_dev;

                            fid = nam.nam$w_fid;
                            do {
                                if( $SUCCESSFUL(status = get_fileheader( of,
                                                                         &devnam,
                                                                         &fid,
                                                                         &clustersize,
                                                                         &buf )) ) {
                                    print_fileheader( of, buf, &vbn, clustersize );
                                }
                                lastfid = fid;
                                fid = buf->fh2$w_ext_fid;
                                free( buf );
                                buf = NULL;
                            } while( $SUCCESSFUL(status) &&
                                     (fid.fid$w_num | fid.fid$w_seq | fid.fid$b_rvn |
                                      fid.fid$b_nmx) != 0 &&
                                     memcmp( &fid, &lastfid, sizeof( fid ) ) != 0 );
                        }
                    } else { /* !full */
                        if (options & dir_date) {
                            if( options & dir_created )
                                status = prvmstime( of, dat.xab$q_cdt, NULL );
                            if( options & dir_modified )
                                status = prvmstime( of, dat.xab$q_rdt, NULL );
                            if( options & dir_expired )
                                status = prvmstime( of, dat.xab$q_edt, NULL );
                            if( options & dir_backup )
                                status = prvmstime( of, dat.xab$q_bdt, NULL );
                        }
                        if (options & dir_owner) {
                            printmsg( DIRECT_UIC, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of,
                                      ((pro.xab$l_uic>>16)&0xFFFF), pro.xab$l_uic&0xFFFF );
                        }
                        if (options & dir_prot) {
                            fprintf( of, "  (" );
                            pprot(pro.xab$w_pro, xab$v_system, ",");
                            pprot(pro.xab$w_pro, xab$v_owner, ",");
                            pprot(pro.xab$w_pro, xab$v_group, ",");
                            pprot(pro.xab$w_pro, xab$v_world, ")");
                        }
                    }
#undef pprot
                    if (options & dir_names)
                        fprintf( of, "\n" );
                }
            }
            dirfiles++;
        }
        if( status == RMS$_NMF )
            status = SS$_NORMAL;
        if( printcol > 0 ) fprintf( of, "\n" );
        if (options & dir_trailing) {
            printmsg( DIRECT_FILETOTAL, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of, dirfiles );
            dirtotal( of, options, dirblocks, diralloc );
            fputs( ".\n", of );
        }
        stats->filecount += dirfiles;
        stats->totblocks += dirblocks;
        stats->totalloc += diralloc;
    }

    fab.fab$b_fns =
        nam.nam$b_ess =
        fab.fab$b_dns = 0;
    nam.nam$b_nop = NAM$M_SYNCHK;
    (void) sys_parse( &fab );

    return status;
}

/*********************************************************** dirtotal() */

static void dirtotal( FILE *of, options_t options, uint32_t size, uint32_t alloc ) {
    if ( !(options & dir_size) )
        return;
    fputs( ", ", of );

    switch( options & (dir_used | dir_allocated) ) {
    case dir_used:
        printmsg( DIRECT_USEDTOT, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of, size );
        break;
    case dir_allocated:
        printmsg( DIRECT_ALLOCTOT, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of, alloc );
        break;
    case dir_used | dir_allocated:
        printmsg( DIRECT_BOTHSIZE, MSG_TEXT|MSG_NOCRLF|MSG_TOFILE, of, size, alloc );
        break;
    default:
        return;
    }
}

/*********************************************************** get_fileheader() */
static unsigned get_fileheader( FILE *of, struct dsc$descriptor *devname,
                                struct fiddef *fid, unsigned *clustersize,
                                struct HEAD **buf ) {
    unsigned status;
    char *idxbuf;
    struct HOME *hom;
    long vbn;
    struct FAB idxfab = cc$rms_fab;
    struct RAB idxrab = cc$rms_rab;
    struct NAM idxnam = cc$rms_nam;
    char rsbuf[NAM$C_MAXRSS + 1] = { "" };

    if( devname->dsc$w_length > NAM$S_DVI )
        return RMS$_FNM;

    idxbuf = (char *)malloc( 512 );
    *buf = (struct HEAD *) idxbuf;
    if( idxbuf == NULL )
        return SS$_INSFMEM;

    idxfab.fab$l_fna = NULL;
    idxfab.fab$b_fns = 0;
    idxfab.fab$l_dna = NULL;
    idxfab.fab$b_dns = 0;
    idxfab.fab$b_fac = FAB$M_GET;

    idxfab.fab$l_fop = FAB$M_NAM;
    idxnam.nam$t_dvi[0] = (char)devname->dsc$w_length;
    memcpy( idxnam.nam$t_dvi + 1, devname->dsc$a_pointer, devname->dsc$w_length );

    idxnam.nam$w_did.fid$w_num = 4;
    idxnam.nam$w_did.fid$w_seq = 4;
    idxnam.nam$w_did.fid$b_nmx = 0;
    idxnam.nam$w_did.fid$b_rvn = fid->fid$b_rvn;

    idxnam.nam$w_fid.fid$w_num = 1;
    idxnam.nam$w_fid.fid$w_seq = 1;
    idxnam.nam$w_fid.fid$b_nmx = 0;
    idxnam.nam$w_fid.fid$b_rvn = fid->fid$b_rvn;
    idxnam.nam$l_rsa = rsbuf;
    idxnam.nam$b_rss = NAM$C_MAXRSS;

    do {
        idxfab.fab$l_nam = &idxnam;
        if( $FAILS(status = sys_open(&idxfab)) )
            break;

        idxrab.rab$l_fab = &idxfab;
        if( $FAILS(status = sys_connect( &idxrab )) ) {
            sys_close( &idxfab );
            break;
        }

        idxrab.rab$l_ubf = idxbuf;
        idxrab.rab$w_usz = 512;
        idxrab.rab$w_rfa[2] = 0;
        idxrab.rab$w_rfa[1] = (uint16_t) (2 >> 16); /* HOMVBN */
        idxrab.rab$w_rfa[0] = (uint16_t) (2);

        if( $FAILS(status = sys_get( &idxrab )) )
            break;

        hom = (struct HOME *)idxbuf;
        vbn = ( F11WORD( hom->hm2$w_ibmapvbn ) +
                F11WORD( hom->hm2$w_ibmapsize ) +
                ((fid->fid$b_nmx << 16) |
                 fid->fid$w_num) -1 );
        *clustersize = F11WORD( hom->hm2$w_cluster );

        idxrab.rab$w_rfa[2] = 0;
        idxrab.rab$w_rfa[1] = (uint16_t) (vbn >> 16);
        idxrab.rab$w_rfa[0] = (uint16_t) (vbn);

        status = sys_get( &idxrab );
    } while( 0 );

    sys_disconnect( &idxrab );
    sys_close( &idxfab );
    if( $FAILED(status) ) {
        printmsg( ODS2_FILHDRACC, 0 );
        status = printmsg( status, MSG_CONTINUE|MSG_NOARGS|MSG_TOFILE, of, ODS2_FILHDRACC );
    }
    return status;
}

/*********************************************************** print_fileheader() */

static void print_fileheader( FILE *of, struct HEAD *fhd, uint32_t *vbn,
                              uint32_t clustersize ) {
    f11word *mp, *ep;
    f11long hiblk;
    struct IDENT *id;
    size_t idsize;
    char fname[ sizeof( id->fi2$t_filename ) +  sizeof( id->fi2$t_filenamext ) + 1 ];

    id = (struct IDENT *)(((f11word *)fhd) + fhd->fh2$b_idoffset);
    idsize = (size_t)(fhd->fh2$b_mpoffset - fhd->fh2$b_idoffset) * 2;

    if( idsize >= sizeof( id->fi2$t_filename ) ) {
        char *p;

        memset( fname, ' ', sizeof( fname ) );
        memcpy( fname, id->fi2$t_filename, sizeof( id->fi2$t_filename ));
        idsize -= offsetof( struct IDENT, fi2$t_filenamext );
        if( idsize ) {
            if( idsize > sizeof( id->fi2$t_filenamext ) )
                idsize = sizeof( id->fi2$t_filenamext );
            memcpy( fname + sizeof( id->fi2$t_filename ), id->fi2$t_filenamext, idsize );
        }
        for( p = fname + sizeof(fname); p > fname && p[-1] == ' '; --p )
            p[-1] = '\0';
        fname[sizeof(fname) -1] = '\0';
        printmsg( DIRECT_IDNAME, MSG_TEXT|MSG_TOFILE, of, fname );
    }

    hiblk = F11SWAP( fhd->fh2$w_recattr.fat$l_hiblk );

    if( fhd->fh2$w_seg_num != 0 )
        printmsg( DIRECT_EXTNHDR, MSG_TEXT|MSG_TOFILE, of,
                  F11WORD( fhd->fh2$w_seg_num ),
                  (((uint32_t)fhd->fh2$w_ext_fid.fid$b_nmx) << 16 |
                   F11WORD(fhd->fh2$w_ext_fid.fid$w_num)),
                  F11WORD(fhd->fh2$w_ext_fid.fid$w_seq),
                  fhd->fh2$w_ext_fid.fid$b_rvn );

    printmsg( DIRECT_HDROFFSETS, MSG_TEXT|MSG_TOFILE, of, fhd->fh2$b_idoffset,
             fhd->fh2$b_mpoffset, fhd->fh2$b_acoffset, fhd->fh2$b_rsoffset );
    printmsg( DIRECT_MAPSIZE, MSG_TEXT|MSG_TOFILE, of,
            fhd->fh2$b_acoffset - fhd->fh2$b_mpoffset,
            fhd->fh2$b_map_inuse, clustersize, hiblk );

    if( fhd->fh2$b_idoffset >= 40 && fhd->fh2$l_highwater )
        printmsg( DIRECT_MAPHIGHWATER, MSG_TEXT|MSG_TOFILE, of,
                  F11LONG( fhd->fh2$l_highwater ) - 1 );

    printmsg( DIRECT_STRUCLEVEL, MSG_TEXT|MSG_TOFILE, of,
              fhd->fh2$w_struclev >> 8, fhd->fh2$w_struclev & 0xFFu );

    mp = ((f11word *)fhd) + fhd->fh2$b_mpoffset;
    ep = mp + fhd->fh2$b_map_inuse;

    if( mp >= ep )
        return;

    printmsg( DIRECT_MAPHEADER, MSG_TEXT|MSG_TOFILE, of );

    while( mp < ep ) {
        f11word e0;
        uint32_t count, pbn;

        e0 = *mp++;
        e0 = F11WORD(e0);
        switch( e0 &  FM2$M_FORMAT3 ) {
        case  FM2$M_FORMAT1:
            count = e0 & 0xff;
            pbn = F11WORD(*mp) | (((e0 & ~FM2$M_FORMAT3) >> 8) << 16);
            mp++;
            break;
        case  FM2$M_FORMAT2:
            count = e0 & ~FM2$M_FORMAT3;
            pbn = *mp++;
            pbn = F11WORD( pbn );
            pbn |= F11WORD (*mp ) << 16;
            mp++;
            break;
        case FM2$M_FORMAT3:
            count = ((e0 & ~FM2$M_FORMAT3) << 16);
            count |= F11WORD( *mp );
            mp++;
            pbn = *mp++;
            pbn = F11WORD( pbn );
            pbn |= F11WORD (*mp ) << 16;
            mp++;
            break;
        default:
            printmsg( DIRECT_MAPFMT0, MSG_TEXT|MSG_TOFILE, of, e0 );
            continue;
        }
        ++count;
        printmsg( DIRECT_MAPENTRY, MSG_TEXT|MSG_TOFILE, of, *vbn,
                  (e0 >> 14), count,
                  pbn, pbn + count -1 );
        *vbn += count;
    }
    printmsg( DIRECT_MAPFREE, MSG_TEXT|MSG_TOFILE, of, *vbn );
    return;
}

