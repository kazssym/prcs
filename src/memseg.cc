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

extern "C" {
#include <fcntl.h>
}

#include "prcs.h"

extern "C" {
#include <unistd.h>
#if HAVE_MMAP
#include <sys/mman.h>
#endif
}

#include "memseg.h"
#include "system.h"

const int default_segment_size = 1 << 14;

PrVoidError MemorySegment::unmap()
{
    bool close_ok = true;

#if HAVE_MMAP
    if(_mapped_segment)
	munmap(_segment, _length);
#endif

    if(_own_fd && _fd >= 0) {
	if(close(_fd) < 0)
	    close_ok = false;
    }

    _segment = 0;
    _length = -1;
#if HAVE_MMAP
    _mapped_segment = 0;
#endif

    /* keep the _memory_segment and _allocated for the next use */

    _fd = -1;
    _own_fd = false;

    if(close_ok) {
	return NoError;
    } else {
	pthrow prcserror << "Close " << squote (_desc) << " failed: " << perror;
    }
}

MemorySegment::~MemorySegment()
{
    unmap();

    if(_memory_segment)
	free(_memory_segment);
}

void MemorySegment::clear_segment()
{
    unmap();

    _length = 0;
}

void MemorySegment::append_segment(const char* s, int len)
{
    expand_memory_segment(len + _length);

    memcpy(_memory_segment + _length, s, len);

    _segment = _memory_segment;

    _length += len;
}

PrVoidError MemorySegment::map_file(const char* filename)
{
    struct stat statbuf;
    int open_flags;

    _desc = filename;

    unmap();

    if(_open_write) {
	open_flags = O_RDWR;
    } else {
	open_flags = O_RDONLY;
    }

    _own_fd = true;

    If_fail(_fd << Err_open(filename, open_flags))
	pthrow prcserror << "Open " << squote (_desc) << " failed" << perror;

    If_fail(Err_fstat(_fd, &statbuf))
	pthrow prcserror << "Stat " << squote (_desc) << " failed" << perror;

    _length = statbuf.st_size;

#if HAVE_MMAP
    return init_mapped_segment();
#else
    return init_memory_segment();
#endif

}

PrVoidError MemorySegment::map_file(const char* desc, int fd0, int length0)
{
    struct stat statbuf;

    _desc = desc;

    unmap();

    _fd = fd0;

    If_fail(Err_fstat(_fd, &statbuf))
	pthrow prcserror << "Fstat " << squote (_desc) << " failed" << perror;

    if(S_ISREG(statbuf.st_mode) && !_force_read) {
	_length = statbuf.st_size;
#if HAVE_MMAP
	return init_mapped_segment();
#else
	return init_memory_segment();
#endif
    } else {
	if(length0 >= 0)
	    _length = length0;
	return init_memory_segment();
    }
}

#if HAVE_MMAP
PrVoidError MemorySegment::init_mapped_segment()
{
    if((_mapped_segment = (caddr_t)mmap(0, _length, PROT_READ, MAP_PRIVATE, _fd, 0)) < 0)
	pthrow prcserror << "Mmap " << squote (_desc) << " failed" << perror;

    _segment = _mapped_segment;

    return NoError;
}
#endif

PrVoidError MemorySegment::init_memory_segment()
{
    if(_length > 0) {
	int n_read, total_read = 0;
	char* read_ptr;

	expand_memory_segment(_length);

	read_ptr = _memory_segment;

	while(total_read < _length) {
	    If_fail(n_read << Err_read(_fd, read_ptr, _length - total_read))
		pthrow prcserror << "Read " << squote (_desc) << " failed" << perror;

	    if (n_read == 0) {
		pthrow prcserror << "File " << squote (_desc) << " too short: expected " << _length << "; recieved " << total_read << prcsendl;
	    }

	    total_read += n_read;
	    read_ptr += n_read;
	}

	_segment = _memory_segment;

	return NoError;
    } else {
	expand_memory_segment(default_segment_size);

	_length = 0;

	while(true) {
	    while(_length < _allocated) {
		int n_read;

		If_fail(n_read << Err_read(_fd, _memory_segment + _length, _allocated - _length))
		    pthrow prcserror << "Read " << squote (_desc) << " failed" << perror;

		_length += n_read;

		if (n_read == 0) {
		    _segment = _memory_segment;
		    return NoError;
		}
	    }

	    _memory_segment = (char*)EXPANDVEC(_memory_segment, _allocated, _allocated);
	    _allocated *= 2;
	}
    }
}

void MemorySegment::expand_memory_segment(int new_length)
{
    if(new_length < _allocated)
	return;

    if(!_memory_segment) {
	_allocated = default_segment_size;
	while(_allocated < new_length)
	    _allocated <<= 1;
	_memory_segment = NEWVEC(char, _allocated);
    } else {
	int old_size = _allocated;

	while(_allocated < new_length)
	    _allocated <<= 1;

	_memory_segment = (char*)EXPANDVEC(_memory_segment, old_size, _allocated - old_size);
    }

    _segment = _memory_segment;
}

MemorySegment::MemorySegment(bool force_read, bool write)
    :_fd(-1), _own_fd(false), _open_write(write),
     _force_read(force_read), _segment(0), _length(0),
     _allocated(0), _memory_segment(0)
#if HAVE_MMAP
    , _mapped_segment(0)
#endif
{ }

const char* MemorySegment::segment() const { return _segment; }
char* MemorySegment::wr_segment()    const { return _segment; }
int MemorySegment::length() const { return _length; }
int MemorySegment::fd() const { return _fd; }
