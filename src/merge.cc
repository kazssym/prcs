/* -*-Mode: C++;-*-
 * PRCS - The Project Revision Control System
 * Copyright (C) 1997  Josh MacDonald
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) an later version.
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

extern "C" {
#include <fcntl.h>
}

#include "prcs.h"
#include "hash.h"
#include "diff.h"
#include "populate.h"
#include "checkin.h"
#include "checkout.h"
#include "repository.h"
#include "projdesc.h"
#include "vc.h"
#include "syscmd.h"
#include "setkeys.h"
#include "fileent.h"
#include "misc.h"
#include "system.h"

struct MergeDef {
    bool         have_working;
    bool         have_selected;
    bool         have_common;
    bool         ws_cmp;
    bool         wc_cmp;
    bool         sc_cmp;
    int          rule_no;
    const char  *help_string;
};

struct MergeCo {
    MergeCo () :working(NULL), selected(NULL), common(NULL) { }

    const char* working;
    const char* selected;
    const char* common;
};

class Mergeable; /* This is an opaque type.  It doesn't exist.  I'm
		  * going to cheat on type-correctness with it. */

struct MergeControl {
protected:

    ProjectDescriptor* project;

    PrVoidError         run_merge (Mergeable *,
				   Mergeable *,
				   Mergeable *);

    virtual omanip<FileTuple> print3 (Mergeable* sel, Mergeable* com, Mergeable* work) = 0;

private:

    virtual MergeAction merge_action_by_rule (Mergeable* m, int rule) = 0;
    virtual const char* merge_desc   (Mergeable* m, int rule) = 0;
    virtual PrBoolError quick_cmp    (Mergeable* sel, Mergeable* com, Mergeable* work, bool* positive) = 0;
    virtual PrVoidError prepare      (Mergeable* selected, const char* sel_name,
				      Mergeable* common, const char* com_name,
				      Mergeable* working, const char* work_name,
				      MergeCo* co) = 0;
    virtual bool        can_add      (Mergeable* file) = 0;
    virtual bool        will_3way    (Mergeable* file) = 0;
    virtual bool        can_view     () = 0;
    virtual PrVoidError view         (Mergeable* sel, Mergeable* com, Mergeable* work, MergeCo* co) = 0;
    virtual PrBoolError mergef       (Mergeable*, Mergeable*, Mergeable*, MergeCo*) = 0;
    virtual PrVoidError addf         (Mergeable*, Mergeable*, Mergeable*, MergeCo*) = 0;
    virtual PrVoidError deletef      (Mergeable*, Mergeable*, Mergeable*, MergeCo*) = 0;
    virtual PrVoidError replacef     (Mergeable*, Mergeable*, Mergeable*, MergeCo*) = 0;
    virtual void        notify       (Mergeable*, Mergeable*, Mergeable*, MergeAction) = 0;
    virtual const char* name         (Mergeable*) = 0;
    virtual const char* type         () = 0;

    void                merge_announce_os (Mergeable* selected,
					   Mergeable* common,
					   Mergeable* working,
					   ostream &os);
    void                merge_announce (Mergeable* selected,
					Mergeable* common,
					Mergeable* working,
					MergeAction action);
    void                merge_action_os (Mergeable* selected,
					 Mergeable* common,
					 Mergeable* working,
					 MergeAction action,
					 int ruleno,
					 bool conflict,
					 ostream &os);

    void                merge_action (Mergeable* selected,
				      Mergeable* common,
				      Mergeable* working,
				      MergeAction action,
				      int ruleno,
				      bool conflict);

    const char*         file_merge_desc (Mergeable* selected,
					 Mergeable* common,
					 Mergeable* working,
					 int rule);

    MergeAction         file_merge_action (Mergeable* selected,
					   Mergeable* common,
					   Mergeable* working,
					   int rule);

    PrVoidError         finish_merge (Mergeable* selected,
				      Mergeable* common,
				      Mergeable* working,
				      const MergeDef* mergedef,
				      MergeCo *co);

    int                 compact (const MergeDef** arr,
				 Mergeable* selected,
				 Mergeable* common,
				 Mergeable* working,
				 int len);

    void                print_merge_help(const char* valid);

};

static const char rule_1_help[] =
"All three files exist and are the same in each version";
static const char rule_2_help[] =
"All three files exist and are different in each version";
static const char rule_3_help[] =
"All three files exist and are alike in the working and common versions";
static const char rule_4_help[] =
"All three files exist and are alike in the selected and common versions";
static const char rule_5_help[] =
"All three files exist and are alike in the selected and working versions";
static const char rule_6_help[] =
"Selected and common files are alike; working file not present";
static const char rule_7_help[] =
"Working and common files are alike; selected file not present";
static const char rule_8_help[] =
"Working and selected files are alike; common file not present";
static const char rule_9_help[] =
"Selected and common files differ; working file not present";
static const char rule_10_help[] =
"Working and common files differ; selected file not present";
static const char rule_11_help[] =
"Working and selected files differ; common file not present";
static const char rule_12_help[] =
"Only the common file is present";
static const char rule_13_help[] =
"Only the working file is present";
static const char rule_14_help[] =
"Only the selected file is present";

static const char rule_2_def_desc[] =
"There are no equivalent files among the three files in each version.  "
"It is assumed that you are interested in merging the selected "
"version's changes with your changes.  Therefore, the default action is "
"merge";
static const char rule_3_def_desc[] =
"Your version of the file is unchanged from the common version, yet "
"the selected version has been modified.  It is assumed then, that the "
"selected version of the file is more up to date, and the default "
"action is to replace your file with the file from the selected "
"version";
static const char rule_6_def_desc[] =
"The file has been deleted from your working version of the project, "
"and still exists in the selected version.  The selected version has "
"not been modified, so it is assumed that the file is obsolete";
static const char rule_7_def_desc[] =
"The file has been deleted from the selected of the project, and still "
"exists in your version.  You have not modified the file, so your version "
"is assumed to be obsolete.  Therefore, the default action is delete";
static const char rule_9_def_desc[] =
"The file has been deleted from your working version of the project, "
"and still exists in the selected version.  The selected version has "
"been modified, but it is still assumed that the file is obsolete";
static const char rule_10_def_desc[] =
"The file has been deleted from the selected of the project, and still "
"exists in your version.  You have modified the file, so it is not clear "
"what to do.  The default action is delete";
static const char rule_11_def_desc[] =
"The file exists in both the selected version and your version, but "
"not in the common version.  The files differ.  The default action is "
"to merge, with an empty file used as the common version.  This is "
"likely to produce serious conflicts";
static const char rule_14_def_desc[] =
"The file exists only in the selected version.  Niether the common "
"version nor your working version of the project contain the file.  The "
"default action is to add the file";

extern const char *default_merge_descs[14] =
{
    NULL,
    rule_2_def_desc,
    rule_3_def_desc,
    NULL,
    NULL,
    rule_6_def_desc,
    rule_7_def_desc,
    NULL,
    rule_9_def_desc,
    rule_10_def_desc,
    rule_11_def_desc,
    NULL,
    NULL,
    rule_14_def_desc
};

extern const MergeAction default_merge_actions[14] =
{
    MergeActionNoPrompt,
    MergeActionMerge,
    MergeActionReplace,
    MergeActionNoPrompt,
    MergeActionNoPrompt,
    MergeActionNothing,
    MergeActionDelete,
    MergeActionNoPrompt,
    MergeActionNothing,
    MergeActionDelete,
    MergeActionMerge,
    MergeActionNoPrompt,
    MergeActionNoPrompt,
    MergeActionAdd
};

static const char incomplete_help_string[] =
"Before proceeding, you must decide whether to consider the incompletely "
"merged against version a parent or not.  If you do, it will be used later "
"in the search for common ancestors";

#define ARRAY_LEN(x) ((int)(sizeof(x)/sizeof(x[0])))

static const MergeDef merge_defaults[14] =
{
    /* W     S      C      WS     WC     SC */
    /* just 1 */
    { false, false, true,  false, false, false, 12, rule_12_help },
    { true,  false, false, false, false, false, 13, rule_13_help },
    { false, true,  false, false, false, false, 14, rule_14_help },
    /* same 2 */
    { false, true,  true,  false, false, false, 6,  rule_6_help },
    { true,  false, true,  false, false, false, 7,  rule_7_help },
    { true,  true,  false, false, false, false, 8,  rule_8_help },
    /* diff 2 */
    { false, true,  true,  false, false, true,  9,  rule_9_help },
    { true,  false, true,  false, true,  false, 10, rule_10_help },
    { true,  true,  false, true,  false, false, 11, rule_11_help },
    /* all */
    { true,  true,  true,  false, false, false, 1,  rule_1_help },
    { true,  true,  true,  true,  true,  true,  2,  rule_2_help },
    { true,  true,  true,  true,  false, true,  3,  rule_3_help },
    { true,  true,  true,  true,  true,  false, 4,  rule_4_help },
    { true,  true,  true,  false, true,  true,  5,  rule_5_help }
};

