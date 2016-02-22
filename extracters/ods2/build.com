$ gccflag := 'cc'
$ if f$extract(0,3,gccflag).nes."GCC" then gccflag=""
$
$ if gccflag.nes.""
$      then p1="/optim=level=3/warn=all"+p1
$      else p1="/warn=enabl=(defunct,obsolescent,questcode,uninit,unused)"+p1
$ endif
$
$ if f$search("SYS$SYSTEM:DECC$COMPILER.EXE").eqs."" .and. gccflag.eqs.""
$ then p1 = "/INCLUDE=SYS$DISK:[]"+p1
$ endif
$ default=f$parse(f$environment("PROCEDURE"),,,"DEVICE","SYNTAX_ONLY")+  -
        f$parse(f$environment("PROCEDURE"),,,"DIRECTORY","SYNTAX_ONLY")
$ set def 'default'
$
$ call cc ods2    'p1'
$ call cc rms     'p1'
$ call cc direct  'p1'
$ call cc access  'p1'
$ call cc device  'p1'
$ call cc cache   'p1'
$ call cc phyvms  'p1'
$ call cc update  'p1'
$ call cc vmstime 'p1'
$
$ write sys$error "''f$time()' Linking..."
$ if gccflag.nes."" .and. f$getsyi("HW_MODEL").lt.1024
$    then library = ",vaxcrtl.tmp/option"
$         create vaxcrtl.tmp
sys$share:vaxcrtl/share
$ endif
$ link 'p2' ods2,rms,direct,access,device,cache,phyvms,vmstime,update 'library'
$ write sys$error "''f$time()' Done"
$ exit
$
$cc: subroutine
$ if f$search(p1+".obj;").nes."" then if f$cvtime(f$file(p1+".obj;","CDT")).ges.f$cvtime(f$file(p1+".c;","CDT")) then exit
$ write sys$error "''f$time()' Compiling ''p1'..."
$ cc 'p2'  'p1'
$ exit
$ endsubroutine
