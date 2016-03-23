/* RMS.h   RMS routine definitions */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

#ifndef _RMS_H
#define _RMS_H

#include "vmstime.h"

#define RMS$_FACILITY 1

#define RMS$_RTB 98728
#define RMS$_EOF 98938
#define RMS$_FNF 98962
#define RMS$_NMF 99018
#define RMS$_WCC 99050
#define RMS$_BUG 99380
#define RMS$_DIR 99532
#define RMS$_ESS 99588
#define RMS$_FNM 99628
#define RMS$_IFI 99684
#define RMS$_NAM 99804
#define RMS$_RSS 99988
#define RMS$_RSZ 100004
#define RMS$_WLD 100164
#define RMS$_DNF 114762

#define NAM$C_MAXRSS 255
#define NAM$M_SYNCHK 0x8

#define XAB$C_DAT 18
#define XAB$C_FHC 29
#define XAB$C_PRO 19
#define XAB$C_ITM 36

struct XABDAT {
    void *xab$l_nxt;
    int xab$b_cod;
    int xab$w_rvn;
    VMSTIME xab$q_bdt;
    VMSTIME xab$q_cdt;
    VMSTIME xab$q_edt;
    VMSTIME xab$q_rdt;
};

#ifdef RMS$INITIALIZE
struct XABDAT cc$rms_xabdat = {NULL,XAB$C_DAT,0,
        VMSTIME_ZERO, VMSTIME_ZERO,
        VMSTIME_ZERO, VMSTIME_ZERO};
#else
extern struct XABDAT cc$rms_xabdat;
#endif



struct XABFHC {
    void *xab$l_nxt;
    int xab$b_cod;
    int xab$b_atr;
    int xab$b_bkz;
    int xab$w_dxq;
    int xab$l_ebk;
    int xab$w_ffb;
    int xab$w_gbc;
    int xab$l_hbk;
    int xab$b_hsz;
    int xab$w_lrl;
    int xab$w_verlimit;
};

#ifdef RMS$INITIALIZE
struct XABFHC cc$rms_xabfhc = {NULL,XAB$C_FHC,0,0,0,0,0,0,0,0,0,0};
#else
extern struct XABFHC cc$rms_xabfhc;
#endif

#define XAB$K_SENSEMODE 1
/* #define XAB$K_SETMODE 2 */
#define XAB$_UCHAR 128
#define XAB$_UCHAR_NOBACKUP 130         /* (set,sense)  FCH$V_NOBACKUP      */
#define XAB$_UCHAR_WRITEBACK 131        /* (sense)      FCH$V_WRITEBACK     */
#define XAB$_UCHAR_READCHECK 132        /* (set,sense)  FCH$V_READCHECK     */
#define XAB$_UCHAR_WRITECHECK 133       /* (set,sense)  FCH$V_WRITECHECK    */
#define XAB$_UCHAR_CONTIGB 134          /* (set,sense)  FCH$V_CONTIGB       */
#define XAB$_UCHAR_LOCKED 135           /* (set,sense)  FCH$V_LOCKED        */
#define XAB$_UCHAR_CONTIG 136           /* (sense)      FCH$V_CONTIG        */
#define XAB$_UCHAR_SPOOL 138            /* (sense)      FCH$V_SPOOL         */
#define XAB$_UCHAR_DIRECTORY 139        /* (sense)      FCH$V_DIRECTORY     */
#define XAB$_UCHAR_BADBLOCK 140         /* (sense)      FCH$V_BADBLOCK      */
#define XAB$_UCHAR_MARKDEL 141          /* (sense)      FCH$V_BADBLOCK      */

struct item_list {
    unsigned short int code;
    unsigned short int length;
    void *buffer;
    int retlen;
};

struct XABITM {
    void *xab$l_nxt;
    int xab$b_cod;
    struct item_list *xab$l_itemlist;
    int xab$b_mode;
};

#ifdef RMS$INITIALIZE
struct XABITM cc$rms_xabitm = {NULL,XAB$C_ITM,NULL,0};
#else
extern struct XABITM cc$rms_xabitm;
#endif

#define xab$m_noread 1
#define xab$m_nowrite 2
#define xab$m_noexe 4
#define xab$m_nodel 8
#define xab$m_prot (xab$m_noread|xab$m_nowrite|xab$m_noexe|xab$m_nodel)
#define xab$v_system 0
#define xab$v_owner  4
#define xab$v_group  8
#define xab$v_world 12

struct XABPRO {
    void *xab$l_nxt;
    int xab$b_cod;
    unsigned int xab$w_pro;
    unsigned int xab$l_uic;
};

#ifdef RMS$INITIALIZE
struct XABPRO cc$rms_xabpro = {NULL,XAB$C_PRO,0,0};
#else
extern struct XABPRO cc$rms_xabpro;
#endif



#define NAM$M_WILDCARD 0x100

struct NAM {
    unsigned short nam$w_did_num;
    unsigned short nam$w_did_seq;
    unsigned char nam$b_did_rvn;
    unsigned char nam$b_did_nmx;
    unsigned short nam$w_fid_num;
    unsigned short nam$w_fid_seq;
    unsigned char nam$b_fid_rvn;
    unsigned char nam$b_fid_nmx;
    int nam$b_ess;
    int nam$b_rss;
    int nam$b_esl;
    char *nam$l_esa;
    int nam$b_rsl;
    char *nam$l_rsa;
    int nam$b_dev;
    char *nam$l_dev;
    int nam$b_dir;
    char *nam$l_dir;
    int nam$b_name;
    char *nam$l_name;
    int nam$b_type;
    char *nam$l_type;
    int nam$b_ver;
    char *nam$l_ver;
    void *nam$l_wcc;
    int nam$b_nop;
    int nam$l_fnb;
    struct NAM *nam$l_rlf;
};

