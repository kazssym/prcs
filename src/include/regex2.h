/* Definitions for data structures and routines for the reg2ular
   expression library, version 0.12.

   Copyright (C) 1985, 1989, 1990, 1991, 1992, 1993 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef __REG2EX2P_LIBRARY_H__
#define __REG2EX2P_LIBRARY_H__

/* POSIX says that <sys/types.h> must be included (by the caller) before
   <reg2ex2.h>.  */

#ifdef VMS
/* VMS doesn't have `size_t' in <sys/types.h>, even though POSIX says it
   should be there.  */
#include <stddef.h>
#endif


/* The following bits are used to determine the reg2ex2p syntax we
   recognize.  The set/not-set meanings are chosen so that Emacs syntax
   remains the value 0.  The bits are given in alphabetical order, and
   the definitions shifted by one from the previous bit; thus, when we
   add or remove a bit, only one other definition need change.  */
typedef unsigned reg2_syntax_t;

/* If this bit is not set, then \ inside a bracket expression is literal.
   If set, then such a \ quotes the following character.  */
#define RE2_BACKSLASH_ESCAPE_IN_LISTS (1)

/* If this bit is not set, then + and ? are operators, and \+ and \? are
     literals.
   If set, then \+ and \? are operators and + and ? are literals.  */
#define RE2_BK_PLUS_QM (RE2_BACKSLASH_ESCAPE_IN_LISTS << 1)

/* If this bit is set, then character classes are supported.  They are:
     [:alpha:], [:upper:], [:lower:],  [:digit:], [:alnum:], [:xdigit:],
     [:space:], [:print:], [:punct:], [:graph:], and [:cntrl:].
   If not set, then character classes are not supported.  */
#define RE2_CHAR_CLASSES (RE2_BK_PLUS_QM << 1)

/* If this bit is set, then ^ and $ are always anchors (outside bracket
     expressions, of course).
   If this bit is not set, then it depends:
        ^  is an anchor if it is at the beginning of a reg2ular
           expression or after an open-group or an alternation operator;
        $  is an anchor if it is at the end of a reg2ular expression, or
           before a close-group or an alternation operator.

   This bit could be (re)combined with RE2_CONTEXT_INDEP_OPS, because
   POSIX draft 11.2 says that * etc. in leading positions is undefined.
   We already implemented a previous draft which made those constructs
   invalid, though, so we haven't changed the code back.  */
#define RE2_CONTEXT_INDEP_ANCHORS (RE2_CHAR_CLASSES << 1)

/* If this bit is set, then special characters are always special
     reg2ardless of where they are in the pattern.
   If this bit is not set, then special characters are special only in
     some contexts; otherwise they are ordinary.  Specifically,
     * + ? and intervals are only special when not after the beginning,
     open-group, or alternation operator.  */
#define RE2_CONTEXT_INDEP_OPS (RE2_CONTEXT_INDEP_ANCHORS << 1)

/* If this bit is set, then *, +, ?, and { cannot be first in an re or
     immediately after an alternation or begin-group operator.  */
#define RE2_CONTEXT_INVALID_OPS (RE2_CONTEXT_INDEP_OPS << 1)

/* If this bit is set, then . matches newline.
   If not set, then it doesn't.  */
#define RE2_DOT_NEWLINE (RE2_CONTEXT_INVALID_OPS << 1)

/* If this bit is set, then . doesn't match NUL.
   If not set, then it does.  */
#define RE2_DOT_NOT_NULL (RE2_DOT_NEWLINE << 1)

/* If this bit is set, nonmatching lists [^...] do not match newline.
   If not set, they do.  */
#define RE2_HAT_LISTS_NOT_NEWLINE (RE2_DOT_NOT_NULL << 1)

/* If this bit is set, either \{...\} or {...} defines an
     interval, depending on RE2_NO_BK_BRACES.
   If not set, \{, \}, {, and } are literals.  */
#define RE2_INTERVALS (RE2_HAT_LISTS_NOT_NEWLINE << 1)

/* If this bit is set, +, ? and | aren't recognized as operators.
   If not set, they are.  */
#define RE2_LIMITED_OPS (RE2_INTERVALS << 1)

/* If this bit is set, newline is an alternation operator.
   If not set, newline is literal.  */
