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
 * $Id: changes.cc 1.2 Sun, 28 Oct 2001 17:24:49 -0800 jmacd $
 */

#include "prcs.h"
#include "projdesc.h"
#include "fileent.h"
#include "repository.h"
#include "vc.h"
#include "misc.h"
#include "checkin.h"
#include "checkout.h"

#define COL 21

PrVoidError get_projects (RepEntry            *rep_entry,
			  ProjectVersionData  *project_data,
			  ProjectVersionData  *parent_data,
			  ProjectDescriptor  **project_desc,
			  ProjectDescriptor  **parent_desc)
{
    /* This routine tries to keep two project descriptors in memory at
     * all times, thus allowing both forward and reverse traversals to
     * be fast in the common case.  When a project has more than one
     * parent it will be a bit slower... */
    static ProjectDescriptor *p1 = NULL;
    static ProjectDescriptor *p2 = NULL;

    static ProjectVersionData *v1 = NULL;
    static ProjectVersionData *v2 = NULL;

    bool p1_used = false;
    bool p2_used = false;

    bool parent_set = false;
    bool project_set = false;

    if (project_data == v1) {
	(*project_desc) = p1;
	p1_used = true;
	project_set = true;
    } else if (project_data == v2) {
	(*project_desc) = p2;
	p2_used = true;
	project_set = true;
    }

    if (parent_data) {
	if (parent_data == v1) {
	    (*parent_desc) = p1;
	    p1_used = true;
	    parent_set = true;
	} else if (parent_data == v2) {
	    (*parent_desc) = p2;
	    p2_used = true;
	    parent_set = true;
	}
    }

    if (! project_set) {
	Return_if_fail ((*project_desc) << rep_entry-> checkout_prj_file (cmd_root_project_full_name,
									  project_data->rcs_version(),
									  KeepNothing));

	eliminate_unnamed_files (*project_desc);

	if (! p1_used) {
	    if (p1) delete p1;
	    p1 = (*project_desc);
	    v1 = project_data;
	    p1_used = true;
	} else if (! p2_used) {
	    if (p2) delete p2;
	    p2 = (*project_desc);
	    v2 = project_data;
	    p2_used = true;
	}
    }

    if (parent_data && ! parent_set) {
	Return_if_fail ((*parent_desc) << rep_entry-> checkout_prj_file (cmd_root_project_full_name,
									 parent_data->rcs_version(),
									 KeepNothing));

	eliminate_unnamed_files (*parent_desc);

	if (! p1_used) {
	    if (p1) delete p1;
	    p1 = (*parent_desc);
	    v1 = parent_data;
	    p1_used = true;
	} else if (! p2_used) {
	    if (p2) delete p2;
	    p2 = (*parent_desc);
	    v2 = parent_data;
	    p2_used = true;
	}
    }

    return NoError;
}

static ProjectVersionData *print_project = NULL;
static ProjectVersionData *print_parent = NULL;

static void print_header (ProjectDescriptor  *pdesc,
			  ProjectVersionData *project,
			  ProjectVersionData *parent)
{
    if (print_project != project) {
	prcsoutput << cmd_root_project_name << ' '
		   << project << ' '
		   << time_t_to_rfc822(project->date()) << " by "
		   << project->author() << prcsendl;
	print_project = project;

	if (option_long_format) {
	    if(strchr(*pdesc->version_log(), '\n') != NULL || strlen(*pdesc->version_log()) > 60) {
		prcsoutput << "Version-Log:" << prcsendl
			   << pdesc->version_log() << prcsendl;
	    } else {
		prcsoutput << "Version-Log:" << setcol(COL+1)
			   << (pdesc->version_log()->index(0) == '\0' ? "empty" : pdesc->version_log()->cast())
			   << prcsendl;
	    }
	}

	if (! parent) {
	    prcsoutput << "  initial version:" << prcsendl;
	}
    }

    if (print_parent != parent) {
	prcsoutput << "  changes from parent " << parent << ":" << prcsendl;
	print_parent = parent;
    }
}

