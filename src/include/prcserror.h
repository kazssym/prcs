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


#ifndef _PRCSERROR_H_
#define _PRCSERROR_H_


#include <iostream>
#include <iomanip>
using namespace std;
#if defined(__GNUG__)
# include <ext/stdio_sync_filebuf.h>
  typedef __gnu_cxx::stdio_sync_filebuf<char> stdiobuf;
# include <sstream>
  typedef std::stringbuf strstreambuf;
#else
# include <fstream.h>
  typedef filebuf stdiobuf;
# include "be-strstream.h"
#endif


extern "C" {
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "config.h" /* So PRCS_DEVEL is defined. */
}

#include "dstring.h"

class PrettyOstream;
class QueryOstream;

extern QueryOstream prcsquery;

extern PrettyOstream prcsoutput;
extern PrettyOstream prcsinfo;
extern PrettyOstream prcswarning;
extern PrettyOstream prcserror;
#ifdef PRCS_DEVEL
extern PrettyOstream prcsdebug;
#endif

#if !defined(PRCS_DEVEL) || !defined(__GNUG__)
extern ErrorToken global_error_token;
extern int return_if_fail_if_ne_val;
#endif

#ifdef PRCS_DEVEL
#define DEBUG(command) prcsdebug << command << prcsendl
#else
#define DEBUG(command) (void)0
#endif

/*********************************************************************/
/************************ Error Return Types *************************/
/*********************************************************************/

/*
 * PrVoidError, NprVoidError --
 *
 *     The two classes below serve to allow functions to return either
 *     an error or some type, the typed errors are derived from these.
 *     The Void error types only return an error or not an error.  The
 *     difference between the two types is that PrVoidError represents
 *     an error which has been printed, and NprVoidError represents an
 *     error which has NOT been printed.  The following methods are
 *     available:
 *
 *     Constructor(NonErrorToken tok) -- constructs a non-error value
 *     Constructor(ostream& stream) -- (PrVoidError only) constructs a
 *          error value.  Assumes that stream is in fact a PrettyOstream,
 *          which has an ErrorVal() method.  In this manner, a PrVoidError
 *          may be constructed with the statement:
 *
 *               return prcserror << FatalError << "exec failed" << perror;
 *
 *     Constructor(ErrorToken tok) -- (NprVoidError only) constructs an
 *          error value with token type.
 *
 *     bool Error() -- returns true if THIS is an error, otherwise false.
 *
 *     ErrorToken ErrorVal() -- assumes Error() is true, and returns the token.
 *
 *     operator bool() -- returns Error();
 *
 *     PrVoidError VoidError() -- returns THIS.  This method is defined to
 *          help convert an non-void error type into a void error.  See
 *          PrError<T>
 */
class PrVoidError {
public:
    PrVoidError(NonErrorToken) :_tok(FatalError), _error(false) { }
#if defined(PRCS_DEVEL) && defined(__GNUG__)
    explicit
#endif
    PrVoidError(ErrorToken tok) :_tok(tok), _error(true) { }
    explicit PrVoidError(ostream& s);

    bool error() const {
#if !defined(PRCS_DEVEL) || !defined(__GNUG__)
	global_error_token = _tok;
#endif
	return _error; }
    ErrorToken error_val() const { return _tok; }
    PrVoidError void_error() const { return PrVoidError(error_val()); }

protected:
    ErrorToken _tok;
    bool _error;

private:
    operator bool () const { return error(); }
    int operator!();
};

#if defined(PRCS_DEVEL) && defined(__GNUG__)
class NprVoidError {
public:
    NprVoidError(NonErrorToken) :_error(false) { }
    NprVoidError(ErrorToken tok) :_tok(tok), _error(true) { }

    bool error() const { return _error; }
    ErrorToken error_val() const { return _tok; }
    NprVoidError void_error() const { return NprVoidError(error_val()); }

protected:

    ErrorToken _tok;
    bool _error;

private:
    operator bool () const { return error(); }
    int operator!();
};
#endif

/*
 * PrError<T>, NprError<T> --
 *
 *     The two classes below serve to allow functions to return either
 *     an error or some type.  The following methods are available:
 *
 *     Constructor(Type) -- a non error constructor
 *
 *     Constructor([N]PrVoidError) -- an error constructor, from a void
 *          error value.  This assumes Error() is true for the
 *          argument.  Itand allows PrError<T> to convert to
 *          PrError<S>, since by calling
 *          PrError<S>(PrError<T>.VoidError()).  In this manner, the
 *          macro Return_if_fail below allows the statement
 *
 *               Return_if_fail(x << foo());
 *
 *          to return from the function if foo() has the same print or
 *          non-print error type as the caller.
 *
 *     VoidError() -- returns the correct void error value.
 */
