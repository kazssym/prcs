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
 * $Id: vclex.l 1.5.1.1.1.16.1.9 Sun, 28 Oct 2001 00:54:02 -0700 jmacd $
 */

%option nounput
%option noinput
%option nostdinit
%option noyywrap
%option never-interactive
%option nostack
%option outfile="lex.yy.c"

%{
/* use this to allow yyout to be NULL and avoid echoing */
#include "utils.h"
#include "vc.h"
#include <ctype.h>
#include <string.h>
#include <errno.h>

void vc_lex_fatal_error(const char* msg);
char* rcs_text;
static char* killnl(char*);
static char* stop_at_ws(char*);

#define ECHO { if(yyout) (void)fwrite( yytext, yyleng, 1, yyout ); }
#define YY_FATAL_ERROR(msg) vc_lex_fatal_error(msg)

/* Note: how would (errno == EAGAIN) occur in the code below?  this
 * scanner is always called on a pipe opened from an RCS process, so
 * the local fd should not block.  However, it may apparently be
 * interupted, according to a report addressed to prcs-list by
 * Christian Hudon on Tue, 26 Jan 1999 16:00:20 -0500.
 */

#define YY_INPUT(buf, result, max_size) {                 \
    int nread;                                            \
    clearerr (yyin);                                      \
    errno = 0;                                            \
    do nread = fread (buf, 1, max_size, yyin);            \
    while (nread == 0 && (errno == EAGAIN || errno == EINTR)); \
    if (nread == 0 && ferror (yyin))                      \
	YY_FATAL_ERROR( "input in flex scanner failed" ); \
    else                                                  \
	result = nread;                                   \
}

%}

