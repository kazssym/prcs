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
#include <stdarg.h>
}

#include "prcs.h"
#include "projdesc.h"
#include "sexp.h"
#include "fileent.h"
#include "vc.h"
#include "repository.h"
#include "misc.h"
#include "hash.h"
#include "quick.h"
#include "system.h"
#include "checkout.h"
#include "memseg.h"
#include "setkeys.h"
#include "populate.h"

#define prj_lex_val prjtext
#define prj_lex_len prjleng

/* in attrs.cc */
extern const struct AttrDesc*
is_file_attribute (register const char *str, register int len);

static const char bad_match_help_string[] =
"When trying to determine how files are related between versions, PRCS "
"first attempts to find a file with the same file family, then looks for "
"a file with the same name.  If both exist, and are not the same file, "
"the match is ambiguous.  This can happen if you rename a file and then "
"recreate a file with the original name.  You may do this, but it is "
"problematic when performing merge and diff operations";

extern const char *default_merge_descs[14];
extern const MergeAction default_merge_actions[14];

EXTERN int         prjleng;
EXTERN const char* prjtext;
EXTERN int         prj_lex_this_index;
EXTERN int         prj_lex_cur_index;
EXTERN int         prj_lineno;

EXTERN int is_builtin_keyword(const char* s, int len);

Dstring* ProjectDescriptor::project_version_name()  const { return _project_version_name;}
Dstring* ProjectDescriptor::project_version_major() const { return _project_version_major; }
Dstring* ProjectDescriptor::project_version_minor() const { return _project_version_minor; }
Dstring* ProjectDescriptor::parent_version_name()  const { return _parent_version_name; }
Dstring* ProjectDescriptor::parent_version_major() const { return _parent_version_major; }
Dstring* ProjectDescriptor::parent_version_minor() const { return _parent_version_minor; }
Dstring* ProjectDescriptor::project_description() const { return _project_description;}
Dstring* ProjectDescriptor::version_log()         const { return _version_log; }
Dstring* ProjectDescriptor::new_version_log()     const { return _new_version_log; }
Dstring* ProjectDescriptor::checkin_time()        const { return _checkin_time; }
Dstring* ProjectDescriptor::checkin_login()       const { return _checkin_login; }
Dstring* ProjectDescriptor::created_by_major()    const { return _created_by_major; }
Dstring* ProjectDescriptor::created_by_minor()    const { return _created_by_minor; }
Dstring* ProjectDescriptor::created_by_micro()    const { return _created_by_micro; }
MergeParentEntryPtrArray* ProjectDescriptor::new_merge_parents() const { return _new_merge_parents; }
void     ProjectDescriptor::merge_notify_incomplete() { _merge_incomplete = true; }
void     ProjectDescriptor::merge_notify_complete()   { _merge_complete = true; }
RepEntry   *ProjectDescriptor::repository_entry() const { return _repository_entry; }
void        ProjectDescriptor::repository_entry(RepEntry* rep) { _repository_entry = rep; }
QuickElim  *ProjectDescriptor::quick_elim()        const { return _quick_elim; }
void        ProjectDescriptor::quick_elim(QuickElim* qe) { _quick_elim = qe; }
const char *ProjectDescriptor::project_full_name() const { return _project_full_name; }
const char *ProjectDescriptor::project_name()      const { return _project_name; }
const char *ProjectDescriptor::project_file_path() const { return _project_file_path; }
const char *ProjectDescriptor::project_aux_path()  const { return _project_aux_path; }
const char *ProjectDescriptor::project_path()      const { return _project_path; }
const char *ProjectDescriptor::full_version()      const { return _full_version; }
OrderedStringTable* ProjectDescriptor::populate_ignore() const { return _populate_ignore; };
ProjectVersionData* ProjectDescriptor::project_version_data() const { return _project_version_data; }
FileEntryPtrArray* ProjectDescriptor::file_entries() { return _file_entries; }
PrettyOstream& ProjectDescriptor::merge_log() const { return *_log; }
void        ProjectDescriptor::merge_log(PrettyOstream& str) { _log = &str; }

ProjectDescriptor::~ProjectDescriptor()
{
    if (_segment) delete (_segment);

    if (_file_entries) {
	foreach_fileent (fe_ptr, this)
	    delete (*fe_ptr);

	delete (_file_entries);
    }

    if (_log_pstream) {
        if (_log_pstream->pubsync()) {
	    prcserror << "warning: Write to merge log failed" << perror;
	}

	delete _log_pstream;
	delete _log_stream;
	delete _log;
    }

    if (_all_estrings) {
	foreach (es_ptr, _all_estrings, DstringPtrArray::ArrayIterator)
	    delete (*es_ptr);

	delete _all_estrings;
    }

    delete _deleted_markers;

    delete_merge_parents (_merge_parents);
    delete_merge_parents (_new_merge_parents);

    if (_nullified_chars)  delete _nullified_chars;
    if (_populate_ignore)  delete _populate_ignore;
    if (_quick_elim)       delete _quick_elim;
    if (_project_keywords) delete _project_keywords;
    if (_project_keywords_extra) delete _project_keywords_extra;
    if (_keyword_id)       delete _keyword_id;
    if (_keyword_pheader)  delete _keyword_pheader;
    if (_keyword_pversion) delete _keyword_pversion;
    if (_file_name_table)  delete _file_name_table;
    if (_file_desc_table)  delete _file_desc_table;
    if (_file_match_cache) delete _file_match_cache;
}

ProjectDescriptor::ProjectDescriptor()
    :_prj_source_name(NULL),
     _read_flags(KeepNothing),
     _quick_elim(NULL),
     _project_name(NULL),
     _populate_ignore(NULL),
     _segment(NULL),
     _nullified_chars(NULL),
     _all_estrings(NULL),
     _files_insertion_point(NULL),
     _project_version_point(NULL),
     _end_of_buffer_point(NULL),
     _project_keywords_point(NULL),
     _populate_ignore_point(NULL),
     _merge_parents_marker(0, 0),
     _new_merge_parents_marker(0, 0),
     _descends_from_marker(0, 0),
     _project_keywords_marker(0, 0),
     _populate_ignore_marker(0, 0),
     _deleted_markers(NULL),
     _repository_entry(NULL),
     _file_entries(NULL),
     _project_version_data(NULL),
     _log_pstream(NULL),
     _log_stream(NULL),
     _log(NULL),
     _project_version_name(NULL),
     _project_version_major(NULL),
     _project_version_minor(NULL),
     _parent_version_name(NULL),
     _parent_version_major(NULL),
     _parent_version_minor(NULL),
     _project_description(NULL),
     _version_log(NULL),
     _new_version_log(NULL),
     _checkin_time(NULL),
     _checkin_login(NULL),
     _created_by_major(NULL),
     _created_by_minor(NULL),
     _created_by_micro(NULL),
     _merge_parents(NULL),
     _new_merge_parents(NULL),
     _project_keywords(NULL),
     _project_keywords_extra(NULL),
     _keyword_id(NULL),
     _keyword_pheader(NULL),
     _keyword_pversion(NULL),
     _merge_entry(NULL),
     _merge_selected_major(NULL),
     _merge_selected_minor(NULL),
     _append_merge_parents(false),
     _alter_popkey(false),
     _merge_complete(false),
     _merge_incomplete(false),
     _file_name_table(NULL),
     _file_desc_table(NULL),
     _file_match_cache(NULL) { }

PrVoidError ProjectDescriptor::bad_project_file(const Sexp *s, const char* mes)
{
    pthrow prcserror << fileline(_prj_source_name, s->line_number())
		    << "Invalid project file, " << mes << dotendl;
}

PrProjectDescriptorPtrError read_project_file(const char* project_full_name,
					      const char* filename,
					      bool is_working,
					      ProjectReadData flags)
{
    ProjectDescriptor* new_project = new ProjectDescriptor();

    Return_if_fail(new_project->init_from_file(project_full_name, filename, is_working, NULL, flags));

    return new_project;
}

PrProjectDescriptorPtrError read_project_file(const char* project_full_name,
					      const char* file_name,
					      bool is_working,
					      FILE* infile,
					      ProjectReadData flags)
{
    ProjectDescriptor* new_project = new ProjectDescriptor();

    Return_if_fail(new_project->init_from_file(project_full_name, file_name, is_working, infile, flags));

    return new_project;
}

PrVoidError ProjectDescriptor::init_repository_entry(const char* file, bool lock, bool create)
{
    Return_if_fail(_repository_entry << Rep_init_repository_entry(file, lock, create, true));

    return NoError;
}

