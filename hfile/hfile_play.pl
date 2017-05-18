#!/usr/bin/perl -w

use strict;
use warnings;

use lib qw(/nas/scripts/perl_lib);

use SerialArduino;
use Data::Dumper; 

# ------------------------------
# Inits
# ------------------------------
my @audio;

my $Arduino = SerialArduino->new(
  port     => '/dev/ttyUSB0',
  baudrate => 115200,
 
  databits => 8,
  parity   => 'none',
);


# ------------------------------
# Process audio
# ------------------------------
if ($#ARGV != 0) { die "Usage: $0 <audio_file>\n"; }
print "Process audio file $ARGV[0]\n";


open(my $fh, '<', $ARGV[0])
  or die "Could not open file '$ARGV[0]' $!";

while (my $row = <$fh>) {
	chomp $row;
	$row =~ s/\n//g;
	$row =~ s/\s//g;
	$row =~ s/,$//;
	@audio = (@audio, split(/,/, $row));
}
print "Samples to process: " . scalar(@audio) . "\n";
#print Dumper(@audio);exit(1);

print "Play file: $ARGV[0]\n";

# ------------------------------
# Init arduino
# ------------------------------
print "Waiting for arduino to setup serial\n";
my $header;
while (1) {
	$header = $Arduino->receive();
	print "ARD:$header\n";
	last if ($header =~ /GO/);
}

# Prepare 10bit long int
my $audio_len = scalar(@audio);
if (length($audio_len) > 10) { die "Audio length does not fit to 10 bit: " . length($audio_len); }
my $charTen = "0" x (10-length($audio_len)) . $audio_len;

print "Sending audio len: " . $charTen . "\n";
$Arduino->communicate($charTen) or die 'Warning, empty string: ', "$!\n";
$header = $Arduino->receive();
print "ARD:$header\n";

print "Entering loop...\n";
my $send;
my $count=0;
my $j=0;
my $tmp;
while(1) {
#	print "Read... " . ($audio_len - $count) . "\n";
	$send = $Arduino->receive();
	#chomp($send);
#	print "ARD" . $j . "!\n";
#	print "ARD:$send\n";
#	print "|" . $j . "ARD\n";
#	$j++;
	foreach my $i (1..$send) {
#	foreach my $i (1..128) {
	#print "-scalar($audio[$count])+";
		$tmp=$audio[$count]+0;
		$tmp = 49 if ($tmp == 48);## PATCH ## Perl can't send 0 char, its probably null
#		print "!$tmp|"; 
		$Arduino->communicate(chr($tmp)) or die 'Warning, empty string: ', "$!\n";
		$count++;
	last if ($count >= $audio_len); # {die "Fatal: arduino asked more than it should";}	
	}	

	last if ($count == $audio_len);
}

print "DONE\n";
