! This is an automatically-generated file.  message version 0.1
!
! Command: message -m -q vms_messages_en_us.msg en_us.msg ssdef.msg
!
! It uses a subset of the VMS Message utility language.
! The message definitions can be processed by the message
! Perl script, which will compile them into a .h file usable
! by sysmsg.c and all the ODS2 modules that rely on condition
! codes.
!
! This technlology was developed by Timothe Litt, and is
! free to use with the ODS2 package, originally developed by
! Paul Nankervis <Paulnank@au1.ibm.com>.
!
! ODS2 may be freely distributed within the VMS community
! providing that the contributions of the original author and
! subsequent contributors are acknowledged in the help text and
! source files.  This is free software; no warranty is offered,
! and while we believe it to be useful, you use it at your own risk.
!


        .FACILITY COPY,85 /SHARED
        .LITERAL COPY_CLOSEOUT        = ((COPY_FACILITY@16) + SHR$_ILLDESQUE + 2)           ! 0x085512d2 Error
        .LITERAL COPY_COPIEDB         = ((COPY_FACILITY@16) + SHR$_NOTTERM + 3)             ! 0x085512db Info
        .LITERAL COPY_COPIEDR         = ((COPY_FACILITY@16) + SHR$_CONFQUAL + 3)            ! 0x085512e3 Info
        .LITERAL COPY_INVALIDNL       = ((COPY_FACILITY@16) + SHR$_CREATEDSTM + 2)          ! 0x085512fa Error
        .LITERAL COPY_INVALIDVFU      = ((COPY_FACILITY@16) + SHR$_NOTRUNC + 2)             ! 0x08551302 Error
        .LITERAL COPY_INVALIDC0       = ((COPY_FACILITY@16) + SHR$_PRODNOTINS + 2)          ! 0x0855130a Error
        .LITERAL COPY_OPENIN          = ((COPY_FACILITY@16) + SHR$_FILDEL + 2)              ! 0x08551322 Error
        .LITERAL COPY_OPENOUT         = ((COPY_FACILITY@16) + SHR$_INVQUAVAL + 2)           ! 0x0855132a Error
        .LITERAL COPY_PARSEFAIL       = ((COPY_FACILITY@16) + SHR$_NOFILES + 2)             ! 0x08551332 Error
        .LITERAL COPY_READERR         = ((COPY_FACILITY@16) + SHR$_FILNOTACC + 2)           ! 0x0855133a Error
        .LITERAL COPY_SEARCHFAIL      = ((COPY_FACILITY@16) + SHR$_QUALMISS + 2)            ! 0x08551342 Error
        .LITERAL COPY_WRITEERR        = ((COPY_FACILITY@16) + SHR$_FILSPCSRCH + 2)          ! 0x0855134a Error

        .END

        .FACILITY COPY,85

        .SEVERITY INFO
        .BASE 400
COPIEDU          <!AZ copied to !AZ (!UL byte!%S)> /IDENT=COPIED /FAO=3
CONFIRM          <copy !AZ to !AZ? > /FAO=2
COPIED           <!UL file(s) copied> /FAO=1
NOFILES          <no files found>
DIRNOTCOPIED     <!AZ is a directory, not copied> /FAO=1

        .SEVERITY WARNING
        .BASE 600
PROTERR          <failed to set protection for !AZ> /FAO=1
NOMATCH          <no local files match !AZ> /FAO=1

        .SEVERITY ERROR
        .BASE 800
NONAME           <no file name specified for output>

        .SEVERITY SEVERE
        .BASE 1000
NOMEM            <out of memory expanding !AZ>
GLOBABT          <read error expanding !AZ> /FAO=1
GLOBERR          <unknown error !UL processing !AZ> /FAO=2

        .END


        .FACILITY CREATE,86 /SHARED
        .LITERAL CREATE_OFFSET          = (86)                                                ! 0x00000050 Warning
        .LITERAL CREATE_CLOSEOUT        = ((CREATE_FACILITY@16) + SHR$_ILLDESQUE + 2)         ! 0x085612d2 Error
        .LITERAL CREATE_CREATED         = ((CREATE_FACILITY@16) + SHR$_ILLDIRCOPY + 3)        ! 0x085612eb Info
        .LITERAL CREATE_EXISTS          = ((CREATE_FACILITY@16) + SHR$_INSVIRMEM + 3)         ! 0x085612f3 Info
        .LITERAL CREATE_INVALIDNL       = ((CREATE_FACILITY@16) + SHR$_CREATEDSTM + 2)        ! 0x085612fa Error
        .LITERAL CREATE_INVALIDVFU      = ((CREATE_FACILITY@16) + SHR$_NOTRUNC + 2)           ! 0x08561302 Error
        .LITERAL CREATE_INVALIDC0       = ((CREATE_FACILITY@16) + SHR$_PRODNOTINS + 2)        ! 0x0856130a Error
        .LITERAL CREATE_NOTDIR          = ((CREATE_FACILITY@16) + SHR$_FILPURG + 2)           ! 0x0856131a Error

        .END

        .FACILITY CREATE,86

        .SEVERITY INFO
        .BASE 400
READY            <!AZ is open for output. Type lines of text ending with Enter> /FAO=1
TERMEOF          <terminate input with !AZ> /FAO=1

        .SEVERITY ERROR
        .BASE 800
CREDIRFAIL       <failed to create directory !AZ> /FAO=1

        .END


        .FACILITY DELETE,87 /SHARED
        .LITERAL DELETE_OPENIN          = ((DELETE_FACILITY@16) + SHR$_FILDEL)                ! 0x08571320 Warning

        .END

        .FACILITY DELETE,87

        .SEVERITY INFO
        .BASE 400
CONFIRM          <delete !AZ? > /FAO=1
DELETED          <deleted !AZ> /FAO=1
NFILES           <!UL files deleted> /IDENT=DELETED /FAO=1

        .SEVERITY WARNING
        .BASE 600
NOFILES          <no files deleted>
NOTDELETED       <error deleting !AZ> /FAO=1
DIRNOTEMPTY      <directory is not empty>

        .END


        .FACILITY DIFF,88 /SHARED
        .LITERAL DIFF_OPENIN          = ((DIFF_FACILITY@16) + SHR$_FILDEL + 2)              ! 0x08581322 Error

        .END

        .FACILITY DIFF,88

        .SEVERITY SUCCESS
        .BASE 200
NODIFF           <no differences encountered>

        .SEVERITY INFO
        .BASE 400
COMPARED         <compared !UL record(s)> /FAO=1

        .SEVERITY WARNING
        .BASE 600
DIFFER           <files differ at record !UL>

        .END


        .FACILITY DIRECT,89 /SHARED
        .LITERAL DIRECT_NOFILES         = ((DIRECT_FACILITY@16) + SHR$_TOTAL)                 ! 0x08591310 Warning
        .LITERAL DIRECT_OPENOUT         = ((DIRECT_FACILITY@16) + SHR$_INVQUAVAL + 2)         ! 0x0859132a Error

        .END

        .FACILITY DIRECT,89

        .SEVERITY INFO
        .BASE 400
DIRHEAD          <!/Directory !AZ!/!/> /FAO=1
FULLSIZE         <!/Size: !9UL> /FAO=1
GRANDTOT         <!/Grand total of !UL directory!%S, !UL file!%S> /FAO=2
FILETOTAL        <!/Total of !UL file!%S> /FAO=1
USEDTOT          <!UL block!%S> /FAO=1
ALLOCTOT         <!UL block!%S> /FAO=1
BOTHSIZE         <!UL/!UL block!%S> /FAO=2
OWNER            <owner:    [!OW,!OW]> /FAO=2
CREATED          <created:  >
REVISED          <revised:  >
EXPIRES          <expires:  >
BACKUP           <backup:   >
FILEORG          <file organization:  >
SEQORG           <sequential>
RELORG           <relative>
IDXORG           <indexed>
UNKORG           <unknown (!UL)> /FAO=1
FILEATT          <!/File attributes:    >
ALLOC            <allocation: !UL> /FAO=1
EXTEND           <, Extend: !UW> /FAO=1
GBC              <, Global buffer count: !UW> /FAO=1
VERLIMIT         <, Version limit: !UW> /FAO=1
DIRLIMIT         <, Default version limit: !UW> /FAO=1
NOVERLIMIT       <, No version limit>
NODIRLIMIT       <, No default version limit>
CONTIG           <, Contiguous>
CONTIGB          <, Contiguous best try>
NOBACKUP         <, Backups disabled>
DIRECTORY        <, Directory file>
WRITEBACK        <, Writeback>
READCHECK        <, Read verification>
WRITECHECK       <, Write verification>
LOCKED           <, File locked>
SPOOL            <, Spool file>
MARKDEL          <, Marked for deletion>
BADBLOCK         <, Contains bad block>
EOFPOS           <end of file:        Block !UL, byte !UW!/> /FAO=2
RECFMT           <record format:      >
RECUDF           <undefined>
RECFIX           <fixed length !UW byte records> /FAO=1
RECVAR           <variable length, maximum !UW bytes> /FAO=1
RECVFC           <VFC, !UB byte header, maximum !UW bytes> /FAO=2
RECSTM           <stream>
RECSTMLF         <stream-LF>
RECSTMCR         <stream-CR>
RECATT           <Record attributes:  >
RECATT_NONE      <none>
RECATT_FTN       <!AZFortran carriage control> /FAO=1
RECATT_CR        <!AZCarriage return carriage control> /FAO=1
RECATT_PRN       <!AZPrint file carriage control> /FAO=1
RECATT_BLK       <!AZNon-spanned> /FAO=1
PROT_SYS         <File protection:    System:>
PROT_OWNER       <, Owner:>
PROT_GROUP       <, Group:>
PROT_WORLD       <, World:>
UIC              <  [!OW,!OW]> /FAO=2
IDNAME           <!/Header filename:       !AZ> /FAO=1
EXTNHDR          <extension header:      !UW, Extension FID (!UL,!UW,!UB)> /FAO=4
HDROFFSETS       <ID area offset (words):!UB!/Map area offset:       !UB!/ACL area offset:       !UB!/Reserved area offset:  !UB> /FAO=4
STRUCLEVEL       <structure level:       !UB.!UB> /FAO=2
MAPSIZE          <map area size:         !UL, in use: !UL!/Cluster size:          !UL!/Highest VBN allocated: !UL> /FAO=4
MAPHIGHWATER     <highest VBN written:   !UL> /FAO=1
MAPHEADER        <!/Retrieval pointers:!/      VBN F     Count Start LBN   End LBN!/--------- - --------- --------- --------->
MAPFMT0          <format 0: !XW> /FAO=1
MAPENTRY         <!9UL !UW !9UL !9UL !9UL> /FAO=5
MAPFREE          "!9UL <= First free" /FAO=1

        .END


        .FACILITY DISM,90

        .SEVERITY INFO
        .BASE 400
DISMOUNTD        <volume !12AD of volume set !12AD dismounted> /IDENT=DISMOUNTED /FAO=4
DISMOUNTDV       <volume !12AD dismounted> /IDENT=DISMOUNTED /FAO=2
DISMAL           <all members of !12AD have been dismounted>

        .SEVERITY WARNING
        .BASE 600
CANNOTDMT        <!12AD cannot be dismounted> /FAO=2

        .SEVERITY SEVERE
        .BASE 1000
DEVNOTDISM       <device not dismounted>
USERFILES        <!SL user file(s) open on volume> /FAO=1

        .END


        .FACILITY DISM,115 /SYSTEM

        .SEVERITY WARNING
MAXDEV           <too many devices>
SYSDEV           <The system device cannot be dismounted>
BADOVR           <unknown keyword on /OVERRIDE qualifier>
SRCMEM           <only source member of shadow set cannot be dismounted>

        .BASE 512