PrVoidError ProjectDescriptor::init_from_file(const char* full_name,
					      const char* filename,
					      bool is_working,
					      FILE* infile0,
					      ProjectReadData flags)
{
    const char* last_slash;
    FILE* infile;

    _read_flags = flags;

    _project_full_name.assign(full_name);
    _project_file_path.assign(full_name);
    _project_file_path.append(".prj");

    last_slash = strrchr(full_name, '/');

    if(last_slash) {
	_project_name = last_slash + 1;
	_project_path.assign(full_name);
	_project_path.truncate(last_slash - full_name + 1);
    } else {
	_project_name = full_name;
	/*_project_path.truncate(0);*/
    }

    _project_aux_path.sprintf("%s.%s.prcs_aux", _project_path.cast(), _project_name);

    if(infile0 == NULL) {
	struct stat buf;

        if((infile = fopen(filename, "r")) == NULL)
	    pthrow prcserror << "Can't open file " << squote(filename) << perror;

	If_fail(Err_stat(filename, & buf))
	    pthrow prcserror << "Stat failed on project file "
			     << squote (filename) << perror;

	_read_mode = buf.st_mode & 0777;
    } else {
	infile = infile0;
	_read_mode = 0;
    }

    _prj_source_name = filename;

    if (_read_flags & KeepMergeParents) {
	_new_merge_parents = new MergeParentEntryPtrArray;
	_merge_parents = new MergeParentEntryPtrArray;
    }

    _populate_ignore = new OrderedStringTable;
    _project_keywords = new KeywordTable;
    _project_keywords_extra = new OrderedStringTable;
    _file_entries = new FileEntryPtrArray;
    _deleted_markers = new ConstListMarkerPtrArray;
    _attrs_table = new AttrsTable (attrs_hash, attrs_equal);

    Return_if_fail(parse_prj_file(infile));

    set_full_version (is_working);

    if(infile0 == NULL)
	fclose(infile);

    return NoError;
}

void ProjectDescriptor::set_full_version (bool is_working)
{
    _full_version.assign (*project_version_major());
    _full_version.append ('.');
    _full_version.append (*project_version_minor());

    if (is_working)
	_full_version.append ("(w)");
}

PrVoidError ProjectDescriptor::verify_merge_parents()
{
    foreach (ent_ptr, _merge_parents, MergeParentEntryPtrArray::ArrayIterator) {
	MergeParentEntry *entry = *ent_ptr;

	Return_if_fail (entry->project_data << check_version_name (entry->selected_version, entry->lineno));
    }

    foreach (ent_ptr, _new_merge_parents, MergeParentEntryPtrArray::ArrayIterator) {
	MergeParentEntry *entry = *ent_ptr;

	Return_if_fail (entry->project_data << check_version_name (entry->selected_version, entry->lineno));
    }

    return NoError;
}

PrVoidError ProjectDescriptor::init_merge_log()
{
    const char* logname_env = get_environ_var ("PRCS_MERGE_LOG");
    Dstring logname;

    if (logname_env)
	logname.assign (logname_env);
    else {
	logname.assign (project_full_name());
	logname.append (".log");
    }

    _log_stream = new filebuf;

    if (!_log_stream->open(logname, ios::out|ios::app))
	pthrow prcserror << "Failed opening merge log file " << squote(logname) << perror;

    _log_pstream = new PrettyStreambuf (_log_stream, &option_report_actions);

    _log_pstream->set_fill_width (1<<20);
    _log_pstream->set_fill_prefix ("");

    _log = new PrettyOstream (_log_pstream, NoError);

    return NoError;
}

/* format should be a string consisting of 'U', 'N', 'L', 'S', and
 * 'A', for user, number, label, string and any.  returns false if a
 * match fails.  arguments following format are the addresses of
 * Dstring*s to fill in. an example is:
 *
 *     sexp_scan(s, "LLL", &d1, &d2, &d3);
 *
 * where dN will get the Nth label of three.
 *
 * if the first character of format is an asterisk, then "-*-" is
 * accepted as a label.  if the first cahracter of format is an 'e',
 * then empty lists are allowed.  */
static bool sexp_scan(const Sexp* list, const char* format, ...)
{
    va_list args;
    char c;
    int len = list->length();
    const char* type = NULL;
    bool accept_ast = false;
    bool accept_empty = false;
    bool loop_done = false;

    while (!loop_done) {
	switch (*format) {
	case '*':
	    accept_ast = true;
	    format += 1;
	    break;
	case 'e':
	    accept_empty = true;
	    format += 1;
	    break;
	default:
	    loop_done = true;
	    break;
	}
    }

    va_start(args, format);

    if(!list->is_pair()) goto error;

    while((c = *format++)) {
	switch(c) {
	case 'N': type = "number"; break;
	case 'S': type = "any"; break;
	case 'L': type = "label"; break;
	case 'U': type = "login"; break;
	case 'A': type = "any"; break;
	}

	/* If there are no characters left, and we are expecting one, bail */
	if(len == 0) goto error;

	bool okay_empty = false;

	if(list->car()->is_pair()) {
	    if (accept_empty && !list->car())
		okay_empty = true;
	    else
		goto error;
	}

	/* now check the type */
	if(!okay_empty && VC_check_token_match(*list->car()->key(), type) <= 0) {
	    if(!(accept_ast && strcmp(*list->car()->key(), "-*-") == 0))
		goto error;
	}

	if (c == 'S' && list->car()->key()->type())
	    goto error;

	Estring** dptr = va_arg(args, Estring**);

	if (dptr) {
	    if (okay_empty)
		(*dptr) = NULL;
	    else
		(*dptr) = list->car()->key();
	}

	list = list->cdr();
	len -= 1;
    }

    va_end(args);
    return len == 0;
error:
    va_end(args);
    return false;
}

/* Project read functions
 */

PrVoidError ProjectDescriptor::created_by_prj_entry_func(const Sexp* s)
{
    if(!sexp_scan(s, "LNNN", NULL, &_created_by_major,
		  &_created_by_minor, &_created_by_micro ))
	return bad_project_file(s, "badly formed Created-By-Prcs-Version entry");

    return NoError;
}

PrVoidError ProjectDescriptor::project_ver_prj_entry_func(const Sexp* s)
{
    if(!sexp_scan(s, "LLLN", NULL, &_project_version_name,
		  &_project_version_major, &_project_version_minor ))
	return bad_project_file(s, "badly formed Project-Version entry");

    _project_version_point = new Estring (this, s->end_index());

    return NoError;
}

PrVoidError ProjectDescriptor::parent_ver_prj_entry_func(const Sexp* s)
{
    if(!sexp_scan(s, "*LLLN", NULL, &_parent_version_name,
		  &_parent_version_major, &_parent_version_minor )) {
	return bad_project_file(s, "badly formed Parent-Version entry");
    } else {
	bool major_absent = strcmp(*_parent_version_name, "-*-") == 0;
	bool minor_absent = strcmp(*_parent_version_major, "-*-") == 0;
	bool name_absent = strcmp(*_parent_version_minor, "-*-") == 0;

	if(major_absent ^ minor_absent ||
	   major_absent ^ name_absent ||
	   name_absent ^ minor_absent)
	    return bad_project_file(s, "illegal use of -*-");
    }

    return NoError;
}

PrVoidError ProjectDescriptor::project_desc_prj_entry_func(const Sexp* s)
{
    if(!sexp_scan(s, "LS", NULL, &_project_description))
	return bad_project_file(s, "badly formed Project-Description entry");

    return NoError;
}

PrVoidError ProjectDescriptor::version_log_prj_entry_func(const Sexp* s)
{
    if(!sexp_scan(s, "LS", NULL, &_version_log)) {
	return bad_project_file(s, "badly formed Version-Log entry");
    }

    return NoError;
}

PrVoidError ProjectDescriptor::new_version_log_prj_entry_func(const Sexp* s)
{
    if(!sexp_scan(s, "LS", NULL, &_new_version_log))
	return bad_project_file(s, "badly formed New-Version-Log entry");

    return NoError;
}

PrVoidError ProjectDescriptor::checkin_time_prj_entry_func(const Sexp* s)
{
    if(!sexp_scan(s, "LS", NULL, &_checkin_time)) {
	return bad_project_file(s, "badly formed Checkin-Time entry");
    }

    return NoError;
}

PrVoidError ProjectDescriptor::checkin_login_prj_entry_func(const Sexp* s)
{
    if(!sexp_scan(s, "LU", NULL, &_checkin_login))
	return bad_project_file(s, "badly formed Checkin-Login entry");

    return NoError;
}

PrVoidError ProjectDescriptor::descends_from_prj_entry_func(const Sexp* s)
{
    /* Backward compatibility, its just ignored. */
    _descends_from_marker = ListMarker (s->start_index(), s->end_index());

    _deleted_markers->append (&_descends_from_marker);

    return NoError;
}

PrVoidError ProjectDescriptor::populate_ignore_prj_entry_func(const Sexp* s)
{
    _populate_ignore_marker = ListMarker (s->start_index(), s->end_index());
    _populate_ignore_point = new Estring (this, s->end_index());

    if(s->length() != 2 || !s->cadr()->is_pair())
	return bad_project_file(s, "malformed Populate-Ignore entry");

    foreach_sexp (t_ptr, s->cadr()) {
	const Sexp *t = *t_ptr;

	if(t->is_pair() || t->key()->type() != EsStringLiteral)
	    return bad_project_file(t, "expected a string");
    }

    foreach_sexp (t_ptr, s->cadr()) {
	Dstring* ds = (*t_ptr)->key();
	const char* t = ds->cast();
	_populate_ignore->insert (t, t);
    }

    return NoError;
}

