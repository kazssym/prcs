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


#ifndef _SETKEYS_H_
#define _SETKEYS_H_

/*
 * setkeys, setkeys_infile, setkeys_inputbuf --
 *
 *     these three commands differ only in where they get their input.
 *     setkeys takes input from a named file.  setkeys_infile takes
 *     input from a FILE* already opened for reading.  setkeys_inputbuf
 *     takes the address and length of a memory segment containing the
 *     file to be compared.  the first integer argument is to be one of
 *     the symbolic constants SETKEYS or UNSETKEYS.  the second integer
 *     argument is to be one of NOSUPPRESS, SUPPRESSIFNOCHANGE, and
 *     SUPPRESSUNCONDITIONALLY.
 *
 *     The exit code is 0 if the output file does not (or would not)
 *     differ from the input, 1 if the output file does (would) differ,
 *     and 2 in case of trouble.
 */

PrBoolError setkeys(const char* fromfile,
		    const char* tofile,
		    FileEntry *fe,
		    SetkeysAction action);
PrBoolError setkeys_infile(const char* desc,
			   int fd,
			   int length,
			   const char* tofile,
			   FileEntry *fe,
			   SetkeysAction action);
PrBoolError setkeys_inputbuf(const char* inputbuf,
			     int inputbuflen,
			     ostream* os,
			     FileEntry *fe,
			     SetkeysAction action);
PrBoolError setkeys_inoutbuf(const char* inputbuf,
			     int inputbuflen,
			     MemorySegment *seg,
			     FileEntry *fe,
			     SetkeysAction action);
PrBoolError setkeys_outbuf(const char* file,
			   MemorySegment *seg,
			   FileEntry *fe,
			   SetkeysAction action);

#endif
