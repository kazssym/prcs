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
#include <pwd.h>
#include <utime.h>
}

#include "prcs.h"

#include "projdesc.h"
#include "fileent.h"
#include "repository.h"
#include "misc.h"
#include "system.h"
#include "vc.h"


FileEntry::FileEntry (Dstring *working_path,
		      const ListMarker &ent_marker,
		      const PrcsAttrs *attrs,
		      Dstring *desc_ver,
		      Dstring *desc_name,
		      Dstring *link_name,
		      mode_t mode)
    :_stat_mode(0),
     _stat_mtime(-1),
     _stat_size(0),
     _stat_ino(0),
     _unkeyed_length(0),
     _unmodified(false),
     _present(false),
     _file_mode(mode),
     _on_command_line(true),
     _ent_marker (ent_marker),
     _user (NULL),
     _date (NULL),
     _link_name (link_name),
     _work_path (working_path),
     _rcs_version_string (desc_ver),
     _descriptor_name (desc_name),
     _temp_path (NULL),
     _full_rep_path (NULL),
     _attrs (attrs) { }

const char* FileEntry::descriptor_name() const
    { return _descriptor_name ? _descriptor_name->cast() : NULL; }
const char* FileEntry::descriptor_version_number() const
    { return _rcs_version_string ? _rcs_version_string->cast() : NULL; }
const char* FileEntry::full_descriptor_name()const
    { return _full_rep_path ? _full_rep_path->cast() : NULL; }
const char* FileEntry::working_path() const { return _work_path ? _work_path->cast() : NULL; }
const char* FileEntry::temp_file_path() const { return _temp_path ? _temp_path->cast() : NULL; }
const char* FileEntry::link_name() const { return _link_name ? _link_name->cast() : NULL; }
const char* FileEntry::last_mod_date() const { return _date ? _date->cast() : NULL; }
const char* FileEntry::last_mod_user() const { return _user ? _user->cast() : NULL; }

const PrcsAttrs* FileEntry::file_attrs()   const { return _attrs; }
ProjectDescriptor* FileEntry::project() const { return _attrs->project(); }
bool     FileEntry::keyword_sub()   const { return _attrs->keyword_sub(); }
FileType FileEntry::file_type()     const { return _attrs->type(); }

const ListMarker* FileEntry::ent_marker()  const { return &_ent_marker; }
mode_t   FileEntry::file_mode()     const { return _file_mode; }
mode_t   FileEntry::stat_permissions() const { return _stat_mode & 0x1ff; }
mode_t   FileEntry::stat_mode()     const { return _stat_mode; }
off_t    FileEntry::stat_length()   const { return _stat_size; }
time_t   FileEntry::stat_mtime()    const { return _stat_mtime; }
ino_t    FileEntry::stat_inode()    const { return _stat_ino; }

int         FileEntry::plus_lines()       const { return _plus_lines; }
int         FileEntry::minus_lines()      const { return _minus_lines; }

void        FileEntry::set_lines (int plus, int minus)
{
    _plus_lines = plus;
    _minus_lines = minus;
}


bool        FileEntry::unmodified()       const { return _unmodified; }
bool        FileEntry::present()          const { return _present; }
bool        FileEntry::on_command_line()  const { return _on_command_line; }
bool        FileEntry::real_on_cmd_line() const
    { return _on_command_line && file_type() == RealFile; }
int         FileEntry::unkeyed_length()     const { return _unkeyed_length; }
const char* FileEntry::checksum()         const { return _checksum; }

void FileEntry::set_on_command_line(bool b) { _on_command_line = b; }
void FileEntry::set_unmodified(bool b)      { _unmodified = b; }
void FileEntry::set_present(bool b)         { _present = b; }
void FileEntry::set_checksum(const char* c) { memcpy(_checksum, c, 16); }
void FileEntry::set_unkeyed_length(int len)   { _unkeyed_length = len; }
void FileEntry::set_file_mode(mode_t mode)  { _file_mode = mode; }
void FileEntry::set_working_path(const char* name) { _work_path->assign (name); }

void FileEntry::set (FileEntry* fe)
{
  switch (fe->file_type()) {
  case RealFile:
    set_descriptor_name(fe->descriptor_name());
    set_version_number(fe->descriptor_version_number());
    set_file_mode(fe->file_mode());
    break;
  case SymLink:
    set_link_name(fe->link_name());
    break;
  case Directory:
    break;
  }

  If_fail (_attrs << project()->intern_attrs(&fe->file_attrs()->_vals,
					     fe->file_attrs()->_ngroup,
					     working_path(),
					     false))
    ASSERT (false, "The attrs were already validated.");
}

void FileEntry::set_link_name(const char* name)
{
    if (!_link_name)
	_link_name = new Dstring;

    _link_name->assign (name);
}

FileEntry::~FileEntry()
{
    if (_full_rep_path) delete _full_rep_path;
    if (_temp_path) delete _temp_path;
    if (_user) delete _user;
    if (_date) delete _date;

    if (_link_name && _link_name->is_dstring()) delete _link_name;
    if (_work_path && _work_path->is_dstring()) delete _work_path;
    if (_rcs_version_string && _rcs_version_string->is_dstring()) delete _rcs_version_string;
    if (_descriptor_name && _descriptor_name->is_dstring()) delete _descriptor_name;
}

