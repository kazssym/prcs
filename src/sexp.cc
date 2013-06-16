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
 * $Id: sexp.cc 1.2.1.5.3.5 Wed, 22 Jan 1997 12:02:00 -0800 jmacd $
 */

#include "prcs.h"
#include "projdesc.h"
#include "sexp.h"
#include "misc.h"

int Sexp::length() const
{
    int l = 0;

    foreach_sexp (s, this)
	l += 1;

    return l;
}

Sexp::Sexp (Estring *key, int line)
{
    _key = key;
    _line = line;
    _is_empty = false;
    _is_pair = false;
}

Sexp::Sexp (const Sexp *s1, const Sexp *s2, int line)
{
    _pair._car = s1;
    _pair._cdr = s2;
    _pair._start = 0;
    _pair._end = 0;
    _line = line;
    _is_pair = true;
    _is_empty = false;
}

Sexp::Sexp (int line)
{
    _pair._car = NULL;
    _pair._cdr = NULL;
    _pair._start = 0;
    _pair._end = 0;
    _line = line;
    _is_pair = true;
    _is_empty = true;
}

Sexp::~Sexp()
{
    if (!is_empty() && is_pair()) {
	if (car() && !car()->is_empty()) delete car();
	if (cdr() && !cdr()->is_empty()) delete cdr();
    }
}

Estring* Sexp::key()      const { return _key; }
ListMarker Sexp::marker() const { return ListMarker(_pair._start, _pair._end); }

bool Sexp::is_pair()  const { return _is_pair || _is_empty; }
bool Sexp::is_empty() const { return _is_empty; }

const Sexp* Sexp::car()     const { return _pair._car; }
const Sexp* Sexp::cdr()     const { return _pair._cdr; }
const Sexp* Sexp::cadr()    const { return cdr()->car(); }
const Sexp* Sexp::caddr()   const { return cdr()->cdr()->car(); }
const Sexp* Sexp::cadddr()  const { return cdr()->cdr()->cdr()->car(); }
const Sexp* Sexp::caar()    const { return car()->car(); }

int Sexp::line_number() const { return _line; }

void Sexp::set_end(size_t end)     { _pair._end = end; }
void Sexp::set_start(size_t start) { _pair._start = start; }

size_t Sexp::start_index() const { return _pair._start; }
size_t Sexp::end_index()   const { return _pair._end; }

Sexp::SexpIterator::SexpIterator(const Sexp* s0)  { s = s0; }
const Sexp* Sexp::SexpIterator::operator*() const { return s->car(); }
void Sexp::SexpIterator::next()                   { s = s->cdr(); }
bool Sexp::SexpIterator::finished() const         { return s->is_empty(); }
