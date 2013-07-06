/* -*- C -*- code produced by gperf version 2.1 (K&R C version) */
/* -- modified jmacd@cs.berkeley.edu */
/* Command-line: jgperf -A -C -a -r -k1,9 -N is_builtin_keyword -H is_builtin_keyword_hash  */



#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 19
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 26
/*
   14 keywords
   23 is the maximum key range
*/

int
is_builtin_keyword_hash (register const char *str, register int len)
{
  static const unsigned char hash_table[] =
    {
     26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
     26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
     26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
     26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
     26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
     26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
     26, 26, 26, 26, 26, 14, 11, 26,  8, 26,
     12, 26, 26,  2, 26, 26, 26, 26, 26, 26,
      4, 26,  2,  3, 26, 26, 26, 26, 26, 26,
     26, 26, 26, 26, 26, 26, 26,  1, 26, 26,
     26,  8, 26, 26, 26,  0, 26, 26, 26, 26,
     26, 26, 26, 26, 26, 26, 26,  4, 26, 26,
     26, 26, 26, 26, 26, 26, 26, 26,
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 9:
        hval += hash_table[str[8]];
      case 8:
      case 7:
      case 6:
      case 5:
      case 4:
      case 3:
      case 2:
      case 1:
        hval += hash_table[str[0]];
    }
  return hval ;
}

const char *
is_builtin_keyword (register const char *str, register int len)
{

   const char * wordlist[] =
    {
      "", "", "", "", 
      "Id", 
      "", "", "", "", 
      "Source", 
      "Revision", 
      "Project", 
      "Date", 
      "", "", "", 
      "ProjectDate", 
      "", 
      "Format", 
      "Basename", 
      "Author", 
      "ProjectAuthor", 
      "", 
      "ProjectMinorVersion", 
      "ProjectMajorVersion", 
      "ProjectHeader", 
      "ProjectVersion", 
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = is_builtin_keyword_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const char *s = wordlist[key];

          if (*s == *str && !strcmp (str + 1, s + 1))
            return s;
        }
    }
  return 0;
}
