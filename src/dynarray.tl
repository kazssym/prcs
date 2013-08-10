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
 * $Id: dynarray.tl 1.1.1.3.1.6 Mon, 01 Jun 1998 01:44:36 -0700 jmacd $
 */

#include "prcs.h"
#include "dynarray.h"
#include "dstring.h"
#include "syscmd.h"
#include "vc.h"
/* this is a Dstring child class */
template class Dynarray< char, DSTRING_DEFAULT_SIZE, true >;
/* this is an ArgList */
template class Dynarray< const char*, 64, true >;
/* this is a RcsDeltaPtrArray */
template class Dynarray< RcsDelta*, 64, false >;
/* this is a RcsVersionDataPtrArray */
template class Dynarray< RcsVersionData*, 64, false >;
/* this is a ProjectVersoinDataPtrArray */
template class Dynarray< ProjectVersionData*, 64, false >;
/* this is a MergedParentVersions */
template class Dynarray< Dstring*, 64, false >;
/* this is a file entry, project->file_entries */
template class Dynarray< FileEntry*, 64, false >;
/* this is used by rebuild */
template class Dynarray< RcsVersionDataPtrArray*, 64, false >;
template class Dynarray< MergeParentEntry *, 4, false >;
template class Dynarray< MergeParentFile *, 64, false >;
template class Dynarray< const ListMarker *, 4, false >;
template class Dynarray< MergeCandidate* , 4, false >;
template class Dynarray< int, 4, false >;
template class Dynarray< const char*, 64, false >;
