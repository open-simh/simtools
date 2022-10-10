! All facilities defined here must be listed in the message compiler.
!
! This file defines all the ODS2-specific messages.  The message compiler
! (a perl script, not the VMS utility) compiles this into a .h and mdf files for
! ODS2.  Note that none of the facility codes chosen are registered with
! DEC :-).  Also note that these are all customer facilites; they will not
! conflict with VMS facilities of the same name.  Reconstructed messages from
! the VMS facilities are merged with this file when it is compiled.
! The comments in the (auto-generated) ssdef.h explain how to generate
! those sources.  Only necessary if you are translating (I18n)...
!
! Copyright (C) 2016, 2022 Timothe Litt - litt at acm .org
!
! Provided as part of the ODS2 utility, originally written by
!
! Paul Nankervis,
! email address:  Paulnank@au1.ibm.com
!
! ODS2 is distributed freely for all members of the
! VMS community to use. However all derived works
! must maintain comments in their source to acknowledge
! the contributions of the original author and
! subsequent contributors.   This is free software; no
! warranty is offered,  and while we believe it to be useful,
! you use it at your own risk.   This is free software; no
! warranty is offered,  and while we believe it to be useful,
! you use it at your own risk.

! ********************************************************************************
	.FACILITY	ODS2,84
	.LITERAL      ODS2_OFFSET = ODS2_FACILITY - ^X800


	.SEVERITY	WARNING

	.LITERAL      ODS2_NORMAL = (ODS2_OFFSET@16) + SHR_NORMAL + SHR_SUCCESS
	.LITERAL      ODS2_OPENIN = (ODS2_OFFSET@16) + SHR_OPENIN + SHR_WARN

	.SEVERITY SUCCESS
	.BASE 200

CONFIRM_ALL	</CONFIRM ALL response>
CONFIRM_YES	</CONFIRM YES response>
CONFIRM_NO	</CONFIRM NO response> /WARNING
CONFIRM_QUIT    </CONFIRM QUIT response> /WARNING

EXIT            <exiting>


	.SEVERITY INFO
	.BASE 400

MSGFILE         <Message definition file: !AZ> /FAO=1
INVDEF          <!AZ does not exist> /FAO=1
HELPFILE        <Help library file: !AZ> /FAO=1
HELPFILEVER1    <Version !UL !UL-bit created !AZ> /FAO=3 /IDENT=HELPFILEVER
HELPFILEVER2    <Version !UL !UL/!UL-bit created !AZ> /FAO=4 /IDENT=HELPFILEVER

	.SEVERITY WARNING
	.BASE 600

FILHDRACC	<unable to read file header>
SETCWD          <failed to set cwd>
LOCALE          <unable to set locale>

	.SEVERITY ERROR
	.BASE 800

AMBIGUOUS       <!AZ '!AZ' is ambiguous> /FAO=2
UNKNOWN         <unrecognized !AZ '!AZ'> /FAO=2
NOVALUE         <no value allowed for !AZ !AZ> /FAO=2
VALUEREQ        <value required for !AZ !AZ> /FAO=2
NOPAREN         <value is missng ')' for !AZ '!AZ'> /FAO=2
TOOMANY         <too many !AZ specified> /FAO=1
TOOFEW          <too few !AZ specified> /FAO=1

MAXIND          <maximum indirect command file nesting limit (!UL) exceeded> /FAO=1
UNTERMSTR       <unterminated quoted string>
INVNUMBER       <!AZ is not a valid number> /FAO=1
NOQUALS         <no qualifiers specified>
NOQUALSOK       <no qualifiers permitted> /IDENT=NOQUALS
NOPAROK         <no parameters permitted>
INCONQUAL       <inconsistent use of qualifiers>
BADPRO          <invalid syntax for file or directory protection>
BADUIC          <invalid syntax for UIC. [group,member] must be numeric>
BADVALUE        <invalid value>

OPENIND         <failed to open indirect command file !AZ> /FAO=1
NOSHELL         <failed to run !AZ> /FAO=1
OSERROR         <the last host OS error was: !AZ> /FAO=1
OSERRORNO       <the last host OS error was !UL.> /FAO=1 /IDENT=OSERROR

