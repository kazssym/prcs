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

#include "prcs.h"

#include "projdesc.h"
#include "vc.h"
#include "misc.h"
#include "repository.h"


PrProjectVersionDataPtrError resolve_version(const char* maj,
					     const char* min,
					     const char* project_full_name,
					     const char* project_file_path,
					     ProjectDescriptor* project /* or null */,
					     RepEntry* rep_entry)
{
    ProjectDescriptor* local_project = project;
    ProjectVersionData* local_project_data = NULL;
    ProjectVersionData* new_project_data = NULL;
    Dstring new_major_version;
    Dstring new_minor_version;

    if(!maj[0] || !min[0]) {
	/* caller doesn't have a project descriptor and we need one */
	if(!local_project) {
	    If_fail(local_project << read_project_file(project_full_name,
						       project_file_path,
						       true,
						       KeepNothing))
		pthrow prcserror << "Failed reading project file, can't resolve null version"
				 << dotendl;
	}

	local_project_data = rep_entry->lookup_version(local_project);

	if(!local_project_data)
	    pthrow prcserror << "Project version in working project file "
			    << local_project->full_version()
			    << " does not exist, cannot resolve null version" << dotendl;
    }

    if(!maj[0]) {
	new_major_version.assign(local_project_data->prcs_major());
    } else if(strcmp(maj, "@") == 0) {
	int highest_major;

	if(rep_entry->version_count() == 0)
	    pthrow prcserror << "No versions in the repository, can't "
		"resolve major version '@'" << dotendl;

	highest_major = rep_entry->highest_major_version();

	if(highest_major >= 0) {
	    new_major_version.assign_int(highest_major);
	} else
	    pthrow prcserror << "No numeric major versions in the repository, "
		"can't resolve major version '@'" << dotendl;
    } else {
	new_major_version.assign(maj);
    }

    if(!min[0]) {
	/* Assumption: if min is null then so is maj, as enforced in prcs.cc */
	new_minor_version.assign(local_project_data->prcs_minor());
    } else if(strcmp(min, "@") == 0) {
	/* Highest_minor_version returns 0 if no major is found */
	new_minor_version.assign_int(rep_entry->highest_minor_version(new_major_version, false));
    } else {
	new_minor_version.assign(min);
    }

    if(!project && local_project)
	delete local_project;

    new_project_data = rep_entry->lookup_version(new_major_version, new_minor_version);

    if(strcmp(new_minor_version, "0") != 0 && !new_project_data) {
	/* its not an empty version and we didn't find it in the repository */
	pthrow prcserror << "Could not resolve version " << maj << "." << min << dotendl;
    }

    if(new_project_data) {
	if(new_project_data->deleted())
	    pthrow prcserror << "Project version " << new_project_data << " has been deleted" << dotendl;

	return new_project_data;
    } else {
	pthrow prcserror << "No such version: " << maj << '.' << min << dotendl;
    }
}
