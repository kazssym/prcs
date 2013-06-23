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
 * $Id: prcs.cc 1.23.1.1.1.14.1.18.1.8.1.16.1.1.1.7.1.41.1.2 Wed, 06 Feb 2002 20:57:16 -0800 jmacd $
 */

/* $Format: "static const char prcs_version_id[] = \"$ProjectVersion$ $ProjectAuthor$ $ProjectDate$\";"$ */
static const char prcs_version_id[] = "1.3.3-relase.21 jmacd Sat, 27 Jan 2007 17:17:01 -0800";

#include "fnmatch.h"

#include "prcs.h"
#include "lock.h"
#include "docs.h"
#include "vc.h"
#include "misc.h"
#include "projdesc.h"
#include "fileent.h"
#include "syscmd.h"
#include "repository.h"
#include "system.h"

#if defined(__BEOS__)
#include <KernelKit.h>
#endif

extern "C" {
#include <signal.h>
#include <sys/time.h>
#include "getopt.h"
}

const int prcs_version_number[3] = {
    PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_MICRO
};

const char prcs_version_string[] = PACKAGE_VERSION;

/* The following classes are only used inside this file, and oraganize
 * several arrays of information used to select the command and
 * configuration. */
enum ProjectArgType {
    InsureProjectName,
    NoInsureProjectName,
    NoProjectRequired,
    OptionalProjectName
};

enum RevisionSpecifierArgType {
    NoRevisionArg,
    OneRevisionArg,
    TwoRevisionArgs
};

enum RevisionFlags {
    NoFlags        = 0,
    AllowWildCards = 1 << 1,
    NoDefault      = 1 << 2
};

enum DiffArgType {
    ReallyDiffArgs,
    ReallyExecuteArgs,
    NoDiffArgs
};

struct CommandOptionPair {
    char opt;
    const char* desc;
    int *addr;
};

struct CommandNamePair {
    SystemCommand* command;
    const char* name;
};

struct PrcsCommand {
    const char *command_name;
    const char *defmaj;
    const char *defmin;
    const char *defmin_maj_only;
    int revision_flags;
    PrPrcsExitStatusError (*command_function)();
    const char *help_string;
    RevisionSpecifierArgType revision_arg_type;
    ProjectArgType project_arg_type;
    const char* optstring;
    DiffArgType diff_arg_type;
    int n_files_allowed;
};

struct CleanupHandler {
    CleanupHandler(PrVoidError (*h)(void*, bool), void* d, struct CleanupHandler* n)
	:handler(h), data(d), next_handler(n) { }
    PrVoidError (*handler)(void*, bool);
    void *data;
    CleanupHandler* next_handler;
};

/* Static data */

static PrcsCommand *command = NULL;
static PrcsCommand *subcommand = NULL;
static bool illegal_option = false;
static struct CleanupHandler *cleanup_handler_list = NULL;
static struct CleanupHandler *alarm_handler_list = NULL;

static const char check_rcs_arg[] = "-V";
static const char check_rcs_expected[] = "*GNU RCS* 5.*";
static const CommandNamePair check_rcs = { &rcs_command, "rcs" };

static const char check_diff_arg[] = "-v";
static const char check_diff_expected[] = "*GNU diffutils*";
static const CommandNamePair check_diff = { &gdiff_command, "gdiff" };

static const char check_diff3_arg[] = "-v";
static const char check_diff3_expected[] = "*GNU diffutils*";
static const CommandNamePair check_diff3 = { &gdiff3_command, "gdiff3" };

static CommandNamePair const command_names[] = {
    { &rcs_command, "rcs" },
    { &ci_command, "ci" },
    { &co_command, "co" },
    { &rlog_command, "rlog" },
    { &gdiff_command, "diff" },
    { &gdiff3_command, "diff3" },
    { &gzip_command, "gzip" },
    { &ls_command, "ls" },
    { &tar_command, "tar" },
    { NULL, NULL }
};

#define MATCH_CHAR       '\300'
#define NOTMATCH_CHAR    '\301'
#define ALL_CHAR         '\302'
#define PRE_CHAR         '\303'
#define PIPE_CHAR        '\304'
#define PLAINFORMAT_CHAR '\305'
#define SORT_CHAR        '\306'
#define NOKEYW_CHAR      '\307'
#define VLOG_CHAR        '\310'

#define EXECUTE_STR      "\300\301\302\303\304"
#define PLAINFORMAT_STR  "\305"
#define SORT_STR         "\306"
#define NOKEYW_STR       "\307"
#define VLOG_STR         "\310"

static CommandOptionPair const command_options[] = {
    { 'i', "--immediate", &option_immediate_uncompression },
    { 'k', "--keywords", &option_diff_keywords },
    { 'N', "--new", &option_diff_new_file },
    { 'd', "--delete", &option_populate_delete },
    { 'z', "--compress", &option_package_compress },
    { 'p', "--preserve", &option_preserve_permissions },
    { 'P', "--exclude-project-file", &option_exclude_project_file },
    { 'u', "--unlink", &option_unlink },
    { 's', "--skilled-merge", &option_skilled_merge },
    { NOKEYW_CHAR, "--no-keywords", &option_nokeywords },
    { VLOG_CHAR, "--version-log", &option_version_log },
    { SORT_CHAR, "--sort", &option_sort },
    { MATCH_CHAR, "--match", &option_match_file },
    { NOTMATCH_CHAR, "--not", &option_not_match_file },
    { ALL_CHAR, "--all", &option_all_files },
    { PRE_CHAR, "--pre", &option_preorder },
    { PIPE_CHAR, "--pipe", &option_pipe },
    { 0, NULL }
};

