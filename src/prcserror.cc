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


#include "prcs.h"

extern "C" {
#include <unistd.h>
#include <termios.h>
#include <stdarg.h>
#include <ctype.h>
#include <signal.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
}

#include "misc.h"

static const int min_tty_width = 40;
static char** argv_save = NULL;
static int argc_save;

const char* re_query_message = NULL;
int re_query_len;

#if !defined(PRCS_DEVEL) || !defined(__GNUG__)
ErrorToken global_error_token;
int return_if_fail_if_ne_val;
#endif

#if defined(__APPLE__)
 stdiobuf stdout_stream(STDOUT_FILENO);
 stdiobuf stderr_stream(STDERR_FILENO);
#else
 stdiobuf stdout_stream(stdout);
 stdiobuf stderr_stream(stderr);
#endif /* if defined(__APPLE__) */
strstreambuf query_stream;

static PrettyStreambuf pretty_stdout_stream(&stdout_stream, NULL);
static PrettyStreambuf pretty_stderr_stream(&stderr_stream, NULL);
static PrettyStreambuf pretty_severable_stream(&stderr_stream, &option_be_silent);
static PrettyStreambuf pretty_query_stream(&query_stream, NULL);
#ifdef PRCS_DEVEL
static PrettyStreambuf pretty_debug_stream(&stderr_stream, &option_n_debug);
#endif

PrettyOstream prcsoutput(&pretty_stdout_stream, NoError);
PrettyOstream prcsinfo(&pretty_severable_stream, NonFatalError);
PrettyOstream prcswarning(&pretty_severable_stream, NonFatalError);
PrettyOstream prcserror(&pretty_stderr_stream, NonFatalError);
#ifdef PRCS_DEVEL
PrettyOstream prcsdebug(&pretty_debug_stream, NonFatalError);
#endif

QueryOstream prcsquery(&query_stream, &pretty_query_stream,
		       &stdout_stream, &stderr_stream);

const char default_fail_query_message[] = "Fail and abort PRCS";

static inline int next_word_length(const char* s, int n)
{
    int length = 0;

    while(length < n && *s && !isspace(*s)) {
	s+=1;
	length += 1;
    }

    return length;
}

static inline void advance_buffer(const char*&s, int& n, int& col, int a)
{
    s += a;
    n -= a;
    col += a;
}

int PrettyStreambuf::overflow(int c)
{
    line_buffer.append(c);
    if(isspace(c))
	xsputn(NULL, 0);

    return 1;
}

int PrettyStreambuf::xsputn(const char* s0, int n0)
{
#ifndef __GNUG__
#  define xsputn sputn
#endif

    if (dont_print && *dont_print)
	return n0;

    int wordlen;
    const char* s = line_buffer.cast();
    int n = line_buffer.length();

    for(int i = 0; i < 2; i += 1, line_buffer.truncate(0), s = s0, n = n0) {
	if(n < 1)
	    continue;

	while(true) {
	    if(n <= 0) {
		break;
	    } else if(col == 0 && prefix.length() > 0) {
		while(n > 0 && isspace(*s)) {
		    s += 1;
		    n -= 1;
		}

		if(n == 0)
		    break;

		if(!new_line) {
		    for(int j = prefix.length(); j; j -= 1) {
			forward->xsputn(" ", 1);
			col += 1;
		    }
		} else {
		    forward->xsputn(prefix.cast(), prefix.length());
		    col += prefix.length();
		}

		while(isspace(*s) && *s != '\n' && *s != '\0') {
		    n -= 1;
		    s += 1;
		}
		if(n < 1)
		    continue;
	    }

	    if(!breakon)
		wordlen = n;
	    else
		wordlen = next_word_length(s, n);

	    if(wordlen == 0) {
		if(*s == '\n' || (col + 1 > width)) {
		    new_line = false;
		    forward->xsputn("\n", 1);
		    advance_buffer(s, n, col, 1);
		    col = 0;
		} else {
		    forward->xsputn(s, 1);
		    advance_buffer(s, n, col, 1);
		}
	    } else if(wordlen + col <= width) {
		forward->xsputn(s, wordlen);
		advance_buffer(s, n, col, wordlen);
	    } else if((wordlen + prefix.length() >= width) && (col == prefix.length())) {
		forward->xsputn(s, wordlen);
		advance_buffer(s, n, col, wordlen);
	    } else if(wordlen == 1 && col == width) {
		new_line = false;
		forward->xsputn(s, 1);
		advance_buffer(s, n, col, wordlen);
		forward->xsputn("\n", 1);
		col = 0;
	    } else {
		new_line = false;
		forward->xsputn("\n", 1);
		col = 0;
	    }
	}
    }

    return n;

#ifndef __GNUG__
#  undef xsputn
#endif
}

