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
#include <signal.h>
}

#include "prcs.h"
#include "hash.h"
#include "vc.h"
#include "syscmd.h"
#include "system.h"
#include "misc.h"
#include "memseg.h"

EXTERN char* rcs_text;       /* declared in vc.l */
EXTERN int VC_get_token(FILE* is);
EXTERN int VC_init_seg_get_c(char* seg, size_t len);

static bool VC_expect_token(FILE* stream, int type, Dstring* result);
static PrVoidError fix_version_data(const ProjectVersionDataPtrArray *pvda,
				    const DstringPtrArray            *old_fmt_parents);

extern "C" void vc_lex_fatal_error(const char* msg)
{
    prcserror << msg;

    if (errno != 0)
	prcserror << perror;
    else
	prcserror << dotendl;

    kill (getpid(), SIGINT);
}

void VC_init_seg_get(MemorySegment* seg)
{
    seg->append_segment("\0\0", 2);

    VC_init_seg_get_c((char*)seg->segment(), seg->length());
}

/*
 * VC_checkin --
 *
 *     Buffers checkins of working files into RCS versionfiles.  It
 *     first calls upon RCS to lock the parent version.  Currently,
 *     one fork to RCS is required for each file.  This could possibly
 *     be replaced in the future by either some analysis of the time
 *     stamp on the file (and assume nothing has been checked in since
 *     the last was checked in) or by parsing the RCS file according
 *     to rcsfile(5).  This is the simplest and most transparent way
 *     of doing this, however, it only relies on RCS's external
 *     interface.
 *
 *     when VC_checkin is called with all arguments except the log
 *     NULL, the actual checkin takes place.  Also, if the argument
 *     size approaches sysconf(_SC_ARG_MAX), the buffer will be
 *     flushed.  The checkin unlocks the RCS file.
 * Results:
 *     true if everything is going okay.  false if an error has
 *     occured.  the new RCS version assigned to newfile is returned
 *     in the Dstring *parentversion.  If parent version was empty,
 *     then checkin will fill RCS version 1.1.
 * Side Effects:
 *     if sucessful, the RCS files will be modified.
 * Parameters:
 *     newfile -- path of the working file.
 *     parentversion -- return buffer for new version number
 *     versionfile -- path of the RCS version file.
 *     log -- a log message
 */
typedef void (* CheckinHook)(void* data, const char* new_version);

struct CheckinData {
    CheckinData(const char* file0,
		const char* ver0,
		const char* log0,
		void* data0,
		CheckinHook notify_func0)
	:file(file0), ver(ver0), log(log0), data(data0),
	 notify_func(notify_func0) { }
    const char* file;
    const char* ver;
    const char* log;
    void* data;
    CheckinHook notify_func;
};

struct CheckinData2 {
    CheckinData2 (VoidPtrList* l0, CheckinHook notify_func0, int n0)
	:l(l0), notify_func(notify_func0), n(n0) { }
    VoidPtrList *l;
    CheckinHook notify_func;
    int n;
};

PrVoidError VC_checkin_stage2(MemorySegment*, MemorySegment*, int exit_val, void* vdata);
PrVoidError VC_checkin_stage3(MemorySegment*, MemorySegment*, int exit_val, void* vdata);

PrVoidError VC_checkin(const char* newfile,
		       const char* parentversion,
		       const char* versionfile,
		       const char* log,
		       void* data,
		       CheckinHook notify_func)
{
    CheckinData* c_data = new CheckinData(newfile, versionfile,
					   log, data, notify_func);

    if( newfile != NULL) {
	Dstring ver;
	ArgList *argl;

	Return_if_fail(argl << rcs_command.new_arg_list());

	/* if the file being checked in has a parent version, then
	 * instruct RCS to pick a good new version number by locking the
	 * parent version
	 */
        if (parentversion != NULL && parentversion[0]) {

	    ver.append("-l");
	    ver.append(parentversion);

	    argl->append("-q");
	    argl->append("-T"); /* preserve time stamp */
	    argl->append("-u");
	    argl->append("-M");
	    argl->append(ver.cast());
	    argl->append(versionfile);

	} else if (parentversion != NULL && !parentversion[0]) {
	    /*
	     * if the version is empty, assume the file will be the *
	     * initial version.  We still must try to lock the version
	     * in the case the file is being checked in by a different
	     * owner and no locks are set (eg. the project file, which
	     * doesn't track RCS versions).   To prove this:
	     * user1% rcs -i -U foo,v
	     * # creates initial version without strict locking
	     * user2% co foo
	     * user2% chmod +w foo
	     * # essentially what PRCS does, since it checks out on stdout
	     * # modify foo
	     * user2% ci foo
	     * # no locks set by user2, fail
	     * user1% ci foo
	     * # since user1 owns it, it succeeds, I guess.
	     *
	     * The point is, you won't see why this is neccesary until
	     * you try using two users.
	     */

	    argl->append("-q");
	    argl->append("-T"); /* preserve time stamp */
	    argl->append("-u");
	    argl->append("-M");
	    argl->append("-l");
	    argl->append(versionfile);
	}

	Return_if_fail(rcs_command.open_delayed (VC_checkin_stage2, c_data, false, false));
    } else {
        Return_if_fail(DelayedJob::delay_wait(true));

	DEBUG ("starting ci processes");

	Return_if_fail(VC_checkin_stage2(NULL, NULL, 0, c_data));

	Return_if_fail(DelayedJob::delay_wait(true));
    }

    return NoError;
}

