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


#include <stdarg.h>
#include "config.h"
#ifdef HAVE_STD_H
#include <std.h>
#endif
#include "dynarray.h"
#include "dstring.h"
#include "utils.h"

#define generic template<class T, int DefaultSize, bool ZeroTerm>
#define selftype Dynarray<T, DefaultSize, ZeroTerm>
#define member selftype::

#define MAYBECNEWVEC(zeroit, type, elts) ((zeroit) ? NEWVEC0(type, elts) : NEWVEC(type, elts))
#define MAYBECEXPANDVEC(zeroit, T, V, N, I) (T*)((zeroit) ? EXPANDVEC0(V, N, I) : EXPANDVEC(V, N, I))

static int nearest_pow (int x) { int i = 1; while (i <= x) i <<= 1; return i < 16 ? 16 : i; }

generic int member v_length(const T* p) const
{
    int l = 0;
    while (*p++ != 0) l += 1;
    return l;
}

generic member Dynarray()
    :_vec(NULL), _filled(0), _alloc(0)
{}

generic member Dynarray(const T *s, int N0)
{
    ASSERT(N0 < 0 || ZeroTerm, "Can't end a non-zero-terminated vector");

    if (N0 < 0)
	N0 = v_length(s);

    int i = nearest_pow (N0 + ZeroTerm);

    _vec = MAYBECNEWVEC(ZeroTerm, T, i);
    _filled = N0;
    _alloc = i;

    for(i = 0; i < N0; i += 1)
	_vec[i] = s[i];
}

generic member Dynarray(int N0)
{
    int i = nearest_pow(N0 + ZeroTerm);

    _vec = MAYBECNEWVEC(ZeroTerm, T, i);
    _filled = 0;
    _alloc = i;
}

generic member Dynarray(const T c, int N0)
{
    int i = nearest_pow(N0 + ZeroTerm);

    _vec = MAYBECNEWVEC(ZeroTerm, T, i);
    _filled = N0;
    _alloc = i;

    for(i = 0; i < N0; i += 1)
	_vec[i] = c;
}

generic member Dynarray(const selftype& s)
{
    _vec = MAYBECNEWVEC(ZeroTerm, T, s._alloc);
    _filled = s.length();
    _alloc = s._alloc;

    for(int i = 0; i < s.length(); i += 1)
	_vec[i] = s.index(i);
}

generic member ~Dynarray()
{
    if (_alloc > 0)
	free(_vec);
}

generic void member maybe_expand(int elts)
{
    modify();

    if (!_vec) {

	ASSERT (_filled == 0, "otherwise an error");

	_alloc = nearest_pow (elts + ZeroTerm);
	_vec = MAYBECNEWVEC(ZeroTerm, T, _alloc);

    } else if (_filled + elts > _alloc - ZeroTerm) {
	int goalsize = nearest_pow (_filled + elts + ZeroTerm);

	_vec = MAYBECEXPANDVEC(ZeroTerm, T, _vec, _alloc, goalsize - _alloc);
	_alloc = goalsize;
    }
}

generic void member expand(int elts, const T& val)
{
    modify();

    if(_filled < elts) {

	maybe_expand(elts - _filled);

	while(_filled < elts) {
	    _vec[_filled] = val;
	    _filled += 1;
	}
    }
}

generic void member append(const T c)
{
    modify();

    maybe_expand(1);
    _vec[_filled] = c;
    _filled += 1;
}

generic void member append(const T *s, int N0)
{
    modify();

    ASSERT(s != NULL, "Tried to dereference null pointer");
    ASSERT(N0 < 0 || ZeroTerm, "Can't end a non-zero-terminated vector");

    if (N0 < 0)
	N0 = v_length (s);

    maybe_expand (N0);

    memcpy (_vec + _filled, s, sizeof(T) * N0);

    _filled += N0;
}

generic void member append(const selftype& s)
{
    modify();

    maybe_expand (s.length());

    memcpy (_vec + _filled, s._vec, s.length() * sizeof(T));

    _filled += s.length();
}

generic void member prepend(const T c)
{
    modify();
    maybe_expand(1);
    memmove(_vec + 1, _vec, sizeof(c) * _filled);
    _vec[0] = c;
    _filled += 1;
}

generic void member truncate(unsigned int N)
{
    modify();

    _filled = N;

    if(ZeroTerm) {
	memset(_vec + N, 0, sizeof(T) * (_alloc - N));
    }
}

generic void member index(int N, const T& elt)
{
    modify();

    ASSERT(N < _filled, "indexed past end of array");

    _vec[N] = elt;
}

generic void member assign(const T* s, int N)
{
    modify();

    if (N < 0)
	N = v_length(s);

    if (N > _filled)
	maybe_expand(N - _filled);

    memmove(_vec, s, N * sizeof(*s));

    if (ZeroTerm) {
	int old_filled = _filled;
	_filled = N;
	if (_filled < old_filled)
	    memset(_vec + _filled, 0, sizeof(*s) * (old_filled - _filled));
    } else {
	_filled = N;
    }
}

generic void member assign(const selftype& s)
{
    assign(s.cast(), s.length());
}

generic void member sort(int (*compare)(const void*, const void*))
{
    modify();

    qsort(_vec, _filled, sizeof(T), compare);
}

/* Const methods.
 */

generic member operator const T*() const
{
    return cast();
}

generic const T* member cast() const
{
    ASSERT(this != NULL, "no null objects please");                                                                                                                               
                                                                                                                                                                                  
    if (_vec) {                                                                                                                                                                   
        if(ZeroTerm)                                                                                                                                                              
            ASSERT(_vec[_filled] == 0, "not zero-terminated");                                                                                                                    
                                                                                                                                                                                  
        return _vec;                                                                                                                                                              
    } else {                                                                                                                                                                      
        /*ASSERT (_filled == 0 && ZeroTerm, "what?");*/                                                                                                                           
                                                                                                                                                                                  
        _alloc = 16;                                                                                                                                                              
        _vec = MAYBECNEWVEC(ZeroTerm, T, 16);                                                                                                                                     
                                                                                                                                                                                  
        return _vec;                                                                                                                                                              
    }                                                                                                                                                                             
}

generic int member length() const
{
    return _filled;
}

generic T member index(int N) const
{
    ASSERT(N < (_filled + ZeroTerm), "indexed past end of array");

    if (_vec)
	return _vec[N];
    else {
	ASSERT (_filled == 0, "what?");

	return 0;
    }
}

generic T member last_index() const
{
    return index(length() - 1);
}

#if defined(__MWERKS__) && defined(EXPLICIT_TEMPLATES)
/* Some methods in Dynarray are not defined, propably because
 * they haven't been used yet. However, to instantiate them
 * there must be something.
 */

generic void member prepend(const T * , int N)
{
  ASSERT(false, "Undefined method Dynarray::prepend(const T *, int) called");
}

generic void member prepend(const selftype &)
{
  ASSERT(false, "Undefined method Dynarray::prepend(const Dynarray&) called");
}

generic void member operator=(const selftype &)
{
  ASSERT(false, "Undefined method Dynarray::operator=(const Dynarray&) called");
}

#endif

#undef member
#undef generic
#undef selftype

#if EXPLICIT_TEMPLATES
#include "dynarray.tl"
#endif
