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


extern "C" {
#include <sys/types.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
}

#if __GNUG__ < 3
#define pubsync sync
#endif

#include "prcs.h"

#include "hash.h"
#include "prcsdir.h"
#include "repository.h"
#include "vc.h"
#include "checkout.h"
#include "misc.h"
#include "lock.h"
#include "syscmd.h"
#include "system.h"
#include "convert.h"
#include "memseg.h"
#include "rebuild.h"
#include "projdesc.h"

static const char radix[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static const char prcs_tagname[] = "prcs_tag";
static const char prcs_compression_tag[] = "prcs_compress";
const char prcs_lock_dir[] = ".locks";

static int rep_root_path_len;
static Dstring *rep_root_path = NULL;

static const char prcs_created_by[] = "created by PRCS version ";
static const char prcs_usable_by[] = "usable down to PRCS version ";

extern PrVoidError save_package();

static PrVoidError Rep_init_command(const char* path);
static const char* Rep_name_in_repository(const char *s);
static bool version_less_than(const int* a, const int* b);

static PrVoidError repository_cleanup_handler(void* data, bool);
static PrVoidError repository_alarm_handler(void* data, bool);

/* The lowest version which may use a repository last touched by this
 * version of PRCS */
static int const prcs_usable_number[3] = { 1, 3, 0 };

/* The lowest version which this version of PRCS may use without being
 * converted.  */
static int const prcs_may_use_number[3] = { 1, 3, 0 };

/* Rep_guess_repository_path --
 *
 *     returns the calculated repository path from environment
 *     variables or command line options, without actually
 *     initilaizing anything.  */
NprConstCharPtrError Rep_guess_repository_path()
{
    if(rep_root_path) {
	return Rep_name_in_repository("");
    }

    const char* rp;
    rep_root_path = new Dstring;

    if(cmd_repository_path)
	rp = cmd_repository_path;
    else
	rp = get_environ_var ("PRCS_REPOSITORY");

    if ( rp == NULL ) {
	rp = get_environ_var ("HOME");
	if (rp == NULL)
	    return NonFatalError;

	rep_root_path->assign(rp);
	rep_root_path->append("/PRCS");
    } else {
	rep_root_path->assign(rp);
    }

    while(rep_root_path->index(rep_root_path->length() - 1) == '/')
	rep_root_path->truncate(rep_root_path->length() - 1);

    rep_root_path_len = rep_root_path->length();

    return Rep_name_in_repository("");
}

/* Rep_init_repository --
 *
 *      Must be called before any of the below commands are used.
 *      This places a read or write lock in the repository, reads the
 *      repository tag, insures that this version of PRCS is
 *      compatible with the repository format being opened.  It will
 *      prompt the user to upgrade repository versions if the version
 *      is outdated and return NoError only when the repository is
 *      locked and ready to use by the current version of PRCS.  */
PrVoidError Rep_init_repository()
{
    static bool do_once = true;

    if(do_once) {
	do_once = false;
    } else {
	return NoError;
    }

    If_fail(Rep_guess_repository_path())
	pthrow prcserror << "Please set your PRCS_REPOSITORY environment variable" << dotendl;

    rep_root_path->truncate(rep_root_path_len);
    Return_if_fail(Rep_init_command(rep_root_path->cast()));

    return NoError;
}

static PrVoidError Rep_init_command(const char* path)
{
    if (!fs_file_exists(path) ) {
	if (option_report_actions)
	    pthrow prcsinfo << "Create repository " << squote(path) << dotendl;

	If_fail( Err_mkdir(path, 0777 | S_ISGID | 01000))
	    pthrow prcserror << "Cannot create repository " << squote(path) << perror;
	else
	    prcswarning << "Created repository " << squote(path)
			<< ", you may wish to run " << squote("prcs admin access")
			<< " to set its permissions" << dotendl;
    }

    const char* rep_lock_name = Rep_name_in_repository(prcs_lock_dir);

    if(!fs_is_directory(rep_lock_name)) {
        if (option_report_actions)
	    pthrow prcserror << "Create repository lock directory "
			    << squote(rep_lock_name) << perror;

	Umask mask (0);

	If_fail( Err_mkdir(rep_lock_name, 0777 | S_ISGID) )
	    pthrow prcserror << "Cannot create repository lock directory "
			     << squote(rep_lock_name) << perror;
    }

    if(!fs_file_writeable(rep_lock_name))
       pthrow prcserror << "You must have write permission on the lock directory "
		       << squote(rep_lock_name) << perror;

    return NoError;
}

static const char* Rep_name_in_repository(const char *s)
{
    rep_root_path->truncate(rep_root_path_len);
    rep_root_path->append('/');
    rep_root_path->append(s);

    return rep_root_path->cast();
}

PrVoidError RepEntry::Rep_lock(bool write_lock)
{
    _repository_lock = NULL;

#ifdef PRCS_DEVEL
    if (option_debug)
	return NoError;
#endif

    Dstring lock_name;

    lock_name.assign(Rep_name_in_repository(prcs_lock_dir));
    lock_name.append('/');
    lock_name.append(_entry_name);

    _repository_lock = new AdvisoryLock(lock_name);

    Return_if_fail (obtain_lock (_repository_lock, write_lock));

    return NoError;
}

PrVoidError obtain_lock(AdvisoryLock* lock, bool write_lock)
{
    int lockval;
    Umask mask (0);

    if(write_lock)
	lockval = lock->write_lock_nowait();
    else
	lockval = lock->read_lock_nowait();

    while(true) {
	if(lockval == LOCK_FAIL)
	    pthrow prcserror << "Failed obtaining a repository lock" << dotendl;

	if(lockval == LOCK_SUCCESS)
	    return NoError;

	char buf[256];
	int lock_count = 0;
	FILE* other_locks;

	other_locks = lock->who_is_locking();

	if(!other_locks)
	    pthrow prcserror << "Could not obtain a list of lockers, unknown "
		"locking error" << dotendl;

	prcsinfo << "Could not obtain a lock because of the following locks:" << prcsendl;

	while(fgets(buf, 256, other_locks) == buf) {
	    if(!isspace(buf[0])) {
		cout << buf;
		lock_count += 1;
	    }
	}

	fclose(other_locks);

	if(lock_count == 0)
	    pthrow prcserror << "Inconsistant lock information, cannot continue" << dotendl;

	if(lockval == LOCK_TRY_STEAL) {
	    prcsquery << "Lock age exceeds timeout age.  "
		      << force("Stealing")
		      << report("Steal")
		      << optfail('n')
		      << defopt('y', "Steal the lock and continue")
		      << query("Steal the lock");

	    Return_if_fail(prcsquery.result());

	    if(!lock->steal_lock())
		return PrVoidError(FatalError);

	} else {
	    prcsquery << force("Waiting for the lock")
		      << report("Waiting for the lock")
		      << optfail('n')
		      << defopt('y', "Wait for the lock")
		      << query("Wait for the lock");

	    Return_if_fail(prcsquery.result());
	}

	if(write_lock)
	    lockval = lock->write_lock_wait();
	else
	    lockval = lock->read_lock_wait();
    }
}

void RepEntry::Rep_unlock_repository()
{
    if (_repository_lock)
	_repository_lock->unlock();
}

static bool version_less_than(const int* a, const int* b)
{
    for(int i = 0; i < 3;  i += 1) {
	if(a[i] < b[i])
	    return true;
	if(b[i] < a[i])
	    return false;
    }

    return false;
}

PrVoidError RepEntry::Rep_make_default_tag()
{
    return Rep_make_tag(prcs_version_number, prcs_usable_number);
}

PrVoidError RepEntry::Rep_make_tag(const int created[3], const int usable[3])
{
    const char* file_name = Rep_name_in_entry(prcs_tagname);

    unlink(file_name);

    ofstream tag_file(file_name, ios::out);

    tag_file << prcs_created_by << created[0] << ' ' << created[1] << ' ' << created[2] << "\n"
	     << prcs_usable_by  << usable[0]  << ' ' << usable[1]  << ' ' << usable[2]  << "\n";

    tag_file.close();

    if(tag_file.bad()) {
	pthrow prcserror << "Failed writing repository tagfile "
			 << squote(file_name) << perror;
    }

    If_fail(Err_chmod(file_name, 0444)) {
 	pthrow prcserror << "Chmod failed on new repository tagfile "
			 << squote(file_name) << perror;
    }

    return NoError;
}

PrVoidError RepEntry::Rep_verify_tag(UpgradeRepository* upgrades)
{
    const char* file_name = Rep_name_in_entry(prcs_tagname);
    char buf[256];
    int created[3];
    int usable[3];

    if(!fs_file_exists(file_name)) {
	pthrow prcserror << "Repository is not tagged, the file "
			 << squote(file_name) << " does not exist" << dotendl;
    }

    if(!fs_file_readable(file_name)) {
	pthrow prcserror << "Can't read repository tag " << squote(file_name) << dotendl;
    }

    ifstream tag_file(file_name);

    if(tag_file.bad()) {
	pthrow prcserror << "Can't open repository tag file " << squote(file_name) << dotendl;
    }

    tag_file.get(buf, sizeof(prcs_created_by));
    tag_file >> created[0];
    tag_file >> created[1];
    tag_file >> created[2];

    if(strncmp(buf, prcs_created_by, strlen(prcs_created_by)) != 0 || tag_file.bad()) {
	pthrow prcserror << "Repository tag file "
			 << squote(file_name) << " is invalid" << dotendl;
    }

    tag_file.get();
    tag_file.get(buf, sizeof(prcs_usable_by));
    tag_file >> usable[0];
    tag_file >> usable[1];
    tag_file >> usable[2];

    if(strncmp(buf, prcs_usable_by, strlen(prcs_usable_by)) != 0 || tag_file.bad()) {
	pthrow prcserror << "Repository tag file " << squote(file_name) << " is invalid" << dotendl;
    }

    bool confirm_upgrade = false;
    while(upgrades && upgrades->upgradeFunc) {
	if(version_less_than(created, upgrades->version)) {
	    if(!_writeable) {
		pthrow prcserror << "This version of PRCS needs to modify your repository before "
		    "continuing.  You do not currently have a write lock, required to do so.  "
		    "You can run " << squote("prcs admin rebuild") << " to upgrade your repository" << dotendl;
	    }

	    if(!confirm_upgrade) {
		prcsquery << "This version of PRCS needs to modify your repository.  "
			  << force("Upgrading")
			  << report("Upgrade")
			  << defopt('s', "Upgrade the repository, after saving a package in case of trouble")
			  << optfail('n')
			  << option('y', "Upgrade the repository, don't save a package")
			  << query("Upgrade");

		char c;

		Return_if_fail(c << prcsquery.result());

		confirm_upgrade = true;

		if(c == 's' && !option_report_actions) {
		    Return_if_fail(save_package());
		}
	    }

	    prcsinfo << "Upgrading repository to version format "
		     << upgrades->update_version[0] << '.'
		     << upgrades->update_version[1] << '.'
		     << upgrades->update_version[2] << dotendl;

	    UpgradeRepository *tmp_upgrades = upgrades;

	    for (; tmp_upgrades->upgradeFunc; tmp_upgrades += 1) {

		prcsinfo << "[" << tmp_upgrades->version[0] << "."
			 << tmp_upgrades->version[1] << "."
			 << tmp_upgrades->version[2]
			 << "] " << tmp_upgrades->description << dotendl;

		if (upgrades->update_version[0] == tmp_upgrades->version[0] &&
		    upgrades->update_version[1] == tmp_upgrades->version[1] &&
		    upgrades->update_version[2] == tmp_upgrades->version[2])
		    break;
	    }

	    created[0] = usable[0] = upgrades->update_version[0];
	    created[1] = usable[1] = upgrades->update_version[1];
	    created[2] = usable[2] = upgrades->update_version[2];

	    bool should_exit;

	    Return_if_fail(should_exit << upgrades->upgradeFunc(this));
	    Return_if_fail(Rep_make_tag(upgrades->update_version, upgrades->update_version));

	    prcsinfo << "Finished upgrade to version format "
		     << upgrades->update_version[0] << '.'
		     << upgrades->update_version[1] << '.'
		     << upgrades->update_version[2] << dotendl;

	    if(should_exit) {
		return PrVoidError(UserAbort);
	    }
	}

	upgrades += 1;
    }

    /* If the tag says an older version which this version is unable
     * to use, and the upgrade did not help, tell the user the
     * repository is too old to use.  */
    if(version_less_than(created, prcs_may_use_number)) {
	pthrow prcserror << "This version of PRCS outdates the version which created your repository, "
			 << created[0] << '.'
			 << created[1] << '.'
			 << created[2]
			 << ".  Please convert your repository" << dotendl;
    }

    /* If the tag says a newer version which this version is unable to
     * use, print a message saying to upgrade the binary to the newer
     * PRCS.  */
    if(version_less_than(prcs_version_number, usable)) {
	pthrow prcserror << "This version of PRCS is outdated by version "
			 << created[0] << '.'
			 << created[1] << '.'
			 << created[2] << ".  You must upgrade before using this repository"
			 << dotendl;
    }

    /* If the tag has a newer version number than this version which
     * is still usable, notify the user that a newer version has
     * touched the repository and continue.  */
    if(version_less_than(prcs_version_number, created))
	prcswarning << "This version of PRCS is outdated by version "
		    << created[0] << '.'
		    << created[1] << '.'
		    << created[2]
		    << ".  It is still compatible, but you should consider upgrading" << dotendl;

    /* If we have a write lock, modify the tag with the up to date
     * versions */
    if(version_less_than(created, prcs_version_number) && _writeable) {
	Return_if_fail(Rep_make_default_tag());
    }

    return NoError;
}

PrVoidError RepEntry::Rep_create_repository_entry()
{
    const char* file_name = Rep_entry_path();

    if ( option_report_actions ) {
	pthrow prcserror << "Create repository entry " << squote(_entry_name);
    } else {
	If_fail( Err_mkdir(file_name, 0777 | S_ISGID) ) {
	    pthrow prcserror << "Cannot create repository entry " << squote(_entry_name) << perror;
	}

	Return_if_fail(Rep_make_default_tag());

	Return_if_fail(VC_register(_project_file));

	If_fail(VC_register(NULL)) {
	    fs_nuke_file(file_name);
	    pthrow prcserror << "Failed registering project version file"
			    << squote(_project_file) << dotendl;
	}

	Return_if_fail(create_empty_data(_entry_name, Rep_name_in_entry("prcs_data")));

	/* If the user doesn't have permission on the group under
	 * whose ownership the new directory was just created, this
	 * chmod fails, so ignore it. */

	If_fail (Err_creat (Rep_name_in_entry("prcs_log"), 0666 ^ get_umask()))
	    prcsinfo << "Create failed on repository log" << perror;

	prcsinfo << "Created repository entry " << squote(_entry_name) << dotendl;

	If_fail(Rep_chmod(0777 ^ get_umask()))
	    prcsinfo << "Chmod failed on new repository entry.  This has the consequence that "
	    "permissions may be wrong, either allowing too much, too little, or incorrect access "
	    "to the repository.  After the command finishes, please run "
		     << squote("prcs admin access")
		     << " to attempt to set its group and access permissions correctly"
		     << dotendl;
    }

    return NoError;
}

PrBoolError Rep_repository_entry_exists(const char* entry)
{
    Return_if_fail(Rep_init_repository());

    return fs_is_directory(Rep_name_in_repository(entry));
}

/* The repsository entry class */

/* Rep_init_repository_entry --
 *
 *     assures that the named entry exists, is executable, and if
 *     writeable is true, also writeable.  Also checks the repository
 *     tag created by Rep_create_repository_entry to see that this
 *     entry has not been used by some future version of PRCS making
 *     it unusable by the version running */
PrRepEntryPtrError Rep_init_repository_entry(const char* entry, bool writeable,
					     bool create, bool require_db)
{
    RepEntry* ret = new RepEntry();

    Return_if_fail(ret->init(entry, writeable, create, require_db));

    return ret;
}

PrVoidError RepEntry::init(const char* entry, bool writeable0, bool create, bool require_db)
{
    Return_if_fail(Rep_init_repository());

    _project_file.sprintf("%s/%s/%s.prj,v", Rep_name_in_repository(""), entry, entry);

    _entry_name.assign(entry);

    _writeable = writeable0;

    _base.assign(Rep_name_in_repository(entry));

    _baselen = _base.length();

    _compressed = false;

    _rebuild_file = NULL;

    _rep_comp_cache = new CharPtrArray;

    bool exists;

    Return_if_fail(exists << Rep_repository_entry_exists(entry));

    if(!exists) {
	if(create) {
	    Return_if_fail(Rep_create_repository_entry());
	} else {
	    pthrow prcserror << "Repository entry " << squote(entry)
			     << " does not exist, you can create an entry with "
			     << squote("prcs checkin")
			     << dotendl;
	}
    }

    If_fail(Err_stat(Rep_entry_path(), &_rep_stat_buf))
	pthrow prcserror << "Stat failed on repository entry "
			 << squote(Rep_entry_path()) << perror;

    _rep_mask = (_rep_stat_buf.st_mode & 0777) ^ 0777;

    Return_if_fail(Rep_lock(_writeable));

    install_cleanup_handler(repository_cleanup_handler, this);
    install_alarm_handler  (repository_alarm_handler, this);

    if(_writeable && !(fs_file_rwx(Rep_entry_path()))) {
	pthrow prcserror << "You need read, write, and execute permission on the repository entry "
			 << squote(Rep_entry_path()) << dotendl;
    } else if(!fs_file_executable(Rep_entry_path())) {
	pthrow prcserror << "You need execute permission on the repository entry "
			 << squote(Rep_entry_path()) << dotendl;
    }

    if (require_db) {
	Return_if_fail(Rep_verify_tag(entry_upgrades));

	if(!fs_file_exists(Rep_name_in_entry(prcs_data_file_name))) {
	    pthrow prcserror << "Repository entry " << squote(entry)
			    << " is missing its data file, please run "
			    << squote("prcs admin rebuild")
			    << " to generate this file" << dotendl;
	}

	Return_if_fail(_rebuild_file <<
		       read_repository_data(Rep_name_in_entry (prcs_data_file_name),
					    _writeable));

	_project_data_array = _rebuild_file->get_project_summary();

	_rcs_file_table = _rebuild_file->get_rcs_file_summary();

	/* Keith Owens 1.3.0 bug: a consistency check here for the
	 * case where a checkin aborts after finishing the RCS checkin
	 * but before updating the repository entry. */

	int rev_count;

	If_fail (rev_count << VC_get_version_count (Rep_name_of_version_file ())) {
	    pthrow prcserror << "Cannot determine the number of versions for repository entry "
			     << squote(entry) << dotendl;
	}

	if (_project_data_array->length () != rev_count) {
	    pthrow prcserror << "Detected an inconsistent data file in repository entry " << squote(entry)
			     << " (possibly due to an aborted checkin), please run "
			     << squote("prcs admin rebuild")
			     << " to generate this file" << dotendl;
	}
    }

    _rfl = new RepFreeFilename(Rep_entry_path(), _rep_mask);

    _compressed = fs_file_exists(Rep_name_in_entry(prcs_compression_tag));

    if (_writeable) {
	_log_stream = new filebuf;
	if (!_log_stream->open(Rep_name_in_entry("prcs_log"), ios::out|ios::app))
	    pthrow prcserror << "Failed opening repository log file" << perror;
	_log_pstream = new PrettyStreambuf(_log_stream, NULL);
	_log_pstream->set_fill_width (1<<20);
	_log_pstream->set_fill_prefix ("log: ");
	_log = new PrettyOstream (_log_pstream, NoError);
    }

    return NoError;
}

static PrVoidError uncompress_file(const char* name, const void* data)
{
    static bool once = true;
    const char* basename = strip_leading_path(name);
    int len = strlen(basename);
    ArgList* args;

    if (len > 4 && strcmp(".gz", basename + len - 3) == 0) {
	if (option_report_actions)
	    pthrow prcserror << "You may not perform this command on a compressed repository with -n" << dotendl;

	if(once) {
	    prcsinfo << "Uncompressing repository entry "
		     << (const char*)data << dotendl;
	    once = false;
	}

	Return_if_fail(args << gzip_command.new_arg_list());

	args->append("-d");
	args->append("-f");
	args->append(name);

	Return_if_fail_if_ne(gzip_command.open_stdout(), 0) {
	    pthrow prcserror << "Gzip returned non-zero status on file "
			    << squote (name)
			    << ", aborting" << dotendl;
	}
    }

    return NoError;
}

static PrVoidError compress_file(const char* name, const void* data)
{
    static bool once = true;
    const char* basename = strip_leading_path(name);
    int len = strlen(basename);
    Dstring prjname((const char*)data);
    ArgList* args;

    prjname.append(".prj,v");

    if(len > 3 && strcmp(",v", basename + len - 2) == 0 &&
       strcmp(basename, prjname) != 0) {
	if (option_report_actions)
	    pthrow prcserror << "You may not perform this command on a compressed repository with -n" << dotendl;

	if(once) {
	    prcsinfo << "Compressing repository entry "
		     << (const char*)data << dotendl;
	    once = false;
	}

	Return_if_fail(args << gzip_command.new_arg_list());

	args->append("-f");
	args->append(name);

	Return_if_fail_if_ne(gzip_command.open_stdout(), 0) {
	    pthrow prcserror << "Gzip returned non-zero status on file "
			    << squote (name)
			    << ", aborting" << dotendl;
	}
    }

    return NoError;
}

PrVoidError RepEntry::Rep_uncompress_all_files()
{
    ASSERT(_writeable, "you need a write lock");

    Return_if_fail (directory_recurse(Rep_entry_path(),
				      _entry_name.cast(),
				      uncompress_file));

    return NoError;
}

PrVoidError RepEntry::Rep_compress_all_files()
{
    if(!Rep_needs_compress())
	return NoError;

    ASSERT(_writeable, "you need a write lock");

    Return_if_fail (directory_recurse(Rep_entry_path(),
				      _entry_name.cast(),
				      compress_file));

    return NoError;
}

/*
 * Rep_prepare_file --
 *
 *     this procedure has to copy the file to a temp location in order
 *     to avoid writing the repository if the file is compressed and
 *     the repository is only read-only.  */
PrConstCharPtrError RepEntry::Rep_prepare_file(const char* file)
{
    static Dstring ret;
    Dstring fullname, zipname;
    ArgList *args;

    ASSERT(strlen(file) > 0, "no empty files");

    Rep_name_in_entry(file, &fullname);
    Rep_name_in_entry(file, &zipname);

    fullname.append(",v");
    zipname.append(",v.gz");

    if(fs_file_exists(zipname)) {
	Return_if_fail(args << gzip_command.new_arg_list());

	if(_writeable) {
	    args->append("-f");
	    args->append("-d");
	    args->append(zipname);

	    Return_if_fail_if_ne(gzip_command.open_stdout(), 0)
		pthrow prcserror << "Gzip returned non-zero status, aborting" << dotendl;
	} else {
	    const char* temp = make_temp_file(",v");

	    args->append("-f");
	    args->append("-d");
	    args->append("-c");
	    args->append(zipname);

	    Return_if_fail(gzip_command.open(true, false));

	    Return_if_fail(fs_write_filename(gzip_command.standard_out(), temp));

	    Return_if_fail_if_ne(gzip_command.close(), 0)
		pthrow prcserror << "Gzip returned non-zero status, aborting" << dotendl;

	    _rep_comp_cache->append (temp); /* this memory is ours to
					     * free, except that it's
					     * part of a Dstring
					     * segment and if you ever
					     * clamp down on the stuff
					     * allocated by
					     * make_temp_file this'll
					     * break.*/

	    ret.assign(temp);

	    return ret.cast();
	}
    }

    if(!fs_file_exists(fullname))
	pthrow prcserror << "Repository file " << squote(fullname)
			 << " not found--try running " << squote("prcs admin rebuild")
			 << " for a complete diagnosis" << dotendl;

    ret.assign(fullname);

    return ret.cast();
}

void RepEntry::Rep_clear_compressed_cache (void)
{
    foreach (ch_ptr, _rep_comp_cache, CharPtrArray::ArrayIterator) {
	unlink (*ch_ptr);
	delete (*ch_ptr);
    }

    _rep_comp_cache->truncate(0);
}

bool RepEntry::Rep_file_exists(const char* name)
{
    Dstring d;
    Rep_name_in_entry(name, &d);
    if(fs_file_exists(d)) return true;
    d.append(".gz");
    if(fs_file_exists(d)) return true;
    return false;
}

const char* RepEntry::Rep_name_in_entry(const char* name)
{
    _base.truncate(_baselen);
    _base.append('/');
    _base.append(name);
    return _base;
}

void RepEntry::Rep_name_in_entry(const char* name, Dstring * buf)
{
    _base.truncate(_baselen);
    buf->assign(_base);
    buf->append('/');
    buf->append(name);
}

static PrVoidError copy_change_ownership(const char* name)
{
    Dstring temp (name);

    make_temp_file_same_dir (&temp);

    Return_if_fail(fs_copy_filename(name, temp));

    If_fail(Err_rename(temp, name))
	pthrow prcserror << "Rename " << squote(temp) << " to " << squote(name)
			<< " failed while changing group -- correct this manually!" << perror;

    return NoError;
}

static PrVoidError set_ugrid(const char* name, uid_t uid, gid_t gid, struct stat *buf)
{
    if (buf->st_uid != uid && get_user_id() != 0)
	Return_if_fail(copy_change_ownership(name));

    If_fail (Err_chown(name, uid, gid))
	pthrow prcserror << "Chown failed on " << squote(name) << perror;

    return NoError;
}

struct RepChownCallbackData {
    gid_t gid;
    uid_t uid;
};

PrVoidError Rep_chown_dir_callback (const char* name, const void* v_data)
{
    struct stat buf;
    RepChownCallbackData* data = (RepChownCallbackData*) v_data;

    If_fail (Err_stat(name, &buf))
	pthrow prcserror << "Stat failed on " << squote(name) << perror;

    if (buf.st_uid != data->uid || buf.st_gid != data->gid) {
	/* No copying here, only the owner or root can do it. */
	If_fail (Err_chown(name, data->uid, data->gid))
	    pthrow prcserror << "Chown failed on " << squote(name) << perror;
    }

    return NoError;
}

PrVoidError Rep_chown_callback (const char* name, const void* v_data)
{
    struct stat buf;
    RepChownCallbackData* data = (RepChownCallbackData*) v_data;

    If_fail (Err_stat(name, &buf))
	pthrow prcserror << "Stat failed on " << squote(name) << perror;

    if (buf.st_uid != data->uid || buf.st_gid != data->gid)
	Return_if_fail(set_ugrid(name, data->uid, data->gid, &buf));

    return NoError;
}

PrVoidError RepEntry::Rep_chown_file (const char* desc)
{
    Dstring full_name(Rep_name_in_entry (desc));

    full_name.append (",v");

    If_fail(Err_chmod (full_name, (0777 ^ _rep_mask) & 0444))
	pthrow prcserror << "Chown failed on " << full_name << perror;

    return NoError;
}

PrVoidError RepEntry::Rep_chown (const char* user, const char* group)
{
    struct group* grp;
    struct passwd* pwd;
    RepChownCallbackData data;

    grp = getgrnam(group);

    if (!grp)
        pthrow prcserror << "Cannot lookup group " << squote(group) << perror;

    pwd = getpwnam(user);

    if (!pwd)
        pthrow prcserror << "Cannot lookup user " << squote(user) << perror;

    data.gid = grp->gr_gid;
    data.uid = pwd->pw_uid;

    const char* dir_name = Rep_name_in_repository(_entry_name);

    If_fail (Err_chown(dir_name, data.uid, data.gid))
	pthrow prcserror << "Chown failed on " << squote(dir_name) << perror;

    Return_if_fail (directory_recurse_dirs (dir_name, &data, Rep_chown_dir_callback));

    Return_if_fail (directory_recurse (dir_name, &data, Rep_chown_callback));

    return NoError;
}

#define DO_CHMOD_REP(f, m2) do {                                   \
        const char* file = (f);                                    \
	const mode_t m = (m2);                                     \
	If_fail(Err_chmod(file, m))                                \
	    pthrow prcserror << "Chmod failed on repository entry " \
			    << squote(file) << perror;             \
	} while (false)

