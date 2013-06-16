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
 * $Id: repository.h 1.9.1.20.1.21 Sat, 02 Feb 2002 13:07:41 -0800 jmacd $
 */


#ifndef _REPOSITORY_H_
#define _REPOSITORY_H_

/* No more than 52 */
#define MAX_REPOSITORY_COUNT 52

extern PrVoidError Rep_init_repository();
extern NprConstCharPtrError Rep_guess_repository_path();
extern PrVoidError obtain_lock(AdvisoryLock* lock, bool write_or_n_read);

extern const char prcs_lock_dir[];

PrRepEntryPtrError Rep_init_repository_entry(const char*,
					     bool writeable,
					     bool create,
					     bool require_db);

PrBoolError Rep_repository_entry_exists(const char* entry);

class Umask {
public:
    Umask (mode_t mode) { _old = umask(mode); }
    ~Umask () { umask (_old); }
private:
    Umask (const Umask&);
    Umask& operator=(const Umask&);
    mode_t _old;
};

class RepFreeFilename {
public:
    RepFreeFilename(const char* name, int umask);

    PrVoidError find_next (const char* base, Dstring* fill);

private:

    PrVoidError real_open_dir (const char* dir);

    int     _umask;

    Dstring _root;
    int     _directory;  /* A number, to be converted to radix
			  * MAX_REPOSITORY_COUNT. */
    char    _offset[64]; /* In base MAX_REPOSITORY_COUNT, this
			  * is enough digits to hold over a 64
			  * bit quantity of directories. */
    bool    _available[MAX_REPOSITORY_COUNT];
};


class RepEntry {
public:
    RepEntry() { }
    ~RepEntry();

    /*
     * Rep_prepare_file --
     *
     *     returns a filename if file exists, or if file.gz exists and can be
     *     uncompressed.  returns NULL if the uncompression fails or
     *     file{,gz} does not exist.  the filename returned is the named of
     *     the uncompressed file, which will be different if the file was
     *     compressed and the repository isn't writable.
     */
    PrConstCharPtrError Rep_prepare_file(const char* file);
    void Rep_clear_compressed_cache (void);

    /*
     * Rep_compress_all_files --
     *
     *     compresses all files with a ,v extension.  needs write lock.
     */
    PrVoidError Rep_compress_all_files();

    /*
     * Rep_uncompress_all_files --
     *
     *     uncompresses all files with a .gz extension.  needs write lock.
     */
    PrVoidError Rep_uncompress_all_files();

    /* Rep_needs_recompress --
     *
     *     true if repository should be recompressed
     */
    bool Rep_needs_compress() const;

    mode_t Rep_get_umask() const;
    PrVoidError Rep_chmod(mode_t mode);
    PrVoidError Rep_chown(const char* user, const char* group);
    /* This is a hack for the RCS bug described in checkin.cc.
     * Advice that a particular file is wrong. */
    PrVoidError Rep_chown_file (const char* descriptor_name);

    PrVoidError Rep_lock(bool write_lock);

    /*
     * Rep_name_in_entry --
     *
     *     these should be used only when askng for a path to create a new
     *     file in a writeable repository.
     */
    const char* Rep_entry_path();
    const char* Rep_name_in_entry(const char* file);
    void Rep_name_in_entry(const char* file, Dstring* name);

    /*
     * Rep_set_compression --
     *
     *     this is used to change the compression status of a repsitory
     *     entry after initialization.
     */
    void Rep_set_compression(bool c);

    const char* Rep_name_of_version_file() const;

    /* returns in new_filename the name of a file in the repository
     * which is unique. */
    PrVoidError Rep_new_filename(const char* base,
				 Dstring* new_filename);

    PrVoidError init(const char* name, bool write, bool create, bool require_db);

    /* Rep_unlock_repository --
     *
     *     unlocks the repository, if a lock has been obtained.  */
    void Rep_unlock_repository();
    PrIntError Rep_print_access();

    int version_count() const;
    ProjectVersionDataPtrArray* project_summary() const;
    void project_summary(ProjectVersionDataPtrArray* pda0);

    RcsFileTable* rcs_file_summary() const;
    void rcs_file_summary(RcsFileTable* rft0);

    void add_project_data(ProjectVersionData* new_data);
    void add_rcs_file_data(const char* filename, RcsVersionData* new_data);

    PrVoidError close_repository();

    PrProjectDescriptorPtrError checkout_prj_file(const char* fullname,
						  const char* rcs_version,
						  ProjectReadData flags);

    PrProjectDescriptorPtrError checkout_create_prj_file(const char* filename,
							 const char* fullname,
							 const char* rcs_version,
							 ProjectReadData flags);

    const char* Rep_owner_name() const;
    const char* Rep_group_name() const;

    PrVoidError Rep_alarm() const;

    PrettyOstream& Rep_log() const;

    /* Lookup project version data */

    ProjectVersionData* lookup_version(const char* major, const char* minor) const;
    ProjectVersionData* lookup_version(ProjectDescriptor* project) const;
    ProjectVersionData* lookup_version(const char* major, int minor) const;
    ProjectVersionData* highest_minor_version_data(const char* major, bool may_be_deleted) const;
    int highest_major_version() const;
    int highest_minor_version(const char* major, bool may_be_deleted) const; /* 0 if none */
    bool major_version_exists(const char* major) const;

    ProjectVersionDataPtrArray* common_version(ProjectVersionData* one,
					       ProjectVersionData* two) const;

    ProjectVersionDataPtrArray* common_lineage (ProjectVersionData* one,
						ProjectVersionData* two) const;


    /* Lookup file version data. */

    PrRcsVersionDataPtrError lookup_rcs_file_data(const char* filename, const char* vesion_num) const;

    PrVoidError Rep_make_default_tag();

private:
    /*
     * Rep_file_exists --
     *
     *     this returns true if file or file.gz is in the repository
     */
    bool Rep_file_exists(const char* name);
    PrVoidError Rep_make_tag(const int[3], const int[3]);
    PrVoidError Rep_verify_tag(UpgradeRepository* upgrades);
    PrVoidError Rep_create_repository_entry();
    void common_version_dfs_1(ProjectVersionData* node) const;
    void common_version_dfs_2(ProjectVersionData* node,
			      ProjectVersionDataPtrArray* res) const;
    void common_version_dfs_3(ProjectVersionData* node) const;
    bool common_version_dfs_4(ProjectVersionData* from,
			      ProjectVersionData* to,
			      ProjectVersionDataPtrArray* res) const;
    void clear_flags() const;

    ProjectVersionDataPtrArray *_project_data_array;
    RcsFileTable *_rcs_file_table;
    RebuildFile *_rebuild_file;

    AdvisoryLock* _repository_lock;
    Dstring _base;
    Dstring _project_file;
    Dstring _entry_name;
    int _baselen;
    mode_t _rep_mask;
    bool _writeable;
    bool _compressed;

    struct stat _rep_stat_buf;
    const char* _owner_name;
    const char* _group_name;

    RepFreeFilename *_rfl;
    PrettyStreambuf *_log_pstream;
    filebuf         *_log_stream;
    PrettyOstream   *_log;

    CharPtrArray* _rep_comp_cache;
};

#endif