PrVoidError VC_checkin_stage2(MemorySegment*, MemorySegment*, int exit_val, void* v_data)
{
    static VoidPtrList *accumulate = NULL;
    static ArgList     *args = NULL;

    CheckinData *c_data = (CheckinData*) v_data;

    const char* newfile = c_data->file;
    const char* log = c_data->log;
    const char* versionfile = c_data->ver;
    void* data = c_data->data;
    CheckinHook func = c_data->notify_func;

    delete c_data;

    if (exit_val != 0)
      pthrow prcserror << "RCS failed on file " << versionfile << dotendl;;

    if (!args && !accumulate && !newfile)
	return NoError;

    if (!args)
	args = new ArgList;

    if (newfile == NULL) {
	ArgList* ci_args;

	int file_count     = args->length() / 2;
	int ratio          = file_count / option_jobs;
	int files_per_proc = 4 > ratio ? 4 : ratio;
	int max_arg_size   = sysconf(_SC_ARG_MAX) - (4 * MAXPATHLEN);

#ifdef __BEOS__
        /* ci sometimes freezes when given more than one file,
         * and is remarkably slow when given more then 32 files in
         * any case. *sigh*
         */
        files_per_proc = 1;
#endif

	DEBUG ("using " << files_per_proc << " files per child");

	VoidPtrList* acc_end;

	accumulate = accumulate->nreverse();

        Dstring logmes;
	Dstring date;

	date.append("-d");
	date.append(get_utc_time());

	if (log) {
	    logmes.append("-m");
	    logmes.append(log);
	}

	for (int j = 0; j < file_count;) {

	    int arg_list_size = 0;
	    int this_round = 0;

	    acc_end = accumulate;

	    Return_if_fail(ci_args << ci_command.new_arg_list());

	    if (log)
		ci_args->append(logmes.cast());
	    ci_args->append(date.cast());

	    ci_args->append("-T");
	    ci_args->append("-f");

	    /*if (keep)
		ci_args->append("-u");*/

	    for (; j < file_count &&
		     arg_list_size < max_arg_size &&
		     this_round < files_per_proc; j += 1, this_round += 1) {
		ci_args->append(args->index(2*j));
		ci_args->append(args->index(2*j+1));

		arg_list_size += 2;
		arg_list_size += strlen(args->index(2*j));
		arg_list_size += strlen(args->index(2*j+1));

		acc_end = acc_end->tail();
	    }

	    DEBUG("ci with " << this_round << " jobs");

	    Return_if_fail(ci_command.open_delayed(VC_checkin_stage3,
						   new CheckinData2(accumulate, func, this_round),
						   false, true));

	    accumulate = acc_end;
	}

	delete args;

	args = new ArgList;

	accumulate = NULL;
    } else {
        accumulate = new VoidPtrList(data, accumulate);

        args->append(newfile);
        args->append(versionfile);
    }

    return NoError;
}

PrVoidError VC_checkin_stage3(MemorySegment*, MemorySegment* err_seg, int exit_val, void* v_data)
{
    CheckinData2* data = (CheckinData2*) v_data;

    int n            = data->n;
    CheckinHook func = data->notify_func;
    VoidPtrList *l   = data->l;
    VoidPtrList *t;

    delete data;

    if (exit_val != 0) {
	cerr.write(err_seg->segment(), err_seg->length());
	pthrow prcserror << "RCS ci failed on batch command" << dotendl;
    }

    VC_init_seg_get(err_seg);

    for (; n > 0; n -= 1) {

	int vclex = VC_get_token(NULL);

	if(vclex != NewRevision &&
	   vclex != InitialRevision)
	    pthrow prcserror << "Error scanning RCS ci output" << dotendl;

	if (l->head() != NULL)
	    (* func) (l->head(), rcs_text);

	t = l;

	l = l->tail();

	delete t;
    }

    return NoError;
}