VMSTIME         <failed to convert VMS time quadword>

FCS2DV          <rename operation failed--different devices specified>
BADORG          <unsupported file organization>
ILLDIRWRT       <directories may not be written as files>

FILESTRUCT      <unsupported file structure level !UL.!UL> /FAO=2

	.SEVERITY SEVERE
	.BASE 1000

INITIALFAIL     <initialization failed>

BADMSGFILE      <invalid message file format>
NOMSG           <message number !8XL> /FAO_COUNT=1

        .END

! ********************************************************************************
	.FACILITY	COPY,85
	.LITERAL        COPY_OFFSET = COPY_FACILITY - ^X800

	.SEVERITY SUCCESS
	.BASE 200

	.SEVERITY INFO
	.BASE 400

        .LITERAL  COPY_COPIEDB = (COPY_OFFSET@16) + SHR_COPIEDB + SHR_INFO
        .LITERAL  COPY_COPIEDR = (COPY_OFFSET@16) + SHR_COPIEDR + SHR_INFO

COPIEDU          <!AZ copied to !AZ (!UL byte!%S)> /IDENT=COPIED /FAO=3

CONFIRM 	<copy !AZ to !AZ? > /FAO=2
COPIED          <!UL file(s) copied> /FAO=1
NOFILES         <no files found>
DIRNOTCOPIED    <!AZ is a directory, not copied> /FAO=1

	.SEVERITY WARNING
	.BASE 600

	.LITERAL  COPY_OPENIN = (COPY_OFFSET@16) + SHR_OPENIN + SHR_WARN

PROTERR         <failed to set protection for !AZ> /FAO=1
NOMATCH         <no local files match !AZ> /FAO=1

	.SEVERITY ERROR
	.BASE 800

	.LITERAL  COPY_CLOSEOUT = (COPY_OFFSET@16) + SHR_CLOSEOUT + SHR_ERROR
	.LITERAL  COPY_OPENIN = (COPY_OFFSET@16) + SHR_OPENIN + SHR_ERROR
	.LITERAL  COPY_OPENOUT = (COPY_OFFSET@16) + SHR_OPENOUT + SHR_ERROR
	.LITERAL  COPY_PARSEFAIL = (COPY_OFFSET@16) + SHR_PARSEFAIL + SHR_ERROR
	.LITERAL  COPY_SEARCHFAIL = (COPY_OFFSET@16) + SHR_SEARCHFAIL + SHR_ERROR
        .LITERAL  COPY_READERR = (COPY_OFFSET@16) + SHR_READERR + SHR_ERROR
        .LITERAL  COPY_WRITEERR = (COPY_OFFSET@16) + SHR_WRITEERR + SHR_ERROR
        .LITERAL        COPY_INVALIDNL = (COPY_OFFSET@16) + SHR_INVALIDNL + SHR_ERROR
        .LITERAL        COPY_INVALIDVFU = (COPY_OFFSET@16) + SHR_INVALIDVFU + SHR_ERROR
        .LITERAL        COPY_INVALIDC0 = (COPY_OFFSET@16) + SHR_INVALIDC0 + SHR_ERROR

NONAME            <no file name specified for output>

	.SEVERITY SEVERE
	.BASE 1000

NOMEM         <out of memory expanding !AZ> /GAO=1
GLOBABT         <read error expanding !AZ> /FAO=1
GLOBERR         <unknown error !UL processing !AZ> /FAO=2

        .END

! ********************************************************************************
	.FACILITY	CREATE,86
	.LITERAL        CREATE_OFFSET = CREATE_FACILITY - ^X800

	.SEVERITY INFO
	.BASE 400
        .LITERAL        CREATE_EXISTS = (CREATE_OFFSET@16) + SHR_EXISTS + SHR_INFO
        .LITERAL        CREATE_CREATED = (CREATE_OFFSET@16) + SHR_CREATED + SHR_INFO

READY   <!AZ is open for output. Type lines of text ending with Enter> /FAO=1
TERMEOF <terminate input with !AZ> /FAO=1

        .SEVERITY       ERROR
        .BASE 800
