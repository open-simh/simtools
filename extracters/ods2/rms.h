/* RMS.h   RMS routine definitions */

/*
 *      This is part of ODS2 written by Paul Nankervis,
 *      email address:  Paulnank@au1.ibm.com
 *
 *      ODS2 is distributed freely for all members of the
 *      VMS community to use. However all derived works
 *      must maintain comments in their source to acknowledge
 *      the contributions of the original author and
 *      subsequent contributors.   This is free software; no
 *       warranty is offered,  and while we believe it to be useful,
 *       you use it at your own risk.
 */

#ifndef _RMS_H
#define _RMS_H

#include <stdint.h>

#include "ods2.h"
#include "f11def.h"
#include "vmstime.h"

#define NAM$C_MAXRSS 255u
#define NAM$M_SYNCHK 0x8u
#define NAM$S_DVI    32u

#define XAB$C_DAT 18u
#define XAB$C_ALL 20u
#define XAB$C_FHC 29u
#define XAB$C_PRO 19u
#define XAB$C_RDT 30u
#define XAB$C_ITM 36u

/* N.B. xab$l_nxt and xab$b_cod MUST have the same offsets in all XABs */

struct XABALL {
    void *xab$l_nxt;
    uint8_t xab$b_cod;
    uint16_t xab$w_vol;
    uint32_t xab$l_alq;
    uint16_t xab$w_deq;
    uint8_t xab$b_bkz;
};

#ifdef RMS$INITIALIZE
struct XABALL cc$rms_xaball = { NULL, XAB$C_ALL, 0,
                                0, 0, 0 };
#else
extern struct XABALL cc$rms_xaball;
#endif

struct XABRDT {
    void *xab$l_nxt;
    uint8_t xab$b_cod;
    uint16_t xab$w_rvn;
    VMSTIME  xab$q_rdt;
};
#ifdef RMS$INITIALIZE
struct XABRDT cc$rms_xabrdt = { NULL, XAB$C_RDT, 0, VMSTIME_ZERO };
#else
extern struct XABRDT cc$rms_xabrdt;
#endif

struct XABDAT {
    void *xab$l_nxt;
    uint8_t xab$b_cod;
    uint16_t xab$w_rvn;
    VMSTIME xab$q_bdt;
    VMSTIME xab$q_cdt;
    VMSTIME xab$q_edt;
    VMSTIME xab$q_rdt;
};

#ifdef RMS$INITIALIZE
struct XABDAT cc$rms_xabdat = { NULL, XAB$C_DAT, 0,
                                VMSTIME_ZERO, VMSTIME_ZERO,
                                VMSTIME_ZERO, VMSTIME_ZERO };
#else
extern struct XABDAT cc$rms_xabdat;
#endif

struct XABFHC {
    void *xab$l_nxt;
    uint8_t xab$b_cod;
    uint8_t xab$b_atr;
    uint16_t xab$w_ffb;
    uint32_t xab$l_ebk;
    uint32_t xab$l_hbk;
    uint16_t xab$w_lrl;
    uint16_t xab$w_dxq;
    uint16_t xab$w_gbc;
    uint16_t xab$w_verlimit;
    uint8_t xab$b_bkz;
    uint8_t xab$b_hsz;
};