RMTDSMFAIL       <!AS failed to dismount on node !AS> /FAO=2
DSMCLUFAIL       <dismount/cluster failed on one or more nodes in the cluster>
CANNOTDMT        <!AS cannot be dismounted>
INSWPGFIL        <!UW swap or page file!%S installed on volume>
SPOOLEDEV        <!UW device!%S spooled to volume>
INSTIMAGE        <!UW image!%S installed on volume>
USERFILES        <!UW user file!%S open on volume>
FILESOPEN        <volume has files open on remote node>

        .BASE 1024
MARKEDDMT        <!AS has been marked for dismount>
WAITINGDMT       <waiting for !AS to dismount on node !AS> /FAO=2
RMTDMTSUCC       <!AS successfully dismounted on node !AS> /FAO=2

        .END


        .FACILITY EXTEND,91

        .SEVERITY INFO
        .BASE 400
NEWSIZE          <new allocated size is !UL block!%S> /FAO=1

        .SEVERITY ERROR
        .BASE 800
CONFLQUAL        <can not specify both !AZallocate and !AZtruncate> /FAO=2

        .END


        .FACILITY HELP,92

        .SEVERITY INFO
        .BASE 400
TERMWID          <A minimum terminal window width of !UL is recommended for correct formatting of HELP> /FAO=1
TERMSIZE         <Terminal width is !UL, height is !UL> /FAO=2
KEYWORD          < keyword, one of the following:>
KEYWORDS         < keywords, one or more of the following:>
QUALIFIER        < qualifier, one of the following>
QUALIFIERS       < qualifiers, one or more of the following>
LIST             <list, !AZ> /FAO=1
LISTS            <lists, !AZ> /FAO=1
HEAD             < Commands are:>
MORE             < Additional information:>
EXPLAIN          < Type HELP COMMAND or HELP TOPIC for information on any command or topic.!/ Type HELP HELP for information on the help system.>
NOCMDHELP        <!AZ: No help available> /FAO=1
NOPARHELP        <no parameter '!AZ' found> /FAO=1
PARNOTACT        <parameter is not used for this command>
NOPARAMS         <!AZ has no parameters> /FAO=1
PARHEADING       <!/  Parameters:!/    >
PARNOLIMIT       <  "*" indicates that a parameter can be used more than once.>
PAREXPLAIN       <  Type help !AZ PARAMETER for more about each parameter.> /FAO=1
QUALHEAD         <qualifiers>
SETVALUE         <type 'help set value ITEM' for item details>
SHOWCWD          <  Current working directory is !AZ> /FAO=1
SHOWDEFQ         <  Default qualifiers for !AZ command> /FAO=1
SHOWNODEFQ       <      None set>
SHOWQSTYLE       <  Qualifier style: !AZ> /FAO=1
SHOWSTATS        <Statistics:>
SHOWVERON        <Command file verification is on>
SHOWVEROFF       <Command file verification is off>
SHOWVERS         < !AZ !AZ built !AZ !AZ> /FAO=4
SHOWVERSRL       < !AZ !AZ built !AZ !AZ with libedit version !UL.!UL> /FAO=6
SHOWVERSCSI      < Direct SCSI access is supported>
SHOWVERNOVHD     < VHD format image file support is not configured>
SHOWVERPLATFM    < Platform: !AZ> /FAO=1
SHOWVERCOMPLR    < Compiler: !AZ, version !AZ> /FAO=2
SHOWVLD          < Visual Leak Detector is configured>
SHOWCMDHISTLN    <!5UL: !AZ> /FAO=2
SHOWMSGPRINTED   <(printed): >

        .SEVERITY ERROR
        .BASE 800
OPENHLP          <failed to open help file !AZ> /FAO=1
NOHELP           <can't locate help topic "!AZ" in !AZ> /FAO=2
HELPFMT          <help file !AZ has invalid format or is empty> /IDENT=NOHELP /FAO=1
HELPLEVEL        <help level error at line !UL> /FAO=1
NOTEXT           <no text for topic !AZ at line !UL> /FAO=2
NOTEXT2          <no text for topic !AZ> /IDENT=HELPNOTEXT /FAO=1
SHOWNOCMDHIST    <No command history exists>
SETCMDHISTVAL    <Command history limit must be greater than zero>

        .END


        .FACILITY INIT,93

        .SEVERITY INFO
        .BASE 400
CONFIRM_PHY      <this will delete all data on the physical device !AZ. Proceed? > /IDENT=CONFIRM /FAO=1
CONFIRM_VIRT     <this will delete all data in !AZ and initialize a !AZ image file. Proceed? > /IDENT=CONFIRM /FAO=2
START_PHY        <initializing !AZ on device !AZ as !AZ> /IDENT=STARTING /FAO=3
START_VIRT       <initializing !AZ on disk image !AZ as !AZ> /IDENT=STARTNG /FAO=3
VOLINIT          <!AZ initialized successfully> /FAO=1
SIZING1          <volume size !UL blocks, cluster size !UL, maximum files !UL> /IDENT=SIZING /FAO=3
SIZING2          <preallocated !UL file headers !UL directory entries> /IDENT=SIZING /FAO=2
PLACED1          <boot block at LBN !UL, HOM blocks at LBNs !UL and !UL> /IDENT=SIZING /FAO=3
PLACED2          < MFD at LBN !UL, index file at LBNs !UL thru !UL> /IDENT=SIZING /FAO=3
MFGBAD           <writing manufacturer's bad block index>
SWBAD            <writing software bad block descriptor>

        .SEVERITY WARNING
        .BASE 600
INVOPT           <!AZcreate and !AZreplace only apply to disk image files> /FAO=2

        .SEVERITY ERROR
        .BASE 800
BADVOLLABEL      <volume label must be 1-12 alphanumeric charactes>
BADUSERNAME      <username must be 1-12 alphanumeric characters>
BADMEDIUM        <unknown medium '!AZ'; HELP INITIALIZE for list> /FAO=1

        .SEVERITY SEVERE
        .BASE 1000
TOOSMALL         <device is too small to hold a useful Files-11 volume>
NOHOMLOC         <no good block found for primary HOM block>
NOHOM2LOC        <no good block found for secondary HOM block> /IDENT=NOHOMLOC

        .END


        .FACILITY INIT,117 /SYSTEM

        .SEVERITY WARNING
BADDATCHK        <invalid keyword on /DATACHECK qualifier>
BADDENS          <unknown value on /DENSITY qualifier>
BADPRO           <invalid syntax on /PROTECTION qualifier>
BADUIC           <invalid UIC syntax>
CONFQUAL         <conflicting qualifiers>
ILLOPT           <qualifier(s) not appropriate to this device>
NONLOCAL         <device is not a local device>
NOTRAN           <logical name cannot be translated>
UNKDEV           <unknown device type>
BADLABELQ        <invalid keyword on /LABEL qualifier>
VALCNVERR        <error converting value>

        .BASE 15
ALLOCFAIL        <index file allocation failure>
BADBLOCKS        <invalid syntax in bad block list>
BADINDEX         <invalid syntax on /INDEX qualifier>
BADRANGE         <bad block address not on volume>
CLUSTER          <unsuitable cluster factor>
DIAGPACK         <disk is a diagnostic pack>
FACTBAD          <cannot read factory bad block data>
INDEX            <invalid index file position>
MAXBAD           <bad block table overflow>

        .BASE 25
BADOVR           <unknown keyword on /OVERRIDE qualifier>
BADVOL1          <bad VOL1 ANSI label>
LARGECNT         <disk too large to be supported>
BADSTRUCT        <invalid value on /STRUCTURE qualifier>
NOTSTRUC1        <qualifier(s) not available with structure level 1>
BADVOLACC        <invalid value on /LABEL=VOLUME_ACCESSIBILITY qualifier>
BADVOLLBL        <invalid value for ANSI tape volume label>
MTLBLLONG        <label specified for !AC!UW: is longer then 6 characters!/> /FAO=2
MTLBLNONA        <label specified for !AC!UW: contains non-ANSI "a" characters!/> /FAO=2
BADOWNID         <invalid value on /LABEL=OWNER_IDENTIFIER qualifier>
BADACCESSED      <value for /ACCESSED qualifier out of range>
BADCLUSTER       <value for /CLUSTER_SIZE qualifier out of range>
BADDIRECTORIES   <value for /DIRECTORIES qualifier out of range>
BADEXTENSION     <value for /EXTENSION qualifier out of range>
BADHEADERS       <value for /HEADER qualifier out of range>
BADMAXIMUM       <value for /MAXIMUM_FILES qualifier out of range>
BADWINDOWS       <value for /WINDOWS qualifier out of range>

        .BASE 512
BLKZERO          <block zero is bad - volume not bootable>
NOBADDATA        <bad block data not found on volume>
ERASEFAIL        <volume not completely erased>

        .END


        .FACILITY IO,94

        .SEVERITY INFO
        .BASE 400
STATS            <initializations: !UL Reads: !UL Writes: !UL> /FAO=3
LARGEFILE        "large files and devices (>2GB) are supported"
NOLARGEFILE      "large files and devices (>2GB) are NOT supported"
UNXDEVHDR        < Physical devices>
NODEVS           <no devices found>
NTDEVHEADER      < Physical devices!/  Drv   Type    Physical Name!/  --- --------- ------------->
NTDEVREMOVABLE   <   !AZ Removable !AZ> /FAO=2
NTDEVFIXED       <   !AZ Fixed     !AZ> /FAO=2
NTDEVREMOTE      <   !AZ Network> /FAO=1
NTDEVCDROM       <   !AZ CDROM     !AZ> /FAO=2
NTDEVRAMDISK     <   !AZ RAMdisk   !AZ> /FAO=2
NTDEVOTHER       <   !AZ Other> /FAO=1
VNODEVS          < No virtual devices are assigned>
VDEVHEADER       < Virtual devices>
VHDAVAIL         <VHD format image files are supported>
VHDNOTAVAIL      <VHD format image file support is configured, but not available>
VHDFMTSTART      <formatting !AZ> /FAO=1
VHDFMTEND        <VHD volume formatting completed>
VMSMOUNT         <device !AZ mounted /FOREIGN> /FAO=1
VMSDMT           <device !AZ dismounted> /FAO=1

        .SEVERITY WARNING
        .BASE 600
VHDSAVED         <!AZ contains a suspended system and should not be modified> /FAO=1

        .SEVERITY ERROR
        .BASE 800
NOVHD            <VHD disk support is not available in this version>
ISVIRTDEV        <!AD is a virtual device> /FAO=2
VIRTDEVINUSE     <!AZ is in use by !AZ> /FAO=2
VIRTFILEINUSE    <!AZ is in use on virtual drive !AZ> /FAO=2
BADPATH          <invalid path: !AZ> /FAO=1
DIRRDERR         <error reading directory !AZ> /FAO=1
LOCKVOL          <error locking volume on !AZ> /FAO=1
NOGEO            <unable to obtain geometry for !AZ> /FAO=1
OPENDEV          <error opening !AZ> /FAO=1
READERR          <read error at sector !UL> /FAO=1
READLEN          <read error at lbn !UL, got !UL, expected !UL> /FAO=3
WRITELEN         <read error at lbn !UL, got !UL, expected !UL> /FAO=3
SEEKERR          <seek failure at sector !UL> /FAO=1
WRITEERR         <write error at sector !UL> /FAO=1
UNXNOTREG        <!AZ is not a regular file> /FAO=1
UNXNOTBLK        <!AZ is not a block device> /FAO=1
EXISTS           <file !AZ already exists, not superseded> /FAO=1
RDBLK2BIG        <read from non-existent LBN !UL> /IDENT=BLK2BIG /FAO=1
WRBLK2BIG        <write to non-existent LBN !UL> /IDENT=BLK2BIG /FAO=1
VHDONLY          <!AZ Snapshots can only be taken of VHD format image files> /FAO=1
VHDSNAPFAIL      <snapshot of !AZ failed> /FAO=1
VHDMEM           <failed to allocate memory for VHD>
VHDCREATEERR     <error creating VHD volume !AZ> /FAO=1
VMSDEVSCAN       <error scanning devices>
VMSNOMOUNT       <unable to mount device !AZ /FOREIGN> /FAO=1
VMSNODMT         <unable to dismount device !AZ> /FAO=1

        .SEVERITY SEVERE
        .BASE 1000
