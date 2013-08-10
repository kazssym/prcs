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
 * $Id: memseg.h 1.9.2.4 Sun, 17 Sep 2000 17:56:26 -0700 jmacd $
 */


#ifndef _MEMSEG_H_
#define _MEMSEG_H_

/* determines the minimum buffer size requested via a malloc call,
 * currently set to 16Kb. */
extern const int default_segment_size;

struct Foobie {
    int x;
};

class MemorySegment {
public:

    /* write says whether to open the fd O_RDWR or just O_RDONLY */
    MemorySegment(bool force_read, bool write = false);

    /* closes and frees resources */
    ~MemorySegment();

    /* Map file will either open a file and map it or, if the file is already
     * open check to see if the file is regular and use mmap to map the file.
     * If the file is irregular and length is -1, default_segment_size will
     * be used to guess the initial size and doubled until the file is read.
     * If length is specified, it is assumed to be the length of the file and
     * only that much will be read.  It will not report an error if there is
     * more to be read.  If map_file reads the file, it will take care of
     * closing the descriptor, otherwise, it will be left open.  a call to
     * map_file will automatically take care of closing any previously open
     * files. */
    PrVoidError map_file(const char* file);
    PrVoidError map_file(const char* desc, int fd, int length = -1);

    const char* segment() const;
    char* wr_segment()    const;

    /* For writing to a segment */

    void clear_segment();
    void append_segment(const char* s, int len);

    /* returns the current segment length.  If the file was completely mapped,
     * it will return the file length.  Otherwise, it returns the current
     * segment length, which will be less than or equal to the segment size. */
    int length() const;

    int fd() const;

    PrVoidError unmap();

private:
#if HAVE_MMAP
    PrVoidError init_mapped_segment();
#endif
    PrVoidError init_memory_segment();

    void expand_memory_segment(int length);

    int  _fd;
    bool _own_fd; /* whether responsible for closing this fd. */
    bool _open_write; /* is fd writable */
    bool _force_read;

    char* _segment; /* Universal segment. */
    int _length;  /* Length of valid segment. */

    int _allocated; /* length of _memory_segment */
    char* _memory_segment;   /* malloced segment w/ _length worth of valid data */
#if HAVE_MMAP
    caddr_t _mapped_segment; /* mmaped segment of length _length */
#endif
    const char* _desc;
};

#endif
