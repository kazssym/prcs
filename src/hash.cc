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


#include "hash.h"
#include "config.h"
#include <string.h>

class FileEntry;
class PrcsAttrs;

template <class T>
List<T>* List<T>::nreverse()
{
    List<T> *point, *prev, *next;

    if(tail() == NULL)
	return this;

    prev = this;
    point = tail();
    next = tail()->tail();

    while(true){

	point->tail() = prev;

	if ( next == NULL )
	{
	    this->tail() = next;
	    return point;
	}
	prev = point;
	point = next;
	next = next->tail();
    }
}

static const int primes[] = {
    23, 47, 101, 211, 431, 863, 1733, 3467, 6949, 13901, 27803, 55609,
    11239, 22481, 44963, 89939, 199999, 400009, 800029, 1600061,
    16000627, 160006271};

const int NUM_PRIMES = sizeof(primes) / sizeof(primes[0]);

bool equal(const unsigned long long& x, const unsigned long long& y) {
    return x == y;
}

bool equal(const long long& x, const long long& y) {
    return x == y;
}

bool equal(const unsigned long& x, const unsigned long& y) {
    return x == y;
}

bool equal(const unsigned int& x, const unsigned int& y) {
    return x == y;
}

bool equal(const int& x, const int& y) {
    return x == y;
}

bool equal(const char*const& x, const char*const& y) {
    return strcmp(x, y) == 0;
}

bool equal(FileEntry*const& x, FileEntry*const& y) {
    return x == y;
}

extern bool attrs_equal(const PrcsAttrs*const& x, const PrcsAttrs*const& y);
extern int attrs_hash(const PrcsAttrs*const& s, int M);

bool equal(const PrcsAttrs*const& x, const PrcsAttrs*const& y) {
    return attrs_equal (x, y);
}

int hash(const PrcsAttrs*const& x, int M)
{
    return attrs_hash (x, M);
}

int hash(const char *const & s, int M)
    /* a char* hash function from Aho, Sethi, and Ullman */
{
    const char *p;
    unsigned int h(0), g;
    for(p = s; *p != '\0'; p += 1) {
	h = ( h << 4 ) + *p;
	if ( ( g = h & 0xf0000000 ) ) {
	    h = h ^ (g >> 24);
	    h = h ^ g;
	}
    }

    return h % M;
}

int hash(const int& x, int M)
{
    return (unsigned int)x % M;
}

static int hash(const long long& x, int M)
{
    return (long long)x % M;
}

static int hash(const unsigned long long& x, int M)
{
    return (unsigned long long)x % M;
}

int hash(const unsigned long& x, int M)
{
    return (unsigned long)x % M;
}

int hash(const unsigned int& x, int M)
{
    return (unsigned int)x % M;
}

int hash(FileEntry*const& x, int M)
{
  return (unsigned int)x % M;
}

#define generic template<class Key, class Data>
#define selftype HashTable<Key, Data>
#define member selftype::

generic void member init(int p)
    /* Used to initialize a HashTable to be empty and with primes[p] */
    /* buckets. */
{
    N = 0;
    prime = p;
    M = primes[p];
    L = new ItemList*[primes[p]];
    for (int i = 0; i < M; i += 1)
	L[i] = NULL;
}

generic member HashTable(int (*func0)(const Key&, int),
			 bool (*equal0)(const Key&, const Key&))
    /* An empty HashTable. */
{
    if(func0 == NULL) {
	func = hash;
    } else {
	func = func0;
    }
    if(equal0 == NULL) {
	equalfunc = equal;
    } else {
	equalfunc = equal0;
    }
    init(0);
}

generic member ~HashTable()
{
    for (int i = 0; i < M; i += 1)
	for (i=0; i < M; i += 1)
	    if (L[i] != NULL)
		L[i]->deleteList();
    delete [] L;
}

generic Data* member find(const Key& x, int hashValue) const
    /* Internal routine to find a Data item on L[hashValue]. */
{
    ItemList* p = L[hashValue];
    while (p != NULL) {
	if(equalfunc(x, p->head().x()))
	    break;
	p = p->tail();
    }

    if (p == NULL)
	return NULL;
    else
	return & p->head().y();
}