PrVoidError Rep_chmod_dir_callback (const char* name, const void* data)
{
    DO_CHMOD_REP (name, (* (mode_t*) data) | S_ISGID);

    return NoError;
}

PrVoidError Rep_chmod_callback (const char* name, const void* data)
{
    if (strncmp (strip_leading_path (name), "prcs_", 5) != 0)
	DO_CHMOD_REP (name, * (mode_t*) data & 0444);

    return NoError;
}

PrVoidError RepEntry::Rep_chmod (mode_t mode)
{
    DO_CHMOD_REP (Rep_name_in_entry(prcs_tagname), 0444 & mode);

    if (_compressed)
	DO_CHMOD_REP (Rep_name_in_entry(prcs_compression_tag), 0444 & mode);

    DO_CHMOD_REP (Rep_name_in_entry("prcs_log"), 0666 & mode);

    DO_CHMOD_REP (Rep_name_in_entry(prcs_data_file_name), 0666 & mode);

    const char* dir_name = Rep_name_in_repository(_entry_name);

    Return_if_fail (directory_recurse_dirs (dir_name, &mode, Rep_chmod_dir_callback));

    Return_if_fail (directory_recurse (dir_name, &mode, Rep_chmod_callback));

    return NoError;
}

#undef DO_CHMOD_REP