OFFSET_TOO_BIG   <Volume is too large for local file system: needs 64-bit I/O>
VHDNOLIB         <VHD library !AZ could not be loaded> /FAO=1
VHDNOSYM         <symbol !AZ not present in !AZ> /FAO=2
VHDNOTVALID      <the VHD library !AZ is not usable> /FAO=1

        .END


        .FACILITY MOUNT,95 /SHARED
        .LITERAL MOUNT_OFFSET          = (95)                                                ! 0x00000058 Warning

        .END

        .FACILITY MOUNT,95

        .SEVERITY INFO
        .BASE 400
MOUNTED          <!12AD mounted on !AZ> /FAO=3
SNAPOK           <!AZ created from !AZ> /FAO=2
VSMOUNTED        <volume set !12AD mounted> /FAO=1
WRITELCK         <!AZ can not be written, mounted read-only> /FAO=1

        .SEVERITY WARNING
        .BASE 600
BADSCB           <invalid storage control block on !AZ> /FAO=1

        .SEVERITY ERROR
        .BASE 800
BADLABEL         <label !AZ is too long> /FAO=1
FAILED           <mount failed>
HOMBLKCHK        <!UL home block software consistency error!%S> /FAO=1
BITMAPSYS        <unable to access BITMAP.SYS on !AZ> /FAO=1
BITUPDFAIL       <unable to compute disk usage for !AZ> /FAO=1
INCONVOL         <volume '!12AD' on !AZ is a member of '!12AD', not a member of '!12AD'> /FAO=7
NOTSAME          <you must specify the same number of existing files (!UL) with !AZSNAPSHOT as filenames to create (!UL)> /FAO=3
RVN1NOTMNT       <volume 1 of the volume set must be mounted>
VOLOOO           <RVN !UL of '!12AD' is '!12AD'.  !AZ contains '!AD'> /FAO=8
VOLSETSYS        <unable to access VOLSET.SYS on !AZ> /FAO=1
WRONGRVN         <device !AZ contains !12AD, which is RVN !UW of !12AD. !UL of !12AD expected> /IDENT=WRONGVOL /FAO=9
NOTVSMEM         <device !AZ contains !12AD, which is not part of a volume set.  RVN !UL of !12AD expected> /FAO=6
WRONGVOL         <device !AZ contains volume '!12AD', '!12AD' expected> /FAO=5
WRONGVOLCNT      <volume set !12AD has !UL members, but !UL specified> /FAO=3
WRONGVOLMEM      <volume set name on !AZ is '!12AD', but VOLSET.SYS is for '!12AD'> /FAO=5

        .END


        .FACILITY MOUNT,114 /SYSTEM

        .SEVERITY WARNING
BADDATCHK        <invalid keyword on /DATACHECK qualifier>
BADDENS          <unknown value on /DENSITY qualifier>
BADPRO           <invalid syntax on /PROTECTION qualifier>
BADUIC           <invalid UIC syntax>
CONFQUAL         <conflicting qualifiers>
ILLOPT           <qualifier(s) not appropriate to this device>
NONLOCAL         <device is not a local device>
NOTRAN           <logical name cannot be translated>
UNKDEV           <unknown device type>
BADLABELQ        <invalid keyword on /LABEL qualifier>
VALCNVERR        <error converting value>

        .BASE 15
LOGNAME          <logical name is too long>
MAXDEV           <too many devices>
MAXLAB           <too many volume labels>
NOACPDEV         <device named by /PROCESSOR:SAME has no ACP>
MOUVOL           <MOUNT relative volume !ZB (!AD) on !AD!/> /FAO=5
INCOMPACP        <indicated ACP is incompatible with volume>
INCOMPAT         <qualifiers incompatible with already mounted volume>
VOLALRMNT        <another volume of same label already mounted>
BADVOL1          <bad VOL1 ANSI label>

        .BASE 25
NOTGENDEV        <not a generic device>
DUPDEVSPC        <duplicate device specified>
ILLANSIBS        <ANSI magnetic tape block size must be at least 18 characters>
ACCERR           <non-blank accessibility field in volume labels on !AD!/> /FAO=2
FILNOTEXP        <file not expired on volume on !AD!/> /FAO=2
NOPRIV           <user denied privilege to write to volume on !AD!/> /FAO=2
NOTANSI          <not ANSI volume on !AD!/> /FAO=2
NOTLABEL         <(!AD) not on !AD!/> /FAO=4
NOTVOLSET        <volume on !AD not in volume set !AD!/> /FAO=4
NOTRELVOL        <Relative volume !ZB not on !AD!/> /FAO=3
OFFLINE          <!AD offline> /FAO=2
IOERROR          <I/O error initializing volume on !AD!/> /FAO=2
ILLQUAL          <invalid qualifier>
WRITLCK          <volume on !AD is write locked!/> /FAO=2
RECGTRBL         <record size is greater than block size>
ACPNAME          <ACP file name is too long>
ACPTIME          <ACP does not respond>
AQBTIME          <ACP queue is unstable>
BADACP           <invalid syntax on /PROCESSOR qualifier>
BADEBC           <unknown keyword on /EBCDIC qualifier>
BADOVR           <unknown keyword on /OVERRIDE qualifier>
DEVICES          <only one device allowed>
SZTOOBIG         <record size or block size greater than 65534>
INCONSDEV        <inconsistent device types>
DEVCOUNT         <number of devices must match number of volumes>
VOLINSET         <volume is already part of another volume set>
RVN1NOTMT        <root volume is not mounted>
SETLIMIT         <too many volumes in volume set>
DUPVOLNAM        <volume label already present in set>
HOMBLKCHK        <home block software consistency error>
DUPRVN           <duplicate volume number already mounted>
BADCACHE         <invalid syntax on /CACHE qualifier>
UNRELVOL         <volume is not part of volume set being mounted>
MBXCRERR         <error creating operator reply mailbox>
MBXRDERR         <error reading operator reply mailbox>
BADREPLY         <unknown type of operator reply, reply = !XW!XW!XL!AF> /FAO=5
OPRSNDERR        <error sending request to operator>
OPRABORT         <mount aborted by operator>
BATCHNOOPR       <no operator available to service batch request>
MOUNEWVOL        <MOUNT new relative volume !ZB (!AD) on !AD!/> /FAO=5
REMOUVOL         <REMOUNT relative volume !ZB (!AD) on !AD!/> /FAO=5
OBSOLETE         < >
VOLOERR          <must override owner identifier field in VOL1 label on !AD!/> /FAO=2
BADINIT          <unknown keyword on /INITIALIZE qualifier>
DISMAL           <all of the volumes have been dismounted>
INCONSHR         <inconsistent sharing option.  Cluster mounted !AD> /FAO=2
INCONPROT        <inconsistent /PROTECTION option.  Cluster mounted !AD> /FAO=2
INCONQUOTA       <inconsistent /(NO)QUOTA option.  Cluster mounted !AD> /FAO=2
INCONOWNER       <inconsistent /OWNER_UIC option.  Cluster mounted !AD> /FAO=2
INCONWRITE       <inconsistent /(NO)WRITE option.  Cluster mounted !AD> /FAO=2
INCONFOR         <inconsisten /FOREIGN option.  Cluster mounted !AD> /FAO=2
NOVOLACC         <user has no access to magnetic tape volume on !AD!/> /FAO=2
NOFILACC         <user has no access to magnetic tape file on !AD!/> /FAO=2
TAPEPOSLOST      <magnetic tape postion lost on !AD!/> /FAO=2
BADSHADOW        <invalid syntax on /SHADOW qualifer>
NOSHDWMEM        <no eligible shadow set members to mount the shadow set>
INVSHDWMEM       <shadow set member !AS is 'newer' than the shadow set> /FAO=1
SHDWCOPYREQ      <shadow copy required>
SHADOWFAIL       <failed to create (or add to) the shadow set>
SHDWINCVOL       <shadow set member device contains the wrong volume>
NOTSHDWDEV       <not a shadowing device>
BADMDFMT         <invalid syntax on /MEDIA_FORMAT qualifier>
NOTACLU          <!AS is not a cluster-wide device> /FAO=1
NOALOCLASS       <allocation class not allowed with shadowing phase II virtual unit name>
BADASNUNIT       <invalid keyword on /ASSIGNMENT_UNIT qualifier>
BADDISP          <invalid keyword on /DISPOSITION qualifier>

        .BASE 512
HOMBLKBAD        <primary home block is bad; backup used>
IDXHDRBAD        <index file header is bad; backup used>
MAPHDRBAD        <storage map header is bad; volume locked>
IDXMAPERR        <I/O error on index file bitmap; volume locked>
BITMAPERR        <I/O error on storage bitmap; volume locked>
BADHOMBLK        <bad home block encountered on volume>
WRTHOMBLK        <I/O error rewriting home block>
QUOTAFAIL        <failed to activate quota file; volume locked>
BITMAPINV        <storage bitmap is invalid; volume locked>
WRITESCB         <write error on storage control block; volume write locked>
DISMPART         <some of the volumes have been dismounted>
UNUSED           <unused>
DISMOUNTD        <volume !AS of volume set !AS dismounted> /FAO=2
VOLSHDWMEM       <mounting a shadow set member volume; volume write locked>
RMTMNTFAIL       <!AS failed to mount on node !AS> /FAO=2
MNTCLUFAIL       <mount/cluster failed on one or more nodes in the cluster>
QUOTAFNF         <quota file not found on volume>

        .BASE 1024
MOUNTED          <!AF mounted on !AS> /FAO=3
VOLIDENT         <label = '!AF', owner = '!AF', format = '!AF'> /FAO=6
WRITELOCK        <volume is write locked>
REBUILD          <volume was improperly dismounted; rebuild in progress>
OPRQST           <!AS> /FAO=1
OPREPLY          <!AS> /FAO=1
OPRQSTCAN        <operator request canceled>
NOOPR            <no operator available to service request>
INVLDRESP        <invalid operator response>
MOUNTDEV         <Please mount!AS device !AS!AS> /FAO=3
VOLNAME          < volume !AS in> /FAO=1
NODEVAVL         <Device !AS is not available for mounting.!AS> /FAO=2
RPLYARRIVD       <operator reply has arrived>
WRONGVOL         <device !AS contains the wrong volume> /FAO=1
RQSTDON          <operator request canceled - mount completed successfully>
VOLSTATUS        <change of status for volume !AS> /FAO=1
VOLSETSTS        <change of status for volume set !AS> /FAO=1
REDCACHE         <volume mounted with reduced cache size>
REBLDREQD        <rebuild not performed; some free space unavailable; diskquota usage stale>
SHDWMEMSUCC      <!AS is now a valid member of the shadow set> /FAO=1
SHDWMEMCOPY      <!AS added to the shadow set with a copy operation> /FAO=1
SHDWMEMFAIL      <!AS failed as a member of the shadow set> /FAO=1
MOUNT            < Mount  >
WAITINGMNT       <waiting for !AS to mount on node !AS> /FAO=2
RMTMNTSUCC       <!AS mounted successfully on node !AS> /FAO=2
SHDWMEMERGE      <!AS added to the shadow set with a merge operation> /FAO=1
AUTOMEMSUCC      <!AS automatically added to the shadow set> /FAO=1
AUTOMEMCOPY      <!AS automatically added to the shadow set with a copy operation> /FAO=1
AUTOMEMERGE      <!AS automatically added to the shadow set with a merge operation> /FAO=1
MOUNTVOL         <Please mount!AS !AS!AS> /FAO=3
ANYSCRATCH       < a scratch volume in>
SCRATCHCOL       < a scratch volume from collection !AS in> /FAO=1
SCRATCHLOC       < a scratch volume from location !AS in> /FAO=1
SCRCOLLOC        < a scratch volume from collection !AS, location !AS in> /FAO=2
SUITDEV          <a suitable device>
DEVTYPE          <a device of type !AS>
DEVICE           <device !AS>
MEDIANAME        < media !AS in>
CARTRIDGE        < cartridge !AS in>
CARTSIDE         < cartridge !AS, side !UW in>

        .BASE 1500