/*
 * VC_register --
 *
 *     Used to create an initial RCS versionfile with no versions.  This
 *     avoids a condition in VC_checkin where RCS would be confused by
 *     a missing file.
 * Results:
 *     If sucessful, file has been registered.  Files are not actually
 *     created via a fork/exec to rcs until VC_register is called with
 *     NULL as its filename.  It will not register the same file twice
 *     even if the buffered files have not been created.  That is, two
 *     sucessive calls to VC_register with the same name will fail until
 *     VC_register(NULL) has been called.  After that, it will succeed
 *     again.  It is someone elses duty to check that the file doesn't
 *     already exist.
 *
 *     Should the buffer size approach sysconf(_SC_ARG_MAX), it will
 *     automatically flush the buffer.
 * Side Effects:
 *     when VC_register(NULL) gets called, files will be created.
 * Parameters:
 *     file names a file, or flushes the buffer if NULL.
 */
#define REGISTER_COUNT 64 /* I determined experimentally that this
			   * is reasonably optimal for both single
			   * and multi-process checkins. */
PrVoidError VC_register_callback (MemorySegment*,
				  MemorySegment*,
				  int exit_val,
				  void* data)
{
    bool *n_success = (bool*) data;

    if (exit_val != 0)
	*n_success = true;

    return NoError;
}

PrVoidError VC_register(const char* file)
{
    static ArgList *accumulate = NULL;
    static bool n_success;

    if ((accumulate == NULL || accumulate->length() == 0) && file == NULL)
        return NoError;

    if (accumulate == NULL)
        accumulate = new ArgList;

    if (file == NULL || accumulate->length() == REGISTER_COUNT) {
	ArgList *argl;

        Return_if_fail(argl << rcs_command.new_arg_list());

        argl->append("-i");          /* RCS init */
        argl->append("-t/dev/null"); /* No initial description message. */
        argl->append("-U");          /* unset strict locking */
        argl->append("-q");          /* be quiet */

	for(int i = 0; i < accumulate->length(); i += 1)
	    argl->append(accumulate->index(i));

        Return_if_fail(rcs_command.open_delayed(VC_register_callback,
						&n_success,
						false,
						false));

	accumulate->truncate(0);
    }

    if(file != NULL) {
	accumulate->append(file);
    } else {
	Return_if_fail(DelayedJob::delay_wait(true));

	bool tmp = n_success;
	n_success = false;

	if (tmp)
	    pthrow prcserror << "RCS -i failed on batch init" << dotendl;
    }

    return NoError;
}

/*
 * VC_expect_token --
 *
 *     Used when a certain token type is expected from the token stream.
 * Results:
 *     Returns true when the expected token type is found, false otherwise.
 *     the external variable rcs_text will also contain the token text.
 * Side Effects:
 *     One token is read from the stream via a call to VC_get_token.
 * Parameters:
 *     stream is the argument passed to VC_get_token, possibly NULL to
 *     continue reading the last stream passed. type is one of the
 *     #defined types in include/vc.h.  result gets the value of the
 *     token text if succesful, if non-null.
 */
static bool VC_expect_token(FILE* stream, int type, Dstring* result)
{
    if(type != VC_get_token(stream))
        return false;
    else if(result)
        result->assign(rcs_text);
    return true;
}

/*
 * VC_checkout_stream --
 *
 *     Retrieves version from revisionfile, into a stream.
 * Results:
 *     returns a FILE* opened for reading or NULL upon failure.
 * Side Effects:
 *     a file descriptor is open.  VC_close_checkout_stream should
 *     be called.
 * Parameters:
 *     versionfile names the pathname of an RCS file.  version is
 *     retrieved.
 */
PrCFilePtrError VC_checkout_stream(const char* version,
				   const char* revisionfile)
{
    ArgList *argl;
    Dstring ver("-p");

    Return_if_fail(argl << co_command.new_arg_list());

    ver.append(version);

    argl->append("-ko");
    argl->append(ver);
    argl->append(revisionfile);

    If_fail(co_command.open(true, true))
        pthrow prcserror << "Failed checking out version " << version
			 << " from file " << squote(revisionfile) << dotendl;

    return co_command.standard_out();
}

/*
 * VC_close_checkout_stream --
 *
 *     cleans up after a VC_checkout_stream command and checks that RCS
 *     did not report any errors.
 * Results:
 *     true if everything went okay.
 * Side effects:
 *     FILE *f will be closed.
 * Parameters:
 *     RCS versionfile file and version.
 */
