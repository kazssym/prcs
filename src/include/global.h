/* GLOBAL.H - RSAREF types and constants
 */

#ifndef _GLOBAL_H
#define _GLOBAL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* PROTOTYPES should be set to one if and only if the compiler supports
  function argument prototyping.
The following makes PROTOTYPES default to 0 if it has not already
  been defined with C compiler flags.
 */
/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
#if SIZEOF_SHORT == 2
typedef unsigned short UINT2;
#else
#error "can't find a 2-byte type"
#endif

/* UINT4 defines a four byte word */
#if SIZEOF_LONG == 4
typedef unsigned long UINT4;
#elif SIZEOF_INT == 4
typedef unsigned int UINT4;
#else
#error "can't find a 4-byte type"
#endif


/* PROTO_LIST is defined depending on how PROTOTYPES is defined above.
If using PROTOTYPES, then PROTO_LIST returns the list, otherwise it
  returns an empty list.
 */
#define PROTO_LIST(list) list

#endif
