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
#include "checkout.h"
#include "checkin.h"
#include "projdesc.h"
#include "fileent.h"
#include "repository.h"
#include "misc.h"
#include "memseg.h"
#include "setkeys.h"
#include "system.h"

static PrBoolError rekey_file(FileEntry* fe, ProjectDescriptor* project);

PrPrcsExitStatusError rekey_command()
{
    ProjectDescriptor *project;

    Return_if_fail(project << read_project_file(cmd_root_project_full_name,
						cmd_root_project_file_path,
						true,
						KeepNothing));

    Return_if_fail(project->init_repository_entry(cmd_root_project_name, false, false));

    eliminate_unnamed_files(project);

    Return_if_fail(eliminate_working_files(project, NoQueryUserRemoveFromCommandLine));

    Return_if_fail(warn_unused_files(true));

    prcsinfo << "Setting keys in project " << squote(cmd_root_project_name)
	     << " for version " << project->full_version() << dotendl;

    int processed = 0;

    foreach_fileent(fe_ptr, project) {
	FileEntry* fe = *fe_ptr;
	bool replaced_keys;

	if(fe->file_type() != RealFile ||
	   !fe->keyword_sub() ||
	   !fe->on_command_line() ||
	   !fe->descriptor_name())
	    continue;

	If_fail(fe->stat())
	    pthrow prcserror << "Stat failed on " << squote(fe->working_path()) << perror;

	Return_if_fail(replaced_keys << rekey_file(fe, project));

	if(replaced_keys) {
	    Return_if_fail(fe->chmod(fe->stat_permissions()));

	    processed += 1;
	}
    }

    if(option_long_format && !option_report_actions)
	prcsoutput << processed << " files rekeyed" << dotendl;

    return ExitSuccess;
}

static MemorySegment setkeys_buffer(false);

static PrBoolError rekey_file(FileEntry* fe, ProjectDescriptor* project)
{
    RepEntry* rep_entry = project->repository_entry();
    RcsVersionData* version_data;
    bool changed;
    const char* filename = fe->working_path();

    Return_if_fail(fe->get_repository_info(rep_entry));

    Return_if_fail(version_data << rep_entry->lookup_rcs_file_data(fe->descriptor_name(),
								   fe->descriptor_version_number()));

    Return_if_fail(changed << setkeys_outbuf(filename,
					     &setkeys_buffer,
					     fe,
					     Setkeys));

    if(!option_report_actions && changed) {

	WriteableFile outfile;

	Return_if_fail(outfile.open(filename));

	Return_if_fail(outfile.write(setkeys_buffer.segment(), setkeys_buffer.length()));

	Return_if_fail(outfile.close());
    }

    if(option_long_format && changed) {
	prcsinfo << (option_report_actions ? "Replace" : "Replaced")
		 << " keywords in file " << squote(fe->working_path()) << dotendl;
    }

    return changed;
}
