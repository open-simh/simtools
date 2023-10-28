#ifndef MACRO11_H
#define MACRO11_H

#ifndef SKIP_GIT_INFO
#include "git-info.h"
#endif

#define PROGRAM_NAME "macro11"
#define BASE_VERSION "0.9"
#define THIS_VERSION BASE_VERSION " (22 Aug 2023)"     // Mike Hill
//      THIS_VERSION "0.9"        " (22 Aug 2023)"     // Mike Hill
//      THIS_VERSION "0.8.9"      " (29 Jun 2023)"     // Mike Hill
//      THIS_VERSION "0.8.8"      " (25 May 2023)"     // Mike Hill
//      THIS_VERSION "0.8.7"      " (09 May 2023)"     // Mike Hill
//      THIS_VERSION "0.8.6"      " (04 May 2023)"     // Mike Hill
//      THIS_VERSION "0.8.5"      " (28 Apr 2023)"     // Mike Hill
//      THIS_VERSION "0.8.4"      " (04 Apr 2023)"     // Mike Hill
//      THIS_VERSION "0.8.3"      " (28 Mar 2023)"     // Mike Hill
//      THIS_VERSION "0.8.2"      " (21 Mar 2023)"     // Mike Hill
//      THIS_VERSION "0.8.1"      " (09 Mar 2023)"     // Mike Hill
//      THIS_VERSION "0.8"        " (07 Jul 2022)"     // Olaf 'Rhialto' Seibert
//      THIS_VERSION "0.3"        " (April 21, 2009)"  // Joerg Hoppe
//      THIS_VERSION "0.2"        "   July 15, 2001"   // Richard Krehbiel

#if defined(GIT_VERSION)
#define VERSIONSTR   BASE_VERSION " (" GIT_VERSION "\n        " GIT_AUTHOR_DATE ")"
#else
#define VERSIONSTR   THIS_VERSION " compiled on " __DATE__ " at " __TIME__ ")"
#endif


/*
Copyright (c) 2001, Richard Krehbiel
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

o Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

o Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

o Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.
*/


#endif
