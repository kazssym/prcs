This is an alpha release of the PFE version tree tool. This tool is
a little rough around the edges yet, but I find it useful. 

This tool will build a version tree for projects under PRCS version
control. This package is written in Tea, but you don't need Tea installed; 
everything is included here to run it.


FILES
=====
vertree.pot   Tea pot archive containing the version tree and Tea core code
tre           Tea Runtime Environment tool to run the above tea pot
vertree       Shell script to launch tool
versiontree/* Source code to the versiontree package


INSTALLATION
============

Unpack the tar archive somewhere.

The best way to run it is to use the vertree shell script. Update the
`pfedir' variable to point to where you installed the files, and copy
vertree, or make a symlink, to a directory in your path.


RUNNING
=======

Syntax:

  vertree ?options?

Currently, vertree creates a version tree from the project in the current
directory. If it cannot find a .prj file, or found more than one, an error
is printed and it exits.

vertree options:

  -w    suppress warning messages
  -q    quiet, suppress all superfluous output
  -s    short form; show only minor versions that have something important, 
        like a branch going off or a merge coming in.
  -V    prints out the version number of vertree and exits.

The version tree is pretty straightforward, although quite different than what
the PRCS team's documentation depicts. I modelled vertree after Pure Atria's 
Clearcase product.

Basically you have branches that are rectangles. Minor versions checked in 
under that branch are in circles. Merges are arcs, branches are straight lines.
If the mesh of lines gets confusing, you can click on a connector to turn it
red so you can see where it is going. You can also select the circles and
rectangles. This is for future functionality.

Printing will dump out the canvas into a file called versiontree.ps.

For versions that no longer exist, they are shown in gray.


MODIFYING TEA SOURCES
=====================

If you have Tea installed (http://www.doitnow.com/~iliad/Tcl/tea), you can
modify the sources in versiontree/*. To run, just enter 

          tea +cp . -cs versiontree.VersionTree ?options?

To build your own tea pot:

          teac -depend versiontree/*.tea
          cp <tea lib dir>/classes.pot ./myown.pot
          tpot a ./myown.pot versiontree/*.t versiontree/versiontree.init
          tpot m ./myown.pot versiontree.VersionTree


CAVEATS
=======

  * vertree is slow. You can blame Tea for this. When the optimizing Tea
    compiler is done, vertree should speed up dramatically.
    
  * the algorithm for building the tree structure is a little shakey, so
    you might get odd or cramped looking trees. I'll be working on this.
    
  * printing doesn't work well. I don't know if the canvas postscript
    command is buggy or if I just don't know how to use it. Running ghostview
    on the output looks fine, but dumping it to my HP inkjet ends up cropping
    the top and bottom.


FUTURE
======

The PFE project (PRCS Front End) will be a friendlier interface to the
PRCS system, providing a file browser and version tree utility. The goal
is to be able to do everything that is possible from the command line in
either the file browser or the version tree tools. PFE will insulate the
user from some of the ugliness of PRCS, such as hand editing .prj files
and using the populate subcommand.


john stump
sep 1998
iliad@doitnow.com
http://www.doitnow.com/~iliad/Tcl/PFE