static PrVoidError print_changes (RepEntry           *rep_entry,
				  ProjectVersionData *project_data,
				  ProjectVersionData *parent_data,
				  ProjectDescriptor  *project,
				  ProjectDescriptor  *parent)
{
    /* this code was copied from checkin.cc compute_modified, then changed */

    foreach_fileent(fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

	if (! fe->on_command_line ())
	    continue;

	const char *new_name = fe->working_path();
	const char *old_name = NULL;

	const char *new_lname = fe->link_name ();
	const char *old_lname = NULL;

	const char *new_desc = fe->descriptor_version_number();
	const char *old_desc = NULL;

	FileType    new_type = fe->file_type ();
	FileType    old_type = (FileType) 0;

	FileEntry  *pred_fe = NULL;

	if (parent) {
	    Return_if_fail (pred_fe << parent->match_fileent (fe));
	}

	if (! pred_fe) {
	    print_header (project, project_data, parent_data);
	    prcsoutput << "    " << new_name << setcol(COL) << " added "
		       << format_type (new_type, false) << prcsendl;
	} else {

	    bool rename  = false;
	    bool lchange = false;
	    bool vchange = false;
	    bool tchange = false;

	    old_type  = pred_fe->file_type ();
	    old_name  = pred_fe->working_path ();
	    old_lname = pred_fe->link_name ();
	    old_desc  = pred_fe->descriptor_version_number ();

	    if (! pathname_equal (old_name, new_name)) {
		rename = true;
	    }

	    if (new_type != old_type) {
		tchange = true;
	    }

	    if (new_lname && old_lname && strcmp (new_lname, old_lname) != 0) {
		lchange = true;
	    }

	    if (new_desc && old_desc && strcmp (new_desc, old_desc) != 0) {
		vchange = true;
	    }

	    if (rename || lchange || tchange || vchange) {
		print_header (project, project_data, parent_data);
		prcsoutput << "    " << new_name << setcol(COL) << " ";

		if (rename) {
		    prcsoutput << "renamed from " << old_name;
		}

		if (tchange) {
		    if (rename) { prcsoutput << ", "; }
		    prcsoutput << "type changed from " << format_type (old_type, false) << " to " << format_type (new_type, false);
		}

		if (lchange) {
		    if (rename || tchange) { prcsoutput << ", "; }
		    prcsoutput << "link changed from " << old_lname << " to " << new_lname;
		}

		if (vchange) {
		    if (rename || tchange || lchange) { prcsoutput << ", "; }

		    RcsVersionData *data;

		    Return_if_fail (data << rep_entry->lookup_rcs_file_data (fe->descriptor_name (),
									     fe->descriptor_version_number ()));

		    prcsoutput << "modified +" << data->plus_lines () << " -" << data->minus_lines ();
		}

		prcsoutput << prcsendl;
	    }
	}
    }

    if (parent) {
	foreach_fileent (fe_ptr, parent) {
	    FileEntry  *fe = *fe_ptr;
	    FileEntry  *proj_fe = NULL;

	    if (! fe->on_command_line ())
		continue;

	    Return_if_fail (proj_fe << project->match_fileent (fe));

	    if (! proj_fe) {
		print_header (project, project_data, parent_data);
		prcsoutput << "    " << fe->working_path () << setcol(COL) << " deleted "
			   << format_type (fe->file_type (), false) << prcsendl;
	    }
	}
    }

    return NoError;
}

PrVoidError output_changes (RepEntry *rep_entry, ProjectVersionData *project_data, ProjectVersionData *parent_data)
{
    ProjectDescriptor *project_desc = NULL, *parent_desc = NULL;

    Return_if_fail (get_projects (rep_entry, project_data, parent_data, & project_desc, & parent_desc));

    Return_if_fail (print_changes (rep_entry, project_data, parent_data, project_desc, parent_desc));

    return NoError;
}

PrPrcsExitStatusError changes_command()
{
    RepEntry *rep_entry;
    ProjectVersionData *first_data;
    ProjectVersionData *second_data;
    ProjectVersionDataPtrArray *ancestors;

    kill_prefix(prcsoutput);

    Return_if_fail(rep_entry << Rep_init_repository_entry(cmd_root_project_name,
							  false, false, true));

    if (strcmp (cmd_version_specifier_minor, "0") == 0) {
	pthrow prcserror << "You may not specify the 0-minor version for this command" << dotendl;
    }

    Return_if_fail(first_data << resolve_version(cmd_version_specifier_major,
						 cmd_version_specifier_minor,
						 cmd_root_project_full_name,
						 cmd_root_project_file_path,
						 NULL,
						 rep_entry));

    if (! cmd_alt_version_specifier_major) {
	second_data = first_data;
    } else {

	if (strcmp (cmd_alt_version_specifier_minor, "0") == 0) {
	    pthrow prcserror << "You may not specify the 0-minor version for this command" << dotendl;
	}

	Return_if_fail(second_data << resolve_version(cmd_alt_version_specifier_major,
						      cmd_alt_version_specifier_minor,
						      cmd_root_project_full_name,
						      cmd_root_project_file_path,
						      NULL,
						      rep_entry));
    }

    ancestors = rep_entry->common_lineage (first_data, second_data);

    if (ancestors == NULL) {
	pthrow prcserror << "Versions " << first_data << " and "
			 << second_data << " are not ancestors of each other" << dotendl;
    }

    ProjectVersionDataPtrArray* summary = rep_entry->project_summary();

    foreach (pvd_ptr, ancestors, ProjectVersionDataPtrArray::ArrayIterator) {

	ProjectVersionData *pvd = *pvd_ptr;

	print_project = NULL;
	print_parent = NULL;

	if (pvd->parent_count () == 0) {
	    Return_if_fail (output_changes (rep_entry, pvd, NULL));
	} else {
	    for (int i = 0; i < pvd->parent_count (); i += 1) {
		print_parent = NULL;
		Return_if_fail (output_changes (rep_entry, pvd, summary->index (pvd->parent_index (i))));
	    }
	}
    }

    return ExitSuccess;
}