#ifdef NDEBUG
#define UNUSED(x)
#else
#define UNUSED(x) x
#endif
PrVoidError VC_close_checkout_stream(FILE* UNUSED(f), const char* version, const char* file)
{
    if(!VC_expect_token(co_command.standard_err(), RlogVersion, NULL) ||
       strcmp(rcs_text, version) != 0)
        pthrow prcserror << "Consistency check failed checking out version "
			 << version << " from file " << squote(file) << dotendl;

    ASSERT(f == co_command.standard_out(), "no way, man!");

    Return_if_fail_if_ne(co_command.close(), 0)
	pthrow prcserror << "co exited with non-zero status" << dotendl;

    return NoError;
}

/*
 * VC_checkout_file --
 *
 *     checks out a file and places it in newfile.
 * Results:
 *     true if everything went okay.
 * Side effects:
 *     None.
 * Parameters:
 *     RCS versionfile file and version.  File is placed in newfile.
 */
PrVoidError VC_checkout_file(const char* newfile,
			     const char* version,
			     const char* revisionfile)
{
    FILE *infile, *outfile;

    Return_if_fail(infile << VC_checkout_stream(version, revisionfile));

    If_fail(outfile << Err_fopen(newfile, "w"))
        pthrow prcserror << "Can't open file " << squote(newfile)
		         << " for writing" << perror;

    Return_if_fail(write_file(infile, outfile));
    Return_if_fail(VC_close_checkout_stream(infile, version, revisionfile));

    If_fail(Err_fclose(outfile))
	pthrow prcserror << "Write failed on file " << squote(newfile) << perror;

    return NoError;
}

/*
 * VC_get_project_version_data --
 *
 *     Retrieves all the log information for a PRCS project file. (which
 *     is used to map PRCS versions to RCS versions) from an RCS versionfile.
 * Results:
 *     If return is successful, return value is a heap allocated array of
 *     ProjectVersionData.
 * Side Effects:
 *     Memory is allocated for the return value.
 * Parameters:
 *     versionfile names the pathname of an RCS file.
 */
PrProjectVersionDataPtrArrayPtrError
VC_get_project_version_data(const char* versionfile)
{
    ArgList *argl;
    int rev_count, i;

    Return_if_fail(argl << rlog_command.new_arg_list());

    argl->append(versionfile);

    Return_if_fail(rlog_command.open(true, false));

    if(VC_get_token(rlog_command.standard_out()) == RlogTotalRevisions)
	rev_count = atoi(rcs_text);
    else
	pthrow prcserror << "Unrecognized output from RCS rlog command on RCS file "
			<< squote(versionfile) << dotendl;

    ProjectVersionDataPtrArray* pvda = new ProjectVersionDataPtrArray (rev_count);

    if(rev_count == 0)
	return pvda;

    /* This crap is here for backward compatiblity.  The new formats
     * require knowing the index of the parent, not the version name,
     * for simplicity, but the versions arrive out of order, so the
     * parent names or NULL are stored until all versions arrive. */
    DstringPtrArray* old_fmt_parents = new DstringPtrArray (rev_count);
    DstringPtrArray* old_fmt_descends = new DstringPtrArray (rev_count);

    pvda->expand (rev_count, (ProjectVersionData *) 0);
    old_fmt_parents->expand (rev_count, (Dstring *) 0);
    old_fmt_descends->expand (rev_count, (Dstring *) 0);

    if(VC_get_token(NULL) != RlogVersion)
	goto error;

    for(i = 0; i < rev_count; i += 1) {
	RcsToken vclex;
	Dstring *par_ver_maj, *desc_ver_maj;
	ProjectVersionData* pvd = new ProjectVersionData(rev_count - 1 - i);

	pvd->rcs_version(p_strdup(rcs_text));

	pvda->index(rev_count - 1 - i, pvd);

	while(true) {
	    vclex = (RcsToken)VC_get_token(NULL);
	    if(vclex == RlogVersion || vclex == NoTokenMatch)
		break;
	    switch(vclex) {
	    case RlogLines:
		/* Don't care for the PVD lines */
		break;
	    case RlogDate:
		pvd->date(timestr_to_time_t(rcs_text));
		break;
	    case RlogAuthor:
		pvd->author(p_strdup(rcs_text));
		break;
	    case PrcsMajorVersion:
		pvd->prcs_major(p_strdup(rcs_text));
		break;
	    case PrcsMinorVersion:
		pvd->prcs_minor(p_strdup(rcs_text));
		break;
	    case PrcsDescendsMajorVersion:
		desc_ver_maj = new Dstring (rcs_text);

		vclex = (RcsToken)VC_get_token(NULL);

		if (vclex != PrcsDescendsMinorVersion)
		    goto error;

		if (strcmp (rcs_text, "0") == 0) {
		    /* Its a empty parent version, no parents. */
		    delete desc_ver_maj;
		    break;
		}

		desc_ver_maj->append ('.');
		desc_ver_maj->append (rcs_text);

		old_fmt_descends->index(rev_count - 1 - i, desc_ver_maj);

		break;
	    case PrcsParentMajorVersion:
		par_ver_maj = new Dstring (rcs_text);

		vclex = (RcsToken)VC_get_token(NULL);

		if (vclex != PrcsParentMinorVersion)
		    goto error;

		if (strcmp (rcs_text, "0") == 0) {
		    /* Its a empty parent version, no parents. */
		    delete par_ver_maj;
		    break;
		}

		par_ver_maj->append ('.');
		par_ver_maj->append (rcs_text);

		old_fmt_parents->index(rev_count - 1 - i, par_ver_maj);

		break;
	    case PrcsVersionDeleted:
		pvd->deleted(true);
		break;
	    case PrcsParents:

		if (rcs_text[0]) {
		    const char* pis = strtok (rcs_text, ":");
		    int pi = atoi (pis);

		    if (!pis[0] || pi < 0 || pi >= (rev_count - 1 - i))
			pthrow prcserror << "Invalid parent version index " << squote (rcs_text)
					<< " in " << squote (versionfile)
					<< " RCS version 1." << (rev_count - i) << dotendl;

		    pvd->new_parent(pi);

		    while ((pis = strtok (NULL, ":")) != NULL) {
			pi = atoi (pis);

			if (!pis[0] || pi < 0 || pi >= (rev_count - 1 - i))
			    pthrow prcserror << "Invalid parent version index " << squote (rcs_text)
					    << " in " << squote (versionfile)
					    << " RCS version 1." << (rev_count - i) << dotendl;

			pvd->new_parent(pi);
		    }
		}

		break;

	    case NoTokenMatch:
	    case RlogTotalRevisions:
	    case RlogVersion:
	    case PrevRevision:
	    case NewRevision:
	    case InitialRevision:
	    case PrcsParentMinorVersion:
	    case PrcsDescendsMinorVersion:
	    case RcsAbort:
		goto error;
	    }
	}

	if (!pvd->OK())
	    pthrow prcserror << "Incomplete version log information in RCS file "
			    << squote(versionfile)
			    << " version 1." << (rev_count - i) << dotendl;
    }

    Return_if_fail_if_ne(rlog_command.close(), 0)
	pthrow prcserror << "RCS rlog command returned non-zero status on RCS file "
			<< squote(versionfile) << dotendl;

    Return_if_fail(fix_version_data(pvda, old_fmt_parents));
    Return_if_fail(fix_version_data(pvda, old_fmt_descends));

    foreach (ds_ptr, old_fmt_parents, DstringPtrArray::ArrayIterator) {
	if (*ds_ptr) {
	    delete *ds_ptr;
	}
    }

    delete old_fmt_parents;

    return pvda;

error:
    delete old_fmt_parents;
    delete pvda;

    pthrow prcserror << "Error scanning RCS rlog output on RCS file "
		    << squote(versionfile) << dotendl;

}