#define temp_file_working temp_file_1
#define temp_file_common temp_file_2
#define temp_file_selected temp_file_3

static bool         any_obsolete = false;
static CharPtrArray all_conflicts;

static PrVoidError checkout_unkey (FileEntry* selected, const char* sel_name,
				   FileEntry* common, const char* com_name,
				   FileEntry* working, const char* work_name,
				   MergeCo* co);
static PrVoidError                 run_merge_rename         (FileEntry* selected,
							     FileEntry* common,
							     FileEntry* working);
static PrVoidError                 view_merge_diffs         (FileEntry*,
							     FileEntry*,
							     FileEntry*,
							     MergeCo*);
static PrVoidError                 merge_common_files       (ProjectDescriptor*,
							     ProjectDescriptor*,
							     ProjectDescriptor*);
static PrVoidError                 merge_projects           (ProjectDescriptor*,
							     ProjectDescriptor*,
							     ProjectDescriptor*);
static PrVoidError                 merge_selected_only_files(ProjectDescriptor*,
							     ProjectDescriptor*,
							     ProjectDescriptor*);
static PrVoidError                 merge_working_only_files (ProjectDescriptor*,
							     ProjectDescriptor*,
							     ProjectDescriptor*);
static PrBoolError                 file_quick_cmp           (FileEntry* selected,
							     FileEntry* common,
							     FileEntry* working,
							     bool*      positive);
static PrBoolError                 merge_files             (ProjectDescriptor*,
							    FileEntry*,
							    FileEntry*,
							    FileEntry*,
							    MergeCo*);
static PrVoidError                 replace_file            (ProjectDescriptor*,
							    FileEntry*,
							    FileEntry*,
							    FileEntry*,
							    MergeCo*);
static PrVoidError                 add_file                (ProjectDescriptor*,
							    FileEntry*,
							    FileEntry*,
							    FileEntry*,
							    MergeCo*);
static PrVoidError                 delete_file             (ProjectDescriptor*,
							    FileEntry*,
							    FileEntry*,
							    FileEntry*,
							    MergeCo*);
static PrVoidError                 merge_cleanup_handler   (void* data, bool);
static PrProjectDescriptorPtrError checkout_common_version (ProjectVersionData*,
							    ProjectVersionData*,
							    ProjectDescriptor*,
							    RepEntry* rep_entry);
static PrVoidError                 make_obsolete           (FileEntry *fe,
							    ProjectDescriptor*,
							    bool force_copy);
static PrBoolError                 remerge_files           (FileEntry *work,
							    FileEntry *common,
							    FileEntry *selected);
static bool                        any_names_differ        (FileEntry* working,
							    FileEntry* common,
							    FileEntry* selected);
static bool                        merge_help_is_complete  (MergeParentEntryPtrArray* entries,
							    const char* selected_version);
static PrVoidError                 merge_help_query_complete (void);


/**********************************************************************/
/*		      FileEntry merge controller                      */
/**********************************************************************/

struct FileMergeControl : public MergeControl {

#define CAST(m) ((FileEntry*)m)

public:

    FileMergeControl (ProjectDescriptor *proj) { project = proj; }

    PrVoidError file_run_merge (FileEntry *sel,
				FileEntry *com,
				FileEntry *work)
    {
	return run_merge ((Mergeable*)sel, (Mergeable*)com, (Mergeable*)work);
    }

protected:

    virtual omanip<FileTuple> print3 (Mergeable* sel, Mergeable* com, Mergeable* work)
    {
	return omanip<FileTuple>(__omanip_filetuple,
				 FileTuple(CAST(work), CAST(com), CAST(sel), FileTuple::MergeTriple));
    }

private:

    virtual MergeAction merge_action_by_rule (Mergeable* m, int rule) {
	return CAST(m)->file_attrs()->merge_action(rule - 1);
    }

    virtual const char* merge_desc   (Mergeable* m, int rule)
    {
	return CAST(m)->file_attrs()->merge_desc(rule - 1);
    }

    virtual PrBoolError quick_cmp    (Mergeable* sel, Mergeable* com, Mergeable* work, bool* positive)
    {
	return file_quick_cmp (CAST(sel), CAST(com), CAST(work), positive);
    }

    virtual PrVoidError prepare      (Mergeable* selected, const char* sel_name,
				      Mergeable* common, const char* com_name,
				      Mergeable* working, const char* work_name,
				      MergeCo* co)
    {
	return checkout_unkey (CAST(selected), sel_name,
			       CAST(common), com_name,
			       CAST(working), work_name,
			       co);
    }

    virtual bool        can_add      (Mergeable* file)
    {
	return project->can_add (CAST(file));
    }

    virtual bool        will_3way    (Mergeable* file)
    {
	return CAST(file)->file_type() == RealFile;
    }

    virtual bool        can_view     ()
    {
	return true;
    }

    virtual PrVoidError view (Mergeable* sel, Mergeable* com, Mergeable* work, MergeCo* co)
    {
	return view_merge_diffs (CAST(sel), CAST(com), CAST(work), co);
    }

    virtual PrBoolError mergef       (Mergeable* sel, Mergeable* com, Mergeable* work, MergeCo* co)
    {
	return merge_files (project, CAST(sel), CAST(com), CAST(work), co);
    }

    virtual PrVoidError addf       (Mergeable* sel, Mergeable* com, Mergeable* work, MergeCo* co)
    {
	return add_file (project, CAST(sel), CAST(com), CAST(work), co);
    }

    virtual PrVoidError deletef       (Mergeable* sel, Mergeable* com, Mergeable* work, MergeCo* co)
    {
	return delete_file (project, CAST(sel), CAST(com), CAST(work), co);
    }

    virtual PrVoidError replacef       (Mergeable* sel, Mergeable* com, Mergeable* work, MergeCo* co)
    {
	return replace_file (project, CAST(sel), CAST(com), CAST(work), co);
    }

    virtual void        notify       (Mergeable* sel, Mergeable* com, Mergeable* work, MergeAction act)
    {
	project->merge_notify (CAST(sel), CAST(com), CAST(work), act);
    }

    virtual const char* name         (Mergeable* file)
    {
	return CAST(file)->working_path();
    }

    virtual const char* type         ()
    {
	return "file";
    }
};

/**********************************************************************/
/*		      Populate merge controller                       */
/**********************************************************************/

typedef Pair<const char*, const char*> StringPair;

#undef CAST
#define CAST(m) ((StringPair*)m)

ostream& __omanip_popkey(ostream& o, FileTuple tup)
{
    FileEntry* non_null;

    if(tup.work_fe)
	non_null = tup.work_fe;
    else if (tup.sel_fe)
	non_null = tup.sel_fe;
    else
	non_null = tup.com_fe;

    o << squote (CAST(non_null)->x());

    return o;
}

struct KeyPopMergeControl : public MergeControl {

public:

    KeyPopMergeControl (ProjectDescriptor *proj) { project = proj; }

    PrVoidError kp_run_merge (StringPair *sel,
			      StringPair *com,
			      StringPair *work)
    {
	return run_merge ((Mergeable*)sel, (Mergeable*)com, (Mergeable*)work);
    }

protected:

    virtual omanip<FileTuple> print3 (Mergeable* sel, Mergeable* com, Mergeable* work)
    {
	return omanip<FileTuple>(__omanip_popkey, FileTuple((FileEntry*)sel, (FileEntry*)com, (FileEntry*)work, FileTuple::MergeTriple));
    }

private:

    virtual MergeAction merge_action_by_rule (Mergeable* , int rule) {
	return default_merge_actions[rule-1];
    }

    virtual const char* merge_desc   (Mergeable* , int rule)
    {
	return default_merge_descs[rule-1];
    }

    virtual PrBoolError quick_cmp    (Mergeable* sel, Mergeable* com, Mergeable* work, bool* positive)
    {
	*positive = true;
	return (sel && com && strcmp (CAST(sel)->y(), CAST(com)->y()) != 0) ||
	       (sel && work && strcmp (CAST(sel)->y(), CAST(work)->y()) != 0) ||
	       (com && work && strcmp (CAST(com)->y(), CAST(work)->y()) != 0);
    }

