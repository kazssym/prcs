#!/usr/bin/perl
# File: fix-perl-path.pl
# Description: Fix path for Perl scripts after initial `#!' in Debian systems
# Author: Rafael Laboissière <rafael@icp.inpg.fr>
# Created on: 15 Jun 1998 22:03:41 +0200
# Last modified on: Mon Nov 15 01:33:58 CET 1999

die "usage: $0 file\n" if $#ARGV != 0;

open (FILE, "<$ARGV[0]")
  or die "$0: Could not open file $ARGV[0]\n";

$tmpfile = "/tmp/fix-perl-path-$$";
open (TMP, ">$tmpfile")
  or die "$0: Could not open temp file\n";

$debian_perl_path = "/usr/bin/perl";

@tmp = ();
$done = 0;
while (<FILE>) {
  if ((not $done) && /^\#\![ ]*(.*perl.*)\s+(.*)$/ ) {
    print TMP "#! ".$debian_perl_path." $2\n";
    $done = 1;
  }
  else {
    print TMP $_;
  }
} 
close (FILE);
close (TMP);

system ("mv $tmpfile $ARGV[0]") == 0 
  or die "$0: Could not move temp file into $ARGV[0]\n";

system ("chmod ugo+x $ARGV[0]") == 0 
  or die "$0: Could not chmod +x $ARGV[0]\n";