void fix_file_name (const char* filename, Dstring* safe_file_name)
{
    safe_file_name->assign(filename);

    if(safe_file_name->length() > 10)
	safe_file_name->truncate(10);
    else
	while(safe_file_name->length() < 2)
	    safe_file_name->append('x');

    if(safe_file_name->length() > 1 &&
       strcmp(filename + safe_file_name->length() - 2, ",v") == 0)
	safe_file_name->append('x');

    if(safe_file_name->length() > 2 &&
       strcmp(filename + safe_file_name->length() - 3, ".gz") == 0)
	safe_file_name->append('x');
}

PrVoidError RepFreeFilename::find_next (const char* base, Dstring* fill)
{
    Dstring safe_file_name;

    fix_file_name (base, &safe_file_name);

here:

    for (int i = 0; i < MAX_REPOSITORY_COUNT; i += 1) {

	if (_available[i]) {

	    fill->sprintf ("%s%d_%s,v", _offset, i, safe_file_name.cast());

	    _available[i] = false;

	    return NoError;
	}
    }

    _directory += 1;

    int val = _directory;
    int degree_max = 1;
    int index = 2;

    while (MAX_REPOSITORY_COUNT * degree_max <= _directory) {
	degree_max *= MAX_REPOSITORY_COUNT;
	index += 2;
    }

    _offset[index] = 0;
    _offset[index-1] = '/';

    index -= 2;

    while (degree_max >= 1) {
	int c = val / degree_max;

	_offset[index] = radix[c];

	if (index > 0)
	    _offset[index-1] = '/';

	index -= 2;

	val -= c * degree_max;

	degree_max /= 52;
    }

    Return_if_fail(real_open_dir(_offset));

    goto here;

    return NoError;
}

