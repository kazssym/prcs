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

%option nounput
%option noinput
%option nostdinit
%option noyywrap
%option never-interactive
%option nostack
%option noreject
%option noyylineno
%option outfile="lex.yy.c"
%option prefix="prj"

%{
#include "utils.h"
#include "projdesc.h"

int prj_lex_this_index;
int prj_lex_cur_index;
int prj_lineno;

void vc_lex_fatal_error(const char* msg);

static inline void
count_lines()
{
    const char* s = prjtext;
    const char* se = prjtext + prjleng;

    for (; s < se; s += 1)
	if (*s == '\n')
	    prj_lineno += 1;
}

#define YY_FATAL_ERROR(msg) vc_lex_fatal_error(msg)
#define YY_USER_ACTION prj_lex_this_index = prj_lex_cur_index; \
                       prj_lex_cur_index += prjleng;
#define YY_INPUT(buf,result,max_size) result = prjinput(buf,max_size);
%}

space   [\v\f\r\t\n ]
comment ";"[^\0\n]*
ws      ({space}|{comment})+
escape  \\([^\0])
string  \"({escape}|[^\0\\\"])*\"
bstring \"({escape}|[^\0\\\"])*
open    \(
close   \)
name    ({escape}|[^\0\r\v\f\n\t \\\"\(\)])+

%%

\0        { return PrjNull; }
{ws}      { count_lines(); }
{bstring} { count_lines(); return PrjBadString; }
{name}    { count_lines(); return PrjName; }
{string}  { count_lines(); return PrjString; }
{open}    { return PrjOpen; }
{close}   { return PrjClose; }
<<EOF>>   { return PrjEof; }

%%