PrVoidError ProjectDescriptor::project_keywords_prj_entry_func (const Sexp* s)
{
    _project_keywords_marker = ListMarker (s->start_index(), s->end_index());
    _project_keywords_point = new Estring (this, s->end_index());

    foreach_sexp (t_ptr, s->cdr()) {
	const Sexp *t = *t_ptr;

	if(!t->is_pair() ||
	   t->length() != 2 ||
	   t->car()->is_pair() ||
	   t->cadr()->is_pair() ||
	   t->car()->key()->type() != EsNameLiteral)
	    return bad_project_file(t, "malformed Project-Keywords entry");

	if (is_builtin_keyword (t->car()->key()->cast(), t->car()->key()->length()))
	    return bad_project_file(t, "Project-Keywords may not contain a builtin keyword");

	if (_project_keywords->isdefined (t->car()->key()->cast()))
	    return bad_project_file(t, "duplicate keyword");

	if (strchr (t->cadr()->key()->cast(), '\n'))
	    return bad_project_file(t, "keyword value may not contain a `$' character");

	_project_keywords->insert (t->car()->key()->cast(), t->cadr()->key()->cast());
	_project_keywords_extra->insert (t->car()->key()->cast(), t->cadr()->key()->cast());
    }

    return NoError;
}

PrVoidError ProjectDescriptor::new_merge_parents_prj_entry_read (int offset)
{
    _new_merge_parents_marker.start = offset;

    Return_if_fail (read_merge_parents(_new_merge_parents, &_new_merge_parents_marker));

    return NoError;
}


PrVoidError ProjectDescriptor::merge_parents_prj_entry_read(int offset)
{
    _merge_parents_marker.start = offset;

    Return_if_fail (read_merge_parents(_merge_parents, &_merge_parents_marker));

    return NoError;
}

PrVoidError ProjectDescriptor::files_prj_entry_read(int /*offset*/)
{
    const Sexp* s;
    DstringPtrArray gtags;
    bool allow_tags = true;

    Return_if_fail (s << read_list_elt());

    while (!s->is_empty()) {

	if (s->is_pair()) {
	    Return_if_fail(individual_insert_fileent(s, &gtags));
	    allow_tags = false;
	} else if (allow_tags && s->key()->index(0) == ':') {
	    gtags.append (s->key());
	} else {
	    return bad_project_file(s, "unexpected or illegal file attribute");
	}

	delete s;

	Return_if_fail(s << read_list_elt());
    }

    /* @@@ */
    _files_insertion_point = new Estring(this, prj_lex_this_index);

    Return_if_fail(verify_project_file());

    return NoError;
}

/* Read Helpers.
 */

static ListMarker *finish_me = NULL;

PrjTokenType prj_lex()
{
    int val = prjlex();

    if (finish_me) {
	finish_me->stop = prj_lex_this_index + (val == PrjEof);
	finish_me = NULL;
    }

    return (PrjTokenType) val;
}

PrVoidError ProjectDescriptor::read_merge_parents (MergeParentEntryPtrArray* array,
						   ListMarker* finish)
{
    PrjTokenType tok;

    while (true) {
	tok = prj_lex();

	switch (tok) {
	case PrjNull:
	case PrjBadString:
	case PrjEof:
	case PrjString:
	case PrjName:
	    return read_parse_bad_token(tok);

	case PrjClose:
	    finish_me = finish;

	    return NoError;
	case PrjOpen:
	    Return_if_fail (read_merge_parents_entry (array));
	}
    }

    return NoError;
}

PrVoidError ProjectDescriptor::read_merge_parents_file_name (const Sexp* s, Estring** fill)
{
    if (s->is_pair() && s->length() == 0)
	*fill = NULL;
    else if (!s->is_pair())
	*fill = s->key();
    else
	return bad_project_file (s, "bad merge parents entry");

    return NoError;
}

PrVoidError ProjectDescriptor::read_merge_parents_entry (MergeParentEntryPtrArray* array)
{
    const Sexp* s;
    MergeParentState state = MergeStateParent;
    MergeParentEntry* entry = NULL;
    MergeParentFile* file;
    Dstring* selected_version = NULL;
    Estring* working = NULL;
    Estring* common = NULL;
    Estring* selected = NULL;
    MergeAction action;

    Return_if_fail (s << read_list_elt());

    if (s->is_pair() || !strchr (s->key()->cast(), '.'))
	return bad_project_file (s, "expected version number");

    selected_version = s->key();

    delete s;
    Return_if_fail (s << read_list_elt());

    /* So, if we encounter 3 elements, its the stupid 1.1 format, 1
     * element, its the stupid 1.0 and earlier format, 2 elements and
     * its the (hopefully less stupid) current format. */

    if (!s->is_pair()) {
	/* 1.1 or 1.2 format. */
	const Sexp* elt_2 = s;
	const Sexp* elt_3;

	Return_if_fail (elt_3 << read_list_elt());

	if (elt_3->is_pair()) {
	    /* 1.2 format. */

	    if (strcmp (elt_2->key()->cast(), "complete") == 0) {
		state = MergeStateParent;
	    } else if (strcmp (elt_2->key()->cast(), "incomplete-parent") == 0) {
		state = MergeStateIncompleteParent;
	    } else if (strcmp (elt_2->key()->cast(), "incomplete") == 0) {
		state = MergeStateIncomplete;
	    } else {
		return bad_project_file (elt_2, "expected completion state");
	    }

	    delete elt_2;
	    s = elt_3;
	} else {
	    /* 1.1 format. */

	    if (strcmp (elt_3->key()->cast(), "complete") == 0) {
		state = MergeStateParent;
	    } else if (strcmp (elt_3->key()->cast(), "incomplete") == 0) {
		state = MergeStateIncomplete;
	    } else {
		return bad_project_file (elt_3, "expected completion state");
	    }

	    delete elt_2;
	    delete elt_3;
	    Return_if_fail (s << read_list_elt());
	}
    }

    if (array) {
	entry = new MergeParentEntry (*selected_version,
				      state,
				      prj_lineno);
	array->append (entry);
    }

    while (!s->is_empty()) {
	if (!s->is_pair() || s->length() != 4)
	    return bad_project_file (s, "bad merge parents entry");

	Return_if_fail (read_merge_parents_file_name (s->car(), &working));
	Return_if_fail (read_merge_parents_file_name (s->cadr(), &common));
	Return_if_fail (read_merge_parents_file_name (s->caddr(), &selected));

	if (s->cadddr()->is_pair() || s->cadddr()->key()->length() != 1)
	    return bad_project_file (s, "bad merge parents entry");

	switch (s->cadddr()->key()->index(0)) {
	case 'a': case 'd': case 'n': case 'm': case 'r':
	    action = (MergeAction) s->cadddr()->key()->index(0);
	    break;
	default:
	    return bad_project_file (s, "bad merge parents entry");
	}

	if (entry) {
	    file = new MergeParentFile (working ? working->cast() : NULL,
					common ? common->cast() : NULL,
					selected ? selected->cast() : NULL,
					action);

	    entry->all_files.append (file);
	}

	delete s;
	Return_if_fail (s << read_list_elt());
    }

    return NoError;
}