INVQUAVAL        <value '!AD' invalid for /!AS qualifier> /FAO=2
DEVNOFE          <device does not support FORCED ERROR handling.!/To override use the /OVERRIDE=NO_FORCED_ERROR qualifier>

        .END


        .FACILITY ODS2,84 /SHARED
        .LITERAL ODS2_NORMAL          = ((ODS2_FACILITY@16) + SHR_SUCCESS)                  ! 0x08540001 Success
        .LITERAL ODS2_OPENIN          = ((ODS2_FACILITY@16) + SHR$_FILDEL)                  ! 0x08541320 Warning

        .END

        .FACILITY ODS2,84

        .SEVERITY SUCCESS
        .BASE 200
CONFIRM_ALL      </CONFIRM ALL response>
CONFIRM_YES      </CONFIRM YES response>

        .BASE 204
EXIT             <exiting>

        .SEVERITY INFO
        .BASE 400
MSGFILE          <Message definition file: !AZ> /FAO=1
INVDEF           <!AZ does not exist> /FAO=1
HELPFILE         <Help library file: !AZ> /FAO=1
HELPFILEVER1     <Version !UL !UL-bit created !AZ> /IDENT=HELPFILEVER /FAO=3
HELPFILEVER2     <Version !UL !UL/!UL-bit created !AZ> /IDENT=HELPFILEVER /FAO=4

        .SEVERITY WARNING
        .BASE 202
CONFIRM_NO       </CONFIRM NO response>
CONFIRM_QUIT     </CONFIRM QUIT response>

        .BASE 600
FILHDRACC        <unable to read file header>
SETCWD           <failed to set cwd>
LOCALE           <unable to set locale>

        .SEVERITY ERROR
        .BASE 800
AMBIGUOUS        <!AZ '!AZ' is ambiguous> /FAO=2
UNKNOWN          <unrecognized !AZ '!AZ'> /FAO=2
NOVALUE          <no value allowed for !AZ !AZ> /FAO=2
VALUEREQ         <value required for !AZ !AZ> /FAO=2
NOPAREN          <value is missng ')' for !AZ '!AZ'> /FAO=2
TOOMANY          <too many !AZ specified> /FAO=1
TOOFEW           <too few !AZ specified> /FAO=1
MAXIND           <maximum indirect command file nesting limit (!UL) exceeded> /FAO=1
UNTERMSTR        <unterminated quoted string>
INVNUMBER        <!AZ is not a valid number> /FAO=1
NOQUALS          <no qualifiers specified>
NOQUALSOK        <no qualifiers permitted> /IDENT=NOQUALS
NOPAROK          <no parameters permitted>
INCONQUAL        <inconsistent use of qualifiers>
BADPRO           <invalid syntax for file or directory protection>
BADUIC           <invalid syntax for UIC. [group,member] must be numeric>
BADVALUE         <invalid value>
OPENIND          <failed to open indirect command file !AZ> /FAO=1
NOSHELL          <failed to run !AZ> /FAO=1
OSERROR          <the last host OS error was: !AZ> /FAO=1
OSERRORNO        <the last host OS error was !UL.> /IDENT=OSERROR /FAO=1
VMSTIME          <failed to convert VMS time quadword>
FCS2DV           <rename operation failed--different devices specified>
BADORG           <unsupported file organization>
ILLDIRWRT        <directories may not be written as files>
FILESTRUCT       <unsupported file structure level !UL.!UL> /FAO=2

        .SEVERITY SEVERE
        .BASE 1000
INITIALFAIL      <initialization failed>
BADMSGFILE       <invalid message file format>
NOMSG            <message number !8XL> /FAO=1

        .END


        .FACILITY RENAME,96 /SHARED
        .LITERAL RENAME_PARSEFAIL       = ((RENAME_FACILITY@16) + SHR$_NOFILES + 2)           ! 0x08601332 Error
        .LITERAL RENAME_SEARCHFAIL      = ((RENAME_FACILITY@16) + SHR$_QUALMISS + 2)          ! 0x08601342 Error

        .END

        .FACILITY RENAME,96

        .SEVERITY INFO
        .BASE 400
RENAMED          <!AZ renamed to !AZ> /FAO=2
CONFIRM          <rename !AZ to !AZ? > /FAO=2
NRENAMED         <!UL file(s) renamed> /IDENT=RENAMED /FAO=1
NOFILES          <no files renamed>

        .SEVERITY ERROR
        .BASE 800
FAILED           <rename !AZ to !AZ failed> /FAO=2

        .END


        .FACILITY RMS,1 /SYSTEM /SHARED
        .LITERAL RMS$_NORMAL          = ((RMS$_FACILITY@16) + SHR_SUCCESS)                  ! 0x00010001 Success
        .LITERAL RMS$_CONTROLO        = ((RMS$_FACILITY@16) + SS$_CONTROLO)                 ! 0x00010609 Success
        .LITERAL RMS$_CONTROLY        = ((RMS$_FACILITY@16) + SS$_CONTROLY)                 ! 0x00010611 Success
        .LITERAL RMS$_CREATED         = ((RMS$_FACILITY@16) + SS$_CREATED)                  ! 0x00010619 Success
        .LITERAL RMS$_SUPERSEDE       = ((RMS$_FACILITY@16) + SS$_SUPERSEDE)                ! 0x00010631 Success
        .LITERAL RMS$_CONTROLC        = ((RMS$_FACILITY@16) + SS$_CONTROLC)                 ! 0x00010651 Success
        .LITERAL RMS$_OVRDSKQUOTA     = ((RMS$_FACILITY@16) + SS$_OVRDSKQUOTA)              ! 0x00010669 Success
        .LITERAL RMS$_FILEPURGED      = ((RMS$_FACILITY@16) + SS$_FILEPURGED)               ! 0x00010679 Success

        .END

        .FACILITY RMS,1 /SYSTEM

        .SEVERITY SUCCESS
        .BASE 0
STALL            <synchronize with operation completion>
PENDING          <asynchronous operation not yet completed>
OK_DUP           <record had duplicate key>
OK_IDX           <index update error occurred>
OK_RLK           <record locked but read anyway>
OK_RRL           <record locked against read but read anyway>
KFF              <known file found>
OK_ALK           <record is already locked>
OK_DEL           <deleted record successfully accessed>
OK_RNF           <nonexistent record successfully accessed>
OK_LIM           <retrieved record exceeds specified key value>
OK_NOP           <XAB not filled in for block I/O>
OK_WAT           <record locked after wait>
CRE_STM          <file was created in stream format>
OK_RULK          <record relocked in recovery unit>
SYNCH            <synchronous completion>

        .SEVERITY WARNING
OK_ACT           <operation successfully completed on active file or stream>
OK_NOCURTID      <no default TID is defined>

        .BASE 51
BOF              <beginning of file detected>
RNL              <record not locked>
RTB              <!UL byte record too large for user's buffer> /FAO=1
TMO              <timeout period expired>
TNS              <terminator not seen>
BES              <bad escape sequence>
PES              <partial escape sequence>

        .BASE 109
OPNOTSUP         <operation not supported by RMS Extension>
EXTNOTFOU        <RMS Extension not found>
EXT_ERR          <error in RMS Extension; reason code %X!XL> /FAO=1
SEMANTICS        <inconsistent usage of RMS Semantics>

        .BASE 2064
DDTM_ERR         <error returned by Digital Distributed Transaction Manager >

        .SEVERITY ERROR
        .BASE 75
ACT              <file activity precludes operation>
DEL              <RFA-accessed record was deleted>
INCOMPSHR        <incompatible file sharing requested>
DNR              <device not ready, not mounted, or unavailable>
EOF              <end of file detected>
FEX              <file already exists, not superseded>
FLK              <file currently locked by another user>
FNF              <file not found>
PRV              <insufficient privilege or file protection violation>
REX              <record already exists>
RLK              <target record currently locked by another stream>
RNF              <record not found>
WLK              <device currently write locked>
EXP              <file expiration date not yet reached>
NMF              <no more files found>
SUP              <network operation not supported; DAP code = !XL> /FAO=1
RSA              <record stream currently active>
CRC              <network DAP level CRC check failed>
WCC              <invalid wild card context (WCC) value>
IDR              <invalid directory rename operation>
LWC              <lock wait canceled>
UNUSED1          <unused error message 1>
NOVALPRS         <$SEARCH operation not preceded by valid $PARSE>
KEY_MISMATCH     <record found, but key doesn't match>
RUH              <error encountered by RMS transaction event handler, PID = !XL> /FAO=1
JND              <journaling disabled on this file>
BADPHASE         <recovery unit phase invalid for this operation>
TOWDR            <timed out waiting for detached RU recovery to complete>
NEXDR            <non-existent detached RU recovery process>
INVDRMSG         <invalid detached RU recovery mailbox message>
RU_ACTIVE        <operation prohibited while recovery unit active>
UNKRUFAC         <file has active/incomplete non-RMS recovery units>
LIMBO            <recovery unit is in limbo>
IVATRACE         <invalid RMS attributes ACE encountered>

        .BASE 2048
ACC              <ACP file access failed>
CRE              <ACP file create failed>
DAC              <ACP file deaccess failed during $CLOSE>
ENT              <ACP enter function failed>
EXT              <ACP file extend failed>
FND              <ACP file or directory lookup failed>
MKD              <ACP could not mark file for deletion>
DPE              <device positioning error>
SPL              <spool or submit of command file failed on $CLOSE>
DNF              <directory not found>
RUF              <recovery unit facility request failed>
WRTJNL_AIJ       <error writing to after image journal file>
WRTJNL_BIJ       <error writing to before image journal file>
WRTJNL_ATJ       <error writing to audit trail journal file>
WRTJNL_RUJ       <error writing to recovery unit journal file>
RRF              <recovery unit recovery failed>

        .SEVERITY SEVERE
        .BASE 125