generic Data* member lookup(const Key& x) const
{
    return find(x, func(x, M));
}

generic void member remove(const Key& x)
{
    int hashValue = func (x, M);

    ItemList* p = L[hashValue];

    if (equalfunc (x, p->head().x())) {
      L[hashValue] = p->tail();

      delete p;

      return;
    }

    for (; p && p->tail(); p = p->tail()) {
	if(equalfunc (x, p->tail()->head().x())) {
	    ItemList* tmp = p->tail();

	    p->tail() = p->tail()->tail();

	    delete tmp;

	    return;
	}
    }
}

generic Pair<Key, Data>* member lookup_pair(const Key& x) const
{
    int hashValue = func (x, M);

    ItemList* p = L[hashValue];

    while (p != NULL) {
	if(equalfunc(x, p->head().x()))
	    break;
	p = p->tail();
    }

    if (p == NULL)
	return NULL;
    else
	return &p->head();
}

generic Data* member insert(const Key& x, const Data& init)
    /* A pointer to the Data object currently hashed to by x. If there */
    /* currently is none, adds an entry from x to a new Data object */
    /* initialized  with init, and returns a pointer to that new object. */
{
    int h = func(x, M);
    Data *dp = find(x, h);
    if (dp == NULL) {
	L[h] = new ItemList(HashItem(x, init), L[h]);
	N += 1;
	if (2*M < N && prime+1 < NUM_PRIMES) {
	    ItemList** bucketPointer(L);
	    ItemList* listPointer;
	    int newM = primes[prime + 1], j;
	    ItemList** tmp = new ItemList*[newM];
	    for ( j = 0; j < newM ; j += 1 )
		tmp[j] = NULL;
	    while ( bucketPointer < L + M ) {
		listPointer = *bucketPointer;
		while (listPointer != NULL) {
		    int h = func(listPointer->head().x(), newM);
		    tmp[h] = new ItemList(HashItem(listPointer->head().x(),
						   listPointer->head().y()),
					  tmp[h]);
		    listPointer = listPointer->tail();
		}
		bucketPointer += 1;
	    }
 	    for ( j=0; j < M; j += 1 )
 		if (L[j] != NULL)
 		    L[j]->deleteList();
 	    delete [] L;
	    L = tmp;
	    M = newM;
	    prime += 1;
	    return lookup(x);
	}
	else
	    return & L[h]->head().y();
    } else
	*dp = init;

    return dp;
}

#undef member
#undef selftype
#undef generic


#define generic template<class Key, class Data>
#define selftype OrderedTable<Key, Data>
#define member selftype::

generic member ~OrderedTable ()
{
    delete _key_array;
    delete _data_array;
    delete _table;
}

generic member OrderedTable ()
{
    _key_array = new KeyArray;
    _data_array = new DataArray;
    _table = new Table;
}

generic Data* member insert(const Key& x, const Data& init)
{
    remove (x);

    _key_array->append (x);
    _data_array->append (init);

    return _table->insert (x, init);
}

generic Data* member lookup(const Key& x) const
{
    return table()->lookup (x);
}

generic void member remove(const Key& x)
{
    if (lookup (x)) {

	for (int i = 0; i < _key_array->length(); i += 1) {
	    if (equal (x, _key_array->index(i))) {

		for (int j = i; j < _key_array->length()-1; j += 1) {
		    _key_array->index(j, _key_array->index(j+1));
		    _data_array->index(j, _data_array->index(j+1));
		}

		_key_array->truncate (_key_array->length() - 1);
		_data_array->truncate (_data_array->length() - 1);

		break;
	    }
	}

	_table->remove (x);
    }
}

generic HashTable<Key, Data>* member table() const { return _table; }
generic Dynarray<Key, 64, false>* member key_array() const { return _key_array; }
generic Dynarray<Data, 64, false>* member data_array() const { return _data_array; }

#undef member
#undef selftype
#undef generic

#if EXPLICIT_TEMPLATES
#include "hash.tl"
#endif
