#!/usr/bin/perl -w
#
#	prcs_tree_info.pl.
#	Copyright 1998 Keith Owens <kaos@ocs.com.au>.
#	Minor modifications (C) 2001  by Hugo Cornelis <hugo@bbf.uia.ac.be>
#	Released under the GNU Public Licence.
#
#	Read the output from "prcs info -l project" and plot the relationship
#       between versions.  Output is commands for "VCG tool -
#       visualization of compiler graphs" by Iris Lemke, Georg Sander,
#       and the Compare Consortium.
#
#	Typical use :-
#	prcs info -l project > /var/tmp/$$a
#	prcs_tree_info.pl < /var/tmp/$$a > /var/tmp/$$b
#	xvcg /var/tmp/$$b
#	rm /var/tmp/$$[ab]
#
#	You can also use different branches to check how they depend
#	on each other.  This example assumes a couple of branches starting
#	with 'EDS', a couple of branches starting with 'HC', asks info
#	and feeds the output to xvcg.  Versions that are merged into the
#	'EDS*' or 'HC*', have a single node to show this, but don't have
#	any node information.
#
#	( prcs info -l --plain-format -r "EDS*" genesis.prj \
#		&& prcs info -l --plain-format -r "HC*" genesis.prj ) \
#	| /genesis/prcs_support/visualtree/prcs_tree_info.pl \
#	| /other/vcg/vcg.1.30/src/xvcg -
#
#	The info1 field of the nodes is used for the version logs.
#	Perhaps the info2 field could be used for date.
#

require 5;
use strict;

my $output = 1;
my $lineno = 0;
my $versionlog = 0;
my ($project, $version, $weekday, $day, $month, $year, $time, $user, $parent);
my %node = ();
my @edge = ();

while (<>)
  {
    chop();
    if (++$lineno == 1)
      {
	($project) = (split(' ', $_, 2))[0];
	printf "graph:\t{ title: \"PRCS tree for $project\"\n";
	printf "\tport_sharing: no\n";
	printf "\tlayoutalgorithm: minbackward\n";
	printf "\tlayout_downfactor: 39\n";
	printf "\tlayout_upfactor: 0\n";
	printf "\tlayout_nearfactor: 0\n";
	printf "\tnearedges: no\n";
	printf "\tsplines: yes\n";
	printf "\tstraight_phase: yes\n";
	printf "\tpriority_phase: yes\n";
	printf "\tcmin: 10\n";
      }

    if ($versionlog)
      {
	if (/^Project-Description:/)
	  {
	
	    if ($output)
	      {
		# end version logs
	    
		printf "\"\n";
	    
		# end node
	    
		print "\t}\n"
	      }	    
	
	    $versionlog = 0;
	  }
	elsif ($output)
	  {
	    # continue version log
	    
	    ++$versionlog;
	    
	    # replace any '"', they confuse vcg

	    s(")(\\")g ;

	    print "$_\\n" ;
	  }
      }
    elsif (/^$project/)
      {
	($version, $weekday, $day, $month, $year, $time, $user)
	  = (split(' ', $_, 10))[1..6,9];
	
	# if node not part of previous input
	
	if (!exists($node{$version}))
	  {
	    # enable output
	    
	    $output = 1;
	    
	    # remember node part of input

	    $node{$version} = undef;

	    printf "\tnode:\t{ title: \"$version\"";
	    # Show the mainline (digits.1) in red and thicker
	    if ($version =~ /^\d+\.1$/)
	      {
		printf " bordercolor: red borderwidth: 7";
	      }
	    printf " label: \"$version\\n";
	    printf "$weekday $day $month $year $time $user\"";
	    printf " info1:\n\"$version\\n";
	  }
	else
	  {
	    $output = 0;
	  }
      }
    elsif (/^Version-Log:/)
      {
	if ($output)
	  {
	    s/^Version-Log:\s*//;
	    
	    # start version log
	    
	    $versionlog = 1;

	    # replace any '"', they confuse vcg

	    s(")(\\")g ;

	    print "$_\\n" ;
	  }
      }
    elsif (/^Parent-Version:/)
      {
	if ($output)
	  {
	    s/^Parent-Version:\s*//;
	    push(@edge, $version, $_);
	  }
      }
  }

while ($#edge >= 0) {
  my ($from, $to) = (pop(@edge), pop(@edge));
  my ($fv, $tv);
  printf "\tedge:\t{ sourcename: \"%s\" targetname: \"%s\"",
  $from, $to;
  # Show the mainline (digits.1) in red and thicker
  if ($from =~ /^\d+\.1$/ && $to =~ /^\d+\.1$/) {
    printf " priority: 1000 color: red thickness: 7";
  }
  else {
    # Local version branches are black, merge branches are blue
    ($fv = $from) =~ s/\..*//;
    ($tv = $to) =~ s/\..*//;
    if ($fv eq $tv) {
      printf " color: black";
    }
    else {
      printf " color: blue";
    }
  }
  printf "}\n";
  if (! exists($node{$from}))
    {
      printf "\tnode:\t{ title: \"$from\" label: \"$from\\n";
      printf "Not part of input\"}\n";

      $node{$from} = undef;
    }
}

printf "}\n";