#ifdef RMS$INITIALIZE
struct XABFHC cc$rms_xabfhc = { NULL, XAB$C_FHC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#else
extern struct XABFHC cc$rms_xabfhc;
#endif

#define XAB$K_SENSEMODE 1u
#define XAB$K_SETMODE   2u
#define XAB$_UCHAR            128u
#define XAB$_UCHAR_NOBACKUP   130u       /* (set, sense)  FCH$V_NOBACKUP     */
#define XAB$_UCHAR_WRITEBACK  131u       /* (sense)      FCH$V_WRITEBACK     */
#define XAB$_UCHAR_READCHECK  132u       /* (set, sense)  FCH$V_READCHECK    */
#define XAB$_UCHAR_WRITECHECK 133u       /* (set, sense)  FCH$V_WRITECHECK   */
#define XAB$_UCHAR_CONTIGB    134u       /* (set, sense)  FCH$V_CONTIGB      */
#define XAB$_UCHAR_LOCKED     135u       /* (set, sense)  FCH$V_LOCKED       */
#define XAB$_UCHAR_CONTIG     136u       /* (sense)      FCH$V_CONTIG        */
#define XAB$_UCHAR_SPOOL      138u       /* (sense)      FCH$V_SPOOL         */
#define XAB$_UCHAR_DIRECTORY  139u       /* (sense)      FCH$V_DIRECTORY     */
#define XAB$_UCHAR_BADBLOCK   140u       /* (sense)      FCH$V_BADBLOCK      */
#define XAB$_UCHAR_MARKDEL    141u       /* (sense)      FCH$V_BADBLOCK      */

struct item_list {
    uint16_t  code;
    uint16_t  length;
    void     *buffer;
    uint16_t *retlen;
};

struct XABITM {
    void   *xab$l_nxt;
    uint8_t xab$b_cod;
    uint8_t xab$b_mode;
    struct item_list *xab$l_itemlist;
};

#ifdef RMS$INITIALIZE
struct XABITM cc$rms_xabitm = {NULL, XAB$C_ITM, 0, NULL};
#else
extern struct XABITM cc$rms_xabitm;
#endif

#define xab$m_noread  1u
#define xab$m_nowrite 2u
#define xab$m_noexe   4u
#define xab$m_nodel   8u
#define xab$m_prot    (xab$m_noread|xab$m_nowrite|xab$m_noexe|xab$m_nodel)
#define xab$v_system  0u
#define xab$v_owner   4u
#define xab$v_group   8u
#define xab$v_world   12u

struct XABPRO {
    void    *xab$l_nxt;
    uint8_t  xab$b_cod;
    uint16_t xab$w_pro;
    uint32_t xab$l_uic;
};

#ifdef RMS$INITIALIZE
struct XABPRO cc$rms_xabpro = { NULL, XAB$C_PRO, 0, 0 };
#else
extern struct XABPRO cc$rms_xabpro;
#endif


#define NAM$M_EXP_VER      UINT32_C(0x00000001)
#define NAM$M_EXP_TYPE     UINT32_C(0x00000002)
#define NAM$M_EXP_NAME     UINT32_C(0x00000004)
#define NAM$M_WILD_VER     UINT32_C(0x00000008)
#define NAM$M_WILD_TYPE    UINT32_C(0x00000010)
#define NAM$M_WILD_NAME    UINT32_C(0x00000020)
#define NAM$M_EXP_DIR      UINT32_C(0x00000040)
#define NAM$M_EXP_DEV      UINT32_C(0x00000080)
#define NAM$M_WILDCARD     UINT32_C(0x00000100)
#define NAM$M_SEARCH_LIST  UINT32_C(0x00000800)
#define NAM$M_CNCL_DEV     UINT32_C(0x00001000)
#define NAM$M_ROOT_DIR     UINT32_C(0x00002000)
#define NAM$M_LOWVER       UINT32_C(0x00004000)
#define NAM$M_HIGHVER      UINT32_C(0x00008000)
#define NAM$M_PPF          UINT32_C(0x00010000)
#define NAM$M_NODE         UINT32_C(0x00020000)
#define NAM$M_QUOTED       UINT32_C(0x00040000)
#define NAM$M_GRP_MBR      UINT32_C(0x00080000)
#define NAM$M_WILD_DIR     UINT32_C(0x00100000)
#define NAM$M_DIR_LVLS     UINT32_C(0x00e00000)
#define NAM$V_DIR_LVLS             20u
#define NAM$M_WILD_UFD     UINT32_C(0x01000000)
#define NAM$M_WILD_SFD1    UINT32_C(0x02000000)
#define NAM$M_WILD_SFD2    UINT32_C(0x04000000)
#define NAM$M_WILD_SFD3    UINT32_C(0x08000000)
#define NAM$M_WILD_SFD4    UINT32_C(0x10000000)
#define NAM$M_WILD_SFD5    UINT32_C(0x20000000)
#define NAM$M_WILD_SFD6    UINT32_C(0x40000000)
#define NAM$M_WILD_SFD7    UINT32_C(0x80000000)
/* The following are aliased to WILD_UFD, WILD_SFD1 */
#define NAM$M_WILD_GRP     UINT32_C(0x01000000)
#define NAM$M_WILD_MBR     UINT32_C(0x02000000)

struct NAM {
    char    *nam$l_esa;
    char    *nam$l_rsa;
    char    *nam$l_dev;
    char    *nam$l_dir;
    char    *nam$l_name;
    char    *nam$l_type;
    char    *nam$l_ver;
    struct NAM *nam$l_rlf;
    void    *nam$l_wcc;
    struct fiddef nam$w_fid;
    struct fiddef nam$w_did;
    uint32_t nam$l_fnb;
    char     nam$t_dvi[1+NAM$S_DVI];
    uint8_t  nam$b_ess;
    uint8_t  nam$b_esl;
    uint8_t  nam$b_rss;
    uint8_t  nam$b_rsl;
    uint8_t  nam$b_dev;
    uint8_t  nam$b_dir;
    uint8_t  nam$b_name;
    uint8_t  nam$b_type;
    uint8_t  nam$b_ver;
    uint8_t  nam$b_nop;
};

#ifdef RMS$INITIALIZE
struct NAM cc$rms_nam = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                          NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, 0,
                          { 0, 0, 0, 0, 0, 0, 0, 0,
                            0, 0, 0, 0, 0, 0, 0, 0,
                            0, 0, 0, 0, 0, 0, 0, 0,
                            0, 0, 0, 0, 0, 0, 0, 0, 0 },
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };
#else
extern struct NAM cc$rms_nam;
#endif


