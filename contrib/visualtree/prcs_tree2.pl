#!/usr/bin/perl -w
#
#	prcs_tree2.pl.
#	Copyright 1998 Keith Owens <kaos@ocs.com.au>.
#	Minor modifications/bug fixes (C) 2001  by Hugo Cornelis
#		<hugo@bbf.uia.ac.be>
#	Released under the GNU Public Licence.
#
#	Read the output from "prcs info -l project" and plot the relationship
#       between versions.  Output is commands for "VCG tool -
#       visualization of compiler graphs" by Iris Lemke, Georg Sander,
#       and the Compare Consortium.
#
#	Typical use :-
#	prcs info -l project > /var/tmp/$$a
#	prcs_tree2.pl < /var/tmp/$$a > /var/tmp/$$b
#	xvcg /var/tmp/$$b
#	rm /var/tmp/$$[ab]

require 5;
use strict;

my $lineno = 0;
my $longlog = 0;
my ($project, $version, $weekday, $day, $month, $year, $time, $user, $parent);
my %node = ();
my @edge = ();

while (<>) {
	chop();
	if (++$lineno == 1) {
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
	if (/^$project/ && $longlog == 0) {
		($version, $weekday, $day, $month, $year, $time, $user) =
			(split(' ', $_, 10))[1..6,9];
		$node{$version} = undef;
		printf "\tnode:\t{ title: \"$version\"";
		# Show the mainline (digits.1) in red and thicker
		if ($version =~ /^\d+\.1$/) {
			printf " bordercolor: red borderwidth: 7";
		}
		printf " label: \"$version\\n";
		printf "$weekday $day $month $year $time $user\\n";
	}
	elsif (/^Version-Log:/) {
		s/^Version-Log:\s*//;
		$longlog = (/^$/);
		$longlog = 1;
		if (!$longlog) {
			printf "$_\"\n";
		}
	}
	elsif (/^Project-Description:/) {

	  # end long version logs

	  printf "\"\n";	# end of title
	  $longlog = 0;
	  print "\t}\n"
	}
	elsif ($longlog) {
		++$longlog;
		{
		  print "$_\\n";
		}
	}
	elsif (/^Parent-Version:/) {
		s/^Parent-Version:\s*//;
		push(@edge, $version, $_);
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
	if (! exists($node{$from})) {
		printf "\tnode:\t{ title: \"$from\" label: \"$from\\n";
		printf "Not part of input\"}\n";
	}
}

printf "}\n";

