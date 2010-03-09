#!/usr/bin/perl

$usage = "Usage: touchver file.h define_var lead_str\n";

my ($file, $define, $lead) = @ARGV;

die $usage if ($lead eq '');

die "$file: $!\n" if (!open IN, "<$file");

my $gotit = 0;

while($line = <IN>) {
	if ($line =~ m/^(\s*#define\s+$define\s+")$lead\.(\d+)/) {
		$line = "$1$lead." . ($2+1) . $';
		$gotit = 1;
	}
	push(@lines, $line);
}
close IN;
die "Unable to find #define $define in $file\n" if (!$gotit);

open OUT, ">$file" or die "$file: $!\n";
print OUT @lines;
close OUT;

