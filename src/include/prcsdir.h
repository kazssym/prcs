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


#ifndef _FILE_H_
#define _FILE_H_


extern "C" {
#ifdef sgi
#include <sys/dir.h>       /* SGI's BSD directory functions */
#include <sys/dirent.h>
#define dirent direct
#else
#include <dirent.h>        /* the standard dirent includes */
#endif
}

class Dir {
public:
    Dir(const char* path);
    ~Dir();

    bool Dir_open(const char* path);

    /*
     * Returns true if the directory's state is acceptable.
     */
    bool OK() const;
    /*
     * Dir_entry --
     *
     *     Returns the current filename.  Will NOT return "." or "..".
     */
    const char* Dir_entry() const;
    const char* Dir_full_entry();

    /*
     * Dir_next --
     *
     *     Advances the directory to the next entry.  Returns false if
     *     there are none left or an error occurs.  Check OK() for error.
     */
    bool Dir_next();

    /*
     * Dir_more --
     *
     *     returns true if more remains to be read.
     */
    bool Dir_finished() const;

    void Dir_close();

    class DirIterator {
    public:
	DirIterator(Dir& d0);
	const char* operator*() const;
	void next();
	bool finished() const;
    private:
	Dir* dp;
    };

    class FullDirIterator {
    public:
	FullDirIterator(Dir& d0);
	const char* operator*() const;
	void next();
	bool finished() const;
    private:
	Dir* dp;
    };

private:
    bool finished;
    DIR* thisdir;
    dirent* entry;
    int full_name_len;
    Dstring full_name;
};

#endif