PrVoidError RepFreeFilename::real_open_dir (const char* dirname)
{
    Dstring fullname(_root);

    fullname.append('/');
    fullname.append(dirname);

    fullname.truncate(fullname.length() - 1);

    if (!fs_is_directory(fullname)) {

	Dstring fullname_x (_root);
	const char *p = dirname;

	do {
	    fullname_x.append ('/');
	    fullname_x.append (*p);

	    p += 2;

	    if (!fs_is_directory(fullname_x)) {
	        Umask mask (_umask);

		If_fail(Err_mkdir(fullname_x, 0777 | S_ISGID))
		    pthrow prcserror << "Cannot create repository subdirectory "
				    << squote(fullname_x) << perror;
	    }
	} while (*p);
    }

    Dir dir (fullname);

    for(int i = 0; i < MAX_REPOSITORY_COUNT; i += 1)
	_available[i] = true;

    foreach(ent_ptr, dir, Dir::DirIterator) {
	const char* ent = *ent_ptr;

	if (isdigit(ent[0])) {
	    char* ent_num_end = NULL;
	    long num = strtol(ent, &ent_num_end, 10);

	    if (ent_num_end[0] == '_' && num >= 0 && num < MAX_REPOSITORY_COUNT)
		_available[num] = false;
        }
    }

    if (! dir.OK() )
	pthrow prcserror << "Error reading repository directory " << squote(fullname)
			<< perror;

    return NoError;
}

