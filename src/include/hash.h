/* -*-Mode: C++;-*-
 * PRCS - The Project Revision Control System
 * Copyright (C) 1997  Josh MacDonald
 * Copyright (C) 1995  P. N. Hilfinger
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


#ifndef _HASH_H_
#define _HASH_H_

#include <stdlib.h>
#include "dynarray.h"

template<class X, class Y>
class Pair {
public:
    Pair(const X& x0, const Y& y0)
	:x1(x0), y1(y0) { }

    const X& x() const { return x1; }
    X& x() { return x1; }
    const Y& y() const { return y1; }
    Y& y() { return y1; }

private:
    X x1;
    Y y1;
};

template<class T>
class List {
public:
    List(const T& h0, List<T>* t0)
	:h(h0), t(t0) { }

    const T& head() const { return h; }
    T& head() { return h; }
    List<T>* tail() const { return t; }
    List<T>*& tail() { return t; }
    void deleteList() { if ( t != NULL ) t->deleteList();
			delete this; }
    List<T>* nreverse();

private:

    T h;
    List<T>* t;
};

#define generic template<class Key, class Data>
#define selftype HashTable<Key, Data>

generic
class HashTable {
public:
    typedef Pair< Key, Data > HashItem;
    typedef List< HashItem > ItemList;

    HashTable(int (*func0)(const Key&, int) = NULL,
	      bool (*equal0)(const Key&, const Key&) = NULL);
    ~HashTable();

    void remove(const Key& x);
    Data* lookup(const Key& x) const;
    HashItem* lookup_pair(const Key& x) const;
    Data* insert(const Key& x, const Data& init);
    bool isdefined(const Key& x) const { return lookup(x) != NULL; }
    int M; /* Number of buckets. */

    class HashIterator {
    public:
	HashIterator(selftype* h0)  { h = h0; list_index = -1; list_head = NULL; next(); }
	HashItem& operator*() const { return list_head->head(); }
	void next() {
	    do {
		if(list_head) {
		    list_head = list_head->tail();
		} else {
		    list_index += 1;
		    if(list_index >= h->M)
			return;
		    list_head = h->L[list_index];
		}
	    } while(!list_head);
	}
	bool finished() const { return list_index >= h->M; }
    private:
	selftype* h;
	int list_index;
	ItemList* list_head;
    };

    friend class HashIterator;

private:

    ItemList** L; /* array of buckets */
    int N; /* The number of items in the hash table.  */
    int prime; /* Index into a table of prime sizes. */

    Data* find(const Key& x, int hashValue) const;
    /* Internal routine to find a Data item on L[hashValue]. */

    void init(int p);
    /* Used to initialize a HashTable to be empty and with primes[p] */
    /* buckets. */

    int (*func)(const Key&, int);
    bool (*equalfunc)(const Key&, const Key&);
};

#undef selftype
#undef generic

#define generic template<class Key, class Data>
#define selftype OrderedTable<Key, Data>

generic
class OrderedTable {
public:
    OrderedTable ();
    ~OrderedTable ();

    typedef Dynarray <Key, 64, false> KeyArray;
    typedef Dynarray <Data, 64, false> DataArray;
    typedef HashTable <Key, Data> Table;

    Data* insert(const Key& x, const Data& init);
    Data* lookup(const Key& x) const;
    void remove(const Key& x);

    Table* table() const;
    KeyArray* key_array() const;
    DataArray* data_array() const;

private:
    Table* _table;
    KeyArray* _key_array;
    DataArray* _data_array;
};

#undef selftype
#undef generic

#endif
