dnl I don't like the standard mmap test, its broken in
dnl several ways.

AC_DEFUN(JM_FUNC_MMAP,
[AC_CACHE_CHECK(for working mmap read private, ac_cv_func_mmap,
[AC_TRY_RUN([
/* Josh's READ ONLY mmap test.  This is taken from, but heavily
 * modified, the autoconf 2.10 distribution.  This only tests the
 * features of mmap required by PRCS.  It is advisable to run
 * this test on an NFS filesystem if you intend to use PRCS with
 * a repository on NFS. */
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

int
main()
{
  char *buf1;
  caddr_t     buf2;
  char *buf3;
  int ps = 5000;  /* This is purposely not a pagesize. */
  int ps2 = 2*ps;
  int ps3 = 3*ps;
  int j;
  int fd;

  /* Allocate 3 pages of buffer. */
  if ((buf1 = (char *)malloc(ps3)) == NULL ||
      (buf3 = (char *)malloc(ps3)) == NULL)
    {
      perror ("malloc failed");
      exit(1);
    }

  /* Generate 3 pages of random numbers into buf1. */
  for (j = 0; j < ps3; ++j)
    *(buf1 + j) = rand();

  /* Open for read/write. */
  if ((fd = open("conftestmmap", O_CREAT | O_RDWR, 0666)) < 0)
    {
      perror ("open 1 failed");
      exit(1);
    }

  /* Write 2 pages of the random numbers to the file. */
  if (write(fd, buf1, ps2) != ps2)
    {
      perror ("write 1 failed");
      exit(1);
    }

  /* Now map the file onto buf2. */
  if((buf2=mmap(NULL, ps2, PROT_READ, MAP_PRIVATE, fd, 0)) == NULL)
    {
      perror ("mmap 1 failed");
      exit (1);
    }

  /* Compare buf1 and buf2. */
  for (j = 0; j < ps2; ++j)
    if (*(buf1 + j) != *(buf2 + j))
      {
	fprintf (stderr, "read/mmap comparison failed\n");
	exit(1);
      }

  /* Write the 3rd page to the file. */
  if (write(fd, buf1+ps2, ps) != ps)
    {
      perror ("write 2 failed");
      exit(1);
    }

  /* Unamp the memory. */
  if (munmap (buf2, ps2) < 0)
    {
      perror ("munmap failed");
      exit(1);
    }

  /* Close the file. */
  if (close (fd) < 0)
    {
      perror ("close failed");
      exit(1);
    }

  /* Reopen the file. */
  if ((fd = open("conftestmmap", O_RDWR, 0666)) < 0)
    {
      perror ("open 2 failed");
      exit(1);
    }

  /* Recompare all 3 pages. */
  /* ... map the file onto buf2. */
  if((buf2=mmap(NULL, ps3, PROT_READ, MAP_PRIVATE, fd, 0)) == NULL)
    {
      perror ("mmap 1 failed");
      exit (1);
    }

  /* ... compare buf1 and buf2. */
  for (j = 0; j < ps3; ++j)
    if (*(buf1 + j) != *(buf2 + j))
      {
	fprintf (stderr, "read/mmap/unmap/append/mmap comparison failed");
	exit(1);
      }

  /* Now read it. */
  if (read(fd, buf3, ps3) != ps3)
    {
      perror ("read 1 failed");
      exit(1);
    }

  /* And compare again. */
  for (j = 0; j < ps3; ++j)
    if (*(buf1 + j) != *(buf3 + j))
      {
	fprintf (stderr, "read/mmap/unmap/append/read comparison failed");
	exit(1);
      }

  exit(0);
}
], ac_cv_func_mmap=yes, ac_cv_func_mmap=no, ac_cv_func_mmap=no)])
if test $ac_cv_func_mmap = yes; then
  AC_DEFINE([HAVE_MMAP], [], [Define to 1 if 'mmap' works.])
fi
])