/* This is here mostly for backward compatibility. */
static PrVoidError
fix_version_data(const ProjectVersionDataPtrArray *pvda,
		 const DstringPtrArray            *old_fmt_parents)
{
    /* Use a slow (O(n^2) in the number of versions) search here, it
     * avoids a lot of complexity, C++ hassle, and is only required
     * for versions prior to 1.2 during rebuild.  In fact, avert your
     * eyes, if I could start over, I would. */

    for (int i = 0; i < pvda->length(); i += 1) {
	if (old_fmt_parents->index(i)) {
	    for (int j = 0; j < i; j += 1) {
		const char* par_name = *old_fmt_parents->index(i);

		if (strcmp (pvda->index(j)->prcs_minor(), minor_version_of (par_name)) == 0 &&
		    strcmp (pvda->index(j)->prcs_major(), major_version_of (par_name)) == 0) {

		    pvda->index(i)->new_parent(j);
		    break;
		}
	    }
	}
    }

    return NoError;
}

/*
 * VC_get_version_data --
 *
 *     Retrieves all the log information from an RCS file.
 * Results:
 *     If return is successful, return value is a heap allocated array of
 *     RcsVersionData.
 * Side Effects:
 *     Memory is allocated for the return value.
 * Parameters:
 *     versionfile names the pathname of an RCS file.
 */