CREDIRFAIL      <failed to create directory !AZ> /FAO=1
        .LITERAL        CREATE_NOTDIR = (CREATE_OFFSET@16) + SHR_NOTDIR + SHR_ERROR
        .LITERAL        CREATE_CLOSEOUT = (CREATE_OFFSET@16) + SHR_CLOSEOUT + SHR_ERROR
        .LITERAL        CREATE_INVALIDNL = (CREATE_OFFSET@16) + SHR_INVALIDNL + SHR_ERROR
        .LITERAL        CREATE_INVALIDVFU = (CREATE_OFFSET@16) + SHR_INVALIDVFU + SHR_ERROR
        .LITERAL        CREATE_INVALIDC0 = (CREATE_OFFSET@16) + SHR_INVALIDC0 + SHR_ERROR

        .END

! ********************************************************************************
	.FACILITY	DELETE,87
	.LITERAL      DELETE_OFFSET = DELETE_FACILITY - ^X800

	.SEVERITY SUCCESS
	.BASE 200

	.SEVERITY INFO
	.BASE 400

CONFIRM 	<delete !AZ? > /FAO:1
DELETED         <deleted !AZ> /FAO=1
NFILES          <!UL files deleted> /FAO=1 /IDENT=DELETED

	.SEVERITY WARNING
	.BASE 600

        .LITERAL        DELETE_OPENIN = (DELETE_OFFSET@16) + SHR_OPENIN + SHR_WARN

NOFILES         <no files deleted>
NOTDELETED      <error deleting !AZ> /FAO=1
DIRNOTEMPTY     <directory is not empty>

	.SEVERITY ERROR
	.BASE 800


	.SEVERITY SEVERE
	.BASE 1000

        .END

! ********************************************************************************
	.FACILITY       DIFF,88
	.LITERAL      DIFF_OFFSET = DIFF_FACILITY - ^X800

	.SEVERITY       SUCCESS
	.BASE 200

NODIFF          <no differences encountered>

	.SEVERITY       INFO
	.BASE 400

COMPARED        <compared !UL record(s)> /FAO=1

	.SEVERITY       WARNING
	.BASE 600

	.LITERAL DIFF_OPENIN = (DIFF_OFFSET@16) + SHR_OPENIN + SHR_ERROR

DIFFER       <files differ at record !UL>

        .END

! ********************************************************************************
	.FACILITY	DIRECT,89
	.LITERAL      DIRECT_OFFSET = DIRECT_FACILITY - ^X800

	.SEVERITY	WARNING

	.SEVERITY SUCCESS
	.BASE 200

	.SEVERITY INFO
	.BASE 400
DIRHEAD       <!/Directory !AZ!/!/> /FAO=1
FULLSIZE      <!/Size: !9UL> /FAO=1
GRANDTOT      <!/Grand total of !UL directory!%S, !UL file!%S> /FAO=2
FILETOTAL     <!/Total of !UL file!%S> /FAO=1
USEDTOT       <!UL block!%S> /FAO=1
ALLOCTOT      <!UL block!%S> /FAO=1
BOTHSIZE      <!UL/!UL block!%S> /FAO=2
OWNER         <owner:    [!OW,!OW]> /FAO=2
CREATED       <created:  >
REVISED       <revised:  >
EXPIRES       <expires:  >
BACKUP        <backup:   >
FILEORG       <file organization:  >
SEQORG        <sequential>
RELORG        <relative>
IDXORG        <indexed>
UNKORG        <unknown (!UL)> /FAO=1
FILEATT       <!/File attributes:    >
ALLOC         <allocation: !UL> /FAO=1
EXTEND        <, Extend: !UW> /FAO=1
GBC           <, Global buffer count: !UW> /FAO=1
VERLIMIT      <, Version limit: !UW> /FAO=1
DIRLIMIT      <, Default version limit: !UW> /FAO=1
NOVERLIMIT    <, No version limit>
NODIRLIMIT    <, No default version limit>

CONTIG	      <, Contiguous>
CONTIGB	      <, Contiguous best try>
NOBACKUP      <, Backups disabled>
DIRECTORY     <, Directory file>
WRITEBACK     <, Writeback>
READCHECK     <, Read verification>
WRITECHECK    <, Write verification>
LOCKED	      <, File locked>
SPOOL	      <, Spool file>
MARKDEL	      <, Marked for deletion>
BADBLOCK      <, Contains bad block>