DTFCDDREC        <error accessing CDD record; reason code %X!XL> /FAO=1
AID              <invalid area ID = !UL> /FAO=1
ALN              <alignment options error for area id = !UL> /FAO=1
ALQ              <invalid allocation quantity (negative, or 0 on $EXTEND)>
ANI              <not ANSI "D" format>
AOP              <allocation options error for area id = !UL> /FAO=1
BKS              <bucket size too large (FAB)>
BKZ              <inconsistent bucket size for area id = !UL> /FAO=1
BLN              <invalid block length for control block (RAB/FAB)>
BUG              <fatal RMS condition detected, process deleted>
BUG_DDI          <invalid default directory>
BUG_DAP          <Data Access Protocol error detected; DAP code = !XL> /FAO=1
BUG_RU_ACTIVE    <active recovery units on $CLOSE during RMS rundown>
BUG_RURECERR     <error during detached RU recovery>
BUG_FLUSH_JNL_FAILED <flush of journal failed> /IDENT=BUG_FLUSH_JNL_F
BUG_RU_ABORT_FAIL <recovery unit abort failed> /IDENT=BUG_RU_ABORT_FA
BUG_RU_COMMIT_FAIL <recovery unit commit failed> /IDENT=BUG_RU_COMMIT_F
BUG_XX6          <reserved bug code>
BUG_XX7          <reserved bug code>
BUG_XX8          <reserved bug code>
BUSY             <user structure (FAB/RAB) still in use>
CCR              <cannot connect RAB>
CHG              <invalid key change in $UPDATE (CHG not set)>
CHK              <bucket format check failed for VBN = !UL> /FAO=1
COD              <invalid or unsupported type field in XAB at !XL> /FAO=1
CUR              <no current record (operation not preceded by $GET/$FIND)>
DAN              <invalid data area number for key = !UL> /FAO=1
DEV              <error in device name or inappropriate device type for operation>
DIR              <error in directory name>
DME              <dynamic memory exhausted>
DNA              <invalid default file name string address>
DTP              <invalid key data type for key = !UL> /FAO=1
DUP              <duplicate key detected (DUP not set)>
DVI              <invalid device ID value in NAM block>
ESA              <invalid expanded string address>
ESS              <expanded string area too small>
FAB              <invalid FAB or FAB not accessible>
FAC              <record operation not permitted by specified file access (FAC)>
FLG              <invalid key definition flags for key = !UL> /FAO=1
FNA              <invalid file name string address>
FNM              <error in file name>
FSZ              <invalid fixed control header size>
FOP              <invalid file options>
FUL              <device full (insufficient space for allocation)>
IAL              <invalid argument list>
IAN              <invalid index area number for key = !UL> /FAO=1
IDX              <index not initialized>
IFI              <invalid internal file identifier (IFI) value>
IMX              <invalid duplicate XAB or non-dense XAB at !XL> /FAO=1
IOP              <operation invalid for file organization or device>
IRC              <illegal record encountered; VBN or record number = !UL> /FAO=1
ISI              <invalid internal stream identifier (ISI) value>
KBF              <invalid key buffer>
KEY              <invalid record number key or key value>
KRF              <invalid key-of-reference for $GET/$FIND>
KSZ              <invalid key size for $GET/$FIND>
LAN              <invalid lowest-level-index area number for key = !UL> /FAO=1
RUNDOWN          <operation invalid during RMS rundown>
LNE              <logical name translation count exceeded>
DTFCVT           <error converting data in record; reason code %X!XL> /FAO=1
MRN              <invalid maximum record number or record number exceeds MRN>
MRS              <invalid maximum record size>
NAM              <invalid NAM block or NAM block not accessible>
NEF              <not positioned to EOF on $PUT (sequential organization only)>
DTFQUASYN        <syntax error in foreign file specification; reason code %X!XL> /FAO=1
NOD              <error in node name>
NPK              <no primary key defined for indexed file>
ORD              <XABs not in correct order at !XL> /FAO=1
ORG              <invalid file organization value>
PBF              <invalid prompt buffer>
PLG              <error detected in file's prolog (reconstruct file)>
POS              <invalid key position (greater than MRS) for key = !UL> /FAO=1
DTFQUAVAL        <invalid file name qualifier value specified; reason code %X!XL> /FAO=1
QUO              <error in quoted string>
RAB              <invalid RAB or RAB not accessible>
RAC              <invalid record access mode>
RAT              <invalid record attributes>
RBF              <invalid record buffer>
RFA              <invalid record's file address (RFA)>
RFM              <invalid record format>
RHB              <invalid record header buffer>
RLF              <invalid related NAM block>
ROP              <invalid record options>
RRV              <invalid RRV record encountered>
RVU              <error updating RRV's, some paths to data may be lost>
RSS              <invalid resultant string size>
RST              <invalid resultant string address>
RSZ              <invalid record size>
SEQ              <primary key out of sequence (SEQ access)>
SHR              <invalid file sharing (SHR) options>
SIZ              <invalid key size for key = !UL> /FAO=1
SQO              <operation not sequential (SQO set)>
DTFSESEST        <error establishing SNA session; reason code %X!XL> /FAO=1
SYN              <file specification syntax error>
TRE              <error in index tree>
TYP              <error in file type>
UBF              <invalid user buffer>
USZ              <invalid user buffer size>
VER              <error in version number>
XNF              <required XAB not found>
XAB              <invalid XAB or XAB not accessible at !XL> /FAO=1
ESL              <invalid expanded string length>
DTFSESTER        <SNA session terminated unexpectedly; reason code %X!XL> /FAO=1
ENV              <support for RMS service call or feature not present>
PLV              <unsupported prolog version>
MBC              <invalid multi-block count>
RSL              <invalid resultant string length>
WLD              <invalid wildcard operation>
NET              <network operation failed at remote node; DAP code = !XL> /FAO=1
IBF              <illegal bucket format in VBN = !UL> /FAO=1
REF              <invalid key-of-reference = !UL> /FAO=1
IFL              <index bucket fill size larger than bucket size for key = !UL> /FAO=1
DFL              <data bucket fill size larger than bucket size for key = !UL> /FAO=1
KNM              <key name buffer not accessible for key = !UL> /FAO=1
IBK              <LAN bucket size not equal to IAN bucket size for key = !UL> /FAO=1
KSI              <index bucket will not hold two keys for key-of-reference = !UL> /FAO=1
LEX              <invalid $EXTEND for area !UL containing an unused extent> /FAO=1
SEG              <overlapping segments or segmented key must be string for key = !UL> /FAO=1
SNE              <file sharing not enabled>
SPE              <file sharing pagecount exceeded>
UPI              <UPI not set when sharing and BIO or BRO set>
ACS              <error in access control string>
STR              <user structure (FAB/RAB) became invalid during operation>
FTM              <network file transfer mode precludes operation (SQO set)>
GBC              <invalid global buffer count>
DEADLOCK         <deadlock detected>
EXENQLM          <exceeded enqueue quota>
JOP              <journaling options not supported on device>
RUM              <recovery-unit handler called from incorrect access mode>
JNS              <operation not supported by RMS journaling>
NRU              <operation prohibited outside recovery-unit>
IFF              <invalid file attributes for requested file access>
DTFTRATBL        <error loading translation table; reason code %X!XL> /FAO=1
DTFUNSTYP        <unsupported data type in record; reason code %X!XL> /FAO=1
DTFVERMIS        <incompatible version of control file; reason code %X!XL> /FAO=1
DTFACC           <error accessing file; reason code %X!XL> /FAO=1
BOGUSCOL         <invalid or inaccessible collate table>
ERRREADCOL       <error encountered while reading collate table>
ERRWRITECOL      <error encountered while writing collate table>
SNS              <statistics monitoring not supported for non-disk devices>
NOEXTEND         <previous $EXTEND operation failed - further $EXTENDs prohibited>
DTFCRE           <error creating file; reason code %X!XL> /FAO=1
DELJNS           <deleting a file marked for journaling not yet supported (unmark file for journaling first)>
NOTSAMEJNL       <files in a recovery unit must use same RMS journal>
SNPPF            <statistics monitoring not supported for process permanent files>

        .BASE 2073
ATR              <file attributes read error>
ATW              <file attributes write error>
CCF              <cannot close file>
CDA              <cannot deliver AST>
CHN              <assign channel system service request failed>
RER              <file read error>
RMV              <ACP remove function failed>
RPL              <error detected while reading prolog>
SYS              <QIO system service request failed>
WER              <file write error>
WPL              <error detected while writing prolog>
IFA              <illegal file attributes detected (file header corrupted)>
WBE              <error on write behind>
ENQ              <ENQ system service request failed>
NETFAIL          <network operation failed at remote node>
SUPPORT          <network operation not supported>
CRMP             <CRMPSC system service failed to map global buffers>
DTFCFGFIL        <error accessing configuration file>
REENT            <file could not be renamed and recovery failed; file has been lost>
ACC_RUJ          <recovery unit journal can not be accessed>
TMR              <SETIMR system service request failed>
ACC_AIJ          <after image journal can not be accessed>
ACC_BIJ          <before image journal can not be accessed>
ACC_ATJ          <audit trail journal can not be accessed>
DTFDEFFIL        <error accessing file definition file>
DTFREGFIL        <error accessing registration file>
JNLNOTAUTH       <VAX RMS Journaling not authorized; operation not performed>

        .END


        .FACILITY SEARCH,97 /SHARED
        .LITERAL SEARCH_CONTINUE        = ((SEARCH_FACILITY@16) + SHR$_DIRNOTCRE + 3)         ! 0x086112c3 Info
        .LITERAL SEARCH_NOFILES         = ((SEARCH_FACILITY@16) + SHR$_TOTAL)                 ! 0x08611310 Warning
        .LITERAL SEARCH_OPENIN          = ((SEARCH_FACILITY@16) + SHR$_FILDEL)                ! 0x08611320 Warning
        .LITERAL SEARCH_PARSEFAIL       = ((SEARCH_FACILITY@16) + SHR$_NOFILES + 2)           ! 0x08611332 Error
        .LITERAL SEARCH_SEARCHFAIL      = ((SEARCH_FACILITY@16) + SHR$_QUALMISS + 2)          ! 0x08611342 Error

        .END

        .FACILITY SEARCH,97

        .SEVERITY INFO
        .BASE 400
CONFIRM          <search !AZ? > /FAO=1
NOMATCH          <no strings matched>
MATCHED          <!AZ searched, !UL record!%S, !UL match!%S> /FAO=3
FILE             <!AZ searched, !UL record!%S, no matches> /FAO=2

        .SEVERITY ERROR
        .BASE 800
BADSTRING        <search string !UL too short> /FAO=1

        .END


        .FACILITY SHR,0 /SHARED

        .SEVERITY WARNING
        .BASE 0
NORMAL           <normal successful completion>
        .LITERAL SHR_SEVERE          = (4)                                                 ! 0x00000000 Warning

        .BASE 600
CONTINUE         " Type <CR> to continue, f<CR> to finish, q<cr> to abort "
CLOSEIN          <error closing !AZ as input> /FAO=1
CLOSEOUT         <error closing !AZ as output> /FAO=1
COPIEDB          <!AZ copied to !AZ (!UL block!%S)> /IDENT=COPIED /FAO=3
COPIEDR          <!AZ copied to !AZ (!UL record!%S)> /IDENT=COPIED /FAO=3
CREATED          <!AZ created> /FAO=1
EXISTS           <!AZ already exists> /FAO=1
INVALIDNL        <VFC newline count must be between 1 and 128>
INVALIDVFU       <VFU channel must be between 1 an 16>
INVALIDC0        <VFC ASCII control must be between 0 and 31>
NOFILES          <no files found>
NOTDIR           <!AZ is not a directory> /FAO=1
OPENIN           <error opening !AZ as input> /FAO=1
OPENOUT          <error opening !AZ for output> /FAO=1
PARSEFAIL        <error parsing !AZ> /FAO=1
READERR          <error reading !AZ> /FAO=1
SEARCHFAIL       <error searching for !AZ> /FAO=1
WRITEERR         <error writing !AZ> /FAO=1

        .END


        .FACILITY SHR,0 /SYSTEM /SHARED

        .SEVERITY WARNING
        .BASE 512
