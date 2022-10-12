# -*- Makefile -*-

# Builds vhd-util and the libvhd shared library.

PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib
BINDIR = $(PREFIX)/bin

CPPFLAGS =  -D_GNU_SOURCE -I include -I $(LIBVHD)/include
CFLAGS = -O4
LDFLAGS = -L. -L $(LIBVHD)
LDLIBS = -lvhd-util -luuid -lvhd

LDCONFIG_D = /etc/ld.so.conf.d/vhd-util

LIBVHD = ../libvhd

SRCS = atomicio.c 	 \
libvhd-journal.c 	 \
vhd-util-check.c  	\
vhd-util-coalesce.c  	\
vhd-util-create.c  	\
vhd-util-fill.c  	\
lvm-util.c 		\
vhd-util-modify.c  	\
vhd-util-query.c  	\
vhd-util-read.c  	\
vhd-util-repair.c  	\
vhd-util-resize.c  	\
vhd-util-revert.c  	\
vhd-util-scan.c  	\
vhd-util-set-field.c  	\
vhd-util-snapshot.c

OBJS = $(SRCS:.c=.o)

LIBVHDSO = libvhd.so
LIBVHDSONAME = $(LIBVHDSO).2
LIBVHDSOFILE = $(LIBVHDSONAME).1.0

all: vhd-util $(LIBVHD)/$(LIBVHDSO)

# The utility

libvhd-util.a : $(OBJS)
	$(AR) -rs libvhd-util.a $(OBJS)

vhd-util : vhd-util.o libvhd-util.a $(LIBVHD)/$(LIBVHDSO)
	$(CC) $(LDFLAGS) -Wl,-rpath,$(LIBDIR) -o vhd-util vhd-util.o $(LDLIBS)

# The sharable library

$(LIBVHD)/$(LIBVHDSOFILE) : $(LIBVHD)/libvhd.c $(LIBVHD)/relative-path.c
	$(CC) -shared -o $(LIBVHD)/$(LIBVHDSOFILE) -Wl,-soname,$(LIBVHDSONAME) $(CPPFLAGS) $(CFLAGS) -fPIC $(LIBVHD)/libvhd.c  $(LIBVHD)/relative-path.c -luuid

# The symlinks in the local directory necessary for linking

$(LIBVHD)/$(LIBVHDSO) : $(LIBVHD)/$(LIBVHDSOFILE)
	ldconfig -n $(LIBVHD)
	ln -sf $(LIBVHD)/$(LIBVHDSOFILE) $(LIBVHD)/$(LIBVHDSO)

# Install (note that to run the utility before installation, you must set
# LD_LOAD_LIBRARY to $(VLDLIB)

install: all
	cp -p $(LIBVHD)/$(LIBVHDSOFILE) $(LIBDIR)/
	cp -p vhd-util $(BINDIR)/
	echo "$(LIBDIR)" >$(LDCONFIG_D)
	ldconfig
	ln -sf $(LIBVHDSONAME) $(LIBDIR)/$(LIBVHDSO)

# Un-install

uninstall:
	rm -f $(BINDIR)/vhd-util $(LIBDIR)/$(LIBVHDSOFILE)  $(LIBDIR)/$(LIBVHDSONAME)  $(LIBDIR)/$(LIBVHDSO)
	ldconfig
	rm -f  $(LDCONFIG_D)

# Clean working area

clean: tidy
	rm -f vhd-util vhd-util.exe $(LIBVHD)/$(LIBVHDSOFILE) $(LIBVHD)/$(LIBVHDSONAME) $(LIBVHD)/$(LIBVHDSO)

# Clean intermediate files only, leaving outputs

tidy:
	rm -f $(OBJS) libvhd-util.a vhd-util.o

KITFILES = \
vhd-util/README \
vhd-util/INSTALL \
vhd-util/COPYING \
vhd-util/*.c \
vhd-util/include/*.h \
vhd-util/vhd-util \
vhd-util/vhd-util.exe \
vhd-util/Makefile \
vhd-util/vhd-util.vcxproj \
vhd-util/vhd-util.vcxproj.filters \
\
libvhd/*.c \
libvhd/libvhd.so* \
libvhd/libvhd.sln \
libvhd/libvhd.vcxproj \
libvhd/libvhd.vcxproj.filters \
libvhd/include/*.h \

KITEXES = \
vhd-util/vhd-util \
vhd-util/vhd-util.exe \
libvhd/libvhd.so*

kit: all
	cd .. && chown -h $(USER).$(USER) $(KITFILES) && chmod -x $(KITFILES) && chmod +x $(KITEXES) && tar -czf vhd-tools.tgz $(KITFILES)

