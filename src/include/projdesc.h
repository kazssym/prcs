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


#ifndef _PROJDESC_H_
#define _PROJDESC_H_

EXTERN int         prjlex();
EXTERN int         prjinput(void* buf, int max_size);

enum PrjTokenType {
    PrjString,
    PrjName,
    PrjOpen,
    PrjClose,
    PrjNull,
    PrjBadString,
    PrjEof
};

#ifdef __cplusplus

extern FILE* prj_input;

#define foreach_fileent(name, project) \
    foreach(name, (project)->file_entries(), FileEntryPtrArray::ArrayIterator)

class PrjFields {
public:
    const char *name;
    PrVoidError(ProjectDescriptor:: *parse_func)(const Sexp* list);
    PrVoidError(ProjectDescriptor:: *read_func) (int offset);
    bool must_have;
};

class Estring : public Dstring {

public:
    Estring (ProjectDescriptor* project,
	     char* s,
	     size_t off,
	     size_t len,
	     EstringType type);
    Estring (ProjectDescriptor* project, size_t position); /* type = EsUnProtected */

    virtual bool is_dstring();

    void append_protected(const char*); /* To insert a NameLiteral into an EsUnProtected string */
    void append_string(const char*); /* To insert a StringLiteral into an EsUnProtected string */

    EstringType type() const;

    /* A special Dstring that remembers where it was in the project
     * file, allows itself to be modified, adds itself to the
     * descriptors list of strings, and will update the project file
     * when it needs to get written out.  Modify is called before the
     * string is modified.  the alloc field will be set to -1 until
     * it is actually modified. */
    virtual void modify();

    size_t offset()        const;
    size_t offset_length() const;

protected:

    size_t _offset;
    size_t _len;
    EstringType _type;
};

class ListMarker {
public:
    ListMarker (size_t start0, size_t stop0) :start(start0), stop(stop0) { }
    ListMarker () :start(0), stop(0) { }
    size_t start;
    size_t stop;
};

class MergeParentFile {
public:
    MergeParentFile (const char* working0, const char* common0,
		     const char* selected0, MergeAction action0)
	:working(working0), common(common0),
	 selected(selected0), action(action0) { }

    const char* working;
    const char* common;
    const char* selected;
    MergeAction action;
};

class MergeParentEntry {
public:
    MergeParentEntry (const char* s_version0, MergeParentState state0, int lineno0)
	:selected_version(s_version0),
	 project_data(NULL),
	 state(state0),
	 working_table(NULL),
	 common_table(NULL),
	 selected_table(NULL),
	 lineno(lineno0) { }

    Dstring selected_version;
    ProjectVersionData *project_data;
    MergeParentState state;
    MergeParentTable *working_table;
    MergeParentTable *common_table;
    MergeParentTable *selected_table;
    MergeParentFilePtrArray all_files;
    int lineno;
};

class AttrDesc { public: const char* name; AttrType type; };

class PrcsAttrs {
    friend class ProjectDescriptor;
    friend class FileEntry;
    friend bool attrs_equal(const PrcsAttrs*const& x, const PrcsAttrs*const& y);
    friend int attrs_hash(const PrcsAttrs*const& s, int M);
public:

    FileType type() const;
    bool keyword_sub() const;
    ProjectDescriptor* project() const;

    void print_to_string (Dstring* str, bool lead_if) const;
    void print (ostream& os, bool lead_if) const;
    bool regex_matches (reg2ex2_t *re) const;

    MergeAction merge_action (int rule) const;
    const char* merge_desc (int rule) const;

    const char* merge_tool () const;
    const char* diff_tool () const;

    /* Its a bug, really, but group attributes don't print right now.
     * This tells you how many *do* print. */
    int nprint () const { return _vals.length () - _ngroup; }

private:
    PrcsAttrs (const DstringPtrArray *vals0, int ngroup);

    /* used to test equality in attrs_equal */
    DstringPtrArray _vals;
    int             _ngroup;

    /* set by ProjectDescriptor::init_attr */
    FileType        _type;
    ProjectDescriptor *_project;
    bool            _keyword_sub;
    Dstring         _mergetool;
    Dstring         _difftool;
    MergeAction     _merge_actions[14];
    const char*     _merge_descs[14];
};

PrProjectDescriptorPtrError read_project_file(const char* project_full_name,
					      const char* file_name,
					      bool is_working,
					      ProjectReadData flags);
PrProjectDescriptorPtrError read_project_file(const char* project_full_name,
					      const char* file_name,
					      bool is_working,
					      FILE* pipe,
					      ProjectReadData flags);

class ProjectDescriptor {

    friend PrProjectDescriptorPtrError read_project_file(const char* project_full_name,
							 const char* filename,
							 bool is_working,
							 ProjectReadData flags);

    friend PrProjectDescriptorPtrError read_project_file(const char* project_full_name,
							 const char* file_name,
							 bool is_working,
							 FILE* pipe,
							 ProjectReadData flags);

    friend class Estring;