/* rab$b_rac */
#define RAB$C_SEQ 0u
#define RAB$C_RFA 2u

/* rab$l_rop */
#define RAB$M_TPT 2
#define RAB$M_EOF 256u

struct RAB {
    struct RAB *rab$a__next;
    struct FAB *rab$l_fab;
    char    *rab$l_ubf;
    char    *rab$l_rhb;
    char    *rab$l_rbf;
    uint32_t rab$l_stv;
    uint32_t rab$l_rop;
    uint32_t rab$w_isi;
    uint16_t rab$w_usz;
    uint16_t rab$w_rsz;
    uint8_t  rab$b_rac;
    uint16_t rab$w_rfa[3];
};

#ifdef RMS$INITIALIZE
struct RAB cc$rms_rab = { NULL, NULL, NULL, NULL, NULL,
                          0, 0, 0, 0, 0, 0, {0, 0, 0} };
#else
extern struct RAB cc$rms_rab;
#endif


/* FAB$B_ORG : N.B. These are all high nibble values for fat$b_rtype */
#define FAB$C_SEQ 0u
#define FAB$C_REL 16u
#define FAB$C_IDX 32u
#define FAB$C_HSH 48u

/* FAB$B_RAT : N.B. These are all low nibble values for fat$b_rtype */
#define FAB$M_FTN 0x1u
#define FAB$M_CR  0x2u
#define FAB$M_PRN 0x4u
#define FAB$M_BLK 0x8u

/* FAB$B_FAC */
#define FAB$M_PUT 0x1u
#define FAB$M_GET 0x2u
#define FAB$M_DEL 0x4u
#define FAB$M_UPD 0x8u
#define FAB$M_TRN 0x10u
#define FAB$M_BIO 0x20u
#define FAB$M_BRO 0x40u
#define FAB$M_EXE 0x80u

/* FAB$B_RFM */
#define FAB$C_UDF   0u
#define FAB$C_FIX   1u
#define FAB$C_VAR   2u
#define FAB$C_VFC   3u
#define FAB$C_STM   4u
#define FAB$C_STMLF 5u
#define FAB$C_STMCR 6u