PrVoidError RepEntry::Rep_new_filename(const char* filename, Dstring* buf)
{
    Dstring gzname;

    do {
	Return_if_fail(_rfl->find_next(filename, buf));
	gzname.assign(*buf);
	gzname.append(".gz");
    } while (fs_file_exists(Rep_name_in_entry(buf->cast())) ||
	     fs_file_exists(Rep_name_in_entry(gzname.cast())) ||
	     _rcs_file_table->isdefined(buf->cast()));

    buf->truncate (buf->length() - 2); /* Remove ,v */

    return NoError;
}

/* Lookup project data. */

ProjectVersionData* RepEntry::lookup_version(const char* major, int minorint) const
{
    for(int i = version_count() - 1; i >= 0; i -= 1) {
	if (minorint == _project_data_array->index(i)->prcs_minor_int() &&
	    strcmp(_project_data_array->index(i)->prcs_major(), major) == 0) {
	    return _project_data_array->index(i);
	}
    }

    return NULL;
}

ProjectVersionData* RepEntry::lookup_version(const char* major, const char* minor) const
{
    return lookup_version (major, atoi(minor));
}

ProjectVersionData* RepEntry::lookup_version(ProjectDescriptor *project) const
{
    return lookup_version (project->project_version_major()->cast(),
			   project->project_version_minor()->cast());
}

int RepEntry::highest_major_version() const /* -1 if none */
{
    int max = -1;

    for(int i = 0; i < version_count(); i += 1) {
	if (VC_check_token_match(_project_data_array->index(i)->prcs_major(), "number")) {
	    int val = atoi(_project_data_array->index(i)->prcs_major());

	    if(val > max)
		max = val;
	}
    }

    return max;
}

ProjectVersionData* RepEntry::highest_minor_version_data(const char* major, bool may_be_deleted) const
{
    int max = 0;
    ProjectVersionData* data = NULL;

    for(int i = 0; i < version_count(); i += 1) {
	ProjectVersionData* pd = _project_data_array->index(i);

	if (strcmp(pd->prcs_major(), major) == 0) {

	    if (! may_be_deleted && pd->deleted ()) {
		continue;
	    }

	    int val = pd->prcs_minor_int();

	    if(val > max) {
		data = pd;
		max  = val;
	    }
	}
    }

    return data;
}


