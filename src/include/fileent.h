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


#ifndef _FILEENT_H_
#define _FILEENT_H_

class FileEntry {

public:

    FileEntry (Dstring *working_path,
	       const ListMarker &ent_marker,
	       const PrcsAttrs *attrs,
	       Dstring *desc_ver,
	       Dstring *desc_name,
	       Dstring *link_name,
	       mode_t mode);

    ~FileEntry();

    /* Stat(), set present(), and generate error if the working
     * type is incorrect. */
    PrBoolError  check_working_file();
    PrVoidError  initialize_descriptor(RepEntry* rep,
				       bool regist,
				       bool init_temp_path);
    PrVoidError  get_repository_info(RepEntry* rep_entry);
    PrVoidError  get_file_sys_info();
    NprVoidError stat();

    PrVoidError  chmod(mode_t mode) const;
    PrVoidError  utime(time_t date) const;

    /* Prepare to write prj file */

    void update_repository(RepEntry* rep_entry) const;

    /* Accessors */

    const char* descriptor_version_number() const;
    const char* descriptor_name()           const;
    const char* full_descriptor_name()      const;
    const char* working_path()              const;
    const char* temp_file_path()            const;
    const char* link_name()                 const;
    const char* last_mod_date()             const;
    const char* last_mod_user()             const;

    const ListMarker* ent_marker()  const;

    const PrcsAttrs* file_attrs() const;

    mode_t   file_mode()     const;
    FileType file_type()     const;
    bool     keyword_sub()   const;
    mode_t   stat_mode()     const;
    mode_t   stat_permissions() const;
    off_t    stat_length()   const;
    time_t   stat_mtime()    const;
    ino_t    stat_inode()    const;

    /* Checkin Info */

    bool        unmodified()       const;
    bool        present()          const;
    bool        real_on_cmd_line() const;
    bool        on_command_line()  const;
    bool        empty_descriptor() const;
    int         unkeyed_length()   const;
    const char* checksum()         const;
    int         plus_lines()       const;
    int         minus_lines()      const;

    void set_on_command_line(bool b);
    void set_unmodified(bool b);
    void set_present(bool b);
    void set_checksum(const char* c);
    void set_unkeyed_length(int len);
    void set_file_mode(mode_t mode);
    void set_link_name(const char* c);
    void set_lines (int plus, int minus);
    void set (FileEntry *fe);
    void set_working_path (const char* name);

    void set_version_number(const char* new_version);
    void set_descriptor_name(const char* new_desc);

    void describe (Dstring &ds);

    ProjectDescriptor *project() const;

private:

    mode_t _stat_mode;
    time_t _stat_mtime;
    off_t  _stat_size;
    ino_t  _stat_ino;
    uid_t  _stat_uid;
    long _unkeyed_length;
    char _checksum[16];        /* You can get rid of this, except its 104 bytes
				* before removal, and probably won't save anything. */
    bool _unmodified;          /* True if this file is known to be unmodified from its predecessor. */
    bool _present;

    int _plus_lines;
    int _minus_lines;

    mode_t   _file_mode;
    bool _on_command_line;     /* true if this file was selected */

    ListMarker _ent_marker;

    /* set by a call to get_{filesys,repository}_info() */
    Dstring *_user;
    Dstring *_date;

    /* set if :symlink when read, otherwise in check_working_file() */
    Dstring *_link_name;

    /* set when read */
    Dstring *_work_path;
    Dstring *_rcs_version_string;
    Dstring *_descriptor_name;

    /* set by initialize_descriptor */
    Dstring *_temp_path;
    Dstring *_full_rep_path;

    /* Attrsibutes shared by many files. */
    const PrcsAttrs *_attrs;
};

void describe_versionfile (const char* file, RcsVersionData *ver, Dstring &ds);

#endif /* FILEENT_H */