    friend PrBoolError setkeys_internal(const char* input_buffer0,
					int input_buffer_len0,
					ostream* os,
					MemorySegment* seg,
					FileEntry *fe,
					SetkeysAction action);
public:

    ~ProjectDescriptor();

    PrVoidError write_project_file(const char* filename);
    void append_files_data (const char*); /* unprotected, for comments and such. */
    void append_file (const char* working_path,
		      bool keywords);
    void append_file (FileEntry* fe);
    void append_file_deletion (FileEntry* fe);
    void append_link (const char* working_path,
		      const char* link_name);
    void append_directory (const char* working_path);
    void delete_file (FileEntry* fe);

    Dstring* project_version_name()  const;
    Dstring* project_version_major() const;
    Dstring* project_version_minor() const;

    Dstring* parent_version_name()  const;
    Dstring* parent_version_major() const;
    Dstring* parent_version_minor() const;

    Dstring* project_description() const;
    Dstring* version_log()         const;
    Dstring* new_version_log()     const;
    Dstring* checkin_time()        const;
    Dstring* checkin_login()       const;
    Dstring* created_by_major()    const;
    Dstring* created_by_minor()    const;
    Dstring* created_by_micro()    const;

    /* merge_continuing instructs the project descriptor that future
     * merge_notify and has_been_merged requests refer to the named
     * project. */
    bool merge_continuing(ProjectVersionData* selected);
    void merge_notify_incomplete();
    void merge_notify_complete();
    void merge_notify(FileEntry* w, FileEntry* c, FileEntry* s, MergeAction ma) const;
    bool has_been_merged(FileEntry* w, FileEntry* c, FileEntry* s) const;
    void merge_finish();
    MergeParentEntryPtrArray* merge_parents() const;
    MergeParentEntryPtrArray* new_merge_parents() const;
    PrVoidError verify_merge_parents();
    PrVoidError adjust_merge_parents();

    OrderedStringTable* populate_ignore () const;
    KeywordTable* project_keywords (FileEntry* fe, bool setting);
    OrderedStringTable* project_keywords_extra () const;

    void read_quick_elim();
    void quick_elim_unmodified();
    void quick_elim_update();
    void quick_elim_update_fe(FileEntry* fe);
    QuickElim* quick_elim() const;
    void quick_elim(QuickElim* qe);

    ProjectVersionData* project_version_data() const;

    FileEntryPtrArray* file_entries();

    PrProjectVersionDataPtrError resolve_checkin_version(const char* maj, ProjectVersionData **pred);
    PrVoidError checkin_project_file(ProjectVersionData *parent);

    PrVoidError init_repository_entry(const char* name, bool lock, bool create);
    RepEntry* repository_entry() const;
    void repository_entry(RepEntry* rep);

    PrVoidError init_merge_log();
    PrettyOstream& merge_log() const;
    void merge_log(PrettyOstream& rep);

    PrFileEntryPtrError match_fileent (FileEntry* fe);
    FileEntry*          match_working (const char* name);
    bool can_add (FileEntry *fe);

    const char* project_full_name() const;
    const char* project_name()      const;
    const char* project_file_path() const;
    const char* project_aux_path()  const;
    const char* project_path()      const;
    const char* full_version()      const;

    PrVoidError update_attributes (ProjectVersionData* new_data);

    /* Attributes */
    PrPrcsAttrsPtrError intern_attrs (const DstringPtrArray*, int ngroup, const char* name, bool validate);
    PrVoidError validate_attrs (const PrcsAttrs *attrs, const char* name);
    void init_attrs (PrcsAttrs *attrs);

    /* Keyword and Populate ignore manipulation. */

    void add_keyword (const char* key, const char* val);
    void rem_keyword (const char* key);
    void set_keyword (const char* key, const char* val);

    void add_populate_ignore (const char* pi);
    void rem_populate_ignore (const char* pi);

private:

    PrProjectVersionDataPtrError check_version_name (const char* name, int line) const;

    PrVoidError created_by_prj_entry_func        (const Sexp* s);
    PrVoidError project_ver_prj_entry_func       (const Sexp* s);
    PrVoidError parent_ver_prj_entry_func        (const Sexp* s);
    PrVoidError project_desc_prj_entry_func      (const Sexp* s);
    PrVoidError version_log_prj_entry_func       (const Sexp* s);
    PrVoidError new_version_log_prj_entry_func   (const Sexp* s);
    PrVoidError checkin_time_prj_entry_func      (const Sexp* s);
    PrVoidError checkin_login_prj_entry_func     (const Sexp* s);
    PrVoidError descends_from_prj_entry_func     (const Sexp* s);
    PrVoidError populate_ignore_prj_entry_func   (const Sexp* s);
    PrVoidError project_keywords_prj_entry_func  (const Sexp* s);

    /* These 3 entries in the project file grow with project
     * file count, so reading the entire list is a waste of
     * memory. */
    PrVoidError files_prj_entry_read             (int offset);
    PrVoidError merge_parents_prj_entry_read     (int offset);
    PrVoidError new_merge_parents_prj_entry_read (int offset);

