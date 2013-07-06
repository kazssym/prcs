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
 * $Id: checkin.h 1.3.1.4.1.7.1.1 Sun, 05 Jan 1997 02:59:04 -0800 jmacd $
 */


#ifndef _CHECKIN_H_
#define _CHECKIN_H_

extern PrVoidError check_project(ProjectDescriptor*);

extern void eliminate_unnamed_files(ProjectDescriptor*);

extern PrVoidError warn_unused_files(bool prompt_abort);

extern void format_version_log(ProjectVersionData* project_data, Dstring& log);

PrVoidError eliminate_working_files(ProjectDescriptor* project, MissingFileAction action);

#endif