#define RE2_NEWLINE_ALT (RE2_LIMITED_OPS << 1)

/* If this bit is set, then `{...}' defines an interval, and \{ and \}
     are literals.
  If not set, then `\{...\}' defines an interval.  */
#define RE2_NO_BK_BRACES (RE2_NEWLINE_ALT << 1)

/* If this bit is set, (...) defines a group, and \( and \) are literals.
   If not set, \(...\) defines a group, and ( and ) are literals.  */
#define RE2_NO_BK_PARENS (RE2_NO_BK_BRACES << 1)

/* If this bit is set, then \<digit> matches <digit>.
   If not set, then \<digit> is a back-reference.  */
#define RE2_NO_BK_REFS (RE2_NO_BK_PARENS << 1)

/* If this bit is set, then | is an alternation operator, and \| is literal.
   If not set, then \| is an alternation operator, and | is literal.  */
#define RE2_NO_BK_VBAR (RE2_NO_BK_REFS << 1)

/* If this bit is set, then an ending range point collating higher
     than the starting range point, as in [z-a], is invalid.
   If not set, then when ending range point collates higher than the
     starting range point, the range is ignored.  */
#define RE2_NO_EMPTY_RANGES (RE2_NO_BK_VBAR << 1)

/* If this bit is set, then an unmatched ) is ordinary.
   If not set, then an unmatched ) is invalid.  */
#define RE2_UNMATCHED_RIGHT_PAREN_ORD (RE2_NO_EMPTY_RANGES << 1)

/* This global variable defines the particular reg2ex2p syntax to use (for
   some interfaces).  When a reg2ex2p is compiled, the syntax used is
   stored in the pattern buffer, so changing this does not affect
   already-compiled reg2ex2ps.  */
extern reg2_syntax_t re2_syntax_options;

/* Define combinations of the above bits for the standard possibilities.
   (The [[[ comments delimit what gets put into the Texinfo file, so
   don't delete them!)  */
/* [[[begin syntaxes]]] */
#define RE2_SYNTAX_EMACS 0

#define RE2_SYNTAX_AWK							\
  (RE2_BACKSLASH_ESCAPE_IN_LISTS | RE2_DOT_NOT_NULL			\
   | RE2_NO_BK_PARENS            | RE2_NO_BK_REFS				\
   | RE2_NO_BK_VBAR               | RE2_NO_EMPTY_RANGES			\
   | RE2_UNMATCHED_RIGHT_PAREN_ORD)

#define RE2_SYNTAX_POSIX_AWK 						\
  (RE2_SYNTAX_POSIX_EXTENDED | RE2_BACKSLASH_ESCAPE_IN_LISTS)

#define RE2_SYNTAX_GREP							\
  (RE2_BK_PLUS_QM              | RE2_CHAR_CLASSES				\
   | RE2_HAT_LISTS_NOT_NEWLINE | RE2_INTERVALS				\
   | RE2_NEWLINE_ALT)

#define RE2_SYNTAX_EGREP							\
  (RE2_CHAR_CLASSES        | RE2_CONTEXT_INDEP_ANCHORS			\
   | RE2_CONTEXT_INDEP_OPS | RE2_HAT_LISTS_NOT_NEWLINE			\
   | RE2_NEWLINE_ALT       | RE2_NO_BK_PARENS				\
   | RE2_NO_BK_VBAR)

#define RE2_SYNTAX_POSIX_EGREP						\
  (RE2_SYNTAX_EGREP | RE2_INTERVALS | RE2_NO_BK_BRACES)

/* P1003.2/D11.2, section 4.20.7.1, lines 5078ff.  */
#define RE2_SYNTAX_ED RE2_SYNTAX_POSIX_BASIC

#define RE2_SYNTAX_SED RE2_SYNTAX_POSIX_BASIC

/* Syntax bits common to both basic and extended POSIX reg2ex2 syntax.  */
#define _RE2_SYNTAX_POSIX_COMMON						\
  (RE2_CHAR_CLASSES | RE2_DOT_NEWLINE      | RE2_DOT_NOT_NULL		\
   | RE2_INTERVALS  | RE2_NO_EMPTY_RANGES)

#define RE2_SYNTAX_POSIX_BASIC						\
  (_RE2_SYNTAX_POSIX_COMMON | RE2_BK_PLUS_QM)