PrRcsDeltaPtrError
VC_get_version_data(const char* versionfile)
{
    ArgList *argl;
    int rev_count, i;

    Return_if_fail(argl << rlog_command.new_arg_list());

    argl->append(versionfile);

    Return_if_fail(rlog_command.open(true, false));

    if(VC_get_token(rlog_command.standard_out()) == RlogTotalRevisions)
	rev_count = atoi(rcs_text);
    else
	pthrow prcserror << "Unrecognized output from RCS rlog command on RCS file "
			<< squote(versionfile) << dotendl;

    RcsDelta* pvda = new RcsDelta(NULL);

    if(rev_count == 0)
	return pvda;

    if(VC_get_token(NULL) != RlogVersion)
	goto error;

    for(i = 0; i < rev_count; i += 1) {
	RcsToken vclex;
	RcsVersionData* pvd = new RcsVersionData;

	pvd->rcs_version(p_strdup(rcs_text));

	pvda->insert(pvd->rcs_version(), pvd);

	while(true) {
	    vclex = (RcsToken)VC_get_token(NULL);
	    if(vclex == RlogVersion || vclex == NoTokenMatch)
		break;
	    switch(vclex) {
	    case RlogDate:
		pvd->date(timestr_to_time_t(rcs_text));
		break;
	    case RlogAuthor:
		pvd->author(p_strdup(rcs_text));
		break;
	    case RlogLines:
		pvd->set_lines (rcs_text);
		break;
	    case PrcsMajorVersion:
	    case PrcsMinorVersion:
	    case PrcsParentMajorVersion:
	    case PrcsParentMinorVersion:
	    case PrcsDescendsMajorVersion:
	    case PrcsDescendsMinorVersion:
		/* These are okay to make really really ancient repositories
		 * work, probably only the prcs repository. */
		break;
	    case PrcsVersionDeleted:
	    case PrcsParents:
	    case NoTokenMatch:
	    case RlogTotalRevisions:
	    case RlogVersion:
	    case PrevRevision:
	    case NewRevision:
	    case InitialRevision:
	    case RcsAbort:
		goto error;
	    }
	}

	if(!pvd->OK())
	    pthrow prcserror << "Incomplete version log information in RCS file "
			    << squote(versionfile) << dotendl;
    }

    Return_if_fail_if_ne(rlog_command.close(), 0)
	pthrow prcserror << "RCS rlog command returned non-zero status on RCS file "
			<< squote(versionfile) << dotendl;

    return pvda;

error:
    pthrow prcserror << "Error scanning RCS rlog output on RCS file "
		    << squote(versionfile) << dotendl;

}

PrVoidError
VC_get_one_version_data (const char* versionfile, const char *versionnum, RcsVersionData *rvd)
{
    ArgList *argl;
    RcsToken vclex;

    Return_if_fail(argl << rlog_command.new_arg_list());

    Dstring vstr;

    vstr.append ("-r");
    vstr.append (versionnum);

    argl->append(vstr.cast ());
    argl->append(versionfile);

    Return_if_fail(rlog_command.open(true, false));

    if (VC_get_token(rlog_command.standard_out()) != RlogTotalRevisions) {
	pthrow prcserror << "Unrecognized output from RCS rlog command on RCS file "
			<< squote(versionfile) << dotendl;
    }

    if (VC_get_token(NULL) != RlogVersion || strcmp(versionnum, rcs_text) != 0) {
	goto error;
    }

    rvd->rcs_version(p_strdup(rcs_text));

    while(true) {
	vclex = (RcsToken)VC_get_token(NULL);
	if(vclex == RlogVersion)
	    goto error;
	if (vclex == NoTokenMatch)
	    break;
	switch(vclex) {
	case RlogDate:
	    rvd->date(timestr_to_time_t(rcs_text));
	    break;
	case RlogAuthor:
	    rvd->author(p_strdup(rcs_text));
	    break;
	case RlogLines:
	    rvd->set_lines (rcs_text);
	    break;
	case PrcsMajorVersion:
	case PrcsMinorVersion:
	case PrcsParentMajorVersion:
	case PrcsParentMinorVersion:
	case PrcsDescendsMajorVersion:
	case PrcsDescendsMinorVersion:
	    /* These are okay to make really really ancient repositories
	     * work, probably only the prcs repository. */
	    break;
	case PrcsVersionDeleted:
	case PrcsParents:
	case NoTokenMatch:
	case RlogTotalRevisions:
	case RlogVersion:
	case PrevRevision:
	case NewRevision:
	case InitialRevision:
	case RcsAbort:
	    goto error;
	}
    }

    if(!rvd->OK()) {
	pthrow prcserror << "Incomplete version log information in RCS file "
			 << squote(versionfile) << dotendl;
    }

    Return_if_fail_if_ne(rlog_command.close(), 0) {
	pthrow prcserror << "RCS rlog command returned non-zero status on RCS file "
			 << squote(versionfile) << dotendl;
    }

    return NoError;

error:
    pthrow prcserror << "Error scanning RCS rlog output on RCS file "
		    << squote(versionfile) << dotendl;

}