template <class Type>
class PrError : public PrVoidError {
public:
    PrError(const Type& val) :PrVoidError(NoError), _val(val) { }
    explicit PrError(ostream& s);
    PrError(PrVoidError err) :PrVoidError(err) { }
#if defined(PRCS_DEVEL) && defined(__GNUG__)
    explicit
#endif
    PrError(ErrorToken tok) :PrVoidError(tok) { }

    PrVoidError void_error() const { return PrVoidError(error_val()); }
    Type non_error_val() const { return _val; };

private:
    Type _val;
    int operator!();
};

#if defined(PRCS_DEVEL) && defined(__GNUG__)
template <class Type>
class NprError : public NprVoidError {
public:
    NprError(const Type& val) :NprVoidError(NoError), _val(val) { }
    NprError(ErrorToken tok) :NprVoidError(tok) { }
    NprError(NprVoidError err) :NprVoidError(err) { }

    NprVoidError void_error() const { return NprVoidError(error_val()); }
    Type non_error_val() const { return _val; };

private:
    Type _val;
    int operator!();
};
#endif

template <class Type>
const PrError<Type>& operator<<(Type& var, const PrError<Type>& val)
{
    if(!val.error())
	var = val.non_error_val();
    return val;
}

#if defined(PRCS_DEVEL) && defined(__GNUG__)
template <class Type>
const NprError<Type>& operator<<(Type& var, const NprError<Type>& val)
{
    if(!val.error())
	var = val.non_error_val();
    return val;
}
#endif


/*
 * if expr returns an error, return the same error
 */
#if defined(PRCS_DEVEL) && defined(__GNUG__)
#define Return_if_fail(expr) \
     ({ typeof(expr) _E_(expr); if((_E_).error()) return _E_.void_error(); })
#else
#define Return_if_fail(expr) \
     do { \
        if((expr).error()) \
	    return PrVoidError(global_error_token); \
     } while(false)
#endif

/*
 * if expr returns an error, return the same error, else, if
 * expr's value compares != to val, execute the following block
 */
#if defined(PRCS_DEVEL) && defined(__GNUG__)
#define Return_if_fail_if_ne(expr, val) \
     if( ({ typeof(expr) _E_(expr); \
            typeof(_E_.non_error_val()) _E_Val_(_E_.non_error_val()); \
	    if((_E_).error()) return _E_.void_error(); \
	    _E_Val_ != val; }) )
#else
/* This kind of loses, but oh well */
#define Return_if_fail_if_ne(expr, val) \
     Return_if_fail(return_if_fail_if_ne_val << expr); \
     if(val != return_if_fail_if_ne_val)
#endif

/*
 * since errors evaluate to true, this is just to act as a reminder
 * that an error(which evalulates true) is being checked for.
 */
#define If_fail(expr) if((expr).error())
#define Failure(expr) ((expr).error())

/* So that the error mechanism can be implemented with exception
 * handling later */
#define pthrow return 0 <

/*********************************************************************/
/*************************** Stream Types ****************************/
/*********************************************************************/

/* PrettyStreambuf --
 *
 *     Nice looking output in a harsh environment, or something.  This
 *     class formats all inputs by prepending a fill prefix and
 *     filling lines by greedily inserting line breaks wherever
 *     possible, as set by set_fill_break, with true indicating that
 *     it may break lines at whitespace.  The squote() modifier below
 *     single-quotes a string inside `single quotes' and prevents the
 *     streambuf from breaking lines at whitespace inside the quotes.
 *     It forwards all output to another stremabuf.  Currently, there
 *     are three of these used in PRCS.  The ostream prcserror's
 *     streambuf forwards its output to a stdiobuf(stderr).  The
 *     ostream prcsout's streambuf forwards its output to a
 *     stdiobuf(stdout).  The ostream prcsquery's streambuf forwards
 *     its output to a ostrstreambuf, since prcsquery doesn't know
 *     whether to send its output to stdout or stderr until the query
 *     is received.  */
class PrettyStreambuf : public streambuf {
public:
    PrettyStreambuf(streambuf* forward0, int* dont_print0);
    void set_fill_width(int width0);
    void set_fill_prefix(const char* prefix0);
    bool set_fill_break(bool breakon0);
    bool set_fill_pretty(bool prettyon0);
    int set_column(int col);
    void reset_column();

    const char* fill_prefix() const;
    bool fill_pretty() const;

#ifndef __GNUG__
protected:
#endif
    virtual int xsputn(const char* s, int n);
    virtual int overflow(int c = EOF);
    virtual int sync();

protected:
    streambuf* forward;
    int col, width;
    bool breakon;
    bool prettyon;
    bool new_line;
    int *dont_print;
    Dstring prefix;
    Dstring line_buffer;
};

/* PrettyOstream --
 *
 *     This is used for prcserror and prcsout, as detailed in the
 *     documentation for PrettyStreambuf, above.  */
