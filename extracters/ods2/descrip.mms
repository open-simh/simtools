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
CFLAGS = $(CFLAGS)/DEFINE=(NO_DOLLAR)

.IFDEF __VAXC__
OPTFILE = ,VAXCRTL.OPT
OPTIONS = $(OPTFILE)/OPTIONS
.ELSE
OPTFILE =
OPTIONS =
.ENDIF

OBJS = ODS2,RMS,DIRECT,ACCESS,DEVICE,CACHE,PHYVMS,SYSMSG,UPDATE,PHYVIRT

ODS2$(EXE) :	ODS2$(OLB)($(OBJS))$(OPTFILE)
	$(LINK)$(LINKFLAGS) ODS2$(OLB)/INCLUDE=($(OBJS))$(OPTIONS)

phyvirt$(obj) : phyvirt.c phyvirt.h compat.h ssdef.h

sysmsg$(obj) : sysmsg.c sysmsg.h rms.h ssdef.h

compat$(obj) : compat.c compat.h

cache$(obj) : cache.c cache.h ssdef.h

phyvms$(obj) : phyvms.c phyio.h phyvirt.h ssdef.h descrip.h access.h rms.h

device$(obj) : device.c ssdef.h access.h phyio.h

access$(obj) : access.c ssdef.h access.h phyio.h compat.h sysmsg.h phyvirt.h

update$(obj) : update.c ssdef.h access.h

direct$(obj) : direct.c direct.h access.h fibdef.h descrip.h ssdef.h

rms$(obj) : rms.c rms.h compat.h direct.h access.h fibdef.h descrip.h ssdef.h

ods2$(obj) : ods2.c compat.h sysmsg.h phyio.h ssdef.h descrip.h access.h rms.h version.h

VAXCRTL.OPT :
	@ open/write tmp $(MMS$TARGET)
	@ write tmp "SYS$SHARE:VAXCRTL.EXE/SHARE"
	@ close tmp