    virtual PrBoolError mergef       (Mergeable* , Mergeable* , Mergeable* , MergeCo*) {abort (); /* NOTREACHED */ return PrBoolError(FatalError); }
    virtual bool        can_add (Mergeable* ) { return true; }
    virtual bool        will_3way (Mergeable* ) { return false; }
    virtual bool        can_view () { return false; }
    virtual PrVoidError view (Mergeable* , Mergeable* , Mergeable* , MergeCo* ) { abort (); /* NOTREACHED */ return PrVoidError(FatalError); }
    virtual PrVoidError prepare (Mergeable*, const char*, Mergeable*, const char*, Mergeable*, const char*, MergeCo*) {abort (); /* NOTREACHED */ return PrVoidError(FatalError); }
    virtual void        notify       (Mergeable* , Mergeable* , Mergeable* , MergeAction ) { }
    virtual const char* name         (Mergeable* file) { return CAST(file)->x(); }

};

/**********************************************************************/
/*		      Populate merge controller                       */
/**********************************************************************/

struct PopulateMergeControl : public KeyPopMergeControl {

public:

    PopulateMergeControl (ProjectDescriptor *proj) :KeyPopMergeControl (proj) { }

private:

    virtual PrVoidError addf       (Mergeable* sel, Mergeable* , Mergeable* , MergeCo*)
    {
	ASSERT (sel, "must have a selected file to add");
	project->add_populate_ignore (CAST(sel)->x());
	return NoError;
    }

    virtual PrVoidError deletef       (Mergeable* , Mergeable* , Mergeable* work, MergeCo*)
    {
	ASSERT (work, "must have a working file to remove");
	project->rem_populate_ignore (CAST(work)->x());
	return NoError;
    }

    /* since these are by name, not value, they will always be the same */
    virtual PrVoidError replacef       (Mergeable* , Mergeable* , Mergeable* , MergeCo*) { abort(); /* NOTREACHED */ return PrVoidError(FatalError); }

    virtual const char* type         ()
    {
	return "populate ignore element";
    }
};

/**********************************************************************/
/*		      Keywords merge controller                       */
/**********************************************************************/

struct KeywordMergeControl : public KeyPopMergeControl {

public:

    KeywordMergeControl (ProjectDescriptor *proj) :KeyPopMergeControl (proj) { }

private:

    virtual PrVoidError addf       (Mergeable* sel, Mergeable* , Mergeable* , MergeCo*)
    {
	ASSERT (sel, "must have a selected file to add");
	project->add_keyword (CAST(sel)->x(), CAST(sel)->y());
	return NoError;
    }

    virtual PrVoidError deletef       (Mergeable* , Mergeable* , Mergeable* work, MergeCo*)
    {
	ASSERT (work, "must have a working file to remove");
	project->rem_keyword (CAST(work)->x());
	return NoError;
    }

    virtual PrVoidError replacef       (Mergeable* sel, Mergeable* , Mergeable* , MergeCo*)
    {
	ASSERT (sel, "must have a selected file to replace");
	project->set_keyword (CAST(sel)->x(), CAST(sel)->y());
	return NoError;
    }

    virtual const char* type         ()
    {
	return "project keyword";
    }
};

/**********************************************************************/
/*		      All merge controller decls                      */
/**********************************************************************/

static FileMergeControl* file_entry_control = NULL;
static PopulateMergeControl* populate_control = NULL;
static KeywordMergeControl* keyword_control = NULL;

/**********************************************************************/
/*			     Main Command                             */
/**********************************************************************/

static PrPrcsExitStatusError merge_command2()
{
    ProjectDescriptor *working, *selected, *common;
    ProjectVersionData *selected_version, *pred_project_data;
    RepEntry* rep_entry;
    bool cont;

    Return_if_fail(working << read_project_file(cmd_root_project_full_name,
						cmd_root_project_file_path,
						true,
						(ProjectReadData)(KeepMergeParents)));

    Return_if_fail(working->init_repository_entry(cmd_root_project_name, false, false));

    rep_entry = working->repository_entry();

    Return_if_fail(working->verify_merge_parents());

    pred_project_data = rep_entry->lookup_version(working);

    if(!pred_project_data)
	pthrow prcserror << "Invalid version in working project file, cannot merge" << dotendl;

    eliminate_unnamed_files(working);

    Return_if_fail(eliminate_working_files(working, SetNotPresentWarnUser));

    working->read_quick_elim();
    working->quick_elim_unmodified();

    Return_if_fail(selected_version << resolve_version(cmd_version_specifier_major,
						       cmd_version_specifier_minor,
						       cmd_root_project_full_name,
						       cmd_root_project_file_path,
						       working,
						       rep_entry));

    if(selected_version->prcs_minor_int() == 0) {
 	Return_if_fail(selected << checkout_empty_prj_file(cmd_root_project_full_name,
							   selected_version->prcs_major(),
							   (ProjectReadData)(KeepMergeParents)));
    } else {
	Return_if_fail(selected << rep_entry->checkout_prj_file(cmd_root_project_full_name,
								selected_version->rcs_version(),
								(ProjectReadData)(KeepMergeParents)));
    }

    Return_if_fail(common << checkout_common_version(pred_project_data, selected_version,
						     working, rep_entry));

    if (!common) { /* merge is complete. */
	prcsinfo << "Selected version has already been completely merged against"
		 << dotendl;
	return ExitSuccess;
    }

    eliminate_unnamed_files(selected);
    eliminate_unnamed_files(common);

    common->repository_entry(working->repository_entry());
    selected->repository_entry(working->repository_entry());

    Return_if_fail(warn_unused_files(true));

    /* Tell the project file. */
    cont = working->merge_continuing (selected_version);

    /* Tell the user what's up. */
    prcsinfo << "Working version:  " << working->full_version() << prcsendl;
    prcsinfo << "Common version:   " << common->full_version() << prcsendl;
    prcsinfo << "Selected version: " << selected->full_version() << prcsendl;

    if (cont)
	prcsinfo << "Continuing in progress merge" << dotendl;

    /* Tell the log what's up */
    Return_if_fail(working->init_merge_log());
    common->merge_log(working->merge_log());
    selected->merge_log(working->merge_log());

    working->merge_log() << prcsendl << "*** "
			 << (cont ? "Continuuing" : "Beginning")
			 << " merge of project "
			 << working->project_name() << prcsendl
			 << prcsendl
			 << "User:             " << get_login() << prcsendl
			 << "Date:             " << get_utc_time() << prcsendl
			 << "Common version:   " << common->full_version() << prcsendl
			 << "Selected version: " << selected->full_version() << prcsendl
			 << "Working version:  " << working->full_version() << prcsendl;

    if (option_skilled_merge)
	working->merge_log() << "Option --skilled-merge supplied" << prcsendl;

    if (option_force_resolution)
	working->merge_log() << "Option --force supplied" << prcsendl;

    if (option_report_actions)
	working->merge_log() << "Option --no-action supplied" << prcsendl;

    working->merge_log() << prcsendl;

    foreach (ent_ptr, working->new_merge_parents(), MergeParentEntryPtrArray::ArrayIterator) {
	MergeParentEntry *ent = *ent_ptr;

	if (ent->state & MergeStateParent)
	    working->merge_log() << "Working parent:   " << (*ent_ptr)->selected_version << prcsendl;
    }

    /* Do it. */
    install_cleanup_handler (merge_cleanup_handler, working);

    file_entry_control = new FileMergeControl (working);
    keyword_control    = new KeywordMergeControl (working);
    populate_control   = new PopulateMergeControl (working);

    Return_if_fail (merge_common_files(selected, common, working));
    Return_if_fail (merge_selected_only_files(selected, common, working));
    Return_if_fail (merge_working_only_files(selected, common, working));
    Return_if_fail (merge_projects(selected, common, working));

    working->merge_notify_complete();

    for (int i = 0; i < all_conflicts.length (); i += 1) {
	prcsoutput << "File " << squote (all_conflicts.index (i)) << " has conflicts which you must edit" << dotendl;
    }

    /* Note: merged project file is written out by merge_cleanup_handler */

    return ExitSuccess;
}

PrPrcsExitStatusError merge_command()
{
    PrPrcsExitStatusError ret = merge_command2 ();

    if (Failure (ret)) {

	if (any_obsolete) {
	    prcserror << "Merge failed: check `obsolete' subdirectories for saved originals" << dotendl;
	}
    }

    return ret;
}