int RepEntry::highest_minor_version(const char* major, bool may_be_deleted) const /* 0 if none */
{
    ProjectVersionData* data = highest_minor_version_data(major, may_be_deleted);

    if(!data)
	return 0;

    return data->prcs_minor_int();
}

bool RepEntry::major_version_exists(const char* major) const
{
    for(int i = 0; i < version_count(); i += 1) {
	if (strcmp(_project_data_array->index(i)->prcs_major(), major) == 0) {
	    return true;
	}
    }

    return false;
}

void RepEntry::common_version_dfs_1(ProjectVersionData* node) const
{
    DEBUG ("node: " << node->version_index());

    if (node->flag1(true))
	return;

    for (int i = 0; i < node->parent_count(); i += 1)
	common_version_dfs_1 (project_summary()->index(node->parent_index(i)));
}

void RepEntry::common_version_dfs_2(ProjectVersionData* node,
				    ProjectVersionDataPtrArray* res) const
{
    DEBUG ("node: " << node->version_index());

    if (node->flag2(true))
      return;

    if (node->flag1())
      {
	res->append (node);
	return;
      }

    for (int i = 0; i < node->parent_count(); i += 1)
	common_version_dfs_2 (project_summary()->index(node->parent_index(i)), res);
}

void RepEntry::common_version_dfs_3(ProjectVersionData* node) const
{
    DEBUG ("node: " << node->version_index());

    /* Like CVD_1 but don't set this node, only it's parents. */

    for (int i = 0; i < node->parent_count(); i += 1)
	common_version_dfs_1 (project_summary()->index(node->parent_index(i)));
}

void RepEntry::clear_flags() const
{
    foreach (pvd_ptr, project_summary(), ProjectVersionDataPtrArray::ArrayIterator)
	(*pvd_ptr)->clear_flags();
}

ProjectVersionDataPtrArray* RepEntry::common_version(ProjectVersionData* one,
						     ProjectVersionData* two) const
{
    if (one == NULL || two == NULL)
	return NULL;

    ProjectVersionDataPtrArray* tmppvda = new ProjectVersionDataPtrArray;

    /* Reset flags */
    one->clear_flags();
    two->clear_flags();
    clear_flags ();

    /* Mark all versions visible from one */
    common_version_dfs_1 (one);

    /* Fill tmppvda with possible common versions */
    common_version_dfs_2 (two, tmppvda);

    /* Reset flags */
    clear_flags ();

    /* Mark all parents of all versions in tmppvda. */
    foreach (pvd_ptr, tmppvda, ProjectVersionDataPtrArray::ArrayIterator)
      common_version_dfs_3 (*pvd_ptr);

    /* Create the real array. */
    ProjectVersionDataPtrArray* pvda = new ProjectVersionDataPtrArray;

    /* Transfer all non-marked versions in tmppvda to pvda. */
    foreach (pvd_ptr, tmppvda, ProjectVersionDataPtrArray::ArrayIterator)
      {
	ProjectVersionData *pvd = *pvd_ptr;

	if (!pvd->flag1())
	  pvda->append (pvd);
      }

    delete tmppvda;

    return pvda;
}

bool RepEntry::common_version_dfs_4(ProjectVersionData* from,
				    ProjectVersionData* to,
				    ProjectVersionDataPtrArray* res) const
{
    bool ret = false;

    if (from->flag1(true))
	return from == to;

    for (int i = 0; i < from->parent_count(); i += 1) {
	ProjectVersionData *child = project_summary()->index(from->parent_index(i));

	if (common_version_dfs_4 (child, to, res)) {
	    if (!child->flag2(true)) {
		res->append (child);
	    }
	    ret = true;
	}
    }

    return ret;
}

static ProjectVersionDataPtrArray* reverse_pvda (ProjectVersionDataPtrArray* a)
{
    ProjectVersionDataPtrArray* reversed = new ProjectVersionDataPtrArray;

    for (int i = a->length () - 1; i >= 0; i -= 1) {
	reversed->append (a->index (i));
    }

    delete a;

    return reversed;
}

ProjectVersionDataPtrArray* RepEntry::common_lineage (ProjectVersionData* one,
						      ProjectVersionData* two) const
{
    /* This returns all versions that are in the ancestry lineage of
     * both versions.  The return should be ordered such that if one
     * or two is the ancestor of one another, then reversing them with
     * reverse the order. */
    ASSERT (one && two, "need both");

    ProjectVersionDataPtrArray* result = new ProjectVersionDataPtrArray;

    /* Mark all versions ancestor to one */
    clear_flags ();
    common_version_dfs_1 (one);

    if (two->flag1()) {
	/* Two is an ancestor of one, mark all ancestors of two instead. */
	clear_flags ();
	common_version_dfs_1 (two);
	/* Fill the array with versions starting at one, ending at two. */
	common_version_dfs_4 (one, two, result);
	result->append (one);
	return reverse_pvda (result);
    }

    /* Mark all versions ancestor to two */
    clear_flags ();
    common_version_dfs_1 (two);

    if (one->flag1()) {
	/* One is an ancestor of two, mark all ancestors of one instead. */
	clear_flags ();
	common_version_dfs_1 (one);
	/* Fill the array with versions starting at one, ending at two. */
	common_version_dfs_4 (two, one, result);
	result->append (two);
	return result;
    }

    return NULL;
}

PrRcsVersionDataPtrError RepEntry::lookup_rcs_file_data(const char* filename,
							const char* version_num) const
{
    RcsDelta **lookup;

    if((lookup = _rcs_file_table->lookup(filename)) != NULL) {
	RcsVersionData* data = (*lookup)->lookup(version_num);

	if(data)
	    return data;
    }

    pthrow prcserror << "Repository inconsistency detected in repository file "
		     << squote(filename) << " at file version " << version_num
		     << ", please run " << squote("prcs admin rebuild")
		     << " to diagnose this error" << dotendl;
}

RepEntry::~RepEntry()
{
    if(_log) {
        if (_log_pstream->pubsync()) {
	    prcserror << "warning: Write to repository log failed" << perror;
	}

	delete _log;
	delete _log_pstream;
	delete _log_stream;
    }

    if (_rebuild_file)    delete _rebuild_file;
    if (_repository_lock) delete _repository_lock;
    if (_rfl)             delete _rfl;
    if (_rep_comp_cache) {
	foreach (ch_ptr, _rep_comp_cache, CharPtrArray::ArrayIterator)
	    delete (*ch_ptr);
	delete _rep_comp_cache;
    }
}

void RepEntry::add_project_data(ProjectVersionData* new_data)
{
    if(!option_report_actions)
	_rebuild_file->add_project_data(new_data);
}

void RepEntry::add_rcs_file_data(const char* filename, RcsVersionData* new_data)
{
    if(!option_report_actions)
	_rebuild_file->add_rcs_file_data(filename, new_data);
}

PrVoidError RepEntry::close_repository()
{
    if(!option_report_actions) {
	Return_if_fail(_rebuild_file->update_project_data());

	delete _rebuild_file;

	_rebuild_file = NULL;
    }

    return NoError;
}