PrVoidError ProjectDescriptor::individual_insert_fileent(const Sexp* file, const DstringPtrArray* gtags)
{
    Dstring *working_path    = NULL;
    Dstring *desc_name       = NULL;
    Dstring *version_num     = NULL;
    Dstring *link_name       = NULL;
    const Sexp *file_descriptor = NULL;
    mode_t   file_mode       = 0644;
    ListMarker ent_marker;
    DstringPtrArray tags;
    const PrcsAttrs* attrs;

    if(!file->is_pair())
	return bad_project_file(file, "expected a list");

    if(file->length() < 2)
	return bad_project_file(file, "file entry too short");

    if(file->car()->is_pair())
	return bad_project_file(file, "expected a filename");

    if(!file->cadr()->is_pair())
	return bad_project_file(file, "malformed internal file identifier");

    if(weird_pathname(*file->car()->key()))
	return bad_project_file(file, "illegal pathname");

    if (project_path()[0]) {
	/* This saves allocating a new string if its going to be the
	 * same as the one in the project file. */
	working_path = new Dstring(project_path());

	working_path->append(*file->car()->key());
    } else {
	working_path = file->car()->key();
    }

    ent_marker = file->marker();
    file_descriptor = file->cadr();

    foreach_sexp (elt_ptr, file_descriptor) {
	if ((*elt_ptr)->is_pair())
	    return bad_project_file (*elt_ptr, "malformed internal file descriptor");
    }

    tags.append (*gtags);
    foreach_sexp (tag_ptr, file->cdr()->cdr()) {
	const Sexp* tag = *tag_ptr;

	if(tag->is_pair() || tag->key()->index(0) != ':')
	    return bad_project_file(file, "invalid file attribute");

	tags.append ((*tag_ptr)->key());
    }

    Return_if_fail (attrs << intern_attrs (&tags, gtags->length(), working_path->cast(), true));

    if ( attrs->type() == Directory || attrs->type() == SymLink ) {
	if (attrs->type() == Directory) {
	    if (file_descriptor->length() > 0)
		return bad_project_file(file, "malformed internal file descriptor");
	} else {
	    if (file_descriptor->length() > 1)
		return bad_project_file(file, "malformed internal file descriptor");

	    if (file_descriptor->length() == 1)
		link_name = file_descriptor->car()->key();
	}
    } else {
	if(file_descriptor->length() == 0) {
	    /* its empty */
	} else {
	    if(file_descriptor->length() == 2) {
		/* its the old format -- no file mode */
	    } else if(file_descriptor->length() == 3) {

		const char *mode_str = *file_descriptor->cdr()->cadr()->key();

		file_mode = 0;

		if(strlen(mode_str) != 3)
		    return bad_project_file(file, "invalid file mode");

		while(*mode_str) {
		    char c = *mode_str++;

		    if (c < '8' && c >= '0') {
			file_mode += c - '0';
			if(*mode_str)
			    file_mode <<= 3;
		    } else {
			return bad_project_file(file, "invalid file mode");
		    }
		}

	    } else {
		return bad_project_file(file, "malformed internal file descriptor");
	    }

	    version_num = file_descriptor->cadr()->key();

	    if (strncmp (*version_num, "1.", 2) != 0)
		return bad_project_file (file, "malformed internal file descriptor");

	    Estring *int_desc = file_descriptor->car()->key();

	    const char* int_desc_c  = *int_desc;
	    const char* first_slash = strchr(int_desc_c, '/');

	    if(!first_slash || strlen(first_slash) < 4) /* /0_a is legal (4 chars) */
		return bad_project_file(file, "malformed internal file descriptor");

	    int diff = (first_slash + 1) - int_desc_c;

	    desc_name = new Estring (this,
				     (char*)int_desc_c + diff,
				     int_desc->offset() + diff,
				     int_desc->offset_length() - diff,
				     EsNameLiteral);
	}
    }

    _file_entries->append(new FileEntry(working_path,
					ent_marker,
					attrs,
					version_num,
					desc_name,
					link_name,
					file_mode));

    return NoError;
}



PrVoidError ProjectDescriptor::verify_project_file()
{
    /* This code insures:
     * no descriptor matches the project file's.
     * no filename matches the project file's.
     * no descriptor matches another.
     * no filename matches another.
     * no filename is a prefix of another unless it has :directory
     */

    Dstring project_descriptor;

    project_descriptor.sprintf("%s.prj", project_name());

    /* Using the project_file_path and project_file_aux will
     * be wrong later */
    Dstring project_file_name(project_file_path());
    Dstring project_aux_name(project_aux_path());

    PathTable descriptors(pathname_hash, pathname_equal),
	      filenames  (pathname_hash, pathname_equal);

    descriptors.insert(project_descriptor.cast(), RealFile);
    filenames.insert(project_file_name.cast(), RealFile);
    filenames.insert(project_aux_name.cast(), RealFile);

    foreach_fileent(fe_ptr, this) {
	FileEntry* fe = *fe_ptr;

	const char* fe_name = fe->working_path();

	if(filenames.isdefined(fe_name)) {
	    if(strcmp(fe_name, project_file_name) == 0)
		pthrow prcserror << "Entry " << squote(fe_name)
				<< " duplicates project file name" << dotendl;
	    else if (strcmp(fe_name, project_aux_name) == 0)
		pthrow prcserror << "Entry " << squote(fe_name)
				<< " duplicates project auxiliary file name" << dotendl;
	    else
		pthrow prcserror << "Entry " << squote(fe_name)
				<< " is a duplicate file name" << dotendl;
	}

	filenames.insert(fe_name, fe->file_type());

	if(fe->file_type() != RealFile || !fe->descriptor_name())
	    continue;

	const char* fe_desc = fe->descriptor_name();

	if(descriptors.isdefined(fe_desc)) {
	    if(strcmp(fe_desc, project_descriptor) == 0)
		pthrow prcserror << "Entry " << squote(fe_name)
				<< " duplicates project file descriptor" << dotendl;
	    else
		pthrow prcserror << "Entry " << squote(fe_name)
				<< " contains a duplicate file descriptor" << dotendl;
	}

	descriptors.insert(fe_desc, RealFile);
    }

    foreach_fileent(fe_ptr, this) {
	FileEntry* fe = *fe_ptr;

	Dstring name(fe->working_path());

	for(int i = name.length() - 1; i > 0; i -= 1) {
	    if (name.index(i) == '/') {
		name.truncate(i);

		FileType* ft = filenames.lookup(name);

		if (ft && (*ft) != Directory)
		    pthrow prcserror << "Non-directory filename " << squote(name)
				    << " is prefix to another file" << dotendl;
	    }
	}
    }

    return NoError;
}

/* merge_notify, has_been_merged --
 *
 *     these two functions deserve documentation.  merge_notify is called to
 *     update the project file immediately after a merge action is taken, in
 *     merge_finish.  the MergeAction argument identifies the type of action
 *     taken.  the goal is to save all information and avoid offering to merge
 *     a particular working file twice.  The data saved is as follows:
 *
 *     (working_name common_name selected_name action)
 *
 *     file families are not saved because for common and selected versions
 *     they are well defined.  for working files, they are not guaranteed to
 *     exist.  both names and families may change with merges and users edits
 *     so tracking merges by them is not useful, in my opinion.
 */
void ProjectDescriptor::merge_notify(FileEntry* working,
				     FileEntry* common,
				     FileEntry* selected,
				     MergeAction ma) const
{
    MergeParentFile* new_entry;

    new_entry = new MergeParentFile (working ? working->working_path() + cmd_root_project_path_len : NULL,
				     common ? common->working_path() + cmd_root_project_path_len : NULL,
				     selected ? selected->working_path() + cmd_root_project_path_len : NULL,
				     ma);

    _merge_entry->all_files.append (new_entry);
}

/* algorithm: to be true,
 * for each of w,c,s, if a lookup of filename in the correct
 * table must succeed.
 */
bool ProjectDescriptor::has_been_merged(FileEntry* working,
					FileEntry* common,
					FileEntry* selected) const
{
    return
	(working  && _merge_entry->working_table ->isdefined(working ->working_path() + cmd_root_project_path_len)) ||
	(common   && _merge_entry->common_table  ->isdefined(common  ->working_path() + cmd_root_project_path_len)) ||
	(selected && _merge_entry->selected_table->isdefined(selected->working_path() + cmd_root_project_path_len));
}

bool ProjectDescriptor::merge_continuing (ProjectVersionData *selected)
{
    _merge_selected_major = selected->prcs_major();
    _merge_selected_minor = selected->prcs_minor();

    Dstring sel_version_name;
    bool return_val = false;

    sel_version_name.append (selected->prcs_major());
    sel_version_name.append ('.');
    sel_version_name.append (selected->prcs_minor());

    _deleted_markers->append (&_merge_parents_marker);
    _deleted_markers->append (&_new_merge_parents_marker);

    _append_merge_parents = true;

    _merge_entry = new MergeParentEntry (sel_version_name, MergeStateIncomplete, 0);
    _new_merge_parents->append (_merge_entry);

    _merge_entry->working_table  = new MergeParentTable;
    _merge_entry->common_table   = new MergeParentTable;
    _merge_entry->selected_table = new MergeParentTable;

    foreach (ent_ptr, _new_merge_parents, MergeParentEntryPtrArray::ArrayIterator) {
	MergeParentEntry *entry = *ent_ptr;

	if (strcmp (entry->selected_version, sel_version_name) != 0)
	    continue;

	foreach (file_ptr, &entry->all_files, MergeParentFilePtrArray::ArrayIterator) {
	    MergeParentFile *file = *file_ptr;

	    return_val = true;

	    if (file->working)  _merge_entry->working_table ->insert (file->working , file);
	    if (file->common)   _merge_entry->common_table  ->insert (file->common  , file);
	    if (file->selected) _merge_entry->selected_table->insert (file->selected, file);
	}
    }

    return return_val;
}

void ProjectDescriptor::merge_finish()
{
    if(!_merge_complete || _merge_incomplete) {
	prcsinfo << "Merge incomplete" << dotendl;
	merge_log() << prcsendl << "*** Merge incomplete" << prcsendl << prcsendl;
    } else {
	_merge_entry->state = MergeStateParent;

	prcsinfo << "Merge against version "
		 << _merge_selected_major << "."
		 << _merge_selected_minor << " complete" << dotendl;

	merge_log() << prcsendl << "*** Merge complete" << prcsendl << prcsendl;
    }
}

