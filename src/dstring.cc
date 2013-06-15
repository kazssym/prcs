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

extern "C"{
#include <stdarg.h>
}

#include "prcs.h"

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

    char *str;
    vasprintf(&str, fmt, args);
    append(str);
    free(str);

    va_end(args);
}

void Dstring::sprintf(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    truncate(0);

    char *str;
    vasprintf(&str, fmt, args);
    append(str);
    free(str);

    va_end(args);
}

bool Dstring::is_dstring() { return true; }
