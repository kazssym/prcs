/* -*-Mode: C++;-*-
 * PRCS - The Project Revision Control System
 * Copyright (C) 1997  Josh MacDonald
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: dstring.cc 1.2.1.3.1.1.1.6.2.4 Sun, 09 May 2004 18:21:12 -0700 jmacd $
 */

extern "C"{
#include <stdarg.h>
}

#include "prcs.h"

static strstreambuf sprintfbuf;

/* I used to use strstreambuf::vform to get printf-like formatting
 * into a growing buffer, but that's been taken away.  So there's
 * this, which can buffer-overrun. @@@ */
const int DSTRSZ = (1<<12);
char hack_buf[DSTRSZ];

ostream& operator<<(ostream& os, const Dstring* S)
{
    return os << S->cast();
}

ostream& operator<<(ostream& os, const Dstring& S)
{
    return os << S.cast();
}

void Dstring::assign_int(int x)
{
    sprintf("%d", x);
}

void Dstring::append_int(int x)
{
    sprintfa("%d", x);
}

void Dstring::sprintfa(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    sprintfbuf.freeze(0);
    sprintfbuf.pubseekoff(0, ios::beg, ios::out);

    hack_buf[DSTRSZ-1] = '\0';
    vsprintf(hack_buf, fmt, args);
    ASSERT('\0' == hack_buf[DSTRSZ-1], "Output buffer bounds exceeded");
    sprintfbuf.sputn (hack_buf, strlen (hack_buf) + 1);

    append(sprintfbuf.str());

    va_end(args);
}

void Dstring::sprintf(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    truncate(0);

    sprintfbuf.freeze(0);
    sprintfbuf.pubseekoff(0, ios::beg, ios::out);

    hack_buf[DSTRSZ-1] = '\0';
    vsprintf(hack_buf, fmt, args);
    ASSERT('\0' == hack_buf[DSTRSZ-1], "Output buffer bounds exceeded");
    sprintfbuf.sputn (hack_buf, strlen (hack_buf)+1);

    append(sprintfbuf.str());

    va_end(args);
}

bool Dstring::is_dstring() { return true; }
