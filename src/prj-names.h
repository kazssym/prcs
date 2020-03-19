/* -*- C -*- code produced by gperf version 2.1 (K&R C version) */
/* -- modified jmacd@cs.berkeley.edu */
/* Command-line: jgperf -A -G -C -a -t -T -p -N ProjectDescriptor::prj_lookup_func -H ProjectDescriptor::prj_lookup_hash -W ProjectDescriptor::_pftable  */



#define MIN_WORD_LENGTH 5
#define MAX_WORD_LENGTH 23
#define MIN_HASH_VALUE 5
#define MAX_HASH_VALUE 32
/*
   14 keywords
   28 is the maximum key range
*/

int
ProjectDescriptor::prj_lookup_hash (register const char *str, register int len)
{
  static const unsigned char hash_table[] =
    {
     32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
     32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
     32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
     32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
     32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
     32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
     32, 32, 32, 32, 32, 32, 32,  0,  5, 32,
      0, 32, 32, 32, 32, 32, 32, 15, 15, 32,
      0, 32, 32, 32, 32, 32,  0, 32, 32, 32,
     32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
     32, 10, 32,  0, 32, 32, 32, 32, 32,  0,
      0, 32, 32, 32, 32,  0, 32, 32, 32, 32,
     32, 32, 32, 32, 32, 32, 32, 32,
    };
  return len + hash_table[str[len - 1]] + hash_table[str[0]];
}


 const struct PrjFields  ProjectDescriptor::_pftable[] =
{
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"Files",                    NULL, &ProjectDescriptor::files_prj_entry_read,             true},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"Version-Log",              &ProjectDescriptor::version_log_prj_entry_func,      NULL,  true},
      {"",}, 
      {"Checkin-Login",            &ProjectDescriptor::checkin_login_prj_entry_func,    NULL,  true},
      {"Parent-Version",           &ProjectDescriptor::parent_ver_prj_entry_func,       NULL,  true},
      {"Project-Version",          &ProjectDescriptor::project_ver_prj_entry_func,      NULL,  true},
      {"Project-Keywords",         &ProjectDescriptor::project_keywords_prj_entry_func, NULL,  false},
      {"",}, 
      {"Descends-From",            &ProjectDescriptor::descends_from_prj_entry_func,    NULL,  false},
      {"Project-Description",      &ProjectDescriptor::project_desc_prj_entry_func,     NULL,  true},
      {"",}, {"",}, 
      {"Checkin-Time",             &ProjectDescriptor::checkin_time_prj_entry_func,     NULL,  true},
      {"Created-By-Prcs-Version",  &ProjectDescriptor::created_by_prj_entry_func,       NULL,  true},
      {"",}, 
      {"Populate-Ignore",          &ProjectDescriptor::populate_ignore_prj_entry_func,  NULL,  false},
      {"",}, {"",}, 
      {"Merge-Parents",            NULL, &ProjectDescriptor::merge_parents_prj_entry_read,     false},
      {"",}, 
      {"New-Version-Log",          &ProjectDescriptor::new_version_log_prj_entry_func,  NULL,  true},
      {"",}, 
      {"New-Merge-Parents",        NULL, &ProjectDescriptor::new_merge_parents_prj_entry_read, false},
};

const struct PrjFields *
ProjectDescriptor::prj_lookup_func (register const char *str, register int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = ProjectDescriptor::prj_lookup_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const char *s = ProjectDescriptor::_pftable[key].name;

          if (*s == *str && !strcmp (str + 1, s + 1))
            return &ProjectDescriptor::_pftable[key];
        }
    }
  return 0;
}
