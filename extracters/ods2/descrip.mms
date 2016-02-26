#
#  MMS (MMK) description file to build ODS2 for VMS
#
#  To compile on VAX using VAX C, use:
#
#	$ mmk/macro=vaxc=1
#
#
.IFDEF EXE
.ELSE
EXE = .EXE
OBJ = .OBJ
OLB = .OLB
.ENDIF

.IFDEF __DEBUG__
CFLAGS = $(CFLAGS)/DEBUG/NOOPTIMIZE
LINKFLAGS = $(LINKFLAGS)/DEBUG
.ELSE
LINKFLAGS = $(LINKFLAGS)/NOTRACE
.ENDIF

.IFDEF __VAXC__
CFLAGS = $(CFLAGS)/INCLUDE=SYS$DISK:[]
OPTFILE = ,VAXCRTL.OPT
OPTIONS = $(OPTFILE)/OPTIONS
.ELSE
OPTFILE =
OPTIONS =
.ENDIF

OBJS = $(ODS2_OBJS) $(ODS2I_OBJS)

COMMON_OBJS = RMS,DIRECT,ACCESS,DEVICE,CACHE,UPDATE,SYSMSG,VMSTIME

ODS2_OBJS = ODS2$(OBJ) $(COMMON_OBJS) PHYVMS

ODS2I_OBJS = ODS2I$(OBJ) $(COMON_OBJS) DISKIO

ODS2 : ODS2$(EXE) ODS2I(EXE)

ODS2I$(EXE) : ODS2I$(OLB)($(ODS2I_OBJS))$(OPTFILE)
	$(LINK) $(LINKFLAGS) ODS2I$(OLB)/INCLUDE=($(ODS2I_OBJS))$(OPTIONS)

ODS2$(EXE) :	ODS2$(OLB)($(OBJS))$(OPTFILE)
	$(LINK)$(LINKFLAGS) ODS2$(OLB)/INCLUDE=($(ODS2_OBJS))$(OPTIONS)

vmstime$(OBJ) : vmstime.c vmstime.h

sysmsg$(OBJ) : sysmsg.c sysmsg.h ssdef.h rms.h

diskio$(OBJ) : diskio sysmsg.h ssdef.h rms.h

cache$(OBJ) : cache.c cache.h ssdef.h

phyvms$(OBJ) : phyvms.c phyio.h ssdef.h

device$(OBJ) : device.c ssdef.h access.h phyio.h

access$(OBJ) : access.c ssdef.h access.h phyio.h

update$(OBJ) : update.c ssdef.h access.h

direct$(OBJ) : direct.c direct.h access.h fibdef.h descrip.h ssdef.h

rms$(OBJ) : rms.c rms.h direct.h access.h fibdef.h descrip.h ssdef.h

ods2$(OBJ) : ods2.c compat.h sysmsg.h phyio.h ssdef.h descrip.h access.h rms.h

CFLAGS=$(CFLAGS)/DEFINE=DISKIMAGE
ods2i$(OBJ) : ods2.c compat.h diskio.h sysmsg.h phyio.h ssdef.h descrip.h access.h rms.h

VAXCRTL.OPT :
	@ open/write tmp $(MMS$TARGET)
	@ write tmp "SYS$SHARE:VAXCRTL.EXE/SHARE"
	@ close tmp