PrVoidError FileEntry::initialize_descriptor(RepEntry* rep,
					     bool regist,
					     bool init_temp_path)
{
    if(file_type() == Directory || file_type() == SymLink)
	return NoError;

    _full_rep_path = new Dstring;

    if (init_temp_path)
	_temp_path = new Dstring;

    if (!_descriptor_name) {
	_descriptor_name = new Dstring;
	_rcs_version_string = new Dstring;

	Return_if_fail(rep->Rep_new_filename(strip_leading_path(working_path()),
					     _descriptor_name));

	rep->Rep_name_in_entry(_descriptor_name->cast(), _full_rep_path);
	_full_rep_path->append(",v");

	if(regist)
	    Return_if_fail(VC_register(*_full_rep_path));

    } else {
	const char* vc_file_path;

	Return_if_fail(vc_file_path << rep->Rep_prepare_file(_descriptor_name->cast()));

	_full_rep_path->assign(vc_file_path);
    }

    if (init_temp_path) {
	_temp_path->assign(*_full_rep_path);
	_temp_path->truncate(_full_rep_path->length() - 2);
    }

    return NoError;
}

void FileEntry::set_descriptor_name(const char* name)
{
    if (!_descriptor_name)
	_descriptor_name = new Dstring;

    _descriptor_name->assign(name);
}

void FileEntry::set_version_number(const char* ver)
{
    if (!_rcs_version_string)
	_rcs_version_string = new Dstring;

    _rcs_version_string->assign(ver);
}

bool FileEntry::empty_descriptor() const
{
    switch (file_type()) {
    case SymLink:
	return _link_name == NULL;
    case RealFile:
	return _descriptor_name == NULL;
    default:
	return false;
    }
}

PrBoolError FileEntry::check_working_file()
{
    const char* name = working_path();

    /* see that its there and that its readable */
    if (Failure(stat()) || (file_type() == RealFile && !fs_file_readable(name))) {
	if(errno != ENOENT)
	    prcswarning << "Can't stat " << squote(name) << perror;

	return false;
    }

    switch (file_type()) {
    case Directory:
	if (!S_ISDIR(_stat_mode))
	    pthrow prcserror << "Named directory " << squote(name)
			    << " is not a directory" << dotendl;
	break;
    case SymLink:
	if (!S_ISLNK(_stat_mode))
	    pthrow prcserror << "Named symlink " << squote(name)
			    << " is not a symlink" << dotendl;
	break;
    case RealFile:
	if (!S_ISREG(_stat_mode))
	    pthrow prcserror << "Named file " << squote(name)
			    << " is not a regular file" << dotendl;
	break;
    }

    return true;
}

PrVoidError FileEntry::get_repository_info(RepEntry* rep_entry)
{
    RcsVersionData* data;

    Return_if_fail(data << rep_entry->lookup_rcs_file_data(descriptor_name(), descriptor_version_number()));

    _stat_mtime = data->date();
    _user  = new Dstring(data->author());
    _date  = new Dstring(time_t_to_rfc822(_stat_mtime));
    _plus_lines = data->plus_lines ();
    _minus_lines = data->minus_lines ();

    return NoError;
}

PrVoidError FileEntry::get_file_sys_info()
{
    If_fail(stat())
	pthrow prcserror << "Stat failed on file "
			 << squote(working_path()) << perror;

    struct passwd* pwd = getpwuid(_stat_uid);
    const char* name = pwd == NULL ? "unknown" : pwd->pw_name;

    _user = new Dstring(name);
    _date = new Dstring(time_t_to_rfc822(_stat_mtime));

    return NoError;
}

NprVoidError FileEntry::stat()
{
    struct stat buf;

    if (file_type() == SymLink) {
	if (::lstat(working_path(), &buf) < 0)
	    return NonFatalError;
    } else {
	if (::stat(working_path(), &buf) < 0)
	    return NonFatalError;
    }

    _present = true;
    _stat_mode = buf.st_mode;
    _stat_mtime = buf.st_mtime;
    _stat_size = buf.st_size;
    _stat_ino = buf.st_ino;
    _stat_uid = buf.st_uid;

    return NoError;
}

PrVoidError FileEntry::chmod(mode_t new_mode0) const
{
    mode_t new_mode = new_mode0;

    if (new_mode == 0)
	new_mode = _file_mode;

    if(!option_preserve_permissions && !new_mode0)
	/* Only clip with umask if -p and we're creating a file with
	 * _file_mode, otherwise preserve old mode. */
	new_mode &= (0777 ^ get_umask());

    If_fail(Err_chmod(working_path(), new_mode))
	prcswarning << "Chmod failed on " << squote(working_path()) << perror;

    return NoError;
}


PrVoidError FileEntry::utime(time_t mod_time) const
{
    if (mod_time > 0) {
	struct utimbuf ut;

	ut.modtime = mod_time;
	ut.actime = get_utc_time_t();

	If_fail (Err_utime(working_path(), &ut))
	    prcswarning << "Utime failed on " << squote(working_path())
			<< perror;
    }

    return NoError;
}

void describe_versionfile (const char* file, RcsVersionData *ver, Dstring &ds)
{
    ds.append (file);
    ds.append (":");
    ds.append (ver->rcs_version ());
}

void FileEntry::describe (Dstring &ds)
{
    ds.append (full_descriptor_name ());
    ds.append (":");
    ds.append (descriptor_version_number ());
}