static struct option const long_options[] = {
  /* standard operands */
  {"version",                 no_argument, 0, 'v'},
  {"help",                    no_argument, 0, 'h'},
  {"long-long-format",        no_argument, 0, 'L'},
  {"long-format",             no_argument, 0, 'l'},
  {"no-action",               no_argument, 0, 'n'},
  {"quiet",                   no_argument, 0, 'q'},
  {"force",                   no_argument, 0, 'f'},
  {"repository",              required_argument, 0, 'R'},
  {"jobs",                    required_argument, 0, 'j'},
  {"plain-format",            no_argument, 0, PLAINFORMAT_CHAR },
#ifdef PRCS_DEVEL
  {"debug",                   no_argument, 0, 'D'},
  {"tune",                    required_argument, 0, 't'},
#endif

  /* non-standard operands */
  {"new",                     no_argument, 0, 'N'},
  {"keywords",                no_argument, 0, 'k'},
  {"immediate",               no_argument, 0, 'i'},
  {"delete",                  no_argument, 0, 'd'},
  {"compress",                no_argument, 0, 'z'},
  {"preserve-permissions",    no_argument, 0, 'p'},
  {"preserve",                no_argument, 0, 'p'},
  {"permissions",             no_argument, 0, 'p'},
  {"exclude-project-file",    no_argument, 0, 'P'},
  {"revision",                required_argument, 0, 'r'},
  {"unlink",                  no_argument, 0, 'u'},
  {"skilled-merge",           no_argument, 0, 's'},
  {"no-keywords",             no_argument, 0, NOKEYW_CHAR},
  {"version-log",             required_argument, 0, VLOG_CHAR},
  {"sort",                    required_argument, 0, SORT_CHAR},
  {"match",                   required_argument, 0, MATCH_CHAR},
  {"not",                     required_argument, 0, NOTMATCH_CHAR},
  {"all",                     no_argument, 0, ALL_CHAR},
  {"pre",                     no_argument, 0, PRE_CHAR},
  {"pipe",                    no_argument, 0, PIPE_CHAR},
  {0,0,0,0}};

/* It looks like a bug in the optstring that EXECUTE_STR members don't
 * have ':' characters, but GNU getopt doesn't seem to mind. */
static const char prcs_optstring[] = "+vhHLlnqfR:r:NkidzpPruj:s"
                                     EXECUTE_STR PLAINFORMAT_STR NOKEYW_STR VLOG_STR ":" SORT_STR ":"
#ifdef PRCS_DEVEL
"Dt:"
#endif
;

static PrcsCommand commands[] = {
    /* commands accepting -r */
    {"checkin", "", "@", "@", NoFlags,
     checkin_command, checkin_help_string,
     OneRevisionArg, InsureProjectName, "uj" VLOG_STR, NoDiffArgs, -1 },

    {"checkout", "@", "@", "@", NoFlags,
     checkout_command, checkout_help_string,
     OneRevisionArg, InsureProjectName, "upP", NoDiffArgs, -1 },

    {"diff", "", "", "@", NoFlags,
     diff_command, diff_help_string,
     TwoRevisionArgs, InsureProjectName, "NkP", ReallyDiffArgs, -1 },

    {"merge", "", "@", "@", NoFlags,
     merge_command, merge_help_string,
     OneRevisionArg, InsureProjectName, "us", ReallyDiffArgs, -1 },

    {"delete", "", "", "@", NoDefault,
     delete_command, delete_help_string,
     OneRevisionArg, InsureProjectName, NULL, NoDiffArgs, 0 },

    {"info", "*", "*", "*", AllowWildCards,
     info_command, info_help_string,
     OneRevisionArg, InsureProjectName, SORT_STR, NoDiffArgs, -1 },

    {"changes", "", "", "@", NoFlags,
     changes_command, changes_help_string,
     TwoRevisionArgs, InsureProjectName, "", NoDiffArgs, -1 },

    {"execute", "", "", "@", NoFlags,
     execute_command, execute_help_string,
     OneRevisionArg, InsureProjectName, "P" EXECUTE_STR, ReallyExecuteArgs, -1 },

    /* commands not accepting -r */
    {"populate", NULL, NULL, NULL, NoFlags,
     populate_command, populate_help_string,
     NoRevisionArg, InsureProjectName, "du" NOKEYW_STR, NoDiffArgs, -1 },

    {"depopulate", NULL, NULL, NULL, NoFlags,
     depopulate_command, depopulate_help_string,
     NoRevisionArg, InsureProjectName, "u", NoDiffArgs, -1 },

    {"package", NULL, NULL, NULL, NoFlags,
     package_command, package_help_string,
     NoRevisionArg, InsureProjectName, "z", NoDiffArgs, 1 },

    {"unpackage", NULL, NULL, NULL, NoFlags,
     unpackage_command, unpackage_help_string,
     NoRevisionArg, NoInsureProjectName, NULL, NoDiffArgs, -2 },

    {"rekey", NULL, NULL, NULL, NoFlags,
     rekey_command, rekey_help_string,
     NoRevisionArg, InsureProjectName, "u", NoDiffArgs, -1 },

    {"config", NULL, NULL, NULL, NoFlags,
     config_command, config_help_string,
     NoRevisionArg, NoProjectRequired, NULL, NoDiffArgs, 0 },

    /* fake command entries */
    {"admin", NULL, NULL, NULL, NoFlags,
     NULL, NULL, NoRevisionArg, InsureProjectName, NULL, NoDiffArgs, 0 },

    {NULL, NULL, NULL, NULL, NoFlags,
     NULL, NULL, NoRevisionArg, InsureProjectName, NULL, NoDiffArgs, 0 }
};