APPENDEDB        <!AS appended to !AS (!UL block!%S)> /IDENT=APPENDED /FAO=3
APPENDEDR        <!AS appended to !AS (!UL record!%S)> /IDENT=APPENDED /FAO=3
BADBYTE          <invalid data (!XB) at !XL> /IDENT=BADDATA /FAO=2
BADFIELD         <!AS field invalid at !XL> /FAO=2
BADLONG          <invalid data (!XL) at !XL> /IDENT=BADDATA /FAO=2
BADWORD          <invalid data (!XW) at !XL> /IDENT=BADDATA /FAO=2
BEGIN            <!AS beginning> /FAO=1
BEGIND           <!AS beginning at !%D> /IDENT=BEGIN /FAO=2
BEGINT           <!AS beginning at !%T> /IDENT=BEGIN /FAO=2
CLICB            <CLI control block at !XL> /FAO=1
CLOSEIN          <error closing !AS as input> /FAO=1
CLOSEOUT         <error closing !AS as output> /FAO=1
COPIEDB          <!AS copied to !AS (!UL block!%S)> /IDENT=COPIED /FAO=3
COPIEDR          <!AS copied to !AS (!UL record!%S)> /IDENT=COPIED /FAO=3
CREATED          <!AS created> /FAO=1
ENDED            <!AS ended> /FAO=1
ENDEDD           <!AS ended at !%D> /IDENT=ENDED /FAO=2
ENDEDT           <!AS ended at !%T> /IDENT=ENDED /FAO=2
NEWFILES         <!UL file!%S created> /FAO=1
OPENIN           <error opening !AS as input> /FAO=1
OPENOUT          <error opening !AS as output> /FAO=1
OVERLAY          <!AS being overwritten> /FAO=1
READERR          <error reading !AS> /FAO=1
REPLACED         <!AS being replaced> /FAO=1
WILDCONCAT       <wildcard specification cannot be concatenated> /IDENT=CONCAT
WILDOUTVER       <wildcard version required for output file>
WRITEERR         <error writing !AS> /FAO=1
ABEND            <!AS aborted> /IDENT=ABORT /FAO=1
ABENDD           <!AS aborted at !%D> /IDENT=ABORT /FAO=2
ABENDT           <!AS aborted at !%T> /IDENT=ABORT /FAO=2
SYSERRORPC       <!AS system service error at PC !XL> /IDENT=SYSERROR /FAO=2
SYNTAX           <error parsing '!AS'> /FAO=1
NOVALUE          <'!AS' keyword requires a value> /FAO=1
BADKEY           <'!AS' is an invalid keyword> /FAO=1
BADVALUE         <'!AS' is an invalid keyword value> /FAO=1
BADDELIM         <invalid delimiter following '!AS'> /FAO=1
BADLOGIC         <internal logic error detected>
NOWILD           <no wildcard permitted>
TEXT             <!AS> /FAO=1
IDXCONCAT        <indexed file cannot be concatenated> /IDENT=CONCAT
RELCONCAT        <relative file cannot be concatenated> /IDENT=CONCAT
HIGHVER          <higher version of !AS already exists> /FAO=1
BADLOGICPC       <internal logic error detected at PC !XL> /IDENT=BADLOGIC /FAO=1
ATPC             <at PC !XL> /FAO=1
BADCOPIES        <'!AS' is an invalid number of copies> /IDENT=BADVALUE /FAO=1
BADFORM          <'!AS' is an invalid form type> /IDENT=BADVALUE /FAO=1
BADJOBID         <'!AS' is an invalid job identifier> /IDENT=BADVALUE /FAO=1
BADJOBNAME       <'!AS' is an invalid job name> /IDENT=BADVALUE /FAO=1
BADPRTY          <'!AS' is an invalid priority> /IDENT=BADVALUE /FAO=1
BADQNAME         <'!AS' is an invalid queue name> /IDENT=BADVALUE /FAO=1
BADTIME          <'!AS' is an invalid time specification> /IDENT=BADVALUE /FAO=1
NOQUEUE          <!AS queue does not exist> /FAO=1
NOJOBID          <job !UW not found in !AS queue> /IDENT=NOJOB /FAO=1
NOJOBNAME        <job !AS not found in !AS queue> /IDENT=NOJOB /FAO=1
SYSERROR         <system service error>
NOTCOPIED        <!AS not copied> /FAO=1
NOTCMPLT         <!AS not completely copied> /FAO=1
RMSERROR         <RMS service error>
UNXPCTSTS        <unexpected status detected. Expected '!AS'. Found:> /FAO=1
HASHCONCAT       <hashed file cannot be concatenated> /IDENT=CONCAT
INCOMPAT         <!AS (input) and !AS (output) have incompatible attributes> /FAO=2
VALERR           <specified value is out of legal range>
FILNOTDEL        <error deleting !AS> /FAO=1
CONFDEL          <!/!AS, delete? (Y or N): > /FAO=1
DELETED          <!AS deleted> /FAO=1
DELVER           <explicit version number or wild card required>
PURGEVER         <version numbers not permitted>
CLOSEDEL         <error closing !AS> /FAO=1
DIRTOOBUS        <obsolete>
NOFILPURG        <no files purged>
FILNOTPUR        <error deleting !AS> /FAO=1
SEARCHFAIL       <error searching for !AS> /FAO=1
DELINTERR        <obsolete>
PARSEFAIL        <error parsing !AS> /FAO=1
FILPURGED        <!AS deleted> /FAO=1
ENDABORT         <!AS compilation aborted> /FAO=1
ENDDIAGS         <!AS completed with !UL diagnostic!%S> /FAO=2
ENDNOOBJ         <!AS completed with !UL diagnostic!%S - object deleted> /FAO=2
HALTED           <transfer has been halted>
NOCMDMEM         <no command memory has been allocated>
QEMPTY           <queue empty>
CBT              <insufficient contiguous space, file copied non-contiguously>
EXISTS           <!AS already exists> /FAO=1
UNLOCKED         <!AS unlocked> /FAO=1
RENAMED          <!AS renamed to !AS> /FAO=2
PROTECTED        <protection on !AS changed to !AS> /FAO=2
NOTLOCKED        <!AS was not locked> /FAO=1
ACTIMAGE         <error activating image !AS> /FAO=1
DIRNOTCRE        <!AS directory file not created> /FAO=1
NODESTQUE        <no such destination queue !AS> /FAO=1
ILLDESQUE        <illegal destination queue name !AS> /FAO=1
NOTTERM          <input device is not a terminal>
CONFQUAL         <conflicting qualifiers>
ILLDIRCOPY       <illegal directory copy of !AS attempted> /FAO=1
INSVIRMEM        <insufficient virtual memory>
CREATEDSTM       <!AS has been created in stream format> /FAO=1
NOTRUNC          <truncation not allowed for this file organization>
PRODNOTINS       <optional software product !AS not installed> /FAO=1
TOTAL            <!UL file!%S deleted (!UL block!%S)> /FAO=2
FILPURG          <!AS deleted (!UL block!%S)> /FAO=2
FILDEL           <!AS deleted (!UL block!%S)> /FAO=2
INVQUAVAL        <value '!AS' invalid for /!AS qualifier> /FAO=2
NOFILES          <no files selected>
FILNOTACC        <file not accessible !AS> /FAO=1
QUALMISS         <qualifier, !AS, missing from command line> /FAO=1
FILSPCSRCH       <file specification !AS contains a search list> /FAO=1
NOSRCHLST        <this command does not support search lists>
NOSUCHID         <unknown rights identifier>
AMBIGANS         <!AS is an ambiguous answer - supply more characters> /FAO=1
INVALANS         <!AS is an invalid answer> /FAO=1
PRDCONT          <Product execution continuing>
NOTDELETED       <error deleting !AS> /FAO=1
NOMEANING        <qualifiers !AS are no longer meaningful for the !AS command> /FAO=2
OPENERR          <error opening !AS> /FAO=1
CLOSEERR         <error closing !AS> /FAO=1
BADFORMAT        <format version mismatch in file !AS> /FAO=1

        .END


        .FACILITY SS,0 /SYSTEM /SHARED

        .SEVERITY SUCCESS
        .BASE 0
NORMAL           <normal successful completion>

        .BASE 192
BUFFEROVF        <output buffer overflow>
CONTROLO         <output completed under CTRL/O>
CONTROLY         <operation completed under CTRL/Y>
CREATED          <file/section did not exist - was created>
MSGNOTFND        <message not in system message file>
NOTRAN           <no string translation performed>
SUPERSEDE        <logical name superseded>
WASECC           <successful transfer; no data check>
DEVALRALLOC      <device already allocated to this job>
REMOTE           <assignment completed on remote node>
CONTROLC         <operation completed under CTRL/C>
NOTMODIFIED      <no pages in the section were modified>
RDDELDATA        <read deleted data address mark>
OVRDSKQUOTA      <disk usage exceeds disk quota>
SYSVERDIF        <system version mismatch - privilege removed - please relink> /IDENT=OBSOLETE_1
FILEPURGED       <oldest file version purged>
NOTALLPRIV       <not all requested privileges authorized>
SYNCH            <synchronous successful completion>
CONCEALED        <logical device name concealed physical device name>
INCOMPAT         <feature incompatible with previous system version>
DBGOPCREQ        <debugger opcode request for PC !XL !+> /FAO=2
ALRDYCLOSED      <connection already closed>
LNMCREATED       <logical name table did not exist - was created>
ACEIDMATCH       <ACE identifier match>
DBGEVENT         <debugger event>
REMOVED          <directory entry removed>
LASTID           <OOB AST has been Queued - but won't be delivered> /IDENT=QUEUED
NOMOREID         <protected subsystems disabled on volume> /IDENT=SUBDISABLED

        .SEVERITY INFO
        .BASE 384
EOTIN            <EOT response to text>
CHAINW           <chained write received>
NOTINSEC         <virtual address not in global section>
NONXPAG          <non-existent page>

        .SEVERITY WARNING
        .BASE 155
VECDIS           <vector disabled fault, code=!XL ,PC=!XL, PSL=!XL> /FAO=3
VARITH           <vector arithmetic fault, summary=!XL, mask=!XL, PC=!XL, PSL=!XL> /FAO=4
ILLVECOP         <illegal vector opcode fault, opcode=!XW, PC=!XL, PSL=!XL> /FAO=3
VECALIGN         <vector alignment exception, reason mask=!XB, virtual address=!XL, PC=!XL, PSL=!XL> /FAO=4

        .BASE 220
FORGET           <participant has no further need for this transaction state>
PREPARED         <only phase one commit processing completed>
NOMOREITEMS      <no more items to be returned>
VOLATILE         <participant has no recoverable transaction state>

        .BASE 256
ACCONFLICT       <file access conflict>
BADCHKSUM        <bad file header checksum>
BADFILEHDR       <bad file header>
BADFILENAME      <bad file name syntax>
BADFILEVER       <bad file version number>
BADIRECTORY      <bad directory file format>
CANCEL           <operation canceled>
DATAOVERUN       <data overrun>
DEVALLOC         <device already allocated to another user>
DEVASSIGN        <device has channels assigned>
DEVICEFULL       <device full - allocation failure>
DEVNOTALLOC      <device not allocated>
DIRFULL          <directory is full>
DUPFILENAME      <duplicate file name>
ENDOFFILE        <end of file>
ENDOFTAPE        <end of tape>
EXTIDXFILE       <***** obsolete - delete when convenient>
FCPREADERR       <file processor read error>
FCPREWNDERR      <file processor rewind error>
FCPSPACERR       <file processor space error>
FCPWRITERR       <file processor write error>
FILELOCKED       <file is deaccess locked>
FILENUMCHK       <file identification number check>
FILESEQCHK       <file identification sequence number check>
FILESTRUCT       <unsupported file structure level>
HEADERFULL       <file header is full>
IDXFILEFULL      <index file is full>
MBFULL           <mailbox is full>
NOHOMEBLK        <Files-11 home block not found on volume>
NONEXPR          <nonexistent process>
NONLOCAL         <device is not a local device>
NOHANDLER        <no condition handler found>
NOSIGNAL         <no signal currently active>
NOSUCHDEV        <no such device available>
NOSUCHFILE       <no such file>
RESIGNAL         <resignal condition to next handler>
UNWIND           <unwind currently in progress>
UNWINDING        <unwind already in progress>
NOMOREFILES      <no more files>
BEGOFFILE        <beginning of file>
BLOCKCNTERR      <block count error>
MUSTCLOSEFL      <must close file>
WAITUSRLBL       <waiting for user labels>
ILLUSRLBLRD      <illegal read of user labels>
ILLUSRLBLWT      <illegal write of user labels>
ILLLBLAST        <illegal user label AST control block address>
ENDOFUSRLBL      <end of user labels>
NOSUCHSEC        <no such (global) section>
CLIFRCEXT        <CLI forced exit>
FCPREPSTN        <file processor reposition error>
TOOMANYVER       <too many higher file versions>
NOTVOLSET        <volume is not part of a volume set>
ENDOFVOLUME      <end of volume>
NOMOREPROC       <no more processes>
NODEVAVL         <no device available>
NOTQUEUED        <request not queued>
DGQINCOMP        <datagram queue operation incomplete>
DIRALLOC         <allocation failure on directory file>
ACLEMPTY         <access control list is empty>
NOENTRY          <access control entry not found>
NOMOREACE        <access control list is exhausted>
RIGHTSFULL       <rights list is full>
VALNOTVALID      <value block is not valid>
ACLFULL          <ACL is full>
NOMORENODE       <no more nodes>
NOMORELOCK       <no more locks>
BEGOFTAPE        <beginning of tape encountered>
OBJLOCKHELD      <object's lock already held>
RLSEMPTY         <CPU !UL startup in progress, cannot stop> /IDENT=CPUSTARTIP /FAO=1
ALLSTARTED       <all configured processors are already started>
ALRDYSTRT        <CPU !UL is already started, or startup in progress> /FAO=1
ALRDYSTPPD       <CPU !UL is already stopped> /FAO=1
NSTPPD           <CPU !UL is not stopped> /FAO=1
UNKRESULT        <transaction result is unknown>
ITEMNOTFOUND     <requested item cannot be returned>
NOMOREDEV        <no more devices>
EFNOTSET         <specified EFN is common, not set>
PRIMNOSTP        <PRIMARY CPU cannot be stopped>
BOOTREJECT       <CPU !UL is in the BOOT_REJECTED state and cannot be started> /FAO=1
RMTPATH          <description of path between two remote nodes>

        .BASE 388