PrProjectDescriptorPtrError RepEntry::checkout_prj_file(const char* fullname,
							const char* version,
							ProjectReadData flags)
{
    FILE* cofile;
    ProjectDescriptor* project;
    Dstring filename;

    filename.sprintf("%s:%s", Rep_name_of_version_file(), version);

    Return_if_fail(cofile << VC_checkout_stream(version, Rep_name_of_version_file()));

    PrVoidError it = (project << read_project_file(fullname, filename, false, cofile, flags));

    Return_if_fail(VC_close_checkout_stream(cofile, version, Rep_name_of_version_file()));

    Return_if_fail (it);

    project->repository_entry(this);

    return project;
}

PrProjectDescriptorPtrError RepEntry::checkout_create_prj_file (const char *filename,
								const char *fullname,
								const char *version,
								ProjectReadData flags)
{
    ProjectDescriptor *project;

    Return_if_fail(project << checkout_prj_file(fullname, version, flags));

    Return_if_fail(project->write_project_file(filename));

    return project;
}

PrVoidError RepEntry::Rep_alarm() const
{
    if (_repository_lock && _repository_lock->touch_lock() != LOCK_SUCCESS)
	prcsinfo << "Failed refreshing repository lock, something bad "
	    "is happening" << dotendl;

    return NoError;
}

static void print_user_access(int r, int w, int x)
{
    if(!r && !w && !x) {
	prcsoutput << "no permission" << prcsendl;
    } else if (r && w && x) {
	prcsoutput << "read/write" << prcsendl;
    } else if (x && r) {
	prcsoutput << "read only" << prcsendl;
    } else {
	prcsoutput << "neither read nor write -- fix me" << prcsendl;
    }
}

static void print_access(struct stat *buf, const char *pwd_name, const char *grp_name)
{
    prcsoutput << "Repository entry owner:  " << pwd_name << prcsendl;
    prcsoutput << "Repository entry group:  " << grp_name << prcsendl;
    prcsoutput << "Repository entry access: " << prcsendl;
    prcsoutput << "Owner:                   ";
    print_user_access(buf->st_mode & S_IRUSR,
		      buf->st_mode & S_IWUSR,
		      buf->st_mode & S_IXUSR);
    prcsoutput << "Group:                   ";
    print_user_access(buf->st_mode & S_IRGRP,
		      buf->st_mode & S_IWGRP,
		      buf->st_mode & S_IXGRP);
    prcsoutput << "Other:                   ";
    print_user_access(buf->st_mode & S_IROTH,
		      buf->st_mode & S_IWOTH,
		      buf->st_mode & S_IXOTH);
}

PrIntError RepEntry::Rep_print_access()
{
    struct passwd *pwd;
    struct group *grp;

    pwd = getpwuid(_rep_stat_buf.st_uid);
    grp = getgrgid(_rep_stat_buf.st_gid);

    _owner_name = p_strdup((pwd && pwd->pw_name) ? pwd->pw_name : "nouser");
    _group_name = p_strdup((grp && grp->gr_name) ? grp->gr_name : "nogroup");

    print_access(&_rep_stat_buf, _owner_name, _group_name);

    return _rep_stat_buf.st_mode;
}

static PrVoidError can_set_group(const char* group_name, const char *current_group_name)
{
    /* The reason for this function is just to give a nice error
     * message.  If you wait until chmod then you get "Operation not
     * permitted".  */
    if (strcmp(group_name, current_group_name) != 0) {

	struct group *grp;

	if((grp = getgrnam(group_name)) == NULL)
	    pthrow prcserror << "Can't lookup group " << squote(group_name) << dotendl;

	char** grp_members = grp->gr_mem;

	if (get_user_id() == 0)
	    return NoError;

	while(*grp_members) {
	    if(strcmp(get_login(), *grp_members++) == 0) {
		return NoError;
	    }
	}

	/* Linux (at least) doesn't return users whose default group
	 * is this group, check that too. */
	struct passwd *pwd = getpwuid (get_user_id ());

	if (pwd && pwd->pw_gid == grp->gr_gid) {
	    return NoError;
	}

	pthrow prcserror << "You are not a member of that group" << dotendl;
    }

    return NoError;
}

static PrIntError query_mask(mode_t old_mode)
{
    int mask = 0;

    mask |= 0077; /* clear group/other read/write */

    prcsquery << "Group members' access, ";

    if (old_mode & 0020)
	prcsquery << force("read/write")
		  << report("read/write")
		  << option('r', "Allow group read access")
		  << defopt('w', "Allow group read/write access")
		  << option('n', "Disallow group access");
    else if (old_mode & 0010)
	prcsquery << force("read only")
		  << report("read only")
		  << defopt('r', "Allow group read access")
		  << option('w', "Allow group read/write access")
		  << option('n', "Disallow group access");
    else
	prcsquery << force("read only")
		  << report("read only")
		  << option('r', "Allow group read access")
		  << option('w', "Allow group read/write access")
		  << defopt('n', "Disallow group access");


    prcsquery << query("select");

    char c;

    Return_if_fail(c << prcsquery.result());

    if (c == 'r')
	mask &= 0727;
    else if (c == 'w')
	mask &= 0707;

    prcsquery << "Other users' access, ";

    if (old_mode & 0002)
	prcsquery << force("read/write")
		  << report("read/write")
		  << option('r', "Allow other users read access")
		  << option('n', "Disallow other users access")
		  << defopt('w', "Allow other users read/write access");
    else if (old_mode & 0001)
	prcsquery << force("read only")
		  << report("read only")
		  << defopt('r', "Allow other users read access")
		  << option('n', "Disallow other users access")
		  << option('w', "Allow other users read/write access");
    else
	prcsquery << force("none")
		  << report("none")
		  << option('r', "Allow other users read access")
		  << defopt('n', "Disallow other users access")
		  << option('w', "Allow other users read/write access");

    prcsquery << query("select");

    Return_if_fail(c << prcsquery.result());

    if (c == 'r')
	mask &= 0772;
    else if(c == 'w')
	mask &= 0770;

    return mask;
}

const char* RepEntry::Rep_entry_path()
{
    return Rep_name_in_repository(_entry_name);
}


RepFreeFilename::RepFreeFilename(const char* name, int umask)
{
    _umask = umask;
    _directory = 0;
    _root.assign (name);
    real_open_dir ("");
    memset (_offset, 0, sizeof (_offset));
}

PrettyOstream& RepEntry::Rep_log() const
{
    ASSERT(_writeable, "only a writeable rep");

    (*_log) << get_login() << " " << getpid() << " " << get_host_name()
	    << " " << get_utc_time() << " " << prcs_version_string << ": ";

    return *_log;
}

bool RepEntry::Rep_needs_compress() const { return _writeable && _compressed; }
mode_t RepEntry::Rep_get_umask() const { return _rep_mask; }
void RepEntry::Rep_set_compression(bool c) { _compressed = c; }
const char* RepEntry::Rep_name_of_version_file() const { return _project_file.cast(); }
int RepEntry::version_count() const { return _project_data_array->length(); }
ProjectVersionDataPtrArray* RepEntry::project_summary() const { return _project_data_array; }
void RepEntry::project_summary(ProjectVersionDataPtrArray* pda0) { _project_data_array = pda0; }
RcsFileTable* RepEntry::rcs_file_summary() const { return _rcs_file_table; }
void RepEntry::rcs_file_summary(RcsFileTable* rft0) { _rcs_file_table = rft0; }
const char* RepEntry::Rep_owner_name() const { return _owner_name; }
const char* RepEntry::Rep_group_name() const { return _group_name; }

