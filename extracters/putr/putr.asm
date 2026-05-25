	title	Peripheral Utility Transfer Routines
;++
;
; File transfer program, knows many DEC formats.
;
; By John Wilson <wilson@dbit.com>.
;
; Copyright (C) 1995-2001 By John Wilson.  All rights reserved.
;
; This program may be freely distributed provided that no charge is made for
; distribution and that all versions include this copyright notice and give
; proper credit to the original author (including first billing in the message
; displayed when the program is started).  OK so I'm vain, but I put a *lot* of
; work into this program, and I don't want anyone else taking credit for it.
;
; This program still has a few loose ends.  The Files-11 and TSS/8 support is
; read-only, and the Catweasel driver needs to be finished and debugged
; (possibly after the hardware is debugged?!).  Also it would be nice to
; support SCSI tapes, if I can figure out how to fit them into the command
; language.  The sequence of operations involved in locating a directory and
; opening and accessing files there is really too complicated.
;
; A basic assumption of this program is that there are never more than one
; input and one output file open at any given time, since the user can only
; type one COPY command (and wildcards are handled iteratively like on any
; normal system).  So we can use static storage for variables related to open
; files, and we don't have to worry about multiple output files competing for
; contiguous disk space or directory slots or anything like that.  Converting
; this code into live filesystem drivers would be a big project though.
;
; TO DO LIST:
;
; Should handle bad block replacement with RT-11 DL: and DM: volumes.  Doesn't
; matter though, if the image file was made using the RT-11 drivers, since they
; will have untangled the block replacements.  Actually in that case doing our
; own relocation would screw it up so maybe it's better to leave this alone.
;
; RENAME/DATE:xxx ???
;
; Several different cleanup concepts need to be sorted out:
;
; SQUEEZE should move empty blocks to end under RT-11 and OS/8.
;
; CLEAN should update SATT.SYS under RSTS, or BITMAP.SYS under Files-11, or
; bitmap under DOS/BATCH, and check for blocks that are allocated to >1 file.
;
; Something should combine contiguous empty blocks under RT-11 (SQUEEZE/DIR ?).
;
; Defragmenters for all the supported file systems would be wonderful, but
; they'd be a whole lot of work.  It might be better to do that in PDP-11
; (etc.) code so that it would be useful on real machines too.
;
; 09/18/1990	JMBW	Created (OS/278, COS-310 read-only version).
; 03/22/1994	JMBW	Bypass BIOS to emulate RX50, RX02/03 (sort of) and RX01
;			(maybe -- it seems most PC FDC's blow off SD support).
; 03/23/1994	JMBW	Concept of "logged-in" device.
; 03/30/1994	JMBW	TYPE command works (DOS, OS/8, RT-11).
; 04/02/1994	JMBW	TU58/COM port driver added.
; 07/09/1994	JMBW	COPY command can write into RT-11 file system.
; 07/23/1994	JMBW	LD: disk files work like subdirectories.
; 07/30/1994	JMBW	Reads RSTS disks (RDS 0.0-1.2).
; 11/01/1994	JMBW	FORMAT creates image files, WIPEOUT added for RT-11.
; 01/10/1995	JMBW	V1.0 released.
; 01/29/1995	JMBW	Cached dir code for OS/8 (adapted from RT-11 code).
; 05/18/1995	JMBW	Writes and inits OS/8 disks.  V1.1 released.
; 05/30/1995	JMBW	Exclude tracks 0, 78, 79 when initting OS/278 RX50s.
; 07/09/1995	JMBW	RX23, RX33 support (RX33 untested w/real media).
; 07/11/1995	JMBW	RX26, "RX52" support (RX26 untested).
; 08/05/1996	JMBW	Auto-sense image file type on MOUNT.
; 08/08/1996	JMBW	Indirect command files ("@ file"), PUTR.INI.
; 08/11/1996	JMBW	COPY/FILE/DEV, PC-style floppy disks and images.
; 08/24/1996	JMBW	RX26 support debugged (finally got a 2.88 MB drive).
; 08/26/1996	JMBW	Maintains software TG43 signal for CompatiCard.  V1.2.
; 10/24/1996	JMBW	Fixed problems with null OS/8 tentative files.  V1.21.
; 07/24/1997	JMBW	Supports SCSI disks using ASPI driver.
; 07/27/1997	JMBW	INIT/SEGMENTS:n switch.
; 10/30/1997	JMBW	Fixed TU58 bug with COPY/FILE/DEV.  V1.22.
; 11/24/1997	JMBW	MOUNT/PARTITION:n switch (for RT-11 DU:).  V1.23.
; 12/01/1997	JMBW	Fixed TU58 baud rate parser.  DISMOUNT/UNLOAD.  V1.24.
; 12/02/1997	JMBW	SET DISMOUNT command.
; 04/11/1998	JMBW	Added RAxx series drive types.
; 04/12/1998	JMBW	Fixed handling of OS/278 disks with 2nd word of dir seg
;			set wrong, also fixed size for INIT /OS8/RX50.  V1.25.
; 06/04/1998	JMBW	Fixed math error in RTWDIR, got incorrect # of dir segs
;			for disks with exactly 289. files.  V1.26.
; 06/09/1998	JMBW	Fixed DXILV for OS/8 RX02s (never worked!).  V1.27.
; 06/11/1998	JMBW	I'm an idiot.  Now split into DXILV/DYILV.  V1.28.
; 07/14/1998	JMBW	Added Massbus RMxx/RPxx series drive types.
; 07/26/1998	JMBW	Added RP02/03 drive types.
; 11/06/1998	JMBW	Reads DOS/BATCH disks.
; 11/10/1998	JMBW	Reads new-format XXDP+ disks (old ones use DOS/BATCH).
; 11/12/1998	JMBW	Allows BLOCKS/MB/etc. in file size for FORMAT /MSCP.
; 03/26/1999	JMBW	File sizes over 2 GB on FAT32 systems (only).
;			SHOW w/no arg shows all mounted devices.
; 06/19/1999	JMBW	Beginnings of Catweasel floppy support.
; 12/06/1999	JMBW	Added RD32, RD51-53 drive types.
; 01/12/2000	JMBW	Fixed hang reading RSTS files with clustersize >= 128.
; 02/08/2000	JMBW	Fixed CHKXX to check bitmap sequence numbers.
; 03/04/2000	JMBW	CDROMx: driver reads raw CDs with MSCDEX/NWCDEX.
; 03/06/2000	JMBW	Writes DOS/BATCH and XXDP+ disks.
; 03/31/2000	JMBW	Writes RSTS disks, finally!
; 04/26/2000	JMBW	Reads ODS-1 disks in text or image mode.
; 05/18/2000	JMBW	Reads/writes TSS/8 PUTR DECtapes.  Can get dir listing
;			and *some* files on PS/8 DECtapes too (I have only one
;			image to test and it has weird interleave).
; 05/23/2000	JMBW	Reads TSS/8.24 disks (SEGSIZ=256 only).
; 06/11/2001	JMBW	Fixed pesky pointer bug in RSTS writing (RSWL).
; 06/15/2001	JMBW	V2.0 released at very long last.
; 09/05/2001	JMBW	Fixed typo in OPN, also DOS/BATCH and XXDP+ buffer bug,
;			added support for DOS/BATCH DECtapes, and cleaned up
;			auto-detection of OS/8 and PUTR.SAV DECtapes.  V2.01.
;
; FDCs that work in single density mode:
; SMC		FDC37C65+ (non-+ version suspected to work too)
; Goldstar	GM82C765B
; NatSemi	PC8477B
;
;--
	.radix	8	;por supuesto
;
;	include	sym.inc	;(symbols for SYMDEB/386SWAT only, this line optional)
;
tab=	11
lf=	12
cr=	15
crlf=	cr+(lf*400)
;
dbufl=	512d		;length of floppy buffer
indsiz=	128d		;length of indirect file buffer
bufsiz=	2000		;size of general purpose buf (one RT-11 dir segment)
;
	.386		;allow 32-bit constant
cdsize=	700000000d	;max size of CD in bytes (approx., MSCDEX won't tell)
	.8086
;
; Cram in-line string into buffer at ES:DI.
; (or call RTN with in-line arg, if specified.)
cram	macro	string,rtn
	local	a,b
ifb <rtn>
	call	cram1		;;CRAM1 crams in-line counted arg into ES:DI
else
	call	rtn
endif
a	db	b,string
b=	$-a-1
	endm
;
; Define a byte field in a record (_ is current offset).
; Num is # bytes, default is 1.
defb	macro	nam,num:=<1>
&nam=	byte ptr _
_=	_+&num
	endm
;
; Define a word field in a record (_ is current offset).
; Num is # words, default is 1.
defw	macro	nam,num:=<1>
&nam=	word ptr _
_=	_+(&num*2)
	endm
;
; Define a dispatch table entry, giving an error message if order doesn't
; agree with NAM.  ADDR is the call address.
disp	macro	nam,addr
if nam eq _
	dw	&addr
_=_+2
else
	%out	Dispatch table out of order:  &nam
	.err
endif
	endm
;
; Define a keyword record for TBLUK, given a string containing exactly one
; hyphen to show the minimum allowable abbreviation.
;	db	length to match
;	db	total length
;	db	'KEYWORD'
;	dw	ADDR
kw	macro	text,addr
kh=	0
ki=	0
	irpc	kc,text
ki=	ki+1
ifidn <kc>,<->
kh=	ki
endif
	endm ;; irpc
ife kh
	.err
	%out	No hyphen in string:  &text
	exitm
endif ;; ife kh
	db	kh-1,ki-1
	irpc	kc,text
ifdif <kc>,<->
	db	'&kc'
endif
	endm ;; irpc
	dw	addr
	endm
;
; Give an error message if an assumption is false (for code that depends on
; special knowledge of something in order to work).
.assume	macro	cond
if &cond
else
	.err
	%out	Assumption &cond is false
endif
	endm
;
; Define an ASPI error message table entry
amsg	macro	val,str
	local	a,b
a	db	&val,0,b,&str
b=	$-a-3
	endm
;
; Define a token for file system, dev type etc.
token	macro	name
&name=_
_=_+2
	endm
;
; Call and return.
callr	macro	dst
	jmp	&dst
	endm
;
; Set up OS types, each must be unique, low byte is word size (12. or 16.)
;
ostype	macro	nam,sym
&nam=	&sym
&sym=	&sym+100h
	endm
;
twelv=	12d	;PDP-8 file structures
sixtn=	16d	;PDP-11 file structures
;
	ostype	frgn,sixtn	;foreign (no file system, PDP-11 interleave)
	ostype	cos310,twelv	;COS-310 (binary files only)
	ostype	dosbat,sixtn	;DOS/BATCH
	ostype	ods1,sixtn	;RSX-11, IAS, early VAX/VMS
	ostype	os8,twelv	;OS/8, OS/78, OS/278
	ostype	ps8,twelv	;PS/8 (DECtapes seem to have interleave???)
	ostype	putr,twelv	;TSS/8, PUTR.SAV format
	ostype	rsts,sixtn	;RSTS disk structure 0.0, 1.1, 1.2
	ostype	rt11,sixtn	;RT-11
	ostype	tss8,twelv	;TSS/8.24 disk structure (SEGSIZ=256.)
	ostype	xxdp,sixtn	;XXDP+ V2 mutation of DOS/BATCH format
				;(older versions use regular DOS/BATCH format)
;
; Drive type (real, equivalent to real (5.25" vs. 8"), or simulated (image)):
;
_=2	; (all valid types are non-zero)
	token	mscp	;size is whatever they say
	token	ra60	;UDA50/RA60 (205 MB)
	token	ra70	;UDA50/RA70 (280 MB)
	token	ra71	;UDA50/RA71 (700 MB)
	token	ra72	;UDA50/RA72 (1.0 GB)
	token	ra73	;UDA50/RA73 (2.0 GB)
	token	ra80	;UDA50/RA80 (121 MB)
	token	ra81	;UDA50/RA81 (456 MB)
	token	ra82	;UDA50/RA82 (622 MB)
	token	ra90	;UDA50/RA90 (1.2 GB)
	token	ra92	;UDA50/RA92 (1.5 GB)
;	token	rc25	;KLESI/RC25 (25 GB)
; (need exact size -- saw 50840 blocks in one table on fiche but is it right?)
; (NetBSD/VAX /etc/disktab says 50736 -- is *it* right?)
;;	token	rd31	;RQDX3/RD31 (20 MB -- Seagate ST225)
	token	rd32	;RQDX3/RD32 (40 MB -- Seagate ST238 I think?)
;;	token	rd50	;RQDX3/RD50 (5 MB -- Seagate ST412)
	token	rd51	;RQDX3/RD51 (10 MB -- Seagate ST506)
	token	rd52	;RQDX3/RD52 (30 MB)
	token	rd53	;RQDX3/RD53 (69 MB -- Micropolis 1325)
	token	rd54	;RQDX3/RD54 (159 MB -- Maxtor XT2190)
	token	rk02	;RK11/RK02 (1.2 MB)
	token	rk05	;RK11/RK05 (2.5 MB)
	token	rk06	;RK611/RK06 (14 MB)
	token	rk07	;RK611/RK07 (27 MB)
	token	rl01	;RL11/RL01 (5 MB)
	token	rl02	;RL11/RL02 (10 MB)
	token	rm02	;RH11/RM02 (67 MB)
	token	rm03	;RH11/RM03 (67 MB)
	token	rm05	;RH70/RM05 (256 MB)
	token	rm80	;RH70/RM80 (124 MB)
	token	rp02	;RP11C/RP02 (20 MB)
	token	rp03	;RP11C/RP03 (40 MB)
	token	rp04	;RH11/RP04 (88 MB)
	token	rp05	;RH11/RP05 (88 MB -- Memorex version of RP04)
	token	rp06	;RH11/RP06 (174 MB)
	token	rp07	;RH70/RP07 (516 MB)
	token	rs03	;RH11/RS03 (512 KB)
	token	rs04	;RH11/RS04 (1.0 MB)
	token	rs08	;RF08/RS08 (256 KW PDP-8 fixed-head disk)
flpbeg=	_	; beginning of floppy types
	token	rx01	;RX11/RX01 (250.25 KB) 77x1x26
	token	rx02	;RX211/RX02 (500.5 KB) 77x1x26
	token	rx03	;RX211/RX03 (1001 KB) -- semi-documented RX02 DS mod
	token	rx23	;???/RX23 (1.44 MB) 80x2x18
	token	rx24	;???/RX24 (720 KB) 80x2x9
	token	rx26	;???/RX26 (2.88 MB) 80x2x36
	token	rx31	;???/RX31 (360 KB) 40x2x9 (needs double-step on AT)
;;; not currently supported by anything since we don't double-step
	token	rx33	;RQDX3/RX33 (1.2 MB) 80x2x15
	token	rx50	;RX50 (400 KB) 80x1x10
	token	rx52	;RX50/double sided (800 KB) -- "RX52" is my name for it
			;(may not really exist, but supported by P/OS)
flpend=	_-1	; end of floppy types
.assume <flpend lt 400>	;FDCMOS and FDTYPE expect token to fit in a byte
	token	tu56	;TC08/TU56 (129.x12.x2702'), TC11/TU56 (256.x16.x578.)
	token	tu58	;DL11/TU58 (256 KB)
;
; Actual hardware type used to simulate the drive
;
_=0
	token	hwfile	;image file
	token	hwflop	;PC floppy drive (DU:, DX:, DY:, DZ:)
	token	hwcwsl	;Catweasel floppy drive
	token	hwpart	;hard disk partition (never finished this one)
	token	hwtu58	;TU58 attached to serial port (DD:)
	token	hwaspi	;ASPI SCSI disk
	token	hwcdex	;raw CD-ROM via MSCDEX.EXE (or DR-DOS NWCDEX.EXE)
;
; Logical device record (one for each mounted device)
;
_=0
	defw	next		;ptr to next record
	defw	logd		;logical device name (right-justified)
	defw	logu		;flag,,unit #
	defw	hwtype		;hardware type (image file, floppy, etc.)
	defw	fsys,0		;file system structure
	defb	wsize,2		; low byte is word size (12d or 16d)
	defw	med		;medium type (RX01, etc.)
	defw	dsrini		;init DSR (called on each dev change)
	defw	rdblk		;routine to read CX blocks starting at DX:AX
	defw	rdasc		;RDBLK, ASCII mode
	defw	wrblk		;routine to write CX blocks starting at DX:AX
	defw	wrasc		;WRBLK, ASCII mode (if there's a difference)
	defw	handle		;handle to file (if disk image)
	defw	fintlv		;routine to compute floppy interleave for AX
iovecs=_	; I/O vectors start here
	defw	defset		;set up defaults before first access
	defw	savcwd		;save dir context before switching devs
	defw	rstcwd		;restore dir context after switching devs
	defw	volnam		;print vol name
	defw	gdir		;get dir name
	defw	gfile		;get dir/filename
	defw	pfile		;print dir/filename
	defw	dirhdr		;print directory listing header
	defw	dirinp		;init for dir lookup
	defw	dirget		;get next dir entry
	defw	dirdis		;display dir entry
	defw	diremp		;display empty dir entry
	defw	dirnam		;print dir name
	defw	dirfin		;finish up dir access (flush dirty bufs etc.)
	defw	dirsum		;print summary after directory listing
	defw	open		;open most recently DIRGETted file for input
	defw	read		;read from input file
	defw	reset		;close input file
	defw	dirout		;init for dir write
	defw	create		;create output file
	defw	write		;write to output file
	defw	wlast		;write last buffer (partial block) to file
	defw	close		;close output file
	defw	delfil		;delete file
	defw	chdir		;change current dir
	defw	wboot		;write bootstrap
	defw	wipout		;wipe out (zero) unused areas for compression
niovec=(_-iovecs)/2
	; (I'm labeling what sets each of these size fields, just to get my
	; head straight, the code is pretty cluttered and I want to fix it up)
	defw	totsiz,4	;total dev size in bytes (including trk 0 etc.)
				;set by:
				;  FORMAT (image files)
				;  SETMED (unless /MSCP)
				;  per-device "open" routines called by IMAGE
	defw	totblk,4	;total dev size in blocks (including DEC166)
				;set by:
				;  SETMED (unless /MSCP)
				;  per-device "open" routines called by IMAGE
	defw	devsiz,4	;usable (for data) device size in 512-byte blks
				;set by:
				;  SETMED (unless /MSCP)
				;  per-device "open" routines called by IMAGE
	defw	actsiz,2	;active device (subset) size (RT-11)
				;set by:  RTGD, RTCD
	defb	initf		;init flag, cleared in all devs at each prompt
	defb	ecache		;NZ => enable dir cache (RSTS)
			;(doesn't really work, dir blocks are always cached)
	defb	tapflg		;NZ => tape (otherwise disk)
	; filesystem-dependent data
	defw	pcs		;pack clustersize (in bytes under DOS/BATCH)
	defw	dcs		;dev clustersize
	defw	numclu,2	;# of clusters on device (DOS/BATCH)
				;# of PCs on device (RSTS)
				;# of PCs on device (32 bits, ODS-1)
	defw	pcsdcs		;PCS/DCS, # of DCs per PC (RSTS)
	defw	blkbuf		;block buffer (DOS/BATCH)
odpmax=	8d			;max # of path elements for ODS-1
	defw	curdir,2	;.WORD current PPN (RSTS, DOS/BATCH)
				;or .LONG starting blk # (RT-11, OS/8)
				;or .RAD50 path name (ODS-1)
_=	curdir+(odpmax*6)	;(ODS-1:  3 words for each dir element)
				;(ends when full, or if following word is 0)
	defw	cursiz,2	;current LD: size (RT-11)
				;or dir entry size (DOS/BATCH)
	defw	actdir,2	;active dir (during search, usually = CURDIR)
_=	actdir+(odpmax*6)	;(3 words for each dir element)
				;(ends when full, or if following word is 0)
	defw	mfddcn		;DCN of first cluster of MFD (RSTS, TSS/8)
	defw	satptr,2	;DCN of last cluster allocated (RSTS)
				;same for ODS-1, but 32 bits wide
	defw	bitmap,2	;first cluster of storage bitmap (DOS/BATCH)
				;first LBN of SATT.SYS (RSTS)
	defw	indmap,2	;first LBN of INDEXF.SYS header bitmap (ODS-1)
	defw	indbsz		;size of INDEXF.SYS header bitmap (ODS-1)
	defw	filhdr,2	;LBN of first file header in INDEXF.SYS (ODS-1)
				;(first 16 are contig with bitmap)
	defw	extra		;# extra bytes/dir segment (OS/8, RT-11)
	defw	dirbuf		;ptr to head of dir buffer chain
	defw	dirclu		;dir clustersize
	defw	dirwnd,7	;dir retrieval window (MUST FOLLOW DIRCLU)
	defw	rdslev		;RDS level word (RSTS)
				;ODS level word (ODS-1)
	defw	paksts		;pack status word (RSTS)
	defw	filext		;default # blks to add when extending (ODS-1)
	defw	defprt		;default protection bits (ODS-1)
	; device-dependent data
	defb	drive		;physical BIOS drive #, TU58 unit, SCSI target
	defb	host		;host adapter # (SCSI)
	defb	lun		;logical unit # (SCSI)
	defw	port		;COM port base I/O address
	defw	secsz,2		;sector size (only LSW valid unless SCSI)
	defb	ilimg		;1 => interleaved image file (must be undone)
	defb	blksec		;shift count to convert blk # to sec #
				;=9-LOG2(SECSZ)
	defb	ncyls		;# cylinders
	defb	nhds		;# heads
	defb	nsecs		;# sectors per track
	defw	rdsec		;read sector (FDRDS or equiv.)
	defw	wrsec		;write sector (FDWRS or equiv.)
	defw	rdid		;read ID (FDRID or equiv.)
reclen=	_
;
; ODS-1 file header offsets:
_=	0
	defb	h_idof		;word offset of identification area
	defb	h_mpof		;word offset of map area
	defw	h_fnum		;file number
	defw	h_fseq		;file sequence #
	defw	h_flev		;ODS-1 version # used to create file (=101h)
	defw	h_fown		;owner UIC
	defw	h_fpro		;file protection (4-bit fields: W/G/O/S)
fp_rdv=	1	;deny read access
fp_wrv=	2	;deny write access
fp_ext=	4	;deny extend access
fp_del=	10	;deny delete access
	defw	h_fcha,0	;file characteristics
	defb	h_ucha		;user-controlled characteristics
uc_con=	200	;contiguous
uc_dlk=	100	;deaccess locked, not properly closed
	defb	h_scha		;system-controlled characteristics
sc_mdl=	200	;marked for delete
sc_bad=	100	;file contains bad block(s)
sc_dir=	40	;file is a dir (not stored on disk, only in memory)
sc_spl=	20	;this is a spool intermediate file
	defb	h_ufat,32d	;user file attributes, see below
s_hdhd=	_			;length w/o ODS 1.4 extended file #
	defb	h_fext,2	;8 MSBs of file number (2nd byte is what?)
;
; FCS file attributes (RMS attrs are a superset)
_=0
	defb	f_rtyp		;record type
r_fix=	1	;fixed-length records
r_var=	2	;variable-length records
r_seq=	3	;sequenced variable-length records
r_stm=	4	;stream format records (RSTS/E)
	defb	f_ratt		;record attributes
fd_ftn=	1	;FORTRAN carriage control (1st char of line)
fd_cr=	2	;implied carriage control, each record is a line
fd_prn=	4	;R_SEQ sequence #s are actually print control
fd_blk=	10	;records may not span block boundaries
	defw	f_rsiz		;record size (F_FIX), or max record size
	defw	f_hibk,2	;highest VBN allocated (MSW first)
	defw	f_efbk,2	;VBN of EOF (MSW first)
	defw	f_ffby		;first free byte, i.e. byte following EOF in
				;block F_EFBK -- if F_FFBY.EQ.0, F_EFBK is
				;actually the block *following* the EOF block
s_fatt=	14d
;
; BINFLG values:
bin=	0	;binary transfer
text=	2	;text transfer (translate to ASCII and back)
.assume <(rdasc-rdblk) eq (text-bin)>
.assume <(wrasc-wrblk) eq (text-bin)>
;
; File size table entry:
; (for figuring out medium type based on image file size)
_=0
	defw	flmed		;medium type
	defw	flsiz,2		;actual image file size in bytes
	defw	flusz,2		;usable device size in bytes
	defw	flbsz,2		;size of bad sector track (etc.) in bytes
	defw	flcyl		;# cylinders
	defb	flhd		;# heads
	defw	flsec		;# sectors
	defw	flbs		;# b/s
	defb	flilv		;1 if interleaved (must be undone)
fllen=_
;
; Macro to define a file size table entry:
;	dw	med		;medium type code
;	dd	size		;image size in bytes
;	dd	use		;usable part of disk in bytes
;	dd	bad		;size of bad sector file in bytes
;	dw	cyl		;cylinders
;	db	hd		;heads
;	dw	sec		;sectors
;	dw	bs		;bytes/sector
;	db	ilv		;NZ => interleaved file (logical blk order)
;
filsiz	macro	med,siz,use,bad,cyl,hd,sec,bs,ilv
	dw	&med		;;medium type code
	;; size of image file in bytes
ifnb <&siz>
	dd	&siz		;;file size given explicitly
else
	dd	&cyl*&hd*&sec*&bs ;;default is the whole device
endif
	;; size of usable part of device in bytes
ifnb <&use>
	dd	&use		;;usable size given explicitly
else
 ifnb <&bad>
	dd	(&cyl*&hd*&sec*&bs)-(&bad) ;;all but bad block list
 else
	dd	&cyl*&hd*&sec*&bs
 endif
endif
	;; size of bad sector file in bytes
ifnb <&bad>
	dd	&bad		;;size of bad sector file
else
	dd	0		;;no bad sector file
endif
	dw	&cyl		;;cylinders
	db	&hd		;;heads (i.e. tracks)
	dw	&sec		;;sectors
	dw	&bs		;;bytes/sector
ifnb <&ilv>
	db	&ilv		;;NZ => file is interleaved (logical blk order)
else
	db	0
endif
	endm
;
; DOS/BATCH dir buffer format:
;
; N.B. Cluster comes after fixed fields instead of before, since the cluster
; size is variable
;
_=0
	defw	dbblk	;block #
	defw	dbnxt	;ptr to next buf or 0
	defb	dbdrt	;NZ => buf is dirty
	defw	dbdat,0	;data start here
;
; OS/8 dir segment buffer format:
;
_=1000			;256. words of data first
	defw	ossblk	;block #
	defw	ossnxt	;ptr to next buf or 0
	defb	ossdrt	;NZ => buf is dirty
osslen=_
;
; RSTS dir/bitmap/etc. buffer format (used for ODS-1 too):
;
_=1000			;512. bytes of data first
	defw	rdsbkl	;abs 24-bit block number (LSW) (or 0 if not used yet)
	defw	rdsbkh	; " " " (MSW, we store 32 bits for future expansion)
	defb	rdsdrt	;NZ => buf is dirty
	defw	rdsnxt	;ptr to next buf or 0
rdslen=_
;
; RT-11 dir segment buffer format:
;
_=2000			;1024. bytes of data first
	defw	rtsblk,2 ;starting abs blk # of block pair
	defw	rtsnxt	;ptr to next buf or 0
	defb	rtsdrt	;NZ => buf is dirty
rtslen=_
;
; FDC hardware registers:
;
fdc=	3F0h	;base address
fdccc4=	fdc+0	;Micro Solutions CompatiCard IV control register
fdcfcr=	fdc+1	;FDC format control register (SMC FDC37C65C+ only)
		;(my homemade FDC has it at 3F6h)
fdcdor=	fdc+2	;FDC digital output register
fdcmsr=	fdc+4	;FDC main status register
fdcdat=	fdc+5	;FDC data register
fdcdcr=	fdc+7	;FDC diskette control register (data rate, etc.)
fdcdir=	fdc+7	;FDC digital input register (disk change line in b7)
;
mfm=	100	;MF flag set (double density)
fm=	0	;MF flag clear (single density)
;
seek_status equ byte ptr 043Eh	;int flag (b7), "recal needed" bits (b3-b0)
motor_status equ byte ptr 043Fh	;motor bits (b7 = spin-up first (write))
motor_count equ byte ptr 0440h	;# timer ticks until motor turnoff
timer_low equ word ptr 046Ch	;low word of system time
;
code	segment
	assume	cs:code
	org	100h		;.COM file
;
; Global register usage:
;
; BP	generally points at a dev record
; DS	points at code
; ES	points at code (except when temporarily used for buffers)
; SS	points at code (stack is after code, before buffers)
; DF=0
;
; Note that we sometimes have to reload ES from DS before doing directory
; manipulations, in routines which are otherwise using ES to point at a buffer,
; because many of the "read/write directory block" routines depend on ES=DS.
;
start:	cld			;DF=0
	mov	ah,30h		;func=get DOS version
	int	21h
	mov	ds:dosver,ax	;save
	; make sure we have enough memory to run
	and	sp,not 0Fh	;round down to paragraph boundary
	cmp	sp,offset mem	;enough?
	jb	notmem		;no
	mov	ds:himem,sp	;save ptr (PSP:0006 isn't quite right)
;;; huh?  shouldn't SP be set to MEM here, and MEM should be mult of 16.
	mov	bx,sp		;copy
	mov	cl,4		;shift count
	shr	bx,cl		;find # paragraphs to keep
	mov	sp,offset pdl	;shrink stack
	mov	ah,4Ah		;func=setblock
	int	21h
	; allocate a sector I/O buffer (for CD-ROMs, Catweasel etc.)
	mov	dx,8192d	;size of buffer to get
getm3:	mov	bx,dx		;copy size in bytes
	mov	cl,4		;shift count
	shr	bx,cl		;find # paragraphs
	mov	ah,48h		;func=getblock
	int	21h
	jnc	getm4		;got it
	shr	dx,1		;req /2
	cmp	dh,512d/400	;keep going as long as there's a point
	ja	getm3
	xor	ax,ax		;didn't get anything
getm4:	mov	ds:secbuf,ax	;save ptr to sector buf, or 0
	mov	ds:secsiz,dx	;save size of sector buf
	; allocate a large I/O buffer
	mov	bx,-1		;get all available memory
getm1:	mov	ah,48h		;func=getblock
	int	21h		;give it a try
	jnc	getm2		;got it, go see how big
	test	bx,bx		;anything left?
	jnz	getm1		;yes
notmem:	cram	'?Not enough memory',wrng
	mov	ax,4C01h	;func=punt
	int	21h
getm2:	cmp	bx,800h		;at least 32KB?
	jb	notmem		;no, can't even hold one big cluster
	mov	ds:bigbuf,ax	;save ptr
	mov	ax,16d		;segment size
	mul	bx		;find total # bytes
	mov	ds:bigsiz,ax	;save
	mov	ds:bigsiz+2,dx
	mov	dx,offset ctrlc	;pt at ^C vector
	mov	ax,2523h	;func=set INT 23h vector
	int	21h
	; get # of floppy drives
	int	11h		;among other things
	xor	bl,bl		;assume none
	test	al,1		;are there any?
	jz	numfd1		;no
	rol	al,1		;left 2
	rol	al,1
	and	al,3		;isolate
	inc	ax		;get actual #
	mov	bl,al		;copy
numfd1:	mov	al,bl		;copy
	mov	ds:numfd,al	;save
	add	al,'A'-1	;get max valid floppy drive letter
	mov	ds:maxfd,al	;save
	cmp	bl,1		;exactly one floppy?
	jne	numfd2
	 inc	bx		;yes, pretend there are two
numfd2:	mov	ds:firhd,bl	;unit # of first HD
	add	bl,'A'		;physical drive letter of first HD
	mov	ds:lethd,bl	;save
	; get drive types from CMOS if AT
	mov	ax,0FFFFh	;point at last paragraph of real mode memory
	mov	es,ax
	cmp	byte ptr es:0Eh,0FCh ;AT?
	jne	numfd3
	mov	al,10h		;floppy types are at CMOS location 0010
	out	70h,al
	jmp	short $+2	;hocus pocus (I/O delay on non-local-bus ATs)
	in	al,71h		;get types, A in high nibble, B in low
	mov	bl,al		;copy
	and	bx,0Fh		;isolate B: type
	mov	ah,ds:fdcmos[bx] ;translate
	mov	bl,al		;copy
	mov	cl,4		;shift count
	shr	bl,cl		;right-justify A: drive type
	mov	al,ds:fdcmos[bx] ;translate
	mov	word ptr ds:fdtype,ax ;save
numfd3:	; get # of hard drives
	xor	ax,ax		;load 0
	mov	es,ax		;into ES
	mov	al,byte ptr es:0475h ;get # drives
	mov	ds:numhd,al	;save
	; get row count for **MORE** processing
	mov	al,byte ptr es:0484h ;EGA and later, # lines on screen -1
	test	al,al		;did we get anything?
	jnz	numfd4
	 mov	al,25d-1	;no, all pre-EGA boards had 25d lines only
numfd4:	mov	ds:lmax,al	;save
	push	ds		;restore ES (trashed in INT 21h AH=35h above)
	pop	es
	; make sure disk buffer doesn't span 64 KB boundary
	mov	ax,ds		;get seg addr
	mov	cl,4		;bit count
	sal	ax,cl		;left 4 (lose high bits)
	add	ax,offset dbuf1	;pt at buffer
	add	ax,dbufl-1	;see if it spans (OK to stop just short)
	jnc	nospan		;no, skip
	 mov	ds:dbuf,offset dbuf2 ;yes, well this won't
nospan:	; get currently logged in DOS disk letter to use as default
	mov	ah,19h		;func=get logged in DOS disk
	int	21h
	add	al,'A'		;convert to drive letter
	mov	ah,al		;duplicate
	mov	word ptr ds:curdsk,ax ;save
	; look for an ASPI manager (Advanced SCSI Programming Interface)
	mov	dx,offset scsimg ;filename
	mov	ax,3D00h	;func=open /RONLY
	int	21h
	jc	lkasp3		;failed, doesn't exist
	; got something, make sure it's not a file and that it accepts IOCTL RD
	mov	bx,ax		;copy handle
	mov	ax,4400h	;func=IOCTL (get dev flags)
	int	21h
	jc	lkasp1		;failed
	and	dx,40200	;isolate IOCTL RD, DEVICE bits
	cmp	dx,40200	;both must be set
	jne	lkasp1
	; OK so far, ask for entry point
	mov	dx,offset buf	;point at scratch buf
	mov	cx,4		;byte count
	mov	ax,4402h	;func=IOCTL (read control string)
	int	21h
	jc	lkasp1		;failed
	cmp	ax,4		;got all four bytes?
	jne	lkasp1		;no
	mov	ax,word ptr ds:buf ;fetch entry point
	mov	dx,word ptr ds:buf+2
	mov	ds:aspi,ax	;save
	mov	ds:aspi+2,dx
lkasp1:	mov	ah,3Eh		;func=close handle
	int	21h
lkasp2:	mov	ax,ds:aspi	;see if we got anything
	or	ax,ds:aspi+2
	jz	lkasp3		;no, skip this
	; do "SCSI HA inquiry" cmd, see if manager can handle incomplete xfrs
	mov	si,offset hainq	;pt to cmd block
	mov	cx,lhainq
	mov	di,offset srb	;ES:DI=SRB buf
	push	es		;save SRB addr as call arg
	push	di
	rep	movsb		;copy cmd
	call	dword ptr ds:aspi ;call ASPI driver
	pop	ax		;flush stack
	pop	ax
	call	aspwt		;await completion
	call	serr		;check for error
	cmp	word ptr ds:srb+4,55AAh ;did handler see our stupid sig?
	jne	lkasp3
	test	byte ptr ds:srb+3Ah,2 ;residual byte count supported?
	jz	lkasp3
	 inc	ds:resbc	;set flag
lkasp3:	; get SWITCHAR (undocumented!)
	mov	ax,3700h	;func=get SWITCHAR
	int	21h
	mov	ds:swchar,dl	;save it
	; init CRC-CCITT lookup table (for Catweasel/ISA raw FDC)
	call	bldcrc
	; look for PUTR.INI
	mov	dx,offset inifil ;point at init file name
	call	openro		;try to open
	jnc	infl9		;got it
	; check executable's directory if DOS 3.0 or later
	cmp	byte ptr ds:dosver,3 ;DOS V3.X or higher?
	jb	infl11		;no
	push	ds		;save
	mov	ds,word ptr ds:[002Ch] ;point at environment
	xor	si,si		;starting offset
infl1:	lodsb			;read a byte
	test	al,al		;end of environment?
	jz	infl3
infl2:	lodsb			;no, look for end of string
	test	al,al
	jnz	infl2
	jmp	short infl1	;now it could be end of env
infl3:	; found end of environment, copy executable's full filespec
	lodsw			;skip # of parameters
	mov	di,offset buf	;point at where to put filename
	mov	dx,di		;save a ptr
	mov	bx,di		;pt at begn of path element
infl4:	lodsb			;get a byte
	test	al,al		;end of string?
	jz	infl7
	cmp	al,':'		;colon?
	je	infl5
	cmp	al,'/'		;slash?
	je	infl5
	cmp	al,'\'		;backslash?
	jne	infl6
infl5:	lea	bx,[di+1]	;pt at begn of next path element
infl6:	cmp	di,offset buf+bufsiz ;off end of buf?
	je	infl10		;yes
	stosb			;otherwise save it
	jmp	short infl4	;loop
infl7:	; replace last path element with init file's name
	pop	ds		;restore
	mov	di,bx		;point at last path element
	mov	si,offset inifil ;point at init file name
infl8:	lodsb			;get a byte
	cmp	di,offset buf+bufsiz ;off end of buf?
	je	infl11		;yes
	stosb			;save if not
	test	al,al		;done all?
	jnz	infl8		;loop if not
	; try to open it there
	call	openro		;try to open
	jc	infl11		;forget about it
infl9:	mov	ds:indhnd,ax	;save handle
	mov	ds:indctr,0	;buffer initially empty
	jmp	short infl11
infl10:	pop	ds		;restore
infl11:	; put up banner
	mov	di,offset lbuf	;pt at buf
 cram 'PUTR V2.01  Copyright (C) 1995-2001 by John Wilson <wilson@dbit.com>.'
	mov	byte ptr ds:lnum,10d ;don't **MORE**
	call	flush
 cram 'All rights reserved.  See www.dbit.com for other DEC-related software.'
	call	flush		;put up msg
	call	flush		;+blank line
	cram	'COPY mode is ASCII, SET COPY BINARY to change'
	call	flush
;+
;
; Main loop.
;
;-
mloop:	; reset everything (in case error abort, ^C, or 'Q' from **MORE**)
	mov	ax,cs		;copy cs
	mov	ds,ax		;to all
	mov	es,ax
	cli			;ints off (may have been already via INT inst)
	mov	sp,offset pdl	;;reset stack
	sti			;;ints on after next inst
	mov	ss,ax
	; flush dir buffers
	xor	bp,bp		;load 0 (don't get caught in a loop!)
	xchg	bp,ds:indev	;get input dev, zap it
	test	bp,bp		;anything?
	jz	mlp1
	call	ss:dsrini[bp]	;init
	xor	bx,bx		;switch to input
	call	ss:rstcwd[bp]
	call	ss:dirfin[bp]	;finish up dir I/O
mlp1:	xor	bp,bp		;load 0
	xchg	bp,ds:outdev	;get output dev, zap it
	test	bp,bp		;anything?
	jz	mlp2
	call	ss:dsrini[bp]	;init
	mov	bx,2		;switch to output
	call	ss:rstcwd[bp]
	call	ss:dirfin[bp]	;finish up dir I/O
mlp2:	mov	ax,ds:kepmem	;flush unkept memory
	mov	ds:fremem,ax
	; close any extraneous open handles (they may have aborted an xfr)
	mov	bx,-1		;-1 means closed
	xchg	bx,ds:inhnd	;see if it was open
	test	bh,bh
	js	mlp3		;no
	mov	ah,3Eh		;func=close
	int	21h
	mov	bx,-1		;-1 again
mlp3:	xchg	bx,ds:outhnd	;see if this was open
	test	bh,bh
	js	mlp4		;no
	mov	ah,3Eh		;func=close
	int	21h
mlp4:	; flush incomplete dev rec (from TYPE, or aborted MOUNT etc.)
	xor	bp,bp		;load 0
	xchg	bp,ds:tmpdev	;clear it, see if NZ
	test	bp,bp
	jz	mlp5
	call	retrec		;return to free list
mlp5:	; mark all devs as not initted
	mov	bp,ds:logdev	;get head of dev list
	jmp	short mlp7	;jump into loop
mlp6:	mov	ss:initf[bp],0	;mark as not initted
	mov	bp,ss:next[bp]	;follow link
mlp7:	test	bp,bp		;end?
	jnz	mlp6
	mov	byte ptr ds:fdrst,0 ;FDC may need resetting
	test	byte ptr ds:cc4fdc,-1 ;is it a CompatiCard IV?
	jz	mlp8
	mov	dx,fdccc4	;yes, point at CC4 control port
	mov	al,0Ch		;give it a freshly-reset value
	out	dx,al
mlp8:	; turn off Catweasel motors if needed
	test	byte ptr ds:cwmot,-1 ;any motors on?
	jz	short mlp9	;no
	call	cwdes		;deselect drive(s)
	call	cwoff		;turn motor(s) off
mlp9:	; print prompt
	mov	di,offset lbuf	;pt at buffer
	mov	al,'('		;put in parentheses
	stosb
	test	byte ptr ds:nprmpt,-1 ;no prompt?
	jnz	mlp10		;right, don't get caught in a loop
	inc	byte ptr ds:nprmpt ;in case we don't survive this
	mov	dl,ds:curdsk	;is a DOS disk current?
	test	dl,dl
	jnz	mlp11		;yes
	mov	bp,ds:logdev	;[get head of device list]
	push	di		;save
	call	ss:dsrini[bp]	;init DSR
	call	ss:defset[bp]	;set defaults
	pop	di		;restore
	call	pdev		;show device name
	xor	cx,cx		;no trailing \ (or whatever)
	call	ss:dirnam[bp]	;show dir name
	jmp	short mlp12
mlp10:	mov	si,offset badprm ;prompt
	mov	cx,13d		;length
	rep	movsb		;copy
	jmp	short mlp13	;continue
mlp11:	mov	al,dl		;copy
	mov	ah,':'		;add colon
	stosw
	xor	cx,cx		;no trailing \
	call	dosddl		;display dir for drive in DL
mlp12:	dec	byte ptr ds:nprmpt ;success, clear flag
mlp13:	mov	ax,">)"		;add ")>"
	stosw
	mov	byte ptr ds:lnum,2 ;no **MORE**
	call	flush1		;flush
	call	gtlin		;get a line from the keyboard
	; get today's date so we can interpret OS/8 dates
	push	cx		;save
	mov	ah,2Ah		;func=get date
	int	21h
mlp14:	mov	word ptr ds:day,dx ;save
	mov	ds:year,cx	;save
	sub	cx,1970d	;subtract OS/8 base
	and	cl,not 7	;get mult of 8d
	add	cx,1970d	;add this back in
	mov	ds:yrbase,cx	;save OS/8 base year
	mov	ah,2Ch		;func=get time
	int	21h
	mov	word ptr ds:min,cx ;save
	mov	ds:sec,dh
	mov	ah,2Ah		;func=get date
	int	21h
	cmp	word ptr ds:day,dx ;check for change
	jne	mlp14		;whoops, we passed midnight
	pop	cx		;restore
	call	skip		;skip blanks etc.
	jc	mlp15		;blank line, reprompt
	cmp	al,'@'		;open indirect file?
	je	mlp16
	cmp	al,';'		;comment?
	je	mlp15		;ignore line if so
	cmp	al,'!'		;either way
	je	mlp15
	call	getw		;get a word
	jc	mlp15		;just got a separator, reprompt
	mov	ax,offset cmdtab ;pt at command table
	call	tbluk		;look up
	jc	mlp20
	call	ax		;call the routine
mlp15:	jmp	mloop		;loop
mlp16:	; starts with "@", look for filename
	inc	si		;eat the "@"
	dec	cx
	mov	bx,-1		;mark handle as empty
	xchg	bx,ds:indhnd	;and get old value
	test	bx,bx		;was it open?
	js	mlp17
	mov	ah,3Eh		;yes, func=close
	int	21h
mlp17:	call	getw		;get filename
	jc	mlp18		;failed
	call	confrm		;should be EOL
	mov	si,bx		;point at filename
	mov	cx,dx
	mov	di,offset dotcmd ;default extension
	call	defext		;apply it (get filename ptr in DX)
	call	openro		;try to open
	jc	mlp19
	mov	ds:indhnd,ax	;save handle
	mov	ds:indctr,0	;buffer needs to be loaded
	jmp	short mlp15	;back into loop
mlp18:	jmp	misprm
mlp19:	jmp	fnf
mlp20:	; undefined keyword, see if they want to log into a new device
	push	bx		;save
	push	dx
	mov	si,bx		;back up
	add	cx,dx
	mov	ah,1		;must include colon
	call	getlog		;get logical device
	jc	mlp27		;invalid
	; look for dev name on log dev list
	mov	bp,ds:logdev	;get head of list
	xor	dx,dx		;no prev entry
	jmp	short mlp23
mlp21:	cmp	ss:logd[bp],ax	;is this it?
	jne	mlp22
	cmp	ss:logu[bp],bx
	je	mlp25		;yes
mlp22:	mov	dx,bp		;no, save ptr
	mov	bp,ss:next[bp]	;follow link
mlp23:	test	bp,bp		;is there more?
	jnz	mlp21		;loop if so
	; not on list, see if it could be a DOS disk
	or	bh,al		;2nd letter or unit specified?
	jnz	mlp27		;one or the other, invalid
	call	confrm		;yes, make sure confirmed
	push	ax		;save
	mov	dl,ah		;copy
	sub	dl,'A'-1	;convert to number
	mov	si,offset buf	;pt at buf
	mov	ah,47h		;func=get CWD
	int	21h		;(make sure drive is really there)
	jc	mlp24		;nope
	dec	dx		;numbers start at 0 for login
	mov	ah,0Eh		;func=log in
	int	21h
	jc	mlp24		;error, don't log in
	pop	ax		;restore
	mov	al,ah		;duplicate
	mov	word ptr ds:curdsk,ax ;log in
	mov	byte ptr ds:nprmpt,0 ;assume we can prompt
	jmp	mloop		;(will flush stack)
mlp24:	jmp	baddrv		;no such drive
mlp25:	; found it, relink at head of list
	call	confrm		;make sure confirmed
	mov	byte ptr ds:curdsk,0 ;don't use DOS drive
	test	dx,dx		;was there a prev rec?
	jz	mlp26		;it was already logged in, easy (flush stack)
	mov	bx,dx		;copy ptr
	mov	ax,ss:next[bp]	;unlink bp
	mov	ds:next[bx],ax
	mov	ax,bp		;copy
	xchg	ax,ds:logdev	;set new head of list, get old
	mov	ss:next[bp],ax	;link rest of list in
mlp26:	mov	byte ptr ds:nprmpt,0 ;assume we can prompt
	jmp	mloop		;reprompt (flush stack)
mlp27:	; error
	pop	cx		;restore length
	pop	dx		;and ptr
	mov	di,dx		;copy
	add	di,cx		;pt at end
	mov	al,'?'		;?
	stosb
	mov	ax,crlf		;<CRLF>
	stosw
	add	cl,3		;add to length
	mov	bx,0002h	;handle=STDERR
	mov	ah,40h		;func=write
	int	21h
	jmp	mloop
;
cmdtab:	;kw	<A-NALYZE>,analyz ;analyze floppy format
	kw	<B-OOT>,boot	;write bootstrap (RT-11 COPY/BOOT)
	kw	<CD->,cd	;change directory
	kw	<CH-DIR>,cd	;syn. for CD
	kw	<CL-S>,cls	;clear screen
	kw	<CO-PY>,copy	;copy file(s)
	kw	<DEL-ETE>,delete ;delete file(s)
	kw	<DIR-ECTORY>,dir ;directory of device
	kw	<DIS-MOUNT>,dismnt ;dismount mounted device
	kw	<DUM-P>,dump	;dump a block
	kw	<EXI-T>,exit	;exit from program
	kw	<FOR-MAT>,format ;format floppy, create file, zero partition
	kw	<INI-TIALIZE>,init ;write blank file system
	kw	<MOU-NT>,mount	;mount disk or pseudo-disk
	kw	<Q-UIT>,exit	;syn. for EXIT
	kw	<SE-T>,set	;set parameters
	kw	<SH-OW>,show	;show device parameters
;;;;	kw	<SQ-UEEZE>,squeez ;make empty areas contiguous
	kw	<ST-OP>,exit	;syn. for EXIT
	kw	<TYP-E>,typ	;type a file
	kw	<VOL-UME>,vol	;print volume name
	kw	<WIPE-OUT>,wipe	;write zeros on unused blocks (for GZIP etc.)
 kw <WP->,wprot ;;;
cwhack=0 ;;;;; while hacking Catweasel
if cwhack
 kw <I-D>,cwgid
 kw <S-AVE>,cwsav
 kw <R-ESTORE>,cwrst
endif ;;;
	db	0
;+
;
; ^C exit vector.
;
;-
ctrlc:	mov	bx,-1		;mark indirect file as closed
	xchg	bx,cs:indhnd	;and get current handle (DS value not known)
	test	bx,bx		;is it open?
	js	ctrlc1		;no
	mov	ah,3Eh		;func=close
	int	21h
ctrlc1:	jmp	mloop		;restart
;+
;
; Analyze floppy format.
;
; I haven't decided what would be useful output from this.
;
;-
analyz:	call	gdevn		;parse device (if any)
	cmp	ss:dirinp[bp],offset dosdi ;is it a PC-DOS disk?
	je	anlz99		;yes, doesn't count
	cmp	ss:hwtype[bp],hwflop ;is it a floppy disk?
	jne	anlz99		;no
	call	ss:defset[bp]	;set defaults
	call	ss:dsrini[bp]	;init for input
;;; need to do lots of stepping and looping:
	mov	byte ptr ds:flpcyl,0 ;init cyl to 0
;;; do next cyl
	mov	byte ptr ds:flphd,0 ;head too
;;; do next surface

;;; is there a way to wait for an index pulse?
;;; can't find it in FDC data sheets

	mov	ch,ds:flpcyl	;get cyl
	mov	dh,ds:flphd	;and head
	call	fdrid		;read ID
;	jc	...

;;; SI points to C,H,R,N

;;; loop until value repeats

;;; N.B. ints can screw us up, but we can't disable them
;;; maybe check twice and see if we got the same numbers?

	ret
anlz99:	cram	'?Must be floppy disk',error
;+
;
; Write bootstrap.
;
;-
boot:	call	gdevn		;parse device (if any)
	call	ss:defset[bp]	;set defaults
	callr	ss:wboot[bp]	;go, return
noboot:	cram	"?Don't know how to write bootstrap on that file system",error
;+
;
; Change directory.
;
;-
cd:	call	gdev		;parse device (if any)
	call	ss:defset[bp]	;set defaults
	call	skip		;anything given?
	jnc	cd1		;yes
	mov	di,offset lbuf	;pt at buffer
	call	pdev		;show device name
	xor	cx,cx		;no filename
	call	ss:dirnam[bp]	;show dir name
	jmp	flush		;flush, return
cd1:	callr	ss:chdir[bp]	;change dir, return
;+
;
; Clear screen.
;
;-
cls:	call	confrm		;make sure confirmed
;; should check for ANSI.SYS etc. and use ESC [2J if found
	mov	ah,0Fh		;func=get video mode
	int	10h
	xor	ah,ah		;func=set video mode
	int	10h
cret:	ret
;+
;
; COPY inspec [outspec]
;
;-
copy:	mov	ax,ds:bindef	;get default ASCII/binary flag
	mov	ds:binflg,ax	;set text mode by default
	xor	ax,ax		;load 0
	mov	ds:binswt,al	;no /BINARY or /ASCII yet
	mov	ds:devflg,al	;no /DEV yet
	mov	ds:clusiz,ax	;no /CLUSTERSIZE:n
	mov	ds:contig,al	;no /CONTIGUOUS yet
	mov	ds:ovride,al	;don't override protection on output
	call	copswt		;check for switches
	; get input filespec
	call	gdev		;parse device name
	mov	ds:indev,bp	;save input dev rec
	call	ss:defset[bp]	;set defaults
	mov	di,offset fname1 ;pt at buffer
	xor	bl,bl		;no implied wildcards
	call	ss:gfile[bp]	;parse dir/filename
	xor	bx,bx		;input file
	call	ss:savcwd[bp]	;save dir
	call	copswt		;look for any more switches
	mov	byte ptr ds:notfnd,1 ;nothing found yet
	; get output filespec
	call	gdev		;get output dev name
	mov	ds:outdev,bp	;save output dev rec
	call	ss:defset[bp]	;set defaults
	mov	di,offset fname3 ;pt at buffer
	xor	bl,bl		;no implied wildcards
	call	ss:gfile[bp]	;parse dir/filename
	mov	bx,2		;output file
	call	ss:savcwd[bp]	;save dir
	call	copswt		;check for switches
	call	confrm		;make sure EOL
	; decide what we're doing
	mov	al,ds:devflg	;get /DEV, /FIL flag
	test	al,al		;either specified?
	jz	copy1
	 jmp	icopy		;[device image copy, SF set on AL]
copy1:	; copy file(s)
	mov	si,offset fname1 ;pt at source filespec
	cmp	byte ptr [si],0	;anything?
	jnz	copy2
	mov	word ptr [si],".*" ;no, change to *.*
	mov	byte ptr [si+2],'*'
copy2:	mov	di,offset fname3 ;pt at dest filespec
	cmp	byte ptr [di],0	;anything?
	jnz	copy4
copy3:	lodsb			;no, copy input filespec
	stosb
	test	al,al		;end?
	jnz	copy3		;loop if not
copy4:	mov	bp,ds:indev	;get input device again
	call	ss:dsrini[bp]	;init DSR
	xor	bx,bx		;input dev
	call	ss:rstcwd[bp]	;restore CWD
	call	ss:dirinp[bp]	;init dir input
	mov	bp,ds:outdev	;get output dev
	call	ss:dsrini[bp]	;init DSR
	mov	bx,2		;output dev
	call	ss:rstcwd[bp]	;restore CWD
	call	ss:dirout[bp]	;init dir I/O
copy5:	; check next file
	mov	bp,ds:indev	;get input device
	call	ss:dsrini[bp]	;init DSR
	xor	bx,bx		;switch to input dev
	call	ss:rstcwd[bp]
	mov	byte ptr ds:icont,0 ;assume not contig
	call	ss:dirget[bp]	;get next entry
	jc	copy6		;no more
	jz	copy5		;skip empty areas and dirs
	mov	si,offset fname1 ;pt at source wildcard
	mov	bx,offset fname3 ;dest wildcard
	mov	dx,offset fname4 ;dest buffer
	push	di		;save
	call	mapwld		;check for match
	pop	di		;[restore]
	jne	copy5		;ignore
	; it's a match, type filenames
	mov	byte ptr ds:notfnd,0 ;found one
	call	flush		;display filename
	; open output file
	mov	bp,ds:outdev	;get output device
	call	ss:dsrini[bp]	;init DSR
	mov	bx,2		;switch to output
	call	ss:rstcwd[bp]
	mov	si,offset fname4 ;pt at filename
	call	ss:create[bp]	;create the file
	; copy the file
	mov	bp,ds:indev	;get input dev
	call	ss:dsrini[bp]	;init DSR
	xor	bx,bx		;switch to input
	call	ss:rstcwd[bp]
	call	ss:open[bp]	;open
	jc	copy7
	call	fcopy		;copy the file
	mov	bp,ds:indev	;get input device again
	call	ss:dsrini[bp]	;set up input dev
	xor	bx,bx		;select input dir
	call	ss:rstcwd[bp]
	call	ss:reset[bp]	;close input file
	mov	bp,ds:outdev	;get output dev
	call	ss:dsrini[bp]	;init
	mov	bx,2		;switch to output
	call	ss:rstcwd[bp]
	call	ss:close[bp]	;close output file
	jmp	short copy5	;look for more
copy6:	; no more (dir flushing is handled in MLOOP)
	cmp	byte ptr ds:notfnd,0 ;find anything?
	jnz	copy8
	ret
copy7:	jmp	dirio		;dir I/O err
copy8:	jmp	fnf		;file not found
;+
;
; Handle COPY switches.
;
;-
copswt:	call	skip		;skip white space
	jc	copsw3
	cmp	al,ds:swchar	;switch?
	jne	copsw3
	inc	si		;eat the /
	dec	cx
	call	getw		;get the switch
	jc	copsw1
	mov	ax,offset copsws ;point at switch list
	call	tbluk		;look it up
	jc	copsw2
	call	ax		;call the routine w/CF=0
	jmp	short copswt	;check for more
copsw1:	jmp	synerr		;missing switch
copsw2:	jmp	badswt		;bad switch
copsw3:	ret
;
copsws	label	byte
	kw	<A-SCII>,copasc	;/ASCII (convert to/from text)
	kw	<B-INARY>,copbin ;/BINARY (don't convert to/from text)
	kw	<CL-USTERSIZE>,copclu ;/CLUSTERSIZE (override default FCS)
	kw	<CO-NTIGUOUS>,copctg ;/CONTIGUOUS (contig output if possible)
	kw	<D-EVICE>,copdev ;/DEVICE (source and/or dest are whole dev)
	kw	<F-ILES>,copfil	;/FILES (use w/DEV to say src or dst is file)
	kw	<OVERRIDE->,copovr ;/OVERRIDE (override prot on output file)
	db	0
;
copasc:	; /ASCII
	mov	ds:binflg,text	;[set text mode]
	mov	byte ptr ds:binswt,-1 ;[flag given]
	ret		;CF=0
;
copbin:	; /BINARY
	mov	ds:binflg,bin	;[set bin mode (or sector, for image copies)]
	mov	byte ptr ds:binswt,-1
	ret
;
copclu:	; /CLUSTERSIZE:n
	jcxz	cpcl2		;can't be EOL
	lodsb			;eat the ':'
	dec	cx		;count it off
	cmp	al,':'		;must be ':'
	je	cpcl1
	cmp	al,'='		;or '='
	jne	cpcl3
cpcl1:	call	getn		;get number
	test	ax,ax		;must be NZ (CF=0)
	jz	cpcl4		;isn't
	mov	ds:clusiz,ax	;[save]
	ret
cpcl2:	jmp	misprm		;missing parameter
cpcl3:	jmp	synerr		;syntax error
cpcl4:	jmp	outran		;number out of range
;
copctg:	; /CONTIGUOUS
	mov	byte ptr ds:contig,1 ;[set contiguous flag]
	ret
;
copdev:	; /DEVICE
	or	byte ptr ds:devflg,1 ;set device flag (src ior dst is device)
	ret
;
copfil:	; /FILES
	or	byte ptr ds:devflg,200 ;set files flag (src xor dst is file)
	ret
;
copovr:	; /OVERRIDE
	mov	byte ptr ds:ovride,1 ;[delete output file w/same name]
	ret			;[even if protected]
;+
;
; COPY/DEV dev1: dev2:
; COPY/DEV/FILE dev1: dev2:file
; COPY/DEV/FILE dev1:file dev2:
;
; Make block-by-block image copy of entire device (or image file).
;
; /BINARY means to do it sector-by-sector (i.e. bypass floppy interleave).
;
; al	DEVFLG value
;	b0=/DEVICE, b7=/FILES
; FNAME1 .ASCIZ source filename
; FNAME3 .ASCIZ destination filename
;
;-
icopy:	mov	cx,bin		;use binary mode
	xchg	cx,ds:binflg	;set flag, get its value
.assume <(bin eq 0) and (text lt 400)>
	xor	cl,(bin xor text) ;flip it
	and	cl,ds:binswt	;/BINARY only if explicitly given
	mov	ds:secflg,cl	;save (NZ => /BINARY)
	mov	bl,ds:fname1	;get first char of each filename
	mov	bh,ds:fname3	;each will be NUL if no filename
	test	al,al		;/FILES?
	js	icpy2		;yes
	; just /DEVICE
	or	bl,bh		;neither filename should be given
	jnz	icpy1		;error if one is
	; make sure input and output aren't same device
	mov	bp,ds:indev	;get input dev
	cmp	bp,ds:outdev	;same as output dev?
	jne	icpy3		;skip if not
	jmp	iosame		;input and output devs are same
icpy1:	jmp	synerr
icpy2:	test	al,1		;can't have /FILES alone, needs /DEVICE too
	jz	icpy1
	cmp	bl,1		;normalize flags
	sbb	bl,bl		;0 if was non-zero, otherwise -1
	cmp	bh,1
	sbb	bh,bh
	add	bl,bh		;should have exactly one file
	inc	bl		;right?
	jnz	icpy1		;no
icpy3:	; open input device or file
	mov	bp,ds:indev	;get ptr
	call	ss:dsrini[bp]	;init DSR
	xor	bx,bx		;input dev
	call	ss:rstcwd[bp]	;restore CWD
	call	ss:dirinp[bp]	;init dir input (required even if raw)
	test	byte ptr ds:fname1,-1 ;is there an input filename?
	jnz	icpy6		;yes, find the file
	; input is device
	cmp	ss:dirinp[bp],offset dosdi ;is it a DOS disk?
	je	icpy5
	mov	ax,ss:devsiz[bp] ;get size
	mov	bx,ss:devsiz+2[bp]
	mov	cx,ss:devsiz+4[bp]
	mov	dx,ss:devsiz+6[bp]
	mov	ds:isize,ax	;save
	mov	ds:isize+2,bx
	mov	ds:isize+4,cx
	mov	ds:isize+6,dx
	; get device size in bytes
	test	byte ptr ds:secflg,-1 ;doing sector copy?
	jz	icpy7
	mov	al,ss:nsecs[bp]	;get # sectors
	mul	byte ptr ss:nhds[bp] ;* # heads
	mov	dl,ss:ncyls[bp]	;get # cyls
	xor	dh,dh		;DH=0
	mul	dx		;find total # of sectors
	mov	ds:isize,ax	;that's the real input size
	mov	ds:isize+2,dx
	mov	word ptr ds:isize+4,0
	mov	word ptr ds:isize+6,0
	; calculate actual size in bytes, may differ from image file size
	mov	bx,ax		;save low order
	mov	ax,ss:secsz[bp]	;get sector size
	mul	dx		;find high order
	xchg	ax,bx		;save, get low order sector count
	mul	word ptr ss:secsz[bp] ;multiply
	add	dx,bx		;add high order
	mov	bx,ss:med[bp]	;while we're here, check device type
	cmp	bx,flpbeg	;must be floppy for sector copy
	jb	icpy4
	cmp	bx,flpend
	jbe	icpy8
icpy4:	jmp	notflp
icpy5:	jmp	short icpy9
icpy6:	jmp	short icpy11
icpy7:	mov	cl,9d		;shift count
	rol	ax,cl		;left 9 bits
	shl	dx,cl		;make space
	mov	bx,ax		;copy
	and	bh,777/400	;trim to 9 bits
	xor	ax,bx		;isolate high 7
	or	dx,bx		;insert in MSW
icpy8:	mov	ds:fbcnt,ax	;save device size in bytes
	mov	ds:fbcnt+2,dx
	xor	ax,ax		;load 0
	mov	ds:iblk,ax	;init block #
	mov	ds:iblk+2,ax
	mov	ds:iblk+4,ax
	mov	ds:iblk+6,ax
	; get current date and time in case output is file
	mov	ax,ds:year	;get year
	mov	bx,word ptr ds:day ;month,,day
	mov	ds:fyear,ax	;save year
	mov	word ptr ds:fday,bx ;month,,day
	mov	byte ptr ds:fdatf,-1 ;date is valid
	mov	ax,word ptr ds:min ;get hour,,min
	mov	bl,ds:sec	;second
	mov	word ptr ds:fmin,ax ;save hour,,min
	mov	ds:fsec,bl	;second
	mov	byte ptr ds:ftimf,-1 ;time is valid
	mov	ax,offset iread	;pt at raw read routine
	jmp	short icpy12
icpy9:	cram	"?Can't read raw DOS disk",error
icpy10:	jmp	fnf
icpy11:	; find file (or first match if wildcard)
	mov	byte ptr ds:icont,0 ;assume not contig
	call	ss:dirget[bp]	;get next entry
	jc	icpy10		;no more, file not found
	jz	icpy11		;skip empty areas and dirs
	mov	si,offset fname1 ;pt at wildcard
	call	wild		;check for match
	jne	icpy11		;ignore
	call	ss:open[bp]	;open it
	mov	ax,ss:read[bp]	;get read routine
icpy12:	; AX=read routine, open output file or device
	push	ax		;save
	mov	bp,ds:outdev	;get ptr
	call	ss:dsrini[bp]	;init DSR
	mov	bx,2		;output dev
	call	ss:rstcwd[bp]	;restore CWD
	call	ss:dirout[bp]	;init dir I/O (required even if raw)
	test	byte ptr ds:fname3,-1 ;is there an output filename?
	jnz	icpy17		;yes, create the file
	; output is device
	cmp	ss:dirinp[bp],offset dosdi ;is it a DOS disk?
	je	icpy16
	test	byte ptr ds:secflg,-1 ;doing sector copy?
	jz	icpy14
	mov	al,ss:nsecs[bp]	;get # sectors
	mul	byte ptr ss:nhds[bp] ;* # heads
	mov	dl,ss:ncyls[bp]	;get # cyls
	xor	dh,dh		;DH=0
	mul	dx		;find total # of sectors
	; calculate actual size in bytes, may differ from image file size
	mov	bx,ax		;save low order
	mov	ax,ss:secsz[bp]	;get sector size
	mul	dx		;find high order
	xchg	ax,bx		;save, get low order sector count
	mul	word ptr ss:secsz[bp] ;multiply
	add	dx,bx		;add high order
	mov	bx,ss:med[bp]	;while we're here, get device type
	cmp	bx,flpbeg	;must be floppy for sector copy
	jb	icpy13
	cmp	bx,flpend
	jbe	icpy15
icpy13:	jmp	notflp		;isn't
icpy14:	mov	cl,9d		;shift count
	rol	ax,cl		;left 9 bits
	shl	dx,cl		;make space
	mov	bx,ax		;copy
	and	bh,777/400	;trim to 9 bits
	xor	ax,bx		;isolate high 7
	or	dx,bx		;insert in MSW
icpy15:	xor	ax,ax		;load 0
	mov	ds:oblk,ax	;init block #
	mov	ds:oblk+2,ax
	mov	bx,offset iwrite ;pt at raw write/write-last routines
	mov	cx,offset iwlast
	jmp	short icpy18
icpy16:	cram	"?Can't write raw DOS disk",error
icpy17:	; create file
	mov	si,offset fname3 ;pt at filename
	call	ss:create[bp]	;create the file
	mov	bx,ss:write[bp]	;get write routines
	mov	cx,ss:wlast[bp]
icpy18:	; actually copy the device
	pop	ax		;restore read routine ptr
	call	dcopy		;copy the device
	; close input and output
	test	byte ptr ds:fname1,-1 ;was there an input file?
	jz	icpy19		;no
	mov	bp,ds:indev	;get input device again
	call	ss:dsrini[bp]	;set up input dev
	xor	bx,bx		;select input dir
	call	ss:rstcwd[bp]
	call	ss:reset[bp]	;close input file
icpy19:	test	byte ptr ds:fname3,-1 ;was there an output file?
	jz	icpy20		;no
	mov	bp,ds:outdev	;get output dev
	call	ss:dsrini[bp]	;init
	mov	bx,2		;switch to output
	call	ss:rstcwd[bp]
	call	ss:close[bp]	;close output file
icpy20:	ret
;+
;
; Read device data.
;
; On entry:
; es:di	ptr to buf (normalized)
; cx	# bytes free in buf (FFFF if >64 KB)
; ss:bp	log dev rec
;
; On return:
; cx	# bytes really read (.LE. value on entry)
;
; CF=1 if nothing was read:
;  ZF=1 end of device
;  ZF=0 buffer not big enough to hold anything, needs to be flushed
;
;-
iread:	mov	bx,ds:isize	;get remaining size
	mov	ax,ds:isize+2	;check higher words
	or	ax,ds:isize+4
	or	ax,ds:isize+6
	jz	ird1		;zero, low word is right
	 mov	bx,-1		;say 64 K blks (or sectors) -1
ird1:	test	bx,bx		;end of device?
	jz	ird5		;yes, ZF=1
	test	byte ptr ds:secflg,-1 ;sector copy?
	jnz	ird6		;yes
	; logical block mode
	mov	al,ch		;get # full blocks free in buf
	shr	al,1
	jz	ird4		;nothing, say so
	cbw			;AH=0
	cmp	ax,bx		;more than what's left?
	jb	ird2
	 mov	ax,bx		;stop at end of dev
ird2:	mov	cx,ax		;copy block count
	mov	ax,ds:iblk	;get starting block
	mov	dx,ds:iblk+2
	push	cx		;save # blks requested
	push	di		;save ptr (in case of retry)
	call	ss:rdblk[bp]	;read data
;;; dammit, need 32 more bits of block number!
	pop	di		;[restore]
	pop	ax
	jc	ird7		;read error
ird3:	xor	bx,bx		;load 0
	add	ds:iblk,ax	;success, update block
	adc	ds:iblk+2,bx
	adc	ds:iblk+4,bx
	adc	ds:iblk+6,bx
	sub	ds:isize,ax	;update size
	sbb	ds:isize+2,bx	;CF=0 (already checked)
	sbb	ds:isize+4,bx
	sbb	ds:isize+6,bx
	ret
ird4:	or	al,1		;clear ZF (buf full)
ird5:	stc			;set CF
	ret
ird6:	jmp	short ird8
ird7:	; read error, make sure we get the good surrounding blocks
	shr	ax,1		;try for half that many blocks (before bad one)
	jnz	ird2		;unless it was only one block
	push	es		;save
	push	di
	push	ds		;copy DS to ES
	pop	es
	mov	di,offset lbuf	;point at buf
	cram	'%Bad block '
	mov	ax,ds:iblk	;fetch it
	mov	bx,ds:iblk+2
	mov	cx,ds:iblk+4
	mov	dx,ds:iblk+6
	call	pqnum
	cram	' on input device'
	call	flush
	pop	si		;restore (into ES:SI like RTRD etc.)
	pop	es
	mov	ax,1		;block count=1
	mov	cx,512d		;byte count=one block
	jmp	short ird3	;go return it
ird8:	; sector mode
	mov	ax,cx		;copy count
	xor	dx,dx		;zero-extend
	div	ss:secsz[bp]	;find # complete sectors that will fit in buf
	test	ax,ax		;space for anything?
	jz	ird4		;no
	cmp	ax,bx		;more than what's left?
	jb	ird9
	 mov	ax,bx		;stop at end of dev
ird9:	mov	si,di		;put buf addr in ES:SI
	mov	cx,ax		;copy sector count
	mov	ax,ds:iblk	;get starting sector #
	mov	dx,ds:iblk+2	;(fits in 32 bits in sector mode)
	mov	ds:flpvec,offset secxrd ;point at read routine
	call	secxfr		;read a track, or partial track if no space
	xor	bx,bx		;load 0
	add	ds:iblk,cx	;update block #
	adc	ds:iblk+2,bx
	adc	ds:iblk+4,bx
	adc	ds:iblk+6,bx
	sub	ds:isize,cx	;remove from count
	sbb	ds:isize+2,bx
	sbb	ds:isize+4,bx
	sbb	ds:isize+6,bx
	mov	ax,cx		;copy
	mul	ss:secsz[bp]	;find # bytes read
	mov	cx,ax		;in CX
	clc			;happy
	ret
;+
;
; Write to device.
;
; es:si	buffer
; cx	length of data
; ss:bp	log dev rec
;
; On return, cx contains the number of bytes actually written (presumably a
; multiple of the clustersize), .LE. the value on entry.
;
;-
iwrite:	test	byte ptr ds:secflg,-1 ;sector copy?
	jnz	iwr3		;yes
	mov	al,ch		;get # full 512-byte blocks
	shr	al,1
	jz	iwr1		;nothing, skip
	cbw			;AH=0
	push	cx		;save
	mov	cx,ax		;copy
	mov	ax,ds:oblk	;get starting block #
	mov	dx,ds:oblk+2
	add	ds:oblk,cx	;update
	adc	ds:oblk+2,0
	call	ss:wrblk[bp]	;write data
	pop	cx		;[restore]
	jc	iwr2		;failed
iwr1:	and	cx,not 777	;truncate byte count (wrote whole blks only)
	ret
iwr2:	jmp	wrerr		;error
iwr3:	; sector mode
	mov	ax,cx		;copy count
	xor	dx,dx		;zero-extend
	div	ss:secsz[bp]	;find # complete sectors that can be written
	test	ax,ax		;space for anything?
	jz	iwr4		;no
	mov	cx,ax		;copy sector count
	mov	ax,ds:oblk	;get starting sector #
	mov	dx,ds:oblk+2
	mov	ds:flpvec,offset secxwr ;point at write routine
	call	secxfr		;write a track, or partial track if no space
	add	ds:oblk,cx	;update block #
	adc	ds:oblk+2,0
	mov	ax,cx		;copy
	mul	ss:secsz[bp]	;find # bytes written
	mov	cx,ax		;in CX
	ret
iwr4:	xor	cx,cx		;can't write even one sector
	ret
;+
;
; Write last buffer of data to output device.
;
; es:si	string
; cx	length of data
; ss:bp	log dev rec
;
;-
iwlast:	jcxz	iwl1		;none, skip
	mov	bx,512d		;block size
	test	byte ptr ds:secflg,-1 ;sector copy?
	jz	iwl2		;no
	 mov	bx,ss:secsz[bp]	;yes, use sector size instead
iwl2:	sub	bx,cx		;find # NULs to add (in [1,SIZE))
	xchg	bx,cx		;swap
	lea	di,[bx+si]	;point past data
	xor	al,al		;load 0
	rep	stosb		;pad it out
	sub	di,si		;find length
	mov	cx,di		;copy
	jmp	iwrite		;write data, return
iwl1:	ret
;+
;
; Transfer sectors to/from floppy.
;
; We only try to transfer one track at a time.  If there's more space in the
; buffer, FCOPY will call right back anyway so there's no point in getting all
; tangled up maintaining the current block number and counts of the number of
; sectors transferred and the number left to go.
;
; dx:ax	starting sector #
; cx	# sectors to copy
; es:si	buffer (updated on return)
; FLPVEC points to single-sector read or write routine, called with:
;	es:si	buffer addr
;	FLPCYL	cylinder
;	FLPHD	head
;	cl	sector
;
; Returns:
; cx	actual # sectors transferred
;
;-
secxfr:	; transfer one sector at a time if not at beginning of track
	mov	bl,ss:nsecs[bp]	;get # sectors/track
	xor	bh,bh
	div	bx		;DL=sector-1, AX=cyl*nhds+head
	mov	di,ax		;save
	div	byte ptr ss:nhds[bp] ;AH=head, AL=cyl
	mov	ds:flphd,ah	;save
	mov	ds:flpcyl,al
	mov	ax,di		;get cyl*nhds+head again
	test	dl,dl		;at begn of track?
	jnz	secx1		;no, just do one sector at a time
	; also do one at a time if buf too small for whole track
	cmp	cx,bx		;>=1 track?
	jae	secx4		;yes
secx1:	; transfer one sector at a time until buf full or reached end of track
	mov	di,cx		;copy # to go
	inc	dx		;sectors start at 1, not 0
	xor	bx,bx		;init count of # actually done
	mov	cl,dl		;copy starting sector
secx2:	push	di		;save
	push	bx
	push	cx
	call	ds:flpvec	;read or write
	pop	cx		;[restore]
	pop	bx
	pop	di
	inc	bx		;count it
	inc	cx		;bump to next sector
	dec	di		;done all?
	jz	secx3		;yes
	cmp	cl,ss:nsecs[bp]	;more to do?
	jbe	secx2		;yes, loop until reach EOT or satisfy count
secx3:	mov	cx,bx		;copy # sectors transferred
	ret
secx4:	; transfer an entire track, doing the sectors out of order so as to get
	; 2:1 soft interleave, the data are transferred to and from the buffer
	; at the correct offsets so the interleave doesn't affect the data
	; transferred, just the speed (this is assuming we're too slow for 1:1
	; interleave, if this isn't true, which may well be the case these
	; days, then this is actually slower than the obvious approach! should
	; do some timing to find out)
	mov	cl,ss:nsecs[bp]	;get # sectors
	mul	word ptr ds:copskw ;figure out soft skew factor
	xor	ch,ch		;CH=0
	div	cx		;DL=starting sector (numbered starting at 0)
	mov	bl,dl		;copy
	push	es		;save
	push	ds		;copy DS to ES
	pop	es
	mov	di,offset buf	;pt at buf
	xor	ch,ch		;CX=# sectors
	xor	al,al		;load 0
	mov	dx,cx		;save
	rep	stosb		;nuke out table
	pop	es		;restore
	xor	bh,bh		;nuke high byte
secx5:	cmp	ds:buf[bx],bh	;have we done this one?
	jz	secx6		;no
	inc	bl		;+1 (wrapping to 0 not a prob)
	cmp	bl,ss:nsecs[bp]	;off end of track?
	jb	secx5		;no, check this loc
	sub	bl,ss:nsecs[bp]	;wrap
	jmp	short secx5
secx6:	not	byte ptr ds:buf[bx] ;set flag
	push	bx		;save
	push	dx
	push	si
	mov	cl,bl		;get sector
	inc	cx		;starts at 1
	mov	ax,ss:secsz[bp]	;get sector size
	mul	bx		;find byte offset from base of track
	add	si,ax		;index from curr ptr
	call	ds:flpvec	;transfer the sector
	pop	si		;restore
	pop	dx
	pop	bx
	dec	dx		;done all?
	jz	secx8		;yes
	add	bl,ds:copilv	;bump to next sector
	jc	secx7		;wrapped for sure
	cmp	bl,ss:nsecs[bp]	;off EOT?
	jb	secx5
secx7:	sub	bl,ss:nsecs[bp]	;wrap
	jmp	short secx5
secx8:	; finished transferring track
	mov	ax,ss:secsz[bp]	;get sector size
	mov	cl,ss:nsecs[bp]	;get # sectors read
	xor	ch,ch
	mul	cx		;compute size of track in bytes
	add	si,ax		;update ptr in case continuing
	ret
;+
;
; Read sector for SECXFR.
;
; ss:bp	log dev rec
; FLPCYL cyl #
; FLPHD	head #
; cl	sector #
; es:si	buffer pointer (updated on return)
;
;-
secxrd:	push	es		;save
	push	si
	push	cx
	push	ds		;copy DS to ES
	pop	es
	mov	ch,ds:flpcyl	;get cyl
	mov	dh,ds:flphd	;and head
	call	ss:rdsec[bp]	;read it
	jc	scxrd2
	pop	ax		;flush sector #
scxrd1:	pop	di		;restore buffer ptr
	pop	es
	mov	si,ds:dbuf	;point at buf
	mov	cx,ss:secsz[bp]	;get sector size
	shr	cx,1		;word count
	rep	movsw		;copy
	mov	si,di		;updated buf addr
	ret
scxrd2:	; read error, print message
	push	ds		;copy DS to ES
	pop	es
	mov	di,offset lbuf	;point at buf
	cram	'%Read error, track '
	mov	al,ds:flpcyl	;print cyl
	call	pbyte
	cmp	ss:nhds[bp],2	;double sided?
	jb	scxrd3		;no
	cram	', head '
	mov	al,ds:flphd	;print head
	call	pbyte
scxrd3:	cram	', sector '
	pop	ax		;print sector
	call	pbyte
	call	flush
	jmp	short scxrd1
;+
;
; Write sector for SECXFR.
;
; ss:bp	log dev rec
; FLPCYL cyl #
; FLPHD	head #
; cl	sector #
; es:si	buffer pointer (updated on return)
;
;-
secxwr:	push	es		;save
	push	cx		;save sector #
	mov	di,ds:dbuf	;point at buf
	mov	ax,ds		;get segs
	mov	bx,es
	mov	ds,bx		;swap
	mov	es,ax
	mov	cx,ss:secsz[bp]	;get sector size
	shr	cx,1		;/2
	rep	movsw		;copy into buf
	mov	ds,ax		;restore
	pop	cx
	mov	ch,ds:flpcyl	;get cyl
	mov	dh,ds:flphd	;and head
	cmp	ch,ss:ncyls[bp]	;off end of disk?
	jae	scxwr1		;punt if so (wrote all that would fit already)
	push	si		;save pointer
	call	ss:wrsec[bp]	;write it
	jc	scxwr2		;error
	pop	si		;restore
	pop	es
	ret
scxwr1:	jmp	odsmal
scxwr2:	mov	di,offset lbuf	;point at buf
	cram	'%Write error, track '
	mov	al,ds:flpcyl	;print cyl
	call	pbyte
	cmp	ss:nhds[bp],2	;double sided?
	jb	scxwr3		;no
	cram	', head '
	mov	al,ds:flphd	;print head
	call	pbyte
scxwr3:	cram	', sector '
	pop	ax		;print sector
	call	pbyte
	call	flush
	pop	si		;restore
	pop	es
	ret
;+
;
; Copy a file or device.
;
; DS:INDEV ptr to input log dev rec
; DS:OUTDEV ptr to output log dev rec
;
; For copying devices, enter at DCOPY with:
; ax	ptr to READ routine
; bx	ptr to WRITE routine
; cx	ptr to WLAST routine
;
;-
fcopy:	mov	bp,ds:indev	;get input dev
	mov	ax,ss:read[bp]	;get read routine
	mov	bp,ds:outdev	;get output dev
	mov	bx,ss:write[bp]	;get write routines
	mov	cx,ss:wlast[bp]
dcopy:	; enter here with AX, BX, CX set up
	mov	ds:aread,ax	;save
	mov	ds:awrite,bx
	mov	ds:awlast,cx
	mov	byte ptr ds:eof,0 ;not EOF yet
	mov	ax,ds:bigbuf	;get ptr to buf
	mov	ds:bigptr+2,ax	;save
	mov	ds:bigptr,0	;offset=0
	mov	ax,ds:bigsiz	;get size of buf
	mov	ds:bigctr,ax
	mov	ax,ds:bigsiz+2
	mov	ds:bigctr+2,ax
fcpy1:	; fill buffer
	mov	bp,ds:indev	;get input dev
	call	ss:dsrini[bp]	;select it
	xor	bx,bx		;select dir
	call	ss:rstcwd[bp]
fcpy2:	; do next read
	mov	cx,ds:bigctr	;get # bytes left
	cmp	ds:bigctr+2,0	;is it more than 64 KB?
	jz	fcpy3
	 mov	cx,-1		;stop just short
fcpy3:	les	di,dword ptr ds:bigptr ;get ptr
	mov	ax,cx		;copy
	add	ax,di		;will this wrap around?
	jnc	fcpy4		;no
	mov	cx,di		;copy
	not	cx		;# bytes until EOB-1 (so ADD won't overflow)
fcpy4:	call	ds:aread	;read as much as we can
	jc	fcpy5		;EOF or EOB, skip
	sub	ds:bigctr,cx	;eat this chunk
	sbb	ds:bigctr+2,0
	add	cx,ds:bigptr	;advance ptr (don't carry!)
	mov	ax,cx		;copy
	and	cx,0Fh		;normalize ptr
	mov	ds:bigptr,cx
	mov	cl,4		;shift count
	shr	ax,cl		;get # whole pars advanced
	add	ds:bigptr+2,ax	;update
	jmp	short fcpy2	;scarf more
fcpy5:	jnz	fcpy6		;skip
	inc	ds:eof		;set EOF flag
fcpy6:	; empty buffer as much as possible
	mov	bp,ss:outdev	;get output dev rec
	call	ss:dsrini[bp]	;select it
	mov	bx,2		;select dir too
	call	ss:rstcwd[bp]
	mov	ax,ds:bigsiz	;get size of buffer
	mov	dx,ds:bigsiz+2
	sub	ax,ds:bigctr	;-amount free = amount used
	sbb	dx,ds:bigctr+2
	mov	ds:bigctr,ax	;save
	mov	ds:bigctr+2,dx
	mov	ax,ds:bigbuf	;reinit ptr
	mov	ds:bigptr+2,ax
	mov	ds:bigptr,0	;offset=0
fcpy7:	; do next write
	mov	cx,ds:bigctr	;get # bytes left
	cmp	ds:bigctr+2,0	;is it more than 64 KB?
	jz	fcpy8
	 mov	cx,-1		;stop just short
fcpy8:	jcxz	fcpy10		;EOF
	les	si,dword ptr ds:bigptr ;get ptr
	mov	ax,cx		;copy
	add	ax,si		;will this wrap around?
	jnc	fcpy9		;no
	mov	cx,si		;copy
	not	cx		;# bytes until EOB-1
fcpy9:	call	ds:awrite	;write as much as we can
	jcxz	fcpy10		;can't, skip
	sub	ds:bigctr,cx	;eat this chunk
	sbb	ds:bigctr+2,0
	add	cx,ds:bigptr	;advance ptr
	mov	ax,cx		;copy
	and	cx,0Fh		;normalize
	mov	ds:bigptr,cx
	mov	cl,4		;shift count
	shr	ax,cl		;get # whole pars advanced
	add	ds:bigptr+2,ax	;update
	jmp	short fcpy7	;write more
fcpy10:	; copy what's left back to begn of buffer so we can read more
	; (assume all clustersizes<(64 KB-16.) so we can do it in one go)
	mov	cx,ds:bigctr	;get low word of length
	push	ds		;save
	mov	es,ds:bigbuf	;set up dest
	xor	di,di
	lds	si,dword ptr ds:bigptr ;get source
	mov	bx,cx		;save length
	shr	cx,1		;/2 to get wc
	rep	movsw		;[copy words]
	rcl	cl,1		;catch CF
	rep	movsb		;copy odd byte if any
	pop	ds		;restore
	cmp	byte ptr ds:eof,0 ;is that it?
	jnz	fcpy11		;yes
	mov	ax,di		;copy ptr
	and	di,0Fh		;offset
	mov	ds:bigptr,di	;save
	mov	cl,4		;bit count
	shr	ax,cl		;find offset in paragraphs
	add	ax,ds:bigbuf	;add base
	mov	ds:bigptr+2,ax	;save segment
	mov	ax,ds:bigsiz	;get size of buf
	mov	dx,ds:bigsiz+2
	sub	ax,bx		;find # free bytes
	sbb	dx,0
	mov	ds:bigctr,ax	;save
	mov	ds:bigctr+2,dx
	jmp	fcpy1		;refill buffer
fcpy11:	; end of file, force whatever's left out (CX=byte count)
	mov	cx,bx		;get count again
	mov	es,ds:bigbuf	;pt at beginning
	xor	si,si
	call	ds:awlast	;write last buffer to disk
	push	ds		;copy DS to ES
	pop	es
	ret
;+
;
; Delete file(s).
;
;-
delete:	mov	byte ptr ds:noqry,0 ;init flags
	mov	byte ptr ds:ovride,0
	call	delswt		;check for switches
	; get filespec
	call	gdev		;parse device name, get dev rec in bp
	call	ss:defset[bp]	;set defaults
	mov	di,offset fname1 ;pt at buffer
	xor	bl,bl		;no implied wildcards
	call	ss:gfile[bp]	;parse dir/filename
	call	delswt		;check for switches
	call	confrm		;make sure EOL
	call	ss:dsrini[bp]	;init DSR
	call	ss:dirinp[bp]	;init dir I/O
	mov	byte ptr ds:notfnd,1 ;nothing found yet
	; init fake output device
	mov	ds:indev,bp	;set input device
	mov	ds:outdev,bp	;it's both really, doesn't matter
del1:	; check next file
	call	ss:dirget[bp]	;get next entry
	jc	del4		;no more
	jz	del1		;skip empty areas
	mov	si,offset fname1 ;pt at wildcard
	push	di		;save
	call	wild		;check for match
	pop	di		;[restore]
	jne	del1		;ignore
	; it's a match, type filename
	mov	byte ptr ds:notfnd,0 ;found one
	cmp	byte ptr ds:noqry,0 ;should we ask before deleting?
	jnz	del2		;no
	mov	byte ptr ds:lnum,4 ;don't **MORE** (query, error, prompt, +1)
	mov	ax," ?"		;'? '
	stosw
	call	flush1		;print prompt
	call	yorn		;yes or no?
	jc	del1		;loop if no
	jmp	short del3
del2:	call	flush		;display filename
del3:	; delete the file
	call	ss:delfil[bp]	;delete it
	jmp	short del1	;look for more
del4:	; no more
	call	ss:dirfin[bp]	;finish up dir I/O
	cmp	byte ptr ds:notfnd,0 ;find anything?
	jnz	del6
	ret
del5:	jmp	dirio		;dir I/O err
del6:	jmp	fnf		;file not found
;+
;
; Handle DELETE switches.
;
;-
delswt:	call	skip		;skip white space
	jc	delsw3
	cmp	al,ds:swchar	;switch?
	jne	delsw3
	inc	si		;eat the /
	dec	cx
	call	getw		;get the switch
	jc	delsw1
	mov	ax,offset delsws ;point at switch list
	call	tbluk		;look it up
	jc	delsw2
	call	ax		;call the routine
	jmp	short delswt	;check for more
delsw1:	jmp	synerr		;missing switch
delsw2:	jmp	badswt		;bad switch
delsw3:	ret
;
delsws	label	byte
	kw	<NOQ-UERY>,delnoq ;/NOQUERY (don't prompt before deleting)
	kw	<OVERRIDE->,delovr ;/OVERRIDE (override protection)
	db	0
;
delnoq:	mov	byte ptr ds:noqry,1 ;set flag
	ret
;
delovr:	mov	byte ptr ds:ovride,1 ;set flag
	ret
;+
;
; Directory of file(s).
;
;-
dir:	mov	byte ptr ds:dirflg,0 ;no flags set
	call	dirswt		;handle switches
	; get filename
	call	gdev		;parse device name, get dev rec in BP
	call	ss:defset[bp]	;set defaults
	mov	di,offset fname1 ;pt at buffer
	mov	bl,1		;implied wildcards
	call	ss:gfile[bp]	;parse dir/filename
	call	dirswt		;handle switches
	call	confrm		;make sure confirmed
	call	ss:dsrini[bp]	;init DSR (switch drives etc.)
	; print banner
	call	pvol		;print volume name (leave DI=LBUF)
	cram	' Directory of '
	call	pdev		;print dev name
	mov	si,offset fname1 ;pt at buffer
	cmp	byte ptr [si],0	;is there a wildcard?
	jnz	dir1		;yes, good
	 mov	si,offset wldcrd ;no, use "*.*"
dir1:	call	ss:pfile[bp]	;print filespec
	call	flush		;print line
	call	flush		;blank line
	call	ss:dirhdr[bp]	;print header
	call	ss:dirinp[bp]	;init dir input
	mov	byte ptr ds:notfnd,1 ;nothing found yet
dir2:	; process next file
	call	ss:dirget[bp]	;get next entry
	jc	dir5		;end of list
	jnz	dir3		;not .EMPTY.
	; empty area
	cmp	byte ptr ds:fname1,0 ;do we have a filename?
	jnz	dir2		;yes, skip empties
	call	ss:diremp[bp]	;display it
	call	flush		;flush it
	jmp	short dir2
dir3:	cmp	byte ptr ds:fname1,0 ;do we have a wildcard?
	jz	dir4		;no
	; check to see if wildcard match
	push	di		;save output line ptr
	mov	si,offset fname1 ;pt at 1st
	call	wild		;does it?
	pop	di		;[restore]
	jne	dir2		;no, skip
dir4:	mov	byte ptr ds:notfnd,0 ;found one
	call	ss:dirdis[bp]	;display file
	; end of line
	call	flush		;flush it
	jmp	short dir2	;around for more
dir5:	cmp	byte ptr ds:notfnd,0 ;find anything?
	jz	dir6
	 jmp	fnf		;no
dir6:	call	ss:dirsum[bp]	;print summary
	call	ss:dirfin[bp]	;finish up
	mov	di,offset lbuf	;blank line
	callr	flush
;+
;
; Handle DIRECTORY switches.
;
;-
dirswt:	call	skip		;skip white space
	jc	dirsw3
	cmp	al,ds:swchar	;switch?
	jne	dirsw3
	inc	si		;eat the /
	dec	cx
	call	getw		;get the switch
	jc	dirsw1
	mov	ax,offset dirsws ;point at switch list
	call	tbluk		;look it up
	jc	dirsw2
	call	ax		;call the routine
	jmp	short dirswt	;check for more
dirsw1:	jmp	synerr		;missing switch
dirsw2:	jmp	badswt		;bad switch
dirsw3:	ret
;
dirsws	label	byte
	kw	<F-ULL>,dirfu	;/FULL (verbose listing)
	db	0
;
dirfu:	; /FULL
	or	byte ptr ds:dirflg,200 ;show everything
	ret
;+
;
; DISMOUNT ddu: [/UNLOAD]
;
; Dismount mounted logical device.
;
;-
dismnt:	mov	al,ds:dsmunl	;get /[NO]UNLOAD default
	mov	byte ptr ds:unlswt,al ;init flag
	call	dmtswt		;handle switches
	call	gdevn		;look up device
	call	dmtswt		;handle switches
	call	confrm		;should be EOL
	cmp	ss:dirinp[bp],offset dosdi ;is it a PC-DOS disk?
	je	dsmt1		;yes, no good (no permanent dev record anyway)
	mov	al,ds:unlswt	;get switch
	jmp	short dmnt	;dismount it, return
dsmt1:	cram	"?Can't dismount DOS disk",error
;
dmtswt:	; handle DISMOUNT switches
	call	skip		;skip white space
	jc	dmtsw3
	cmp	al,ds:swchar	;switch?
	jne	dmtsw3
	inc	si		;eat the /
	dec	cx
	call	getw		;get the switch
	jc	dmtsw1
	mov	ax,offset dmtsws ;point at switch list
	call	tbluk		;look it up
	jc	dmtsw2
	mov	ds:unlswt,al	;save value
	jmp	short dmtswt	;check for more
dmtsw1:	jmp	synerr		;missing switch
dmtsw2:	jmp	badswt		;bad switch
dmtsw3:	ret
;
dmtsws	label	byte
	kw	<NOUN-LOAD>,0
	kw	<UN-LOAD>,1
	db	0
;+
;
; Dismount device whose logical dev record is at SS:BP.
;
; al	NZ => unload volume (if supported)
;
;-
dmnt:	cmp	ss:hwtype[bp],hwfile ;image file?
	jne	dmnt1
	mov	bx,ss:handle[bp] ;yes, get handle
	mov	ah,3Eh		;func=close
	int	21h
dmnt1:	cmp	ss:hwtype[bp],hwaspi ;ASPI?
	jne	dmnt2		;no
	test	al,al		;/UNLOAD?
	jz	dmnt2		;no
	mov	ds:ssunt1,ds	;set seg addr of BUF
	mov	si,offset ssunt	;pt at cmd to start/stop drive
	mov	cx,lssunt
	call	aspcmd		;send cmd
	jnc	dmnt2		;success
	cram	'?Error ejecting disk',error
dmnt2:	; unlink from dev list
	mov	ax,ss:next[bp]	;get link
	mov	bx,ds:logdev	;get head of list
	cmp	bx,bp		;is this it?
	je	dmnt4		;easy
dmnt3:	; find it on logical dev list
	mov	si,bx		;save ptr
	mov	bx,ds:next[bx]	;follow link
	cmp	bx,bp		;is this it?
	jne	dmnt3		;loop if not
	mov	ds:next[si],ax	;unlink
	jmp	short dmnt5
dmnt4:	; this rec was first on list, change back to DOS disk
	mov	ds:logdev,ax	;unlink
	mov	al,ds:dosdsk	;get DOS disk
	mov	ds:curdsk,al	;make it the logged-in device
dmnt5:	jmp	retrec		;flush record, return
;
if 1 ;;; playing with Zip disks
;
wprot:	call	gdevn		;look up device
	call	confrm		;should be EOL
	cmp	ss:hwtype[bp],hwaspi ;ASPI?
	jne	wprot1		;no
	mov	ds:wpzip1,ds	;set seg addr of BUF
	mov	si,offset wprots
	mov	cx,wprotl
	mov	ds:wpzipl,cx	;save length
	mov	ds:wpzips,cl
	mov	di,offset buf
	rep	movsb		;copy into buf
	mov	si,offset wpzip	;command to write prot/enable Zip
	mov	cx,lwpzip
	call	aspcmd		;send cmd
	jnc	wprot1		;success
	cram	'?Error diddling Zip disk',error
wprot1:	ret
;
;wprots	db	'APlaceForYourStuff'
;wprots	db	'GoToHell'
wprots	db	'FOO'
wprotl=	$-wprots
;
endif
;+
;
; DUMP dev:nnnn
;
; Dump block nnnn (octal) in octal and sixbit.
;
;-
dump:	call	gdev		;get dev name
	test	word ptr ss:fsys[bp],-1 ;doesn't work for native file system
	jz	dump3
	call	getn		;get a number
	cmp	word ptr ss:fsys[bp],rt11 ;RT-11?
	je	dump1
	cmp	word ptr ss:fsys[bp],os8 ;OS/8?
	jne	dump2
dump1:	add	ax,ss:curdir[bp] ;yes, add base of current dir
	adc	dx,ss:curdir+2[bp]
dump2:	mov	cx,1		;read 1 block
	mov	di,offset buf	;pt at temp buf
	call	ss:rdblk[bp]	;get the block
	jnc	dump4
dump3:	jmp	rderr
dump4:	xor	ax,ax		;offset
dump5:	; do next line
	push	ax		;save
	mov	di,offset lbuf	;pt at buf
	mov	cx,3		;digit count
	call	poct		;print offset
	mov	ax," /"		;'/ '
	stosw
	cmp	byte ptr ss:wsize[bp],12d ;12-bit device?
	je	dump10		;yes, display as appropriate
	mov	cx,4		;loop count
dump6:	; dump words in octal
	push	cx		;save
	lodsw			;get a word
	mov	cx,6		;digit count
	call	poct		;write it
	mov	al,' '		;blank
	stosb
	pop	cx		;restore
	loop	dump6		;loop
	stosb			;add another blank
	sub	si,8d		;back to begn
	mov	cl,8d		;reload counter
dump7:	; dump ASCII bytes
	lodsb			;get a byte
	and	al,177		;trim to 7 bits
	cmp	al,' '		;printing char?
	jb	dump8
	cmp	al,177
	jb	dump9
dump8:	mov	al,'.'		;no, change to dot
dump9:	stosb			;save
	loop	dump7		;loop
	jmp	short dump15
dump10:	; 12-bit mode
	mov	cx,4		;loop count
dump11:	; dump words in octal
	push	cx		;save
	lodsw			;get a word
	mov	cx,4		;digit count
	call	poct		;write it
	mov	al,' '		;blank
	stosb
	pop	cx		;restore
	loop	dump11		;loop
	stosb			;add another blank
	sub	si,8d		;back to begn
	mov	cl,4		;reload counter
dump12:	; dump TEXT characters
	lodsw			;get 2 chars
	add	ax,ax		;left 2
	add	ax,ax
	mov	al,ah		;get the char
	call	dump17		;display it
	mov	al,[si-2]	;low char
	call	dump17
	loop	dump12		;loop
if 0 ; dump TSS/8 ASCII (AAAAAAAABBBB BBBBCCCCCCCC => ABC)
	mov	al,' '		;blank
	stosb
	stosb
	sub	si,8d		;back to begn
	mov	cl,2		;reload once again
dump13:	push	cx		;save
	lodsw			;get a word
	mov	cl,4		;shift count
	shr	ax,cl		;right-justify first char
	call	dump19		;save it
	mov	al,[si-2]	;get prev low 4 bits
	shl	al,cl		;left-justify
	or	al,[si+1]	;high 4 bits of next word
	call	dump19		;save
	lodsw			;low 8 bits of next word
	call	dump19		;save
	pop	cx		;restore
	loop	dump13
	mov	al,' '		;blank
	stosb
	sub	si,8d		;back to begn
	mov	cl,2		;reload once again
dump14:	; same thing in opposite order (1st byte will be wrong for sure)
	push	cx		;save
	mov	cl,4		;shift count
	mov	al,[si-2]	;get prev low 4 bits
	shl	al,cl		;left-justify
	or	al,[si+1]	;high 4 bits of this word
	call	dump19		;save
	lodsw			;low 8 bits of this word
	call	dump19		;save
	lodsw			;get next word
	shr	ax,cl		;right-justify first char
	call	dump19		;save it
	pop	cx		;restore
	loop	dump14
endif ;;; 1
dump15:	push	si		;save
	call	flush		;flush line
	pop	si		;restore
	pop	ax
	add	ax,4		;bump by 4 words
	cmp	byte ptr ss:wsize[bp],12d ;12-bit dev?
	je	dump16
	 add	ax,4		;no, skip to next 8. bytes
dump16:	cmp	si,offset buf+512d ;done all of block?
	je	dump18		;yes, return
	jmp	dump5		;loop
dump17:	; display TEXT char in AL
	and	al,77		;isolate
	mov	ah,al		;copy
	sub	ah,40		;get sign bits
	and	ah,100		;the one we want
	or	al,ah		;set it (or not)
	stosb
dump18:	ret
if 0 ;;;
dump19:	and	al,177		;isolate low 7 bits
	cmp	al,177		;RUBOUT?
	je	dump20
	cmp	al,' '		;ctrl char?
	jae	dump21
dump20:	mov	al,'.'		;one or the other, change to dot
dump21:	stosb			;save
	ret
endif ;;; 1
;+
;
; Exit.
;
;-
exit:	; dismount all devices
	mov	bp,ds:logdev	;get head of list
	test	bp,bp		;anything?
	jz	exit1
	mov	al,ds:dsmunl	;get /[NO]UNLOAD default
	call	dmnt		;dismount it
	jmp	short exit	;handle new head of list, until none
exit1:	; reset floppy system
	xor	dl,dl		;say drive 0
	xor	ah,ah		;func=reset
	int	13h
	int	20h		;exit
;+
;
; FORMAT dev: /switches
; (see GMOUNT for switches)
;
;-
format:	mov	bx,-1		;don't get logical name
	mov	ds:othswt,offset inisw ;"INIT" switch table
	mov	ds:nsegs,0	;(use default if no /SEGMENTS:n switch)
	call	gmount		;parse stuff, get temp rec at SS:BP
	call	crtdev		;create device (instead of OPNDEV)
	cmp	ss:hwtype[bp],hwfile ;image file?
	jne	fmt3
	mov	ax,ss:med[bp]	;get medium type
	test	ax,ax		;defaulted?
	jz	fmt1
	cmp	ax,mscp		;MSCP?
	jne	fmt2
fmt1:	; prompt for size of image file
	cram	'File size (bytes): ',wrng
	call	gtlin		;get line
	call	gtsize		;read size of image
	and	ax,not 777	;trim to 512 bytes
	mov	bx,ax		;copy DX:AX to CX:BX
	mov	cx,dx
	call	setsiz		;set up dev size
	jmp	short fmt3
fmt2:	; if image file of floppy, see if it should be interleaved
	cmp	ax,flpbeg	;is it a floppy?
	jb	fmt3
	cmp	ax,flpend
	ja	fmt3
	 call	askilv		;yes, decide whether to interleave
fmt3:	cmp	ss:hwtype[bp],hwflop ;floppy?
	je	fmt4
	cmp	ss:hwtype[bp],hwcwsl ;Catweasel floppy?
	jne	fmt7
fmt4:	test	ss:med[bp],-1	;yes, medium type specified?
	jnz	fmt7
	mov	bl,ss:drive[bp]	;get drive #
	xor	bh,bh
	mov	al,ds:fdtype[bx] ;look up drive type
	test	al,al		;known?
	jz	fmt5		;no, assume they have at least some plan
	cmp	al,rx33		;1.2MB drive?
	jne	fmt6		;no
fmt5:	mov	al,rx50		;otherwise default to RX50
fmt6:	xor	ah,ah		;AH=0
	mov	ss:med[bp],ax	;save
fmt7:	call	setmed		;set medium-related stuff
	call	setsys		;set file system related stuff
fmt8:	; format volume
	mov	bx,ss:hwtype[bp] ;get hardware type
	call	ds:fmthw[bx]	;dispatch (format the volume)

;;; should do DLBAD/DMBAD as a separate step (dev-independent)

;;; then add null BAD.TSK style bad blk list in last good data block

	; do file-system-specific software init
	test	ss:fsys[bp],-1	;did they specify a file system?
	jz	fmt9		;no, don't try to write a blank one
	; write blank file system (if specified)
	mov	bx,ss:fsys[bp]	;get file system
	mov	si,offset inisys ;pt at table
	call	wdluk		;look it up
	jc	fmt9
	 call	ax		;init
fmt9:	cmp	ss:hwtype[bp],hwflop ;floppy drive?
	je	fmt10
	cmp	ss:hwtype[bp],hwcwsl ;Catweasel floppy drive?
	jne	fmt11
fmt10:	; prompt to format more if floppy
	cram	'Format more disks (Y/N)? ',wrng
	call	gtlin		;read a line
	call	getw		;get their answer
	jc	fmt10		;if any
	cmp	byte ptr [bx],'Y' ;does it start with 'Y'?
	je	fmt8
fmt11:	ret
;
_=0
fmthw	label	word
	disp	hwfile,fmtimg	;format image file
	disp	hwflop,fmtflp	;format floppy
	disp	hwcwsl,fmtflp	;format Catweasel floppy
	disp	hwpart,fmthdp	;format hard disk partition
	disp	hwtu58,cret	;format TU58 (nothing to do)
;+
;
; Format image file.
;
;-
fmtimg:	mov	ax,ss:totsiz[bp] ;get total size
	mov	dx,ss:totsiz+2[bp]
	sub	ax,ds:badsec	;subtract bad sector file info from SETMED
	sbb	dx,ds:badsec+2
	mov	ds:fbcnt,ax	;save
	mov	ds:fbcnt+2,dx
	; write FBCNT zeros
	mov	es,ds:bigbuf	;point at buffer
	xor	di,di		;point at beginning
	mov	cx,ds:bigsiz	;get size
	cmp	ds:bigsiz+2,di	;is that it?
	jz	fmti1
	 mov	cx,-1		;no, >64 KB so stop at end
fmti1:	mov	ds:bigctr,cx	;save length
	xor	ax,ax		;load 0
	shr	cx,1		;/2
	rep	stosw		;clear out begn of buf
	adc	cl,cl		;get CF back
	rep	stosb		;clear odd byte
fmti2:	; write next buffer of zeros
	mov	es,ds:bigbuf	;pt at buffer
	mov	cx,ds:bigctr	;get # bytes in buf, or 64 KB-1
	test	ds:fbcnt+2,-1	;almost done?
	jnz	fmti3		;no
	cmp	ds:fbcnt,cx	;yes, should we truncate?
	jae	fmti3
	 mov	cx,ds:fbcnt	;yes
fmti3:	jcxz	fmti4		;nothing to write, done
	mov	bx,ss:handle[bp] ;get file handle
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	xor	dx,dx		;offset=0
	mov	ah,40h		;func=write
	int	21h
	pop	ds		;[restore]
	jc	fmti7		;error
	cmp	ax,cx		;did we write all?
	jne	fmti7
	sub	ds:fbcnt,ax	;subtract
	sbb	ds:fbcnt+2,0
	jmp	short fmti2	;loop
fmti4:	; all done with zeros, add bad block file if any
	xor	di,di		;point at begn of buf
	mov	cx,ds:bigctr	;get # bytes in first page of buf
	; ES:DI=byte after last byte to be written, CX=# bytes free there
	; add bad block file if appropriate
	mov	bx,ds:tmed	;get medium type
	mov	si,offset filbad ;pt at table
	call	wdluk		;look it up
	jc	fmti5
	 call	ax		;add it, update DI
fmti5:	mov	cx,di		;copy
	jcxz	fmti6		;nothing to write, skip
	mov	bx,ss:handle[bp] ;get file handle
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	xor	dx,dx		;offset=0
	mov	ah,40h		;func=write
	int	21h
	pop	ds		;[restore]
	jc	fmti7		;error
	cmp	ax,cx		;did we write all?
	jne	fmti7
fmti6:	push	ds		;restore original ES
	pop	es
	ret
fmti7:	jmp	wrerr		;write error
;
if 0
;;;; save this for CREATE
fmtimg:	; create image file (replace bp with new temp record)
	mov	ds:indev,bp	;save temp rec ptr
	mov	si,ds:tfnam	;get ptr to filename
	mov	cx,ds:tfnam+2	;length
	call	gdev		;parse device name
	mov	ds:outdev,bp	;save dev rec
	call	ss:defset[bp]	;set defaults
	mov	di,offset fname1 ;pt at buffer
	xor	bl,bl		;no implied wildcards
	call	ss:gfile[bp]	;parse dir/filename
	call	confrm		;make sure EOL
	; say what we're doing
	mov	di,offset lbuf	;pt at buf
	cram	'Creating image file '
	call	pdev		;print dev name
	mov	cx,1		;filename follows directory name
	call	ss:dirnam[bp]	;print dir name
	mov	si,offset fname1 ;pt at buffer
fmti1:	lodsb			;get a byte
	test	al,al		;end?
	jz	fmti2
	stosb			;store
	jmp	short fmti1
fmti2:	call	flush		;print line
	call	aysure		;see if they're sure
;;; this should be rigged for DOS, foreign container files are too confusing
	mov	ax,ds:tfsys	;get file system (not defaulted)
	mov	ss:fsys[bp],ax	;save it
;;;
	; determine length
	xor	ax,ax		;clear for now
	mov	ds:fbcnt,ax
	mov	ds:fbcnt+2,ax
	mov	ds:badsec,ax	;no bad sector file
	mov	ds:badsec+2,ax
	mov	bx,ds:tmed	;get medium type
	mov	si,offset filfmt ;pt at table
	call	wdluk		;look it up
	jc	fmti3
	call	ax		;go figure length, set up for bad sec file
	jmp	short fmti4
fmti3:	; unknown, ask them
	cram	'File size (bytes): ',wrng
	call	gtlin		;get line
	call	getn		;read number
	and	ax,not 777	;trim to 512 bytes
fmti4:	mov	ds:fbcnt,ax	;save
	mov	ds:fbcnt+2,dx
	; init DEVSIZ
	mov	cx,ss:devsiz[bp] ;DEVSIZ initted?
	or	cx,ss:devsiz+2[bp]
;;; when wouldn't it be?  this may be needless now
	jnz	fmti5		;yes, skip it
	mov	ss:totsiz[bp],ax ;set total size in bytes
	mov	ss:totsiz+2[bp],dx
	mov	cl,9d		;bit count
	shr	ax,cl		;convert to blocks
	ror	dx,cl
	mov	cx,dx		;copy
	and	cl,200		;isolate high 9 bits
	or	ax,cx		;compose low word
	mov	ss:devsiz[bp],ax
	and	dx,177		;low 7 bits
	mov	ss:devsiz+2[bp],dx
	mov	ss:totblk[bp],ax ;set total size in blocks
	mov	ss:totblk+2[bp],dx
fmti5:	; get creation date
	mov	ax,ds:year	;copy date
	mov	ds:fyear,ax
	mov	ax,word ptr ds:day
	mov	word ptr ds:fday,ax
	mov	ax,word ptr ds:min ;time
	mov	word ptr ds:fmin,ax
	mov	al,ds:sec
	mov	ds:fsec,al
	mov	word ptr ds:ftimf,-1 ;mark both as valid
	; open the file
	mov	bp,ds:outdev	;make sure we still have bp
	call	ss:dsrini[bp]	;init DSR
	call	ss:dirinp[bp]	;init dir I/O
	call	ss:dirout[bp]	;both ways (in case different)
	mov	si,offset fname1 ;pt at filename
	call	ss:create[bp]	;create the file
	; remove space reserved for bad sector file
	mov	ax,ds:fbcnt	;fetch
	mov	dx,ds:fbcnt+2
	sub	ax,ds:badsec	;subtract
	sbb	dx,ds:badsec+2
	mov	ds:fbcnt,ax	;save
	mov	ds:fbcnt+2,dx
	; write FBCNT zeros
	mov	ds:binflg,bin	;binary mode
	mov	es,ds:bigbuf	;point at buffer
	xor	di,di		;point at beginning
	mov	cx,ds:bigsiz	;get size
	cmp	ds:bigsiz+2,di	;is that it?
	jz	fmti6
	 mov	cx,-1		;no, >64 KB so stop at end
fmti6:	mov	ds:bigctr,cx	;save length
	xor	ax,ax		;load 0
	shr	cx,1		;/2
	rep	stosw		;clear out begn of buf
	adc	cl,cl		;get CF back
	rep	stosb		;clear odd byte
fmti7:	; write next buffer of zeros
	mov	es,ds:bigbuf	;pt at buffer
	xor	si,si
	mov	cx,ds:bigctr	;get # bytes in buf
	cmp	ds:fbcnt+2,si	;almost done?
	jnz	fmti8		;no
	cmp	ds:fbcnt,cx	;yes, should we truncate?
	jae	fmti8
	mov	cx,ds:fbcnt	;yes
fmti8:	jcxz	fmti9		;nothing to write, done
	call	ss:write[bp]	;write a buffer
	jcxz	fmti9		;must be just about done, go finish up
	sub	ds:fbcnt,cx	;subtract
	sbb	ds:fbcnt+2,0
	jmp	short fmti7	;loop
fmti9:	; wrote all except possibly one partial block, add bad block file
	xor	si,si		;point at data
	cmp	ds:fbcnt+2,si	;<64 KB left right?
	jnz	fmti11		;we're bugged if not
	mov	di,ds:fbcnt	;get # bytes we need to write
	mov	cx,ds:bigctr	;get # bytes in first page of buf
	sub	cx,di		;find # free
	; ES:DI=byte after last byte to be written, CX=# bytes free there
	; add bad block file if appropriate
	mov	bx,ds:tmed	;get medium type
	mov	si,offset filbad ;pt at table
	call	wdluk		;look it up
	jc	fmti10
	call	ax		;add it, update di
fmti10:	xor	si,si		;point at begn
	mov	cx,di		;get length
	push	cx		;save
	call	ss:write[bp]	;write it
	mov	si,cx		;skip whatever couldn't be written
	pop	cx		;catch desired length
	sub	cx,si		;find # bytes still to go
	call	ss:wlast[bp]	;should have fit, well pad it anyway
	push	ds		;restore ES
	pop	es
	call	ss:close[bp]	;close file
	call	ss:dirfin[bp]	;clean up
	mov	bp,ds:indev	;restore original BP
	ret
fmti11:	jmp	wrerr		;BIGBUF must be too small for cluster, ugh
endif ;;;; 0
;+
;
; Prompt to see if they want a block or sector image file.
;
;-
askilv:	mov	ax,ds:tintlv	;get /INTERLEAVE flag
	mov	byte ptr ss:ilimg[bp],al ;assume sector image, or what they say
	test	ah,ah		;did they say anything?
	jnz	askil1		;yes, don't prompt
	cram	'Create (B)lock or (S)ector image file? [S] ',wrng
	call	gtlin		;read a line
	call	getw		;get their answer
	jc	askil1		;if any
	cmp	byte ptr [bx],'S' ;does it start with 'S'?
	je	askil1
	cmp	byte ptr [bx],'B' ;how about 'B'?
	jne	askilv
	inc	ss:ilimg[bp]	;block-by-block
askil1:	ret
;
; List of routines to construct bad block table.
;
filbad	dw	rl01,dlbad	;RL01
	dw	rl02,dlbad	;RL02
	dw	rk06,dmbad	;RK06
	dw	rk07,dmbad	;RK07
	dw	rm02,dmbad	;RM02
	dw	rm03,dmbad	;RM03
	dw	rm05,dmbad	;RM05
	dw	rm80,dmbad	;RM80
	dw	rp04,dmbad	;RP04
	dw	rp05,dmbad	;RP05
	dw	rp06,dmbad	;RP06
	dw	rp07,dmbad	;RP07
	dw	0
;
dlbad:	; construct RL01 bad sector file (ES:DI points to buf, updated)
	; N.B. RSTS 7.0 DSKINT is bugged, displays SN as 32-bit octal #
	; instead of skipping bit 15 of each word.  9.7 is OK though.
	cmp	cx,40d*256d	;enough space for bad block file?
	jb	membad		;no, we lose
	push	es		;save
	push	di
	push	ds		;copy DS to ES
	pop	es
	cram	'Serial # [1234512345]: ',wrng ;prompt for serial #
	call	gtlin		;get a line
	call	getw		;get a word
	jc	dlbad2
	call	cvto		;get a number
	sal	ax,1		;left a bit
	rcl	dx,1		;into dx
	shr	ax,1		;fix, clear b15
	and	dh,177		;clear b15
dlbad1:	; pack SN in DX:AX
	pop	di		;point at free space
	pop	es
	mov	si,di		;save a copy
	stosw			;save low word
	mov	ax,dx		;high word
	stosw
	xor	ax,ax		;load 0
	stosw			;(reserved)
	stosw			;data pack (not alignment pack)
	dec	ax		;all ones
	mov	cx,(126d*2)+(128d*2) ;pad out, 18-bit version is all ones
	rep	stosw		;(different from other last-track disks which
				;expect valid 18-bit version too -- but there
				;was never an 18-bit controller for RLs)
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	mov	cx,9d*256d*2	;# words to copy (9 more block pairs)
	rep	movsw
	pop	ds		;restore
	ret
dlbad2:	mov	ax,12345	;default
	mov	dx,ax
	jmp	short dlbad1
membad:	cram	'?Out of memory writing bad block file',error
;
dmbad:	; construct RK06 bad sector file (ES:DI points to buf, updated)
	; (should be OK for any last-track device)
	cmp	cx,ds:badsec	;enough space for bad block file?
	jb	membad		;no, we lose
	push	es		;save
	push	di
	push	ds		;copy DS to ES
	pop	es
	cram	'Serial # [12345]: ',wrng ;prompt for serial #
	call	gtlin		;get a line
	call	getw		;get a word
	jc	dmbad2
	call	cvto		;get a number
dmbad1:	; pack SN in AX
	pop	di		;point at free space
	pop	es
	mov	si,di		;save a copy
	stosw			;save SN
	xor	ax,ax		;load 0
	stosw			;(reserved, high word of SN on RLs)
	stosw			;(reserved)
	stosw			;data pack (not alignment pack)
	dec	ax		;all ones
	mov	cx,(126d*2)	;pad out
	rep	stosw
	mov	cx,ds:badsec	;# bytes in last track
	sub	ch,512d/400	;minus the block we already did
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	rep	movsb		;fill the rest (16/18-bit vers are the same)
	pop	ds		;restore
	ret
dmbad2:	mov	ax,12345	;default
	jmp	short dmbad1
dmbad3:	jmp	membad
;+
;
; Format floppy.
;
; ss:bp	log dev rec
;
;-
fmtflp:	call	prsent		;wait until they're ready
	call	ss:dsrini[bp]	;init for I/O
	mov	word ptr ds:flphd,0 ;init head, cyl #
	; set up FLPVEC to give special handling for DECmate 5.25" disks
	mov	ax,offset fdzrt	;normally use 1:1
	mov	bx,ss:med[bp]	;get medium type
	cmp	bx,rx50		;RX50 or RX52?
	je	fflp1
	cmp	bx,rx52
	jne	fflp3
fflp1:	mov	bx,ss:fsys[bp]	;get file system
	cmp	bx,os8		;OS/8 etc.?
	je	fflp2
	cmp	bx,ps8
	je	fflp2
	cmp	bx,putr
	je	fflp2
	cmp	bx,cos310
	jne	fflp3
fflp2:	mov	ax,offset fdzos	;interleave depends on track #
fflp3:	mov	ds:flpvec,ax	;save
fflp4:	; format next track
	call	ptrkhd		;display track (and head if DS)
	mov	al,ds:flpcyl	;get cyl #
	call	ds:flpvec	;compute hard interleave table for this track
	mov	di,ds:dbuf	;pt at buffer
	push	si		;save
	mov	cl,ss:nsecs[bp]	;loop count
	xor	ch,ch
fflp5:	; write C,H,R,N values for each sector
	mov	ax,word ptr ds:flphd ;get head,,cyl
	xchg	al,ah		;><
	stosw			;save C, H
	lodsb			;R is from table
	mov	ah,ds:fdlen	;N is whatever we set
	stosw
	loop	fflp5
	mov	dh,ds:flphd	;get head
	mov	ch,ds:flpcyl	;and cyl
	call	fdftk		;format track
	jc	fflp7
	pop	si		;restore ptr to interleave table
	call	vtrack		;verify
	cmp	ss:nhds[bp],2	;double sided?
	jb	fflp6
	xor	byte ptr ds:flphd,1 ;yes, flip head #
	jnz	fflp4		;go do side 1 of this cyl
fflp6:	inc	ds:flpcyl	;go to next cyl
	mov	al,ds:flpcyl	;get it
	cmp	al,ss:ncyls[bp]	;done all?
	jb	fflp4		;loop if not
	callr	fmtcmp		;print msg, return
fflp7:	jmp	fderr		;go decode error
;+
;
; RX50 format interleave routines.
;
; Each of these routines is called once before each track is formated, with the
; cylinder number in AL of the track about to be formatted.  They should return
; a pointer in si to a 10-byte list of sector numbers, in the order they should
; be written on the disk.
;
;-
fdzrt:	; format RX50 for RT-11
	mov	si,offset ilv11	;always 1:1, driver does 2:1 in software
	ret
;
fdzos:	; format RX50 for OS/8
	; 1:1 except tracks 0, 78, and 79, which are 2:1
	test	al,al		;track 0?
	jz	fdzos2
	cmp	al,78d		;78. or above?
	jae	fdzos1
	; tracks 1-77, start 2 sectors later each track
	mov	si,offset ostk1	;pt at starting posn
	dec	ax		;-1 (start pattern at track 1)
	cbw			;AH=0
	mov	bl,5		;get track-1 mod 5
	div	bl		;into ah
	mov	al,ah		;get remainder
	cbw			;AH=0
	sal	al,1		;*2
	sub	si,ax		;back up
	ret
fdzos1:	ja	fdzos3		;79., skip
fdzos2:	; track 0 or 78., use 2:1 interleave
	mov	si,offset ostk78 ;pt at table
	ret
fdzos3:	mov	si,offset ostk79 ;pt at table
	ret
;
	db	    3,4,5,6,7,8d,9d,10d
ostk1	db	1,2,3,4,5,6,7,8d,9d,10d ;1:1 2-sector skew tracks 1-77.
ostk78	db	1,6,2,7,3,8d,4,9d,5,10d ;2:1 for 0 and 78.
ostk79	db	5,10d,1,6,2,7,3,8d,4,9d ;2:1, 2-sector skew from track 78.
;+
;
; Print cyl # from FLPCYL, and head from FLPHD if double sided.
;
; bp	dev rec ptr
;
;-
ptrkhd:	mov	di,offset lbuf	;pt at buf
	cram	'Track '
	mov	al,ds:flpcyl	;get curr cyl #
	call	pbyte		;convert to decimal
	cmp	ss:nhds[bp],2	;double-sided?
	jb	ptkhd1		;no
	cram	' Head '
	mov	al,ds:flphd	;get head #
	call	pbyte		;decimal
ptkhd1:	mov	al,cr		;cr
	stosb
	inc	ds:lnum		;don't count this because no LF
	jmp	flush1		;display, return
;+
;
; Print "Format complete" message
;
;-
fmtcmp:	mov	di,offset lbuf	;pt at buf
	cram	'Format complete'
	jmp	flush		;display, return
;+
;
; Verify a track.
;
; si	ptr to interleave table used to format the track
;
;-
vtrack:	; build table of sectors which we've already formated
	mov	di,offset buf	;pt at BUF
	mov	cl,ds:fdeot	;get track length
	mov	dl,cl		;copy
	mov	dh,cl		;again
	xor	ch,ch		;CH=0
	mov	al,ch		;AL too
	rep	stosb		;clear flags
	xor	bx,bx		;starting offset
vtk1:	; find next unread sector (2:1 soft interleave)
	inc	bx		;+1 (will be +2 in a sec)
	jmp	short vtk3
vtk2:	cmp	ds:buf[bx],bh	;have we done this one?
	jz	vtk4		;no, do it
vtk3:	inc	bx		;+1
	cmp	bl,dh		;off end of track?
	jb	vtk2
	sub	bl,dh		;yes, wrap
	jmp	short vtk2
vtk4:	inc	ds:buf[bx]	;set flag
	push	bx		;save
	push	dx
	push	si
	mov	dh,ds:flphd	;get head #
	mov	cl,[bx+si]	;sec #
	mov	ch,ds:flpcyl	;cyl #
	call	fdrds		;read sector (with retries)
	jc	vtk5		;error, punt
	pop	si		;restore
	pop	dx
	pop	bx
	dec	dl		;done?
	jnz	vtk1		;loop if not
	ret
vtk5:	call	error		;punt
verr1	db	verr2,lf,'?Verify error'
verr2=	$-verr1-1
;+
;
; "Format" a hard disk partition.
;
;-
fmthdp:	cram	"?Don't know how to do HDs yet",error
;+
;
; INIT dev /FILESYSTEM /MEDIUM
;
; Wipe all files from the volume, initialize it for use.
;
;-
init:	mov	bx,-1		;don't get logical name
	mov	ds:othswt,offset inisw ;"INIT" switch table
	mov	ds:nsegs,0	;(use default if no /SEGMENTS:n switch)
	call	gmount		;parse stuff, get temp rec at SS:BP
	call	opndev		;actually open the device
	call	defmed		;figure out medium defaults if needed
	call	setmed		;set medium-related stuff
	call	setsys		;set file system related stuff
	; write blank filesystem
	mov	bx,ss:fsys[bp]	;get file system
	test	bx,bx		;any?
	jz	init1
	mov	si,offset inisys ;pt at table
	call	wdluk		;look it up
	jc	init2		;not found
	callr	ax		;init, return
init1:	cram	'?Must specify file system type',error
init2:	cram	"?Don't know how to init that filesystem",error
;
inisw	label	byte
	kw	<SEG-MENTS>,iniseg ;set # segments in RT-11 directory
	db	0
;
inisys	dw	os8,zos		;init OS/8 disk/DECtape
	dw	ps8,zos		;init PS/8 DECtape
	dw	putr,zos	;init TSS/8 PUTR DECtape
	dw	rt11,zrt	;init RT-11 disk
	dw	0
;+
;
; Zero a disk with OS/8 file structure.
;
; bp	log dev rec
;
;-
zos:	call	ss:dsrini[bp]	;init I/O
	; get # info words per entry
	mov	di,offset lbuf	;pt at buf
	cram	'Number of info words per dir entry [1]: '
	call	flush1
	call	gtlin		;get a line
	call	getw		;get a word
	jc	zos1
	call	cvto		;get a number
	jmp	short zos2
zos1:	; default # info words is 1 (for date)
	mov	ax,1		;default is 1
	cwd			;DX=0
zos2:	test	dx,dx		;DX must be 0
	jnz	zos3
	cmp	ax,256d-5	;at least one entry must fit in dir seg
	jbe	zos4
zos3:	jmp	zos8		;invalid # of info words
zos4:	mov	cx,10000	;negate
	sub	cx,ax
	and	ch,7777/400	;might be 0
	push	cx		;save
	mov	di,offset lbuf	;pt at buf
	cram	'Allocate space for system [N]? '
	call	flush1
	call	yorn		;get response
	pop	cx		;[restore]
	mov	dx,7		;[start of data area for data disk]
	jc	zos5
	 mov	dl,70		;start of data area for system disk
zos5:	; ensure that device size is in range
	mov	ax,ss:devsiz+6[bp] ;make sure size .LT. 64 K blocks
	or	ax,ss:devsiz+4[bp]
	or	ax,ss:devsiz+2[bp]
	jnz	zos7		;nope
	cmp	ss:devsiz[bp],dx ;space for dir plus at least one data blk?
	jbe	zos7		;(utterly useless if not)
	; init home block
	mov	di,offset buf	;point at buffer
	mov	si,di		;save a copy
	mov	ax,7777		;0000/ one entry in seg
	stosw
	mov	ax,dx		;0001/ starting blk # of empty area
	stosw
	xor	ax,ax		;0002/ next dir seg (none)
	stosw
	stosw			;0003/ no tentative file
	mov	ax,cx		;0004/ -(# info words)
	stosw
	xor	ax,ax		;0005/ .EMPTY. block
	stosw
	mov	ax,dx		;get starting block
	sub	ax,ss:devsiz[bp] ;subtract device size (get -(size of .EMPTY.))
	and	ah,7777/400	;trim to 12 bits
	stosw			;0006/ -length
	xor	ax,ax		;might as well clear out rest
	mov	cx,256d-7	;first 7 words already done
	rep	stosw
	; write directory in block 1 (si=ptr to buf)
	mov	ax,1		;home block is block 1
	cwd
	mov	cx,ax		;write 1 block
	add	ax,ss:curdir[bp] ;add base of partition
	adc	dx,ss:curdir+2[bp]
	call	ss:wrblk[bp]	;go
	jc	zos6
;;; write dummy boot block
	ret
zos6:	jmp	wrerr
zos7:	cram	'?Invalid device size',error
zos8:	cram	'?Invalid # of info words',error
;+
;
; Zero a disk with RT-11 file structure.
;
; bp	log dev rec
;
;-
zrt:	call	ss:dsrini[bp]	;init I/O
	; ensure that device size is in range
	cmp	ss:cursiz[bp],6+2+1 ;space for boot + one dir seg + one blk?
	jae	zrt1		;(utterly useless if not)
	cram	'?Invalid device size',error
zrt1:	; init home block
	mov	bx,offset buf	;pt at disk buffer
	mov	di,bx		;copy
	mov	cx,1000/2	;word count
	xor	ax,ax		;load 0's
	rep	stosw		;nuke it
	lea	di,[bx+722]	;pt at cluster size
	inc	ax		;(HB+722) cluster size is always 1
	stosw
	mov	al,6		;(HB+724) dir starts at 6 (regardless of this)
	stosw
	mov	ax,107123	;(HB+726) version=.RAD50 /V05/
	stosw
	mov	ax,"  "		;blanks
	mov	cx,3*12d/2	;byte count (pad all 3 text fields)
	rep	stosw
	; get volume label
	mov	di,offset lbuf	;pt at buf
	cram	'Volume ID <RT11A>: '
rt11a=	$-8d
	call	flush1
	call	gtlin		;read a line
	mov	di,offset buf+730 ;pt at vol ID
	call	skip		;did user enter anything?
	jc	zrt4
	mov	dx,12d		;max field size
zrt2:	lodsb			;get a byte
	cmp	al,' '		;ctrl char?
	jb	zrt3		;(ZF=0)
	stosb			;save
	dec	dx		;count
zrt3:	loopnz	zrt2		;loop until full or done
	jmp	short zrt5
zrt4:	mov	si,offset rt11a	;pt at string
	mov	cx,5		;count
	rep	movsb
zrt5:	; get owner name
	mov	di,offset lbuf	;pt at buf
	cram	'Owner name: '
	call	flush1
	call	gtlin		;read a line
	mov	di,offset buf+744 ;pt at owner name
	call	skip		;did user enter anything?
	jc	zrt8		;no
	mov	dx,12d		;max field size
zrt6:	lodsb			;get a byte
	cmp	al,' '		;ctrl char?
	jb	zrt7		;(ZF=0)
	stosb			;save
	dec	dx		;count
zrt7:	loopnz	zrt6		;loop until full or done
zrt8:	mov	di,offset buf+760 ;system ID
	cram	'DECRT11A'
	mov	si,offset buf	;pt at buf
	mov	cx,(1000/2)-1	;word count, except for first and last words
	xor	bx,bx		;init sum
zrt9:	lodsw			;get next
	add	bx,ax		;add it in
	loop	zrt9		;loop
	neg	bx		;negate
	mov	[si],bx		;poke it in
	; write home block
	mov	si,offset buf	;pt at buffer
	mov	ax,ss:curdir[bp] ;get base of partition
	mov	dx,ss:curdir+2[bp]
	add	ax,1		;home block is block 1
	adc	dx,0
	mov	cx,1		;write 1 block
	call	ss:wrblk[bp]	;go
	; decide how many dir segments to use
	mov	dx,ds:nsegs	;get value from /SEGMENTS:n switch
	test	dx,dx		;if any
	jnz	zrt11		;yes, use it
	mov	si,offset rtnseg ;pt at # of segments table
zrt10:	lodsw			;get size
	mov	bx,ax		;copy
	lodsw			;get # segments
	mov	dx,ax		;take it
	cmp	bx,ss:cursiz[bp] ;< actual size?
	jb	zrt10		;loop if so
zrt11:	; write a null directory
	mov	di,offset buf	;pt at buffer
	xor	ax,ax		;load 0
	mov	cx,1000/2	;WC
	rep	stosw		;nuke it
	mov	di,offset buf	;pt at begn again
	; write 5-word segment header
	mov	ax,dx		;get # segs
	stosw			;word 1 -- total # segments
	xor	ax,ax		;no next seg
	stosw			;word 2 -- ptr to next segment in chain
	inc	ax		;1
	stosw			;word 3 -- # of highest segment in use
	xor	ax,ax		;0
	stosw			;word 4 -- # of extra words per entry
	mov	ax,dx		;get # segs
	sal	ax,1		;*2 = # blocks in directory
	add	ax,6		;add starting block #
	stosw			;word 5 -- starting data blk for this seg
	; write < UNUSED > entry
	mov	ax,1000		;type=< UNUSED >
	stosw
	mov	ax,23747	;.RAD50/FOO/
	stosw			;filename is meaningless
	stosw
	stosw
	mov	ax,ss:cursiz[bp] ;get size of partition
	sub	ax,word ptr ds:buf+8d ;find amount left for files
	stosw			;length
	xor	ax,ax		;job/chan info
	stosw
	stosw			;date is meaningless on empties
	mov	ax,4000		;end-of-segment marker
	stosw
	mov	si,offset buf	;pt at buffer
	mov	ax,ss:curdir[bp] ;get base of partition
	mov	dx,ss:curdir+2[bp]
	add	ax,6		;first dir block
	adc	dx,0
	mov	cx,1		;write 1 block
	call	ss:wrblk[bp]	;go
;;; write dummy boot block
	ret
;
; Table from RT-11 V04.00 SUG:
;
; dev	mnem	segs	total blocks
; RX01	DX	1	494
; RX01	PD	1	494
; TU58	DD	1	512
; TU56	DT	1	578	N.B. SUG is *wrong*, DUP correctly writes 4
; RX02	DY	4	988
; RF11	RF	4	1024
; RS03/4 DS	4	1024/2048
; RK05	RK	16	4800
; RL01	DL	16	10240/20480
; RK07	DM	31	x/53790
; RP02	DP	31	40000
; RP03	DP	31	40000 per half (each drive works as two RT-11 units)
;
; Sizes from Tony Konashenok <avk@hafnium.cchem.berkeley.edu> (who got them out
; of RT-11 V5.4D DUP.SAV), confirmed by Bob Schor <bschor@vms.cis.pitt.edu>
; (who got them by experimenting with DUP)
rtnseg	dw	512d	;Up to 512 blocks
	dw	1	;we get 1 segment.
	dw	2048d	;Up to 2048 blocks
	dw	4	;we get 4 segments.
	dw	12288d	;Up to 12288 blocks
	dw	16d	;we get 16 segments.
	dw	-1	;Over that
	dw	31d	;we get 31 segments (max allowed).
;
iniseg:	; INIT /SEGMENTS:n switch
	jcxz	insg2		;can't be EOL
	lodsb			;eat the ':'
	dec	cx		;count it off
	cmp	al,':'		;must be ':'
	je	insg1
	cmp	al,'='		;or '='
	jne	insg3
insg1:	call	getn		;get number
	dec	ax		;change 1 to 0
	cmp	ax,31d-1	;in range [1,31.]?
	ja	insg4		;no
	inc	ax		;yes, fix it
	mov	ds:nsegs,ax	;save
	ret
insg2:	jmp	misprm		;missing parameter
insg3:	jmp	synerr		;syntax error
insg4:	jmp	outran		;number out of range
;+
;
; MOUNT log: [filespec]/struc/medium
;
; log: is logical device name (of the form d[d][n]:)
; filespec is name of disk image file (if not real disk)
; /STRUC is the file structure (e.g. /OS8, /RT11, default is to auto-detect)
; /MEDIUM is the medium type (default is to auto-detect)
;
; Possible commands:
; MOUNT A: /OS8 (defaults to A:, RX50)
; MOUNT DZ0: A: /OS8
; MOUNT DY0: B: /RX02 /RT11
; MOUNT DX0: A: /RX01 /RT11
; MOUNT DL0: C:SY.DSK /RT11
; MOUNT DU0: /DRIVE=0 /BPART=2 (doesn't work yet)
; MOUNT DD0: COM1:19200/DRIVE=1
; MOUNT X: SCSI5: /MSCP/RT11/PART:1
;
;-
mount:	xor	bx,bx		;get logical dev name
	mov	ds:othswt,0	;no "other" switches
	call	gmount		;get MOUNT command line
	push	word ptr ds:fremem ;save free memory pointer
				;(so MNTSYS code can alloc dir bufs)
	call	opndev		;actually open the device
	call	defmed		;figure out medium defaults if needed
	call	setmed		;set medium-related stuff
	call	defsys		;figure out file system if needed
	call	setsys		;set file system related stuff
	; do file-system-specific init
	mov	bx,ss:fsys[bp]	;get it
	mov	si,offset mntsys ;pt at table
	call	wdluk		;look it up
	jc	mount1
	 call	ax
mount1:	; we'll succeed, so make sure MLOOP doesn't close file or flush rec
	pop	word ptr ds:kepmem ;make sure we don't flush the dev rec's RAM
	mov	ds:tmpdev,0	;tell MLOOP not to return it
	mov	ds:outhnd,-1	;and don't close the image file, if any
	; link logical device record into table second (so as not to log in)
	mov	bx,ds:logdev	;get head of list
	test	bx,bx		;is there anything?
	jz	mount2
	mov	ax,bp		;yes, copy
	xchg	ax,ds:next[bx]	;link us in, fetch rest of list
	mov	ss:next[bp],ax	;link it to us
	ret
mount2:	mov	ss:next[bp],bx	;rest of list is empty
	mov	ds:logdev,bp	;just us
	ret
;
mnthd:	; mount hard drive partition
;;;; should get geometry
;;;;
	ret
;
; mount file system (once hardware is set up)
mntsys	dw	cos310,mntos	;COS-310
	dw	dosbat,mntdb	;DOS/BATCH
	dw	ods1,mntod	;Files-11 ODS-1
	dw	os8,mntos	;OS/8
	dw	ps8,mntos	;PS/8
	dw	putr,mntos	;TSS/8 PUTR
	dw	rsts,mntrs	;RSTS
	dw	rt11,mntrt	;RT11
	dw	tss8,mntts	;TSS/8.24
	dw	xxdp,mntxx
	dw	0
;
mntdb:	; mount DOS/BATCH file system
	mov	ax,101h		;UIC=[1,1]
	mov	ss:curdir[bp],ax ;set current dir
	mov	ss:actdir[bp],ax ;active dir too
	call	dbdi		;init for dir input
	call	dbfd		;find this dir
	mov	ss:cursiz[bp],cx ;save size of dir ents
	mov	word ptr ss:dirbuf[bp],0 ;unlink dir bufs so MLOOP won't touch
				;(after dir buf memory flushed by MOUNT)
	ret
;
mntod:	; mount Files-11 ODS-1 file system
	call	odhome		;find home block
	jnc	mntod1
	 jmp	rderr		;failed
mntod1:	;; save DX:AX (home block LBN) somewhere if we need to
	mov	dx,[si+2]	;get H.IBLB (used to be just 24 bits)
	mov	ax,[si+4]	;(VBN 3 of INDEXF.SYS, start of header bitmap)
	mov	ss:indmap[bp],ax ;save
	mov	ss:indmap+2[bp],dx
	mov	cx,[si]		;get H.IBSZ (# blks in header bitmap, was byte)
	mov	ss:indbsz[bp],cx ;save
	add	ax,cx		;add that in to get to first file header
	adc	dx,0
	mov	ss:filhdr[bp],ax ;save (addr of first 16 file headers)
	mov	ss:filhdr+2[bp],dx

	mov	ax,[si+10]	;get H.SBCL (storage bitmap cluster factor)
	mov	ss:pcs[bp],ax	;equivalent to RSTS PCS, use same variable
	mov	ax,[si+14]	;get H.VLEV (ODS volume structure level)
	mov	ss:rdslev[bp],ax
	mov	ax,[si+44]	;get H.DFPR (default file protection bits)
	mov	ss:defprt[bp],ax
	mov	al,[si+55]	;get H.FIEX (default file extension value)
	cmp	al,1		;must be NZ for extending INDEXF.SYS and dirs
	sbb	ah,ah		;all ones if was 0
	and	ah,16d		;say 16. if field is 0
	or	al,ah
	xor	ah,ah		;zero-extend
	mov	ss:filext[bp],ax


	mov	ax,(36*50+36)*50+36 ;^R000 (36 is .RAD50 for zero)
	mov	ss:curdir[bp],ax ;CWD=[0,0]
	mov	ss:curdir+2[bp],ax
	xor	ax,ax		;load 0
	mov	ss:curdir+4[bp],ax ;finish off .RAD50 /xxxyyyzzz/
	mov	ss:curdir+6[bp],ax ;end of path
	mov	ss:satptr[bp],ax ;start allocating at begn
	mov	ss:satptr+2[bp],ax

	ret
;
mntos:	; mount OS/8 (or COS-310) file system
	call	ospart		;set up partition
	jc	mntos1		;failed
	ret
mntos1:	jmp	nxpart
;
ospart:	; set up OS/8 partition selected by TPART
	xor	ax,ax		;load 0
	mov	ss:curdir[bp],ax ;log in to root (no LD:s)
	mov	ss:curdir+2[bp],ax
	mov	ax,7777		;assume max size
	cmp	word ptr ss:med[bp],rk05 ;only matters for RK05s
	jne	osprt1		;skip it, all set
	mov	ax,3200d	;# blocks in an OS/8 RK05 partition
	mov	cx,ds:tpart	;get partition #
	cmp	cx,2		;must be 0 or 1
	jae	osprt2		;isn't
	jcxz	osprt1		;part 0, already there
	 mov	ss:curdir[bp],ax ;change starting addr
osprt1:	mov	ss:cursiz[bp],ax ;set size of partition
	clc			;happy
	ret
osprt2:	stc			;bad partition #
	ret
;
mntrs:	; mount RSTS file system
	; calculate device cluster size (smallest power of 2 for 16-bit clu #s)
	mov	ax,1		;assume DCS=1
	mov	bx,ss:devsiz+2[bp] ;get high order of dev size
	mov	cx,ss:devsiz+4[bp]
	mov	dx,ss:devsiz+6[bp]
mtrs1:	; see how many times we have to divide the dev size by 2 to make the
	; size fit in 16 bits
	mov	si,bx		;see if high bits are all 0
	or	si,cx
	or	si,dx
	jz	mtrs2		;they are, we're done
	shr	dx,1		;high-order size right a bit
	rcr	cx,1
	rcr	bx,1
	add	ax,ax		;DCS left a bit
	cmp	ax,64d		;<=64. right?
	jbe	mtrs1		;yes, keep looking
	cram	'?Invalid DCS',error
mtrs2:	mov	ss:dcs[bp],ax	;save DCS
	; read pack label (in MFD if RDS 0.0)
	call	ss:dsrini[bp]	;init DSR
	mov	di,ds:dbuf	;pt at buffer
	mov	ax,1		;home block
	mov	cx,ax		;read 1 block
	mul	ss:dcs[bp]	;starting at DCN 1
	call	ss:rdblk[bp]	;fetch it
	mov	si,ds:dbuf	;get ptr to home block
	mov	ax,[si+12]	;get pack status word
	mov	ss:paksts[bp],ax ;save (for NFF check in RSCL)
	mov	di,offset lbuf	;pt at buf
	cram	'Pack ID='
	mov	ax,[si+14]	;get pack ID again
	call	radnb
	mov	ax,[si+16]
	call	radnb
	cram	', PCS='
	mov	ax,[si+10]	;get pack clustersize
	mov	ss:pcs[bp],ax	;save
	mov	bx,ax		;copy
	dec	bx		;-1
	and	bx,ax		;make sure power of 2
	jnz	mtrs3		;nope
	cmp	ax,64d		;.LE.64?
	ja	mtrs3
	cmp	ax,ss:dcs[bp]	;PCS.GE.DCS?
	jb	mtrs3		;nope, invalid
	push	ax		;yes, save
	xor	dx,dx		;zero-extend
	div	word ptr ss:dcs[bp] ;find # DCs per PC
	mov	ss:pcsdcs[bp],ax ;save
	pop	ax		;restore
	call	pbyte		;convert it
	cram	', DCS='
	mov	ax,ss:dcs[bp]	;get DCS
	call	pbyte
	cram	', RDS level '
	test	byte ptr [si+13],40 ;new (V8.0+) pack?
	jnz	mtrs4
	; RDS 0.0
	xor	ax,ax		;RDS 0.0
	jmp	short mtrs5
mtrs3:	cram	'?Invalid PCS',error
mtrs4:	; RDS 1.X
	mov	ax,[si+4]	;DCN of MFD
	mov	ss:mfddcn[bp],ax ;save
	mov	ax,[si+6]	;get RDS level
mtrs5:	; RDS level AH.AL
	mov	ss:rdslev[bp],ax ;set in log dev rec
	push	ax		;save
	mov	al,ah
	call	pbyte		;print major version
	mov	al,'.'		;.
	stosb
	pop	ax		;restore
	call	pbyte
	call	flush		;flush
	mov	ax,1		;load 1
	mov	ss:curdir[bp],ax ;PPN=[0,1]
	mov	ss:actdir[bp],ax ;active dir too (for RSGD)
	mov	ss:satptr[bp],ax ;init SATT.SYS cluster alloc ptr
	; compute # of PCs on disk
	mov	ax,ss:devsiz[bp] ;get dev size (known to fit in 32 bits)
	mov	dx,ss:devsiz+2[bp] ;get high order of dev size
	sub	ax,ss:dcs[bp]	;remove DCN 0
	sbb	dx,0
	div	word ptr ss:pcs[bp] ;find # PCs (remainder in DX is wasted)
	mov	ss:numclu[bp],ax ;save
	; look up [0,1]SATT.SYS
	xor	cx,cx		;no command line
	call	rsgd		;find [0,1] dir
	call	rsdi		;init for dir input
mtrs6:	call	rsdg		;get next dir entry
	jc	mtrs10		;shouldn't get this far
	mov	si,offset satsys ;point at SATT.SYS filename
	mov	cx,9d		;LEN('SATT.SYS'<0>)
	repe	cmpsb		;is this it?
	jne	mtrs6		;loop if not
	; make sure size is correct for the # of PCNs on this pack
	cmp	ds:fsize+2,cx	;MSW of size should be 0 (CX=0 from REPE CMPSB)
	jnz	mtrs10		;error if not
	mov	ax,ss:numclu[bp] ;get # PCs on disk
	add	ax,(512d*8d)-1	;round to next block boundary
	rcr	ax,1		;catch carry, if any
	mov	cl,(9d+3)-1	;shift count, minus the one we already did
	shr	ax,cl		;find # blocks needed to map all PCs
	cmp	ds:fsize,ax	;LSW of SATT.SYS size is big enough?
	jb	mtrs10		;no
	; get SATT.SYS retrieval list
	mov	ax,ds:iretr	;get first retrieval pointer
	call	rslink		;follow it
	jc	mtrs10		;null!
	mov	ax,[si+2]	;get first retrieval pointer
	mul	word ptr ss:dcs[bp] ;find starting block of SATT.SYS
	mov	ss:bitmap[bp],ax ;save
	mov	ss:bitmap+2[bp],dx
	mov	ax,ds:ifcs	;get file cluster size
	xor	dx,dx		;DX=0
	div	word ptr ss:dcs[bp] ;find # DCs per FC
	mov	di,ax		;save
	mov	dx,[si+2]	;restore first retr pointer
	sub	dx,di		;back up
	; make sure SATT.SYS is contiguous
mtrs7:	mov	cx,8d-1		;# of retrieval pointers left
	lodsw			;get link to next
	mov	bx,ax		;copy
mtrs8:	lodsw			;get next DCN from retrieval list
	add	dx,di		;find expected DCN
	cmp	ax,dx		;correct?
	jne	mtrs10		;no
	mov	ax,ds:ifcs	;get SATT.SYS FCS
	sub	ds:fsize,ax	;count off the cluster
	jbe	mtrs9		;done, file checks out OK
	loop	mtrs8		;handle next
	mov	ax,bx		;copy link word
	push	dx		;save
	push	di
	call	rslink		;follow it
	pop	di		;[restore]
	pop	dx
	jnc	mtrs7		;loop if OK
	jmp	short mtrs10
mtrs9:	mov	word ptr ss:dirbuf[bp],0 ;unlink dir bufs so MLOOP won't touch
				;(after dir buf memory flushed by MOUNT)
	ret
mtrs10:	cram	'?Missing or invalid [0,1]SATT.SYS',error
;
mntrt:	; mount RT11 file system
	call	rtpart		;set up partition
	jc	nxpart		;failed
	ret
nxpart:	cram	'?Nonexistent partition',error
;
rtpart:	; set up partition selected by TPART
	mov	ss:curdir[bp],0	;log in to root (no LD:s)
	mov	ax,ds:tpart	;get partition # (default=0)
	mov	ss:curdir+2[bp],ax ;save
	mov	bx,ss:devsiz+4[bp] ;see if dev is huge
	or	bx,ss:devsiz+6[bp]
	jnz	rtprt2		;yes, all partitions are full size
	cmp	ax,ss:devsiz+2[bp] ;is this last partition?
	jb	rtprt2		;no
	ja	rtprt1		;beyond, error
	mov	ax,ss:devsiz[bp] ;get low order size
	test	ax,ax		;if any
	jnz	rtprt3		;yes
	; exact 32 MB multiple so part # is too high
rtprt1:	stc			;bad partition #
	ret
rtprt2:	mov	ax,0FFFFh	;give max possible size
rtprt3:	mov	ss:cursiz[bp],ax ;set size of partition
	clc			;happy
	ret
;
mntts:	; mount TSS/8.24 file system (SEGSIZ must be 256.)
	mov	ax,ds:tpart	;get /PARTITION:n value (use to specify JOBMAX)
	test	ax,ax		;defaulted?
	jnz	mntts4		;no, keep it
	mov	ax,(5+1)*16d	;skip 5 fields for TSS/8 itself, assume JOBMAX
				;is at least 1 so skip one more field
mntts1:	; check next field to see if could be MFD (AX=starting block #)
	; (format of first few accounts is fixed, laid down by REFRESH cmd)
	; autodetection will be defeated if a swap track happens to contain a
	; valid-looking MFD -- on real system # of swap tracks is known by the
	; system so it doesn't have this problem
	xor	dx,dx		;zero-extend block #
	mov	di,offset buf	;pt at buffer
	mov	cx,1		;read 1 block
	push	ax		;save
	call	ss:rdblk[bp]	;go
	pop	ax		;[restore]
	jc	mntts3		;error
	cmp	cx,512d		;got all 512. bytes of block?
	jb	mntts3		;no
	mov	di,offset tssmfd ;point at list
	xor	bh,bh		;load 0
mntts2:	mov	bl,[di]		;get next address (zero-extended)
	inc	di		;+1
	cmp	bl,-1		;end of list?
	je	mntts5		;yes, success
	mov	cx,[si+bx]	;get value at that offset
	mov	bl,[di]		;get expected value (still zero-extended)
	inc	di		;+1
	cmp	bx,cx		;match?
	je	mntts2		;yes, keep going
	add	ax,16d		;no, skip to next field
	cmp	ax,(5+25d)*16d	;past maximum JOBMAX of 25?
	jbe	mntts1		;loop if not
mntts3:	cram	"?Can't find MFD",error
mntts4:	cmp	ax,25d		;too high?
	ja	mntts6		;yes, out of range
	add	ax,5		;skip 5 fields for TSS/8 itself
	mov	cl,4		;shift count to multiply by 16
	shl	ax,cl		;find base
mntts5:	mov	ss:mfddcn[bp],ax ;set addr of MFD
	mov	ss:curdir[bp],0001 ;cur dir = [0,1]
	ret
mntts6:	jmp	outran
;
tssmfd	label	word		;expected TSS/8 MFD contents
				;(N.B. all fit in a byte)
	db	3*2,10		;first blockette points at second
	db	10*2,0001	;[0,1]
	db	13*2,40		;next dir entry
	db	17*2,20		;retrieval list
	db	21*2,1		;MFD starts with segment #1 for sure
	db	40*2,0002	;[0,2]
	db	43*2,60		;next dir entry
	db	47*2,50		;retrieval list
	db	60*2,0003	;[0,3]
	db	67*2,70		;retrieval list
	db	-1		;end of list
;
mntxx:	; mount new XXDP+ file system
;;	mov	ss:curdir[bp],101h ;UIC=[1,1]
	ret
;+
;
; Set various things.
;
;-
set:	call	getw		;get parameter
	jc	set1		;missing
	mov	ax,offset setlst ;point at keyword list
	call	tbluk		;look it up
	jc	set2		;not found
	callr	ax		;call the routine
set1:	jmp	misprm		;missing keyword
set2:	jmp	synerr		;bad keyword
;
setlst	label	byte
	kw	<A:->,setfd0	;set floppy drive A: type
	kw	<B:->,setfd1
	kw	<C:->,setfd2
	kw	<CO-PY>,stcopy	;set ASCII/BINARY default for COPY
	kw	<D:->,setfd3
	kw	<DI-SMOUNT>,setdsm ;set DISMOUNT /[NO]UNLOAD default
	kw	<F-DC>,setfdc	;set FDC type (CompatiCard or generic)
	kw	<M-ORE>,stmore	;enable/disable **MORE** processing
	db	0
;
setfd0:	; set physical drive types for floppy drives
	; (override CMOS defaults, in case user has hooked up some kind of
	; reanimated corpse to their computer -- that's what we're here for!)
	xor	bl,bl		;unit 0
	jmp	short setfd4
setfd1:	mov	bl,1		;1
	jmp	short setfd4
setfd2:	mov	bl,2		;2
	jmp	short setfd4
setfd3:	mov	bl,3		;3
setfd4:	xor	bh,bh		;zero-extend unit #
	push	bx		;save
	call	getw		;get type
	pop	ax		;[restore]
	jc	setfd5
	push	ax		;(save)
	mov	ax,offset medtab ;point at keyword list
	call	tbluk		;look it up
	pop	bx		;[restore unit #]
	jc	setfd6		;not found
	; check to make sure it's a valid floppy drive type
	mov	di,offset fdlist ;pt at list
	mov	cx,fdlstl	;# entries
	repne	scasw		;see if this is on it
	jne	setfd6
	mov	ds:fdtype[bx],al ;we're OK, save it
	ret
setfd5:	jmp	misprm		;missing keyword
setfd6:	jmp	synerr		;bad keyword
;
fdlist	label	byte
	dw	rx01		;250 KB 8" SS SD
	dw	rx02		;500 KB 8" SS DD
	dw	rx03		;1001 KB 8" DS DD
	dw	rx31		;(we don't support 360 KB drives, but still)
	dw	rx50		;real RX50 drive with FDADAP (or TM100-4 etc.)
	dw	rx33		;1.2 MB 5.25"
	dw	rx24		;720 KB 3.5"
	dw	rx23		;1.44 MB 3.5"
	dw	rx26		;2.88 MB 3.5"
fdlstl=	($-fdlist)/2
;
stcopy:	; SET COPY
	call	getw		;get parameter
	jc	stcpy1
	mov	ax,offset scpytb ;pt at table
	call	tbluk		;look up
	jc	stcpy2
	mov	ds:bindef,ax	;save value
	callr	confrm		;should be EOL
stcpy1:	jmp	misprm		;missing parameter
stcpy2:	jmp	synerr		;bad keyword
;
setdsm:	; SET DISMOUNT
	call	getw		;get parameter
	jc	stdsm1
	mov	ax,offset dmtsws ;pt at table
	call	tbluk		;look up
	jc	stdsm2
	mov	ds:dsmunl,al	;save value
	callr	confrm		;should be EOL
stdsm1:	jmp	misprm		;missing parameter
stdsm2:	jmp	synerr		;bad keyword
;
setfdc:	; SET FDC
	call	getw		;get parameter
	jc	stfdc1
	mov	ax,offset sfdctb ;pt at table
	call	tbluk		;look up
	jc	stfdc2
	mov	byte ptr ds:cc4fdc,al ;save value
	callr	confrm		;should be EOL
stfdc1:	jmp	misprm		;missing parameter
stfdc2:	jmp	synerr		;bad keyword
;
stmore:	; SET MORE
	call	getw		;get parameter
	jc	stmor1
	mov	ax,offset onoff	;pt at table
	call	tbluk		;look up
	jc	stmor2
	mov	ds:more,al	;save value
	callr	confrm		;should be EOL
stmor1:	jmp	misprm		;missing parameter
stmor2:	jmp	synerr		;bad keyword
;
scpytb	label	byte
	kw	<A-SCII>,text
	kw	<B-INARY>,bin
	db	0
;
sfdctb	label	byte
	kw	<C-OMPATICARD>,1 ;Micro Solutions CompatiCard IV
	kw	<G-ENERIC>,0	;anything else
;;; maybe should add an entry for 37C65 chips that use an unusual way to set
;;; vertical mode, for now I just write the port if the version code matches
;;; but that could lead to trouble with FDCs that return the same version code
;;; (a lot do) but don't like writes to 3F1h
	db	0
;
onoff	label	byte
	kw	<OF-F>,0
	kw	<ON->,-1
	db	0
;+
;
; Show dev parameters.
;
;-
show:	call	skip		;blank line?
	jnc	show4		;no
	mov	bp,ds:logdev	;get head of logical device list
	test	bp,bp		;if any
	jz	show3
show1:	call	show5		;print next device
	mov	bp,ss:next[bp]	;follow link
show2:	test	bp,bp		;anything left?
	jz	show3		;no
	mov	di,offset lbuf	;pt at buf
	call	flush		;blank line between devs
	jmp	short show1
show3:	ret
show4:	; non-blank command line
	call	gdevn		;get dev name (or use default)
	call	confrm		;check for EOL
show5:	; print information for device at SS:BP
	mov	di,offset lbuf	;point at buf
	call	pdev		;print dev name
	mov	al,' '		;add a blank
	stosb
	cmp	ss:dirinp[bp],offset dosdi ;is it a DOS disk?
	jne	show6		;no
	cram	'DOS'		;DOS device, no useful information
	callr	flush
show6:	cmp	ss:hwtype[bp],hwfile ;image file?
	jne	show7		;no
	test	byte ptr ss:ilimg[bp],-1 ;yes, interleaved?
	jz	show7
	cram	'Interleaved '	;yes
show7:	mov	bx,ss:med[bp]	;get medium type
	mov	si,offset devnam ;point at name list
	call	pname		;print it
	mov	ax," ,"		;', '
	stosw
	mov	bx,ss:fsys[bp]	;get file system
	mov	si,offset fsynam ;point at name list
	call	pname		;print it
	mov	ax," ,"		;', '
	stosw
	mov	ax,ss:devsiz[bp] ;get # usable blocks
	mov	bx,ss:devsiz+2[bp]
	mov	cx,ss:devsiz+4[bp]
	mov	dx,ss:devsiz+6[bp]
	push	ax		;(save)
	push	bx
	push	cx
	push	dx
	call	pqnum		;print it
	cram	' total usable block'
	pop	dx		;(restore)
	pop	cx
	pop	bx
	pop	ax
	dec	ax		;size=1?
	or	ax,bx
	or	ax,cx
	or	ax,dx
	jz	show8		;yes
	mov	al,'s'		;add 's' if not
	stosb
show8:	; add current partition/LD: size for RT-11
	cmp	ss:fsys[bp],rt11 ;RT?
	jne	show11
	call	flush		;flush line, start new one
	mov	ax,ss:cursiz[bp] ;get # usable blocks
	mov	dx,ss:cursiz+2[bp]
	push	dx		;save
	push	ax
	call	pdnum		;print it
	cram	' block'
	pop	dx		;(restore)
	pop	ax
	dec	ax		;size=1?  (as above)
	jnz	show9
	cmp	dx,ax		;(AX now =0)
	jz	show10
show9:	mov	al,'s'		;add 's' if not
	stosb
show10:	cram	' in current volume'
show11:	callr	flush
;+
;
; TYPE {wildcard}
;
;-
typ:	; get filespec
	call	gdev		;parse device name, get dev rec in bp
	call	ss:defset[bp]	;set defaults
	mov	di,offset fname1 ;pt at buffer
	xor	bl,bl		;no implied wildcards
	call	ss:gfile[bp]	;parse dir/filename
	call	confrm		;make sure EOL
	xor	bx,bx		;input file
	call	ss:savcwd[bp]	;save dir
	call	ss:dsrini[bp]	;init DSR
	call	ss:dirinp[bp]	;init dir input
	mov	byte ptr ds:notfnd,1 ;nothing found yet
	; init fake output device
	mov	ds:binflg,text	;set text mode
	push	bp		;save
	call	getrec		;get a record (to be flushed in main loop)
	mov	ss:dsrini[bp],offset cret ;no init needed
	mov	ss:rstcwd[bp],offset cret ;no settings to restore
	mov	ss:write[bp],offset wtty ;output routine
	mov	ss:wlast[bp],offset wtty ;write last block (not called)
	mov	ss:dirfin[bp],offset cret ;no dir finish
	mov	ds:outdev,bp	;set output pseudo-device
	pop	bp		;restore
	mov	ds:indev,bp	;set input device
typ1:	; check next file
	call	ss:dirget[bp]	;get next entry
	jc	typ4		;no more
	jz	typ1		;skip empty areas and dirs
	mov	si,offset fname1 ;pt at wildcard
	push	di		;save
	call	wild		;check for match
	pop	di		;[restore]
	jne	typ1		;ignore
	; it's a match, type filename
	call	flush		;display filename
	xor	al,al		;load 0
	xchg	al,ds:notfnd	;found one, see if first
	test	al,al		;is it first?
	jnz	typ2		;yes, skip
	mov	byte ptr ds:lnum,1 ;**MORE** on next line
typ2:	; type the file
	mov	ds:optr,offset lbuf ;init output ptr
	mov	byte ptr ds:col,0 ;init col
	call	ss:open[bp]	;open it
	jc	typ5
	call	fcopy		;copy the file
	mov	di,ds:optr	;get output ptr
	cmp	di,offset lbuf	;anything on line?
	je	typ3		;no
	call	flush		;yes, flush it
typ3:	mov	bp,ds:indev	;get input device again
	call	ss:dsrini[bp]	;init it
	xor	bx,bx		;select input dir
	call	ss:rstcwd[bp]
	call	ss:reset[bp]	;close input file
	jmp	short typ1	;look for more
typ4:	; no more
	call	ss:dirfin[bp]	;finish up dir I/O
	cmp	byte ptr ds:notfnd,0 ;find anything?
	jnz	typ6
	ret
typ5:	jmp	dirio		;dir I/O err
typ6:	jmp	fnf		;file not found
;+
;
; Write to TTY for TYPE command.
;
; es:si	string
; cx	length (preserved on return)
;
;-
wtty:	jcxz	wtty6		;nothing (WLAST), skip
	push	cx		;save length (we always succeed)
	mov	di,ds:optr	;get output buf ptr
	mov	dx,ds		;copy
	mov	bx,es
wtty1:	mov	ds,bx		;exchange for now
	mov	es,dx
wtty2:	lodsb			;get a char
	cmp	al,' '		;ctrl char?
	jb	wtty8
wtty3:	cmp	byte ptr cs:col,78d ;will this be penultimate col?
	je	wtty7		;yes
wtty4:	stosb			;save
	inc	cs:col		;col+1
wtty5:	loop	wtty2		;loop
	mov	ds,dx		;restore
	mov	es,bx
	mov	ds:optr,di	;save
	pop	cx		;restore # chars written
wtty6:	clc			;got it all
	ret
wtty7:	; wrap while writing text char
	call	wrap		;go handle wrap
	jmp	short wtty4	;continue
wtty8:	; control character
	cmp	al,cr		;ignore CR
	je	wtty5
	cmp	al,lf		;EOL?
	je	wtty12
	cmp	al,tab		;tab?
	je	wtty10
	push	ax		;generic control char, save
	cmp	byte ptr cs:col,78d ;should we wrap before "^"?
	jne	wtty9
	 call	wrap		;yes
wtty9:	mov	al,'^'		;^
	stosb
	inc	cs:col		;col+1
	pop	ax		;restore char
	xor	al,100		;convert to printing char
	cmp	al,'L'		;^L?
	jne	wtty3
	; ^L causes **MORE**
	cmp	byte ptr cs:col,78d ;will this be penultimate col?
	jne	$+5
	 call	wrap
	stosb			;save
	push	bx		;save
	push	cx
	push	dx
	push	si
	mov	ds,dx		;restore seg
	call	flush		;flush line
	pop	si		;restore
	pop	dx
	pop	cx
	pop	bx
	mov	byte ptr ds:col,0 ;start over
	mov	byte ptr ds:lnum,1 ;**MORE** on next line
	mov	ds,bx
	jmp	short wtty5
wtty10:	; tab
	push	cx		;save
	xor	ch,ch		;CH=0
	mov	al,' '		;load blank
	mov	ah,cs:col	;get col
	add	ah,8d		;+8d
	and	ah,not 7	;back up to next tab stop
	cmp	ah,80d		;off EOL?
	je	wtty11
	mov	cl,ah		;copy col
	xchg	ah,cs:col	;update
	sub	cl,ah		;# cols to go
	rep	stosb		;write them
	pop	cx		;restore
	jmp	wtty5
wtty11:	mov	ah,78d		;stop 2 before
	mov	cl,ah		;copy
	xchg	ah,cs:col	;update
	sub	cl,ah		;# cols to go (may be 0)
	rep	stosb
	pop	cx		;restore
	call	wrap		;wrap
	jmp	wtty5
wtty12:	; LF
	push	bx		;save
	push	cx
	push	dx
	push	si
	mov	ds,dx		;restore seg
	call	flush		;flush line
	pop	si		;restore
	pop	dx
	pop	cx
	pop	bx
	mov	byte ptr ds:col,0 ;start over
	mov	ds,bx
	jmp	wtty5
;
wrap:	; handle wrap in WTTY
	push	ax		;save
	push	bx
	push	cx
	push	dx
	push	si
	mov	al,'!'		;indicate wrap
	stosb
	mov	ds,dx		;restore seg
	call	flush		;flush line (resets DI)
	pop	si		;restore
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	mov	byte ptr ds:col,0 ;back to 0
	mov	ds,bx
	ret
;
if 0
costyp:	; type COS-310 file
	mov	ax,ds:stblk	;starting block
	mov	ds:blk,ax	;save
	mov	ax,ds:fsize	;# blks
	mov	ds:cnt,ax	;save
	xor	cx,cx		;count=0
cost1:	; get next line
	call	cosw		;get a word
	jc	cost5		;EOF
	mov	bx,10000	;abs(length)
	sub	bx,ax
	and	bx,7777		;isolate (may have been 0)
	jz	cost5		;EOF (length must be gt 0)
	push	bx		;save length
	; print line #
	call	cosw		;get line #
;;	mov	di,offset buf+4	;pt at buf
	push	cx		;save
	mov	bx,10d		;base
	mov	cx,4		;loop count
cost2:	cwd			;DX=0
	div	bx		;/10
	or	dl,'0'		;cvt rem to ASCII
	dec	di		;-1
	mov	[di],dl		;save char
	loop	cost2		;loop
	; print the line itself
	pop	cx		;restore count
	add	di,4		;advance again
	mov	al,tab		;tab
	stosb			;save
	pop	dx		;get WC back
	jmp	short cost4	;go count line #, return
cost3:	call	cosw		;get next word
	mov	bl,al		;save
	sal	ax,1		;left 2
	sal	ax,1
	mov	al,ah		;get left char
	call	coschr		;save it
	mov	al,bl		;restore
	call	coschr		;save right char
cost4:	dec	dx		;done all?
	jnz	cost3		;loop if not
	mov	ax,cr+(lf*400)	;crlf
	stosw			;save it
	push	cx		;save
;;	mov	dx,offset buf	;buf addr
	mov	cx,di		;length
	sub	cx,dx
	mov	bx,1		;STDOUT
	mov	ah,40h		;func=write
	int	21h		;do it
	pop	cx		;restore
	jmp	short cost1	;loop
cost5:	jz	cost6		;no i/o err
	 clc			;no err
cost6:	ret
;
cosw:	; get next word in COS-310 file, C=1 Z=1 if EOF, C=1 Z=0 if I/O err
	jcxz	cosw2		;skip
cosw1:	lodsw			;get a word
	dec	cx		;-1
	clc			;no prob
	ret
cosw2:	cmp	ds:cnt,cx	;anything left to get?  (CX=0)
	jz	cosw3		;no, fake it
	mov	ax,ds:blk	;get blk #
	push	dx		;save
	push	di
;;;	call	rdsec		;read the sector
	pop	di		;[restore]
	pop	dx
	jc	cosw4		;bugged
	inc	ds:blk		;block+1
	dec	ds:cnt		;count-1
	jmp	short cosw1	;try again
cosw3:	xor	ax,ax		;return 0 (Z=1)
	stc			;err
	ret
cosw4:	xor	ax,ax		;return 0
	or	bh,1		;Z=0
	stc			;C=1
	ret
;
endif
;
coschr:	; write COS-310 char in AL
	and	al,77		;isolate
	jz	cosch1		;NUL, don't even bother
	add	al,' '-1	;cvt to ASCII
	cmp	al,'\'		;backslash?
	je	cosch2		;yes, special
	stosb			;save
cosch1:	ret
cosch2:	mov	al,tab		;\ is tab
	stosb
	ret
;+
;
; Print volume name.
;
;-
vol:	call	gdevn		;parse device name, get dev rec in BP
	call	ss:defset[bp]	;set defaults
	call	ss:gdir[bp]	;get dir name (matters for RT-11)
	call	confrm		;make sure confirmed
	call	ss:dsrini[bp]	;init DSR
	call	pvol		;print volume name (leave DI=LBUF)
	callr	flush		;blank line
;+
;
; Wipe out unused parts of disk (so Kermit, GZIP, PKZIP, etc. can win big).
;
;-
wipe:	call	gdevn		;parse device name, get dev rec in bp
	callr	ss:wipout[bp]	;wipe it out, return
;
	subttl	device mount/init code
;
	comment _

Steps in preparing a volume for use:

GMOUNT	get name, switches

OPNDEV	open device (or CRTDEV for FORMAT)

DEFMED	iff medium type defaulted (guess from size or by reading floppy), not
	for FORMAT (which fills in medium type/size stuff itself)

SETMED	now that medium type is finalized (except possibly the ILIMG flag for
	RX50/52 image files), set all vectors and sizes for it

DEFSYS	iff OS defaulted, try to guess it, try interleaved and not for
	RX50/RX52 image files if guessing (MOUNT only)

SETSYS	now that OS type is finalized, set all vectors for it

_
;+
;
; Parse MOUNT/FORMAT/INIT command args.
;
; [ldev:] [filespec] [/switches]
;
; bx	0 to parse logical dev name, -1 not to
; si,cx	cmd line descriptor
; OTHSWT  table of other switches (besides the medium/filesystem ones)
;
; Prints message and returns to MLOOP on error.
; Otherwise sets up TAREA and dev record as much as possible, returns ptr to
; dev record in SS:BP.
;
;-
gmount:	push	cx		;save
	xor	al,al		;load 0
	mov	di,offset tarea	;pt at temp area
	mov	cx,ltarea	;length
	rep	stosb		;clear it out
	pop	cx		;restore
	mov	ds:tdev,bx	;skip logical dev name for FORMAT/INIT cmds
gmnt1:	call	skip		;skip blanks, tabs
	jc	gmnt2		;done, see if we have everything
	cmp	al,ds:swchar	;switch?
	je	gmnt3		;handle it
	cmp	ds:tdev,0	;do we already have a logical device?
	jz	gmnt10		;no, this must be it
	cmp	ds:tfnam,0	;do we already have a filename?
	jnz	gmnt9		;yes, this is an error
	call	getw		;get filename
	mov	ds:tfnam,bx	;save ptr
	mov	ds:tfnam+2,dx	;and length
	jmp	short gmnt1
gmnt2:	jmp	gmnt11
gmnt3:	; handle switches
	inc	si		;eat the / or whatever
	dec	cx
	call	getan		;get alphanumeric switch (stop at ':' etc.)
	jc	gmnt9
	mov	ax,offset strtab ;pt at structure types
	call	tbluk		;is it one of those?
	jc	gmnt4
	cmp	ds:tfsys,0	;was there already a file system type?
	jnz	gmnt9		;yes
	mov	ds:tfsys,ax	;save this one
	jmp	short gmnt1
gmnt4:	mov	ax,offset medtab ;pt at medium types
	call	tbluk		;is it one of those?
	jc	gmnt5
	cmp	ds:tmed,0	;do we have one already?
	jnz	gmnt9		;yes
	mov	ds:tmed,ax	;save this one
	jmp	short gmnt1
gmnt5:	mov	ax,offset partab ;pt at partition stuff table
	call	tbluk		;look it up
	jc	gmnt6
	call	ax		;call the routine
	jmp	short gmnt1
gmnt6:	mov	ax,offset mnttab ;point at other mount switches
	call	tbluk		;look it up
	jc	gmnt7
	call	ax		;call the routine
	jmp	short gmnt1
gmnt7:	mov	ax,ds:othswt	;point at other switches
	test	ax,ax		;anything?
	jz	gmnt8		;no
	call	tbluk		;look it up
	jc	gmnt8
	call	ax		;call the routine
	jmp	short gmnt1
gmnt8:	jmp	badswt		;bad switch
gmnt9:	jmp	synerr		;go complain
gmnt10:	xor	ah,ah		;no colon needed
	call	getlog		;get logical name
	jc	gmnt9
	mov	ds:tdev,ax	;save
	mov	ds:tdev+2,bx
	jmp	gmnt1
gmnt11:	; end of line, check to see if this all made sense
	cmp	ds:tdev,0	;is there a logical name
	jnz	gmnt12		;yes
	cram	'?Missing logical name',error
gmnt12:	mov	ax,ds:tdev	;get dev name
	mov	bx,ds:tdev+2	;and flag,,unit
	; if there's already a device with this name, dismount it
	mov	bp,ds:logdev	;get head of list
	jmp	short gmnt15
gmnt13:	cmp	ss:logd[bp],ax	;match?
	jne	gmnt14
	cmp	ss:logu[bp],bx
	jne	gmnt14
	push	ax		;save
	push	bx
	mov	al,ds:dsmunl	;get /[NO]UNLOAD default
	call	dmnt		;dismount device
	pop	bx		;restore
	pop	ax
	jmp	short gmnt16	;go allocate it
gmnt14:	mov	bp,ss:next[bp]	;get next
gmnt15:	test	bp,bp		;is there one?
	jnz	gmnt13		;loop if so
gmnt16:	; set up a record for it
	call	getrec		;get a record
	mov	ss:logd[bp],ax	;set dev name
	mov	ss:logu[bp],bx	;and unit #
	mov	ax,ds:tfsys	;get file system (if any)
	mov	ss:fsys[bp],ax	;save
	mov	ax,ds:tmed	;get medium type (if any)
	mov	ss:med[bp],ax	;save
	; interpret filename and/or switches
	cmp	ds:tfnam,0	;got image filename?
	jnz	gmnt19		;yes
	cmp	byte ptr ds:tdrv+1,0 ;how about /DRIVE?
	jnz	gmnt17		;yes
	; no filename or /DRIVE, use logical name if just one letter
	mov	ax,ds:tdev	;get log dev name
	test	al,al		;is log dev name just one letter?
	jnz	gmnt18		;no, bugged
	cmp	byte ptr ds:tdev+3,0 ;was unit # given?
	jnz	gmnt18		;yes, bugged
	mov	al,ah		;copy
	callr	gdrvl		;handle drive letter
gmnt17:	callr	gdrive		;handle drive
gmnt18:	cram	'?Missing drive or file name',error
gmnt19:	callr	image		;parse image filename (or drive or COM port)
;
strtab	label	byte
	kw	<COS-310>,cos310 ;uses OS/8 format but text files are weird
	kw	<DOS-BATCH>,dosbat ;DOS/BATCH disk structure
				;(DECtape is similar, but different)
	kw	<FI-LES11>,ods1	;Files-11 ODS-1
	kw	<FO-REIGN>,frgn	;just blocks as far as we're concerned
	kw	<IA-S>,ods1	;synonym for Files-11
	kw	<OD-S1>,ods1	;synonym for Files-11
	kw	<OS-8>,os8	;OS/8, OS/78, OS/278 all use the same format
	kw	<OS78->,os8
	kw	<OS278->,os8
	kw	<PO-S>,ods1	;synonym for Files-11
	kw	<PS-8>,ps8	;PS/8
	kw	<PU-TR>,putr	;TSS/8 PUTR format
	kw	<RD-S>,rsts	;RSTS Disk Structure
	kw	<RST-SE>,rsts	;synonym
	kw	<RSX-11M+>,ods1	;synonym for Files-11
	kw	<RT-11>,rt11	;RT-11 format
	kw	<TS-S8>,tss8	;TSS/8.24 format
	kw	<XX-DP+>,xxdp	;XXDP new format
	db	0
;
medtab	label	byte
	kw	<7-20KB>,rx24	;here for backwards compatibility only
	kw	<MS-CP>,mscp
	kw	<RA6-0>,ra60
	kw	<RA70->,ra70
	kw	<RA71->,ra71
	kw	<RA72->,ra72
	kw	<RA73->,ra73
	kw	<RA80->,ra80
	kw	<RA81->,ra81
	kw	<RA82->,ra82
	kw	<RA90->,ra90
	kw	<RA92->,ra92
	kw	<RD32->,rd32
	kw	<RD51->,rd51
	kw	<RD52->,rd52
	kw	<RD53->,rd53
	kw	<RD54->,rd54
	kw	<RK02->,rk02
	kw	<RK-05>,rk05
	kw	<RK06->,rk06
	kw	<RK07->,rk07
	kw	<RL-01>,rl01
	kw	<RL02->,rl02
	kw	<RM02->,rm02
	kw	<RM03->,rm03
	kw	<RM05->,rm05
	kw	<RM80->,rm80
	kw	<RP02->,rp02
	kw	<RP03->,rp03
	kw	<RP04->,rp04
	kw	<RP05->,rp05
	kw	<RP-06>,rp06
	kw	<RP07->,rp07
	kw	<RS03->,rs03
	kw	<RS04->,rs04
	kw	<RS08->,rs08
	kw	<RX01->,rx01
	kw	<RX02->,rx02
	kw	<RX03->,rx03
	kw	<RX23->,rx23
	kw	<RX24->,rx24
	kw	<RX26->,rx26
	kw	<RX33->,rx33
	kw	<RX-50>,rx50
	kw	<RX52->,rx52
	kw	<TU56->,tu56
	kw	<TU58->,tu58
	db	0
;
partab	label	byte
	kw	<BP-ARTITION>,mbpar ;BIOS partition #, 1-4
	kw	<DR-IVE>,mdrv	;hard or floppy disk unit #
	kw	<TY-PE>,mtype	;partition type (hex)
	db	0
;
mnttab	label	byte
	kw	<I-NTERLEAVED>,intlv ;interleaved image file
	kw	<NOI-NTERLEAVED>,noilv ;non-interleaved image file
	kw	<PA-RTITION>,part ;RT-11 partition #
	kw	<RO-NLY>,ronly	;read-only access to device
	kw	<RW->,rdwrt	;read/write access to device
	db	0
;
mdrv:	; /DRIVE parm
	call	eqnum		;get "=n"
	mov	ds:tdrv,ax	;save
	ret
;
mbpar:	; /BPARTITION parm
	call	eqnum		;get "=n"
	test	al,al		;make sure it's from 1 to 4
	jz	outra1
	cmp	al,4
	ja	outra1
	mov	ds:tbpart,ax	;save
	ret
;
eqnum:	; parse "=n", return number +100h in AX (# must be .LE.255)
	call	skip		;skip blanks, etc.
	jc	mispr1
	cmp	al,':'		;:
	je	eqnum1
	cmp	al,'='		;or =, right?
	jne	mispr1
eqnum1:	inc	si		;yes, eat it
	dec	cx
	call	getn		;get number
	test	ah,ah		;not ridiculous?
	jnz	outra1
	inc	ah		;=1
	ret
mispr1:	jmp	misprm		;missing parm
outra1:	jmp	outran		;out of range
;
mtype:	; /TYPE=hh
	call	skip		;skip blanks, etc.
	jc	mispr1
	cmp	al,'='		;=, right?
	jne	mispr1
	inc	si		;yes, eat it
	dec	cx
	call	geth		;get number
	test	ah,ah		;out of range?
	jnz	outra1
	inc	ah		;=1
	mov	ds:ttype,ax	;save
	ret
;
intlv:	; /INTERLEAVE
	mov	ds:tintlv,101h	;high byte set, low byte=flag
	ret
;
noilv:	; /NOINTERLEAVE
	mov	ds:tintlv,100h	;high byte set, low byte=flag
	ret
;
part:	; /PARTITION=n
	call	skip		;skip blanks, etc.
	jc	part2
	cmp	al,':'		;:
	je	part1
	cmp	al,'='		;or =, right?
	jne	part2
part1:	inc	si		;yes, eat it
	dec	cx
	call	getn		;get number
	mov	ds:tpart,ax	;save
	ret
part2:	jmp	misprm		;missing parm
;
ronly:	; /RONLY
	mov	ds:tronly,101h	;high byte set, low byte=RO flag
	ret
;
rdwrt:	; /RW
	mov	ds:tronly,100h	;high byte set, low byte=RO flag
	ret
;+
;
; Decode image filename from GMOUNT.
;
; ss:bp	log dev rec
;
;-
image:	; filename given, see if it's just a floppy drive name
	mov	si,ds:tfnam	;get ptr to name
	mov	bx,ds:tfnam+2	;length of name
	mov	ah,[si+bx-1]	;get last char
	cmp	bx,2		;right length for "x:"?
	jne	imag1
	cmp	ah,':'		;ends in colon?
	jne	imag1
	mov	al,[si]		;get drive letter
	cmp	al,'A'		;is it really a letter?
	jb	imag1
	cmp	al,'Z'
	ja	imag1
	; drive letter
	mov	ds:tfnam,0	;forget filename
	jmp	gdrvl		;go handle it
imag1:	; not a drive letter, see if SCSIht_l:
	cmp	bx,5		;at least long enough for "SCSI:"?
	jb	imag2		;no
	cmp	ah,':'		;ends in colon?
	jne	imag2
	cmp	word ptr [si],"CS" ;SCSI?
	jne	imag2
	cmp	word ptr [si+2],"IS"
	jne	imag2		;no
	jmp	gscsi		;handle it
imag2:	; not SCSI, try CDROMx:
	cmp	bx,7		;correct length for "CDROMx:"?
	jne	imag3
	cmp	word ptr [si],"DC" ;CD
	jne	imag3
	cmp	word ptr [si+2],"OR" ;RO
	jne	imag3
	cmp	byte ptr [si+4],'M' ;M
	jne	imag3
	cmp	byte ptr [si+6],':' ;ends in :
	jne	imag3
	mov	al,[si+5]	;looks good, get drive letter
	sub	al,'A'		;is it really a letter?
	cmp	al,'Z'-'A'
	ja	imag3
	mov	ds:tfnam,0	;forget filename
	jmp	gcdrom		;go handle it, AL=letter
imag3:	; not CDROM, maybe then it's a COM port
	cmp	bx,5		;long enough for "COMn:"?
	jb	imag5
	cmp	byte ptr [si+4],':' ;colon in the right place?
	jne	imag5
	cmp	word ptr [si],"OC" ;COM?
	jne	imag5
	cmp	byte ptr [si+2],'M'
	jne	imag5		;no
	mov	al,[si+3]	;get unit #
	sub	al,'1'		;subtract base
	cmp	al,4		;valid?
	jae	imag5		;no
	jmp	getcom		;get COM port
imag4:	jmp	swtcon
imag5:	; maybe a Catweasel drive?
	cmp	bx,4		;correct length for "CWx:"?
	jne	imag6		;no
	cmp	word ptr [si],"WC" ;CW?
	jne	imag6
	mov	ax,[si+2]	;get 2nd 2 letters
	cmp	ah,':'		;ends in colon?
	jne	imag6		;no
	sub	al,'0'		;0 or 1?
	cmp	al,1		;(now binary)
	ja	imag6
	jmp	getcw		;get Catweasel drive
imag6:	; it's a file, make sure no /DRIVE, /BPART, /TYPE
	mov	ss:hwtype[bp],hwfile ;set type
	mov	al,byte ptr ds:tdrv+1 ;see if any set
	or	al,byte ptr ds:tbpart+1
	or	al,byte ptr ds:ttype+1
	jnz	imag4		;switch conflict if so
	; set default ptrs to block I/O routines (for linear block image)
;;	mov	ss:rdblk[bp],offset rdfil ;read from file
;;	mov	ss:wrblk[bp],offset wrfil ;write to file
;;; OPNIMG sets those too, do we need to bother?
imag7:	mov	ss:dsrini[bp],offset cret ;no DSR init
	ret
;+
;
; Parse COM port parameters for actual TU58 drive.
;
; bp	log dev rec
; al	unit # (0-3)
; si	pts at 'COMn:bbbbb' string
;
;-
getcom:	; it's a COM port, see if it exists
	add	al,al		;*2
	push	ds		;save
	xor	bx,bx		;point at BIOS data
	mov	ds,bx
	mov	bh,4		;RS232_BASE=400h
	mov	bl,al		;index
	mov	cx,[bx]		;get base port #
	pop	ds		;restore
	jcxz	gcom2		;no such port
	mov	ss:port[bp],cx	;save
	mov	ax,tu58		;default medium is TU58
	cmp	ax,ss:med[bp]	;is that it?
	je	gcom5
	cmp	ss:med[bp],0	;is it something else?
	jz	gcom4		;no, good
gcom1:	jmp	swtcon
gcom2:	cram	'?No such port',error
gcom3:	cram	'?Invalid baud rate',error
gcom4:	mov	ss:med[bp],ax	;set medium type if defaulted
gcom5:	; it does, get baud rate if given
	mov	bx,3		;baud rate divisor for 38.4kbps (default)
	mov	cx,ds:tfnam+2	;get length
	sub	cx,5		;subtract what we ate
	jz	gcom7		;that's all there is
	add	si,5		;skip past colon
	call	getn		;parse baud rate
	test	cx,cx		;that's everything, right?
	jnz	gcom3
	mov	bx,2		;min baud rate
	cmp	ax,2		;will their baud rate cause overflow?
	jb	gcom6
	 mov	bx,ax		;no, use it
gcom6:	mov	ax,0C200h	;divisor is 1C200h/(baud rate)
	mov	dx,1
	div	bx
	mov	bx,ax		;copy
gcom7:	; BX=baud rate divisor, init port for programmed I/O
	mov	dx,ss:port[bp]	;get base port #
	add	dx,3		;line ctrl reg
	mov	al,203		;DLAB=1
	out	dx,al
	sub	dx,3		;baud rate divisor
	mov	al,bl		;write low byte
	out	dx,al
	inc	dx		;+1
	mov	al,bh		;write high byte
	out	dx,al
	inc	dx		;+2
	inc	dx
	mov	al,3		;8 data bits, 1 stop, no parity
	out	dx,al
	dec	dx		;-2
	dec	dx
	xor	al,al		;disable all ints
	out	dx,al
	add	dx,3		;modem ctrl reg
	mov	al,3		;set RTS, DTR
	out	dx,al
	; make sure switches are consistant for TU58
	mov	al,byte ptr ds:tdrv ;get drive # (default=0)
	cmp	al,1		;0 or 1, right?
	ja	gcom8
	mov	ss:drive[bp],al	;save
	mov	al,byte ptr ds:tbpart+1 ;make sure nothing extraneous set
	or	al,byte ptr ds:ttype+1
	jnz	gcom9		;switch conflict if so
	mov	ss:dsrini[bp],offset ddinit
	mov	ss:rdblk[bp],offset rddd
	mov	ss:wrblk[bp],offset wrdd
	mov	ss:devsiz[bp],512d ;# blks
	mov	ss:totblk[bp],512d ;total is same
	mov	ss:totsiz+2[bp],4 ;total in bytes=2^18.
	mov	ss:hwtype[bp],hwtu58 ;it's a real TU58 drive
	ret
gcom8:	jmp	baddrv		;invalid drive #
gcom9:	jmp	swtcon		;switch conflict
;+
;
; Decode hard drive/floppy information.
;
; ss:bp	logical dev record
; al	drive letter
;
;-
gdrvl:	cmp	byte ptr ds:tdrv+1,0 ;/DRIVE already given?
	jnz	gdrvl1		;switch conflict if so
	mov	bl,byte ptr ds:tbpart+1 ;see if /BPART or /TYPE given
	or	bl,byte ptr ds:ttype+1 ;(required for hard drives)
	sub	al,'A'		;convert to phys unit #
	cmp	al,ds:firhd	;floppy or hard?
	jb	gdrvl2		;floppy
if 0
 jmp short gdrvl2 ;;;;
 %out temp fix for 8" C:
endif
	sub	al,ds:firhd	;hard, subtract base
	test	bl,bl		;they gave /BPART and/or /TYPE right?
	jnz	gdrvl3		;yes
	cram	'?Partition not specified',error
gdrvl1:	jmp	swtcon		;switch conflict
gdrvl2:	test	bl,bl		;floppy, partition must not be specified
	jnz	gdrvl1		;switch conflict
gdrvl3:	mov	ah,1		;set "valid" flag
	mov	ds:tdrv,ax	;save
gdrive:	; enter here with DS:TDRV set up
	mov	bl,byte ptr ds:tbpart+1 ;check for /BPART or /TYPE (again)
	or	bl,byte ptr ds:ttype+1 ;(to see if hard or floppy)
	jnz	gdrv2		;hard, skip
	; floppy
	cmp	al,ds:firhd	;floppy, in range?
	jae	gdrv1		;no
if 0
 %out temp fix for 8" C:
 ;;; (comment out the JAE GDRV1 above)
endif
	mov	ss:hwtype[bp],hwflop ;floppy drive
	mov	ss:drive[bp],al	;save unit #
	mov	ss:rdsec[bp],offset fdrds ;set up initial sector I/O routines
	mov	ss:wrsec[bp],offset fdwrs
	mov	ss:rdid[bp],offset fdrid
	ret
gdrv1:	jmp	baddrv		;no such drive
gdrv2:	; hard disk partition
	cmp	al,ds:numhd	;in range?
	jae	gdrv1		;punt if not
	or	al,80h		;set "HD" bit
	mov	ss:drive[bp],al	;save
	; read partition table
	mov	cx,5		;retry count
gdrv3:	push	cx		;save
	mov	dl,byte ptr ds:tdrv ;get drive #
	xor	dh,dh		;head=0
	mov	cx,1		;track 0, sector 1
	mov	bx,ds:dbuf	;pt at buffer
	mov	ax,0201h	;func=read 1 sector
	int	13h
	jnc	gdrv4
	xor	ah,ah		;func=reset
	int	13h
	pop	cx		;restore retry count
	loop	gdrv3
	cram	'?Error reading partition table',error
gdrv4:	; search partition table for our partition
	pop	cx		;flush count
	mov	cl,4		;loop count (CH=0 from above)
	xor	dx,dx		;load 0
	xchg	dx,ds:tbpart	;get flag,,partition, zero it
	mov	ax,ds:ttype	;and flag,,type
	add	bx,1BEh		;pt at first table slot
gdrv5:	cmp	byte ptr [bx+4],0 ;is this slot empty?
	jz	gdrv7		;yes, skip it
	inc	byte ptr ds:tbpart ;actual partition +1
	test	dh,dh		;are we going by partition #?
	jz	gdrv6
	dec	dl		;yes, see if we're there
	jz	gdrv8		;we are, check type if we have it
	jmp	short gdrv7	;go check next slot
gdrv6:	cmp	al,[bx+4]	;is this the type byte we're looking for?
	je	gdrv9		;we're done if so
gdrv7:	add	bx,10h		;skip to next slot
	loop	gdrv5		;loop through all
	cram	'?No such partition',error
gdrv8:	test	ah,ah		;found partition, can we double-check type?
	jz	gdrv9		;no, we're satisfied
	cmp	al,[bx+4]	;yes, does it match?
	je	gdrv9		;yes
	cram	'?Partition type mismatch',error
gdrv9:	; DS:BX points to partition table entry
	mov	dh,[bx+1]	;get head
	mov	cx,[bx+2]	;cyl, sector
	mov	al,[bx+4]	;type
;;; put them somewhere
	mov	ss:hwtype[bp],hwpart ;type=H.D. partition
	ret
;+
;
; Actually open the device or container file found by GMOUNT.
;
; ss:bp	logical device record
;
;-
opndev:	mov	bx,ss:hwtype[bp] ;get hardware type
	callr	ds:opnhw[bx]	;dispatch
;
_=0
opnhw	label	word
	disp	hwfile,opnimg	;open image file
	disp	hwflop,cret	;open floppy (already done)
	disp	hwcwsl,cret	;open Catweasel floppy (already done)
	disp	hwpart,opnhd	;open hard disk partition
	disp	hwtu58,cret	;open TU58 (already done)
	disp	hwaspi,cret	;open ASPI device (already done)
	disp	hwcdex,cret	;open CD-ROM (already done)
;
opnimg:	; open image file
	mov	ss:dsrini[bp],offset cret ;no DSR init
	mov	ss:rdblk[bp],offset rdfil ;read from file
	mov	ss:wrblk[bp],offset wrfil ;write to file
	mov	si,ds:tfnam	;get ptr to filename
	mov	cx,ds:tfnam+2	;get length
	mov	di,offset dotdsk ;default extension
	call	defext		;apply it if needed
	xor	al,al		;func=open /RO
	test	byte ptr ds:tronly,-1 ;did they say /RO?
	jnz	opni1
	 mov	al,02h		;no, change it to open /RW
opni1:	call	opn		;try to open
	jnc	opni2		;skip if OK
	 jmp	fnf		;file not found
opni2:	mov	ss:handle[bp],ax ;save handle
	mov	ds:outhnd,ax	;make sure it gets closed if we abort
	xor	dx,dx		;set offset=0000:0000
	xor	cx,cx
	mov	bx,ax		;copy handle
	mov	ax,4202h	;func=lseek from EOF
	int	21h
	jc	opni3		;failed
	mov	bx,ax		;copy DX:AX to CX:BX
	mov	cx,dx
	callr	setsiz		;set size from DX:AX, return
opni3:	mov	ah,3Eh		;func=close
	int	21h
	jmp	rderr		;flag as read error
;
opnhd:	; one of these days
;;;
	ret
;+
;
; Same as above, but creates file for FORMAT.
;
; ss:bp	log dev rec
;
;-
crtdev:	mov	bx,ss:hwtype[bp] ;get hardware type
	callr	ds:crthw[bx]	;dispatch
;
_=0
crthw	label	word
	disp	hwfile,crtimg	;create image file
	disp	hwflop,cret	;create floppy (nothing to do)
	disp	hwcwsl,cret	;create Catweasel floppy (nothing to do)
	disp	hwpart,crthd	;create hard disk partition
	disp	hwtu58,cret	;create TU58 (nothing to do)
;
crtimg:	; create image file
	test	byte ptr ds:tronly,-1 ;did they say /RO?
	jnz	crti2		;pinhead
	call	aysure		;make sure they're sure
;;; maybe should ask only if the file exists already?
	mov	ss:dsrini[bp],offset cret ;no DSR init
	mov	ss:rdblk[bp],offset rdfil ;read from file
	mov	ss:wrblk[bp],offset wrfil ;write to file
	mov	si,ds:tfnam	;get ptr to filename
	mov	cx,ds:tfnam+2	;get length
	mov	di,offset dotdsk ;default extension
	call	defext		;apply it if needed
	call	crt		;create file
	jnc	crti1		;skip if OK
	jmp	crerr		;file creation error
crti1:	mov	ss:handle[bp],ax ;save handle
	mov	ds:outhnd,ax	;make sure it gets closed if we abort
	ret
crti2:	jmp	rodev		;can't format a read-only device
;
crthd:	; one of these days
	ret
;+
;
; Figure out default medium type (from size or by reading if floppy).
;
; Note that we can't tell an interleaved RX50 (or RX52) image file from a
; non-interleaved one by the size alone (it's 400 KB either way), so we'll hold
; off setting ILIMG until DEFSYS for RX50/52s.
;
; ss:bp	logical device record
;
;-
defmed:	mov	bx,ss:hwtype[bp] ;get hardware type
	callr	ds:medhw[bx]	;dispatch, return
;
_=0
medhw	label	word		;choose default medium based on HW type
	disp	hwfile,medfil	;image file
	disp	hwflop,medflp	;floppy
	disp	hwcwsl,medflp	;Catweasel floppy
	disp	hwpart,medbig	;hard disk partition
	disp	hwtu58,med58	;TU58
	disp	hwaspi,medbig	;ASPI device
	disp	hwcdex,medbig	;CD-ROM
;
medfil:	; set default medium type for image file
	; look up file type on size (take the biggest that fits)
	mov	ax,ss:totsiz[bp] ;get total image file size
	mov	dx,ss:totsiz+2[bp]
	mov	bx,offset fsztab ;pt at table
	xor	si,si		;nothing found yet
mfil1:	cmp	dx,ds:flsiz+2[bx] ;too small?
	jb	mfil4
	ja	mfil2		;definitely fits
	cmp	ax,ds:flsiz[bx]	;check low word
	jb	mfil4		;too small
mfil2:	; if medium type already known, we're just checking for interleave
	; (filter out other medium types because file may be bigger than
	; necessary w/o hurting anything, that shouldn't confuse us)
	mov	cx,ss:med[bp]	;get current medium type
	jcxz	mfil3		;none, great
	cmp	ds:flmed[bx],cx	;yes, is this the right dev type?
	jne	mfil4		;skip it if not
	mov	ax,ds:tintlv	;get /[NO]INTERLEAVE flag
	test	ah,ah		;did they specify interleave or not?
	jz	mfil3		;no, just take it
	cmp	al,ds:flilv[bx]	;does it match?
	jne	mfil4		;no
mfil3:	mov	si,bx		;save a copy
mfil4:	add	bx,fllen	;skip to next entry
	cmp	bx,offset fszend ;off end?  (including catch-all)
	jb	mfil1		;loop if not
	test	si,si		;get anything?
	jz	mfil6		;no, file too small
	mov	al,ds:flilv[si]	;get interleave flag
	mov	ss:ilimg[bp],al	;save (may already match)
	mov	ax,ds:flmed[si]	;get medium type
	mov	ss:med[bp],ax	;set it, if not already set
mfil5:	ret
mfil6:	; didn't find anything <= this dev's size
	cmp	ss:med[bp],mscp	;did they already say /MSCP?
	je	mfil5		;yes
	test	ss:med[bp],-1	;did they say anything?
	jnz	mfil7		;yes, guess the size was just wrong
	 mov	ss:med[bp],mscp	;assume MSCP if nothing else fits
mfil7:	ret
;
medflp:	; set default medium type for floppy drive
	; not an image file, see if floppy
	test	ss:med[bp],-1	;medium specified?
	jnz	mflp5		;yes
	; try reading a sector a few different ways to guess disk type
	mov	si,offset fdtyps ;point at table
mflp1:	lodsw			;get a word
	test	ax,ax		;end of table?
	jz	mflp6		;too bad
	push	si		;save
	call	ax		;set up FDC for test
	pop	si
	lodsw			;get table addr
	mov	bx,ax		;copy
	test	bx,bx		;does it exist?
	jnz	mflp2
	 lodsw			;no, get type (this is a leaf on the tree)
mflp2:	push	ax		;save both (AX is junk if BX is non-zero)
	push	bx
	lodsb			;get cyl
	mov	ch,al
	lodsb			;head
	mov	dh,al
	lodsb			;sector
	mov	cl,al
	push	si		;save
	test	cl,cl		;read specific sector, or just read header?
	js	mflp3		;header
	call	fdrds		;try to read
	jmp	short mflp4
mflp3:	call	fdrid		;read a header
	jc	mflp4		;failed
	mov	al,[si+3]	;get N (SI points to C,H,R,N)
	cmp	ds:fdlen,al	;match?
	je	mflp4		;yes
	stc			;say error
mflp4:	pop	si		;[restore]
	pop	bx
	pop	ax
	jc	mflp1		;loop if failed
	mov	si,bx		;follow branch
	test	si,si		;or was it a leaf?
	jnz	mflp1		;branch, follow it
	mov	ss:med[bp],ax	;leaf, we got it
mflp5:	ret
mflp6:	cram	'?Unable to detect disk type',error
;
medbig:	; set default medium type for big devices (SCSI, CD-ROM, raw partition)
	test	ss:med[bp],-1	;medium specified?
	jnz	mbig1		;yes
	 mov	ss:med[bp],mscp	;default is MSCP for all large raw devices
mbig1:	ret
;
med58:	; set default medium type for TU58 tapes (duh!)
	test	ss:med[bp],-1	;medium specified?
	jnz	m58a		;yes
	 mov	ss:med[bp],tu58	;the only logical choice
m58a:	ret
;
; Table of file sizes and device geometry.  Used by DEFMED to guess the medium
; type, and by SETMED to set up all the lengths once the medium type is known.
; This table is sorted in order of increasing image file size, so that DEFMED
; can use the latest entry <= the current image file to determine the image
; type.
;
; "2BSD" means geometry was supplied or supplemented by /etc/disktab entry
; from 2BSD UNIX (in cases where DEC docs were lacking or wrong).
;
fsztab	label	byte
	filsiz	rx01,252928d,252928d,,77d,1,26d,128d,1 ;252928. interleaved
	filsiz	rx01,,252928d,,77d,1,26d,128d,0 ;256256. non-interleaved
	filsiz	tu58,,,,1,1,512d,512d,0	;TU58 256 KB
	; this is tricky, three different flavors of TU56 image
dt256	label	byte		;256-word DECtape blocks (PDP-11)
	filsiz	tu56,,,,1,1,578d,512d,0	;TU56 (256 16-bit words x 578)
dt128	label	byte		;128-word DECtape blocks (PDP-8, OS/8)
	filsiz	tu56,,,,1,1,1474d,128d*2,0 ;TU56 (128 12-bit words x 1474)
dt129	label	byte		;129-word DECtape blocks (PDP-8, TSS/8)
	filsiz	tu56,,,,1,1,1474d,129d*2,0 ;TU56 (129 12-bit words x 1474)
	filsiz	rx50,,,,80d,1,10d,512d,1 ;RX50 400 KB (interleaved or not)
	filsiz	rx50,,,,80d,1,10d,512d,0 ;(2nd so ILIMG is cleared for DEFSYS)
	filsiz	rx02,505856d,505856d,,77d,1,26d,256d,1 ;505856. interleaved
	filsiz	rx02,,505856d,,77d,1,26d,256d,0 ;512512. non-interleaved
	filsiz	rs03,,,,64d,1,64d,128d,0 ;RS03 512 KB
	filsiz	rs08,,,,64d,1,64d,128d,0 ;RS08 256 KW (512 KB)
	filsiz	rx24,,,,80d,2,9d,512d,0	;RX24 720 KB
	filsiz	rx52,,,,80d,2,10d,512d,1 ;RX52 800 KB (interleaved or not)
	filsiz	rx52,,,,80d,2,10d,512d,0
	filsiz	rx03,1011712d,1011712d,,77d,2,26d,256d,1 ;1011712. interleaved
	filsiz	rx03,,1011712d,,77d,2,26d,256d,0 ;1025024. non-interleaved
	filsiz	rs04,,,,64d,1,64d,256d,0 ;RS04 1024 KB
	filsiz	rk02,,,,200d,2,12d,256d,0 ;RK02 1.2 MB (ignore 3 spare tracks)
	filsiz	rx33,,,,80d,2,15d,512d,0 ;RX33 1.2 MB
	filsiz	rx23,,,,80d,2,18d,512d,0 ;RX23 1.44 MB
dk12	label	byte		;12-sector RK05 with 16-bit words (PDP-11)
	filsiz	rk05,,,,200d,2,12d,512d,0 ;RK05 2.5 MB (ignore 3 spare tracks)
	filsiz	rx26,,,,80d,2,36d,512d,0 ;RX26 2.88 MB
dk16	label	byte		;16-sector RK05 with 12-bit words (PDP-8)
	filsiz	rk05,,,,200d,2,16d,512d,0 ;RK05 1.6 MW (ignore 3 spare tracks)
	filsiz	rl01,,,40d*256d,256d,2,40d,256d,0 ;RL01 5 MB
	filsiz	rd51,19584d*512d,19584d*512d,,1024d,8d,17d,512d ;RD51 10 MB
					;(C/H/S values are wrong)
	filsiz	rl02,,,40d*256d,512d,2,40d,256d,0 ;RL02 10 MB
	filsiz	rk06,,,22d*512d,411d,3,22d,512d,0 ;RK06 14 MB
	filsiz	rp02,,,,200d,20d,10d,512d,0 ;RP02 20 MB (ignore 3 spare tracks)
	filsiz	rk07,,,22d*512d,815d,3,22d,512d,0 ;RK07 28 MB
	filsiz	rd52,60480d*512d,60480d*512d,,1024d,8d,17d,512d ;RD52 30 MB
					;(C/H/S values are wrong)
	filsiz	rp03,,,,400d,20d,10d,512d,0 ;RP03 40 MB (ignore 6 spare tracks)
	filsiz	rm03,,,32d*512d,823d,5d,32d,512d,0 ;RM03 67 MB (put it first)
	filsiz	rm02,,,32d*512d,823d,5d,32d,512d,0 ;RM02 67 MB
	filsiz	rd53,138672d*512d,138672d*512d,,1024d,8d,17d,512d ;RD53 69 MB
	filsiz	rp05,,,22d*512d,411d,19d,22d,512d,0 ;RP05 87 MB (put it first)
	filsiz	rp04,,,22d*512d,411d,19d,22d,512d,0 ;RP04 87 MB
	filsiz	ra80,,,,546d,14d,31d,512d ;RA80 121 MB (560. cyls w/RCT)
	filsiz	rm80,,,31d*512d,1118d,7d,31d,512d,0 ;RM80 124 MB
	filsiz	rd54,311200d*512d,311200d*512d,,1220d,15d,17d,512d ;RD54 159 MB
	filsiz	rp06,,,22d*512d,815d,19d,22d,512d,0 ;RP06 174 MB
	filsiz	ra60,,,,1588d,6,42d,512d ;RA60 205 MB (1592. cyls w/RCT)
	filsiz	rm05,,,32d*512d,823d,19d,32d,512d,0 ;RM05 256 MB
	filsiz	ra70,,,,1507d,11d,33d,512d ;RA70 280 MB (no RCT 2BSD)
	filsiz	ra81,,,,1248d,14d,51d,512d ;RA81 456 MB (1258. cyls w/RCT 2BSD)
	filsiz	rp07,,,50d*512d,1260d,16d,50d,512d,0 ;RP07 516 MB
	filsiz	ra82,,,,1423d,15d,57d,512d ;RA82 622 MB (1427. cyls w/RCT 2BSD)
	filsiz	ra71,,,,1915d,14d,51d,512d ;RA71 700 MB (no RCT 2BSD)
	filsiz	ra72,,,,1915d,20d,51d,512d ;RA72 1.0 GB (no RCT 2BSD)
	filsiz	ra90,,,,2649d,13d,69d,512d ;RA90 1.2 GB (2656. cyls w/RCT)
	filsiz	ra92,,,,3099d,13d,73d,512d ;RA92 1.5 GB (3101. cyls w/RCT)
				;(2BSD RA92 /etc/disktab entry differs wildly)
	filsiz	ra73,,,,2667d,21d,70d,512d ;RA73 2.0 GB (no RCT 2BSD)
;
fszend	label	byte	;end of actual device types
;
; Floppy disk autosizing tables.
;
; Each entry looks like this:
;
;	dw	addr of routine to set up FDC this type, 0 if end of list
;	dw	addr of table to skip to if successful, or 0 if just one type
; [	dw	disk type if successful, if previous word was 0 ]
;	db	cyl,head,sec	to try to read
;
; If SEC is negative then any sector will do (so use FDRID because it's
; faster).
;
fdtyps	dw	inidz,dd512	;512 b/s DD disks in HD drive
	db	0,0,-1
	dw	ini23,hd512	;512 b/s HD disks
	db	0,0,-1
	dw	inidy,hd256	;256 b/s HD disks (8" or workalikes)
	db	0,0,-1
	dw	inidx,0,rx01	;RX01 if this works
	db	0,0,-1
	dw	ini24,0,rx24	;RX24 (250 kHz vs. 300 kHz with DD512)
	db	0,0,-1		;(this would see 360 KB drives too)
	dw	ini26,0,rx26	;RX26 if this works
	db	0,0,-1
	dw	0		;end of list
;
dd512	dw	inidz,0,rx52	;RX52 if DS and has 10 sectors
	db	0,1,10d
	dw	inidz,0,rx50	;RX50 if SS and has 10 sectors
	db	0,0,10d
	dw	0
;
hd256	dw	inidy,0,rx03	;RX03 if has side 1
	db	0,1,-1
	dw	inidy,0,rx02	;otherwise must be RX02
	db	0,0,-1
	dw	0
;
hd512	dw	ini23,0,rx23	;RX23 if has 18 sectors
	db	0,0,18d
	dw	ini33,0,rx33	;otherwise must be RX33
	db	0,0,15d
	dw	0
;+
;
; Set all parameters that depend on medium type.
;
; We handle a few special cases in case there's more than one device size with
; the same medium type.  For example, TU56 images come in different sizes
; depending on whether they're 16/18-bit images (578 blocks of 256 words each)
; or 12-bit images (1474 blocks of 129 words, although for OS/8 we only need to
; store 128 words since the 129th is dropped and all blocks are transferred in
; the forwards direction).
;
; bp	log dev rec
; BADSEC  is left set up for this device (with size of bad sector track)
;
;-
setmed:	mov	ds:badsec,0	;init size of bad sector track
	mov	ds:badsec+2,0
	mov	bx,ss:med[bp]	;get disk type
	cmp	bx,mscp		;MSCP (i.e. variable size) device?
	je	smed5		;yes, keep actual file size
	; look up the device type (which we either were given explicitly, or
	; guessed based on the device size), and set the size parameters to
	; match that device regardless of the actual physical size (otherwise
	; we may write incorrect directories etc.)
	mov	al,ss:ilimg[bp]	;get interleaved image file flag
	mov	si,offset fsztab ;point at file size table
smed1:	cmp	[si],bx		;is this it?
.assume <flmed eq 0>
	je	smed3		;yes
smed2:	add	si,fllen	;bump to next
	cmp	si,offset fszend ;done all?
	jb	smed1		;loop if not
	jmp	short smed5	;whatever, keep current file size info
smed3:	; found matching device, copy its info if interleave flag matches
	cmp	ds:flilv[si],al	;match?
	jne	smed2		;no
	; special case:  differentiate between 12- and 16-bit versions of the
	; same media (RK05s, DECtapes)
	mov	bx,ss:med[bp]	;get disk type
	push	si		;save FSZTAB ptr
	mov	si,offset smedwh ;pt at table
	call	wdluk		;lookup
	pop	si		;[restore]
	jc	smed4		;invalid
	mov	bx,ss:totsiz[bp] ;do we know volume size?  (no => FORMAT)
	or	bx,ss:totsiz+2[bp]
	or	bx,ss:totsiz+4[bp]
	or	bx,ss:totsiz+6[bp]
	call	ax		;choose which one, ZF=1 if creating
smed4:	mov	ax,ds:flbsz[si]	;get size of bad block track in bytes
	mov	dx,ds:flbsz+2[si]
	mov	ds:badsec,ax	;save
	mov	ds:badsec+2,dx
	; set up device geometry (currently used only for floppies & DECtapes)
	mov	ax,ds:flcyl[si]	;copy # cyls
	mov	ss:ncyls[bp],al	;save low byte
	mov	al,ds:flhd[si]	;# heads
	mov	ss:nhds[bp],al
	mov	ax,ds:flsec[si]	;# sectors
	mov	ss:nsecs[bp],al	;save low byte
	mov	ax,ds:flbs[si]	;# bytes/sector
	mov	ss:secsz[bp],ax
	mov	ax,ds:flsiz[si]	;get file size in bytes
	mov	dx,ds:flsiz+2[si]
	mov	bx,ds:flusz[si]	;get usable file size
	mov	cx,ds:flusz+2[si]
	jmp	short smed6	;already set size
smed5:	mov	ax,ss:totsiz[bp] ;get size
	mov	dx,ss:totsiz+2[bp]
	mov	bx,ax		;copy to CX:BX too
	mov	cx,dx
smed6:	call	setsiz		;set size information
	; handle any needed special treatment
	mov	bx,ss:med[bp]	;get disk type
	mov	si,offset smedfd ;pt at table
	call	wdluk		;lookup
	jc	smed7		;invalid
	 callr	ax		;init I/O routine ptrs, return
smed7:	ret
;
smedwh	label	word		;choose which of several identical types to use
				;(entered with ZF=1 if creating fresh volume)
	dw	rk05,smdk	;RK05, can have 12 or 16 sectors
	dw	tu56,smdt	;TU56, can have 2 or 3 different block sizes
	dw	0
;
smdk:	; set medium type for RK05
	mov	si,offset dk12	;[assume PDP-11 format, 12 secs/track]
	jz	smdk1		;creating, go set size
	cmp	word ptr ss:fsys[bp],0 ;have they set a file system?
	jnz	smdk1		;yes, definitely use that
	mov	ax,ss:totsiz+6[bp] ;see if huge
	or	ax,ss:totsiz+4[bp]
	jnz	smdk2		;yes, must be PDP-8 format
	cmp	word ptr ss:totsiz+2[bp],50d ;big enough for PDP-8 format?
	jae	smdk2		;yes
	ret			;nope, keep PDP-11 format
smdk1:	cmp	byte ptr ss:wsize[bp],16d ;really PDP-11 format?
	je	smdk3
smdk2:	mov	si,offset dk16	;no, PDP-8 format, 16 secs/track
smdk3:	ret
;
smdt:	; set medium type for TU56
	mov	si,offset dt256	;[assume PDP-11 format, 256 words/block]
	jz	smdt1		;creating, go set size
;;	cmp	word ptr ss:fsys[bp],0 ;have they set a file system?
;;	jnz	smdt1		;yes, definitely use that
;;;; not really, we need to be able to read OS/8 tapes both ways, and make our
;;;; best effort on chopped TSS/8 tapes
	mov	ax,ss:totsiz+6[bp] ;see if huge
	or	ax,ss:totsiz+4[bp]
	jnz	smdt2		;yes, must be PDP-8 129-word format
	mov	dx,ss:totsiz+2[bp] ;get actual size
	mov	ax,ss:totsiz[bp]
	; see if big enough for 1474. blocks of 129. words each
	.386			;32-bit intermediate values in constants
	cmp	dx,(1474d*129d*2)/10000h ;big enough for 129 word/block format?
	.186
	ja	smdt2		;yes
	jb	smdt3		;no, keep PDP-11 format
	.386			;same deal
	cmp	ax,(1474d*129d*2) and 0FFFFh ;big enough?
	.186
	jae	smdt2		;yes
	; try 1474. blocks of 128. words each
	.386
.assume <((1474d*129d*2)/10000h) eq ((1474d*128d*2)/10000h)> ;MSWs are the same
	cmp	ax,(1474d*128d*2) and 0FFFFh ;big enough?
	.186
	jae	smdt4		;yes
	ret			;nope, keep PDP-11 format
smdt1:	cmp	byte ptr ss:wsize[bp],16d ;really PDP-11 format?
	je	smdt3		;yes
	cmp	word ptr ss:fsys[bp],putr ;PUTR.SAV format?
	jne	smdt4		;yes
smdt2:	mov	si,offset dt129	;PDP-8 format, 129 words/block
smdt3:	ret
smdt4:	mov	si,offset dt128	;PDP-8 format, 128 words/block
	ret
;
smedfd	label	word		;set up floppy parameters
				;cyls/heads/sectors, # bytes/sector
	dw	rx01,sfddx	;77/1/26, 128 b/s
	dw	rx02,sfddy	;77/1/26, 256 b/s
	dw	rx03,sfdda	;77/2/26, 256 b/s
	dw	rx23,sfd23	;80/2/18, 512 b/s
	dw	rx26,sfd26	;80/2/36, 512 b/s
	dw	rx33,sfd33	;80/2/15, 512 b/s
	dw	rx50,sfddz	;80/1/10, 512 b/s
	dw	rx52,sfddz	;80/2/10, 512 b/s
	dw	rx24,sfd24	;80/2/9, 512 b/s
	dw	0
;
sfddx:	; RX01 floppy
	mov	ax,offset inidx	;init
	mov	bx,offset dxilv	;interleave routine
	jmp	short sfddx1
;
sfddy:	; RX02(-like) floppy
	mov	ax,offset inidy
	mov	bx,offset dyilv	;interleave routine
	jmp	short sfddx1
;
sfdda:	; RX03(-like) floppy
	mov	ax,offset inidy
	mov	bx,offset dyilv
sfddx1:	; enter here to finish setting up RX01/02/03 floppy
	; AX=init routine, BX=interleave routine
	mov	ss:dsrini[bp],ax ;FDC init routine
	mov	ss:fintlv[bp],bx ;interleave routine
	mov	ss:rdblk[bp],offset rddx ;read from floppy
	mov	ss:wrblk[bp],offset wrdx
	mov	ax,offset fdrds	;assume actual hardware
	mov	bx,offset fdwrs
	cmp	ss:hwtype[bp],hwflop ;right?
	je	sfddx2
	mov	ax,offset cwrds ;maybe Catweasel hardware
	mov	bx,offset cwwrs
	cmp	ss:hwtype[bp],hwcwsl ;right?
	je	sfddx2
	mov	ax,offset rddxf	;no, must be file otherwise
	mov	bx,offset wrdxf
sfddx2:	mov	ss:rdsec[bp],ax	;save
	mov	ss:wrsec[bp],bx
	mov	cl,2		;shift count to convert 128 (sec) to 512 (blk)
	cmp	ss:secsz[bp],256d ;double density?
	jne	sfddx3
	 dec	cx		;yes, fix shift count
sfddx3:	mov	ss:blksec[bp],cl ;save it
	ret
;
sfd24:	; RX24 floppy (720 KB)
	mov	ax,offset ini24
	jmp	short sfddu
;
sfd23:	; RX23 floppy (1.44 MB)
	mov	ax,offset ini23
	jmp	short sfddu
;
sfd26:	; RX26 floppy (2.88 MB)
	mov	ax,offset ini26
	jmp	short sfddu
;
sfd33:	; RX33 floppy (1.2 MB)
	mov	ax,offset ini33
	;jmp	short sfddu
;
sfddu:	; set up RX23/RX24/RX26/RX33 disk or image file
	; AX=DSRINI value
	mov	ss:dsrini[bp],ax ;save
	mov	ss:rdblk[bp],offset rddz
	mov	ss:wrblk[bp],offset wrdz
	mov	ss:fintlv[bp],offset duilv
	mov	ax,offset fdrds	;assume actual hardware
	mov	bx,offset fdwrs
	cmp	ss:hwtype[bp],hwflop ;right?
	je	sfddu1
	mov	ax,offset cwrds	;maybe Catweasel hardware
	mov	bx,offset cwwrs
	cmp	ss:hwtype[bp],hwcwsl ;right?
	je	sfddu1
	mov	ax,offset rdduf	;no, must be file otherwise
	mov	bx,offset wrduf
sfddu1:	mov	ss:rdsec[bp],ax	;save
	mov	ss:wrsec[bp],bx
	ret
;
sfddz:	; RX50 floppy
	mov	ss:dsrini[bp],offset inidz
	mov	ss:rdblk[bp],offset rddz
	mov	ss:wrblk[bp],offset wrdz
	mov	ss:fintlv[bp],offset dzilv
	mov	ax,offset fdrds	;assume actual hardware
	mov	bx,offset fdwrs
	cmp	ss:hwtype[bp],hwflop ;right?
	je	sfddz1
	mov	ax,offset cwrds ;maybe Catweasel hardware
	mov	bx,offset cwwrs
	cmp	ss:hwtype[bp],hwcwsl ;right?
	je	sfddz1
	mov	ax,offset rddzf	;no, must be file otherwise
	mov	bx,offset wrdzf
sfddz1:	mov	ss:rdsec[bp],ax	;save
	mov	ss:wrsec[bp],bx
	ret
;+
;
; Set device size information.
;
; ss:bp	log dev rec
; dx:ax	total size in bytes
; cx:bx	usable size in bytes
; si	preserved
;
;-
setsiz:	mov	ss:totsiz[bp],ax ;save
	mov	ss:totsiz+2[bp],dx
	; assume a block is 512. bytes and init TOTBLK
	; (can change it later if assumption is wrong)
	push	cx		;save
	mov	cl,9d		;shift count
	shr	ax,cl		;divide by 512.
	ror	dx,cl
	mov	cx,dx		;copy
	and	dl,not 177	;isolate high 9
	xor	cx,dx		;isolate low 7
	mov	ss:totblk+2[bp],cx ;save high order
	or	ax,dx		;compose low order
	mov	ss:totblk[bp],ax ;save
	pop	dx		;catch high order usable size (DX:BX)
	; same for DEVSIZ
	mov	cl,9d		;shift count again
	shr	bx,cl		;divide by 512.
	ror	dx,cl
	mov	cx,dx		;copy
	and	dl,not 177	;isolate high 9
	xor	cx,dx		;isolate low 7
	mov	ss:devsiz+2[bp],cx ;save high order
	or	bx,dx		;compose low order
	mov	ss:devsiz[bp],bx ;save
	;callr	<short finsiz>	;finalize size, return
;+
;
; Finalize size, so that INIT and SHOW will get correct block count.
;
; ss:bp	log dev rec (preserved)
;
;-
finsiz:	; special cases for devices which can be 12- or 16-bit volumes
	mov	ax,ss:med[bp]	;get disk type
	mov	bx,ss:fsys[bp]	;and file system
	cmp	ax,rk05		;RK05?
	jne	fnsz2		;no
	; RK05, special case for OS/8 (and presumably PS/8)
	cmp	bx,os8		;OS/8?
	je	fnsz1
	cmp	bx,ps8		;os PS/8?
	jne	fnsz4
fnsz1:	mov	word ptr ss:devsiz[bp],3200d ;yes, 3200 blks/volume
	ret
fnsz2:	cmp	ax,tu56		;TU56?
	jne	fnsz4		;no
	cmp	bx,os8		;OS/8, PS/8, TSS/8 PUTR.SAV?
	je	fnsz3
	cmp	bx,ps8
	je	fnsz3
	cmp	bx,putr
	jne	fnsz4
fnsz3:	mov	word ptr ss:devsiz[bp],1474d/2 ;737 blks/tape
fnsz4:	ret
;+
;
; Try to guess file system if defaulted.
;
;-
defsys:	test	ss:fsys[bp],-1	;did they tell us?
	jnz	dsys6		;yes
	xor	cx,cx		;no idea yet
dsys1:	push	cx		;save current file system guess
	push	dx		;save ILIMG value that goes with CX
	call	ss:dsrini[bp]	;init DSR
	pop	dx		;restore
	pop	cx
	mov	si,offset chklst ;point at list of routines
dsys2:	lodsw			;get next addr
	test	ax,ax		;end of table?
	jz	dsys3		;yes
	push	si		;save
	push	cx		;save current file system guess
	push	dx		;save ILIMG of that guess
	call	ax		;see if this could be it
	pop	dx		;[restore]
	pop	cx
	pop	si
	jc	dsys2		;no, keep trying
	test	cx,cx		;did we already have a guess?
	jnz	dsys7		;yes, ambiguous
	mov	cx,ax		;save current guess
	mov	dl,ss:ilimg[bp]	;remember whether interleaved
	jmp	short dsys2
dsys3:	; finished trying all tests in table
	cmp	ss:hwtype[bp],hwfile ;image file?
	jne	dsys5		;no
	cmp	ss:med[bp],rx50	;is it an RX50 image?
	je	dsys4
	cmp	ss:med[bp],rx52	;or "RX52" image?
	jne	dsys5
dsys4:	; RX50 or RX52 image file, make second pass through table with ILIMG on
	; (since these files are the same size whether interleaved or not, we
	; can't set ILIMG based on file size alone)
	test	byte ptr ds:tintlv+1,-1 ;did they give explicit interleave?
	jnz	dsys5		;yes, don't mess with it then
	cmp	ss:ilimg[bp],al	;have we already tried both interleaves?
	jnz	dsys5		;yes, see what we got
	inc	ss:ilimg[bp]	;try it the other way
	jmp	short dsys1
dsys5:	jcxz	dsys7		;never found anything
	mov	ss:fsys[bp],cx	;set it
	mov	ss:ilimg[bp],dl	;remember how ILIMG was set

;;; maybe display message saying what we got?

dsys6:	ret
dsys7:	jmp	unksys		;unknown or ambiguous filesystem
;
; List of routines to call (with SS:BP=DSRINI'd dev rec) to see whether the
; current device could possibly be a particular file system.  If not, the
; routine should return CF=1, if so return CF=0 and the file system code in ax.
;
chklst	dw	chkdb		;DOS/BATCH
	dw	chkod		;Files-11 ODS-1
	dw	chkos		;OS/8
	dw	chkpu		;TSS/8 PUTR.SAV
	dw	chkrs		;RSTS/E
	dw	chkrt		;RT-11
	dw	chkts		;TSS/8.24
	dw	chkxx		;XXDP+
	dw	0
;
chkdb:	; see if it's a DOS/BATCH disk (by checking to see if bitmap linked
	; list matches retrieval pointers in block 1)
	push	word ptr ds:fremem ;save free memory ptr
	mov	ax,dosbat	;our file type
	xchg	ax,ss:fsys[bp]	;assume we win, save old value
	push	ax
	call	dbclu		;figure out cluster size to use
	pop	ss:fsys[bp]	;restore
	mov	cx,ss:pcs[bp]	;fetch it
	call	getmem		;get enough memory to hold home MFD block
	push	si		;save
	pop	si		;restore
	mov	di,si		;copy
	mov	ax,1		;MFD home is cluster #1
	mov	cx,ax		;read 1 cluster
	cmp	ss:med[bp],tu56	;TU56?
	jne	chkdb1
	 mov	ax,64d		;DT: MFD is at 64., not 1
chkdb1:	call	dbrclu		;go
	jc	chkdb3		;error
	cmp	cx,ss:pcs[bp]	;got all of MFD home block?
	jb	chkdb3		;no
	mov	di,si		;copy
	add	si,4		;skip to map
	lodsw			;fetch starting blk # of bitmap, SI=home+6
	test	ax,ax		;it exists, right?
	jz	chkdb3		;no
	xor	dx,dx		;init bitmap sequence #
chkdb2:	; check next block of map
	; AX = expected block #, DX = seq #, DS:SI = ptr into retrieval list
	; DI = begn of MFD home cluster
	mov	cx,di		;copy MFD ptr
	add	cx,ss:pcs[bp]	;pt past end of buf
	cmp	si,cx		;off end of block?
	je	chkdb3		;yes, way too many retrieval ptrs!
	cmp	ax,[si]		;matches next retrieval ptr right?
	jne	chkdb3		;no
	test	ax,ax		;end of list?  (CF=0)
	jz	chkdb4		;yes, we win
	lodsw			;eat it (same as existing AX value)
	cmp	word ptr ss:pcs[bp],bufsiz ;will cluster fit in BUF?
	ja	chkdb3		;no
	push	si		;save
	push	di
	push	dx
	mov	di,offset buf	;pt at buffer (before copy of home block)
	mov	cx,1		;read 1 cluster
	call	dbrclu		;go
	mov	bx,si		;[copy]
	pop	dx		;[restore]
	pop	di
	pop	si
	jc	chkdb3		;error
	cmp	cx,ss:pcs[bp]	;got all of cluster?
	jb	chkdb3		;no
	; got the block, make sure it's what we expect
	mov	ax,[bx]		;get link ptr
	inc	dx		;bump expected seq
	cmp	dx,[bx+2]	;correct?
	jne	chkdb3
	mov	cx,[bx+6]	;get first blk of bitmap
	cmp	cx,word ptr [di+4] ;matches value in home block?
	je	chkdb2		;loop if so
chkdb3:	stc			;no match
chkdb4:	mov	ax,dosbat	;[in case we like it]
	pop	word ptr ds:fremem ;[flush our memory]
	ret
;
chkod:	; see if it's an ODS-1 disk (by finding home block)
	mov	ss:fsys[bp],ods1 ;make RX02 interleave work temporarily
	call	odhome		;find ODS-1 home block, CF=1 on failure
	mov	ss:fsys[bp],0	;[clear again]
	mov	ax,ods1		;[use this type if found]
	ret
;
chkos:	; see if it's an OS/8 disk (by doing dir consistency check)
	mov	ss:fsys[bp],os8	;make TRIM12 work temporarily
	mov	ax,offset osilv	;interleave routine
	call	chkosx		;check it
	jc	chkos1		;not us, skip
	test	bl,bl		;could be OS/8?  (CF=0)
	jnz	chkos1		;yes
	 stc			;not ours
chkos1:	ret
;
chkosx:	; enter here for PS/8, PUTR checks (AX=DECtape interleave routine)
	; returns flags in BX:
	; BL=-1 if possibly could be OS/8, 0 if couldn't
	; BH=-1 if possibly could be PUTR, 0 if couldn't
	call	setdta		;set up DTA parameters, if DECtape
	call	ospart		;try mounting OS/8 partition
	jc	ckos2		;failed
	mov	word ptr ds:rtfile,-1 ;could be either OS/8 (+0) or PUTR (+1)
	xor	dl,dl		;init bitmap of blks 1-6
	mov	ax,1		;start with dir segment #1 (i.e. blk #1)
ckos1:	; handle dir seg # AX (1-6)
	mov	cl,al		;copy seg #
	mov	bl,1		;load a 1
	sal	bl,cl		;shift into place
	test	dl,bl		;bit already set?
	jnz	ckos2		;infinite segment loop, not valid OS/8 dir
	or	dl,bl		;otherwise remember we've been here
	push	ax		;save blk #
	push	dx		;and bitmap
	push	di		;and # info words if any
	mov	di,offset buf	;pt at buffer
	xor	dx,dx		;zero-extend blk #
	mov	cx,1		;read 1 block
	call	ss:rdblk[bp]	;go
	pop	di		;[restore]
	pop	dx
	pop	ax
	jc	ckos2		;error
	cmp	cx,512d		;got all 512. bytes of home block?
	jb	ckos2		;no
	; check header
	mov	bx,[si+10]	;get -(# info words)
	neg	bx		;make positive
	and	bh,7777/400	;trim to 12 bits
	add	bx,bx		;*2
	dec	ax		;is this block 1?
	mov	ax,[si+2]	;[get starting blk # for files in this seg]
	jnz	ckos3		;no
	mov	di,bx		;yes, this is the defn of # info words
	mov	ds:iblk,ax	;init block ctr
	jmp	short ckos4
ckos2:	call	unsdta		;unset DECtape parms, if set
	stc			;fail (keep this centrally located)
	mov	ss:fsys[bp],0	;[back the way it was]
	ret
ckos3:	cmp	di,bx		;does this match what was in block 1?
	jne	ckos2
	cmp	ds:iblk,ax	;continues where prev seg left off right?
	jne	ckos2
ckos4:	mov	bx,[si]		;get -(# entries)
	neg	bx		;make positive
	and	bh,7777/400	;trim to 12 bits
	add	si,5*2		;skip to begn of dir
ckos5:	; go through all file entries, make sure it's all believable
	cmp	si,offset buf+512d-4 ;space for 2 words?
	ja	ckos2
	lodsw			;eat a word
	test	ax,ax		;.EMPTY. block?
	jz	ckos8		;yes (otherwise file)
	add	si,4*2-2	;skip remainder of filename
	jc	ckos2
	mov	ax,[si-2]	;fetch extension
	test	al,77		;R.H. character non-blank?
	jz	ckos6		;nope, nothing to learn here
	test	ax,7700		;yes, L.H. character blank?
	jnz	ckos6		;no, never mind
	 mov	byte ptr ds:rtfile+0,0 ;<blank><nonblank>, couldn't be OS/8
ckos6:	and	ax,7740		;get high 7 bits of extension
	cmp	ax,0040		;must be 0040 for PUTR ext. encoding
	je	ckos7
	 mov	byte ptr ds:rtfile+1,0 ;isn't, can't be PUTR.SAV
ckos7:	add	si,di		;skip info words
	jc	ckos2
ckos8:	cmp	si,offset buf+512d-2 ;space for one word?
	ja	ckos2
	lodsw			;get -(length)
	mov	cx,10000	;abs val
	sub	cx,ax
	and	ch,7777/400	;trim in case 0
	add	ds:iblk,cx	;update ptr
	cmp	ds:iblk,10000	;>4K blocks?
	ja	ckos2		;invalid if so
	dec	bx		;done all entries?
	jnz	ckos5		;loop if not
	mov	ax,word ptr ds:buf+4 ;get ptr to next dir seg
	test	ax,ax		;end of chain?  (CF=0)
	jz	ckos9		;yes, yay
	cmp	ax,6		;otherwise must be in [1,6]
	ja	ckos2
	jmp	ckos1
ckos9:	call	unsdta		;unset DECtape parms, if set
	mov	bx,ds:rtfile	;get flag
	xor	ax,ax		;load 0, CF=0
	xchg	ax,ss:fsys[bp]	;[back the way it was, AX=file sys we thought]
	ret			;CF=0 from XOR
;
chkpu:	; see if it's a TSS/8 PUTR.SAV DECtape (by doing dir consistency check)
	cmp	word ptr ss:med[bp],tu56 ;must be DECtape
	jne	chkpu1		;isn't
	mov	ss:fsys[bp],putr ;assume PUTR.SAV for now
	mov	ax,offset puilv	;interleave routine
	call	chkosx		;use OS/8 check
	jc	chkpu2		;not us, skip
	cmp	bx,0FF00h	;PUTR,,OS/8 flag = true,,false?
	je	chkpu2		;yes, CF=0
	; (no good if could be either, let OS/8 take it)
chkpu1:	stc			;not ours
chkpu2:	ret
;
chkrs:	; see if it's a RSTS/E disk (by checking pack label record)
	; calculate device cluster size
	mov	ax,1		;assume DCS=1
	mov	bx,ss:devsiz+2[bp] ;get high order of dev size
	mov	cx,ss:devsiz+4[bp]
	mov	dx,ss:devsiz+6[bp]
chrs1:	; see how many times we have to divide the dev size by 2 to make the
	; size fit in 16 bits
	mov	si,bx		;see if high bits are all 0
	or	si,cx
	or	si,dx
	jz	chrs2		;they are, we're done
	shr	dx,1		;high-order size right a bit
	rcr	cx,1
	rcr	bx,1
	add	ax,ax		;DCS left a bit
	cmp	ax,64d		;<=64. right?
	jbe	chrs1		;yes, keep looking
	jmp	short chrs9
chrs2:	inc	cx		;read 1 block (known 0 from above)
	mov	di,offset buf	;pt at buffer
	call	ss:rdblk[bp]	;go (DX known 0 from above)
	jc	chrs9		;error
	cmp	cx,512d		;got all 512. bytes of home block?
	jb	chrs9		;no
	; RDS 0.0 MFD header and 1.X pack label records are both at the begn of
	; block 1 and look enough alike that we can check for both
	lodsw			;+00 is 0 or 1, or link word in RDS 0.0
	lodsw			;+02 is flag marking blockette as used
	inc	ax		;must be -1
	jnz	chrs9
	lodsw			;+04 is reserved (but non-zero on my V9.7 pack)
	lodsw			;+06 is reserved MBZ (0.0) or RDS level (1.X)
	test	ax,ax		;0 is OK for RDS 0.0
	jz	chrs3
	cmp	ah,1		;1.anything is OK
	jne	chrs9
chrs3:	lodsw			;+10 is pack clustersize
	mov	bx,ax		;copy
	sub	bx,1		;-1 (btw, can't be 0)
	jc	chrs9		;too bad
	and	ax,bx		;must be power of 2
	jnz	chrs9
	lodsw			;+12 pack status bits
	lodsw			;+14 first word of pack ID
	mov	di,offset buf	;(overlay beginning of buf)
	call	rad50		;convert to decimal
	lodsw			;+16 second word of pack ID
	call	rad50
	; see if pack label looks reasonable
	lea	si,[di-6]	;point at it
	mov	cx,6		;length
chrs4:	lodsb			;get a char
	cmp	al,' '		;blank?
	je	chrs7		;yes, make sure no more non-blank
	cmp	al,'0'		;must be letter or number
	jb	chrs9
	cmp	al,'9'
	jbe	chrs5
	cmp	al,'A'
	jb	chrs9
	cmp	al,'Z'
	ja	chrs9
chrs5:	loop	chrs4		;keep going
	jmp	short chrs8	;6-character label, no sweat
chrs6:	lodsb			;get another
	cmp	al,' '		;must be blank
	jne	chrs9		;no, invalid
chrs7:	loop	chrs6		;count through blanks
chrs8:	mov	ax,rsts		;looks good
	clc
	ret
chrs9:	stc			;no
	ret
;
chkrt:	; see if it's an RT-11 disk (by checking signature in home block)
	call	rtpart		;try mounting RT-11 partition
	jc	chkrt2		;failed
	mov	di,offset buf	;pt at buffer
	mov	ax,ss:curdir[bp] ;get base of partition
	mov	dx,ss:curdir+2[bp]
	add	ax,1		;home block is block 1
	adc	dx,0
	mov	cx,1		;read 1 block
	call	ss:rdblk[bp]	;go
	jc	chkrt2		;error
	cmp	cx,512d		;got all 512. bytes of home block?
	jb	chkrt2		;no
	add	si,760		;index to system ID field
	mov	ax,si		;save
	mov	di,offset sgnrt	;point at "DECRT11A" signature
	mov	cx,12d/2	;12 chars long including blanks
	repe	cmpsw		;match?
	je	chkrt1		;yes
	mov	si,ax		;restore SI
	mov	di,offset sgnvex ;point at "DECVMSEXCHNG" signature
				;(thanks to Tim Shoppa for this information)
	mov	cl,12d/2	;12 chars long (CH=0 from above)
	repe	cmpsw		;match?
	jne	chkrt2		;no
chkrt1:	mov	ax,rt11		;yay
	ret			;CF=0 from REPE CMPSW
chkrt2:	stc			;it's not us
	ret
;
chkts:	; see if it's a TSS/8.24 disk
	mov	ss:fsys[bp],tss8 ;make TRIM12 work temporarily
	; check SATCNT/SATPNT for validity
	mov	ax,16d+(7251/256d) ;blk # of SATCNT in field 1 of disk
	cwd			;zero-extend
	mov	cx,(10000-7251+255d)/256d ;2 blocks from here to end of SAT
.assume <bufsiz ge ((10000-7251+255d)/256d)*512d>
	mov	di,offset buf	;pt at buffer
	call	ss:rdblk[bp]	;go
	jc	chkts4		;error
	cmp	cx,((10000-7251+255d)/256d)*512d ;got it all?
	jb	chkts4		;no
	mov	si,offset buf+((7251*2) and 777) ;point at SATCNT (at 7251)
	lodsw			;get it
	mov	dx,ax		;save
	lodsw			;get SATPNT (7252)
	cmp	ax,7253		;valid?  (must point into SAT)
	jb	chkts4		;no
	mov	cx,10000-7253	;get # words in SAT
chkts1:	lodsw			;get a word
	xor	ax,7777		;flip
	jz	chkts3		;was all ones, skip
chkts2:	shr	ax,1		;get a bit
	sbb	dx,0		;subtract from DX if segment available
	test	ax,ax		;any free segs left?
	jnz	chkts2		;loop if so
chkts3:	loop	chkts1		;fetch next word
	mov	ax,tss8		;assume success
	test	dx,dx		;did numbers match?
	jz	chkts5		;yes, return with CF=0
chkts4:	stc			;fail
chkts5:	mov	ss:fsys[bp],0	;[back the way it was]
	ret
;
chkxx:	; see if it's a new-format XXDP+ disk
	; (by checking lengths of UFD and bitmap chains)
	mov	ss:pcs[bp],512d	;cluster size is always 512.
	mov	ax,-1		;assume huge disk
	mov	bx,ss:devsiz+6[bp] ;see if high order of size is NZ
	or	bx,ss:devsiz+4[bp]
	or	bx,ss:devsiz+2[bp]
	jnz	chkxx1
	 mov	ax,ss:devsiz[bp] ;no, get actual size
chkxx1:	mov	ss:numclu[bp],ax ;set # of clusters
	mov	di,offset buf	;point at buf
	mov	ax,1		;MFD1 is block #1
	cwd			;DX=0
	mov	cx,ax		;read 1 cluster
	cmp	word ptr ss:med[bp],tu56 ;TU56?
	jne	chkxx2
	 mov	ax,100		;MFD1 is block 100
chkxx2:	call	ss:rdblk[bp]	;read the block
	jc	chkxx3		;error
	cmp	cx,ss:pcs[bp]	;got all of MFD home block?
	jne	chkxx3		;no
	; quick check -- several words are supposed to be 0
	mov	ax,[si]		;add them up
	or	ax,[si+14]
	or	ax,[si+24]
	or	ax,[si+30]
 xor ax,ax ;; mostly NZ on my ZRQC?? boot disk
	jnz	chkxx3
	mov	ax,[si+6]	;get bitmap ptr/length
	mov	cx,[si+10]
	push	word ptr [si+2]	;save UFD ptr/length for later
	push	word ptr [si+4]
	mov	dx,1		;check sequence numbers, starting at 1
	call	chnxx		;check bitmap chain
	pop	cx		;[catch UFD length/ptr]
	pop	ax
	jc	chkxx3
	xor	dx,dx		;no seq #s in UFD chain
	call	chnxx		;check UFD chain
	jc	chkxx3
	mov	ax,xxdp		;[happy]
	ret
chkxx3:	stc			;no match
	ret
;
chnxx:	; check chain -- AX = starting blk, CX = length, DX = 1 to check seq #s
	; (DX = 0 if UFD chain)
	jcxz	chnxx4		;length must be NZ
chnxx1:	push	cx		;save
	push	dx
	xor	dx,dx		;zero-extend
	mov	cx,1		;read 1 blk
	mov	di,offset buf	;point at buf
	call	ss:rdblk[bp]	;read it
	jc	chnxx3		;error
	cmp	cx,ss:pcs[bp]	;got all?
	jne	chnxx3		;no
	pop	dx		;restore
	pop	cx
	lodsw			;fetch link word
	test	dx,dx		;checking seq numbers?
	jz	chnxx2		;no
	mov	bx,ax		;save
	lodsw			;fetch sequence number
	xchg	ax,bx		;switch back
	cmp	bx,dx		;is seq # correct?
	jne	chnxx4		;no
	inc	dx		;ready for next
chnxx2:	test	ax,ax		;end of chain?
	loopnz	chnxx1		;loop if not
	or	ax,cx		;should both be zero now (CF=0)
	jnz	chnxx4		;[no, error]
	ret			;happy, CF=0 already
chnxx3:	pop	dx		;flush stack
	pop	cx
chnxx4:	stc			;no good
	ret
;+
;
; Set all parameters that depend on file system.
;
;-
setsys:	call	finsiz		;finalize size once and for all
	mov	bx,ss:fsys[bp]	;get file system
	mov	si,offset setsy	;pt at table
	call	wdluk		;look it up
	jc	ssys2		;not found?!
	call	ax
	test	byte ptr ds:tronly,-1 ;did they say /RONLY?
	jz	ssys2		;no
	; read-only, shoot out vectors that would try to write
	mov	si,offset rovecs ;point at list
ssys1:	lodsw			;get an entry
	mov	di,ax		;copy
	inc	ax		;end of table?
	jz	ssys2		;yes
	mov	word ptr ss:[bp+di],offset rodev ;pt at routine to bomb out
	jmp	short ssys1	;loop
ssys2:	ret
;
rovecs	dw	wrblk
	dw	wrasc
	dw	dirout
	dw	create
	dw	write
	dw	wlast
	dw	close
	dw	delfil
	dw	wboot
	dw	wipout
	dw	-1
;
; Routines to set up pointers for file system
setsy	dw	cos310,setos	;COS-310
	dw	dosbat,setdb	;DOS/BATCH
	dw	frgn,setfr	;FOREIGN
	dw	ods1,setod	;ODS-1
	dw	os8,setos	;OS/8
	dw	ps8,setos	;PS/8
	dw	putr,setpu	;TSS/8 PUTR
	dw	rsts,setrs	;RSTS
	dw	rt11,setrt	;RT11
	dw	tss8,setts	;TSS/8.24
	dw	xxdp,setxx	;XXDP+
	dw	0
;
setdb:	; set up for DOS/BATCH file system
;;;	mov	ss:rdasc[bp],offset rddb ;routine to read and strip NULs
;;; (never called through RDASC)
	call	dbclu		;figure out cluster size, get buf if needed
	mov	si,offset dbvecs ;pt at vectors
	callr	setvec		;set them, return
;
setfr:	; set up for FOREIGN file system
	mov	si,offset frvecs ;pt at vectors
	callr	setvec		;set them, return
;
setod:	; set up for Files-11 ODS-1 file system
	mov	si,offset odvecs ;pt at vectors
	callr	setvec		;set them, return
;
setos:	; set up for OS/8, PS/8, or COS-310 file system
	mov	ax,ss:med[bp]	;get medium type
	mov	bx,767d		;# usable blocks in RX50
	cmp	ax,rx50
	je	setos1
	add	bx,bx		;*2 for DS RX50
	cmp	ax,rx52
	jne	setos2
setos1:	; RX50 tracks 0, 78, 79, are for slushware on OS/278
	; also last 3 blocks of track 77 (for some reason)
	mov	ss:devsiz[bp],bx ;fix up size
	mov	ss:totblk[bp],bx
setos2:	; special handling for DECtapes
	cmp	ax,tu56		;is that what it is?
	jne	setos4
	cmp	word ptr ss:rdblk[bp],offset rdfil ;must be file
	jne	setos4
	; David Gesswein gave me a PS/8 DECtape image which has 2:1 interleave,
	; since it's all I have to go on I'll assume that's typical (someone
	; please tell me if not!)
	mov	ax,offset psilv	;assume PS/8
	cmp	word ptr ss:fsys[bp],ps8 ;right?
	je	setos3
	 mov	ax,offset osilv	;nope, use OS/8 interleave routine (no op)
setos3:	call	setdta		;set up DTA parameters
setos4:	mov	ss:rdasc[bp],offset rdos ;read and convert to ASCII
	mov	ss:wrasc[bp],offset wros ;convert to 12-bit and write
	call	mntos		;mount appropriate partition, or bomb
	mov	si,offset osvecs ;pt at other vectors
	callr	setvec		;set them, return
;
setpu:	; set up PUTR DECtape format (variant of OS/8 format)
	mov	ax,offset puilv	;interleave routine
	call	setdta		;set up DTA parameters (if it's a DECtape)
	mov	ss:rdasc[bp],offset rdpu ;read and convert to ASCII
	mov	ss:wrasc[bp],offset wrpu ;convert to 12-bit and write
	mov	si,offset osvecs ;pt at other vectors
	callr	setvec		;set them, return
;
setdta:	; set up 12-bit DECtape parameters, AX=interleave routine
	cmp	word ptr ss:med[bp],tu56 ;must be DECtape
	jne	setdt3		;nope
	cmp	word ptr ss:rdblk[bp],offset rdfil ;must be file too
	jne	setdt3		;isn't, no op
	mov	ss:fintlv[bp],ax ;save addr
	mov	bx,129d*2	;assume we have all 129. words
	mov	ax,ss:totsiz+6[bp] ;see if huge
	or	ax,ss:totsiz+4[bp]
	jnz	setdt2		;yes, must be PDP-8 129-word format
	mov	dx,ss:totsiz+2[bp] ;get actual size
	mov	ax,ss:totsiz[bp]
	; see if big enough for 1474. blocks of 129. words each
	.386			;32-bit intermediate values in constants
	cmp	dx,(1474d*129d*2)/10000h ;big enough for 129 word/block format?
	.186
	ja	setdt2		;yes
	jb	setdt1		;no, must be 128 words/block
	.386			;same deal
	cmp	ax,(1474d*129d*2) and 0FFFFh ;big enough?
	.186
	jae	setdt2		;yes
setdt1:	dec	bx		;change 129.*2 to 128.*2
	dec	bx
setdt2:	mov	word ptr ss:secsz[bp],bx ;block size
	mov	ss:rdblk[bp],offset rddt ;read from DECtape image w/interleave
	mov	ss:wrblk[bp],offset wrdt
	mov	ss:rdsec[bp],offset rddtf ;low-level block I/O routines
	mov	ss:wrsec[bp],offset wrdtf
setdt3:	ret
;
unsdta:	; unset SETDTA parameters
	cmp	word ptr ss:med[bp],tu56 ;must be DECtape
	jne	unsdt1		;nope
	cmp	word ptr ss:rdblk[bp],offset rddt ;did we change vectors?
	jne	unsdt1		;isn't, no op
	mov	ss:rdblk[bp],offset rdfil ;back to regular file I/O, for now
	mov	ss:wrblk[bp],offset wrfil
unsdt1:	ret
;
setrs:	; set up for RSTS/E file system
	mov	ss:rdasc[bp],offset rsrd ;stripping NULs done after clu read
	mov	si,offset rsvecs ;pt at other vectors
	callr	setvec		;set them, return
;
setrt:	; set up for RT-11 file system
	mov	ax,ss:med[bp]	;get medium type
	cmp	ax,rl01		;see if DL: or DM:
	je	setrt1
	cmp	ax,rl02
	je	setrt1
	cmp	ax,rk06
	je	setrt1
	cmp	ax,rk07
	jne	setrt2
setrt1:	; DL: or DM:, knock 10. blocks off DEVSIZ for bad block replacements
	sub	ss:devsiz[bp],10d ;do it (DEVSIZ+2 is 0 for these disk types)
;;; this would be a good place to replace RDBLK/WRBLK with routines that check
;;; the bad block replacement table
setrt2:	mov	ss:rdasc[bp],offset rdrt ;routine to read and strip NULs
	call	mntrt		;mount appropriate partition, or bomb
	mov	si,offset rtvecs ;pt at other vectors
	callr	setvec		;set them, return
;
setts:	; set up for TSS/8.24 file system
	mov	ss:rdasc[bp],offset rdpu ;read and convert to ASCII
	mov	ss:wrasc[bp],offset wrpu ;convert to 12-bit and write
	mov	si,offset tsvecs ;pt at other vectors
	callr	setvec		;set them, return
;
setxx:	; set up for new XXDP+ file system
	mov	ss:pcs[bp],512d	;cluster size is always 512.
	mov	ax,-1		;assume huge disk
	mov	bx,ss:devsiz+6[bp] ;see if high order of size is NZ
	or	bx,ss:devsiz+4[bp]
	or	bx,ss:devsiz+2[bp]
	jnz	setxx1
	 mov	ax,ss:devsiz[bp] ;no, get actual size
setxx1:	mov	ss:numclu[bp],ax ;set # of clusters
	mov	si,offset xxvecs ;pt at vectors
	callr	setvec		;set them, return
;
	subttl	parsing routines
;+
;
; Parse directory and filename (assumes dir comes first).
;
; si,cx	command line (updated on return)
; di,bl	passed to FILNAM
; ss:bp	dev rec (preserved)
;
;-
dirfil:	push	bx		;save
	push	di
	call	ss:gdir[bp]	;get directory
	pop	di		;restore
	pop	bx
	;jmp	short filnam
;+
;
; Parse a (possibly wildcarded) filename.
;
; On entry:
; si,cx	cmd line descriptor
; di	buffer in which to put .ASCIZ wildcard (updated on return)
; bl<0>	0 => omitted fields are null
;	1 => omitted fields are *
;
;-
filnam:	jcxz	fn5		;nothing, skip
	mov	dx,si		;to see if we get anything
	xor	ah,ah		;no '.' yet
	cmp	byte ptr [si],'.' ;null name?
	jne	fn1		;no
	test	bl,1		;yes, leave it?
	jz	fn1		;yes
	mov	al,'*'		;save a *
	stosb
fn1:	lodsb			;get a char
	cmp	al,' '		;separator?
	jbe	fn4
	cmp	al,ds:swchar	;switch?
	je	fn4
	cmp	al,'['		;DOS/BATCH UIC?
	je	fn4
	cmp	al,'('		;or RSTS?
	je	fn4
	cmp	al,'#'		;RSTS 1-character logicals?
	je	fn4
	cmp	al,'$'
	je	fn4
	cmp	al,'!'
	je	fn4
	cmp	al,'%'
	je	fn4
	cmp	al,'&'
	je	fn4
	cmp	al,'.'		;ext?
	jne	fn2		;no
	 mov	ah,al		;remember so
fn2:	cmp	al,'a'		;lc?
	jb	fn3
	cmp	al,'z'
	ja	fn3
	and	al,not 40	;convert to uc
fn3:	stosb			;save it
	loop	fn1		;loop
	inc	si		;correct for below
fn4:	dec	si		;unget delimiter
	cmp	dx,si		;did we get anything?
	je	fn5		;no
	test	ah,ah		;was there an ext?
	jnz	fn5		;yes
	mov	al,'.'		;add dot
	stosb
	test	bl,1		;do we care?
	jz	fn5		;no
	mov	al,'*'		;*
	stosb
fn5:	xor	al,al		;AL=0, CF=0
	stosb			;mark end
	ret
;+
;
; Apply a default extension to a DOS filename.
;
; si,cx	describe filename (with 5 bytes available for "/.EXT/<0>")
;	(cx must not be 0)
; di	points at default .ASCIZ extension (with "." at beginning)
; dx	returns ptr to .ASCIZ filename
;
;-
defext:	mov	dx,si		;save a ptr
	xor	bl,bl		;no '.' yet
dfex1:	lodsb			;get a char
	cmp	al,'.'		;possible extension?
	je	dfex3
	cmp	al,'/'		;slash?
	je	dfex2
	cmp	al,'\'		;either way?
	jne	dfex4
dfex2:	xor	al,al		;clear flag (previous "." was part of path)
dfex3:	mov	bl,al		;set "." flag, or clear it
dfex4:	loop	dfex1		;loop
	test	bl,bl		;already got a "." in final path element?
	jnz	dfex6		;yes, we're done
	; copy in default extension
	xchg	si,di		;swap places
dfex5:	lodsb			;get a char
	stosb			;save it
	test	al,al		;done all?  (including final NUL)
	jnz	dfex5		;loop if not
	ret
dfex6:	mov	[si],cl		;mark end (CL=0 from LOOP)
	ret
;+
;
; Convert two TEXT characters to ASCII.
;
; ds:si	pointer to word to LODSW (or enter at TEXT3 if already done)
; es:di	output buffer (including blanks)
; ds:bx	output buffer (blanks squished out)
;
;-
text2:	lodsw			;get them
text3:	; enter here if AX already loaded
	mov	ch,al		;save low char
	sal	ax,1		;left 2
	sal	ax,1
	mov	al,ah		;get the char
	call	text4		;display it
	mov	al,ch		;low char
text4:	and	al,77		;isolate
	jz	text5		;space, special case
	mov	ah,al		;copy
	sub	ah,40		;get sign bits
	and	ah,100		;the one we want
	or	al,ah		;set it (or not)
	stosb
	ret
text5:	mov	al,' '		;blank
	stosb			;not saved in buf at [BX]
	ret
;+
;
; Convert word in AX to .RAD50 at ES:DI.
;
;-
rad50:	mov	bx,50		;radix
	xor	dx,dx		;DX=0
	div	bx		;divide (remainder in DL)
	div	bl		;divide (AL=quotient, AH=rem)
	mov	bl,al		;get first char
	mov	al,ds:r50[bx]
	stosb
	mov	bl,ah		;2nd char
	mov	al,ds:r50[bx]
	stosb
	mov	bl,dl		;3rd char
	mov	al,ds:r50[bx]
	stosb
	ret
;+
;
; Convert word in AX to .RAD50 at ES:DI, squishing out blanks.
;
;-
radnb:	mov	bx,50		;radix
	xor	dx,dx		;DX=0
	div	bx		;divide (remainder in DL)
	div	bl		;divide (AL=quotient, AH=rem)
	mov	bl,al		;get first char
	mov	al,ds:r50[bx]
	cmp	al,' '		;save unless blank
	je	radnb1
	 stosb
radnb1:	mov	bl,ah		;2nd char
	mov	al,ds:r50[bx]
	cmp	al,' '
	je	radnb2
	 stosb
radnb2:	mov	bl,dl		;3rd char
	mov	al,ds:r50[bx]
	cmp	al,' '
	je	radnb3
	 stosb
radnb3:	ret
;+
;
; Get radix-50 filename (no dir path) into RTFILE.
;
; ax	default extension
; si,cx	cmd line descriptor (updated on return)
;
; CF=1 means no filename was given
;
;-
gradfn:	push	ax		;save
	call	skip		;skip to filename
	call	grad		;get filename part
	jc	gradf1		;nothing, error
	mov	ds:rtfile,bx	;save filename
	mov	ds:rtfile+2,dx
	test	al,al		;end of filename?
	jz	gradf2		;yes, use default
	cmp	al,'.'		;otherwise must be .
	jne	gradf3		;nope
	inc	si		;eat the .
	dec	cx
	call	grad		;get extension
	mov	ds:rtfile+4,bx	;save
	clc			;happy return
gradf1:	pop	ax		;flush default extension
	ret
gradf2:	pop	ds:rtfile+4	;catch default extension (CF=0 from TEST AL,AL)
	ret
gradf3:	jmp	synerr		;syntax error
;+
;
; Get a radix-50 filename element (up to 9 chars).
;
; si,cx	input buf descriptor (updated on return)
; bx,dx,di  return first, second, third groups of 3 chars
; al	returns char we stopped on (0 if EOL)
;
; CF=1 if nothing gotten (AL=0).
;
;-
grad:	xor	bx,bx		;in case null
	xor	dx,dx
	call	rword		;get 3 chars
	jc	grad3
	mov	bx,di		;copy
	call	rword		;middle 3
	jc	grad2
	mov	dx,di
	call	rword		;final 3 (RSX only)
	jc	grad2
grad1:	call	rdig		;eat excess digits
	jnc	grad1		;and throw them away
grad2:	clc			;happy
	ret
grad3:	mov	di,dx		;[DI=0 too]
	ret
;+
;
; Get three .RAD50 chars.
;
; si,cx	input buf descriptor (updated on return)
; di	returns RAD50 value (0 if CF=1)
; al	returns non-RAD50 value we stopped on, if CF=1
;
; AH trashed, others preserved.
;
;-
rword:	mov	di,dx		;save
	xor	dx,dx		;init
	call	rdig		;get a digit
	jc	rword2		;nothing at all
	mov	dx,50*50	;multiplier
	mul	dx		;put in posn
	mov	dx,ax		;save in DX
	call	rdig		;2nd digit
	jc	rword1
	mov	ah,50		;multiplier
	mul	ah		;put in posn
	add	dx,ax
	call	rdig		;3rd digit
	jc	rword1
	add	dx,ax
rword1:	clc			;happy
rword2:	xchg	dx,di		;[restore]
	ret
;+
;
; Get radix 50 digit into AX.
; Returns CF=1 and AL=char if not a radix 50 char (0 if EOL).
;
;-
rdig:	xor	al,al		;in case EOL
	jcxz	rdig2		;end of line, skip
	lodsb			;get a char
	dec	cx		;assume we're taking it
	cbw			;AH=0 if valid
	cmp	al,'$'		;$?
	je	rdig3
	cmp	al,'%'		;%?
	je	rdig4
	cmp	al,'0'		;digit?
	jb	rdig1
	cmp	al,'9'
	jbe	rdig5		;yes
	cmp	al,'A'		;letter?
	jb	rdig1
	cmp	al,'Z'
	jbe	rdig7		;yes
	cmp	al,'a'		;lower case letter?
	jb	rdig1
	cmp	al,'z'
	jbe	rdig6		;yes
rdig1:	dec	si		;invalid, unget
	inc	cx
rdig2:	stc			;unhappy return
	ret
rdig3:	mov	al,33		;$ (CF=0 from CMP)
	ret
rdig4:	mov	al,35		;% (CF=0 from CMP)
	ret
rdig5:	sub	al,'0'-36	;digits start at 36 (CF=0)
	ret
rdig6:	and	al,not 40	;convert to upper
rdig7:	sub	al,'A'-1	;letters start at 1 (CF=0)
	ret
;+
;
; Convert 12-bit word in AX to SIXBIT at ES:DI.
;
;-
sixbit:	mov	cl,6		;shift count
	push	ax		;save
	shr	ax,cl		;right-justify
	add	al,40		;convert to ASCII
	stosb
	pop	ax		;restore
	and	al,77		;isolate
	add	al,40		;convert to ASCII
	stosb
	ret
;+
;
; Get a TEXT filename element (up to 6 chars).
;
; si,cx	input buf descriptor (updated on return)
; bx,dx,di  return first, second, third pairs of chars
; al	returns char we stopped on (0 if EOL)
;
; CF=1 if nothing gotten (AL=0).
;
;-
gtext:	xor	bx,bx		;in case null
	xor	dx,dx
	xor	di,di
	call	tchar		;get a char
	jc	gtxt3
	mov	bh,al		;copy
	shr	bx,1		;shift into place
	shr	bx,1
	call	tchar		;2nd digit
	jc	gtxt2
	or	bl,al
	call	tchar		;3rd digit
	jc	gtxt2
	mov	dh,al		;copy
	shr	dx,1		;shift into place
	shr	dx,1
	call	tchar		;4th digit
	jc	gtxt2
	or	dl,al
	call	tchar		;5th digit
	jc	gtxt2
	xchg	al,ah		;><
	shr	ax,1		;shift into place
	shr	ax,1
	mov	di,ax		;save
	call	tchar		;6th digit
	jc	gtxt2
	or	di,ax
gtxt1:	call	tchar		;eat excess digits
	jnc	gtxt1		;and throw them away
gtxt2:	clc			;happy
gtxt3:	ret
;+
;
; Get TEXT char into AL (AH=0).
; Returns CF=1 and AL=char if not a radix 50 char (0 if EOL).
;
;-
tchar:	xor	al,al		;in case EOL
	jcxz	tchr2		;end of line, skip
	lodsb			;get a char
	dec	cx		;assume we're taking it
	cmp	al,'0'		;digit?
	jb	tchr1
	cmp	al,'9'
	jbe	tchr3		;yes
	cmp	al,'A'		;letter?
	jb	tchr1
	cmp	al,'Z'
	jbe	tchr3		;yes
	cmp	al,'a'		;lower case letter?
	jb	tchr1
	cmp	al,'z'
	jbe	tchr3		;yes
tchr1:	dec	si		;invalid, unget
	inc	cx
tchr2:	stc			;unhappy return
	ret
tchr3:	and	ax,77		;truncate, AH=0, CF=0
	ret
;+
;
; Find length of .ASCIZ string.
;
; si	points to string (preserved)
; cx	returns length
;
;-
lenz:	mov	cx,si		;copy
lenz1:	lodsb			;get a byte
	test	al,al		;end?
	jnz	lenz1		;loop if not
	xchg	si,cx		;restore si, copy
	sub	cx,si		;find length (including ^@)
	dec	cx		;(not including ^@)
	ret
;+
;
; Check for a wildcard (or not) match.
;
; On entry:
; si	points to .ASCIZ /wildcard/
; di	points to .ASCIZ /test string/
;
; On return, ZF is set if it was a match.
;
;-
wild:	lodsb			;get a byte
	cmp	al,'?'		;match 1 char?
	je	wild2		;yes
	cmp	al,'*'		;match any # of chars?
	je	wild3		;yes
	; not wildcard char - just check it
	scasb			;is it the same?
	jne	wild1		;no, return
	test	al,al		;yes, end (of both)?
	jnz	wild		;loop if not
wild1:	ret
wild2:	; ? - match exactly one char
	xor	al,al		;load 0
	scasb			;skip 1 char
	jnz	wild		;OK if not end of string
	or	al,1		;Z=0
	ret
wild3:	; * - match anything
	push	si		;save
	push	di
	call	wild		;check (recursively for each posn of di)
	pop	di		;restore
	pop	si
	jz	wild1		;got it
	mov	al,[di]		;get char
	inc	di		;skip
	cmp	al,'.'		;can't match .
	je	wild4		;skip if that's it
	test	al,al		;end?
	jnz	wild3		;loop if not end
wild4:	or	al,1		;never found it, Z=0
	ret
;+
;
; Map a test string from a source wildcard to a destination wildcard, if it
; matches the source wildcard.
;
; The idea is, if the user says COPY *.FOR *.FTN then we should (for example)
; copy TEST.FOR to TEST.FTN.
;
; On entry:
; si	points to .ASCIZ /source wildcard/
; di	points to .ASCIZ /test string/
; bx	points to .ASCIZ /destination wildcard/
; dx	points to output buffer (receives .ASCIZ string)
;
; On return, ZF=1 if it was a match.
;
;-
mapwld:	lodsb			;get a byte
	cmp	al,'?'		;match 1 char?
	je	mpwld2		;yes
	cmp	al,'*'		;match any # of chars?
	je	mpwld3		;yes
	; not wildcard char - just check it
	scasb			;is it the same?
	jne	mpwld1		;no, return
	test	al,al		;yes, end (of both)?
	jnz	mapwld		;loop if not
	call	mpwcpy		;copy whatever's left
	jnc	mpwld6		;should be no more wildcard chars
mpwld1:	ret
mpwld2:	; ? - match exactly one char
	mov	ah,[di]		;get the char
	inc	di		;skip it
	test	ah,ah		;EOL?
	jz	mpwld5		;yes, bugged
	cmp	ah,'.'		;can't match .
	je	mpwld5		;skip if that's it
	call	mpwcpy		;copy up to ?
	jc	mpwld6		;missing, invalid dest wildcard
	cmp	al,'?'		;it IS a ? right?
	jne	mpwld6		;invalid if not
	xchg	dx,di		;swap for now
	mov	al,ah		;copy
	stosb			;save
	xchg	dx,di		;restore things
	jmp	short mapwld
mpwld3:	; * - match anything
	call	mpwcpy		;copy up to *
	jc	mpwld6		;invalid if missing
	cmp	al,'*'		;it IS a * right?
	jne	mpwld6		;invalid if not
mpwld4:	push	si		;save
	push	di
	push	bx
	push	dx
	call	mapwld		;check (recursively for each posn of DI)
	pop	dx		;restore
	pop	bx
	pop	di
	pop	si
	jz	mpwld1		;got it
	mov	al,[di]		;get char
	inc	di		;skip
	cmp	al,'.'		;can't match .
	je	mpwld5		;skip if that's it
	xchg	dx,di		;swap
	stosb			;save char
	xchg	dx,di		;restore
	test	al,al		;end?
	jnz	mpwld4		;loop if not end
mpwld5:	or	al,1		;never found it, ZF=0
	ret
mpwld6:	cram	'?Incompatible source and destination wildcards',error
;
mpwcpy:	; copy non-wildcard part of output spec up to next wildcard char
	; return CF=1 if reached end of string w/no wildcard
	xchg	bx,si		;swap for a second
	xchg	dx,di
mpwcp1:	lodsb			;get a char
	test	al,al		;end?
	jz	mpwcp2
	cmp	al,'?'		;wildcard char?
	je	mpwcp3
	cmp	al,'*'
	je	mpwcp3
	stosb			;no, save
	jmp	short mpwcp1	;loop
mpwcp2:	stosb			;write EOL
	stc			;EOL
mpwcp3:	xchg	bx,si		;[restore]
	xchg	dx,di
	ret
;
	subttl	DOS (native) file structure
;+
;
; Print volume name.
;
;-
dosvn:	mov	dx,offset dosroo ;pt at wildcard
	mov	cx,08h		;find VOLSER
	mov	ah,4Eh		;func=find first match
	int	21h		;do it
	jc	nolbl		;not found, no label (leave as blanks)
	cram	' is '		;it worked
	mov	si,80h+30d	;pt at filename
	mov	dx,di		;init
dosvn1:	lodsb			;get a byte
	test	al,al		;end?
	jz	dosvn3
	cmp	al,'.'		;period?
	jne	dosvn2
	cmp	si,80h+30d+8d+1	;yes, after 8 chars of vol name?
	je	dosvn1		;yes, ignore it
dosvn2:	stosb			;copy
	cmp	al,' '		;blank?
	jne	dosvn1		;loop if not
	lea	dx,[di-1]	;save ptr to blank
	jmp	short dosvn1
dosvn3:	cmp	al,' '		;were there trailing blanks?
	jne	dosvn4		;no
	mov	di,dx		;back up to last blank
dosvn4:	ret
nolbl:	cram	' has no label'
	ret
;+
;
; Set default dir for DOS.
;
;-
dosds:	push	si		;save
	push	cx
	mov	cx,64d		;get buf for path
	call	getmem
	mov	ss:actdir[bp],si ;save ptr to dir name
	mov	di,si		;copy
	mov	dl,byte ptr ss:logd+1[bp] ;get drive letter
	sub	dl,'A'-1	;convert to 1=A, 2=B, etc.
	mov	ah,47h		;func=get curr directory
	int	21h
	jc	dosds5
	; normalize path separators
	mov	ah,'/'		;SWITCHAR is usually '/'
	cmp	byte ptr ds:swchar,ah ;is it?
	jne	dosds1		;no, use '/' for path separator
	 mov	ah,'\'		;yes, use '\'
dosds1:	lodsb			;get a char
	test	al,al		;end of string?
	jz	dosds4
	cmp	al,'/'		;forward slash?
	je	dosds2
	cmp	al,'\'		;backslash?
	jne	dosds3
dosds2:	mov	al,ah		;normalize slashes
dosds3:	stosb
	jmp	short dosds1
dosds4:	pop	cx		;restore
	pop	si
	ret
dosds5:	jmp	dirio		;dir I/O error
;+
;
; Save DOS CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
dossav:	mov	ax,ss:actdir[bp] ;get ptr to active dir name
	mov	ds:savdir[bx],ax
	ret
;+
;
; Restore DOS CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
dosrst:	mov	ax,ds:savdir[bx] ;get ptr to active dir name
	mov	ss:actdir[bp],ax
	ret
;
dosdh:	; DOS dir listing header (there is none, but init counters)
	xor	ax,ax		;load 0
	mov	ds:nfile,ax	;no files yet
	mov	ds:nblk,ax	;use NBLK for byte count
	mov	ds:nblk+2,ax
	ret
;+
;
; Init for dir lookup (called through SS:DIRINP[BP]).
;
;-
dosdi:	mov	di,offset buf	;pt at temp buf
	mov	dx,di		;save ptr
	mov	al,byte ptr ss:logd+1[bp] ;get drive letter
	mov	ah,':'		;d:
	stosw
	mov	cx,1		;filename prefix
	call	dosdn		;add dir name
	mov	ax,".*"		;'*.'
	stosw
	mov	ax,'*'		;'*'<0>
	stosw
	mov	cx,177761	;everything but volser, system, hidden
	mov	ah,4Eh		;func=find first match
	int	21h		;do it
	sbb	ax,ax		;-1 if err, else 0
	not	ax		;flip
	mov	ds:matchf,ax	;save
	ret
;+
;
; Get next dir entry (called through SS:DIRGET[BP]).
;
; On return:
; CF=1		no more entries
; CF=0 ZF=1	empty block (for file systems that support them)
; CF=0 ZF=0	file
;
; If file, DS:LBUF contains filename for display, DI points to end of display
; filename and beginning of .ASCIZ filename with blanks collapsed for
; comparison.
;
; For example:
; FOO   .SVFOO.SV
; ^LBUF    ^DI   ^0
;
; Various information about the file is stored in global variables, some of it
; in file system-specific variables (to be interpreted by the system-specific
; DIRDIS routines) and the rest in standard places:
;
; DS:LBUF	DB filename, see above
; DS:FBCNT	DD max equivalent file size in 8-bit bytes depending on BINFLG
;		(may really be less if ASCII file due to stripping nulls, etc.)
; DS:FDATF	DB NZ if date is valid:
;  DS:FDAY	DB day (1-31.)
;  DS:FMONTH	DB month (1-12.)
;  DS:FYEAR	DW year (4 digits)
; DS:FTIMF	DB NZ if time is valid:
;  DS:FSEC	DB sec (0-59. or -1 if not supported)
;  DS:FMIN	DB minute (0-59.)
;  DS:FHOUR	DB hour (0-23.)
;
;-
dosdg:	; get DOS dir entry
	cmp	ds:matchf,0	;error last time?
	jnz	dsdg1		;no
	stc			;no more files
	ret
dsdg1:	mov	word ptr ds:ftimf,0 ;no date or time
	mov	si,80h+21d	;pt at file info
	; get file type
	lodsb			;get it
	mov	ds:fattr,al	;save
	; DOS time is 16 bits:  HHHHHMMMMMMSSSSS
	; HHHHH=hour (0-23.), MMMMMM=minute (0-59.), SSSSS=second/2 (0-29.)
	; note that 12 midnight is not representable because 0 means "no time"
	lodsw			;get time
	test	ax,ax		;was there one?
	jz	dsdg2
	mov	bl,al		;get sec/2
	and	bl,37		;isolate
	sal	bl,1		;*2=seconds
	mov	ds:fsec,bl	;save
	mov	cl,3		;bit count
	shr	ax,cl		;align hour
	dec	cx		;count=2
	shr	al,cl		;align minute
	and	ax,1F3Fh	;isolate
	mov	word ptr ds:fmin,ax ;save
	inc	ds:ftimf	;time is valid
dsdg2:	; DOS date is 16 bits:  YYYYYYYMMMMDDDDD
	; YYYYYYY is year-1980, MMMM and DDDD are the usual
	lodsw			;get date
	test	ax,ax		;was there one?
	jz	dsdg3
	mov	cl,3		;bit count
	mov	bx,ax		;copy
	sal	bx,cl		;shift month into place
	mov	bl,al		;get day again
	and	bx,0F1Fh	;isolate month,,day
	mov	word ptr ds:fday,bx ;save
	mov	al,ah		;get year-1980
	shr	al,1		;shift into place
	cbw			;AH=0
	add	ax,1980d	;add base
	mov	ds:fyear,ax	;save
	inc	ds:fdatf	;date is valid
dsdg3:	; get file size
	lodsw			;low word
	mov	ds:fbcnt,ax
	lodsw			;high word
	mov	ds:fbcnt+2,ax
	; copy and format the filename
	mov	di,offset lbuf	;pt at line buffer
	mov	cx,12d/2	;wc
	mov	ax,"  "		;data
	rep	stosw		;blank 12. bytes (FILENAME.EXT)
	mov	cx,di		;save ptr to begn of filename
	; copy the filename as-is
	xor	ah,ah		;no '.' yet
dsdg4:	lodsb			;get next byte
	test	al,al		;end?
	jz	dsdg5
	cmp	al,'.'		;dot?
	jne	$+4		;no
	 mov	ah,al		;yes, remember
	stosb			;save
	jmp	short dsdg4	;loop
dsdg5:	; add dot if there was none (. and .. fool us the right way here)
	test	ah,ah		;was there a dot?
	jnz	dsdg6		;yes (AL=0)
	mov	al,'.'		;no, write one
	stosb
	xor	al,al		;load a 0
dsdg6:	stosb			;mark end
	; now copy the filename alone, or the whole thing if . or ..
	mov	di,offset lbuf	;pt at buf again
	mov	si,offset lbuf+12d ;pt at begn of copied filename
	cmp	byte ptr [si],'.' ;does it start with .?
	je	dsdg9		;yes, extension is meaningless
dsdg7:	; copy filename (up to .)
	lodsb			;get a char
	cmp	al,'.'		;end of filename?
	je	dsdg8		;yes, skip
	stosb			;save
	jmp	short dsdg7	;loop
dsdg8:	; end of filename, copy '.' and extension
	mov	di,offset lbuf+8d ;pt at ext
	stosb			;save the '.'
dsdg9:	; copy extension (or whole filename if . or ..)
	lodsb			;get a char
	test	al,al		;end of filespec?
	jz	dsdg10		;yes, skip
	stosb			;save
	jmp	short dsdg9	;loop
dsdg10:	; make a copy of the squished name for opening, etc.
	mov	si,offset lbuf+12d ;pt at filename
	mov	di,offset dosfil ;pt at buffer
dsdg11:	lodsb			;get a byte
	stosb			;save
	test	al,al		;end?
	jnz	dsdg11		;loop until copied
	; search for next file
	mov	dx,80h		;some people think DOS 3.0+ needs DTA in DS:DX
	mov	ah,4Fh		;func=find next
	int	21h		;do it
	adc	ds:matchf,0	;up to 0 if err
	mov	di,offset lbuf+12d ;pt at filename
	mov	al,ds:fattr	;get attribute
	not	al		;flip
	and	al,20		;CF=0, ZF=0 unless directory
	ret
;
dossum:	; DOS dir listing summary
	mov	di,offset lbuf	;pt at buf
	; print # files, # bytes
	mov	ax,ds:nfile	;get # files
	push	ax		;save
	mov	cx,9d		;field width
	xor	dx,dx		;0-extend
	call	cnum		;print
	cram	' file'
	pop	ax		;restore #
	call	adds		;add s
	mov	cx,offset lbuf+28d ;pt at dest col
	sub	cx,di		;find # blanks to write
	mov	ax,ds:nblk	;get byte count
	mov	dx,ds:nblk+2
	push	dx		;save
	push	ax
	call	cnum
	cram	' byte'
	pop	ax		;restore
	pop	dx
	call	dadds		;add s
	call	flush		;flush line
	; print # free bytes
	mov	dl,byte ptr ss:logd+1[bp] ;get drive letter
	sub	dl,'A'-1	;convert to 1=A, 2=B, etc.
	mov	ah,36h		;func=get disk size info
	int	21h		;AX=clustersize, BX=avail clusters, CX=b/s
	mul	cx		;calc bytes/cluster
	mov	cx,ax		;save low order
	mov	ax,bx		;get free space
	mul	dx		;find high order partial product
	xchg	ax,cx		;save middle word, get old low order
	mul	bx		;find low order partial product
	add	dx,cx		;find low 32 bits of 48-bit product
	push	dx		;save
	push	ax
	mov	cx,28d		;field size
	call	cnum		;print it
	cram	' byte'
	pop	dx		;restore
	pop	ax
	call	dadds		;add s
	cram	' free'
	callr	flush		;flush, return
;+
;
; Put current directory name in output buffer.  Called through SS:DIRNAM[BP].
;
; ss:bp	log dev rec
; es:di	buffer ptr (updated on exit)
; cx	0 if for prompt (omit trailing '\' for DOS)
;	1 if path prefix
; dx	preserved
;
;-
dosdn:	; print DOS dir name (CX=1 to add \, 0 not to)
	mov	al,'/'		;SWITCHAR is usually '/'
	cmp	byte ptr ds:swchar,al ;is it?
	jne	dosdn1		;no, use '/' for path separator
	 mov	al,'\'		;yes, use '\'
dosdn1:	stosb			;write the first one for sure
	mov	ah,al		;save
	mov	si,ss:actdir[bp] ;get ptr to dir
	lodsb			;get a byte
	test	al,al		;root?
	jz	dosdn3		;yes, leave it regardless of cx
dosdn2:	stosb			;save it
	lodsb			;get next
	test	al,al		;end?
	jnz	dosdn2		;loop if not
	mov	al,ah		;get path sep
	rep	stosb		;write one if CX=1
dosdn3:	ret
;
dosddl:	; print dir for drive in dl
	mov	ah,'/'		;SWITCHAR is usually '/'
	cmp	byte ptr ds:swchar,ah ;is it?
	jne	dospd1		;no, use '/' for path separator
	 mov	ah,'\'		;yes, use '\'
dospd1:	mov	al,ah		;add leading slash
	stosb
	mov	si,di		;copy
	sub	dl,'A'-1	;convert to 1=A, 2=B, etc.
	push	ax		;save path separater
	mov	ah,47h		;func=get curr directory
	int	21h
	pop	ax		;[restore]
	jc	dospd7
	cmp	byte ptr [si],0	;nothing?  (i.e. root)
	jz	dospd6		;yes, give just one slash (regardless of cx)
dospd2:	lodsb			;get a char
	test	al,al		;end of string?
	jz	dospd5
	cmp	al,'/'		;forward slash?
	je	dospd3
	cmp	al,'\'		;backslash?
	jne	dospd4
dospd3:	mov	al,ah		;normalize slashes
dospd4:	stosb
	jmp	short dospd2
dospd5:	mov	al,ah		;add trailing \ if filename follows
	rep	stosb		;0 or 1
dospd6:	ret
dospd7:	jmp	dirio		;dir I/O error
;+
;
; Change DOS directory.
;
;-
doscd:	mov	di,offset buf	;pt at buffer
	mov	dx,di		;with dx too
	mov	al,byte ptr ss:logd+1[bp] ;get drive letter
	mov	ah,':'		;d:
	stosw
doscd1:	lodsb			;get a char
	cmp	al,' '		;blank or ctrl?
	jbe	doscd2		;yes, ignore
	stosb
doscd2:	loop	doscd1		;loop through all
	xor	al,al		;mark end
	stosb
	mov	ah,3Bh		;func=CHDIR
	int	21h
	jc	doscd3		;err
	ret
doscd3:	jmp	dnf
;+
;
; Get directory name, make it temporarily current.
;
;-
dosgd:	jcxz	dsgd5		;done if nothing
	mov	al,[si]		;get curr char
	cmp	al,' '		;blank or ctrl char?
	jbe	dsgd5
	cmp	al,ds:swchar	;switchar?
	je	dsgd5
	cmp	al,'/'		;path from root?
	je	dsgd1
	cmp	al,'\'		;either way
	jne	dsgd2
dsgd1:	mov	bx,ss:actdir[bp] ;yes, get ptr to begn of dir name
	mov	byte ptr [bx],0	;start from root
	inc	si		;eat the '\'
	dec	cx
dsgd2:	; parse next dir name
	jcxz	dsgd5		;done if nothing
	push	si		;save posn
	push	cx
dsgd3:	lodsb			;get a char
	cmp	al,' '		;blank or ctrl char?
	jbe	dsgd4
	cmp	al,ds:swchar	;switch?
	je	dsgd4
	cmp	al,'/'		;path sep?
	je	dsgd6
	cmp	al,'\'
	je	dsgd6
	loop	dsgd3		;loop if not
dsgd4:	pop	cx		;unget
	pop	si
dsgd5:	ret
dsgd6:	; got a path element
	dec	cx		;count it
	mov	ah,'/'		;get path sep
	cmp	ah,ds:swchar	;can't use / if it's for switches
	jne	$+4
	 mov	ah,'\'		;use \ instead
	mov	bx,si		;save cmd line ptr
	mov	dx,cx		;and remaining length
	mov	di,ss:actdir[bp] ;get ptr to path as it is now
	xor	al,al		;look for 0
	mov	cx,64d		;length
	repnz	scasb		;find it (always succeeds)
	mov	di,64d-1	;find length (not including NUL)
	sub	di,cx
	pop	cx		;restore length
	pop	si		;and ptr
	sub	cx,dx		;find length of pathname element
	dec	cx		;don't count the path sep
	jz	dsgd10		;syntax error if null
	cmp	byte ptr [si],'.' ;. or ..?
	je	dsgd12		;yes, see which
	; add new dir name onto end of path, if it will fit
	push	di		;save
	test	di,di		;is there anything already?
	jz	$+3
	 inc	di		;yes, we'll add a \ to it
	add	di,cx		;find total length with everything
	cmp	di,64d-1	;will it fit in 64. bytes with the NUL?
	pop	di		;[restore DI]
	ja	dsgd11		;no
	add	di,ss:actdir[bp] ;add base
	cmp	di,ss:actdir[bp] ;did we move?
	je	dsgd7		;no, starting at root
	mov	al,ah		;yes, add path sep
	stosb
dsgd7:	lodsb			;get a char
	cmp	al,'a'		;lower case?
	jb	dsgd8
	cmp	al,'z'
	ja	dsgd8
	and	al,not 40	;yes, convert to upper
dsgd8:	stosb			;save
	loop	dsgd7		;copy whole dir name
	cmp	al,'.'		;was last char .?
	jne	$+3
	 dec	di		;yes, un-put
	xor	al,al		;mark end
	stosb
dsgd9:	mov	si,bx		;restore
	mov	cx,dx
	jmp	short dsgd2
dsgd10:	jmp	synerr		;null dir name
dsgd11:	jmp	lngnam		;name too long
dsgd12:	; . or .. -- see which
	inc	si		;eat the .
	dec	cx
	jz	dsgd9		;that's it, skip it
	lodsb			;get another char
	dec	cx		;count it
	jnz	dsgd10		;that should be all
	cmp	al,'.'		;must be ..
	jne	dsgd10
	mov	si,ss:actdir[bp] ;get ptr to dir
	cmp	byte ptr [si],0	;is it root?
	jz	dsgd9		;yes, don't go above
	mov	di,si		;in case only one path element
	jmp	short dsgd14	;into loop
dsgd13:	mov	di,si		;save a copy
	dec	di		;back up
dsgd14:	lodsb			;get a char
	cmp	al,ah		;is this a path sep?
	je	dsgd13		;yes, save posn
	test	al,al		;end of path?
	jnz	dsgd14		;loop if not
	stosb			;lop off last dir name (AL=0)
	jmp	short dsgd9	;around for more
;+
;
; Display dir entry from most recent DIRGET.  Called through SS:DIRDIS[BP].
;
; es:di	buf ptr as returned by DIRGET (updated on return)
;
;-
dosdd:	; display DOS dir entry
	inc	ds:nfile	;bump # files
	test	byte ptr ds:fattr,20 ;directory?
	jz	dosdd1
	; it's a directory, length is meaningless
	cram	'  <DIR>    '
	jmp	short dosdd2
dosdd1:	; it's a file, give the size
	mov	cx,11d		;# spaces
	mov	ax,ds:fbcnt	;byte count
	mov	dx,ds:fbcnt+2
	add	ds:nblk,ax	;(add to total)
	adc	ds:nblk+2,dx
	call	cnum		;print it right-justified
dosdd2:	; add date, if any
	cmp	byte ptr ds:fdatf,0 ;was there a date?
	jz	dosdd3		;no
	mov	ax,"  "		;2 blanks
	stosw
	call	pdate		;print date
	cmp	byte ptr ds:ftimf,0 ;time?
	jz	dosdd3		;no
	mov	ax,"  "		;2 more blanks
	stosw
	call	ptime		;print time
dosdd3:	ret
;+
;
; Open most recently DIRGETted file for input.  Called through SS:OPEN[BP].
;
; bp	input dev rec
;
; Returns CF=1 if can't proceed (tried to read directory).
;
;-
dosop:	; open DOS file for input
	test	byte ptr ds:fattr,20 ;was it a directory?
	jnz	dosop2		;yes, can't read that
	; whip up filename in BUF
	mov	di,offset buf	;pt at temp buf
	mov	dx,di		;save ptr
	mov	al,byte ptr ss:logd+1[bp] ;get drive letter
	mov	ah,':'		;d:
	stosw
	mov	cx,1		;filename prefix
	call	dosdn		;add dir name
	mov	si,offset dosfil ;pt at DOS filename
dosop1:	lodsb			;get a char
	stosb			;save
	test	al,al		;end?
	jnz	dosop1		;loop if not
	call	openro		;open file
	jc	dosop3
	mov	ds:inhnd,ax	;save
	ret
dosop2:	stc			;error return
	ret
dosop3:	cram	'?Open error',error
;+
;
; Read file data.
;
; On entry:
; es:di	ptr to buf (normalized)
; cx	# bytes free in buf (FFFF if >64 KB)
;
; On return:
; cx	# bytes read (may be 0 in text mode if block was all NULs etc.)
;
; CF=1 if nothing was read:
;  ZF=1 end of file
;  ZF=0 buffer not big enough to hold anything, needs to be flushed
;
;-
dosrd:	; read DOS file data
	jcxz	dosrd1		;nothing to read
	mov	bx,ds:inhnd	;get file handle
	push	ds		;save DS
	push	es		;copy ES to DS
	pop	ds
	mov	dx,di		;point at buf
	mov	ah,3Fh		;func=read
	int	21h
	pop	ds		;[restore]
	jc	dosrd3		;error
	mov	cx,ax		;copy
	test	cx,cx		;get anything?
	jz	dosrd2		;no, ZF=1, set CF
	ret
dosrd1:	or	al,1		;ZF=0
dosrd2:	stc			;CF=1
	ret
dosrd3:	jmp	rderr
;+
;
; Close input file opened by SS:OPEN[BP].  Called through SS:RESET[BP].
;
;-
dosrs:	; reset input file
	mov	bx,-1		;mark as closed
	xchg	bx,ds:inhnd	;get handle
	mov	ah,3Eh		;func=close
	int	21h
	ret
;+
;
; Create output file.
;
; ds:si	.ASCIZ filename
; DS:FBCNT max size of file in bytes
;
;-
doscr:	; whip up filename in BUF
	mov	di,offset buf	;pt at temp buf
	mov	dx,di		;save ptr
	mov	al,byte ptr ss:logd+1[bp] ;get drive letter
	mov	ah,':'		;d:
	stosw
	mov	cx,1		;filename prefix
	push	si		;save
	call	dosdn		;add dir name
	pop	si		;restore
doscr1:	lodsb			;get a char
	stosb			;save
	test	al,al		;end?
	jnz	doscr1		;loop if not
	call	crt		;create file
	jc	doscr2
	mov	ds:outhnd,ax	;save handle
	ret
doscr2:
;;; if error=protected file of same name exists, check if ds:ovride set,
;;; unprotect and retry if so
	jmp	crerr
;+
;
; Write data to output file (handle conversion if ASCII mode).
; Called through SS:WRITE[BP].
;
; es:si	buffer
; cx	length of data
;
; On return, CX contains the number of bytes actually written (presumably a
; multiple of the clustersize).
;
;-
doswr:	; write to DOS file
	push	ds		;save
	mov	dx,si		;copy ptr
	mov	bx,ds:outhnd	;get handle
	push	es		;copy ES to DS
	pop	ds
	mov	ah,40h		;func=write
	int	21h
	pop	ds		;[restore]
	jc	$+3
	 ret
	jmp	wrerr
;+
;
; Close output file.  Called through SS:CLOSE[BP].
;
; ss:bp	log dev rec
;
;-
doscl:	; close DOS output file
	; first set date and time
	xor	ax,ax		;(assume no time)
	cmp	ds:fdatf,al	;do we know date?
	jz	doscl3		;no, leave time/date as-is (=now)
	; convert date to DOS disk format
	mov	dx,ds:fyear	;get year
	sub	dx,1980d	;subtract base
	jnc	$+4
	 xor	dx,dx		;can't go earlier than 1980.
	cmp	dx,119d		;DOS dates run out at 2099d (postdated file?)
	jbe	$+5
	 mov	dx,119d		;stop if after (?!)
	mov	cl,4		;make space for month
	sal	dx,cl
	or	dl,ds:fmonth	;OR in month
	inc	cx		;make space for day
	sal	dx,cl
	or	dl,ds:fday	;OR in day
	; convert time to DOS disk format (or use 0 if undefined)
	cmp	ds:ftimf,al	;is time defined?
	jz	doscl1		;no, leave AX=0
	mov	al,ds:fhour	;get hour
	mov	cl,6		;make space for minute
	sal	ax,cl
	or	al,ds:fmin	;OR in minute
	dec	cx		;make space for second/2
	sal	ax,cl
	mov	ch,ds:fsec	;get second
	sar	ch,1		;sec/2
	js	$+4		;<0, seconds not supported by file system
	 or	al,ch		;OR it in
	test	ax,ax		;exactly 12 midnight?
	jnz	doscl2
doscl1:	inc	ax		;yes, say 00:00:02 or DIR won't show it
doscl2:	mov	cx,ax		;copy time
	mov	bx,ds:outhnd	;get handle
	mov	ax,5701h	;func=set file date/time
	int	21h
doscl3:	mov	bx,-1		;mark as closed
	xchg	bx,ds:outhnd	;get handle
	mov	ah,3Eh		;func=close
	int	21h
	ret
;+
;
; Delete file.  Called through SS:DELFIL[BP] after DIRGET.
;
;-
dosdl:	; whip up filename in BUF
	mov	di,offset buf	;pt at temp buf
	mov	dx,di		;save ptr
	mov	al,byte ptr ss:logd+1[bp] ;get drive letter
	mov	ah,':'		;d:
	stosw
	mov	cx,1		;filename prefix
	call	dosdn		;add dir name
	mov	si,offset dosfil ;pt at DOS filename
dosdl1:	lodsb			;get a char
	stosb			;save
	test	al,al		;end?
	jnz	dosdl1		;loop if not
	mov	ah,41h		;func=delete
	int	21h		;give it a shot
	jc	dosdl2
	ret
dosdl2:	jmp	delerr
;
	subttl	DOS/BATCH file structure
;
	comment	_

DOS/BATCH uses a link word in the first word of most kinds of blocks to point
at the next block in a chain, ending with a block whose link word is 0.
Contiguous files are the only exception I know of.  I don't know how BADB.SYS
works if there's more than one bad block, maybe the same file appears more than
once?  I can't find one that's >1 block long (it covers block 2 on a freshly
zeroed error-free RK05 for some reason).

Block 0 is the boot block
Block 1 (or 64. if DECtape) is the MFD1 (MFD home block):

+0	.word	mfd	;link to first block of MFD2 chain
+2	.word	?	;dunno, I've seen 1 and 5 here
			;XXDP+ docs say it's the interleave
			;(used as block allocation hint only)
+4	.word	map	;first block of bitmap
+6	.blkw	n	;complete list of blocks in bitmap (first same as +4)
			;0 after last one

Each MFD2 block (if there's more than one) has a link to the next block (or 0)
in the first word.  Starting at offset 2 there are as many four-word UFD
pointers as fit in the cluster:

+0	.word	uic	;user ID code (PPN), 0 for empty slot
+2	.word	ufd	;beginning of UFD chain (or 0 if not yet created)
+4	.word	size	;# words per UFD entry (normally 9.)
+6	.word	?	;??? seems to be 0 (XXDP+ doc agrees)

New XXDP+ disks combine MFD1 and MFD2 into one block (at device block #1):

+0	.word	0
+2	.word	ufd	;beginning of UFD chain
+4	.word	ufdlen	;# blocks in UFD chain
+6	.word	map	;beginning of bitmap chain
+10	.word	maplen	;# blocks in bitmap chain
+12	.word	mfd12	;"pointer to MFD 1/2" (isn't that this block???)
+14	.word	0
+16	.word	blksup	;number of supported blocks
+20	.word	blkpre	;number of preallocated blocks (reserved for MFD, UFD,
			;map)
+22	.word	intrlv	;interleave factor (block allocation hint)
+24	.word	0
+26	.word	monitr	;pointer to first block of monitor core image
+30	.word	0
+32	.word	trksec	;SD track,,sector of DEC STD 144 bad sector file
+34	.word	cyl	;SD cyl of " " " " " "
+36	.word	trksec	;DD track,,sector of DEC STD 144 bad sector file
+40	.word	cyl	;DD cyl of " " " " " "

Bitmap blocks look like this:

+0	.word	link	;link to next or 0
+2	.word	seq	;sequence # of this block within bitmap (starting at 1)
+4	.word	size	;# of valid words in this block
+6	.word	map	;first block of bitmap (so can always find start)
+10	.blkw	size	;bitmap words

Each UFD is a chain of linked blocks.  The first word is a link word as usual,
then starting at offset 2 there are 28. nine-word file entries:

+0	.rad50	/FIL/	;filename (3 zero words for empty slot)
+2	.rad50	/NAM/
+4	.rad50	/EXT/	;extension
+6	.word	date	;date ((year-1970.)*1000.+daywithinyear) in b14:0
			;b15 => contiguous file
+10	.word	?	;beats me ("ACT-11 logical end" in XXDP+ fiche)
+12	.word	stblk	;starting block of file data
+14	.word	len	;length of file (not trustworthy for contig files???)
+16	.word	endblk	;ending block of file data (can't we figure this out?)
			;(maybe it's here to help with extending?)
+20	.word	prot	;almost always 233, or 377 for BADB.SYS
			;"ACT-11 logical 52" in XXDP+ fiche)
_
;
dbds:	; set defaults
	mov	ax,ss:curdir[bp] ;get current UIC
	mov	ss:actdir[bp],ax ;set active UIC
	mov	ax,ss:cursiz[bp] ;get size of UFD entries
	mov	ss:extra[bp],ax	;save
	ret
;+
;
; Save DOS/BATCH CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
dbsav:	mov	ax,ss:actdir[bp] ;get active dir
	mov	ds:savdir[bx],ax
	mov	ax,ss:extra[bp]	;get UFD entry size
	mov	ds:savsiz[bx],ax
	ret
;+
;
; Restore DOS/BATCH CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
dbrst:	mov	ax,ds:savdir[bx] ;get saved dir
	mov	ss:actdir[bp],ax ;set active dir
	mov	ax,ds:savsiz[bx] ;restore size of UFD entries
	mov	ss:extra[bp],ax
	ret
;
dbgd:	; get dir name
	push	si		;save
	push	cx
	call	dbdi		;DIR calls us before SS:DIRINP[BP], need to
				;allocate first dir buffer
	pop	cx		;restore
	pop	si
	mov	bx,ss:curdir[bp] ;default=current
	cmp	ss:fsys[bp],xxdp ;actually a new XXDP+ disk?
	je	dbgd2		;yes, don't parse dir, but look it up anyway
	jcxz	dbgd2		;nothing, skip
	mov	al,[si]		;get next char
	cmp	al,'['		;open bracket?
	jne	dbgd2		;no
	inc	si		;yes, eat it
	dec	cx
	call	goctb		;get project
	mov	bh,ah		;save
	cmp	al,','		;comma?
	jne	dbgd1
	inc	si		;yes, eat it
	dec	cx
	call	goctb		;get programmer
	inc	si		;eat delimiter
	dec	cx
	mov	bl,ah		;save
	cmp	al,']'		;close bracket?
	je	dbgd2		;yes
dbgd1:	jmp	synerr
dbgd2:	mov	ss:actdir[bp],bx ;save
	; check existence of dir
	push	si		;save
	push	cx
	call	dbfd		;make sure dir exists
	mov	ss:extra[bp],cx	;save dir ent size
	pop	cx		;restore
	pop	si
	ret
;+
;
; Parse DOS/BATCH file and UIC.
;
;-
dbgf:	call	filnam		;do filename first
	callr	ss:gdir[bp]	;directory comes after (or not, with XXDP+)
;+
;
; Print DOS/BATCH file and UIC.
;
;-
dbpf:	lodsb			;get a byte
	test	al,al		;end?
	jz	dbpf1
	stosb			;store
	jmp	short dbpf
dbpf1:	callr	ss:dirnam[bp]	;print dir name (CX value doesn't matter)
;+
;
; Find directory.
;
; ss:bp	log dev rec, preserved (SS:ACTDIR[BP] contains UIC to find)
; ax	returns starting clu # of UFD (0 if none yet)
; cx	returns size (in bytes) of UFD entries, normally 18 bytes (9 words)
; SS:BITMAP[BP]  returns starting clu # of bitmap
;
; For DOS/BATCH disks, DS:DBDAT[BX+SI] points at the UFD's slot in the MFD on
; return.  Not for XXDP+ disks though, since they have one combined MFD1/2 and
; the (single) UFD is pre-extended.
;
;-
dbfd:	mov	ax,1		;start at MFD1
	cmp	ss:med[bp],tu56	;TU56?
	jne	dbfd1
	 mov	ax,64d		;DT: dir starts at 64., not 1
dbfd1:	xor	cl,cl		;really read it
	call	dbdir		;fetch MFD1 (DOS) or MFD1/2 (XXDP+)
	cmp	ss:fsys[bp],xxdp ;really a new XXDP+ disk?
	je	dbfd7		;yes, MFD1 is different (and no MFD2)
	; search MFD for UFD
	mov	ax,ds:dbdat+4[si] ;get ptr to bitmap
	mov	ss:bitmap[bp],ax ;save
	mov	ax,ds:dbdat[si]	;get link to MFD2
	jmp	short dbfd5
dbfd2:	; search next MFD block
	xor	cl,cl		;really read it
	call	dbdir		;fetch
	mov	cx,ds:dbdat[si]	;get link
	mov	bx,2		;init offset
dbfd3:	; check next entry (BX=curr MFD entry, SI+DBDAT=current clu)
	mov	ax,bx		;copy
	add	ax,4*2		;add length of UFD entry
	cmp	ax,ss:pcs[bp]	;will it fit?
	ja	dbfd4		;no
	mov	ax,ds:dbdat[bx+si] ;get UIC
	cmp	ax,ss:actdir[bp] ;is this it?
	je	dbfd6		;yes
	add	bx,4*2		;no, skip to next entry
	jmp	short dbfd3	;loop
dbfd4:	mov	ax,cx		;restore link
dbfd5:	test	ax,ax		;if any
	jnz	dbfd2		;go follow it if non-zero
	jmp	dnf		;dir not found
dbfd6:	; found it
	mov	ax,ds:dbdat+2[bx+si] ;get starting block (0 if no files)
	mov	cx,ds:dbdat+4[bx+si] ;get # valid words per UFD entry
	add	cx,cx		;# valid bytes
	ret
dbfd7:	; new style XXDP+ disk, has only one (huge!) UFD
	mov	ax,ds:dbdat+6[si] ;get ptr to bitmap
	mov	ss:bitmap[bp],ax ;save
	mov	ax,ds:dbdat+2[si] ;get ptr to first UFD block
	mov	cx,9d*2		;entries are always 9 words each
	ret
;+
;
; Print directory heading.
;
;-
dbdh:	xor	ax,ax		;load 0
	mov	ds:nfile,ax	;no files yet
	mov	ds:nblk,ax	;no blocks seen
	mov	ds:nblk+2,ax
	mov	di,offset lbuf	;pt at buf
	mov	al,' '		;blank
	stosb
	mov	ax,word ptr ds:day ;get month,,day
	mov	dx,ds:year	;year
	call	pdate1		;print
	callr	flush		;flush, return
;
dbdi:	; DOS/BATCH dir input init
	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	dbdi1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir blk buffer (nothing is possible if this fails)
	mov	cx,dbdat	;begn of data field
	add	cx,ss:pcs[bp]	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:dbblk[si],ax	;nothing in here yet
	mov	ds:dbdrt[si],al	;not dirty
	mov	ds:dbnxt[si],ax	;no next
dbdi1:	; find dir and fetch retrieval
	call	dbfd		;find dir
	mov	ss:dirwnd[bp],ax ;save starting block number
;;; save size, or should we?
	mov	word ptr ds:inent,2 ;start at first entry
	ret
;
dbdg:	; get DOS/BATCH dir entry
	mov	ax,ss:dirwnd[bp] ;get current blk #
	test	ax,ax		;UFD exists?
	jz	dbdg3		;no, done
	xor	cl,cl		;really read it
	call	dbdir		;make sure we have the cluster
	jc	dbdg4		;error
dbdg1:	; get next entry in this UFD block
	mov	ax,ds:inent	;get offset to next entry
	mov	bx,ax		;copy
	add	bx,ss:extra[bp]	;pt at start of the entry after that
	jc	dbdg2		;definitely off end of cluster
	cmp	bx,ss:pcs[bp]	;does this one span end of cluster?
	jb	dbdg5		;no, continue
dbdg2:	mov	ax,ds:dbdat[si]	;get link to next UFD block
	mov	ss:dirwnd[bp],ax ;save
	mov	word ptr ds:inent,2 ;(start over after its link word)
	test	ax,ax		;end of UFD?
	jnz	dbdg		;no, follow link
dbdg3:	stc			;no more entries
	ret
dbdg4:	jmp	dirio
dbdg5:	add	si,ax		;point at dir entry
	mov	ds:inent,bx	;save addr of next entry for next time
	add	si,dbdat	;add base of data
	test	word ptr [si],-1 ;null entry?
	jz	dbdg		;yes, skip it (reload SI)
	mov	di,offset lbuf	;pt at LBUF
	lodsw			;FILNAM
	call	rad50
	lodsw
	call	rad50
	mov	al,'.'		;.
	stosb
	lodsw			;EXT
	call	rad50
	lodsw			;get date
	mov	ds:fattr,ah	;save contig bit (b15)
	and	ah,177		;trim it off
	push	si		;save
	call	decdos		;convert
	pop	si		;restore
	mov	ds:fyear,dx	;store date
	mov	word ptr ds:fday,cx
	mov	ds:fdatf,al	;set flag
	mov	byte ptr ds:ftimf,0 ;no file time
	lodsw			;skip next word
	lodsw			;get starting block #
	mov	ds:stblk,ax	;save
	mov	word ptr ds:stblk+2,0
	lodsw			;get file length
	mov	bx,ax		;copy
	lodsw			;get ending block #
	test	byte ptr ds:fattr,200 ;contiguous file?
	jnz	dbdg6		;yes
	mov	ax,-2		;no, BX is accurate length, deduct link word
	jmp	short dbdg7
dbdg6:	inc	ax		;pt past last blk
	sub	ax,ds:stblk	;find length of contig file
	mov	bx,ax		;copy
	xor	ax,ax		;no link words in contig files, use whole clu
dbdg7:	mov	ds:fsize,bx	;save length in clusters
	mov	word ptr ds:fsize+2,0
	add	ax,ss:pcs[bp]	;find # data bytes per cluster
	mul	bx		;find length in bytes
	mov	ds:fbcnt,ax	;save
	mov	ds:fbcnt+2,dx
	lodsw			;get protection code
	mov	ds:fprot,al	;save
	callr	sqfn		;make squished copy of filename, return
;
dbdd:	; display DOS/BATCH dir entry
	mov	ax,ds:fsize	;size
	mov	dx,ds:fsize+2	;high order
	inc	ds:nfile	;bump # files
	add	ds:nblk,ax	;bump # clusters
	adc	ds:nblk+2,dx
	mov	cx,6d		;field size
	call	cnum		;print it right-justified
	mov	ax," C"		;assume contiguous
	test	byte ptr ds:fattr,200 ;right?
	jnz	dbdd1		;yes
	 mov	al,' '		;no
dbdd1:	stosw			;save
	call	pdate		;add file date
	cmp	ss:fsys[bp],xxdp ;new XXDP+ format?
	je	dbdd2		;yes, no prot code
	mov	ax,"< "		;begin prot code
	stosw
	mov	al,ds:fprot	;protection code
	xor	ah,ah		;AH=0
	mov	cx,3		;field size=3
	call	poct		;print prot code
	mov	al,'>'		;>
	stosb
dbdd2:	test	byte ptr ds:dirflg,200 ;verbose?
	jz	dbdd3
	mov	ax,ds:stblk	;get starting block #
	mov	dx,ds:stblk+2
	mov	cx,6		;6-column field
	call	cnum		;print it right-justified
dbdd3:	ret
;
dbdn:	; display DOS/BATCH dir name
	mov	al,'['		;[
	stosb
	mov	al,byte ptr ss:actdir+1[bp] ;proj
	xor	ah,ah
	call	poctv
	mov	al,','		;,
	stosb
	mov	al,byte ptr ss:actdir[bp] ;prog
	xor	ah,ah
	call	poctv
	mov	al,']'		;]
	stosb
	ret
;
dbdf:	; DOS/BATCH dir finish (flush dirty dir buffers)
	mov	si,ss:dirbuf[bp] ;get ptr to buf chain
dbdf1:	call	dbfdir		;flush
	mov	si,ds:dbnxt[si]	;follow chain
	test	si,si		;anything?
	jnz	dbdf1		;loop if so
	ret
;
dbsum:	; display DOS/BATCH dir listing summary
	xor	bx,bx		;nothing read
	xor	cx,cx		;start scanning at cluster 0
	mov	ds:nfree,cx	;init # free clusters
dbsum1:	call	dbfree		;find next free area
	jc	dbsum2		;EOD or error, skip
	add	ds:nfree,dx	;add to total
	add	cx,dx		;skip free area
	jmp	short dbsum1	;find next one
dbsum2:	mov	di,offset lbuf	;pt at buf
	mov	al,' '		;blank
	stosb
	mov	ax,ds:nfile	;get # files shown
	push	ax		;save
	call	pnum
	cram	' File'
	pop	ax		;restore
	call	adds		;add s
	mov	ax," ,"		;', '
	stosw
	mov	ax,ds:nblk	;get # blocks shown
	push	ax		;save
	call	pnum		;print
	cram	' Block'
	pop	ax		;restore
	call	adds		;add s
	mov	ax," ,"		;', '
	stosw
	mov	ax,ds:nfree	;get # blocks free
	call	pnum		;print
	cram	' Free'
	callr	flush		;flush, return
;
dbop:	; open DOS/BATCH file for input
	mov	ax,ds:stblk	;get starting cluster #
	mov	dx,ds:stblk+2
	mov	ds:iblk,ax	;save
	mov	ds:iblk+2,dx
	mov	ax,ds:fsize	;get size
	mov	ds:isize,ax	;save
	clc			;happy
	ret
;
dbrd:	; read DOS/BATCH file data
	test	word ptr ds:isize,-1 ;anything left?
	jz	dbrd3		;no
	test	byte ptr ds:fattr,200 ;contiguous file?
	jnz	dbrd5		;yes, easy
	cmp	cx,ss:pcs[bp]	;enough space for data block?  (incl link)
	jb	dbrd2		;no
	mov	ax,ds:iblk	;get starting cluster
	mov	cx,1		;read just 1 cluster (since they're linked)
	sub	ds:isize,cx
	mov	si,ds:binflg	;get binary flag
	test	si,si		;binary?
.assume <bin eq 0>
	jz	dbrd1		;yes
	call	rddb		;read data
	jc	dbrd4		;error
	ret
dbrd1:	call	dbrclu		;read data
	jc	dbrd4		;error
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	mov	di,si		;point at blk
	lodsw			;fetch link word
	sub	cx,2		;count it
	push	cx		;save
	shr	cx,1		;BC/2=WC
	rep	movsw		;move back to cover link word
	pop	cx		;restore
	pop	ds
	mov	ds:iblk,ax	;save
	clc
	ret
dbrd2:	or	al,1		;ZF=0
dbrd3:	stc			;ZF set already, set CF
	ret
dbrd4:	jmp	rderr
dbrd5:	; read contiguous data
	mov	ax,ds:isize	;get remaining size
	mul	word ptr ss:pcs[bp] ;convert to bytes, CF=1 if >=64 KB
	sbb	dx,dx		;all ones if >=64 KB, zeros otherwise
	or	ax,dx		;peg at 64 KB -1 if >=64 KB
	cmp	ax,cx		;will entire file fit in buffer?
	jb	dbrd6		;yes
	 mov	ax,cx		;no, stop short
dbrd6:	xor	dx,dx		;zero-extend
	div	word ptr ss:pcs[bp] ;find # whole clusters
	mov	cx,ax		;copy
	mov	ax,ds:iblk	;get starting block
	mov	dx,ds:iblk+2
	add	ds:iblk,cx	;update
	adc	ds:iblk+2,0
	sub	ds:isize,cx
	call	dbrclu		;read cluster(s)
	jc	dbrd4		;read error
	test	word ptr ds:binflg,-1 ;binary mode?
.assume <bin eq 0>
	jnz	dbrd7		;no
	 ret
dbrd7:	callr	strip		;strip NULs, return
;
dbdo:	; DOS/BATCH dir output init
	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	dbdo1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir blk buffer (nothing is possible if this fails)
	mov	cx,dbdat	;begn of data field
	add	cx,ss:pcs[bp]	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:dbblk[si],ax	;nothing in here yet
	mov	ds:dbdrt[si],al	;not dirty
	mov	ds:dbnxt[si],ax	;no next
dbdo1:	; find dir and fetch retrieval
	call	dbfd		;find dir
	mov	ss:dirwnd+2[bp],ax ;save starting block number
;;; save entry size
;;	mov	word ptr ds:outent,2 ;start at first entry
	ret
;+
;
; Create DOS/BATCH output file.
;
; ds:si	.ASCIZ filename
; DS:FBCNT  max size of file in bytes
;
;-
dbcr:	; parse filename
	call	lenz		;get length
	call	grad		;get filename
	jc	dbcr2
	cmp	al,'.'		;ends with dot, right?
	jne	dbcr2		;no
	mov	ds:rtfile,bx	;save filename
	mov	ds:rtfile+2,dx
	inc	si		;eat the dot
	dec	cx
	call	grad		;get extension
	test	al,al		;ends at end of string, right?
	jnz	dbcr2
	mov	ds:rtfile+4,bx	;save extension
	; find out how many clusters this file will need
	mov	bx,ss:pcs[bp]	;get cluster size
	dec	bx		;-2 for link word
	dec	bx		;BX = # data bytes per cluster
;; don't subtract 2 if contiguous file, no link words
;; also, contiguous files are placed in the *highest* available clusters, not
;; the lowest
	mov	ax,ds:fbcnt+2	;get high order byte count
	xor	dx,dx		;zero-extend
	div	bx		;find high-order word of clu count
	test	ax,ax		;must be zero
	jz	dbcr4		;OK
dbcr1:	jmp	odfull		;full
dbcr2:	jmp	badnam		;bad filename
dbcr3:	jmp	dirful		;directory is full
dbcr4:	mov	ax,ds:fbcnt	;get low order byte count
	div	bx		;find low-order word of clu count
	add	dx,-1		;CF=1 if remainder NZ
	adc	ax,0		;add partial cluster if any
	jc	dbcr1		;too much
	mov	ds:osize,ax	;save # clusters needed
	mov	ds:osize+2,ax	;twice
	mov	word ptr ds:onew,0 ;assume we're not adding a new dir cluster
	call	dbslot		;find free dir slot
	jnc	dbcr5		;success, skip
	cmp	word ptr ss:fsys[bp],xxdp ;XXDP+?
	je	dbcr3		;yes, don't extend UFD
	; UFD is full or nonexistent, add a cluster
	call	dbanew		;allocate new dir cluster now
dbcr5:	; scan disk to make sure we have at enough free space
	xor	bx,bx		;nothing read
	xor	cx,cx		;start scanning at cluster 0
	cmp	ds:osize,cx	;do we need anything?
	jz	dbcr7		;no, null file
	xor	si,si		;init total count
dbcr6:	push	si		;save
	call	dbfree		;find next free area
	pop	si		;[restore]
	jc	dbcr8		;EOD or error, skip
	add	si,dx		;add to total
	add	cx,dx		;bump clu to end of free area
	cmp	si,ds:osize	;enough space yet?
	jb	dbcr6		;loop if not
dbcr7:	mov	word ptr ds:ofree,0 ;no free area yet
	call	dbnext		;get free region
	mov	ax,ds:ofree	;get first clu # (or 0 of length=0)
	mov	ds:oblk,ax	;set starting clu #
	mov	ds:oblk+2,ax	;ending clu # too if 0
	ret
dbcr8:	call	dbfnew		;free the new cluster we added, if any
	jmp	odfull		;output device is full
;+
;
; Write data to output file.
;
; es:si	buffer
; cx	length of data
;
; On return, CX contains the number of bytes actually written (presumably a
; multiple of the clustersize).
;
;-
dbwr:	xor	dx,dx		;nothing written yet
dbwr1:	; write next block
	mov	ax,ss:pcs[bp]	;get pack cluster size
	dec	ax		;find # data bytes per cluster
	dec	ax		;(deduct link word at beginning)
;;; not for contiguous files -- handle that differently
	sub	cx,ax		;enough for one cluster?
	jb	dbwr3		;no
	add	dx,ax		;add it to our count
	; have a least a whole cluster, copy to BUF so we can add the link word
	; (this means we write only one cluster at a time, yech)
	cmp	word ptr ss:pcs[bp],bufsiz ;will cluster fit in BUF?
	ja	dbwr4		;no
	push	cx		;save byte counts
	push	dx
	dec	word ptr ds:osize ;count this clu
	mov	bx,ds:ofree	;get the next clu # to write
	inc	word ptr ds:ofree ;bump to next
	dec	word ptr ds:onfree ;count off of total
	jz	dbwr5		;nothing left, get more if needed
dbwr2:	mov	ax,ds:ofree	;get next free clu #, or 0 if EOF
	mov	di,offset buf	;point at buf
	push	ds		;change places
	push	es
	pop	ds
	pop	es
	stosw			;write link word
	mov	cx,ss:pcs[bp]	;get # bytes per cluster
	shr	cx,1		;# words
	dec	cx		;deduct link word
	rep	movsw		;copy data into buffer
	push	ds		;save
	push	si
	push	es		;restore DS
	pop	ds
	mov	ax,bx		;get cluster # to write
	mov	ds:oblk+2,ax	;save most recently written clu #
	inc	cx		;write one cluster (was 0 from REP MOVSW)
	mov	si,offset buf	;ES:SI points at buf
	call	dbwclu		;write cluster
	pop	si		;restore buf pointer
	pop	es
	pop	dx		;completed byte count
	pop	cx		;remaining byte count
	jmp	short dbwr1	;around for more
dbwr3:	mov	cx,dx		;# actually written
	ret
dbwr4:	jmp	nomem		;cluster too large
dbwr5:	; get next contiguous range of blocks to write into
	push	es		;save ptr
	push	si
	push	bx		;save block #
	push	ds		;copy DS to ES
	pop	es
	call	dbnext		;get more blocks, mark as in use
	pop	bx		;restore
	pop	si
	pop	es
	jmp	short dbwr2	;back in
;+
;
; Obtain next free area into which file data will be written.
;
; DS:OFREE returns the first cluster # of the region
; DS:ONFREE returns the # of clusters in the region
;
;-
dbnext:	xor	bx,bx		;start looking at begn of bitmap
	cmp	ds:osize,bx	;do we need anything more?
	jz	dbnxt2		;no
	mov	cx,ds:ofree	;starting blk we expect next
	call	dbfree		;find next free area (known to exist)
	cmp	dx,ds:osize	;do we need the whole thing?
	jb	dbnxt1		;yes
	 mov	dx,ds:osize	;no, take just what we need
dbnxt1:	mov	ds:ofree,cx	;save
	mov	ds:onfree,dx
	callr	dbsbit		;mark as allocated
dbnxt2:	mov	ds:ofree,bx	;EOF, set both numbers to 0
	mov	ds:onfree,bx
	ret
;
dbwl:	; write last partial sector
	jcxz	dbwl1		;nothing to do
	mov	dx,ss:pcs[bp]	;get pack cluster size
	dec	dx		;find # data bytes per cluster
	dec	dx		;(deduct link word at beginning)
	sub	dx,cx		;find # padding bytes needed (may be 0)
	; copy to BUF so we can add the (zero) link word
	cmp	word ptr ss:pcs[bp],bufsiz ;will cluster fit in BUF?
	ja	dbwl2		;no
	mov	di,offset buf	;point at buf
	push	ds		;change places
	push	es
	pop	ds
	pop	es
	xor	ax,ax		;load 0
	stosw			;write link word (0 = EOF)
	rep	movsb		;copy data into buffer
	mov	cx,dx		;get # bytes to pad, if any
	rep	stosb		;clear
	push	ds		;save
	push	si
	push	es		;restore DS
	pop	ds
	mov	ax,ds:ofree	;get cluster # to write
	mov	ds:oblk+2,ax	;save most recently written clu #
	inc	cx		;write one cluster (was 0 from REP STOSB)
	mov	si,offset buf	;ES:SI points at buf
	call	dbwclu		;write cluster
	pop	si		;restore buf pointer
	pop	es
dbwl1:	ret
dbwl2:	jmp	nomem		;cluster too large
;
dbcl:	; create a slot for this file in the UFD
	call	dbslot		;find our free slot
	jnc	dbcl3		;got it, skip
	; we must have allocated a new UFD cluster, link it in
	test	ax,ax		;does the UFD exist at all?
	mov	ax,ds:onew	;[get new cluster]
	jnz	dbcl1		;yes, and it's full, tack this on the end
	; this is the first block for this UFD
	mov	ds:dbdat+2[bx+si],ax ;link it in
	jmp	short dbcl2
dbcl1:	mov	ds:dbdat[si],ax	;link to last block of UFD
dbcl2:	mov	byte ptr ds:dbdrt[si],1 ;mark buffer as dirty
	mov	cl,1		;we're creating this
	call	dbdir		;get buffer
	lea	di,ds:dbdat[si]	;point at data field
	mov	cx,ss:pcs[bp]	;get pack cluster size
	shr	cx,1		;in words
	xor	ax,ax		;load 0
	rep	stosw		;clear it out, including link word
	mov	bx,2		;put new entry at beginning
dbcl3:	; DS:DBDAT[BX+SI] points at empty slot in UFD
	mov	ax,ds:dbblk[si]	;get dir clu #
	mov	word ptr ds:outseg,ax ;save
	mov	ds:outent,bx	;save this entry's index into clu
	lea	di,ds:dbdat[bx+si] ;get pointer to dir slot
	mov	ax,ds:rtfile	;copy filename
	stosw			;+00
	mov	ax,ds:rtfile+2
	stosw
	mov	ax,ds:rtfile+4	;extension
	stosw			;+04
	; translate date to DOS/BATCH form
	; since the "contiguous" flag is using up bit 15 of the date word, we
	; can't use it the way RSTS does, so we "fold" a second set of 32 years
	; in by adding 366. to the day within the year -- so:
	; 1970-2002:  daywithinyear+(year-1970)*1000.  (0-32365.)
	; 2003-2035:  daywithinyear+366.+(year-2003)*1000.  (367.-32731.)
	xor	ax,ax		;assume we don't know the date
	cmp	ds:fdatf,al	;do we know date?
	jz	dbcl5		;no, leave it as 0 (=unknown)
	push	si		;save
	call	encdos		;encode day and year
	pop	si		;restore
	cmp	dx,32d		;will year offset fit in original format?
	jbe	dbcl4		;yes, leave it as-is
	add	bx,366d		;add bias to use "impossible" day numbers
	sub	dx,33d		;start over with year 32 and up
	cmp	dx,32d		;OK?
	jbe	dbcl4		;yes
	 mov	dx,32d		;peg at max
dbcl4:	mov	ax,1000d	;multiplier
	mul	dx
	add	ax,bx		;add day within year, plus bias if any
dbcl5:	stosw			;+06 store date word
	xor	ax,ax		;don't know what 5th word is for???
;;; (I've seen 001Ch in disks written by real DOS/BATCH, use that???)
	stosw			;+10
	mov	ax,ds:oblk	;get starting clu #
	stosw			;+12
	mov	ax,ds:osize+2	;size in clusters
	stosw			;+14
	mov	ax,ds:oblk+2	;ending clu #
	stosw			;+16
	mov	ax,233		;protection
	stosw			;+20
	mov	byte ptr ds:dbdrt[si],1 ;flag cluster as dirty
	; search for and delete any *other* file of the same name
	call	dbfd		;get start of UFD yet again (NZ for sure now)
dbcl6:	; search cluster AX for a match
	push	ax		;save
	xor	cl,cl		;really read it
	call	dbdir		;read UFD cluster
	pop	ax		;restore
	mov	bx,2		;start at beginning
dbcl7:	; check next entry
	mov	dx,bx		;copy
	add	dx,ss:extra[bp]	;point at next entry
	jc	dbcl11		;definitely end of clu
	cmp	dx,ss:pcs[bp]	;off end of cluster?
	ja	dbcl11		;yes
	push	si		;save
	lea	si,ds:dbdat[bx+si] ;point at entry
	mov	di,offset rtfile ;filename to match
	mov	cx,3		;word count
	repe	cmpsw		;match?
	pop	si		;[restore]
	je	dbcl9		;yes
dbcl8:	mov	bx,dx		;advance to next
	jmp	short dbcl7
dbcl9:	cmp	ax,word ptr ds:outseg ;same UFD cluster as our file?
	jne	dbcl10		;no, delete it for sure
	cmp	bx,ds:outent	;yes, is this our entry?
	je	dbcl8		;don't delete it if so
dbcl10:	callr	dbdel		;delete it, return
dbcl11:	mov	ax,ds:dbdat[si]	;get link to next
	test	ax,ax		;end of UFD?
	jnz	dbcl6		;loop if not
	ret
;+
;
; Partly encode date (from FYEAR/FMONTH/FDAY) into DOS/BATCH form.
;
; On return:
; dx	year-1970
; bx	day within year
;
;-
encdos:	mov	dx,ds:fyear	;get year
	mov	al,28d		;# days in Feb
	test	dl,3		;leap year?  (2000. AD counts)
	jnz	encds1
	 inc	ax		;yes
encds1:	mov	ds:feb,al	;save
	mov	cl,ds:fmonth	;get month
	mov	si,offset jan	;point at # days in each month
	mov	bl,ds:fday	;get day within month
	xor	bh,bh		;zero-extend
	xor	ah,ah		;clear high order
encds2:	dec	cl		;gotten to our month yet?
	jz	encds3
	lodsb			;get # days in month
	add	bx,ax		;add to total
	jmp	short encds2	;loop
encds3:	sub	dx,1970d	;subtract base year
	jnc	encds4
	 xor	dx,dx		;went negative, stop at 1970.
encds4:	ret
;+
;
; Decode DOS/BATCH date to date, month, year.
;
; DOS/BATCH uses the following date format:
;	(year-1970.)*1000. + (day within year)
;
; Unlike RSTS/E (which uses this format too with unsigned numbers), DOS/BATCH
; can't use bit 15 because that's the "contiguous file" flag in UFD entries.
; This problem isn't solved in real DOS/BATCH (since it almost completely died
; out long before the Y2K/etc. updates were made to other OSes), so its date
; format runs out on 01-Jan-2003.  Or it would if it didn't have Y2K problems
; already.
;
; My solution is to "fold" another 33 years into the existing date format, by
; using "day within year" numbers from 367. to 732.  So my format goes like
; this:
;
; 1970-2002:	(year-1970.)*1000. + (day within year)
; 2003-2035:	(year-2003.)*1000. + (day within year) + 366.
;
; ax	DOS/BATCH date
;
; On return:
; cl	day
; ch	month
; dx	year
; al	flag (NZ => date is valid)
;
;-
decdos:	xor	dx,dx		;0-extend
	mov	bx,1000d	;split year from month/day
	div	bx
	add	ax,1970d	;add base year
	mov	cx,dx		;catch remainder
	mov	dx,ax		;copy year
	cmp	cx,366d		;.LE.366?
	jbe	decds1		;yes
	sub	cx,366d		;rebias to use 2nd set of 366. numbers
	add	dx,33d		;advance starting year to 2003.
decds1:	mov	ax,28d		;# days in Feb
	test	dl,3		;leap year?  (2000. AD counts)
	jnz	decds2
	 inc	ax		;yes
decds2:	mov	ds:feb,al	;save
	add	ax,365d-28d	;find length of year
	cmp	cx,ax		;is it possible?
	ja	decds4		;no
	jcxz	decds4		;can't be 0 either
	mov	si,offset jan	;point at year table
	xor	ah,ah		;AH=0
decds3:	lodsb			;get month
	sub	cx,ax		;subtract
	ja	decds3		;loop unless 0 or carry
	add	cx,ax		;correct
	sub	si,offset jan	;find month-1, +1
	mov	ax,si		;copy
	mov	ch,al		;copy again
	mov	al,1		;valid
	ret
decds4:	xor	al,al		;not valid
	ret
;+
;
; Find a free slot for a new file entry in a UFD.
;
; SS:ACTDIR  PPN to search
;
; On return, DS:DBDAT[SI] points at the last directory cluster read:
; If CF=0, the cluster is in the UFD and BX is the offset of a free slot.
; If CF=1 and AX=0, the UFD contains no blocks, the cluster is in the MFD and
; BX is the offset of this UFD's entry.
; If CF=1 and AX<>0, the UFD is full, and the cluster is its last cluster.
;
;-
dbslot:	call	dbfd		;find head of directory
	test	ax,ax		;does UFD have even one cluster?
	jz	dbslt4		;no, need to add it to create a file
dbslt1:	xor	cl,cl		;really read it
	call	dbdir		;make sure we have the cluster
	mov	bx,2		;start after link word
dbslt2:	mov	ax,bx		;copy offset
	add	ax,ss:extra[bp]	;add length of file entry
	jc	dbslt3		;definitely off end
	cmp	ax,ss:pcs[bp]	;off end of cluster?
	jae	dbslt3		;yes, link to next
	test	word ptr ds:dbdat[bx+si],-1 ;empty slot?
	jz	dbslt5		;yes, success
	mov	bx,ax		;move up to next entry
	jmp	short dbslt2	;loop
dbslt3:	mov	ax,ds:dbdat[si]	;follow link
	test	ax,ax		;end of UFD chain?
	jnz	dbslt1		;keep searching if not
	inc	ax		;UFD chain did exist (AX=1)
dbslt4:	stc			;failed
dbslt5:	ret
;+
;
; Allocate a cluster, to be added to a UFD or the MFD2 chain.
;
; ax	returns cluster # (also saved in DS:ONEW)
;
;-
dbanew:	xor	bx,bx		;nothing read
	xor	cx,cx		;start scanning at cluster 0
	call	dbfree		;find next free area
	jc	dbanw1		;EOD or error, skip
	mov	dx,1		;we need just one cluster
	push	cx		;save cluster #
	call	dbsbit		;set bitmap bit for this cluster
	pop	ax		;restore
	mov	ds:onew,ax	;save
	ret
dbanw1:	jmp	dirful
;+
;
; Didn't have space to write the file after all, free the new dir cluster that
; we added, if any.
;
;-
dbfnew:	mov	cx,ds:onew	;fetch cluster #
	jcxz	dbfnw1		;none, skip it
	mov	dx,1		;clear one bit
	callr	dbcbit
dbfnw1:	ret
;
dbdl:	; delete file
	mov	ax,ss:dirwnd[bp] ;get current blk #
	test	ax,ax		;no UFD?
	jz	dbdl2		;shouldn't happen
	xor	cl,cl		;really read it
	call	dbdir		;make sure we have the cluster
	jc	dbdl2		;error
	mov	bx,ds:inent	;get offset to next entry
	sub	bx,ss:extra[bp]	;point at the match
	callr	dbdel		;delete file
dbdl2:	jmp	rderr
;+
;
; Delete a file.
;
; ss:bp	log dev rec
; ds:si	points at UFD cluster buf containing entry
; bx	offset of entry to delete
;
;-
dbdel:	; mark UFD entry as free
	xor	ax,ax		;load 0
	mov	ds:dbdat[bx+si],ax ;clear out filename
	mov	ds:dbdat+2[bx+si],ax
	mov	ds:dbdat+4[bx+si],ax
	mov	byte ptr ds:dbdrt[si],1 ;mark clu as dirty
	; find the file's clusters
	mov	cx,ds:dbdat+12[bx+si] ;get starting clu #
	mov	dx,ds:dbdat+14[bx+si] ;get length
	test	dx,dx		;null file?
	jz	dbdel1		;yes, nothing to do
	test	byte ptr ds:dbdat+6+1,200 ;contiguous file?
	jz	dbdel2		;no
	callr	dbcbit		;easy, clear bits and return
dbdel1:	ret
dbdel2:	; follow cluster chain and free each one in bitmap
	push	cx		;save
	mov	dx,1		;clear one bit
	call	dbcbit
	pop	ax		;restore clu #
	cmp	word ptr ss:pcs[bp],bufsiz ;will cluster fit in BUF?
	ja	dbdel3		;no
	mov	cx,1		;# to read
	mov	di,offset buf	;point at buffer
	call	dbrclu		;read it
	jc	dbdel4		;error
	mov	cx,[si]		;get link word
	test	cx,cx		;EOF?
	jnz	dbdel2		;loop if not
	ret
dbdel3:	jmp	nomem		;couldn't read clusters to get link words
dbdel4:	jmp	rderr		;error reading cluster to get link word
;
dbcd:	; change DOS/BATCH dir
	call	dbgd		;get dir
	call	confrm		;make sure confirmed
	mov	ax,ss:actdir[bp] ;get active dir, make current
	mov	ss:curdir[bp],ax
	call	dbfd		;look it up
	mov	ss:cursiz[bp],cx ;save UFD entry size
	ret
;+
;
; Figure out clustersize for DOS/BATCH disk, and allocate block buffer if
; cluster size isn't a block multiple (i.e. if it's 128 bytes).
;
; ss:bp	dev rec
;
; On return, SS:PCS[BP] is filled in.
;
; This is based on the table on page 3-14 of the V9 DOS/BATCH Handbook:
;
;  DIRECTORY	BLOCK SIZE	MAXIMUM NUMBER
;   DEVICE	(in words)	   OF UIC'S
; ---------------------------------------------
;  RC11		     64		      15
;  RF11		     64		      15
;  RK11		    256		      63
;  RP03		    512		     127
;  DECtape	    256		      63	(N.B. different MFD location)
;
; Experimentation shows that RK02s and RP02s are both in the 256/63 category
; just like RK03/RK05s.
;
; Note:  sizes are as follows (in blocks):
;	RC11	256
;	RF11	1024
;	RK05	4800
;
;-
dbclu:	mov	ax,-1		;assume device is huge
	mov	dx,ax
	mov	cx,ss:devsiz+6[bp] ;get high longword of size
	or	cx,ss:devsiz+4[bp] ;see if NZ
	jnz	dbclu1		;yes, pretend it's 2**32-1 blocks
	mov	dx,ss:devsiz+2[bp] ;fetch actual size
	mov	ax,ss:devsiz[bp]
dbclu1:	cmp	ss:med[bp],tu56	;TU56?
	je	dbclu2		;yes, 512-byte blocks
	test	dx,dx		;>32 MB?
	jnz	dbclu2		;yes, forget about tiny blocks
	mov	cx,64d*2	;assume 64 w/s
	cmp	ax,1024d	;RF11 or smaller?
	jb	dbclu4		;yes, use 128 b/s
dbclu2:	mov	cx,256d*2	;assume normal 512-byte blocks
dbclu3:	; search for smallest cluster size which will allow us to address every
	; cluster on the disk with a 16-bit number
	test	dx,dx		;will blk # fit in 16 bits?
	jz	dbclu4		;yes
	shr	dx,1		;block count /2
	rcr	ax,1
	add	cx,cx		;block size *2
	jnc	dbclu3		;loop unless overflowed
	mov	cx,8000h	;good luck fitting this in memory!
	mov	dx,cx		;make the below code count 64 K -1 clusters
dbclu4:	mov	ss:pcs[bp],cx	;save cluster size (in *bytes*)
	test	dx,dx		;device too big for this to work?
	jz	dbclu5
	 mov	ax,-1
dbclu5:	mov	ss:numclu[bp],ax ;save # of clusters
	test	cx,777		;smaller than a block?
	jz	dbclu6		;no
	mov	cx,512d		;yes, get block buffer for DBRCLU
	call	getmem
	mov	ss:blkbuf[bp],si ;save
dbclu6:	ret
;+
;
; Read a DOS/BATCH cluster.
;
; ax	cluster number
; cx	cluster count
; es:di	buffer
; ss:bp	dev rec (preserved)
;
; On return:
; cx	# bytes read
; es:si	buffer (same as ES:DI on entry)
;
;-
dbrclu:	xchg	ax,cx		;save starting cluster, get cluster count
	mul	word ptr ss:pcs[bp] ;find # bytes to transfer
	; truncate to fit in 64 KB
	; (is this ever really necessary?)
	mov	bx,ax		;copy
	test	dx,dx		;>64 KB?
	jz	dbrc1		;no
	mov	bx,ss:pcs[bp]	;get cluster size
	neg	bx		;64 KB - cluster size (still cluster multiple)
dbrc1:	mov	ax,cx		;restore starting cluster
	mul	word ptr ss:pcs[bp] ;find starting byte address
	mov	cx,ax		;save starting offset within first block
	and	ch,777/400	;(trim to 9 bits)
	mov	al,ah		;right 8 bits
	mov	ah,dl
	mov	dl,dh
	xor	dh,dh
	shr	dl,1		;9 bits
	rcr	ax,1
	; DX:AX=starting block, BX=byte count, CX=starting offset, ES:DI=buf
	push	ax		;save
	mov	ax,cx		;copy starting offset
	add	ax,bx		;find ending offset +1
	jc	dbrc2		;carried
	 add	ax,777		;round to next block
dbrc2:	sbb	si,si		;all ones if wrapped, 0 if not
	or	ax,si		;peg at 64 KB -1 if needed
	mov	al,ah		;right 9 bits to get block count
	shr	al,1
	cbw			;AH=0
	mov	si,cx		;save starting offset
	mov	cx,ax		;copy block count
	pop	ax		;restore
	test	si,si		;starting at begn of block?
	jnz	dbrc6		;no, skip
dbrc3:	; starting on a block boundary, easy
	test	bx,777		;ending on one too?
	jnz	dbrc4		;no
	callr	ss:rdblk[bp]	;later
dbrc4:	dec	cx		;don't read final block
	push	ax		;save
	push	bx
	push	cx
	push	dx
	call	ss:rdblk[bp]	;read whole blocks
	pop	dx		;[restore]
	pop	di
	pop	bx
	pop	ax
	jc	dbrc5		;failed
	add	cx,777		;round almost to next block
	cmp	cx,bx		;got all whole blocks, right?
	jb	dbrc5		;no, CF=1
	push	es		;save starting addr of whole blocks
	push	si
	push	cx		;length too
	push	bx		;byte count
	add	ax,cx		;skip to final block
	adc	dx,0
	mov	cx,1		;read one block
	push	ds		;copy DS to ES
	pop	es
	mov	si,ss:blkbuf[bp] ;point at block buffer
	call	ss:rdblk[bp]	;read the final block
	pop	bx		;[restore]
	pop	ax
	pop	dx
	pop	es
	jc	dbrc5		;error
	cmp	cx,512d		;got it all?
	jb	dbrc5		;CF=1
	mov	cx,bx		;copy byte count
	and	cx,777		;last partial block
	mov	di,dx		;point at begn of buf
	add	di,ax		;skip whole blocks
	rep	movsb		;copy final partial block
	mov	cx,bx		;get count
	mov	si,dx		;point at it
	clc			;happy
dbrc5:	ret
dbrc6:	; not starting at begn of block
	; read first block into scratch buf and extract what we need
	push	dx		;save blk
	push	ax
	push	cx		;blk count
	push	bx		;byte count
	push	si		;starting offset
	push	di		;buf ptr
	push	es		;and buf seg
	push	ds		;copy DS to ES
	pop	es
	mov	di,ss:blkbuf[bp] ;point at block buffer
	mov	cx,1		;read one sector
	call	ss:rdblk[bp]	;do it
	pop	es		;[restore]
	pop	di
	jc	dbrc8		;error
	cmp	cx,512d		;got it all right?
	jne	dbrc8		;no, error
	pop	ax		;starting offset
	add	si,ax		;index out to it
	pop	bx		;total byte count
	sub	cx,ax		;find max # bytes until end of block
	cmp	cx,bx		;does cluster end before EOB?
	jb	dbrc7
	 mov	cx,bx		;yes, stop short
dbrc7:	sub	bx,cx		;count off of total
	push	cx		;save
	rep	movsb		;copy into caller's buffer
	pop	cx		;restore # bytes copied
	test	bx,bx		;done all?
	jz	dbrc10		;yes
	mov	si,cx		;save count
	pop	cx		;restore
	pop	ax
	pop	dx
	dec	cx		;count the block we did
	add	ax,1		;advance to next
	adc	dx,0
	push	si		;save count already read
	call	dbrc3		;read the rest
	pop	ax		;[restore]
	jc	dbrc9
	add	cx,ax		;bump count
	sub	si,ax		;back up starting addr (CF=0)
	ret
dbrc8:	add	sp,5*2		;flush stack
	stc			;error
dbrc9:	ret
dbrc10:	add	sp,3*2		;flush 3 words from stack
	mov	si,di		;copy addr
	sub	si,cx		;point at begn (CF=0)
	ret
;+
;
; Write a DOS/BATCH cluster.
;
; ax	cluster number
; cx	cluster count
; es:si	buffer
; ss:bp	dev rec (preserved)
;
; On return:
; CF=1 on error
;
;-
dbwclu:	cmp	word ptr ss:pcs[bp],512d ;easy case?  (1 clu = 1 block)
	jne	dbwc1		;no
	xor	dx,dx		;zero-extend block number
	callr	ss:wrblk[bp]	;write, return
dbwc1:	; read/modify/write to insert our data
	cram	'?Cluster size not 512',error ;;; I'll handle this, later
;+
;
; Find next free area on disk.
;
; ax	cluster # of most recently read bitmap block
; bx	disk cluster # mapped by first bit of clu # AX, 0 if starting scan
; cx	starting cluster # to search (find first free area >= this disk loc)
;
; On return:
; ax/bx  updated to reflect latest bitmap block scanned
; cx	starting cluster # of free area
; dx	# clusters in free area
;
;-
dbfree:	test	bx,bx		;already got something?
	jnz	dbfr1		;yes
	 mov	ax,ss:bitmap[bp] ;get pointer to first bitmap block
dbfr1:	cmp	cx,ss:numclu[bp] ;off EOD?
	jae	dbfr3		;yes, no more free space
	push	ax		;save
	push	bx
	push	cx
	xor	cl,cl		;read if not cached
	call	dbdir		;get dir block
	pop	cx		;restore
	pop	bx
	pop	ax
	mov	dx,ds:dbdat+4[si] ;get # valid map words in bitmap block
	add	dx,dx		;# valid bytes
	jbe	dbfr3		;>= 64 KB, or 0, invalid
	mov	di,ss:pcs[bp]	;get cluster size
	sub	di,4*2		;don't count header
	cmp	dx,di		;could this # of map words fit in cluster?
	ja	dbfr3		;no, invalid
	mov	di,cx		;copy starting cluster number
	sub	di,bx		;find relative cluster (bit # in map)
	shr	di,1		;find byte # in map
	shr	di,1
	shr	di,1
	cmp	di,dx		;is the starting cluster in this map?
	jb	dbfr4		;yes
	test	dh,340		;make sure in range
	jnz	dbfr3		;too large
	add	dx,dx		;# map bytes *8 = # of clusters mapped
	add	dx,dx
	add	dx,dx
	add	bx,dx		;advance to starting block # of next map
	mov	ax,ds:dbdat[si]	;follow link to next map
	jmp	short dbfr1	;keep looking
dbfr2:	pop	bx		;flush stack
dbfr3:	stc			;end of disk, or corrupt map structure
	ret			;either way, no more free clusters
dbfr4:	; found our starting block, start searching for free bits
	push	bx		;save starting mapped cluster #
	mov	bx,di		;copy offset
	mov	al,1		;load a 1
	push	cx		;save
	and	cl,7		;isolate starting bit #
	rol	al,cl		;rotate into place
	pop	cx		;restore
dbfr5:	; search for next cleared bit
	test	byte ptr ds:dbdat+(4*2)[bx+si],al ;found one?
	jz	dbfr6		;yes, start counting
	inc	cx		;bump to next block
	cmp	cx,ss:numclu[bp] ;off EOD?
	jae	dbfr2		;yes, no more free space
	rol	al,1		;shift the bit left
	jnc	dbfr5		;keep looking
	inc	bx		;bump to next byte
	cmp	bx,dx		;finished this map block?
	jb	dbfr5		;loop if not
	pop	bx		;catch starting blk # of map
	call	dbnmap		;read next map cluster
	push	bx		;[save new value]
	mov	bx,0		;[start with first byte of map]
	jnc	dbfr5		;loop
	pop	bx		;[error, clear stack]
	ret
dbfr6:	; search for end of cleared bits
	mov	di,cx		;save start
dbfr7:	cmp	di,ss:numclu[bp] ;off EOD?
	jae	dbfr8		;yes, end of cleared bits
	test	byte ptr ds:dbdat+(4*2)[bx+si],al ;clear?
	jnz	dbfr8		;no, we're done
	inc	di		;count the block
	rol	al,1		;shift the bit left
	jnc	dbfr7		;keep looking
	inc	bx		;bump to next byte
	cmp	bx,dx		;finished this map block?
	jb	dbfr7		;loop if not
	test	word ptr ds:dbdat[si],-1 ;end of chain?
	jz	dbfr8		;yes, keep what we've got
	pop	bx		;catch starting blk # of map
	call	dbnmap		;read next map cluster
	push	bx		;[save new value]
	mov	bx,0		;[start with first byte of map]
	jnc	dbfr7		;loop
	pop	bx		;[error, clear stack]
	ret
dbfr8:	; finished
	mov	ax,ds:dbblk[si]	;get bitmap cluster #
	pop	bx		;catch starting cluster mapped by AX
	mov	dx,di		;copy clu # 1 past last free clu
	sub	dx,cx		;find # free clusters, CF=0
	ret
;
dbnmap:	; read next map cluster
	; BX=starting clu # mapped by previous map, DX=# bytes in curr map
	test	dh,340		;make sure in range
	jnz	dbnmp1		;too large
	add	dx,dx		;# map bytes *8 = # of clusters mapped
	add	dx,dx
	add	dx,dx
	add	bx,dx		;advance to starting block # of next map
	jc	dbnmp1		;too large
	cmp	bx,ss:numclu[bp] ;off end of disk?
	jae	dbnmp1
	push	bx		;save
	push	cx
	push	di
	mov	ax,ds:dbdat[si]	;follow link to next map
	xor	cl,cl		;read if not cached
	call	dbdir		;get dir block
	pop	di		;[restore]
	pop	cx
	pop	bx
	jc	dbnmp1		;error
	mov	dx,ds:dbdat+4[si] ;get # valid map words in bitmap block
	add	dx,dx		;# valid bytes
	jbe	dbnmp1		;>= 64 KB, or 0, invalid
	mov	ax,ss:pcs[bp]	;get cluster size
	sub	ax,4*2		;don't count header
	cmp	dx,ax		;could this # of map words fit in cluster?
	ja	dbnmp1		;no, invalid
	mov	al,1		;starting with first bit
	clc			;happy
	ret			;[DX/BX updated, AL=1 again]
dbnmp1:	stc			;error
	ret
;+
;
; Set bit(s) in DOS/BATCH bitmap.
;
; cx	starting cluster to mark as in use
; dx	number of clusters to mark as in use
; ss:bp	log dev rec
;
;-
dbsbit:	push	dx		;save # bits to set
	xor	bx,bx		;start counting at beginning of disk
	mov	ax,ss:bitmap[bp] ;get pointer to first bitmap block
dbsb1:	cmp	bx,ss:numclu[bp] ;off EOD?
	jae	dbsb2		;yes, no more free space
	push	ax		;save
	push	bx
	push	cx
	xor	cl,cl		;read if not cached
	call	dbdir		;get dir block
	pop	cx		;restore
	pop	bx
	pop	ax
	mov	dx,ds:dbdat+4[si] ;get # valid map words in bitmap block
	add	dx,dx		;# valid bytes
	jbe	dbsb2		;>= 64 KB, or 0, invalid
	mov	di,ss:pcs[bp]	;get cluster size
	sub	di,4*2		;don't count header
	cmp	dx,di		;could this # of map words fit in cluster?
	ja	dbsb2		;no, invalid
	mov	di,cx		;copy starting cluster number
	sub	di,bx		;find relative cluster (bit # in map)
	shr	di,1		;find byte # in map
	shr	di,1
	shr	di,1
	cmp	di,dx		;is the starting cluster in this map?
	jb	dbsb3		;yes
	test	dh,340		;make sure in range
	jnz	dbsb2		;too large
	add	dx,dx		;# map bytes *8 = # of clusters mapped
	add	dx,dx
	add	dx,dx
	add	bx,dx		;advance to starting block # of next map
	mov	ax,ds:dbdat[si]	;follow link to next map
	jmp	short dbsb1	;keep looking
dbsb2:	pop	dx
	stc			;end of disk, or corrupt map structure
	ret			;either way, no more free clusters
dbsb3:	; found our starting block, start setting bits
	pop	ax		;catch # bits to set
	push	bx		;save starting mapped cluster #
	mov	bx,di		;copy offset
	mov	di,ax		;copy count
	mov	al,1		;load a 1
	push	cx		;save
	and	cl,7		;isolate starting bit #
	rol	al,cl		;rotate into place
	pop	cx		;restore
dbsb4:	; start setting bits in new cluster buf
	mov	byte ptr ds:dbdrt[si],al ;buffer will be dirty
dbsb5:	; set next bit
	or	byte ptr ds:dbdat+(4*2)[bx+si],al ;set a bit
	dec	di		;done?
	jz	dbsb6		;yes
	inc	cx		;bump to next block
	rol	al,1		;shift the bit left
	jnc	dbsb5		;keep looking
	inc	bx		;bump to next byte
	cmp	bx,dx		;finished this map block?
	jb	dbsb5		;loop if not
	pop	bx		;catch starting blk # of map
	push	di		;save
	call	dbnmap		;read next map cluster
	pop	di		;[restore]
	push	bx		;[save new value]
	mov	bx,0		;[start with first byte of map]
	jnc	dbsb4		;loop
	pop	bx		;[error, clear stack]
	ret
dbsb6:	pop	bx		;flush stack
	clc			;happy
	ret
;+
;
; Clear bit(s) in DOS/BATCH bitmap.
;
; cx	starting cluster to mark as free
; dx	number of clusters to mark as free
; ss:bp	log dev rec
;
;-
dbcbit:	push	dx		;save # bits to clear
	xor	bx,bx		;start counting at beginning of disk
	mov	ax,ss:bitmap[bp] ;get pointer to first bitmap block
dbcb1:	cmp	bx,ss:numclu[bp] ;off EOD?
	jae	dbcb2		;yes, no more free space
	push	ax		;save
	push	bx
	push	cx
	xor	cl,cl		;read if not cached
	call	dbdir		;get dir block
	pop	cx		;restore
	pop	bx
	pop	ax
	mov	dx,ds:dbdat+4[si] ;get # valid map words in bitmap block
	add	dx,dx		;# valid bytes
	jbe	dbcb2		;>= 64 KB, or 0, invalid
	mov	di,ss:pcs[bp]	;get cluster size
	sub	di,4*2		;don't count header
	cmp	dx,di		;could this # of map words fit in cluster?
	ja	dbcb2		;no, invalid
	mov	di,cx		;copy starting cluster number
	sub	di,bx		;find relative cluster (bit # in map)
	shr	di,1		;find byte # in map
	shr	di,1
	shr	di,1
	cmp	di,dx		;is the starting cluster in this map?
	jb	dbcb3		;yes
	test	dh,340		;make sure in range
	jnz	dbcb2		;too large
	add	dx,dx		;# map bytes *8 = # of clusters mapped
	add	dx,dx
	add	dx,dx
	add	bx,dx		;advance to starting block # of next map
	mov	ax,ds:dbdat[si]	;follow link to next map
	jmp	short dbcb1	;keep looking
dbcb2:	pop	dx
	stc			;end of disk, or corrupt map structure
	ret			;either way, no more free clusters
dbcb3:	; found our starting block, start clearing bits
	pop	ax		;catch # bits to clear
	push	bx		;save starting mapped cluster #
	mov	bx,di		;copy offset
	mov	di,ax		;copy count
	mov	al,not 1	;load a 0
	push	cx		;save
	and	cl,7		;isolate starting bit #
	rol	al,cl		;rotate into place
	pop	cx		;restore
dbcb4:	; start clearing bits in new cluster buf
	mov	byte ptr ds:dbdrt[si],al ;buffer will be dirty
dbcb5:	; clear next bit
	and	byte ptr ds:dbdat+(4*2)[bx+si],al ;clear a bit
	dec	di		;done?
	jz	dbcb6		;yes
	inc	cx		;bump to next block
	rol	al,1		;shift the bit left
	jc	dbcb5		;keep looking
	inc	bx		;bump to next byte
	cmp	bx,dx		;finished this map block?
	jb	dbcb5		;loop if not
	pop	bx		;catch starting blk # of map
	push	di		;save
	call	dbnmap		;read next map cluster
	pop	di		;[restore]
	push	bx		;[save new value]
	mov	bx,0		;[start with first byte of map]
	mov	al,not 1	;[bit 0 of that byte]
	jnc	dbcb4		;loop
	pop	bx		;[error, clear stack]
	ret
dbcb6:	pop	bx		;flush stack
	clc			;happy
	ret
;+
;
; Get DOS/BATCH dir cluster.
;
; ax	cluster number
; cl	NZ to return buf but don't read if not cached (going to write it)
; ss:bp	log dev rec
;
; Returns ptr to cluster buffer in DS:SI (data at SI+DBDAT).
;
;-
dbdir:	mov	si,ss:dirbuf[bp] ;head of buffer chain
	test	word ptr ds:dbblk[si],-1 ;is this our first buf request?
	jz	dbdr8		;yes, use our pre-alloced buf
	xor	bx,bx		;no prev
dbdr1:	; see if we already have it
	; SI=buf to check, BX=prev or 0, AX=cluster number
	cmp	ax,ds:dbblk[si]	;is this it?
	je	dbdr2		;yes, relink and return
	mov	di,ds:dbnxt[si]	;get ptr to next
	test	di,di		;if any
	jz	dbdr4		;none, need to read it
	mov	bx,si		;save for linking
	mov	si,di		;follow link
	jmp	short dbdr1	;keep looking
dbdr2:	; found it
	test	bx,bx		;is it first in line?
	jz	dbdr3		;yes, no need to relink
	mov	ax,si		;pt at it
	xchg	ax,ss:dirbuf[bp] ;get head of chain, set new head
	xchg	ax,ds:dbnxt[si]	;link rest of chain to head
	mov	ds:dbnxt[bx],ax	;unlink the one we just moved
dbdr3:	ret
dbdr4:	; we don't have it, try to allocate a new buffer
	; SI=LRU buf, BX=the one before that or 0 if there's only one buf
	; AX=cluster #
	push	si		;save
	push	cx
	mov	cx,dbdat	;begn of data field
	add	cx,ss:pcs[bp]	;size of dir buffer
	call	askmem		;ask for a buffer
	pop	cx		;[catch flag]
	pop	di		;[catch ptr to last buf in chain]
	jnc	dbdr7		;got it, read the buf in
	mov	si,di		;restore
	; failed, write out the last one
	test	bx,bx		;is there a penultimate buffer?
	jz	dbdr5		;no
	mov	ds:dbnxt[bx],0	;yes, unlink final buffer from it
	jmp	short dbdr6
dbdr5:	mov	ss:dirbuf[bp],bx ;this is the only one, unlink it
dbdr6:	push	ax		;save blk
	push	cx		;and flag
	call	dbfdir		;flush it
	pop	cx		;restore
	pop	ax
dbdr7:	; link buf in as first
	mov	bx,si		;copy ptr
	xchg	bx,ss:dirbuf[bp] ;get current chain, set new head
	mov	ds:dbnxt[si],bx	;link it on
dbdr8:	; buf is linked in at head of chain -- SI=buf, AX=clu #, CL=flg
	mov	ds:dbblk[si],ax	;set cluster #
	mov	ds:dbdrt[si],0	;buf is clean
	test	cl,cl		;should we read it?
	jnz	dbdr9		;no, all we wanted was the buf
	mov	cx,1		;read 1 clu
	lea	di,ds:dbdat[si]	;point at buffer
	push	si		;save
	call	dbrclu		;read cluster
	pop	si		;[restore]
	jc	dbdr10		;err
dbdr9:	ret
dbdr10:	jmp	dirio
;+
;
; Flush DOS/BATCH dir buffer, if dirty.
;
; ss:bp	dev rec (preserved)
; ds:si	ptr to buffer (preserved)
;
;-
dbfdir:	test	byte ptr ds:dbdrt[si],-1 ;is buf dirty?
	jz	dbfdr1		;no
	mov	ax,ds:dbblk[si]	;get clu #
	mov	cx,1		;write one cluster
	push	si		;save
	add	si,dbdat	;point at data
	call	dbwclu		;write cluster
	pop	si		;[restore]
	jc	dbfdr2		;err
	mov	byte ptr ds:dbdrt[si],0 ;no longer dirty
dbfdr1:	ret
dbfdr2:	jmp	dirio
;
	subttl	RT-11 file structure
;
	comment	_

An RT-11 directory consists of 1 to 31. segments, each of which is two
256-word blocks long, including a 5-word header:

+0	dw	total number of segments (1-31.)
+2	dw	next dir seg (1-31., not block #) or 0 if no more
+4	dw	seg # of highest seg in use (valid in seg 1 only)
+6	dw	# info bytes per dir entry (must be even)
+10	dw	starting blk # of 1st file (for this seg)
+12 first dir entry starts here

Directory segments are normally allocated with segment 1 occupying device
blocks 6 and 7, segment 2 in blocks 8. and 9., and so on, with the first data
block starting immediately after the last directory segment.  Segments higher
than the highest seg in use (3rd word of header) are uninitialized but are
still reserved for later directory use (they will be linked into the directory
when there is no space left in the current chain).  The directory is actually
supposed to be movable with the actual starting block number located in the
word at offset 724(8) in the home block, but it appears that RT-11 actually
ignores this and is hard-coded for block 6.

In each segment, directory entries begin immediately after the 5-word header.

Entry format:

+0	.word	status		;status bits, see below
+2	.rad50	/FILNAM/	;file name
+6	.rad50	/EXT/		;extension
+10	.word	length		;length of file in blocks
+12	.byte	channel		;channel on which file is open (if tentative)
+13	.byte	job		;job # that has file open (if tentative)
+14	.word	creation	;date of creation, YYMMMMDDDDDYYYYY (Y=yr-1972)
+16	.blkb	n		;extra bytes, n is even, word 4 of header

Status bits:
400	tentative file (not .CLOSEd yet)
1000	< UNUSED > area
2000	permanent file
102000  protected permanent file
4000	end of segment (rest of entry is missing)

The high two YY bits in the date are an afterthought added in RT-11 V5 to
extend the date format beyond AD 2004.  Previous versions don't know about
them, and there are bugs in the SYSLIB functions that touch them anyway.

Note that TSX+ saves the creation time of permanent files in the word at +12
(which is no longer needed once the file is no longer tentative).  Just to be
annoying, the time is stored as the # of seconds since midnight, divided by 3
to get a positive 16-bit number (while RT-11's internal time is the 32-bit # of
50/60 Hz clock ticks since midnight).
_
;+
;
; Print volume name.
;
; ss:bp	log dev rec
; es:di	buf (updated on return)
;
;-
rtvn:	push	di		;save
	mov	di,ds:dbuf	;pt at buffer
	mov	ax,ss:actdir[bp] ;get base
	mov	dx,ss:actdir+2[bp]
	add	ax,1		;rel blk 1 (home blk)
	adc	dx,0
	mov	cx,1		;count=1 blk
	call	ss:rdblk[bp]	;fetch it
	jc	rtvn3
	add	si,730		;get ptr to vol ID
	pop	di
	cram	' is '		;assume this will work
	mov	bx,di		;save a copy
	call	rtvncp		;copy vol name
	cmp	bx,di		;did we move?
	je	rtvn1		;skip if not
	mov	al,' '		;add a blank
	stosb			;save
rtvn1:	call	rtvncp		;copy owner name
	cmp	di,bx		;did we get anything?
	jne	rtvn2		;no
	sub	di,4		;back up
	jmp	nolbl		;no label
rtvn2:	ret
rtvn3:	jmp	rderr
;
rtvncp:	; copy vol name described by SI, CX
	mov	cx,12d		;length
	call	skip		;skip any leading blanks
	jc	rtvnc3		;missing, skip
	mov	dx,di		;init
rtvnc1:	lodsb			;get a byte
	stosb			;save it
	cmp	al,' '		;blank?
	je	rtvnc2
	mov	dx,di		;no, save position
rtvnc2:	loop	rtvnc1
	cmp	al,' '		;were there trailing blanks?
	jne	rtvnc3		;no, don't trim
	mov	di,dx		;pt past last non-blank char
rtvnc3:	ret
;+
;
; Set defaults for RT-11.
;
; si,cx	preserved
;
;-
rtds:	mov	ax,ss:curdir[bp] ;get current LD:
	mov	dx,ss:curdir+2[bp]
	mov	ss:actdir[bp],ax ;set active LD:
	mov	ss:actdir+2[bp],dx
	mov	ax,ss:cursiz[bp] ;get size of LD:
	mov	ss:actsiz[bp],ax
	xor	ax,ax		;high word always 0
	mov	ss:actsiz+2[bp],ax
	ret
;+
;
; Save RT-11 CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
rtsav:	mov	ax,ss:actdir[bp] ;get active LD:
	mov	dx,ss:actdir+2[bp]
	mov	ds:savdir[bx],ax
	mov	ds:svdirh[bx],dx
	mov	ax,ss:actsiz[bp]
	mov	ds:savsiz[bx],ax
	ret
;+
;
; Restore RT-11 CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
rtrst:	mov	ax,ds:savdir[bx] ;get saved stuff
	mov	dx,ds:svdirh[bx]
	mov	ss:actdir[bp],ax ;set active LD:
	mov	ss:actdir+2[bp],dx
	mov	ax,ds:savsiz[bx]
	mov	ss:actsiz[bp],ax
	ret
;+
;
; Print directory heading.
;
;-
rtdh:	mov	ds:nfile,0	;no files yet
	mov	ds:nblk,0	;no blocks seen
	mov	ds:nfree,0	;no frees
	mov	di,offset lbuf	;pt at buf
	mov	al,' '		;blank
	stosb
	mov	ax,word ptr ds:day ;get month,,day
	mov	dx,ds:year	;year
	call	pdate1		;print
	callr	flush		;flush, return
;
rtdi:	; RT-11 dir input init
	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	rtdi1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir seg buffer (nothing is possible if this fails)
	mov	cx,rtslen	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:rtsblk[si],ax ;nothing in here yet
	mov	ds:rtsblk+2[si],ax
	mov	ds:rtsdrt[si],al ;not dirty
	mov	ds:rtsnxt[si],ax ;no next
rtdi1:	mov	ax,1		;first seg
	mov	ds:inseg,al	;(save)
	call	rtseg		;read it (AH=0)
	mov	ax,ss:actdir[bp] ;get base of dir
	mov	dx,ss:actdir+2[bp]
	add	ax,[si+8d]	;add starting block for 1st file
	adc	dx,0
	mov	ds:stblk,ax	;save
	mov	ds:stblk+2,dx
	mov	ds:fsize,0	;say preceding file null
	mov	ds:inent,5*2	;start just after header
	ret
;
rtdg:	; get RT-11 dir entry
	mov	al,ds:inseg	;make sure seg is still there
	cbw			;AH=0 (really read it)
	call	rtseg		;(in case output is same device, may page out)
	mov	bx,si		;save base (subtracted before returning)
	add	si,ds:inent	;index to curr posn
rtdg1:	; get next entry (SI=ptr, BX=base of buf)
	mov	word ptr ds:ftimf,0 ;no date or time
	mov	ax,ds:fsize	;get size of previous
	add	ds:stblk,ax	;update starting blk ptr
	adc	ds:stblk+2,0
	mov	di,offset lbuf	;pt at buf (whether empty or not)
	lodsw			;get status word
	test	ah,4000/100h	;end of segment?
	jz	$+5
	 jmp	rtdg5		;yes, go get next seg
	mov	ds:fattr,ah	;save
	test	ah,1400/100h	;empty area or tentative file?
	jnz	rtdg4
	push	bx		;save BX
	lodsw			;FILNAM
	call	rad50
	lodsw
	call	rad50
	mov	al,'.'		;.
	stosb
	lodsw			;EXT
	call	rad50
	pop	bx		;restore
	lodsw			;get length
	mov	ds:fsize,ax	;save
	mov	cl,7d		;shift count
	ror	ax,cl		;compute # bytes in file (including NULs)
	mov	dx,ax		;get high 7 bits
	and	ax,177000	;isolate low 7 bits
	and	dh,1		;and high 9
	mov	ds:fbcnt,ax	;save
	mov	ds:fbcnt+2,dx
	inc	si		;skip channel #
	inc	si		;job #
	lodsw			;get date
	test	ax,ax		;valid?
	jz	rtdg2		;no
	; RT-11 date format is:  YYMMMMDDDDDYYYYY
	; (leftmost two Y bits were added (compatibly) in RT-11 V5)
	; MMMM, DDDDD -- 1-12. and 1-31. as usual
	; YYYYYYY is YEAR-1972.
	mov	dx,ax		;get month,,day
	mov	cl,2		;bit count
	shr	dx,cl		;shift both
	inc	cx		;change to 3
	shr	dl,cl		;move day into place
	and	dx,0F1Fh	;isolate
	mov	word ptr ds:fday,dx ;save
	and	ax,140037	;isolate year
	shr	ah,1		;V5 bits right 1
	or	al,ah		;concatenate
	cbw			;AH=0
	add	ax,1972d	;add base year
	mov	ds:fyear,ax	;save
	inc	ds:fdatf	;set flag
rtdg2:	sub	si,bx		;subtract base
	mov	ds:pextra,si	;save ptr to RTS or who knows what
	add	si,ss:extra[bp]	;skip extra bytes if any
	mov	ds:inent,si	;update
	callr	sqfn		;make squished copy of filename, return
rtdg4:	jmp	short rtdg6
rtdg5:	; no more entries in this segment, read next one
	mov	ax,[bx+2]	;get next seg #
	test	ax,ax		;any?
	jz	rtdg7		;no
	mov	ds:inseg,al	;yes, save #
	cbw			;AH=0 (really read)
	call	rtseg		;get the segment
	mov	bx,si		;save ptr to base of buf
	mov	ds:fsize,0	;say prev file null (already added)
	add	si,4*2		;skip to starting blk
	lodsw			;get starting data blk #
;	cmp	ax,ds:stblk	;should match
;	jne	rtdg8		;dir consistency error
;;; yeah it should match but need to subtract ACTDIR first, and now
;;; both STBLK and ACTDIR are 32-bit numbers
	jmp	rtdg1		;try again
rtdg6:	; < UNUSED >
	add	si,6		;skip filename
	lodsw			;get file length
	mov	ds:fsize,ax	;save
	add	ds:nfree,ax	;update # frees (even if not displayed)
	add	si,4		;skip open file info, creation date
	add	si,ss:extra[bp]	;skip extra bytes
	sub	si,bx		;subtract base
	mov	ds:inent,si	;update
	xor	al,al		;CF=0 ZF=1
	ret
rtdg7:	; end of dir
	stc			;no more
	ret
rtdg8:	jmp	baddir		;starting blk # from header is wrong
;+
;
; Make squished copy of filename in LBUF.
;
; di	ptr to end of filename
;
; On return, DI is unchanged, the buffer space starting at DI contains an
; ASCIZ copy of the filename between LBUF and DI, with blanks removed.
;
; Returns CF=ZF=0, suitable for direct return from DIRGET.
;
;-
sqfn:	mov	dx,di		;save ptr
	mov	si,offset lbuf	;pt at formated filename
	mov	cx,di		;calc length
	sub	cx,si
sqfn1:	lodsb			;get one
	cmp	al,' '		;blank?
	je	sqfn2
	 stosb			;no, save
sqfn2:	loop	sqfn1		;loop
	xor	al,al		;mark end (CF=0)
	stosb
	mov	di,dx		;[restore]
	inc	ax		;CF still 0, ZF=0
	ret
;
rtdf:	; RT-11 dir finish (flush dirty segment buffers)
	mov	si,ss:dirbuf[bp] ;get ptr to buf chain
rtdf1:	call	rtfseg		;flush
	mov	si,ds:rtsnxt[si] ;follow chain
	test	si,si		;anything?
	jnz	rtdf1		;loop if so
	ret
;+
;
; Print directory listing summary.
;
;-
rtsum:	mov	di,offset lbuf	;pt at buf
	mov	al,' '		;blank
	stosb
	mov	ax,ds:nfile	;get # files shown
	push	ax		;save
	call	pnum
	cram	' File'
	pop	ax		;restore
	call	adds		;add s
	mov	ax," ,"		;', '
	stosw
	mov	ax,ds:nblk	;get # blocks shown
	push	ax		;save
	call	pnum		;print
	cram	' Block'
	pop	ax		;restore
	call	adds		;add s
	call	flush		;flush
	mov	al,' '		;blank
	stosb
	mov	ax,ds:nfree	;get # free blocks
	push	ax		;save
	call	pnum
	cram	' Free block'
	pop	ax		;restore
	call	adds		;add s
	callr	flush		;flush, return
;
adds:	; add "s" to output buf unless AX=1
	dec	ax		;just one?
	jz	adds1
	mov	al,'s'		;add s if not
	stosb
adds1:	ret
;
dadds:	; double precision version of above ("s" unless DX:AX=1)
	test	dx,dx		;definitely not 1?
	jnz	dadds1
	dec	ax		;just one?
	jz	dadds2
dadds1:	mov	al,'s'		;add s if not
	stosb
dadds2:	ret
;+
;
; Display RT-11 dir name.
;
; ss:bp	log dev rec
; es:di	buf
; cx	1 to print trailing \, 0 not to
;
;-
rtdn:	; display RT-11 dir name
	mov	al,'\'		;starts with \
	stosb
	xor	ax,ax		;load 0
	xchg	ax,ss:actdir[bp] ;get active dir, set to 0
	test	ax,ax		;already at root?
	jnz	rtdn1
	 ret			;yes, no dir name
rtdn1:	; search for a file containing that offset
	mov	dx,ss:actdir+2[bp] ;get high addr (partition #)
	mov	ds:rtdnam+2,ax	;save abs block #
	mov	ds:rtdnam+4,dx
	mov	ds:rtdnam,0	;search by block #
	mov	ax,ss:devsiz[bp] ;get root dev size
	mov	ss:actsiz[bp],ax
	xor	cl,1		;1 not to print trailing \
	mov	ds:rtdsep,cx	;save flag
rtdn2:	push	di		;save buf ptr
	call	rtdir		;look for dir
	pop	di		;[restore]
	jc	rtdn4		;not found, should never happen
	push	ax		;save relative offset
	mov	ax,ds:stblk	;get starting abs block of dir file
	mov	dx,ds:stblk+2
	mov	ss:actdir[bp],ax ;save
	mov	ss:actdir+2[bp],dx
	mov	ax,[si+8d]	;get size of dir
	mov	ss:actsiz[bp],ax ;save
	inc	si		;skip flags
	inc	si
	lodsw			;get dir name
	call	radnb
	lodsw
	call	radnb
	cmp	word ptr [si],16003 ;.rad50/DSK/?
	je	rtdn3		;yes, save
	mov	al,'.'		;'.'
	stosb
	lodsw			;extension
	call	radnb
rtdn3:	mov	al,'\'		;'\'
	stosb
	pop	ax		;get relative block #
	test	ax,ax		;is this the dir itself?
	jnz	rtdn2		;loop if not
	sub	di,ds:rtdsep	;unput \ if needed
	ret
rtdn4:	jmp	baddir
;+
;
; Change RT-11 directory.
;
;-
rtcd:	mov	ax,1		;no \ needed after last pathname element
	call	rtgetd		;get dir
	call	confrm		;make sure confirmed
	mov	ax,ss:actdir[bp] ;get active dir, make current
	mov	dx,ss:actdir+2[bp]
	mov	ss:curdir[bp],ax
	mov	ss:curdir+2[bp],dx
	mov	ax,ss:actsiz[bp]
	mov	ss:cursiz[bp],ax
	ret
;+
;
; Get RT-11 directory name (and make it active).
;
;-
rtgd:	xor	ax,ax		;require \ at end of each pathname element
rtgetd:	; enter here with AX=flag (for terminating char)
	mov	ds:rtdsep,ax	;save
	jcxz	rtgd1		;done if nothing
	cmp	al,ds:swchar	;switchar?
	je	rtgd1		;EOL
	cmp	al,'/'		;path from root?
	je	rtgd2
	cmp	al,'\'		;either way
	je	rtgd2
	jmp	short rtgd5	;no, skip
rtgd1:	ret
rtgd2:	; starting at root, not current LD:
	mov	ss:actdir[bp],0	;start at begn of partition
	mov	ax,ss:actdir+6[bp] ;see if huge disk
	or	ax,ss:actdir+4[bp]
	jnz	rtgd3		;yes
	mov	ax,ss:actdir+2[bp] ;get partition
	cmp	ax,ss:devsiz+2[bp] ;is this last partition of disk?
	jne	rtgd3		;no
	mov	ax,ss:devsiz[bp] ;yes, use partial partition
	jmp	short rtgd4	;skip
rtgd3:	mov	ax,0FFFFh	;biggest possible partition
rtgd4:	mov	ss:actsiz[bp],ax ; save
	inc	si		;eat the '\'
	dec	cx
rtgd5:	push	si		;save posn
	push	cx
	; see if . or ..
	jcxz	rtgd6		;skip if nothing
	cmp	byte ptr [si],'.' ;starts with .?
	je	rtgd14
rtgd6:	call	grad		;get filename
	jc	rtgd12
	mov	ds:rtdnam,bx	;save filename
	mov	ds:rtdnam+2,dx
	mov	bx,16003	;default ext (.RAD50/DSK/)
	cmp	al,'.'		;ends with dot, right?
	jne	rtgd7
	inc	si		;eat the dot
	dec	cx
	call	grad		;get extension
rtgd7:	mov	ds:rtdnam+4,bx	;save extension
	cmp	ds:rtdsep,0	;does it have to end in path sep?
	jnz	rtgd9		;no, so don't check
	; part of a filespec, make sure path element ends with / or \
	cmp	al,ds:swchar	;switch?
	je	rtgd12		;yes, not path sep
	cmp	al,'/'		;path sep?
	je	rtgd8
	cmp	al,'\'		;either way
	jne	rtgd12		;no
rtgd8:	inc	si		;yes, eat the separator
	dec	cx
rtgd9:	pop	ax		;flush stack
	pop	ax
	push	si		;save line descriptor
	push	cx
	call	rtdir		;look up dir element
	mov	ax,[si+10]	;[get size]
	pop	cx		;[restore]
	pop	si
	jc	rtgd11		;error
	mov	ss:actsiz[bp],ax ;save size
	mov	ax,ds:stblk	;get starting block
	mov	dx,ds:stblk+2
	mov	ss:actdir[bp],ax ;save
	mov	ss:actdir+2[bp],dx
rtgd10:	; decide whether to look for more (depending on separator)
	cmp	ds:rtdsep,0	;did path el have to end in path sep?
	jz	rtgd5		;yes, so we've already checked
	jcxz	rtgd13		;that's it
	mov	al,[si]		;get separator char
	cmp	al,ds:swchar	;switch?
	je	rtgd13		;yes, return
	inc	si		;no, eat it
	dec	cx
	cmp	al,'/'		;more to come?
	je	rtgd5
	cmp	al,'\'
	je	rtgd5
	jmp	synerr		;no
rtgd11:	jmp	dnf		;dir not found
rtgd12:	pop	cx		;unget
	pop	si
rtgd13:	ret
rtgd14:	; . or ..
	inc	si		;eat the .
	dec	cx
	jz	rtgd15		;just .
	cmp	byte ptr [si],'.' ;..?
	je	rtgd18		;yes
rtgd15:	; . -- do nothing
	call	rdig		;make sure that's the last radix 50 char
	jnc	rtgd12		;it should be
	cmp	ds:rtdsep,0	;do we care what the sep is?
	jnz	rtgd17		;no
	cmp	al,ds:swchar	;switch?
	je	rtgd12		;yes, not path sep
	cmp	al,'/'		;path sep?
	je	rtgd16
	cmp	al,'\'		;either way
	jne	rtgd12		;no
rtgd16:	inc	si		;eat it
	dec	cx
rtgd17:	pop	ax		;flush stack
	pop	ax
	jmp	short rtgd10
rtgd18:	; .. -- go up a directory
	inc	si		;eat the .
	dec	cx
	call	rdig		;make sure that's the last radix 50 char
	jnc	rtgd12		;filename if not
	cmp	ds:rtdsep,0	;do we care what the sep is?
	jnz	rtgd20		;no
	cmp	al,ds:swchar	;switch?
	je	rtgd12		;yes, not path sep
	cmp	al,'/'		;path sep?
	je	rtgd19
	cmp	al,'\'		;either way
	jne	rtgd12		;no
rtgd19:	inc	si		;eat it
	dec	cx
rtgd20:	pop	ax		;flush stack
	pop	ax
	; search disk for directory containing current dir
	xor	ax,ax		;load 0
	xchg	ax,ss:actdir[bp] ;get active dir, set to 0
	mov	dx,ss:actdir+2[bp]
	test	ax,ax		;root already?
	jz	rtgd10		;yes, no op
	mov	bx,ss:actdir+6[bp] ;see if huge disk
	or	bx,ss:actdir+4[bp]
	jnz	rtgd21		;yes
	mov	bx,ss:actdir+2[bp] ;get partition
	cmp	bx,ss:devsiz+2[bp] ;is this last partition of disk?
	jne	rtgd21		;no
	mov	bx,ss:devsiz[bp] ;yes, use partial partition
	jmp	short rtgd22	;skip
rtgd21:	mov	bx,0FFFFh	;biggest possible partition
rtgd22:	mov	ss:actsiz[bp],bx ;save
	push	si		;save line descriptor
	push	cx
	; search for a file containing this dir's offset
	mov	ds:rtdnam+2,ax	;save abs block #
	mov	ds:rtdnam+4,dx
	mov	ds:rtdnam,0	;search by block #
rtgd23:	call	rtdir		;look for dir
	jc	rtgd25		;not found, should never happen
	test	ax,ax		;is this the dir itself?
	jz	rtgd24		;yes, don't CD into it
	mov	ax,ds:stblk	;get starting abs block of dir file
	mov	dx,ds:stblk+2
	mov	ss:actdir[bp],ax ;save
	mov	ss:actdir+2[bp],dx
	mov	ax,[si+8d]	;get size of dir
	mov	ss:actsiz[bp],ax ;save
	jmp	short rtgd23
rtgd24:	pop	cx
	pop	si
	jmp	rtgd10
rtgd25:	jmp	baddir
;
rtdd:	; display RT-11 dir entry
	mov	ax,ds:fsize	;size
	inc	ds:nfile	;bump # files
	add	ds:nblk,ax	;and # blks
	xor	dx,dx		;0-extend
	mov	cx,6		;size of field
	call	cnum		;print it right-justified
	; add P for Protected
	mov	cx,14d		;# blanks to write if RTS but no date
	test	byte ptr ds:fattr,200 ;protected?
	jz	rtdd1
	mov	al,'P'		;yes
	stosb
	dec	cx
rtdd1:	; add date, if any
	cmp	byte ptr ds:fdatf,0 ;was there a date?
	jz	rtdd2		;no
	sub	cl,12d		;1 or 2 blanks
	mov	al,' '
	rep	stosb
	call	pdate		;print
	mov	cx,2		;just two blanks if RTS
rtdd2:	; if there are two extra words, it may be RSTS RTS name (from FIT.BAC)
	cmp	word ptr ss:extra[bp],4 ;4 bytes?
	jne	rtdd3
	mov	al,' '		;yes, pad
	rep	stosb
	mov	si,ds:pextra	;point at them
	add	si,ss:dirbuf[bp] ;add base of dir buf
	lodsw			;RTS
	call	rad50
	lodsw			;NAM
	call	rad50
	mov	cx,2		;2 blanks needed
rtdd3:	; add starting block if verbose listing
	test	byte ptr ds:dirflg,200 ;verbose?
	jz	rtdd4
	add	cx,5		;add 5-digit block # to # cols coming
	mov	ax,ds:stblk	;get starting block #
	mov	dx,ds:stblk+2
	sub	ax,ss:actdir[bp] ;subtract base
	sbb	dx,ss:actdir+2[bp]
	call	cnum		;print it right-justified
rtdd4:	ret
;
rtde:	; display empty RT-11 entry
	mov	di,offset lbuf	;pt at buf again
	cram	'< UNUSED >'
	mov	cx,6		;# spaces
	mov	ax,ds:fsize	;size
	xor	dx,dx		;0-extend
	jmp	cnum		;print it right-justified, return (no date)
;
rtop:	; open RT-11 file for input
	mov	ax,ds:stblk	;get starting blk #
	mov	dx,ds:stblk+2
	mov	ds:iblk,ax	;save
	mov	ds:iblk+2,dx
	mov	ax,ds:fsize	;get size
	mov	ds:isize,ax	;save
	clc			;happy
	ret
;
rtrd:	; read RT-11 file data
	mov	bx,ds:isize	;get remaining size
	test	bx,bx		;EOF?
	jz	rtrd3		;yes, ZF=1
	mov	al,ch		;get # full blocks free in buf
	shr	al,1
	jz	rtrd2
	cbw			;AH=0
	cmp	ax,bx		;more than what's left?
	jb	rtrd1
	 mov	ax,bx		;stop at EOF
rtrd1:	mov	cx,ax		;copy block count
	mov	ax,ds:iblk	;get starting block
	mov	dx,ds:iblk+2
	add	ds:iblk,cx	;update
	adc	ds:iblk+2,0
	sub	ds:isize,cx
	mov	si,ds:binflg	;get binary flag
	call	ss:rdblk[bp+si]	;read data
	jc	rtrd4
	ret
rtrd2:	or	al,1		;ZF=0
rtrd3:	stc			;ZF set already, set CF
	ret
rtrd4:	jmp	rderr
;
rtdo:	; RT-11 dir output init
	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	rtdo1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir seg buffer (nothing is possible if this fails)
	mov	cx,rtslen	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:rtsblk[si],ax ;nothing in here yet
	mov	ds:rtsblk+2[si],ax
	mov	ds:rtsdrt[si],al ;not dirty
	mov	ds:rtsnxt[si],ax ;no next
rtdo1:	ret
;
rtcr:	; create RT file
	; parse filename (at DS:SI), convert to radix-50
	call	lenz		;get length
	call	grad		;get filename
	jc	rtcr2
	cmp	al,'.'		;ends with dot, right?
	jne	rtcr2		;no
	mov	ds:rtfile,bx	;save filename
	mov	ds:rtfile+2,dx
	inc	si		;eat the dot
	dec	cx
	call	grad		;get extension
	test	al,al		;ends at end of string, right?
	jnz	rtcr2
	mov	ds:rtfile+4,bx	;save extension
	; search for smallest empty area that's big enough for it
	mov	ax,ds:fbcnt	;get byte count
	mov	dx,ds:fbcnt+2
	mov	bx,1000		;bytes/block
	cmp	dx,bx		;make sure it'll fit in <64 K blks
	jb	rtcr3
rtcr1:	jmp	odfull		;full
rtcr2:	jmp	badnam		;bad filename
rtcr3:	add	ax,777		;round up to next block
	adc	dx,0
	div	bx		;find # blocks needed
	; search directory for first < UNUSED > area .LE. AX blocks long
	mov	dx,ax		;save length
	mov	al,1		;starting seg #
rtcr4:	mov	ds:outseg,al	;save
	push	dx		;save min size
	cbw			;AH=0 (really read it)
	call	rtseg		;get the seg
	mov	bx,si		;save base of buf
	add	si,8d		;skip to starting blk #
	lodsw			;get it
	xor	dx,dx		;high order=0
	mov	ds:osize,dx	;say prev file was null (preserve OBLK)
	add	ax,ss:actdir[bp] ;add base of LD: (if any)
	adc	dx,ss:actdir+2[bp]
	mov	ds:oblk,ax	;save
	mov	ds:oblk+2,dx
	pop	dx		;restore
rtcr5:	; examine next file entry within this segment (DS:SI=ptr)
	mov	ax,ds:osize	;get size of previous
	add	ds:oblk,ax	;update starting blk ptr
	adc	ds:oblk+2,0
	lodsw			;get status word
	test	ah,4000/100h	;end of segment?
	jnz	rtcr7		;yes, go get next seg
	mov	cx,[si+6]	;get size
	mov	ds:osize,cx	;save
	test	ah,1400/100h	;empty area or tentative file?
	jz	rtcr6
	cmp	cx,dx		;yes, is it big enough?
	jae	rtcr8		;yes
rtcr6:	add	si,6*2		;skip FILNAM.EXT, length, job,,chan, date
	add	si,ss:extra[bp]	;skip extra bytes if any
	jmp	short rtcr5	;get next entry
rtcr7:	; get next seg
	mov	ax,[bx+2]	;get link to next seg
	test	ax,ax		;any?
	jnz	rtcr4		;get it if so
	jmp	short rtcr1
rtcr8:	; found one that's big enough, save ptr to entry (OBLK, OSIZE set up)
	dec	si		;-2 (undo LODSW)
	dec	si
	sub	si,bx		;subtract base (in case bufs shuffled)
	mov	ds:outent,si	;save
	mov	ds:owrtn,0	;init size
	ret
;
rtwr:	; write to RT-11 file
	mov	al,ch		;get # full blocks
	shr	al,1
	jz	rtwr1		;nothing, skip
	cbw			;AH=0
	push	cx		;save
	mov	cx,ax		;copy
	sub	ds:osize,ax	;eat this out of < UNUSED > area
	jc	rtwr3		;whoops, this should never happen
	add	ds:owrtn,ax	;update # blks written
	mov	ax,ds:oblk	;get starting block #
	mov	dx,ds:oblk+2
	add	ds:oblk,cx	;update
	adc	ds:oblk+2,0
	call	ss:wrblk[bp]	;write data
	pop	cx		;[restore]
	jc	rtwr2		;failed
rtwr1:	and	cx,not 777	;truncate byte count (wrote whole blks only)
	ret
rtwr2:	jmp	wrerr		;error
rtwr3:	jmp	oswbug		;error msg
;+
;
; Write last buffer of data to output file.
;
; es:si	string
; cx	length of data
;
;-
rtwl:	; write last partial block to RT-11 file
	jcxz	rtwl1		;none, skip
	mov	bx,1000		;RT-11 block size
	sub	bx,cx		;find # NULs to add (in [1,777])
	xchg	bx,cx		;swap
	lea	di,[bx+si]	;point past data
	xor	al,al		;load 0
	rep	stosb		;pad it out
	mov	cx,1000		;write a whole block
	jmp	short rtwr	;write data, return
rtwl1:	ret
;+
;
; Close RT-11 output file.
;
; If the < UNUSED > area we wrote the file on turned out to be exactly the
; right length then all we have to do is write in the file info and mark it as
; a permanent file.  If it's not an exact fit we also have to create a new
; < UNUSED > area after this one for the leftover block(s).
;
; We'll take the cheap way out, instead of splitting segments and shuffling
; things around like RT-11 does (which doesn't always work anyway, but it's
; constrained by trying to make do with just one segment buffer), we'll read
; the entire dir into a contiguous buffer, fix it, and write it back out.  If
; the directory fits entirely into the RTSEG buf chain this will be about as
; fast as any other way, and it will always work unless the directory is
; completely full.
;
;-
rtcl:	; close RT-11 output file
	call	rtrdir		;read directory
	; delete any other file of the same name
	mov	dx,ds:dirent	;length of entry
	mov	ax,ds:dirsiz	;size of dir
	mov	ds,ds:bigbuf	;pt at buffer
	mov	bx,5*2		;starting posn
rtcl1:	cmp	bx,ax		;done?
	je	rtcl4
	test	byte ptr [bx+1],2000/100h ;permanent file?
	jz	rtcl3		;no, don't bother
	lea	si,[bx+2]	;pt at filename
	mov	di,offset rtfile ;pt at output file
	mov	cx,3		;wc
	repe	cmpsw		;match?
	jne	rtcl3		;no
	; found a file of the same name, delete it
	test	byte ptr [bx+1],100000/100h ;protected?
	jz	rtcl2
	cmp	byte ptr cs:ovride,0 ;yes, should we override it?
	jnz	rtcl2		;yes
	push	es		;restore DS
	pop	ds
	jmp	prtfil		;can't overwrite protected file
rtcl2:	mov	word ptr [bx],1000 ;mark as < UNUSED >
rtcl3:	add	bx,dx		;bump to next entry
	jmp	short rtcl1	;loop
rtcl4:	push	es		;restore DS
	pop	ds
	; create new entry
	mov	es,ds:bigbuf	;pt at buffer
	mov	bx,ds:outptr	;get ptr to output file entry
	mov	ax,es:[bx+10]	;get size of area
	sub	ax,ds:owrtn	;-size of file
	jz	rtcl5		;exact fit, easy
	; slide rest of dir down a slot to make space for new < UNUSED > area
	mov	si,ds:dirsiz	;point past end of dir
	mov	cx,si		;find # bytes to move
	sub	cx,bx
	shr	cx,1		;/2=WC
	dec	si		;pt at last word
	dec	si
	mov	di,si		;copy
	mov	dx,ds:dirent	;length of file entry
	add	di,dx		;pt at dest
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	std			;move backwards
	rep	movsw		;copy
	cld			;restore
	mov	[di+2+10],ax	;save smaller size in new area
	pop	ds		;restore
	add	ds:dirsiz,dx	;update dir size
	; update INPTR if it's after this point
	cmp	bp,ds:indev	;is it the right dev?
	jne	rtcl5		;no
	mov	ax,ss:actdir[bp] ;yes, get LD: base
	mov	dx,ss:actdir+2[bp]
	cmp	ax,ds:savdir	;is it the input one?
	jne	rtcl5
	cmp	dx,ds:svdirh
	jne	rtcl5
	mov	al,ds:dirseg	;yes, get curr seg
	cmp	al,ds:inseg	;is it the curr input one too?
	jne	rtcl5
	cmp	ds:inptr,bx	;is INPTR below this spot?
	jb	rtcl5		;no
	mov	di,ds:dirent	;yes, get length
	add	ds:inptr,di	;update
rtcl5:	mov	di,bx		;point at file entry
	mov	ax,2000		;permanent file
	stosw
	mov	si,offset rtfile ;output filename
	movsw			;copy it
	movsw
	movsw
	mov	ax,ds:owrtn	;size
	stosw
	xor	ax,ax		;nuke open file info
	stosw
	cmp	ds:fdatf,al	;do we know date?
	jz	rtcl6		;no, leave it as 0 (=unknown)
	; convert date to RT-11 disk format
	mov	dx,ds:fyear	;get year
	sub	dx,1972d	;subtract base
	jnc	$+4
	 xor	dx,dx		;can't go earlier than 1972.
	cmp	dx,127d		;dates run out at 2099. (postdated file?)
	jbe	$+5
	 mov	dx,127d		;stop if after (?!)
	mov	dh,dl		;copy high 2 bits into b15:14
	sal	dh,1
	and	dx,140037	;isolate + separate
	mov	ch,ds:fmonth	;get month
	sal	ch,1		;shift into place
	sal	ch,1
	or	dh,ch
	mov	ah,ds:fday	;get day (AX=0 from above)
	mov	cl,3		;bit count
	shr	ax,cl
	or	ax,dx		;compose final date
rtcl6:	stosw			;save date (or 0 if unknown)
	push	ds		;restore ES
	pop	es
	; set extra bytes to ^R/RT11  / if exactly 4 bytes (RTS for FIT.BAC)
	cmp	ss:extra[bp],4	;exactly 4 extra bytes?
	jne	rtcl7		;no
	mov	ax,71677	;.rad50/RT1/
	stosw
	mov	ax,140700	;.rad50/1  /
	stosw
rtcl7:	jmp	rtwdir		;write dir back, return
;+
;
; Delete most recently DIRGETted file.  Called through SS:DELFIL[BP].
;
;-
rtdl:	mov	al,ds:inseg	;get input seg #
	cbw			;AH=0 (really get it)
	call	rtseg		;(shouldn't be necessary, it should be in)
	mov	ds:rtsdrt[si],1	;mark seg buf as dirty
	add	si,ds:inent	;add offset
	sub	si,16		;back up to prev entry
	sub	si,ss:extra[bp]
	mov	al,[si+1]	;get flags
	test	al,al		;protected?
	jns	rtdl1
	cmp	byte ptr ds:ovride,0 ;should we override protection?
	jnz	rtdl1		;yes
	cram	'?Protected file',error
rtdl1:	test	al,2000/100h	;permanent?
	jz	rtdl2		;no, so what is it?
	mov	word ptr [si],1000 ;now it's < UNUSED >
	ret
rtdl2:	jmp	baddir		;corrupt directory
;+
;
; Write bootstrap.
;
;-
rtboot:	call	rtds		;set defaults
	mov	ax,1		;no \ needed after last pathname element
	call	rtgetd		;get dir
	call	confrm		;make sure at EOL
	mov	di,offset lbuf	;pt at buffer
	cram	'Writing bootstrap on '
	call	pdev		;print dev name
	xor	cx,cx		;no filename follows
	call	ss:dirnam[bp]	;print dir name
	call	flush		;flush line
	call	flush		;blank line afterwards
rtbt1:	cram	'Monitor file [.SYS]: ',wrng
	call	rtbtfl		;look up file
	jc	rtbt1		;reprompt
	mov	ax,ds:stblk	;get starting block of monitor file
	mov	dx,ds:stblk+2
	add	ax,1		;skip to blk 1
	adc	dx,0
	mov	cx,4		;read blks 1-4 (not 0-4, SSM is wrong)
	mov	es,ds:bigbuf	;pt at buffer
	mov	di,1000		;starting addr
	call	ss:rdblk[bp]	;read it
	jc	rtbt3
	mov	ax,ds:rtfile	;get monitor filename
	mov	es:4724,ax	;patch it in
	mov	ax,ds:rtfile+2
	mov	es:4726,ax
	push	ds		;restore ES
	pop	es
rtbt2:	cram	'Device handler file [.SYS]: ',wrng
	call	rtbtfl		;look up file
	jc	rtbt2		;reprompt
	mov	ax,ds:stblk	;get starting block of handler file
	mov	dx,ds:stblk+2
	mov	cx,1		;read first block
	mov	di,offset buf	;pt at buf
	call	ss:rdblk[bp]	;read it
	jnc	rtbt5
rtbt3:	jmp	rderr
rtbt4:	cram	'?Corrupt device handler file',error
rtbt5:	mov	ax,[si+62]	;get addr of start of driver
	mov	cx,[si+64]	;get length
	cmp	ch,1000/100h	;>1 blk?
	ja	rtbt4
	push	ax		;save
	push	cx
	push	[si+66]		;save offset of B.READ into primary driver
	; find blk # of last byte of primary driver
	add	cx,ax		;bump to just after end of primary driver
	dec	cx		;pt at last byte
	mov	cl,ch		;get blk # containing last byte
	shr	cl,1
	xor	ch,ch		;CH=0
	cmp	cx,ds:fsize	;off EOF?
	jae	rtbt4		;yes, invalid
	; find blk # of first byte of primary driver
	mov	al,ah		;get starting blk #
	shr	al,1		;(byte addr /1000)
	cbw			;AH=0
	cwd			;DX=0
	; find # blks to read and read them
	sub	cx,ax		;1 if spans blks, 0 if not
	inc	cx		;so read 1 or 2 blks
	add	ax,ds:stblk	;add base of file
	adc	dx,ds:stblk+2
	mov	di,offset buf	;pt at buffer
	call	ss:rdblk[bp]	;read in the primary driver
	jc	rtbt3
	pop	dx		;restore B.READ offset
	pop	cx		;restore length, ptr
	pop	ax
	and	ah,777/100h	;trim off blk #
	add	ax,offset buf	;add base of buf
	mov	si,ax		;pt with ds:si
	mov	es,ds:bigbuf	;pt at big buf
	; the next few patches aren't in SSM but someone did them, must be DUP
	mov	es:4730,dx	;set B$READ = B.READ offset
	mov	ax,ds:rtfile	;set B$DEVN = dev name
	mov	es:4716,ax	;(hope this is right!)
	xor	di,di		;dest offset
	rep	movsb		;copy the primary driver
	xor	al,al		;AL=0
	mov	ch,1000/100h	;pt at end of blk
	sub	cx,di		;find # free bytes
	rep	stosb		;clear out rest of primary boot
	; write out primary driver
	xor	si,si		;pt at it
	mov	ax,ss:actdir[bp] ;blk 0 of LD:
	mov	dx,ss:actdir+2[bp]
	mov	cx,1		;count
	call	ss:wrblk[bp]	;write it out
	jc	rtbt6
	; write out secondary boot
	mov	si,1000		;pt at it
	mov	ax,ss:actdir[bp] ;blks 2-5 of LD:
	mov	dx,ss:actdir+2[bp]
	add	ax,2
	adc	dx,0
	mov	cx,4		;count
	call	ss:wrblk[bp]	;write it out
	jc	rtbt6
	ret
rtbt6:	jmp	wrerr
;
rtbtfl:	; get filename, look it up
	call	gtlin		;read response
	mov	ax,75273	;^RSYS (default ext)
	call	gradfn		;get radix-50 filename
	jc	rtbf4		;none, go print dir
	call	confrm		;make sure EOL
	call	rtdi		;init for dir input
rtbf1:	call	rtdg		;get next dir entry
	jc	rtbf2		;end
	jz	rtbf1		;empty, skip it
	; retrieve pointer to dir entry
	mov	si,ds:inent	;get input entry
	add	si,ss:dirbuf[bp] ;add base of buf
	sub	si,16		;back up an entry
	sub	si,ss:extra[bp]
	push	si		;save
	inc	si		;skip to filename
	inc	si
	mov	di,offset rtfile ;pt at name to look up
	mov	cx,3		;count
	repe	cmpsw		;match?
	pop	si		;[restore]
	jne	rtbf1		;loop if not
	ret			;CF=0
rtbf2:	mov	di,offset lbuf	;pt at buffer
	cram	'%Not found'
	call	flush		;print msg
	stc			;error return
rtbf3:	ret
rtbf4:	; they pressed return, give a list of .SYS files
	call	rtdi		;init for dir input
rtbf5:	call	rtdg		;get next dir entry
	jc	rtbf3		;end
	jz	rtbf5		;loop if empty
	cmp	word ptr [di-2],"SY" ;see if .SYS
	jne	rtbf5
	cmp	byte ptr [di-3],'S'
	jne	rtbf5
	call	flush		;print name if so
	jmp	short rtbf5	;loop
;+
;
; Wipe out unused areas in RT-11 or OS/8 disk or LD:.
;
; bp	dev rec
; si,cx	point to cmd line just after dev (may be dir name (LD:))
;
;-
rtwp:	call	ss:defset[bp]	;set defaults
	call	ss:gdir[bp]	;get dir name
	call	confrm		;make sure confirmed
	call	ss:dsrini[bp]	;init DSR (switch drives etc.)
	call	ss:dirinp[bp]	;init dir input
	; load up a bufferful of zeros
	mov	es,ds:bigbuf	;point at buffer
	xor	di,di		;point at beginning
	mov	cx,ds:bigsiz	;get size
	cmp	ds:bigsiz+2,di	;is that it?
	jz	rtwp1
	 mov	cx,-1		;no, >64 KB so stop at end
rtwp1:	push	cx		;save
	xor	ax,ax		;load 0
	shr	cx,1		;/2
	rep	stosw		;clear out begn of buf (ignore odd byte)
	pop	ax		;get length back
	push	ds		;restore ES
	pop	es
	mov	cl,9d		;bit count
	shr	ax,cl		;find # blocks in buf
	jz	rtwp8		;not even one, lose
	mov	ds:bigctr,ax	;save length
rtwp2:	; process next file
	call	ss:dirget[bp]	;get next entry
	jc	rtwp6		;end of list
	jnz	rtwp2		;not < UNUSED >, skip it
	; it's < UNUSED >, zero it
	mov	ax,ds:stblk	;get starting blk #
	mov	dx,ds:stblk+2
	mov	ds:oblk,ax	;save
	mov	ds:oblk+2,dx
	mov	ax,ds:fsize	;get size
	mov	ds:osize,ax	;save
rtwp3:	; write next bufferload of zeros
	mov	es,ds:bigbuf	;get buf ptr
	xor	si,si
	mov	ax,ds:oblk	;get block #
	mov	dx,ds:oblk+2
	mov	cx,ds:bigctr	;get # blks in buf
	cmp	cx,ds:osize	;> than what's left?
	jb	rtwp4
	mov	cx,ds:osize
	jcxz	rtwp5		;nothing to do, skip
rtwp4:	push	cx		;save
	call	ss:wrblk[bp]	;write
	pop	cx		;[restore]
	jc	rtwp7		;error
rtwp5:	add	ds:oblk,cx	;update blk #
	adc	ds:oblk+2,0
	sub	ds:osize,cx	;and # blks to go
	jnz	rtwp3		;loop if nonzero
	jmp	short rtwp2	;handle next file
rtwp6:	ret
rtwp7:	jmp	wrerr
rtwp8:	cram	'?Not enough memory for buffer',error
;+
;
; Read entire RT-11 directory into BIGBUF.
;
; ss:bp	log dev rec
; cx	returns length in bytes of dir read (missing all end-of-seg markers)
;	(also recorded in DIRSIZ)
; DS:DIRENT  records length of entry
;
;-
rtrdir:	xor	ax,ax		;load 0
	mov	ds:inptr,ax	;say files not yet found
	mov	ds:outptr,ax
	inc	ax		;get segment #1
	mov	ds:dirseg,al	;save
	call	rtseg		;read it (AH=0)
	cmp	ds:bigsiz+2,0	;make sure buf can hold the whole dir
	jnz	rtrdr2		;definitely
	mov	ax,[si+4]	;get # segs in use
	test	ax,ax		;valid?
	jz	rtrdr1
	cmp	ax,31d
	ja	rtrdr1
	mov	cl,10d		;1024 bytes in a segment
	sal	ax,cl		;find # bytes needed
	cmp	ds:bigsiz,ax	;will it fit?
	jae	rtrdr2		;yes
rtrdr1:	jmp	baddir		;corrupt directory
rtrdr2:	mov	es,ds:bigbuf	;pt at buffer
	xor	di,di		;pt at begn
	mov	cx,5		;copy first 5 words
	rep	movsw		;go
	sub	si,5*2		;correct ptr
	mov	ax,[si+6]	;get # extra bytes
	add	ax,16		;add size of rest of dir entry
	mov	ds:dirent,ax	;save
rtrdr3:	; set up ptr to detect falling off end
	mov	bx,si		;copy
	neg	bx		;make negative for LEA later
	add	si,5*2		;pt past header
	lea	dx,[di+2000-12-2] ;-12 for header, -2 for end-of-segment word
	sub	dx,ds:dirent	;subtract # info bytes in last dir entry
rtrdr4:	; check next entry (SI=ptr, BX=-<base of seg buf>, DX=max DI value)
	test	byte ptr [si+1],4000/100h ;end of segment?
	jnz	rtrdr7
	cmp	di,dx		;space for a file entry?
	jae	rtrdr1		;no, invalid dir
	; see if this is the current input entry
	cmp	bp,ds:indev	;is it the right dev?
	jne	rtrdr5		;no
	mov	ax,ss:actdir[bp] ;yes, get LD: base
	cmp	ax,ds:savdir	;is it the input one?
	jne	rtrdr5
	mov	ax,ss:actdir+2[bp]
	cmp	ax,ds:svdirh
	jne	rtrdr5
	mov	al,ds:dirseg	;yes, get curr seg
	cmp	al,ds:inseg	;is it the curr input one too?
	jne	rtrdr5
	lea	ax,[bx+si]	;get relative ptr
	cmp	ax,ds:inent	;is this the input entry?
	jne	rtrdr5
	mov	ds:inptr,di	;save
	mov	ds:iptr1,di	;init updated version
rtrdr5:	; see if this is the current output entry
	cmp	bp,ds:outdev	;is it the right dev?
	jne	rtrdr6		;no
	mov	ax,ss:actdir[bp] ;yes, get LD: base
	cmp	ax,ds:savdir+2	;is it the output one?
	jne	rtrdr6
	mov	ax,ss:actdir+2[bp]
	cmp	ax,ds:svdirh+2
	jne	rtrdr6
	mov	al,ds:dirseg	;yes, get curr seg
	cmp	al,ds:outseg	;is it the curr output one too?
	jne	rtrdr6
	lea	ax,[bx+si]	;get relative ptr
	cmp	ax,ds:outent	;is this the output entry?
	jne	rtrdr6
	mov	ds:outptr,di	;save
	mov	ds:optr1,di	;init updated version
rtrdr6:	; copy dir entry
	mov	cx,ds:dirent	;get size of entry
	shr	cx,1		;/2=WC
	rep	movsw		;copy
	jmp	short rtrdr4	;loop
rtrdr7:	; end of segment
	neg	bx		;flip BX back
	mov	ax,[bx+2]	;get next dir seg
	test	ax,ax		;end?
	jz	rtrdr8
	mov	ds:dirseg,al	;update curr seg #
	push	di		;save
	push	es
	push	ds		;copy DS to ES
	pop	es
	cbw			;AH=0 (really read)
	call	rtseg		;get next seg
	pop	es		;restore
	pop	di
	jmp	rtrdr3		;loop
rtrdr8:	mov	ds:dirsiz,di	;save
	push	ds		;restore ES
	pop	es
	mov	cx,di		;get length
	ret
;+
;
; Write entire RT-11 directory from BIGBUF (read by RTRDIR).
;
; ss:bp	log dev rec
; DS:DIRSIZ  gives size of dir in bytes (incl 5-word header, no end-seg words)
;
;-
rtwdir:	mov	es,ds:bigbuf	;pt at buf
	; find # segments we'll need
	mov	ax,2000-(5*2)-2	;seg size, -<header>, -<end-of-segment word>
	cwd			;zero-extend
	div	word ptr ds:dirent ;find # whole dir entries per dir seg
	mov	bx,ax		;save
	mov	ax,ds:dirsiz	;get # bytes of dir info
	sub	ax,5*2		;subtract header
	xor	dx,dx		;zero-extend
	div	word ptr ds:dirent ;find # dir entries to be written (DX=0)
	add	ax,bx		;round up to next seg
	dec	ax
	div	bx		;AX=total number of dir segments needed
	cmp	ax,es:0		;.LE. total # available?
	jbe	rtwdr1
	jmp	dirful		;dir is full
rtwdr1:	mov	es:4,ax		;update highest seg in use
	mov	byte ptr ds:dirseg,1 ;number of first dir seg
	mov	si,5*2		;pt past header
	push	ds		;restore ES
	pop	es
rtwdr2:	; copy next segment
	push	si		;save BIGBUF ptr
	mov	al,ds:dirseg	;seg #
	mov	ah,1		;alloc buf but don't read from disk
	call	rtseg		;get buf
	mov	ds:rtsdrt[si],1	;mark dirty
	mov	di,si		;copy ptr
	mov	bx,di		;copy
	neg	bx		;make negative for LEA later
	; set link to next segment
	mov	al,ds:dirseg	;get curr seg #
	cbw			;AH=0
	mov	ds,ds:bigbuf	;pt into buf
	cmp	ax,ds:4		;is this the last seg?
	jne	rtwdr3
	 mov	ax,-1		;yes, no next seg
rtwdr3:	inc	ax		;+1
	mov	ds:2,ax		;set ptr to next dir seg
	; calc last addr to start copying an entry
	lea	dx,[di+2000-2]	;get last possible addr for end-of-segment word
	sub	dx,cs:dirent	;subtract # info bytes in last dir entry
	; copy header
	xor	si,si		;get header from first 5 words
	mov	cx,5		;word count
	rep	movsw		;copy
	pop	si		;restore BIGBUF ptr
rtwdr4:	; copy next file entry
	cmp	si,cs:dirsiz	;done?
	je	rtwdr7		;yes
	cmp	di,dx		;any space left in segment?
	jae	rtwdr8		;no, continue with next segment
	mov	ax,[si+10]	;get length
	add	ds:10,ax	;update starting posn of file area
	; see if this is the current input entry
	cmp	bp,cs:indev	;is it the right dev?
	jne	rtwdr5		;no
	mov	ax,ss:actdir[bp] ;get active LD:
	cmp	ax,cs:savdir	;is it the input one?
	jne	rtwdr5
	mov	ax,ss:actdir+2[bp]
	cmp	ax,cs:svdirh
	jne	rtwdr5
	cmp	si,cs:inptr	;yes, does ptr match?
	jne	rtwdr5
	mov	al,cs:dirseg	;save current dir seg
	mov	cs:inseg,al
	lea	ax,[bx+di]	;save relative ptr
	mov	cs:inent,ax
rtwdr5:	; see if this is the current output entry
	cmp	bp,cs:outdev	;is it the right dev?
	jne	rtwdr6		;no
	mov	ax,ss:actdir[bp] ;get active LD:
	cmp	ax,cs:savdir+2	;is it the input one?
	jne	rtwdr6
	mov	ax,ss:actdir+2[bp]
	cmp	ax,cs:svdirh+2
	jne	rtwdr6
	cmp	si,cs:outptr	;yes, does ptr match?
	jne	rtwdr6
	mov	al,cs:dirseg	;save current dir seg
	mov	cs:outseg,al
	lea	ax,[bx+di]	;save relative ptr
	mov	cs:outent,ax
rtwdr6:	; copy dir entry
	mov	cx,cs:dirent	;get size of dir entry
	shr	cx,1		;/2=WC
	rep	movsw		;copy
	jmp	rtwdr4		;loop
rtwdr7:	jmp	short rtwdr9
rtwdr8:	; finish off this segment
	mov	ax,4000		;end of segment marker
	stosw			;save
	push	es		;restore DS
	pop	ds
	inc	ds:dirseg	;bump seg #
	jmp	rtwdr2		;loop
rtwdr9:	; finish off whole directory
	mov	ax,4000		;end of segment marker
	stosw
	push	es		;restore DS
	pop	ds
	ret
;+
;
; Search active RT-11 directory for a directory file.
;
; ss:bp	log dev rec
; RTDNAM is set up with either the 3-word RADIX-50 dir filename,
;	or if the first word is 0, the 2nd/3rd word is the abs blk # to find
;
; On return, CF=0 if found and:
; si	points at file entry in BUF
; ax	contains relative blk # of RTDNAM+2 if searching by block
;
; Otherwise CF=1 if not found.
;
;-
rtdir:	mov	ax,1		;first seg
rtdir1:	call	rtdseg		;read it
	mov	ax,ss:actdir[bp] ;get base of active dir
	mov	dx,ss:actdir+2[bp]
	add	ax,[si+8d]	;add starting block for 1st file
	adc	dx,0
	mov	ds:stblk,ax
	mov	ds:stblk+2,dx
	mov	ds:fsize,0	;say preceding file null
	add	si,5*2		;start just after header
	mov	dx,offset buf+2000-16-2 ;end of dir, -16 for dir entry,
				;-2 for EOS word
	sub	dx,word ptr ds:buf+6 ;subtract # info bytes
rtdir2:	; SI=dir ptr, DX=last acceptable starting addr
	mov	ax,ds:fsize	;get size of previous
	add	ds:stblk,ax	;update starting blk ptr
	adc	ds:stblk+2,0
	test	byte ptr [si+1],4000/100h ;end of segment?
	jnz	rtdir5		;yes, go get next seg
	cmp	si,dx		;off end of dir?
	ja	rtdir6		;yes, corrupt
	test	byte ptr [si+1],2000/100h ;permanent file?
	jz	rtdir4		;no, skip it
	; see if it's a match
	mov	di,offset rtdnam ;pt at dir name
	cmp	word ptr [di],0	;are we looking for a name or a blk #?
	jz	rtdir3		;blk #
	push	si		;save
	inc	si		;pt at filename
	inc	si
	mov	cx,3		;3 words
	repe	cmpsw		;match?
	pop	si		;[restore]
	jne	rtdir4		;no
	ret			;got it
rtdir3:	mov	ax,[di+2]	;get desired blk #
	mov	cx,[di+4]
	sub	ax,ds:stblk	;subtract starting block of this file
	sbb	cx,ds:stblk+2
	jnz	rtdir4		;(too big to compare)
	cmp	ax,[si+8d]	;< length?
	jae	rtdir4
	clc			;yes, we got it (AX=relative blk #)
	ret
rtdir4:	mov	ax,[si+8d]	;get size
	mov	ds:fsize,ax	;save
	add	si,16		;skip file entry
	add	si,word ptr ds:buf+6 ;skip extra bytes
	jmp	short rtdir2	;loop
rtdir5:	mov	ax,word ptr ds:buf+2 ;get link to next word
	test	ax,ax		;end?
	jz	rtdir7		;yes, EOF
	cmp	ax,31d		;.LE.31 right?
	ja	rtdir6
	jmp	rtdir1
rtdir6:	jmp	baddir		;corrupt directory
rtdir7:	stc			;not found
	ret
;+
;
; Read dir seg for tree handling routines (into BUF, w/no caching since we're
; making only one pass and the cache wouldn't work).
;
; ax	seg to read
; si	returns ptr to BUF
;
;-
rtdseg:	add	ax,ax		;*2 (blks/seg)
	add	ax,6-2		;dir starts at blk 6
	cwd			;DX=0
	add	ax,ss:actdir[bp] ;add base of current LD: file (or 0 if none)
	adc	dx,ss:actdir+2[bp]
	mov	cx,2		;read 2 blks
	mov	di,offset buf	;point at buffer
	call	ss:rdblk[bp]	;go
	mov	si,offset buf	;point at buffer
	ret
;+
;
; Get RT-11 dir segment.
;
; al	segment #
; ah	NZ to return buf but don't read if not cached (going to write it)
; ss:bp	log dev rec
;
; Returns ptr to 1024-byte segment in SI.
;
;-
rtseg:	test	al,al		;make sure valid
	jz	rtsg3
	cmp	al,31d		;must be in [1,31.]
	ja	rtsg3
	mov	cl,ah		;save flag
	cbw			;AH=0
	cwd			;DX=0
	add	ax,ax		;*2 (blks/seg)
	add	ax,6-(2*1)	;dir seg 1 starts at blk 6
	add	ax,ss:actdir[bp] ;add base of current LD: file (or 0 if none)
	adc	dx,ss:actdir+2[bp]
	mov	si,ss:dirbuf[bp] ;head of buffer chain
	mov	bx,ds:rtsblk[si] ;is this our first buf request?
	or	bx,ds:rtsblk+2[si] ;(never filled in blk #)
	jz	rtsg10		;yes, use our pre-alloced buf
	xor	bx,bx		;no prev
rtsg1:	; see if we already have it
	; SI=buf to check, BX=prev or 0, DX:AX=abs starting block # of dir seg
	cmp	ax,ds:rtsblk[si] ;is this it?
	jne	rtsg2
	cmp	dx,ds:rtsblk+2[si]
	je	rtsg4		;yes, relink and return
rtsg2:	mov	di,ds:rtsnxt[si] ;get ptr to next
	test	di,di		;if any
	jz	rtsg6		;none, need to read it
	mov	bx,si		;save for linking
	mov	si,di		;follow link
	jmp	short rtsg1	;keep looking
rtsg3:	jmp	baddir
rtsg4:	; found it
	test	bx,bx		;is it first in line?
	jz	rtsg5		;yes, no need to relink
	mov	ax,si		;pt at it
	xchg	ax,ss:dirbuf[bp] ;get head of chain, set new head
	xchg	ax,ds:rtsnxt[si] ;link rest of chain to head
	mov	ds:rtsnxt[bx],ax ;unlink the one we just moved
rtsg5:	ret
rtsg6:	; we don't have it, try to allocate a new buffer
	; SI=LRU buf, BX=the one before that or 0 if there's only 1
	; DX:AX=starting abs blk # of dir seg
	push	si		;save
	push	cx
	mov	cx,rtslen	;# bytes
	call	askmem		;ask for a buffer
	pop	cx		;[catch flag]
	pop	di		;[catch ptr to last buf in chain]
	jnc	rtsg9		;got it, read the buf in
	mov	si,di		;restore
	; failed, write out the last one
	test	bx,bx		;is there a penultimate buffer?
	jz	rtsg7		;no
	mov	ds:rtsnxt[bx],0	;yes, unlink final buffer from it
	jmp	short rtsg8
rtsg7:	mov	ss:dirbuf[bp],bx ;this is the only one, unlink it
rtsg8:	push	ax		;save blk
	push	dx
	push	cx		;and flag
	call	rtfseg		;flush it
	pop	cx		;restore
	pop	dx
	pop	ax
rtsg9:	; link buf in as first
	mov	bx,si		;copy ptr
	xchg	bx,ss:dirbuf[bp] ;get current chain, set new head
	mov	ds:rtsnxt[si],bx ;link it on
rtsg10:	; buf is linked in at head of chain -- SI=buf, DX:AX= abs blk #, CL=flg
	mov	ds:rtsblk[si],ax ;set starting blk #
	mov	ds:rtsblk+2[si],dx
	mov	ds:rtsdrt[si],0	;buf is clean
	test	cl,cl		;should we read it?
	jnz	rtsg11		;no, all we wanted was the buf
	mov	cx,2		;read 2 blks
	mov	di,si		;point at buffer
	push	si		;save
	call	ss:rdblk[bp]	;go
	pop	si		;[restore]
	jc	rtsg12		;err
	mov	ax,[si+6]	;get # extra bytes
	mov	ss:extra[bp],ax	;save
rtsg11:	ret
rtsg12:	jmp	dirio
;+
;
; Flush RT-11 dir segment buffer, if dirty.
;
; ss:bp	dev rec
; ds:si	ptr to buffer (preserved on exit)
;
;-
rtfseg:	test	byte ptr ds:rtsdrt[si],-1 ;is buf dirty?
	jz	rtfsg1		;no
	mov	ax,ds:rtsblk[si] ;get blk #
	mov	dx,ds:rtsblk+2[si]
	mov	cx,2		;2 blks/seg
	push	si		;save
	call	ss:wrblk[bp]	;go
	pop	si		;[restore]
	jc	rtfsg2		;err
	mov	byte ptr ds:rtsdrt[si],0 ;no longer dirty
rtfsg1:	ret
rtfsg2:	jmp	dirio
;
	subttl	RSX (Files-11) file structure
;
	comment	_

The whole ODS-1 world revolves around the file [0,0]INDEXF.SYS;1 (ID 1,1,0).
This file is laid out as follows:

VBN 1	= the boot block, LBN 0.
VBN 2	= the home block, LBN 1 or the first readable multiple of 256. if bad.
VBN 3:n	= bitmap of file headers in use.
	  bitmap is fixed size and physically contiguous (located by first 3
	  words of home block), INDEXF.SYS may or may not have pre-allocated
	  blocks for all headers covered by the bitmap.
n+1 up	= actual file headers, numbered starting with 1.
	  the first 16 entries follow the bitmap as one physically contiguous
	  area (so it can be located from the home block).
	  the first five headers are magic and hard-coded:
	  1,1,0 = [0,0]INDEXF.SYS;1  (i.e. this file)
	  2,2,0 = [0,0]BITMAP.SYS;1  (storage bitmap, 1 bit per cluster)
	  3,3,0 = [0,0]BADBLK.SYS;1  (bad block file)
			VBN 1 = BAD.TSK's list from last good block of disk
			other VBNs = the actual bad blocks
	  4,4,0 = [0,0]000000.DIR;1  (MFD)
	  5,5,0 = [0,0]CORIMG.SYS;1  (core image, for OS use)

Each file on the system gets a file header, which is one block out of
INDEXF.SYS.  This contains the original filename (used when created), file
dates, protection, attributes, etc., as well as the actual retrieval pointers
for the file.  If necessary, additional header blocks can be linked to the
first one to supply space for more retrieval pointers.  These extension headers
will never be referenced themselves as files (they can be told apart from file
headers to enforce this).

When a file is deleted, the first word of the header is cleared, ONLY.  That
way the sequence number (H.FSEQ) is still valid so that it can be incremented
when the header is reused to guarantee a unique file ID in case someone tries
to open the old file by ID (and in case any dir entries are still around
pointing at it, ODS-1 is very sloppy about that and doesn't have back pointers
or link counts so there's no easy way to know if a file somehow has multiple
dir entries).  Clearing the first word also guarantees that the header's
checksum will be invalid (since normally the first word contains a couple of
structure lengths and is never zero).

All files in the system are entered in INDEXF.SYS, even though they may not
necessarily have a directory entry.  They may also have multiple directory
entries, but keeping track of that is not our problem, the file sequence
numbers are supposed to avoid any problems caused by deleting files but not
directory entries, so all we have to do is return an error if the user tries to
open a stale dir entry.

_
;+
;
; Print volume name.
;
;-
odvn:	cram	' is '		;we always succeed
	push	di		;save
	call	odhome		;fetch home block
	pop	di		;[restore]
	jc	odvn3
	add	si,16		;H.VNAM
	mov	cx,12d		;max # chars
odvn1:	lodsb			;get a char
	test	al,al		;end?
	jz	short odvn2
	stosb			;save if not
	loop	odvn1		;copy all chars
odvn2:	ret
odvn3:	jmp	rderr
;
odds:	; set defaults
	push	si		;save
	push	cx
	lea	si,ss:curdir[bp] ;point at curr dir
	lea	di,ss:actdir[bp] ;active dir
	mov	cx,odpmax*3	;word count
	rep	movsw
	pop	cx		;restore
	pop	si
	ret
;+
;
; Save ODS-1 CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
odsav:	push	si		;save
	push	cx
	lea	si,ss:actdir[bp] ;point at active dir
	mov	cx,odpmax*3	;size of SAVDIR entries
	mov	al,cl		;(copy)
	mul	bx		;find index
	add	ax,offset savods ;add base
	mov	di,ax		;point at it
	rep	movsw		;copy
	pop	cx		;restore
	pop	si
	ret
;+
;
; Restore ODS-1 CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
odrst:	push	si		;save
	push	cx
	mov	cx,odpmax*3	;size of SAVDIR entries
	mov	al,cl		;(copy)
	mul	bx		;find index
	add	ax,offset savods ;add base
	mov	si,ax		;point at it
	lea	di,ss:actdir[bp] ;where it goes
	rep	movsw		;copy
	pop	cx		;restore
	pop	si
	ret
;
oddi:	; ODS-1 dir input init
	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	oddi1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir blk buffer (nothing is possible if this fails)
	mov	cx,rdslen	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:rdsbkl[si],ax ;nothing in here yet
	mov	ds:rdsbkh[si],ax
	mov	ds:rdsdrt[si],al ;not dirty
	mov	ds:rdsnxt[si],ax ;no next
oddi1:	; find dir and fetch retrieval
	mov	ss:ecache[bp],1	;enable caching
	call	odfd		;find directory in SS:ACTDIR[BP]
	ret
;
oddn:	; display ODS-1 dir name
	mov	al,'['		;[
	stosb
	lea	si,ss:actdir[bp] ;point at path
	mov	ax,[si+4]	;get 3rd word
	or	ax,[si+6]	;one element, of .LE. 6 chars?
	jnz	oddn2		;no
	cmp	[si+2],ax	;must be >3 chars long
	; might be numeric, give it a shot
	push	di		;save
	mov	ax,[si]		;convert first word
	call	oddn4
	jc	oddn1		;contains non-digit
	mov	al,','		;,
	stosb
	mov	ax,[si+2]	;second word
	call	oddn4
	jc	oddn1		;contains non-digit
	pop	ax		;flush
	jmp	short oddn3
oddn1:	pop	di		;restore starting posn
oddn2:	; print out path verbatim, no xxxyyy => x,y translation
	lodsw			;do three words
	call	radnb
	lodsw
	call	radnb
	lodsw
	call	radnb
	lea	ax,ss:actdir+(odpmax*6)[bp] ;find ending value
	cmp	si,ax		;all done?
	je	oddn3
	test	word ptr [si],-1 ;end of path?
	je	oddn3		;yes
	mov	al,'.'		;.
	stosb
	jmp	short oddn2
oddn3:	mov	al,']'		;]
	stosb
	ret
oddn4:	; convert .RAD50 word in AX to digits
	; return CF=1 if contains non-digit
	push	di		;save
	call	radnb		;convert word
	mov	dx,di		;get ending DI
	pop	di		;restore
	mov	bx,di		;copy
oddn5:	; trim off leading zeros
	cmp	bx,dx		;done?  (all zeros?)
	je	oddn7		;yes, store the one zero
	mov	al,[bx]		;get char
	inc	bx
	cmp	al,'0'		;zero?
	je	oddn5		;loop if so
	jmp	short oddn7	;store
oddn6:	; repack digits
	cmp	bx,dx		;done?  (CF=0 if so)
	je	oddn8		;yes
	mov	al,[bx]		;get char
	inc	bx
oddn7:	stosb			;save
	sub	al,'0'		;convert to binary
	cmp	al,7		;octal digit?
	jbe	oddn6		;loop if so
	stc			;otherwise CF=1
oddn8:	ret
;
odcd:	; change ODS-1 dir
	call	odgd		;get dir
	call	confrm		;make sure confirmed
	lea	si,ss:actdir[bp] ;point at active dir
	lea	di,ss:curdir[bp] ;make it current dir
	mov	cx,odpmax*3	;word count
	rep	movsw
	ret
;+
;
; Get directory name.
;
;-
odgd:	; get dir name
	jcxz	odgd1		;nothing, skip
	mov	al,[si]		;get next char
	mov	ah,']'		;(closing bracket)
	cmp	al,'['		;open bracket?
	je	odgd2
	mov	ah,'>'		;(closing angle bracket)
	cmp	al,'<'		;open angle bracket?
	je	odgd2
odgd1:	ret			;leave CURDIR from ODDS
odgd2:	; start of dir name
	mov	ds:delim,ah	;save
	inc	si		;eat the char
	dec	cx
	jcxz	odgd4		;skip if end
	mov	al,[si]		;fetch next char
	cmp	al,'-'		;starts with parent dir?
	je	odgd4		;yes, will bite last SFD off of default dir
	cmp	al,'.'		;starts with child dir?
	je	odgd3		;yes
	mov	word ptr ss:actdir[bp],0 ;no, assume absolute path, clear it
	jmp	short odgd4
odgd3:	inc	si		;eat the dot
	dec	cx		;(this dir will be appended to default one)
odgd4:	; get next element of path name
	jcxz	odgd12		;gotta have something left
	cmp	byte ptr [si],'-' ;go to parent?
	jne	odgd10		;nope
odgd5:	; "-", go up a level
	xor	di,di		;start at beginning
odgd6:	test	word ptr ss:actdir[bp+di],-1 ;found end?
	jz	odgd7		;yes
	add	di,3*2		;advance to next slot
	cmp	di,odpmax*3*2	;full?
	jb	odgd6		;loop if not
odgd7:	test	di,di		;already at root?
	jz	odgd8		;yes, this is as far up as we can go
	mov	word ptr ss:actdir-(3*2)[bp+di],0 ;remove last entry
odgd8:	xor	al,al		;assume EOL
	inc	si		;eat the '-'
	dec	cx
	jz	odgd9		;EOL already
	mov	al,[si]		;get next char
	cmp	al,'-'		;multiple hyphens in a row?
	je	odgd5		;yes, don't require dots between them
odgd9:	jmp	odgd18		;no, should be ./]/>, go check it out
odgd10:	; try for [P,PN]
	push	si		;assume not a PPN
	push	cx
	call	goctb		;get project
	jz	odgd11		;didn't get any digits
	mov	bh,ah		;save
	cmp	al,','		;comma?
	jne	odgd11
	inc	si		;yes, eat it
	dec	cx
	call	goctb		;get programmer (preserve BX)
	jz	odgd11		;didn't get any digits
	mov	bl,ah		;save
	cmp	al,'.'		;path separator?
	je	odgd13
	cmp	al,ds:delim	;close bracket?
	je	odgd13		;yes
odgd11:	pop	cx		;restore (unget anything we got)
	pop	si
	; wasn't [P,PN], parse RAD50 name
	call	grad		;fetch filename
	jnc	odgd14		;got it
odgd12:	jmp	synerr		;nothing at all, fail
odgd13:	; BX=PPN
	pop	dx		;flush stack
	pop	dx
	push	ax		;save delimiter
	call	odgd21		;convert proj (in BH) to .RAD50 in DX
	push	dx		;save
	mov	bh,bl		;get prog
	call	odgd21		;convert
	pop	bx		;catch
	xor	di,di		;final 3 chars are zeros
	pop	ax		;restore
odgd14:	; store string in BX,DX,DI, AL=delimiter
	push	di		;save
	xor	di,di		;start at beginning
odgd15:	test	word ptr ss:actdir[bp+di],-1 ;found where to tack it on?
	jz	odgd16		;yes
	add	di,3*2		;advance to next slot
	cmp	di,odpmax*3*2	;full?
	jb	odgd15		;loop if not
	cram	'?Path too long',error
odgd16:	; see if parent directory is [0,0] -- trim it off if so
	cmp	di,3*2		;was there exactly one preceding element?
	jne	odgd17
	cmp	word ptr ss:actdir[bp],(36*50+36)*50+36 ;^R000?
	jne	odgd17
	cmp	word ptr ss:actdir+2[bp],(36*50+36)*50+36
	test	word ptr ss:actdir+4[bp],-1 ;(followed by zeros)
	jne	odgd17
	 xor	di,di		;^R/000000   /, replace it with this
odgd17:	mov	ss:actdir[bp+di],bx ;store name
	mov	ss:actdir[bp+di+2],dx
	pop	word ptr ss:actdir[bp+di+4]
	add	di,3*2		;skip to next 3-word entry
	cmp	di,odpmax*3*2	;full?
	je	odgd18		;yes
	 mov	word ptr ss:actdir[bp+di],0 ;mark possible end of dir name
odgd18:	cmp	al,'.'		;dot?
	jne	odgd19
	inc	si		;eat the dot
	dec	cx
	jmp	odgd4		;go get next path element
odgd19:	cmp	al,ds:delim	;closing bracket?
	jne	odgd12		;no
	inc	si		;yes, eat it
	dec	cx
	; change to [0,0] if no path left
	test	word ptr ss:actdir[bp],-1 ;anything left?
	jnz	odgd20
	mov	ax,(36*40d+36)*40d+36 ;^R000
	mov	ss:actdir[bp],ax ;CWD=[0,0]
	mov	ss:actdir+2[bp],ax
	xor	ax,ax		;load 0
	mov	ss:actdir+4[bp],ax ;finish off .RAD50 /xxxyyyzzz/
	mov	ss:actdir+6[bp],ax ;end of path
odgd20:	ret			;done
;
odgd21:	; convert octal # in BH to .RAD50 in DX
	mov	al,bh		;copy
	and	ax,300		;isolate high digit *100
	mov	dx,(40d*40d)/100 ;convert to .RAD50 * (40.**2) (MSD)
	mul	dx
	mov	dx,ax		;save
	mov	al,bh		;copy again
	and	al,70		;isolate middle digit *10
	mov	ah,40d/10	;convert to .RAD50 *40.
	mul	ah
	add	dx,ax		;save
	mov	al,bh		;copy yet again
	and	ax,7		;isolate low digit
	add	dx,ax		;save
	add	dx,(36*40d+36)*40d+36 ;add ^R000
	ret
;+
;
; Find directory.
;
; ss:bp	log dev rec (SS:ACTDIR[BP] contains dir to find)
;
; On return, variables are set up for call(s) to ODENT.
;
;-
odfd:	mov	ax,4		;file ID 4,4 = [0,0]000000.DIR;1
	mov	bx,ax
	cwd			;DX=0
	mov	ds:dirptr,dx	;init offset into ACTDIR
odfd1:	; search this directory (MFD/UFD/SFD) for next element of path
	; DL:AX = file #, BX = sequence #
	push	bx		;save
	call	odhdr		;get file header for next dir in line
	pop	ax		;[restore]
	jc	odfd3
	cmp	ax,ds:h_fseq[si] ;check H.FSEQ
	jne	odfd3
	; save file information for this directory file
	mov	ax,ds:rdsbkl[si] ;save LBN of header block
	mov	ds:idind,ax
	mov	ax,ds:rdsbkh[si]
	mov	ds:idind+2,ax
	mov	ds:idret,di	;save starting retrieval pointer
	mov	word ptr ds:idcnt,1 ;load retrieval on next read
	mov	word ptr ds:idcnt+2,0
	mov	word ptr ds:inent,512d ;say off end of block
	; find directory file size
	call	odsize		;get file size
	add	di,1		;+1 so ODENT can pre-dec
	adc	si,0
	mov	ds:idlen,di	;save
	mov	ds:idlen+2,si
	; see if this is the final file in the path
	mov	di,ds:dirptr	;fetch offset
	cmp	di,odpmax*6	;done max possible path?
	je	odfd2		;must be final dir
	test	word ptr ss:actdir[bp+di],-1 ;at end of path?
	jne	odfd4		;no
odfd2:	ret
odfd3:	jmp	dnf
odfd4:	; check next dir slot for match
	call	odent		;get next dir entry
	jc	odfd3		;EOF, no match
	jz	odfd4		;empty slot, ignore
	cmp	word ptr [si+14],15172 ;ext = .DIR?  (.RAD50)
	jne	odfd4		;no
	cmp	word ptr [si+16],1 ;version = ;1?
	jne	odfd4		;no
	push	si		;save
	add	si,6		;point at filename
	mov	di,ds:dirptr	;get offset into ACTDIR
	lea	di,ss:actdir[bp+di] ;index
	mov	cx,3		;word count
	repe	cmpsw		;check for match
	pop	si		;[restore]
	jne	odfd4		;nope, keep looking
	; found it
	add	word ptr ds:dirptr,3*2 ;skip 3 words
	mov	ax,[si]		;get file number
	mov	bx,[si+2]
	mov	dx,[si+4]
	jmp	odfd1		;go chase that down
;+
;
; Fetch next dir slot from a file.
;
; ss:bp	log dev rec (preserved)
; DS:IDIND  LBN of current header block
; DS:IDRET  offset into header block of next retrieval entry
; DS:IDLBN  current LBN in dir file
; DS:IDCNT  # blocks left in current retrieval descriptor, +1
; DS:INENT  current offset in dir block
; DS:IDLEN  remaining length of dir in blocks
;
; On return, all of the above are updated, and:
;
; ds:si	points at dir slot if CF=0, ZF=1 if slot is empty.  CF=1 if EOF.
; DS:OINENT contains the offset of the dir slot returned
;
;-
odent:	; see if we've reached the end of the current dir block
	cmp	word ptr ds:inent,512d ;off end of block?
	jb	oden3		;no
	; get next block of dir
	sub	word ptr ds:idlen,1 ;count block off of total size
	sbb	word ptr ds:idlen+2,0
	mov	ax,ds:idlen	;anything left?
	or	ax,ds:idlen+2
	jz	oden2		;no, EOF
	; see if there's anything left in the current retrieval chunk
	sub	word ptr ds:idcnt,1 ;count off the block
	sbb	word ptr ds:idcnt+2,0
	mov	ax,ds:idcnt	;anything left?
	or	ax,ds:idcnt+2
	jnz	oden5		;still more left in curr retrieval desc, go
	; end of chunk, fetch next dir retrieval descriptor from INDEXF.SYS
	mov	ax,ds:idind	;get curr block # in INDEXF.SYS of dir
	mov	dx,ds:idind+2
	xor	cl,cl		;really read it
	call	rsrdir
	; get pointer into map (validity already checked by ODHDR)
	mov	bl,[si+1]	;(H.MPOF) get offset to retrieval descriptors
	xor	bh,bh
	add	bx,bx		;*2
	add	bx,si		;add base of block buf
	mov	di,ds:idret	;get pointer to retrieval
oden1:	call	odgret		;get next retrieval pointer
	jnc	oden4		;success
	call	odgext		;get extension header
	jc	oden2		;EOF
	mov	ax,ds:rdsbkl[si] ;save LBN of new header block
	mov	ds:idind,ax
	mov	ax,ds:rdsbkh[si]
	mov	ds:idind+2,ax
	jmp	short oden1	;keep looking for more retrieval descriptors
oden2:	stc			;EOF
	ret
oden3:	jmp	short oden7
oden4:	; got a retrieval descriptor for the directory
	mov	ds:idret,di	;save
	mov	ax,ds:retlbn	;save LBN of next chunk
	mov	ds:idlbn,ax
	mov	ax,ds:retlbn+2
	mov	ds:idlbn+2,ax
	mov	ax,ds:retcnt	;and block count
	mov	ds:idcnt,ax
	mov	ax,ds:retcnt+2
	mov	ds:idcnt+2,ax
	jmp	short oden6
oden5:	add	word ptr ds:idlbn,1 ;increment LBN
	adc	word ptr ds:idlbn+2,0
oden6:	mov	word ptr ds:inent,0 ;init offset
oden7:	; all set, pick up next slot in this block
	mov	ax,ds:idlbn	;fetch the block
	mov	dx,ds:idlbn+2
	xor	cl,cl		;really read it
	call	rsrdir
	mov	ax,ds:inent	;get offset
	add	si,ax		;add to start of block
	mov	ds:oinent,ax	;save
	add	ax,8d*2		;bump to next entry for next time
	mov	ds:inent,ax	;update
	; DS:SI points at dir slot
	mov	ax,[si]		;get file number
	inc	ax		;-1?  (no more files in block?)
	jz	oden8		;right, skip rest of block
	sub	ax,1		;CF=0 (AX known NZ), and ZF=1 if empty
	ret
oden8:	mov	word ptr ds:inent,512d ;ignore rest of block
	xor	al,al		;CF=0, ZF=1
	ret
;+
;
; Get next dir entry.
;
; On return:
; CF=1		no more entries
; CF=0 ZF=0	file
;
; If file, DS:LBUF contains filename for display, DI points to end of display
; filename and beginning of .ASCIZ filename with blanks collapsed for
; comparison.
;
; For example:
; FOO      .TSK;1FOO.TSK;1
; ^LBUF          ^DI      ^0
;
;-
oddg:	call	odent		;get next dir entry
	jnc	oddg1
	 ret			;EOF
oddg1:	jz	oddg		;empty slot, skip it
	mov	di,offset ifid	;save file ID
	movsw
	movsw
	movsw
	; save filename
	mov	di,offset lbuf	;point at line buffer
	lodsw			;FILENAME$
	call	radnb
	lodsw
	call	radnb
	lodsw
	call	radnb
	mov	al,'.'		;.
	stosb
	lodsw			;EXT
	call	radnb
	mov	al,';'		;semicolon
	stosb
	lodsw			;get version #
	call	pnum		;print in decimal (octal should be an option!)
	mov	cx,offset lbuf+20d ;allow for max length filename
	sub	cx,di		;find # blanks to pad
	mov	al,' '		;blank
	rep	stosb		;fill to where size goes
	; look up other file info in INDEXF.SYS
	mov	ax,ds:ifid	;get file ID
	mov	dl,byte ptr ds:ifid+4
	call	odhdr		;look up file header
	jc	oddg2
	mov	ax,ds:ifid+2	;get sequence #
	cmp	ax,ds:h_fseq[si] ;check H.FSEQ
	je	oddg3		;OK
oddg2:	; no file header exists, or sequence number is wrong
	mov	di,offset lbuf+20d ;restore DI
	call	sqfn		;squish filename
	xor	al,al		;CF=0, ZF=1 (empty block)
	ret
oddg3:	; get file information
	mov	al,ds:h_ucha[si] ;get user-controlled characteristics
	and	al,uc_con	;NZ if contiguous
	mov	ds:icont,al	;save
	; get dates/times
	push	bx		;save ODHDR values
	push	si
	push	di
	mov	al,[si]		;get offset to ident area
	xor	ah,ah		;AH=0
	add	ax,ax		;*2
	add	si,ax		;bump to ident area
	add	si,14		;revision date/time
	call	odgdat
	call	odgtim
;; add si,7+6
;;; move that out of the way if we care
	call	odgdat		;creation date/time
	call	odgtim
	pop	di		;restore
	pop	si
	pop	bx
	call	odsize		;get file size
	mov	ds:fsize,di	;save size in blocks
	mov	ds:fsize+2,si
	mov	ds:fbcnt,ax	;save size in bytes
	mov	ds:fbcnt+2,dx
	mov	di,offset lbuf+20d ;restore DI
	callr	sqfn		;squish filename, return
;+
;
; Get file size from header.
;
; ds:si	pointer to file header
; bx,di	set up from ODHDR (map area base, offset)
;
; On return:
; si:di	file size in blocks (rounded up)
; dx:ax	file size in bytes
;
;-
odsize:	mov	dx,ds:h_ufat+f_efbk[si] ;get F.EFBK (from H.UFAT file attrs)
	mov	ax,ds:h_ufat+f_efbk+2[si]
	mov	cl,ds:h_ufat+f_rtyp[si] ;get record type
	xor	ch,ch		;zero-extend
	or	cx,ax		;size and record type all 0?
	or	cx,dx
	jnz	odsz4		;no, assume valid
	; it seems that INI doesn't fill in the attributes for some of the
	; vital system files, so count up the retrieval by hand
odsz1:	push	dx		;save length (already initted to 0 above)
	push	ax
odsz2:	call	odgret		;get next retrieval ptr
	jc	odsz3
	pop	ax		;restore
	pop	dx
	add	ax,ds:retcnt	;add this retrieval desc to total
	adc	dx,ds:retcnt+2
	jmp	short odsz1
odsz3:	call	odgext		;get extension header
	jnc	odsz2		;not EOF, read more retrieval descriptors
	pop	ax		;restore
	pop	dx
	mov	si,dx		;SI:DI = size rounted to block multiple
	mov	di,ax
	mov	bx,512d		;say F.FFBY = 512. (already have true blk cnt)
	jmp	short odsz6	;skip
odsz4:	; header looks OK, grab values out
	mov	bx,ds:h_ufat+f_ffby[si] ;get F.FFBY
	push	dx		;save
	push	ax
	test	bx,bx		;F.FFBY=0?
	jnz	odsz5		;no, last block is significant
	mov	bh,512d/400	;use all of preceding block
	sub	ax,1		;back to previous block
	sbb	dx,0
	jnc	odsz5		;OK
	inc	ax		;guess it was a null file
	inc	dx
	xor	bx,bx
odsz5:	; DX:AX = size rounded up to block multiple, stack = whole blocks,
	; BX = # valid bytes in last block
	mov	si,dx		;SI:DI = size rounted to block multiple
	mov	di,ax
	pop	ax		;restore whole blocks
	pop	dx
odsz6:	; convert to byte count (BX=F.FFBY)
	sub	ax,1		;remove last block
	sbb	dx,0
	jb	odsz7		;went to -1, must be null file
	mov	cl,9d		;shift count
	shl	dx,cl		;*512. to convert to bytes
	rol	ax,cl
	mov	cx,ax		;copy
	and	ch,777/400	;isolate 9 bits shifted out
	xor	ax,cx		;remove from LSW
	or	dx,cx		;add to MSW
	; add # significant bytes in last block
	add	ax,bx		;add # extra bytes
	adc	dx,0
	ret
odsz7:	inc	ax		;after all that, length is 0
	inc	dx
	ret
;
oddd:	; display dir entry
	mov	ax,ds:fsize	;get file size
	mov	dx,ds:fsize+2
	call	pdnum		;convert to decimal
	mov	al,'.'		;add decimal point
	stosb
	mov	cx,offset lbuf+28d ;column for "C" if contig file
	sub	cx,di		;find # to go
	jc	oddd1		;none, skip
	mov	al,' '		;pad with blanks
	rep	stosb
oddd1:	mov	al,'C'		;assume contig
	test	byte ptr ds:icont,-1 ;is it?
	jnz	oddd2
	 mov	al,' '		;nope
oddd2:	stosb
	test	byte ptr ds:fdatf,-1 ;valid date?
	jz	oddd3
	mov	ax,"  "		;two blanks
	stosw
	call	pdate		;print date
	test	byte ptr ds:ftimf,-1 ;valid time?
	jz	oddd3
	mov	al,' '		;blank
	stosb
	call	ptime		;add time
oddd3:	ret
;
odde:	; dir entry has no matching header block
	cram	'<STALE LINK>'
	ret
;
odsum:	; ODS-1 dir listing summary
	ret
;
odop:	; open ODS-1 file for input
	mov	ax,ds:ifid	;get file ID
	mov	dl,byte ptr ds:ifid+4
	call	odhdr		;get file header
	jc	odop1
	mov	ax,ds:ifid+2	;get sequence #
	cmp	ax,ds:h_fseq[si] ;check H.FSEQ
	jne	odop1
	; save file information
	mov	ax,ds:rdsbkl[si] ;save LBN of header block
	mov	ds:ifind,ax
	mov	ax,ds:rdsbkh[si]
	mov	ds:ifind+2,ax
	mov	ds:ifret,di	;save starting retrieval pointer
	xor	ax,ax		;load 0
	mov	ds:ifrec,ax	;[no outstanding record to finish]
	mov	ds:ifcnt,ax	;[load retrieval on next read]
	mov	ds:ifcnt+2,ax
	ret			;CF=0 from XOR
odop1:	jmp	fnf
;+
;
; Read file data.
;
; On entry:
; es:di	ptr to buf (normalized)
; cx	# bytes free in buf (FFFF if >64 KB)
;
; On return:
; cx	# bytes read (may be 0 in text mode if block was all NULs etc.)
;
; CF=1 if nothing was read:
;  ZF=1 end of file
;  ZF=0 buffer not big enough to hold anything, needs to be flushed
;
;-
odrd:	; see if buf needs emptying, before we try to fill it
	sub	cx,2		;-2 to guarantee we can tack on <CRLF>
	jc	odrd3		;nope
	mov	cl,ch		;get # full blocks free in buf
	shr	cl,1
	jz	odrd3		;none
	xor	ch,ch		;clear high byte
odrd1:	; see if we already have a retrieval pointer loaded up
	mov	ax,ds:ifcnt	;do we have anything ready to go?
	or	ax,ds:ifcnt+2
	jnz	odrd6		;yes
	; off end of current retrieval pointer
	push	es		;save
	push	di
	push	cx
	mov	ax,ds:fbcnt	;anything left in file?
	or	ax,ds:fbcnt+2
	jz	odrd4		;no
	; read next retrieval pointer
	mov	ax,ds:ifind	;get curr block # in INDEXF.SYS of dir
	mov	dx,ds:ifind+2
	xor	cl,cl		;really read it
	call	rsrdir
	; get pointer into map (validity already checked by ODHDR)
	mov	bl,[si+1]	;(H.MPOF) get offset to retrieval descriptors
	xor	bh,bh
	add	bx,bx		;*2
	add	bx,si		;add base of block buf
	mov	di,ds:ifret	;get pointer to retrieval
odrd2:	call	odgret		;get next retrieval pointer
	jnc	odrd5		;success
	call	odgext		;get extension header
	jc	odrd4		;EOF
	mov	ax,ds:rdsbkl[si] ;save LBN of new header block
	mov	ds:ifind,ax
	mov	ax,ds:rdsbkh[si]
	mov	ds:ifind+2,ax
	jmp	short odrd2	;keep looking for more retrieval descriptors
odrd3:	or	al,1		;ZF=0
	stc			;buf needs flushing
	ret
odrd4:	pop	cx		;flush stack
	pop	di
	pop	es
	xor	al,al		;ZF=1
	stc			;EOF
	ret
odrd5:	; got a retrieval descriptor for the directory
	mov	ds:ifret,di	;save
	mov	ax,ds:retlbn	;save LBN of next chunk
	mov	ds:iflbn,ax
	mov	ax,ds:retlbn+2
	mov	ds:iflbn+2,ax
	mov	ax,ds:retcnt	;and block count
	mov	ds:ifcnt,ax
	mov	ax,ds:retcnt+2
	mov	ds:ifcnt+2,ax
	pop	cx		;restore
	pop	di
	pop	es
odrd6:	; we have DS:IFCNT blocks starting at DS:IFLBN
	; ES:DI = buf ptr, CX = # available blocks in buf
	test	word ptr ds:ifcnt+2,-1 ;>=64 K blks available?
	jnz	odrd7		;yes, keep CX=count
	mov	ax,ds:ifcnt	;get low word
	cmp	cx,ax		;do we want it all?
	jb	odrd7		;no
	 mov	cx,ax		;yes, stop short
odrd7:	mov	ax,ds:iflbn	;get starting block
	mov	dx,ds:iflbn+2
	add	ds:iflbn,cx	;update
	adc	word ptr ds:iflbn+2,0
	sub	ds:ifcnt,cx	;deduct from count
	sbb	word ptr ds:ifcnt+2,0
	call	ss:rdblk[bp]	;read data
	jc	odrd9
	; shrink count if needed, EOF may be partway through last block
	test	word ptr ds:fbcnt+2,-1 ;definitely keeping it all?
	jnz	odrd8		;yes, >=64 KB to go
	mov	ax,ds:fbcnt	;get low word
	cmp	cx,ax		;do we have too much?
	jb	odrd8
	 mov	cx,ax		;yes, stop short
odrd8:	sub	ds:fbcnt,cx	;subtract from total
	sbb	word ptr ds:fbcnt+2,0
	; all done if binary mode, return raw block data
	test	word ptr ds:binflg,-1 ;binary mode?  (CF=0)
.assume <bin eq 0>
	jnz	odrd10		;no
	ret
odrd9:	jmp	rderr
odrd10:	; repack records if variable length ASCII file
	push	si		;save starting position
	mov	di,si		;copy
	mov	dx,ds:ifrec	;get remaining count in current record
	mov	bl,ds:ifodd	;NZ if record length was odd
	test	dx,dx		;are we in a record?
	jnz	odrd12		;yes
	; start new record
	sub	cx,2		;count off word (CX always even here)
	jc	odrd16		;no more to get
	lods	word ptr es:[si] ;fetch next length word
	mov	dx,ax		;copy
odrd11:	mov	bl,dl		;get LSB of length
	and	bl,1		;NZ if odd
	inc	dx		;force even
	and	dl,not 1
odrd12:	; copy next record (or as much of it as we have)
	; ES:SI = input ptr, CX = # input bytes left, DX = # bytes left in rec,
	; ES:DI = output ptr
	mov	ax,cx		;get # bytes left
	cmp	ax,dx		;do we need them all?
	jb	odrd13
	 mov	ax,dx		;no, stop short
odrd13:	push	ds		;save
	push	cx
	push	es		;copy ES to DS
	pop	ds
	mov	cx,ax		;get count
	rep	movsb		;copy record data
	pop	cx		;restore
	pop	ds
	sub	cx,ax		;count off of input data
	sub	dx,ax		;deduct from rec too
	jnz	odrd16		;more to go, finish this rec later (CX must =0)
	; record is finished
	test	bl,bl		;was it an odd length?
	jz	odrd14
	 dec	di		;yes, un-put last byte
odrd14:	sub	cx,2		;count off word (CX always even here)
	jc	odrd15		;no more to get
	lods	word ptr es:[si] ;fetch next length word
	mov	dx,ax		;copy
	mov	ax,cr+(lf*400)	;add CRLF
	stosw			;(may overwrite length word)
	jmp	short odrd11	;go read rec
odrd15:	mov	ax,cr+(lf*400)	;add CRLF
	stosw
	xor	dx,dx		;not in a record
odrd16:	; done, for now
	mov	ds:ifrec,dx	;save
	mov	ds:ifodd,bl
	pop	si		;restore starting position
	mov	cx,di		;ending posn
	sub	cx,si		;find length (CF=0)
	ret
;+
;
; Create output file.
;
; ds:si	.ASCIZ filename
; DS:FBCNT max size of file in bytes
;
;-
odcr:	xor	ax,ax		;load 0
	mov	ds:rtfile+6,ax	;assume no extension
	mov	ds:ofver,ax	;save output file version #
	call	lenz		;get length
	; get filename
	call	grad		;get filename
;;;	jc	odcr1		;null filename OK?
	mov	ds:rtfile,bx	;save filename
	mov	ds:rtfile+2,dx
	mov	ds:rtfile+4,di
	test	al,al		;no extension or version #?
	jz	odcr3		;right
	cmp	al,';'		;just version #?
	je	odcr2		;leave null extension
	cmp	al,'.'		;ends with dot, right?
	jne	odcr1		;no
	inc	si		;eat the dot
	dec	cx
	; extension
	call	grad		;get extension
	mov	ds:rtfile+6,bx	;save extension
	test	al,al		;end of string?
	je	odcr3
	cmp	al,';'		;version #?
	je	odcr2
	cmp	al,'.'		;TOPS-20 style syntax
	je	odcr2
odcr1:	jmp	badnam		;bad filename
odcr2:	; get version #
	jcxz	odcr3		;missing version # after all
	call	getn		;parse it
	mov	ds:ofver,ax	;save
	test	cx,cx		;that should be all
	jnz	odcr1		;isn't
odcr3:	; check for space in directory
	call	odfd		;look up the directory
odcr4:	call	odent		;find next entry
	jc	odcr5		;ran off end
	jnz	odcr4		;got one, not empty
;;; save OINENT, IDLBN for sticking in dir entry later
	jmp	short odcr6
odcr5:	; directory is full, needs to be extended
	mov	cx,1		;by 1 block
;;; get file number
;;; %out \\\UNFINISHED///
	call	odext		;do it
;;; if failed because INDEXF.SYS needs extending, extend it and try again
	jmp	short odcr3	;try again (slow but rare)
odcr6:	; allocate new header for this file
	call	odnew		;do it

; 1. Allocate header (extend INDEXF.SYS if needed)


;+
;
; Allocate a new file header.
;
; ss:bp	log dev rec
;
; dl:ax	returns new header number
; ds:si	points at dir buf containing header (need to bump seq #)
;
;-
odnew:	mov	ax,ss:indmap[bp] ;get first LBN of header bitmap
	mov	dx,ss:indmap+2[bp]
	mov	cx,ss:indbsz[bp]
odnw1:	; check next block
	push	dx		;save
	push	ax
	push	cx
	xor	cl,cl		;really read it
	call	rsrdir
	mov	di,si		;copy
	mov	cx,512d		;# bytes in a block
	mov	al,-1		;pattern to skip
	repe	scasb		;find first byte that isn't all ones
	jne	odnw2		;success, go deal
	pop	cx		;restore
	pop	ax
	pop	dx
	add	ax,1		;bump to next LBN
	adc	dx,0
	loop	odnw1		;check next block of header bitmap
	xor	al,al		;ZF=1 (index is totally full)
	stc			;failed
	ret
odnw2:	; found a byte with a free bit, figure out which bit
	dec	di		;point at the byte with the free bit
	mov	bx,1		;init offset (headers start at 1 not 0)
	mov	ch,bl		;first bit
odnw3:	test	[di],ch		;found free bit?
	jz	odnw4		;yes
	add	ch,ch		;no, shift left
	inc	bx		;bump #
	jmp	short odnw3	;loop (will always succeed)
odnw4:	; find the header # corresponding to this bit
	sub	di,si		;find offset into buf
	pop	ax		;flush loop count
	pop	ax		;restore LBN within bitmap
	pop	dx
;;; save DX:AX, DI, CH for setting bit later
	mov	cl,3		;shift count for # bits per byte
	shl	di,cl		;get bit # of base of byte
	add	bx,di		;add bit within byte (+1 since hdrs start @ 1)
	sub	ax,ss:indmap[bp] ;find relative block #
	sbb	dx,ss:indmap+2[bp]
	mov	cl,12d		;shift count for 4096 bits per block
	shl	dx,cl		;move left
	rol	ax,cl
	mov	cx,ax		;copy
	and	ch,777/400	;isolate 9 LSBs
	or	dx,cx		;insert in DX
	xor	ax,cx		;remove from AX
	add	ax,bx		;add bit within block (+1 since hdrs start @ 1)
	adc	dx,0		;could carry thanks to that pesky +1
	; fetch that header block
	push	dx		;save header #
	push	ax
	call	odhdr		;fetch header
	jnc	odnw5		;succeeded?!  but we *know* it's free!
	test	al,al		;error *should* be "header deleted"
	jz	odnw6		;it is, excellent
	jns	odnw5		;other errors are crazy
	; INDEXF.SYS isn't fully pre-extended, the block doesn't exist
	mov	ax,1		;file # 1,1
	cwd
	mov	cx,ss:filext[bp] ;default by pack default # of blocks
	call	odext
;;; error?




	jmp	short odnew	;start over from scratch
				;(may have added an extension header to itself
				;and eaten the free header we just found?)
odnw5:	jmp	baddir		;something impossible happened
odnw6:	; looks good, set our header bitmap bit

;;; get regs back to set bitmap bit
;;; %out ///////

	pop	ax		;restore header #
	pop	dx
	push	dx		;save again
	push	ax
	call	odhdr		;fetch header (will get "deleted" error)
	pop	dx		;restore
	pop	ax
	; fill in header
	inc	word ptr ds:h_fseq[si] ;bump sequence #
	mov	cx,ss:rdslev[bp] ;get pack ODS-1 version
	mov	ds:h_flev[bp],cx
	mov	ds:h_fnum[si],ax ;set file #
	cmp	cx,104h		;do extension file #s exist?
	jb	odnw7
	 mov	word ptr ds:h_fext[si],dx ;yes, save it
odnw7:
;;; get other stuff too (depends on whether extension vs. new file)


	ret





;+
;
; Extend a file, clearing the new blocks.
;
; ss:bp	log dev rec
; dl:ax	file number (already accessed before call so seq # not needed)
; cx	# blocks to add (16 bits are enough, since this routine is used only
;	for adding a little bit to INDEXF.SYS or a .DIR file, not general-
;	purpose extension of any file by an arbitrary amount)
;
;-

;;; errors:

;;; disk full
;;; INDEXF.SYS needs extending (give error rather than recursing)

odext:	mov	ds:odfno,ax	;save file number
	mov	byte ptr ds:odfno+2,dl
	mov	ds:odsiz,cx	;save size needed
	xor	ax,ax		;load 0
;;; could be NZ if needed
	mov	ds:odsiz+2,ax
odex1:	mov	di,ds:odsiz	;get size needed
	mov	si,ds:odsiz+2
	call	odff		;search for contiguous free area (OK if fails)
	mov	si,ax		;see what we got
	or	si,dx
;;	jz	...		;nothing?!

;;; add to retrieval list

;;; save PCN/length
;;; failed, take max, clear the bits, add to header

odex2:





;+
;
; Find next contiguous free area that's .GE. a given size, or the largest one
; available if none exists in that size.
;
; si:di	size needed in PCs
;
; bx:cx	returns starting PCN of region (CF=0 if .GE. SI:DI)
; dx:ax	returns length available (=SI:DI if CF=0, less if CF=1)
;
;-
odff:	xor	ax,ax		;load 0
	mov	ds:odmax,ax	;nothing yet
	mov	ds:odmax+2,ax
	mov	cx,ss:satptr[bp] ;get location to start
	mov	bx,ss:satptr+2[bp]
	call	odff1		;search for contiguous free area
	jnc	odff6		;got something
	xor	bx,bx		;wrap around to begn of disk and drop through
	xor	cx,cx
odff1:	; get next area, see if big enough
	push	si		;save
	push	di
	call	odfree		;find next free area
	pop	di		;[restore]
	pop	si
	jc	odff7		;nothing left
	cmp	dx,si		;big enough?
	ja	odff5		;yes
	jb	odff2		;no
	cmp	ax,di		;check low word?
	jae	odff5		;yes
odff2:	; see if this is a new max
	cmp	dx,ds:odmax+2	;biggest yet?
	ja	odff3		;yes
	jb	odff4		;no
	cmp	ax,ds:odmax	;check low word
	jb	odff4		;no good
odff3:	mov	ds:odmpcn,cx	;save starting PCN
	mov	ds:odmpcn+2,bx
	mov	ds:odmax,ax	;save size
	mov	ds:odmax+2,dx
odff4:	sub	ax,1		;-1 to get to end
	sbb	dx,0
	add	cx,ax		;advance to end of free area
	adc	bx,dx
	jmp	short odff1	;try again
odff5:	mov	ax,di		;requested size is fine
	mov	dx,si
	clc			;happy
odff6:	ret
odff7:	; failed, grab the closest match in case 2nd time through
	mov	cx,ds:odmpcn	;[get starting PCN (junk if ODMAX=0)]
	mov	bx,ds:odmpcn+2
	mov	ax,ds:odmax	;[get size]
	mov	dx,ds:odmax+2
	ret			;CF=1 from ODFREE
;+
;
; Find next free area on disk.
;
; bx:cx	starting PCN to search (find first free area >= this disk loc)
; ss:bp	log dev rec
;
; On return:
; bx:cx	starting PCN of free area
; dx:ax	PCs in free area
;
;-
odfree:	call	odgb		;get bitmap
	jc	odfr3		;off end
odfr1:	; search for a set bit
	; DS:SI=block buffer, BX=offset, AL=bit, DX:CX=corresponding PCN
	test	[bx+si],al	;free?
	jnz	odfr4		;yes
	add	cx,1		;PCN+1
	adc	dx,0
	cmp	dx,ss:numclu+2[bp] ;off EOD?
	ja	odfr3
	jb	odfr2
	cmp	cx,ss:numclu[bp] ;check LSW
	jae	odfr3
odfr2:	rol	al,1		;left a bit
	jnc	odfr1		;loop
	inc	bx		;bump to next byte
	cmp	bh,1000/400	;reached end of block?
	jb	odfr1		;no
	call	odnb		;get next bitmap block
	jnc	odfr1		;loop unless EOF
odfr3:	stc			;nothing available
	ret
odfr4:	; found a free cluster, count # consecutive free PCs
	push	dx		;save
	push	cx
odfr5:	; search for a set bit
	; DS:SI=block buffer, BX=offset, AL=bit, DX:CX=corresponding PCN
	test	[bx+si],al	;in use?
	jz	odfr7		;yes
	add	cx,1		;PCN+1
	adc	dx,0
	cmp	dx,ss:numclu+2[bp] ;off EOD?
	ja	odfr7
	jb	odfr6
	cmp	cx,ss:numclu[bp] ;check LSW
	jae	odfr7
odfr6:	rol	al,1		;left a bit
	jnc	odfr5		;loop
	inc	bx		;bump to next byte
	cmp	bh,1000/400	;reached end of block?
	jb	odfr5		;no
	call	odnb		;get next bitmap block
	jnc	odfr5		;loop unless EOF
	pop	cx		;[restore]
	pop	dx
	ret
odfr7:	mov	ax,cx		;copy first PCN in use (or EOD) to DX:AX
	pop	cx		;restore starting PCN in BX:CX
	pop	bx
	sub	ax,cx		;find length, CF=0
	sbb	dx,bx
	ret
;+
;
; Set bit(s) in [0,0]BITMAP.SYS.
;
; bx:cx	starting PCN to mark as free
; dx:ax	number of PCs to mark as free
;
; ss:bp	log dev rec
;
;-
odsbit:	push	dx		;save
	push	ax
	call	odgb		;get bitmap
	pop	cx		;restore
	pop	dx
odsb1:	mov	ds:rdsdrt[si],al ;buffer is dirty
odsb2:	; set a bit
	; DS:SI=block buffer, BX=offset, AL=bit, DX:CX=loop counter
	or	[bx+si],al	;set a bit
	dec	cx		;count it
	jz	odsb4		;done (probably)
odsb3:	rol	al,1		;left a bit
	jnc	odsb2		;loop
	inc	bx		;bump to next byte
	cmp	bh,1000/400	;reached end of block?
	jb	odsb2		;no
	call	odnb		;get next bitmap block
	jmp	short odsb1	;go mark as dirty and continue
odsb4:	sub	dx,1		;deduct from high-order count
	jnc	odsb3		;continue unless was 0
	ret
;+
;
; Clear bit(s) in [0,0]BITMAP.SYS.
;
; bx:cx	starting PCN to mark as in use
; dx:ax	number of PCs to mark as in use
; ss:bp	log dev rec
;
;-
odcbit:	push	dx		;save
	push	ax
	call	odgb		;get bitmap
	pop	cx		;restore
	pop	dx
odcb1:	mov	ds:rdsdrt[si],al ;buffer is dirty
	not	al		;flip mask
odcb2:	; clear a bit
	; DS:SI=block buffer, BX=offset, AL=bit, DX:CX=loop counter
	and	[bx+si],al	;clear a bit
	dec	cx		;count it
	jz	odcb4		;done (probably)
odcb3:	rol	al,1		;left a bit
	jnc	odcb2		;loop
	inc	bx		;bump to next byte
	cmp	bh,1000/400	;reached end of block?
	jb	odcb2		;no
	call	odnb		;get next bitmap block
	jmp	short odcb1	;go mark as dirty and continue
odcb4:	sub	dx,1		;deduct from high-order count
	jnc	odcb3		;continue unless was 0
	ret
;+
;
; Read [0,0]BITMAP.SYS;1 file.
;
; bx:cx	starting PCN to which to index
;
; On return:
; ds:si	points at dir buf
; bx	offset of byte containing starting bit
; al	1 for starting bit, 0 for others
; dx:cx	starting PCN
;
;-
odgb:	; find starting block offset
	push	bx		;save starting PCN
	push	cx
	mov	ax,cx		;get LSBs
	and	ch,7777/400	;isolate bit offset within block
	push	cx		;save
	mov	cl,12d		;shift count
	shr	ax,cl		;right-justify
	ror	bx,cl		;MSW too
	mov	cx,bx		;copy
	and	cl,not 0Fh	;isolate middle 12. bits
	xor	bx,cx		;isolate MSBs
	or	ax,cx		;compose LSBs
	add	ax,1		;+1 to skip VBN 1 of file (control block)
	adc	bx,0		;(actual bitmap starts at VBN 2)
	push	bx		;save
	push	ax
	; fetch bitmap file
	mov	ax,2		;file ID 2,2 = [0,0]BITMAP.SYS;1
	cwd			;DL=0
	push	ax		;save
	call	odhdr		;get file header
	pop	ax		;[restore]
	jc	odgb2
	cmp	ax,ds:h_fseq[si] ;check H.FSEQ (must also be 2)
	jne	odgb2
	call	odsize		;get file size
	pop	ax		;catch relative VBN
	pop	dx
	push	dx		;save again
	push	ax
	sub	di,ax		;find # blocks following ours
	sbb	si,dx
	jc	odgb4		;overflowed
	mov	ax,si		;exact match?
	or	ax,di
	jz	odgb4		;that's no good either
	mov	ds:oblen,di	;save remaining length
	mov	ds:oblen+2,si
	mov	ax,2		;get header back again
	cwd
	call	odhdr		;(we know this will succeed)
odgb1:	; look for our starting block
	call	odgret		;get next retrieval pointer
	jc	odgb3		;time for extension header
	pop	ax		;restore relative VBN
	pop	dx
	sub	ax,ds:retcnt	;subtract out the size of this retrieval
	sbb	dx,ds:retcnt+2
	jc	odgb5		;passed it, back up
	push	dx		;save updated value
	push	ax
	jmp	short odgb1	;get next range
odgb2:	jmp	baddir		;file 2,2,0 must always exist!
odgb3:	call	odgext		;get extension header
	jnc	odgb1		;success
odgb4:	add	sp,5*2		;flush block count, bit index, starting PCN
	stc			;EOF, failed
	ret
odgb5:	; found starting block
	mov	bx,ds:retcnt	;get size of range
	mov	cx,ds:retcnt+2
	add	ax,bx		;restore relative VBN within retrieval range
	adc	dx,cx
	sub	bx,ax		;remove from count
	sbb	cx,dx
	mov	ds:obcnt,bx	;save # left in retrieval range
	mov	ds:obcnt+2,cx
	add	ax,ds:retlbn	;advance to starting block
	adc	dx,ds:retlbn+2
	mov	ds:oblbn,ax	;save
	mov	ds:oblbn+2,dx
	mov	bx,ds:rdsbkl[si] ;save LBN of header block
	mov	cx,ds:rdsbkh[si]
	mov	ds:obind,bx
	mov	ds:obind+2,cx
	mov	ds:obret,di	;save starting retrieval pointer
	xor	cl,cl		;really read the block
	call	rsrdir
	pop	cx		;restore bit offset within block (0-7777)
	mov	bx,cx		;copy
	mov	al,1		;load a 1
	and	cl,7		;isolate bit within byte
	shl	al,cl		;shift into place
	mov	cl,3		;shift count
	shr	bx,cl		;get byte offset
	and	bh,777/400	;isolate 9-bit block offset, CF=0
	pop	cx		;[restore starting PCN]
	pop	dx
	ret
;+
;
; Fetch next block in bitmap file.
;
; ss:bp	log dev rec (preserved)
;
; CF=1 if EOF, or:
; ds:si	buffer containing next bitmap block
; bx	0 (starting buf offset)
; al	1 (starting bit mask)
; cx:dx	preserved
;
;-
odnb:	push	cx		;save
	push	dx
	sub	word ptr ds:obcnt,1 ;count off the block we just finished
	sbb	word ptr ds:obcnt+2,0
	mov	ax,ds:obcnt	;0?
	or	ax,ds:obcnt+2
	jz	odnb2		;yes, get next retrieval ptr
	mov	ax,ds:oblbn	;get blk #
	mov	dx,ds:oblbn+2
	add	ax,1		;+1
	adc	dx,0
odnb1:	mov	ds:oblbn,ax	;update
	mov	ds:oblbn+2,dx
	xor	cl,cl		;really read the block
	call	rsrdir
	pop	dx		;restore
	pop	cx
	mov	al,1		;starting bit
	xor	bx,bx		;starting offset, CF=0
	ret
odnb2:	; end of chunk, fetch next dir retrieval descriptor from INDEXF.SYS
	mov	ax,ds:obind	;get curr block # in INDEXF.SYS of dir
	mov	dx,ds:obind+2
	xor	cl,cl		;really read it
	call	rsrdir
	; get pointer into map (validity already checked by ODHDR)
	mov	bl,[si+1]	;(H.MPOF) get offset to retrieval descriptors
	xor	bh,bh
	add	bx,bx		;*2
	add	bx,si		;add base of block buf
	mov	di,ds:obret	;get pointer to retrieval
odnb3:	call	odgret		;get next retrieval pointer
	jnc	odnb5		;success
	call	odgext		;get extension header
	jc	odnb4		;EOF
	mov	ax,ds:rdsbkl[si] ;save LBN of new header block
	mov	ds:obind,ax
	mov	ax,ds:rdsbkh[si]
	mov	ds:obind+2,ax
	jmp	short odnb3	;keep looking for more retrieval descriptors
odnb4:	pop	dx		;restore
	pop	cx
	stc			;EOF
	ret
odnb5:	; got a retrieval descriptor for the directory
	mov	ds:obret,di	;save
	mov	ax,ds:retcnt	;save block count of next chunk
	mov	ds:obcnt,ax
	mov	ax,ds:retcnt+2
	mov	ds:obcnt+2,ax
	mov	ax,ds:retlbn	;get starting LBN
	mov	dx,ds:retlbn+2
	jmp	short odnb1
;+
;
; Locate the ODS-1 home block.
;
; Normally block 1, but failing that we search multiples of 256.
;
; ss:bp	log dev rec
;
; On return, CF=1 if not found, otherwise:
; dx:ax	block # of home block
; ds:si	points at home block (in BUF)
;
;-
odhome:	mov	ax,1		;first try is blk 1
	cwd
odhm1:	; try next home block location, DX:AX = phys block #
	mov	cx,ss:devsiz+6[bp] ;check high bits of DEVSIZ
	or	cx,ss:devsiz+4[bp]
	jnz	odhm2		;NZ, blk # is definitely in range
	cmp	dx,ss:devsiz+2[bp] ;in range?
	ja	odhm3		;no
	jb	odhm2		;yes
	cmp	ax,ss:devsiz[bp] ;check low word
	jae	odhm3		;no
odhm2:	mov	di,offset buf	;pt at buffer
	mov	cx,1		;read 1 block
	push	dx		;save
	push	ax
	call	ss:rdblk[bp]	;go
	pop	ax		;[restore]
	pop	dx
	jc	odhm4		;error
	cmp	cx,512d		;got all 512. bytes of home block?
	jb	odhm4		;no
	add	si,760		;index to system ID field
	mov	di,offset sgnods ;point at "DECFILE11A" signature
	mov	cx,12d/2	;12 chars long including blanks
	repe	cmpsw		;match?
	je	odhm5		;yes, looks valid
odhm3:	stc			;not found
	ret
odhm4:	add	ax,256d		;bump to next multiple of 256.
	adc	dx,0
	and	al,not 1	;clear LSB (was block 1 on first try)
	jmp	short odhm1
odhm5:	mov	si,offset buf	;point at buf again
	clc			;happy
	ret
;+
;
; Read an ODS-1 file header.
;
; ss:bp	log dev rec
; dl:ax	file header number to find (.LT.65536. unless structure 1.4)
;
; Yes I know ODS level 1.4 doesn't really exist but I want to plan ahead in
; case it ever does, so I'll honor the 24-bit file numbers.  Note that there's
; no space in the header map area for the high 8 bits of an extension header's
; file number.  So files in 1.4 had better be either small or not very
; fragmented, or else somehow we'd have to guarantee that the extensions are
; below header # 65536.
;
; ds:si	returns pointer to file header (in a dir buf)
; ds:bx	returns pointer to map area of buf
; di	returns offset of first retrieval descriptor
;
;-
odhdr:	xor	dh,dh		;zero-extend
	push	dx		;save
	push	ax
	; see if it's one of the first 16 file numbers
	; (headers are always contiguous with the header bitmap, easy to find)
	test	dx,dx		;MSW=0?
	jnz	odhd8		;no
	test	ax,ax		;but # is NZ, right?
	jz	odhd7		;invalid
	cmp	ax,16d		;1-16?
	ja	odhd8
	; it's one of the first 16, we know where it is w/o checking INDEXF.SYS
	dec	ax		;-1
	add	ax,ss:filhdr[bp] ;add starting block # (following index bitmap)
	adc	dx,ss:filhdr+2[bp]
odhd1:	xor	cl,cl		;really read it
	call	rsrdir
	pop	ax		;restore file #
	pop	dx
	test	word ptr [si],-1 ;deleted?
	jz	odhd4		;yes
	; check checksum
	mov	cx,255d		;word count
	xor	bx,bx		;init checksum
odhd2:	add	bx,[si]		;add to total
	inc	si		;+2
	inc	si
	loop	odhd2
	cmp	bx,[si]		;checksum OK?
	jne	odhd5		;no
	sub	si,255d*2	;restore SI
	cmp	ax,ds:h_fnum[si] ;H.FNUM = file #?
	jne	odhd6		;no
	cmp	word ptr ds:h_flev[si],104h ;ODS 1.4?
	jne	odhd3
	cmp	dl,ds:h_fext[si] ;H.FEXT = file # extension?
	jne	odhd6
odhd3:	; find map area
	mov	bl,[si+1]	;(H.MPOF) get offset to retrieval descriptors
	xor	bh,bh
	add	bx,bx		;*2
	add	bx,si		;add base of block buf
	mov	di,12		;M.RTRV (init ptr)
;;; check map area length byte for validity

	clc			;happy
	ret
odhd4:	xor	al,al		;header deleted
	stc
	ret
odhd5:	mov	al,1		;bad checksum
	stc
	ret
odhd6:	mov	al,2		;file # doesn't match
	stc
	ret
odhd7:	jmp	baddir		;file #0, should never happen
odhd8:	; not in the first 16, need to look up in INDEXF.SYS retrieval
	add	ax,2-1		;skip boot, home blocks
	adc	dx,0		;(but correct since #s start at 1 not 0)
	add	ax,ss:indbsz[bp] ;also deduct header bitmap
	adc	dx,0
	push	dx		;save # headers until ours
	push	ax
	mov	ax,1		;INDEXF.SYS=1,1,0
	cwd			;zero-extend
	call	odhdr		;read its own header out of itself
odhd9:	; check next retrieval descriptor
	call	odgret		;fetch next
	jc	odhd11		;EOF or time for extension header
	pop	ax		;catch remaining count to skip
	pop	dx
	cmp	dx,ds:retcnt+2	;is it within area covered by this descriptor?
	ja	odhd10		;no
	jb	odhd12		;yes
	cmp	ax,ds:retcnt	;check low word
	jb	odhd12		;yes
odhd10:	sub	ax,ds:retcnt	;subtract it out
	sbb	dx,ds:retcnt+2
	push	dx		;save again
	push	ax
	jmp	short odhd9	;around for next
odhd11:	; get next extension header to map more of INDEXF.SYS
	call	odgext		;get extension header
	jnc	odhd9		;loop
	add	sp,4*2		;flush stack
	mov	al,-1		;header # too high, off EOF of INDEXF.SYS
	stc			;error return
	ret
odhd12:	; found the retrieval for our header block
	add	ax,ds:retlbn	;add starting LBN of area
	adc	dx,ds:retlbn+2
	jmp	odhd1		;go fetch the header block, check it, return
;+
;
; Get next retrieval descriptor.
;
; ds:bx	pointer to map region (preserved)
; di	offset of next retrieval descriptor (updated on return)
; si	preserved (presumably block buffer)
;
; DS:RETLBN  returns starting LBN (32 bits)
; DS:RETCNT  returns block count (32 bits)
;
; Or CF=1 if off end of header
;
;-
odgret:	; see if pointer already off end
	mov	al,[bx+10]	;get M.USE
	xor	ah,ah		;zero-extend
	add	ax,ax		;*2 to get bytes
	add	ax,12		;add M.RTRV to get max pointer value
	cmp	di,ax		;at (or beyond?!) end of list?
	jae	odgr1		;yes, EOF or time for extension header
	mov	word ptr ds:retcnt+2,0 ;assume clearing high order
	mov	dx,[bx+6]	;get M.LBSZ,,M.CTSZ
	cmp	dx,301h		;24-bit LBN, 8-bit count?
	jne	short odgr2
	; 1,3 format
	mov	al,[bx+di+1]	;get count
	xor	ah,ah		;zero-extend
	inc	ax		;+1
	mov	ds:retcnt,ax	;save
	cwd			;DX=0
	mov	dl,[bx+di]	;get LBN<23:16> (DH=0 already)
	mov	ax,[bx+di+2]
	jmp	short odgr3	;return
odgr1:	stc			;end of retrieval list for this header
	ret
odgr2:	cmp	dx,202h		;16-bit LBN and count?
	jne	short odgr4
	; 2,2 format
	mov	ax,[bx+di]	;get count
	add	ax,1		;+1
	mov	ds:retcnt,ax	;[save]
	adc	word ptr ds:retcnt+2,0
	mov	ax,[bx+di+2]	;get LBN
	xor	dx,dx		;zero-extend
odgr3:	add	di,4		;skip, CF=0
	jmp	short odgr5	;[return]
odgr4:	cmp	dx,402h		;2,4 format?
	jne	short odgr6
	; 2,4 format
	mov	ax,[bx+di]	;get count
	add	ax,1		;+1
	mov	ds:retcnt,ax	;[save]
	adc	word ptr ds:retcnt+2,0
	mov	ax,[bx+di+4]	;get LBN
	mov	dx,[bx+di+2]
	add	di,6		;skip, CF=0
odgr5:	mov	ds:retlbn,ax	;[save]
	mov	ds:retlbn+2,dx
	ret
odgr6:	; retrieval descriptor format is none of the above
;;; could try to figure it out, looks like LBN always has the words backwards,
;;; take a wild guess with count
	jmp	baddir
;+
;
; Get next extension header.
;
; ds:bx	pointer to map region of current header
;
;-
odgext:	mov	ax,[bx+2]	;(M.EFNU) get extension file #
	test	ax,ax		;EOF?
	jz	odgx1		;yes
	xor	dl,dl		;apparently no way to get 8 MSBs in ODS 1.4
	push	word ptr [bx]	;save M.ESQN (extension sequence #)
	push	word ptr [bx+4]	;and M.EFSQ (file sequence #)
	call	odhdr		;get next header
	pop	ax		;[restore]
	pop	dx
	jc	odgx2		;failed
	cmp	ax,ds:h_fseq[si] ;does M.EFSQ match H.FSEQ?
	jne	odgx2
	inc	dx		;should be next header sequence #
	cmp	dl,[bx]		;is it?  (CF=0 if so)
	jne	odgx2
	ret			;yes
odgx1:	stc			;EOF
	ret
odgx2:	jmp	baddir
;+
;
; Get an ODS-1 date from a buffer.
;
; ds:si	points at ASCII date (DDMMMYY)
;
; The high digit of the YY field is allowed to overflow into successive ASCII
; character values after '9' to get around the Y2K problem with this format.
; Yech!
;
; On return, FYEAR/FMONTH/FDAY are filled in and FDATF is set if valid date.
;
;-
odgdat:	push	si		;save
	mov	byte ptr ds:fdatf,0 ;not valid yet
	lodsw			;get DD
	call	ogdt5		;convert a dig
	jc	ogdt3
	xchg	al,ah		;><
	call	ogdt5		;convert other dig
	jc	ogdt3
	aad			;AX=AH*10+AL
	cmp	al,31d		;in range?
	ja	ogdt3
	mov	ds:fday,al	;save if so
	mov	di,offset months+3 ;point at first valid month
	xor	dx,dx		;init month #
	lodsb			;get char 1 of month
	mov	bl,al		;copy
	lodsw			;get chars 2-3 of month
	and	bl,not 40	;convert to UC if LC (should be UC already)
	and	ax,337*101h
ogdt1:	mov	bh,[di]		;get next month
	inc	di
	mov	cx,[di]
	inc	di
	inc	di
	inc	dx		;bump to next month # (1-12)
	and	cx,337*101h	;convert chars 2-3 to UC (char 1 is already)
	cmp	bl,bh		;match?
	jne	ogdt2
	cmp	ax,cx
	je	ogdt4		;yes, got it
ogdt2:	cmp	dl,12d		;done all 12?
	jb	ogdt1		;loop if not
ogdt3:	pop	si		;restore starting posn
	add	si,7		;skip the invalid date
	ret			;no good
ogdt4:	mov	ds:fmonth,dl	;save
	lodsb			;get high digit
	sub	al,'0'		;cvt to binary, allow any value (Y2K kludge!)
	mov	ah,10d		;multiply by 10
	mul	ah
	mov	bx,ax		;copy
	lodsb			;get low digit
	call	ogdt5		;convert and check it
	jc	ogdt3		;failed
	cbw			;zero-extend
	add	ax,bx		;compose year since 1900
	add	ax,1900d	;add base
	mov	ds:fyear,ax
	inc	byte ptr ds:fdatf ;date is valid
	pop	ax		;flush stack
	ret
ogdt5:	sub	al,'0'		;convert to binary
	cmp	al,10d		;0-9?
	cmc			;CF=1 if not
	ret
;+
;
; Get an ODS-1 time from a buffer.
;
; ds:si	points at ASCII time (HHMMSS)
;
; On return, FHOUR/FMIN/FSEC are filled in and FTIMF is set if valid time.
;
;-
odgtim:	push	si		;save
	mov	byte ptr ds:ftimf,0 ;not valid yet
	mov	bl,24d		;range for HH is 0-23
	call	ogtm2		;convert HH
	jc	ogtm1
	mov	ds:fhour,al	;yes
	mov	bl,60d		;range for MM/SS is 0-59
	call	ogtm2		;convert MM
	jc	ogtm1
	mov	ds:fmin,al
	call	ogtm2		;convert SS
	jc	ogtm1
	mov	ds:fsec,al
	inc	byte ptr ds:ftimf ;time is valid
	pop	ax		;flush stack
	ret
ogtm1:	pop	si		;restore starting posn
	add	si,6		;skip the invalid time
	ret			;no good
ogtm2:	lodsw			;get two digits
	call	ogtm4		;convert high digit
	jc	ogtm3
	xchg	al,ah		;><
	call	ogtm4		;convert low digit
	jc	ogtm3
	aad			;AL=AH*10+AL
	cmp	al,bl		;in range?
	cmc			;CF=1 if not
ogtm3:	ret
ogtm4:	sub	al,'0'		;convert to binary
	cmp	al,10d		;0-9?
	cmc			;CF=1 if not
	ret
;
	subttl	RSTS file structure
;
	comment	_

I'd better write down a few things that Paul Koning <koning@lkg.dec.com>
(formerly of the RSTS group) clarified for me before I forget them:

PCNs are used only to index SATT.SYS, and even though it seems weird the
Internals manual is right about PCN 0 starting at DCN 1, the purpose is to keep
the boot block from being involved in [0,1]SATT.SYS at all.  PCNs are not used
anywhere else, but the PCS sets a lower bound for FCSes and dir cluster sizes.

All retrieval ptrs (including the ones in the RDS 1.X MFD/GFD arrays and the
ones in blockette 31 of every UFD block) are DCNs, and point to the first DC
of a file (or directory) cluster.  The appropriate number of consecutive bits
are set in [0,1]SATT.SYS to cover all PCs of the cluster.  The retrieval list
contains one retrieval DCN for each file (or directory) cluster in the file
(dir), even if it's a contiguous file.

If DCS>1, the block(s) between the boot block and the first block of DC 1 are
wasted, as are the blocks in the partial PC at the end of the pack (if the pack
size isn't a multiple of PCS, plus DCS).

The idea of modifying the directory link word format for dir clustersizes >16
was proposed but never implemented.  On disks with PCS>16, (the max was
extended to 64 in RSTS 9.5) the directories all remain at clu=16, with the
remainder of each directory cluster wasted.

----------------------------------------
General notes (some from Infernals manuals, some based on above):

Cluster sizes are always a power of two.

Names:
LBN	logical block number (actual physical block #)
DCS	device cluster size
DCN	device cluster number
PCS	pack cluster size
PCN	pack cluster number
FCS	file cluster size (there are no FCNs, files are addressed by block)
FBN	FIP block number

Typical pack layout with DCS=2 and PCS=4 to show relationships in numbers:

	+---+---+---+---+---+---+---+---+---+---+---
LBN	| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |...
	+---+---+---+---+---+---+---+---+---+---+---
DCN	|   0   |   1   |   2   |   3   |   4   |...  = LBN/DCS
	+---+---+---+---+---+---+---+---+---+---+---
FBN	| 0 |///| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |...  (internal to FIP only)
	+---+---+---+---+---+---+---+---+---+---+---
PCN	        |       0       |       1       |...  PCN 0 starts at DCN 1
	        +---------------+---------------+---

DCS is the natural device cluster size, i.e. the smallest power of two which
results in less than 65,536 clusters in the pack.  DCNs are used for all
retrieval pointers, and the DCS sets the lower bound for the PCS.

PCS is the pack cluster size assigned when it was DSKINTed.  PCS.GE.DCS.  PCN 0
starts at DCN 1.  Pack clusters are the minimum allocation unit, each bit in
[0,1]SATT.SYS represents one PCN.  SATT.SYS is the *only* place where PCNs are
used.

FCS is the file cluster size, up to 256.  FCS.GE.PCS.  This seems to be mainly
a retrieval optimization, PCNs within an FC are contiguous but they don't have
to begin on an FCS multiple.  Hmm, is that true?  Experimentation suggests that
they *do* begin on an FCS multiple.  FCS granularity is used in retrieval
lists, so a bigger FCS means a smaller retrieval list.  However the entire FC
is allocated for each cluster even if the file ends before using all of it, so
there may be wasted space.

FBNs are FIP block numbers, they don't appear on the disk but they're used
internally by FIP.  They're 23-bit numbers which are *almost* the physical
block #, but not quite:  FBN 0 is always physical block 0, but FBN 1 is the
first cluster of DCN 1, and the rest go linearly up from there.  As Paul said,
if DCS>1 there will be inaccessible blocks between FBN 0 and DCN 1.

I can't find anything that specifically admits that [0,1]SATT.SYS is always a
contiguous file, or that its FCS=PCS, but it does seem to be true.  MNTRS
checks the retrieval chain just to make absolutely sure (so it's happy even if
the file doesn't have US.NOX set, as long as it happens in fact to be
contiguous).  So once the disk is mounted, we know for sure that we can just
use SS:BITMAP[BP] to index into SATT.SYS, and since the length was checked by
MNTRS too we know that the bits exist for all PCNs we would use as indices.

_
;+
;
; Print volume name.
;
;-
rsvn:	cram	' is '		;we always succeed
	push	di		;save
	mov	di,ds:dbuf	;pt at buffer
	mov	ax,1		;DCN 1 (pack label (MFD in 0.0))
	mov	cx,ax		;count=1 blk
	mul	ss:dcs[bp]	;*DCS to get block #
	call	ss:rdblk[bp]	;fetch it
	lea	si,[di+14]	;point at pack ID
	pop	di		;restore
	lodsw			;convert to ASCII
	call	radnb
	lodsw
	callr	radnb		;and return
;
rsds:	; set defaults
	mov	ax,ss:curdir[bp] ;get current PPN
	mov	ss:actdir[bp],ax ;set active PPN
	ret
;+
;
; Save RSTS CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
rssav:	mov	ax,ss:actdir[bp] ;get active dir
	mov	ds:savdir[bx],ax
	push	si		;save
	push	cx
	mov	cl,3		;shift count
	sal	bx,cl		;buf size = 8 words
	lea	si,ss:dirclu[bp] ;pt at clu, retrieval pointers
	lea	di,ds:savwnd[bx] ;pt at save buffer
	mov	cx,8d		;copy 8 words
.assume <dirwnd eq dirclu+2>
	rep	movsw		;copy
	pop	cx		;restore
	pop	si
	ret
;+
;
; Restore RSTS CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
rsrst:	mov	ax,ds:savdir[bx] ;get saved stuff
	mov	ss:actdir[bp],ax ;set active LD:
	mov	cl,3		;shift count
	sal	bx,cl		;buf size = 8 words
	lea	si,ds:savwnd[bx] ;pt at save buffer
	lea	di,ss:dirclu[bp] ;pt at clu, retrieval pointers
	mov	cx,8d		;copy 8 words
.assume <dirwnd eq dirclu+2>
	rep	movsw		;copy
	ret
;+
;
; Print directory heading.
;
;-
rsdh:	xor	ax,ax		;load 0
	mov	ds:nfile,ax	;no files yet
	mov	ds:nblk,ax	;no blocks seen
	mov	ds:nblk+2,ax
	mov	di,offset lbuf	;pt at buf
	cram	' Name .Ext    Size    Prot'
	test	byte ptr ds:dirflg,200 ;verbose?
	jz	rsdh1		;no
	cram	'   Access   '
rsdh1:	cram	'    Date    '
	test	byte ptr ds:dirflg,200 ;verbose?
	jz	rsdh2		;no
	cram	' Time  Clu  RTS    Pos'
rsdh2:	callr	flush		;print, return
;
rsdi:	; RSTS dir input init
	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	rsdi1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir blk buffer (nothing is possible if this fails)
	mov	cx,rdslen	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:rdsbkl[si],ax ;nothing in here yet
	mov	ds:rdsbkh[si],ax
	mov	ds:rdsdrt[si],al ;not dirty
	mov	ds:rdsnxt[si],ax ;no next
rsdi1:	; find dir and fetch retrieval
	mov	ss:ecache[bp],1	;enable caching
	mov	ax,ss:dirwnd[bp] ;get retrieval ptr for first dir block
	mul	ss:dcs[bp]	;*DCS to get block
	xor	cl,cl		;really read it
	call	rsrdir		;read dir block
	lodsw			;get ptr to first entry
	mov	ds:inent,ax	;save
	mov	word ptr ds:oinent,0 ;no previous
	ret
;+
;
; Find directory.
;
; ss:bp	log dev rec (SS:ACTDIR[BP] contains PPN to find)
; ax	returns starting DCN of dir
;
;-
rsfd:	cmp	ss:rdslev[bp],0	;RDS level 0.0?
	jnz	rsfd6		;no
	; RDS 0.0 -- search MFD for UFD
;;; should be able clear ECACHE and use RSLINK
	mov	ax,1		;start at MFD root
	mul	ss:dcs[bp]	;*DCS to get starting block
	mov	ds:rsblk,ax	;save
	mov	ds:rsblk+2,dx
	mov	cx,1		;count=1
	mov	di,offset buf	;pt at buffer
	call	ss:rdblk[bp]	;read in dir block
	jc	rsfd5		;error
rsfd1:	; check next entry (SI=curr blockette ptr, link in first word)
	lodsw			;get link
	and	ax,not 17	;end?
	jz	rsfd4		;yes, not found
	mov	bl,ah		;get cluster #
	mov	si,ax		;save
	and	bx,7*2		;isolate
	mov	ax,word ptr ds:buf+762[bx] ;look up DCN
	mul	ss:dcs[bp]	;*DCS to get base block
	push	si		;save
	mov	cl,12d		;shift count
	shr	si,cl		;get blk within cluster
	add	ax,si		;add it in
	adc	dx,0
	cmp	ax,ds:rsblk	;see if that's the block we have
	jne	rsfd2
	cmp	dx,ds:rsblk+2
	je	rsfd3		;yes, no need to re-read
rsfd2:	mov	ds:rsblk,ax	;update
	mov	ds:rsblk+2,dx
	mov	cx,1		;count=1
	mov	di,offset buf	;pt at buffer
	call	ss:rdblk[bp]	;read
	jc	rsfd5		;error
rsfd3:	pop	si		;catch blockette ptr
	and	si,37*20	;isolate offset
	add	si,offset buf	;index into buf
	test	byte ptr [si+10],100 ;check USTAT byte to see if UFD
	jz	rsfd1		;no, loop
	mov	ax,[si+2]	;get PPN
	cmp	ax,ss:actdir[bp] ;is this it?
	jne	rsfd1		;loop if not
	mov	ax,[si+16]	;get starting DCN
;;; Mayfield manual says that DCN may be 0 on freshly created account,
;;; if that's true what we should really do is read the accounting entry
;;; to get the correct size for DIRCLU (and clear out DIRWND) so that RSCR
;;; will add cluster(s) to the UFD correctly
if 0
	mov	ax,[si+14]	;link to accounting entry
;;;	call	rslink		;(I forget why this isn't OK in this routine)
				;(dir window isn't loaded for one thing???)
	mov	ax,[si+16]	;get dir cluster size
	mov	ss:dirclu,ax
;;; clear out all of DIRWND
endif
	ret
rsfd4:	jmp	dnf		;dir not found
rsfd5:	jmp	dirio		;dir read error
rsfd6:	; RDS 1.1 or later -- look up through MFD and GFD
	mov	ax,ss:mfddcn[bp] ;get starting DCN of MFD
	mul	ss:dcs[bp]	;*DCS to get blk #
	mov	cx,1		;read 1 blk
	add	ax,cx		;+1 to starting DCN map (clu guaranteed >=4)
	adc	dx,0
	mov	di,offset buf	;pt at buffer
	call	ss:rdblk[bp]	;read the block
	jc	rsfd5
	mov	bl,byte ptr ss:actdir+1[bp] ;get project #
	xor	bh,bh		;BH=0
	add	bx,bx		;*2
	mov	ax,[si+bx]	;look up base DCN of GFD
	test	ax,ax		;is there any?
	jz	rsfd4		;no
	mul	ss:dcs[bp]	;*DCS to get blk #
	mov	cx,1		;read 1 blk
	add	ax,cx		;+1 to starting DCN map (clu guaranteed >=4)
	adc	dx,0
	mov	di,offset buf	;pt at buffer
	call	ss:rdblk[bp]	;read the block
	jc	rsfd5
	mov	bl,byte ptr ss:actdir[bp] ;get programmer #
	xor	bh,bh		;BH=0
	add	bx,bx		;*2
	mov	ax,[si+bx]	;look up base DCN of UFD
	test	ax,ax		;is there any?
	jz	rsfd4
	ret
;
rsdn:	; display RSTS dir name
	mov	al,'['		;[
	stosb
	mov	al,byte ptr ss:actdir+1[bp] ;proj
	call	pbyte
	mov	al,','		;,
	stosb
	mov	al,byte ptr ss:actdir[bp] ;prog
	call	pbyte
	mov	al,']'		;]
	stosb
	ret
;
rscd:	; change RSTS dir
	call	rsgd		;get dir
	call	confrm		;make sure confirmed
	mov	ax,ss:actdir[bp] ;get active dir, make current
	mov	ss:curdir[bp],ax
	ret
;
rsgd:	; get dir name
	jcxz	rsgd5		;nothing, skip
	mov	al,[si]		;get next char
	mov	dl,']'		;(closing bracket)
	cmp	al,'['		;open bracket?
	je	rsgd1
	mov	dl,')'		;(closing paren)
	cmp	al,'('		;open paren?
	jne	rsgd3		;no
rsgd1:	inc	si		;yes, eat it
	dec	cx
	push	dx		;(save)
	call	gbyte		;get project
	pop	dx		;(restore)
	mov	bh,ah		;save
	cmp	al,','		;comma?
	jne	rsgd2
	inc	si		;yes, eat it
	dec	cx
	push	dx		;(save)
	call	gbyte		;get programmer
	pop	dx		;(restore)
	mov	bl,ah		;save
	cmp	al,dl		;close bracket?
	je	rsgd4		;yes
rsgd2:	jmp	synerr
rsgd3:	; look for one-character logicals -- !@#$%&
	mov	bh,byte ptr ss:curdir+1[bp] ;[proj,0]
	xor	bl,bl
	cmp	al,'#'		;#
	je	rsgd4
	mov	bx,102h		;[1,2]
	cmp	al,'$'		;$
	je	rsgd4
	mov	bx,103h		;[1,3]
	cmp	al,'!'
	je	rsgd4
	mov	bx,104h		;[1,4]
	cmp	al,'%'		;%
	je	rsgd4
	mov	bx,105h		;[1,5]
	cmp	al,'&'		;&
;;; we won't support '@', the ASSIGNable PPN
	jne	rsgd5
rsgd4:	mov	ss:actdir[bp],bx ;save
	inc	si		;eat delimiter
	dec	cx
rsgd5:	; find dir
	push	si		;save
	push	cx
	call	rsfd		;find directory
	mul	ss:dcs[bp]	;*DCS to get blk #
	mov	cx,1		;block count=1
	mov	di,offset buf	;pt at buf
	call	ss:rdblk[bp]	;read starting dir block (should be cached!)
	jc	rsgd6
	add	si,760		;point at clu, retrieval list
	lea	di,ss:dirclu[bp] ;pt into dev rec
.assume <dirwnd eq dirclu+2>	;assume same order as in dir
	mov	cx,8d		;copy 8 words
	rep	movsw
	and	ss:dirclu[bp],37 ;trim to 5 bits (lose flag in b15)
;; are larger cluster sizes reflected here or not?
;; dir clusters never have more than 16 valid blocks, even if the PCS is more
	pop	cx		;restore
	pop	si
	ret
rsgd6:	jmp	dirio
;+
;
; Parse RSTS filename and directory.
;
; RSTS allows the PPN to come either before or after the filename.
;
;-
rsgf:	; get filename
	push	bx		;save
	push	di
	call	skip		;skip white space
	push	si		;save
	call	rsgd		;get directory
	pop	ax		;restore starting ptr
	pop	di		;restore
	pop	bx
	sub	ax,si		;NZ if did parse dir name
	push	ax		;save
	call	filnam		;parse filename
	pop	ax		;restore flag
	test	ax,ax		;did we already get a dir name?
	jnz	rsgf1		;yes
	callr	rsgd		;no, try now, return
rsgf1:	ret
;
rsdd:	; display RSTS dir entry
	mov	ax,ds:fsize	;size
	mov	dx,ds:fsize+2	;high order
	inc	ds:nfile	;bump # files
	add	ds:nblk,ax	;bump # blocks
	adc	ds:nblk+2,dx
	mov	cx,8d		;field size
	call	cnum		;print it right-justified
	mov	dx,di		;save
	mov	ax,"  "		;pad field out to 3 blanks
	stosw
	stosb
	xchg	dx,di		;restore, save
	mov	ah,ds:fattr	;get USTAT byte
	mov	al,'C'		;Contiguous?
	test	ah,20
	jz	rsdd1
	 stosb			;yes
rsdd1:	mov	al,'P'		;Protected?
	test	ah,40
	jz	rsdd2
	 stosb			;yes
rsdd2:	mov	al,'L'		;Located?
	test	ah,2
	jz	rsdd3
	 stosb			;yes
rsdd3:	mov	di,dx		;restore
	mov	ax,'<'		;<
	stosb
	mov	al,ds:fprot	;protection code (AH=0 from above)
	cwd			;DX=0
	mov	cx,3		;field size=3
	call	cnum		;print prot code
	mov	al,'>'		;>
	stosb
	test	byte ptr ds:dirflg,200 ;verbose?
	jz	rsdd4		;no
	mov	al,' '		;blank
	stosb
	mov	ax,ds:idla	;get date of last access
	call	dosdat		;convert
	mov	ax,cx		;copy
	call	pdate1		;print it
rsdd4:	mov	al,' '		;blank
	stosb
	call	pdate		;date of creation
	test	byte ptr ds:dirflg,200 ;verbose?
	jz	rsdd8		;no
	mov	al,' '		;blank
	stosb
	call	ptime		;time of creation
	mov	ax,ds:ifcs	;file cluster size
	cwd			;DX=0
	mov	cx,4		;field size
	call	cnum		;convert
	mov	al,' '		;blank
	stosb
	mov	ax,ds:irts	;get RTS name
	test	ax,ax		;skip it if large file
	jz	rsdd6
	call	rad50		;convert
	mov	ax,ds:irts+2
	call	rad50
	jmp	short rsdd6
rsdd5:	cram	'------'	;no RTS name for large files
rsdd6:	mov	ax,ds:fsize	;null file?
	or	ax,ds:fsize+2
	jz	rsdd7
	mov	ax,ds:iretr	;get retrieval pointer
	push	di		;save
	call	rslink		;fetch
	pop	di		;[restore]
	jc	rsdd9		;corrupt dir
	inc	si		;skip link
	inc	si
	lodsw			;get ptr
	xor	dx,dx		;0-extend
	mov	cx,6		;field size
	callr	cnum		;print, return
rsdd7:	cram	' -----'	;no posn for null files
rsdd8:	ret
rsdd9:	jmp	baddir		;missing retrieval list, corrupt dir
;+
;
; Get RSTS dir entry.
;
; UFD entry format:
;
; +0	.word	next	;link to next (!2=contains bad, !4=cacheable)
; +2	.rad50	/FILNAM/ ;filename
; +6	.rad50	/EXT/	;extension
; +10	.byte	ustat	;USTAT byte, bitmapped flags:
;			;1	(US.OUT) obsolete -- dunno what it did
;			;2	(US.PLC) placed
;			;4	(US.WRT) opened for write
;			;10	(US.UPD) opened for update
;			;20	(US.NOX) contiguous (non-extendable)
;			;40	(US.NOK) protected (non-killable)
;			;100	(US.UFD) UFD (appears only in [1,1] on RDS 0.0)
;			;200	(US.DEL) marked for deletion
; +11	.byte	prot	;protection code
;			;!1=read prot against owner (UP.RPO), or exec if !100
;			;!2=write prot against owner (UP.WPO), or R/W if !100
;			;!4=read prot against proj (UP.RPG), or exec if !100
;			;!10=write prot against proj (UP.WPG), or R/W if !100
;			;!20=read prot against world (UP.RPW), or exec if !100
;			;!40=write prot against world (UP.WPW), or R/W if !100
;			;!100=executable (UP.RUN), modifies others as above
;			;!200=zero first on delete, priv'ed if !100 (UP.PRV)
; +12	.byte	count	;access count (for system files only), 0 otherwise
; +13	.byte	res	;reserved (these used to be OP/RR in RDS 0.0)
; +14	.word	acc	;link to accounting blockette (!4=seq cach if NEXT&4)
; +16	.word	retr	;link to first retrieval blockette, or 0 if size=0
;
;-
rsdg:	; get RSTS dir entry
	mov	ax,ds:inent	;get ptr to next entry
	call	rslink		;follow it
	jnc	rsdg1
	 ret
rsdg1:	lodsw			;fetch link to next
	xchg	ax,ds:inent	;save link, get old value
	xchg	ax,ds:oinent	;save link to this entry, get prev
	mov	ds:oinent+2,ax	;save prev
	mov	di,offset lbuf	;pt at LBUF
	test	byte ptr [si-2+10],100 ;file or UFD?
	jnz	rsdg		;UFD, ignore (possible only in RDS 0.0 [1,1])
	lodsw			;FILNAM
	call	rad50
	lodsw
	call	rad50
	mov	al,'.'		;.
	stosb
	lodsw			;EXT
	call	rad50
	lodsw			;get UPROT,,USTAT
	mov	word ptr ds:fattr,ax ;save
	inc	si		;skip access count or open/read regardless
	inc	si
	lodsw			;get link to accounting entry
	mov	bx,ax		;save
	lodsw			;get link to first retrieval (or 0)
	mov	ds:iretr,ax	;save
	mov	ax,bx		;restore
	call	rslink		;follow link to accounting stuff
	jc	rsdg3		;error, must always be present
	lodsw			;get link to attribute stuff
	mov	ds:iattr,ax	;save
	lodsw			;get DLA
	mov	ds:idla,ax
	lodsw			;get low order size
	mov	ds:fsize,ax	;save
	mov	ds:fsize+2,0	;assume high order is 0
	lodsw			;get DOC
	push	si		;save
	call	dosdat		;convert
	pop	si		;restore
	mov	ds:fyear,dx	;store date
	mov	word ptr ds:fday,cx
	mov	ds:fdatf,al	;set flag
	lodsw			;get TOC
	; low 11 bits of RSTS time are # minutes until midnight
	and	ah,3777/100h	;trim to 11 bits in case V10.0+ pack
	neg	ax		;make that minutes *since* midnight
	add	ax,24d*60d
	mov	bl,60d		;separate hours from minutes
	div	bl
	xchg	al,ah		;get hour,,minute
	mov	word ptr ds:fmin,ax ;save
	mov	byte ptr ds:fsec,-1 ;don't know second
	mov	byte ptr ds:ftimf,1 ;time is valid
	lodsw			;get 1st word of RTS name
	mov	ds:irts,ax
	test	ax,ax		;is it 0?
	lodsw			;[get 2nd word]
	jnz	rsdg2
	mov	ds:fsize+2,ax	;yes, high byte (or word) of large file size
	xor	ax,ax		;shoot out 2nd word of RTS
rsdg2:	mov	ds:irts+2,ax	;save
	lodsw			;get FCS
	mov	ds:ifcs,ax	;save
	; calculate size of file in bytes (including NULs if text file)
;;; it might be nice to use RMS attributes if there are any, to find out
;;; the number of significant bytes in the last block
	mov	cl,9d		;shift count
	mov	ax,ds:fsize	;get it
	mov	dx,ds:fsize+2
	sal	dx,cl		;*512.
	rol	ax,cl
	mov	bx,ax		;copy middle bits
	and	bh,777/100h	;trim to 9 bits
	or	dx,bx
	and	ax,not 777	;clear out low 9 bits
	mov	ds:fbcnt,ax	;save
	mov	ds:fbcnt+2,dx
	mov	di,offset lbuf+10d ;restore DI
	callr	sqfn		;squish filename, return
rsdg3:	jmp	baddir		;corrupt dir
;+
;
; Convert RSTS/DOS-11 date to date, month, year.
;
; RSTS and DOS/BATCH dates are 16-bit words containing the year minus 1970.
; times 1000., plus the day within the year.
;
; ax	DOS-11 date
;
; On return:
; cl	day
; ch	month
; dx	year
; al	flag (NZ => date is valid)
;
;-
dosdat:	xor	dx,dx		;0-extend
	mov	bx,1000d	;split year from month/day
	div	bx
	add	ax,1970d	;add base year
	mov	cx,dx		;catch remainder
	mov	dx,ax		;copy year
	mov	ax,28d		;# days in Feb
	test	dl,3		;leap year?  (2000. AD counts)
	jnz	dsdat1
	 inc	ax		;yes
dsdat1:	mov	ds:feb,al	;save
	add	ax,365d-28d	;find length of year
	cmp	cx,ax		;is it possible?
	ja	dsdat3		;no
	jcxz	dsdat3		;can't be 0 either
	mov	si,offset jan	;point at year table
	xor	ah,ah		;AH=0
dsdat2:	lodsb			;get month
	sub	cx,ax		;subtract
	ja	dsdat2		;loop unless 0 or carry
	add	cx,ax		;correct
	sub	si,offset jan	;find month-1, +1
	mov	ax,si		;copy
	mov	ch,al		;copy again
	mov	al,1		;valid
	ret
dsdat3:	xor	al,al		;not valid
	ret
;
rsdf:	; RSTS dir finish (flush dirty dir buffers)
	mov	si,ss:dirbuf[bp] ;get ptr to buf chain
rsdf1:	call	rswdir		;flush
	mov	si,ds:rdsnxt[si] ;follow chain
	test	si,si		;anything?
	jnz	rsdf1		;loop if so
	ret
;
rssum:	; RSTS dir listing summary
	mov	di,offset lbuf	;pt at buf
	call	flush		;blank line
	cram	'Total of '
	mov	ax,ds:nblk	;get # blocks
	mov	dx,ds:nblk+2
	push	dx		;save
	push	ax
	call	pdnum		;display 32-bit number
	cram	' block'
	pop	ax		;restore
	pop	dx
	call	dadds		;add s
	cram	' in '
	mov	ax,ds:nfile	;get # files
	push	ax		;save
	call	pnum		;print
	cram	' file'
	pop	ax		;restore
	call	adds		;add s
	cram	' in '
	call	pdev		;print dev name
	call	rsdn		;and dir name
	callr	flush		;flush, return
;
rsop:	; open RSTS file for input
	mov	ax,ds:fsize	;get size
	mov	ds:isize,ax	;save
	mov	ax,ds:fsize+2
	mov	ds:isize+2,ax
	mov	ds:irptr,8d*2	;init retrieval ptr to follow link to first
	ret
;+
;
; Read file data.
;
; On entry:
; es:di	ptr to buf (normalized)
; cx	# bytes free in buf (FFFF if >64 KB)
;
; On return:
; cx	# bytes read (may be 0 in text mode if block was all NULs etc.)
;
; CF=1 if nothing was read:
;  ZF=1 end of file
;  ZF=0 buffer not big enough to hold anything, needs to be flushed
;
;-
rsrd:	push	di		;save starting ptr
rsrd1:	; DI=addr, CX=length
	push	cx		;save # free bytes left in buf
	mov	bx,-1		;assume >64 K blocks
	test	ds:isize+2,bx	;is it?
	jnz	rsrd5		;yes
	and	bx,ds:isize	;get remaining size
	jnz	rsrd5		;not EOF
rsrd2:	; can't fit anything more in buf (DI pts past what we've read)
	pop	ax		;flush free count in buf
	mov	cx,di		;copy end of stuff read
	pop	si		;restore ptr to begn
	sub	cx,si		;get length (CF=0)
	jz	rsrd3		;got nothing, skip
	ret
rsrd3:	; read nothing, see if EOF
	mov	ax,ds:isize	;get size
	or	ax,ds:isize+2	;ZF=0 if not EOF
	stc			;unhappy return
	ret
rsrd4:	jmp	baddir		;retrieval list ran out before EOF
rsrd5:	shr	ch,1		;get # full blocks free in buf
	jz	rsrd2		;can't hold even one block
	mov	cl,ch		;copy
	xor	ch,ch		;zero-extend
	cmp	cx,bx		;more than what's left?
	jb	rsrd6
	 mov	cx,bx		;stop at end
rsrd6:	; read next cluster
	mov	bx,ds:irptr	;get retrieval ptr
	cmp	bx,8d*2		;off end of window?
	jb	rsrd7		;no
	; window turn
	push	cx		;save
	push	di
	push	es
	push	ds		;copy DS to ES
	pop	es
	mov	ax,ds:iretr	;get link to next
	call	rslink		;follow it
	jc	rsrd4		;shouldn't happen!
	mov	di,offset iretr	;point at retrieval window buf
	mov	cx,8d		;blockette size
	rep	movsw		;copy
	mov	ds:ioff,cx	;start at beginning of cluster
	pop	es		;restore
	pop	di
	pop	cx
	mov	bx,1*2		;point at first retrieval word
rsrd7:	; CX = # blocks we can read
	mov	ax,ds:iretr[bx]	;look up retrieval word
	mul	ss:dcs[bp]	;*DCS to get starting blk #
	add	ax,ds:ioff	;add offset within cluster
	adc	dx,0
	mov	si,ds:ifcs	;get input file cluster size
	sub	si,ds:ioff	;subtract # blocks already read
	cmp	cx,si		;will our request finish the cluster?
	jb	short rsrd8	;no, skip
	mov	cx,si		;yes, stop at end
	inc	bx		;+2 to next retrieval pointer
	inc	bx
	mov	word ptr ds:ioff,0 ;start at beginning of next clu
	jmp	short rsrd9
rsrd8:	add	ds:ioff,cx	;update offset within clu
rsrd9:	mov	ds:irptr,bx	;update
	sub	ds:isize,cx	;subtract from size read
	sbb	ds:isize+2,0
	call	ss:rdblk[bp]	;read data
	jc	rsrd12		;error, skip
	mov	di,si		;pt past data
	add	di,cx
	cmp	ds:binflg,0	;binary mode?
	jz	rsrd11		;yes
	; text mode, remove NULs
	mov	di,si		;pt at data
	mov	dx,di		;save starting posn
rsrd10:	lods	byte ptr es:[si] ;get a byte
	test	al,al		;NUL?
	jz	$+3
	 stosb			;save if not
	loop	rsrd10		;loop
	mov	cx,di		;find # bytes written
	sub	cx,dx
rsrd11:	; CX=# bytes just read, DI=first free buf loc
	mov	ax,cx		;copy
	pop	cx		;restore # that were free
	sub	cx,ax		;find # to go
	jmp	rsrd1		;around for next file cluster
rsrd12:	jmp	rderr		;read error
;
rsdo:	; RSTS dir output init

;;; punt in flames if pack needs CLEANing
;;; (iff RSTS -- might be ODS since it shares this entry)

	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	rsdo1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir blk buffer (nothing is possible if this fails)
	mov	cx,rdslen	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:rdsbkl[si],ax ;nothing in here yet
	mov	ds:rdsbkh[si],ax
	mov	ds:rdsdrt[si],al ;not dirty
	mov	ds:rdsnxt[si],ax ;no next
rsdo1:	ret
;+
;
; Create RSTS output file.
;
; ds:si	.ASCIZ filename
; DS:FBCNT  max size of file in bytes
;
;-
rscr:	; parse filename
	call	lenz		;get length
	call	grad		;get filename
	jc	rscr4
	cmp	al,'.'		;ends with dot, right?
	jne	rscr4		;no
	mov	ds:rtfile,bx	;save filename
	mov	ds:rtfile+2,dx
	inc	si		;eat the dot
	dec	cx
	call	grad		;get extension
	test	al,al		;ends at end of string, right?
	jnz	rscr4
;;; could act like RSTS .FSS sys call and allow switches like /CLU:n or /MODE:n
;;; or /SIZE:n as long as they're immediately next to the filename, but we'd
;;; need cooperation with the higher-level parser since switches have already
;;; been peeled off by the time we get here
	mov	ds:rtfile+4,bx	;save extension
	; find out if a protected file by this name already exists
	test	byte ptr ds:ovride,-1 ;override protection?
	jnz	rscr1		;yes, so no need to check
	xor	ax,ax		;nothing created yet
	call	rssame		;does file exist with same name?
	jc	rscr1		;no
	test	byte ptr [si+10],40 ;US.NOK set (protected file)?
	jz	rscr1		;no, good
	 jmp	prtfil		;can't overwrite protected file
rscr1:	; find out how many FCs this file will need
	mov	bx,ss:pcs[bp]	;get default cluster size
	mov	cx,ds:clusiz	;get /CLUSTERSIZE value
	jcxz	rscr2
	mov	si,cx		;copy
	dec	si		;power of 2?
	and	si,cx
	jnz	rscr2		;no, ignore
	cmp	cx,bx		;.GE.PCS?
	jb	rscr2		;no
	cmp	cx,256d		;.LE.256?
	ja	rscr2		;no
	 mov	bx,cx		;yes, use it
rscr2:	mov	ds:ofcs,bx	;set output FCS
	mov	si,ds:fbcnt	;get byte count into AX:SI
	mov	ax,ds:fbcnt+2
	add	si,777		;round up to block boundary
	adc	ax,0
	mov	cl,9d		;shift count
	shr	si,cl		;right 9 bits
	ror	ax,cl
	mov	dx,ax		;copy
	and	dl,200		;isolate high 9 bits
	or	si,dx		;combine with LSW
	xor	ax,dx		;clear out of MSW
	mov	ds:oblk,si	;save
	mov	ds:oblk+2,ax
	cwd			;zero-extend (AX<15:7> know 0 from XOR)
	div	bx		;find high-order word of clu count
	test	ax,ax		;must be zero
	jz	rscr5		;OK
rscr3:	jmp	odfull		;full
rscr4:	jmp	badnam		;bad filename
rscr5:	mov	ax,si		;get low order byte count
	div	bx		;find low-order word of clu count
	add	dx,-1		;CF=1 if remainder NZ
	adc	ax,0		;add partial cluster if any
	jc	rscr3		;too much
	mov	ds:osize,ax	;save # FCs needed, for blockette allocation
	mul	bx		;convert to # PCs
	div	word ptr ss:pcs[bp]
	mov	ds:osize+2,ax	;save that for free space search
	; compute # dir blockettes needed for retrieval list (7 FCs each)
	add	ax,7-1		;round up to multiple of 7
	sbb	dx,dx		;catch overflow
	neg	dx		;1 if overflowed
	mov	bx,7		;divisor
	div	bx		;find # blockettes
	inc	ax		;+1 for the name entry itself
	inc	ax		;+1 again for accounting entry
	mov	dx,ax		;copy
	mov	word ptr ds:onew,0 ;assume we're not extending the UFD
	call	rsslot		;find that many free dir slots
	jnc	rscr9		;no problem, already have enough
	; not enough space in directory, find out how many blocks we need to
	; extend it by
	mov	ax,dx		;get # blockettes we need
	add	ax,31d-1	;round to multiple of 31.
	sbb	dx,dx		;all ones if overflowed
	neg	dx		;change to +1
	mov	bx,31d		;find # blocks needed
	div	bx
	; round that up to the # of clusters
	mov	bx,ss:dirclu[bp] ;get clustersize
	add	ax,bx		;round past next cluster size
	dec	ax		;(unless already cluster multiple)
	cwd			;zero-extend (AX<15>=0 after dividing by 31.)
	div	bx		;find # full clusters to add
	; make sure the directory has space for that many (max is 7 due to
	; 8-word clustersize/retrieval list at offset 760 of every block
	mov	cx,ax		;copy
	mov	si,6d*2		;point at last dir retrieval ptr
rscr6:	test	word ptr ss:dirwnd[bp+si],-1 ;empty slot?
	jnz	rscr7		;no
	dec	ax		;count it
	jz	rscr8		;happy
	dec	si		;back up one
	dec	si
	jnz	rscr6		;loop
rscr7:	jmp	dirful
rscr8:	call	rsanew		;add new dir cluster(s) now
rscr9:	; scan disk to make sure we have at enough free space
	mov	si,ds:osize+2	;get # PCs needed
	test	si,si		;if any
	jz	rscr12		;null file
	xor	cx,cx		;start scanning at cluster 0
rscr10:	push	si		;save
	call	rsfree		;find next free area
	pop	si		;[restore]
	jc	rscr13		;EOD or error, skip
	mov	ax,ds:ofcs	;get file cluster size
	dec	ax		;mask LSBs
	not	ax		;MSBs
	and	ax,dx		;round size of free space down to FCS mult
	add	cx,dx		;bump clu to end of free area
	test	byte ptr ds:contig,-1 ;looking for contiguous space?
	jz	rscr11		;nope, skip
	cmp	ax,si		;big enough for whole file?
	jb	rscr10		;no (otherwise drop through)
rscr11:	sub	si,ax		;count off of total
	ja	rscr10		;loop if not enough
rscr12:	; we have space for everything
	xor	ax,ax		;load 0
	mov	ds:oretr0,ax	;no retrieval chain at all yet
	mov	ds:orptr,ax	;init ptr into first chain
	mov	di,offset oretr	;point at chain
	mov	cx,8d		;# words
	rep	stosw		;clear it out
	mov	ds:onfree,ax	;no free area yet
	mov	ds:ooff,ax	;init offset in first cluster
	ret
rscr13:	call	rsfnew		;free the new cluster(s) we added, if any
	jmp	odfull		;output device is full
;+
;
; Find any file with the same name as the one we're creating.
;
; ax	link word pointing at newly created name blockette (match to this entry
;	*doesn't* count), or 0 if none yet
;
; On return, CF=1 if not found, otherwise:
; ds:si	name blockette of the file
; ds:bx	base of block buffer containing blockette
; dx	link word pointing to the preceding blockette (which links to this one)
;
;-
rssame:	and	al,not 17	;clear LSBs of link word
	push	ax		;save
	xor	ax,ax		;start at beginning
	call	rslnkn		;get UFD label
	pop	di		;catch link to new file (or 0)
	xor	cx,cx		;prev=UFD label
rssm1:	; get next blockette, DS:SI=current, CX=its addr as link word, DI=new
	push	di		;save new file
	push	cx		;link to current blockette
	lodsw			;follow link
	and	al,not 17	;clear LSBs
	push	ax		;save
	call	rslink		;link to next
	pop	cx		;[restore new link]
	pop	dx		;[previous link]
	pop	di		;[link to new file]
	jc	rssm2		;end of chain, not found
	; DS:SI=new, CX=new link, DX=prev link, DI=new file
	cmp	cx,di		;is this the new one?  (can't match if DI=0)
	je	rssm1		;yes, skip it
	mov	ax,[si+2]	;check FILNAM.EXT for match
	cmp	ax,ds:rtfile
	jne	rssm1
	mov	ax,[si+4]
	cmp	ax,ds:rtfile+2
	jne	rssm1
	mov	ax,[si+6]
	cmp	ax,ds:rtfile+4	;(CF=0 if matches)
	jne	rssm1
rssm2:	ret
;+
;
; Write data to output file.
;
; es:si	buffer
; cx	length of data
;
; On return, CX contains the number of bytes actually written (block multiple).
;
;-
rswr:	mov	cl,ch		;right 9 bits to get block count
	xor	ch,ch		;(zero-extend)
	shr	cl,1
	push	si		;save starting addr
	jmp	short rswr4
rswr1:	push	es		;save
	push	si
	push	cx
	push	ds		;copy DS to ES
	pop	es
	call	rsnext		;get some free space
	pop	cx		;restore
	pop	si
	pop	es
	jmp	short rswr5	;back in
rswr2:	jmp	wrerr		;write error
rswr3:	mov	cx,si		;get ending address
	pop	si		;restore starting addr
	sub	cx,si		;find length
	ret
rswr4:	; write next segment of buffer (ES:SI=pointer, CX=remaining blk count)
	jcxz	rswr3		;nothing left to do
	test	word ptr ds:onfree,-1 ;do we have some free space lined up?
	jz	rswr1		;no, go get some
rswr5:	mov	ax,ds:onfree	;get # free clusters
	mov	bx,ds:ofcs	;cluster size
	dec	ax		;-1 (first one may be partially used)
	mul	bx		;find # blocks available as whole FCs
	sub	bx,ds:ooff	;# blocks available in first FC
	add	ax,bx		;add to total
	adc	dx,0
	mov	bx,cx		;save requested count
	test	dx,dx		;can we do that for sure?
	jnz	rswr6		;yes
	cmp	cx,ax		;do we need the whole free area?
	jb	rswr6		;nope, keep what we've got
	 mov	cx,ax		;yes, cut our request short
rswr6:	mov	ah,cl		;get actual block count
	add	ah,ah		;left 9 bits (=actual byte count)
	xor	al,al
	add	ax,si		;find buf addr after transfer
	sub	bx,cx		;and remaining block count after transfer
	push	ax		;save new buf addr
	push	bx		;new blk count
	; actually write the data (easiest part)
	push	cx		;blk count for this segment
	mov	ax,ds:ofree	;get PCN of first free FC
	mul	word ptr ss:pcs[bp] ;find starting block # of FC
	add	ax,ss:dcs[bp]	;skip DCN 0
	adc	dx,0
	add	ax,ds:ooff	;add starting offset in this cluster
	adc	dx,0		;DX:AX=starting block #
	call	ss:wrblk[bp]	;write the blocks
	jc	rswr2		;failed
	pop	cx		;restore
	; figure out how many file clusters have been completely filled
	mov	ax,cx		;get # blocks written (known .LE. 64 KB worth)
	add	ax,ds:ooff	;add starting offset (can't carry)
	xor	dx,dx		;zero-extend
	div	word ptr ds:ofcs ;AX = # of FCs we've finished
	mov	ds:ooff,dx	;remainder is offset into the FC after that
	test	ax,ax		;did we completely fill any clusters at all?
	jz	rswr8		;no
	push	es		;save ES
	push	ds		;copy DS to ES during dir manipulation
	pop	es
	; allocate newly completed file cluster(s) in [0,1]SATT.SYS
	push	ax		;save FC count
	mul	word ptr ds:ofcs ;find # blocks to fill
	div	word ptr ss:pcs[bp] ;make that # PCs
	mov	dx,ax		;copy
	mov	cx,ds:ofree	;get starting PCN
	call	rssbit		;set bit(s) in SATT.SYS
	pop	cx		;restore FC count
	; add newly completed FC(s) to retrieval list
	sub	ds:onfree,cx	;update # free FCs
	sub	ds:osize,cx	;and deduct from file size
rswr7:	; add next FC, CX=count
	mov	ax,ds:ofree	;get starting PCN
	mov	ss:satptr[bp],ax ;save
	mul	word ptr ss:pcsdcs[bp] ;convert PCN to DCN
	inc	ax		;skip DCN 0
	push	cx		;save
	call	rsaret		;add to retrieval list
	pop	cx		;restore
	mov	ax,ds:ofcs	;get FCS
	xor	dx,dx		;zero-extend
	div	word ptr ss:pcs[bp] ;find # PCs per FC
	add	ds:ofree,ax	;update
	loop	rswr7		;loop through all
	pop	es		;restore buf seg ptr
rswr8:	pop	cx		;# blocks left to do
	pop	si		;buf ptr
	jmp	rswr4		;loop
;+
;
; Obtain next free area into which file data will be written.
;
; DS:OFREE returns the starting PCN of the region
; DS:ONFREE returns the # of FCs in the region
; Note the difference in units.
;
;-
rsnext:	mov	ax,ds:ofcs	;get file cluster size
	call	rsfclu		;find free cluster
	jc	rsnxt3		;EOD
	test	byte ptr ds:contig,-1 ;contiguous output file?
	jnz	rsnxt2		;yes
rsnxt1:	mov	ds:ofree,cx	;save starting PCN
	mov	cx,ax		;copy FCS in PCs
	mov	ax,dx		;get # PCs in free area
	xor	dx,dx		;zero-extend
	div	cx		;find # FCs (known NZ from RSFCLU)
	mov	ds:onfree,ax	;save
	ret
rsnxt2:	cmp	dx,ds:osize+2	;is area big enough for whole file?
	jae	rsnxt1		;yes
	add	cx,dx		;no, skip to end
	dec	cx		;last cluster of free area
	mov	ss:satptr[bp],cx ;save
	jmp	short rsnext	;try again with next free area
rsnxt3:	; didn't have free space on disk, even though we checked earlier?!
	call	rsfnew		;free clusters added to dir, if any
	jmp	odfull		;no space (still have a half-added file!)
;+
;
; Add a cluster to the retrieval list.
;
; ax	DCN to add
;
;-
rsaret:	mov	bx,ds:orptr	;get ptr
	inc	bx		;+2
	inc	bx
	mov	ds:oretr[bx],ax	;add new DCN
	cmp	bx,7*2		;is it full now?
	je	rsar1		;yes
	mov	ds:orptr,bx	;save
	ret
rsar1:	; flush the retrieval blockette
	mov	dx,1		;need 1 blockette
	call	rsslot		;look up next free blockette
	jc	rsar4
	mov	cx,ax		;save link word to this blockette, for below
	; copy retrieval window into dir buffer
	add	bx,si		;index to start of retrieval blockette
	xor	di,di		;init offset
	mov	ds:orptr,di	;(rewind pointer)
rsar2:	xor	ax,ax		;load 0
	xchg	ax,ds:oretr[di]	;fetch a DCN, set to 0
	mov	[bx+di],ax	;save
	inc	di		;+2
	inc	di
	cmp	di,8d*2		;done all 8 words?
	jb	rsar2		;loop if not
	mov	byte ptr ds:rdsdrt[si],1 ;mark buffer as dirty
	sub	bx,si		;restore offset within block
	; save this location so we can link to next retrieval blockette
	mov	ax,ds:rdsbkl[si] ;fetch abs block # of blk we just added to
	mov	dx,ds:rdsbkh[si]
	xchg	ax,ds:opretr	;save, get blk # of previous window
	xchg	dx,ds:opretr+2
	xchg	bx,ds:opretr+4	;(same for offset)
	test	word ptr ds:oretr0,-1 ;is this the begn of the retrieval list?
	jz	rsar3		;yes, just save it
	; re-read block containing previous retrieval blockette, link to it
	push	cx		;save
	push	bx
	xor	cl,cl		;really read it
	call	rsrdir		;read dir block
	pop	bx		;restore offset
	pop	word ptr [bx+si] ;catch link word
	mov	byte ptr ds:rdsdrt[si],1 ;mark buffer as dirty
	ret
rsar3:	mov	ds:oretr0,cx	;save link to start of chain
	ret
rsar4:	; didn't have free space in dir, even though we checked earlier?!
	call	rsfnew		;free clusters added to dir, if any
	jmp	odfull		;no space (still have a half-added file!)
;+
;
; Write last partial block to file.
;
; es:si	data
; cx	length of data
;
; N.B. ES is trashed.
;
;-
rswl:	jcxz	rswl1		;none, skip
	mov	bx,512d		;RSTS block size
	sub	bx,cx		;find # NULs to add (in [1,777])
	xchg	bx,cx		;swap
	lea	di,[bx+si]	;point past data
	xor	al,al		;load 0
	rep	stosb		;pad it out
	mov	cx,512d		;write a whole block
	call	rswr		;write it
rswl1:	; BIGBUF no longer in use, clear a block's worth and use it to pad to
	; FC boundary
	mov	es,ds:bigbuf	;pt at beginning
	xor	di,di
	xor	ax,ax		;load 0
	mov	cx,512d/2	;word count
	rep	stosw		;fill with 512 NULs
rswl2:	xor	si,si		;init offset (ES:SI => 512 NULs)
	cmp	ds:ooff,si	;reached file cluster boundary?
	jz	rswl3
	mov	cx,512d		;byte count
	call	rswr		;write it
	jmp	short rswl2	;loop
rswl3:	; pad last retrieval blockette and write it out
	xor	ax,ax		;load 0
	cmp	ds:orptr,ax	;at blockette boundary?
	jz	rswl4		;yes, done
	push	ds		;(copy DS to ES during dir manipulation)
	pop	es
	call	rsaret		;add the 0, flush if full
	jmp	short rswl3
rswl4:	ret
;+
;
; Close output file.
;
; ss:bp	log dev rec
;
;-
rscl:	; should create attributes first
;;; if this file has RMS attributes, RSCR should have reserved one or two
;;; extra blockettes for them, get link word into AX
	xor	ax,ax		;no attributes blockettes
	; create accounting entry for this file
	push	ax		;save link to attrs
	mov	dx,1		;need 1 blockette
	call	rsslot		;look up next free blockette
	pop	cx		;[restore attrs link]
	jc	rscl4
	push	ax		;save link word to this blockette
	mov	byte ptr ds:rdsdrt[si],1 ;buffer will be dirty
	add	bx,si		;index to blockette
	mov	[bx],cx		;+00 = link to attrs list (or 0 if none)
	; get RTS name
	mov	si,1343		;/ RS/
	mov	di,77770	;/TS /  (constant for now)
	; get length
	mov	ax,ds:oblk	;get file size in 512-byte blocks
	mov	dx,ds:oblk+2
	test	dx,dx		;large file?
	jz	rscl1		;no
	; large file, hack RTS name (do protection code later)
	xor	si,si		;nuke 1st word of RTS name
	mov	di,dx		;2nd word is MSW of file size
rscl1:	mov	[bx+4],ax	;+04 = file size (or LSW at least)
	mov	[bx+12],si	;+12 = RTS name
	mov	[bx+14],di
	mov	ax,ds:ofcs	;+16 = file cluster size
	mov	[bx+16],ax
	; translate date to RSTS form
	xor	ax,ax		;assume we don't know the date
	cmp	ds:fdatf,al	;do we know date?
	jz	rscl2		;no (should really just use today's date)
	push	bx		;save pointer
	call	encdos		;encode day and year
	mov	ax,1000d	;multiplier
	mul	dx
	add	ax,bx		;add day within year
	pop	bx		;restore point
rscl2:	mov	[bx+2],ax	;+02 = date of last access (or maybe DLW)
	mov	[bx+6],ax	;+06 = creation date
	; translate time to RSTS form
	xor	ax,ax		;assume we don't know the time
	cmp	ds:ftimf,al	;do we know time?
	jz	rscl3		;no
	mov	al,60d		;# minutes per hour
	mul	byte ptr ds:fhour ;find # minutes since midnight
	add	al,byte ptr ds:fmin ;add minute within hour
	adc	ah,0		;=minutes since midnight
	neg	ax		;make that minutes *until* midnight
	add	ax,24d*60d	;as an 11-bit number
;;; MSBs are used for something else in RDS V1.2
rscl3:	mov	[bx+10],ax	;+10 = creation time
	; accounting entry is finished, create name entry
	mov	dx,1		;need 1 blockette
	call	rsslot		;look up next free blockette
	pop	cx		;[restore accounting link]
	jnc	rscl5		;success (N.B. link word at +00 is already 0)
rscl4:	call	rsfnew		;free clusters added to dir, if any
	jmp	odfull		;no space (still have a half-added file!)
rscl5:	push	ax		;save link word to this blockette
	mov	byte ptr ds:rdsdrt[si],1 ;buffer will be dirty
	add	bx,si		;index to blockette
	mov	[bx+14],cx	;+14 = link to acct entry
	; copy filename
	mov	ax,ds:rtfile	;+02 = FILNAM
	mov	[bx+2],ax
	mov	ax,ds:rtfile+2
	mov	[bx+4],ax
	mov	ax,ds:rtfile+4	;+06 = EXT
	mov	[bx+6],ax
	; get protection, USTAT byte
	mov	ax,60d*400	;prot=<60>, USTAT=nothing
	cmp	ds:contig,al	;is it contiguous?  (AL=0)
	jz	rscl6
	 mov	al,20		;yes, set US.NOX
rscl6:	xor	dx,dx		;load 0
	cmp	ds:oblk+2,dx	;large file?
	jz	rscl7		;no
	 or	ah,64d		;yes, mark as executable (but invalid RTS)
rscl7:	mov	[bx+10],ax	;+10 = prot,,USTAT
	mov	[bx+12],dx	;+12 = access count (0)
	mov	ax,ds:oretr0	;get retrieval list, or 0
	mov	[bx+16],ax	;save
	; link name blockette into UFD
	xor	ax,ax		;get UFD label
	call	rslnkn
	test	byte ptr ss:paksts+1[bp],1000/400 ;UC.TOP (new files first)?
	jz	rscl8		;no, go link to end of chain
	pop	ax		;catch link to new blockette
	mov	dx,ax		;copy
	xchg	dx,[si]		;put ours first, get former head of list
	mov	byte ptr ds:rdsdrt[bx],1 ;buffer is dirty
	push	ax		;save again
	push	dx		;save link to new #2 (or 0 if null dir)
	call	rslink		;go find our new entry
	pop	word ptr [si]	;link rest of dir to us
	mov	byte ptr ds:rdsdrt[bx],1 ;buffer is dirty
	pop	ax		;restore link to ours
	jmp	short rscl10	;go delete matching file, return
rscl8:	test	word ptr [si],not 17 ;end of list?
	jz	rscl9		;yes
	lodsw			;follow link
	call	rslink
	jmp	short rscl8
rscl9:	pop	ax		;catch link to new blockette
	mov	[si],ax		;link it in here
	mov	byte ptr ds:rdsdrt[bx],1 ;buffer is dirty
rscl10:	; search for and delete any *other* file of the same name
	call	rssame		;find file with same name (AX=link to new file)
	jc	rscl11		;easy
	call	rsdel		;delete it
rscl11:	ret
;+
;
; Delete most recently DIRGETted file.
;
;-
rsdl:	mov	ax,ds:oinent	;get link to this entry
	call	rslink		;known to exist
	mov	dx,ds:oinent+2	;get link to predecessor
	;callr	rsdel		;delete, return
;+
;
; Delete a file.
;
; ss:bp	log dev rec
; ds:si	name blockette of the file
; ds:bx	base of block buffer containing blockette
; dx	link word pointing to the preceding blockette (which links to this one)
;
; This is the same as what's returned by RSSAME.
;
;-
rsdel:	; see if protected (already checked for COPY, but not if cmd=DELETE)
	test	byte ptr ds:ovride,-1 ;override protection?
	jnz	rsdel1		;yes, so no need to check
	test	byte ptr [si+10],40 ;US.NOK set (protected file)?
	jz	rsdel1		;no, good
	 jmp	prtfil		;can't overwrite protected file
rsdel1:	; free the name blockette
	push	word ptr [si+16] ;save first retrieval entry
	push	word ptr [si+14] ;link to accounting/attributes chain
	xor	ax,ax		;load 0
	mov	[si+2],ax	;mark slot as free
	xchg	ax,[si]		;fetch link to next
	mov	byte ptr ds:rdsdrt[bx],1 ;mark buf as dirty
	; unlink name blockette from chain
	push	ax		;save link
	mov	ax,dx		;fetch our predecessor
	call	rslnkn		;(might be UFD label if this is first file)
	pop	word ptr [si]	;link our successor to our predecessor
	mov	byte ptr ds:rdsdrt[bx],1 ;mark *this* buf as dirty
	; flush accounting/attributes chain
	mov	word ptr ds:ofcs,0 ;init OFCS
	pop	ax		;catch link
rsdel2:	call	rslink		;follow chain
	jc	rsdel3		;end, skip
	xor	ax,ax		;load 0
	mov	[si+2],ax	;mark slot as free
	xchg	ax,[si]		;fetch link to next
	mov	byte ptr ds:rdsdrt[bx],1 ;mark buf as dirty
	mov	dx,[si+16]	;get FCS
	test	word ptr ds:ofcs,-1 ;did we already set FCS?
	jnz	rsdel2		;yes, this must be an attribute block
	mov	ds:ofcs,dx	;save for RSFRET
	jmp	short rsdel2	;keep going until end (1-3 blockettes)
rsdel3:	; flush retrieval list
	pop	ax		;restore link
	;callr	rsfret		;free it, return
;+
;
; Free a retrieval chain.
;
; ax	link word to start of chain, or 0 if none
; DS:OFCS  must contain the FCS of the file being deleted
;
;-
rsfret:	xor	bx,bx		;init offset
	push	bx		;save
rsfrt1:	push	ax		;save link to this word
	call	rslink		;get retrieval blockette
	mov	di,bx		;[copy buf base out of the way]
	pop	ax		;[restore]
	pop	bx
	jc	rsfrt3		;EOF, done
	inc	bx		;+2
	inc	bx
	push	bx		;save again
	push	ax
	cmp	bx,8d*2		;done whole list?
	je	rsfrt2		;yes, flush the blockette itself
	mov	ax,[si+bx]	;fetch next retrieval DCN
	sub	ax,1		;skip DCN 0
	jc	rsfrt2		;EOF, done
	xor	dx,dx		;zero-extend
	div	word ptr ss:pcsdcs[bp] ;find starting PCN of cluster
	mov	cx,ax		;copy
	mov	ax,ds:ofcs	;get # blocks to free
	xor	dx,dx		;zero-extend
	div	word ptr ss:pcs[bp] ;make that # PCs
	mov	dx,ax		;copy
	call	rscbit		;clear bit(s) in SATT.SYS
	pop	ax		;restore link word to retrieval blockette
	jmp	short rsfrt1	;go handle next cluster
rsfrt2:	; finished clearing out retrieval blockette, flush it
	pop	ax		;flush stack
	pop	ax
	xor	ax,ax		;load 0
	mov	[si+2],ax	;mark slot as free
	xchg	ax,[si]		;fetch link to next
	mov	byte ptr ds:rdsdrt[di],1 ;mark buf as dirty
	jmp	short rsfret	;follow chain
rsfrt3:	ret
;+
;
; Get RSTS dir (or SATT.SYS) block.
;
; dx:ax	block # (RSTS itself supports only 23-bit LBNs so DH will always be 0)
; cl	NZ to return buf but don't read if not cached (going to write it)
; ss:bp	log dev rec
;
; Returns ptr in SI.
;
;-
rsrdir:	mov	si,ss:dirbuf[bp] ;head of buffer chain
	xor	bx,bx		;no prev
	cmp	ds:rdsbkl[si],bx ;is this our first buf request?
	jnz	rsdr1		;(assumption:  blk 0 (boot) is not cached)
	cmp	ds:rdsbkh[si],bx ;hm?
	jz	rsdr9		;yes, use our pre-alloced buf
rsdr1:	; see if we already have it
	cmp	ax,ds:rdsbkl[si] ;is this it?
	jne	rsdr2
	cmp	dx,ds:rdsbkh[si] ;hm?
	je	rsdr3		;yes, relink and return
rsdr2:	test	word ptr ds:rdsnxt[si],-1 ;end of list?
	jz	rsdr5		;yes, need to read it
	mov	bx,si		;save for linking
	mov	si,ds:rdsnxt[si] ;follow link
	jmp	short rsdr1	;keep looking
rsdr3:	; found it
	test	bx,bx		;is it first in line?
	jz	rsdr4		;yes, no need to relink
	mov	ax,si		;pt at it
	xchg	ax,ss:dirbuf[bp] ;get head of chain, set new head
	xchg	ax,ds:rdsnxt[si] ;link rest of chain to head
	mov	ds:rdsnxt[bx],ax ;unlink the one we just moved
rsdr4:	ret
rsdr5:	; we don't have it, try to allocate a new buffer
	push	si		;save
	push	cx
	mov	cx,rdslen	;# bytes
	call	askmem		;ask for a buffer
	pop	cx		;[catch "really read" flag]
	pop	di		;[catch ptr to last buf in chain]
	jnc	rsdr8		;got it, read the buf in
	mov	si,di		;restore
	; failed, write out the last one
	test	bx,bx		;is there a penultimate buffer?
	jz	rsdr6		;no
	mov	ds:rdsnxt[bx],0	;yes, unlink final buffer from it
	jmp	short rsdr7
rsdr6:	mov	ss:dirbuf[bp],bx ;this is the only one, unlink it
rsdr7:	push	ax		;save blk
	push	dx
	push	cx		;and flag
	call	rswdir		;flush it
	pop	cx		;restore
	pop	dx
	pop	ax
rsdr8:	; link buf in as first
	mov	bx,si		;copy ptr
	xchg	bx,ss:dirbuf[bp] ;get current chain, set new head
	mov	ds:rdsnxt[si],bx ;link it on
rsdr9:	; buf is linked in at head of chain
	mov	ds:rdsbkl[si],ax ;set blk
	mov	ds:rdsbkh[si],dx
	mov	byte ptr ds:rdsdrt[si],0 ;buf is clean
	test	cl,cl		;should we read it?
	jnz	rsdr10		;no, all we wanted was the buf
	mov	cx,1		;read 1 blk (DH=0)
	mov	di,si		;point at buffer with ES:DI
	push	si		;save
	call	ss:rdblk[bp]	;go
	pop	si		;[restore]
	jc	rsdr11		;err
rsdr10:	ret
rsdr11:	jmp	dirio
;+
;
; Write RSTS dir block buffer, if dirty.
;
; ss:bp	dev rec
; ds:si	ptr to buffer (preserved on exit)
;
;-
rswdir:	cmp	ds:rdsdrt[si],0	;is buf dirty?
	jz	rswd1		;no
	mov	ax,ds:rdsbkl[si] ;get blk #
	mov	dx,ds:rdsbkh[si]
	mov	cx,1		;1 blk
	push	si		;save
	push	es		;save
	push	ds		;copy DS to ES
	pop	es
	call	ss:wrblk[bp]	;go
	pop	es		;[restore]
	pop	si
	jc	rswd2		;err
rswd1:	ret
rswd2:	jmp	dirio
;+
;
; Follow RDS dir link.
;
; ax	link to follow
; si	points to destination blockette on return
; bx	points to base of dir block buf on return (so can set "dirty" flag)
; CF=1	if no next blockette (RSLNKE only)
;
; link:
; 15:12	relative blk # in clu
; 11:9	relative clu # in dir (0-6)
; 8:4	blockette (8. words) within block
; 3:0	don't care (used for flags or to ensure blockette is non-null)
;
;-
rslink:	and	ax,not 17	;end?
	jz	rslnk1		;yes
rslnkn:	; same with no end-of-chain check (so can address blockette 0)
	push	ax		;save
	mov	bl,ah		;copy clu #
	and	bx,7*2		;isolate it
	cmp	bl,7*2		;make sure it's blk 0-6
	je	rslnk2		;no, error
	add	bx,bp		;add base of log dev record
	mov	cl,12d		;bit count
	mov	dh,ah		;get blk within cluster
	shr	dx,cl		;into DX
	cmp	dx,ss:dirclu[bp] ;less than dir clustersize right?
	jae	rslnk2		;no, invalid
	mov	ax,ss:dirwnd[bx] ;look up retrieval
	test	ax,ax		;0?  (unmapped part of dir)
	jz	rslnk2		;error if so
	mov	bx,dx		;save blk within cluster
	mul	ss:dcs[bp]	;*DCS
	add	ax,bx		;add it in
	adc	dx,0
	xor	cl,cl		;really read it
	call	rsrdir		;get the block
	pop	bx		;restore
	and	bx,37*20	;isolate byte offset
	xchg	bx,si		;swap (so BX=base of block)
	add	si,bx		;add it in (CF=0)
	ret
rslnk1:	stc			;end of chain
	ret
rslnk2:	jmp	baddir		;link was to nonexistent block
;+
;
; Find next free area on disk.
;
; cx	starting PCN to search (find first free area >= this disk loc)
; ss:bp	log dev rec
;
; On return:
; cx	starting PCN of free area
; dx	PCs in free area
;
; PCN can be looked at this way:
;
;  15   12 11            3 2   0
; +-------+---------------+-----+
; | BLOCK |  BYTE OFFSET  | BIT |
; +-------+---------------+-----+
;
;-
rsfree:	mov	al,ch		;get blk #
	push	cx		;save
	mov	cl,4		;shift count
	shr	al,cl		;right-justify
	cbw			;zero-extend (AH<7>=0 from SHR)
	cwd
	add	ax,ss:bitmap[bp] ;add start of [0,1]SATT.SYS
	adc	dx,ss:bitmap+2[bp]
	xor	cl,cl		;actually read the block
	call	rsrdir
	pop	cx		;restore
	mov	bx,cx		;copy
	mov	al,1		;load a 1
	and	cl,7		;isolate bit within byte
	shl	al,cl		;shift into place
	mov	cx,bx		;restore
	shr	bx,1		;right 3 bits
	shr	bx,1
	shr	bx,1
	and	bh,777/400	;isolate 9-bit block offset
rsfr1:	; search for a clear bit
	; DS:SI=block buffer, BX=offset, AL=bit, CX=corresponding PCN
	test	[bx+si],al	;free?
	jz	rsfr3		;yes
	inc	cx		;PCN+1
	cmp	cx,ss:numclu[bp] ;off EOD?
	jae	rsfr2
	rol	al,1		;left a bit
	jnc	rsfr1		;loop
	inc	bx		;bump to next byte
	cmp	bh,1000/400	;reached end of block?
	jb	rsfr1		;no
	mov	ax,ds:rdsbkl[si] ;get block #
	mov	dx,ds:rdsbkh[si]
	add	ax,1		;bump to next
	adc	dx,0
	push	cx		;save
	;xor	cl,cl		;actually read the block
				;(CX is known to be a multiple of 10000)
	call	rsrdir
	pop	cx		;restore
	xor	bx,bx		;init offset
	mov	al,1		;first bit
	jmp	short rsfr1
rsfr2:	stc			;nothing available
	ret
rsfr3:	; found a free cluster, count # consecutive free PCs
	push	cx		;save
rsfr4:	; search for a set bit
	; DS:SI=block buffer, BX=offset, AL=bit, CX=corresponding PCN
	test	[bx+si],al	;in use?
	jnz	rsfr5		;yes
	inc	cx		;PCN+1
	cmp	cx,ss:numclu[bp] ;off EOD?
	jae	rsfr5
	rol	al,1		;left a bit
	jnc	rsfr4		;loop
	inc	bx		;bump to next byte
	cmp	bh,1000/400	;reached end of block?
	jb	rsfr4		;no
	mov	ax,ds:rdsbkl[si] ;get block #
	mov	dx,ds:rdsbkh[si]
	add	ax,1		;bump to next
	adc	dx,0
	push	cx		;save
	;xor	cl,cl		;actually read the block
				;(CX is known to be a multiple of 10000)
	call	rsrdir
	pop	cx		;restore
	xor	bx,bx		;init offset
	mov	al,1		;first bit
	jmp	short rsfr4
rsfr5:	mov	dx,cx		;copy first PCN in use (or EOD)
	pop	cx		;restore start of free area
	sub	dx,cx		;find length, CF=0
	ret
;+
;
; Set bit(s) in [0,1]SATT.SYS.
;
; cx	starting PCN to mark as in use
; dx	number of PCs to mark as in use
; ss:bp	log dev rec
;
;-
rssbit:	mov	al,ch		;get blk #
	push	cx		;save
	push	dx
	mov	cl,4		;shift count
	shr	al,cl		;right-justify
	cbw			;zero-extend (AH<7>=0 from SHR)
	cwd
	add	ax,ss:bitmap[bp] ;add start of [0,1]SATT.SYS
	adc	dx,ss:bitmap+2[bp]
	xor	cl,cl		;actually read the block
	call	rsrdir
	pop	dx		;restore
	pop	cx
	mov	bx,cx		;copy
	mov	al,1		;load a 1
	and	cl,7		;isolate bit within byte
	shl	al,cl		;shift into place
	shr	bx,1		;right 3 bits
	shr	bx,1
	shr	bx,1
	and	bh,777/400	;isolate 9-bit block offset
rssb1:	; start a new block
	mov	ds:rdsdrt[si],al ;buffer is dirty
rssb2:	; set a bit
	; DS:SI=block buffer, BX=offset, AL=bit, DX=loop counter
	or	[bx+si],al	;set a bit
	dec	dx		;count it
	jz	rssb3		;done
	rol	al,1		;left a bit
	jnc	rssb2		;loop
	inc	bx		;bump to next byte
	cmp	bh,1000/400	;reached end of block?
	jb	rssb2		;no
	push	dx
	mov	ax,ds:rdsbkl[si] ;get block #
	mov	dx,ds:rdsbkh[si]
	add	ax,1		;bump to next
	adc	dx,0
	xor	cl,cl		;actually read the block
	call	rsrdir
	pop	dx		;restore
	xor	bx,bx		;init offset
	mov	al,1		;first bit
	jmp	short rssb1
rssb3:	ret
;+
;
; Clear bit(s) in [0,1]SATT.SYS.
;
; cx	starting PCN to mark as free
; dx	number of PCs to mark as free
; ss:bp	log dev rec
;
;-
rscbit:	mov	al,ch		;get blk #
	push	cx		;save
	push	dx
	mov	cl,4		;shift count
	shr	al,cl		;right-justify
	cbw			;zero-extend (AH<7>=0 from SHR)
	cwd
	add	ax,ss:bitmap[bp] ;add start of [0,1]SATT.SYS
	adc	dx,ss:bitmap+2[bp]
	xor	cl,cl		;actually read the block
	call	rsrdir
	pop	dx		;restore
	pop	cx
	mov	bx,cx		;copy
	mov	al,not 1	;load a 0
	and	cl,7		;isolate bit within byte
	rol	al,cl		;shift into place
	shr	bx,1		;right 3 bits
	shr	bx,1
	shr	bx,1
	and	bh,777/400	;isolate 9-bit block offset
rscb1:	; start a new block
	mov	ds:rdsdrt[si],al ;buffer is dirty
rscb2:	; clear a bit
	; DS:SI=block buffer, BX=offset, AL=bit mask, DX=loop counter
	and	[bx+si],al	;clear a bit
	dec	dx		;count it
	jz	rscb3		;done
	rol	al,1		;left a bit
	jc	rscb2		;loop
	inc	bx		;bump to next byte
	cmp	bh,1000/400	;reached end of block?
	jb	rscb2		;no
	push	dx
	mov	ax,ds:rdsbkl[si] ;get block #
	mov	dx,ds:rdsbkh[si]
	add	ax,1		;bump to next
	adc	dx,0
	xor	cl,cl		;actually read the block
	call	rsrdir
	pop	dx		;restore
	xor	bx,bx		;init offset
	mov	al,not 1	;first bit
	jmp	short rscb1
rscb3:	ret
;+
;
; Find a free blockette for a new directory item in a UFD.
;
; dx	# empty blockettes to search for
; ss:bp	log dev rec
;
; On return, CF=0 if that many empty blockettes were available, and the
; registers are set up to point at the last blockette found:
; ax	link word to this blockette
; di	UFD cluster number (0-6)
; cx	block within cluster
; ds:si	dir buf containing that block
; bx	offset into buf of the empty blockette
; If you leave these regs alone and call back at RSSLTN with a new non-zero
; count in DX, you can continue searching at that point.
;
; If CF=1 then not enough empty slots were found:
; dx	# empty blockettes still needed
;
;-
rsslot:	xor	di,di		;cluster 0
	xor	cx,cx		;block 0 of that cluster
	mov	bx,8d*2		;blockette 1 of that block
rssl1:	; fetch next block of directory
	push	bx		;save
	push	cx
	push	dx
	push	di
	add	di,di		;*2 to get index
	mov	ax,ss:dirwnd[bp+di] ;get base of cluster
	test	ax,ax		;off end of dir?
	jz	rssl3
	mul	ss:dcs[bp]	;*DCS to get starting LBN of cluster
	add	ax,cx		;add block within cluster
	adc	dx,0
	xor	cl,cl		;really read it
	call	rsrdir		;read dir block
	pop	di		;restore
	pop	dx
	pop	cx
	pop	bx
rssl2:	; see if slot is empty
	mov	ax,[bx+si]	;first two words of blockette both 0?
	or	ax,[bx+si+2]
	jz	rssl4		;yes
rssltn:	; enter here to continue existing search
	; BX=blockette offset, SI=dir buf ptr, CX=block within clu, DI=clu #
	add	bx,8d*2		;bump to next
	cmp	bx,760		;reached cluster map yet?
	jb	rssl2		;loop if not
	xor	bx,bx		;yes, rewind to begn of next block
	inc	cx		;bump block within cluster
	cmp	cx,ss:dirclu[bp] ;hit end?
	jb	rssl1		;loop if not
	xor	cx,cx		;yes, rewind to begn of next cluster
	inc	di		;cluster +1
	cmp	di,6d		;cluster 0-6?
	jbe	rssl1
	stc			;failed
	ret
rssl3:	pop	di		;restore
	pop	dx
	pop	cx
	pop	bx
	stc			;failed
	ret
rssl4:	dec	dx		;done all?
	jnz	rssltn		;loop if not
	; compose link word to final blockette
	push	cx		;save
	mov	ax,cx		;copy block # within cluster (will be b15:12)
	mov	cl,3		;shift count
	shl	ax,cl
	or	ax,di		;add in clu # (will be b11:9)
	mov	cl,9d
	shl	ax,cl
	or	ax,bx		;get blockette offset within block in b8:4
	inc	ax		;set LSB to mark as in use
	pop	cx		;restore
	clc			;happy
	ret
;+
;
; Add new cluster(s) to a RSTS directory.
;
; cx	# clusters to add
; ss:bp	log dev rec
;
; SS:DIRCLU[BP], SS:DIRWND[BP] describe the directory.
;
;-
rsanew:	mov	word ptr ds:onew,0 ;none added yet
rsan1:	; add another cluster
	push	cx		;save count
	mov	ax,ss:dirclu[bp] ;get dir cluster size
	call	rsfclu		;find free cluster
	jc	rsan3		;EOD
	mov	dx,cx		;get starting PCN
	add	dx,ax		;advance to next PC
	dec	dx		;almost
	mov	ss:satptr[bp],dx ;save for next time
	mov	dx,ax		;copy # PCs to reserve
	push	cx		;save starting PC
	call	rssbit		;allocate the new dir cluster
	pop	ax		;restore
	mul	word ptr ss:pcsdcs[bp] ;convert PCN to DCN
	inc	ax		;(skip DCN 0)
	; add to cluster map
	xor	di,di		;start at 0
rsan2:	test	word ptr ss:dirwnd[bp+di],-1 ;find first free slot
	jz	rsan4		;got it (known to exist)
	inc	di		;+2
	inc	di
	jmp	short rsan2
rsan3:	jmp	short rsan6	;failed
rsan4:	mov	ss:dirwnd[bp+di],ax ;add to list
	inc	word ptr ds:onew ;count it (will delete on error)
	; clear all blocks of cluster
	mul	word ptr ss:dcs[bp] ;convert DCN to phys block #
	mov	cx,ss:dirclu[bp] ;get # blocks in dir cluster
rsan5:	push	ax		;save
	push	cx
	push	dx
	mov	cl,1		;no need to actually read anything
	call	rsrdir		;fetch dir buffer
	mov	di,si		;copy
	xor	ax,ax		;load 0
	mov	cx,512d/2	;# words in a block
	rep	stosw		;clear out block
	mov	byte ptr ds:rdsdrt[si],1 ;mark as dirty
	pop	dx		;restore
	pop	cx
	pop	ax
	add	ax,1		;bump to next block
	adc	dx,0
	loop	rsan5		;clear it out too
	pop	cx		;restore dir clu counter
	loop	rsan1		;loop through all
	callr	rsumap		;update cluster map in each UFD block, return
rsan6:	; disk is full
	pop	cx		;flush loop count
	call	rsfnew		;free clusters added to dir, if any
	jmp	odfull		;no space
;+
;
; Failed to write file, free any dir clusters we added.
;
;-
rsfnew:	mov	cx,ds:onew	;get # clusters we added
	jcxz	rsfn2		;none
	mov	di,6*2		;start at highest cluster
rsfn1:	test	word ptr ss:dirwnd[bp+di],-1 ;find last used slot
	jnz	rsfn3		;got it (known to exist)
	dec	di		;-2
	dec	di
	jns	rsfn1		;should always find it
rsfn2:	ret			;but just in case, exit now
rsfn3:	; find next cluster
	push	cx		;save
	push	di
	xor	ax,ax		;load 0
	cwd			;DX=0
	xchg	ax,ss:dirwnd[bp+di] ;remove cluster from list, get DCN
	dec	ax		;skip DCN 0
	; deallocate from [0,1]SATT.SYS
	div	word ptr ss:pcsdcs[bp] ;divide DCN by # of DCs per PC
	mov	cx,ax		;starting PCN of cluster
	mov	ax,ss:dirclu[bp] ;get size of dir cluster
	xor	dx,dx		;zero-extend cluster size (should be 0 already)
	div	word ptr ss:pcs[bp] ;find # PCs per cluster
	cmp	ax,1		;dir clusize can be < PCS, change to 1
	adc	ax,0		;(dirs don't use more than clusize=16)
	mov	dx,ax		;copy # PCs to free
	call	rscbit		;clear bit(s) in bitmap
;;; would be nice to clear the dirty bits too if it's not too late, but all
;;; that'll happen is we'll scribble on the un-allocated blocks that would have
;;; been ours anyway so it's probably not worth the effort, we only come here
;;; on "disk full" errors anyway
	pop	di		;restore
	pop	cx
	dec	di		;-2
	dec	di
	loop	rsfn3		;remove all clusters we added
	callr	rsumap		;update maps in all dir blocks, return
;+
;
; Update cluster map in each block of a UFD.
;
; ss:bp	log dev rec
; SS:DIRWND[BP]  gives correct cluster map
; SS:DIRCLU[BP]  gives cluster size
;
;-
rsumap:	xor	di,di		;init cluster #
rsum1:	; update next cluster
	mov	ax,ss:dirwnd[bp+di] ;get starting DC of cluster
	test	ax,ax		;if any
	jz	rsum3		;done
	mul	word ptr ss:dcs[bp] ;find starting block #
	mov	cx,ss:dirclu[bp] ;get dir cluster size
	push	di		;save
rsum2:	; update next block of cluster
	push	ax		;save
	push	cx
	push	dx
	xor	cl,cl		;really read it
	call	rsrdir		;get the cluster
	mov	byte ptr ds:rdsdrt[si],1 ;buf will be dirty
	lea	di,[si+760]	;point at cluster map in block
	lea	si,ss:dirclu[bp] ;point at correct copy
.assume <dirwnd eq dirclu+2>	;(same order as in dir)
	mov	cx,8d		;# words to copy
	rep	movsw		;copy
;;; one of my old comments seems to think that the MSB of the clustersize
;;; can hold a flag, doesn't seem to be true in RDS 0.0 or 1.1 according to the
;;; Mike Mayfield book but I better dig out my DEC RDS 1.2 doc again.
;;; It does seem to say that b15 of the *MFD* cluster size is set on RDS 1.x
	pop	dx		;restore
	pop	cx
	pop	ax
	add	ax,1		;bump to next block in cluster
	adc	dx,0
	loop	rsum2		;write next block in cluster
	pop	di		;restore cluster # *2
	inc	di		;bump to next slot
	inc	di
	cmp	di,6d*2		;still in range 0-6?
	jbe	rsum1		;loop if so
rsum3:	; update SAVWND for "other" device (i.e. input vs. output) if it's the
	; same device and PPN as this one
	xor	bx,bx		;check input device
	call	rsum4		;do it
	mov	bx,2		;output device
rsum4:	; check device # BX (0=input, 2=output), OK if it's just us
	cmp	bp,ds:indev[bx]	;same device?
	jne	rsum5		;no
	mov	ax,ss:actdir[bp] ;get our dir
	cmp	ax,ds:savdir[bx] ;same dir as theirs?
	jne	rsum5		;no
	callr	rssav		;yes, reload SAVWND, return
rsum5:	ret
;+
;
; Find the next free area that's at least big enough for a cluster.
;
; I can't find anything in the doc that says the cluster must begin on a
; multiple of its own size, but that appears to be what RSTS does, and CLEAN
; seems to hallucinate overlap with other files (so maybe it masks the
; retrieval pointers) if we don't do it too.
;
; ax	cluster size (in blocks)
; ss:bp	log dev rec
;
; If none found, CF=1 on return.  Otherwise:
; ax	cluster size in PCs
; cx	starting PCN of free area
; dx	# of PCs in free area
;
;-
rsfclu:	xor	dx,dx		;zero-extend cluster size
	div	word ptr ss:pcs[bp] ;find # PCs per cluster
	cmp	ax,1		;dir clusize can be < PCS, change to 1
	adc	ax,0		;(dirs don't use more than clusize=16)
	mov	cx,ss:satptr[bp] ;start where we left off before
	inc	cx		;first block after last one touched
	jz	rsfc2		;(wrapped)
	cmp	cx,ss:numclu[bp] ;off end?
	jae	rsfc2		;yes, wrap around already
	; find first free area big enough for a cluster
	call	rsfc40		;search
	jnc	rsfc4		;got it
rsfc2:	; hit end of disk, wrap around but stop when we reach SATPTR again
	xor	cx,cx		;start at beginning of disk
	call	rsfc40
rsfc4:	ret
;
rsfc40:	; search until EOD, CX=starting PCN, AX=# PCs per FC
	push	ax		;save
	call	rsfree		;find next free area
	pop	ax		;[restore]
	jc	rsfc49		;out of space, CF=1
	; round CX up to FCS multiple
	mov	bx,ax		;copy
	sub	bx,cx		;find # blocks from CX to next multiple
	dec	ax		;bitmask
	and	bx,ax		;mask to 0::FCS-1
	inc	ax		;restore
	cmp	dx,bx		;will there be anything left?
	jbe	rsfc41		;no
	add	cx,bx		;yes, round
	sub	dx,bx
	cmp	dx,ax		;big enough for a cluster?
	jae	rsfc49		;yes, CF=0
rsfc41:	add	cx,dx		;no, advance to end of free area
	jmp	short rsfc40	;check next free area
rsfc49:	ret
;
	subttl	OS/8 (and PUTR and COS-310) file structure
;
	comment	_

OS/8 disks have a directory in blocks 1-6, consisting of a linked list of
256-word blocks (starting at block 1), each of which includes a 5-word header:

+0000	dw	-(# entries in seg)
+0001	dw	starting blk # of 1st file in seg
+0002	dw	next dir seg or 0
+0003	dw	tentative file ptr (meaningful while file is open under OS/8)
+0004	dw	-(# info words per dir entry) (usually -1 -- date)
+0005 first dir entry starts here

Each file entry looks like this:

	text	/FILNAM/
	text	/EX/
	; optional info words
	; first may be date:  MMMMDDDDDYYY, where YYY=MOD(YEAR-1970,8)
	-length

Entries for .EMPTY. areas are just two words:

	0
	-length

TSS/8 PUTR DECtapes use a variant of the OS/8 tape format.  Differences are:

* tape is written in 11:1 interleave with every other pass over the tape being
  in the reverse direction (necessary because TSS/8 has only one DECtape block
  buffer, so can't read consecutive blocks w/o repositioning)

* filename extension word encodes 3-letter extension instead of TEXT /EX/

* ASCII 3-into-2 packing is different:
  OS/8 encodes ABC as CCCCAAAAAAAA, CCCCBBBBBBBB
  PUTR encodes ABC as AAAAAAAABBBB, BBBBCCCCCCCC

COS-310 is still another variant:

* text files are stored in a weird 6-bit format, with line numbers

_
;+
;
; Set defaults for OS/8.
;
; si,cx	preserved
;
;-
osds:	mov	ax,ss:curdir[bp] ;get current LD:
	mov	ss:actdir[bp],ax ;set active LD:
	mov	ax,ss:cursiz[bp] ;get size of LD:
	mov	ss:actsiz[bp],ax
	xor	ax,ax		;high word always 0
	mov	ss:actsiz+2[bp],ax
	ret
;+
;
; Save OS/8 CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
ossav:	mov	ax,ss:actdir[bp] ;get active LD:
	mov	ds:savdir[bx],ax
	mov	ax,ss:actsiz[bp]
	mov	ds:savsiz[bx],ax
	ret
;+
;
; Restore OS/8 CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
osrst:	mov	ax,ds:savdir[bx] ;get saved stuff
	mov	ss:actdir[bp],ax ;set active LD:
	mov	ax,ds:savsiz[bx]
	mov	ss:actsiz[bp],ax
	ret
;
osdh:	; print OS/8 dir header
	mov	di,offset lbuf	;pt at buffer
	cram	' Name .Ex'
	cmp	word ptr ss:fsys[bp],putr ;PUTR DECtape?
	jne	osdh1
	mov	al,'t'		;yes, 3-letter extensions
	stosb
osdh1:	cram	'  Size     Date'
	test	byte ptr ds:dirflg,200 ;verbose listing?
	jz	osdh2
	cram	'      Loc'	;add location
osdh2:	call	flush		;flush line
	xor	ax,ax		;load 0
	mov	ds:nfile,ax	;nuke # files, blocks, frees
	mov	ds:nblk,ax
	mov	ds:nfree,ax
	ret
;
osdi:	; OS/8 & COS-310 dir input init
	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	osdi1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir seg buffer (nothing is possible if this fails)
	mov	cx,osslen	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:ossblk[si],ax ;nothing in here yet
	mov	ds:ossdrt[si],al ;not dirty
	mov	ds:ossnxt[si],ax ;no next
osdi1:	mov	ax,1		;first seg
	call	osnext		;get it
	mov	ds:fsize,0	;say preceding file null
	callr	osfix		;make sure we're pointing at an entry, return
;
osdg:	; get OS/8 & COS-310 dir entry
	mov	al,ds:inseg	;make sure seg is still there
	cbw			;AH=0 (really read it)
	call	osseg		;(in case output is same device, may page out)
	mov	ax,ds:inent	;get offset of current input entry
	test	ax,ax		;at end?
	jns	osdg1
	stc			;yes
	ret
osdg1:	; get next entry (SI=buffer, AX=offset)
	mov	bx,si		;save base (subtracted before returning)
	add	si,ax		;index to curr posn
	mov	ds:oinent,ax	;take snapshot in case delete/rename
	mov	dl,ds:inseg
	mov	ds:oinseg,dl
	push	bx		;save
	cmp	ax,5*2		;at begn of seg?
;;	je	osdg2		;yes, no previous
	mov	ax,ds:fsize	;get size of previous
	add	ds:stblk,ax	;update starting blk ptr
osdg2:	mov	word ptr ds:ftimf,0 ;no date or time
	mov	di,offset lbuf	;pt at buf (whether empty or not)
	lodsw			;get first word
	xor	dx,dx		;(in case empty area)
	mov	ds:pextra,dx	;in case no extras
	test	ax,ax		;is it empty?
	jz	osdg10		;yes (AX=0 to update free space count)
	call	text3		;FILNAM
	call	text2
	call	text2
	mov	al,'.'		;.
	stosb
	cmp	word ptr ss:fsys[bp],putr ;PUTR DECtape?
	je	osdg3
	call	text2		;EX
	jmp	short osdg6
osdg3:	; PUTR DECtape, look up 3-letter extension
	lodsw			;fetch it
	push	si		;save
	mov	bx,ax		;copy
	and	bx,37		;trim to 5 bits
	mov	si,offset putext ;point at extension list
osdg4:	lodsb			;get next entry
	test	al,al		;end of list?
	js	osdg5
	cbw			;no, AH=0
	cmp	ax,bx		;match?
	je	osdg5
	add	si,3		;no, skip it
	jmp	short osdg4	;keep searching
osdg5:	movsw			;copy
	movsb
	pop	si		;restore
osdg6:	push	si		;save
	call	sqfn		;make squished copy of filename
	pop	si		;restore
	mov	ds:pextra,si	;save ptr to info word(s)
	; get file date if there was one
	mov	cx,ss:extra[bp]	;get extra byte count
	jcxz	osdg9		;no date if nothing
	mov	ax,[si]		;fetch first extra word (if any)
	add	si,cx		;skip extra word(s)
	test	ax,ax		;is date valid?
	jz	osdg9		;no
	; OS/8 date is 12 bits:  MMMMDDDDDYYY
	; MMMM=month (1-12.), DDDDD=day (1-31.), YYY=year-1970 (0-7)
	; obviously the yyy format ran out long ago, so normally we would add
	; some multiple of 8 to it (retrieved from a word in the OS/8 monitor)
	;
	; We'll use DIRECT.SV's heuristic:  add the most recent year which is a
	; multiple of 8. years after 1970 (computed from the DOS date), then
	; subtract 8 years if this would yield a year which is after today.
	; This means dates will be right for files less than 8 years old.
	; Otherwise you lose.  TSS/8 used to use a similar date system for the
	; on-disk dates, and they patched all the CUSPs a couple of times to
	; give new base years other than 1970 (each time it ran out).  The last
	; one ran out around 1984...  PUTR uses OS/8's date format.
	mov	dx,ax		;copy month,,day*8d
	mov	cl,3		;bit count
	shr	dl,cl
	and	dx,0F1Fh	;isolate both
	and	ax,7		;isolate MOD(YEAR-1970,8)
	add	ax,ds:yrbase	;add base
	cmp	ax,ds:year	;is it after this year?
	jb	osdg8		;before, skip
	ja	osdg7		;after, back up
	cmp	dx,word ptr ds:day ;this year, check day
	jbe	osdg8		;today or before, skip
osdg7:	sub	ax,8d		;back 8 years
osdg8:	mov	word ptr ds:fday,dx ;save month,,day
	mov	ds:fyear,ax	;and year
	inc	ds:fdatf	;date is valid
osdg9:	mov	dx,100h		;flag for XOR below (CF=0 ZF=0)
	mov	ax,-1		;mask
osdg10:	; DL,DH -- XORed together to set ZF, AX=0 if .EMPTY., -1 if file
	mov	bx,ax		;copy
	not	bx		;invert
	lodsw			;get -(length)
	mov	cx,10000	;abs val
	sub	cx,ax
	and	ch,7777/400	;might have been 0
	mov	ds:fsize,cx
	and	bx,cx		;copy size if .EMPTY.
	add	ds:nfree,bx	;add it on (or not if not .EMPTY.)
	mov	bx,dx		;copy
	mov	ax,384d		;384d 8-bit chars per 256d 12-bit words
	cmp	ds:binflg,text	;text mode?
	je	osdg11		;keep this value if so
	 mov	ax,512d		;no, 512d 8-bit bytes per 256d 12-bit words
osdg11:	mul	cx		;find byte count
	mov	ds:fbcnt,ax	;save
	mov	ds:fbcnt+2,dx
	pop	ax		;catch base of buffer
	sub	si,ax		;find offset
	mov	ds:inent,si	;update
	dec	ds:ndent	;# entries -1
	; find next entry now so that INENT will point at it
	push	bx
	push	di
	call	osfix
	pop	di
	pop	bx
	xor	bh,bl		;CF=0, ZF=(BH.EQ.BL)
	ret
;
; TSS/8 EXTENSIONS:
;
; PROT & 7700	EXTENSION
; 0000		[NONE]
; 0200		.ASC
; 0400		.SAV
; 0600		.BIN
; 1000		.BAS
; 1200		.BAC
; 1400		.FCL
; 1600		.TMP
; 2000		[NONE]
; 2200		.DAT
; 2400		.LST
; 2600		.PAL
;
; Those are what's stored in the protection code word on a TSS/8 disk (I made
; this list years ago by experimenting on my live TSS/8 system).  I'm assuming
; that these are shifted right 7 bits and stored in a PUTR tape (with 40 ORed
; in for some reason), at least it seems that way from a little testing.
;
putext	label	byte		;PUTR filename extension codes
	db	00,'   '
	db	01,'ASC'
	db	02,'SAV'
	db	03,'BIN'
	db	04,'BAS'
	db	05,'BAC'
	db	06,'FCL'
	db	07,'TMP'
	db	10,'   '
	db	11,'DAT'
	db	12,'LST'
	db	13,'PAL'
	db	-1,'XXX'	;end of list
;+
;
; Routine to ensure that INSEG/INENT point at an actual directory entry (file
; or .EMPTY.), rather than pointing off the end of a segment.  This is to
; ensure that OSCL, OSDL, OSRDIR, and OSWDIR will handle relocating INSEG/INENT
; correctly if they reorganize the directory.
;
; INENT is set to -1 if there are no more entries in the directory (normal
; range is 5*2 to 254.*2).
;
;-
osfix:	cmp	ds:ndent,0	;anything left in seg?
	jz	$+3
	 ret			;yes, we must be OK
	; link to next segment
	mov	al,ds:inseg	;make sure seg is still there
	cbw			;AH=0 (really read it)
	call	osseg
	mov	ax,[si+4]	;get link to next
	call	osnext		;follow it
	jnc	osfix		;make sure it's not empty
	mov	ds:inent,-1	;flag as end (will never match a valid ptr)
	ret
;+
;
; Move to next segment of OS/8 dir.
;
; ax	link word (or 0 if end)
;
; On return, CF=1 if end of directory, otherwise:
;
; bx	return pointer to buffer
; si	returns pointer to dir entry area (5 words past BX)
;
;-
osnext:	test	ax,ax		;any?
	jz	osnxt3		;no
	mov	ds:inseg,al	;yes, save #
	xor	ah,ah		;really read
	call	osseg		;get dir seg
	mov	bx,si		;save base
	lodsw			;get -(# entries)
	mov	cx,10000	;abs
	sub	cx,ax
	and	ch,7777/400	;trim to 12 bits in case 0000
	mov	ds:ndent,cx
	lodsw			;get starting blk # of 1st file
	add	ax,ss:actdir[bp] ;add base of dir
	cmp	byte ptr ds:inseg,1 ;first segment?
	je	osnxt1		;yes, accept w/o question
	; make sure it's what we expect
	cmp	ax,ds:stblk	;should match
;;	jne	...		;error (does happen in OS/278, computed value
				;is the correct one)
	jmp	short osnxt2	;well keep computed value anyway
osnxt1:	mov	ds:stblk,ax	;save
osnxt2:	add	si,3*2		;skip (next dir seg),(tentative file info)
				;and -(# info words per dir entry), CF=0
	mov	ds:inent,5*2	;[save offset]
	ret
osnxt3:	stc			;end of dir
	ret
;
osdd:	; display OS/8 dir entry
	mov	ax,ds:fsize	;size
	inc	ds:nfile	;bump # files
	add	ds:nblk,ax	;and # blks
	cwd			;0-extend (always <=7777)
	mov	cx,6		;# spaces
	call	cnum		;print it right-justified
	; add date, if any
	mov	cx,2+11d+2+4	;# cols for loc if no date
	cmp	byte ptr ds:fdatf,0 ;was there a date?
	jz	osdd1		;no
	mov	ax,"  "		;2 blanks
	stosw
	call	pdate		;print date
	mov	cx,2+4		;2 blanks before loc
osdd1:	test	byte ptr ds:dirflg,200 ;verbose listing?
	jz	osdd2		;no
	mov	ax,ds:stblk	;get starting block #
	xor	dx,dx		;0-extend
	callr	cnum		;add blanks, print it, return
osdd2:	ret
;
osde:	; display empty OS/8 entry
	mov	di,offset lbuf	;pt at buf again
	cram	'.EMPTY.'
	mov	ax,ds:fsize	;size
	cwd			;0-extend (always <=7777)
	mov	cx,8d		;size of field
	callr	cnum		;print it right-justified, return (no date)
;
osdf:	; OS/8 dir finish (flush dirty segment buffers)
	mov	si,ss:dirbuf[bp] ;get ptr to buf chain
osdf1:	call	osfseg		;flush
	mov	si,ds:ossnxt[si] ;follow chain
	test	si,si		;anything?
	jnz	osdf1		;loop if so
	ret
;
ossum:	; OS/8 dir listing summary
;;; this isn't based on anything, should look at DIRECT.PA
	mov	di,offset lbuf	;pt at buf
	call	flush		;blank line
	mov	ax,ds:nfile	;# files
	push	ax		;save
	call	pnum
	cram	' file'
	pop	ax		;restore
	call	adds		;add s
	mov	ax," ,"		;', '
	stosw
	mov	ax,ds:nblk	;# blocks
	push	ax		;save
	call	pnum
	cram	' block'
	pop	ax		;restore
	call	adds		;add s
	mov	ax," ,"		;', '
	stosw
	mov	ax,ds:nfree	;# frees
	call	pnum
	cram	' free'
	callr	flush		;flush, return
;
osop:	; open OS/8 file for input
	mov	ax,ds:stblk	;get starting blk #
	mov	ds:iblk,ax	;save
	mov	ax,ds:fsize	;get size
	mov	ds:isize,ax	;save
	mov	byte ptr ds:ieof,0 ;not EOF
	ret
;
osrd:	; read OS/8 file data
	mov	bx,ds:isize	;get remaining size
	test	bx,bx		;EOF?
	jz	osrd3		;yes, ZF=1
	mov	al,ch		;get # full blocks free in buf
	shr	al,1
	jz	osrd2		;nothing will fit
	cbw			;AH=0
	cmp	ax,bx		;more than what's left?
	jb	osrd1
	 mov	ax,bx		;stop at EOF
osrd1:	mov	cx,ax		;copy block count
	mov	ax,ds:iblk	;get starting block
	xor	dx,dx		;OS/8 devs are always .LE.4096 blks
	add	ds:iblk,cx	;update
	sub	ds:isize,cx
	mov	si,ds:binflg	;get binary flag
	jmp	ss:rdblk[bp+si]	;read data, return
osrd2:	or	al,1		;ZF=0
osrd3:	stc			;ZF already set, set CF
	ret
;
osdo:	; OS/8 dir output init
	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	osdo1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir seg buffer (nothing is possible if this fails)
	mov	cx,osslen	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:ossblk[si],ax ;nothing in here yet
	mov	ds:ossdrt[si],al ;not dirty
	mov	ds:ossnxt[si],ax ;no next
osdo1:	ret
;
oscr:	; create OS/8 file
	; parse filename (at DS:SI), convert to TEXT form (truncated ASCII)
	call	lenz		;get length
	call	gtext		;get filename
	jc	oscr5
	cmp	al,'.'		;ends with dot, right?
	jne	oscr5
	mov	ds:rtfile,bx	;save filename
	mov	ds:rtfile+2,dx
	mov	ds:rtfile+4,di
	inc	si		;eat the dot
	dec	cx
	call	gtext		;get extension
	test	al,al		;ends at end of string, right?
	jnz	oscr5
	cmp	word ptr ss:fsys[bp],putr ;PUTR.SAV?
	jne	oscr2		;no, BX contains two-letter extension
	; PUTR uses codes to represent 3-letter extensions from a fixed list
	mov	ax,bx		;get first 2 chars
	call	oscr13		;convert to ASCII
	push	ax		;save
	mov	ax,dx		;get 3rd/4th chars
	call	oscr13		;convert that too (3rd in AL)
	mov	dl,al		;save
	pop	cx		;restore 1st/2nd chars
	mov	si,offset putext ;point at list
oscr1:	lodsb			;get next code
	mov	bx,40		;in case unknown extension (dunno why b5 set)
				;probably to guarantee NZ low byte while high
				;byte =0, so can't be confused with OS/8 tapes
	test	al,al		;end of list?
	js	oscr2		;yes
	or	bl,al		;save in case correct
	lodsw			;get next 3 chars
	mov	dh,[si]
	inc	si
	cmp	ax,cx		;match?
	jne	oscr1		;loop if not
	cmp	dh,dl		;check 3rd char
	jne	oscr1
oscr2:	mov	ds:rtfile+6,bx	;save extension
	; search for smallest .EMPTY. area that's big enough for it
	mov	ax,ds:fbcnt	;get byte count
	mov	dx,ds:fbcnt+2
	mov	bx,384d-1	;ASCII chars/block -1
	cmp	ds:binflg,text	;text mode?
	je	oscr3
	 mov	bx,512d-1	;binary bytes/block -1
oscr3:	cmp	dx,bx		;make sure it'll fit in <64 K blks
	jbe	oscr6
oscr4:	jmp	odfull		;full
oscr5:	jmp	badnam		;bad filename
oscr6:	add	ax,bx		;round up to next block
	adc	dx,0
	inc	bx		;+1 to full block size
	div	bx		;find # blocks needed
	; search directory for first .EMPTY. area .LE. AX blocks long
	mov	dx,ax		;save length
	mov	al,1		;starting seg #
oscr7:	mov	ds:outseg,al	;save
	push	dx		;save min size
	cbw			;AH=0 (really read it)
	call	osseg		;get the seg
	pop	dx		;restore
	mov	bx,si		;save base of buf
	add	si,5*2		;skip to data
	mov	cx,10000	;find +<# entries>
	mov	di,cx		;(copy)
	sub	cx,[bx]
	and	ch,7777/400	;(force 12 bits in case 0)
	jcxz	oscr11		;null seg
	mov	ax,[bx+2]	;get starting blk
	add	ax,ss:actdir[bp] ;add base of LD: (if any)
	mov	ds:oblk,ax	;save
	sub	di,[bx+8d]	;get # info words on each dir entry
	and	di,7777		;trim to 12 bits in case 0
	sal	di,1		;*2 to get # bytes
	mov	ds:osize,0	;say prev file was null (preserve OBLK)
oscr8:	; examine next file entry within this segment (SI=ptr, BX=base,
	; CX=# file entries to go, DX=size needed, DI=# info bytes)
	mov	ax,ds:osize	;get size of previous
	add	ds:oblk,ax	;update starting blk ptr
	lodsw			;get first word
	test	ax,ax		;.EMPTY.?
	jnz	oscr9		;no
	mov	ax,10000	;yes, find size
	sub	ax,[si]
	and	ah,7777/400
	cmp	ax,dx		;is it big enough?
	jae	oscr12		;yes, take it
	jmp	short oscr10
oscr9:	add	si,8d-2		;skip rest of filename
	add	si,di		;skip info word(s) if any
	mov	ax,10000	;find size
	sub	ax,[si]
	and	ah,7777/400
oscr10:	mov	ds:osize,ax	;save
	inc	si		;skip length
	inc	si
	loop	oscr8
oscr11:	; get next seg
	mov	ax,[bx+4]	;get link to next seg
	test	ax,ax		;any?
	jnz	oscr7		;get it if so
	jmp	short oscr4
oscr12:	; found one that's big enough, save ptr to entry (OBLK, OSIZE set up)
	mov	ds:osize,ax	;save
	dec	si		;-2 (undo LODSW)
	dec	si
	sub	si,bx		;subtract base (in case bufs shuffled)
	mov	ds:outent,si	;save
	mov	ds:owrtn,0	;init size
	ret
oscr13:	; expand two TEXT letters in AX to 2 ASCII chars
	; trashes CX, others preserved
	mov	cl,2		;shift count
	shl	ax,cl		;left 2 bits
	shr	al,cl		;low byte right 2 bits
	call	oscr14		;convert char in AL
	xchg	al,ah		;swap
oscr14:	and	al,77		;isolate (should be already)
	jz	oscr15		;space, special case
	mov	cl,al		;copy
	sub	cl,40		;get sign bits
	and	cl,100		;the one we want
	or	al,cl		;set it (or not)
	ret
oscr15:	mov	al,' '		;blank
	ret
;
oswr:	; write to OS/8 file
	mov	di,ds:binflg	;get binary flag
	mov	ax,cx		;copy # bytes
	mov	bx,512d		;get # bytes per block (binary mode, 256 words)
	test	di,di		;binary mode?
.assume <bin eq 0>
	jz	oswr1
	 mov	bx,384d		;# bytes/block in ASCII mode
oswr1:	xor	dx,dx		;zero-extend
	div	bx		;find # full blocks
	mov	cx,ax		;save count
	mul	bx		;round byte count to block multiple
	push	ax		;save
	test	ax,ax		;anything?
	jz	oswr2
	sub	ds:osize,cx	;eat this out of .EMPTY. area
	jc	oswbug		;whoops, this should never happen
	add	ds:owrtn,cx	;update # blks written
	mov	ax,ds:oblk	;get starting block #
	xor	dx,dx		;always <32MB in OS/8
	add	ds:oblk,cx	;update
	mov	di,ds:binflg	;get binary flag
	call	ss:wrblk[bp+di]	;write data
	jc	oswr3		;failed
oswr2:	pop	cx
	ret
oswr3:	pop	cx		;flush stack
	jmp	wrerr		;error
oswbug:	cram	'?BUG:  Output area too small',error
;+
;
; Write last buffer of data to output file.
;
; es:si	string
; cx	length of data
;
;-
oswl:	; write last partial block to OS/8 file
	jcxz	oswl3		;nothing, skip
	mov	dx,512d		;OS/8 block size
	cmp	ds:binflg,bin	;binary file?
	je	oswl1
	 mov	dx,384d		;text file block size
oswl1:	mov	bx,dx		;OS/8 block size
	sub	bx,cx		;find # NULs to add (in [1,777])
	xchg	bx,cx		;swap
	lea	di,[bx+si]	;point past data
	cmp	ds:binflg,bin	;binary file?
	je	oswl2
	mov	al,'Z'-100	;add ^Z if not
	stosb
	dec	cx		;subtract from count
oswl2:	xor	al,al		;load 0
	rep	stosb		;pad it out
	mov	cx,dx		;write a whole block
	jmp	oswr		;write data, return
oswl3:	ret
;+
;
; Close OS/8 output file.
;
; Same logic as RT-11 version.
;
;-
oscl:	; close OS/8 output file
	call	osrdir		;read directory, init IPTR1, OPTR1
	; delete any other file of the same name
	mov	ax,ds:dirsiz	;size of dir
	mov	ds,ds:bigbuf	;pt at buffer
	push	ds		;copy to ES too
	pop	es
	mov	bx,5*2		;starting posn
	mov	dx,bx		;output posn too
oscl1:	; check next entry (BX=input ptr, DX=output ptr, AX=end)
	cmp	bx,ax		;done?
	je	oscl6		;yes, skip
	; see if this is the current input entry
	cmp	bp,cs:indev	;is it the right dev?
	jne	oscl2		;no
	mov	si,ss:actdir[bp] ;yes, get LD: base
	cmp	si,cs:savdir	;is it the input one?
	jne	oscl2
	cmp	bx,cs:inptr	;is this the file?
	jne	oscl2
	 mov	cs:iptr1,dx	;update
oscl2:	; see if this is the current output entry
	cmp	bx,cs:outptr	;is this the output file (dir/dev match)
	jne	oscl3
	 mov	cs:optr1,dx	;update
oscl3:	cmp	word ptr [bx],0	;file or .EMPTY.?
	jz	oscl4		;.EMPTY., don't bother
	mov	si,bx		;point at filename
	push	es		;save
	push	cs		;copy CS to ES
	pop	es
	mov	di,offset rtfile ;pt at output file
	mov	cx,4		;WC
	repe	cmpsw		;match?
	pop	es		;[restore]
	jne	oscl5
	; found a file of the same name, delete it
	add	bx,cs:dirent	;skip file entry
	sub	bx,4		;pretend it's a .EMPTY. (size at [BX+2]
oscl4:	; .EMPTY. (or deleting file)
	mov	di,dx		;copy
	mov	word ptr [di],0	;.EMPTY.
	mov	si,[bx+2]	;get -length
	mov	[di+2],si	;save
	add	bx,4		;skip both
	add	dx,4
	jmp	short oscl1	;loop
oscl5:	; keep file
	mov	si,bx		;point at source
	mov	di,dx		;dest
	mov	cx,cs:dirent	;get size
	shr	cx,1		;/2
	rep	movsw		;copy
	mov	bx,si		;update
	mov	dx,di
	jmp	short oscl1
oscl6:	; finished deleting all files w/same name
	mov	cs:dirsiz,dx	;save updated dir size
	push	cs		;restore DS but leave ES
	pop	ds
	; create new entry (calc amount to slide dir down)
	mov	bx,ds:optr1	;get (relocated) ptr to output file entry
	mov	di,ds:dirent	;get dir entry size
	sub	di,4		;-4 bytes we already have (.EMPTY.)
	mov	ax,10000	;find positive value of size of .EMPTY. area
	sub	ax,es:[bx+2]
	and	ah,7777/400	;trim in case 0
	sub	ax,ds:owrtn	;-size of file
	jz	oscl7		;exact fit, easy
	 add	di,4		;we'll need 4 bytes for another .EMPTY.
oscl7:	; see if input file posn needs updating
	cmp	bp,ds:indev	;is it the right dev?
	jne	oscl8		;no
	mov	si,ss:actdir[bp] ;yes, get LD: base
	cmp	si,ds:savdir	;is it the input one?
	jne	oscl8
	cmp	ds:iptr1,bx	;is file after here?
	jb	oscl8
	add	ds:iptr1,di	;update
oscl8:	; slide rest of dir down to make space for new entry
	mov	si,ds:dirsiz	;point past end of dir
	add	ds:dirsiz,di	;(update)
	mov	cx,si		;find # bytes to move
	sub	cx,bx
	shr	cx,1		;/2=wc
	dec	si		;pt at last word
	dec	si
	add	di,si		;point past new end
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	std			;move backwards
	rep	movsw		;copy
	cld			;restore
	neg	ax		;find negative size of extra space
	and	ah,7777/400
	mov	[di+4],ax	;save smaller size in new area (if any)
	pop	ds		;restore
	; write new file entry
	mov	di,bx		;point at file entry
	mov	si,offset rtfile
	movsw			;copy filename
	movsw
	movsw
	movsw			;extension
	cmp	ds:dirent,5*2	;are there any info words?
	je	oscl10		;no
	; write date in first info word
	xor	ax,ax		;init to 0
	cmp	ds:fdatf,al	;do we know date?
	jz	oscl9		;no, leave it as 0 (=unknown)
	; convert date to OS/8 disk format (MMMMDDDDDYYY, YYY=MOD(YEAR-1970,8))
	mov	dx,ds:fyear	;get year
	sub	dx,1970d	;subtract base
	jnc	$+4
	 xor	dl,dl		;can't go earlier than 1970.
	and	dl,7		;isolate
	mov	al,ds:fday	;get day
	mov	cl,3		;bit count
	sal	al,cl		;shift into place
	or	al,dl		;OR in year offset
	mov	ah,ds:fmonth	;get month
oscl9:	stosw			;save
	mov	cx,ds:dirent	;get total dir entry size
	sub	cx,6*2		;subtract out filename, 1st info word, length
	xor	al,al		;load 0
	rep	stosb		;nuke out rest of info words (or not if CX=0)
oscl10:	mov	ax,10000	;-size
	sub	ax,ds:owrtn
	and	ah,7777/400	;trim in case 0
	stosw
	push	ds		;copy DS to ES
	pop	es
	callr	oswdir		;write dir back, return
;+
;
; Delete most recently DIRGETted file.  Called through SS:DELFIL[BP].
;
;-
osdl:	mov	al,ds:oinseg	;get old input seg #
	cbw			;AH=0 (really get it)
	call	osseg		;read it
	mov	ax,10000	;find positive # info words
	sub	ax,[si+8d]
	and	ah,7777/400	;may be 0
	add	ax,ax		;*2
	add	ax,5*2		;add in filename, extension, length
	mov	ds:dirent,ax	;save length in bytes
	mov	ds:ossdrt[si],1	;mark seg buf as dirty
	mov	bx,ds:oinent	;get offset
	lea	di,[bx+si]	;point at begn of file entry
	add	bx,ds:dirent	;point at whatever follows this entry
	mov	cx,256d*2+1	;block length, +1 (for length word)
	sub	cx,bx		;find # words to copy (include length word)
	lea	si,[bx+si-2]	;point at length word of this entry
	shr	cx,1		;word count
	xor	ax,ax		;mark this entry as .EMPTY.
	stosw
	rep	movsw		;copy length word, rest of segment
	cmp	ds:inent,cx	;at end of dir?
	js	osdl1		;yes, we're done
	; relocate INENT if still in this segment
	mov	al,ds:oinseg	;get seg # we're messing with
	cmp	al,ds:inseg	;still current seg even after DIRGET?
	jne	osdl1		;no, relax
	mov	ax,ds:dirent	;get size of file entry
	sub	ax,2*2		;subtract size of .EMPTY. we changed it to
	sub	ds:inent,ax	;back up INENT by the difference
osdl1:	ret
;+
;
; Read entire OS/8 directory into BIGBUF.
;
; ss:bp	log dev rec
; cx	returns length in bytes of dir read
;	(also recorded in DIRSIZ)
; DS:DIRENT  records length of file entry
;
; If the input file is on this volume, INPTR and IPTR1 are set to point at the
; file described by INDEV/SAVDIR+0/INSEG/INENT.  Same goes for OUTPTR and OPTR1
; based on OUTDEV/SAVDIR+2/OUTSEG/OUTENT.
;
;-
osrdir:	; init, load first seg
	xor	ax,ax		;load 0
	mov	ds:inptr,ax	;say files not yet found
	mov	ds:outptr,ax
	inc	ax		;get segment #1
	mov	ds:dirseg,al	;save
	call	osseg		;read it (AH=0)
	cmp	ds:bigsiz+2,0	;make sure buf can hold the whole dir
	jnz	ordr1		;definitely
	cmp	ds:bigsiz,5*2	;big enough for header?
	jb	ordr4
ordr1:	; copy 5-word header into BIGBUF
	mov	es,ds:bigbuf	;pt at buffer
	xor	di,di		;pt at begn
	mov	cx,5		;copy first 5 words
	rep	movsw		;go
	sub	si,5*2		;correct ptr
	mov	ax,10000	;find positive # info words
	sub	ax,[si+8d]
	and	ah,7777/400	;may be 0
	add	ax,ax		;*2
	add	ax,5*2		;add in filename, extension, length
	mov	ds:dirent,ax	;save length in bytes
ordr2:	; process next seg -- set up ptr to detect falling off end
	mov	bx,si		;copy
	neg	bx		;make negative for LEA later
	mov	ax,10000	;find positive # entries
	sub	ax,[si]
	and	ax,7777		;isolate, see if zero
	jz	ordr3		;zero, skip
	mov	ds:dircnt,ax	;save
	add	si,5*2		;pt past header
	lea	dx,[di+1000-12]	;-12 for header
	jmp	short ordr6
ordr3:	jmp	ordr11		;null segment, skip
ordr4:	jmp	nomem		;out of memory
ordr5:	jmp	baddir		;corrupt directory
ordr6:	; check next entry
	; SI=input ptr, DI=output ptr, BX=-<base of seg buf>, DX=max DI value
	; see if this is the current input entry
	cmp	bp,ds:indev	;is it the right dev?
	jne	ordr7		;no
	mov	ax,ss:actdir[bp] ;yes, get LD: base
	cmp	ax,ds:savdir	;is it the input one?
	jne	ordr7
	mov	al,ds:dirseg	;yes, get curr seg
	cmp	al,ds:inseg	;is it the curr input one too?
	jne	ordr7
	lea	ax,[bx+si]	;get relative ptr
	cmp	ax,ds:inent	;is this the input entry?
	jne	ordr7
	mov	ds:inptr,di	;save
ordr7:	; see if this is the current output entry
	cmp	bp,ds:outdev	;is it the right dev?
	jne	ordr8		;no
	mov	ax,ss:actdir[bp] ;yes, get LD: base
	cmp	ax,ds:savdir+2	;is it the output one?
	jne	ordr8
	mov	al,ds:dirseg	;yes, get curr seg
	cmp	al,ds:outseg	;is it the curr output one too?
	jne	ordr8
	lea	ax,[bx+si]	;get relative ptr
	cmp	ax,ds:outent	;is this the output entry?
	jne	ordr8
	mov	ds:outptr,di	;save
ordr8:	; copy dir entry
	mov	cx,4		;assume .EMPTY.
	cmp	word ptr [si],0	;is it?
	jz	ordr9		;yes
	 mov	cx,ds:dirent	;no, get length
ordr9:	mov	ax,di		;copy output ptr
	add	ax,cx		;what it will be if we do it
	cmp	di,dx		;space for a file entry?
	jae	ordr5		;no, invalid dir
	cmp	ds:bigsiz+2,0	;make sure we have space
	jnz	ordr10		;definitely
	cmp	ds:bigsiz,ax
	jb	ordr4		;no
ordr10:	shr	cx,1		;bc/2=wc
	rep	movsw		;copy
	dec	ds:dircnt	;done all in seg?
	jnz	ordr6		;loop if not
ordr11:	; end of segment
	neg	bx		;flip bx back
	mov	ax,[bx+4]	;get next dir seg
	test	ax,ax		;end?
	jz	ordr12
	mov	ds:dirseg,al	;update curr seg #
	push	di		;save
	push	es
	push	ds		;copy DS to ES
	pop	es
	cbw			;AH=0 (really read)
	call	osseg		;get next seg
	pop	es		;restore
	pop	di
	jmp	ordr2		;loop
ordr12:	mov	ds:dirsiz,di	;save
	push	ds		;restore ES
	pop	es
	mov	cx,di		;get length
	ret
;+
;
; Write entire OS/8 directory from BIGBUF (read by OSRDIR).
;
; ss:bp	log dev rec
; DS:DIRSIZ  gives size of dir in bytes
;
;-
oswdir:	mov	es,ds:bigbuf	;pt at buf
	; count up # segments needed to check for overflow
	mov	si,5*2		;point at first file
	mov	dl,1		;init segment
	xor	cx,cx		;nothing to add on first pass
oswd1:	mov	bx,5*2		;hypothetical posn in output segment
	add	bx,cx		;add curr file entry
oswd2:	cmp	si,ds:dirsiz	;at end of dir?
	je	oswd4		;yes, done
	mov	cx,2*2		;assume .EMPTY.
	cmp	word ptr es:[si],0 ;is it?
	jz	oswd3		;skip if so
	 mov	cx,ds:dirent
oswd3:	add	si,cx		;eat the entry
	add	bx,cx		;(pretend to) add to current segment
	cmp	bx,256d*2	;off end?
	jbe	oswd2		;no, loop
	inc	dx		;bump to new segment
	cmp	dl,6		;overflowed 6 segments?
	jbe	oswd1		;loop if not
	jmp	dirful		;dir is full
oswd4:	mov	byte ptr ds:dirseg,1 ;number of first dir seg
	mov	si,5*2		;pt past header
	mov	word ptr es:0,10000 ;init -(# entries)
	mov	word ptr es:4,0	;no next
	mov	word ptr es:6,0	;no tentative file
	push	ds		;restore ES
	pop	es
oswd5:	; copy next segment
	push	si		;save BIGBUF ptr
	mov	al,ds:dirseg	;seg #
	mov	ah,1		;alloc buf but don't read from disk
	call	osseg		;get buf
	mov	ds:ossdrt[si],1	;mark dirty
	mov	di,si		;copy ptr
	mov	bx,si		;save start of segment
	mov	ds,ds:bigbuf	;pt into buf
	; copy initial header
	xor	si,si		;get header from first 5 words
	mov	cx,5		;word count
	rep	movsw		;copy
	pop	si		;restore BIGBUF ptr
oswd6:	; copy next dir entry
	cmp	si,cs:dirsiz	;done?
	je	oswd10		;yes
	lea	ax,[bx+(256d*2)] ;point past end of segment buf
	mov	cx,2*2		;assume 2 words
	cmp	word ptr [si],0	;.EMPTY.?
	jz	oswd7		;yes, only two words
	 mov	cx,cs:dirent	;get dir entry size
oswd7:	sub	ax,cx		;back up
	cmp	di,ax		;will it fit?
	ja	oswd11		;no
	; see if this is the current input entry
	cmp	bp,cs:indev	;is it the right dev?
	jne	oswd8		;no
	mov	ax,ss:actdir[bp] ;get active LD:
	cmp	ax,ds:savdir	;is it the input one?
	jne	oswd8
	cmp	si,cs:iptr1	;yes, does ptr match?
	jne	oswd8
	mov	al,cs:dirseg	;save current dir seg
	mov	cs:inseg,al
	mov	ax,di		;save relative ptr
	sub	ax,bx
	mov	cs:inent,ax
oswd8:	; see if this is the current output entry
	cmp	bp,cs:outdev	;is it the right dev?
	jne	oswd9		;no
	mov	ax,ss:actdir[bp] ;get active LD:
	cmp	ax,ds:savdir+2	;is it the input one?
	jne	oswd9
	cmp	si,cs:optr1	;yes, does ptr match?
	jne	oswd9
	mov	al,cs:dirseg	;save current dir seg
	mov	cs:outseg,al
	mov	ax,di		;save relative ptr
	sub	ax,bx
	mov	cs:outent,ax
oswd9:	; copy dir entry
	shr	cx,1		;word count
	rep	movsw		;copy
	dec	word ptr es:[bx] ;update # entries in seg
	mov	ax,10000	;find positive size
	sub	ax,[si-2]
	and	ah,7777/400	;trim in case 0
	add	ds:2,ax		;update starting block #
	jmp	oswd6
oswd10:	; end of dir
	push	es		;restore ds
	pop	ds
	ret
oswd11:	; segment is full, chain to next
	push	es		;restore ds
	pop	ds
	inc	ds:dirseg	;bump seg #
	mov	al,ds:dirseg	;fetch it
	mov	[bx+4],al	;set link ptr
	jmp	oswd5		;loop
;+
;
; Get OS/8 dir segment.
;
; al	segment #
; ah	NZ to return buf but don't read if not cached (going to write it)
; ss:bp	log dev rec
;
; Returns ptr in SI.
;
;-
osseg:	test	al,al		;make sure valid
	jz	ossg2
	cmp	al,6		;must be in [1,6]
	ja	ossg2
	mov	dl,ah		;save flag
	cbw			;AH=0
	add	ax,ss:actdir[bp] ;add base of current LD: file (or 0 if none)
ossegb:	; enter here with AX=starting blk #, DL=flag
	mov	si,ss:dirbuf[bp] ;head of buffer chain
	xor	bx,bx		;no prev
	cmp	ds:ossblk[si],bx ;is this our first buf request?
	jz	ossg9		;yes, use our pre-alloced buf
ossg1:	; see if we already have it
	cmp	ax,ds:ossblk[si] ;is this it?
	je	ossg3		;yes, relink and return
	mov	cx,ds:ossnxt[si] ;get ptr to next
	jcxz	ossg5		;none, need to read it
	mov	bx,si		;save for linking
	mov	si,cx		;follow link
	jmp	short ossg1	;keep looking
ossg2:	jmp	baddir
ossg3:	; found it
	test	bx,bx		;is it first in line?
	jz	ossg4		;yes, no need to relink
	mov	ax,si		;pt at it
	xchg	ax,ss:dirbuf[bp] ;get head of chain, set new head
	xchg	ax,ds:ossnxt[si] ;link rest of chain to head
	mov	ds:ossnxt[bx],ax ;unlink the one we just moved
ossg4:	ret
ossg5:	; we don't have it, try to allocate a new buffer
	push	si		;save
	mov	cx,osslen	;# bytes
	call	askmem		;ask for a buffer
	pop	di		;[catch ptr to last buf in chain]
	jnc	ossg8		;got it, read the buf in
	mov	si,di		;restore
	; failed, write out the last one
	test	bx,bx		;is there a penultimate buffer?
	jz	ossg6		;no
	mov	ds:ossnxt[bx],0	;yes, unlink final buffer from it
	jmp	short ossg7
ossg6:	mov	ss:dirbuf[bp],bx ;this is the only one, unlink it
ossg7:	push	ax		;save blk
	push	dx		;and flag
	call	osfseg		;flush it
	pop	dx		;restore
	pop	ax
ossg8:	; link buf in as first
	mov	bx,si		;copy ptr
	xchg	bx,ss:dirbuf[bp] ;get current chain, set new head
	mov	ds:ossnxt[si],bx ;link it on
ossg9:	; buf is linked in at head of chain -- SI=buf, AL=seg to read, AH=flag
	mov	ds:ossblk[si],ax ;set blk #
	mov	ds:ossdrt[si],0	;buf is clean
	test	dl,dl		;should we read it?
	jnz	ossg10		;no, all we wanted was the buf
	xor	dx,dx		;high word=0
	mov	cx,1		;read 1 blk
	mov	di,si		;point at buffer
	push	si		;save
	call	ss:rdblk[bp]	;go
	pop	si		;[restore]
	jc	ossg11		;err
	mov	ax,10000	;12-bit negate
	sub	ax,[si+8d]	;get # info words per dir entry
	and	ah,7777/400	;may be 0
	sal	ax,1		;change to # bytes
	mov	ss:extra[bp],ax
ossg10:	ret
ossg11:	jmp	dirio
;+
;
; Flush OS/8 dir segment buffer, if dirty.
;
; ss:bp	dev rec
; ds:si	ptr to buffer (preserved on exit)
;
;-
osfseg:	cmp	ds:ossdrt[si],0	;is buf dirty?
	jz	osfsg1		;no
	mov	ax,ds:ossblk[si] ;get blk #
	xor	dx,dx		;high word=0
	mov	cx,1		;1 blk/seg
	push	si		;save
	call	ss:wrblk[bp]	;go
	pop	si		;[restore]
	jc	osfsg2		;err
osfsg1:	ret
osfsg2:	jmp	dirio
;
	subttl	TSS/8.24 file structure
;
	comment	_

I can dream, can't I!!!

OK the MFD starts at the very beginning of the data part of the disk, whose
location unfortunately depends on JOBMAX so we have to know that to be sure
we've found the right spot.  On my system, JOBMAX was 20., so after we skip 5
fields for TSS/8 itself (SI, FIP, INIT, TS8, TS8II), we skip 20. more fields
for the swap tracks (which are staticly assigned to the JOBMAX possible user
jobs in TSS/8), and the MFD starts right there at field 25. (segment #1).

This is all very similar to RSTS/E, which isn't surprising since some of the
TSS/8 folks worked on RSTS.  All the details are different of course.

MFDs and UFDs are made up of 8-word blockettes.  The MFD starts with a dummy
blockette:

	0;0;0;link
	0;0;0;0

Which points at the first real blockette (normally the entry for [0,1] at
offset 0010).

A name block for a file looks like this:
0,	FI	/filename (6 chars SIXBIT, i.e. ASCII minus 240 octal)
1,	LN
2,	AM
3,	link	/link to next file entry, or 0
4,	prot	/protection code in 7 LSBs, encoded extension in 5 MSBs
5,	size	/file size
6,	date	/creation date
7,	retr	/pointer to retrieval list for file data

A name block for an account (i.e. anything entered in [0,1] itself) is like
this:

0,	PPN	/P,PN (6,6 bits in spite of section F.3 claiming 7,5 bits)
1,	PS	/password (4 chars SIXBIT)
2,	WD
3,	link	/link to next entry, or 0
4,	12	/"grace space", amount user can go over quota, default=10.
5,	CPU	/CPU time used (another part of the manual switches fields 5/6)
6,	dev	/device time used
7,	retr	/pointer to retrieval list for UFD

Retrieval lists consist of one link word (0 if no more) and up to 7 retrieval
pointers, which each contain a segment number.  Segment numbers start at 1, not
0 (I guess all so that the MFD itself will have a valid segment number).  Link
words look like this:

  0  1   3 4        7
+---+-----+----------+
| / | SEG |   WORD   |
+---+-----+----------+

Where SEG is the relative segment number in the directory (0-6).

The SAT isn't a visible file, it's stored in the high end of FIP itself, in
field 1 of the disk (always).  FIP writes out the changed parts when necessary.
The SAT has a fixed size of 530 (octal) words, which is big enough for the max
config of four RS08 units.  A bit is clear when the corresponding segment is
available, set when it's in use or nonexistent.  Bits are numbered left to
right, naturally.  Variables that matter:

SATBOT=7250	bottom of SAT (in field 1) -- this word not used (mistake???)
SATCNT=7251	count of free segments on disk (must match # of zero bits)
SATPNT=7252	pointer to next word in SAT to check for non-zero bits
		(like SATPTR in RSTS)
7253-7777	the actual bitmap
_
;
tsds:	; set defaults
	mov	ax,ss:curdir[bp] ;get current PPN
	mov	ss:actdir[bp],ax ;set active PPN
	ret
;+
;
; Save TSS/8 CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
tssav:	mov	ax,ss:actdir[bp] ;get active dir
	mov	ds:savdir[bx],ax
	push	si		;save
	push	cx
	mov	cl,3		;shift count
	sal	bx,cl		;buf size = 8 words
	lea	si,ss:dirwnd[bp] ;pt at retrieval pointers
	lea	di,ds:savwnd[bx] ;pt at save buffer
	mov	cx,7		;copy 7 words
	rep	movsw		;copy
	pop	cx		;restore
	pop	si
	ret
;+
;
; Restore TSS/8 CWD.
;
; bx	0 for input dev, 2 for output dev
;
;-
tsrst:	mov	ax,ds:savdir[bx] ;get saved stuff
	mov	ss:actdir[bp],ax ;set active
	mov	cl,3		;shift count
	sal	bx,cl		;buf size = 8 words
	lea	si,ds:savwnd[bx] ;pt at save buffer
	lea	di,ss:dirwnd[bp] ;pt at retrieval pointers
	mov	cx,7		;copy 7 words
	rep	movsw		;copy
	ret
;
tsdh:	; print directory heading
	xor	ax,ax		;load 0
	mov	ds:nfile,ax	;no files yet
	mov	ds:nblk,ax	;no blocks seen
	mov	ds:nblk+2,ax
	mov	di,offset lbuf	;pt at buf
	cmp	word ptr ss:actdir[bp],0001 ;[0,1]?
	je	tsdh1		;yes, UFDs
	cram	' Name .Ext  Size    Prot       Date'
	test	byte ptr ds:dirflg,200 ;verbose?
	jz	tsdh2		;no
	cram	'       Pos'
	jmp	short tsdh2	;skip
tsdh1:	cram	' PPN  Password' ;heading for MFD
tsdh2:	callr	flush		;print, return
;
tsdi:	; TSS/8 dir input init
	cmp	ss:initf[bp],0	;already initted since last prompt?
	jnz	tsdi1		;yes
	inc	ss:initf[bp]	;no, but it will be
	; allocate first dir blk buffer (nothing is possible if this fails)
	mov	cx,rdslen	;size of dir buffer
	call	getmem		;allocate dir buffer
	mov	ss:dirbuf[bp],si ;save ptr
	xor	ax,ax		;load 0
	mov	ds:rdsbkl[si],ax ;nothing in here yet
	mov	ds:rdsbkh[si],ax
	mov	ds:rdsdrt[si],al ;not dirty
	mov	ds:rdsnxt[si],ax ;no next
tsdi1:	; find dir and fetch retrieval
	mov	ss:ecache[bp],1	;enable caching
	mov	ax,ss:dirwnd[bp] ;get retrieval ptr for first dir block
	dec	ax		;-1
	add	ax,ss:mfddcn[bp] ;add base
	xor	dx,dx		;zero-extend
	xor	cl,cl		;really read it
	call	rsrdir		;read dir block
	mov	ax,[si+(3*2)]	;get ptr to first entry
	mov	ds:inent,ax	;save
	mov	word ptr ds:oinent,0 ;no previous
	ret
;+
;
; Find directory.
;
; ss:bp	log dev rec (SS:ACTDIR[BP] contains PPN to find)
; ax	returns starting block # of dir
;
;-
tsfd:	; fetch MFD retrieval
	mov	ax,ss:mfddcn[bp] ;get start of MFD
	xor	dx,dx		;zero-extend
	mov	ds:rsblk,ax	;save
	mov	ds:rsblk+2,dx
	mov	cx,1		;count=1
	mov	di,offset buf	;pt at buffer
	call	ss:rdblk[bp]	;read in dir block
	jc	tsfd3		;error
	xor	bx,bx		;initial offset
tsfd1:	; search for [0,1] entry (normally at offset +10)
	mov	bx,[si+bx+(3*2)] ;get link to next entry at +3
	and	bx,7770		;isolate
	jz	tsfd2		;can't be end of list
	test	bh,bh		;must still be in block 0 of MFD
	jnz	tsfd2		;isn't
	add	bx,bx		;*2
	cmp	word ptr [si+bx],0001 ;is this entry for [0,1]?
	jne	tsfd1		;loop if not
	; found it, find retrieval window for the MFD itself
	mov	bx,[si+bx+(7*2)] ;get link to retrieval at +7
	and	bx,7770		;isolate
	jz	tsfd2		;nonexistent
	test	bh,bh		;must still be in block 0 of MFD
	jnz	tsfd2		;isn't
	add	bx,bx		;*2
	; copy retrieval pointers into DIRWND, so that TSLINK works
	push	es		;save
	push	ss		;copy SS to ES
	pop	es
	lea	si,[si+bx+2]	;pt at 7 retrieval ptrs (following link word)
	lea	di,ss:dirwnd[bp] ;point at dir window
	mov	cx,7		;word count
	rep	movsw		;copy
	pop	es		;restore
	; search UFD list for this UFD
	xor	ax,ax		;point at first entry
	call	tslnkn		;fetch link to first UFD entry
	jnc	tsfd5		;got it, go
tsfd2:	jmp	baddir		;what the hell
tsfd3:	jmp	dirio		;dir read error
tsfd4:	jmp	dnf		;dir not found
tsfd5:	mov	ax,[si+(3*2)]	;get link to next
tsfd6:	call	tslink		;follow link to next
	jc	tsfd4
	mov	ax,ss:actdir[bp] ;get PPN to search for
	cmp	ax,[si]		;is this it?
	jne	tsfd5		;loop if not
	mov	ax,[si+(7*2)]	;get link to retrieval blocks
	call	tslink		;follow it
	jc	tsfd4		;nonexistent
	; copy retrieval pointers into DIRWND
	push	es		;save
	push	ss		;copy SS to ES
	pop	es
	lea	si,[si+2]	;pt at 7 retrieval ptrs (following link word)
	lea	di,ss:dirwnd[bp] ;point at dir window
	mov	cx,7		;word count
	rep	movsw		;copy
	pop	es		;restore
	clc			;happy
	ret
;
tsdn:	; display TSS/8 dir name
	mov	bx,ss:actdir[bp] ;get PPN
tsdn1:	; enter from TSDG with PPN in BX
	mov	al,'['		;[
	stosb
	mov	ax,bx		;proj
	mov	cl,6		;shift count
	shr	ax,cl		;right-justify
	push	bx		;save
	call	poctv		;display
	mov	al,','		;,
	stosb
	pop	ax		;catch prog
	and	ax,77		;isolate
	call	poctv
	mov	al,']'		;]
	stosb
	ret
;
tscd:	; change TSS/8 dir
	call	tsgd		;get dir
	call	confrm		;make sure confirmed
	mov	ax,ss:actdir[bp] ;get active dir, make current
	mov	ss:curdir[bp],ax
	ret
;
tsgd:	; get dir name
	jcxz	tsgd4		;nothing, skip
	cmp	byte ptr [si],'[' ;open bracket?
	jne	tsgd2		;no
	inc	si		;yes, eat it
	dec	cx
	call	goctb		;get project
	mov	bh,ah		;save
	and	bx,77*100h	;isolate
	cmp	al,','		;comma?
	jne	tsgd1
	inc	si		;yes, eat it
	dec	cx
	push	bx		;(save)
	call	goctb		;get programmer
	pop	bx		;(restore)
	and	ah,77		;isolate
	shr	bx,1		;move proj into place
	shr	bx,1
	or	bl,ah		;save prog
	cmp	al,']'		;close bracket?
	je	tsgd3		;yes
tsgd1:	jmp	synerr
tsgd2:	; look for one-character logical -- should be * but that confuses our
	; wildcard parser, so I'll substitute $ (like RSTS/E)
	mov	bx,0002		;[0,2]
	cmp	al,'$'		;$
	jne	tsgd4
tsgd3:	mov	ss:actdir[bp],bx ;save
	inc	si		;eat delimiter
	dec	cx
tsgd4:	; find dir
	push	si		;save
	push	cx
	call	tsfd		;find directory, load DIRWND
	pop	cx		;restore
	pop	si
	ret
;+
;
; Parse TSS/8 filename and directory.
;
; RSTS allows the PPN to come either before or after the filename.
;
;-
tsgf:	; get filename
	push	bx		;save
	push	di
	call	skip		;skip white space
	push	si		;save
	call	tsgd		;get directory
	pop	ax		;restore starting ptr
	pop	di		;restore
	pop	bx
	sub	ax,si		;NZ if did parse dir name
	push	ax		;save
	call	filnam		;parse filename
	pop	ax		;restore flag
	test	ax,ax		;did we already get a dir name?
	jnz	tsgf1		;yes
	callr	tsgd		;no, try now, return
tsgf1:	ret
;
tsdd:	; display TSS/8 dir entry
	mov	ax,"  "		;4 blanks
	stosw
	stosw
	; file size (6-column field)
	push	di		;save
	mov	ax,ds:fsize	;get file size
	add	ds:nblk,ax	;bump # blocks
	inc	word ptr ds:nfile ;bump # files
	call	pnum		;print it
	pop	cx		;restore starting posn
	add	cx,6		;column we're shooting for
	sub	cx,di		;find # to go
	mov	al,' '		;load blank
	rep	stosb		;pad to 6 columns
	; file protection (8-column field)
	push	di		;save
	mov	al,ds:fprot	;get file protection
	and	al,177		;trim to 7 bits (only 5 actually used)
	call	poctv		;print it
	pop	cx		;as above, pad to 8 columns
	add	cx,8d
	sub	cx,di
	mov	al,' '
	rep	stosb
	; file creation date
	call	pdate		;date of creation
	; starting segment #
	test	byte ptr ds:dirflg,200 ;verbose?
	jz	tsdd1		;no
	mov	ax,ds:iretr	;get retrieval pointer
	push	di		;save
	call	tslink		;fetch
	pop	di		;[restore]
	jc	tsdd2		;corrupt dir
	inc	si		;skip link
	inc	si
	lodsw			;get ptr
	xor	dx,dx		;0-extend
	mov	cx,6		;field size
	callr	cnum		;print, return
tsdd1:	ret
tsdd2:	jmp	baddir		;missing retrieval list, corrupt dir
;
tsde:	; display "empty" dir entry (we use for UFD entries in [0,1])

;;; add accounting stuff?

	ret
;
tsdg:	; get TSS/8 dir entry
	mov	ax,ds:inent	;get ptr to next entry
	call	tslink		;follow it
	jnc	tsdg1
	 ret
tsdg1:	mov	ax,[si+(3*2)]	;fetch link to next
	xchg	ax,ds:inent	;save link, get old value
	xchg	ax,ds:oinent	;save link to this entry, get prev
	mov	ds:oinent+2,ax	;save prev
	mov	di,offset lbuf	;pt at LBUF
	cmp	word ptr ss:actdir[bp],0001 ;is dir [0,1]?
	jne	tsdg2		;no
	; all entries in [0,1] are actually UFDs
	; 0/ PPN
	lodsw			;get it
	push	si		;save
	push	di
	mov	bx,ax		;copy
	call	tsdn1		;display it
	pop	cx		;restore
	pop	si
	add	cx,8d		;pad to 8 columns
	sub	cx,di		;find # to go
	mov	al,' '		;blanks
	rep	stosb
	lodsw			;PSWD
	call	sixbit
	lodsw
	call	sixbit
	xor	ax,ax		;CF=0, ZF=1 (doesn't count as file)
	ret
tsdg2:	; 0/ filename (3 words SIXBIT)
	lodsw			;FILNAM
	call	sixbit
	lodsw
	call	sixbit
	lodsw
	call	sixbit
	mov	al,'.'		;.
	stosb
	; 3/ link to next file entry
	lodsw			;skip link to next name (already done)
	; 4/ protection code, encoded filename extension
	lodsw			;get it
	mov	ds:fprot,al	;7 LSBs are protection code
	add	ax,ax		;5-bit filename extension code in AH
	push	si		;save
	mov	si,offset putext-3 ;point at extension list -3
tsdg3:	add	si,3		;skip to next (or first) entry
	lodsb			;get next entry
	test	al,al		;end of list?
	js	tsdg4
	cmp	al,ah		;match?
	jne	tsdg3		;loop if not
tsdg4:	movsw			;copy
	movsb
	pop	si		;restore
	; 5/ file size
	lodsw			;get size
	mov	ds:fsize,ax	;save
	mov	bx,256d*2	;# bytes per segment (256 words) in binary mode
	cmp	ds:binflg,bin	;binary mode?
	je	tsdg5
	 mov	bx,384d		;no, # characters per segment
tsdg5:	mul	bx		;find # 8-bit bytes in file
	mov	ds:fbcnt,ax	;save
	mov	ds:fbcnt+2,dx
	; 6/ creation date
	lodsw			;fetch creation date
	; TSS/8.24 date format (base year is vers-dependent, originally 1964):
	; ((YEAR-1974.)*12.+MONTH-1)*31.+DAY-1
	xor	dx,dx		;zero-extend
	mov	bx,31d		;divide out days
	div	bx
	inc	dx		;rem +1
	mov	ds:fday,dl	;save day
	mov	bl,12d		;divide out months
	cwd			;DX=0
	div	bx
	inc	dx		;rem +1
	mov	ds:fmonth,dl	;save month
	add	ax,1974d	;add base year (was 1964. in old TSS/8)
	mov	ds:fyear,ax
	mov	ds:fdatf,dl	;set flag
	mov	ds:ftimf,dh	;don't know time
	; 7/ link to retrieval list (always exists, no null files in TSS/8)
	lodsw			;get link to first retrieval
	mov	ds:iretr,ax	;save
	mov	di,offset lbuf+10d ;restore DI
	callr	sqfn		;squish filename, return
;
tsdf:	; TSS/8 dir finish (flush dirty dir buffers)
	mov	si,ss:dirbuf[bp] ;get ptr to buf chain
tsdf1:	call	rswdir		;flush
	mov	si,ds:rdsnxt[si] ;follow chain
	test	si,si		;anything?
	jnz	tsdf1		;loop if so
	ret
;
tssum:	; TSS/8 dir listing summary
	mov	di,offset lbuf	;pt at buf
	call	flush		;blank line
	cram	'Total disc segments:  ' ;that's how CAT.SAV spells it
	mov	ax,ds:nblk	;get # blocks
	call	pnum		;display 32-bit number
	callr	flush		;flush, return
;
tsop:	; open TSS/8 file for input
	mov	ax,ds:fsize	;get size
	mov	ds:isize,ax	;save
	mov	ds:irptr,8d*2	;init retrieval ptr to follow link to first
	ret
;+
;
; Read file data.
;
; On entry:
; es:di	ptr to buf (normalized)
; cx	# bytes free in buf (FFFF if >64 KB)
;
; On return:
; cx	# bytes read (may be 0 in text mode if block was all NULs etc.)
;
; CF=1 if nothing was read:
;  ZF=1 end of file
;  ZF=0 buffer not big enough to hold anything, needs to be flushed
;
;-
tsrd:	push	di		;save starting offset
tsrd1:	; try to add a segment, DI=buf ptr, CX=# free bytes
	test	word ptr ds:isize,-1 ;anything left?
	jz	tsrd6		;no
	mov	bx,512d		;# bytes to store one 256-word segment
	cmp	word ptr ds:binflg,bin ;binary mode?
	je	tsrd2
	 mov	bx,384d		;# ASCII chars in one 256-word segment
tsrd2:	cmp	cx,bx		;will it fit?
	jb	tsrd6		;no, need to flush buffer
	push	cx		;save free count
	mov	bx,ds:irptr	;get retrieval ptr
	cmp	bx,8d*2		;off end of window?
	jb	tsrd3		;no
	; window turn
	push	di		;save
	push	es
	push	ds		;copy DS to ES
	pop	es
	mov	ax,ds:iretr	;get link to next
	call	tslink		;follow it
	jc	tsrd4		;shouldn't happen!
	mov	di,offset iretr	;point at retrieval window buf
	mov	cx,8d		;blockette size
	rep	movsw		;copy
	pop	es		;restore
	pop	di
	mov	bx,1*2		;point at first retrieval word
tsrd3:	mov	ax,ds:iretr[bx]	;look up retrieval word
	inc	bx		;+2 to next retrieval pointer
	inc	bx
	mov	ds:irptr,bx	;update
	sub	ax,1		;segments start at 1 not 0
	jc	tsrd4		;null retrieval ptr before official EOF, err
	add	ax,ss:mfddcn[bp] ;add base of data part of disk
	xor	dx,dx		;zero-extend
	mov	cx,1		;read one block
	mov	si,ds:binflg	;get binary flag
	call	ss:rdblk[bp+si]	;read data
	jc	tsrd5		;read error
	mov	bx,cx		;copy actual count out of the way
	pop	cx		;restore requested count
	mov	di,si		;copy addr of data read
	add	di,bx		;update
	sub	cx,bx		;deduct from count
	dec	word ptr ds:isize ;count the segment
	jmp	short tsrd1	;continue unless EOF
tsrd4:	jmp	baddir
tsrd5:	jmp	rderr
tsrd6:	mov	cx,di		;get current ptr
	pop	di		;restore starting addr
	sub	cx,di		;find actual count read
	jnz	tsrd7		;success, CF=0
	cmp	ds:isize,cx	;anything left?  ZF=1 if not (CX=0)
	stc			;EOF or EOB
tsrd7:	ret
;+
;
; Follow TSS/8 dir link.
;
; ax	link to follow
; si	points to destination blockette on return
; bx	points to base of dir block buf on return (so can set "dirty" flag)
; CF=1	if no next blockette
;
; link (these bit #s are in 80x86 order):
; 11	not used?
; 10:8	relative blk # in dir (0-6)
; 8:3	blockette (8. words) within block
; 2:0	seem to be always 0 (addresses are multiples of 8)
;
;-
tslink:	test	ax,ax		;end?
	jz	tslnk1		;yes
tslnkn:	; same with no end-of-chain check (so can address blockette 0)
	push	ax		;save
	mov	bl,ah		;copy blk #
	add	bl,bl		;*2
	and	bx,7*2		;isolate it
	cmp	bl,7*2		;make sure it's blk 0-6
	je	tslnk2		;no, error
	add	bx,bp		;add base of log dev record
	mov	ax,ss:dirwnd[bx] ;look up retrieval
	sub	ax,1		;off by 1
	jc	tslnk2		;nonexistent block, error
	add	ax,ss:mfddcn[bp] ;add base of disk
	xor	dx,dx		;zero-extend
	xor	cl,cl		;really read it
	call	rsrdir		;get the block
	pop	bx		;restore
	add	bx,bx		;*2 to get byte offset
	and	bx,370*2	;isolate byte offset
	xchg	bx,si		;swap (so BX=base of block)
	add	si,bx		;add it in (CF=0)
	ret
tslnk1:	stc			;end of chain
	ret
tslnk2:	jmp	baddir		;link was to nonexistent block
;
	subttl	WPS-8 file structure
;
	comment	_

My understanding of WPS-8 disks is based on the DMPROCES program, written by
Kevin Handy and modified by David Gesswein (his version is on ftp.pdp8.net).

Disk blocking appears to be similar to OS/8, each block is 256 words.  The
first two words of each block seem to not be part of the data stream, maybe
checksums or something?????

Index is on block 2:

000/	?	(7401 in the disk I'm looking at)
001/	?	(0230 " " " " " ")
002/	6-character volume name, add ^O037 to each 6-bit byte to get ASCII
003/
004/
005/	?
006/	?
007/	?
010/	?
011/	?
012/	index starts here, each slot holds starting block # of file header,
	or 0 for empty slots (80. or 90. slots, depending whether RX01 or RX50)

Index block contains retrieval pointers, optionally links to more index blocks.
Filename etc. appears to be embedded in text?!?!?!

	n>filename<
	#>file ID<
	<> no op???
_


	subttl	random utility routines
;+
;
; Read a line from the keyboard into LBUF.
;
; si,cx	returns descriptor of line
;
;-
gtlin:	mov	al,ds:lmax	;reset **MORE** count
	mov	ds:lnum,al
	mov	di,offset lbuf	;pt at buffer
	test	ds:indhnd,-1	;is there an indirect file open?
	jns	gtln1		;yes, use it instead of keyboard
	mov	dx,offset kbbuf	;pt at buffer
	mov	ah,0Ah		;func=read line
	int	21h		;do it
	mov	dl,lf		;echo LF (DOS didn't)
	mov	ah,02h		;func=CONOUT
	int	21h
	mov	si,offset kbbuf+1 ;pt at length
	lodsb			;get length
	cbw			;AH=0
	mov	cx,ax		;copy length
	mov	bx,di		;copy buf ptr
	rep	movsb		;copy the line into LBUF
	mov	si,bx		;point at it
	mov	cx,ax		;get length
	ret
gtln1:	; reading from indirect file, get next line
	mov	cx,ds:indctr	;get # bytes left in buf
	jcxz	gtln4		;none, refill buffer
	mov	si,ds:indptr	;get buf ptr
gtln2:	lodsb			;get a byte
	cmp	al,cr		;carriage return?
	je	gtln3		;ignore if so
	cmp	al,lf		;end of line?
	je	gtln5		;yes
	cmp	di,offset lbuf+80d ;buf full?
	je	gtln3
	 stosb			;save in LBUF if it will fit
gtln3:	loop	gtln2		;loop
gtln4:	; refill file buf
	mov	dx,offset indbuf ;point at buf
	mov	si,dx		;rewind SI too
	mov	cx,indsiz	;size of buf
	mov	bx,ds:indhnd	;handle
	mov	ah,3Fh		;func=read
	int	21h
	jc	gtln6
	mov	cx,ax		;copy length
	test	ax,ax		;end of file?
	jnz	gtln2		;no, back into loop
	mov	ah,3Eh		;func=close
	int	21h
	mov	ds:indhnd,-1	;mark as closed
	cmp	di,offset lbuf	;had we read a partial line?
	je	gtlin		;no, get one from the keyboard
gtln5:	; process line
	dec	cx		;count the LF (or not if EOF)
	mov	ds:indptr,si	;save
	mov	ds:indctr,cx
	mov	cx,di		;get current ptr
	mov	si,offset lbuf	;point at begn of line
	sub	cx,si		;find length
	mov	dx,si		;point at it
	mov	bx,0001h	;handle=STDOUT
	mov	ah,40h		;func=print
	int	21h		;no point in worrying about errors
	mov	dl,cr		;echo <CRLF>
	mov	ah,02h
	int	21h
	mov	dl,lf
	mov	ah,02h
	int	21h
	ret
gtln6:	; "@" file read error
	mov	ah,3Eh		;func=close
	int	21h
	mov	ds:indhnd,-1	;mark as closed
	cram	'?Indirect file read error',error
;+
;
; Ask user if they're sure (abort if not).
;
;-
aysure:	cram	'Are you sure (Y/N)? ',wrng
	call	gtlin		;read a line
	call	getw		;get their answer
	jc	aysure		;if any
	cmp	byte ptr [bx],'Y' ;does it start with 'Y'?
	jne	$+3
	 ret
	cram	'?Command canceled',error
;+
;
; Wait for user to press ENTER.
;
;-
prsent:	cram	'Press ENTER when ready...',wrng
prsen1:	mov	ah,08h		;func=unbuffered input
	int	21h
	cmp	al,cr		;cr?
	jne	prsen1		;loop until it is (or ^C)
	mov	dx,offset crlfs	;pt at string
	mov	cx,2		;length
	mov	bx,0002h	;handle=STDERR
	mov	ah,40h		;func=write
	int	21h
	ret
;+
;
; Get response to yes-or-no question.
;
; Return CF=0 on 'Y', CF=1 on anything else.
;
;-
yorn:	call	gtlin		;read a line
	call	getw		;get their answer
	jc	yorn1		;if any
	cmp	byte ptr [bx],'Y' ;does it start with 'Y'?
	je	yorn1
	 stc
yorn1:	ret
;+
;
; Cram in-line string into output buf.
;
;-
cram1:	mov	cx,si		;save SI
	pop	si		;restore source ptr
	lodsb			;get length
	cbw			;AH=0
	xchg	cx,ax		;copy length, get original SI
	rep	movsb		;copy string
	xchg	ax,si		;OK, now restore SI
	jmp	ax		;return
;+
;
; Convert byte (AL) to decimal in output buf (ES:DI).
;
;-
pbyte:	cmp	al,10d		;small enough to just print?
	jb	pbyte1
	aam			;no, split it
	push	ax		;save remainder
	mov	al,ah		;get quotient
	call	pbyte		;recurse
	pop	ax		;restore
pbyte1:	or	al,'0'		;convert to decimal
	stosb			;save
	ret
;+
;
; Convert word (AX) to decimal in output buf (ES:DI).
;
;-
pnum:	mov	bx,10d		;divisor
pnum1:	cmp	ax,bx		;small enough to just print?
	jb	pnum2
	xor	dx,dx		;0-extend
	div	bx		;divide
	push	dx		;save remainder
	call	pnum1		;recurse
	pop	ax		;restore
pnum2:	or	al,'0'		;convert
	stosb
	ret
;+
;
; Convert doubleword (DX:AX) to decimal in output buf (ES:DI).
;
;-
pdnum:	mov	bx,10d		;radix
pdnum1:	test	dx,dx		;anything in high-order?
	jnz	pdnum2		;yes
	cmp	ax,bx		;small enough to just print?
	jb	pdnum3		;yes, skip all this
pdnum2:	mov	cx,ax		;copy out of the way
	mov	ax,dx		;copy high order
	xor	dx,dx		;zero-extend
	div	bx		;AX=high word of quotient, DX=rem
	xchg	ax,cx		;get low word of dividend
	div	bx		;AX=low word of quotient, DX=rem
	push	dx		;save remainder
	mov	dx,cx		;DX:AX=quotient
	call	pdnum1		;recurse
	pop	ax		;restore
pdnum3:	or	al,'0'		;convert to digit
	stosb			;save
	ret
;+
;
; Convert quadword (DX:CX:BX:AX) to decimal in output buf (ES:DI).
;
;-
pqnum:	mov	si,dx		;anything in high-order?
	or	si,cx
	or	si,bx
	mov	si,10d		;[load radix]
	jnz	pqnum1		;yes
	cmp	ax,si		;small enough to just print?
	jb	pqnum2		;yes, skip all this
pqnum1:	; do long division
	push	bp		;save
	mov	bp,ax		;save high order
	mov	ax,dx		;copy
	xor	dx,dx		;zero-extend
	div	si		;AX=high word of quotient, DX=intermediate rem
	push	ax		;save new DX value
	mov	ax,cx		;next one down
	div	si
	mov	cx,ax		;save
	mov	ax,bx		;3rd
	div	si
	mov	bx,ax
	mov	ax,bp		;4th
	div	si
	mov	bp,dx		;save remainder
	pop	dx		;restore original quotient
	call	pqnum		;recurse
	mov	ax,bp		;restore
	pop	bp
pqnum2:	or	al,'0'		;convert to digit
	stosb			;save
	ret
;+
;
; Convert CX-digit word (AX) to octal in output buf (ES:DI).
;
;-
poct:	add	di,cx		;move to end of buf
	mov	bx,di		;copy
poct1:	mov	dl,al		;get digit
	and	dl,7
	or	dl,'0'		;convert to ASCII
	dec	bx		;store
	mov	[bx],dl
	shr	ax,1		;/8
	shr	ax,1
	shr	ax,1
	loop	poct1		;loop
	ret
;+
;
; Convert word (AX) to variable-width octal in output buf (ES:DI).
;
;-
poctv:	test	ax,not 7	;small enough to just print?
	jz	poctv1
	push	ax		;no, save remainder
	mov	cl,3		;shift count
	shr	ax,3		;right 3 bits
	call	poctv		;recurse
	pop	ax		;restore
	and	al,7		;isolate low byte
poctv1:	or	al,'0'		;convert to decimal
	stosb			;save
	ret
;+
;
; Convert number to decimal, right-justified.
;
; dx:ax	number to print
; cx	size of field
; di	pts to buffer
;
;-
cnum:	mov	bl,al		;save
	mov	al,' '		;load blank
	rep	stosb		;fill field
	mov	al,bl		;restore
	mov	si,di		;copy ptr
cnum1:	cmp	dx,9d		;number .le. 655359?
	jbe	cnum4		;yes, goody (use hardware divide)
	; result won't fit in 16 bits (quo.le.65535, rem.le.9)
	; do the division in software
	; (I wrote this routine long before realizing that it can be easily
	; done as long division with two DIVs since the divisor still fits in
	; 16 bits)
	xor	bl,bl		;init rem
	mov	cx,32d		;bit count
cnum2:	shl	ax,1		;shift
	rcl	dx,1		;into dx
	rcl	bl,1		;into bl
	cmp	bl,10d		;will it fit?
	jb	cnum3		;no
	sub	bl,10d		;yes
	inc	ax		;shift in a 1
cnum3:	loop	cnum2		;loop
	or	bl,'0'		;convert to ascii
	dec	si		;-1
	mov	[si],bl		;put in buf
	jmp	short cnum1	;loop
cnum4:	mov	cl,10d		;load a 10
cnum5:	div	cx		;divide
	or	dl,'0'		;convert to ascii
	dec	si		;-1
	mov	[si],dl		;put in buf
	xor	dx,dx		;DX=0
	test	ax,ax		;anything left?
	jnz	cnum5		;yes, loop
	ret
;+
;
; Print the time in FHOUR, FMIN, FSEC (unless negative).
;
;-
ptime:	mov	bx,"00"		;we'll need this
	mov	al,ds:fhour	;get hour
	aam			;split digits
	xchg	al,ah		;><
	or	ax,bx		;make ASCII
	stosw			;save
	mov	al,':'		;:
	stosb
	mov	al,ds:fmin	;minute
	aam
	xchg	al,ah
	or	ax,bx
	stosw
	mov	al,':'		;:
	stosb
	mov	al,ds:fsec	;second
	test	al,al		;negative?
	js	ptime1		;yes, file system doesn't support seconds
	aam			;OK, go ahead
	xchg	al,ah
	or	ax,bx
	stosw
	ret
ptime1:	dec	di		;unput the ':'
	ret
;+
;
; Print the date in FYEAR, FMONTH, FDAY.
;
;-
pdate:	mov	ax,word ptr ds:fday ;get month,,day
	mov	dx,ds:fyear
pdate1:	; use DX=year, AH=month, AL=day
	mov	bl,ah		;save month
	aam			;split the digits
	xchg	al,ah		;><
	or	ax,"00"		;convert
	stosw			;save
	mov	al,'-'		;-
	stosb
	mov	al,bl		;copy month
	add	al,bl		;*2
	add	al,bl		;*3
	cbw			;AH=0
	add	ax,offset months ;pt at table
	mov	si,ax		;with si
	movsw			;2 bytes
	movsb			;3rd
	mov	al,'-'		;-
	stosb
	mov	ax,dx		;get year
	callr	pnum		;print, return
;+
;
; Print a logical device name.
;
; bp	dev rec
; di	buffer (updated on exit)
;
;-
pdev:	mov	ax,ss:logd[bp]	;get name
	mov	bx,ss:logu[bp]	;and unit info
	test	al,al		;got a 1st letter?
	jz	$+3
	 stosb			;yes
	mov	[di],ah		;save 2nd (or only) letter
	inc	di
	test	bh,bh		;is there a unit #?
	jz	pdev1
	mov	al,bl		;yes, get it
	call	pbyte		;convert
pdev1:	mov	al,':'		;:
	stosb
	ret
;+
;
; Print volume name.
;
;-
pvol:	mov	di,offset lbuf	;pt at buf
	call	flush		;blank line
	cram	' Volume in drive '
	call	pdev		;print dev name
	dec	di		;unput ':'
	call	ss:volnam[bp]	;print vol name
	callr	flush		;print line, return
;+
;
; Print dir name and file spec for common OSes where dir comes first.
;
; es:di	buffer (updated on return)
; ds:si	.ASCIZ filename
;
;-
pspec:	push	si		;save
	mov	cx,1		;filename follows dir
	call	ss:dirnam[bp]	;print dir name
	pop	si		;restore
pspec1:	lodsb			;get a byte
	test	al,al		;end?
	jz	pspec2
	stosb			;store
	jmp	short pspec1
pspec2:	ret
;+
;
; Print name corresponding to a value.
;
; bx	value to look up
; si	pointer to list of:
;	DW value
;	DB length,'string'
;	list must contain all possible values (otherwise will run off end)
; di	buffer (updated on exit)
;
;-
pname:	lodsw			;get an entry
	cmp	ax,bx		;is this it?
	lodsb			;[get length]
	cbw			;[zero extend, they're all <128]
	je	pname1		;this is it
	add	si,ax		;skip it
	jmp	short pname
pname1:	mov	cx,ax		;copy
	rep	movsb		;copy string into buffer
	ret
;
symnam	macro	sym,nam		;;define record giving symbol and its name
	local	a,b
	dw	&sym
a	db	b,&nam
b=	$-a-1
	endm
;
fsynam	label	byte		;file system names
	symnam	frgn,'FOREIGN'
	symnam	cos310,'COS310'
	symnam	dosbat,'DOS/BATCH'
	symnam	ods1,'Files-11'
	symnam	os8,'OS/8'
	symnam	ps8,'PS/8'
	symnam	putr,'PUTR'
	symnam	rsts,'RSTS/E'
	symnam	rt11,'RT-11'
	symnam	tss8,'TSS/8'
	symnam	xxdp,'XXDP+'
;
devnam	label	byte		;device type names
	symnam	mscp,'MSCP'	;arbitrary block image
	symnam	ra60,'RA60'
	symnam	ra70,'RA70'
	symnam	ra71,'RA71'
	symnam	ra72,'RA72'
	symnam	ra73,'RA73'
	symnam	ra80,'RA80'
	symnam	ra81,'RA81'
	symnam	ra82,'RA82'
	symnam	ra90,'RA90'
	symnam	ra92,'RA92'
	symnam	rd32,'RD32'
	symnam	rd51,'RD51'
	symnam	rd52,'RD52'
	symnam	rd53,'RD53'
	symnam	rd54,'RD54'
	symnam	rk02,'RK02'
	symnam	rk05,'RK05'
	symnam	rk06,'RK06'
	symnam	rk07,'RK07'
	symnam	rl01,'RL01'
	symnam	rl02,'RL02'
	symnam	rm02,'RM02'
	symnam	rm03,'RM03'
	symnam	rm05,'RM05'
	symnam	rm80,'RM80'
	symnam	rp02,'RP02'
	symnam	rp03,'RP03'
	symnam	rp04,'RP04'
	symnam	rp05,'RP05'
	symnam	rp06,'RP06'
	symnam	rp07,'RP07'
	symnam	rs03,'RS03'
	symnam	rs04,'RS04'
	symnam	rs08,'RS08'
	symnam	rx01,'RX01'
	symnam	rx02,'RX02'
	symnam	rx03,'RX03'
	symnam	rx23,'RX23'
	symnam	rx24,'RX24'	;PC-style 720 KB disks
	symnam	rx26,'RX26'
	symnam	rx31,'RX31'	;PC-style 360 KB disks
	symnam	rx33,'RX33'
	symnam	rx50,'RX50'
	symnam	rx52,'RX52'	;(my name for it)
	symnam	tu56,'TU56'
	symnam	tu58,'TU58'
;+
;
; Flush output line buffer.
;
; es:di	pointer into LBUF (reset on return)
;
; Preserves SI.
;
;-
flush:	mov	ax,crlf		;<CRLF>
	stosw
flush1:	; no <CRLF>
	push	si		;save
	dec	ds:lnum		;dec line #
	jnz	flush2		;no **MORE**
	mov	al,ds:lmax	;get starting #
	mov	ds:lnum,al	;reset
	test	byte ptr ds:more,-1 ;is **MORE** processing enabled?
	jz	flush2		;no, skip all this
	call	wrng		;say **MORE**
	db	8d,'**MORE**'
	mov	ah,07h		;func=direct console input
	int	21h
	push	ax		;save char
	call	wrng		;no more
	db	10d,cr,8d dup(' '),cr ;blank out the **MORE**
	pop	ax		;restore char
	cmp	al,3		;^C?
	je	flush3		;enough of that
	cmp	al,'Q'		;quit?
	je	flush3
	cmp	al,'q'
	je	flush3
	cmp	al,cr		;CR?
	jne	flush2
	 mov	byte ptr ds:lnum,1 ;lset just 1 line through
flush2:	mov	dx,offset lbuf	;pt at buffer
	mov	cx,di		;calc length
	sub	cx,dx
	mov	bx,0001h	;STDOUT
	mov	ah,40h		;func=write
	int	21h
	pop	si		;restore
	mov	di,offset lbuf	;often needed
	ret
flush3:	jmp	mloop		;restart (flush stack)
;
synerr:	cram	'?Syntax error',error
badswt:	cram	'?Bad switch',error
swtcon:	cram	'?Switch conflict',error
nomem:	cram	'?Insufficient memory',error
misprm:	cram	'?Missing parameter',error
baddrv:	cram	'?No such drive',error
notrdy:	cram	'?Device not ready',error
;badmed:	cram	'?Illegal medium type for that device',error
badnam:	cram	'?Invalid filename',error
outran:	cram	'?Number out of range',error
fnf:	cram	'?File not found',error
dnf:	cram	'?Dir not found',error
dirio:	cram	'?Dir I/O error',error
baddir:	cram	'?Corrupt directory',error
dirful:	cram	'?Directory full',error
delerr:	cram	'?Error deleting file',error
rderr:	cram	'?Read error',error
wrerr:	cram	'?Write error',error
crerr:	cram	'?File creation error',error
iosame:	cram	'?Input and output are same device',error
notflp:	cram	'?Floppy devices required for sector copy',error
prtfil:	cram	'?Output file is protected',error
odfull:	cram	'?Output device full',error
odsmal:	cram	'?Output device too small',error
rodev:	cram	'?Output device is read only',error
lngnam:	cram	'?Filespec too long',error
unksys:	cram	'?Unable to identify file system',error
ilfunc:	cram	'?Illegal function',error
nowipe:	cram	'?WIPEOUT not supported for this file system',error
;+
;
; Error handler.
;
;-
error:	push	ds		;restore ES if changed
	pop	es
	pop	si		;restore
	lodsb			;get length
	cbw			;AH=0
	mov	cx,ax		;copy string to buffer
	mov	di,offset lbuf
	mov	dx,di		;copy ptr
	rep	movsb
	mov	ax,crlf		;<CRLF>
	stosw
	mov	cx,di		;find length
	sub	cx,dx
	mov	bx,0002h	;handle=STDERR
	mov	ah,40h		;func=write
	int	21h
	jmp	mloop		;go flush stack and restart
;+
;
; Warning handler.
;
;-
wrng:	pop	si		;restore
	lodsb			;get length
	cbw			;AH=0
	mov	dx,si		;addr
	mov	cx,ax		;copy length
	add	si,cx		;skip string
	mov	bx,0002h	;handle=STDERR
	mov	ah,40h		;func=write
	int	21h
	jmp	si		;return
;+
;
; Look up a word on a table.
;
; Each entry on the table is the word to match, followed by the word to
; return on a match.  List is terminated by DW 0.
;
; bx	word to find
; si	table
;
; Word from table is returned in ax unless CF=1, in which case no match was
; found.
;
;-
wdluk:	lodsw			;get a word
	test	ax,ax		;end?
	jz	wdluk1
	cmp	ax,bx		;match?
	lodsw			;[get next word]
	jne	wdluk		;no
	ret
wdluk1:	stc			;error
	ret
;+
;
; Parse a word from the input line.
;
; ds:si	current position
; cx	# chars left
;
; On return:
; si	points at posn after last char of word
; cx	updated
; bx	points at begn of word if CF=0
; dx	length of word
;
;-
getw:	jcxz	getw2		;EOL already
getw1:	mov	bx,si		;in case word starts here
	lodsb			;get a char
	cmp	al,' '		;blank or ctrl?
	ja	getw4		;no
	loop	getw1		;loop
getw2:	stc			;no luck
	ret
getw3:	lodsb			;get a char
getw4:	cmp	al,' '		;blank or ctrl?
	jbe	getw6		;yes, end of word
	cmp	al,ds:swchar	;/
	je	getw6
	cmp	al,'='		;=
	je	getw6
if 0
	cmp	al,'<'		;<
	je	getw6
endif
	cmp	al,'a'		;lower case?
	jb	getw5
	cmp	al,'z'		;hm?
	ja	getw5
	and	al,not 40	;yes, convert
	mov	[si-1],al	;put back
getw5:	loop	getw3		;loop
	inc	si		;compensate for next inst
getw6:	dec	si		;unget
	mov	dx,si		;calc length
	sub	dx,bx		;CF=0
	ret
;+
;
; As above but stops at first nonalphanumeric char.
;
;-
getan:	jcxz	getw2		;EOL already
getan1:	mov	bx,si		;in case word starts here
	lodsb			;get a char
	cmp	al,' '		;blank or ctrl?
	ja	getan4		;no
	loop	getan1		;loop
getan2:	stc			;no luck
	ret
getan3:	lodsb			;get a char
getan4:	cmp	al,'0'		;digit?
	jb	getan6		;no, end of word
	cmp	al,'9'
	jbe	getan5		;yes, keep it
	cmp	al,'A'		;upper case letter?
	jb	getan6		;no
	cmp	al,'Z'
	jbe	getan5		;yes, keep it
	cmp	al,'a'		;lower case letter?
	jb	getan6		;no
	cmp	al,'z'
	ja	getan6
	and	al,not 40	;yes, convert
	mov	[si-1],al	;put back
getan5:	loop	getan3		;loop
	inc	si		;compensate for next inst
getan6:	dec	si		;unget
	mov	dx,si		;calc length
	sub	dx,bx		;CF=0
	ret
;+
;
; Parse an octal number from the input line.
;
; si,cx	input line descriptor (updated on return)
; dx:ax	returns number
;
;-
geto:	call	getw		;parse it
	jc	misnum
cvto:	; enter here to parse number from GETW
	mov	di,dx		;get length
	push	si		;save
	mov	si,bx		;point at it
	xor	bx,bx		;init #
	xor	dx,dx		;high word
cvto1:	lodsb			;get a digit
	sub	al,'0'		;convert to binary
	cmp	al,7		;digit?
	ja	badnum
	push	cx		;save
	mov	cl,3		;shift count
	sal	dx,cl		;make space in high word
	rol	bx,cl		;rotate low word
	pop	cx		;restore
	mov	ah,bl		;get 3 bits that belong in high word
	and	ah,7		;isolate
	or	dl,ah		;put them in
	and	bl,not 7	;remove
	or	bl,al		;OR in new digit
	dec	di		;done all?
	jnz	cvto1		;loop if not
	mov	ax,bx		;copy number
	pop	si		;restore
	ret
;+
;
; Parse a decimal number from the input line.
;
; si,cx	input line descriptor (updated on return)
; dx:ax	returns number
;
;-
getn:	call	getw		;parse it
	jc	misnum
cvtn:	; enter here to parse number from GETW
	mov	di,dx		;get length
	push	si		;save
	mov	si,bx		;point at it
	xor	bx,bx		;init #
	xor	dx,dx		;high word
cvtn1:	lodsb			;get a digit
	sub	al,'0'		;convert to binary
	cmp	al,9d		;digit?
	ja	badnum
	cbw			;AH=0
	push	ax		;save new digit
	mov	ax,10d		;multiplier
	mul	dx		;high word *10
	test	dx,dx		;overflow?
	jnz	badnum
	push	ax		;save
	mov	ax,10d		;low word *10
	mul	bx
	pop	bx		;catch high word
	add	dx,bx		;add it in
	pop	bx		;catch new digit
	add	bx,ax		;add it in
	adc	dx,0
	jc	badnum		;overflow
	dec	di		;done all?
	jnz	cvtn1		;loop if not
	mov	ax,bx		;copy number
	pop	si		;restore
	ret
badnum:	; these two labels are ref'ed from above and below too
	cram	'?Bad number',error
misnum:	cram	'?Missing number',error
;+
;
; Parse a hex number from the input line.
;
; si,cx	input line descriptor (updated on return)
; ax	returns number
;
;-
geth:	call	getw		;parse it
	jc	misnum
cvth:	; enter here to parse number from GETW
	push	cx		;save
	mov	di,dx		;get length
	xor	dx,dx		;init #
	mov	cl,4		;shift count
	mov	al,[bx+di-1]	;get last char
	and	al,not 40	;convert to U.C. if letter
	cmp	al,'H'		;trailing H?
	jne	cvth1		;no
	dec	di		;yes, count it
	jz	badnum		;nothing left, complain
cvth1:	mov	al,[bx]		;get a digit
	inc	bx
	sub	al,'0'		;convert to binary
	cmp	al,9d		;digit?
	jbe	cvth2
	sub	al,'A'-('9'+1)+10d ;no, see if in A-F
	cmp	al,5
	ja	badnum		;no, bad number
	add	al,0Ah		;convert back to 0A-0F
cvth2:	cbw			;AH=0
	test	dh,0F0h		;is there space for another digit?
	jnz	badnum
	sal	dx,cl		;yes, slide over
	or	dl,al		;OR in new digit
	dec	di		;done all?
	jnz	cvth1		;loop if not
	mov	ax,dx		;copy number
	pop	cx		;restore
	ret
;+
;
; Parse a file size off the command line.
;
; Units:
;
; nnn		bytes
; nnn KB	kilobytes (*1024)
; nnn MB	megabytes (*1024**2)
; nnn GB	gigabytes (*1024**3)
; nnn BLOCKS	blocks (*512)
;
; si,cx	input line descriptor (updated on return)
; dx:ax	returns number
;
;-
gtsize:	call	skip		;skip leading white space
	jc	misnum
	mov	bx,si		;save starting addr
gsiz1:	mov	al,[si]		;grab a byte
	sub	al,'0'		;see if digit
	cmp	al,9d
	ja	gsiz2		;no
	inc	si		;yes, eat it
	loop	gsiz1		;count it
gsiz2:	mov	dx,si		;copy ptr
	sub	dx,bx		;find length
	jz	misnum		;no valid digits
	call	cvtn		;convert to number
	push	dx		;save
	push	ax
	call	getw		;parse unit, if any
	mov	ax,0		;[assume no shifting]
	jc	gsiz3
	mov	ax,offset sizunt ;point at table of units
	call	tbluk		;look it up
	jc	gsiz5
gsiz3:	push	ax		;save
	call	confrm		;should be EOL
	pop	cx		;catch shift count
	pop	ax		;restore number
	pop	dx
	cmp	cl,16d		;shifting 16 or more bits?
	jb	gsiz4
	mov	dx,ax		;yes, shift by one word
	xor	ax,ax
	sub	cl,16d		;correct count for remaining partial word
gsiz4:	rol	ax,cl		;shift left
	shl	dx,cl		;make space
	mov	bx,-1		;load ones
	shl	bx,cl		;mask for bits to keep in low word
	mov	cx,ax		;copy carried bits
	and	ax,bx		;mask them out of low word
	xor	cx,ax		;isolate carried bits
	or	dx,cx		;put in high word
	ret
gsiz5:	jmp	badnum
;
; Table giving shift count for each unit of size.
;
sizunt	label	byte
	kw	<BLK-S>,9d
	kw	<BL-OCKS>,9d
	kw	<B-YTES>,0
	kw	<G-B>,30d
	kw	<K-B>,10d
	kw	<M-B>,20d
	db	0
;+
;
; Look up a keyword in a table.
;
; ds:bx	keyword } from GETW
; dx	length  }
; cs:ax	table
;
; Returns CF=1 if not found, otherwise AX=number from table.
;
; This routine doesn't require that DS=CS, so it may be used to parse
; environment strings.
;
; si,cx preserved either way.
;
;-
tbluk:	push	cx		;save
	push	si
	push	ds
	mov	si,ax		;pt at table
	push	ds		;copy DS to ES
	pop	es
	push	cs		;and CS to DS
	pop	ds
	xor	ch,ch		;CH=0
tbluk1:	lodsw			;get length,,length to match
	test	al,al		;end?
	jz	tbluk4
	mov	cl,ah		;assume bad length
	cmp	al,dl		;is ours long enough?
	ja	tbluk2		;no
	sub	ah,dl		;too long?
	jc	tbluk2		;yes
	mov	cl,dl		;just right
	mov	di,bx		;point at keyword
	repe	cmpsb		;match?
	je	tbluk3
	add	cl,ah		;no, add extra length
tbluk2:	add	si,cx		;skip to end
	inc	si		;skip jump addr
	inc	si
	jmp	short tbluk1	;loop
tbluk3:	; got it
	mov	cl,ah		;get extra length
	add	si,cx		;skip to end
	lodsw			;get dispatch addr
	stc			;makes CF=0 below
tbluk4:	; not found
	cmc			;CF=-CF
	pop	ds		;restore regs
	pop	si
	pop	cx
	ret
;+
;
; Skip blanks.
;
; si	line ptr
; cx	# chars left
;
; Returns CF=1 if EOL, or CF=0 and AL=char at [SI].
;
;-
skip:	jcxz	skip2		;EOL already
skip1:	lodsb			;get a char
	cmp	al,' '		;blank or ctrl char?
	ja	skip3		;no (CF=0)
	loop	skip1		;loop
skip2:	stc			;EOL
	ret
skip3:	dec	si		;unget
	ret
;+
;
; Make sure there is nothing more on the input line.
;
;-
confrm:	call	skip		;is there?
	jnc	$+3		;yes
	 ret
	cram	'?Not confirmed',error
;+
;
; Get logical device name of the form "d[d][u]:".
;
; si,cx	input line descriptor (updated on exit)
; ah	1 to require ':', 0 not to
;
; ax	returns one- or two-letter logical device name (right-justified)
; bl	returns unit #
; bh	non-zero if BL valid
;
;-
getlog:	call	skip		;skip blanks, tabs
	jc	glog6
	xor	dx,dx		;init dev name
	xor	bx,bx		;init flag,,unit #
	push	si		;save in case invalid
	push	cx
glog1:	; get logical device name
	lodsb			;get a byte
	and	al,not 40	;convert to upper case
	cmp	al,'A'		;letter?
	jb	glog2
	cmp	al,'Z'
	ja	glog2
	test	dl,dl		;already have two letters?
	jnz	glog5		;yes, error
	mov	dl,dh		;shift over
	mov	dh,al
	loop	glog1
	jmp	short glog8	;done
glog2:	; get unit #
	dec	si		;unget
glog3:	lodsb			;get a digit
	sub	al,'0'		;convert to binary
	cmp	al,9d		;is it a digit?
	ja	glog4
	xchg	ah,bl		;yes, get current #, save ':' flag
	cmp	ax,25d*100h+5	;will number overflow?
	ja	glog5
	aad			;add in new digit
	mov	ah,bl		;restore ':' flag
	mov	bl,al		;save number
	mov	bh,1		;set flag
	loop	glog3		;loop
	jmp	short glog8
glog4:	dec	si		;unget
	add	al,'0'		;fix char
	cmp	al,':'		;colon?
	je	glog7
	cmp	al,ds:swchar	;switch?
	je	glog8
	cmp	al,' '		;white space?
	jbe	glog8
glog5:	; invalid device name
	pop	cx		;restore
	pop	si
glog6:	stc			;error return
	ret
glog7:	inc	si		;eat colon
	dec	cx
	cbw			;don't require it any more (AL=':')
glog8:	test	ah,ah		;are we still worried about a colon?
	jnz	glog5		;yes, invalid
	mov	ax,dx		;copy dev name
	add	sp,4		;flush stack, CF=0
	ret
;+
;
; Get a byte number from input line.
;
; si,cx	line descriptor (updated)
; ah	returns number
; al	returns non-digit character following number
;
;-
gbyte:	xor	ah,ah		;init number
	jcxz	gbyt2		;nothing, return
gbyt1:	lodsb			;get a character
	sub	al,'0'		;convert to binary
	cmp	al,9d		;digit?
	ja	gbyt3		;no
	aad			;add in new digit
	mov	ah,al		;copy
	loop	gbyt1		;loop
gbyt2:	xor	al,al		;say delimiter is NUL
	ret
gbyt3:	dec	si		;unget
	mov	al,[si]		;get char again
	ret
;+
;
; As above, for octal numbers.
;
; si,cx	line descriptor (updated)
; ah	returns number
; al	returns non-digit character following number
; bx,dx	preserved (for DBGD, ODGD)
;
; ZF=1 if we didn't get a single digit.
;
;-
goctb:	mov	di,si		;copy starting position
	xor	ah,ah		;init number
	jcxz	goctb2		;nothing, return
goctb1:	lodsb			;get a character
	sub	al,'0'		;convert to binary
	cmp	al,7		;digit?
	ja	goctb3		;no
	sal	ah,1		;make space
	sal	ah,1
	sal	ah,1
	or	ah,al		;OR in new digit
	loop	goctb1		;loop
goctb2:	xor	al,al		;say delimiter is NUL
	jmp	short goctb4
goctb3:	dec	si		;unget
	mov	al,[si]		;get char again
goctb4:	cmp	di,si		;got anything?
	ret			;(ZF=1 if not)
;+
;
; Allocate memory from heap.  Punt (with a message) if we fail.
;
; cx	size of block to allocate
; si	returns pointer
;
;-
getmem:	call	askmem		;get memory
	jc	gmem1		;failed, punt
	ret
gmem1:	cram	'?Out of memory',error
;+
;
; As above, but just return with CF=1 if not available instead of aborting.
;
; The heap is maintained in our combined code/data segment immediately
; following the stack.  Free memory is available starting at the address stored
; in DS:FREMEM, ending at the address stored in DS:HIMEM.  The entire heap is
; flushed before each command by copying DS:KEPMEM (which points to the end of
; memory that will be kept) to DS:FREMEM.  Logical device records are kept
; around by copying DS:FREMEM to DS:KEPMEM after they're allocated, at the end
; of each successful MOUNT command.  Logical device records are freed by
; putting them in a linked list which is checked each time a new record is
; needed before it is allocated from the heap.  That means if the user mounts a
; bunch of devices and then dismounts them, the heap has permanently shrunk and
; we may run out of memory.  But if they mounted too many devices the heap
; would be gone anyway, not really worth worrying about.
;
; AX, BX, DX preserved.
;
;-
askmem:	mov	si,ds:fremem	;get ptr
	add	cx,si		;find end
	jc	amem1
	cmp	cx,ds:himem	;off end of our memory?
	ja	amem1
	mov	ds:fremem,cx	;update ptr
	clc			;got it
	ret
amem1:	stc			;failed
	ret
;+
;
; Get a logical device record.
;
; bp	returns ptr
; si,cx	nuked
;
;-
getrec:	mov	bp,ds:frerec	;get head of list
	test	bp,bp		;did we get anything?
	jz	grec1		;no
	mov	cx,ss:next[bp]	;unlink
	mov	ds:frerec,cx
	jmp	short grec2
grec1:	mov	cx,reclen	;length
	call	getmem		;allocate core
	mov	bp,si		;copy ptr
grec2:	; clear it out
	push	ax		;save
	push	di
	mov	di,bp		;point at it
	xor	al,al		;load 0
	mov	cx,reclen	;# bytes
	rep	stosb		;nuke it
	pop	di		;restore
	pop	ax
	mov	ds:tmpdev,bp	;save ptr
	ret
;+
;
; Return a logical device record to the free list.
;
; bp	ptr to record
;
;-
retrec:	cmp	bp,ds:fremem	;already freed?  (called from MLOOP)
	jae	rrec1		;yes
	mov	ax,bp		;copy ptr
	xchg	ax,ds:frerec	;get list, set new head
	mov	ss:next[bp],ax	;link list to head
rrec1:	ret
;+
;
; Parse a logical dev name and look up its record.
;
; If there's no device name, we use the default, which is either DS:CURDSK (a
; DOS disk letter), or if DS:CURDSK=0 then it's the first logical device on the
; DS:LOGDEV list.  If a device name is given, it is either a logical device (if
; it's on the DS:LOGDEV list), or a DOS drive (if not and it's just one letter
; and no unit number), or invalid.
;
; bp	returns record
; si,cx	updated
;
;-
gdevn:	xor	ah,ah		;no colon needed
	jmp	short gdev1
gdev:	mov	ah,1		;require colon
gdev1:	call	getlog		;get logical name
	mov	bp,ds:logdev	;[get head of device list]
	jnc	gdev3
	; no logical name, use logged-in default
	mov	ah,ds:curdsk	;is a DOS disk current?
	test	ah,ah
	jz	gdev2		;no, we already have ptr
	xor	al,al		;yes, set up dev info
	xor	bx,bx
	jmp	short gdev5	;create a record
gdev2:	ret
gdev3:	; see if it's defined
	cmp	ss:logd[bp],ax	;check for match
	jne	gdev4
	cmp	ss:logu[bp],bx
	je	gdev6		;got it, whee
gdev4:	mov	bp,ss:next[bp]	;follow link
	test	bp,bp		;more?
	jnz	gdev3		;loop if so
	; undefined, see if it's just one letter (DOS disk)
	or	bh,al		;unit flag or second letter?
	jnz	gdev7		;one or the other, error
gdev5:	; DOS disk, create temporary dev record
	push	si		;save cmd line
	push	cx
	call	getrec		;get temp record for DOS pseudo-device
	pop	cx
	pop	si
	; init the record but don't link it anywhere (flushed on next cmd)
	mov	ds:dosroo,ah	;save drive letter
	mov	ss:logd[bp],ax	;set name
	mov	ss:logu[bp],bx
	mov	ss:dsrini[bp],offset cret ;set vectors
	mov	ss:rdasc[bp],offset dosrd ;ASCII and binary are same on DOS
	push	si		;save
	push	di		;;; is anyone really using DI?
	push	cx
	mov	si,offset dosvec ;pt at other vectors
	call	setvec		;set them, return
	pop	cx		;restore
	pop	di
	pop	si
gdev6:	ret
gdev7:	cram	'?Invalid logical device name',error
;+
;
; Set file I/O vectors.
;
; ss:bp	log dev rec
; si	ptr to vector list
;
;-
setvec:	lea	di,[bp+iovecs]	;pt at dest
	mov	cx,niovec	;# words
	rep	movsw		;copy them in
	ret
;+
;
; Multiply two 32-bit numbers (unsigned).
;
; dx:ax	32-bit multiplicand
; bx:cx	32-bit multiplier
;
; On return:
; dx:cx:bx:ax  64-bit product
; bp	preserved
;
;-
mul32:	; (DX:AX)*(CX:BX) => (CX*DX):(AX*CX+BX*DX):(AX*BX) (with carry)
	mov	di,dx		;save high order
	push	ax		;save
	mov	ax,cx		;get CX
	mul	dx		;CX*DX
	xchg	di,dx		;DI=word 3
	mov	si,ax		;SI=word 2
	mov	ax,bx		;get BX
	mul	dx		;BX*DX
	add	si,dx		;add high order to words 2/3
	adc	di,0
	xchg	ax,cx		;save word 1, get CX
	pop	dx		;restore AX
	push	dx		;save again
	mul	dx		;AX*CX
	add	cx,ax		;add to word 1/2/3
	adc	si,dx
	adc	di,0
	pop	ax		;restore AX once again
	mul	bx		;AX*BX
	mov	bx,dx		;copy high order
	add	bx,cx		;add word 1
	adc	si,0		;ripple up
	adc	di,0
	mov	cx,si		;copy
	mov	dx,di
	ret
;+
;
; Divide a 64-bit number by a 32-bit number (unsigned).
;
; dx:cx:bx:ax  64-bit dividend
; di:si	32-bit divisor
;
; On return:
; dx:cx:bx:ax  64-bit quotient
; di:si	32-bit remainder
; bp	preserved
;
;-
div32:	push	bp		;save
	mov	ds:dvsr0,si	;save divisor
	mov	ds:dvsr1,di
	xor	si,si		;init partial remainder
	xor	di,di
	mov	bp,64d		;bit count
div1:	sal	ax,1		;dividend left a bit
	rcl	bx,1
	rcl	cx,1
	rcl	dx,1
	rcl	si,1		;into partial remainder
	rcl	di,1
	cmp	ds:dvsr1,di	;will divisor fit?
	jb	div2		;yes
	ja	div3		;no
	cmp	ds:dvsr0,si	;check low word
	ja	div3		;no
div2:	sub	si,ds:dvsr0	;yes, subtract it out
	sbb	di,ds:dvsr1
	inc	ax		;shift in a 1
div3:	dec	bp		;done all bits?
	jnz	div1		;loop if not
	pop	bp		;restore
	ret
;+
;
; Open or create a file.
;
; ds:dx	.ASCIZ filename
; ax	returns handle or error code if CF=1
; others preserved
;
; CRT		creates file and opens read/write
; OPENRO	opens file read-only
; OPENWO	opens file write-only
; OPENRW	opens file read/write
;
;-
crt:	cmp	byte ptr ss:dosver+1,4 ;V4 or later?
	jb	crt2		;no
	push	bx		;save
	push	cx
	push	dx
	push	si
	mov	si,dx		;DS:SI points at filename
	mov	dx,12h		;action:  FNF=create, exists=replace
	xor	cx,cx		;attr=default
	mov	bx,10002	;flags=>2 GB, mode=RW
;	mov	ax,716Ch	;func=LFN open/whatever
;	test	byte ptr ss:lfn,-1 ;LFN support?
;	jnz	crt1		;yes
	 mov	ax,6C00h	;func=open/whatever
crt1:	int	21h		;open file, or not
	pop	si		;[restore]
	pop	dx
	pop	cx
	pop	bx
	ret
crt2:	; no fancy V4.0 stuff, just use CREATE
	mov	ah,3Ch		;func=create
	push	cx		;save
	xor	cx,cx		;attr=default
	int	21h
	pop	cx		;[restore]
	ret
;
openro:	xor	al,al		;func=open /RONLY
	jmp	short opn
;
openwo:	mov	al,01h		;func=open /WO
	jmp	short opn
;
openrw:	mov	al,02h		;func=open /RW
	;jmp	short opn
;+
;
; Open a DOS file (simulate INT 21h function 3Dh).
;
; al	open mode
; ds:dx	pointer to .ASCIZ filename
; ax	returns handle (CF=0) or error code (CF=1)
;
;-
opn:	cmp	byte ptr ss:dosver+1,4 ;V4 or later?
	jb	opn2		;no
	push	bx		;save
	push	cx
	push	dx
	push	si
	mov	si,dx		;DS:SI points at filename
	mov	dx,01h		;action:  FNF=fail, exists=open
	xor	cx,cx		;attr=default (not used)
	mov	bh,20		;flags=>2 GB
	mov	bl,al		;copy mode
;	mov	ax,716Ch	;func=LFN open/whatever
;	test	byte ptr ss:lfn,-1 ;LFN support?
;	jnz	opn1		;yes
	 mov	ax,6C00h	;func=open/whatever
opn1:	int	21h		;open file, or not
	pop	si		;[restore]
	pop	dx
	pop	cx
	pop	bx
	ret
opn2:	; no fancy V4.0 stuff, just use OPEN
	mov	ah,3Dh		;func=open
	int	21h
	ret
;+
;
; Init BX with downcounter value, e.g. for first call to SDELAY.
; (Also used by WAITMS to poll downcounter value.)
;
; bx	returns current down-counter value
; all others preserved
;
;-
sdel0:	push	ax		;save
	cli			;ints off
	mov	al,06h		;;latch timer 0, mode 3
	out	43h,al
	in	al,40h		;;read it
	mov	bl,al		;;(save)
	in	al,40h
	sti			;;ints on
	mov	bh,al		;BX=starting down-counter value
	pop	ax		;restore
	ret
;+
;
; Synchronous delay until next 838.1 ns clock tick.
;
; bx	starting timer value, updated on return
; all others preserved
;
;-
sdelay:	push	ax		;save
	cli			;ints off
	mov	al,06h		;;latch timer 0, mode 3
	out	43h,al
	in	al,40h		;;read it
	mov	ah,al
	in	al,40h
	sti			;;ints on
	xchg	al,ah		;fix byte order
	cmp	ax,bx		;changed yet?
	je	short sdelay	;spin until it does
	mov	bx,ax		;set new starting value
	pop	ax		;restore
	ret
;+
;
; Wait for a specified interval.
;
; ax	# of milliseconds to delay
; all others preserved
;
;-
waitms:	push	bx		;save
	push	cx
	call	sdel0		;init time in BX
wms1:	push	ax		;save
	mov	cx,bx		;save starting time
wms2:	call	sdel0		;get new counter value
	mov	ax,cx		;get starting time
	sub	ax,bx		;get new time (may wrap around 64 K, fine)
	cmp	ax,1194d	;enough 838.1 ns ticks for 1 msec?
	jb	wms2		;loop if not
	pop	ax		;restore
	dec	ax		;done all msecs?
	jnz	wms1		;loop if not (BX has new starting time)
	pop	cx		;restore
	pop	bx
	ret
;+
;
; Compute CRC-CCITT of a block.
;
; See KPROTO.DOC (Kermit protocol spec) and "C Programmer's Guide to NetBIOS,
; IPX, and SPX" by W. David Schwaderer for explanation of CRC-CCITT computation
; (note, the book says to init with all ones and complement the final result, I
; don't see how this is different from initting to 0 and not complementing,
; KPROTO.DOC agrees with me)
;
; ds:si	buf ptr
; cx	length
; dx	initial value (FFFF, or include CRC of constant 1/4-byte mark string)
; ax	returns CRC-CCITT
;
;-
docrc:	;mov	dx,0FFFFh	;init
dcrc1:	lodsb			;get next data byte
	xor	al,dh		;XOR high byte of old value
	xor	ah,ah		;AH=0
	add	ax,ax		;*2
	mov	bx,ax		;copy
	mov	dh,dl		;running total left 8.
	xor	dl,dl
	xor	dx,ds:crc[bx]	;find new value
	loop	dcrc1		;loop
	mov	ax,dx		;copy
	ret
;+
;
; Build CRC-CCITT lookup table, polynomial = X^16+X^12+X^5+1.
;
; Each bit is an XOR sum of certain bit(s) of the 8-bit index.
;
; Each column contains the bits that are XORed to compute the new partial
; remainder after a new byte is added.  C15:C00 is the previous partial
; remainder, V07:V00 is the new data byte XORed with the previous C07:C00
; values (and is the index into our 256-word table).
;
; I derived these values by doing 8 cycles of a hardware CCITT CRC generator by
; hand, as described (using CRC-16 as an example) in Schwaderer's book.  Note
; that to fix the CRC bit order the XOR insertion points are reversed, the
; X**16 bit comes out the R.H. side and is XORed with the incoming data bit
; (that's why V is defined as the data bit XORed with one of the low 8 C bits),
; the result from this is XORed back in at bit positions 15 (X**0), 10 (X**5),
; and 3 (X**12).
;
; Start:
; b15 b14 b13 b12 b11 b10 b09 b08 b07 b06 b05 b04 b03 b02 b01 b00
; ---------------------------------------------------------------
; C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 C00
;
; Step 0 (shift in V00):
; b15 b14 b13 b12 b11 b10 b09 b08 b07 b06 b05 b04 b03 b02 b01 b00
; ---------------------------------------------------------------
;     C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01
; V00                 V00                         V00
;
; Step 1:
; b15 b14 b13 b12 b11 b10 b09 b08 b07 b06 b05 b04 b03 b02 b01 b00
; ---------------------------------------------------------------
;         C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02
;     V00                 V00                         V00
; V01                 V01                         V01
;
; Step 2:
; b15 b14 b13 b12 b11 b10 b09 b08 b07 b06 b05 b04 b03 b02 b01 b00
; ---------------------------------------------------------------
;             C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03
;         V00                 V00                         V00
;     V01                 V01                         V01
; V02                 V02                         V02
;
; Step 3:
; b15 b14 b13 b12 b11 b10 b09 b08 b07 b06 b05 b04 b03 b02 b01 b00
; ---------------------------------------------------------------
;                 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04
;             V00                 V00                         V00
;         V01                 V01                         V01
;     V02                 V02                         V02
; V03                 V03                         V03
;
; Step 4:
; b15 b14 b13 b12 b11 b10 b09 b08 b07 b06 b05 b04 b03 b02 b01 b00
; ---------------------------------------------------------------
;                     C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05
; V00             V00 V00             V00         V00
;             V01                 V01                         V01
;         V02                 V02                         V02
;     V03                 V03                         V03
; V04                 V04                         V04
;
; Step 5:
; b15 b14 b13 b12 b11 b10 b09 b08 b07 b06 b05 b04 b03 b02 b01 b00
; ---------------------------------------------------------------
;                         C15 C14 C13 C12 C11 C10 C09 C08 C07 C06
;     V00             V00 V00             V00         V00
; V01             V01 V01             V01         V01
;             V02                 V02                         V02
;         V03                 V03                         V03
;     V04                 V04                         V04
; V05                 V05                         V05
;
; Step 6:
; b15 b14 b13 b12 b11 b10 b09 b08 b07 b06 b05 b04 b03 b02 b01 b00
; ---------------------------------------------------------------
;                             C15 C14 C13 C12 C11 C10 C09 C08 C07
;         V00             V00 V00             V00         V00
;     V01             V01 V01             V01         V01
; V02             V02 V02             V02         V02
;             V03                 V03                         V03
;         V04                 V04                         V04
;     V05                 V05                         V05
; V06                 V06                         V06
;
; Step 7 (shift in V07):
; b15 b14 b13 b12 b11 b10 b09 b08 b07 b06 b05 b04 b03 b02 b01 b00
; ---------------------------------------------------------------
;                                 C15 C14 C13 C12 C11 C10 C09 C08
;             V00             V00 V00             V00         V00
;         V01             V01 V01             V01         V01
;     V02             V02 V02             V02         V02
; V03             V03 V03             V03         V03
;             V04                 V04                         V04
;         V05                 V05                         V05
;     V06                 V06                         V06
; V07                 V07                         V07
; ---------------------------------------------------------------
;
; Note that floppy disks use the opposite bit order from what Kermit uses, so
; the entire above chart should be reversed.  I don't feel like retyping
; everything, anyway the LSB is at the left, 7:0 becomes 0:7, and the resulting
; word is byte-swapped.
;
;-
bldcrc:	push	ds		;copy DS to ES
	pop	es
	mov	di,offset crc	;point at table
	xor	dx,dx		;init index
bcrc1:	; bits 7:0 of the array index appear at b12:5 and b7:0
	mov	ax,dx		;copy
	mov	cl,5		;shift count
	shl	ax,cl		;b7:0 => b12:5
	xor	ax,dx		;b7:0 => b7:0
	; bits 3:0 appear at b15:12, bits 7:4 appear at b3:0
	mov	bx,dx		;copy
	dec	cx		;CL=4
	ror	bx,cl		;split around word boundary
	xor	ax,bx		;b3:0 => b15:12, b7:4 => b3:0
	; bits 7:4 also appear at b15:12, b8:5
	mov	bx,dx		;copy
	and	bx,0F0h		;isolate
	xor	ah,bl		;b7:4 => b15:12
	add	bx,bx		;left one
	xor	ax,bx		;b7:4 => b8:5
	stosw			;save
	inc	dl		;bump to next
	jnz	bcrc1		;loop if didn't wrap to 0
	ret
;
	subttl	read binary block routines
;+
;
; All routines have these arguments:
;
; On entry:
; bp	dev record
; dx:ax	block number to read
; cx	number of blocks to read
; es:di	ptr to buf
;
; On return, CF=0 and:
; es:si	ptr to data read
; cx	count of bytes read
;
; Or CF=1 on read error (or cluster out of range).
;
;-
rdfil:	; read block(s) from file
	push	cx		;save length
	mov	ch,dl		;get block *256. into CX:DX
	mov	cl,ah
	mov	dh,al
	xor	dl,dl
	sal	dh,1		;*512.
	rcl	cx,1
	mov	bx,ss:handle[bp] ;get handle
	mov	ax,4200h	;func=LSEEK from BOF
	int	21h
	pop	cx		;[restore cluster count]
	jc	rdfil1
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	mov	dx,di		;get buf ptr
	mov	ch,cl		;get count *512.
	sal	ch,1
	xor	cl,cl
	mov	ah,3Fh		;func=read
	int	21h
	pop	ds		;[restore]
	jc	rdfil1
	cmp	ax,cx		;got it all?
	jne	rdfil1
	mov	si,dx		;point at it (CX is set up)
	jmp	short trim12	;trim if needed, return
rdfil1:	stc			;error return
	ret
;
trim12:	; trim words if 12-bit device (ES:SI=ptr, CX=ctr)
	cmp	ss:wsize[bp],12d ;12-bit device?
	jne	trim14		;no
	mov	di,si		;copy ptr
	mov	bx,cx		;and byte count
	shr	bx,1		;word count
	mov	al,0Fh		;mask
trim13:	inc	di		;+1
	and	es:[di],al	;isolate (CF=0)
	inc	di		;+1
	dec	bx		;loop
	jnz	trim13
trim14:	clc			;read was successful either way
	ret
;
rddx:	; read block(s) from RX01/02/03 floppy
	mov	bx,ax		;get starting block #
	add	bx,cx		;add count
	adc	dx,0		;carry
	jnz	rddx6		;whoops
	cmp	bx,ss:devsiz[bp] ;off end of disk?  (end+1 is OK)
	ja	rddx6
	push	cx		;save block count
	push	di		;save ptr
	mov	bx,cx		;copy
	mov	cl,ss:blksec[bp] ;get shift count
	sal	ax,cl		;abs starting sector number
	sal	bx,cl		;sector count
	mov	cx,bx		;copy back
rddx1:	; read next sector
	push	cx		;save count
	push	ax		;save cluster #
	mov	bl,26d		;divisor
	div	bl		;AX=sector,,cyl
	mov	dh,al		;copy cyl #
	mov	cl,ss:nhds[bp]	;get # sides
	dec	cx		;0 if SS, 1 if DS
	and	dh,cl		;low bit is really side if DS
	shr	al,cl		;cyl /2 if DS
	xchg	al,ah		;><
	mov	cx,ax		;copy
	call	ss:fintlv[bp]	;compute interleave
	push	di		;save ptr
	call	ss:rdsec[bp]	;read sector
	pop	di		;[restore]
	jc	rddx5		;error
	mov	si,ds:dbuf	;pt at data
	mov	cx,ss:secsz[bp]	;byte count
	shr	cx,1		;word count
	; unpack 12-bit words from first 3/4 of sector, or just copy if 16-bit
	cmp	ss:wsize[bp],12d ;12-bit device?
	jne	rddx3		;no
	shr	cx,1		;/2 (2 words at a time)
rddx2:	; from RX8E prints:  bits from disk drive shift into low order end
	; of shift register, clocked 8 times for 8-bit mode or 12x for 12
	lodsw			;get first two bytes
	xchg	al,ah		;high bit first
	shr	ax,1		;right 4
	shr	ax,1
	shr	ax,1
	shr	ax,1
	stosw
	dec	si		;back up
	lodsw			;get 2nd, 3rd byte
	xchg	al,ah		;high bit first
	and	ah,17
	stosw
	loop	rddx2		;loop
rddx3:	rep	movsw		;copy (or drop through after LOOP)
rddx4:	pop	ax		;restore sector #
	inc	ax		;+1
	pop	cx
	loop	rddx1		;do next sector
	pop	si		;restore ptr
	pop	cx		;restore block count
	xchg	cl,ch		;*512.
	sal	ch,1		;CF=0
	ret
rddx5:	; error
	add	sp,8d		;flush stack
rddx6:	stc			;error
	ret
;
rddz:	; read block(s) from RX50 (etc.) floppy (anything with 512 bytes/sec)
	mov	bx,ax		;get starting block #
	add	bx,cx		;add count
	adc	dx,0		;carry
	jnz	rddz3		;whoops
	cmp	bx,ss:devsiz[bp] ;off end of disk?  (end+1 is OK)
	ja	rddz3
	push	cx		;save block count
	push	di		;save ptr
rddz1:	; read next sector
	push	cx		;save count
	push	ax		;save cluster #
	call	ss:fintlv[bp]	;compute interleave
	push	di		;save ptr
	call	ss:rdsec[bp]	;read sector
	pop	di		;[restore]
	jc	rddz2		;error
	mov	si,ds:dbuf	;pt at data
	mov	cx,512d/2	;word count
	rep	movsw		;copy
	pop	ax		;restore sector #
	inc	ax		;+1
	pop	cx
	loop	rddz1		;do next sector
	pop	si		;restore ptr
	pop	cx		;restore block count
	xchg	cl,ch		;*512.
	sal	ch,1
	callr	trim12		;trim if appropriate
rddz2:	; error
	add	sp,8d		;flush stack
rddz3:	stc			;error
	ret
;
	subttl	write binary block routines
;+
;
; All routines have these arguments:
;
; On entry:
; bp	dev record
; ax:dx	block number to write
; cx	number of blocks to write
; es:si	ptr to buf
;
; On return, CF=0 on success.
;
;-
wrfil:	; write block(s) to disk image file
	push	cx		;save length
	mov	ch,dl		;get block *256. into CX:DX
	mov	cl,ah
	mov	dh,al
	xor	dl,dl
	sal	dh,1		;*512.
	rcl	cx,1
	mov	bx,ss:handle[bp] ;get handle
	mov	ax,4200h	;func=LSEEK from BOF
	int	21h
	pop	cx		;[restore cluster count]
	jc	wrfil1
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	mov	dx,si		;get buf ptr
	mov	ch,cl		;get count *512.
	sal	ch,1
	xor	cl,cl
	mov	ah,40h		;func=read
	int	21h
	pop	ds		;[restore]
	jc	wrfil1
	cmp	ax,cx		;got it all?
	je	wrfil2		;yes, CF=0
wrfil1:	stc			;error return
wrfil2:	ret
;
wrdx:	; write block(s) to RX01/02/03 floppy
	mov	bx,ax		;get starting block #
	add	bx,cx		;add count
	adc	dx,0		;carry
	jnz	wrdx4		;whoops
	cmp	bx,ss:devsiz[bp] ;off end of disk?  (end+1 is OK)
	ja	wrdx4
	mov	bx,cx		;copy
	mov	cl,ss:blksec[bp] ;get shift count
	sal	ax,cl		;abs starting sector number
	sal	bx,cl		;sector count
	mov	cx,bx		;copy back
wrdx1:	; write next sector
	push	cx		;save count
	push	ax		;save sector #
	mov	di,ds:dbuf	;pt at buf
	mov	dx,ds		;get segs
	mov	bx,es
	mov	ds,bx		;reverse them
	mov	es,dx
	mov	cx,ss:secsz[bp]	;byte count
	shr	cx,1		;word count
	; pack 12-bit words into first 3/4 of sector, or just copy if 16-bit
	cmp	ss:wsize[bp],12d ;12-bit device?
	jne	wrdx3		;no
	shr	cx,1		;/2 (2 words at a time)
	push	cx		;save sector size /4 for padding
wrdx2:	lodsw			;get first word
	sal	ax,1		;left 4
	sal	ax,1
	sal	ax,1
	sal	ax,1
	xchg	al,ah		;write high bit first
	stosw
	lodsw			;get second word
	and	ah,17		;just to make sure
	or	byte ptr es:[di-1],ah ;OR in high four bits
	stosb			;write low eight
	loop	wrdx2		;loop
	pop	cx		;restore count /4
	rep	stosb		;fill rest of sector with last byte written
wrdx3:	rep	movsw		;copy (or drop through after REP STOSB)
	pop	ax		;catch sector #
	push	ax		;save again
	mov	ds,dx		;restore
	push	bx		;save ES value
	push	si		;save ptr
	mov	bl,26d		;divisor
	div	bl		;AX=sector,,cyl
	mov	dh,al		;copy cyl #
	mov	cl,ss:nhds[bp]	;get # sides
	dec	cx		;0 if SS, 1 if DS
	and	dh,cl		;low bit is really side if DS
	shr	al,cl		;cyl /2 if DS
	xchg	al,ah		;><
	mov	cx,ax		;copy
	call	ss:fintlv[bp]	;compute interleave
	mov	bx,ds:dbuf	;ES:BX=buffer
	call	ss:wrsec[bp]	;write sector
	pop	si		;[restore ptr]
	pop	es
	pop	ax		;[restore sector #]
	pop	cx
	jc	wrdx4		;error
	inc	ax		;+1
	loop	wrdx1		;do next sector
	clc			;happy
	ret
wrdx4:	stc			;error
	ret
;
wrdz:	; write block(s) to RX50 (etc.) floppy (anything with 512 bytes/sec)
	mov	bx,ax		;get starting block #
	add	bx,cx		;add count
	adc	dx,0		;carry
	jnz	wrdz4		;whoops
	cmp	bx,ss:devsiz[bp] ;off end of disk?  (end+1 is OK)
	ja	wrdz4
wrdz1:	; write next sector
	push	cx		;save count
	push	ax		;save cluster #
	mov	di,ds:dbuf	;pt at buf
	mov	dx,ds		;get segs
	mov	bx,es
	mov	ds,bx		;reverse them
	mov	es,dx
	mov	cx,512d/2	;word count
	; trim all words to 12 bits if PDP-8 mode
	cmp	ss:wsize[bp],12d ;12-bit device?
	jne	wrdz3		;no
	push	ax		;save cluster #
wrdz2:	lodsw			;get a word
	and	ah,7777/400	;trim to 12 bits
	stosw			;save
	loop	wrdz2		;(CX=0 afterwards, drop through the REP MOVSW)
	pop	ax		;(restore cluster)
wrdz3:	rep	movsw		;copy into sector buffer (doesn't span 64 KB)
	mov	ds,dx		;restore
	push	bx		;save ES value
	push	si		;save ptr
	call	ss:fintlv[bp]	;compute interleave
	mov	bx,ds:dbuf	;ES:BX=buffer
	call	ss:wrsec[bp]	;write sector
	pop	si		;restore ptr
	pop	es		;restore ES
	pop	ax		;restore sector #
	pop	cx
	jc	wrdz4		;error
	inc	ax		;+1
	loop	wrdz1		;do next sector
	ret			;CF is still 0
wrdz4:	stc			;error
	ret
;
	subttl	read ASCII block routines
;+
;
; Read ASCII block(s).
;
; Register usage is the same as the binary read routines.
;
;-
rddb:	; read DOS/BATCH data, follow link, and strip NULs
	call	dbrclu		;read
	jc	rddb3
	mov	di,si		;copy ptr
	mov	dx,si		;save
	lods	word ptr es:[si] ;get retrieval ptr
	sub	cx,2		;count off 2 bytes
	mov	ds:iblk,ax	;save link
rddb1:	; trim NULs
	lods	byte ptr es:[si] ;get a byte
	test	al,al		;^@?
	jz	rddb2
	 stosb			;save if not
rddb2:	loop	rddb1		;loop
	sub	di,dx		;find length (CF=0)
	mov	cx,di		;[get count]
	mov	si,dx		;[and addr]
rddb3:	ret
;
rdos:	; read OS/8 block and convert to ASCII
	cmp	byte ptr ds:ieof,0 ;end of file?
	jnz	rdos8
	call	ss:rdblk[bp]	;read
	jc	rdos6		;error
	mov	di,si		;copy ptr
	mov	dx,si		;twice
	mov	bx,cx		;copy count
	shr	bx,1		;/2=wc
	shr	bx,1		;/4=count of char triplets
	mov	cl,4		;bit count
rdos1:	; read two words, write three chars
	lods	word ptr es:[si] ;get a word
	and	al,177		;7-bit ASCII
	jz	rdos2		;skip if NUL
	cmp	al,'Z'-100	;EOF?
	je	rdos7
	stosb			;save 1st byte
rdos2:	sal	ah,cl		;left 4
	mov	ch,ah		;save
	lods	word ptr es:[si] ;get 2nd word
	and	al,177		;7 bits
	jz	rdos3
	cmp	al,'Z'-100	;EOF?
	je	rdos7
	stosb
rdos3:	mov	al,ah		;copy
	or	al,ch		;complete 3rd char
	and	al,177		;7 bits
	jz	rdos4
	cmp	al,'Z'-100	;EOF?
	je	rdos7
	stosb
rdos4:	dec	bx		;done all?
	jnz	rdos1		;loop
rdos5:	mov	si,dx		;set ptr
	mov	cx,di		;compute length
	sub	cx,si		;(CF=0)
rdos6:	ret
rdos7:	; ^Z (soft EOF)
	inc	ds:ieof		;set EOF flag
	jmp	short rdos5
rdos8:	; hit ^Z last time
	xor	cx,cx		;CX=0
	stc			;EOF
	ret
;
rdcos:	; read COS310 block and convert to ASCII
;;; this is more complicated
	ret
;
rdrt:	; read RT11 data and strip NULs
	call	ss:rdblk[bp]	;read
	jnc	strip		;success, strip NULs
	ret
;+
;
; Strip NULs from a read buffer.
;
; es:si	buffer (preserved)
; cx	length, assumed NZ (updated on return, may be zero)
;
; CF=0 on return.
;
;-
strip:	mov	di,si		;copy ptr
	mov	dx,si		;save
strip1:	; trim NULs
	lods	byte ptr es:[si] ;get a byte
	test	al,al		;^@?
	jz	strip2
	 stosb			;save if not
strip2:	loop	strip1		;loop
	mov	si,dx		;point at begn of buf
	mov	cx,di		;point at end
	sub	cx,si		;find length (CF=0)
	ret
;
	subttl	write ASCII block routines
;+
;
; Write ASCII block(s).
;
; Register usage is the same as the binary write routines:
;
; On entry:
; bp	dev record
; ax:dx	block number to write
; cx	number of blocks to write
; es:si	ptr to buf
;
; On return, CF=0 on success.
;
;-
wros:	; write ASCII blocks to OS/8 disk
	mov	di,offset buf	;point at buf
	push	cx		;save blk count
	push	dx		;save blk #
	push	ax
	mov	bx,384d/3	;# ASCII char triplets per 256-word block
	mov	cx,4
wros1:	lods	word ptr es:[si] ;get first two chars
	mov	[di],al		;save
	mov	[di+2],ah
	lods	byte ptr es:[si] ;get third
	mov	ah,al		;copy
	shr	ah,cl		;split into two nibbles
	and	al,17
	mov	[di+1],ah	;save LH
	mov	[di+3],al	;RH
	add	di,cx		;skip
	dec	bx		;loop through all
	jnz	wros1
	pop	ax		;catch blk #
	pop	dx
	push	dx		;save again
	push	ax
	push	es		;save source ptr
	push	si
	push	ds		;copy DS to ES
	pop	es
	mov	si,offset buf	;pt at buf
	mov	cx,1		;1 block
	call	ss:wrblk[bp]	;write it
	pop	si		;[restore all]
	pop	es
	pop	ax
	pop	dx
	pop	cx
	jc	wros2		;error, return
	inc	ax		;[bump block (OS/8 disks always <4 K blks)]
	loop	wros		;[loop through all blocks (CF=0)]
wros2:	ret
;
	subttl	interleave
;+
;
; RX01 interleave routine.
;
; bp	logical device rec
; ch	cylinder (0-75.)
; cl	logical sector (0-25.)
;
; On return:
; ch	cylinder (1-76.)
; cl	sector (1-26.)
;
; From RT-11 V04 DY.MAC:
;
; ISEC=(ISEC-1)*2
; IF(ISEC.GE.26) ISEC=ISEC-25
; ISEC=MOD(ISEC+ITRK*6,26)+1
; ITRK=ITRK+1
;
; PDP-8 interleave is the same but there's no skew.
;
;-
dxilv:	add	cl,cl		;sec*2
	cmp	cl,ss:nsecs[bp]	;off EOT?
	jb	dxilv1
	sub	cl,ss:nsecs[bp]	;start over at 1
	inc	cx
dxilv1:	cmp	ss:wsize[bp],16d ;PDP-11 disk?
	jne	dxilv2		;no, no skew
	mov	al,6		;skew factor=6
	mul	ch		;*track
	add	al,cl		;add (sec-1)
	adc	ah,0
	div	ss:nsecs[bp]	;track size
	mov	cl,ah		;get remainder
dxilv2:	add	cx,101h		;track,,sector +1
	ret
;+
;
; RX02/03 interleave routine.
;
; As above except interleave is 3:1 for PDP-8 floppies.
;
;-
dyilv:	cmp	ss:wsize[bp],16d ;PDP-11 disk?
	jne	dyilv2		;no, assume PDP-8
	; 2:1 interleave, 6-sector skew
	add	cl,cl		;sec*2
	cmp	cl,ss:nsecs[bp]	;off EOT?
	jb	dyilv1
	sub	cl,ss:nsecs[bp]	;start over at 1
	inc	cx
dyilv1:	mov	al,6		;skew factor=6
	mul	ch		;*track
	add	al,cl		;add (sec-1)
	adc	ah,0
	div	ss:nsecs[bp]	;track size
	mov	cl,ah		;get remainder
	add	cx,101h		;track,,sector +1
	ret
dyilv2:	; 3:1 interleave, no skew
	mov	al,cl		;copy
	mov	ah,3		;*3
	mul	ah		;(AH=0)
	div	ss:nsecs[bp]	;mod track size
	mov	cl,ah		;copy
	add	cx,101h		;track,,sector +1
	cmp	ch,ss:ncyls[bp]	;wrapped off end (hacked handler)?
	jb	dyilv3		;no
	 xor	ch,ch		;start over at track 0
dyilv3:	ret
;+
;
; RX23/RX24/26/33 interleave routine.
;
; Soft interleave is not used with these drives, so our job is easy.
;
; ss:bp	logical device rec
; ax	absolute sector number
;
; On return:
; ch	cylinder (0 :: NCYLS-1)
; cl	sector (1 :: NSECS)
; dh	head (0 :: NHDS-1)
;
;-
duilv:	div	ss:nsecs[bp]	;AX=sector,,track
	mov	cl,ah		;save sector
	xor	ah,ah		;zero-extend
	div	ss:nhds[bp]	;AX=head,,cyl
	mov	dh,ah		;copy head
	mov	ch,al		;and cyl
	inc	cx		;sector +1
	ret
;
if 0
; Version of above that uses all of side 0 before starting side 1
; in case I run into something that does this with 1:1 interleave, no skew,
; and no track offset (i.e. not RX52 or RX03)
sdilv:	div	ss:nsecs[bp]	;AX=sector,,track
	mov	cl,ah		;save sector
	xor	ah,ah		;zero-extend track
	div	ss:ncyls[bp]	;AX=cyl,,head
	mov	dh,al		;get head
	mov	ch,ah		;get cyl
	inc	cx		;sector +1
	ret
endif
;+
;
; RX50 interleave routine.
;
; bp	logical device rec
; ax	absolute sector number
;
; On return:
; ch	cylinder (0-79.)
; cl	sector (1-10.)
; dh	head (0-1)
;
; From P/OS V3.2 DZDRV.MAC:
;
; IHEAD=ITRK/80
; ITRK=MOD(ITRK,80)
; ISEC=(ISEC-1)*2
; IF(ISEC.GE.10) ISEC=ISEC-9
; ISEC=MOD(ISEC+ITRK*2,10)+1
; ITRK=MOD(ITRK+1,80)
;
; Note that the disk starts on track 1 and wraps around after track 79 so that
; track 0 is the last track.  On double-sided disks (did these ever exist?),
; you then continue onto track 1 of side 1, wrapping at 79 and ending on track
; 0 of side 1.
;
;-
dzilv:	; P/OS DZDRV.MAC supports DS RX50s, don't know if they ever existed
	; interleave scheme starts over on side 1 after completing side 0
	xor	dh,dh		;init head
	cmp	ax,ss:devsiz[bp] ;on side 0?
	jb	dzilv1
	sub	ax,ss:devsiz[bp] ;no, start over on side 1
	inc	dh
dzilv1:	div	ss:nsecs[bp]	;AX=sector,,cyl
	xchg	al,ah		;><
	mov	cx,ax		;copy
	sal	cl,1		;sec*2
	cmp	cl,ss:nsecs[bp]	;>=10?
	jb	dzilv2
	sub	cl,ss:nsecs[bp]	;start over at 1
	inc	cx
dzilv2:	cmp	ss:wsize[bp],16d ;PDP-11 disk?
	jne	dzilv3		;no, no skew
	mov	al,2		;skew factor
	mul	ch		;*track
	add	al,cl		;add (sec-1)
	adc	ah,0
	div	ss:nsecs[bp]	;track size
	mov	cl,ah		;get remainder
dzilv3:	add	cx,101h		;track,,sector +1
	cmp	ch,ss:ncyls[bp]	;off end of disk?
	jne	dzilv4
	 xor	ch,ch		;yes, wrap around to make track 0 last track
dzilv4:	ret
;+
;
; TSS/8 PUTR DECtape interleave routine.
;
; TSS/8 PUTR DECtapes use 11:1 interleave and make multiple passes over the
; tape (necessary to avoid shoeshining since there's only one block buffer),
; half of which are in reverse.
;
; The interleave is arranged so that the first half of what OS/8 would call
; block 1 is in the same place that it would be on an OS/8 tape (i.e.  DECtape
; block 2).  Everything else unfolds from there -- use every 11th block, if the
; next block would be off the beginning or end of the tape, add 1 to the last
; *good* block number (and move it 11 blocks if necessary to get closer to that
; extreme of the tape) and go from there in the opposite direction.
;
; So, starting with OS/8 block #1, it's like this:
;
; 2 ... 1465
; 1466 ... 3
; 4 ... 1467
; 1468 ... 5
; 6 ... 1469
; 1470 ... 7
; 8 ... 1471
; 1472 ... 9
; 10 ... 1473
; 1463 ... 0
; 1 ... 1464
;
; ss:bp	logical device rec (preserved)
; ax	logical 129-word DECtape block number (0::1473.)
;
; On return:
; ax	physical 129-word DECtape block number
; bx	-1 => block will be read/written in reverse, 0 => forwards
;
;-
puilv:	sub	ax,2*1		;whole system starts with logical block 1
	jnc	puilv1
	 add	ax,1474d	;relocate block 0 to end
puilv1:	mov	dl,134d		;# blocks per pass (1474./11.)
	div	dl		;AL=pass number (0::10.), AH=rel block within
				;pass (0::133.)
	mov	dl,al		;copy pass #
	xor	bx,bx		;assume going forwards
	test	dl,1		;odd pass?
	jz	puilv2		;no, even passes are forwards
	; odd passes are in reverse, fix numbers
	neg	ah		;-133.::0
	add	ah,133d		;0::133.
	not	bx		;flip flag, blk will be obverse complement
puilv2:	; apply 11:1 interleave
	mov	al,11d		;we use every 11th block
	mul	ah
	xor	dh,dh		;DH=0
	add	dl,2		;first pass starts at block 2
	cmp	dl,11d		;need to wrap?
	jb	puilv3
	 sub	dl,11d		;yes
puilv3:	add	ax,dx		;find total
	ret
;
osilv:	; OS/8 DECtape interleave routine
	xor	bx,bx		;always forwards, and leave AX alone
	ret
;
psilv:	; PS/8 DECtape interleave routine
	xor	bx,bx		;assume forwards
	add	ax,ax		;*2
if 1
	cmp	ax,1024d	;in first piece?
	jb	psilv1
	sub	ax,1024d-1
else
	cmp	ax,1474d	;still in range?
	jb	psilv1
	sub	ax,1474d+1	;assume starts over at blk 1????
;;; or does it reverse direction???
endif
psilv1:	ret
;
	subttl	DECtape image file I/O
;+
;
; 12-bit DECtape image file I/O routines.
;
;-
rddt:	; read from 12-bit DECtape
	add	cx,cx		;# 256-word blks *2 (fits in 64 KB, no carry)
	add	ax,ax		;starting block # *2 (.LE. 737. => .LE. 1474.)
	push	di		;save starting posn
rddt1:	; read next DECtape block (AX=128-word log blk #, CX=remaining count)
	push	ax		;save logical block #
	push	cx		;save length
	call	ss:fintlv[bp]	;compute interleave
	call	ss:rdsec[bp]	;read sector
	jc	rddt2
	pop	cx		;restore blk count
	pop	ax		;and blk #
	inc	ax		;blk # +1
	loop	rddt1		;loop through all DECtape blocks
	mov	cx,di		;get ending addr
	pop	si		;restore starting addr
	sub	cx,si		;find length read
	jmp	trim12		;trim if needed, return
rddt2:	pop	cx		;[flush stack]
	pop	ax
	pop	si
	ret			;error return
;
wrdt:	; write to 12-bit DECtape
	add	cx,cx		;# 256-word blks *2 (fits in 64 KB, no carry)
	add	ax,ax		;starting block # *2 (.LE. 737. => .LE. 1474.)
wrdt1:	; write next DECtape block (AX=128-word log blk #, CX=remaining count)
	push	ax		;save logical block #
	push	cx		;save length
	call	ss:fintlv[bp]	;compute interleave
	call	ss:wrsec[bp]	;write sector
	jc	wrdt2
	pop	cx		;[restore blk count]
	pop	ax		;[and blk #]
	inc	ax		;[blk # +1]
	loop	wrdt1		;[loop through all DECtape blocks]
	ret			;happy
wrdt2:	pop	cx		;[flush stack]
	pop	ax
	ret			;error return
;+
;
; Read a block from a 12-bit DECtape image file.
;
; es:di	buf ptr (updated on return)
; bx	NZ if block should be accessed in reverse
;
;-
rddtf:	mov	si,bx		;save "reverse" flag
	mov	bx,ss:secsz[bp]	;get # bytes per block (128.*2 or 129.*2)
	mul	bx		;find starting addr
	mov	cx,dx		;in CX:DX
	mov	dx,ax
	test	si,si		;going backwards?
	jz	rddtf1		;no
	cmp	bx,129d*2	;129-word blocks?
	jb	rddtf1		;no, must be 128
	add	dx,2		;yes, skip first word (use last 128 of blk)
	adc	cx,0
rddtf1:	mov	bx,ss:handle[bp] ;get handle
	mov	ax,4200h	;func=LSEEK from BOF
	int	21h
	jc	rddtf3
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	mov	dx,di		;get buf ptr
	mov	cx,128d*2	;get 128. words (regardless of file's blk size)
	mov	ah,3Fh		;func=read (BX=handle)
	int	21h
	pop	ds		;[restore]
	jc	rddtf3
	cmp	ax,cx		;got it all?
	jne	rddtf3
	; undo obverse complement if block was written in reverse
	mov	bx,di		;save pointer in case reverse
	add	di,cx		;advance
	test	si,si		;well?
	jz	rddtf2		;written forwards, already OK
	 call	obv		;convert back from obverse complement
rddtf2:	clc			;happy
	ret
rddtf3:	stc			;error
	ret
;+
;
; Write a block to a 12-bit DECtape image file.
;
; es:si	buf ptr (updated on return)
; bx	NZ if block should be accessed in reverse
;
;-
wrdtf:	mov	di,bx		;save "reverse" flag
	; do obverse complement if block is to be written in reverse
	test	di,di		;well?
	jz	wrdtf2		;written forwards, already OK
	mov	bx,si		;save ptr
	push	si		;save
	call	obv		;convert to obverse complement
	pop	si		;restore
wrdtf2:	mov	bx,ss:secsz[bp]	;get # bytes per block (128.*2 or 129.*2)
	mul	bx		;find starting addr
	mov	cx,dx		;in CX:DX
	mov	dx,ax
	test	di,di		;going backwards?
	jz	wrdtf1		;no
	cmp	bx,129d*2	;129-word blocks?
	jb	wrdtf1		;no, must be 128
	add	dx,2		;yes, skip first word (use last 128 of blk)
	adc	cx,0
wrdtf1:	mov	bx,ss:handle[bp] ;get handle
	mov	ax,4200h	;func=LSEEK from BOF
	int	21h
	jc	wrdtf3
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	mov	dx,si		;get buf ptr
	mov	cx,128d*2	;get 128. words (regardless of file's blk size)
	mov	ah,40h		;func=write (BX=handle)
	int	21h
	pop	ds		;[restore]
	jc	wrdtf3
	cmp	ax,cx		;did it all?
	jne	wrdtf3
	add	si,cx		;advance
	clc			;happy
	ret
wrdtf3:	stc			;error
	ret
;+
;
; Convert a 128-word (12 bits each) block to or from obverse complement form.
;
; DECtape is three bits wide (words are assembled from left to right) and uses
; Manchester encoding, so when you read it backwards, the bits are all
; complemented and the groups of 3 bits come in the opposite order.
;
; This is easy to convert with 12-bit DECtapes.  PDP-11 DECtapes are a little
; confusing because the controller transfers 16-bit words by DMA, but the tape
; format actually uses 18-bit words (the high two bits are ignored), so a
; PDP-11 version of this routine would be more confusing.  Too bad the TC11
; fixes the obverse complement-ness and word alignment in hardware when doing
; I/O in reverse, otherwise this would be a cheapskate way to get the extra 2
; bits in tapes from 18-bit machines w/o having to use RALL, or having to
; bit-bang between words during DMA with interrupts off (the way FILEX.SAV does
; for TOPS-10 tapes).
;
; Anyway for now this routine handles 12-bit words only.
;
; es:bx	points at begn of block
; es:di	preserved
;
;-
obv:	lea	si,[bx+(128d*2)] ;point at end of block
obv1:	; do next pair of words
	mov	ax,es:[bx]	;read next word
	call	obv2		;convert it
	dec	si		;-2
	dec	si
	xchg	ax,es:[si]	;swap with word at other end of block
	call	obv2		;convert that
	mov	es:[bx],ax	;save
	inc	bx		;+2
	inc	bx
	cmp	bx,si		;done all?
	jb	obv1		;loop if not
	ret
obv2:	; compute obverse complement of word in AX
	not	ax		;this part is easy
	mov	dl,ah		;get 3 MSBs
	mov	dh,al		;and 3 LSBs
	add	dh,dh		;left-justify
	shr	dl,1		;and right-justify
	and	dx,7007		;isolate
	mov	cl,2		;shift count
	shl	ax,cl		;left 2 to get byte boundary at 8:7
	inc	cx		;shift count =3
	shl	ah,cl		;move 2nd from left into place
	and	ah,70		;isolate
	or	dl,ah		;save
	add	ax,ax		;2nd from right left 1 bit
	and	ax,700		;isolate
	or	ax,dx		;final result
	ret
;
rdpu:	; read PUTR block and convert to ASCII
	call	ss:rdblk[bp]	;read
	jc	rdpu6		;error
	mov	di,si		;copy ptr
	mov	dx,si		;twice
	mov	bx,cx		;copy count
	shr	bx,1		;/2=wc
	shr	bx,1		;/4=count of char triplets
	mov	cl,4		;bit count
rdpu1:	; read two words, write three chars
	lods	word ptr es:[si] ;get a word (4 MSBs =0)
	ror	ax,cl		;right-justify high 8 bits, save low 4
	and	al,177		;7-bit ASCII
	jz	rdpu2		;skip if NUL
	cmp	al,177		;EDIT.SAV inserts RUBOUTs after TABs
	je	rdpu2
	stosb			;save 1st byte
rdpu2:	mov	al,ah		;get low 4 bits of 1st word
	or	al,es:[si+1]	;combine with high 4 bits of 2nd word
	and	al,177		;7 bits
	jz	rdpu3
	cmp	al,177		;remove RUBOUTs
	je	rdpu3
	stosb
rdpu3:	lods	word ptr es:[si] ;get 2nd word
	and	al,177		;7 bits
	jz	rdpu4
	cmp	al,177		;remove RUBOUTs
	je	rdpu4
	stosb
rdpu4:	dec	bx		;done all?
	jnz	rdpu1		;loop
rdpu5:	mov	si,dx		;set ptr
	mov	cx,di		;compute length
	sub	cx,si		;(CF=0)
rdpu6:	ret
;+
;
; On entry:
; bp	dev record
; ax:dx	block number to write
; cx	number of blocks to write
; es:si	ptr to buf
;
; On return, CF=0 on success.
;
;-
wrpu:	; write ASCII blocks to PUTR DECtape
	mov	di,offset buf	;point at buf
	push	cx		;save blk count
	push	dx		;save blk #
	push	ax
	mov	bx,384d/3	;# ASCII char triplets per 256-word block
	mov	cx,4
wrpu1:	lods	word ptr es:[si] ;get first two chars
	or	ax,200*101h	;set MSBs of both
	xchg	al,ah		;switch places
	shr	ax,cl		;right-justify AAAAAAAABBBB
	mov	[di],ax		;save
	mov	ah,es:[si-1]	;get LSBs of 2nd char
	and	ah,17		;isolate
	lods	byte ptr es:[si] ;get third
	or	al,200		;set MSB
	mov	[di+2],ax	;save
	add	di,cx		;skip
	dec	bx		;loop through all
	jnz	wrpu1
	pop	ax		;catch blk #
	pop	dx
	push	dx		;save again
	push	ax
	push	es		;save source ptr
	push	si
	push	ds		;copy DS to ES
	pop	es
	mov	si,offset buf	;pt at buf
	mov	cx,1		;1 block
	call	ss:wrblk[bp]	;write it
	pop	si		;[restore all]
	pop	es
	pop	ax
	pop	dx
	pop	cx
	jc	wrpu2		;error, return
	inc	ax		;[bump block (PUTR tapes always <4 K blks)]
	loop	wrpu		;[loop through all blocks (CF=0)]
wrpu2:	ret
;
	subttl	floppy image file I/O
;+
;
; Read/write sector from 8" floppy image file.
;
; bp	log dev rec
; cl	sector
; ch	cyl
; dh	head
;
; Buffer is at DS:DBUF
;
; Aborts (printing msg) on error.
;
;-
rddxf:	call	seekdx		;seek
	jc	rddxf2		;track 0 of block image, return E5s
	mov	ah,3Fh		;func=read
	int	21h		;set CF
	jc	rddxf1
	 ret
rddxf1:	jmp	rderr
rddxf2:	push	es		;save
	push	ds		;copy DS to ES
	pop	es
	mov	di,dx		;point at buf
	mov	al,0E5h		;pretend track 0 is freshly formated
	rep	stosb
	pop	es
	clc			;happy return
	ret
;
wrdxf:	call	seekdx		;seek
	jc	wrdxf2		;track 0 of block image
	mov	ah,40h		;func=write
	int	21h		;set CF
	jc	wrdxf1
	 ret
wrdxf1:	jmp	wrerr
wrdxf2:	clc			;say we're happy
	ret
;+
;
; Calculate 8" floppy image file address and seek there.
;
; On entry:
; bp	log dev rec
; ch	track
; cl	sector
; dh	head
;
; On return, file pointer is at the correct sector, and:
; bx	file handle
; cx	sector length
; dx	pointer to DS:DBUF
;
; Or CF=1 if block image and sector is on track 0 (i.e. is not part of file).
;
;-
seekdx:	test	ss:ilimg[bp],-1	;interleaved image file?
	jz	sekdx3		;no
	; undo track-to-track skew if PDP-11 disk
	sub	cx,101h		;track, sec -1
	jc	sekdx5		;it's track 0, bit bucket
	cmp	ss:wsize[bp],16d ;PDP-11 disk?
	jne	sekdx1		;no, had no skew so don't undo
	mov	al,-6		;track skew
	imul	ch		;*track # (-450. :: 0)
	add	al,cl		;add sector
	adc	ah,0
	add	ax,26d*18d	;ensure positive (mod 26.)
	mov	cl,26d		;secs/track
	div	cl		;get sector mod 26.
	mov	cl,ah		;get remainder
sekdx1:	; undo sector interleave
	shr	cl,1		;/2
	jnc	sekdx2
	 add	cl,26d/2	;odd sectors are from 2nd revolution
sekdx2:	inc	cx		;sec +1
sekdx3:	; now do the math, depending on sector size and # of sides
	mov	al,26d		;sectors/track
	cmp	ss:nhds[bp],2	;double sided?
	jb	sekdx4		;no
	add	al,al		;sectors/cylinder
	test	dh,dh		;side 2?
	jz	sekdx4
	add	cl,26d		;yes, skip to other head
sekdx4:	mul	ch		;get base sector # of cylinder
	dec	cx		;start sector #s at 0
	add	al,cl		;add it in
	adc	ah,0
	mul	ss:secsz[bp]	;find base addr
	; DX:AX is starting addr in file, seek there
	mov	cx,dx		;in cx:dx
	mov	dx,ax
	mov	bx,ss:handle[bp] ;get handle
	mov	ax,4200h	;func=seek from BOF
	int	21h
	jc	sekdx6
sekdx5:	; enter here with CF=1 if track 0
	mov	cx,ss:secsz[bp]	;[byte count]
	mov	dx,ds:dbuf	;[buf addr]
	ret			;CF=1 from JC SEKDX5 or 0 from INT 21h
sekdx6:	jmp	sekerr		;seek error
;+
;
; Read/write sector from RX50/RX52 floppy image file.
;
; bp	log dev rec
; cl	sector
; ch	cyl
; dh	head
;
; Buffer is at DS:DBUF
;
; Aborts (printing msg) on error.
;
;-
rddzf:	call	seekdz		;seek
	mov	ah,3Fh		;func=read
	int	21h		;set CF
	jc	rddzf1
	 ret
rddzf1:	jmp	rderr
;
wrdzf:	call	seekdz		;seek
	mov	ah,40h		;func=write
	int	21h		;set CF
	jc	wrdzf1
	 ret
wrdzf1:	jmp	wrerr
;+
;
; Calculate RX50/RX52 floppy image file address and seek there.
;
; On entry:
; bp	log dev rec
; ch	track
; cl	sector
; dh	head
;
; On return, file pointer is at the correct sector, and:
; bx	file handle
; cx	sector length
; dx	pointer to DS:DBUF
;
;-
seekdz:	test	ss:ilimg[bp],-1	;interleaved image file?
	jz	sekdz4		;no
	; undo track-to-track skew if PDP-11 disk (PDP-8 has none)
	sub	cx,101h		;track, sec -1
	jnc	sekdz1		;it's track 0, bit bucket
	 add	ch,ss:ncyls[bp]	;wrap to end of disk
sekdz1:	cmp	ss:wsize[bp],16d ;PDP-11 disk?
	jne	sekdz2		;no, had no skew so don't undo
	mov	al,-2		;track skew
	imul	ch		;*track # (-158. :: 0)
	add	al,cl		;add sector
	adc	ah,0
	add	ax,10d*16d	;ensure positive (mod 10.)
	mov	cl,10d		;secs/track
	div	cl		;get sector mod 26.
	mov	cl,ah		;get remainder
sekdz2:	; undo sector interleave
	shr	cl,1		;/2
	jnc	sekdz3
	 add	cl,10d/2	;odd sectors are from 2nd revolution
sekdz3:	inc	cx		;sec +1
sekdz4:	; now do the math, depending on sector size and # of sides
	mov	al,10d		;sectors/track
	cmp	ss:nhds[bp],2	;double sided?
	jb	sekdz5		;no
	mov	al,10d*2	;sectors/cylinder
	test	dh,dh		;side 2?
	jz	sekdz5
	add	cl,10d		;yes, skip 10. sectors
sekdz5:	mul	ch		;get base sector # of cylinder
	dec	cx		;start sector #s at 0
	add	al,cl		;add it in
	adc	ah,0
	mul	ss:secsz[bp]	;find base addr
	; DX:AX is starting addr in file, seek there
	mov	cx,dx		;in CX:DX
	mov	dx,ax
	mov	bx,ss:handle[bp] ;get handle
	mov	ax,4200h	;func=seek from BOF
	int	21h
	jc	sekdz6
	mov	cx,ss:secsz[bp]	;(byte count)
	mov	dx,ds:dbuf	;(buf addr)
	ret
sekdz6:	jmp	sekerr		;seek error
;+
;
; Read/write sector from RX23/RX24/RX26/RX33 floppy image file.
;
; bp	log dev rec
; cl	sector
; ch	cyl
; dh	head
;
; Buffer is at DS:DBUF
;
; Aborts (printing msg) on error.
;
;-
rdduf:	call	seekdu		;seek
	mov	ah,3Fh		;func=read
	int	21h		;set CF
	jc	rdduf1
	 ret
rdduf1:	jmp	rderr
;
wrduf:	call	seekdu		;seek
	mov	ah,40h		;func=write
	int	21h		;set CF
	jc	wrduf1
	 ret
wrduf1:	jmp	wrerr
;+
;
; Calculate RX23/RX24/RX26/RX33 floppy image file address and seek there.
;
; This has only been tested with RX33s, I'm not actually sure whether there is
; any standard DEC way to connect 3.5" disks to a PDP-11.  I'm assuming that
; the other formats also use 1:1 interleave and no skew since they seem to
; mirror PC formats and PCs don't play tricks with interleave.  Hopefully if
; this is not true someone will tell me!
;
; On entry:
; bp	log dev rec
; ch	track
; cl	sector
; dh	head
;
; On return, file pointer is at the correct sector, and:
; bx	file handle
; cx	sector length
; dx	pointer to DS:DBUF
;
;-
seekdu:	; don't worry about interleaved files since a sector image is the same
	; as a block image with RX33 disks
	; now do the math, depending on sector size and # of sides
	mov	al,ch		;get cyl
	mul	ss:nhds[bp]	;*2 if 2-sided
	add	al,dh		;add in head # to get abs track #
	dec	cx		;start sector #s at 0
	mul	ss:nsecs[bp]	;find abs sector # of base of track
	add	al,cl		;add sector within track
	adc	ah,0
	mul	ss:secsz[bp]	;find byte address of sector
	; DX:AX is starting addr in file, seek there
	mov	cx,dx		;put in CX:DX
	mov	dx,ax
	mov	bx,ss:handle[bp] ;get handle
	mov	ax,4200h	;func=seek from BOF
	int	21h
	jc	sekdu1
	mov	cx,ss:secsz[bp]	;(byte count)
	mov	dx,ds:dbuf	;(buf addr)
	ret
sekdu1:	jmp	sekerr		;seek error
;
	subttl	floppy I/O
;
; Change DX from OLD (if specified) to NEW in the fewest possible opcode bytes.
ioport	macro	new,old
ifnb <old>
if new gt old
 if new le old+2
  if new eq old+2
	inc	dx
  endif
	inc	dx
 else
  if (high new) eq (high old)
	add	dl,new-old
  else
	mov	dx,new
  endif
 endif
endif
if new lt old
 if new ge old-2
  if new eq old-2
	dec	dx
  endif
	dec	dx
 else
  if (high new) eq (high old)
	sub	dl,old-new
  else
	mov	dx,new
  endif
 endif
endif
else
	mov	dx,new		;;no "old" specified
endif
	endm
;+
;
; Init floppy drive for a particular format.
;
; bp	dev record
;
; Specify command takes 2 bytes of args, stored at FDSPC.
;
; First byte:
;
;  7     4 3     0
; +-------+-------+
; |  SRT  |  HUT  |
; +-------+-------+
;
; SRT	step rate time
;	1 MHz:		(16.-SRT)/2 msec
;	500 kHz:	(16.-SRT) msec
;	300 kHz:	(16.-SRT)*5/3 msec
;	250 kHz:	(16.-SRT)*2 msec
;
; HUT	head unload time (treat 0 as 16.)
;	1 MHz:		HUT*8. msec
;	500 kHz:	HUT*16. msec
;	300 kHz:	HUT*80./3 msec
;	250 kHz:	HUT*32. msec
;
; Second byte:
;
;  7       1  0
; +---------+--+
; |   HLT   |ND|
; +---------+--+
;
; HLT	head load time (treat 0 as 128.)
;	1 MHz:		HLT msec
;	500 kHz:	HLT*2 msec
;	300 kHz:	HLT*10./3 msec
;	250 kHz:	HLT*4 msec
;
; ND	not DMA mode if 1, DMA mode if 0
;
;-
inidx:	; RX01 (8" SS SD)
	mov	word ptr ds:fdspc,066Fh ;SRT=10ms @16MHz, HUT=240ms, HLT=6ms,
				;DMA
	mov	word ptr ds:fdlen,1A00h ;26. secs/trk, 128. bytes/sec
	mov	word ptr ds:fdgpl,08007h ;gap length=7, max xfr=128. secs
				;(some FDC chips define DTL to be b/s if N=0
	mov	word ptr ds:fdmtr,18d ;1 second motor spin-up time
	mov	byte ptr ds:fdden,fm ;single density
	mov	word ptr ds:fdgpf,0E51Bh ;gap length (format)=27., fill=E5
	mov	byte ptr ds:fddcr,0 ;data rate = 250kHz/360RPM
	mov	byte ptr ds:fdvrt,0 ;longitudinal mode
	jmp	fdini		;init, return
;
inidy:	; RX02 (8" SS DD) or so-called "RX03" (8" DS DD)
	mov	word ptr ds:fdspc,066Fh ;SRT=10ms @16MHz, HUT=240ms, HLT=6ms,
				;DMA
	mov	word ptr ds:fdlen,1A01h ;26. secs/trk, 256. bytes/sec
	mov	word ptr ds:fdgpl,0FF0Eh ;gap length=14., max xfr=255. secs
	mov	word ptr ds:fdmtr,18d ;1 second motor spin-up time
	mov	byte ptr ds:fdden,mfm ;double density
	mov	word ptr ds:fdgpf,0E536h ;gap length (format)=54., fill=E5
	mov	byte ptr ds:fddcr,0 ;data rate = 500kHz/360RPM
	mov	byte ptr ds:fdvrt,0 ;longitudinal mode
	jmp	fdini		;init, return
;
ini24:	; RX24 3.5" (or 5.25" disk in TM100-4 on regular single-rate PC FDC)
;;; these parameters work for 360 KB disks too, although in a 1.2 MB drive they
;;; must be double-stepped (48 TPI disk in a 96 TPI drive)

;;; 320KB disks are also the same except that GAP3 is 1Bh (GAP3FORMAT is still
;;; 50h)
	mov	word ptr ds:fdspc,02DFh ;SRT=6ms @8MHz, HUT=480ms, HLT=4ms, DMA
	mov	word ptr ds:fdlen,0902h ;9. secs/trk, 512. bytes/sec
	mov	word ptr ds:fdgpl,0FF2Ah ;gap length=42., max xfr=255. secs
				;i82078 book says GAP3=1Bh, WRONG
	mov	word ptr ds:fdmtr,9d+1 ;1/2 second motor spin-up time
	mov	byte ptr ds:fdden,mfm ;double density
	mov	word ptr ds:fdgpf,0F650h ;gap length (format)=80., fill=F6
				;i82078 book says GAP3=54h, WRONG
	mov	byte ptr ds:fddcr,2 ;data rate = 250kHz/300RPM
	mov	byte ptr ds:fdvrt,0 ;longitudinal mode
	jmp	fdini		;init, return
;
ini23:	; RX23 (3.5" DS HD)
	mov	word ptr ds:fdspc,02CFh ;SRT=4ms @16MHz, HUT=240ms, HLT=2ms,
				;DMA
	mov	word ptr ds:fdlen,1202h ;18. secs/trk, 512. bytes/sec
	mov	word ptr ds:fdgpl,0FF1Bh ;gap length=27., max xfr=255. secs
	mov	word ptr ds:fdmtr,9d+1 ;1/2 second motor spin-up time
	mov	byte ptr ds:fdden,mfm ;double density
	mov	word ptr ds:fdgpf,0F66Ch ;gap length (format)=108., fill=F6
				;i82078 book says GAP3=54h, WRONG
	mov	byte ptr ds:fddcr,0 ;data rate = 500kHz/300RPM
	mov	byte ptr ds:fdvrt,0 ;longitudinal mode
	jmp	fdini		;init, return
;
ini26:	; RX26 (3.5" DS ED)
	mov	word ptr ds:fdspc,02AFh ;SRT=3ms (@32MHz), HUT=120ms, HLT=1ms,
				;DMA
	mov	word ptr ds:fdlen,2402h ;36. secs/trk, 512. bytes/sec
	mov	word ptr ds:fdgpl,0FF1Bh ;gap length=27., max xfr=255. secs
				;i82078 data book says GAP3=38h, WRONG
	mov	word ptr ds:fdmtr,9d+1 ;1/2 second motor spin-up time
	mov	byte ptr ds:fdden,mfm ;double density
	mov	word ptr ds:fdgpf,0F654h ;gap length (format)=84., fill=F6
				;PC8477B and i82078 data books both say
				;GAP3=53h, but this is what FORMAT.COM uses
				;(with Award BIOS anyway), and Linux too
	mov	byte ptr ds:fddcr,3 ;data rate = 1MHz/300RPM
	mov	byte ptr ds:fdvrt,-1 ;vertical mode
	jmp	short fdini	;init, return
;
ini33:	; RX33 (5.25" DS HD)
	mov	word ptr ds:fdspc,02DFh ;SRT=3ms, HUT=240ms, HLT=2ms, DMA
	mov	word ptr ds:fdlen,0F02h ;15. secs/trk, 512. bytes/sec
	mov	word ptr ds:fdgpl,0FF1Bh ;gap length=27., max xfr=255. secs
				;i82078 book says GAP3=2Ah, WRONG
	mov	word ptr ds:fdmtr,9d+1 ;1/2 second motor spin-up time
	mov	byte ptr ds:fdden,mfm ;double density
	mov	word ptr ds:fdgpf,0E554h ;gap length (format)=84., fill=E5
				;i82078 book says GAP3=50h, WRONG
	mov	byte ptr ds:fddcr,0 ;data rate = 500kHz/360RPM
	mov	byte ptr ds:fdvrt,0 ;longitudinal mode
	jmp	short fdini	;init, return
;
inidz:	; RX50 (5.25" SS DD 96tpi)
	mov	word ptr ds:fdspc,02DFh ;SRT=6ms, HUT=480ms, HLT=4ms, DMA
	mov	word ptr ds:fdlen,0A02h ;10. secs/trk, 512. bytes/sec
	mov	word ptr ds:fdgpl,0FF14h ;gap length=20., max xfr=255. secs
	mov	word ptr ds:fdmtr,9d+1 ;1/2 second motor spin-up time
	mov	byte ptr ds:fdden,mfm ;double density
	mov	word ptr ds:fdgpf,0E518h ;gap length (format)=24., fill=E5
	mov	byte ptr ds:fddcr,1 ;data rate = 250kHz/300RPM or 300kHz/360RPM
	; use 2 (250kHz/300RPM) for PC/XT w/TM100-4, or FDADAP with real RX50
	mov	bl,ss:drive[bp]	;get floppy unit #
	xor	bh,bh
	cmp	byte ptr ds:fdtype[bx],rx50 ;is it a real RX50?  (FDADAP etc.)
	jne	inidz1		;no
	 mov	byte ptr ds:fddcr,2 ;data rate = 250kHz/300RPM for sure
inidz1:	mov	byte ptr ds:fdvrt,0 ;longitudinal mode
	;jmp	short fdini	;init, return
;+
;
; Init floppy I/O.
;
;-
fdini:	mov	al,ss:drive[bp]	;get drive #
	mov	ds:fddrv,al	;save
	cmp	ss:hwtype[bp],hwcwsl ;Catweasel?
	jne	fdini1
	 jmp	cwinit		;yes, handle it
fdini1:	; build digital output register value
	push	es		;save
	xor	ax,ax		;load 0 into ES
	mov	es,ax
	cli			;so MOTOR_STATUS doesn't change before our eyes
	mov	al,es:motor_status ;;get motor bits
	pop	es		;;restore
	and	al,17		;;isolate
	mov	cx,4		;;loop count
	xor	ah,ah		;;init unit #
	mov	ah,10		;;assume drive 0, set IE
fdini2:	ror	al,1		;;right a bit
	jnc	fdini3
	mov	ah,14		;;calc unit # (leave IE set)
	sub	ah,cl		;;10 + unit #
fdini3:	loop	fdini2		;;loop through all 4 bits
	or	al,ah		;;OR it in (with IE)
	; hard reset FDC (it might be in some new mode we don't know about)
	ioport	fdcdor		;;digital output register
	test	byte ptr ds:fdrst,-1 ;;already done?  (once after each prompt)
	jnz	fdini4
	out	dx,al		;;hard reset (or not, these days)
	jmp	short $+2	;;miniscule I/O delay
	or	al,4		;;reenable
	out	dx,al		;;enable FDC
	inc	ds:fdrst	;;set flag
	ioport	fdccc4,fdcdor	;;switch to CompatiCard IV control reg
	mov	al,10		;;set DMA enable
	mov	ds:cc4ctl,al	;;save
	test	byte ptr ds:cc4fdc,-1 ;;is it a CC4?
	jz	fdini4		;;no
	 out	dx,al		;;(got overwritten when we wrote FDCDOR)
fdini4:	sti			;;reenable ints
	; set data rate
	ioport	fdcdcr		;pt at port (DX is either FDCDOR or FDCCC4)
	mov	al,ds:fddcr	;set value
	out	dx,al
	; send specify command (set timing for this drive)
	; this has to do with the drive head movement timing, not the disk
	; format, the numbers used hopefully will work on all drives, anyway
	; for PC-like disks they're the same numbers BIOS uses
	mov	si,offset necbuf ;pt at buf
	mov	byte ptr [si],3	;cmd=specify
	mov	ax,ds:fdspc	;get specify bytes
	mov	[si+1],ax	;save
	mov	cx,3		;length
	call	sfdc		;send to FDC
	; check FDC chip version
	mov	si,offset necbuf ;pt at buf
	mov	byte ptr [si],20 ;cmd=get version (or invalid cmd on uPD765)
	mov	cx,1		;length
	call	sfdc		;send to FDC
	mov	cx,1		;length
	call	rfdcn		;get reply
	jc	fdini6		;evidently it doesn't understand
	mov	al,ds:necbuf	;get value
	cmp	al,200		;original uPD765 (or clone)?
	je	fdini6		;yes, doesn't know about vertical mode
	; set or clear perpendicular mode
	cmp	al,240		;could it be SMC37C65LJP+?
	jne	fdini5		;no
	ioport	fdcfcr		;point at format control reg (if it exists)
	mov	al,ds:fdvrt	;get vertical flag (in all bits)
	and	al,200		;isolate b7
	out	dx,al		;tell SMC chip which mode to use
fdini5:	mov	si,offset necbuf ;pt at buf
	mov	byte ptr [si],22 ;cmd=perpendicular mode
	mov	al,ds:fdvrt	;get vertical flag (in all bits)
	and	al,3		;set/clear GAP,WGATE
	; the per-drive bits (and OW) are cute but not supported in all FDCs
	mov	[si+1],al	;save
	mov	cx,2		;length
	call	sfdc		;send to FDC
fdini6:	ret
;
fdrds:	; read sector(s)
	mov	byte ptr ds:fdspn,0 ;no spin-up needed (retry instead)
	mov	ax,0A646h	;multitrack, skip deleted data, read data
	jmp	short fdxfr	;(doesn't work w/o multitrack bit!)
;
fdwrs:	; write sector(s)
	mov	byte ptr ds:fdspn,-1 ;spin up before writing
	mov	ax,854Ah	;multitrack, write sector,,DMA cmd
	jmp	short fdxfr	;write, return
;
if 0 ; this is never used
fdvfs:	; verify sector(s)
	mov	byte ptr ds:fdspn,0 ;no spin-up needed (retry instead)
	mov	ax,0A642h	;same as read w/different cmd to DMAC
;; actually, I think it just checks CRCs and needs no host data at all
	jmp	short fdxfr	;verify, return
endif
;
fdftk:	; format track
	mov	byte ptr ds:fdspn,-1 ;spin up before writing
	mov	ax,0D4Ah	;format track,,DMA cmd
	;jmp	short fdxfr	;do it, return
;+
;
; Perform a floppy transfer.
;
; al	DMA command
; ah	FDC command
; cl	sector
; ch	cyl
; dh	head
; ;dl	block count (not used, always 1)
;
; Buffer is always at DS:DBUF
;
; Returns CF=0 on success, otherwise CF=1 and al contains error code:
; 0	controller timeout (hardware failure)
; 1	seek error
; 2	some kind of transfer error (sector not found, fault)
; 200	write protect
;
;-
fdxfr:	mov	ds:fdcmd,ax	;save FDC cmd
	mov	word ptr ds:fdsec,cx ;and cyl,,sec
	mov	word ptr ds:fdnum,dx ;and head,,blk count (not used)
	mov	byte ptr ds:fdtry,5 ;init retry counter
fxfr1:	; compute length
	mov	cl,ds:fdlen	;get sector length (0=128., 1=256., 2=512.)
	add	cl,7		;correct
	mov	dx,1		;always copy one sector
	sal	dx,cl		;find byte count
	mov	cx,dx		;copy
	mov	al,byte ptr ds:fdcmd ;get DMA command
	call	dmaset		;set up DMA controller
				;(count will be too high if formating, but...)
	call	fdmon		;turn motor on
	call	fdsek		;seek if needed
	jc	fxfr5		;error
	; send I/O command to FDC
	mov	al,byte ptr ds:fdcmd+1 ;get FDC command
	or	al,ds:fdden	;get density flag
	mov	si,offset necbuf ;point at where string goes
	mov	ah,ds:fdhd	;get head
	sal	ah,1		;left 2
	sal	ah,1
	or	ah,ds:fddrv	;OR in drive #
	mov	[si],ax		;head/drive,,cmd
	test	al,10		;format cmd?
	jnz	fxfr2		;yes
	; read or write
	mov	al,ds:fdcyl	;cyl
	mov	[si+2],al
	mov	al,ds:fdhd	;head
	mov	[si+3],al
	mov	al,ds:fdsec	;rec (sector)
	mov	[si+4],al
	mov	ax,word ptr ds:fdlen ;get EOT,,N
	mov	[si+5],ax
	mov	ax,word ptr ds:fdgpl ;get DTL,,GPL
	mov	[si+7],ax
	mov	cx,9d		;length
	jmp	short fxfr3
fxfr2:	; format
	mov	ax,word ptr ds:fdlen ;get secs/trk,,bytes/sec
	mov	[si+2],ax
	mov	ax,word ptr ds:fdgpf ;get filler byte,,gap 3 length
	mov	[si+4],ax
	mov	cx,6		;length
fxfr3:	; whichever, send command
	call	sfdc		;write it
	jc	fxfr5		;timeout
	call	fdcwt		;wait for interrupt
	jc	fxfr5		;timeout
	call	rfdc		;read status
	jc	fxfr5		;timeout
	test	byte ptr ds:necbuf,300 ;happy completion?
	jnz	fxfr4		;no
	ret
fxfr4:	mov	al,2		;assume general error
	test	byte ptr ds:necbuf+1,2 ;write protect?
	jz	fxfr5
	mov	al,200		;yes, no point in retrying
	stc			;return
	ret
fxfr5:	; timeout or error
	mov	bl,ds:fddrv	;get drive #
	xor	bh,bh		;BH=0
	mov	byte ptr ds:fdccl[bx],-1 ;force recal
	dec	ds:fdtry	;retry?
	jz	fxfr6		;no, fail
	jmp	fxfr1		;yes
fxfr6:	stc
	ret
;+
;
; Seek to FDCYL if needed.
;
; CF=1 on error, AL=error code.
;
;-
fdsek:	; see whether a seek is needed
	mov	bl,ds:fddrv	;get drive #
	xor	bh,bh		;BH=0
	mov	al,ds:fdcyl	;get desired cyl
	cmp	al,ds:fdccl[bx]	;are we there?
	je	fdsek1		;yes
	xchg	al,ds:fdccl[bx]	;save if not
	cmp	al,-1		;do we even know where we are?
	jne	fdsek2		;yes
	; recal
	mov	ds:fdccl[bx],al	;set back to -1 until we succeed
	mov	si,offset necbuf ;pt at buf
	mov	ah,ds:fddrv	;get unit #
	mov	al,7		;cmd=recal
	mov	[si],ax		;save
	mov	cx,2		;length
	call	fdpos		;do recal operation
	jnc	fdsek2
	; retry the recal once, may require > 77 step pulses on 80-track drives
	; (older FDC chips give up after 77 pulses)
	mov	si,offset necbuf ;pt at buf
	mov	ah,ds:fddrv	;get unit #
	mov	al,7		;cmd=recal
	mov	[si],ax		;save
	mov	cx,2		;length
	call	fdpos		;do recal operation
	jnc	fdsek2
	jmp	short fdsek7
fdsek1:	jmp	short fdsek6
fdsek2:	; seek to cyl
	mov	si,offset necbuf ;pt at buffer
	mov	byte ptr [si],17 ;command=seek
	mov	al,ds:fdhd	;get head
	sal	al,1		;left 2
	sal	al,1
	or	al,ds:fddrv	;OR in drive #
	mov	ah,ds:fdcyl	;cyl #
	mov	[si+1],ax	;save
	mov	cx,3		;length
	call	fdpos		;do seek operation
	jc	fdsek7		;failed
	mov	bl,ds:fddrv	;get drive #
	xor	bh,bh		;BH=0
	mov	al,ds:fdcyl	;get newly seeked cyl
	mov	ds:fdccl[bx],al	;update
	; update TG43 (reduced write current) signal on CompatiCard IV
	mov	ah,10		;set DMA enable
	cmp	al,43d		;track greater than 43?
	jbe	fdsek3		;no
	 mov	ah,11		;yes, set CCEN and PN2 (Micro Solutions docs
				;say you have to set bit 2 also but that seems
				;to actually *prevent* assertion of pin 2
				;(DB37 pin 3) under some conditions
fdsek3:	mov	ds:cc4ctl,ah	;remember what to set
	; wait for head to settle
	; (funny, I thought the HLT parameter in the SPECIFY command took care
	; of this, but BIOS worries about it)
	push	es		;save
	xor	bx,bx		;point at BIOS data
	mov	es,bx
	mov	cx,2		;wait >=1 timer tick for head to settle
fdsek4:	mov	bx,es:timer_low	;get time
fdsek5:	cmp	bx,es:timer_low	;has it changed?
	je	fdsek5		;spin until it does
	loop	fdsek4		;then count the tick
	pop	es		;restore
fdsek6:	clc			;happy
fdsek7:	sbb	ah,ah		;save CF
	; update CompatiCard IV TG43 signal
	test	byte ptr ds:cc4fdc,-1 ;is it a CC4?
	jz	fdsek8		;no
	ioport	fdccc4		;point at CC4 control reg
	mov	al,ds:cc4ctl	;get current value
	out	dx,al
fdsek8:	shr	ah,1		;restore CF
	ret
;+
;
; Send a position (seek/recal) or reset command, and sense int status.
;
; Enter with ds:si, cx set up for SFDC.
;
; CF=1 on error, AL=error code.
;
;-
fdpos:	call	sfdc		;send command
	jc	fdpos1		;timeout
	call	fdcwt		;wait for completion
	jc	fdpos1		;timeout
	mov	si,offset necbuf ;pt at buffer
	mov	byte ptr [si],10 ;cmd=sense int status
	mov	cx,1		;length
	call	sfdc		;send
	jc	fdpos1		;timeout
	call	rfdc		;get result
	jc	fdpos1
	test	byte ptr ds:necbuf,300 ;happy termination?
	jz	fdpos1
	mov	al,1		;no, say seek error
	stc
fdpos1:	ret
;+
;
; Read next ID field.
;
; ch	cylinder
; dh	head
;
; si	points at DB C,H,R,N on successful return
;
; CF=1 on error, AL=error code.
;
;-
fdrid:	mov	byte ptr ds:fdspn,-1 ;spin up before reading
	mov	ds:fdcyl,ch	;save cyl
	mov	ds:fdhd,dh	;and head
	call	fdmon		;turn on motor if not already on
	call	fdsek		;seek if needed
	jc	fdrid2
	mov	al,12		;cmd=READ ID
	or	al,ds:fdden	;get density flag
	mov	si,offset necbuf ;point at where string goes
	mov	ah,ds:fdhd	;get head
	sal	ah,1		;left 2
	sal	ah,1
	or	ah,ds:fddrv	;OR in drive #
	mov	[si],ax		;head/drive,,cmd
	mov	cx,2		;len=2
	call	sfdc		;send command
	jc	fdrid2		;timeout
	call	fdcwt		;wait for completion
	jc	fdrid2		;timeout
	call	rfdc		;get result
	jc	fdrid2		;timeout
	test	byte ptr ds:necbuf,300 ;happy completion?  (CF=0)
	jnz	fdrid1
	mov	si,offset necbuf+3 ;[yes, point at C/H/R/N]
	ret			;CF=0 from TEST
fdrid1:	stc			;error return
fdrid2:	ret
;+
;
; Turn on floppy motor if not already on.
;
;-
fdmon:	push	es		;save
	xor	bx,bx		;load 0 into ES
	mov	es,bx
	mov	es:motor_count,377 ;don't time out yet
	mov	cl,ds:fddrv	;get drive #
	mov	al,1		;1 bit
	sal	al,cl		;shift into place
	mov	cl,es:motor_status ;get bits
	test	cl,al		;already on?
	jnz	fdmon4		;yes, skip this whole bit
	and	cl,not 17	;turn off other bits
	or	cl,al		;set this one
	mov	es:motor_status,cl ;save
	mov	cl,4		;need to shift 4 more bits
	sal	al,cl
	or	al,14		;set DMAEN, /DSELEN, clear SRST
	or	al,ds:fddrv	;OR in drive #
	ioport	fdcdor		;digital output register
	out	dx,al		;turn on motor
	ioport	fdccc4,fdcdor	;switch to CompatiCard IV control reg
	mov	al,ds:cc4ctl	;reset current value
	test	byte ptr ds:cc4fdc,-1 ;is it a CC4?
	jz	fdmon1		;no
	 out	dx,al		;(got overwritten when we wrote FDCDOR)
fdmon1:	; wait for spin-up if writing
	cmp	byte ptr ds:fdspn,0 ;need to spin up?
	jz	fdmon4		;no
	mov	cx,ds:fdmtr	;get spin-up time
fdmon2:	mov	bx,es:timer_low	;get time
fdmon3:	cmp	bx,es:timer_low	;has it changed?
	je	fdmon3		;spin until it does
	loop	fdmon2		;then count the tick
fdmon4:	mov	es:motor_count,18d*2+1 ;2 sec motor timeout
	pop	es		;restore
	ret
;+
;
; Set up DMA controller.
;
; al	mode (see 8237 (or NEC uPD71037) data sheet)
; cx	byte count
;
; We don't check for spanning 64 KB boundaries here because we checked our
; buffer when we allocated it.
;
;-
dmaset:	cli			;ints off
	out	0Bh,al		;;set mode reg
	out	0Ch,al		;;say low byte coming next
	; compute 20-bit absolute address
	mov	dx,ds		;;get addr
	mov	ah,cl		;;save cl
	mov	cl,4		;;bit count
	rol	dx,cl		;;left 4 bits, catch high 4 in low 4
	mov	cl,ah		;;restore cl
	mov	al,dl		;;get them
	and	dl,360		;;zero them out
	and	al,17		;;isolate (not really needed in 20-bit sys)
	add	dx,ds:dbuf	;;find base addr
	adc	al,0		;;add them in
	out	81h,al		;;set high 4 bits
	mov	al,dl		;;write low byte of addr
	out	04h,al		;;for channel 2
	mov	al,dh		;;high byte
	out	04h,al
	; write 16-bit byte count
	dec	cx		;;length -1
	mov	al,cl		;;write low byte of length
	out	05h,al		;;for channel 2
	mov	al,ch		;;get high byte
	out	05h,al
	mov	al,2		;;init DMA channel 2
	out	0Ah,al
	sti			;;ints back on
	ret
;+
;
; Send a string to the floppy disk controller.
;
; si,cx	addr, length of string
;
; Returns CF=1 on timeout.
;
;-
sfdc:	push	es		;save ES
	xor	ax,ax		;pt at BIOS data with es
	mov	es,ax
	and	es:seek_status,7Fh ;clear int bit
	ioport	fdcmsr		;main status register
	mov	ah,2		;timeout loop count (.GE. 1 timer tick)
sfdc1:	mov	bx,es:timer_low	;get time
sfdc2:	in	al,dx		;check it
	and	al,300		;isolate high 2 bits
	jns	sfdc3		;skip if not ready for xfr
	test	al,100		;ready for us to write?
	jnz	sfdc4		;no, it has something to say first
	ioport	fdcdat,fdcmsr	;data port
	lodsb			;get a byte
	out	dx,al
	ioport	fdcmsr,fdcdat	;restore
	loop	sfdc2		;loop
	pop	es		;restore
	clc			;happy
	ret
sfdc3:	; not ready, check for timeout
	cmp	bx,es:timer_low	;has time changed?
	je	sfdc2		;loop if not
	dec	ah		;-1
	jnz	sfdc1		;loop if not done (get new bx)
	pop	es		;restore
	xor	al,al		;error=timeout
	stc
	ret
sfdc4:	; FDC has something to say
	; (won't let us write anything until we've read it)
	ioport	fdcdat,fdcmsr	;data port
	in	al,dx		;that's nice dear
	ioport	fdcmsr,fdcdat	;status reg
	jmp	short sfdc2	;as I was saying...
;+
;
; Read FDC result data into NECBUF.
;
; CF=1 on timeout.
;
;-
rfdc:	mov	cx,7		;should be only 7 bytes
rfdcn:	; enter here with length in cx
	push	es		;save
	xor	bx,bx		;pt at BIOS data
	mov	es,bx
	mov	di,offset necbuf ;pt at buf
	ioport	fdcmsr		;main status reg
	mov	ah,2		;timeout tick counter
rfdc1:	mov	bx,es:timer_low	;get time
rfdc2:	in	al,dx		;get status bits
	and	al,300		;isolate request, direction
	jns	rfdc4		;not ready, see if timeout
	test	al,100		;is it ready to squeal?
	jz	rfdc3		;no, guess it thinks we're done
	ioport	fdcdat,fdcmsr	;point at data port
	in	al,dx		;get data
	mov	[di],al		;save it
	inc	di
	ioport	fdcmsr,fdcdat	;back to status port
	loop	rfdc2		;loop
rfdc3:	pop	es		;restore
	ret			;CF=0 from TEST above (either way)
rfdc4:	; not ready, see if timeout
	cmp	bx,es:timer_low	;has clock ticked
	je	rfdc2		;keep trying until it does
	dec	ah		;give up yet?
	jnz	rfdc1		;no, update bx and continue
	pop	es		;restore
	xor	al,al		;error=timeout
	stc
	ret
;+
;
; Wait for FDC completion interrupt.
;
; This is SOOO stupid, why not just poll the FDC?
;
; Return CF=1 if it takes more than 2 seconds.
;
; This depends on the BIOS being IBM-compatible enough to use the same stupid
; polling/int structure (ISR sets a flag, mainline polls the flag) and the same
; flag (high bit of SEEK_STATUS).
;
;-
fdcwt:	push	es		;save
	xor	cx,cx		;load 0 into ES
	mov	es,cx
	mov	cl,18d*2+1	;tick count (2 sec)
fdcwt1:	mov	ax,es:timer_low	;get timer
fdcwt2:	test	es:seek_status,80h ;has the int happened?
	jnz	fdcwt3		;yes
	cmp	ax,es:timer_low	;has time changed?
	je	fdcwt2
	loop	fdcwt1		;yes, count the tick
	xor	al,al		;error=timeout
	stc
fdcwt3:	pop	es		;[restore]
	ret
;+
;
; Print error message for FDC error.
;
; al	error code
;
;-
fderr:	test	al,al		;write protect?
	js	wrperr
	jz	timerr		;timeout
	cmp	al,1		;seek error?
	je	sekerr
	cram	'?General failure',error
wrperr:	cram	'?Write protect error',error
timerr:	cram	'?Timeout error',error
sekerr:	cram	'?Seek error',error
;
	subttl	Catweasel/ISA floppy I/O
;
	comment	|

;;; need to get CW RDSEC in place in time for DEFMED
;;; need to cache current head # for each CW drive -- no, for CW ctrl itself!

Catweasel registers (partly reverse engineered, may contain errors!!!):

base+0	autoincrementing memory I/O port

base+1	read (and ignore value) to clear address counter
	write 0 (or any value?) to stop I/O (w/o touching addr ctr)

base+2	raw floppy disk control/status signals
	(N.B. IOR and IOW are two different regs)

	control reg (write):
	b7	MO1 (0=asserted)
	b6	???
	b5	DS0 (0=asserted)
	b4	DS1 (0=asserted)
	b3	MO0 (0=asserted)
	b2	HEAD (1=head 0, 0=head 1)
	b1	DIR (1=out, 0=in)
	b0	STEP (0=asserted)

	status reg (read):
	b5	DSKCHG (0=asserted, deasserts on any seek)
	b4	TK00 (0=asserted)
	b3	WPROT (0=asserted)
	... index???

base+3	MSB gives access to a shift register I think???
	(selected by addr ctr so must touch base+0 to advance)

	bits can be read or written, dunno if they're the same ones
	(reading can be used to detect the board but I don't know if that's
	a hard-wired version #, or just the RESET value of the config bits)

	index 0 = 0 to select high-speed clock (HD), 1 for low-speed (DD)
	index 1 = set to 1 before writing -- why?
;;;; maybe makes it start at index pulse???
	index 2 = 0 to make index pulse not appear in data stream

base+4	???

base+5	write 0 to start writing to disk

base+6	???

base+7	read to start reading from disk

|
if cwhack
cwgid:	; get all IDs on current track
	mov	ch,ds:fdcyl	;get existing cyl and head
	mov	dh,ds:fdhd
	call	cwrid		;read next ID
	jc	cwgid1
	mov	di,offset lbuf	;point at buf
	mov	ax,"=C"		;cyl
	stosw
	lodsb
	call	pbyte
	mov	al,' '
	stosb
	mov	ax,"=H"		;head
	stosw
	lodsb
	call	pbyte
	mov	al,' '
	stosb
	mov	ax,"=R"		;rec (sec) #
	stosw
	lodsb
	call	pbyte
	mov	al,' '
	stosb
	mov	ax,"=N"		;sec size code
	stosw
	lodsb
	call	pbyte
	call	flush
	jmp	short cwgid
cwgid1:	ret
;
cwrst:	; restore Catweasel buffer contents from file
	call	getw		;get filename
	jc	cwrst2
	xchg	bx,dx		;swap
	add	bx,dx		;point at end
	mov	byte ptr [bx],0	;mark end
	mov	ax,3D00h	;func=open /RONLY
	int	21h
	jc	cwrst2
	mov	bx,ax		;copy handle
	ioport	ds:cwport	;get base port
	ioport	1,0		;clear addr ctr
	in	al,dx
cwrst1:	mov	dx,offset buf	;point at buf
	mov	si,dx		;(save ptr)
	mov	cx,bufsiz	;size
	mov	ah,3Fh		;func=read
	int	21h
	jc	cwrst2
	mov	cx,ax		;copy actual count
	jcxz	cwrst3		;EOF
	ioport	ds:cwport	;get base port
	.186
	rep	outsb		;(won't be trying this on PC or XT)
	.8086
	jmp	short cwrst1
cwrst2:	cram	'error',error
cwrst3:	mov	ah,3Eh		;func=close
	int	21h
	mov	byte ptr ds:cwnord,1 ;don't actually read the disk any more
	ret
;
cwsav:	; save Catweasel buffer contents to file
	call	getw		;get filename
	jc	cwsav3
	xchg	bx,dx		;swap
	add	bx,dx		;point at end
	mov	byte ptr [bx],0	;mark end
	xor	cx,cx		;default mode
	mov	ah,3Ch		;func=CREAT
	int	21h
	jc	cwsav3
	mov	bx,ax		;copy handle
	ioport	ds:cwport	;get base port
	ioport	1,0		;clear addr ctr
	in	al,dx
cwsav1:	mov	di,offset buf	;point at buf
cwsav2:	ioport	ds:cwport	;get base port
	in	al,dx		;read a byte
	stosb			;save
	test	al,al		;negative?
	js	cwsav4		;yes
	cmp	di,offset buf+bufsiz ;full?
	jb	cwsav2		;loop if not
	call	cwsav5		;flush
	jmp	short cwsav1	;loop
cwsav3:	cram	'error',error
cwsav4:	call	cwsav5		;flush
	mov	ah,3Eh		;func=close
	int	21h
	mov	byte ptr ds:cwnord,0 ;cheap way to clear flag, SAVE NUL
	ret
cwsav5:	; flush BUF
	mov	dx,offset buf	;point at buf
	mov	cx,di		;find size
	sub	cx,dx		;(known NZ)
	mov	ah,40h		;func=write
	int	21h
	jc	cwsav3
	ret
endif ; CWHACK
;+
;
; Parse Catweasel parameters.
;
; ss:bp	logical dev record
; al	drive # (0 or 1)
; si	pts at 'CWx:' string (not currently needed)
; bx	length of " " (not currently needed)
;
;-
getcw:	mov	ss:hwtype[bp],hwcwsl ;type=Catweasel floppy drive
	mov	ss:drive[bp],al	;save unit #
	mov	al,byte ptr ds:tdrv+1 ;see if any set
	or	al,byte ptr ds:tbpart+1
	or	al,byte ptr ds:ttype+1
	jnz	getcw1		;switch conflict if so
if 0
 mov ax,2880d
 xor bx,bx
 xor cx,cx
 xor dx,dx
	mov	ss:totsiz[bp],ax ;save
	mov	ss:totsiz+2[bp],bx
	mov	ss:totsiz+4[bp],cx
	mov	ss:totsiz+6[bp],dx
	mov	si,512d		;DEC block size
	xor	di,di
	call	div32		;find total # blocks in dev
	mov	ss:totblk[bp],ax ;save total block count
	mov	ss:totblk+2[bp],bx
	mov	ss:totblk+4[bp],cx
	mov	ss:totblk+6[bp],dx
	mov	ss:devsiz[bp],ax ;save usable block count
	mov	ss:devsiz+2[bp],bx
	mov	ss:devsiz+4[bp],cx
	mov	ss:devsiz+6[bp],dx
endif
;;;	mov	ss:dsrini[bp],offset cwinit ;DSR init
;;; probably shouldn't set that yet, done by INIxx
;;;	mov	ss:rdblk[bp],offset rdadk ;read from ASPI SCSI disk
;;;	mov	ss:wrblk[bp],offset wradk ;write to ASPI SCSI disk
	mov	ss:rdsec[bp],offset cwrds ;set up initial sector I/O routines
	mov	ss:wrsec[bp],offset cwwrs
	mov	ss:rdid[bp],offset cwrid
	ret
getcw1:	jmp	swtcon
;+
;
; Init controller.
;
;-
cwinit:	mov	al,377		;deassert all control lines
	mov	ds:cwlat,al	;(and turn off drive motors)
	ioport	ds:cwport	;get base port
	ioport	2,0		;ctl port
	out	dx,al		;release everything
	mov	word ptr ds:cwccl,-1 ;don't know either drive posn
	ret
;+
;
; Read a sector using a Catweasel/ISA board.
;
; cl	sector
; ch	cyl
; dh	head
; ;dl	block count (not used, always 1)
;
; Buffer is always at DS:DBUF
;
; Returns CF=0 on success, otherwise CF=1 and AL contains error code:
; 0	controller timeout (hardware failure)
; 1	seek error
; 2	some kind of transfer error (sector not found, fault)
;;; 200	write protect
;
;-
cwrds:	mov	word ptr ds:fdsec,cx ;and cyl,,sec
	mov	word ptr ds:fdnum,dx ;and head,,blk count (not used)
	mov	byte ptr ds:fdtry,5 ;init retry counter
crds1:
if 0 ;; done in CWRID now
	call	cwmon		;turn motor on
	call	cwsek		;seek if needed
	jc	crds7		;error
	je	crds2		;we already have this track
	 call	cwread		;read it into Catweasel buffer
				;(no error possible, YET)
crds2:
endif
	mov	cl,ds:fdeot	;# of secs/track
	xor	ch,ch		;zero-extend
	add	cx,cx		;allow for 2x as many just to be safe
crds3:	; search for our header
	push	cx		;save
	mov	ch,ds:fdcyl	;get cyl
	mov	dh,ds:fdhd	;and head
	call	cwrid		;read ID
	pop	cx		;[restore]
	jc	crds5		;none found
	mov	al,ds:fdcyl	;get requested cyl
	mov	ah,ds:fdhd	;get head
	cmp	ax,[si]		;match?
	jne	crds4
	mov	al,ds:fdsec	;get requested record (sector)
	mov	ah,ds:fdlen	;N
	cmp	ax,[si+2]	;match?
	je	crds6		;yes
crds4:	loop	crds3		;keep looking for our header
crds5:	mov	al,2		;sector not found
	stc			;error
	ret
crds6:	; found our header, read data field
	ioport	ds:cwport	;required by CWMSYN/CWMGET/etc.
	test	byte ptr ds:fdden,-1 ;MFM?
	jz	crds71		;no, skip
crds61:	; IBM System/34 MFM, or some variant
	call	cwmsyn		;find MFM AM sync sequence
	jc	crds64		;EOT
	call	cwmget		;get next byte
	jc	crds64		;EOT
	cmp	al,0FBh		;data address mark?
	jne	crds61		;no, keep looking
	mov	di,ds:dbuf	;point at buf
	mov	ax,128d		;min sector len
	mov	cl,ds:fdlen	;sector length code
	shl	ax,cl		;find actual length
	mov	cx,ax		;copy
	push	cx		;save for later
crds62:	push	cx		;save
	call	cwmget		;get next byte
	pop	cx		;[restore]
	jc	crds63		;EOT (1 word on stack)
	mov	[di],al		;save
	inc	di
	loop	crds62		;fetch data
	mov	di,offset necbuf ;point at buf
	call	cwmget		;get CRC
	jc	crds63		;EOT (1 word on stack)
	mov	[di],al		;save
	call	cwmget
	jc	crds63		;EOT (1 word on stack)
	mov	[di+1],al
	pop	cx		;restore length
	mov	si,ds:dbuf	;point at buf
	push	dx		;save I/O port
	mov	dx,0E295h	;init CRC remainder, including 3*A1, FB
	call	docrc		;check CRC
	pop	dx		;restore
	xchg	al,ah		;><
	cmp	ax,word ptr ds:necbuf ;correct CRC?
	jne	crds7		;no
	ret
crds63:	pop	cx		;restore
crds64:
;;; need to rewind, once anyway
 mov al,2
 stc
 ret





crds71:	; IBM 3740 FM, or some variant
	call	cwfmrk		;find FM addr mark
	jc	crds74		;EOT
	cmp	bx,0F56Fh	;FM data mark (FB/C7)?
	jne	crds71		;no, keep looking
	mov	di,ds:dbuf	;point at buf
	mov	ax,128d		;min sector len
	mov	cl,ds:fdlen	;sector length code
	shl	ax,cl		;find actual length
	mov	cx,ax		;copy
	push	cx		;save for later
crds72:	push	cx		;save
	call	cwfget		;get next byte
	pop	cx		;[restore]
	jc	crds73		;EOT (1 word on stack)
	mov	[di],al		;save
	inc	di
	loop	crds72		;fetch data
	mov	di,offset necbuf ;point at buf
	call	cwfget		;get CRC
	jc	crds73		;EOT (1 word on stack)
	mov	[di],al		;save
	call	cwfget
	jc	crds73		;EOT (1 word on stack)
	mov	[di+1],al
	pop	cx		;restore length
	mov	si,ds:dbuf	;point at buf
	push	dx		;save I/O port
	mov	dx,0BF84h	;init CRC remainder, including FB
	call	docrc		;check CRC
	pop	dx		;restore
	xchg	al,ah		;><
	cmp	ax,word ptr ds:necbuf ;correct CRC?
	jne	crds7		;no
	ret
crds73:	pop	cx		;restore
crds74:

;;; need to rewind, once anyway
 mov al,2
 stc
 ret




crds7:	; timeout or error
	mov	bl,ds:fddrv	;get drive #
	xor	bh,bh		;BH=0
	mov	byte ptr ds:cwccl[bx],-1 ;force recal
	dec	ds:fdtry	;retry?
	jz	crds8		;no, fail
	jmp	crds1		;yes
crds8:	stc
	ret
;
; Don't know how to write yet, will require a track buffer...
;
cwwrs:	xor	al,al
	stc
	ret
;+
;
; Read next ID field.
;
; ch	cylinder
; dh	head
;
; si	points at DB C,H,R,N on successful return
;
; CF=1 on error, AL=error code.
;
;-
cwrid:	mov	ds:fdcyl,ch	;save cyl
	mov	ds:fdhd,dh	;and head
	call	cwmon		;turn on motor if not already on
	call	cwsek		;seek if needed
	jc	cwid6
	je	cwid1		;already have this track
 mov dl,'R'
 mov ah,2
 int 21h
	 call	cwread		;read track
cwid1:	mov	cx,2		;go around twice looking for header
	mov	dx,ds:cwport	;get port base
	test	byte ptr ds:fdden,-1 ;MFM?
	jz	cwid7		;no, skip
cwid2:	; IBM System/34 MFM, or some variant
	push	cx		;save outer loop count
cwid3:	call	cwmsyn		;find MFM AM sync sequence
	jc	cwid5		;EOT
	call	cwmget		;get next byte
	jc	cwid5		;EOT
	cmp	al,0FEh		;ID address mark?
	jne	cwid3		;no, keep looking
	mov	di,offset necbuf ;point at buf
	mov	cx,6		;length of C, H, R, N, 2*CRC
cwid4:	push	cx		;save
	call	cwmget		;get next byte
	pop	cx		;[restore]
	jc	cwid5		;EOT
	mov	[di],al		;save (don't trash ES)
	inc	di
	loop	cwid4		;fetch header
	mov	si,offset necbuf ;point at buf
	mov	cl,4		;length of C, H, R, N (CH=0 from above)
	push	dx		;save I/O port
	mov	dx,0B230h	;init CRC remainder, including 3*A1, FE
	call	docrc		;check CRC
	pop	dx		;restore
	xchg	al,ah		;><
	cmp	ax,[si]		;correct CRC?  (CF=0 if so)
	jne	cwid3		;no
	pop	cx		;[yes, flush loop count]
	mov	si,offset necbuf ;[point at buf]
	ret
cwid5:	; ran off EOT, rewind unless it's time to give up
	ioport	1,0		;base+1
	in	al,dx		;clear addr ctr
	ioport	0,1		;base+0
	in	al,dx		;skip first byte
	pop	cx		;restore try counter
	loop	cwid2		;around for more if needed
	mov	al,2		;error=header not found
	stc			;error return
cwid6:	ret
cwid7:	; IBM 3740 FM, or some variant
	push	cx		;save outer loop count
cwid8:	call	cwfmrk		;find FM address mark
	jc	cwid10		;EOT
	cmp	bx,0F57Eh	;header AM (FE/C7)?
	jne	cwid8		;no, keep looking
	mov	di,offset necbuf ;point at buf
	mov	cx,6		;length of C, H, R, N, 2*CRC
cwid9:	push	cx		;save
	call	cwfget		;get next byte
	pop	cx		;[restore]
	jc	cwid10		;EOT
	mov	[di],al		;save (don't trash ES)
	inc	di
	loop	cwid9		;fetch header
 .186
 pusha
 mov dl,ds:necbuf+2
 add dl,'a'-1
 mov ah,02h
 int 21h
 popa
 .8086
	mov	si,offset necbuf ;point at buf
	mov	cl,4		;length of C, H, R, N (CH=0 from above)
	push	dx		;save I/O port
	mov	dx,0EF21h	;init CRC remainder, including FE
	call	docrc		;check CRC
	pop	dx		;restore
	xchg	al,ah		;><
	cmp	ax,[si]		;correct CRC?  (CF=0 if so)
 .186
 je xyz123
 pusha
 mov dl,'?'
 mov ah,02h
 int 21h
 popa
 int 3
 jmp short cwid8
xyz123:
 .8086

	jne	cwid8		;no
	pop	cx		;[yes, flush]
	mov	si,offset necbuf ;[point at buf]
	ret
cwid10:	; ran off EOT, rewind unless it's time to give up
	ioport	1,0		;base+1
	in	al,dx		;clear addr ctr
	ioport	0,1		;base+0
	in	al,dx		;skip first byte
	pop	cx		;restore try counter
	loop	cwid7		;around for more if needed
	mov	al,2		;error=header not found
	stc			;error return
	ret
;+
;
; Seek to selected track and head.
;
; Returns CF=1 on seek error, otherwise ZF=1 if we were already there.
;
;-
cwsek:	call	cwsel		;select drive
	mov	bl,ds:fddrv	;get drive #
	xor	bh,bh		;BH=0
	cmp	byte ptr ds:cwccl[bx],-1 ;position unknown?
	jne	cwsek2		;no, continue
	push	bx		;save
	call	cwrec		;recalibrate drive
	pop	bx		;[restore]
	jnc	cwsek1		;success
	mov	al,1		;[seek error]
	ret			;CF=1 (seek error)
cwsek1:	test	byte ptr ds:fdcyl,-1 ;done recal, is that what we wanted?
	jz	cwsek5		;yes, but we *weren't* already there, need
				;to select head and read track
cwsek2:	mov	cl,ds:cwccl[bx]	;fetch current cyl
	mov	al,ds:fdcyl	;get desired cyl
	sub	cl,al		;find # to step out
	jz	cwsek6		;already there, no op
	push	ax		;[save new cyl]
	push	bx		;[drive #]
	mov	al,11b		;[assume stepping out]
	jns	cwsek3		;correct, skip
	neg	cl		;take abs val
	mov	al,01b		;stepping in
cwsek3:	mov	ah,ds:cwlat	;get current latch value
	and	ah,not 2	;clear DIR bit
	or	al,ah		;combine
	ioport	ds:cwport	;get base port
	ioport	2,0		;ctl port
	out	dx,al		;set correct DIR
	xor	ch,ch		;clear LH
cwsek4:	; do next step
	dec	ax		;clear b0 (STEP)
	out	dx,al		;assert it
	call	sdel0		;delay a couple of usec
	call	sdelay
	call	sdelay
	call	sdelay
	inc	ax		;set b0 again (STEP)
	out	dx,al		;deassert it
	; delay 6 msec
	push	ax		;save
	mov	ax,6		;delay 6 msec
	call	waitms
	pop	ax		;restore
	loop	cwsek4		;do next step
	pop	bx		;restore ptr
	pop	ax		;final track
	mov	ds:cwccl[bx],al	;save
cwsek5:	; select head
	mov	al,ds:fdhd	;get head #
	add	al,7		;b2 = .NOT. b0
	mov	ah,ds:cwlat	;get latch value
	and	ax,0FB04h	;clear b2 in LH, isolate b2 in RH
	or	al,ah		;compose
	mov	ds:cwlat,al	;save
	ioport	ds:cwport	;get base port
	ioport	2,0		;ctl port
	out	dx,al		;set correct HDSEL
	or	al,1		;CF=0, ZF=0
	ret			;(need to read new data)
cwsek6:	; already on correct cyl, see if we're changing heads
	mov	al,ds:fdhd	;get head #
	add	al,7		;b2 = .NOT. b0
	xor	al,ds:cwlat	;XOR with curr value
	test	al,4		;same as before?
	jnz	cwsek5		;no
	xor	al,al		;CF=0, ZF=1
	ret
;+
;
; Recalibrate.
;
;-
cwrec:	mov	bl,ds:fddrv	;get drive #
	xor	bh,bh		;BH=0
	mov	al,ds:cwlat	;get current latch value
	or	al,11b		;stepping out
	ioport	ds:cwport	;get base port
	ioport	2,0		;ctl port
	out	dx,al		;set correct DIR
	mov	cx,255d		;up to 255 steps
	push	bx		;save
cwrec1:	; see if we're there
	push	ax		;save
	in	al,dx		;read status
	test	al,20		;TK00?
	pop	ax		;[restore]
	jz	cwrec2		;yes, we're there
	; do next step
	dec	ax		;clear b0 (STEP)
	out	dx,al		;assert it
	call	sdel0		;delay a couple of usec
	call	sdelay
	call	sdelay
	call	sdelay
	inc	ax		;set b0 again (STEP)
	out	dx,al		;deassert it
	; delay 6 msec
	push	ax		;save
	mov	ax,6		;delay 6 msec
	call	waitms
	pop	ax		;restore
	loop	cwrec1		;do next step
	pop	bx		;restore
	mov	byte ptr ds:cwccl[bx],-1 ;still unknown
	stc			;error
	ret
cwrec2:	pop	bx		;restore
	mov	byte ptr ds:cwccl[bx],0 ;known to be track 0
	clc			;success
	ret
;+
;
; Turn on drive motor.
;
;-
cwmon:
if cwhack
	test	byte ptr ds:cwnord,-1 ;no reading?
	jnz	cwmon3
endif ; CWHACK
	mov	cl,ds:fddrv	;get drive #
	dec	cl		;A: = -1, B: = 0
	and	cl,4		;4 if A:, 0 if B:
	mov	al,200		;b7 set (MO1)
	shr	al,cl		;change to b3 (MO0) if A:
	test	ds:cwlat,al	;already clear?
	jz	cwmon3		;yes, motor is already on
	xor	al,ds:cwlat	;clear appropriate bit
	mov	ds:cwlat,al	;(update)
	ioport	ds:cwport	;get base port
	ioport	2,0		;ctl port
	out	dx,al		;turn on motor
	; motor turn-on delay
	push	es		;save
	xor	cx,cx		;load 0
	mov	es,cx		;point at BIOS memory
	mov	cx,9d+1		;wait about 1/2 sec
cwmon1:	mov	si,es:timer_low	;get time
cwmon2:	cmp	si,es:timer_low	;wait for next edge
	je	cwmon2		;spin until change
	loop	cwmon1		;do next tick
	pop	es		;restore
	mov	byte ptr ds:cwmot,1 ;at least one motor is on
cwmon3:	ret
;+
;
; Turn off drive motor(s).
;
;-
cwoff:
if 1
	mov	al,ds:cwlat	;get latch
	or	al,210		;disable both motors
else
	mov	cl,ds:fddrv	;get drive #
	dec	cl		;A: = -1, B: = 0
	and	cl,4		;4 if A:, 0 if B:
	mov	al,200		;b7 set (MO1)
	shr	al,cl		;change to b3 (MO0) if A:
	or	al,ds:cwlat	;set appropriate bit
endif
	mov	ds:cwlat,al	;(update)
	ioport	ds:cwport	;get base port
	ioport	2,0		;ctl port
	out	dx,al		;turn off motor(s)
if 0
	and	al,210		;isolate
	cmp	al,210		;both clear?
	jne	cwoff1
endif
	 mov	byte ptr ds:cwmot,0 ;no motors on
cwoff1:	ret
;+
;
; Select drive.
;
;-
cwsel:	mov	cl,ds:fddrv	;get drive #
	mov	ah,not 40	;b5 clear (DS0)
	ror	ah,cl		;change to b4 (DS1) if B:
	mov	al,ds:cwlat	;fetch current bits
	or	al,60		;set both /DS0 and /DS1
	and	al,ah		;clear appropriate bit
	mov	ds:cwlat,al	;(update)
	ioport	ds:cwport	;get base port
	ioport	2,0		;ctl port
	out	dx,al		;select drive
	ret
;+
;
; Deselect drive.
;
;-
cwdes:
if 1
	mov	al,ds:cwlat	;get value
	or	al,60		;set both /DS0 and /DS1
else
	mov	cl,ds:fddrv	;get drive #
	mov	al,40		;b5 set (DS0)
	shr	al,cl		;change to b4 (DS1) if B:
	or	al,ds:cwlat	;set appropriate bit
endif
	mov	ds:cwlat,al	;(update)
	ioport	ds:cwport	;get base port
	ioport	2,0		;ctl port
	out	dx,al		;deselect drive
	ret
;+
;
; Read a track.
;
;-
cwread:
if 0
	add	dh,7		;b2 = .NOT. b0
	and	dh,4		;isolate
	mov	al,ds:cwlat	;get control bits
	and	al,not 4	;clear b2 (HDSEL)
	or	al,dh		;OR in head select
endif
	ioport	ds:cwport	;get base port
if cwhack
	test	byte ptr ds:cwnord,-1 ;should we read?
	jnz	cwrd1		;no
endif ; CWHACK
	ioport	2,0		;ctl port
if 0
	out	dx,al		;select head
endif
	in	al,dx		;read disk status lines
	test	al,40		;disk change?
;	jz	...		;yes
	ioport	1,2		;base+1
	in	al,dx		;clear addr ctr
	ioport	3,1		;base+3
	xor	al,al		;high-speed clock
	out	dx,al
	ioport	1,3		;base+1
	in	al,dx		;clear addr ctr
	ioport	0,1		;base+0
	in	al,dx		;skip first 2 bits of config???
	in	al,dx		;(couldn't combine with clock setting?)
	ioport	3,0		;base+3
	xor	al,al		;no index pulse in data stream
	out	dx,al
	ioport	1,3		;base+1
	in	al,dx		;clear addr ctr
	ioport	7,1		;base+7
	in	al,dx		;start read
	; wait for at least one rotation time (166-200 msec) plus a sector
	mov	ax,220d		;delay 200 msec +10%
 mov ax,250d ;;; Linux driver uses 250
	call	waitms
;;; should be 167 msec +10% if 360 RPM
	ioport	1,7		;base+1
	xor	al,al		;write 0
	out	dx,al		;stop I/O, don't clear addr ctr
	ioport	0,1		;base+0
	mov	al,-1		;end-of-data marker
	out	dx,al
	out	dx,al		;why twice?
if cwhack
cwrd1:
endif
	ioport	1,0		;base+1
	in	al,dx		;clear addr ctr
	ioport	0,1		;base+0
	in	al,dx		;skip 1st byte (no known previous transition)
;	ioport	2,0		;base+2
;	mov	al,ds:cwlat	;restore control bits
;	out	dx,al		;deselect head
	ret
;+
;
; Search for next IBM MFM sync sequence (3* A1 w/0A clock = 4489h).
;
; dx	base I/O port (preserved)
;
; CF=1 if ran off end of track.
;
;-
cwmsyn:	xor	si,si		;init buffers
	xor	di,di
	xor	bx,bx		;(most recent)
cwms1:	in	al,dx		;fetch byte
	cmp	al,-1		;end marker?
	je	cwms3
	; AL is number of Catweasel clocks (oversampled) since last reversal
	add	bx,bx		;1 zero at least
	rcl	di,1
	rcl	si,1
	add	bx,bx
	rcl	di,1
	rcl	si,1
	cmp	al,ds:cwtlo	;below low threshhold?
	jb	cwms2
	add	bx,bx		;no, 2 zeros at least
	rcl	di,1
	rcl	si,1
	cmp	al,ds:cwthi	;below high threshhold?
	jb	cwms2
	add	bx,bx		;3 zeros
	rcl	di,1
	rcl	si,1
cwms2:	inc	bx		;insert the new bit
	cmp	bx,4489h	;found sync?
	jne	cwms1		;loop if not
	cmp	di,bx		;prev 2 must match too
	jne	cwms1
	cmp	si,bx
	jne	cwms1
	mov	byte ptr ds:cwleft,0 ;luckily our sequence ends in 1
	clc			;happy
	ret
cwms3:	stc			;end of track
	ret
;+
;
; Get next MFM data byte (including clock bits).
;
; dx	base I/O port (preserved)
; bx	returns word
;
; CF=1 if ran off end of track.
;
;-
cwmget:	mov	ch,16d		;# bits we want
	add	ch,ds:cwleft	;subtract # leftover bits from prev byte
	mov	bx,1		;init with the 1 from prev string
				;(will get shifted out if CWLEFT was 0)
cwmg1:	in	al,dx		;fetch byte
	cmp	al,-1		;end marker?
	je	cwmg3
	; AL is number of Catweasel clocks (oversampled) since last reversal
	mov	cl,4		;assume 4 clocks total
	cmp	al,ds:cwthi	;CF=1 if below high threshhold
	sbb	cl,0		;-1 if so
	cmp	al,ds:cwtlo	;CF=1 if below low threshhold too
	sbb	cl,0		;-1 if so
	sub	ch,cl		;will this complete the word?
	jbe	cwmg2		;yes
	shl	bx,cl		;make space for new bit
	inc	bx		;insert it
	jmp	short cwmg1
cwmg2:	sbb	al,al		;-1 if passed 0, 0 if hit it exactly
	mov	ds:cwleft,ch	;save -# to do for next time (may be 0)
	inc	al		;1 if exact fit, 0 if not
	add	cl,ch		;find # left to left-justify (may be 0)
	shl	bx,cl		;make space
	or	bl,al		;insert the 1 if we got that far, CF=0
;;; squish out clocks
 mov cl,8d
cwmg201:
 shr bx,1
 rcr al,1
 shr bx,1
 dec cl
 jnz cwmg201
 clc
	ret
cwmg3:	stc			;end of track
	ret
;+
;
; Search for next IBM FM mark char.
;
; dx	base I/O port (preserved)
; bx	returns mark char found (see below)
;
; CF=1 if ran off end of track.
;
;-
cwfmrk:	xor	bx,bx		;init buffer
cwfm1:	in	al,dx		;fetch byte
	cmp	al,-1		;end marker?
	je	cwfm4
	; AL is number of Catweasel clocks (oversampled) since last reversal
	cmp	al,ds:cwtfm	;below threshhold?  (CF=1 if so)
	sbb	ch,ch		;[save CF]
	adc	bx,bx		;shift in new bit (1, or first of 01 if CF=0)
cwfm2:	cmp	bx,0F57Eh	;header AM (FE/C7)?
	je	cwfm3
	cmp	bx,0F56Fh	;FM data mark (FB/C7)?
	je	cwfm3
	cmp	bx,0F56Ah	;FM deleted data mark (F8/C7)?
	je	cwfm3
	cmp	bx,0F57Dh	;RX02 MMFM data mark (FD/C7)
	je	cwfm3
	cmp	bx,0F53Dh	;RX02 MMFM deleted data mark (F9/C7)
	je	cwfm3
	xor	ch,-1		;flip flag, was CF set by CMP above?
	jz	cwfm1		;yes, so we already shifted in the one bit
	add	bx,bx		;shift left
	inc	bx		;add new 1
	jmp	short cwfm2	;see if *now* we have a match
cwfm3:	; found something
	not	ch		;flip flag (-1 if bit owed)
	mov	ds:cwleft,ch	;save
	clc			;happy
	ret
cwfm4:	stc			;end of track
	ret
;+
;
; Get next FM data byte (including clock bits).
;
; dx	base I/O port (preserved)
; bx	returns word
;
; CF=1 if ran off end of track.
;
;-
cwfget:	mov	ch,16d		;# bits we want
	add	ch,ds:cwleft	;subtract # leftover bits from prev byte
	mov	bx,1		;init with the 1 from prev string
				;(will get shifted out if CWLEFT was 0)
cwfg1:	in	al,dx		;fetch byte
	cmp	al,-1		;end marker?
	je	cwfg3
	; AL is number of Catweasel clocks (oversampled) since last reversal
	cmp	al,ds:cwtfm	;below threshhold?  (CF=1 if so)
	sbb	cl,cl		;[save CF]
	adc	bx,bx		;shift in new bit (1, or first of 01 if CF=0)
	dec	ch		;got all?
	jz	cwfg2		;yes
	xor	cl,-1		;flip CL (NZ if owe another)
	jz	cwfg1		;get next bit(s)
	add	bx,bx		;shift over one more bit
	inc	bx		;it's a one
	dec	ch		;got all?
	jnz	cwfg1		;loop if not
cwfg2:	not	cl		;flip flag (-1 if bit owed)
	mov	ds:cwleft,cl	;save
 mov cl,8d
cwfg201:
 shr bx,1
 rcr al,1
 shr bx,1
 dec cl
 jnz cwfg201
 clc
	ret
cwfg3:	stc			;end of track
	ret


if 0
--------------


if 0
	ioport	ds:cwport	;get port addr
	ioport	1,0		;base+1
	in	al,dx		;clear address ctr
	; 16 bits of version flags are cycled out one bit at a time in b7 of
	; base+3, indexed by the 4 LSBs of the (hidden) address counters
	; since they repeat every 16 bits, that's how you detect the card
	ioport	0,1		;base+0
	mov	cx,16d*2	;length of two copies of version flags
@@1:	ioport	3,0		;base+3
	in	al,dx		;read shift reg
	add	al,al		;catch MSB
	rcl	di,1		;into SI:DI
	rcl	si,1
; (order is opposite of CATBASE.EXE)
	ioport	0,3		;base+0
	in	al,dx		;advance addr counter???
	loop	@@1
	cmp	si,di		;halves match?
	jne	@@2		;no
	inc	si		;-1/0 becomes 0/1
	test	si,not 1	;not all zeros or ones?
	jnz	@@3
@@2:	int	20h
@@3:	; write test pattern
	ioport	1,0		;base+1
	in	al,dx		;clear addr ctr
	ioport	0,1		;data port
	mov	si,offset tests	;point at test string
	mov	di,ltests	;length of it
	mov	cx,di		;copy
	rep	outsb
	; read a bit
	ioport	1,0		;base+1
	in	al,dx		;clear addr ctr
	ioport	3,1		;base+3
	in	al,dx		;read a bit, drop it
	ioport	0,3		;data port
	in	al,dx		;read byte, drop
	in	al,dx		;read one more, drop
 rept 10d
 in al,dx
 endm
 int 20h
endif ;;;

if 0 ;;;
ECX = -(1 meg + 100.)
loop:
	ECX+1; if 0, then no card
	read bit (see 32x loop above)
	read base+00, compare to next byte of HAUSTEIN96 signature
;	if matches, bump to next byte and loop unless matched all

add bias to ECX to make it 128K on success (memory size?!)

loop 4x: read base+00, get bit

EBX = A000  -- version stuff???
b11 = something (BP+0B) (= 0)
b13:b14:0 = something else (BP+09) (= 100)
b11:b12 = something else (BP+0A), 0 means 4 (= 01)
endif ;;;

--------------
endif


	subttl	ASPI SCSI I/O
;+
;
; Parse SCSI parameters.
;
; ss:bp	log dev rec
; si	pts at 'SCSIht_l:' string
; bx	length of " "
;
;-
gscsi:	mov	ax,ds:aspi	;does ASPI driver exist?
	or	ax,ds:aspi+2
	jnz	gscs1		;yes
	cram	'?No ASPI driver',error
gscs1:	xor	dl,dl		;init host #
	xor	cl,cl		;init target #
	xor	ch,ch		;init LUN #
	add	si,4		;skip 'SCSI'
	sub	bx,4+1		;count off 'SCSI' and ':'
	jz	gscs4
	lodsb			;get letter or whatever
	cmp	al,'A'		;letter?
	jb	gscs2
	cmp	al,'Z'
	ja	gscs2
	sub	al,'A'		;yes, convert to host index
	mov	dl,al		;save
	dec	bx		;is that it?
	jz	gscs4		;yes
	lodsb			;get next char
gscs2:	cmp	al,'0'		;0-7?
	jb	gscs3
	cmp	al,'7'
	ja	gscs3
;;; N.B. can go higher with wide SCSI!
	sub	al,'0'		;yes, convert to target #
	mov	cl,al		;save
	dec	bx		;is that it?
	jz	gscs4		;yes
	lodsb			;get next char
gscs3:	cmp	al,'_'		;_?
	jne	gscs5		;no, error
	dec	bx		;is that it?
	jz	gscs4		;yes
	lodsb			;get next char
	cmp	al,'0'		;0-7?
	jb	gscs5
	cmp	al,'7'
	ja	gscs5
	sub	al,'0'		;yes, convert to target #
	mov	ch,al		;save
	dec	bx		;should have been last char
	jnz	gscs5		;no
gscs4:	mov	ss:host[bp],dl	;save
	mov	ss:drive[bp],cl
	mov	ss:lun[bp],ch
	mov	ss:hwtype[bp],hwaspi ;set type
	mov	al,byte ptr ds:tdrv+1 ;see if any set
	or	al,byte ptr ds:tbpart+1
	or	al,byte ptr ds:ttype+1
	jnz	gscs6		;switch conflict if so
	; determine device type
	mov	si,offset dvtyp	;cmd
	mov	cx,ldvtyp	;length
	call	aspcmd		;send it
	jc	gscs7
	mov	al,ds:srb+0Ah	;get device type
	and	al,1Fh		;trim off peripheral qualifier
	; 00h disk
	; 01h tape
	; 04h WORM disk
	; 05h CD-ROM
	; 07h optical disk
	; 08h jukebox
	cmp	al,01h		;tape?
	jne	gscs10		;no, assume it's 1 of the above kinds of disks
	mov	ss:tapflg[bp],al ;set "tape" flag (AL=01h)

 cram '?Tapes not supported',error ;;; not yet supported

;;; see if variable or fixed record size

	ret
gscs5:	jmp	synerr
gscs6:	jmp	swtcon
gscs7:	cmp	al,82h		;error=no such drive?
	je	gscs9
gscs8:	call	serr		;print msg
	jmp	mloop		;give up
gscs9:	jmp	baddrv		;no such drive
gscs10:	; get total disk size and sector size
	mov	ds:rdcap1,ds	;set seg addr of BUF
	mov	cx,5		;retry count
gscs11:	push	cx		;save
	mov	si,offset rdcap	;pt at cmd to read capacity into BUF
	mov	cx,lrdcap
	call	aspcmd		;send cmd
	pop	cx		;[restore]
	jnc	gscs12		;success
	loop	gscs11		;keep trying
	cram	'?Error sizing disk',error
gscs12:	mov	dx,word ptr ds:buf+4 ;get sector length in bytes
	mov	ax,word ptr ds:buf+6
	xchg	dl,dh		;reverse order (result in DX:AX)
	xchg	al,ah
	mov	ss:secsz[bp],ax	;save
	mov	ss:secsz+2[bp],dx
	mov	cx,word ptr ds:buf+0 ;get highest sector # on disk
	mov	bx,word ptr ds:buf+2
	xchg	cl,ch		;reverse order (result in CX:BX)
	xchg	bl,bh
	call	mul32		;multiply to get byte # of last sector
	add	ax,ss:secsz[bp]	;add final sector to get dev size
	adc	bx,ss:secsz+2[bp]
	adc	cx,0
	adc	dx,0
	sbb	si,si		;all ones if overflowed (!!!), otherwise 0
	or	ax,si		;change to max 64-bit int if wrapped
	or	bx,si
	or	cx,si
	or	dx,si
	mov	ss:totsiz[bp],ax ;save
	mov	ss:totsiz+2[bp],bx
	mov	ss:totsiz+4[bp],cx
	mov	ss:totsiz+6[bp],dx
	mov	si,512d		;DEC block size
	xor	di,di
	call	div32		;find total # blocks in dev
	mov	ss:totblk[bp],ax ;save total block count
	mov	ss:totblk+2[bp],bx
	mov	ss:totblk+4[bp],cx
	mov	ss:totblk+6[bp],dx
	mov	ss:devsiz[bp],ax ;save usable block count
	mov	ss:devsiz+2[bp],bx
	mov	ss:devsiz+4[bp],cx
	mov	ss:devsiz+6[bp],dx
	mov	ss:dsrini[bp],offset cret ;no DSR init
	mov	ss:rdblk[bp],offset rdadk ;read from ASPI SCSI disk
	mov	ss:wrblk[bp],offset wradk ;write to ASPI SCSI disk
	ret
;+
;
; Read from ASPI SCSI disk.
;
; On entry:
; bp	dev record
; dx:ax	block number to read
; cx	number of blocks to read
; es:di	ptr to buf
;
; On return, CF=0 and:
; es:si	ptr to data read
; cx	count of bytes read
;
; Or CF=1 on read error (or cluster out of range).
;
;-
rdadk:	mov	bx,ss:devsiz+4[bp] ;if >4 G blocks, guaranteed to fit
	or	bx,ss:devsiz+6[bp]
	jnz	radk2
	mov	bx,ax		;copy starting block #
	mov	si,dx
	add	bx,cx		;add count
	adc	si,0		;carry
	jc	radk1		;whoops
	cmp	si,ss:devsiz+2[bp] ;off end of disk?  (exact hit is OK)
	ja	radk1		;whoops
	jb	radk2		;OK
	cmp	bx,ss:devsiz[bp] ;check low word
	jbe	radk2		;OK
radk1:	stc			;error
	ret
radk2:	; convert starting block # to starting sector # + offset
	push	di		;save ptr
	push	cx		;save block count
	push	dx		;save starting addr
	push	ax
	call	radk13		;convert blk (DX:AX) to sector/offset
	mov	ds:secnum,ax	;save starting sector #
	mov	ds:secnum+2,bx
	mov	ds:secoff,si	;save offset into first sector
	mov	ds:secoff+2,di
	pop	ax		;restore starting addr
	pop	dx
	pop	bx		;get blk count
	add	ax,bx		;find blk after final blk
	adc	dx,0
	; find byte count corresponding to block count
	mov	cl,9d		;bit count
	shl	bx,cl		;left 9 bits (known <64 KB due to FCOPY rules)
	mov	ds:reqcnt,bx	;save
	call	radk13		;convert ending blk loc to sector/offset
	or	si,di		;any of this sector required?
	jz	radk3
	add	ax,1		;yes, include it
	adc	dx,0
	sbb	si,si		;-1 if overflowed, 0 if not
	or	ax,si		;max out at all ones
	or	dx,si
radk3:	sub	ax,ds:secnum	;find # physical sectors to transfer
	sbb	dx,ds:secnum+2
	jz	radk4		;fits in 16 bits
	 mov	ax,65535d	;take the max that does fit
radk4:	; make sure that fits in buffer space alotted (reduce sec cnt if not)
	test	ax,ax		;sector count down to 0?
	jz	radk9		;yes, can't fit even a single phys sector
	push	ax		;save
	xor	dx,dx		;zero-extend sector count
	mov	bx,ss:secsz[bp]	;get sector size yet again
	mov	cx,ss:secsz+2[bp]
	call	mul32		;DX:CX:BX:AX=byte count
	or	dx,cx		;must be <= 64 KB
	or	dx,bx
	pop	cx		;[restore phys sector count]
	jnz	radk5		;nope, definitely need to subtract a sector
	cmp	ax,ds:reqcnt	;will it fit?
	jbe	radk10		;fits, go
	cmp	cx,1		;was that our last chance?
	je	radk6		;yes, have to use SECBUF for one sector
radk5:	dec	cx		;subtract one
	mov	ax,cx		;count in AX for MUL32
	jmp	short radk4	;try again
radk6:	; even one sector won't fit in alotted space, use SECBUF
	cmp	ax,ds:secsiz	;space for one sector here?
	ja	radk9		;no, I give up
	push	es		;save
	mov	es,ds:secbuf	;point at sector buffer
	xor	di,di		;offset=0
	call	radk14		;read the sector
	pop	es		;[restore buf ptr]
	pop	di
	jc	radk8		;error
	mov	si,ds:secoff	;get offset into sector
	sub	cx,si		;don't count first part of sector
	cmp	cx,ds:reqcnt	;did we want that much?
	jb	radk7
	 mov	cx,ds:reqcnt	;stop short
radk7:	push	cx		;save
	push	ds
	push	di
	mov	ds,ds:secbuf	;point at buf
	rep	movsb
	pop	si		;restore
	pop	ds
	pop	cx
	clc			;happy
radk8:	ret
radk9:	; didn't have space for even one sector
	pop	si		;restore buf ptr
	xor	cx,cx		;nothing read
	ret
radk10:	; ready to go, CX=phys sector count, AX=corresponding byte count
	pop	di		;restore buf ptr
;;;	mov	ds:reqcnt,ax	;shrink requested size to phys sector multiple
	call	radk14		;read disk data
	jc	radk12		;error
	mov	si,di		;copy
	mov	ax,ds:secoff	;get offset into first sector
	test	ax,ax		;anything?
	jz	radk11		;no
	sub	cx,ax		;remove wasted part
	push	si		;save starting addr
	push	cx		;and actual count
	add	si,ax		;index out to data we really want
	push	ds		;save
	push	es		;copy ES to DS
	pop	ds
	shr	cx,1		;/2 (CF=odd byte)
	rep	movsw		;[copy words]
	rcl	cl,1		;=1 if odd
	rep	movsb		;copy odd byte if any
	pop	ds		;restore
	pop	cx		;get back count used
	pop	si		;and starting addr
radk11:	clc			;happy
radk12:	ret
;
radk13:	; convert block # (DX:AX) to sector (BX:AX) and offset (DI:SI)
	mov	cl,9d		;bit count
	rol	ax,cl		;left 9 bits
	rol	dx,cl
	mov	bx,ax		;copy low order
	and	bh,777/400	;isolate 9 bits shifted out of AX
	xor	ax,bx		;isolate high 7 in AX
	mov	cx,dx		;copy high order
	and	ch,777/400	;isolate 9 bits shifted out of DX
	xor	dx,cx		;isolate high 7 in DX
	or	bx,dx		;compose middle word
	xor	dx,dx		;clear highest order
	; DX:CX:BX:AX = starting byte address
	mov	si,ss:secsz[bp]	;get sector size
	mov	di,ss:secsz+2[bp]
	call	div32		;find starting disk addr
	ret
;
radk14:	; AX=byte count, CX=sector count, DS:SECNUM=starting sector, ES:DI=addr
	; returns ES:DI preserved, CX=byte count
	; decide whether to do short (6-byte) or long (10-byte) request
	; (theoretically could use 10-byte version all the time but this way we
	; have some chance of working with ancient disks that only do READ(6))
	test	ch,ch		;sector count too big for 8 bits?
	jnz	radk15		;yes
	test	word ptr ds:secnum+2,177740 ;sec # too big for 21 bits?
	jnz	radk15		;yes
	; use READ(6)
	mov	byte ptr ds:ioshtc,08h ;cmd=READ(6)
	mov	ds:ioshtl,ax	;set byte count
	mov	word ptr ds:ioshtl+2,0
	mov	ds:ioshta,di	;set addr
	mov	ds:ioshta+2,es
	mov	ds:ioshtc+4,cl	;set sector count
	mov	dx,ds:secnum	;get sector #
	xchg	dl,dh		;(byte swapped)
	mov	word ptr ds:ioshtc+2,dx ;save low 16 bits
	mov	dl,byte ptr ds:secnum+2 ;get high 5 bits
	mov	ds:ioshtc+1,dl
	mov	si,offset iosht	;pt at cmd
	mov	cx,liosht	;length
	jmp	short radk16
radk15:	; use READ(10)
	mov	byte ptr ds:iolngc,28h ;cmd=READ(10)
	mov	ds:iolngl,ax	;set byte count
	mov	word ptr ds:iolngl+2,0
	mov	ds:iolnga,di	;set addr
	mov	ds:iolnga+2,es
	xchg	cl,ch		;(byte swap sector count)
	mov	word ptr ds:iolngc+7,cx ;save
	mov	cx,ds:secnum	;get sector #
	mov	dx,ds:secnum+2
	xchg	cl,ch		;byte swap
	xchg	dl,dh
	mov	word ptr ds:iolngc+2,dx ;save
	mov	word ptr ds:iolngc+4,cx
	mov	si,offset iolng	;pt at cmd
	mov	cx,liolng	;length
radk16:	push	es		;save
	push	di
	push	ax		;save byte count
	push	ds		;copy DS to ES
	pop	es
	call	aspcmd		;send command
	pop	cx		;[restore byte count]
	pop	di		;[restore ptr]
	pop	es
	ret
;+
;
; Write to ASPI SCSI disk.
;
; On entry:
; bp	dev record
; ax:dx	block number to write
; cx	number of blocks to write
; es:si	ptr to buf
;
; On return, CF=0 on success.
;
;-
wradk:	mov	bx,ss:devsiz+4[bp] ;if >4G blocks, guaranteed to fit
	or	bx,ss:devsiz+6[bp]
	jnz	wadk2
	mov	bx,ax		;copy starting block #
	mov	di,dx
	add	bx,cx		;add count
	adc	di,0		;carry
	jc	wadk1		;whoops
	cmp	di,ss:devsiz+2[bp] ;off end of disk?  (exact hit is OK)
	ja	wadk1		;whoops
	jb	wadk2		;OK
	cmp	bx,ss:devsiz[bp] ;check low word
	jbe	wadk2		;OK
wadk1:	stc			;error
	ret
wadk2:	; convert starting block # to starting sector # + offset
	push	si		;save ptr
	push	cx		;save block count
	mov	cl,9d		;bit count
	rol	ax,cl		;left 9 bits
	rol	dx,cl
	mov	bx,ax		;copy low order
	and	bh,1		;isolate 9 bits shifted out of AX
	xor	ax,bx		;isolate high 7 in AX
	mov	cx,dx		;copy high order
	and	ch,1		;isolate 9 bits shifted out of DX
	xor	dx,cx		;isolate high 7 in DX
	or	bx,dx		;compose middle word
	xor	dx,dx		;clear highest order
	; DX:CX:BX:AX = starting byte address
	mov	si,ss:secsz[bp]	;get sector size
	mov	di,ss:secsz+2[bp]
	call	div32		;find starting disk addr
	mov	ds:secnum,ax	;save starting sector #
	mov	ds:secnum+2,bx
	mov	ds:secoff,si	;save offset into first sector
	mov	ds:secoff+2,di
	; convert block count to sector count and byte count
	pop	ax		;get # blocks
	mov	cl,9d		;bit count
	rol	ax,cl		;left 9 bits
	mov	bx,ax		;copy
	and	bh,1		;isolate 9 bits shifted out of AX
	xor	ax,bx		;isolate high 7 in AX (so BX:AX=byte count)
	; (if SECOFF is non-zero, stop and mess with just the one sector)
	mov	cx,ds:secoff	;get low word
	or	cx,ds:secoff+2	;either non-zero?
;;	jnz	...		;yes, read/modify/write
	xor	cx,cx		;clear high order
	xor	dx,dx
	mov	si,ss:secsz[bp]	;get sector size
	mov	di,ss:secsz+2[bp]
	call	div32		;DX:CX:BX:AX = sector count (DX:CX known 0)
	test	bx,bx		;BX must be 0 too, to fit in cmd
	jz	wadk3
	 mov	ax,0FFFFh	;try to do a zillion of the tiny sectors
wadk3:	push	ax		;save
	xor	dx,dx		;zero-extend
	mov	bx,ss:secsz[bp]	;get sector size yet again
	mov	cx,ss:secsz+2[bp]
	call	mul32		;BX:AX=byte count (BX known 0 because of FCOPY)
	pop	cx		;restore sector count
	pop	si		;restore starting offset
	; BX:AX=byte count, CX=sector count, DS:SECNUM=starting sector, DI=offs
	; decide whether to do short (6-byte) or long (10-byte) request
	; (theoretically could use 10-byte version all the time but this way
	; we have some chance of working with ancient disks that only do 6)
	test	ch,ch		;sector count too big for 8 bits?
	jnz	wadk4		;yes
	test	word ptr ds:secnum+2,177740 ;sec # too big for 21 bits?
	jnz	wadk4		;yes
	; use WRITE(6)
	mov	byte ptr ds:ioshtc,0Ah ;cmd=WRITE(6)
	mov	ds:ioshtl,ax	;set byte count
	mov	ds:ioshtl+2,bx
	mov	ds:ioshta,si	;set addr
	mov	ds:ioshta+2,es
	mov	ds:ioshtc+4,cl	;set sector count
	mov	dx,ds:secnum	;get sector #
	xchg	dl,dh		;(byte swapped)
	mov	word ptr ds:ioshtc+2,dx ;save low 16 bits
	mov	dl,byte ptr ds:secnum+2 ;get high 5 bits
	mov	ds:ioshtc+1,dl
	mov	si,offset iosht	;pt at cmd
	mov	cx,liosht	;length
	jmp	short wadk5
wadk4:	; use WRITE(10)
	mov	byte ptr ds:iolngc,2Ah ;cmd=WRITE(10)
	mov	ds:iolngl,ax	;set byte count
	mov	ds:iolngl+2,bx
	mov	ds:iolnga,si	;set addr
	mov	ds:iolnga+2,es
	xchg	cl,ch		;(byte swap sector count)
	mov	word ptr ds:iolngc+7,cx ;save
	mov	cx,ds:secnum	;get sector #
	mov	dx,ds:secnum+2
	xchg	cl,ch		;byte swap
	xchg	dl,dh
	mov	word ptr ds:iolngc+2,dx ;save
	mov	word ptr ds:iolngc+4,cx
	mov	si,offset iolng	;pt at cmd
	mov	cx,liolng	;length
wadk5:	push	es		;save
	push	si
	push	ax		;save byte count
	push	ds		;copy DS to ES
	pop	es
	call	aspcmd		;send command
	pop	cx		;[restore byte count]
	pop	si		;[restore ptr]
	pop	es
	jc	wadk6		;error
	ret
wadk6:	stc			;error
	ret
;+
;
; Execute ASPI cmd, CF=1 on error.
;
; ds:si	ptr to cmd
; cx	length
; ss:bp	dev rec (preserved)
;
;-
aspcmd:	mov	di,offset srb	;ES:DI=SRB buf
	mov	bx,di		;copy ptr
	rep	movsb		;copy cmd
	; set up parms that are always the same
	mov	al,ss:host[bp]	;set host adapter #
	mov	[bx+2],al
	mov	al,ss:drive[bp]	;set SCSI target ID
	mov	[bx+08h],al
	mov	al,ss:lun[bp]	;set LUN (within that SCSI target)
	mov	[bx+09h],al	;almost always 0 (except Adaptec ACB-4000 etc.)
	mov	cl,5		;shift count
	sal	al,cl
	and	byte ptr [bx+41h],37 ;mask out old LUN
	or	[bx+41h],al	;OR it in
if 0
;;;	set buffer addr
;;	mov	[bx+0Fh], offset
;;	mov	[bx+11h], segment
endif
	push	bp		;save
	push	es		;set up call args
	push	bx
	call	dword ptr ds:aspi ;call driver
	pop	ax		;flush stack
	pop	ax
	pop	bp		;restore
	;callr	<short aspwt>	;poll for completion or ^C
;
aspwt:	; wait for ASPI completion, CF=1 on error
	mov	al,ds:srb+1	;get status
	test	al,al		;done (or error)?
	jnz	aspwt1		;got it
	mov	ah,0Bh		;func=test for input
	int	21h		;(actually just looking for ^C)
	jmp	short aspwt	;survived the call, loop
aspwt1:	cmp	al,1		;happy?
	je	aspwt2		;yes, CF=0 from CMP
	 stc			;no
aspwt2:	ret
;+
;
; Sort out ASPI errors and print message.
;
; SRB	set up from ASPI call.
;
;-
asperr:	mov	dl,ds:srb+18h	;get HA status
	mov	si,offset hasts	;pt at table
	test	dl,dl		;error?
	jnz	aerr1		;yes, get msg
	or	dl,ds:srb+19h	;get target status (DL=0)
	jz	aerr2		;=0, why are we here?
	mov	si,offset tgtsts ;pt at table
	cmp	dl,2		;error=check condition?
	jne	aerr1		;no
	mov	dl,ds:srb+46h+2	;get sense key
	and	dl,0Fh		;trim to size
	mov	si,offset snskey ;pt at table
aerr1:	call	aspmsg		;look up msg
	mov	bx,0001h	;handle=STDOUT
	mov	ah,40h		;func=write
	int	21h
	mov	dx,offset crlfs	;pt at <CRLF>
	mov	cx,2		;length=2
	mov	ah,40h		;func=write (handle still set)
	int	21h
aerr2:	ret
;
hasts	label	byte
	amsg	11h,'?Sel timeout'
	amsg	12h,'?Data over/underrun'
	amsg	13h,'?Unexpected bus free'
	amsg	14h,'?Bus protocol error'
	dw	-1
;
tgtsts	label	byte
	amsg	08h,'?Device busy'
	amsg	18h,'?Reservation conflict'
	dw	-1
;
snskey	label	byte
	amsg	00h,'?No sense'
	amsg	01h,'?Soft error'
	amsg	02h,'?Not ready'
	amsg	03h,'?Medium error'
	amsg	04h,'?Hardware error'
	amsg	05h,'?Illegal request'
	amsg	06h,'?Unit attention'
	amsg	07h,'?Write protect error'
	amsg	08h,'?Blank check'
	amsg	09h,'?Data underflow'
	amsg	0Bh,'?Aborted command'
	amsg	0Dh,'?Volume overflow'
	amsg	0Eh,'?Miscompare'
	dw	-1
;+
;
; Look up error message.
;
; dl	byte to look up
; si	pointer to MSG table terminated by "DW -1"
;
; On return, DS:DX=ptr to msg (or IOERR if not found), CX=length
;
;-
aspmsg:	lodsw			;get word
	test	ah,ah		;end of table?
	js	amsg2		;yes
	cmp	al,dl		;match?
	lodsb			;[get length]
	cbw			;[0-extend]
	je	amsg1		;yes
	add	si,ax		;skip text
	jmp	short aspmsg	;loop
amsg1:	mov	dx,si		;copy ptr
	mov	cx,ax		;copy length
	ret
amsg2:	mov	dx,offset ioerr	;pt at default msg
	mov	cx,10d		;LEN('?I/O error')
	ret
;
ioerr	db	'?I/O error'
;+
;
; Check AL for error.
;
;-
serr:	cmp	al,1		;error?
	jne	serr1
	ret
serr1:	callr	asperr
;
	subttl	CD-ROM I/O (or "I" at least)
;+
;
; Set up for CD-ROM input (using MSCDEX.EXE or NWCDEX.EXE).
;
; ss:bp	logical dev record
; al	drive number
;
;-
gcdrom:	mov	ss:drive[bp],al	;save unit #
	push	es		;save
	mov	ax,352Fh	;func=get INT 2Fh vector
	int	21h
	mov	ax,es		;address is in AX:BX
	pop	es		;restore
	or	ax,bx		;is vector 0000:0000?  (possible in DOS V2.x)
	jz	gcdex1		;yes, don't call it
	mov	cl,ss:drive[bp]	;get unit #
	xor	ch,ch		;CH=0
	xor	bx,bx		;in case not installed
	mov	ax,150Bh	;func=MSCDEX V2.0+ drive check
	int	2Fh
	cmp	bx,0ADADh	;MSCDEX installed at all?
	jne	gcdex1		;no, or older version
	test	ax,ax		;unit # OK?
	jz	gcdex1		;no
	mov	ss:dsrini[bp],offset cret ;nothing to init
	mov	ss:rdblk[bp],offset rdcd ;routine to read from CD
	mov	ss:wrblk[bp],offset wrperr ;(always write-protected)
	.386			;(just to allow 32-bit math in constants)
	mov	word ptr ss:devsiz[bp],(cdsize/512d) and 0FFFFh ;set # blocks
	mov	word ptr ss:devsiz+2[bp],cdsize/512d/10000h
	mov	word ptr ss:totblk[bp],(cdsize/512d) and 0FFFFh ;total is same
	mov	word ptr ss:totblk+2[bp],cdsize/512d/10000h
	mov	word ptr ss:totsiz[bp],cdsize and 0FFFFh ;total in bytes
	mov	word ptr ss:totsiz+2[bp],cdsize/10000h
	.8086
	mov	ss:hwtype[bp],hwcdex ;set type
	ret
gcdex1:	cram	"?Can't connect to CD-ROM",error
;+
;
; Read block(s) from a CD.
;
; dx:ax	starting block #
; cx	number of blocks to read
; es:di	buffer
; ss:bp	log dev rec
;
; On return, CF=0 and:
; es:si	ptr to data read
; cx	count of bytes read
;
;-
rdcd:	; check range
	mov	bx,ax		;copy starting block #
	mov	si,dx
	add	bx,cx		;add count
	adc	si,0		;carry
	jc	rdcd4		;whoops
	cmp	si,ss:devsiz+2[bp] ;off end of disk?  (exact hit is OK)
	ja	rdcd4		;whoops
	jb	rdcd1		;OK
	cmp	bx,ss:devsiz[bp] ;check low word
	ja	rdcd4
rdcd1:	; there may be up to three parts of the transfer -- partial first
	; sector, contiguous whole sectors, partial last sector
	push	di		;save starting position
rdcd2:	push	dx		;save
	push	ax
	push	cx
	call	rdcdp		;do partial transfer
	pop	cx		;[restore]
	pop	ax
	pop	dx
	jc	rdcd3
	add	ax,bx		;update starting block #
	adc	dx,0
	sub	cx,bx		;anything left to do?
	jnz	rdcd2		;yes, keep looking
	pop	si		;no, catch starting buf addr
	mov	cx,di		;get ending addr
	sub	cx,si		;find # bytes read, CF=0
	ret
rdcd3:	pop	si		;flush stack
rdcd4:	stc			;error return
	ret
;+
;
; Do partial CD transfer.
;
; dx:ax	starting block #
; cx	# blocks needed
; es:di	buffer addr
;
; On return, CF=1 on error, otherwise:
; es:di	updated
; bx	# blocks actually read
;
;-
rdcdp:	; convert starting block # to sector #
	mov	bl,al		;save LSBs of starting block #
	shr	dx,1		;right 2 bits (4 blocks per 2048-byte sector)
	rcr	ax,1
	shr	dx,1
	rcr	ax,1
	and	bx,3		;isolate block offset within sector
	jnz	rdcdp2		;not on sector boundary, do partial sec first
	cmp	cx,4		;at least a whole sector?
	jb	rdcdp4		;no
	; read whole sectors now, postpone partial final sector (if any)
	push	cx		;save block count
	shr	cx,1		;get sec cnt
	shr	cx,1
	mov	bx,di		;ES:BX=buf
	mov	di,ax		;SI:DI=sector #
	mov	si,dx
	mov	dx,cx		;DX=sector count
	mov	cl,ss:drive[bp]	;get unit #
	xor	ch,ch
	push	bx		;(save)
	mov	ax,1508h	;func=absolute disk read
	int	2Fh
	pop	di		;[restore buf addr]
	pop	bx		;[restore block count]
	jc	rdcdp1		;read error
	and	bl,not 3	;we only did a multiple of 4
	mov	ax,bx		;copy
	mov	cl,9d		;shift count
	shl	ax,cl		;convert block count to byte count
	add	di,ax		;advance ptr
	clc			;happy
	ret
rdcdp1:	stc			;error return
	ret
rdcdp2:	add	cx,bx		;add starting sec offset
	cmp	cx,4		;>=1 sector?
	jb	rdcdp3
	 mov	cx,4		;yes, stop short
rdcdp3:	sub	cx,bx		;find # secs
rdcdp4:	; starts with partial sector, do just that now (rest on next call)
	; DX:AX=sec #, BX=block offset within sec, CX=block count
	test	word ptr ds:secbuf,-1 ;did we get a sector buf at startup?
	jz	rdcdp1		;no, insufficient memory
	cmp	word ptr ds:secsiz,2048d ;big enough for one sector?
	jb	rdcdp1		;same deal
	; SECBUF is big enough, read sector into it
	push	es		;save
	push	di
	push	bx
	push	cx
	mov	es,ds:secbuf	;ES:BX=buf
	xor	bx,bx
	mov	di,ax		;SI:DI=sector #
	mov	si,dx
	mov	dx,1		;DX=sector count
	mov	cl,ss:drive[bp]	;get unit #
	xor	ch,ch
	mov	ax,1508h	;func=absolute disk read
	int	2Fh
	pop	bx		;[restore]
	pop	si
	pop	di
	pop	es
	jc	rdcdp1		;read error
	; extract the block(s) we want
	mov	cl,9d		;shift count
	shl	si,cl		;find starting offset in sector buffer
	mov	dx,bx		;copy block count
	dec	cx		;we want word count
	shl	dx,cl		;find # words to copy
	mov	cx,dx		;get byte count
	push	ds		;save
	mov	ds,ds:secbuf	;point at buffer
	rep	movsw
	pop	ds		;restore
	clc			;happy
	ret
;
	subttl	TU58 I/O
;+
;
; Init TU58 controller.
;
; ss:bp	log dev rec
;
;-
ddinit:	mov	ds:ddtry,3	;retry count
ddini1:	; send break
	mov	dx,ss:port[bp]	;get port
	add	dx,3		;line control reg
	mov	al,103		;BREAK=1, 8 data, 1 stop, no parity
	out	dx,al
	mov	cx,2		;wait for 2 clock edges
	push	ds		;save DS
	xor	ax,ax		;pt at BIOS data
	mov	ds,ax		;with DS
ddini2:	mov	ax,ds:timer_low	;get time
ddini3:	cmp	ax,ds:timer_low	;has it changed?
	je	ddini3		;loop if not
	loop	ddini2		;count it if so
	; wait one more tick so UART has settled down
	mov	ax,ds:timer_low	;get new value
ddini4:	cmp	ax,ds:timer_low	;has it changed?
	je	ddini4		;loop until it does
	pop	ds		;restore
	mov	al,3		;BREAK=0, 8 data, 1 stop, no parity
	out	dx,al
	call	ddflsh		;flush input buf
	; send two INIT commands
	push	es		;save ES
	push	cs		;copy CS to ES
	pop	es
	mov	si,offset initdd ;pt at string
	mov	cx,2		;two bytes long
	call	sdd		;send to TU58
	mov	di,ds:dbuf
	inc	cx		;CX=1
	mov	ds:ddtime,9d	;allow 1/2 second
	call	rdd		;receive the cmd
	pop	es		;[restore]
	jc	ddini7		;timeout, try again
	mov	al,[di-1]	;get the byte
	and	al,37		;isolate low 5 bits
	cmp	al,20		;CONTINUE?
	je	ddini8
ddini7:	dec	ds:ddtry	;retry
	jnz	ddini1
	cram	'?TU58 init failure',error
ddini8:	ret
;
initdd	db	4,4		;INIT, INIT (first is ignored)
;+
;
; Flush COM input buffer.
;
;-
ddflsh:	mov	dx,ss:port[bp]	;get base port addr
ddfls1:	add	dx,5		;pt at status reg
	in	al,dx		;read it
	test	al,1		;input char ready?
	jz	ddfls2		;no, do the init
	sub	dx,5		;yes, pt at data port
	in	al,dx		;eat the char
	jmp	short ddfls1
ddfls2:	ret
;+
;
; Read block(s) from a TU58.
;
; dx:ax	starting block #
; cx	number of blocks to read
; es:di	buffer
; ss:bp	log dev rec
;
;-
rddd:	add	ax,cx		;add blk count
	adc	dx,0		;carry
	jnz	rddd1		;shouldn't be any
	cmp	ax,512d		;in range?  (exact hit on 512. OK)
	ja	rddd1		;no
	mov	ds:ddblk,ax	;save blk # after end
	mov	ds:ddptr,di	;and ptr to begn of block
	mov	ds:ddptr+2,es
	sal	cl,1		;*512=byte count
	xchg	cl,ch
	mov	ds:ddctr,cx	;save byte count
	push	di		;save ptr
	push	cx		;and byte count
	mov	ds:dderr,0	;invalid remaining byte count
	jmp	short rddd2
rddd1:	stc			;error return
	ret
rddd2:	mov	ds:ddtry,3	;3 tries per starting block #
rddd3:	; request all of the remaining blocks
	push	ds		;copy DS to ES
	pop	es
	mov	si,ds:dbuf	;pt at buffer
	mov	di,si		;copy
	mov	ax,0A02h	;flag=02 (cmd), length=10.
	stosw
	cbw			;cmd=02 (read), modifier=0
	stosw
	mov	al,ss:drive[bp]	;unit #, switches=0
	stosw
	xor	al,al		;sequence #
	stosw
	mov	ax,ds:ddctr	;get # bytes to go
	stosw
	mov	bl,ah		;get high byte
	shr	bl,1		;make block count
	xor	bh,bh
	mov	ax,ds:ddblk	;ending block #
	sub	ax,bx		;find starting block # of this xfr
	stosw
	mov	cx,12d		;byte count
	xor	ah,ah		;even byte
	xor	bx,bx		;init checksum
	call	sdd		;send to DD:
	mov	es:[si],bx	;save checksum
	mov	cl,2		;byte count
	call	sdd		;send
rddd4:	; wait for a data packet (ES=DS)
	mov	di,ds:dbuf	;pt at buffer
	mov	cx,1		;read one byte
	xor	ah,ah		;it's an even one
	xor	bx,bx		;init checksum
	mov	ds:ddtime,38d*91d/5 ;wait up to 38 seconds (worst seek is 28)
	call	rdd		;get the mark character
	jc	rddd8
	mov	al,[di-1]	;get the char read
	and	al,37		;trim to 5 bits
	cmp	al,1		;data?
	jne	rddd5		;no, go deal
	inc	cx		;yes
	mov	ds:ddtime,18d	;timeout down to 1 sec
	call	rdd		;get the length byte
	jc	rddd8
	mov	cl,[di-1]
	xor	ch,ch		;CH=0
	mov	dx,ds:ddctr	;get # bytes to go
	sub	dx,cx		;would this overrun the buffer?
	jc	rddd8		;yes
	push	cx		;save
	les	di,dword ptr ds:ddptr ;get ptr
	call	rdd		;read the data
	jc	rddd7		;timeout, flush cx and deal
	push	ds		;copy DS to ES
	pop	es
	mov	di,ds:dbuf	;pt at buffer
	mov	cl,2		;get checksum
	push	bx		;(save it)
	call	rdd
	pop	ax		;[restore]
	jc	rddd7
	cmp	ax,[di-2]	;do checksums match?
	jne	rddd7		;no
	pop	ax		;restore count
	add	ds:ddptr,ax	;update addr
	sub	ds:ddctr,ax	;and count
	jnz	rddd4
	call	ddend		;get end packet
	jc	rddd8		;whoops
	cmp	al,2		;0 or 1?  (OK endings)
	cmc			;CF=0 if so
	pop	cx		;restore
	pop	si
	mov	es,ds:ddptr+2	;get seg
	ret			;CF is already set
rddd5:	; flag is something other than data mark
	cmp	al,2		;control?
	jne	rddd8		;no, reset and retry
	call	ddend1		;presumably end packet
	jc	rddd8		;bad end packet, maybe it wasn't bad news
rddd6:	add	sp,4		;flush stack
	stc			;CF=1
	ret
rddd7:	pop	cx		;flush stack
rddd8:	; timeout, bad checksum, or protocol error -- retry
	call	ddinit		;reinit
	mov	ax,ds:ddctr	;get # bytes to go
	mov	bx,ax		;copy
	add	ax,777		;round up to next block boundary
	and	ax,not 777
	mov	ds:ddctr,ax	;update
	mov	cx,ax		;copy
	sub	ax,bx		;find # bytes already transferred in this blk
	sub	ds:ddptr,ax	;back up
	cmp	cx,ds:dderr	;same place we started at last time?
	jne	rddd9
	dec	ds:ddtry	;yes, count this try
	jz	rddd6		;the hell with this
	jmp	rddd3		;try again
rddd9:	mov	ds:dderr,cx	;remember where we are
	jmp	rddd2		;give it another try
;+
;
; Write block(s) to a TU58.
;
; dx:ax	starting block #
; cx	number of blocks to write
; es:si	buffer
; ss:bp	log dev rec
;
;-
wrdd:	add	ax,cx		;add blk count
	adc	dx,0		;carry
	jnz	wrdd1		;shouldn't be any
	cmp	ax,512d		;in range?  (exact hit on 512. OK)
	ja	wrdd1		;no
	mov	ds:ddblk,ax	;save blk # after end
	mov	ds:ddptr,si	;and ptr to begn of block
	mov	ds:ddptr+2,es
	sal	cl,1		;*512=byte count
	xchg	cl,ch
	mov	ds:ddctr,cx	;save byte count
	mov	ds:dderr,0	;invalid remaining byte count
	jmp	short wrdd2
wrdd1:	stc			;error return
	ret
wrdd2:	mov	ds:ddtry,3	;3 tries per starting block #
wrdd3:	; write all of the remaining blocks
	push	ds		;copy DS to ES
	pop	es
	mov	si,ds:dbuf	;pt at buffer
	mov	di,si		;copy
	mov	ax,0A02h	;flag=02 (cmd), length=10.
	stosw
	mov	ax,3		;cmd=03 (write), modifier=0
	stosw
	mov	al,ss:drive[bp]	;unit #, switches=0
	stosw
	xor	al,al		;sequence #
	stosw
	mov	ax,ds:ddctr	;get # bytes to go
	stosw
	mov	bl,ah		;get high byte
	shr	bl,1		;make block count
	xor	bh,bh
	mov	ax,ds:ddblk	;ending block #
	sub	ax,bx		;find starting block # of this xfr
	stosw
	mov	cx,12d		;byte count
	xor	ah,ah		;even byte
	xor	bx,bx		;init checksum
	call	sdd		;send to DD:
	mov	es:[si],bx	;save checksum
	mov	cl,2		;byte count
	call	sdd		;send
	mov	ax,ds:ddptr	;get curr offset
	mov	ds:ddsav,ax	;save
wrdd4:	; wait for a CONTINUE flag (es=ds)
	mov	di,ds:dbuf	;pt at buffer
	mov	cx,1		;read one byte
	xor	ah,ah		;it's an even one
	xor	bx,bx		;init checksum
	mov	ds:ddtime,38d*91d/5 ;wait up to 38 seconds (worst seek is 28)
	call	rdd		;get the mark character
	jc	wrdd7
	mov	al,[di-1]	;get the char read
	and	al,37		;trim to 5 bits
	cmp	al,20		;CONTINUE?
	jne	wrdd5		;no, go deal
	; write next data packet
	mov	si,offset dddata ;pt at header
	xor	bx,bx		;init checksum
	xor	ah,ah		;even byte
	mov	cl,2		;length=2
	call	sdd		;send first two bytes
	les	si,dword ptr ds:ddptr ;get ptr
	mov	cl,128d		;length
	call	sdd		;send data portion
	push	ds		;copy DS to ES
	pop	es
	mov	si,ds:dbuf	;pt at buffer
	mov	[si],bx		;save checksum
	mov	cl,2		;write 2 bytes
	call	sdd
	add	ds:ddptr,128d	;update ptr
	sub	ds:ddctr,128d	;done yet?
	jnz	wrdd4
	call	ddend		;get end packet
	jc	wrdd7		;whoops
	cmp	al,-17d		;data check error?
	je	wrdd7		;yes (the only recoverable one)
	cmp	al,2		;0 or 1?  (OK endings)
	cmc			;CF=0 if so
	mov	es,ds:ddptr+2	;get seg
	ret			;CF is already set
wrdd5:	; flag is something other than CONTINUE
	cmp	al,2		;control?
	jne	wrdd7		;no, reset and retry
	call	ddend1		;presumably end packet
	jc	wrdd7		;bad end packet, maybe it wasn't bad news
	cmp	al,-17d		;data check error?
	je	wrdd7		;yes, retry
wrdd6:	stc			;CF=1
	ret
wrdd7:	; timeout, error, or protocol error -- retry
	call	ddinit		;reinit
	mov	ax,ds:ddptr	;get curr ptr
	cmp	ax,ds:ddsav	;same as starting posn?
	mov	ax,ds:ddctr	;[get # bytes to go]
	mov	bx,ax		;[copy]
	je	$+5		;yes, there's no prev block to resend
	 add	ax,128d		;include prev data packet
	add	ax,777		;round to prev blk boundary
	and	ax,not 777
	mov	ds:ddctr,ax	;update
	mov	cx,ax		;save
	sub	ax,bx		;find # bytes already transferred in this blk
	sub	ds:ddptr,ax	;back up
	cmp	cx,ds:dderr	;same place we started at last time?
	jne	wrdd8
	dec	ds:ddtry	;yes, count this try
	jz	wrdd6		;the hell with this
	jmp	wrdd3		;try again
wrdd8:	mov	ds:dderr,cx	;remember where we are
	jmp	wrdd2		;give it another try
;
dddata	db	1,128d		;data packet header
;+
;
; Get end packet, return CF=1 if timeout (or not end).
; Otherwise return success code in al.
;
;-
ddend:	mov	ds:ddtime,9d	;1/2 sec timeout
	push	ds		;copy DS to ES
	pop	es
	mov	di,ds:dbuf	;pt at data buf
	mov	cx,1		;count
	xor	bx,bx		;init checksum
	xor	ah,ah		;even byte
	call	rdd		;get command byte
	jc	ddend2
	and	al,37		;trim to 5 bits
	cmp	al,2		;control?
	jne	ddend2		;no
ddend1:	; enter here if above was already done
	mov	ds:ddtime,9d	;1/2 sec timeout (again)
	inc	cx		;get length byte
	call	rdd
	jc	ddend2		;timeout
	mov	cl,[di-1]	;get length
	cmp	cl,10d		;must be 10.
	jne	ddend2
	call	rdd		;get the rest of the packet
	jc	ddend2
	push	bx		;save checksum
	mov	cl,2		;get checksum
	call	rdd
	pop	ax		;restore
	jc	ddend2
	cmp	ax,[di-2]	;does it match?
	jne	ddend2
	mov	al,[di-11d]	;yes, get success code
	cmp	al,-8d		;bad unit #?
	je	ddend3
	cmp	al,-9d		;missing cartridge?
	je	ddend4
	cmp	al,-11d		;write protected?
	je	ddend5
	cmp	al,-32d		;seek error?
	je	ddend6
	clc
	ret
ddend2:	stc			;error return
	ret
ddend3:	jmp	baddrv
ddend4:	jmp	notrdy
ddend5:	jmp	wrperr
ddend6:	jmp	sekerr
;+
;
; Send a string to a TU58.
;
; ah	low bit=0 if starting byte is even (updated on return)
; bx	one's complement checksum (updated one byte at a time depending on AH)
; es:si	string
; cx	length
; ss:bp	log dev rec
;
;-
sdd:	mov	dx,ss:port[bp]	;get port
sdd1:	add	dx,5
sdd2:	in	al,dx		;read port
	test	al,40		;can we write a byte?
	jz	sdd2		;spin if not
	sub	dx,5		;pt at data port
	lods	byte ptr es:[si] ;get a byte
	out	dx,al
	test	ah,1		;even or odd?
	jnz	sdd3
	add	bl,al		;even, add to low byte
	adc	bh,0
	jmp	short sdd4
sdd3:	add	bh,al		;odd, add to high byte
sdd4:	adc	bx,0		;end-around
	inc	ah		;flip low bit
	loop	sdd1		;around for next
	ret
;+
;
; Receive a string from a TU58.
;
; ah	low bit=0 if starting byte is even (updated on return)
; bx	one's complement checksum (updated one byte at a time depending on AH)
; es:di	buffer addr (updated on return)
; cx	# bytes to receive
; ss:bp	log dev rec
; CS:DDTIME  # clock ticks to wait before timing out.
;
; CF=1 on timeout.
;
;-
rdd:	push	ds		;save
	push	bp
	xor	si,si		;point at BIOS data
	mov	ds,si
	mov	dx,ss:port[bp]	;get port
rdd1:	add	dx,5
	mov	bp,cs:ddtime	;get timeout in clock ticks
rdd2:	mov	si,ds:timer_low	;read timer
rdd3:	in	al,dx		;read port
	test	al,1		;is a byte ready?
	jz	rdd6		;skip if not
	sub	dx,5		;pt at data port
	in	al,dx		;get the byte
	stosb			;save it
	test	ah,1		;even or odd?
	jnz	rdd4
	add	bl,al		;even, add to low byte
	adc	bh,0
	jmp	short rdd5
rdd4:	add	bh,al		;odd, add to high byte
rdd5:	adc	bx,0		;end-around
	inc	ah		;flip low bit
	loop	rdd1		;around for next
	pop	bp		;restore
	pop	ds
	clc			;no error
	ret
rdd6:	; see if it's time to time out
	cmp	si,ds:timer_low	;has timer changed?
	je	rdd3		;spin if not
	dec	bp		;timed out yet?
	jnz	rdd2
	pop	bp
	pop	ds
	stc			;timeout
	ret
;
	subttl	pure data
;
crlfs	db	cr,lf
r50	db	' ABCDEFGHIJKLMNOPQRSTUVWXYZ$.?0123456789:' ;radix 50
months	db	'XXXJanFebMarAprMayJunJulAugSepOctNovDecXXXXXXXXX'
inifil	db	'PUTR.INI',0	;name of init file
dotcmd	db	'.CMD',0	;default extension for "@"
dotdsk	db	'.DSK',0	;default extension for disk image files
scsimg	db	'SCSIMGR$',0	;name of ASPI handler
badprm	db	'invalid drive'	;use this prompt after errors
;
sgnrt	db	'DECRT11A    '	;signature in RT-11 disks, blk 1, offset 760
sgnvex	db	'DECVMSEXCHNG'	;signature in VMS EXCHANGE RT-11 disks, " " " "
				;(thanks to Tim Shoppa <shoppa@triumf.ca>)
sgnods	db	'DECFILE11A  '	;signature in ODS-1 disks, blk 1, offset 760
satsys	db	'SATT.SYS',0	;name for RSTS [0,1]SATT.SYS
;
; Sector interleave tables:
ilv11	db	01d,02d,03d,04d,05d,06d,07d,08d,09d,10d ;1:1 table
	db	11d,12d,13d,14d,15d,16d,17d,18d,19d,20d
	db	21d,22d,23d,24d,25d,26d,27d,28d,29d,30d
	db	31d,32d,33d,34d,35d,36d
;
; Table to translate floppy drive types from AT CMOS memory to our numbers
;
fdcmos	db	0	;0 no drive
	db	rx31	;1 360 KB 300 RPM
	db	rx33	;2 1.2 MB 360 RPM (may be 300 RPM too, FDC's problem)
	db	rx24	;3 720 KB 3.5" 300 RPM
	db	rx23	;4 1.44 MB 300 RPM
	db	rx26	;5 2.88 MB 300 RPM
	db	10d dup(0) ;6-F unknown
;
; disk parameter table for RX50 floppies in 1.2MB drive (INT 1Eh vector)
;
if 0 ;; not used (we don't use BIOS any more, except for its ISR)
dparm	db	0DFh,02h	;specify bytes
	db	25h		;motor turn-on time
	db	2,10d		;512 b/s, 10 sec/tk
	db	20d		;gap length
	db	-1		;max transfer
	db	24d		;gap length (format)
	db	0E5h		;fill byte (format)
	db	15d		;head settle (ms)
	db	8d		;motor start time (1/8 sec)
endif ; 0
;
dosvec	label	word		;file I/O vectors for DOS
_=iovecs
	disp	defset,dosds
	disp	savcwd,dossav
	disp	rstcwd,dosrst
	disp	volnam,dosvn
	disp	gdir,dosgd
	disp	gfile,dirfil
	disp	pfile,pspec
	disp	dirhdr,dosdh
	disp	dirinp,dosdi
	disp	dirget,dosdg
	disp	dirdis,dosdd
	disp	diremp,dosdd	;subdirectories are reported as empty blocks
	disp	dirnam,dosdn
	disp	dirfin,cret
	disp	dirsum,dossum
	disp	open,dosop
	disp	read,dosrd
	disp	reset,dosrs
	disp	dirout,cret
	disp	create,doscr
	disp	write,doswr
	disp	wlast,cret
	disp	close,doscl
	disp	delfil,dosdl
	disp	chdir,doscd
	disp	wboot,noboot
	disp	wipout,nowipe
;
dbvecs	label	word		;file I/O vectors for DOS/BATCH
_=iovecs
	disp	defset,dbds
	disp	savcwd,dbsav
	disp	rstcwd,dbrst
	disp	volnam,nolbl
	disp	gdir,dbgd
	disp	gfile,dbgf
	disp	pfile,dbpf
	disp	dirhdr,dbdh
	disp	dirinp,dbdi
	disp	dirget,dbdg
	disp	dirdis,dbdd
	disp	diremp,cret
	disp	dirnam,dbdn
	disp	dirfin,dbdf
	disp	dirsum,dbsum
	disp	open,dbop
	disp	read,dbrd
	disp	reset,cret
	disp	dirout,dbdo
	disp	create,dbcr
	disp	write,dbwr
	disp	wlast,dbwl
	disp	close,dbcl
	disp	delfil,dbdl
	disp	chdir,dbcd
	disp	wboot,noboot
	disp	wipout,nowipe
;
frvecs	label	word		;file I/O vectors for FOREIGN volumes
_=iovecs
	disp	defset,cret
	disp	savcwd,cret
	disp	rstcwd,cret
	disp	volnam,nolbl
	disp	gdir,cret
	disp	gfile,dirfil
	disp	pfile,cret
	disp	dirhdr,ilfunc
	disp	dirinp,cret
	disp	dirget,ilfunc
	disp	dirdis,ilfunc
	disp	diremp,ilfunc
	disp	dirnam,cret
	disp	dirfin,cret
	disp	dirsum,ilfunc
	disp	open,ilfunc
	disp	read,ilfunc
	disp	reset,ilfunc
	disp	dirout,cret
	disp	create,ilfunc
	disp	write,ilfunc
	disp	wlast,ilfunc
	disp	close,ilfunc
	disp	delfil,ilfunc
	disp	chdir,ilfunc
	disp	wboot,ilfunc
	disp	wipout,ilfunc
;
odvecs	label	word		;file I/O vectors for Files-11 ODS-1
_=iovecs
	disp	defset,odds
	disp	savcwd,odsav
	disp	rstcwd,odrst
	disp	volnam,odvn
	disp	gdir,odgd
	disp	gfile,dirfil
	disp	pfile,pspec
	disp	dirhdr,cret
	disp	dirinp,oddi
	disp	dirget,oddg
	disp	dirdis,oddd
	disp	diremp,odde
	disp	dirnam,oddn
	disp	dirfin,rsdf
	disp	dirsum,odsum
	disp	open,odop
	disp	read,odrd
	disp	reset,cret
	disp	dirout,rsdo
	disp	create,ilfunc
	disp	write,ilfunc
	disp	wlast,ilfunc
	disp	close,ilfunc
	disp	delfil,ilfunc
	disp	chdir,odcd
	disp	wboot,noboot
	disp	wipout,nowipe
;
osvecs	label	word		;file I/O vectors for OS/8
_=iovecs
	disp	defset,osds
	disp	savcwd,ossav
	disp	rstcwd,osrst
	disp	volnam,nolbl
	disp	gdir,cret
	disp	gfile,dirfil
	disp	pfile,pspec
	disp	dirhdr,osdh
	disp	dirinp,osdi
	disp	dirget,osdg
	disp	dirdis,osdd
	disp	diremp,osde
	disp	dirnam,cret
	disp	dirfin,osdf
	disp	dirsum,ossum
	disp	open,osop
	disp	read,osrd
	disp	reset,cret
	disp	dirout,osdo
	disp	create,oscr
	disp	write,oswr
	disp	wlast,oswl
	disp	close,oscl
	disp	delfil,osdl
	disp	chdir,cret ;;; actually should be error
	disp	wboot,noboot ;;;
	disp	wipout,rtwp
;
rsvecs	label	word		;file I/O vectors for RSTS/E
_=iovecs
	disp	defset,rsds
	disp	savcwd,rssav
	disp	rstcwd,rsrst
	disp	volnam,rsvn
	disp	gdir,rsgd
	disp	gfile,rsgf
	disp	pfile,pspec
	disp	dirhdr,rsdh
	disp	dirinp,rsdi
	disp	dirget,rsdg
	disp	dirdis,rsdd
	disp	diremp,cret
	disp	dirnam,rsdn
	disp	dirfin,rsdf
	disp	dirsum,rssum
	disp	open,rsop
	disp	read,rsrd
	disp	reset,cret
	disp	dirout,rsdo
	disp	create,rscr
	disp	write,rswr
	disp	wlast,rswl
	disp	close,rscl
	disp	delfil,rsdl
	disp	chdir,rscd
	disp	wboot,noboot ;;;;;
	disp	wipout,nowipe ;;;;
;
rtvecs	label	word		;file I/O vectors for RT-11
_=iovecs
	disp	defset,rtds
	disp	savcwd,rtsav
	disp	rstcwd,rtrst
	disp	volnam,rtvn
	disp	gdir,rtgd
	disp	gfile,dirfil
	disp	pfile,pspec
	disp	dirhdr,rtdh
	disp	dirinp,rtdi
	disp	dirget,rtdg
	disp	dirdis,rtdd
	disp	diremp,rtde
	disp	dirnam,rtdn
	disp	dirfin,rtdf
	disp	dirsum,rtsum
	disp	open,rtop
	disp	read,rtrd
	disp	reset,cret
	disp	dirout,rtdo
	disp	create,rtcr
	disp	write,rtwr
	disp	wlast,rtwl
	disp	close,rtcl
	disp	delfil,rtdl
	disp	chdir,rtcd
	disp	wboot,rtboot
	disp	wipout,rtwp
;
tsvecs	label	word		;file I/O vectors for TSS/8.24
_=iovecs
	disp	defset,tsds
	disp	savcwd,tssav
	disp	rstcwd,tsrst
	disp	volnam,nolbl
	disp	gdir,tsgd
	disp	gfile,tsgf
	disp	pfile,pspec
	disp	dirhdr,tsdh
	disp	dirinp,tsdi
	disp	dirget,tsdg
	disp	dirdis,tsdd
	disp	diremp,tsde
	disp	dirnam,tsdn
	disp	dirfin,tsdf
	disp	dirsum,tssum
	disp	open,tsop
	disp	read,tsrd
	disp	reset,cret
	disp	dirout,ilfunc
	disp	create,ilfunc
	disp	write,ilfunc
	disp	wlast,ilfunc
	disp	close,ilfunc
	disp	delfil,ilfunc
	disp	chdir,tscd
	disp	wboot,noboot
	disp	wipout,ilfunc
;
xxvecs	label	word		;file I/O vectors for new XXDP+
_=iovecs
	disp	defset,dbds
	disp	savcwd,dbsav
	disp	rstcwd,dbrst
	disp	volnam,nolbl
	disp	gdir,dbgd
	disp	gfile,dbgf
	disp	pfile,dbpf
	disp	dirhdr,dbdh
	disp	dirinp,dbdi
	disp	dirget,dbdg
	disp	dirdis,dbdd
	disp	diremp,cret
	disp	dirnam,cret
	disp	dirfin,dbdf
	disp	dirsum,dbsum
	disp	open,dbop
	disp	read,dbrd
	disp	reset,cret
	disp	dirout,dbdo
	disp	create,dbcr
	disp	write,dbwr
	disp	wlast,dbwl
	disp	close,dbcl
	disp	delfil,dbdl
	disp	chdir,dbcd
	disp	wboot,noboot ;;;;;
	disp	wipout,nowipe ;;;;
;
; ASPI cmd to get dev type
dvtyp	db	1		;cmd=1 (get device type)
	db	0		;(status byte)
	db	0		;HA #
	db	0		;SCSI req flags
	dd	0		;rsvd
	db	0		;target ID
	db	0		;LUN
	db	0		;type byte returned here
ldvtyp=	$-dvtyp
;
	subttl	impure data -- initialized but will change
;
fdtype	db	4 dup(0)	;drive types of floppies A-D (0 if unknown)
				;possibilities:  RX31 RX33 RX24 RX23 RX26
				;RX50 may be set by hand, for real RX50
;
dosroo	db	?,':\'		;root dir wildcard (for VOLSER)
wldcrd	db	'*.*',0		;wildcard
;
jan	db	31d
feb	db	?		;poked by DOSDAT
	db	31d,30d,31d,30d,31d,31d,30d,31d,30d,31d
;
logdev	dw	0		;ptr to linked list of logical dev records
tmpdev	dw	0		;temporarily alloced dev rec (may become perm)
frerec	dw	0		;ptr to list of freed logical dev records
fremem	dw	mem		;start of free memory
kepmem	dw	mem		;FREMEM value to keep between cmds
;
dbuf	dw	dbuf1		;ptr to begn of disk sector buffer
;
bindef	dw	text		;default BINFLG value for COPY
;
more	db	1		;NZ => MORE processing enabled
;
dsmunl	db	0		;NZ => DISMOUNT/UNLOAD is default
				;0 => DISMOUNT/NOUNLOAD is default
;
nprmpt	db	0		;NZ => no valid prompt
				;(don't loop forever on error retrieving CWD)
;
cc4fdc	db	1		;NZ => FDC is CompatiCard IV
				;(normally it's OK to write the CC4 ctrl reg
				;since it doesn't exist on generic FDCs, so the
				;default is NZ, but you can SET FDC GENERIC if
				;you've got something that doesn't like writes
				;to I/O port 3F0h
;
; Parameters for COPY/DEV/FILE/BIN.  Patchable, or I'll add SET commands if
; more than a couple of people ever care.
	db	'COPILV'	;(for searching with DEBUG or SYMDEB)
copilv	db	2		;sector-to-sector interleave, >=1
copskw	db	2,0		;track-to-track skew, with a 0 for MUL WORD PTR
;
aspi	dw	0,0		;ptr to ASPI driver entry point, or 0:0 if none
;
; ASPI host adapter inquiry command:
hainq	db	0		;cmd=0 (host adapter inquiry)
	db	0		;(status byte, overwritten)
	db	0		;HA #
	db	0		;SCSI req flags
	dw	0AA55h		;horrible hack to allow add'l length
	dw	2		;# add'l flag bytes to return
lhainq=	$-hainq
;
; ASPI cmd to get dev capacity
rdcap	db	2		;cmd=2
	db	0		;(status byte)
	db	0		;HA #
	db	14		;SCSI req flags (read, residual)
	dd	0		;rsvd
	db	0		;target ID
	db	0		;LUN
	dw	8d,0		;data allocation length (two longwords)
	db	14d		;sense allocation length
	dw	buf		;data buffer offset
rdcap1	dw	?		;data buffer segment (filled in later)
	dd	0		;SRB link ptr
	db	10d		;CDB length (long form cmd)
	db	0		;(host adapter status returned)
	db	0		;(target status returned)
	dd	0		;post routine addr (not used)
	db	22h dup(0)	;rsvd for workspace
	; SCSI command descriptor block
	db	25h		;cmd=read capacity
	db	0		;LUN, RelAdr=0
	dd	0		;starting blk # of logical unit
	db	0,0,0		;reserved, PMI=0
	db	0		;control:  1=FLAG, 0=LINK
	; sense data
;	db	14d dup(?)	;sense data
lrdcap=	$-rdcap
;
; ASPI cmd to start/stop unit
ssunt	db	2		;cmd=2
	db	0		;(status byte)
	db	0		;HA #
	db	0		;SCSI req flags
	dd	0		;rsvd
	db	0		;target ID
	db	0		;LUN
	dw	0,0		;data allocation length (none)
	db	14d		;sense allocation length
	dw	buf		;data buffer offset
ssunt1	dw	?		;data buffer segment (filled in later)
	dd	0		;SRB link ptr
	db	6d		;CDB length (short form cmd)
	db	0		;(host adapter status returned)
	db	0		;(target status returned)
	dd	0		;post routine addr (not used)
	db	22h dup(0)	;rsvd for workspace
	; SCSI command descriptor block
	db	1Bh		;cmd=start/stop unit
	db	0		;LUN, Immed=0
	db	0
	db	0
	db	2		;LoEj=1, Start=0
	db	0		;control:  1=FLAG, 0=LINK
	; sense data
;	db	14d dup(?)	;sense data
lssunt=	$-ssunt
;
if 1
;
; This might be an ASPI cmd to write prot/enable a Zip disk.
; But who knows!
wpzip	db	2		;cmd=2
	db	0		;(status byte)
	db	0		;HA #
	db	0		;SCSI req flags
	dd	0		;rsvd
	db	0		;target ID
	db	0		;LUN
wpzipl	dw	0,0		;data allocation length
	db	14d		;sense allocation length
	dw	buf		;data buffer offset
wpzip1	dw	?		;data buffer segment (filled in later)
	dd	0		;SRB link ptr
	db	6d		;CDB length (short form cmd)
	db	0		;(host adapter status returned)
	db	0		;(target status returned)
	dd	0		;post routine addr (not used)
	db	22h dup(0)	;rsvd for workspace
	; SCSI command descriptor block
	db	0Ch		;cmd=vendor unique
	db	0		;LUN, Immed=0
	db	0
	db	0
wpzips	db	0		;string length
	db	0		;control:  1=FLAG, 0=LINK
	; sense data
;	db	14d dup(?)	;sense data
lwpzip=	$-wpzip
endif
;
; Short (6-byte) I/O cmd SCSI req block
iosht	db	2		;cmd=2
	db	0		;(status byte)
	db	0		;HA #
	db	0;10		;SCSI req flags (read)
	dd	0		;rsvd
	db	0		;target ID
	db	0		;LUN
ioshtl	dw	0,0		;data allocation length
	db	14d		;sense allocation length
ioshta	dw	?		;data buffer offset
	dw	?		;data buffer segment (filled in later)
	dd	0		;SRB link ptr
	db	6		;CDB length (short form cmd)
	db	0		;(host adapter status returned)
	db	0		;(target status returned)
	dd	0		;post routine addr (not used)
	db	22h dup(0)	;rsvd for workspace
	; SCSI command descriptor block
ioshtc	db	?		;cmd=read(6) (08h) or write(6) (0Ah)
	db	0		;<7:5>=LUN <4:0>=MSB of sector #
	db	0		;middle byte of sector #
	db	0		;LSB of sector #
	db	0		;# sectors
	db	0		;control:  1=FLAG, 0=LINK
	; sense data
;	db	14d dup(?)	;sense data
liosht=	$-iosht
;
; Long (10-byte) I/O cmd SCSI req block
iolng	db	2		;cmd=2
	db	0		;(status byte)
	db	0		;HA #
	db	0;10		;SCSI req flags (read)
	dd	0		;rsvd
	db	0		;target ID
	db	0		;LUN
iolngl	dw	0,0		;data allocation length
	db	14d		;sense allocation length
iolnga	dw	?		;data buffer offset
	dw	?		;data buffer segment (filled in later)
	dd	0		;SRB link ptr
	db	10d		;CDB length (long form cmd)
	db	0		;(host adapter status returned)
	db	0		;(target status returned)
	dd	0		;post routine addr (not used)
	db	22h dup(0)	;rsvd for workspace
	; SCSI command descriptor block
iolngc	db	?		;cmd=read(10) (28h) or write(10) (2Ah)
	db	0		;<7:5>=LUN, DPO=0, FUA=0, RelAdr=0
	dd	0		;sector # (MSB first)
	db	0		;reserved
	db	0		;MSB of # sectors
	db	0		;LSB of # sectors
	db	0		;control:  1=FLAG, 0=LINK
	; sense data
;	db	14d dup(?)	;sense data
liolng=	$-iolng
;
resbc	db	0		;NZ => ASPI driver supports residual byte cnts
;
; MLOOP DIRFINs these if it finds them NZ:
indev	dw	0		;input dev rec ptr
outdev	dw	0		;output dev rec ptr
.assume <outdev-indev eq 2>	;RSUMAP index dev #s using INDEV+0 or INDEV+2
;
; MLOOP closes these if it finds them >=0:
inhnd	dw	-1		;input file handle or -1 if closed
outhnd	dw	-1		;output file handle or -1 if closed
;
indhnd	dw	-1		;indirect file handle or -1 if closed
;
fdccl	db	4 dup(-1)	;current cyl for each floppy, or -1 if unknown
;
; Catweasel/ISA impure data:
;
cwport	dw	320h		;base I/O port of Catweasel/ISA board
cwccl	db	2 dup(-1)	;current cyl for each Catweasel floppy, or -1
if 1
cwtlo	db	22h		;threshhold between 2 and 3 MFM bit cells
cwthi	db	31h		;threshhold between 3 and 4 MFM bit cells
cwtfm	db	29h		;threshhold between 1 and 2 FM bit cells
else ;;; values from Linux driver
cwtlo	db	20h		;threshhold between 2 and 3 MFM bit cells
cwthi	db	30h		;threshhold between 3 and 4 MFM bit cells
endif ;;;
cwmot	db	0		;NZ => at least one motor is on
				;(turn off at next prompt)
if cwhack
cwnord	db	0		;NZ => don't actually read
endif
;
kbbuf	db	80d,?,80d dup(?) ;INT 21(0A) record
;
	subttl	pure storage
;
dosver	dw	1 dup(?)	;byte-swapped DOS version
;
; date stuff for decoding OS/8 "year" field in dir entry
year	dw	1 dup(?)	;this year
yrbase	dw	1 dup(?)	;most recent year that was 1970+mult of 8
day	db	1 dup(?)	;day within month
	db	1 dup(?)	;month (accessed with DAY as a word)
;
min	db	1 dup(?)	;minute
hour	db	1 dup(?)	;hour (MUST FOLLOW MIN)
sec	db	1 dup(?)	;second
;
curdsk	db	1 dup(?)	;drive letter if current dev is DOS disk
				;0 if not
dosdsk	db	1 dup(?)	;logged-in DOS disk (MUST FOLLOW CURDSK)
swchar	db	1 dup(?)	;SWITCHAR
;
dirflg	db	1 dup(?)	;DIRECTORY listing flags
;
; input file stuff:
ndent	dw	1 dup(?)	;# dir entries left in this seg (OS/8)
inseg	db	1 dup(?)	;input seg #
oinseg	db	1 dup(?)	;old INSEG from previously DIRGETted file
inent	dw	1 dup(?)	;ptr to next dir entry
idind	dw	2 dup(?)	;ODS-1:  LBN of curr header for dir retrieval
idret	dw	1 dup(?)	;ODS-1:  offset in blk of next dir retrieval
idlbn	dw	2 dup(?)	;ODS-1:  current LBN in directory
idcnt	dw	2 dup(?)	;ODS-1:  count of LBNs starting at IDLBN
idlen	dw	2 dup(?)	;ODS-1:  count of LBNs in entire dir
ifid	dw	3 dup(?)	;ODS-1;  file ID of input file
ifind	dw	2 dup(?)	;ODS-1:  LBN of curr header for file retrieval
ifret	dw	1 dup(?)	;ODS-1:  offset in blk of next file retrieval
iflbn	dw	2 dup(?)	;ODS-1:  current LBN in file
ifcnt	dw	2 dup(?)	;ODS-1:  count of LBNs starting at IFLBN
ifrec	dw	1 dup(?)	;ODS-1:  # bytes left in current record
ifodd	db	1 dup(?)	;ODS-1:  NZ => record length was odd
oinent	dw	2 dup(?)	;OS/8:  old INENT from prev DIRGETted file
				;RSTS:  +0=link to this file, +2=link to prev
				;ODS-1:  INENT that points to current file
pextra	dw	1 dup(?)	;ptr to most recent extra word(s)
stblk	dw	2 dup(?)	;absolute starting blk of current file
fsize	dw	2 dup(?)	;current file size (sys-dependent units)
fbcnt	dw	2 dup(?)	;current file size (in 8-bit bytes)
matchf	dw	1 dup(?)	;NZ if next match found, 0 if not (DOS)
iretr	dw	8d dup(?)	;RSTS, TSS/8 retrieval window (link + 7 DCNs)
irptr	dw	1 dup(?)	;ptr into IRETR (0, 2, 4, 6, ...)
iattr	dw	1 dup(?)	;link to first attribute blkette (RSTS)
idla	dw	1 dup(?)	;date of last access (DOS-11 form)
ifcs	dw	1 dup(?)	;input file cluster size
irts	dw	2 dup(?)	;RTS name
icont	db	1 dup(?)	;NZ => input file is contiguous
;
badsec	dw	2 dup(?)	;# bytes to reserve for bad sec file (FORMAT)
;
iblk	dw	4 dup(?)	;input file curr blk #
ioff	dw	1 dup(?)	;block offset into current input cluster
				;(for RSTS files with FCS .GE. our buf size)
isize	dw	4 dup(?)	;input file size in blocks
ieof	db	1 dup(?)	;NZ => soft EOF reached
iseg	dw	1 dup(?)	;input dir curr seg #
optr	dw	1 dup(?)	;output ptr (into LBUF)
col	db	1 dup(?)	;column # in WTTY
;
; stuff used by RTRDIR/RTWDIR and related routines:  (and OSRDIR/OSWDIR)
dirseg	db	1 dup(?)	;temporary dir seg #
dirsiz	dw	1 dup(?)	;size of directory in BIGBUF in bytes
dirent	dw	1 dup(?)	;size in bytes of each dir entry in BIGBUF
dircnt	dw	1 dup(?)	;dir entry loop counter while handling BIGBUF
inptr	dw	1 dup(?)	;BIGBUF offset corresponding to INSEG,INENT
iptr1	dw	1 dup(?)	;updated INPTR during dir modification
outptr	dw	1 dup(?)	;BIGBUF offset corresponding to OUTSEG,OUTENT
optr1	dw	1 dup(?)	;updated OUTPTR during dir modification
;
; output file stuff:
outseg	db	2 dup(?)	;output seg # (RT-11 -- byte)
				;UFD clu # in DBCL (DOS/BATCH -- word)
outent	dw	1 dup(?)	;offset into seg # OUTSEG of output dir entry
oblk	dw	2 dup(?)	;curr blk # in output dev (starts at BOF)
				;DOS/BATCH:  first, last clu # of output file
				;RSTS:  total # of blocks in file (NOT counter)
ooff	dw	1 dup(?)	;block offset into current output cluster
				;(for RSTS files with FCS .GE. our buf size)
osize	dw	2 dup(?)	;RT/OS:  # blocks left in this < UNUSED > area
				;DOS/BATCH:  # clus required by file
				; (duplicated in both words, 1st counts down)
				;RSTS:
				; first word = # FCs required for file
				; second word = # PCs required for file
owrtn	dw	1 dup(?)	;# blocks written
ofcs	dw	1 dup(?)	;output file cluster size
onew	dw	1 dup(?)	;new cluster # to be added to dir (DOS/BATCH)
				;or 0 if none
ofree	dw	1 dup(?)	;starting clu # of free area (DOS/BATCH)
onfree	dw	1 dup(?)	;# clusters in free area
ofver	dw	1 dup(?)	;output file version #, or 0 if default (ODS-1)
;
obind	dw	2 dup(?)	;ODS-1:  LBN of curr hdr for bitmap retrieval
obret	dw	1 dup(?)	;ODS-1:  offset in blk of next bitmap retrieval
oblbn	dw	2 dup(?)	;ODS-1:  LBN of curr blk in BITMAP.SYS
obcnt	dw	2 dup(?)	;ODS-1:  # contig blks starting at OBLBN
oblen	dw	2 dup(?)	;ODS-1:  total # blks left in BITMAP.SYS
;
oretr0	dw	1 dup(?)	;pointer to first retrieval list of file, or 0
opretr	dw	3 dup(?)	;BLKL, BLKH, OFFSET of prev retrieval blkette
				;(if ORETR0 is NZ)
oretr	dw	8d dup(?)	;RSTS output retrieval window (link + 7 DCNs)
orptr	dw	1 dup(?)	;ptr into ORETR (0, 2, 4, 6, ...)
;
odsiz	dw	2 dup(?)	;size by which to extend file (ODS-1)
odfno	dw	2 dup(?)	;file number in ODEXT (ODS-1)
odmax	dw	2 dup(?)	;max size of free area found in ODFF (ODS-1)
odmpcn	dw	2 dup(?)	;starting PCN of free area " " " (ODS-1)
;
rsblk	dw	2 dup(?)	;RSTS blk # (during MFD search in RDS 0.0)
;
rtfile	dw	4 dup(?)	;RT-11, RSTS, DOS/BATCH filename
				; (3 words .RAD50)
				;or ODS-1 filename (4 words .RAD50)
				;or OS/8 filename (4 words TEXT)
;
rtdnam	dw	3 dup(?)	;dir name, RADIX-50 (RT-11)
rtdsep	dw	1 dup(?)	;dir separator flag
;
delim	db	1 dup(?)	;delimiter char while parsing
dirptr	dw	1 dup(?)	;pointer into ACTDIR in ODFD
;
; Stuff saved by SAVCWD and RSTCWD:
savdir	dw	2 dup(?)	;dir base blk # (RT-11)
savods	dw	odpmax*3 dup (?) ;dir names (ODS-1)
svdirh	dw	2 dup(?)	;(MSW of base blk #)
savsiz	dw	2 dup(?)	;size of dir file (or partition if root)
				;or dir entry size (DOS/BATCH)
savwnd	dw	2*8d dup(?)	;saved dir clu, retrieval pointers (RSTS)
;
notfnd	db	1 dup(?)	;NZ until a match is find in wildcard lookup
noqry	db	1 dup(?)	;NZ => don't ask before doing something
ovride	db	1 dup(?)	;NZ => override protection
devflg	db	1 dup(?)	;b0 => copy whole dev, b7 => src or dst is file
clusiz	dw	1 dup(?)	;if NZ, contains output file cluster size
contig	db	1 dup(?)	;NZ => contiguous output file, if supported
;
; File date/time info set up by the various DIRGET routines:
;
ftimf	db	1 dup(?)	;NZ => file time is valid
fdatf	db	1 dup(?)	;NZ => file date is valid (MUST FOLLOW FTIMF)
fsec	db	1 dup(?)	;file seconds,
fmin	db	1 dup(?)	;minutes,
fhour	db	1 dup(?)	;hours (0-23.) (MUST FOLLOW FMIN)
fday	db	1 dup(?)	;file day,
fmonth	db	1 dup(?)	; month, (MUST FOLLOW FDAY)
fyear	dw	1 dup(?)	;and year (all 4 digits)
fattr	db	1 dup(?)	;file attrib (DOS, RSTS)
fprot	db	1 dup(?)	;file protection code (RSTS, TSS/8)
.assume <fprot eq fattr+1>	;RSDG stores these as a word
;
nfile	dw	1 dup(?)	;# files printed in DIR listing
nblk	dw	2 dup(?)	;# blocks (clusters) " " " "
nfree	dw	1 dup(?)	;# free blocks (clusters)
;
; filename buffers:
fname1	db	80d dup(?)	;#1
fname2	db	80d dup(?)	;#2
fname3	db	80d dup(?)	;#3
fname4	db	80d dup(?)	;#4
;
dosfil	db	80d dup(?)	;.ASCIZ result of DOSDG
;
; Temp area for GMOUNT (stores switches etc. from parsing MOUNT command)
;
tarea	label	word
tdev	dw	2 dup(?)	;logical device name, flag,,unit
tfsys	dw	1 dup(?)	;disk structure type (RT11, OS8 etc.)
tmed	dw	1 dup(?)	;disk medium type (RX50, etc.)
tfnam	dw	2 dup(?)	;image filename ptr, length
tdrv	dw	1 dup(?)	;drive # +100h
tbpart	dw	1 dup(?)	;BIOS partition # +100h
ttype	dw	1 dup(?)	;partition type +100h
tintlv	dw	1 dup(?)	;interleaved image file flag +100h
tpart	dw	1 dup(?)	;RT-11 MSCP (or OS/8 RK05) partition #
tronly	dw	1 dup(?)	;read-only astatus +100h
ltarea=	($-tarea)		;length in bytes
tdnam	dw	1 dup(?)	;buf for 'A:' or whatever if drv not given
;
numfd	db	1 dup(?)	;# of floppy drives
maxfd	db	1 dup(?)	;max valid floppy drive letter (others are HDs)
numhd	db	1 dup(?)	;# of hard drives
firhd	db	1 dup(?)	;unit # (from 0) of first HD (after floppy/ies)
lethd	db	1 dup(?)	;physical drive letter of first HD
;
; Floppy formating variables:
flpvec	dw	1 dup(?)	;ptr to routine to make sector interleave table
				;in FMTFLP, or ptr to I/O routine in SECXFR
flphd	db	1 dup(?)	;curr head #
flpcyl	db	1 dup(?)	;curr cyl # (MUST FOLLOW FLPHD)
;
; Floppy data:
;
necbuf	db	9d dup(?)	;765 I/O buffer
fdden	db	1 dup(?)	;MFM for DD, FM for SD
fdcmd	dw	1 dup(?)	;FDC read/write/verify cmd,,DMA cmd
;;fdadr	dw	1 dup(?)	;offset of buffer addr
fdtry	db	1 dup(?)	;retry count
fdrst	db	1 dup(?)	;NZ => FDC has been reset since last prompt
;
fddrv	db	1 dup(?)	;drive # (0-3)
fdsec	db	1 dup(?)	;sector # (1-36.)
fdcyl	db	1 dup(?)	;cyl # (0-79.) (MUST FOLLOW FDSEC)
fdnum	db	1 dup(?)	;number of sectors to transfer (not used)
fdhd	db	1 dup(?)	;head # (0-1) (MUST FOLLOW FDNUM)
fdlen	db	1 dup(?)	;sector length (0=128., 1=256., 2=512.)
fdeot	db	1 dup(?)	;sectors/track (MUST FOLLOW FDLEN)
fdgpl	db	1 dup(?)	;gap 3 length
fddtl	db	1 dup(?)	;max transfer length (MUST FOLLOW FDGPL)
fdspc	dw	1 dup(?)	;"specify" cmd argument bytes
fdspn	db	1 dup(?)	;NZ => wait for motor to spin up before xfr
fdmtr	dw	1 dup(?)	;# ticks to wait for motor to turn on (write)
fdgpf	db	1 dup(?)	;gap 3 length for format
fdfil	db	1 dup(?)	;filler byte for format (MUST FOLLOW FDGPF)
fddcr	db	1 dup(?)	;value for disk control reg (3F7)
				;0 => 500kHz/360RPM (HD)
				;1 => 250kHz/300RPM or 300kHz/360RPM (DD in HD)
				;2 => 250kHz/300RPM (DD in DD)
				;3 => 1MHz/300RPM (ED)
fdvrt	db	1 dup(?)	;FF => vertical mode (ED), 00 => longitudinal
;
cc4ctl	db	1 dup(?)	;value to write to CompatiCard IV control port
				;at 3F0h (gets overwritten on each write to the
				;digital output reg at 3F2h)
				;b0 => controls HD34 pin 2 (DB37 pin 3)
				;b1 => controls HD34 pin 6 (DB37 pin 5)
				;      on my board, only works for the external
				;      connectors but tracing out the PCB it
				;      looks like this is due to a fried driver
				;      in one of the 7407s, it's *supposed* to
				;      drive both
				;b2 => supposedly enables b0 but
				;      experimentation suggests that pin 2 is
				;      just the OR of b0 and b2
				;b3 => either un-tristates DRQ or enables
				;      writes to an SRAM/EEPROM in the EPROM
				;      socket depending on which doc you
				;      believe, experimentation suggests the
				;      former so it should be set
;
; Catweasel/ISA data:
;
cwlat	db	1 dup(?)	;last value written to disk control line latch
cwleft	db	1 dup(?)	;negative # bits left over from previous byte
				;(if NZ, start next with |N|-1 0s and a 1)
;
; TU58 data:
;
ddtime	dw	1 dup(?)	;# clock ticks to wait for TU58 input
ddblk	dw	1 dup(?)	;TU58 blk #
ddptr	dw	2 dup(?)	;far ptr into TU58 data buf
ddctr	dw	1 dup(?)	;# TU58 bytes left to transfer
ddtry	dw	1 dup(?)	;retry count for this starting blk #
dderr	dw	1 dup(?)	;remaining byte count at last xfr error
ddsav	dw	1 dup(?)	;saved DDPTR on each write (for retries)
;
lnum	db	1 dup(?)	;count of lines until next **MORE**
lmax	db	1 dup(?)	;# lines between **MORE**s
;
dvsr0	dw	1 dup(?)	;LSW of divisor in DIV32
dvsr1	dw	1 dup(?)	;MSW of divisor in DIV32
;
secnum	dw	2 dup(?)	;SCSI starting sector number
secoff	dw	2 dup(?)	;starting offset into first sector of xfr
reqcnt	dw	2 dup(?)	;SCSI requested byte count
;
; Sector I/O buffer for CD-ROMs and SCSI devices:
secbuf	dw	1 dup(?)	;ptr to sector buf, or 0 if none
secsiz	dw	1 dup(?)	;size of sector buf in bytes
;
himem	dw	1 dup(?)	;first unusable memory loc
;
binflg	dw	1 dup(?)	;BIN or TEXT (set transfer mode)
bigbuf	dw	1 dup(?)	;seg addr of big file I/O buffer
bigsiz	dw	2 dup(?)	;length of BIGBUF in bytes
bigptr	dw	2 dup(?)	;far ptr into BIGBUF
bigctr	dw	2 dup(?)	;# bytes free in BIGBUF
aread	dw	1 dup(?)	;address of READ routine in FCOPY
awrite	dw	1 dup(?)	;address of WRITE routine in FCOPY
awlast	dw	1 dup(?)	;address of WLAST routine in FCOPY
secflg	db	1 dup(?)	;NZ => sector-by-sector copy in ICOPY
eof	db	1 dup(?)	;NZ => EOF reached
binswt	db	1 dup(?)	;NZ => /BINARY or /ASCII explicitly given
unlswt	db	1 dup(?)	;NZ => /UNLOAD given
indptr	dw	1 dup(?)	;address of next character in INDBUF
indctr	dw	1 dup(?)	;# characters remaining in INDBUF
othswt	dw	1 dup(?)	;ptr to table of "other" switches in GMOUNT
nsegs	dw	1 dup(?)	;# segs in RT-11 directory (INIT /SEGS:n)
;
retlbn	dw	2 dup(?)	;LBN from ODS-1 retrieval pointer
retcnt	dw	2 dup(?)	;count from ODS-1 retrieval pointer
;
indbuf	db	indsiz dup(?)	;indirect file buffer
;
lbuf	db	80d+4+1 dup(?)	;temp line buffer (+4+1 to add "/.EXT/<0>")
;
buf	db	bufsiz dup(?)	;random buffer (for RT-11 dir segs in RTDIR)
				;and other stuff
;
srb	db	(40h+10d+14d) dup(?) ;SCSI request block buffer (ASPI)
;
; buffer for one block of disk data, we allow almost enough space for 2 blks to
; guarantee that we can get 512. contiguous bytes w/o spanning a 64 KB boundary
;
dbuf1	db	dbufl-1 dup(?)	;disk buffer is here (don't need last byte)
dbuf2	db	dbufl dup(?)	;or here if DBUF1 spans 64 KB bound
;
crc	dw	256d dup(?)	;CRC-CCITT lookup table
;
	dw	100h dup(?)	;stack goes here
pdl	label	word
;
mem	label	byte		;free memory starts here
;
code	ends
	end	start