void ProjectDescriptor::delete_merge_parents (MergeParentEntryPtrArray* array)
{
    if (array) {
	foreach (ent_ptr, array, MergeParentEntryPtrArray::ArrayIterator) {
	    MergeParentEntry *entry = *ent_ptr;

	    if (entry->working_table) delete entry->working_table;
	    if (entry->common_table) delete entry->common_table;
	    if (entry->selected_table) delete entry->selected_table;

	    foreach (file_ptr, &entry->all_files, MergeParentFilePtrArray::ArrayIterator)
		delete (*file_ptr);
	}
    }
}

PrProjectVersionDataPtrError ProjectDescriptor::check_version_name(const char* name, int line) const
{
    ProjectVersionData* data;

    const char* this_version_major = major_version_of(name);
    const char* this_version_minor = minor_version_of(name);

    if(!this_version_major)
	pthrow prcserror << fileline(_prj_source_name, line)
			<< "Illegal version number " << name << dotendl;

    if(VC_check_token_match(this_version_major, "label") <= 0 ||
       VC_check_token_match(this_version_minor, "number") <= 0)
	pthrow prcserror << fileline(_prj_source_name, line)
			<< "Illegal version number " << name << dotendl;

    data = repository_entry()->lookup_version(this_version_major, this_version_minor);

    if (!data)
	pthrow prcserror << fileline(_prj_source_name, line)
			<< "Version " << name
			<< " not found in repository" << dotendl;

    return data;
}

PrVoidError ProjectDescriptor::adjust_merge_parents()
{
    _deleted_markers->append (&_merge_parents_marker);
    _deleted_markers->append (&_new_merge_parents_marker);

    _append_merge_parents = true;

    Return_if_fail(verify_merge_parents());

    delete_merge_parents (_merge_parents);

    _merge_parents = _new_merge_parents;

    _new_merge_parents = NULL;

    return NoError;
}

/* Keywords
 */

void ProjectDescriptor::add_keyword (const char* key, const char* val)
{
    _alter_popkey = true;

    ASSERT (! _project_keywords_extra->lookup (key), "blah");

    _project_keywords->insert (key, val);
    _project_keywords_extra->insert (key, val);
}

void ProjectDescriptor::rem_keyword (const char* key)
{
    _alter_popkey = true;

    ASSERT (_project_keywords_extra->lookup (key), "blah");

    _project_keywords->remove (key);
    _project_keywords_extra->remove (key);
}

void ProjectDescriptor::set_keyword (const char* key, const char* val)
{
    _alter_popkey = true;

    ASSERT (_project_keywords_extra->lookup (key), "blah");

    _project_keywords->insert (key, val);
    _project_keywords_extra->insert (key, val);
}

void ProjectDescriptor::add_populate_ignore (const char* key)
{
    _alter_popkey = true;

    ASSERT (! _populate_ignore->lookup (key), "blah");

    _populate_ignore->insert (key, key);
}

void ProjectDescriptor::rem_populate_ignore (const char* key)
{
    _alter_popkey = true;

    ASSERT (_populate_ignore->lookup (key), "blah");

    _populate_ignore->remove (key);
}

OrderedStringTable* ProjectDescriptor::project_keywords_extra() const
{
    return _project_keywords_extra;
}

KeywordTable* ProjectDescriptor::project_keywords(FileEntry* fe, bool setting)
{
    if (!_keyword_id) {
	_keyword_id = new Dstring;
	_keyword_pversion = new Dstring;
	_keyword_pheader = new Dstring;
    } else if (!setting) {
	/* Values don't matter, just the keywords. */
	return _project_keywords;
    }

    if (setting) {
	ASSERT(fe->last_mod_date() != NULL &&
	       fe->last_mod_user() != NULL, "missing entries in file entry");

	_keyword_id->sprintf("%s %s %s %s",
			     strip_leading_path(fe->working_path()),
			     fe->descriptor_version_number(),
			     fe->last_mod_date(),
			     fe->last_mod_user());

	_project_keywords->insert("Id", _keyword_id->cast());
	_project_keywords->insert("Author", fe->last_mod_user());
	_project_keywords->insert("Date", fe->last_mod_date());
	_project_keywords->insert("Basename", strip_leading_path(fe->working_path()));
	_project_keywords->insert("Revision", fe->descriptor_version_number());
	_project_keywords->insert("Source", fe->working_path());
    } else {
	_project_keywords->insert("Id", "");
	_project_keywords->insert("Author", "");
	_project_keywords->insert("Date", "");
	_project_keywords->insert("Basename", "");
	_project_keywords->insert("Revision", "");
	_project_keywords->insert("Source", "");
    }

    if (_keyword_pversion->length() == 0) {
	_keyword_pversion->sprintf("%s.%s",
				   project_version_major()->cast(),
				   project_version_minor()->cast());

	_keyword_pheader->sprintf("%s %s %s %s",
				  project_name(),
				  _keyword_pversion->cast(),
				  checkin_time()->cast(),
				  checkin_login()->cast());

	_project_keywords->insert("Project", project_name());
	_project_keywords->insert("ProjectDate", checkin_time()->cast());
	_project_keywords->insert("ProjectAuthor", checkin_login()->cast());
	_project_keywords->insert("ProjectMajorVersion", project_version_major()->cast());
	_project_keywords->insert("ProjectMinorVersion", project_version_minor()->cast());
	_project_keywords->insert("ProjectVersion", _keyword_pversion->cast());
	_project_keywords->insert("ProjectHeader", _keyword_pheader->cast());
    }

    return _project_keywords;
}


/* File Insertion
 */

void ProjectDescriptor::append_files_data (const char* data)
{
    _files_insertion_point->append (data);
}

void ProjectDescriptor::append_file (const char* working_path,
				     bool keywords)
{
    _files_insertion_point->append ("\n  (");
    _files_insertion_point->append_protected (working_path);
    _files_insertion_point->append (" ()");

    if (keywords)
	_files_insertion_point->append (" :no-keywords");

    _files_insertion_point->append (")");
}

void ProjectDescriptor::append_link (const char* working_path,
				     const char* link_name)
{
    _files_insertion_point->append ("\n  (");
    _files_insertion_point->append_protected (working_path);
    _files_insertion_point->append (" (");
    _files_insertion_point->append_protected (link_name);
    _files_insertion_point->append (") :symlink)");
}

void ProjectDescriptor::append_directory (const char* working_path)
{
    _files_insertion_point->append ("\n  (");
    _files_insertion_point->append_protected (working_path);
    _files_insertion_point->append (" () :directory)");
}

void ProjectDescriptor::append_file_deletion (FileEntry* fe)
{
    _files_insertion_point->append ("\n  ; (");
    _files_insertion_point->append_protected (fe->working_path());
    _files_insertion_point->append (" ()");

    fe->file_attrs()->print_to_string (_files_insertion_point, true);

    _files_insertion_point->append (")");
}

void ProjectDescriptor::append_file (FileEntry* fe)
{
    _files_insertion_point->append ("\n  (");
    _files_insertion_point->append_protected (fe->working_path());
    if (fe->file_type() == RealFile && fe->descriptor_name()) {
	_files_insertion_point->append (" (");
	_files_insertion_point->append_protected (project_name());
	_files_insertion_point->append ("/");
	_files_insertion_point->append_protected (fe->descriptor_name());
	_files_insertion_point->append (" ");
	_files_insertion_point->append_protected (fe->descriptor_version_number());
	_files_insertion_point->sprintfa (" %03o)", fe->file_mode());
    } else {
	_files_insertion_point->append (" ()");
    }

    fe->file_attrs()->print_to_string (_files_insertion_point, true);

    _files_insertion_point->append (")");
}

/* Attributes */

PrcsAttrs::PrcsAttrs (const DstringPtrArray *vals0, int ngroup)
    :_vals(*vals0),
     _ngroup(ngroup),
     _type(RealFile),
     _project(NULL),
     _keyword_sub(true)
{
    memcpy (_merge_actions, default_merge_actions, sizeof (_merge_actions));
    memcpy (_merge_descs,   default_merge_descs  , sizeof (_merge_descs));

    const char* env;

    env = get_environ_var ("PRCS_MERGE_COMMAND");

    if (env)
	_mergetool.assign (env);

    env = get_environ_var ("PRCS_DIFF_COMMAND");

    if (env)
	_difftool.assign (env);
}

FileType PrcsAttrs::type() const { return _type; }
bool PrcsAttrs::keyword_sub() const { return _keyword_sub; }
ProjectDescriptor* PrcsAttrs::project() const { return _project; };
MergeAction PrcsAttrs::merge_action (int rule) const { return _merge_actions[rule]; };
const char* PrcsAttrs::merge_desc (int rule) const { return _merge_descs[rule]; };
const char* PrcsAttrs::merge_tool () const { return _mergetool.length() > 0 ? _mergetool.cast() : NULL; }
const char* PrcsAttrs::diff_tool () const { return _difftool.length() > 0 ? _difftool.cast() : NULL; }