EOFPOS        <end of file:        Block !UL, byte !UW!/> /FAO=2

RECFMT        <record format:      >
RECUDF        <undefined>
RECFIX        <fixed length !UW byte records> /FAO=1
RECVAR        <variable length, maximum !UW bytes> /FAO=1
RECVFC        <VFC, !UB byte header, maximum !UW bytes> /FAO=2
RECSTM        <stream>
RECSTMLF      <stream-LF>
RECSTMCR      <stream-CR>

RECATT        <Record attributes:  >
RECATT_NONE    <none>
RECATT_FTN     <!AZFortran carriage control> /FAO=1
RECATT_CR      <!AZCarriage return carriage control> /FAO=1
RECATT_PRN     <!AZPrint file carriage control> /FAO=1
RECATT_BLK     <!AZNon-spanned> /FAO=1

PROT_SYS      <File protection:    System:>
PROT_OWNER    <, Owner:>
PROT_GROUP    <, Group:>
PROT_WORLD    <, World:>

UIC           <  [!OW,!OW]> /FAO=2

IDNAME        <!/Header filename:       !AZ> /FAO=1
EXTNHDR       <extension header:      !UW, Extension FID (!UL,!UW,!UB)> /FAO=4
HDROFFSETS    <ID area offset (words):!UB!/Map area offset:       !UB!/ACL area offset:       !UB!/Reserved area offset:  !UB> /FAO=4
STRUCLEVEL    <structure level:       !UB.!UB> /FAO=2
MAPSIZE       <map area size:         !UL, in use: !UL!/Cluster size:          !UL!/Highest VBN allocated: !UL> /FAO=4
MAPHIGHWATER  <highest VBN written:   !UL> /FAO=1
MAPHEADER     <!/Retrieval pointers:!/      VBN F     Count Start LBN   End LBN!/--------- - --------- --------- --------->
MAPFMT0       <format 0: !XW> /FAO=1
MAPENTRY      <!9UL !UW !9UL !9UL !9UL> /FAO=5
MAPFREE       "!9UL <= First free" /FAO=1

	.SEVERITY WARNING
	.BASE 600

        .LITERAL        DIRECT_NOFILES = (DIRECT_OFFSET@16) + SHR_NOFILES + SHR_WARN

	.SEVERITY ERROR
	.BASE 800

	.LITERAL  DIRECT_OPENOUT = (DIRECT_OFFSET@16) + SHR_OPENOUT + SHR_ERROR

	.SEVERITY SEVERE
	.BASE 1000

        .END


! ********************************************************************************
	.FACILITY	DISM,90
	.LITERAL	DISM_OFFSET = DISM_FACILITY - ^X800

	.SEVERITY INFO
	.BASE 400

DISMOUNTD       <volume !12AD of volume set !12AD dismounted> /FAO=4 /IDENT=DISMOUNTED
DISMOUNTDV      <volume !12AD dismounted> /FAO=2 /IDENT=DISMOUNTED

DISMAL          <all members of !12AD have been dismounted>
	.SEVERITY WARNING
	.BASE 600

CANNOTDMT       <!12AD cannot be dismounted> /FAO=2

	.SEVERITY ERROR
	.BASE 800

	.SEVERITY SEVERE
	.BASE 1000

DEVNOTDISM      <device not dismounted>
USERFILES       <!SL user file(s) open on volume> /FAO=1

        .END

! ********************************************************************************
	.FACILITY	EXTEND,91
	.LITERAL      EXTEND_OFFSET = EXTEND_FACILITY - ^X800

	.SEVERITY SUCCESS
	.BASE 200

	.SEVERITY INFO
	.BASE 400

NEWSIZE <new allocated size is !UL block!%S> /FAO=1

	.SEVERITY ERROR
	.BASE 800

CONFLQUAL     <can not specify both !AZallocate and !AZtruncate> /FAO=2

        .END

! ********************************************************************************
	.FACILITY	HELP,92
	.LITERAL	HELP_OFFSET = HELP_FACILITY - ^X800

	.SEVERITY INFO
	.BASE 400