    /* Read helper functions. */
    PrVoidError read_merge_parents               (MergeParentEntryPtrArray* array,
						  ListMarker* finish);
    PrVoidError read_merge_parents_entry         (MergeParentEntryPtrArray* array);
    PrVoidError read_merge_parents_file_name     (const Sexp* s, Estring** fill);
    PrVoidError read_merge_parents_insert_file   (MergeParentTable* table,
						  Estring* filename,
						  const Sexp* s,
						  MergeParentFile* file);
    PrVoidError individual_insert_fileent        (const Sexp* s, const DstringPtrArray *gtags);
    PrVoidError verify_project_file              ();
    PrVoidError bad_project_file                 (const Sexp* s, const char* message);

    /* Friendly constructors. */
    PrVoidError init_from_file(const char* project_full_name,
			       const char* filename,
			       bool is_working,
			       FILE* infile,
			       ProjectReadData flags);
    ProjectDescriptor();
    void set_full_version(bool is_working);

    /* Tables */
    void populate_tables(void);

    /* Illegal. */
    ProjectDescriptor(const ProjectDescriptor&);
    int operator=(const ProjectDescriptor&);

    /* Parsing. */
    int prj_lookup_hash (const char *str, int len);
    const PrjFields *prj_lookup_func (const char *str, int len);
    PrConstSexpPtrError read_list(int offset);
    PrConstSexpPtrError read_list_token(PrjTokenType type, int offset);
    PrConstSexpPtrError read_list_elt();
    PrConstSexpPtrError read_new_estring(PrjTokenType type);
    PrVoidError read_list_checked(size_t offset);
    PrVoidError read_parse_bad_token(PrjTokenType type);
    PrVoidError parse_prj_file(FILE* infile);
    void register_estring(Estring*, char missing);

    /* Writing. */
    void really_write_project_file (ostream& os);
    void write_merge_parents (const char* name, MergeParentEntryPtrArray *table, ostream& os);

    /* Cleanup */
    void delete_merge_parents(MergeParentEntryPtrArray* parents);

    /* Static data. */
    static const int       _prj_entries;
    static const PrjFields _pftable[];
    static bool            _prj_entry_found[];

    /* Parsing data. */
    const char* _prj_source_name;
    ProjectReadData _read_flags;
    mode_t          _read_mode;

    /* QE buffer, if it was available. */
    QuickElim* _quick_elim;

    /* Names */
    const char* _project_name;
    Dstring  _full_version;
    Dstring  _project_full_name;
    Dstring  _project_file_path;
    Dstring  _project_path;
    Dstring  _project_aux_path;

    /* Populate */
    OrderedStringTable* _populate_ignore;

    /* Insertion points and the original buffer. */
    MemorySegment   *_segment;
    Dstring*         _nullified_chars;
    DstringPtrArray *_all_estrings;
    Estring         *_files_insertion_point;
    Estring         *_project_version_point;
    Estring         *_end_of_buffer_point;
    Estring         *_project_keywords_point;
    Estring         *_populate_ignore_point;
    ListMarker       _merge_parents_marker;
    ListMarker       _new_merge_parents_marker;
    ListMarker       _descends_from_marker;
    ListMarker       _project_keywords_marker;
    ListMarker       _populate_ignore_marker;
    ConstListMarkerPtrArray *_deleted_markers;

    /* Repository data. */
    RepEntry*           _repository_entry;
    FileEntryPtrArray*  _file_entries;
    ProjectVersionData* _project_version_data;

    /* Merge log */
    PrettyStreambuf *_log_pstream;
    filebuf         *_log_stream;
    PrettyOstream   *_log;

    /* Project file strings. */
    Dstring*      _project_version_name;
    Dstring*      _project_version_major;
    Dstring*      _project_version_minor;

    Dstring*      _parent_version_name;
    Dstring*      _parent_version_major;
    Dstring*      _parent_version_minor;

    Dstring*      _project_description;
    Dstring*      _version_log;
    Dstring*      _new_version_log;
    Dstring*      _checkin_time;
    Dstring*      _checkin_login;

    Dstring*      _created_by_major;
    Dstring*      _created_by_minor;
    Dstring*      _created_by_micro;

    MergeParentEntryPtrArray *_merge_parents;
    MergeParentEntryPtrArray *_new_merge_parents;

    /* Keyword stuff */
    KeywordTable* _project_keywords;
    OrderedStringTable* _project_keywords_extra;
    Dstring* _keyword_id;
    Dstring* _keyword_pheader;
    Dstring* _keyword_pversion;

    /* Current Merge Data */
    MergeParentEntry *_merge_entry;

    const char* _merge_selected_major;
    const char* _merge_selected_minor;

    bool _append_merge_parents;

    bool _alter_popkey;

    bool _merge_complete;
    bool _merge_incomplete;

    /* Match data */
    FileTable *_file_name_table;
    FileTable *_file_desc_table;
    EntryTable *_file_match_cache;
    BangFlag _match_bang;

    /* Attributes */
    AttrsTable *_attrs_table;
};

#endif

#endif /* PROJDESC_H */