/* FAB$L_FOP */
#define FAB$M_ASY     UINT32_C(1)
#define FAB$M_MXV     UINT32_C(2)
#define FAB$M_SUP     UINT32_C(4)
#define FAB$M_TMP     UINT32_C(8)
#define FAB$M_TMD     UINT32_C(16)
#define FAB$M_DFW     UINT32_C(32)
#define FAB$M_SQO     UINT32_C(64)
#define FAB$M_RWO     UINT32_C(128)
#define FAB$M_POS     UINT32_C(256)
#define FAB$M_WCK     UINT32_C(512)
#define FAB$M_NEF     UINT32_C(1024)
#define FAB$M_RWC     UINT32_C(2048)
#define FAB$M_DMO     UINT32_C(4096)
#define FAB$M_SPL     UINT32_C(8192)
#define FAB$M_SCF     UINT32_C(16384)
#define FAB$M_DLT     UINT32_C(32768)
#define FAB$M_NFS     UINT32_C(65536)
#define FAB$M_UFO     UINT32_C(131072)
#define FAB$M_PPF     UINT32_C(262144)
#define FAB$M_INP     UINT32_C(524288)
#define FAB$M_CTG     UINT32_C(1048576)
#define FAB$M_CBT     UINT32_C(2097152)
#define FAB$M_SYNCSTS UINT32_C(4194304)
#define FAB$M_RCK     UINT32_C(8388608)
#define FAB$M_NAM     UINT32_C(16777216)
#define FAB$M_CIF     UINT32_C(33554432)
#define FAB$M_ESC     UINT32_C(134217728)
#define FAB$M_TEF     UINT32_C(268435456)
#define FAB$M_OFP     UINT32_C(536870912)
#define FAB$M_KFO     UINT32_C(1073741824)

struct FAB {
    char    *fab$l_fna;
    char    *fab$l_dna;
    struct NAM *fab$l_nam;
    void    *fab$l_xab;
    struct RAB *fab$a__rab;
    uint32_t fab$l_isi;
    uint32_t fab$l_alq;
    uint32_t fab$l_fop;
    uint32_t fab$l_openfop;
    uint16_t fab$w_deq;
    uint16_t fab$w_ifi;
    uint16_t fab$w_gbc;
    uint16_t fab$w_mrs;
    uint8_t  fab$b_fns;
    uint8_t  fab$b_dns;
    uint8_t  fab$b_bks;
    uint8_t  fab$b_fsz;
    uint8_t  fab$b_org;
    uint8_t  fab$b_rat;
    uint8_t  fab$b_rfm;
    uint8_t  fab$b_fac;
    uint16_t fab__w_verlimit;
};

#ifdef RMS$INITIALIZE
struct FAB cc$rms_fab = {NULL, NULL, NULL, NULL, NULL,
                         0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0};
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
#define sys$rename      sys_rename
#define sys$extend      sys_extend
#endif

vmscond_t sys_close( struct FAB *fab );
vmscond_t sys_connect( struct RAB *rab );
vmscond_t sys_create( struct FAB *fab );
vmscond_t sys_disconnect( struct RAB *rab );
vmscond_t sys_display( struct FAB *fab );
vmscond_t sys_erase( struct FAB *fab );
vmscond_t sys_extend( struct FAB *fab );
vmscond_t sys_get( struct RAB *rab );
vmscond_t sys_initialize(void);
vmscond_t sys_open( struct FAB *fab );
vmscond_t sys_parse( struct FAB *fab );
vmscond_t sys_put( struct RAB *rab );
vmscond_t sys_rename( struct FAB *oldfab,
                      void *astrtn, void *astprm, struct FAB *newfab );
vmscond_t sys_search( struct FAB *fab );
vmscond_t sys_setddir( struct dsc_descriptor *newdir, uint16_t *oldlen,
                       struct dsc_descriptor *olddir );

#endif /* #ifndef _RMS_H */
