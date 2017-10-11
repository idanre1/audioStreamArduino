/*
 Streaming Audio
 
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
 Matthew solution was only half cooked, had some minor bugs, mainly program memory footprint was bad.
 For some reason the implementation was not ready for infinite playing.
 Also no PC end SW was introduced.
 
 StreamingAudio overcomes these limitations by dynamically 
 sending audio samples to the Arduino via Serial.
 
 It uses a pingpong buffer for the audio samples, using my own
 exprience two 64KB buffer is enough when using 115200 baud.
 Two variables exists to tweak this:
 BUFFER_SIZE, TRANSFER_SIZE.

 Allocated resources
 -------------------
 TIMER 0 (8  bit): FREE. Can be uses to PWM pins 5,6
 TIMER 1 (16 bit): Used to send sample every irq (8000hz) - Disables PWM on pins 9,10
 TIMER 2 (8  bit): Used to PWM on the speaker pin         - Disables PWM on pins 11,3
 
 
 Remote Host
 -----------
 1.  After serial init wait for GO command. This is important since
     sometimes the uart buffer has residues from prev operation

 2.  Send arduino command opcode.

 3.  If the command is TRANSFER_SIZE, remote is asking arduino for its supported TRANSFET_SIZE
     TRANSFER_SIZE is the best chunk of data arduino is able to process in one opcode.

 4.  Each time the host send play command opcode it sends the next
     TRANSFER_SIZE audio samples to fill the Arduino's receive pingpong buffer.
 
 */

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <SoftwareSerial.h>

#define SAMPLE_RATE 8000
#define BUFFER_SIZE 128
#define TRANSFER_SIZE 64
//#define SERIAL_BUFFER_SIZE 512 // ToDo patch in HardwareSerial.cpp inside /usr/share/arduino/hardware/arduino/cores/arduino/

void startPlayback();
void stopPlayback();
void captureByte();
void reset();

unsigned char sounddata_data[BUFFER_SIZE]; // ping pong buffer
unsigned char serial_read;                 // opcode helper
int BufferHead=0;              // ping pong buffer ptr
int BufferTail=0;              // ping pong buffer ptr

unsigned long sample=0;        // How many bytes samples to speaker
unsigned long BytesReceived=0; // How many bytes received in serial

int Playing = 0;               // Indication if arduino currently playing
//int postData = 0;

int ledPin = 13;
int speakerPin = 11;
int motorPin = 8;

//Interrupt Service Routine (ISR)
// This is called at 8000 Hz to load the next sample.
ISR(TIMER1_COMPA_vect) {
    //If not at the end of audio
    if (sample < BytesReceived) {
        //Set the PWM Freq.
        OCR2A = sounddata_data[BufferTail];
        //If circular buffer is not empty
        if (BufferTail != BufferHead) {
        //Increment Buffer's tail index.
	    if (++BufferTail >= BUFFER_SIZE) BufferTail = 0; //BufferTail = ((BufferTail+1) % BUFFER_SIZE);
            //Increment sample number.
            sample++;
        }//End if
    }//End if
    else //We are at the end of audio
    {
        //Stop playing.
        stopPlayback();
    }//End Else

}//End Interrupt

SoftwareSerial swSerial(10, 12); // RX, TX
void setup() {
    //Set LED for OUTPUT mode
    pinMode(ledPin, OUTPUT);
	pinMode(motorPin, OUTPUT);
	digitalWrite(motorPin, LOW);
    
    //Start Serial port.  If your application can handle a
    //faster baud rate, that would increase your bandwidth
    //115200 only allows for 14,400 Bytes/sec.  Audio will
    //require 8000 bytes / sec to play at the correct speed.
    //This only leaves 44% of the time free for processing 
    //bytes.
    swSerial.begin(115200);
    swSerial.println("GO"); // Tell streamer arduino has started
//    swSerial.println("?");  // Tell streamer arduino waiting for command

}//End Setup

