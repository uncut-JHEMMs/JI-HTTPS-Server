#!/usr/bin/perl

use strict;
use warnings;
use Fcntl qw(:seek :flock);

my $counter_dir = $ARGV[0];
my $counter_name = $ARGV[1];

# Open counter file
open my $fh, '+<', "$counter_dir/$counter_name" or die "Couldn't open counter file: $!";

# Lock for exclusive access
flock($fh,LOCK_EX) or die "Couldn't get lock: $!\n";

# Read current value
my $cnt=<$fh>;

# Increment value and write back
seek($fh,0,SEEK_SET);
printf $fh '%d', ++$cnt;

# Close handle
close $fh;