#ifdef RMS$INITIALIZE
struct NAM cc$rms_nam = {0,0,0,0,0,0,0,0,0,0,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0,0,0,NULL};
#else
extern struct NAM cc$rms_nam;
#endif


#define RAB$C_SEQ 0
#define RAB$C_RFA 2

struct RAB {
    struct FAB *rab$l_fab;
    char *rab$l_ubf;
    char *rab$l_rhb;
    char *rab$l_rbf;
    unsigned rab$w_usz;
    unsigned rab$w_rsz;
    int rab$b_rac;
    unsigned short rab$w_rfa[3];
};

#ifdef RMS$INITIALIZE
struct RAB cc$rms_rab = {NULL,NULL,NULL,NULL,0,0,0,{0,0,0}};
#else
extern struct RAB cc$rms_rab;
#endif



#define FAB$C_SEQ 0
#define FAB$C_REL 16
#define FAB$C_IDX 32
#define FAB$C_HSH 48

#define FAB$M_FTN 0x1
#define FAB$M_CR  0x2
#define FAB$M_PRN 0x4
#define FAB$M_BLK 0x8

#define FAB$M_PUT 0x1
#define FAB$M_GET 0x2
#define FAB$M_DEL 0x4
#define FAB$M_UPD 0x8
#define FAB$M_TRN 0x10
#define FAB$M_BIO 0x20
#define FAB$M_BRO 0x40
#define FAB$M_EXE 0x80

#define FAB$C_UDF 0
#define FAB$C_FIX 1
#define FAB$C_VAR 2
#define FAB$C_VFC 3
#define FAB$C_STM 4
#define FAB$C_STMLF 5
#define FAB$C_STMCR 6

/* FAB$L_FOP */
#define FAB$M_ASY 1
#define FAB$M_MXV 2
#define FAB$M_SUP 4
#define FAB$M_TMP 8
#define FAB$M_TMD 16
#define FAB$M_DFW 32
#define FAB$M_SQO 64
#define FAB$M_RWO 128
#define FAB$M_POS 256
#define FAB$M_WCK 512
#define FAB$M_NEF 1024
#define FAB$M_RWC 2048
#define FAB$M_DMO 4096
#define FAB$M_SPL 8192
#define FAB$M_SCF 16384
#define FAB$M_DLT 32768
#define FAB$M_NFS 65536
#define FAB$M_UFO 131072
#define FAB$M_PPF 262144
#define FAB$M_INP 524288
#define FAB$M_CTG 1048576
#define FAB$M_CBT 2097152
#define FAB$M_SYNCSTS 4194304
#define FAB$M_RCK 8388608
#define FAB$M_NAM 16777216
#define FAB$M_CIF 33554432
#define FAB$M_ESC 134217728
#define FAB$M_TEF 268435456
#define FAB$M_OFP 536870912
#define FAB$M_KFO 1073741824

struct FAB {
    struct NAM *fab$l_nam;
    int fab$w_ifi;
    char *fab$l_fna;
    char *fab$l_dna;
    int fab$b_fns;
    int fab$b_dns;
    int fab$l_alq;
    int fab$b_bks;
    int fab$w_deq;
    unsigned int fab$b_fsz;
    int fab$w_gbc;
    unsigned int fab$w_mrs;
    int fab$l_fop;
    int fab$b_org;
    int fab$b_rat;
    int fab$b_rfm;
    int fab$b_fac;
    void *fab$l_xab;
    unsigned short fab__w_verlimit;
};

#ifdef RMS$INITIALIZE
struct FAB cc$rms_fab = {NULL,0,NULL,NULL,0,0,0,0,0,0,0,0,0,0,0,0,0,NULL,0};
#else
extern struct FAB cc$rms_fab;
#endif


#ifndef NO_DOLLAR
#define sys$search      sys_search
#define sys$parse       sys_parse
#define sys$setddir     sys_setddir
#define sys$connect     sys_connect
#define sys$disconnect  sys_disconnect
#define sys$get         sys_get
#define sys$display     sys_display
#define sys$close       sys_close
#define sys$open        sys_open
#define sys$create      sys_create
#define sys$erase       sys_erase
#endif

unsigned sys_search(struct FAB *fab);
unsigned sys_parse(struct FAB *fab);
unsigned sys_connect(struct RAB *rab);
unsigned sys_disconnect(struct RAB *rab);
unsigned sys_get(struct RAB *rab);
unsigned sys_put(struct RAB *rab);
unsigned sys_display(struct FAB *fab);
unsigned sys_close(struct FAB *fab);
unsigned sys_open(struct FAB *fab);
unsigned sys_create(struct FAB *fab);
unsigned sys_erase(struct FAB *fab);
unsigned sys_extend(struct FAB *fab);
unsigned sys_setddir(struct dsc_descriptor *newdir,unsigned short *oldlen,
                     struct dsc_descriptor *olddir);

unsigned sys_initialize(void);

#endif /* #ifndef _RMS_H */