/* Differs from ..._POSIX_BASIC only in that RE2_BK_PLUS_QM becomes
   RE2_LIMITED_OPS, i.e., \? \+ \| are not recognized.  Actually, this
   isn't minimal, since other operators, such as \`, aren't disabled.  */
#define RE2_SYNTAX_POSIX_MINIMAL_BASIC					\
  (_RE2_SYNTAX_POSIX_COMMON | RE2_LIMITED_OPS)

#define RE2_SYNTAX_POSIX_EXTENDED					\
  (_RE2_SYNTAX_POSIX_COMMON | RE2_CONTEXT_INDEP_ANCHORS			\
   | RE2_CONTEXT_INDEP_OPS  | RE2_NO_BK_BRACES				\
   | RE2_NO_BK_PARENS       | RE2_NO_BK_VBAR				\
   | RE2_UNMATCHED_RIGHT_PAREN_ORD)

/* Differs from ..._POSIX_EXTENDED in that RE2_CONTEXT_INVALID_OPS
   replaces RE2_CONTEXT_INDEP_OPS and RE2_NO_BK_REFS is added.  */
#define RE2_SYNTAX_POSIX_MINIMAL_EXTENDED				\
  (_RE2_SYNTAX_POSIX_COMMON  | RE2_CONTEXT_INDEP_ANCHORS			\
   | RE2_CONTEXT_INVALID_OPS | RE2_NO_BK_BRACES				\
   | RE2_NO_BK_PARENS        | RE2_NO_BK_REFS				\
   | RE2_NO_BK_VBAR	    | RE2_UNMATCHED_RIGHT_PAREN_ORD)
/* [[[end syntaxes]]] */

/* Maximum number of duplicates an interval can allow.  Some systems
   (erroneously) define this in other header files, but we want our
   value, so remove any previous define.  */
#ifdef RE2_DUP_MAX
#undef RE2_DUP_MAX
#endif
#define RE2_DUP_MAX ((1 << 15) - 1)


/* POSIX `cflags' bits (i.e., information for `reg2comp').  */

/* If this bit is set, then use extended reg2ular expression syntax.
   If not set, then use basic reg2ular expression syntax.  */
#define REG2_EXTENDED 1

/* If this bit is set, then ignore case when matching.
   If not set, then case is significant.  */
#define REG2_ICASE (REG2_EXTENDED << 1)

/* If this bit is set, then anchors do not match at newline
     characters in the string.
   If not set, then anchors do match at newlines.  */
#define REG2_NEWLINE (REG2_ICASE << 1)

/* If this bit is set, then report only success or fail in reg2ex2ec.
   If not set, then returns differ between not matching and errors.  */
#define REG2_NOSUB (REG2_NEWLINE << 1)


/* POSIX `eflags' bits (i.e., information for reg2ex2ec).  */

/* If this bit is set, then the beginning-of-line operator doesn't match
     the beginning of the string (presumably because it's not the
     beginning of a line).
   If not set, then the beginning-of-line operator does match the
     beginning of the string.  */
#define REG2_NOTBOL 1

/* Like REG2_NOTBOL, except for the end-of-line.  */
#define REG2_NOTEOL (1 << 1)


/* If any error codes are removed, changed, or added, update the
   `re2_error_msg' table in reg2ex2.c.  */
typedef enum
{
  REG2_NOERROR = 0,	/* Success.  */
  REG2_NOMATCH,		/* Didn't find a match (for reg2ex2ec).  */

  /* POSIX reg2comp return error codes.  (In the order listed in the
     standard.)  */
  REG2_BADPAT,		/* Invalid pattern.  */
  REG2_ECOLLATE,		/* Not implemented.  */
  REG2_ECTYPE,		/* Invalid character class name.  */
  REG2_EESCAPE,		/* Trailing backslash.  */
  REG2_ESUBREG2,		/* Invalid back reference.  */
  REG2_EBRACK,		/* Unmatched left bracket.  */
  REG2_EPAREN,		/* Parenthesis imbalance.  */
  REG2_EBRACE,		/* Unmatched \{.  */
  REG2_BADBR,		/* Invalid contents of \{\}.  */
  REG2_ERANGE,		/* Invalid range end.  */
  REG2_ESPACE,		/* Ran out of memory.  */
  REG2_BADRPT,		/* No preceding re for repetition op.  */

  /* Error codes we've added.  */
  REG2_EEND,		/* Premature end.  */
  REG2_ESIZE,		/* Compiled pattern bigger than 2^16 bytes.  */
  REG2_ERPAREN		/* Unmatched ) or \); not returned from reg2comp.  */
} reg2_errcode_t;