LOGNAME          <transaction log name is !AD> /FAO=1
CPUSTARTD        <CPU !UL started> /FAO=1
CPUSTOPPING      <CPU !UL stopping> /FAO=1
NOTALLCANCELED   <at list one of the requested operation(s) cannot be canceled>
NOTHINGDONE      <all the operations are not queued and not in progress>

        .BASE 459
LOGSTALL         <transaction logging activity is stalled>
LOGFULL          <transaction log file is full>
PWDINDIC         <password found in system dictionary; please choose another string>
PWDINHIS         <password found in history list; please choose a new password>
PWDWEAK          <password is too easy to guess; please choose another string>
USEGENPWD        <password history list is full; please use generated passwords>
INVBUSNAM        <invalid Local Area VAXcluster BUS name>
INVCOMPTYPE      <invalid component type>
INVCOMPID        <invalid component ID>
INVCOMPLIST      <invalid component list>
NOCOMPLSTS       <no component lists are defined>
INVSECDOMAIN     <request originated outside of local security domain>
BADCHECKSUM      <message checksum failure>
ARBTOOBIG        <access rights block too big>
ORBTOOBIG        <object rights block too big>

        .BASE 1129
IMGVEXC          <image exiting with pending vector exceptions>
NOSAVPEXC        <no saved vector exception for the exception-id>
NOSUCHTID        <specified transaction not found>
NOSUCHRM         <specified resource manager not found>
NOCURTID         <no process default transaction currently defined>
WRONGSTATE       <invalid transaction state for requested event>
VETO             <resource manager refuses request to commit>
PWDSYNTAX        <illegal password syntax>
PWDNOTDIF        <new password must be different from current password>
INVPWDLEN        <password length must be between !UB and 32 characters> /FAO=1
SYSVERDIF1       <system version mismatch - please relink> /IDENT=SYSVERDIF
HWM_STALL        <internal I/O stalled by highwater mark activity>
NOSUSPEND        <process cannot be suspended>
NOSUCHPART       <specified participant not found>
RMALRDCL         <resource manager name has already been declared>
ALRCURTID        <a default transaction is currently defined>
INVLOG           <transaction log file format is invalid>
BADLOGVER        <transaction log file format version is unsupported>
OPINPROG         <previously requested operation is incomplete>
WRONGACMODE      <operation attempted from insufficiently privileged access mode>
SUBTRACED        <protected subsystem image cannot be linked /TRACE or /DEBUG>
ARCHIVING        <archive in progress>
ARCHIVED         <file contents archived>
SITEPWDFAIL      <failure to load site-specific password policy>
NOSUCHCPB        <capability does not exist>
CPUCAP           <requested CPU unable to run process due to lack of capabilities>
LOADER           <loader cycle error>
HSTBUFACC        <host buffer access error>
INCONOLCK        <inconsistant ORB lock database>
INVEVENT         <invalid event notification request>
NOSUCHFAC        <event facility object does not exist>
NOCALLPRIV       <third-party lacks privilege for attempted operation>
INSFLPGS         <insufficient Fluid Pages available>
INSFRPGS         <insufficient Free Pages available>
BADREASON        <invalid abort reason code>
NOSUCHBID        <specified transaction branch not found>
NOTORIGIN        <the origin transaction branch is not in this process>
NOLOG            <transaction log not found>
TPDISABLED       <the TP_SERVER process is not running on this node>
BRANCHENDED      <specified transaction branch has already been ended>
BRANCHSTARTED    <specified transaction branch has already been started>
NOSUCHREPORT     <specified transaction event report no found>
BADTIME          <invalid timestamp range>
PARTJOINED       <participant of the same name already joined to this transaction>
NOPRESUME        <not all transaction event reports have been given>
RECOVERCANCEL    <transaction recovery cancelled by explicit request>
CURTIDCHANGE     <already a change to the process default transaction in progress>
NOREADER         <no read channel is assigned to the device>
NOWRITER         <no write channel is assigned to the device>
DEVNOTWS         <device is not a workstation device>

        .SEVERITY ERROR
        .BASE 448
LINEABRT         <host aborted transmission or failed to respond>
DEADLOCK         <deadlock detected>
NOLOCKID         <no lock id. available>
EXDEPTH          <exceeded allowed depth>
PARTMAPPED       <some file map pointers not permanently resident>
CANCELGRANT      <cannot cancel a granted lock>
RETRY            <retry operation>
BADACL           <ACL is corrupt>
ACEEXISTS        <ACE already exists in ACL>
UNSUPPORTED      <unsupported operation or function>
NORIGHTSDB       <rights data base file not found>

        .SEVERITY SEVERE
        .BASE 1
ACCVIO           <access violation, reason mask=!XB, virtual address=!XL, PC=!XL, PSL=!XL> /FAO=4
BADPARAM         <bad parameter value>
EXQUOTA          <exceeded quota>
NOPRIV           <no privilege for attempted operation>
ABORT            <abort>
BADATTRIB        <bad attribute control list>
BADESCAPE        <syntax error in escape sequence>
BADIMGHDR        <bad image header>
CHANINTLK        <channel usage interlocked>
CTRLERR          <fatal controller error>
DATACHECK        <write check error>
DEVFOREIGN       <device is mounted foreign>
DEVMOUNT         <device is already mounted>
DEVNOTMBX        <device is not a mailbox>
DEVNOTMOUNT      <device is not mounted>
DEVOFFLINE       <device is not in configuration or not available>
DRVERR           <fatal drive error>
DUPLNAM          <duplicate name>
FILACCERR        <magnetic tape file access is nonblank>
FILALRACC        <file already accessed on channel>
FILNOTACC        <file not accessed on channel>
FILNOTEXP        <file is not expired>
FORMAT           <invalid media format>
GPTFULL          <global page table is full>
GSDFULL          <global section descriptor table is full>
LCKPAGFUL        <no more pages may be locked in memory>
ILLBLKNUM        <illegal logical block number>
ILLCNTRFUNC      <illegal ACP control function>
ILLEFC           <illegal event flag cluster>
ILLIOFUNC        <illegal I/O function code>
ILLPAGCNT        <illegal page count parameter>
ILLSER           <illegal service call number>
INCVOLLABEL      <incorrect volume label>
INSFARG          <insufficient call arguments>
INSFWSL          <insufficient working set limit>
INSFMEM          <insufficient dynamic memory>
INSFRAME         <insufficient call frames to unwind>
IVADDR           <invalid media address>
IVCHAN           <invalid I/O channel>
IVDEVNAM         <invalid device name>
IVGSDNAM         <invalid global section name>
IVLOGNAM         <invalid logical name>
IVLOGTAB         <invalid logical name table>
IVQUOTAL         <invalid quota list>
IVSECFLG         <invalid process/global section flags>
IVSSRQ           <invalid system service request>
IVSTSFLG         <invalid status flag>
IVTIME           <invalid time>
LENVIO           <address space length violation>
LKWSETFUL        <locked portion of working set is full>
MBTOOSML         <mailbox is too small for request>
MEDOFL           <medium is offline>
NODATA           <mailbox is empty>
NOIOCHAN         <no I/O channel available>
NOLOGNAM         <no logical name match>
NONEXDRV         <nonexistent drive>
NOTFILEDEV       <device is not file structured>
NOTINTBLSZ       <block size is greater than 2048>
NOTLABELMT       <tape is not labeled>
NOTSQDEV         <not a sequential device>
PAGOWNVIO        <page owner violation>
PARITY           <parity error>
PARTESCAPE       <partial escape>
PFMBSY           <page fault monitor is in use>
PSTFULL          <process section table is full>
RESULTOVF        <resultant string overflow>
SECTBLFUL        <section table (process/global) is full>
TAPEPOSLOST      <magnetic tape position lost>
TIMEOUT          <device timeout>
UNASEFC          <unassociated event flag cluster>
UNSAFE           <drive unsafe>
VASFULL          <virtual address space is full>
VECINUSE         <AST vector already enabled>
VOLINV           <volume is not software enabled>
WRITLCK          <write lock error>
NOTAPEOP         <no tape operator>
IVCHNLSEC        <invalid channel for create and map section>
NOMBX            <no associated mailbox for inbound connects>
NOLINKS          <maximum network logical links exceeded>
NOSOLICIT        <interrupt message not solicited>
NOSUCHNODE       <remote node is unknown>
REJECT           <connect to network object rejected>
TOOMUCHDATA      <too much optional/interrupt data specified>
BUGCHECK         <internal consistency failure>
FILNOTCNTG       <file is not contiguous as required>
BADSTACK         <bad stack encountered during exception dispatch>
MCHECK           <detected hardware error, PC=!XL, PSL=!XL> /FAO=2
DEVACTIVE        <device is active>
HANGUP           <data set hang-up>
OPINCOMPL        <operation is incomplete>
ILLSEQOP         <illegal sequential operation>
IVSECIDCTL       <invalid section identification match control>
NOTNETDEV        <not a network communication device>
IVPROTECT        <invalid page protection code>
ACPVAFUL         <MTAACP's virtual address space is full>
MTLBLLONG        <magnetic tape volume label can be no more than six characters>
BUFBYTALI        <device does not support byte-aligned transfers>
NOAQB            <ACP queue header not found>
WRONGACP         <wrong ACP for device>
BUFNOTALIGN      <buffer incorrectly aligned>
DEVCMDERR        <device command error>
DEVREQERR        <device request error>
INSFBUFDP        <unable to allocate a buffered datapath>
INSFMAPREG       <insufficient map registers>
IVBUFLEN         <invalid buffer length>
IVMODE           <invalid mode for requested function>
MCNOTVALID       <device microcode is not valid>
POWERFAIL        <power failure occurred>
SHMGSNOTMAP      <shared memory global section not mapped during creation>
TOOMANYLNAM      <logical name translation exceeded allowed depth>
SHMNOTCNCT       <shared memory not connected>
NOTCREATOR       <request denied, user is not on creator port>
INTERLOCK        <unable to acquire system data structure interlock>
BADQUEUEHDR      <interlocked queue corrupted>
NOSLOT           <no PCB available>
SUSPENDED        <process is suspended>
EXPORTQUOTA      <port's quota exceeded>
NOSHMBLOCK       <no free shared memory block available for creation>
BADQFILE         <invalid disk quota file format>
NOQFILE          <no disk quota file available>
QFACTIVE         <disk quota file is already active>
QFNOTACT         <disk quotas not enabled on this volume>
DUPDSKQUOTA      <duplicate disk quota file entry>
NODISKQUOTA      <no disk quota entry for this UIC>
EXDISKQUOTA      <disk quota exceeded>
IDMISMATCH       <ident mismatch with existing global section>
NOWRT            <cannot create writable section to read-only file>
PLHLDR           <reserved for future use>
ASTFLT           <AST fault, SP=!XL, param=!XL, PC=!XL, PSL=!XL, target PC=!XL, PSL=!XL> /FAO=6
BREAK            <breakpoint fault at PC=!XL, PSL=!XL> /FAO=2
CMODSUPR         <change mode to supervisor trap, code=!XL, PC=!XL, PSL=!XL> /FAO=3
CMODUSER         <change mode to user trap, code=!XL, PC=!XL, PSL=!XL> /FAO=3
COMPAT           <compatibility mode fault (code !UB) at PC=!XL, PSL=!XL> /FAO=3
OPCCUS           <opcode reserved to customer fault at PC=!XL, PSL=!XL> /FAO=2
OPCDEC           <opcode reserved to DIGITAL fault at PC=!XL, PSL=!XL> /FAO=2
PAGRDERR         <page read error, reason mask=!XB, virtual address=!XL, PC=!XL, PSL=!XL> /FAO=4
RADRMOD          <reserved addressing fault at PC=!XL, PSL=!XL> /FAO=2
ROPRAND          <reserved operand fault at PC=!XL, PSL=!XL> /FAO=2
SSFAIL           <system service failure exception, status=!XL, PC=!XL, PSL=!XL> /FAO=3
TBIT             <T-bit pending trap at PC=!XL, PSL=!XL> /FAO=2
DEBUG            <command interpreter debugger signal at PC=!XL, PSL=!XL> /FAO=2
ARTRES           <reserved arithmetic trap at PC=!XL, PSL=!XL> /FAO=2
INTOVF           <arithmetic trap, integer overflow at PC=!XL, PSL=!XL> /FAO=2
INTDIV           <arithmetic trap, integer divide by zero at PC=!XL, PSL=!XL> /FAO=2
FLTOVF           <arithmetic trap, floating overflow at PC=!XL, PSL=!XL> /FAO=2
FLTDIV           <arithmetic trap, floating/decimal divide by zero at PC=!XL, PSL=!XL> /FAO=2
FLTUND           <arithmetic trap, floating underflow at PC=!XL, PSL=!XL> /FAO=2
DECOVF           <arithmetic trap, decimal overflow at PC=!XL, PSL=!XL> /FAO=2
SUBRNG           <arithmetic trap, subscript out of range at PC=!XL, PSL=!XL> /FAO=2
FLTOVF_F         <arithmetic fault, floating overflow at PC=!XL, PSL=!XL> /FAO=2
FLTDIV_F         <arithmetic fault, floating divide by zero at PC=!XL, PSL=!XL> /FAO=2
FLTUND_F         <arithmetic fault, floating underflow at PC=!XL, PSL=!XL> /FAO=2
INHCHMK          <inhibited CHMKernel trap, code=!XL, PC=!XL, PSL=!XL> /FAO=3
INHCHME          <inhibited CHMExecutive trap, code=!XL, PC=!XL, PSL=!XL> /FAO=3

        .BASE 1024
