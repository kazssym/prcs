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
 * $Id: quick.h 1.3.4.5 Sun, 25 May 1997 21:10:06 -0700 jmacd $
 */


#ifndef _QUICK_H_
#define _QUICK_H_

NprQuickElimPtrError read_quick_elim(const char* filename);

#include "sexp.h"
#include "hash.h"

class QuickElimEntry {
public:
    Dstring name;
    off_t length;
    time_t mtime;
    Dstring desc_name;
    Dstring version_num;
};

class QuickElim {
public:
    QuickElim();
    ~QuickElim();

    friend NprQuickElimPtrError read_quick_elim(const char* filename);

    bool unchanged(const FileEntry* fe) const;

    void update(const FileEntry* fe);

    PrVoidError update_file(const char* filename) const;

private:

    NprVoidError init_from_file(const char* filename);

    QuickElimTable* quick_table;
};

#endif
