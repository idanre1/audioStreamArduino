#!/bin/sh

echo "Converting files to 8bit unsigned PCM WAV"

ORIG=/nas/music/*
echo "Orignal files are located at $ORIG"

for file in $ORIG
do
	FILE=`basename $file`
	if [ ! -f $FILE.wav ]; then
		echo "*** Processing $FILE"
		ffmpeg -n -i $file -ar 8000 -acodec pcm_u8 -ac 1 $FILE.wav
	else	
		echo "*** $FILE exists"
	fi
done
