#!/usr/bin/perl

use strict;

my @msgid = ();
my @new_msgid = ();
my @keywords = ("title", "repname", "name", "label");

open(POTFILE, "soundtracker.pot") or die "No soundtracker.pot";
while ((my $line = <POTFILE>)) {
	# We simply ignore mutiline msgids with empty first line since they cannot be
	# used in menu files
	if ($line =~ /msgid \"(.+)\"/) {
		push @msgid, $1;
	}
}
close POTFILE;

sub scan_extension {
	open(EXT, $_[0]);
	while ((my $line = <EXT>)) {
		foreach (@keywords) {
			if ($line =~ /$_=\"(.+?)\"/) {
				my $newid = $1;
				my $found = 0;
				foreach (@msgid) {
					if ($_ eq $newid) {
						$found = 1;
						last;
					}
				}
				if (!$found) {
					foreach (@new_msgid) {
						if ($_ eq $newid) {
							$found = 1;
							last;
						}
					}
					push @new_msgid, $newid if (!$found);
				}
			}
		}
	}
	close EXT;
}

sub walk {
	my $dname = $_[0];
	opendir(CURDIR, $dname) or die "No directory";
	foreach (readdir(CURDIR)) {
		my $path = $dname . "/" . $_;
		if (-d $path) {
			walk($path) if ($_ ne "." && $_ ne "..");
		} elsif ($_ =~ /.+\.menu$/) {
			scan_extension($path);
		}
	}
	closedir INFILE;
}

walk($ARGV[0]);

open(POTFILE, ">>", "soundtracker.pot");
foreach (@new_msgid) {
	print POTFILE "\n#:\nmsgid \"$_\"\nmsgstr \"\"\n";
}
close POTFILE;
