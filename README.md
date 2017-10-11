# audioStreamArduino
Streaming audio over serial port to arduino

 Coded by Idan Regev

 Adapted from Matthew Vaterlaus http://forum.arduino.cc/index.php?topic=8817.0
 Second Adapted largely from Michael Smith's speaker_pcm.
 <michael@hurts.ca>


 Plays 8-bit PCM audio on pin 11 using pulse-width modulation.
 For Arduino with Atmega at 16 MHz.

 The audio data needs to be unsigned, 8-bit, 8000 Hz.

 Although Smith's speaker_pcm was very well programmed, it
 had two major limitations:  1. The size of program memory only
 allows ~5 seconds of audio at best.  2. To change the adudio,
 the microcontroller would need to be re-programmed with a new
 sounddata.h file.
 Matthew solution better but only half cooked, and had some minor bugs.
 For some reason Matthew implementation was not ready for infinite playing.
 Also remote SW was introduced.
 StreamingAudio overcomes these limitations by dynamically
 sending audio samples to the Arduino via Serial.

 The biggest problem both codes have is that you **cannot use serial to do
 anything else other than playing sound**.
 StreamingAudio also gives the coder a posibility add new commands to arduino to execute during the middle of the run.
 I have used this feature to play music during the time operating a DC motor on/off.

## StreamingAudio implementation
Best implementation, with the ability to send other command using serial and not only streaming music.
Implementation uses a pingpong buffer for the audio samples, using my own
exprience two 64KB buffer is enough when using 115200 baud.
Two variables exists to tweak this:
BUFFER_SIZE, TRANSFER_SIZE.
### Remote Host
 1.  After serial init, wait for GO command. This is important since
     sometimes the serial buffer has residues from prev operation. (called goblins)

 2.  Send arduino command opcode.

 3.  If the command is TRANSFER_SIZE, remote is asking arduino for its supported TRANSFET_SIZE
     TRANSFER_SIZE is the best chunk of data arduino is able to process in one opcode.

 4.  Each time the host send play command opcode it sends the next
     TRANSFER_SIZE audio samples to fill the Arduino's receive pingpong buffer.


## More implementations
I developed this code in stages. I also provide some major implementation which might hele somebody
### arduinoSlaveHW
Almost identical to arduinoSlaveSw except the data is fed from HW serial pins of the arduino instead
of the current implementation contains the data from SW serial.
SW serial implementation gives the ability to send the data using BT hc-05 alike module.

There are also minor bug fixes in the arduinoSlaveSw w.r.t arduinoSlaveHW version.

### arduinoMaster
 Code is more close to Matthew implementation.
 Also remote SW is provided.
 Biggest effort was **auto convert** wav file on the fly.
 __Notice inside music there is a convert.sh file__ that outputs 8bit PCM file from original,
 still in wav file format.
 The play file can handle this wav file completely!
#### Master Is receiving byte using fetch protocol
 1.  After serial init, wait for GO command. This is important since
     sometimes the serial buffer has residues from prev operation. (called goblins)

 2.  Send arduino command opcode.

 3.  If the command is play, send TRANSFET_SIZE from remote to arduino
     TRANSFER_SIZE can be determined using related command opcode.

 4.  Each time the host send command opcode it sends the next
     TRANSFER_SIZE audio samples to fill the Arduino's receive pingpong buffer.
#### Remote Host
 1.  Sends 10 bytes of data representing the number 
     of samples.  Each byte is 1 digit of an unsigned long.
 2.  Each time the host recieves a byte it sends the next
     128 audio samples to fill the Arduino's receive buffer.
### hfile
 Streaming arduino same file as Smith implementation.
 Also remote SW is provided.
  
