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
 * $Id: info.cc 1.11.1.1.1.15.1.3.1.17.2.6 Fri, 08 Feb 2002 18:02:20 -0800 jmacd $
 */
#include "fnmatch.h"

#include "prcs.h"
#include "projdesc.h"
#include "fileent.h"
#include "repository.h"
#include "vc.h"
#include "misc.h"
#include "checkout.h"
#include "checkin.h"

static PrVoidError print_info(ProjectVersionData* project_data, RepEntry *rep_entry);
static bool matches(const char* pat, const char* text);

static ProjectVersionData* create_pseudo_empty_version(ProjectVersionData *first)
{
    ProjectVersionData *it = new ProjectVersionData(-1);

    it->prcs_major(first->prcs_major());
    it->prcs_minor("0");
    it->date(first->date());
    it->author(first->author());

    return it;
}

static ProjectVersionDataPtrArray* info_array (ProjectVersionDataPtrArray* pvda,
					       const char* minpat)
{
    /* This implements the sorting of -r VERSION for the info command.
     * It only filters out versions in case the minor pattern is "0"
     * or "@", otherwise it just takes care of the order and leaves
     * the minor pattern matching for later.  */
    ProjectVersionDataPtrArray *ret = new ProjectVersionDataPtrArray;
    bool byversion = strcmp (option_sort_type, "version") == 0;
    bool bydate    = strcmp (option_sort_type, "date") == 0;
    bool iszero    = strcmp (minpat, "0") == 0;
    bool islast    = strcmp (minpat, "@") == 0;

    /* If any of these three variables are set, then we loop through
     * the PVDA array, which is sorted by checkin-time. */
    if (iszero || islast || byversion) {

	/* Copy is destroyed by the loop below */
	ProjectVersionDataPtrArray copy (*pvda);

	for (int i = 0; i < pvda->length(); i += 1) {

	    /* The inner loop sets array entries to NULL as it
	     * processes them.  The outer loop finds the first minor
	     * version for a given major. */
	    if (copy.index (i) != NULL) {
		ProjectVersionData *last = NULL;

		if (iszero) {
		    ret->append (create_pseudo_empty_version(pvda->index(i)));
		} 

		/* The second loop processes all the minor versions of
		 * the major version. */
		for (int j = i; j < pvda->length(); j += 1) {

		    if (strcmp (pvda->index(i)->prcs_major(),
				pvda->index(j)->prcs_major()) == 0) {

			/* islast and iszero are special, otherwise just insert
			 * this version into the resulting order. */
			if (!islast && !iszero) {
			    ret->append (pvda->index(j));
			}

			/* the "@" pattern only matches non-deleted versions. */
			if (islast && ! pvda->index (j)->deleted ()) {
			    last = pvda->index (j);
			}
			
			copy.index(j, (ProjectVersionData*)0);
		    }
		}
		    
		if (islast && (last != NULL)) {
		    ret->append (last);
		}
	    }
	}

    } else {
	ASSERT (bydate, "read_command_line checked this");
	ret->assign (*pvda);
    }

    return ret;
}

PrPrcsExitStatusError info_command()
{
    bool first = true;
    RepEntry *rep_entry;
    const char* major_pat = cmd_version_specifier_major;
    const char* minor_pat = cmd_version_specifier_minor;

    kill_prefix(prcsoutput);

    Return_if_fail(rep_entry << Rep_init_repository_entry(cmd_root_project_name,
							  false, false, true));

    if(!major_pat[0] || !minor_pat[0]) {
	ProjectDescriptor* project;

	If_fail(project << read_project_file(cmd_root_project_full_name,
					     cmd_root_project_file_path,
					     true,
					     KeepNothing))
	    pthrow prcserror << "Failed reading project file, can't resolve "
	       "null version" << dotendl;

	if(!major_pat[0])
	    major_pat = *project->project_version_major();

	if(!minor_pat[0])
	    minor_pat = *project->project_version_minor();
    }

    ProjectVersionDataPtrArray* project_data_array;

    project_data_array = info_array (rep_entry->project_summary(), minor_pat);

    if (strcmp (minor_pat, "@") == 0)
	minor_pat = "*"; /* since info_array set the versions to only these, its
			  * okay.  otherwise, it won't match. */

    for (int i = 0; i < project_data_array->length(); i += 1) {
	if (matches (major_pat, project_data_array->index(i)->prcs_major()) &&
	    matches (minor_pat, project_data_array->index(i)->prcs_minor())) {

	    if (first)
		first = false;
	    else if (option_long_format)
		prcsoutput << prcsendl;

	    Return_if_fail(print_info(project_data_array->index(i), rep_entry));
	}
    }

    if(first) {
        prcswarning << "No versions match "
		    << (cmd_version_specifier_major[0] == '\0' ? "(null)" : major_pat)
		    << '.'
		    << (cmd_version_specifier_minor[0] == '\0' ? "(null)" : minor_pat)
		    << dotendl;
    }

    delete project_data_array;

    return ExitSuccess;
}

