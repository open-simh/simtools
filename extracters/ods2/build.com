$ gccflag = "CC"
$ If ( F$Extract( 0, 3, gccflag ) .nes. "GCC" ) Then $ gccflag = ""
$
$ default = F$Environment( "DEFAULT" )
$ this    = F$Environment( "PROCEDURE" )
$ src     = F$Parse( this, , , "DEVICE",    "SYNTAX_ONLY" ) +  -
            F$Parse( this, , , "DIRECTORY", "SYNTAX_ONLY" )
$
$ If ( P1 .eqs. "CLEAN" )
$ Then
$    Set Verify
$ Set Default 'src'
$ Delete ODS2.EXE;*,*.OBJ;*,DECC$SHR.LIS;*
$ Set Default 'default'
$    junk = 'F$Verify( 0 )'
$    Exit
$ EndIf
$
$ If ( gccflag .nes. "" )
$ Then
$    CCompiler = "GCC"
$    CFlags = "/Optim=Level=3/Warn=all"
$ Else
$    CFlags = "/Define=(NO_DOLLAR)"
$    If ( F$GetSYI( "HW_MODEL" ) .lt. 1024 )
$    Then
$       CCompiler = "CC/VAXC"
$    Else
$       CCompiler = "CC/DECC"
$!      64-bit off_t requires OpenVMS/Alpha V7.2, DECC$SHR V7.2-1-04
$       Set Verify
$ Analyze/Image Sys$Library:DECC$SHR.exe/Header/Output=DECC$SHR.LIS;1
$       junk = 'F$Verify( 0 )'
$       version = ""
$       Open/Read lis_file DECC$SHR.LIS;1
$LIS_LOOP:
$       Read/End=CLOSE_LIS lis_file line
$       line_len = F$Length( line )
$       If ( F$Locate( "image file identification:", line ) .lt. line_len )
$       Then
$          line = line - "image file identification:"
$          version := 'line'
$          Goto CLOSE_LIS
$       EndIf
$       Goto LIS_LOOP
$CLOSE_LIS:
$       Close lis_file
$       If ( ( F$Extract( 0, 1, version ) .eqs. "V" ) .and. -
             ( version .ges. "V7.2-1-04" ) ) Then -
$          CFlags = "/Define=(NO_DOLLAR,_LARGEFILE)"
$    EndIf
$ EndIf
$
$ Set Verify
$ Set Def 'src'
$ junk = 'F$Verify( 0 )'
$
$ Call Compile ods2
$ Call Compile rms
$ Call Compile direct
$ Call Compile access
$ Call Compile device
$ Call Compile cache
$ Call Compile phyvms
$ Call Compile update
$ Call Compile phyvirt
$
$ If ( CCompiler .eqs. "CC/VAXC" )
$ Then
$    Set Verify
$ Define/User_Mode LNK$Library Sys$Library:VAXCRTL
$    junk = 'F$Verify( 0 )'
$ EndIf
$ Set Verify
$ Link ods2,rms,direct,access,device,cache,phyvms,phyvirt,update
$ junk = 'F$Verify( 0 )'
$ Write Sys$Error "''F$Time()' Done"
$
$ Set Verify
$ Set Def 'default'
$ junk = 'F$Verify( 0 )'
$ Exit
$
$Compile:
$	Subroutine
$	If ( F$Search( "''P1'.obj" ) .nes. "" )
$	Then
$	   If ( F$CvTime( F$File( "''P1'.obj", "CDT" ) ) .ges. -
            F$CvTime( F$File( "''P1'.c",   "CDT" ) ) ) Then $ Exit
$	EndIf
$	Set Verify
$ 'CCompiler' 'CFlags' 'P1'
$	junk = 'F$Verify( 0 )'
$	EndSubroutine