TERMWID     <A minimum terminal window width of !UL is recommended for correct formatting of HELP> /FAO=1
TERMSIZE    <Terminal width is !UL, height is !UL> /FAO:2
KEYWORD     < keyword, one of the following:>
KEYWORDS    < keywords, one or more of the following:>
QUALIFIER    < qualifier, one of the following>
QUALIFIERS   < qualifiers, one or more of the following>
LIST        <list, !AZ> /FAO=1
LISTS       <lists, !AZ> /FAO=1
HEAD        < Commands are:>
MORE        < Additional information:>
EXPLAIN     < Type HELP COMMAND or HELP TOPIC for information on any command or topic.!/ Type HELP HELP for information on the help system.>
NOCMDHELP   <!AZ: No help available> /FAO=1
NOPARHELP   <no parameter '!AZ' found> /FAO=1
PARNOTACT   <parameter is not used for this command>
NOPARAMS    <!AZ has no parameters> /FAO=1
PARHEADING  <!/  Parameters:!/    >
PARNOLIMIT  <  "*" indicates that a parameter can be used more than once.>
PAREXPLAIN  <  Type help !AZ PARAMETER for more about each parameter.> /FAO=1
QUALHEAD    <qualifiers>

SETVALUE    <type 'help set value ITEM' for item details>

SHOWCWD     <  Current working directory is !AZ> /FAO=1
SHOWDEFQ    <  Default qualifiers for !AZ command> /FAO=1
SHOWNODEFQ  <      None set>
SHOWQSTYLE  <  Qualifier style: !AZ> /FAO=1
SHOWSTATS   <Statistics:>
SHOWVERON   <Command file verification is on>
SHOWVEROFF  <Command file verification is off>
SHOWVERS    < !AZ !AZ built !AZ !AZ> /FAO=4
SHOWVERSRL  < !AZ !AZ built !AZ !AZ with libedit version !UL.!UL> /FAO=6
SHOWVERSCSI < Direct SCSI access is supported>
SHOWVERNOVHD < VHD format image file support is not configured>
SHOWVERPLATFM < Platform: !AZ> /FAO=1
SHOWVERCOMPLR < Compiler: !AZ, version !AZ> /FAO=2
SHOWVLD      < Visual Leak Detector is configured>
SHOWCMDHISTLN <!5UL: !AZ> /FAO=2
SHOWMSGPRINTED <(printed): >

	.SEVERITY WARNING
	.BASE 600

	.SEVERITY ERROR
	.BASE 800

OPENHLP         <failed to open help file !AZ> /FAO=1
NOHELP          <can't locate help topic "!AZ" in !AZ> /FAO=2
HELPFMT         <help file !AZ has invalid format or is empty> /FAO=1 /IDENT=NOHELP
HELPLEVEL       <help level error at line !UL> /FAO=1
NOTEXT          <no text for topic !AZ at line !UL> /FAO=2
NOTEXT2         <no text for topic !AZ> /FAO=1 /IDENT=HELPNOTEXT
SHOWNOCMDHIST   <No command history exists>
SETCMDHISTVAL   <Command history limit must be greater than zero>

        .END
! ********************************************************************************
	.FACILITY	INIT,93
	.LITERAL      INIT_OFFSET = INIT_FACILITY - ^X800

	.SEVERITY SUCCESS
	.BASE 200

	.SEVERITY INFO
	.BASE 400

CONFIRM_PHY	<this will delete all data on the physical device !AZ. Proceed? > /FAO:1 /IDENT=CONFIRM
CONFIRM_VIRT  <this will delete all data in !AZ and initialize a !AZ image file. Proceed? > /FAO:2 /IDENT=CONFIRM

START_PHY        <initializing !AZ on device !AZ as !AZ> /FAO=3 /IDENT=STARTING
START_VIRT       <initializing !AZ on disk image !AZ as !AZ> /FAO=3 /IDENT=STARTNG

VOLINIT         <!AZ initialized successfully> /FAO=1
SIZING1         <volume size !UL blocks, cluster size !UL, maximum files !UL> /IDENT=SIZING /FAO=3
SIZING2         <preallocated !UL file headers !UL directory entries> /IDENT=SIZING /FAO=2
PLACED1          <boot block at LBN !UL, HOM blocks at LBNs !UL and !UL> /IDENT=SIZING /FAO=3
PLACED2          < MFD at LBN !UL, index file at LBNs !UL thru !UL> /IDENT=SIZING /FAO=3

