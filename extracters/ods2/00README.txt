ODS2 provides access to DEC Files-11 On-Disk Structure Level 2 volumes from
Unix, VMS & Windows hosts.

It is capable of reading and writing physical volumes, such as SCSI disks and
CDROMs, as well as SimH simulator files (native, and optionally VHD).

The original code was developed by Paul Nankervis, email address:  Paulnank@au1.ibm.com.

This version has been extensively re-worked - debugged, completed and enhanced - by
Timothe Litt - litt@acm.org.  It also contains contributions from Larry Baker of
the USGS and from Hunter Goatley.  The VHD support is open-source from the Xen
project - with some minor changes on Unix & some significant changes for Windows.

This version provides full read and write support for ODS2 volumes - physical and
virtual.  It can initialize volumes, create, read, write, and extend files.  It
allows files to be copied between the host filesystem and the Files-11 volume(s),
converting all RMS formats except indexed.  Volume sets and multiple independent volumes
can be accessed simultaneously.

ODS2 is distributed freely for all members of the VMS community to use. However all
derived works must maintain comments in their source to acknowledge the contributions
of the original author and subsequent contributors.   This is free software; no warranty
is offered,  and while we believe it to be useful, you use it at your own risk.

There are opportunities for additional contributions, among them:
- message file translations
- help file completion and translations
- testing on other host platforms
Please use the https://github.com/open-simh/simtools repository for pull
requests and issues.

ODS2 starts by executing a command file, which it looks for in the following order:
     The environment variable (or logical name) ODS2INIT
     The user's home directory
         Windows: %HOMEDRIVE%%HOMEPATH%\.ods2.ini
         VMS: SYS$LOGIN:_ODS2.INI
         Unix: $HOME/.ods2.ini
This can be used to specify defaults, mount a favorite device...
No error is reported if the file does not exist.

Next, ODS2 executes any commands specified on the command line.
Multiple command are separated by '$' (most shells require $ to be quoted).

Finally, ODS2 enters interactive mode.  Under Unix, with READLINE support, command
history is preserved in $HOME/.ods2.

The HELP command provides full command syntax, which is very similar to DCL.
The help text is located in ods2.hlb, in the same directory as ODS2.  You can
set the environment variable ODS2HELP to an alternate helpfile or use the
set helpfile command.  Note that helpfiles are binary - see 00INSTALL.TXT..

N.B. The default help library filename is "ods2.hlb", in the same directory as
the ods2 executable.  This file is made a symlink or copy of the default
language by the makefile.  If another available language is desired,
softlink (or copy) the desired language file to it (or set the ODS2HELP
 environment variable.)
E.g. for US English help: ln -s ods2_en_us.hlb ods2.hlb

The following is a brief summary of commonly-used command sequences.
Abbreviations can be used. Full VMS filenames (device:[dir.sfd...]name.ext;gen)
can be used for files on the FILES-11 volume.  Wildcards are supported for most
commands.  Wildcards can be used on the host filesystem, e.g. when copying files
to the volume.

The most important command for any program:
ODS2$> EXIT

(QUIT also works.)

Create a new Files-11 volume:

ODS2$> INITIALIZE /medium:RM02/LOG rm02.vhd myvolume

N.B. The .vhd file type will create a dynamic (sparse) VHD-format simulator file.
     It is important to specify the medium type, as FILES-11 is sensitve to volume
     size and geometry.

Create a directory:
ODS2$> CREATE/DIRECTORY/LOG [FRED.PAKS]

Copy files to the volume:
ODS2$> COPY /TO_FILES-11/ASCII *.txt *.doc [FRED.PAKS]*.*

Set default directory on the volume:
ODS2$> SET DEFAULT [FRED.PAKS]

See what's there:
ODS2$> DIR /DATE/SIZE

Copy files from the volume:
ODS2$> COPY /FROM_FILES-11/ASCII *.txt *.*

Mount an existing volume:
ODS2$> MOUNT /medium:RM02 /WRITE rmo2.vhd myvolume

N.B. The volume name is optional, but if specified must match.

Dismount the volume:
ODS2$> DISMOUNT A:
(The drive letter will vary - it's reported by INITIALIZE/MOUNT)

N.B. It's VERY important to dismount the volume, especially if it's mounted
     for write.  ODS2 makes heavy use of caching, and any interruption will
     leave it in an inconsistent state.

Inspect a file on the volume:
ODS2$> TYPE/PAGE *.TXT

Obtain information:
ODS2$> SHOW DEVICES
ODS2$> SHOW VOLUMES

Interactively create a file:
ODS2$> CREATE FILE.TXT
mumble
gobble
^D
(or ^Z on Windows/VMS)

Delete a file
ODS2$> DELETE FILE.TXT;2

More advanced usage:

When asked for confirmation, the VMS DCL responses are accepted:
     0, no, false : Don't do this action, ask again for additional items
     1, yes, true : Proceed with this action, ask again for additional items
     quit         : Don't do this action, and don't consider any more items
     all          : Proceed with this action, and do all other items without asking

Lines beginning with ';' or '!' are considered comments, and ignored. This
may be used for commenting command files.

With editline (libedit) support, a comment entry is made in the command history
when an initialization file is opened.  This can be used to trace initialization
issues.  editlline also uses ~/.editrc and can be customized using the
'ods2' tag.  editline is a Unix command history editor that can be compiled
with ods2.  'man 7 editline' and 'man 5 editrc' for usage details.  editline
is available on most Linux distributions; building ods2 requires the corresponding
-dev(el) package.

It may be necessary to quote command arguments, e.g. when specifying a
file name on a Unix system that includes '/'.  Use '"' or "'"; double
the quote character to include it in the string. e.g. """hello""" produces
    "hello"

Alternatively, you can switch to unix-style options with SET QUALIFIER_STYLE UNIX.

A heuristic is used to distinguish physical devices from simulator disk image
files in the mount and initialize commands.  Use /device or /image if it
guesses wrong.

It is possible to mount multiple volumes simultaneously, though not
(currently) to move files directly from one Files-11 volume to another.

It is possible to mount volumesets, though this hasn't been completely debugged.

Command files can be executed with @filename.  They can be nested.

Favorite directory and/or copy qualifiers can be set in the .ini file, e.g.
SET DIRECTORY /DATE/SIZE

With VHD support, you can write to a snapshot of the volume:
ODS2$> MOUNT /SNAPSHOTS_OF=myvolume.vhd testvolume.vhd /write

For more details on these and other capabilities, use the HELP command.

For build/installation information, see 00INSTALL.TXT

Bug reports, contributions, and feature requests are welcome
at https://github.com/open-simh/simtools/issues.


-- Timothe Litt
   April 2016, October 2022