void loop() {
  //If audio not started yet...
  if (Playing == 0) {
  	//Check to see if the first 1000 bytes are buffered.
  	if ((BytesReceived-sample) >= (TRANSFER_SIZE)) {
//		swSerial.println("YAY");
    		startPlayback();
  	}//End if
  }//End if
  
  //While the serial port buffer has data
  while (swSerial.available()>0) {

    //If the sample buffer isn't full    
    if (((BufferHead+1) % BUFFER_SIZE) != BufferTail) {

    //if the Serial port starting new buffer
//---------------------------------------
// Command portion
//---------------------------------------
    if ((BytesReceived % TRANSFER_SIZE) == 0) {
		if (sample > (16 * BUFFER_SIZE)) {
			// Handling infinite streaming. reduce counters
			BytesReceived -= 8 * BUFFER_SIZE;
			sample -= 8 * BUFFER_SIZE;
		}

	serial_read = swSerial.read(); 
	switch (serial_read) {
	case 'p': // 'p' for Play buffer
		// next TRANSFER_SIZE bytes will be operated as data
		// TRANSFER_SIZE start with command opcode.
		// If special thing needs to be done it will be presented
		// in this case. else its just continue playing the buffer
		// keep in mind TRANSFER_SIZE is only the "neto" size without the opcode.
//		postData = 1;
		while (swSerial.available() == 0) { } // make sure first byte is arrived
		captureByte();
		break;
	case 't': // 't' for TRANSFER_SIZE. tell streamer what is the supported TRANSFER_EIZE
		swSerial.print(uint8_t(TRANSFER_SIZE));
		swSerial.println("ACKt");
		break;
	case 's': // 's' for stop playing
		stopPlayback();
		swSerial.println("ACKs");
		break;
	case 'i': // 'i' for info
		swSerial.print("BytesReceived:");
		swSerial.println(BytesReceived);
		swSerial.print("sample:");
		swSerial.println(sample);
                swSerial.print("Playing:");
                swSerial.println(Playing);
		swSerial.println("ACKi");
		break;
	case 'm': // 'm' for motor On
		digitalWrite(motorPin, HIGH);
		swSerial.println("ACKm");
		break;
	case 'M': // 'M' for motor Off
		digitalWrite(motorPin, LOW);
		swSerial.println("ACKM");
		break;
	}// switch
	swSerial.println("?"); // Tell streamer arduino waiting for command

    }//End if if the Serial port starting new buffer
//---------------------------------------
// Data portion
//---------------------------------------
    else { // This is the data portion of the transfer
	captureByte();
   }
   }//End if "sample buffer isn't full"
}//End While
}//End Loop

void captureByte() {
	//Store the sample freq.
	sounddata_data[BufferHead] = swSerial.read();
	//Increment the buffer's head index.
	BufferHead = (BufferHead+1) % BUFFER_SIZE;
	//Increment the bytes received
	BytesReceived++;
}

void startPlayback() {
    //Set pin for OUTPUT mode.
    pinMode(speakerPin, OUTPUT);

    //---------------TIMER 2-------------------------------------
    // Set up Timer 2 to do pulse width modulation on the speaker
    // pin.  
    //This plays the music at the frequency of the audio sample.

    // Use internal clock (datasheet p.160) newpdf.p213
    //ASSR = Asynchronous Status Register
    ASSR &= ~(_BV(EXCLK) | _BV(AS2));

    // Set fast PWM mode  (p.157)
    //Timer/Counter Control Register A/B for Timer 2
    TCCR2A |= _BV(WGM21) | _BV(WGM20);
    TCCR2B &= ~_BV(WGM22);

    // Do non-inverting PWM on pin OC2A (p.155)
    // On the Arduino this is pin 11.
    TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0); //Clear OC2A on Compare Match, set OC2A at BOTTOM (non-inverting mode)
    TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0)); // Normal port operation, OC2B disconnected

    // No prescaler (p.158)
    TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

    //16000000 cycles       1 increment    2000000 increments   (16000000/8=2000000)
    //--------        *  ----            = -------
    //       1 second       8 cycles             1 second

    //Continued...
    //2000000 increments     1 overflow      7812 overflows     (2000000/256=7812) overflows per second
    //-------            * ---            = -----
    //      1 second       256 increments       1 second




    // Set PWM Freq to the sample at the end of the buffer.
    OCR2A = sounddata_data[BufferTail];


    //--------TIMER 1----------------------------------
    // Set up Timer 1 to send a sample every interrupt.
    // This will interrupt at the sample rate (8000 hz)
    //

    cli();

    // Set CTC mode (Clear Timer on Compare Match) (p.133)
    // Have to set OCR1A *after*, otherwise it gets reset to 0!
    TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
    TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

    // No prescaler (p.134)
    TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

    // Set the compare register (OCR1A).
    // OCR1A is a 16-bit register, so we have to do this with
    // interrupts disabled to be safe.
    OCR1A = F_CPU / SAMPLE_RATE;    // 16e6 / 8000 = 2000

    //Timer/Counter Interrupt Mask Register
    // Enable interrupt when TCNT1 == OCR1A (p.136)
    TIMSK1 |= _BV(OCIE1A);


    //Init Sample.  Start from the beginning of audio.
//TODO    sample = 0;
    Playing=1;

    //Enable Interrupts
    sei();  
}//End StartPlayback





void stopPlayback() {
    Playing=0;
    
    // Disable playback per-sample interrupt.
    TIMSK1 &= ~_BV(OCIE1A);

    // Disable the per-sample timer completely.
    TCCR1B &= ~_BV(CS10);

    // Disable the PWM timer.
    TCCR2B &= ~_BV(CS10);

    digitalWrite(speakerPin, LOW);
}//End StopPlayback


