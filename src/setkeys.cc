/* -*-Mode: C++;-*-
 * PRCS - The Project Revision Control System
 * Copyright (C) 1997  Josh MacDonald
 * Copyright (C) 1994  P. N. Hilfinger
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
#include <unistd.h>
#include <fcntl.h>
}

#include "prcs.h"
#include "setkeys.h"
#include "memseg.h"
#include "system.h"
#include "projdesc.h"
#include "fileent.h"

static ostream* output_file; /* non-null if outputting to a file. */
static MemorySegment *output_segment; /* non-null if outputting to memory. */

static int deletekeys; /* true iff unsetting keys. */
static KeywordTable* keywords;
static KeywordTable* keywords_replacing;

static const char* unchanged_segment;
static int unchanged_len;
static bool output_changed;

static PrConstCharPtrError process_normal_key(const char* inp, long len, bool simple, bool no_dollars);

typedef Pair <const char*, const char*> StringPair;

static const char* find_keyword(const char* s, int len, int* old_len, const char** key)
{
    int i;

    for (i = 0; i < len; i += 1) {
	if (s[i] == '$' || s[i] == ':')
	    break;
    }

    static Dstring* buf = NULL;

    if (!buf)
	buf = new Dstring;

    buf->assign (s, i);
    *old_len = buf->length();

    StringPair* lu = keywords->lookup_pair (buf->cast());

    if (lu) {
	(*key) = lu->x();
	return lu->y();
    } else
	return NULL;
}

static PrVoidError checked_write(const char* s, int len)
{
    if(output_file) {
	output_file->write(s, len);

	if (output_file->bad())
	    pthrow prcserror << "Write failed while setting keys" << perror;
    }

    if (output_segment)
	output_segment->append_segment(s, len);

    return NoError;
}

static PrVoidError output_text_segment(const char* s, long len)
{
    if (s == NULL) {
	/* nothing */
    } else if (len == 0) {
	return NoError;
    } else if(unchanged_segment+unchanged_len == s ||
	      memcmp(unchanged_segment+unchanged_len, s, len) == 0) {
	unchanged_len += len;
	return NoError;
    } else {
	output_changed = true;
    }

    if (unchanged_len > 0) {
	Return_if_fail(checked_write(unchanged_segment, unchanged_len));
	unchanged_segment += unchanged_len;
	unchanged_len = 0;
    }

    if (s != NULL)
	Return_if_fail(checked_write(s, len));

    return NoError;
}

static PrVoidError output_simple_key_replaced (const char* format, int format_len, bool no_dollars)
{
    while (format_len > 0) {
	switch (*format) {
	case '\\':
	    switch (format[1]) {
	    case 't':
		Return_if_fail(output_text_segment("\t", 1));
		break;
	    default:
		Return_if_fail(output_text_segment(format+1, 1));
		break;
	    }
	    format += 2; format_len -= 2;
	    break;
	case '$': {
	    const char* end_phrase;

	    Return_if_fail(end_phrase << process_normal_key(format+1, format_len-1, true, no_dollars));

	    if (end_phrase == NULL) {
		Return_if_fail(output_text_segment(no_dollars ? "|" : format, 1));
		format += 1; format_len -= 1;
	    } else {
		format_len -= end_phrase - format;
		format = end_phrase;
	    }
	    break;
	}
	default:
	    Return_if_fail(output_text_segment(format, 1));
	    format += 1; format_len -= 1;
	    break;
	}
    }

    return NoError;
}

/* Assuming that inp is positioned just after a '$' that might begin */
/* a keyword phrase other than Format, convert the phrase and send to */
/* output_text_segment, returning a pointer to just after the closing */
/* '$' in the input.  If this is not the start of a keyword phrase, */
/* return NULL.   If SIMPLE, then just output the keyword's value, */
/* without any surrounding keyword:...$ text. */
static PrConstCharPtrError process_normal_key(const char* inp, long len, bool simple, bool no_dollars)
{
    const char* end_phrase;
    const char* after_key;
    int new_len;
    const char* replace;
    const char* current_key;
    int key_len;

    end_phrase = (const char*)memchr(inp, '$', len);

    if (end_phrase == NULL || memchr(inp, '\n', end_phrase - inp) != NULL)
	return (const char*)0;

    end_phrase += 1;

    replace = find_keyword(inp, len, &key_len, &current_key);

    if (replace) {

	after_key = inp + key_len;
	new_len = strlen(replace);

	if ( deletekeys ) {
	    Return_if_fail(output_text_segment(inp, key_len));
	    Return_if_fail(output_text_segment("$", 1));
	} else	if (keywords_replacing->lookup (current_key)) {
	    return (const char*)0;
	} else {
	    if (! simple) {
		Return_if_fail(output_text_segment(inp, key_len));

		Return_if_fail(output_text_segment(": ", 2));
	    }

	    keywords_replacing->insert (current_key, current_key);

	    Return_if_fail (output_simple_key_replaced (replace, new_len, no_dollars));

	    keywords_replacing->remove (current_key);

	    if (! simple)
		Return_if_fail(output_text_segment(" $", 2));
	}

	return end_phrase;
    } else {
	return (const char*)0;
    }
}