static PrVoidError merge_cleanup_handler(void* data, bool)
{
    if(option_report_actions)
	return NoError;

    ProjectDescriptor* merged = ((ProjectDescriptor*)data);

    merged->merge_finish();

    Return_if_fail(merged->write_project_file(cmd_root_project_file_path));

    return NoError;
}

static PrProjectDescriptorPtrError checkout_common_version(ProjectVersionData *pred_version_data,
							   ProjectVersionData *selected_version_data,
							   ProjectDescriptor  *working_project,
							   RepEntry           *rep_entry)
{
    MergeParentEntryPtrArray* merge_parents = working_project->new_merge_parents();
    Dstring selected_version;
    ProjectVersionDataPtrArray *ancestors;
    ProjectVersionData working_project_data(-1);
    ProjectVersionData *common_version_data = NULL;

    selected_version.assign (selected_version_data->prcs_major());
    selected_version.append (".");
    selected_version.append (selected_version_data->prcs_minor());

    working_project_data.prcs_major("no major");
    working_project_data.prcs_minor("666");
    working_project_data.new_parent(pred_version_data->version_index());

    if (merge_parents->length() != 0) {

	if (merge_parents->last_index()->state & MergeStateIncomplete &&
	    strcmp (merge_parents->last_index()->selected_version, selected_version) != 0) {
	    /* Last merge is incomplete, ask to abort this one and finish last one. */

	    Return_if_fail (merge_help_query_incomplete(merge_parents->last_index()));
	}

	if (merge_help_is_complete(merge_parents, selected_version)) {
	    /* Merge is complete already, if skilled ask to continue, otherwise
	     * just exit. */

	    if (option_skilled_merge) {
		Return_if_fail (merge_help_query_complete());
	    } else {
		return (ProjectDescriptor*) 0;
	    }
	}

	/* Add in any merge parents. */
	foreach (ent_ptr, working_project->new_merge_parents(), MergeParentEntryPtrArray::ArrayIterator) {
	    MergeParentEntry *ent = *ent_ptr;

	    if (ent->state & MergeStateParent)
		working_project_data.new_parent(ent->project_data->version_index());
	}
    }

    ancestors = rep_entry->common_version (&working_project_data, selected_version_data);

    if (ancestors->length() == 1) {
	common_version_data = ancestors->index(0);
    } else if (ancestors->length() == 0) {
	prcsquery << "There is no common ancestor, you may use the empty version "
		  << pred_version_data->prcs_major() << ".0"
		  << ", but you had better know what you are doing.  "
		  << force ("Aborting")
		  << report ("Abort")
		  << option ('n', "Continue using empty version")
		  << deffail ('y')
		  << query ("Abort");

	Return_if_fail (prcsquery.result());

	return checkout_empty_prj_file(working_project->project_full_name(),
				       pred_version_data->prcs_major(),
				       (ProjectReadData)(KeepMergeParents));
    } else {
	prcsquery << "There is no unique common ancestor between the working project and "
		  << selected_version_data << dotendl;

	Dstring def_com_ver;

	def_com_ver.assign (ancestors->index(0)->prcs_major());
	def_com_ver.append (".");
	def_com_ver.append (ancestors->index(0)->prcs_minor());

	while (!common_version_data) {
	    prcsinfo << "Possible common ancestors:" << prcsendl;

	    foreach (ent_ptr, ancestors, ProjectVersionDataPtrArray::ArrayIterator)
		prcsinfo << *ent_ptr << prcsendl;

	    prcsquery << definput (def_com_ver)
		      << string_query ("Please select a common ancestor");

	    const char* result;

	    Return_if_fail (result << prcsquery.string_result());

	    const char* result_major = major_version_of (result);
	    const char* result_minor = minor_version_of (result);

	    if (!result_major) {
		prcsinfo << "Illegal version name" << dotendl;
		continue;
	    }

	    foreach (ent_ptr, ancestors, ProjectVersionDataPtrArray::ArrayIterator) {
		ProjectVersionData *ent = *ent_ptr;

		if (strcmp (result_major, ent->prcs_major()) == 0 &&
		    strcmp (result_minor, ent->prcs_minor()) == 0) {

		    common_version_data = ent;

		    break;
		}
	    }
	}
    }

    if (common_version_data->deleted())
	pthrow prcserror << "The common ancestor between the working project and "
			<< selected_version_data << " has been deleted" << dotendl;

    return rep_entry->checkout_prj_file(working_project->project_full_name(),
					common_version_data->rcs_version(),
					(ProjectReadData)(KeepMergeParents));
}

static bool merge_help_is_complete(MergeParentEntryPtrArray* entries, const char* selected_version)
{
    foreach (ent_ptr, entries, MergeParentEntryPtrArray::ArrayIterator) {
	MergeParentEntry *entry = *ent_ptr;

	if (!(entry->state & MergeStateIncomplete) &&
	    strcmp (entry->selected_version, selected_version) == 0)
	    return true;
    }

    return false;
}

static PrVoidError merge_help_query_complete()
{
    prcsquery << "Selected version has already been completely merged against.  "
	      << force ("Aborting")
	      << report ("Abort")
	      << option ('y', "Continue anyway, you better know what you're doing")
	      << deffail ('n')
	      << query ("Continue");

    Return_if_fail (prcsquery.result());

    return NoError;
}

PrVoidError merge_help_query_incomplete (MergeParentEntry* entry)
{
    char c;

    prcsquery << "Last merged against version, "
	      << entry->project_data
	      << ", was not completely merged against.  "
	      << force("Aborting")
	      << report("Abort")
	      << help(incomplete_help_string)
	      << option('p', "Continue with this merge, consider it a parent")
	      << option('c', "Continue with this merge, do not consider it a parent")
	      << deffail('n')
	      << query("Continue");

    Return_if_fail (c << prcsquery.result());

    if (c != 'c') {
	entry->state = MergeStateIncompleteParent;
    } else {
	entry->state = MergeStateIncomplete;
    }

    return NoError;
}

static PrVoidError merge_common_files(ProjectDescriptor* selected,
				      ProjectDescriptor* common,
				      ProjectDescriptor* working)
{
    FileEntry *common_fe, *selected_fe, *working_fe;

    foreach_fileent(fe_ptr, common) {
	common_fe = *fe_ptr;

	common->repository_entry()->Rep_clear_compressed_cache();
	working->repository_entry()->Rep_clear_compressed_cache();
	selected->repository_entry()->Rep_clear_compressed_cache();

	Return_if_fail (working_fe << working->match_fileent (common_fe));
	Return_if_fail (selected_fe << selected->match_fileent (common_fe));

	bool was_merged = working->has_been_merged(working_fe, common_fe, selected_fe);

	if(was_merged) {
	    bool remerge;
	    Return_if_fail (remerge << remerge_files (working_fe, common_fe, selected_fe));

	    if (!remerge)
		continue;
	}

	if(!was_merged &&
	   !common_fe->on_command_line() &&
	   (working_fe && !working_fe->on_command_line()) &&
	   (selected_fe && !selected_fe->on_command_line())) {

	    working->merge_notify_incomplete();
	    continue;
	}

	Return_if_fail (run_merge_rename (selected_fe, common_fe, working_fe));
    }

    return NoError;
}

static PrVoidError merge_selected_only_files(ProjectDescriptor* selected,
					     ProjectDescriptor* common,
					     ProjectDescriptor* working)
{
    FileEntry *selected_fe;

    foreach_fileent(fe_ptr, selected) {
	selected_fe = *fe_ptr;

	common->repository_entry()->Rep_clear_compressed_cache();
	working->repository_entry()->Rep_clear_compressed_cache();
	selected->repository_entry()->Rep_clear_compressed_cache();

	FileEntry *bogus;

	Return_if_fail (bogus << common->match_fileent(selected_fe));

	if (bogus)
	    continue;

	Return_if_fail (bogus << working->match_fileent(selected_fe));

	if (bogus)
	    continue;

	bool was_merged = working->has_been_merged(NULL, NULL, selected_fe);

	if(was_merged) {
	    bool remerge;
	    Return_if_fail (remerge << remerge_files (NULL, NULL, selected_fe));

	    if (!remerge)
		continue;
	}

	if(!was_merged && !selected_fe->on_command_line()) {

	    working->merge_notify_incomplete();
	    continue;
	}

	Return_if_fail (run_merge_rename (selected_fe, NULL, NULL));
    }

    return NoError;
}

