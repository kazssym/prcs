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


#ifndef _DSTRING_H_
#define _DSTRING_H_

#define DSTRING_DEFAULT_SIZE 16

#include "dynarray.h"

#define parenttype Dynarray<char, DSTRING_DEFAULT_SIZE, true>

class Dstring : public parenttype {
public:
    Dstring() :parenttype() {}
    Dstring(const char c, int N = 1) :parenttype(c, N) {}
    Dstring(const char* str) :parenttype(str) {}
    Dstring(const Dstring& str) :parenttype(str) {}

    virtual bool is_dstring();

    void sprintf(const char*, ...);
    void sprintfa(const char*, ...);
    void append_int(int n);
    void assign_int(int n);
};

#undef parenttype

ostream& operator<<(ostream&, const Dstring*);
ostream& operator<<(ostream&, const Dstring&);

#endif
