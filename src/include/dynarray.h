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


#ifndef _DYNARRAY_H_
#define _DYNARRAY_H_

#include <iostream>
#include "config.h"

#define generic template<class T, int DefaultSize, bool ZeroTerm>
#define selftype Dynarray<T, DefaultSize, ZeroTerm>
#define member selftype::

generic class Dynarray {
public:
    Dynarray();
    Dynarray(const T, int N = 1);
    Dynarray(const T*, int N = -1);
    Dynarray(int N);
    Dynarray(const selftype&);
    virtual ~Dynarray();

    /* Non-const methods. */
    void append(const T);
    void append(const T*, int N = -1);
    void append(const selftype&);

    void prepend(const T);
    void prepend(const T*, int N = -1);
    void prepend(const selftype&);

    void assign(const T*, int N = -1);
    void assign(const selftype&);

    void index(int N, const T& val);
    void truncate(unsigned int);
    void expand(int len, const T& val);
    void sort(int (*compare)(const void* a, const void* b));

    /* Const methods. */
    T index(int N) const;
    T last_index() const;
    int length() const;
    operator const T*() const;
    const T* cast() const;

    /* Iterator. */
    class ArrayIterator {
    public:
	ArrayIterator(const selftype* a0) { a = a0; len = a0->length(); i = 0; }
	T operator*() const         { return a->index(i); }
	void next()                 { i += 1; }
	bool finished() const       { return i >= len; }
    private:
	const selftype* a;
	int len, i;
    };

protected:

    virtual void modify() { }
    void operator=(const selftype&); /* force use of assign() */
    void maybe_expand(int len);
    int v_length(const T* p) const;

    mutable T *_vec;
    int _filled;
    mutable int _alloc;
};

#undef member
#undef generic
#undef selftype

#endif