static PrVoidError merge_working_only_files(ProjectDescriptor* selected,
					    ProjectDescriptor* common,
					    ProjectDescriptor* working)
{
    FileEntry *working_fe, *selected_fe;

    foreach_fileent(fe_ptr, working) {
	working_fe = *fe_ptr;

	common->repository_entry()->Rep_clear_compressed_cache();
	working->repository_entry()->Rep_clear_compressed_cache();
	selected->repository_entry()->Rep_clear_compressed_cache();

	FileEntry *bogus;

	Return_if_fail (bogus << common->match_fileent(working_fe));

	if (bogus)
	    continue;

	Return_if_fail (selected_fe << selected->match_fileent(working_fe));

	bool was_merged = working->has_been_merged(working_fe, NULL, selected_fe);

	if(was_merged) {
	    bool remerge;
	    Return_if_fail (remerge << remerge_files (working_fe, NULL, selected_fe));

	    if (!remerge)
		continue;
	}

	if(!was_merged &&
	   !working_fe->on_command_line() &&
	   (selected_fe && !selected_fe->on_command_line())) {

	    working->merge_notify_incomplete();
	    continue;
	}

	Return_if_fail (run_merge_rename (selected_fe, NULL, working_fe));
    }

    return NoError;
}

static PrVoidError merge_project_table (KeyPopMergeControl* cont, OrderedStringTable* sel, OrderedStringTable* com, OrderedStringTable* work)
{
    OrderedStringTable* tables[3];

    tables[0] = sel;
    tables[1] = com;
    tables[2] = work;

    for (int i = 0; i < 3; i += 1) {
	if (!tables[i])
	    continue;

	foreach (k_ptr, tables[i]->table(), OrderedStringTable::Table::HashIterator) {
	    StringPair* k = &(* k_ptr);
	    bool skip = false;

	    for (int j = i-1; j >= 0; j -= 1) {
		if (! tables[j])
		    continue;

		if (tables[j]->lookup (k->x())) {
		    skip = true;
		    break;
		}
	    }

	    if (skip)
		continue;

	    Return_if_fail (cont->kp_run_merge (tables[0]->table()->lookup_pair (k->x()),
						tables[1]->table()->lookup_pair (k->x()),
						tables[2]->table()->lookup_pair (k->x())));

	}
    }

    return NoError;
}

static PrVoidError merge_projects (ProjectDescriptor* sel,
				   ProjectDescriptor* com,
				   ProjectDescriptor* work)
{
    Return_if_fail (merge_project_table (populate_control,
					 sel->populate_ignore(),
					 com->populate_ignore(),
					 work->populate_ignore()));
    Return_if_fail (merge_project_table (keyword_control,
					 sel->project_keywords_extra(),
					 com->project_keywords_extra(),
					 work->project_keywords_extra()));
    return NoError;
}

static PrBoolError remerge_files (FileEntry *work,
				  FileEntry *common,
				  FileEntry *selected)
{
    static BangFlag remerge_bang;

    if (option_skilled_merge) {
	prcsquery << "The file " << merge_tuple(work, common, selected)
		  << " has already been merged against the selected version.  "
		  << force ("Remerging")
		  << report ("Remerge")
		  << allow_bang (remerge_bang)
		  << option ('n', "Ignore this file")
		  << defopt ('y', "Remerge this file")
		  << query ("Remerge");

	char c;

	Return_if_fail (c << prcsquery.result());

	if (c == 'y')
	    return true;
    }

    return false;
}

static bool any_names_differ(FileEntry* working, FileEntry* common, FileEntry* selected)
{
    return (working && common   && (working->file_type() != common->file_type() ||
				    strcmp(working->working_path(), common->working_path()) != 0)) ||
	   (working && selected && (working->file_type() != selected->file_type() ||
				    strcmp(working->working_path(), selected->working_path()) != 0)) ||
	   (common  && selected && (common->file_type() != selected->file_type() ||
				    strcmp(common->working_path(), selected->working_path()) != 0));
}

static void print_filetuple(ostream& s, FileTuple tup)
{
    bool first = true;

    if (tup.work_fe) {
	if (tup.type == FileTuple::MergeTriple)
	    s << "[W: ";
	else
	    s << "[FROM: ";

	if (tup.work_fe->file_type() != RealFile)
	    s << format_type (tup.work_fe->file_type()) << ": ";

	s << tup.work_fe->working_path();

	first = false;
    }

    if (tup.com_fe) {
	if (first)
	    s << "[";
	else
	    s << ", ";

	if (tup.type == FileTuple::MergeTriple)
	    s << "C: ";
	else
	    s << "TO: ";

	if (tup.com_fe->file_type() != RealFile)
	    s << format_type (tup.com_fe->file_type()) << ": ";

	s << tup.com_fe->working_path();

	first = false;
    }

    if (tup.sel_fe) {
	if (first)
	    s << "[";
	else
	    s << ", ";

	if (tup.sel_fe->file_type() != RealFile)
	    s << format_type (tup.sel_fe->file_type()) << ": ";

	s << "S: " << tup.sel_fe->working_path();
    }

    s << "]";
}

static void output_filetuple (ostream& s, FileTuple tup)
{
    if (any_names_differ(tup.work_fe, tup.com_fe, tup.sel_fe)) {

	print_filetuple(s, tup);

    } else {

	FileEntry* non_null;

	if(tup.work_fe)
	    non_null = tup.work_fe;
	else if (tup.sel_fe)
	    non_null = tup.sel_fe;
	else
	    non_null = tup.com_fe;

	if (non_null->file_type() != RealFile)
	    s << "[" << format_type (non_null->file_type()) << ":" << non_null->working_path() << "]";
	else
	    s << squote(non_null->working_path());
    }
}

ostream& __omanip_filetuple(ostream& s, FileTuple tup)
{
    output_filetuple (s, tup);
    return s;
}

/*********************************************************************/
/*                        ACTION HANDLERS                            */
/*********************************************************************/


static PrVoidError make_obsolete(FileEntry *fe, ProjectDescriptor *working_project, bool force_copy)
{
    if (fe->file_type() == Directory)
	return NoError;

    if (fe->file_type() == SymLink) {
	unlink (fe->working_path());
	return NoError;
    }

    if (!fs_file_exists (fe->working_path()))
	return NoError;

    const char *work_path = fe->working_path();
    const char *lastslash = strrchr(work_path, '/');
    int len, path_len, tried = 0;
    Dstring obs(work_path);

    if(lastslash != NULL)
	path_len = len = (lastslash - work_path) + 1;
    else
	path_len = len = 0;

    obs.truncate(len);
    obs.append ("obsolete");
    len = obs.length();

    do {
	obs.truncate(len);
	if (tried > 0)
	    obs.append_int(tried++);
	else
	    tried += 1;
    } while(fs_file_exists(obs) && !fs_is_directory(obs));

    if(!fs_file_exists(obs)) {
	If_fail(Err_mkdir(obs, 0777))
	    pthrow prcserror << "Mkdir failed on directory " << squote(obs) << perror;
    }

    obs.append('/');
    obs.append((const char*)work_path + path_len);

    len = obs.length();
    tried = 0;

    do {
	obs.truncate (len);
	obs.sprintfa (".v%d", tried++);
    } while (fs_file_exists (obs));

    if (force_copy)
      Return_if_fail (fs_copy_filename (work_path, obs));
    else
      Return_if_fail (fs_move_filename (work_path, obs));

    any_obsolete = true;

    if(option_long_format)
	prcsinfo << "Copied working file " << squote(work_path)
		 << " to " << squote(obs) << prcsendl;

    working_project->merge_log() << "Copied working file " << squote(work_path)
				 << " to " << squote(obs) << prcsendl;

    return NoError;
}

