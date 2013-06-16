PRCS 1.2 for BeOS R3.
Ported September/Oktober 1998 by Lars Duening <lars@cableinet.co.uk>.

  PRCS, the Project Revision Control System, is a set of tools that (like
  CVS) provide a way to deal with sets of files and directories as an
  entity, preserving coherent versions of the entire set. Its purpose is
  similar to that of SCCS, RCS, and CVS, but (according to its authors, at
  least), it is much simpler than any of those systems.

  Please read the original documentation - this file only deals with the
  specialities of the Be port.

  If you find bugs, or have comments or improvements, I'm glad to hear
  from you.


Requirements
------------

  A machine (PowerPC or x86) running BeOS R3 or better to run PRCS.
  PRCS needs RCS 5.7 and the GNU diffutils, which come preinstalled
  with BeOS.

  The full Metrowerks Codewarrior to compile the source distributions,
  and the GeekGadget makeinfo and texi2html to create the documentation.

  The GeekGadget autoconf/-make/-header suite to configure the source
  distribution.


Distributions
-------------

  PRCS comes in one of three packages:

  The original source distribution from the PRCS main site (1.2.10 or newer).
  
    This distribution contains just the plain sources. To create a
    binary, it has to be configured and compiled.

  The Beified source distribution from BeWare.

    This distribution contains just the plain sources, but already
    configured for Be (no autoconf necessary).

  The binary distribution from BeWare.

    This distribution contains the precompiled binaries and the
    documentation in all sorts of formats.

    NOTE: up to now, the x86 binary is just cross-compiled and
    provided without any guarantee or test.


Installation
------------

  If you are using one of the source distributions, you have to compile
  the binary first (this also makes the documentation). For instructions
  on how to do this, see the next paragraph.
  
  The binary distribution comes with precompiled binaries for PowerPC
  and Intel x86. As the x86 binary is just cross-compiled, it comes with
  no guarantee for functionality whatsoever. Of course, I'd like to hear
  about your experiences with this binary.

  The binaries are located in the directories src/obj.ppc and src/obj.x86.
  To install them on your system, simply copy the binary of your choice
  into a directory in your shell search path (/boot/home/config/bin is
  probably most appropriate). With the command

    prcs config

  you can then test if prcs is happy where it is.

  The documentation is in the doc/ subdirectory and comes at least
  as man-page, as html files and as texinfo file. Put them whereever
  you keep your online documentation.


Compilation
-----------

  The source distributions come in two flavours: the original source
  distribution from the PRCS author, and the already configured
  distribution from BeWare. Where the latter can be made directly, the
  former _has_ to be configured first.

  To (re)configure PRCS, you need the GeekGadget tools autoconf, automake
  and autoheader. The actual configuration is taken care of by the
  script 'config-be' in the top directory. It is not advisable to
  call 'configure' directly as the created Makefiles need some after-
  patching to become useful. To configure the original source distribution,
  the argument '-f' must be given on the first call to config-be - it
  causes the necessary recreation of the configure script. For further
  information about config-be, give the argument '--help'.

  To rebuild the PRCS package, you need the full version of the Metrowerks
  Codewarrior, and the GeekGadget tools makeinfo and texi2html for the
  documentation. The whole package is rebuilt by giving the command 'make'
  in the top directory. To just recompile the binary, cd into the src/
  directory and command 'make' there.

  Depending on the type of Makefile chosen via config-be, the binary
  is created in one of two places. The default Makefile creates the
  binaries (and object files) directly in src/, whereas the special
  Be-Makefile (created with 'config-be --be') puts these files into
  the directory src/obj.<cpu>/, with <cpu> being either 'x86' or 'ppc'.
  The Be-Makefile also offers the possibility of cross-compilation by
  giving the argument 'CPU=<cpu>' to the make command.

  
About the port
--------------

  Porting prcs was quite easy. Besides modifying the sources to use the
  standard C++ library instead of the Gnu C++ library, most of the work
  went into the (auto)configuration and bug hunting.

  In detail, the following problems had to be solved:

   - PRCS optimizes the calls to rcs by passing as many files per call
     as possible. However, I observed that calls to ci with 48 files or
     more tend to become very slow. Furthermore, some calls with more
     than one file causes ci to freeze (note to self: check in amylaar
     3.2.1@141 over 3.2@319 to see this behaviour). The solution is
     an arbitrary limit of one file per call - it's slow, but it works.

   - After forking more than 31900 child processes, _something_ in
     the OS breaks and PRCS cannot continue. The symptoms can be a
     access violation in either PRCS or the child process, or PRCS
     suddenly can't find the repository lockfile (though it's still
     there). Since in order to run into this problem you would have
     to checkin/checkout more than 10000 files at once, I left it
     at this for now.
     
   - PRCS uses an alarm signal handler to keep its .lock files up to date.
     Unfortunately the handler uses non-thread safe functions which on
     Be can cause the program to fail or, worse, to freeze. Fortunately
     it is possible to cautiously use threads in conjunction with fork(),
     so the alarm signal was replaced by a parallel thread snoozing
     for the allocated time and then calling the alarm signal handler
     in a well-defined thread-safe manner.

     Thanks to Chris Herborth (<chrish@qnx.com>) and Jon Watte
     (<hplus@zilker.net>) for their helpful comments on this subject
     
   - index() and friends are defined in <posix/bsd_mem.h> as macros
     to strchr() etc; bsd_mem.h is included as part of the iostream
     definitions. This breaks classes which define 'index' functions :-(
     Fix: #define USG on the compiler commandline.
     
   - Be include <cpp/mcompile.h> defines READ_LOCK() and others, clashing
     with the READ_LOCK define in lock.h.
     Fix: change lock.h to undef READ_LOCK and WRITE_LOCK.
     
   - BeOS has no itimer, it was simulated with the alarm thread
     described above.
   
   - -DMSIPL_DEF_TEMPARG is needed on commandline to make the C++ library
     work properly (see mailing by Jon Watte on BeDevTalk, Aug 98).
     
   - BeOS up to R3 inclusive can't select on files. syscmd.cc is affected
     by this, I poll instead :-( As soon as I get R4 in my hands, I'll
     make this behaviour depending on the actual OS version.
     
   - The Makefiles created by the GeekGadget auto{conf,make} packages
     are incomplete and geared towards the gnu compiler. As described
     above, I put a wrapper around the configure mechanism and do
     some patching on the Makefiles.
     
   - The configure created by autoconf fails to determine a few
     properties of the BeOS environment:
       - the type 'ino_t' is expected by inclusion of <stdio.h>, under
         BeOS <sys/types.h> has to be included explicitely.
       - the type 'fd_set' is hidden in <socket.h>, not where
         configure.in expects it to be.
       - signal handler must be compiled with 'int' as argument type,
         else the generated code will mess up the stack.
     The solution in all cases was to give configure.in explicite
     knowledge about the BeOS platform.


TODO
----

  PRCS knows nothing about Be's file attributes. As it relies on rcs
  for the file handling, this is tricky to solve. It might be better
  to wait until PRCS 2 with this, but I'm working on it.

  Also on my list (though admittedly not on the first position :-)
  is adding support for the gnu compiler in the config-be script.

