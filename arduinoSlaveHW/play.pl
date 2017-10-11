#!/usr/bin/perl -w

use strict;
use warnings;

use lib qw(/nas/scripts/perl_lib);

use SerialArduino;
use Data::Dumper; 

# ------------------------------
# Config
# ------------------------------
my $com = '/dev/ttyUSB0';
#my $com = '/dev/rfcomm0';


# ------------------------------
# Inits
# ------------------------------
my @audio;

my $Arduino = SerialArduino->new(
  port     => "$com",
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

my $audio_len = $wavLength;
my $TRANSFER_SIZE;
print "Asking for TRANSFER_SIZE\n";
while (1) {
	$Arduino->communicate('t') or die 'Warning, empty string: ', "$!\n";
	$header = $Arduino->receive();
	print "ARD:$header\n";
	if ($header =~ /(\d+)ACK/) {
		$TRANSFER_SIZE=$1;
		last;
	}
#	sleep(1);
}
print "TRANSFER_SIZE: $TRANSFER_SIZE\n";

print "Entering loop...\n";
my $count=0;
my $m=0;
my $j=0;
my $tmp;
LOOP:while(1) {
#print "LOOP\n";
#	print "Read... " . ($count) . "\n";
#print "$m\n";
	if ($m > 10000 && $m < 10064) {
	# Turn on motor during specific interval
		$tmp = $Arduino->receive();
		unless ($tmp =~ /\?/) {
			print "?m Warning, arduino protocol violation: $tmp\n";
			next LOOP;
		} else { print "?m:$tmp\n"; }

		$Arduino->communicate('m') or die 'Warning, empty string: ', "$!\n";
		$tmp = $Arduino->receive();
		unless ($tmp =~ /ACK(M|m)/) {
			print "!m Warning, arduino protocol violation: $tmp\n";
			next LOOP;
		} else { print "!m:$tmp\n"; }
	} elsif ($m > 19900) {
	# Turn off motor during specific interval
		$tmp = $Arduino->receive();
		unless ($tmp =~ /\?/) {
			print "?M Warning, arduino protocol violation: $tmp\n";
			next LOOP;
		} #else { print "?M:$tmp\n"; }
		$Arduino->communicate('M') or die 'Warning, empty string: ', "$!\n";
		$tmp = $Arduino->receive();
		unless ($tmp =~ /ACK(M|m)/) {
			print "!M Warning, arduino protocol violation: $tmp\n";
			next LOOP;
		} #else { print "!M:$tmp\n"; }
	}

	$tmp = $Arduino->receive();
	unless ($tmp =~ /\?/) {
		print "? Warning, arduino protocol violation: $tmp\n";
		next LOOP;
	} #else { print "ARD?:$tmp\n"; }
	$Arduino->communicate('p') or die 'Warning, empty string: ', "$!\n";
	foreach my $i (1..$TRANSFER_SIZE) {

		if ($count >= $audio_len) {
		# Send zeroes to silence residue
			$tmp=0;
		} else {
		#Still play
			read($fh, $oneByte, 1) or die "Error reading $filename! count $count, len $audio_len";
			$u_int8 = unpack 'C', $oneByte;
			$tmp=$u_int8+0;
			$tmp = 49 if ($tmp == 48);## PATCH ## Perl can't send 0 char, its probably null
		}
#		print "!$tmp|"; 
		$Arduino->communicate(chr($tmp)) or die 'Warning, empty string: ', "$!\n";
		$count++;
		if ($m > 20000) { $m=0; } else { $m++; }
	} # foreach	
        last if ($count > $audio_len); # Arduino don't knows when it will be done. perl should stop
#	print "count $count, len $audio_len\n";
#		last if ($count > 128);
} # While


print "info1\n";
&info();
sleep(3);
print "info2\n";
&info();
sleep(3);
print "info3\n";
&info;

$Arduino->communicate('s') or die 'Warning, empty string: ', "$!\n";
$header = $Arduino->receive();

print "DONE\n";


sub info {
	$Arduino->communicate('i') or die 'Warning, empty string: ', "$!\n";
	while (1) {
		$header = $Arduino->receive();
		print "$header\n";
		last if ($header =~ /ACK/);
	}
}