static PrBoolError merge_files(ProjectDescriptor* working_project,
			       FileEntry* selected,
			       FileEntry* common,
			       FileEntry* working,
			       MergeCo* co)
{
    ArgList *args;
    Dstring slabel, clabel, wlabel;
    int ret;

    /* If merging a no_ancestor set of files, common will be null.
     */
    ASSERT (!common  || common  ->file_type() == RealFile, "check first");
    ASSERT (selected && selected->file_type() == RealFile, "check first");
    ASSERT (working  && working ->file_type() == RealFile, "check first");
    ASSERT (working->present(), "how could this fail?");

    Return_if_fail (checkout_unkey (selected, temp_file_selected,
				    common, temp_file_common,
				    working, temp_file_working,
				    co));

    Return_if_fail(working->get_file_sys_info());

    Return_if_fail(selected->get_repository_info(selected->project()->repository_entry()));

    if (common)
	Return_if_fail(common->get_repository_info(common->project()->repository_entry()));

    SystemCommand* merge_command = NULL;

    if (working->file_attrs()->merge_tool())
	merge_command = sys_cmd_by_name(working->file_attrs()->merge_tool());

    if (merge_command)
	Return_if_fail (args << merge_command->new_arg_list());
    else
	Return_if_fail (args << gdiff3_command.new_arg_list());

    format_diff_label (selected, selected->working_path(), &slabel);
    format_diff_label (working, working->working_path(), &wlabel);

    if(common)
	format_diff_label(common, common->working_path(), &clabel);
    else
	clabel.sprintf("-L*** empty file ***");

    if (!merge_command) {
	args->append("-maE");
	args->append(wlabel);
	args->append(temp_file_working);
	args->append(clabel);
	args->append(common ? temp_file_common : "/dev/null");
	args->append(slabel);
	args->append(temp_file_selected);

 	WriteableFile outfile;

 	Return_if_fail(outfile.open(working->working_path()));

 	Return_if_fail(gdiff3_command.open(true, false));

 	Return_if_fail(outfile.copy(gdiff3_command.standard_out()));

 	Return_if_fail(ret << gdiff3_command.close());

	/* This is in the order it is because if temp_file_working is a
	 * symlink we can't move working_path() until gdiff finishes.
	 * simiilarly, we can't overwrite working_path() ... gdiff
	 * freaks out. */
	Return_if_fail(make_obsolete(working, working_project, false));

 	Return_if_fail(outfile.close());
    } else {
	args->append(wlabel + 2);
	args->append(temp_file_working);
	args->append(clabel + 2);
	args->append(temp_file_common);
	args->append(slabel + 2);
	args->append(temp_file_selected);
	args->append(working->working_path());

	Return_if_fail(make_obsolete(working, working_project, true));

	/* @@@ Really, I should set the utime and mode of the files
	 * here. */

	Return_if_fail(ret << merge_command->open_stdout());
    }

    if (ret > 1) {
	pthrow prcserror << (merge_command ? merge_command->path() : gdiff3_command.path())
			 << " exited with status " << ret << dotendl;
    }

    Return_if_fail(working->chmod(working->stat_permissions()));

    /* Take the selected version history here, because the working
     * version is not always guaranteed to have one. */
    working->set (selected);

    return (bool)ret;
}

static PrVoidError replace_file(ProjectDescriptor* working_project,
				FileEntry* selected,
				FileEntry* /* common */,
				FileEntry* working,
				MergeCo *co)
{
    working->set (selected);

    if (working->present ()) {
	If_fail(working->stat())
	    pthrow prcserror << "Stat failed on "
			    << squote(working->working_path()) << perror;
    }

    Return_if_fail (make_subdirectories (working->working_path()));
    Return_if_fail (make_obsolete (working, working_project, false));

    switch (selected->file_type()) {
    case RealFile:
	Return_if_fail (checkout_unkey (selected, working->working_path(),
					NULL, NULL,
					NULL, NULL,
					co));

	if (working->present ())
	    Return_if_fail (working->chmod(working->stat_permissions()));
	else
	    Return_if_fail (working->chmod(0));

	break;
    case SymLink:
	unlink (working->working_path());

	If_fail (Err_symlink (selected->link_name(), working->working_path()));
	break;
    case Directory:
	Return_if_fail (check_create_subdir (working->working_path()));

	break;
    }

    return NoError;
}

static PrVoidError delete_file(ProjectDescriptor* working_project,
			       FileEntry* /* selected */,
			       FileEntry* /* common */,
			       FileEntry* working,
			       MergeCo* /* co */)
{
    Return_if_fail (make_obsolete (working, working_project, false));
    Return_if_fail (make_subdirectories (working->working_path()));

    working_project->delete_file (working);

    return NoError;
}

static PrVoidError add_file(ProjectDescriptor* working_project,
			    FileEntry* selected,
			    FileEntry*,
			    FileEntry*,
			    MergeCo *co)
{
    static bool do_once = true;

    if (do_once) {
	working_project->append_files_data("\n");
	do_once = false;
    }

    working_project->append_file(selected);

    Return_if_fail (make_obsolete (selected, working_project, false));
    Return_if_fail (make_subdirectories (selected->working_path()));

    switch (selected->file_type()) {
    case RealFile:
	Return_if_fail (checkout_unkey (selected, selected->working_path(),
					NULL, NULL,
					NULL, NULL,
					co));

	Return_if_fail (selected->chmod(0));
	break;
    case SymLink:
	unlink (selected->working_path());

	If_fail (Err_symlink (selected->link_name(), selected->working_path()));
	break;
    case Directory:
	Return_if_fail (check_create_subdir (selected->working_path()));

	break;
    }

    return NoError;
}

/*********************************************************************/
/*                           UTILITIES                               */
/*********************************************************************/

static PrBoolError file_cmp(const char* file1, const char* file2)
{
    int one, two;

    If_fail(one << Err_open(file1, O_RDONLY)) {
	pthrow prcserror << "Open failed on " << squote(file1) << perror;
    }

    If_fail(two << Err_open(file2, O_RDONLY)) {
	pthrow prcserror << "Open failed on " << squote(file2) << perror;
    }

    bool diffs;

    If_fail(diffs << cmp_fds(one, two)) {
	close(one);
	close(two);
	pthrow prcserror << "Read failed while comparing " << squote(file1)
			<< " and " << squote(file2) << perror;
    }

    close(one);
    close(two);

    return diffs;
}

static PrVoidError checkout_unkey_work (FileEntry* fe, const char* name)
{
    const char* rcsversion, *versionfile, *working_path, *fullpath;
    RepEntry *rep_entry = fe->project()->repository_entry();

    Return_if_fail(fe->initialize_descriptor(rep_entry, false, false));

    Return_if_fail(fe->get_file_sys_info());

    rcsversion = fe->descriptor_version_number();
    versionfile = fe->full_descriptor_name();
    working_path = fe->working_path();

    if(fe->keyword_sub()) {
	unlink (name);

	Return_if_fail( setkeys(working_path,
				name,
				fe,
				Unsetkeys) );
    } else {
	unlink (name);

	Return_if_fail(fullpath << name_in_cwd(working_path));

	If_fail (Err_symlink (fullpath, name))
	    pthrow prcserror << "Failed creating symlink " << squote (name) << perror;
    }

    return NoError;

}

static PrVoidError checkout_unkey_rep (FileEntry* fe, const char* name)
{
    const char* rcsversion, *versionfile;
    RepEntry *rep_entry = fe->project()->repository_entry();

    Return_if_fail(fe->initialize_descriptor(rep_entry, false, false));

    rcsversion = fe->descriptor_version_number();
    versionfile = fe->full_descriptor_name();

    Return_if_fail(VC_checkout_file(name, rcsversion, versionfile));

    return NoError;
}

static PrVoidError checkout_unkey (FileEntry* selected, const char* sel_name,
				   FileEntry* common, const char* com_name,
				   FileEntry* working, const char* work_name,
				   MergeCo* co)
{
    if (selected && sel_name && !co->selected) {
	Return_if_fail (checkout_unkey_rep (selected, sel_name));
	co->selected = sel_name;
    }

    if (common && com_name && !co->common) {
	Return_if_fail (checkout_unkey_rep (common, com_name));
	co->common = com_name;
    }

    if (working && work_name && !co->working) {
	if (working->present())
	    Return_if_fail (checkout_unkey_work (working, work_name));
	else
	    Return_if_fail (checkout_unkey_rep (working, work_name));

	co->working = work_name;
    }

    /* now files are checked out.  see if the names are right */

    if (sel_name && selected && strcmp (sel_name, co->selected) != 0) {
	Return_if_fail (fs_move_filename (co->selected, sel_name));
	co->selected = sel_name;
    }

    if (work_name && working && strcmp (work_name, co->working) != 0) {
	Return_if_fail (fs_move_filename (co->working, work_name));
	co->working = work_name;
    }

    if (com_name && common && strcmp (com_name, co->common) != 0) {
	Return_if_fail (fs_move_filename (co->common, com_name));
	co->common = com_name;
    }

    return NoError;
}

