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
 * $Id: diff.h 1.3.1.5.1.7 Mon, 26 May 1997 02:39:28 -0700 jmacd $
 */


#ifndef _DIFF_H_
#define _DIFF_H_

void format_diff_label(FileEntry* fe, const char* name, Dstring* ds_out);

PrBoolError diff_pair(FileEntry* from,
		      FileEntry* to,
		      const char* from_file,
		      const char* to_file);

PrVoidError merge_help_query_incomplete (MergeParentEntry* entry);

#endif