MFGBAD           <writing manufacturer's bad block index>
SWBAD            <writing software bad block descriptor>

	.SEVERITY WARNING
	.BASE 600

INVOPT        <!AZcreate and !AZreplace only apply to disk image files> /FAO=2

	.SEVERITY ERROR
	.BASE 800

BADVOLLABEL   <volume label must be 1-12 alphanumeric charactes>
BADUSERNAME   <username must be 1-12 alphanumeric characters>
BADMEDIUM     <unknown medium '!AZ'; HELP INITIALIZE for list> /FAO=1

	.SEVERITY SEVERE
	.BASE 1000

TOOSMALL        <device is too small to hold a useful Files-11 volume>
NOHOMLOC        <no good block found for primary HOM block>
NOHOM2LOC       <no good block found for secondary HOM block> /IDENT=NOHOMLOC

        .END

! ********************************************************************************
	.FACILITY	IO,94
	.LITERAL	IO_OFFSET = IO_FACILITY - ^X800

	.SEVERITY INFO
	.BASE 400

STATS         <initializations: !UL Reads: !UL Writes: !UL> /FAO=3
LARGEFILE     "large files and devices (>2GB) are supported"
NOLARGEFILE   "large files and devices (>2GB) are NOT supported"
UNXDEVHDR     < Physical devices>
NODEVS        <no devices found>

NTDEVHEADER   < Physical devices!/  Drv   Type    Physical Name!/  --- --------- ------------->
NTDEVREMOVABLE <   !AZ Removable !AZ> /FAO=2
NTDEVFIXED     <   !AZ Fixed     !AZ> /FAO=2
NTDEVREMOTE    <   !AZ Network>       /FAO=1
NTDEVCDROM     <   !AZ CDROM     !AZ> /FAO=2
NTDEVRAMDISK   <   !AZ RAMdisk   !AZ> /FAO=2
NTDEVOTHER     <   !AZ Other>         /FAO=1

VNODEVS        < No virtual devices are assigned>
VDEVHEADER     < Virtual devices>

VHDAVAIL       <VHD format image files are supported>
VHDNOTAVAIL    <VHD format image file support is configured, but not available>
VHDFMTSTART    <formatting !AZ> /FAO=1
VHDFMTEND      <VHD volume formatting completed>
VMSMOUNT       <device !AZ mounted /FOREIGN> /FAO=1
VMSDMT         <device !AZ dismounted> /FAO=1
	.SEVERITY WARNING
	.BASE 600

VHDSAVED      <!AZ contains a suspended system and should not be modified> /FAO=1

	.SEVERITY ERROR
	.BASE 800

NOVHD         <VHD disk support is not available in this version>
ISVIRTDEV     <!AD is a virtual device> /FAO=2
VIRTDEVINUSE  <!AZ is in use by !AZ> /FAO=2
VIRTFILEINUSE <!AZ is in use on virtual drive !AZ> /FAO=2
BADPATH       <invalid path: !AZ> /FAO=1
DIRRDERR      <error reading directory !AZ> /FAO=1
LOCKVOL       <error locking volume on !AZ> /FAO=1
NOGEO         <unable to obtain geometry for !AZ> /FAO=1
OPENDEV       <error opening !AZ> /FAO=1
READERR       <read error at sector !UL> /FAO=1
READLEN       <read error at lbn !UL, got !UL, expected !UL> /FAO=3
WRITELEN      <read error at lbn !UL, got !UL, expected !UL> /FAO=3
SEEKERR       <seek failure at sector !UL> /FAO=1
WRITEERR      <write error at sector !UL> /FAO=1
UNXNOTREG     <!AZ is not a regular file> /FAO=1
UNXNOTBLK     <!AZ is not a block device> /FAO=1
EXISTS        <file !AZ already exists, not superseded> /FAO=1
RDBLK2BIG     <read from non-existent LBN !UL> /FAO=1 /IDENT=BLK2BIG
WRBLK2BIG     <write to non-existent LBN !UL> /FAO=1 /IDENT=BLK2BIG
VHDONLY       <!AZ Snapshots can only be taken of VHD format image files> /FAO=1
VHDSNAPFAIL   <snapshot of !AZ failed> /FAO=1
VHDMEM        <failed to allocate memory for VHD>
VHDCREATEERR  <error creating VHD volume !AZ> /FAO=1
VMSDEVSCAN    <error scanning devices>
VMSNOMOUNT    <unable to mount device !AZ /FOREIGN> /FAO=1
VMSNODMT      <unable to dismount device !AZ> /FAO=1

	.SEVERITY SEVERE
	.BASE 1000