PrBoolError VC_delete_version(const char* version,
			      const char* revisionfile)
{
    ArgList* args;
    Dstring outdate;

    Return_if_fail(args << rcs_command.new_arg_list());

    outdate.append("-o");
    outdate.append(version);

    args->append("-q");
    args->append(outdate);
    args->append(revisionfile);

    Return_if_fail(rcs_command.open(true, true));

    Return_if_fail_if_ne(rcs_command.close(), 0)
	return false;

    return true;
}

PrVoidError VC_set_log(const char* new_log_text,
		       const char* version,
		       const char* revisionfile)
{
    Dstring log;
    ArgList* args;

    log.append("-m");
    log.append(version);
    log.append(":");
    log.append(new_log_text);

    Return_if_fail(args << rcs_command.new_arg_list());

    args->append(log);
    args->append(revisionfile);

    Return_if_fail(rcs_command.open(true, true));

    Return_if_fail_if_ne(rcs_command.close(), 0) {
	pthrow prcserror << "RCS exited abnormally while updating log" << dotendl;
    }

    return NoError;
}

/* RcsDelta */

void RcsDelta::insert(const char* index, RcsVersionData* data)
{
    insertion_count += 1;

    if(index[0] == '\0') {
	rcs_data = data;
	return;
    }

    char* end_ptr;
    int delta_num = (int)strtol(index, &end_ptr, 10);

    if(!delta_array)
	delta_array = new RcsDeltaPtrArray;

    delta_array->expand(delta_num + 1, (RcsDelta*)0);

    if(delta_array->index(delta_num) == NULL)
	delta_array->index(delta_num, new RcsDelta(this));

    if(*end_ptr)
	delta_array->index(delta_num)->insert(end_ptr + 1, data);
    else
	delta_array->index(delta_num)->insert(end_ptr, data);
}

RcsVersionData* RcsDelta::lookup(const char* index)
{
    char* end_ptr;
    int delta_num = (int)strtol(index, &end_ptr, 10);

    if(!index[0])
	return rcs_data;
    else if(!delta_array || delta_array->length() <= delta_num)
       return NULL;
    else if(delta_array->index(delta_num)) {
	if(*end_ptr)
	    return delta_array->index(delta_num)->lookup(end_ptr + 1);
	else
	    return delta_array->index(delta_num)->lookup(end_ptr);

    } else
	return NULL;
}

void RcsDelta::remove(const char* index)
{
    insertion_count -= 1;

    char* end_ptr;
    int delta_num = (int)strtol(index, &end_ptr, 10);

    if(!index[0])
	rcs_data = NULL;
    else if(!delta_array || delta_array->length() <= delta_num)
       /* its not here? */
       return;
    else if(delta_array->index(delta_num)) {
	if(*end_ptr)
	    delta_array->index(delta_num)->remove(end_ptr + 1);
	else
	    delta_array->index(delta_num)->remove(end_ptr);

    }
    /* else its not here whats up with that */
}

/* This does an iterative table walk, you dont have to read it */
void RcsDelta::DeltaIterator::next()
{
    static int array_length; /* this doesnt need to eat stack space */

    _index += 1;

    if(!_current->delta_array) {
	array_length = 0;
    } else {
	array_length = _current->delta_array->length();
    }

    /* Its a leaf, 0 is the length of the non-existant array,
     * the data of a particular node is returned after the
     * children have been passed over. */
    if(_index == array_length) {
	_current_data = _current->rcs_data;
	/* _current_data may be NULL if this version was just deleted,
	 * if so, call next() again. */
	if(!_current_data)
	    next();
	return;
    } else if(_index < array_length) {
	if(_current->delta_array->index(_index)) {
	    _current->my_iter_index = _index;
	    _current = _current->delta_array->index(_index);
	    _index = -1;
	}

	next();
	return;
    } else {
	if(!_current->parent) {
	    /* this is the root, so we have covered all of them. */
	    _finished = true;
	    return;
	} else {
	    /* Restore the parents state */
	    _current = _current->parent;
	    _index = _current->my_iter_index;
	    next();
	    return;
	}
    }
}

RcsDelta::~RcsDelta()
{
    if(delta_array) {
	foreach(child, delta_array, RcsDeltaPtrArray::ArrayIterator) {
	    if(*child) {
		delete *child;
	    }
	}

	delete delta_array;
    }

    if(rcs_data) {
	delete rcs_data;
    }
}


RcsDelta::RcsDelta(RcsDelta* parent0)
    :rcs_data(NULL), delta_array(NULL),
     insertion_count(0),
     parent(parent0), my_iter_index(-1) { }

int RcsDelta::count() const { return insertion_count; }

RcsDelta::DeltaIterator::DeltaIterator(RcsDelta* d0) :_current(d0), _finished(false), _index(-1) { next(); }
RcsVersionData* RcsDelta::DeltaIterator::operator*() const { return _current_data; }
bool RcsDelta::DeltaIterator::finished() const { return _finished; }

