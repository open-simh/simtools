#!/usr/bin/env python3

"""SIMH disk container trailer stripper

Copyright (C) 2022 by Paul Koning

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the names of The Authors shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the Authors.

This program strips the "trailer" from Mark Pizzolato's version of SIMH from
disk container files.

To use it, run the script with the names of the containers to be
stripped.  Two switches may be included in the argument list; they
take effect when seen, so typically you will want them at the start of
the argument list:

-q: don't print any messages
-f: if a trailer is found, strip it without asking

By default, messages are printed as to whether a trailer is seen or
not, and if found, the user is prompted whether to strip it.  In that
prompt, the default answer is "no".
"""

import sys
import io
from zlib import crc32

m1 = 0xffffffff
force = quiet = False

def zap (fn):
    with open (fn, "r+b") as f:
        l = f.seek (0, io.SEEK_END)
        if l < 8192:
            if not quiet:
                print (fn, "is too small to consider")
            return
        lastblk = f.seek (-512, io.SEEK_END)
        if lastblk % 512:
            if not quiet:
                print (fn, "is not a multiple of 512 bytes long")
            return
        tail = f.read (512)
        if not tail.startswith (b"simh"):
            if not quiet:
                print (fn, "last block does not start with 'simh'")
            return
        c1 = crc32 (tail[:508]) & m1
        c = int.from_bytes (tail[508:], "big")
        if c1 != c:
            print (fn, "trailer CRC does not match, {:0>8x} vs. {:0>8x}".format (c, c1))
            return
        if force or input ("{} strip trailer? ".format (fn)).lower ().startswith ("y"):
            f.truncate (lastblk)
            if not quiet:
                print ("trailer stripped from", fn)
        else:
            if not quiet:
                print (fn, "not changed")
            
if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        print ("usage: zap.py [-q] [-f] container ...")
    for fn in args:
        if fn == "-q":
            quiet = True
            continue
        if fn == "-f":
            force = True
            continue
        zap (fn)