OFFSET_TOO_BIG	<Volume is too large for local file system: needs 64-bit I/O>

VHDNOLIB      <VHD library !AZ could not be loaded> /FAO=1
VHDNOSYM      <symbol !AZ not present in !AZ> /FAO=2
VHDNOTVALID   <the VHD library !AZ is not usable> /FAO=1

        .END

! ********************************************************************************
	.FACILITY	MOUNT,95
	.LITERAL      MOUNT_OFFSET = MOUNT_FACILITY - ^X800

	.SEVERITY	WARNING

	.SEVERITY SUCCESS
	.BASE 200

	.SEVERITY INFO
	.BASE 400
MOUNTED         <!12AD mounted on !AZ> /FAO=3
SNAPOK          <!AZ created from !AZ> /FAO=2
VSMOUNTED       <volume set !12AD mounted> /FAO=1
WRITELCK        <!AZ can not be written, mounted read-only> /FAO=1

	.SEVERITY WARNING
	.BASE 600

BADSCB          <invalid storage control block on !AZ> /FAO=1

	.SEVERITY ERROR
	.BASE 800

BADLABEL        <label !AZ is too long> /FAO=1
FAILED          <mount failed>
HOMBLKCHK       <!UL home block software consistency error!%S> /FAO=1
BITMAPSYS       <unable to access BITMAP.SYS on !AZ> /FAO=1
BITUPDFAIL      <unable to compute disk usage for !AZ> /FAO=1
INCONVOL        <volume '!12AD' on !AZ is a member of '!12AD', not a member of '!12AD'> /FAO=7
NOTSAME         <you must specify the same number of existing files (!UL) with !AZSNAPSHOT as filenames to create (!UL)> /FAO=3
RVN1NOTMNT      <volume 1 of the volume set must be mounted>
VOLOOO          <RVN !UL of '!12AD' is '!12AD'.  !AZ contains '!AD'> /FAO=8
VOLSETSYS       <unable to access VOLSET.SYS on !AZ> /FAO=1
WRONGRVN       <device !AZ contains !12AD, which is RVN !UW of !12AD. !UL of !12AD expected> /FAO=9 /IDENT=WRONGVOL
NOTVSMEM        <device !AZ contains !12AD, which is not part of a volume set.  RVN !UL of !12AD expected> /FAO=6
WRONGVOL        <device !AZ contains volume '!12AD', '!12AD' expected> /FAO=5
WRONGVOLCNT     <volume set !12AD has !UL members, but !UL specified> /FAO=3
WRONGVOLMEM     <volume set name on !AZ is '!12AD', but VOLSET.SYS is for '!12AD'> /FAO=5

	.SEVERITY SEVERE
	.BASE 1000

        .END

! ********************************************************************************
	.FACILITY	RENAME,96
	.LITERAL        RENAME_OFFSET = RENAME_FACILITY - ^X800

	.SEVERITY SUCCESS
	.BASE 200

	.SEVERITY INFO
	.BASE 400


RENAMED       <!AZ renamed to !AZ> /FAO=2

CONFIRM 	<rename !AZ to !AZ? > /FAO=2
NRENAMED        <!UL file(s) renamed> /FAO=1 /IDENT=RENAMED
NOFILES         <no files renamed>

	.SEVERITY WARNING
	.BASE 600

	.SEVERITY ERROR
	.BASE 800

FAILED  <rename !AZ to !AZ failed> /FAO=2

	.LITERAL  RENAME_PARSEFAIL = (RENAME_OFFSET@16) + SHR_PARSEFAIL + SHR_ERROR
	.LITERAL  RENAME_SEARCHFAIL = (RENAME_OFFSET@16) + SHR_SEARCHFAIL + SHR_ERROR

	.SEVERITY SEVERE
	.BASE 1000

        .END