/* This data structure represents a compiled pattern.  Before calling
   the pattern compiler, the fields `buffer', `allocated', `fastmap',
   `translate', and `no_sub' can be set.  After the pattern has been
   compiled, the `re2_nsub' field is available.  All other fields are
   private to the reg2ex2 routines.  */

struct re2_pattern_buffer
{
/* [[[begin pattern_buffer]]] */
	/* Space that holds the compiled pattern.  It is declared as
          `unsigned char *' because its elements are
           sometimes used as array indexes.  */
  unsigned char *buffer;

	/* Number of bytes to which `buffer' points.  */
  unsigned long allocated;

	/* Number of bytes actually used in `buffer'.  */
  unsigned long used;

        /* Syntax setting with which the pattern was compiled.  */
  reg2_syntax_t syntax;

        /* Pointer to a fastmap, if any, otherwise zero.  re2_search uses
           the fastmap, if there is one, to skip over impossible
           starting points for matches.  */
  char *fastmap;

        /* Either a translate table to apply to all characters before
           comparing them, or zero for no translation.  The translation
           is applied to a pattern when it is compiled and to a string
           when it is matched.  */
  char *translate;

	/* Number of subexpressions found by the compiler.  */
  size_t re2_nsub;

        /* Zero if this pattern cannot match the empty string, one else.
           Well, in truth it's used only in `re2_search_2', to see
           whether or not we should use the fastmap, so we don't set
           this absolutely perfectly; see `re2_compile_fastmap' (the
           `duplicate' case).  */
  unsigned can_be_null : 1;

        /* If REG2S_UNALLOCATED, allocate space in the `reg2s' structure
             for `max (RE2_NREG2S, re2_nsub + 1)' groups.
           If REG2S_REALLOCATE, reallocate space if necessary.
           If REG2S_FIXED, use what's there.  */
#define REG2S_UNALLOCATED 0
#define REG2S_REALLOCATE 1
#define REG2S_FIXED 2
  unsigned reg2s_allocated : 2;

        /* Set to zero when `reg2ex2_compile' compiles a pattern; set to one
           by `re2_compile_fastmap' if it updates the fastmap.  */
  unsigned fastmap_accurate : 1;

        /* If set, `re2_match_2' does not return information about
           subexpressions.  */
  unsigned no_sub : 1;

        /* If set, a beginning-of-line anchor doesn't match at the
           beginning of the string.  */
  unsigned not_bol : 1;

        /* Similarly for an end-of-line anchor.  */
  unsigned not_eol : 1;

        /* If true, an anchor at a newline matches.  */
  unsigned newline_anchor : 1;

/* [[[end pattern_buffer]]] */
};

typedef struct re2_pattern_buffer reg2ex2_t;


/* search.c (search_buffer) in Emacs needs this one opcode value.  It is
   defined both in `reg2ex2.c' and here.  */
#define RE2_EXACTN_VALUE 1

/* Type for byte offsets within the string.  POSIX mandates this.  */
typedef int reg2off_t;


/* This is the structure we store register match data in.  See
   reg2ex2.texinfo for a full description of what registers match.  */
struct re2_registers
{
  unsigned num_reg2s;
  reg2off_t *start;
  reg2off_t *end;
};


/* If `reg2s_allocated' is REG2S_UNALLOCATED in the pattern buffer,
   `re2_match_2' returns information about at least this many registers
   the first time a `reg2s' structure is passed.  */
#ifndef RE2_NREG2S
#define RE2_NREG2S 30
#endif


/* POSIX specification for registers.  Aside from the different names than
   `re2_registers', POSIX uses an array of structures, instead of a
   structure of arrays.  */
typedef struct
{
  reg2off_t rm_so;  /* Byte offset from string's start to substring's start.  */
  reg2off_t rm_eo;  /* Byte offset from string's start to substring's end.  */
} reg2match_t;

/* Declarations for routines.  */

