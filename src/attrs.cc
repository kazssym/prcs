/* -*- C -*- code produced by gperf version 2.1 (K&R C version) */
/* -- modified jmacd@cs.berkeley.edu */
/* Command-line: jgperf -A -G -C -a -t -T -p -N is_file_attribute -H is_file_attribute_hash  */

#include "prcs.h"
#include "projdesc.h"

#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 19
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 41
/*
   23 keywords
   38 is the maximum key range
*/

int
is_file_attribute_hash (register const char *str, register int len)
{
  static const unsigned char hash_table[] =
    {
     41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
     41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
     41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
     41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
     41, 41, 41, 41, 41, 41, 41, 41, 20,  5,
     10, 15,  0, 20, 25, 30,  3, 18,  0, 41,
     41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
     41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
     41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
     41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
     41,  5, 41,  0, 41, 41, 41,  0, 15, 41,
     41, 41, 41, 41, 41, 25, 41, 41, 41, 41,
     41,  0, 41, 41, 41, 41, 41, 41,
    };
  return len + hash_table[str[len - 1]] + hash_table[str[0]];
}


 const class AttrDesc  wordlist[] =
{
      {"",}, {"",}, {"",}, {"",},
      {":tag",            TagAttr},
      {"",}, {"",}, {"",},
      {":symlink",        SymlinkAttr},
      {"",},
      {":directory",      DirectoryAttr},
      {":mergerule4",     Mergerule4Attr},
      {":mergerule14",    Mergerule14Attr},
      {"",},
      {":mergerule8",     Mergerule8Attr},
      {":real-file",      RealFileAttr},
      {":mergerule1",     Mergerule1Attr},
      {":mergerule11",    Mergerule11Attr},
      {":project-file",   ProjectFileAttr},
      {":implicit-directory",  ImplicitDirectoryAttr},
      {"",},
      {":mergerule2",     Mergerule2Attr},
      {":mergerule12",    Mergerule12Attr},
      {"",},
      {":difftool",       DifftoolAttr},
      {":mergetool",      MergetoolAttr},
      {":mergerule3",     Mergerule3Attr},
      {":mergerule13",    Mergerule13Attr},
      {"",},
      {":mergerule9",     Mergerule9Attr},
      {"",},
      {":mergerule5",     Mergerule5Attr},
      {":mergerule10",    Mergerule10Attr},
      {"",}, {"",}, {"",},
      {":mergerule6",     Mergerule6Attr},
      {":no-keywords",    NoKeywordAttr},
      {"",}, {"",}, {"",},
      {":mergerule7",     Mergerule7Attr},
};

const class AttrDesc *
is_file_attribute (register const char *str, register int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = is_file_attribute_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const char *s = wordlist[key].name;

          if (*s == *str && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