static bool matches(const char* pat, const char* text)
{
    return fnmatch(pat, text, FNM_NOESCAPE) == 0;
}

static PrVoidError print_info(ProjectVersionData* project_data, RepEntry *rep_entry)
{
    /* Up until version 1.3.1, print_info returned without printing anything for
     * deleted versions. */
    ProjectDescriptor *P = NULL;
    const PrcsAttrs *last_attrs = NULL;

    if(option_long_format || option_really_long_format) {
        if(project_data->rcs_version()) {
	    Return_if_fail(P << rep_entry->checkout_prj_file(cmd_root_project_full_name,
							     project_data->rcs_version(),
							     KeepNothing));
        }
	/* Otherwise its an empty version... return later... (P == NULL) */
    }

    prcsoutput << cmd_root_project_name << ' '
	       << project_data << ' '
	       << time_t_to_rfc822(project_data->date()) << " by "
	       << project_data->author()
	       << (project_data->deleted () ? " *DELETED*" : "")
	       << prcsendl;


    if (P == NULL) {
	return NoError;
    }

    if(option_long_format) {

        static Dstring *desc = NULL;

	DEBUG("Index:" << setcol(21) << project_data->version_index());
	DEBUG("Parent count: " << project_data->parent_count());

	for (int i = 0; i < project_data->parent_count(); i += 1) {

	    DEBUG("Parent-Index:" << setcol(21) << project_data->parent_index(i));

	    ProjectVersionData* par_data = rep_entry -> project_summary() ->
		                           index (project_data->parent_index(i));

	    prcsoutput << "Parent-Version:" << setcol(21) << par_data << prcsendl;
	}

	if(strchr(*P->version_log(), '\n') != NULL || strlen(*P->version_log()) > 60) {
	    prcsoutput << "Version-Log:" << prcsendl
		       << P->version_log() << prcsendl;
	} else {
	    prcsoutput << "Version-Log:" << setcol(21)
		       << (P->version_log()->index(0) == '\0' ? "empty" : P->version_log()->cast())
		       << prcsendl;
	}

        if( (desc && strcmp(*desc, *P->project_description()) != 0) ||
	    !desc || P->project_description()->length() == 0) {

            if(strchr(*P->project_description(), '\n') != NULL ||
	       strlen(*P->project_description()) > 60) {
                prcsoutput << "Project-Description:" << prcsendl
			   << P->project_description() << prcsendl;
            } else {
                prcsoutput << "Project-Description:" << setcol(21)
			   << (P->project_description()->index(0) == '\0' ? "empty" : P->project_description()->cast()) << prcsendl;
            }
        } else {
            prcsoutput << "Project-Description:" << setcol(21) << "as above" << prcsendl;
        }

	if (desc == NULL)
	    desc = new Dstring;

	desc->assign (*P->project_description());

	int ks = P->project_keywords_extra()->key_array()->length();

	if (ks > 0)
	  prcsoutput << "Project-Keywords:" << prcsendl;

	for (int i = 0; i < ks; i += 1) {
	  prcsoutput << "  " << P->project_keywords_extra()->key_array()->index(i)
		     << setcol(21)
		     << P->project_keywords_extra()->data_array()->index(i) << prcsendl;
	}
    }

    /* Can't print deleted file info because the descriptors have
     * possibly been reused--we don't know all their attributes any
     * longer. */
    if(option_really_long_format && !project_data->deleted ()) {

        if(P->file_entries()->length() == 0) {
            prcsoutput << "No working files" << dotendl;
        } else {
            FileType type;
            int width = 0;

	    eliminate_unnamed_files(P);

	    foreach_fileent(fe_ptr, P) {
		FileEntry* fe = *fe_ptr;

		if (!fe->on_command_line())
		    continue;

                int thiswidth = strlen(fe->working_path());

                if (thiswidth > width)
                    width = thiswidth;
            }

            if(width < 21)
                width = 21;

	    foreach_fileent(fe_ptr2, P) {
		FileEntry* fe = *fe_ptr2;

		if (!fe->on_command_line())
		    continue;

                type = fe->file_type();

                prcsoutput << fe->working_path()
			   << setcol(width);

		if (fe->file_type() == RealFile && fe->descriptor_name()) {
		    RcsVersionData* version_data;

		    Return_if_fail(version_data << rep_entry->
				   lookup_rcs_file_data(fe->descriptor_name(),
							fe->descriptor_version_number()));

		    prcsoutput << " MD5=";

		    for(int i = 0; i < 16; i += 1) {
			char buf[8];
			sprintf(buf, "%02x", 0xff & version_data->unkeyed_checksum()[i]);
			prcsoutput << buf;
                    }
		}

		/* @@@ Note: this doesn't print group attributes. */
		if (last_attrs == fe->file_attrs()) {
		    if (last_attrs->nprint () > 0) {
			prcsoutput << " attributes as above";
		    }
		} else {
		    fe->file_attrs()->print (prcsoutput, true);
		    last_attrs = fe->file_attrs();
		}

		prcsoutput << prcsendl;
            }
        }
    }

    if (P) delete P;

    return NoError;
}