/* Assuming that inp is positioned just after a '$' before the string */
/* "Format:", convert the phrase (with the subsequent line) and send to */
/* output_text_segment, returning a pointer to just after the replaced */
/* line in the input.  If this is not the start of a proper Format phrase, */
/* return NULL. */
static PrConstCharPtrError process_format(const char* inp0, long len0)
{
    const char* inp;
    const char* format;
    const char* end_format_line;
    int len, format_len;

    inp = inp0; len = len0;

    inp += 7; len -= 7;

    while (isspace(*inp) && len > 0)
	inp += 1, len -= 1;

    if (*inp != '\"')
	return (const char*)0;

    format = inp += 1;
    len -= 1;

    while(true) {
	if (len == 0 || *inp == '\n')
	    return (const char*)0;
	else if (*inp == '\"')
	    break;
	else if (*inp == '\\' && len > 1)
	    inp += 2, len -= 2;
	else
	    inp += 1, len -= 1;
    }

    format_len = inp - format;

    do {
	inp += 1, len -= 1;
    } while (len > 0 && *inp != '$' && *inp != '\n');

    if (len == 0 || *inp != '$')
	return (const char*)0;

    do {
	inp += 1; len -= 1;
    } while (len > 0 && *inp != '\n');

    if (len <= 0)
	return (const char*)0;

    end_format_line = inp;

    do {
	inp += 1; len -= 1;
    } while (len > 0 && *inp != '\n');

    Return_if_fail (output_text_segment(inp0, end_format_line - inp0 + 1));

    if ( deletekeys )
	return inp;

    Return_if_fail (output_simple_key_replaced (format, format_len, false));

    return inp;
}

/* Convert input[0 .. inputLen-1] according to keyword_number and		*/
/* keyword_pair_list as indicated in the comments at the beginning of this	*/
/* file, sending the results to output_text_segment.  */
static PrVoidError convert(const char* input_buffer, int input_buffer_len)
{
    output_changed = false;
    unchanged_len = 0;
    unchanged_segment = input_buffer;

    while(true) {
	const char* end_phrase;
	const char* next_possible_key;

	if (output_changed && !output_file && !output_segment)
	    return NoError;

	next_possible_key = (const char*)memchr(input_buffer, '$', input_buffer_len);

	if (next_possible_key == NULL) {
	    Return_if_fail(output_text_segment(input_buffer, input_buffer_len));
	    break;
	}

	next_possible_key += 1;

	Return_if_fail(output_text_segment(input_buffer, next_possible_key - input_buffer));

	input_buffer_len -= next_possible_key - input_buffer;
	input_buffer = next_possible_key;

	if (strncmp(input_buffer, "Format:", 7) == 0)
	    Return_if_fail(end_phrase << process_format(input_buffer, input_buffer_len));
	else
	    Return_if_fail(end_phrase << process_normal_key(input_buffer, input_buffer_len, false, true));

	if (end_phrase == NULL)
	    continue;

	input_buffer_len -= end_phrase - input_buffer;
	input_buffer = end_phrase;
    }

    return output_text_segment(NULL, 0);
}

/*****/

static MemorySegment buffer(false);

PrBoolError setkeys_internal(const char* input_buffer0,
			     int input_buffer_len0,
			     ostream* os,
			     MemorySegment* seg,
			     FileEntry *fe,
			     SetkeysAction action)
{
    KeywordTable replace_table;

    keywords = fe->project()->project_keywords (fe, action == Setkeys);
    deletekeys = action == Unsetkeys;

    output_segment = seg;
    output_file = os;

    keywords_replacing = &replace_table;

    if (seg) seg->clear_segment();

    Return_if_fail(convert(input_buffer0, input_buffer_len0));

    return output_changed;
}

PrBoolError setkeys_infile(const char* desc,
			   int fd,
			   int length,
			   const char* file1, FileEntry *fe,
			   SetkeysAction action)
{
    Return_if_fail (buffer.map_file(desc, fd, length));

    ofstream os(file1);

    Return_if_fail(setkeys_inputbuf(buffer.segment(), buffer.length(), &os, fe, action));

    os.close();

    if (os.bad())
	pthrow prcserror << "Write failed on file " << squote(file1) << perror;

    return output_changed;
}

PrBoolError setkeys(const char* file0, const char* file1, FileEntry *fe,
		    SetkeysAction action)
{
    Return_if_fail (buffer.map_file (file0));

    ofstream os(file1);

    Return_if_fail(setkeys_inputbuf(buffer.segment(), buffer.length(), &os, fe, action));

    os.close();

    if (os.bad())
	pthrow prcserror << "Write failed on file " << squote(file1) << perror;

    return output_changed;
}

PrBoolError setkeys_inputbuf(const char* input_buffer0,
			     int input_buffer_len0,
			     ostream* os,
			     FileEntry *fe,
			     SetkeysAction action)
{
    return setkeys_internal(input_buffer0, input_buffer_len0, os, NULL, fe, action);
}

PrBoolError setkeys_inoutbuf(const char* inputbuf,
			     int inputbuflen,
			     MemorySegment* seg,
			     FileEntry *fe,
			     SetkeysAction action)
{
    return setkeys_internal(inputbuf, inputbuflen, NULL, seg, fe, action);
}

PrBoolError setkeys_outbuf(const char* file,
			   MemorySegment* seg,
			   FileEntry *fe,
			   SetkeysAction action)
{
    Return_if_fail(buffer.map_file(file));

    return setkeys_internal(buffer.segment(), buffer.length(), NULL, seg, fe, action);
}