idchar  [a-zA-Z\-_+#^]
dig     [0-9]
num     {dig}+
dotnum  "."{num}
rcnum   {num}{dotnum}+
date    {num}"/"{num}"/"{num}
time    {num}":"{num}":"{num}
tad	{date}" "{time}
login   {dig}*{idchar}({dig}|{idchar})*
label	[A-Za-z0-9#^_+,][A-Za-z0-9#^\-_+=,.]*
lines   "+"{num}" -"{num}

%%

   /* The rlog information that I am interested in */
^"PRCS major version: "{label}\n { rcs_text = killnl(yytext + 20);
				   return PrcsMajorVersion; }
^"PRCS minor version: "{label}\n { rcs_text = killnl(yytext + 20);
				   return PrcsMinorVersion; }
^"PRCS parent major version: "{label}\n { rcs_text = killnl(yytext + 27);
					  return PrcsParentMajorVersion; }
^"PRCS parent minor version: "{label}\n { rcs_text = killnl(yytext + 27);
					  return PrcsParentMinorVersion; }
^"PRCS descends from major: "{label}\n { rcs_text = killnl(yytext + 26);
                                          return PrcsDescendsMajorVersion; }
^"PRCS descends from minor: "{label}\n { rcs_text = killnl(yytext + 26);
                                          return PrcsDescendsMinorVersion;}
^"PRCS descends from major: -*-\n" { }
^"PRCS descends from minor: -*-\n" { }
^"PRCS version deleted"\n { return PrcsVersionDeleted; }
^"PRCS parent indices: "(({num}":")*{num})?\n { rcs_text = killnl(yytext + 21);
                                                return PrcsParents; }

   /* this also serves as the co revision name */
^"revision "{rcnum}.*\n { rcs_text = stop_at_ws(yytext + 9);
		          return RlogVersion; }
^"date: "{tad} { rcs_text = yytext + 6;
		return RlogDate; }
";  author: "{login} { rcs_text = yytext + 11;
		       return RlogAuthor; }
";  state: "{label}";"(\n)? { }
"  lines: "{lines}\n { rcs_text = yytext + 9;
                        return RlogLines; }

   /* checkin stuff */
^"new revision: "{rcnum} { rcs_text = yytext + 14;
			  return NewRevision; }
^"initial revision: "{rcnum}\n { rcs_text = killnl(yytext + 18);
				 return InitialRevision; }
^"file is unchanged; reverting to previous revision "{rcnum}\n {
                               rcs_text = killnl(yytext + 50);
			       return PrevRevision; }
"; previous revision: "{rcnum}\n { }

^"total revisions: "{num}.*\n { rcs_text = yytext + 17;
                                return RlogTotalRevisions; }

^"done"\n { }
^.*",v  <--  ".*\n { }
^.*",v  -->  ".*\n { }

^"ci aborted"\n { return RcsAbort; }
^"rcs aborted"\n { return RcsAbort; }
^"rlog aborted"\n { return RcsAbort; }
^"co aborted"\n { return RcsAbort; }

   /* Stuff I'm not interested in. Mostly the rlog header. */
^-*\n { }
^=*\n { }
^"RCS file:".*\n { }
^"Working file:".*\n { }
^\t{login}": "{rcnum}\n { }
   /* \t"locked by: "{login}";"\n { } */
^"head:".*\n { }
^"branch".*\n { }
^"locks:".*\n { }
^"access list:".*\n { }
^"symbolic names:".*\n { }
^"keyword substitution:".*\n { }
^"description:".*\n { }
^"checked in by PRCS version".*\n { }
^"*** empty log message ***"\n { } /* This shouldn't really happen. */
^"*** no log ***"\n { }            /* This shouldn't really happen. */

%%


/*
 * VC_get_token --
 *
 *     from the lex parser, a function which matches one of the
 *     tokens named in include/vc.h.  This is used to determine what
 *     type of output some rcs command has and the interesting information.
 *     if its argument is NULL, it continues matching from the end
 *     of the last match.  text of the token match is left in rcs_text.
 * Results:
 *     one of the token types.
 * Side effects:
 *     the stream is advanced.
 * Parameters:
 *     new stream or NULL.
 */
static YY_BUFFER_STATE buffer = NULL;

int VC_get_token(FILE* is)
{
    yyout = stderr;

    if(is) {
        if(buffer)
            yy_delete_buffer(buffer);
        yyin = is;
        buffer = yy_create_buffer(yyin, YY_BUF_SIZE);
        yy_switch_to_buffer(buffer);
    }

    return yylex();
}

int VC_init_seg_get_c(char* seg, size_t len)
{
    if(buffer)
	yy_delete_buffer(buffer);

    buffer = yy_scan_buffer(seg, len);
    yy_switch_to_buffer(buffer);
}

/* check_token_match --
 *
 *     used in checking that a given string will match a type of token
 *     so that bad tokens are not inserted into the rcs log
 *     information.
 * Results:
 *     return value is 1 if it matches, 0 if it doesn't, -1 if
 *     type is invalid.
 * Side effects:
 *     None.
 * Parameters:
 *     TYPE must be one of "number", which checks that the string
 *     contains only digits, "label" which checks that the string *
 *     satisfies the definition of label in prcs.texi ([A-Za-z0- 9#^\-
 *     *_+=,.]+), and "login", which checks that the number satisfies
 *     ([a- * zA-Z][a-zA-Z0-9\-_]*).  This uses the exact same
 *     patterns * VC_get_token does, so if this fails it means using
 *     that token will * cause output to be misconstrued at some later
 *     date.
 */
int VC_check_token_match(const char* token, const char* type)
{
    YY_BUFFER_STATE buffer;
    int ret, len;
    char* test;
    const char* p;

    yyout = NULL;

    switch (type[2]) {
    case /*lo*/'g'/*in*/:
	test = NEWVEC(char, 32 + strlen(token));
	strcpy(test, ";  author: ");
	strcat(test, token);
	len = strlen(test);
	test[len] = test[len + 1] = YY_END_OF_BUFFER_CHAR;
	buffer = yy_scan_buffer(test, len + 2);
	ret = yylex();
	free(test);
	yy_delete_buffer (buffer);
	if (ret == RlogAuthor && yyleng == len) {
	    return 1;
	} else {
	    return 0;
	}
	break;
    case /*la*/'b'/*el*/:
	test = NEWVEC(char, 32 + strlen(token));
	strcpy(test, "PRCS major version: ");
	strcat(test, token);
	strcat(test, "\n");
	len = strlen(test);
	test[len] = test[len + 1] = YY_END_OF_BUFFER_CHAR;
	buffer = yy_scan_buffer(test, len + 2);
	ret = yylex();
	free(test);
	yy_delete_buffer (buffer);
	if (ret == PrcsMajorVersion && yyleng == len) {
	    return 1;
	} else {
	    return 0;
	}
	break;
    case /*nu*/'m'/*ber*/:
	p = token;

	if(token[0] == '0' && token[1] != '\0') {
	    return 0;
	}

	while(*p != '\0') {
	    if(!isdigit(*p)) {
		return 0;
	    }
	    p += 1;
	}
	return 1;
	break;
    case /*an*/'y':
	return 1;
	break;
    default:
	return -1;
    }
}

/*
 * killnl --
 *
 *     Kills a newlines at the end of patterns.
 * Results:
 *     Actually, it will kill the last character of any string.
 * Side Effects:
 *     Modifies s.
 * Parameters:
 *     Don't let s be "" or NULL.
 */
static char* killnl(char* s)
{
    s[strlen(s) - 1] = '\0';
    return s;
}

static char* stop_at_ws(char* s)
{
    char* s0 = s;
    while(!isspace(*s)) { s += 1; }
    *s = 0;
    return s0;
}

#if 0
int main()
{
    int it;
    while(1) {
	it = VC_get_token(stdin);
	fprintf(stdout, "got token %d\n", it);
    }
}
#endif