void PrcsAttrs::print_to_string (Dstring* str, bool lead_if) const
{
    if (lead_if && _vals.length() > _ngroup)
	str->append (' ');

    for (int i = _ngroup; i < _vals.length(); i += 1) {
	str->append (_vals.index(i)->cast());

	if (i < (_vals.length() - 1))
	    str->append (' ');
    }
}

void PrcsAttrs::print (ostream& os, bool lead_if) const
{
    if (lead_if && _vals.length() > _ngroup)
	os << ' ';

    for (int i = _ngroup; i < _vals.length(); i += 1) {
	os << _vals.index(i);

	if (i < (_vals.length() - 1))
	    os << ' ';
    }
}

bool PrcsAttrs::regex_matches (reg2ex2_t *re) const
{
    foreach (ds_ptr, &_vals, DstringPtrArray::ArrayIterator) {
	if (prcs_regex_matches ((*ds_ptr)->cast(), re))
	    return true;
    }

    return false;
}

PrVoidError ProjectDescriptor::validate_attrs (const PrcsAttrs *attrs, const char* name)
{
    bool seen_type = false;

    foreach (ds_ptr, &attrs->_vals, DstringPtrArray::ArrayIterator) {
	Dstring an = (*ds_ptr)->cast();
	Dstring av;

	const char* equal = strchr (an.cast(), '=');

	if (equal) {
	    av.assign (equal + 1);
	    an.truncate (equal - an.cast());
	}

	const AttrDesc* ad = is_file_attribute (an.cast(), an.length());

	if (!ad)
	    pthrow prcserror  << "Unrecognized attribute " << an << " for file " << squote(name) << dotendl;

	switch (ad->type) {
	case RealFileAttr:
	case DirectoryAttr:
	case SymlinkAttr:
	    if (seen_type)
		pthrow prcserror << "Duplicate file type attribute for file " << squote(name) << dotendl;
	    seen_type = true;
	    break;
	case Mergerule1Attr:
	case Mergerule2Attr:
	case Mergerule3Attr:
	case Mergerule4Attr:
	case Mergerule5Attr:
	case Mergerule6Attr:
	case Mergerule7Attr:
	case Mergerule8Attr:
	case Mergerule9Attr:
	case Mergerule10Attr:
	case Mergerule11Attr:
	case Mergerule12Attr:
	case Mergerule13Attr:
	case Mergerule14Attr:
	    if (av.length() != 1 || strchr("andrm", av.index(0)) == NULL)
		pthrow prcserror << "Illegal merge action " << av << dotendl;
	    break;
	case MergetoolAttr:
	case DifftoolAttr:
	    if (av.length() < 1)
		pthrow prcserror << "Illegal tool name " << av << dotendl;
	default:
	    break;
	}
    }

    return NoError;
}

void ProjectDescriptor::init_attrs (PrcsAttrs *attrs)
{
    attrs->_project = this;

    foreach (ds_ptr, &attrs->_vals, DstringPtrArray::ArrayIterator) {
	Dstring an = (*ds_ptr)->cast();
	Dstring av;

	const char* equal = strchr (an.cast(), '=');

	if (equal) {
	    av.assign (equal + 1);
	    an.truncate (equal - an.cast());
	}

	const AttrDesc* ad = is_file_attribute (an.cast(), an.length());
	int mergerule = 0;

	switch (ad->type) {
	case RealFileAttr:
	case ProjectFileAttr:
	    attrs->_type = RealFile;
	    break;
	case DirectoryAttr:
	case ImplicitDirectoryAttr:
	    attrs->_type = Directory;
	    break;
	case SymlinkAttr:
	    attrs->_type = SymLink;
	    break;
	case NoKeywordAttr:
	    attrs->_keyword_sub = false;
	    break;
	case MergetoolAttr:
	    attrs->_mergetool.assign (av);
	    break;
	case DifftoolAttr:
	    attrs->_difftool.assign (av);
	    break;
	case TagAttr:
	    break;
	case Mergerule14Attr: mergerule += 1;
	case Mergerule13Attr: mergerule += 1;
	case Mergerule12Attr: mergerule += 1;
	case Mergerule11Attr: mergerule += 1;
	case Mergerule10Attr: mergerule += 1;
	case Mergerule9Attr: mergerule += 1;
	case Mergerule8Attr: mergerule += 1;
	case Mergerule7Attr: mergerule += 1;
	case Mergerule6Attr: mergerule += 1;
	case Mergerule5Attr: mergerule += 1;
	case Mergerule4Attr: mergerule += 1;
	case Mergerule3Attr: mergerule += 1;
	case Mergerule2Attr: mergerule += 1;
	case Mergerule1Attr: /* notice zero origin */
	    attrs->_merge_descs[mergerule] = "User supplied action, no help available";
	    attrs->_merge_actions[mergerule] = (MergeAction)av.index(0);
	    break;
	}
    }
}

PrPrcsAttrsPtrError ProjectDescriptor::intern_attrs (const DstringPtrArray* tagvals,
						     int ngroup,
						     const char* name,
						     bool validate)
{
    PrcsAttrs *it = new PrcsAttrs (tagvals, ngroup);
    PrcsAttrs **lu;

    lu = _attrs_table->lookup (it);

    if (lu) {
	delete it;
	return *lu;
    } else {
	if (validate)
	    Return_if_fail (validate_attrs (it, name));

	init_attrs (it);

	_attrs_table->insert (it, it);

	return it;
    }
}

bool attrs_equal(const PrcsAttrs*const& x, const PrcsAttrs*const& y)
{
    if (x->_ngroup != y->_ngroup || x->_vals.length() != y->_vals.length())
	return false;

    for (int i = 0; i < x->_vals.length(); i += 1) {
	if (strcmp (x->_vals.index(i)->cast(), y->_vals.index(i)->cast()) != 0)
	    return false;
    }

    return true;
}

extern int hash(const char *const& s, int M);

int attrs_hash(const PrcsAttrs*const & s, int M)
{
    int h = 0;
    const char* p;

    for (int i = 0; i < s->_vals.length(); i += 1) {
	p = s->_vals.index(i)->cast();
	h += hash (p, M);
    }

    return h % M;
}


/* Updates and Deletions.
 */

PrVoidError ProjectDescriptor::write_project_file(const char* filename)
{
    if (strcmp (*project_version_name(), project_name()) != 0) {

	_project_version_point->sprintfa ("\n;; PRCS note: this project version was "
					  "originally checked in when\n;; the project "
					  "was named `%s'.\n",
					  project_version_name()->cast());

	project_version_name()->assign(project_name());
    }

    project_version_name()->assign(project_name());

    if (strcmp(parent_version_minor()->cast(), "-*-") != 0)
	parent_version_name()->assign(project_name());

    created_by_major()->assign_int(prcs_version_number[0]);
    created_by_minor()->assign_int(prcs_version_number[1]);
    created_by_micro()->assign_int(prcs_version_number[2]);

    if (_alter_popkey) {
	_deleted_markers->append (& _populate_ignore_marker);
	_deleted_markers->append (& _project_keywords_marker);

	int first = 0;

	_project_keywords_point->append ("(Project-Keywords");
	_populate_ignore_point->append  ("(Populate-Ignore (");

	int ks = _project_keywords_extra->key_array()->length();

	for (int i = 0; i < ks; i += 1) {
	    int nspaces = (first * strlen ("(Project-Keywords")) + 1;

	    for (int j = 0; j < nspaces; j += 1)
		_project_keywords_point->append (" ");

	    _project_keywords_point->append ("(");
	    _project_keywords_point->append_protected (_project_keywords_extra->key_array()->index(i));
	    _project_keywords_point->append (" ");
	    _project_keywords_point->append_string (_project_keywords_extra->data_array()->index(i));
	    _project_keywords_point->append (")");

	    if (i < (ks - 1))
		_project_keywords_point->append ("\n");

	    first = 1;
	}

	first = 0;

	ks = _populate_ignore->key_array()->length();

	for (int i = 0; i < ks; i += 1) {
	    int nspaces = (first * strlen ("(PopulateIgnore ( "));

	    for (int j = 0; j < nspaces; j += 1)
		_populate_ignore_point->append (" ");

	    _populate_ignore_point->append_string (_populate_ignore->key_array()->index(i));

	    if (i < (ks - 1))
		_populate_ignore_point->append ("\n");

	    first = 1;
	}

	_project_keywords_point->append (")");
	_populate_ignore_point->append ("))");
    }

    Return_if_fail(make_subdirectories(filename));

    WriteableFile outfile;

    Return_if_fail (outfile.open(filename));

    really_write_project_file (outfile.stream());

    Return_if_fail (outfile.close());

    if (_read_mode != 0) {
	If_fail (Err_chmod (filename, _read_mode))
	    prcswarning << "Could not reset permissions on file " << squote (filename) << dotendl;
    }

    return NoError;
}