static PrBoolError file_quick_cmp  (FileEntry* selected,
				    FileEntry* common,
				    FileEntry* working,
				    bool*      positive)
{
    FileEntry* one = selected ? selected : common;
    FileEntry* two = working ? working : common;

    if (one->file_type() != two->file_type()) {
	*positive = true;
	return true;
    }

    if (one->file_type() == SymLink) {
        if (two == working) {
	    const char* lname;

	    Return_if_fail (lname << read_sym_link (two->working_path()));

	    two->set_link_name (lname);
        }

	*positive = true;
	return strcmp (one->link_name(), two->link_name()) != 0;
    }

    if (one->file_type() == Directory) {
	*positive = true;
	return false;
    }

    if (working && working->present()) {
	if (!working->unmodified() || !working->descriptor_name())
	    return false; /* value unimportant, positive not set */
    }

    RcsVersionData* onedat;
    RcsVersionData* twodat;

    Return_if_fail (onedat << one->project()->repository_entry()->
		    lookup_rcs_file_data(one->descriptor_name(),
					 one->descriptor_version_number()));
    Return_if_fail (twodat << two->project()->repository_entry()->
		    lookup_rcs_file_data(two->descriptor_name(),
					 two->descriptor_version_number()));

    *positive = true;

    return memcmp (onedat->unkeyed_checksum(), twodat->unkeyed_checksum(), 16) != 0;
}

static PrVoidError maybe_rename (FileEntry* selected,
				 FileEntry* common,
				 FileEntry* working)
{
    if (selected && working && common &&
	strcmp (selected->working_path(), working->working_path()) != 0 &&
	strcmp (selected->working_path(), common->working_path()) != 0) {

	static BangFlag bang_flag;

	prcsquery << "The file " << merge_tuple (working, common, selected)
		  << " changes name in the selected version, you may rename the file.  "
		  << force ("Renaming")
		  << report ("Rename")
		  << allow_bang (bang_flag)
		  << option ('n', "Do not rename this file")
		  << defopt ('y', "Rename the file")
		  << query ("Select");

	char c;

	Return_if_fail (c << prcsquery.result());

	if (c == 'y' && ! option_report_actions) {
	    working->project()->merge_log() << "The file " << squote (working->working_path())
					    << " was renamed to "
					    << squote (selected->working_path()) << prcsendl;
	    Return_if_fail (make_subdirectories(selected->working_path()));
	    Return_if_fail (fs_move_filename (working->working_path(), selected->working_path()));
	    working->set_working_path (selected->working_path());
	}
    }

    return NoError;
}

static PrVoidError run_merge_rename (FileEntry* selected,
				     FileEntry* common,
				     FileEntry* working)
{
    Return_if_fail (maybe_rename (selected, common, working));
    Return_if_fail (file_entry_control->file_run_merge (selected, common, working));
    return NoError;
}

static PrVoidError view_merge_diffs(FileEntry* selected,
				    FileEntry* common,
				    FileEntry* working,
				    MergeCo* co)
{
    while(true) {
	if(selected && common)
	    prcsquery << option('s', "View common -> selected diffs");

	if(working && common)
	    prcsquery << option('w', "View common -> working diffs");

	if(selected && working)
	    prcsquery << option('d', "View selected -> working diffs");

	prcsquery << defopt('l', "Leave this menu") << query("Please select");

	char c;

	Return_if_fail(c << prcsquery.result());

	switch(c) {
	case 's':
	    Return_if_fail (checkout_unkey (selected, temp_file_selected,
					    common, temp_file_common,
					    NULL, NULL,
					    co));
	    Return_if_fail(diff_pair(common, selected, temp_file_common, temp_file_selected));
	    break;
	case 'w':
	    Return_if_fail (checkout_unkey (NULL, NULL,
					    common, temp_file_common,
					    working, temp_file_working,
					    co));
	    Return_if_fail(diff_pair(common, working, temp_file_common, temp_file_working));
	    break;
	case 'd':
	    Return_if_fail (checkout_unkey (selected, temp_file_selected,
					    NULL, NULL,
					    working, temp_file_working,
					    co));
	    Return_if_fail(diff_pair(selected, working, temp_file_selected, temp_file_working));
	    break;
	case 'l':
	    return NoError;
	}
    }
}

/*********************************************************************/
/*                     PRINTING AND LOGGING                         */
/*********************************************************************/

void MergeControl::merge_announce_os (Mergeable* selected,
				      Mergeable* common,
				      Mergeable* working,
				      ostream &os)
{
    os << "*** Action on " << type() << " " << print3 (selected, common, working) << prcsendl;
}

void MergeControl::merge_action_os (Mergeable* selected,
				    Mergeable* common,
				    Mergeable* working,
				    MergeAction action,
				    int ruleno,
				    bool conflict,
				    ostream &os)
{
    const char* desc;

    switch (action) {
    case MergeActionMerge:
	/* with tool */
	desc = "Merge";
	break;
    case MergeActionAdd:
	desc = "Add";
	break;
    case MergeActionDelete:
	desc = "Delete";
	break;
    case MergeActionNothing:
	desc = "Do nothing with";
	break;
    case MergeActionReplace:
	desc = "Replace";
	break;
    default:
	desc = "No prompt for";
	break;
    }

    ASSERT (ruleno != 0, "because no SKIP_FILE");

    os << desc << " " << type() << " " << print3 (selected, common, working) << " by rule " << ruleno;

    if (conflict)
	os << ", conflicts created";

    os << prcsendl;
}

void MergeControl::merge_announce (Mergeable* selected,
				   Mergeable* common,
				   Mergeable* working,
				   MergeAction action)
{
    merge_announce_os (selected, common, working, project->merge_log());

    bool skip_noprompt  = (action == MergeActionNoPrompt) && ! option_really_long_format;
    bool skip_donothing = (action == MergeActionNothing)  && (option_force_resolution || ! option_really_long_format);

    if (! skip_noprompt && ! skip_donothing) {

	merge_announce_os (selected, common, working, prcsoutput);
    }
}

void MergeControl::merge_action (Mergeable* selected,
				 Mergeable* common,
				 Mergeable* working,
				 MergeAction action,
				 int ruleno,
				 bool conflict)
{
    merge_action_os (selected, common, working, action, ruleno, conflict, project->merge_log());

    bool skip_noprompt  = (action == MergeActionNoPrompt) && ! option_really_long_format;
    bool skip_donothing = (action == MergeActionNothing)  && (option_force_resolution || ! option_really_long_format);

    if (! skip_noprompt && ! skip_donothing) {

	merge_action_os (selected, common, working, action, ruleno, conflict, prcsoutput);
    }

    project->merge_log() << prcsendl;
}

/*********************************************************************/
/*                       CONTROLLING LOGIC                           */
/*********************************************************************/

const char* MergeControl::file_merge_desc (Mergeable* selected,
					   Mergeable* common,
					   Mergeable* working,
					   int rule)
{
    Mergeable *non_null;

    if (working)
	non_null = working;
    else if (selected)
	non_null = selected;
    else {
	non_null = common;
	ASSERT (non_null, "blah");
    }

    return merge_desc (non_null, rule);
}

MergeAction MergeControl::file_merge_action (Mergeable* selected,
					     Mergeable* common,
					     Mergeable* working,
					     int rule)
{
    Mergeable *non_null;

    if (working)
	non_null = working;
    else if (selected)
	non_null = selected;
    else {
	non_null = common;
	ASSERT (non_null, "blah");
    }

    return merge_action_by_rule (non_null, rule);
}