int PrettyStreambuf::sync()
{
    xsputn(NULL, 0);
#ifdef __GNUG__
    return forward->sync();
#else
    return forward->pubsync();
#endif
}

static int tty_width(int fileno)
{
    struct winsize ws;
    int width;

    if (ioctl(fileno, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	width = 79;
    else
	width = ws.ws_col - 1;

    return width < min_tty_width ? min_tty_width : width;
}

void re_query()
{
#ifdef __GNUG__
    stdout_stream.xsputn(re_query_message, re_query_len);
    stdout_stream.sync();
#else
    stdout_stream.sputn(re_query_message, re_query_len);
    stdout_stream.pubsync();
#endif
}

void continue_handler(SIGNAL_ARG_TYPE)
{
    if(re_query_message)
	re_query();
}

void adjust_streams()
{
    if (option_plain_format) {
	pretty_stdout_stream.set_fill_pretty (false);
	pretty_stderr_stream.set_fill_pretty (false);
	pretty_severable_stream.set_fill_pretty (false);
	pretty_query_stream.set_fill_pretty (false);
#ifdef PRCS_DEVEL
	pretty_debug_stream.set_fill_pretty (false);
#endif

	/* This is much easier than modifying the line-breaking
	 * function.  I think it works well enough too. */
	pretty_stdout_stream.set_fill_width (1<<20);
	pretty_stderr_stream.set_fill_width (1<<20);
	pretty_severable_stream.set_fill_width (1<<20);
	pretty_query_stream.set_fill_width (1<<20);
#ifdef PRCS_DEVEL
	pretty_debug_stream.set_fill_width (1<<20);
#endif
    }

    if (option_force_resolution || option_report_actions) {
	Dstring prefix(strip_leading_path(argv_save[0]));
	prefix.append(": ");

	pretty_query_stream.set_fill_prefix(prefix);
    }
}

void setup_streams(int argc, char** argv)
{
    argv_save = argv;
    argc_save = argc;

    Dstring prefix(strip_leading_path(argv[0]));
    prefix.append(": ");

    int stdout_width = tty_width(STDOUT_FILENO);
    int stderr_width = tty_width(STDERR_FILENO);

    pretty_stdout_stream.set_fill_width(stdout_width);
    pretty_stderr_stream.set_fill_width(stderr_width);
    pretty_query_stream.set_fill_width(stdout_width);
    pretty_severable_stream.set_fill_width(stderr_width);
    pretty_stdout_stream.set_fill_prefix(prefix);
    pretty_stderr_stream.set_fill_prefix(prefix);
    pretty_severable_stream.set_fill_prefix(prefix);
#ifdef PRCS_DEVEL
    pretty_debug_stream.set_fill_width(stderr_width);
    pretty_debug_stream.set_fill_prefix("DEBUG: ");
#endif
}

ostream& __omanip_squote(ostream& o, const char* s)
{
    PrettyStreambuf& ps = ((PrettyOstream&)o).ostreambuf();

    if (!ps.fill_pretty()) {
	o << s;
	return o;
    }

    bool cb = ps.set_fill_break(false);

    Dstring copy;
    copy.append('`');

    char c;

    while ((c = *s++) != 0) {
	if (!isgraph(c) && c != ' ')
	    copy.sprintfa("\\%03o", c);
	else
	    copy.append(c);
    }

    copy.append('\'');
    o << copy.cast();

    ps.set_fill_break(cb);

    return o;
}

ostream& __omanip_setcol(ostream& o, int col)
{
    PrettyStreambuf& ps = ((PrettyOstream&)o).ostreambuf();

#ifdef __GNUG__
    ps.xsputn(NULL, 0);
#else
    ps.sputn(NULL, 0);
#endif
    ps.set_column(col);

    return o;
}

void kill_prefix(ostream& o)
{
    PrettyStreambuf &ps = ((PrettyOstream&)o).ostreambuf();

    ps.set_fill_prefix("");
}

ostream& __omanip_fileline(ostream& o, FileLine fl)
{
    PrettyStreambuf &ps = ((PrettyOstream&)o).ostreambuf();

    Dstring old_prefix(ps.fill_prefix());

    ps.set_fill_prefix("");

    o << fl.file << ":" << fl.line << ": ";

    ps.set_fill_prefix(old_prefix);

    return o;
}

void PrettyStreambuf::set_fill_width(int width0)
{
    width = width0;
}

void PrettyStreambuf::set_fill_prefix(const char* prefix0)
{
    prefix.assign(prefix0);
}

bool PrettyStreambuf::set_fill_break(bool breakon0)
{
    bool oldbreakon = breakon;
    breakon = breakon0;
    return oldbreakon;
}

bool PrettyStreambuf::set_fill_pretty(bool prettyon0)
{
    bool oldprettyon = prettyon;
    prettyon = prettyon0;
    return oldprettyon;
}

int PrettyStreambuf::set_column(int col0)
{
    int diff = col0 - col;

    if(diff <= 0)
 	return 0;

    Dstring spaces(' ', diff);
    bool cb = set_fill_break(false);
    xsputn(spaces.cast(), diff);
    set_fill_break(cb);

    return col0;
}

void PrettyStreambuf::reset_column()
{
    col = 0;
    new_line = true;
}

ostream& prcsendl(ostream& s)
{
    PrettyStreambuf &ps = ((PrettyOstream&)s).ostreambuf();

#ifdef __GNUG__
    ps.xsputn("\n", 1);
#else
    ps.sputn("\n", 1);
#endif

    ps.reset_column();

    return s;
}

ostream& dotendl(ostream& s)
{
    PrettyStreambuf &ps = ((PrettyOstream&)s).ostreambuf();

#ifdef __GNUG__
    ps.xsputn(".\n", 2);
#else
    ps.sputn(".\n", 2);
#endif

    ps.reset_column();

    return s;
}

ostream& perror(ostream& s)
{
    PrettyStreambuf &ps = ((PrettyOstream&)s).ostreambuf();

    s << ": " << strerror(errno) << '\n';

    ps.reset_column();

    return s;
}

bool PrettyStreambuf::fill_pretty() const { return prettyon; }
const char* PrettyStreambuf::fill_prefix() const  { return prefix.cast(); }

PrettyStreambuf::PrettyStreambuf(streambuf *forward0, int* dont_print0)
    :streambuf(), forward(forward0), col(0),
     width(80), breakon(true), prettyon(true),
     new_line(true), dont_print(dont_print0) {
}

/* QueryOstream */
QueryOstream::QueryOstream(strstreambuf* base_stream0,
			   PrettyStreambuf* query_stream0,
			   stdiobuf* stdout_stream0,
			   stdiobuf* stderr_stream0)
    :
#ifndef __GNUG__
     ios(query_stream0),
#endif
     PrettyOstream(query_stream0, NoError),
     base_stream(base_stream0),
     query_stream(query_stream0),
     stdout_stream(stdout_stream0),
     stderr_stream(stderr_stream0),
     val(0), string_val((const char*) 0),
     default_option(0), force_option(0),
     force_message(NULL), report_message(NULL),
     option_count(0), bang_flag(NULL) { }

ostream& QueryOstream::option_manip(QueryOption opt)
{
    options[option_count++] = opt;
    return *this;
}

ostream& QueryOstream::default_manip(QueryOption opt)
{
    default_option = option_count;
    options[option_count++] = opt;
    return *this;
}

ostream& QueryOstream::report_manip(const char* message)
{
    report_message = message;
    return *this;
}

ostream& QueryOstream::force_manip(const char* message)
{
    force_message = message;
    return *this;
}

ostream& QueryOstream::force_manip(const char* message, char different)
{
    force_message = message;
    force_option = different;
    return *this;
}

ostream& QueryOstream::definput_manip(const char* input)
{
    default_input = input;
    return *this;
}

ostream& QueryOstream::bang(BangFlag* flag)
{
    bang_flag = flag;
    return *this;
}

ostream& QueryOstream::string_query_manip(const char* message)
{
#ifndef __GNUG__
#  define xsputn       sputn
#  define sync         pubsync
#  define seekoff(p,w) pubseekoff((p),(w), ios::out)
#endif
    if(option_force_resolution && option_be_silent) {
	/* silence */
    } else if(option_force_resolution) {
	*this << " -- " << default_input << prcsendl;
	string_val = PrConstCharPtrError(default_input);
	stderr_stream->xsputn(base_stream->str(),
			      base_stream->out_waiting());
	stderr_stream->sync();
    } else if(option_report_actions) {
	*this << " -- " << default_input << prcsendl;
	string_val = PrConstCharPtrError(default_input);
	stdout_stream->xsputn(base_stream->str(),
			      base_stream->out_waiting());
	stdout_stream->sync();
    } else {
	*this << "[" << default_input << "] " << message;
	query_stream->sync();

	re_query_message = base_stream->str();
	re_query_len = base_stream->out_waiting();

	while(true) {
	    stdout_stream->xsputn(re_query_message, re_query_len);
	    stdout_stream->sync();

	    Dstring *ans = new Dstring; /* leak */

	    if (!ans) ans = new Dstring;

	    If_fail(read_string(stdin, ans)) {
		cout << "\n";
		string_val = PrConstCharPtrError(UserAbort);
		break;
	    }

	    if(ans->length() > 0) {
		string_val = PrConstCharPtrError(ans->cast());
		break;
	    } else {
		string_val = PrConstCharPtrError(default_input);
		break;
	    }
	}
    }

    re_query_message = NULL;

    query_stream->reset_column();
    base_stream->freeze(0);
    base_stream->seekoff(0, ios::beg);
    option_count = 0;
    force_option = 0;
    bang_flag = NULL;
    help_string = NULL;

    return *this;
#ifndef __GNUG__
#  undef xsputn
#  undef sync
#  undef seekoff
#endif
}

ostream& QueryOstream::help_manip(const char* message)
{
    help_string = message;
    return *this;
}

ostream& QueryOstream::query_manip(const char* message)
{
#ifndef __GNUG__
#  define xsputn  sputn
#  define sync    pubsync
#  define seekoff(p,w) pubseekoff((p),(w), ios::out)
#endif
    if(force_option != 0 && option_force_resolution) {
        for (int i = 0; i < option_count; i += 1) {
	    if (force_option == options[i].let) {
	        default_option = i;
		break;
	    }
	}
    }

    if(option_force_resolution && option_be_silent) {
	/* silence */
	val = options[default_option].val;
    } else if(option_report_actions) {
	*this << report_message << dotendl;
	query_stream->sync();
	stdout_stream->xsputn(base_stream->str(),
			      base_stream->out_waiting());
	stdout_stream->sync();
	val = options[default_option].val;
    } else if(option_force_resolution) {
	*this << force_message << dotendl;
	query_stream->sync();
	stderr_stream->xsputn(base_stream->str(),
			      base_stream->out_waiting());
	stderr_stream->sync();
	val = options[default_option].val;
    } else if(bang_flag && bang_flag->flag) {
	*this << force_message << dotendl;
	query_stream->sync();
	stdout_stream->xsputn(base_stream->str(),
			      base_stream->out_waiting());
	stdout_stream->sync();
	val = options[default_option].val;
    } else {
	char query_buf[40];
	int query_buf_index = 0;

	query_buf[query_buf_index++] = '(';

	for(int i = 0; i < option_count; i += 1)
	    query_buf[query_buf_index++] = options[i].let;

	if (help_string)
	    query_buf[query_buf_index++] = 'h';

	query_buf[query_buf_index++] = 'q';

	if(bang_flag)
	    query_buf[query_buf_index++] = '!';

	query_buf[query_buf_index++] = '?';
	query_buf[query_buf_index++] = ')';
	query_buf[query_buf_index++] = '[';
	query_buf[query_buf_index++] = options[default_option].let;
	query_buf[query_buf_index++] = ']';
	query_buf[query_buf_index++] = ' ';
	query_buf[query_buf_index] = 0;

	*this << message << query_buf;

	query_stream->sync();

	re_query_message = base_stream->str();
	re_query_len = base_stream->out_waiting();

	while(true) {
	    char c;
	    int found = false;

	    stdout_stream->xsputn(re_query_message, re_query_len);
	    stdout_stream->sync();

	    c = get_user_char();

	    if(c == '\n') { /* User accepts default. */
		c = options[default_option].let;
	    } else if(c == '\0') { /* EOF. */
		cout << "\n";
		val = PrCharError(UserAbort);
		break;
	    } else if(c == '?') { /* User needs help. */
		prcsoutput << "Valid options are:" << prcsendl;
		for(int i = 0; i < option_count; i += 1) {
		    prcsoutput << options[i].let << " -- ";
		    if(i == default_option)
			prcsoutput << "[default] ";
		    prcsoutput << options[i].descr << dotendl;
		}
		prcsoutput << "q -- " << default_fail_query_message << dotendl;
		if(bang_flag) {
		    prcsoutput << "! -- Take default action each time this query "
			"is reached" << dotendl;
		}
		continue;
	    } else if(bang_flag && c == '!') {
		val = options[default_option].val;
		bang_flag->flag = true;
		found = true;
	    } else if(c == 'q') {
		val = PrVoidError(UserAbort);
		found = true;
	    } else if (help_string && c == 'h') { /* User needs lots of help. */
		prcsoutput << help_string << dotendl;
		continue;
	    }

	    for(int i = 0; i < option_count; i += 1) {
		if(c == options[i].let) {
		    val = options[i].val;
		    found = true;
		}
	    }

	    if(found) break;
	}
    }

    re_query_message = NULL;

    query_stream->reset_column();
    base_stream->freeze(0);
    base_stream->seekoff(0, ios::beg);
    force_option = 0;
    option_count = 0;
    bang_flag = NULL;
    help_string = NULL;

    return *this;
#ifndef __GNUG__
#  undef xsputn
#  undef sync
#  undef seekoff
#endif
}

ostream& __omanip_query(ostream& s, const char* message) {
    return ((QueryOstream&)s).query_manip(message); }
omanip<const char*> query(const char* message) {
    return omanip<const char*>(__omanip_query, message); }

ostream& __omanip_help(ostream& s, const char* message) {
    return ((QueryOstream&)s).help_manip(message); }
omanip<const char*> help(const char* message) {
    return omanip<const char*>(__omanip_help, message); }

ostream& __omanip_string_query(ostream& s, const char* message) {
    return ((QueryOstream&)s).string_query_manip(message); }
omanip<const char*> string_query(const char* message) {
    return omanip<const char*>(__omanip_string_query, message); }

ostream& __omanip_definput(ostream& s, const char* message) {
    return ((QueryOstream&)s).definput_manip(message); }
omanip<const char*> definput(const char* message) {
    return omanip<const char*>(__omanip_definput, message); }

ostream& __omanip_force(ostream& s, const char* message) {
    return ((QueryOstream&)s).force_manip(message); }
omanip<const char*> force(const char* message) {
    return omanip<const char*>(__omanip_force, message); }

ostream& __omanip_force(ostream& s, CharAndStr cas) {
    return ((QueryOstream&)s).force_manip(cas.str, cas.c); }
omanip<CharAndStr> force(const char* message, char different) {
    return omanip<CharAndStr>(__omanip_force, CharAndStr (message, different)); }

ostream& __omanip_report(ostream& s, const char* message) {
    return ((QueryOstream&)s).report_manip(message); }
omanip<const char*> report(const char* message) {
    return omanip<const char*>(__omanip_report, message); }

ostream& __omanip_option(ostream& s, QueryOption o) {
    return ((QueryOstream&)s).option_manip(o);}
omanip<QueryOption> option(char let, const char* descr) {
    return omanip<QueryOption>(__omanip_option, QueryOption(let, descr)); }
omanip<QueryOption> option(char let, const char* descr, ErrorToken tok) {
    return omanip<QueryOption>(__omanip_option, QueryOption(let, descr, tok)); }

omanip<QueryOption> optfail(char let) {
    return omanip<QueryOption>(__omanip_option, QueryOption(let, default_fail_query_message, UserAbort)); }

ostream& __omanip_bang(ostream& s, BangFlag* flag) {
    ((QueryOstream&)s).bang(flag); return s; }
omanip<BangFlag*> allow_bang(BangFlag& flag) {
    return omanip<BangFlag*>(__omanip_bang, &flag); }

ostream& __omanip_default(ostream& s, QueryOption o) {
    return ((QueryOstream&)s).default_manip(o); }
omanip<QueryOption> defopt(char let, const char* descr) {
    return omanip<QueryOption>(__omanip_default, QueryOption(let, descr)); }
omanip<QueryOption> defopt(char let, const char* descr, ErrorToken tok) {
    return omanip<QueryOption>(__omanip_default, QueryOption(let, descr, tok)); }

omanip<QueryOption> deffail(char let) {
    return omanip<QueryOption>(__omanip_default, QueryOption(let, default_fail_query_message, UserAbort)); }

omanip<FileLine> fileline(const char* file, int line) {
    return omanip<FileLine>(__omanip_fileline, FileLine(file, line)); }

omanip<const char*> squote(const char* file) {
    return omanip<const char*>(__omanip_squote, file); }
omanip<const char*> squote(const Dstring* file) {
    return omanip<const char*>(__omanip_squote, file->cast()); }
omanip<const char*> squote(const Dstring& file) {
    return omanip<const char*>(__omanip_squote, file.cast()); }


omanip<FileTuple> merge_tuple(FileEntry* work,
			      FileEntry* com,
			      FileEntry* sel) {
    return omanip<FileTuple>(__omanip_filetuple,
			     FileTuple(work, com, sel, FileTuple::MergeTriple)); }
omanip<FileTuple> diff_tuple(FileEntry* from,
			     FileEntry* to) {
    return omanip<FileTuple>(__omanip_filetuple,
			     FileTuple(from, to, NULL, FileTuple::DiffPair)); }

omanip<int> setcol(int col)
    { return omanip<int>(__omanip_setcol, col); }

#if defined(__MWERKS__) && defined(EXPLICIT_TEMPLATES)
/* Some methods are not defined, propably because
 * they haven't been used yet. However, to instantiate them
 * there must be something.
 */

template<class Type> PrError<Type>::operator!()
{
  ASSERT(false, "Undefined method PrError::operator!() called");
  return true;
}

#endif

#if EXPLICIT_TEMPLATES
#include "prcserror.tl"
#endif