static PrcsCommand subcommands[] = {
    {"compress", NULL, NULL, NULL, NoFlags,
     admin_compress_command, admin_compress_help_string,
     NoRevisionArg, InsureProjectName, NULL, NoDiffArgs, 0 },

    {"uncompress", NULL, NULL, NULL, NoFlags,
     admin_uncompress_command, admin_uncompress_help_string,
     NoRevisionArg, InsureProjectName, "i", NoDiffArgs, 0 },

    {"rebuild",  NULL,  NULL, NULL, NoFlags,
     admin_rebuild_command, admin_rebuild_help_string,
     NoRevisionArg, InsureProjectName, NULL, NoDiffArgs, 0 },

    {"init",  NULL,  NULL, NULL, NoFlags,
     admin_init_command, admin_init_help_string,
     NoRevisionArg, InsureProjectName, NULL, NoDiffArgs, 0 },

    {"access", NULL,  NULL, NULL, NoFlags,
     admin_access_command, admin_access_help_string,
     NoRevisionArg, OptionalProjectName, NULL, NoDiffArgs, 0 },

    {"pdelete", NULL, NULL, NULL, NoFlags,
     admin_pdelete_command, admin_pdelete_help_string,
     NoRevisionArg, InsureProjectName, NULL, NoDiffArgs, 0 },

    {"pinfo", NULL, NULL, NULL, NoFlags,
     admin_pinfo_command, admin_pinfo_help_string,
     NoRevisionArg, NoProjectRequired, NULL, NoDiffArgs, 0 },

    {"prename", NULL, NULL, NULL, NoFlags,
     admin_prename_command, admin_prename_help_string,
     NoRevisionArg, InsureProjectName, NULL, NoDiffArgs, 1 },

    {NULL, NULL, NULL, NULL, NoFlags,
     NULL, NULL, NoRevisionArg, InsureProjectName, NULL, NoDiffArgs, 0 }
};

static const int friendly_signals[] = {
    SIGINT, SIGTERM, SIGPIPE, SIGHUP,
#ifdef SIGXCPU
    SIGXCPU,
#endif
#ifdef SIGXFSZ
    SIGXFSZ,
#endif
    0
};

static const int unfriendly_signals[] = {
    SIGBUS, SIGFPE, SIGILL, SIGQUIT, SIGSEGV, 0
};

#if defined(__BEOS__)
static thread_id beos_alarm_id = 0;
#endif

/* End static data */

static PrVoidError check_system_command(CommandNamePair com,
					const char* arg,
					const char* expected)
{
    Dstring output;

    ArgList *args;

    Return_if_fail(args << com.command->new_arg_list());

    args->append(arg);

    Return_if_fail(com.command->open(true, false));

    If_fail(read_string(com.command->standard_out(), &output))
	pthrow prcserror << "Read failed from " << com.name << perror;

    Return_if_fail_if_ne(com.command->close(), 0) {
	pthrow prcserror << com.name << ' ' << arg
			<< " failed, " << com.name
			<< " is probably too old for PRCS to use"
			<< dotendl;
    }

    if (fnmatch (expected, output.cast (), FNM_NOESCAPE) != 0) {
	pthrow prcserror << com.name
			 << " installation is too old, please install "
			 << expected << dotendl;
    }

    return NoError;
}

static PrVoidError check_system_commands()
{
    Return_if_fail(check_system_command(check_rcs,
					check_rcs_arg,
					check_rcs_expected));

    Return_if_fail(check_system_command(check_diff,
					check_diff_arg,
					check_diff_expected));

    Return_if_fail(check_system_command(check_diff3,
					check_diff3_arg,
					check_diff3_expected));

    return NoError;
}

/* Config checks and reports the paths of all the programs upon which
 * PRCS depends, and exits according to whether everything checked
 * out. */