/* To avoid duplicating every routine declaration -- once with a
   prototype (if we are ANSI), and once without (if we aren't) -- we
   use the following macro to declare argument types.  This
   unfortunately clutters up the declarations a bit, but I think it's
   worth it.  */

#if __STDC__ || defined(__cplusplus)

#define _RE2_ARGS(args) args

#else /* not __STDC__ */

#define _RE2_ARGS(args) ()

#endif /* not __STDC__ */

/* Sets the current default syntax to SYNTAX, and return the old syntax.
   You can also simply assign to the `re2_syntax_options' variable.  */
extern reg2_syntax_t re2_set_syntax _RE2_ARGS ((reg2_syntax_t syntax));

/* Compile the reg2ular expression PATTERN, with length LENGTH
   and syntax given by the global `re2_syntax_options', into the buffer
   BUFFER.  Return NULL if successful, and an error string if not.  */
extern const char *re2_compile_pattern
  _RE2_ARGS ((const char *pattern, int length,
             struct re2_pattern_buffer *buffer));


/* Compile a fastmap for the compiled pattern in BUFFER; used to
   accelerate searches.  Return 0 if successful and -2 if was an
   internal error.  */
extern int re2_compile_fastmap _RE2_ARGS ((struct re2_pattern_buffer *buffer));


/* Search in the string STRING (with length LENGTH) for the pattern
   compiled into BUFFER.  Start searching at position START, for RANGE
   characters.  Return the starting position of the match, -1 for no
   match, or -2 for an internal error.  Also return register
   information in REG2S (if REG2S and BUFFER->no_sub are nonzero).  */
extern int re2_search
  _RE2_ARGS ((struct re2_pattern_buffer *buffer, const char *string,
            int length, int start, int range, struct re2_registers *reg2s));


/* Like `re2_search', but search in the concatenation of STRING1 and
   STRING2.  Also, stop searching at index START + STOP.  */
extern int re2_search_2
  _RE2_ARGS ((struct re2_pattern_buffer *buffer, const char *string1,
             int length1, const char *string2, int length2,
             int start, int range, struct re2_registers *reg2s, int stop));


/* Like `re2_search', but return how many characters in STRING the reg2ex2p
   in BUFFER matched, starting at position START.  */
extern int re2_match
  _RE2_ARGS ((struct re2_pattern_buffer *buffer, const char *string,
             int length, int start, struct re2_registers *reg2s));


/* Relates to `re2_match' as `re2_search_2' relates to `re2_search'.  */
extern int re2_match_2
  _RE2_ARGS ((struct re2_pattern_buffer *buffer, const char *string1,
             int length1, const char *string2, int length2,
             int start, struct re2_registers *reg2s, int stop));


/* Set REG2S to hold NUM_REG2S registers, storing them in STARTS and
   ENDS.  Subsequent matches using BUFFER and REG2S will use this memory
   for recording register information.  STARTS and ENDS must be
   allocated with malloc, and must each be at least `NUM_REG2S * sizeof
   (reg2off_t)' bytes long.

   If NUM_REG2S == 0, then subsequent matches should allocate their own
   register data.

   Unless this function is called, the first search or match using
   PATTERN_BUFFER will allocate its own register data, without
   freeing the old data.  */
extern void re2_set_registers
  _RE2_ARGS ((struct re2_pattern_buffer *buffer, struct re2_registers *reg2s,
             unsigned num_reg2s, reg2off_t *starts, reg2off_t *ends));

/* 4.2 bsd compatibility.  */
extern char *re2_comp _RE2_ARGS ((const char *));
extern int re2_exec _RE2_ARGS ((const char *));

/* POSIX compatibility.  */
extern int reg2comp _RE2_ARGS ((reg2ex2_t *preg2, const char *pattern, int cflags));
extern int reg2ex2ec
  _RE2_ARGS ((const reg2ex2_t *preg2, const char *string, size_t nmatch,
             reg2match_t pmatch[], int eflags));
extern size_t reg2error
  _RE2_ARGS ((int errcode, const reg2ex2_t *preg2, char *errbuf,
             size_t errbuf_size));
extern void reg2free _RE2_ARGS ((reg2ex2_t *preg2));

#endif /* not __REG2EX2P_LIBRARY_H__ */

/*
Local variables:
make-backup-files: t
version-control: t
trim-versions-without-asking: nil
End:
*/