static int sort_marker (const void* a, const void* b)
{
    return (*((const ListMarker**)a ))->start -
	   (*((const ListMarker**)b ))->start;
}

void ProjectDescriptor::really_write_project_file (ostream& os)
{
    int buffer_index   = 0;
    int buffer_length  = _segment->length() - 1 /* the null */;

    int file_number    = 0;
    int marker_number  = 0;
    int estring_number = 0;

    int file_number_max = _file_entries->length();
    int marker_number_max = 0;
    int estring_number_max = _all_estrings->length();

    if (_deleted_markers) {
	marker_number_max = _deleted_markers->length();

	/* File entries and Estrings are in sorted order, they are read that way. */
	/* Markers must be sorted. */
	_deleted_markers->sort(sort_marker);
    }

    int next_file_index;
    int next_marker_index;
    int next_estring_index;
    int next_index;

    while (buffer_index <= buffer_length) {
	/* markers are never skipped, so get the next index. */
	if (marker_number < marker_number_max)
	    next_marker_index = _deleted_markers->index(marker_number)->start;
	else
	    next_marker_index = buffer_length + 1;

	/* estrings and files may be skipped by markers, so if the index
	 * found is less than the buffer_index, then its been skipped and
	 * we incr their number until one ahead of the point is found. */
	do {
	    if (file_number < file_number_max)
		next_file_index = _file_entries->index(file_number)->ent_marker()->start;
	    else
		next_file_index = buffer_length + 1;
	} while (next_file_index < buffer_index && (file_number += 1, true));

	do {
	    if (estring_number < estring_number_max)
		next_estring_index = ((Estring*)_all_estrings->index(estring_number))->offset();
	    else
		next_estring_index = buffer_length + 1;
	} while (next_estring_index < buffer_index && (estring_number += 1, true));

	next_index = next_estring_index < next_file_index   ? next_estring_index : next_file_index;
	next_index = next_index         < next_marker_index ? next_index         : next_marker_index;

	ASSERT (buffer_index <= next_index, "no way man!");

	if (next_index > buffer_length)
	    break;

	if (buffer_index < next_index) {
	    os.write (_segment->segment() + buffer_index, next_index - buffer_index );
	    buffer_index = next_index;
	} else if (next_index == next_marker_index) {
	    const ListMarker* marker = _deleted_markers->index(marker_number++);

	    buffer_index = marker->stop;
	} else if (next_index == next_estring_index) {
	    Estring* string = (Estring*) _all_estrings->index(estring_number);
	    char missing_char = _nullified_chars->index(estring_number++);

	    switch (string->type()) {
	    case EsStringLiteral:

		for (int i = 0; i < string->length(); i += 1) {
		    char c = string->index(i);

		    if (c == '\"' || c == '\\')
			os << '\\';

		    os << c;
		}

		ASSERT (missing_char == '\"', "must be");

		os << missing_char;

		buffer_index = string->offset() + string->offset_length() + 1;

		break;
	    case EsNameLiteral:
		print_protected (string->cast(), os);

		os << missing_char;

		buffer_index = string->offset() + string->offset_length() + 1;

		break;
	    case EsUnProtected:
		os << string;

		break;
	    }

	} else /* (next_index == next_file_index) */ {
	    FileEntry *fe = _file_entries->index (file_number++);

	    os << "(";
	    print_protected(fe->working_path(), os);
	    os << " (";

	    if (fe->file_type() == SymLink && fe->link_name()) {
		print_protected(fe->link_name(), os);
	    } else if (fe->file_type() == RealFile &&
		       fe->descriptor_name() &&
		       fe->descriptor_version_number()[0] != 0) {
		print_protected (project_name(), os);
		os << "/";
		print_protected (fe->descriptor_name(), os);

		char buf[8];
		sprintf(buf, "%03o", fe->file_mode());
		os << " " << fe->descriptor_version_number()
		   << " " << buf;
	    }

	    os << ")";

	    fe->file_attrs()->print (os, true);

	    os << ")";

	    buffer_index = fe->ent_marker()->stop;
	}
    }

    if (_append_merge_parents) {
	write_merge_parents ("Merge-Parents", _merge_parents, os);
	write_merge_parents ("New-Merge-Parents", _new_merge_parents, os);
    }
}

void ProjectDescriptor::write_merge_parents (const char* name,
					     MergeParentEntryPtrArray *entries,
					     ostream& os)
{
    os << "(" << name;

    if (entries) {
	if (entries->length() > 0)
	    os << "\n";

	for (int i = 0; i < entries->length(); i += 1) {
	    MergeParentEntry *entry = entries->index(i);

	    os << "  (" << entry->selected_version << " ";

	    switch (entry->state) {
	    case MergeStateIncomplete:
		os << "incomplete"; break;
	    case MergeStateParent:
		os << "complete"; break;
	    case MergeStateIncompleteParent:
		os << "incomplete-parent"; break;
	    }

	    int j = 0;

	    for (; j < entry->all_files.length(); j += 1) {
		MergeParentFile* file = entry->all_files.index(j);

		if (!(j % 2)) os << "\n    ";

		os << "(";
		if (file->working)
		    print_protected (file->working, os);
		else
		    os << "()";
		os << " ";
		if (file->common)
		    print_protected(file->common, os);
		else
		    os << "()";
		os << " ";
		if (file->selected)
		    print_protected(file->selected, os);
		else
		    os << "()";
		os << " " << (char)file->action << ")";

		if (!(j % 2)) os << " ";
	    }

	    if (j % 2) os << "\n  ";

	    os << ")\n";
	}
    }

    os << ")\n";
}

void ProjectDescriptor::delete_file (FileEntry* fe)
{
    _deleted_markers->append(fe->ent_marker());
}

/* Quick Elim
 */

void ProjectDescriptor::read_quick_elim()
{
    _quick_elim = NULL;

    If_fail(_quick_elim << ::read_quick_elim(project_aux_path()))
	return;
}

void ProjectDescriptor::quick_elim_update()
{
    if(!_quick_elim)
	return;

    _quick_elim->update_file(project_aux_path());
}

void ProjectDescriptor::quick_elim_update_fe(FileEntry* fe)
{
    if(!_quick_elim)
	_quick_elim = new QuickElim;

    _quick_elim->update(fe);
}

/* Match Fileent
 */
void ProjectDescriptor::populate_tables (void)
{
    _file_name_table = new FileTable(pathname_hash, pathname_equal);
    _file_desc_table = new FileTable(pathname_hash, pathname_equal);
    _file_match_cache = new EntryTable;

    foreach_fileent (fe_ptr, this) {
	FileEntry *fe = *fe_ptr;

	_file_name_table->insert (fe->working_path(), fe);

	if (fe->file_type() == RealFile && fe->descriptor_name())
	    _file_desc_table->insert (fe->descriptor_name(), fe);
    }
}

PrFileEntryPtrError ProjectDescriptor::match_fileent (FileEntry* fe)
{
    ProjectDescriptor *project = fe->project();

    /* fe is a FileEntry belonging to another project.  Try
     * to match it according to the standard rules (which are new
     * in 1.2.0b8). */

    if (!_file_name_table)
	populate_tables();

    FileEntry **cache_lu = _file_match_cache->lookup (fe);

    if (cache_lu)
	return *cache_lu;

    FileEntry **name_lu = _file_name_table->lookup(fe->working_path());

    if (fe->file_type() != RealFile || !fe->descriptor_name())
	return name_lu ? *name_lu : NULL;

    FileEntry **desc_lu = _file_desc_table->lookup(fe->descriptor_name());

    if (name_lu && desc_lu && *desc_lu != *name_lu) {
	prcsquery << "An attempt was made to match files between project version "
		  << full_version() << " and " << project->full_version()
		  << ".  The result is ambiguous, due to (perhaps pathological) "
	    "naming and renaming of files.  The file " << squote (fe->working_path())
		  << " in version " << full_version() << " matches by name with "
		  << squote ((*name_lu)->working_path()) << " and by file family with "
		  << squote ((*desc_lu)->working_path()) << " in version "
		  << project->full_version() << ".  You are advised to abort and prevent this "
		  << " from occuring, if possible.  You may continue by selecting "
	    "one of the matches, or abort.  "
		  << force ("Using the file family match")
		  << report ("Use the file family match")
		  << allow_bang (_match_bang)
		  << help (bad_match_help_string)
		  << option ('n', "Continue matching by name")
		  << defopt ('f', "Continue matching by file family")
		  << query ("Select");

	char c;

	Return_if_fail (c << prcsquery.result());

	if (c == 'n') {
	    _file_match_cache->insert (fe, *name_lu);
	    return *name_lu;
	} else {
	    _file_match_cache->insert (fe, *desc_lu);
	    return *desc_lu;
	}
    } else if (desc_lu) {
	return *desc_lu;
    } else if (name_lu) {
	return *name_lu;
    } else
	return (FileEntry*) 0;
}