/*
 * Compression is implemented by lazily uncompressing repository files
 * when they are needed an compressing all uncompressed files upon
 * exiting.  Therefore, the compress_command and uncompress_command
 * need only touch and remove the file COMPRESSION_TAG in the repository
 * to do this.  The code that compresses the directory is called from
 * clean_up() in prcs.cc.
 */
PrPrcsExitStatusError admin_compress_command()
{
    RepEntry* rep_entry;

    Return_if_fail(rep_entry << Rep_init_repository_entry(cmd_root_project_name, true, false, true));

    if(!rep_entry->Rep_needs_compress()) {

	Umask mask (rep_entry->Rep_get_umask());

	if(creat(rep_entry->Rep_name_in_entry(prcs_compression_tag), 0444) < 0)
	    pthrow prcserror << "Failed writing repository tagfile" << perror;

	rep_entry->Rep_set_compression(true);
    }

    return ExitSuccess;
}

PrPrcsExitStatusError admin_uncompress_command()
{
    RepEntry* rep_entry;

    Return_if_fail(rep_entry << Rep_init_repository_entry(cmd_root_project_name, true, false, true));

    if(!rep_entry->Rep_needs_compress()) {
	if(option_immediate_uncompression) {
	    Return_if_fail(rep_entry->Rep_uncompress_all_files());
	} else {
	    prcsinfo << "Repository entry " << cmd_root_project_name
		     << "is already uncompressed.  Use -i to immediately decompress "
		"the entire repository" << dotendl;
	}

	return ExitSuccess;
    }

    If_fail(Err_unlink(rep_entry->Rep_name_in_entry(prcs_compression_tag))) {
	pthrow prcserror << "Failed unlinking repository compression tag " << perror;
    }

    if(option_immediate_uncompression) {
	Return_if_fail(rep_entry->Rep_uncompress_all_files());
    } else {
	prcsinfo << "Repository lazily uncompressed.  Actual decompression takes "
	    "place the next time you access each file of the project" << dotendl;
    }

    rep_entry->Rep_set_compression(false);

    return ExitSuccess;
}

PrPrcsExitStatusError admin_access_command()
{
    int mask = 0;
    const char* group_name, *user_name;
    int uid = get_user_id();
    int old_mode;

    if (cmd_root_project_name) {
	RepEntry* rep_entry;

	prcsinfo << "Setting access permissions for project " << squote(cmd_root_project_name)
		 << dotendl;

	Return_if_fail(rep_entry <<
		       Rep_init_repository_entry(cmd_root_project_name, true, false, true));

	Return_if_fail(old_mode << rep_entry->Rep_print_access());

	if (option_report_actions)
	    return ExitSuccess;

	if (! fs_same_file_owner(rep_entry->Rep_entry_path()) && uid != 0) {
	    pthrow prcserror <<"Only the owner of this repository entry may modify its "
		"access permissions" << dotendl;
	}

	if (uid == 0) {
	    prcsquery << "Enter a user"
		      << definput(rep_entry->Rep_owner_name())
		      << string_query("");

	    Return_if_fail(user_name << prcsquery.string_result());
	} else {
	    user_name = get_login();
	}

	prcsquery << "Enter a group"
		  << definput(rep_entry->Rep_group_name())
		  << string_query("");

	Return_if_fail (group_name << prcsquery.string_result());

	Return_if_fail (can_set_group(group_name, rep_entry->Rep_group_name()));

	Return_if_fail (mask << query_mask(old_mode));

	Return_if_fail(rep_entry->Rep_chown(user_name, group_name));

	Return_if_fail(rep_entry->Rep_chmod(0777 ^ mask));

    } else {
	/* Set permissions for repository */
	Return_if_fail(Rep_init_repository());

	const char* rep_name = Rep_name_in_repository("");

	prcsinfo << "Setting access permissions for repository "
		 << rep_name << dotendl;

	struct passwd *pwd;
	struct group *grp;
	const char* group_name, *owner_name, *new_group_name;
	struct stat stat_buf;

	If_fail(Err_stat(Rep_name_in_repository(""), &stat_buf))
	    pthrow prcserror << "Stat failed on " << squote(rep_name) << perror;

	pwd = getpwuid(stat_buf.st_uid);
	grp = getgrgid(stat_buf.st_gid);

	owner_name = p_strdup((pwd && pwd->pw_name) ? pwd->pw_name : "nouser");
	group_name = p_strdup((grp && grp->gr_name) ? grp->gr_name : "nogroup");

	print_access(&stat_buf, owner_name, group_name);

	if(option_report_actions)
	    return ExitSuccess;

	if(!fs_same_file_owner(rep_name) && uid != 0) {
	    pthrow prcserror <<"Only the owner of this repository may modify its "
		"access permissions" << dotendl;
	}

	if (uid == 0) {
	    prcsquery << "Enter a user"
		      << definput(owner_name)
		      << string_query("");

	    Return_if_fail(user_name << prcsquery.string_result());
	} else {
	    user_name = get_login();
	}

	prcsquery << "Enter a group"
		  << definput(group_name)
		  << string_query("");

	Return_if_fail(new_group_name << prcsquery.string_result());

	Return_if_fail(can_set_group(new_group_name, group_name));

	Return_if_fail(mask << query_mask(stat_buf.st_mode));

	If_fail(Err_chmod(rep_name, (0777 ^ mask) | S_ISGID | 01000))
	    pthrow prcserror << "Chmod failed on " << squote(rep_name) << perror;

	const char* rep_lock_name = Rep_name_in_repository(prcs_lock_dir);

	If_fail(Err_chmod(rep_lock_name, 0777 ^ mask))
	    pthrow prcserror << "Chmod failed on " << squote(rep_lock_name) << perror;

    }

    return ExitSuccess;
}

PrPrcsExitStatusError admin_init_command()
{
    RepEntry* rep_entry;

    Return_if_fail(rep_entry <<
		   Rep_init_repository_entry(cmd_root_project_name, true, true, true));

    rep_entry->Rep_print_access();

    return ExitSuccess;
}

static PrVoidError repository_cleanup_handler(void* data, bool signaled)
{
    RepEntry* rep_entry;

    rep_entry = (RepEntry*) data;

    if(signaled) {
	if(rep_entry->Rep_needs_compress()) {
	    prcswarning << "Repository entry " << rep_entry->Rep_entry_path()
			<< " not recompressed" << dotendl;
	}
    } else if (! option_report_actions) {
	rep_entry->Rep_compress_all_files();
    }

    rep_entry->Rep_unlock_repository();

#ifdef PURIFY
    delete rep_entry; /* About to exit, not worth it. */
#endif

    return NoError;
}

static PrVoidError repository_alarm_handler(void* data, bool)
{
    RepEntry* rep_entry;

    rep_entry = (RepEntry*) data;

    Return_if_fail(rep_entry->Rep_alarm());

    return NoError;
}