! ********************************************************************************
	.FACILITY       SEARCH,97
	.LITERAL      SEARCH_OFFSET = SEARCH_FACILITY - ^X800

	.SEVERITY INFO
	.BASE 400

CONFIRM <search !AZ? > /FAO=1
NOMATCH <no strings matched>
MATCHED <!AZ searched, !UL record!%S, !UL match!%S> /FAO=3
FILE    <!AZ searched, !UL record!%S, no matches> /FAO=2

	.SEVERITY       WARNING
	.BASE 600

	.LITERAL  SEARCH_OPENIN = (SEARCH_OFFSET@16) + SHR_OPENIN + SHR_WARN
        .LITERAL  SEARCH_NOFILES = (SEARCH_OFFSET@16) + SHR_NOFILES + SHR_WARN
        .LITERAL  SEARCH_CONTINUE = (SEARCH_OFFSET@16) + SHR_CONTINUE + SHR_INFO

	.SEVERITY ERROR
	.BASE 800

        .LITERAL        SEARCH_PARSEFAIL = (SEARCH_OFFSET@16) + SHR_PARSEFAIL + SHR_ERROR
        .LITERAL        SEARCH_SEARCHFAIL = (SEARCH_OFFSET@16) + SHR_SEARCHFAIL + SHR_ERROR

BADSTRING       <search string !UL too short> /FAO=1


        .END

! ********************************************************************************
	.FACILITY       TYPE,98
	.LITERAL      TYPE_OFFSET = TYPE_FACILITY - ^X800

	.SEVERITY INFO
	.BASE 400

        .LITERAL        TYPE_CONTINUE = (TYPE_OFFSET@16) + SHR_CONTINUE + SHR_INFO

	.SEVERITY       WARNING
	.BASE 600

	.LITERAL  TYPE_OPENIN = (TYPE_OFFSET@16) + SHR_OPENIN + SHR_WARN

	.SEVERITY ERROR
	.BASE 800

        .LITERAL        TYPE_PARSEFAIL = (TYPE_OFFSET@16) + SHR_PARSEFAIL + SHR_ERROR
        .LITERAL        TYPE_SEARCHFAIL = (TYPE_OFFSET@16) + SHR_SEARCHFAIL + SHR_ERROR


        .END

! ********************************************************************************
	.FACILITY	SHR,0 /SHARED

        .LITERAL      SHR_WARN = 0
        .LITERAL      SHR_SUCCESS = 1
        .LITERAL      SHR_ERROR = 2
        .LITERAL      SHR_INFO = 3
        .LITERAL      SHR_SEVERE = 4

        ! Convention shared with DEC:
        ! All shared messaged defined as WARNING so that user can
        ! add one of the severity codes above

	.SEVERITY WARNING
        .BASE     0
NORMAL          <normal successful completion>

	.BASE 600

CONTINUE        " Type <CR> to continue, f<CR> to finish, q<cr> to abort "

CLOSEIN          <error closing !AZ as input> /FAO=1
CLOSEOUT         <error closing !AZ as output> /FAO=1
COPIEDB          <!AZ copied to !AZ (!UL block!%S)> /IDENT=COPIED /FAO=3
COPIEDR          <!AZ copied to !AZ (!UL record!%S)> /IDENT=COPIED /FAO=3
CREATED          <!AZ created> /FAO=1
EXISTS           <!AZ already exists> /FAO=1

INVALIDNL <VFC newline count must be between 1 and 128>
INVALIDVFU <VFU channel must be between 1 an 16>
INVALIDC0  <VFC ASCII control must be between 0 and 31>

NOFILES         <no files found>
NOTDIR          <!AZ is not a directory> /FAO=1

OPENIN          <error opening !AZ as input> /FAO=1
OPENOUT         <error opening !AZ for output> /FAO=1

PARSEFAIL        <error parsing !AZ> /FAO=1
READERR          <error reading !AZ> /FAO=1
SEARCHFAIL       <error searching for !AZ> /FAO=1

WRITEERR         <error writing !AZ> /FAO=1

        .END