class PrettyOstream : public ostream {
public:
    PrettyOstream(PrettyStreambuf* stream, ErrorToken err)
	:
#ifndef __GNUG__
         ios(stream),
#endif
         ostream(stream), _buf(stream), _err(err)
        { }
    PrettyOstream(PrettyStreambuf* stream, NonErrorToken err)
        :
#ifndef __GNUG__
         ios(stream),
#endif
	 ostream(stream), _buf(stream), _err(err)
        { }
    PrettyStreambuf& ostreambuf() const { return *_buf; }
    PrVoidError error() const { return _err; }
protected:
    PrettyStreambuf *_buf;
    PrVoidError _err;
};

/* I have the problem of, if a user hasn't specified -f (force, which
 * guarantees no interactive queries) ask the user a question, and
 * otherwise warn the user of some decision -f has caused.  You'd like
 * to keep the messages short and still allow help for
 * first-time-users.  Also, there is a -n open for only reporting
 * actions (like make).

 * In each of these cases, you want to output something different and
 * take a different course of actions, depending on several global
 * variables and possibly user input.  So I have devised the a class
 * QueryOstream to inherit from some type of ostream, which buffer
 * some initial part of a message and then a bunch of omanips for
 * declaring what you want it to print where.

       omanip force(const char*, ErrorToken);
       omanip force(const char*, NonErrorToken);

 * If -f is present, it will finish the output on the standard error and
 * set the given return value.

       omanip report(const char*, ErrorToken);
       omanip report(const char*, NonErrorToken);

 * If -n is present, it will finish the output on the standard output
 * and set the given return value.

       omanip option(char, const char*, ErrorToken);
       omanip option(char, const char*, NonErrorToken);
       omanip default(char, const char*, ErrorToken);
       omanip default(char, const char*, NonErrorToken);

 * Otherwise, tell prcsquery the options available, with a message to
 * display in case the user asks for help and a return value.
 * default() sets the default return value for if the user just types
 * enter.

       omanip query(const char*);

 * Finally, set the query text and finish the command.

       prcsquery << "Subproject file " << squote(subfile) << " contains errors.  "
                 << force("Ignoring entire subproject", NoError)
                 << report("Ignore entire subproject", NoError)
                 << option('n', "Fail and abort PRCS.", UserAbort)
                 << default('y', "Ignore this subproject, as if it were omitted "
                            "from the file-or-directory list on the command line.",
                            NoError)
                 << query("Ignore and continue");

 * If -f was present, on standard error:

   prcs: Subproject file `dir/file.prj' contains errors.  Ignoring entire
   prcs: subproject.

 * If -n was present, on standard output:

   prcs: Subproject file `dir/file.prj' contains errors.  Ignore entire
   prcs: subproject.

 * Otherwise, query the user:

   prcs: Subproject file `dir/file.prj' contains errors.  Ignore entire
   prcs: and continue(yn?)[y]?

 * where ? will yield:

   prcs: n -- Fail and abort PRCS.
   prcs: y -- Ignore this subproject, as if it were omitted from the
   prcs: file-or-directory list on the command line.
   prcs: Ignore and continue(yn?)[y]

 */

typedef PrError<char> PrCharError;
typedef PrError<const char*> PrConstCharPtrError;

struct QueryOption {
    QueryOption()
	:val(NoError) { }
    QueryOption(char let0, const char* descr0)
	:let(let0), descr(descr0), val(let0) { }
    QueryOption(char let0, const char* descr0, ErrorToken tok0)
	:let(let0), descr(descr0), val(tok0) { }

    char let;
    const char* descr;
    PrCharError val;
};

struct BangFlag {
    BangFlag() :flag(false) { }
    bool flag;
};

class QueryOstream : public PrettyOstream {
private:

#define MAX_QUERY_OPTIONS 10

public:
    QueryOstream(strstreambuf *base_stream0,
		 PrettyStreambuf* query_stream0,
		 stdiobuf* stdout_stream0,
		 stdiobuf* stderr_stream0);

    /*
     * Manipulator methods.
     */
    ostream& option_manip(QueryOption);
    ostream& default_manip(QueryOption);
    ostream& force_manip(const char* message);
    ostream& force_manip(const char* message, char different);
    ostream& report_manip(const char* message);
    ostream& query_manip(const char* message);
    ostream& string_query_manip(const char* message);
    ostream& definput_manip(const char* message);
    ostream& help_manip(const char* message);
    ostream& bang(BangFlag*);

    PrCharError result() const { return val; }
    PrConstCharPtrError string_result() const { return string_val; }

protected:

    strstreambuf *base_stream;
    PrettyStreambuf* query_stream;
    stdiobuf *stdout_stream, *stderr_stream;
    PrCharError val;
    PrConstCharPtrError string_val;

