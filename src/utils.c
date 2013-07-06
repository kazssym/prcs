/* -*-Mode: C;-*-
 * PRCS - The Project Revision Control System
 * Copyright (C) 1994  P. N. Hilfinger
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
 * $Id: utils.c 1.6.1.1 Sun, 05 Jan 1997 02:59:04 -0800 jmacd $
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"

void *_malloc_(size_t size)
    /* As for malloc, but screams if storage exhausted. */
{
    char *space = malloc(size);

    if (space == NULL) {
	(void) fputs("panic: storage exhausted.\n", stderr);
	exit(2);
    }

    return (void *) space;
}

void *_mrealloc_(void *V, int size, int N, int INCR)
    /* Assuming V points to a heap-allocated vector of size N*size if	*/
    /* N>0, and INCR > 0, return a pointer (possibly == V) to an area	*/
    /* of storage containing the same data, and an additional		*/
    /* size*INCR bytes at the end.  If N is 0, then acts like		*/
    /* _malloc_, and ignores the value of V.				*/
{
    if (N == 0)
	V = malloc(size * INCR);
    else
	V = realloc(V, size * (N+INCR));
    if (V == NULL) {
	(void) fputs("panic: storage exhausted.\n", stderr);
	exit(2);
    }
    return V;
}

void *_malloc0_(size_t size)
    /* As for _malloc_, but zeroes storage. */
{
    char *space = calloc(size, 1);
    if (space == NULL) {
	(void) fputs("panic: storage exhausted.\n", stderr);
	exit(2);
    }
    return (void *) space;
}

void *_mrealloc0_(void *V, int size, int N, int INCR)
     /* As for _mrealloc_, but zeroes added storage. */
{
    if (N == 0)
	V = malloc(size * INCR);
    else
	V = realloc(V, size * (N+INCR));
    if (V == NULL) {
	(void) fputs("panic: storage exhausted.\n", stderr);
	exit(2);
    }
    memset((char *) V + N * size, 0, INCR * size);
    return V;
}
