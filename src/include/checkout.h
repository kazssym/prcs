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


#ifndef _CHECKOUT_H_
#define _CHECKOUT_H_

extern PrVoidError make_subdirectories(const char*);

extern PrVoidError check_create_subdir(const char* dir);

extern PrProjectVersionDataPtrError resolve_version(const char* version_specifier_major,
						    const char* version_specifier_minor,
						    const char* project_full_name,
						    const char* project_file_path,
						    ProjectDescriptor* project, /* or null */
						    RepEntry* rep_entry);

extern PrProjectDescriptorPtrError checkout_empty_prj_file(const char* fullname,
							   const char* maj,
							   ProjectReadData flags);

extern PrProjectDescriptorPtrError checkout_create_empty_prj_file(const char* fullname,
								  const char* name,
								  const char* maj,
								  ProjectReadData flags);

class WriteableFile {
public:
    WriteableFile();

    PrVoidError open(const char* filename);

    PrVoidError write(const char* seg, int len);

    PrVoidError copy(FILE* copy_me);

    PrVoidError copy(const char* copy_me);

    PrVoidError close();

    ostream& stream();

private:
    ofstream os;

    const char* real_name;
    Dstring temp_name;
};

#endif
