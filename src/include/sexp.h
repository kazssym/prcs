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


#ifndef _SEXP_H_
#define _SEXP_H_

#define _key _u._u_key
#define _pair _u._u_pair

#define foreach_sexp(s, s0) foreach(s, s0, Sexp::SexpIterator)

class Sexp {
public:

    Sexp  (Estring *key, int line = -1);
    Sexp  (const Sexp *s1, const Sexp *s2, int line = -1);
    Sexp  (int line = -1); /* empty */
    ~Sexp ();

    Estring* key()      const;
    ListMarker marker() const;

    bool is_pair()  const;
    bool is_empty() const;

    const Sexp* car()     const;
    const Sexp* cdr()     const;
    const Sexp* cadr()    const;
    const Sexp* caddr()   const;
    const Sexp* cadddr()  const;
    const Sexp* caar()    const;

    int line_number() const;
    int length()      const;

    void set_end(size_t end);
    void set_start(size_t start);

    size_t start_index() const;
    size_t end_index()   const;

    class SexpIterator {
    public:
	SexpIterator(const Sexp* s0);
	const Sexp* operator*() const;
	void next();
	bool finished() const;
    private:
	const Sexp* s;
    };

private:

    union {
	Estring* _u_key;
	struct { size_t _start; size_t _end; const Sexp *_car; const Sexp* _cdr; } _u_pair;
    } _u;

    int  _line;
    bool _is_pair;
    bool _is_empty;
};


#endif