BADISD           <illegal image section descriptor>
RELINK           <obsolete executable image - please relink>
NOTINSTALL       <writable shareable images must be installed>
SHARTOOBIG       <new version of shareable image too big - relink all images>
NOP1VA           <P1 space not supported in shareable images>
MULTRMS          <multiple RMS vectors specified>
VECFULL          <privileged vector limit exceeded>
IVLVEC           <section not installed with privilege>
INSFSPTS         <insufficient SPTEs available>
DISCONNECT       <process is disconnected from requested interrupt vector>
PRIVINSTALL      <shareable images must be installed to run privileged image>
PROTINSTALL      <protected images must be installed>
BADVEC           <invalid change mode or message vector>
REMRSRC          <insufficient system resources at remote node>
PROTOCOL         <network protocol error>
THIRDPARTY       <network logical link disconnected by a third party>
NOSUCHUSER       <login information not recognized at remote node>
SHUT             <remote node no longer accepting connects>
UNREACHABLE      <remote node is not currently reachable>
INVLOGIN         <login information invalid at remote node>
NOSUCHOBJ        <network object is unknown at remote node>
EXCPUTIM         <CPU time limit expired>
OPRABORT         <aborted by operator>
SHRIDMISMAT      <ident mismatch with shareable image>
COMMHARD         <communications hardware error detected>
IVCHAR           <invalid character>
DEVINACT         <device inactive>
CONNECFAIL       <connect to network object timed-out or failed>
LINKABORT        <network partner aborted logical link>
LINKDISCON       <network partner disconnected logical link>
LINKEXIT         <network partner exited>
PATHLOST         <path to network partner node lost>
CLEARED          <virtual call was cleared>
RESET            <circuit was reset>
UNSOLICIT        <response not solicited>
TOOMANYREDS      <too many redirects>
IVLOCKID         <invalid lock id>
SUBLOCKS         <cannot dequeue a lock with sublocks>
PARNOTGRANT      <parent lock must be granted>
CVTUNGRANT       <cannot convert an ungranted lock>
FORCEDERROR      <forced error flagged in last sector read>
ILLSELF          <illegal self reference>
ILLCDTST         <illegal connection descriptor table state>
NOLISTENER       <specified remote system process not listening>
EXGBLPAGFIL      <exceeded global page file limit>
BADRCT           <Replacement and Caching Table unrecoverable I/O error>
DIRNOTEMPTY      <directory file is not empty>
FORCEDEXIT       <forced exit of image or process by Job Controller>
NOTPRINTED       <failed to queue spool file for print>
JBCERROR         <error communicating with job controller>
NOLICENSE        <operation requires software license>
VCBROKEN         <virtual circuit broken>
VCCLOSED         <virtual circuit closed>
INSFCDT          <insufficient CDT entries>
DEVNOTDISM       <device not dismounted>
NOSHRIMG         <privileged shareable image cannot have outbound calls>
DUPUNIT          <MSCP Controller - Duplicate Unit Number>
BADACLCTX        <ACL context has been corrupted>
SERIOUSEXCP      <TMSCP Controller - Serious Exception>
TEMPLATEDEV      <template device cannot be allocated>
IVACL            <invalid access control list entry syntax>
NOSUCHID         <unknown rights identifier>
NOLCLMEDA        <no local program storage media available>
NOREGAVIL        <no region available to execute program>
NOREGSUIT        <no region suited to executing program>
NOSUCHPGM        <no such program found>
PGMLDFAIL        <program load failure>
PGMSTDALN        <program must be run standalone>
IVIDENT          <invalid identifier format>
DUPIDENT         <duplicate identifier>
INCSEGTRA        <incomplete segmented transfer request posted>
NODELEAVE        <node leaving the cluster>
KERNELINV        <kernel mode execution invalid>
EXLNMQUOTA       <logical name table is full>
PARENT_DEL       <illegal attempt to delete parent logical name table>
PARNOTSYS        <parent not system owned lock>
INSSWAPSPACE     <insufficient swap file space available>
VOLOERR          <must override owner identifier field in VOL1 label>
DATALATE         <Mass Storage - Data Late Error>
OVRMAXARG        <maximum parameter count exceeded>
SHACHASTA        <Shadow set has changed state>
TERMNETDEV       <Terminal is a network communications device>
NOLOGTAB         <no logical name table name match>
WRONGNAME        <local node name must match SCSNODE>
NOVOLACC         <user has no access to magnetic tape volume>
NOFILACC         <user has no access to magnetic tape file>
INVEXHLIST       <termination handler control block list is corrupted>
NOACLSUPPORT     <ACLs not supported on selected object>
INVSECLASS       <invalid security classification block>
INCSHAMEM        <incompatible shadow set member>
DEVNOTSHR        <device not currently shareable>
RUCONFLICT       <another facility has active recovery units on file>
DATALOST         <data lost>
REMOTE_PROC      <operation not allowed, process is on remote node>
CPUNOTACT        <requested CPU not active>
SRVMMAT          <cryptographic service mismatch>
EXLICENSE        <licensed product has exceeded current license limits>
INVLICENSE       <licensed product is not authorized for this operating mode>
LICENSE_LEVEL    <license level does not match product version>
INV_HW_ID        <invalid hardware identifier (reserved for future use)>
BADCONTEXT       <invalid or corrupted context encountered>
TOOMUCHCTX       <too many concurrent context streams requested>
VA_IN_USE        <virtual address already in use>
NODELETE         <object cannot be deleted>
NOSUCHCPU        <CPU does not exist, system is not a multiprocessor, or multiprocessing disabled>

        .BASE 1280
NOCMKRNL         <operation requires CMKRNL privilege>
NOCMEXEC         <operation requires CMEXEC privilege>
NOSYSNAM         <operation requires SYSNAM privilege>
NOGRPNAM         <operation requires GRPNAM privilege>
NOALLSPOOL       <operation requires ALLSPOOL privilege>
NODETACH         <operation requires DETACH privilege>
NODIAGNOSE       <operation requires DIAGNOSE privilege>
NOLOG_IO         <operation requires LOG_IO privilege>
NOGROUP          <operation requires GROUP privilege>
NOACNT           <operation requires ACNT privilege>
NOPRMCEB         <operation requires PRMCEB privilege>
NOPRMMBX         <operation requires PRMMBX privilege>
NOPSWAPM         <operation requires PSWAPM privilege>
NOALTPRI         <operation requires ALTPRI privilege>
NOSETPRV         <operation requires SETPRV privilege>
NOTMPMBX         <operation requires TMPMBX privilege>
NOWORLD          <operation requires WORLD privilege>
NOMOUNT          <operation requires MOUNT privilege>
NOOPER           <operation requires OPER privilege>
NOEXQUOTA        <operation requires EXQUOTA privilege>
NONETMBX         <operation requires NETMBX privilege>
NOVOLPRO         <operation requires VOLPRO privilege>
NOPHY_IO         <operation requires PHY_IO privilege>
NOBUGCHK         <operation requires BUGCHK privilege>
NOPRMGBL         <operation requires PRMGBL privilege>
NOSYSGBL         <operation requires SYSGBL privilege>
NOPFNMAP         <operation requires PFNMAP privilege>
NOSHMEM          <operation requires SHMEM privilege>
NOSYSPRV         <operation requires SYSPRV privilege>
NOBYPASS         <operation requires BYPASS privilege>
NOSYSLCK         <operation requires SYSLCK privilege>
UNUSED_1         <unused error message> /IDENT=UNUSED_4
UNUSED_2         <unused error message> /IDENT=UNUSED_5
NODOWNGRADE      <operation requires DOWNGRADE privilege>
NOUPGRADE        <operation requires UPGRADE privilege>
NOGRPPRV         <operation requires GRPPRV privilege>
NOREADALL        <operation requires READALL privilege>
NOSHARE          <operation requires SHARE privilege>
NOSECURITY       <operation requires SECURITY privilege>

        .BASE 1344
EXASTLM          <exceeded AST quota>
EXBIOLM          <exceeded buffered I/O quota>
EXBYTLM          <exceeded byte count quota>
EXDIOLM          <exceeded direct I/O quota>
EXFILLM          <exceeded open file quota>
EXPGFLQUOTA      <exceeded pagefile quota>
EXPRCLM          <exceeded process limit>
EXTQELM          <exceeded timer queue entry quota>
EXENQLM          <exceeded enqueue quota>

        .END


        .FACILITY TYPE,98 /SHARED
        .LITERAL TYPE_OFFSET          = (98)                                                ! 0x00000060 Warning
        .LITERAL TYPE_CONTINUE        = ((TYPE_FACILITY@16) + SHR$_DIRNOTCRE + 3)           ! 0x086212c3 Info
        .LITERAL TYPE_OPENIN          = ((TYPE_FACILITY@16) + SHR$_FILDEL)                  ! 0x08621320 Warning
        .LITERAL TYPE_PARSEFAIL       = ((TYPE_FACILITY@16) + SHR$_NOFILES + 2)             ! 0x08621332 Error
        .LITERAL TYPE_SEARCHFAIL      = ((TYPE_FACILITY@16) + SHR$_QUALMISS + 2)            ! 0x08621342 Error

        .END

