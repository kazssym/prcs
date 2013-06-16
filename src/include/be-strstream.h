/* This is part of PRCS, using the -*- C++ -*- standard library of
 * the Metrowerks compiler to simulate parts of GNU libio/strstream.
 * Copyright (C) 1998 Lars Duening
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
 * MA  02111-1307, USA.
 */

#ifndef __STRSTREAM_H
#define __STRSTREAM_H

#include <sstream.h>
#include <bstring.h>
#include <cstdarg.h>
#include <cstdio.h>
#include "utils.h"

class strstreambuf : public stringbuf {
  private:
    string tmpstr; // holds result of str()
  public:
    int out_waiting() { return pptr() - pbase() - (0 == *pptr() ? 1 : 0); }
    const char * str() {
      tmpstr = stringbuf::str();
      return tmpstr.c_str();
    }
    void freeze(int n=1) 
      { ASSERT(n==0, "strstreambuf::freeze(1) not implemented."); }
};

class ostrstream : public ostringstream {
  private:
    string tmpstr; // holds result of str()
  public:
    const char *str() { tmpstr = rdbuf()->str(); 
    return tmpstr.c_str(); }
    void freeze(int n = 1) 
      { ASSERT(n==0, "ostrstream::freeze(1) not implemented."); }
    void vform(const char * fmt, va_list args)
    {
      const int BUFSIZE = 1024;
      char buf[BUFSIZE];

      buf[BUFSIZE-1] = '\0';
      vsprintf(buf, fmt, args);
      ASSERT('\0' == buf[BUFSIZE-1], "Output buffer bounds exceeded");
      *this << (const char *) buf;
    }
};

#endif /*!__STRSTREAM_H*/