PrVoidError MergeControl::finish_merge (Mergeable* selected,
					Mergeable* common,
					Mergeable* working,
					const MergeDef* mergedef,
					MergeCo *co)
{
    MergeAction action = file_merge_action (selected, common, working, mergedef->rule_no);
    bool may_add = (working == NULL) && (selected != NULL);
    bool may_merge = (working != NULL) && (selected != NULL);

    merge_announce (selected, common, working, action);

    if (may_add && !can_add (selected)) {
	static BangFlag bang_flag;

	/* This doesn't happen for KeyPop */

	prcsquery << "Normally, you would be prompted to add the " << type() << " "
		  << print3 (selected, common, working)
		  << ", but it already exists in the working project "
	    "either by file name or descriptor and therefore cannot be "
	    "added.  "
		  << force ("Skipping this file")
		  << report ("Skip this file")
		  << allow_bang (bang_flag)
		  << optfail ('n')
		  << defopt ('y', "Do nothing (as if the merge action were 'n')")
		  << query ("Skip this file");

	Return_if_fail (prcsquery.result());

	project->merge_log () << "Cannot add this file due to conflict with currently present file" << prcsendl;

	if (action == MergeActionAdd)
	    action = MergeActionNothing;

	may_add = false;
    }

    if (selected && working &&
	(!will_3way (working) ||
	 !will_3way (selected) ||
	 (common && !will_3way (common)))) {

	if (action == MergeActionMerge) {
	    action = MergeActionNothing;

	    project->merge_log () << "Cannot merge this " << type() << ", as it is not a regular file" << prcsendl;
	    prcsinfo << "Cannot merge non-regular file "
		     << print3 (selected, common, working)
		     << ", setting default action to nothing" << dotendl;
	}

	may_merge = false;
    }

    static bool bang_flags[14];

    if (!option_report_actions && !option_force_resolution && !bang_flags[mergedef->rule_no]) {
	Dstring valid;
	Dstring query;

	prcsoutput << "Choose an action on " << type() << " " << print3 (selected, common, working)
		   << " for rule " << mergedef->rule_no << ": " << mergedef->help_string << dotendl;

	if (may_add)
	    valid.append ('a');

	if (working)
	    valid.append ('d');

	valid.append ('n');

	if (may_merge)
	    valid.append ('m');

	if (working && selected) {
	    valid.append ('r');
	}

	query.sprintf("Please select(%sh%sq!?)[%c] ", valid.cast(), can_view() ? "v" : "", (char)action);

	while(true) {
	    char c;

	    re_query_message = query.cast();
	    re_query_len = query.length();

	    re_query();

	    c = get_user_char();

	    if(c == 0) fprintf(stdout, "\n");

	    if(c == 0 || c == 'q')
		return PrVoidError(UserAbort);

	    if(c == '?')
		print_merge_help(valid);
	    else if(c == 'h')
		prcsoutput << file_merge_desc (selected, common, working, mergedef->rule_no)
			   << prcsendl;
	    else if(can_view() && c == 'v')
		Return_if_fail(view (selected, common, working, co));
	    else if(c == '\n') {
		break;
	    } else if(c == '!') {
	        bang_flags[mergedef->rule_no] = true;
		break;
	    } else if(strchr(valid, c) != NULL) {
		action = (MergeAction)c;
		break;
	    }
	}

	re_query_message = NULL;
    }

    bool conflict = false;

    if (!option_report_actions) {
	switch(action) {
	case MergeActionMerge:
	    Return_if_fail(conflict << mergef (selected, common, working, co));
	    break;
	case MergeActionReplace:
	    Return_if_fail(replacef (selected, common, working, co));
	    break;
	case MergeActionAdd:
	    Return_if_fail(addf (selected, common, working, co));
	    break;
	case MergeActionDelete:
	    Return_if_fail(deletef (selected, common, working, co));
	    break;
	case MergeActionNothing:
	    break;
	default:
	    ASSERT (false, "notreached");
	    break;
	}
    }

    merge_action (selected, common, working, action, mergedef->rule_no, conflict);

    notify (working, common, selected, action);

    if (conflict) {

	all_conflicts.append (name (working));

	const char* env = get_environ_var ("PRCS_CONFLICT_EDITOR");

	if (env) {
	    SystemCommand* edit_cmd = sys_cmd_by_name (env);

	    ArgList* args;

	    Return_if_fail(args << edit_cmd->new_arg_list());

	    args->append(name (working));

	    int retval;

	    Return_if_fail(retval << edit_cmd->open_stdout());

	    if (retval != 0)
		prcswarning << "warning: Conflict editor exited with status "
			    << retval << dotendl;
	}
    }

    return NoError;
}

void MergeControl::print_merge_help(const char* valid)
{
    prcsoutput << "Valid options are:" << prcsendl;

    if (strchr (valid, 'a'))
	prcsoutput << "a -- Add selected " << type() << " to working version" << dotendl;

    if (strchr (valid, 'd'))
	prcsoutput << "d -- Delete working " << type() << dotendl;

    prcsoutput << "n -- Do nothing" << dotendl;

    if (strchr (valid, 'm'))
	prcsoutput << "m -- Merge selected and working " << type() << dotendl;

    if (strchr (valid, 'r'))
	prcsoutput << "r -- Replace working " << type() << " with selected " << type() << dotendl;

    prcsoutput << "h -- Explain this condition" << dotendl
	       << "v -- View diffs" << dotendl
	       << "q -- " << default_fail_query_message << dotendl
	       << "! -- Take the default and do not offer this query again" << dotendl;
}

/* Can skip a rule entirely if this: */
//  #define SKIP_FILE(act) ((!option_long_format) &&
//  			((act == MergeActionNoPrompt) ||
//  			 ((act == MergeActionNothing) &&
//  			  (!option_force_resolution))))

int MergeControl::compact (const MergeDef** arr,
			   Mergeable* selected,
			   Mergeable* common,
			   Mergeable* working,
			   int len)
{
    int nlen = 0;

    for (int i = 0; i < len; i += 1) {
	if (arr[i]) { // && !SKIP_FILE(file_merge_action(selected, common, working, arr[i]->rule_no))) {
	    arr[nlen++] = arr[i];
	}
    }

    return nlen;
}

PrVoidError MergeControl::run_merge (Mergeable* selected,
				     Mergeable* common,
				     Mergeable* working)
{
    /* Do some analysis to determine how far we can get without
     * actually checking out the files.  It can be determined
     * statically, but that makes it impossible to do anything with
     * the no-prompt rules. */
    MergeCo co;
    const MergeDef *found_defs[5];
    int found = 0;

    for (int i = 0; i < ARRAY_LEN(merge_defaults); i += 1) {

	if ((merge_defaults[i].have_working  == (working  != NULL)) &&
	    (merge_defaults[i].have_common   == (common   != NULL)) &&
	    (merge_defaults[i].have_selected == (selected != NULL))) {

	    found_defs[found++] = merge_defaults + i;
	}
    }

    int pass = 0;

    found = compact (found_defs, selected, common, working, found);

    bool wc_pos = false, ws_pos = false, sc_pos = false;
    bool wc_cmp;
    bool ws_cmp;
    bool sc_cmp;

    while (found > 1 || pass == 0) {

	if (pass == 0) {
	    /* All quick comparisons without checking out files. */
	    if (working && common)
		Return_if_fail(wc_cmp << quick_cmp (NULL, common, working, &wc_pos));
	    if (selected && working)
		Return_if_fail(ws_cmp << quick_cmp (selected, NULL, working, &ws_pos));
	    if (common && selected)
		Return_if_fail(sc_cmp << quick_cmp (selected, common, NULL, &sc_pos));

	} else if (pass == 1 && selected && common) {

	    /* Nothing to do here, S-C is always positive */
	    ASSERT (sc_pos, "not working, must be positive");

	} else if (pass == 2 && working && selected && !ws_pos) {
	    /* The WS comparison */
	    Return_if_fail (prepare (selected, temp_file_selected,
				     NULL, NULL,
				     working, temp_file_working,
				     &co));

	    Return_if_fail(ws_cmp << file_cmp (temp_file_selected, temp_file_working));

	    ws_pos = true;
	} else if (pass == 3 && working && common && !wc_pos) {
	    /* The WC comparison */
	    Return_if_fail (prepare (NULL, NULL,
				     common, temp_file_common,
				     working, temp_file_working,
				     &co));

	    Return_if_fail(wc_cmp << file_cmp (temp_file_common, temp_file_working));

	    wc_pos = true;
	} else if (pass == 4) {
	    ASSERT (false, "this loses badly");
	}

	for (int i=0; i < found; i += 1) {
	    if ( (working  && common   && wc_pos && (wc_cmp != found_defs[i]->wc_cmp)) ||
		 (working  && selected && ws_pos && (ws_cmp != found_defs[i]->ws_cmp)) ||
		 (selected && common   && sc_pos && (sc_cmp != found_defs[i]->sc_cmp)) )
		found_defs[i] = NULL;
	}

	pass += 1;
	found = compact (found_defs, selected, common, working, found);
    }

    ASSERT (found == 1, "no SKIP_FILE");

    if (file_merge_action (selected, common, working, found_defs[0]->rule_no) == MergeActionNoPrompt) {

	int rule = (found == 0) ? 0 : found_defs[0]->rule_no;

	merge_announce (selected, common, working, MergeActionNoPrompt);
	merge_action   (selected, common, working, MergeActionNoPrompt, rule, false);

	return NoError;
    }

    return finish_merge (selected, common, working, found_defs[0], &co);
}
