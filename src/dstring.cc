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
 * $Id$
 */

#include "dstring.h"
#include "config.h"
#ifdef HAVE_STD_H
#include <std.h>
#endif
#include <stdarg.h>

#if defined(__GNUG__) || defined(__MWERKS__)
#if defined(__GNUG__)
#include <strstream.h>
#else
#include "be-strstream.h"
#endif
static ostrstream sprintfbuf;
#endif

#include "utils.h"

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
#ifdef __GNUG__
    sprintfbuf.rdbuf()->seekoff(0, ios::beg);
#else
    sprintfbuf.rdbuf()->pubseekoff(0, ios::beg, ios::out);
#endif

    sprintfbuf.vform(fmt, args);

    sprintfbuf << ends;

    append(sprintfbuf.str());

    va_end(args);
}

void Dstring::sprintf(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    truncate(0);

    sprintfbuf.freeze(0);
#ifdef __GNUG__
    sprintfbuf.rdbuf()->seekoff(0, ios::beg);
#else
    sprintfbuf.rdbuf()->pubseekoff(0, ios::beg, ios::out);
#endif

    sprintfbuf.vform(fmt, args);

    sprintfbuf << ends;

    append(sprintfbuf.str());

    va_end(args);
}

bool Dstring::is_dstring() { return true; }