bool ProjectDescriptor::can_add (FileEntry *fe)
{
    if (!_file_name_table)
	populate_tables();

    FileEntry **name_lu = _file_name_table->lookup(fe->working_path());

    if (name_lu)
	return false;

    if (fe->file_type() != RealFile || !fe->descriptor_name())
	return true;

    FileEntry **desc_lu = _file_desc_table->lookup(fe->descriptor_name());

    return desc_lu == NULL;
}

/* Estring
 */

Estring::Estring (ProjectDescriptor* project, char* s, size_t off, size_t len, EstringType type)
    :Dstring(), _offset(off), _len (len), _type(type)
{
    project->register_estring(this, s[len]);
    s[len] = 0;

    _filled = len;
    _vec = s;
}

Estring::Estring (ProjectDescriptor* project, size_t position)
    :Dstring(), _offset(position), _len (0), _type(EsUnProtected)
{
    project->register_estring(this, 0);
}

void Estring::modify()
{
    if (_alloc > 0 || _filled == 0)
	return;

    _alloc = 1;
    char* oldvec = _vec;

    while (_alloc < _filled + 1) _alloc <<= 1;

    _vec = NEWVEC0(char, _alloc);

    strcpy (_vec, oldvec);
}

void Estring::append_protected (const char* string)
{
    protect_path (string, this);
}

void Estring::append_string (const char* string)
{
    append ("\"");
    protect_string (string, this);
    append ("\"");
}

void ProjectDescriptor::register_estring(Estring* estring, char missing)
{
    if (!_all_estrings)
	_all_estrings = new DstringPtrArray;

    if (!_nullified_chars)
	_nullified_chars = new Dstring;

    _all_estrings->append (estring);
    _nullified_chars->append (missing);
}

bool Estring::is_dstring() { return false; }
EstringType Estring::type() const { return _type; };
size_t Estring::offset() const { return _offset; }
size_t Estring::offset_length() const { return _len; }

/* Parser
 */

#include "prj-names.h"

#define ENTRIES sizeof(ProjectDescriptor::_pftable)/sizeof(ProjectDescriptor::_pftable[0])

const int ProjectDescriptor::_prj_entries = ENTRIES;
bool ProjectDescriptor::_prj_entry_found[ENTRIES];

static MemorySegment *prj_seg;
static int prj_read_current_offset;

FILE* prj_input = NULL;

extern "C"
int prjinput(void* buf, int max_size)
{
    if (prj_input) {
	int c;

	If_fail (c << Err_fread( buf, max_size, prj_input))
	  return -1;

	return c;
    } else if (prj_seg) {
	int length = prj_seg->length() - 1;

	if (prj_read_current_offset + max_size > length)
	    max_size = length - prj_read_current_offset;

	memcpy (buf, prj_seg->segment() + prj_read_current_offset, max_size);

	prj_read_current_offset += max_size;

	return max_size;
    } else {
	return 0;
    }
}

PrVoidError ProjectDescriptor::parse_prj_file(FILE* infile)
{
    PrjTokenType tok;

    prj_seg = NULL;

    while (prj_lex() != PrjEof) { }

    _segment = prj_seg = new MemorySegment(true);

    prj_read_current_offset = 0;
    prj_lineno = 1;
    prj_lex_cur_index = 0;
    prj_lex_this_index = 0;

    Return_if_fail (prj_seg->map_file (_prj_source_name, fileno(infile)));

    _segment->append_segment("", 1); /* Added a null here for safety, going to
				      * have to get rid of it when writing. */

    memset(_prj_entry_found, 0, sizeof(_prj_entry_found));

    while (true) {
	tok = prj_lex();

	switch (tok) {
	case PrjNull:
	case PrjBadString:
	case PrjString:
	case PrjName:
	case PrjClose:
	    return read_parse_bad_token (tok);

	case PrjOpen:
	    Return_if_fail(read_list_checked(prj_lex_this_index));

	    break;
	case PrjEof:
	    for (int i = 0; i < _prj_entries; i += 1)
		if (_pftable[i].must_have && !_prj_entry_found[i])
		    pthrow prcserror << "Missing project file entry "
				    << squote(_pftable[i].name) << dotendl;

	    _end_of_buffer_point = new Estring (this, prj_lex_cur_index);

	    return NoError;
	}
    }
}

PrConstSexpPtrError ProjectDescriptor::read_list_elt()
{
    PrjTokenType tok = prj_lex();

    switch (tok) {
    case PrjEof:
    case PrjNull:
    case PrjBadString:
	return read_parse_bad_token (tok);
    case PrjString:
    case PrjName:
	return read_new_estring(tok);
    case PrjClose:
	return new Sexp (prj_lineno);
    case PrjOpen:
	break;
    }
    return read_list(prj_lex_this_index);
}

PrConstSexpPtrError ProjectDescriptor::read_new_estring(PrjTokenType type)
{
    int start_index = prj_lex_this_index + (type == PrjString);
    int stop_index = prj_lex_cur_index - (type == PrjString);

    Estring *estring = new Estring(this,
				   _segment->wr_segment() + start_index,
				   start_index,
				   stop_index - start_index,
				   type == PrjName ? EsNameLiteral : EsStringLiteral);

    if (strchr (estring->cast(), '\\')) {
	/* All names/strings are deslashified when read. */
	estring->truncate(0);
	correct_path(_segment->wr_segment() + start_index, estring);
    }

    return new Sexp (estring, prj_lineno);
}

PrVoidError ProjectDescriptor::read_parse_bad_token(PrjTokenType tok)
{
    switch (tok) {
    case PrjEof:
	pthrow prcserror << fileline(_prj_source_name, prj_lineno)
			<< "Unexpected EOF" << dotendl;
    case PrjNull:
	pthrow prcserror << fileline(_prj_source_name, prj_lineno)
			<< "Illegal null character" << dotendl;
    case PrjBadString:
	pthrow prcserror << fileline(_prj_source_name, prj_lineno)
			<< "Unterminated string constant" << dotendl;
    case PrjString:
    case PrjName:
	pthrow prcserror << fileline(_prj_source_name, prj_lineno)
			<< "Unexpected token: " << squote(prj_lex_val) << dotendl;
    case PrjClose:
	pthrow prcserror << fileline(_prj_source_name, prj_lineno)
			<< "Unexpected ')'" << dotendl;
    case PrjOpen:
	pthrow prcserror << fileline(_prj_source_name, prj_lineno)
			<< "Unexpected '('" << dotendl;
    }
    return NoError;
}

PrConstSexpPtrError ProjectDescriptor::read_list_token(PrjTokenType tok, int start_index)
{
    const Sexp *s1, *s2;
    Sexp *s;
    int start_line = prj_lineno;

    switch (tok) {
    case PrjEof: case PrjNull: case PrjBadString:
	return read_parse_bad_token (tok);

    case PrjString:
    case PrjName:
	Return_if_fail(s2 << read_new_estring (tok));

	Return_if_fail(s1 << read_list(start_index));

	s = new Sexp(s2, s1, prj_lineno);

	s->set_start (start_index);
	s->set_end (prj_lex_cur_index);

	return s;

    case PrjClose:
	s = new Sexp (prj_lineno);

	s->set_start (start_index);
	s->set_end (prj_lex_cur_index);

	return s;

    case PrjOpen:
	break;
    }

    Return_if_fail(s1 << read_list(prj_lex_this_index));
    Return_if_fail(s2 << read_list(start_index));

    s = new Sexp(s1, s2, start_line);

    s->set_start (start_index);
    s->set_end (prj_lex_cur_index);

    return s;
}

PrConstSexpPtrError ProjectDescriptor::read_list(int offset)
{
    return read_list_token (prj_lex(), offset);
}

PrVoidError ProjectDescriptor::read_list_checked(size_t offset)
{
    const PrjFields* pf;
    PrjTokenType tok;
    const Sexp* s;
    int h;

    tok = prj_lex();

    if (tok != PrjName)
	pthrow prcserror << fileline(_prj_source_name, prj_lineno)
			<< "Expecting label at head of list" << dotendl;

    pf = prj_lookup_func(prj_lex_val, prj_lex_len);
    h  = prj_lookup_hash(prj_lex_val, prj_lex_len);

    if (!pf)
	pthrow prcserror << "Unexpected label " << squote (prj_lex_val) << dotendl;

    if (_prj_entry_found[h] && h != prj_lookup_hash ("Files", strlen ("Files")))
	pthrow prcserror << fileline(_prj_source_name, prj_lineno)
			<< "Duplicate project file entry" << dotendl;

    _prj_entry_found[h] = true;

    if (pf->parse_func) {
	Return_if_fail (s << read_list_token (tok, offset));

	/* I finally found a use for the ->* operator, fear it. */
	/*Return_if_fail( (this ->* (pf->parse_func)) (s) );*/
	Return_if_fail( (this ->* (pf->parse_func)) (s) );

	delete s;
    } else {
	Return_if_fail( (this ->* (pf->read_func)) (offset) );
    }

    return NoError;
}
