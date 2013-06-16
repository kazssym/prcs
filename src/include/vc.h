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
 * $Id: vc.h 1.6.1.5.1.11.1.1.1.10 Wed, 06 Feb 2002 20:57:16 -0800 jmacd $
 */


#ifndef _VC_H_
#define _VC_H_

#include "utils.h"
#include "config.h"

EXTERN int VC_check_token_match(const char* token, const char* type);

#ifdef __cplusplus    /* the C flex parser includes this and doesn't like
                         some of the C++ below (especially the include) */

PrVoidError VC_register(const char* newfile);

PrVoidError VC_checkin(const char* workingfile,
		       const char* parentversion,
		       const char* revisionfile,
		       const char* log,
		       void* data,
		       void (* notify_func)(void* data, const char* new_version));

PrCFilePtrError VC_checkout_stream(const char* version,
				   const char* revisionfile);

PrVoidError VC_close_checkout_stream(FILE* stream, const char* version,
				     const char* revisionfile);

PrVoidError VC_checkout_file(const char* newfile,
			     const char* version,
			     const char* revisionfile);

PrBoolError VC_delete_version(const char* version,
			      const char* revisionfile);

PrVoidError VC_set_log(const char* new_log_text,
		       const char* version,
		       const char* revisionfile);

/* Used to check a single version after checkin, needed for getting
 * +lines and -lines */
PrVoidError VC_get_one_version_data (const char     *versionfile,
				     const char     *versionnum,
				     RcsVersionData *rvd);

/* Used for a consistency check of P.prj,v and prcs_data files. */
PrIntError  VC_get_version_count    (const char* versionfile);

/*
 * RcsVersionData --
 *
 *     This data structure stores all the important information about
 *     an RCS delta as would be extracted from its log.  It contains
 *     the time it was checked in(seconds since the epoch), the
 *     author("jmacd"), the MD5 digest, and its version string("1.2") */
class RcsVersionData {
public:
    enum GCMark { Referenced, NotReferenced, ReferencedTraversed };

    RcsVersionData();
    virtual ~RcsVersionData();

    time_t date() const;
    int length() const;
    const char* author() const;
    const char* rcs_version() const;
    const char* unkeyed_checksum() const;
    int plus_lines () const;
    int minus_lines () const;
    GCMark referenced() const;

    void date(time_t t0);
    void length(int l0);
    void author(const char* a0);
    void rcs_version(const char* rv0);
    void unkeyed_checksum(const char* ck0);
    void reference(GCMark gc0);
    void set_lines(const char *l0);
    void set_plus_lines(const char *pl);
    void set_minus_lines(const char *ml);
    void set_plus_lines(int pl);
    void set_minus_lines(int ml);

    virtual bool OK() const;

protected:
    time_t _date;
    const char* _author;
    const char* _rcs_version;
    int         _plus_lines;
    int         _minus_lines;
    char _unkeyed_checksum[16];
    int _length;
    GCMark _gc_mark;
};

/*
 * RcsVersionData --
 *
 *     This data structure stores all the important information about
 *     an project version file's RCS delta.  It will always have the
 *     following data to be considered OK().
 *
 *     deleted -- whether this version has been deleted.  The delta remains
 *     for the purposes of tracking its history.
 *
 *     PrcsMajor, PrcsMinor, ParentMajor, ParentMinor -- strings representing
 *     the various version numbers.
 *
 *     The following members may or may not be present:
 *
 *     MergeParentMajors, MergeParentMinors -- If this version was merged
 *     with another, these represent the history of "correct" parents.  If
 *     version A.3 with parent A.2 was merged with B.4 with parent B.3
 *     and checked in, the version's parent will be B.4, and the first
 *     MergeParent will be A.3.
 */
class ProjectVersionData : public RcsVersionData {
public:
    ProjectVersionData(int index);
    virtual ~ProjectVersionData ();

    bool deleted() const;
    const char* prcs_major() const;
    const char* prcs_minor() const;
    int prcs_minor_int() const;

    int parent_count() const;
    int parent_index(int p_num) const;

    void deleted(bool del0);
    void prcs_major(const char* pm0);
    void prcs_minor(const char* pm0);
    void new_parent(int pi0);

    int version_index() const;

    void clear_flags();
    bool flag1(bool ts);
    bool flag2(bool ts);
    bool flag1() const;
    bool flag2() const;

    virtual bool OK() const;

    friend ostream& operator<<(ostream& o, const ProjectVersionData* pvd);

protected:

    const char* _prcs_major;
    const char* _prcs_minor;
    int         _prcs_minor_int;
    int         _index;
    IntArray    _parents;
    bool        _deleted;
    bool        _flag1;
    bool        _flag2;
};

/*
 * RcsDelta --
 *
 *     RCS version numbers are organized as described in rcsfile(5).  These
 *     two classes organize a set of deltas in a particular file so that
 *     lookup of a version string "1.2.3.4" may be done with 4 lookups.
 *     A RcsBranch has a branch number such as 1 in the above example and
 *     an array of RcsDeltas.  A RcsDelta has a delta number such as 2 in
 *     the above example and an array of branches.  They are constructed
 *     for an entire RCS file with a call to VC_construct_delta_array.
 */
class RcsDelta {
public:
    RcsDelta(RcsDelta* parent0);

    ~RcsDelta();

    RcsVersionData* lookup(const char* index);
    void insert(const char* index, RcsVersionData* data);

    class DeltaIterator {
    public:
	DeltaIterator(RcsDelta* d0);
	RcsVersionData* operator*() const;
	void next();
	bool finished() const;
    private:
	RcsDelta *_current;
	RcsVersionData* _current_data;
	bool _finished;
	int _index;
    };

    void remove(const char* index);
    int count() const;

    friend class DeltaIterator;

private:

    RcsVersionData* rcs_data;
    RcsDeltaPtrArray* delta_array;
    int insertion_count;

    /* for use by DeltaIterator */
    RcsDelta *parent;
    int my_iter_index;
};

PrRcsDeltaPtrError
VC_get_version_data(const char* versionfile);

PrProjectVersionDataPtrArrayPtrError
VC_get_project_version_data(const char* versionfile);

#endif

enum RcsToken {
    NoTokenMatch = 0,            /* This is actually what yylex() returns */
    RcsAbort,                    /* This matches the aborted message */

    InitialRevision,             /* Possible checkin outputs. */
    NewRevision,
    PrevRevision,

    RlogVersion,                 /* Possible rlog outputs. */
    RlogDate,                    /* RlogVersion also matches the co output */
    RlogAuthor,                  /* author: */
    RlogLines,                   /* lines: +x -x */

    PrcsMajorVersion,
    PrcsMinorVersion,
    PrcsParentMajorVersion,      /* Obsolete, used prior to 1.2 */
    PrcsParentMinorVersion,      /* Obsolete, used prior to 1.2 */
    PrcsDescendsMajorVersion,    /* Obsolete, used prior to 1.2 */
    PrcsDescendsMinorVersion,    /* Obsolete, used prior to 1.2 */
    PrcsVersionDeleted,
    PrcsParents,                 /* Used in versions 1.2 and later. */

    RlogTotalRevisions,
};

#endif
