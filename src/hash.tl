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
 * $Id: hash.tl 1.1.1.2.1.2.1.7 Mon, 01 Jun 1998 01:44:36 -0700 jmacd $
 */

#include "prcs.h"
#include "dstring.h"
#include "populate.h"
#include "projdesc.h"
#include "fileent.h"
#include "sexp.h"

#define MkHash(key, data) \
    template class HashTable<key, data>; \
    template class Pair<key, data>; \
    template class List< Pair<key, data> >

MkHash(ino_t, FileType); /* an inode hash table used by the populate routine */
MkHash(const char*, FileType); /* a const char* hash table used by check_prj_sanity */
MkHash(const char*, FileEntry*); /* for diff */
MkHash(const char*, RcsDelta*); /* for rcsfiletable */
MkHash(const char*, MergeParentFile*);
MkHash(const char*, QuickElimEntry*);
MkHash(const char*, const char*);
MkHash(FileEntry*, FileEntry*);
MkHash(const PrcsAttrs*, PrcsAttrs*);
MkHash(const char*, SystemCommand*);

template class OrderedTable<const char*, const char*>;

template class List< FileRecord >; /* a list of records for populate */
template class List< MemorySegment* >;
template class List< DelayedJob* >;
template class List< void* >;
