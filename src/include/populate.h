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


#ifndef _POPULATE_H_
#define _POPULATE_H_

extern "C" {
#include "regex2.h"
}

class PrVoidError;

extern PrVoidError prcs_compile_regex(const char* pat, reg2ex2_t *reg2);
extern bool prcs_regex_matches(const char* pat, reg2ex2_t *reg2);

class FileEntry;

class FileRecord {
public:
    FileRecord() { }
    FileRecord(FileType t0, Dstring* n0) :type(t0), name(n0), fe(NULL) { }
    FileRecord(FileType t0, Dstring* n0, FileEntry* fe0) :type(t0), name(n0), fe(fe0) { }
    FileType type;
    Dstring *name;
    FileEntry *fe; /* Used for deletion */
};

#endif