    QueryOption options[ MAX_QUERY_OPTIONS ];

    int default_option;
    int force_option;
    const char* force_message;
    const char* report_message;
    const char* default_input;
    const char* help_string;
    int option_count;
    BangFlag* bang_flag;
};

extern char const default_fail_query_message[];

#if !defined(__MWERKS__)
template <class TP> class omanip {
    ostream& (*_f)(ostream&, TP);
    TP _a;
public:
    omanip(ostream& (*f)(ostream&, TP), TP a) : _f(f), _a(a) { }

    friend ostream& operator<<(ostream& o, const omanip<TP>& m);
};
#endif

ostream& __omanip_query(ostream& s, const char* message);
omanip<const char*> query(const char* message);

ostream& __omanip_help(ostream& s, const char* message);
omanip<const char*> help(const char* message);

ostream& __omanip_string_query(ostream& s, const char* message);
omanip<const char*> string_query(const char* message);

ostream& __omanip_definput(ostream& s, const char* message);
omanip<const char*> definput(const char* message);

ostream& __omanip_force(ostream& s, const char* message);
omanip<const char*> force(const char* message);

struct CharAndStr {
  CharAndStr (const char* str0, char c0) :str(str0), c(c0) { }
  const char* str;
  char c;
};

ostream& __omanip_force(ostream& s, CharAndStr cs);
omanip<CharAndStr> force(const char* message, char different);

ostream& __omanip_report(ostream& s, const char* message);
omanip<const char*> report(const char* message);

ostream& __omanip_option(ostream& s, QueryOption o);
omanip<QueryOption> option(char let, const char* descr);
omanip<QueryOption> option(char let, const char* descr, ErrorToken tok);
omanip<QueryOption> optfail(char let);

ostream& __omanip_bang(ostream& s, BangFlag* flag);
omanip<BangFlag*> allow_bang(BangFlag& flag);

ostream& __omanip_default(ostream& s, QueryOption o);
omanip<QueryOption> defopt(char let, const char* descr);
omanip<QueryOption> defopt(char let, const char* descr, ErrorToken tok);
omanip<QueryOption> deffail(char let);

/* squote --
 *
 *     This manipulator takes a name and prints it on the ostream,
 *     with single quotes around it, and sets nobreak so that the name
 *     is not line broken.  */
ostream& __omanip_squote(ostream&, const char*);
omanip<const char*> squote(const char* file);
omanip<const char*> squote(const Dstring* file);
omanip<const char*> squote(const Dstring& file);

/* fileline --
 *
 *     This manipulator takes a filename and line number and prints
 *     formats an error message filename:lineno: error message.
 */
struct FileLine {
    FileLine(const char* file0, int line0) :file(file0), line(line0) { }
    const char* file;
    int line;
};

ostream& __omanip_fileline(ostream& s, FileLine fl);
omanip<FileLine> fileline(const char* file, int line);

/* filetuple --
 */
class FileEntry;
struct FileTuple {
    enum TupleType { DiffPair, MergeTriple };

    FileTuple(FileEntry* work, FileEntry* com, FileEntry* sel, TupleType type0)
	:work_fe(work), com_fe(com), sel_fe(sel), type(type0) { }

    FileEntry* work_fe; /* or from */
    FileEntry* com_fe;  /* or to */
    FileEntry* sel_fe;

    TupleType type;
};

ostream& __omanip_filetuple(ostream& s, FileTuple tup);
omanip<FileTuple> merge_tuple(FileEntry* work,
			      FileEntry* com,
			      FileEntry* sel);
omanip<FileTuple> diff_tuple(FileEntry* from,
			     FileEntry* to);


/* setcol --
 *
 *     This manipulator forces the min column to aid in aligning text.  */
ostream& __omanip_setcol(ostream&, int);
omanip<int> setcol(int col);

/*
 * Manipulators for class PrettyOstream which take no arguments
 */
extern ostream& dotendl(ostream& s);
extern ostream& prcsendl(ostream& s);
extern ostream& perror(ostream& s);

/* setup_streams initializes the three streams */
extern void setup_streams(int argc, char** argv);
extern const char* re_query_message;
extern int re_query_len;
extern void re_query();
/* call after command line has been parsed */
extern void adjust_streams();
extern void kill_prefix(ostream& s);
extern void continue_handler(SIGNAL_ARG_TYPE);

/* Error Type constructor definitions */
inline PrVoidError::PrVoidError(ostream& s)
    { *this = ((PrettyOstream&)s).error(); }

template <class type> inline PrError<type>::PrError(ostream& s)
    :PrVoidError(((PrettyOstream&) s).error()) { }

/* Serious kludge */

inline PrVoidError operator< (int /*x*/, ostream& os)
{
    return PrVoidError(os);
}

#endif
