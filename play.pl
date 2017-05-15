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
my $filename=$ARGV[0];
print "Process audio file $filename\n";

my $wavInfo = qx /wavInfo $filename/;
#print $wavInfo;
my $header;
my $hdr_size=44;
my $wavLength;

if ($wavInfo =~ /RIFF header: RIFF, length \d*, type WAVE
\n*\s*Format: fmt, length \d*, audio format PCM, channels 1,
\n*\s*sample rate 8000, bytes per second 8000,
\n*\s*bytes per sample 1, bits per sample 8
\n*\s*Junk: length (\d*)
\n*\s*Data:.*length (\d*)/) {
	print "WAV file comply streaming rules!\n";
	$hdr_size+=$1;
	$wavLength = $2;
} else { 
	die "WAV file failed required WAV structure: mono 800Hz 1 Byte unsigned";
}

print "Header size (with garbage): $hdr_size\n";
print "Samples to process: $wavLength\n";
my $fh;
open $fh, "<:raw", $filename or die "Couldn't open $filename!";
my $oneByte;
my $u_int8;
read($fh, $header, $hdr_size) or die "Error reading $filename!";
#print "$header\n";

print "Play file: $filename\n";

# ------------------------------
# Init arduino
# ------------------------------
print "Waiting for arduino to setup serial\n";
while (1) {
	$header = $Arduino->receive();
	print "ARD:$header\n";
	last if ($header =~ /GO/);
}

# Prepare 10bit long int
my $audio_len = $wavLength;
if (length($audio_len) > 10) { die "Audio length does not fit to 10 bit: " . length($audio_len); }
my $charTen = "0" x (10-length($audio_len)) . $audio_len;

print "Sending audio len: " . $charTen . "\n";
while (1) {
	$Arduino->communicate($charTen) or die 'Warning, empty string: ', "$!\n";
	$header = $Arduino->receive();
	print "ARD:$header\n";
	last if ($header =~ /$audio_len/);
}

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
		read($fh, $oneByte, 1) or die "Error reading $filename!";
		$u_int8 = unpack 'C', $oneByte;
		$tmp=$u_int8+0;
		$tmp = 49 if ($tmp == 48);## PATCH ## Perl can't send 0 char, its probably null
#		print "!$tmp|"; 
		$Arduino->communicate(chr($tmp)) or die 'Warning, empty string: ', "$!\n";
		$count++;
	if ($count > $audio_len) {die "Fatal: arduino asked more than it should";}	
	}	

	last if ($count == $audio_len);
}

print "DONE\n";
