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


#ifndef _PRCS_H_
#define _PRCS_H_

extern "C" {
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

/* S_ISLNK() should be defined in sys/stat.h, but it isn't in Unixware-1.1.x
 * according to Thanh Ma <tma@encore.com>. */
#if !defined(S_ISLNK) && defined(S_IFLNK)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "utils.h"
}

/* EGCS doesn't have this header. */
#ifdef HAVE_STD_H
#include <std.h>
#endif

#include <iostream.h>
#include <fstream.h>

#ifdef __GNUG__
/* This gets defined in config.h now. */
/*#define PRCS_DEVEL*/
#endif

#ifdef NULL
#undef NULL
#endif
#define NULL 0 /* Some systems define NULL as (void*)0, but that messes
		* uses of the ?: operator with NULL. */

#include "typedefs.h"
#include "prcserror.h"

/* all variables beginning with 'cmd_' are derived from information
 * present after the options part of the command line.  */

/* for command line project name path1/path2/prcs */
extern const char  *cmd_root_project_name;         /* prcs */
extern const char  *cmd_root_project_file_path;    /* path1/path2/prcs.prj */
extern const char  *cmd_root_project_full_name;    /* path1/path2/prcs */
extern const char  *cmd_root_project_path;         /* path1/path2/ */
extern int          cmd_root_project_path_len;
extern char       **cmd_filenames_given;           /* from command line */
extern const char **cmd_corrected_filenames_given; /* corrected for ./ and . nonsense */
extern int          cmd_filenames_count;           /* number of filenames */
extern bool        *cmd_filenames_found;           /* index i is true if the ith file matched */
extern char       **cmd_diff_options_given;
extern int          cmd_diff_options_count;
extern const char  *cmd_version_specifier_major;
extern const char  *cmd_version_specifier_minor;
extern int          cmd_version_specifier_minor_int;
extern const char  *cmd_alt_version_specifier_major;
extern const char  *cmd_alt_version_specifier_minor;
extern bool         cmd_prj_given_as_file;
extern const char  *cmd_repository_path;

extern const char  prcs_version_string[];

/* all variables beginning with 'option_' are derived from information
 * in the options part of the command line.  */
extern int option_force_resolution;        /* -f */
extern int option_long_format;             /* -l */
extern int option_really_long_format;      /* -L */
extern int option_report_actions;          /* -n */
extern int option_version_present;         /* -r */
extern int option_diff_keywords;           /* -k */
extern int option_diff_new_file;           /* -N */
extern int option_populate_delete;         /* -d */
extern int option_package_compress;        /* -z */
extern int option_immediate_uncompression; /* -i */
extern int option_be_silent;               /* -q */
extern int option_preserve_permissions;    /* -p */
extern int option_exclude_project_file;    /* -P */
extern int option_unlink;                  /* -u */
extern int option_jobs;                    /* -j */
extern int option_match_file;              /* --match */
extern int option_not_match_file;          /* --not */
extern int option_all_files;               /* --all */
extern int option_preorder;                /* --pre */
extern int option_pipe;                    /* --pipe */
extern int option_skilled_merge;           /* -s */
extern int option_plain_format;            /* --plain-format */
extern int option_sort;                    /* --sort */
extern int option_nokeywords;              /* --no-keywords */
extern int option_version_log;             /* --version-log */
#ifdef PRCS_DEVEL
#define option_debug (! option_n_debug)    /* --debug */
extern int option_n_debug;                 /* ! --debug */
extern int option_tune;                    /* --tune */
#endif

extern const char *option_match_file_pattern;
extern const char *option_not_match_file_pattern;
extern const char *option_sort_type;
extern const char *option_version_log_string;

extern const int prcs_version_number[3];

extern const char* temp_file_1;
extern const char* temp_file_2;
extern const char* temp_file_3;
extern const char* temp_directory;

extern PrPrcsExitStatusError checkin_command();            /* checkin.cc */
extern PrPrcsExitStatusError populate_command();           /* populate.cc */
extern PrPrcsExitStatusError depopulate_command();         /* populate.cc */
extern PrPrcsExitStatusError diff_command();               /* diff.cc */
extern PrPrcsExitStatusError execute_command();            /* execute.cc */
extern PrPrcsExitStatusError info_command();               /* info.cc */
extern PrPrcsExitStatusError changes_command();            /* changes.cc */
extern PrPrcsExitStatusError package_command();            /* package.cc */
extern PrPrcsExitStatusError unpackage_command();          /* package.cc */
extern PrPrcsExitStatusError merge_command();              /* merge.cc */
extern PrPrcsExitStatusError checkout_command();           /* checkout.cc */
extern PrPrcsExitStatusError delete_command();             /* rebuild.cc */
extern PrPrcsExitStatusError rekey_command();              /* rekey.cc */
extern PrPrcsExitStatusError config_command();             /* prcs.cc */

extern PrPrcsExitStatusError admin_rebuild_command();      /* rebuild.cc */
extern PrPrcsExitStatusError admin_compress_command();     /* repository.cc */
extern PrPrcsExitStatusError admin_uncompress_command();   /* repository.cc */
extern PrPrcsExitStatusError admin_access_command();       /* repository.cc */
extern PrPrcsExitStatusError admin_init_command();         /* repository.cc */
extern PrPrcsExitStatusError admin_pdelete_command();      /* repository.cc */
extern PrPrcsExitStatusError admin_pinfo_command();        /* repository.cc */
extern PrPrcsExitStatusError admin_prename_command();      /* repository.cc */

extern void install_cleanup_handler(PrVoidError (* handler)(void *, bool),
				    void* data);
extern void install_alarm_handler  (PrVoidError (* handler)(void *, bool),
				    void* data);

#endif