PrPrcsExitStatusError config_command()
{
    const int width = 40;
    const CommandNamePair* command = command_names;
    PrPrcsExitStatusError ret(ExitSuccess);

    prcsoutput << "Version " << prcs_version_string << prcsendl;

    prcsoutput << prcs_version_id << prcsendl;

    if(get_environ_var("RCS_PATH"))
	prcsinfo << "Using $RCS_PATH to find binaries" << dotendl;

    for(;command->command;command += 1) {
	If_fail(command->command->init()) {
	    ret = PrPrcsExitStatusError (FatalError);
	    continue;
	}

	prcsoutput << "System path for " << squote(command->name)
		   << " is: " << setcol(width) << command->command->path() << prcsendl;
    }

    const char* rep;

    If_fail(rep << Rep_guess_repository_path())
	rep = "-*- unavailable -*-";

    prcsoutput << "Repository path is: " << setcol(width) << rep << prcsendl;

    const char* log = get_environ_var ("PRCS_LOGQUERY");

    prcsoutput << "Log querying is: " << setcol(width)
	       << (log ? "on" : "off") << prcsendl;

    const char* editor = get_environ_var ("PRCS_CONFLICT_EDITOR");

    if (editor)
	prcsoutput << "Conflict editor is: " << setcol(width) << editor << prcsendl;

    const char* merge = get_environ_var ("PRCS_MERGE_COMMAND");

    if (merge)
	prcsoutput << "Merge command is: " << setcol(width) << merge << prcsendl;

    const char* diff = get_environ_var ("PRCS_DIFF_COMMAND");

    if (diff)
	prcsoutput << "Diff command is: " << setcol(width) << diff << prcsendl;

    prcsoutput << "Job number is: " << setcol(width) << option_jobs << prcsendl;

    diff = get_environ_var ("PRCS_DIFF_OPTIONS");

    if (diff)
	prcsoutput << "Diff options are: " << setcol(width) << diff << prcsendl;

    const char* tmpdir = get_environ_var ("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    prcsoutput << "Temp directory is: " << setcol(width) << tmpdir << prcsendl;

    prcsoutput << "Plain formatting is: " << setcol(width)
	       << (option_plain_format ? "on" : "off") << prcsendl;

    //Return_if_fail(check_system_commands());

    return ret;
}

static bool check_ignored_options(const char* optstring)
{
    const CommandOptionPair *p = command_options;
    bool warning = false;

    for(; p->opt; p += 1) {
	if(*p->addr && !strchr(optstring, p->opt)) {
	    prcsinfo << "Ignoring command line switch "
		     << p->desc << dotendl;
	    warning = true;
	    *p->addr = 0;
	}
    }

    if (option_all_files && option_pipe) {
	prcsinfo << "Ignoring --pipe with --all specified" << dotendl;
	option_pipe = 0;
	warning = true;
    }

    return warning;
}

static PrVoidError check_major_version_name(const char* name)
{
    if (name[0] && strcmp(name, "@") != 0 &&
	VC_check_token_match(name, "label") <= 0)
	pthrow prcserror << "Illegal major version name "
			 << squote(name)
			 << dotendl;

    return NoError;
}

static PrVoidError check_minor_version_name(const char* name)
{
    if (name[0] && strcmp(name, "@") != 0 &&
	VC_check_token_match(name, "number") <= 0)
	pthrow prcserror << "Illegal minor version name "
			 << squote(name)
			 << dotendl;

    return NoError;
}

static PrPrcsExitStatusError invoke_command(PrcsCommand *c)
{
    bool warning = false;

    if (c->n_files_allowed >= 0 && cmd_filenames_count != c->n_files_allowed) {
	pthrow prcserror << "This command requires " << c->n_files_allowed
			<< " file-or-dir arguments" << dotendl;
    }

    if(c->project_arg_type == InsureProjectName ||
       (c->project_arg_type == OptionalProjectName && cmd_root_project_name)) {
	if(VC_check_token_match(cmd_root_project_name, "label") <= 0 ||
	   /* this length is arbitrary */
	   strlen(cmd_root_project_name) > 200) {
	    prcserror << "Illegal project name " << squote(cmd_root_project_name)
		       << dotendl;
	    exit(2);
	}
    }

    if (c->revision_arg_type != NoRevisionArg) {
	if (c->revision_flags & NoDefault &&
	    !cmd_version_specifier_major &&
	    !cmd_version_specifier_minor)
	    pthrow prcserror << "You must specify a revision argument with "
			    << squote ("-r") << " for this command" << dotendl;

	if (!cmd_version_specifier_major && !cmd_version_specifier_minor) {
	    cmd_version_specifier_major = c->defmaj;
	    cmd_version_specifier_minor = c->defmin;
	}

	if (!cmd_version_specifier_minor)
	    cmd_version_specifier_minor = c->defmin_maj_only;

	if (!cmd_version_specifier_minor[0] && cmd_version_specifier_major[0])
	    pthrow prcserror << "It is illegal to specify a null minor "
		"version without a null major version" << dotendl;

	if (!(c->revision_flags & AllowWildCards)) {
	    Return_if_fail(check_major_version_name(cmd_version_specifier_major));
	    Return_if_fail(check_minor_version_name(cmd_version_specifier_minor));
	}

	if (c->revision_arg_type == TwoRevisionArgs &&
	    cmd_alt_version_specifier_major) {
	    if (!cmd_alt_version_specifier_minor)
		cmd_alt_version_specifier_minor = c->defmin_maj_only;

	    if (!(c->revision_flags & AllowWildCards)) {
		Return_if_fail(check_major_version_name(cmd_alt_version_specifier_major));
		Return_if_fail(check_minor_version_name(cmd_alt_version_specifier_minor));
	    }
	}
    }

    warning |= check_ignored_options(c->optstring ? c->optstring : "");

    if(warning) {
	prcsquery << "Command line options are being ignored.  "
		  << force("Continuing")
		  << report("Continue")
		  << optfail('n')
		  << defopt('y', "Continue, ignoring these options")
		  << query("Continue");

	Return_if_fail(prcsquery.result());
    }

    if (cmd_version_specifier_minor &&
	cmd_version_specifier_minor[0] &&
	strcmp(cmd_version_specifier_minor, "@") != 0) {
	cmd_version_specifier_minor_int = atoi(cmd_version_specifier_minor);
    }

    return c->command_function();
}

static PrVoidError set_project_name(const char* specifier0)
{
    /* There is ambiguity between interpreting "prcs" as a directory
     * or as a project name.  It will only be interpreted as a dir
     * if the directory is really a directory. */
    int len = strlen(specifier0);
    const char* last_slash = strrchr(specifier0, '/');

    Dstring* project_path = new Dstring;
    Dstring* project_name = new Dstring;

    if (len > 4 && strcmp (specifier0 + len - 4, ".prj") == 0) {

	if (last_slash) {
	    project_path->assign(specifier0);
	    project_path->truncate(last_slash - specifier0);

	    project_name->assign(last_slash + 1);
	} else {
	    /* project_path is correct */
	    project_name->assign(specifier0);
	}

	project_name->truncate(project_name->length() - 4);

    } else if (fs_is_directory(specifier0)) {

	Dstring* prj;

	Return_if_fail(prj << guess_prj_name(specifier0));

	if (prj == NULL) {
	    pthrow prcserror << "Cannot determine project name, there must be a single "
		"project file in the directory " << squote(specifier0) << dotendl;
	}

	project_path->assign(specifier0);

	project_name->assign(*prj);

	delete prj;

    } else if (specifier0[len - 1] == '/') {
	pthrow prcserror << "Directory " << squote(specifier0)
			<< " does not exist, cannot determine project name" << dotendl;
    } else if (last_slash) {
	project_path->assign(specifier0);
	project_path->truncate(last_slash - specifier0);
	project_name->assign(last_slash + 1);
    } else {
	project_name->assign(specifier0);
    }

    if (project_path->length() > 0 &&
	project_path->index(project_path->length() - 1) != '/')
	project_path->append('/');

    while (strncmp(*project_path, "./", 2) == 0)
	project_path->assign(project_path->cast() + 2);

    Dstring* project_file = new Dstring;
    Dstring* project_full = new Dstring;

    project_full->assign(*project_path);
    project_full->append(*project_name);

    project_file->assign(*project_full);
    project_file->append(".prj");

    /* These all leak. */
    cmd_root_project_name      = project_name->cast();
    cmd_root_project_file_path = project_file->cast();
    cmd_root_project_full_name = project_full->cast();
    cmd_root_project_path      = project_path->cast();
    cmd_root_project_path_len  = project_path->length();
#if 0
    prcsinfo << "cmd_root_project_name = " << cmd_root_project_name << prcsendl;
    prcsinfo << "cmd_root_project_file_path = " << cmd_root_project_file_path << prcsendl;
    prcsinfo << "cmd_root_project_full_name = " << cmd_root_project_full_name << prcsendl;
    prcsinfo << "cmd_root_project_path = " << cmd_root_project_path << prcsendl;
#endif
    return NoError;
}

static PrIntError determine_project(PrcsCommand* command, int argc, char** argv)
{
    if(command->project_arg_type != NoProjectRequired) {
	if(argc > 0 && strcmp(argv[0], "--") != 0) {

	    Return_if_fail(set_project_name(argv[0]));

	    return 1;
	} else if (command->project_arg_type == InsureProjectName) {

	    Return_if_fail(set_project_name("./"));

	} else if (command->project_arg_type != OptionalProjectName) {
	    pthrow prcserror << "Missing operand" << dotendl;
	}
    }

    return 0;
}

static const char* correct_cmd_filename(char* cmd_filename)
{
    Dstring correct(cmd_filename);

    while (strncmp(correct.cast(), "./", 2) == 0)
	correct.assign(correct.cast() + 2);

    /* let "foo/" be the root project path.  We want to accept anything prefixed
     * with "foo/" or "foo". */

    if (cmd_root_project_path_len > 0) {
	if (correct.length() < cmd_root_project_path_len - 1)
	    /* its too short */
	    goto nowayman;

	if (strncmp(correct, cmd_root_project_path, cmd_root_project_path_len - 1) != 0)
	    /* the prefix is incorrect */
	    goto nowayman;

	if (correct.length() == cmd_root_project_path_len - 1) {
	    /* its "foo", cool */
	    correct.append('/');
	} else if (strcmp(correct.cast() + cmd_root_project_path_len, ".") == 0)
	    /* its "foo/.", get rid of the "." */
	    correct.truncate(cmd_root_project_path_len);
    }

    while (correct.length() > 0 && correct.last_index() == '/')
      correct.truncate (correct.length() - 1);

    return p_strdup(correct);

nowayman:

    prcswarning << "Command line file-or-dir " << squote(cmd_filename)
		<< " is not prefixed with project path " << squote(cmd_root_project_path)
		<< dotendl;

    return NULL;
}

static PrVoidError set_cmd_filenames()
{
    if(cmd_filenames_count > 0) {
       cmd_filenames_found = new bool[cmd_filenames_count];

       for(int i = 0; i < cmd_filenames_count; i += 1)
	  cmd_filenames_found[i] = false;

       cmd_corrected_filenames_given = new const char*[cmd_filenames_count];

       int j = 0;
       bool warn = false;
       for(int i = 0; i < cmd_filenames_count; i += 1) {
	   const char* new_name = correct_cmd_filename(cmd_filenames_given[i]);

	   if (!new_name) {
	       warn = true;
	       continue;
	   }

	   cmd_corrected_filenames_given[j] = new_name;
	   cmd_filenames_given[j++] = cmd_filenames_given[i];
       }

       cmd_filenames_count = j;

       for (int i = 0; i < cmd_filenames_count; i += 1) {
	   if (strcmp(cmd_corrected_filenames_given[i], cmd_root_project_path) == 0) {
	       	/* This is okay, there may be bogus file-or-dirs that won't be reported. */
#if 0
	       prcsinfo << "Command line file-or-dir " << squote(cmd_filenames_given[i])
			<< " matches project root, proceeding with all files selected"
			<< dotendl;
#endif

	       cmd_filenames_count = 0;
	   }
       }

       if (warn) {
	   prcsquery << "Command line file-or-dirs are being ignored.  "
		     << force("Continuing")
		     << report("Continue")
		     << optfail('n')
		     << defopt('y', "Proceed, ignore these filenames")
		     << query ("Proceed");

	   char c;

	   Return_if_fail(c << prcsquery.result());
       }
    }

    return NoError;
}

static bool read_command_line(int argc, char** argv)
{
    int c;
    PrcsCommand *pc = commands;
    int num_revs = 0, i;
    int longind;
    bool saw_j = false;

    if(argc > 1)
	for(; pc->command_name != NULL; pc += 1)
	    if (strcmp(pc->command_name, argv[1]) == 0) {
		argv += 1;
		argc -= 1;
		command = pc;
		break;
	    }

    if(command && strcmp(command->command_name, "admin") == 0) {
	if(argc <= 1)
	    return false;

	for(pc = subcommands; pc->command_name != NULL; pc += 1)
	    if (strcmp(pc->command_name, argv[1]) == 0) {
		argv += 1;
		argc -= 1;
		subcommand = pc;
		break;
	    }

	if(subcommand == NULL)
	    return false;
    }

    while ((c = getopt_long(argc, argv, prcs_optstring, long_options, &longind)) != EOF)
    {
	char *min;
	switch (c) {
	case 'v':
	    prcsoutput << "Version " << prcs_version_string << prcsendl;
	    exit(0);
	    break;
	case 'h': case 'H': return false; break;
	case 'f': option_force_resolution = 1; break;
	case 'k': option_diff_keywords = 1; break;
	case 'l': option_long_format = 1; break;
	case 'i': option_immediate_uncompression = 1; break;
	case 'z': option_package_compress = 1; break;
	case 'd': option_populate_delete = 1; break;
	case 'q': option_be_silent = 1; break;
	case 'N': option_diff_new_file = 1; break;
	case 'L': option_really_long_format = 1; break;
	case 'n': option_report_actions = 1; break;
	case 'p': option_preserve_permissions = 1; break;
	case 'P': option_exclude_project_file = 1; break;
	case 'u': option_unlink = 1; break;
	case 'R': cmd_repository_path = optarg; break;
	case 'j':
	  {
	    char* end_ptr;
	    saw_j = true;

	    option_jobs = strtol (optarg, &end_ptr, 10);

	    if (end_ptr[0] != 0)
		option_jobs = -1;
	  }
	break;
	case MATCH_CHAR:
	    option_match_file = 1;
	    option_match_file_pattern = optarg;
	    break;
	case NOTMATCH_CHAR:
	    option_not_match_file = 1;
	    option_not_match_file_pattern = optarg;
	    break;
	case ALL_CHAR: option_all_files = 1; break;
	case PRE_CHAR: option_preorder = 1; break;
	case PIPE_CHAR: option_pipe = 1; break;
	case PLAINFORMAT_CHAR: option_plain_format = 1; break;
	case SORT_CHAR: option_sort = 1; option_sort_type = optarg; break;
	case NOKEYW_CHAR: option_nokeywords = 1; break;
	case VLOG_CHAR: option_version_log = 1; option_version_log_string = optarg; break;
#ifdef PRCS_DEVEL
	case 'D': option_n_debug = 0; break;
	case 't': option_tune = atoi (optarg); break;
#endif
	case 's': option_skilled_merge = 1; break;
	case 'r':
	    option_version_present = 1;

	    if(!command) break;

	    if (num_revs < 1 && !command->revision_arg_type == NoRevisionArg) {
		min = strrchr(optarg, '.'); /* last occurance of a period */
		if(min == NULL){
		    cmd_version_specifier_major = optarg;
		} else {
		    *min = '\0';
		    cmd_version_specifier_minor = min + 1;
		    cmd_version_specifier_major = optarg;
		}
		num_revs += 1;
	    } else if(num_revs < 2 && command->revision_arg_type == TwoRevisionArgs) {
		min = strrchr(optarg, '.'); /* last occurance of a period */
		if(min == NULL){
		    cmd_alt_version_specifier_major = optarg;
		} else {
		    *min = '\0';
		    cmd_alt_version_specifier_minor = min + 1;
		    cmd_alt_version_specifier_major = optarg;
		}
		num_revs += 1;
	    } else {
		prcserror << "Too many -r options given on command line" << dotendl;
		exit(2);
	    }

	    break;
	case '?':
	default:
	    illegal_option = true;
	    return false;
	    break;
	}
    }

    if (!option_sort)
	option_sort_type = "version";

    if (strcmp(option_sort_type, "version") != 0 &&
	strcmp(option_sort_type, "date") != 0) {
	prcserror << "Illegal sort type " << squote (option_sort_type) << dotendl;
	exit(2);
    }

    if (!saw_j) {
	const char* j = get_environ_var ("PRCS_JOB_NUMBER");
	char *je = NULL;

	if (j) {
	    option_jobs = strtol (j, &je, 10);

	    if (je[0] != 0)
		option_jobs = -1;
	}
    }


    if (option_jobs < 1 || option_jobs > 100 /* arbitrary */) {
	if (saw_j)
	    prcserror << "Illegal value for -j" << dotendl;
	else
	    prcserror << "Illegal value for PRCS_JOB_NUMBER" << dotendl;
	exit(2);
    }

    if(get_environ_var("PRCS_PLAIN_FORMAT"))
	option_plain_format = 1;

    if(option_report_actions)
	option_long_format = 1;

    if(subcommand)
	command = subcommand;

    if(command == NULL)
	return false;

    argc -= optind;
    argv += optind;

    int used;

    If_fail(used << determine_project(command, argc, argv))
	exit(2);

    argv += used;
    argc -= used;

    cmd_filenames_given = argv;
    cmd_filenames_count = argc;

    i = 0;
    cmd_diff_options_given = 0;
    cmd_diff_options_count = -1;

    while(i < cmd_filenames_count) {
	if(strcmp(cmd_filenames_given[i], "--") == 0) {
	    cmd_filenames_count = i;
	    cmd_diff_options_count = argc - i - 1;
	    cmd_diff_options_given = argv + i + 1;
	}
	i += 1;
    }

    if (command->diff_arg_type == NoDiffArgs && cmd_diff_options_count >= 0) {
	prcserror << "This command does not accept arguments following "
		  << squote ("--") << dotendl;
	exit(2);
    }

    if (cmd_diff_options_count < 0 && command->diff_arg_type == ReallyDiffArgs) {
	const char* pdo = get_environ_var ("PRCS_DIFF_OPTIONS");

	if (pdo) {
	    CharPtrArray *env_diff_options = new CharPtrArray;
	    char *p;

	    p = strtok (p_strdup (pdo), " \t\n");

	    while (p) {
		env_diff_options->append (p);
		p = strtok(NULL, " \t\n");
	    }

	    cmd_diff_options_given = (char**)env_diff_options->cast();
	    cmd_diff_options_count = env_diff_options->length();
	    /* @@@ leak */
	}
    }

    if (command->n_files_allowed == -1) {
	If_fail(set_cmd_filenames())
	    exit(2);
    }

    return true;
}

static void usage(char** argv)
{
    if (illegal_option) {
	cout << options_summary << "\n";
    } else if(command == NULL) {
	cout << "Usage: " << strip_leading_path(argv[0])
	     << " command [subcommand] [option ...] "
	    "[project [file-or-dir]]\n";
	cout << general_help_string;
    } else if(strcmp(command->command_name, "admin") == 0) {
	if(subcommand == NULL) {
	    cout << admin_help_string;
	} else {
	    cout << subcommand->help_string;
	}
    } else {
	cout << command->help_string;
    }

    exit(2);
}

/* Cleanup */

void install_cleanup_handler(PrVoidError (* handler)(void *, bool), void* data)
{
    cleanup_handler_list = new CleanupHandler(handler, data, cleanup_handler_list);
}

void install_alarm_handler(PrVoidError (* handler)(void *, bool), void* data)
{
    alarm_handler_list = new CleanupHandler(handler, data, alarm_handler_list);
}

static PrVoidError clean_up(bool signaled)
{
    sigset_t signal_mask;

    sigfillset(&signal_mask);
    sigdelset(&signal_mask, SIGABRT);

    sigprocmask(SIG_SETMASK, &signal_mask, NULL);

    CleanupHandler* last_handler;

    while(cleanup_handler_list) {
	last_handler = cleanup_handler_list;
	cleanup_handler_list = cleanup_handler_list->next_handler;

	Return_if_fail(last_handler->handler(last_handler->data, signaled));

	delete last_handler;
    }

    if(temp_directory
#ifdef PRCS_DEVEL
       && !option_debug
#endif
	) {
	fs_nuke_file(temp_directory);
	temp_directory = NULL;
    }

#if defined(__BEOS__)
    if (beos_alarm_id > 0) {
        kill_thread(beos_alarm_id);
        beos_alarm_id = 0;
    }
#endif
    return NoError;
}

static void bad_clean_up_handler(SIGNAL_ARG_TYPE)
{
    prcserror << "Internal error, attempting to drop core" << dotendl;

    bug ();

    clean_up(true);

    abort();
}

static void clean_up_handler(SIGNAL_ARG_TYPE)
{
    clean_up(true);

    exit(2);
}

static void alarm_handler(SIGNAL_ARG_TYPE)
{

    CleanupHandler* handler = alarm_handler_list;

    for (; handler; handler = handler->next_handler)
	handler->handler(handler->data, false);
}

#if defined(__BEOS__)
static int32 beos_alarm_thread (void * data)
{
    while (1) {
        snooze( (STALE_LOCK_TIMEOUT - 30) * 1000000 );
        alarm_handler(SIGALRM);
    }
}
#endif

static void chld_handler(SIGNAL_ARG_TYPE)
{
  /* Silly Solaris does not interrupt select() for SIGCHLD when there
   * is no handler!!! */
}

static void handle_signals()
{
    struct sigaction act;
    sigset_t signal_mask;
    int i;

    sigfillset(&signal_mask);
    sigdelset(&signal_mask, SIGABRT);

    act.sa_handler = clean_up_handler;
    act.sa_mask = signal_mask;
#ifdef SA_RESTART
    act.sa_flags = SA_RESTART;
#else
    act.sa_flags = 0;
#endif

    for(i = 0; friendly_signals[i]; i += 1)
	sigaction(friendly_signals[i], &act, NULL);

    act.sa_handler = bad_clean_up_handler;

    for(i = 0; unfriendly_signals[i]; i += 1)
	sigaction(unfriendly_signals[i], &act, NULL);

    sigemptyset(&signal_mask);

    /* SIGCONT */
    act.sa_handler = continue_handler;
    act.sa_mask = signal_mask;
    sigaction(SIGCONT, &act, NULL);

    /* SIGCHLD */
    act.sa_handler = chld_handler;
    act.sa_mask = signal_mask;
    sigaction (SIGCHLD, &act, NULL);

#if !defined(__BEOS__)
    /* SIGALRM */
    act.sa_handler = alarm_handler;
    act.sa_mask = signal_mask;
    sigaction (SIGALRM, &act, NULL);

    struct itimerval itval;

    itval.it_interval.tv_sec = STALE_LOCK_TIMEOUT - 30;
    itval.it_interval.tv_usec = 0;
    itval.it_value.tv_sec = STALE_LOCK_TIMEOUT - 30;
    itval.it_value.tv_usec = 0;

    setitimer (ITIMER_REAL, &itval, NULL);
#endif

}

/* Temp dirs */

static PrVoidError make_temp_dir()
{
    const char* tmpdir;
    Dstring *dir;
    int len, count = getpid();
    struct stat sbuf;

    if((tmpdir = get_environ_var ("TMPDIR")) == NULL)
	tmpdir = "/tmp";

    if(!fs_is_directory(tmpdir) || !fs_file_wrandex(tmpdir))
	pthrow prcserror << "Can't access temp directory "
		        << squote(tmpdir) << perror;

    if(tmpdir[0] != '/')
	pthrow prcserror << "TMPDIR must be an absolute path" << dotendl;

    dir = new Dstring(tmpdir);
    if (dir->index(dir->length() - 1) != '/')
	dir->append ('/');
    dir->append("prcs_temp.");
    len = dir->length();

    while(true) {
	dir->append_int(count);
	if(stat(*dir, &sbuf) < 0)
	    break;
	dir->truncate(len);
	count += 1;
    }

    temp_directory = *dir;

    If_fail(Err_mkdir(temp_directory, (mode_t)0700))
	pthrow prcserror << "Mkdir failed for file " << squote(temp_directory) << perror;

    return NoError;
}

static PrVoidError make_temp_files()
{
    /* all of this memory gets leaked */

    Return_if_fail(make_temp_dir());

    temp_file_1 = make_temp_file("");
    temp_file_2 = make_temp_file("");
    temp_file_3 = make_temp_file("");

    return NoError;
}

static void check_umask()
{
    mode_t user_file_creation_mask = get_umask();

    if(user_file_creation_mask & 0700) {
	prcswarning << "Your umask will prevent PRCS from functioning properly.  "
	    "Proceeding with user RWX permissions" << dotendl;
	umask(user_file_creation_mask & 0077);
    }
}

int main(int argc, char** argv)
{
    PrPrcsExitStatusError exitval(ExitSuccess);

    setup_streams(argc, argv);

    handle_signals();

#if defined(__BEOS__)
    /* We simulate the alarm() with a dedicated thread - this makes
     * executing the alarm signal handler much safer.
     */
    beos_alarm_id = spawn_thread( beos_alarm_thread, "prcs alarm thread"
                                , B_NORMAL_PRIORITY, NULL);
    if (beos_alarm_id < 0) {
        prcserror << "Can't create the alarm thread: "
                  << strerror(beos_alarm_id)
                  << dotendl;
        exit(2);
    }
    status_t beos_thread_state = resume_thread(beos_alarm_id);
    if (B_NO_ERROR != beos_thread_state) {
        prcserror << "Can't start the alarm thread: "
                  << strerror(beos_thread_state)
                  << dotendl;
        exit(2);
    }
#endif

    if(VC_check_token_match(get_login(), "login") <= 0) {
	prcserror << "Your login name, " << squote(get_login()) << ", may confuse the lexer" << dotendl;
	bug();
	exit(2);
    }

    check_umask();

    if (read_command_line(argc, argv) == false) {
	usage(argv);
	exit(2);
    }

    adjust_streams();

    If_fail(make_temp_files())
	exit(2);

    if(!option_force_resolution && !isatty(STDIN_FILENO) ) {
        prcserror << "Please specify -f to run PRCS without a controlling terminal"
		  << dotendl;
	exit(2);
    }

    If_fail(check_system_commands()) {
	exit(2);
    }

    if(subcommand == NULL) {
	exitval = invoke_command(command);
    } else { 
	exitval = invoke_command(subcommand);
    }

    bool clean_failed = false;

    If_fail(clean_up(false))
	clean_failed = true;

    if(Failure(exitval) || clean_failed) {
	if(exitval.error_val() != UserAbort || clean_failed) {
	    prcserror << "Command failed" << dotendl;
	}
	return 2;
    }

    switch(exitval.non_error_val()) {
    case ExitSuccess:
    case ExitNoDiffs:
	return 0;
    case ExitDiffs:
    default:
	return 1;
    }
}