/* RcsVersionData */

RcsVersionData::RcsVersionData()
    :_date(0),
     _author(NULL),
     _rcs_version(NULL),
     _plus_lines (0),
     _minus_lines (0),
     _gc_mark(NotReferenced)  { }

RcsVersionData::~RcsVersionData() { }

void RcsVersionData::set_lines(const char *lines)
{
    ASSERT (lines[0] == '+', "the format is...");
    char *endp, *endp2;
    long pl = strtol (lines+1, &endp, 10);
    ASSERT (strncmp (endp, " -", 2) == 0, "the format is...");
    long ml = strtol (endp + 2, & endp2, 10);
    ASSERT (endp2[0] == '\n', "the format is...");
    _plus_lines = pl;
    _minus_lines = ml;
    //prcsoutput << "set lines: " << lines << " plus " << pl << " minus " << ml << prcsendl;
}

void RcsVersionData::set_plus_lines(int pls)
{
    _plus_lines = pls;
}

void RcsVersionData::set_minus_lines(int mls)
{
    _minus_lines = mls;
}

void RcsVersionData::set_plus_lines(const char *pls)
{
    char *endp;
    long  pl = strtol (pls, &endp, 10);
    ASSERT (endp[0] == 0, "bad format");
    _plus_lines = pl;
}

void RcsVersionData::set_minus_lines(const char *mls)
{
    char *endp;
    long  ml = strtol (mls, &endp, 10);
    ASSERT (endp[0] == 0, "bad format");
    _minus_lines = ml;
}

time_t RcsVersionData::date() const { return _date; }
int RcsVersionData::length() const { return _length; }
const char* RcsVersionData::author() const { return _author; }
const char* RcsVersionData::rcs_version() const { return _rcs_version; }
const char* RcsVersionData::unkeyed_checksum() const { return _unkeyed_checksum; }
int RcsVersionData::plus_lines() const { return _plus_lines; }
int RcsVersionData::minus_lines() const { return _minus_lines; }
RcsVersionData::GCMark RcsVersionData::referenced() const { return _gc_mark; }

void RcsVersionData::date(time_t t0) { _date = t0; }
void RcsVersionData::length(int l0) { _length = l0; }
void RcsVersionData::author(const char* a0) { _author = a0; }
void RcsVersionData::rcs_version(const char* rv0) { _rcs_version = rv0; }
void RcsVersionData::unkeyed_checksum(const char* ck0) { memcpy(_unkeyed_checksum, ck0, 16); }
void RcsVersionData::reference(GCMark gc0) { _gc_mark = gc0; }
bool RcsVersionData::OK() const { return _date > 0 && _author && _rcs_version; }

/* ProjectVersionData */

ProjectVersionData::ProjectVersionData(int index)
    :RcsVersionData(),
     _prcs_major(NULL), _prcs_minor(NULL),
     _prcs_minor_int(-1), _index(index),
     _deleted(false) { }
ProjectVersionData::~ProjectVersionData() { }

void ProjectVersionData::clear_flags() { _flag1 = false; _flag2 = false; }
bool ProjectVersionData::flag1(bool n) { bool o = _flag1; _flag1 = n; return o; }
bool ProjectVersionData::flag2(bool n) { bool o = _flag2; _flag2 = n; return o; }
bool ProjectVersionData::flag1() const { return _flag1; }
bool ProjectVersionData::flag2() const { return _flag2; }

bool ProjectVersionData::deleted() const { return _deleted; }
const char* ProjectVersionData::prcs_major() const { return _prcs_major; }
const char* ProjectVersionData::prcs_minor() const { return _prcs_minor; }
int ProjectVersionData::prcs_minor_int() const { return _prcs_minor_int; }
int ProjectVersionData::parent_count() const { return _parents.length(); }
int ProjectVersionData::parent_index(int p_num) const { return _parents.index(p_num); }
void ProjectVersionData::new_parent(int pi0) { _parents.append (pi0); }
int ProjectVersionData::version_index() const { return _index; }

void ProjectVersionData::deleted(bool del0) { _deleted = del0; }
void ProjectVersionData::prcs_major(const char* pm0) { _prcs_major = pm0; }
void ProjectVersionData::prcs_minor(const char* pm0) { _prcs_minor = pm0; _prcs_minor_int = atoi(pm0); }

bool ProjectVersionData::OK() const { return RcsVersionData::OK() && _prcs_major && _prcs_minor; }

ostream& operator<<(ostream& o, const ProjectVersionData* pvd)
{
    o << pvd->_prcs_major << "." << pvd->_prcs_minor;
    return o;
}
