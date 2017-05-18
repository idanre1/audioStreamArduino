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
 
 StreamingAudio overcomes these limitations by dynamically 
 sending audio samples to the Arduino via Serial.
 
 It uses a 1k circular buffer for the audio samples, since
 the ATMEGA328 only has 2k of RAM.  For chips with less RAM,
 the BUFFER_SIZE variable can be reduced.
 
 The only limit on length is the number of samples must fit
 into an long integer.  (ie:  4,294,967,295 samples).
 At 8000 samples / second, that allows 8,947 minutes of audio.
 Even this could be overcome if needed.  (The only reason to
 have the number of samples is to know when to turn the speaker
 off.)
 
 
 
 Remote Host
 -----------
 1.  Sends 10 bytes of data representing the number 
     of samples.  Each byte is 1 digit of an unsigned long.
 
 
 2.  Each time the host recieves a byte it sends the next
     128 audio samples to fill the Arduino's receive buffer.
 
 */

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#define SAMPLE_RATE 8000
#define BUFFER_SIZE 128
#define TRANSFER_SIZE 64
//#define SERIAL_BUFFER_SIZE 512 // ToDo patch in HardwareSerial.cpp inside /usr/share/arduino/hardware/arduino/cores/arduino/

void startPlayback();
void stopPlayback();
long powlong(long x, long y);
void reset();


unsigned long sounddata_length=0;
unsigned char sounddata_data[BUFFER_SIZE];
int BufferHead=0;
int HalfBufferSize=BUFFER_SIZE/2;
int BufferTail=0;
unsigned long sample=0;
unsigned long BytesReceived=0;

unsigned long Temp=0;
unsigned long NewTemp=0;

int ledPin = 13;
int speakerPin = 11;
int Playing = 0;

//Interrupt Service Routine (ISR)
// This is called at 8000 Hz to load the next sample.
ISR(TIMER1_COMPA_vect) 
{
    //If not at the end of audio
    if (sample < sounddata_length)   
    {
        //Set the PWM Freq.
        OCR2A = sounddata_data[BufferTail];
        //If circular buffer is not empty
        if (BufferTail != BufferHead)  
        {
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

void startPlayback()
{
    //Set pin for OUTPUT mode.
    pinMode(speakerPin, OUTPUT);

    //---------------TIMER 2-------------------------------------
    // Set up Timer 2 to do pulse width modulation on the speaker
    // pin.  
    //This plays the music at the frequency of the audio sample.

    // Use internal clock (datasheet p.160)
    //ASSR = Asynchronous Status Register
    ASSR &= ~(_BV(EXCLK) | _BV(AS2));

    // Set fast PWM mode  (p.157)
    //Timer/Counter Control Register A/B for Timer 2
    TCCR2A |= _BV(WGM21) | _BV(WGM20);
    TCCR2B &= ~_BV(WGM22);

		// Do non-inverting PWM on pin OC2A (p.155)
    // On the Arduino this is pin 11.
    TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0);
    TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0));

    // No prescaler (p.158)
    TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

    //16000000 cycles       1 increment    2000000 increments
    //--------        *  ----            = -------
    //       1 second       8 cycles             1 second

    //Continued...
    //2000000 increments     1 overflow      7812 overflows
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
    sample = 0;
    
    //Enable Interrupts
    sei();  
}//End StartPlayback





void stopPlayback()
{
    // Disable playback per-sample interrupt.
    TIMSK1 &= ~_BV(OCIE1A);

    // Disable the per-sample timer completely.
    TCCR1B &= ~_BV(CS10);

    // Disable the PWM timer.
    TCCR2B &= ~_BV(CS10);

    digitalWrite(speakerPin, LOW);
}//End StopPlayback



    //Use the custom powlong() function because the standard
    //pow() function uses floats and has rounding errors.
    //This powlong() function does only integer powers.
    //Be careful not to use powers that are too large, otherwise
    //this function could take a really long time.
long powlong(long x, long y)
{
  //Base case for recursion
  if (y==0)
  {
    return(1);
  }//End if
  else
  {
    //Do recursive call.
    return(powlong(x,y-1)*x);
  }//End Else
}



void setup()
{
    //Set LED for OUTPUT mode
    pinMode(ledPin, OUTPUT);
    
    //Start Serial port.  If your application can handle a
    //faster baud rate, that would increase your bandwidth
    //115200 only allows for 14,400 Bytes/sec.  Audio will
    //require 8000 bytes / sec to play at the correct speed.
    //This only leaves 44% of the time free for processing 
    //bytes.
    Serial.begin(115200);
    Serial.println("GO");

    //PC sends audio length as 10-digit ASCII
    //While audio length hasn't arrived yet
    while (Serial.available()<10)
    {
    //Blink the LED on pin 13.
    digitalWrite(ledPin,!digitalRead(ledPin));
    delay(100);
    }
    digitalWrite(ledPin,1);
    
    //Init number of audio samples.
    sounddata_length=0;
    
    //Convert 10 ASCII digits to an unsigned long.
    for (int i=0;i<10;i++)
    {
    //Convert from ASCII to int
    Temp=Serial.read()-48; 
    
    //Shift the digit the correct location.
    NewTemp = Temp * powlong(10,9-i);  
    //Add the current digit to the total.
    sounddata_length = sounddata_length + NewTemp;
    }//End for

    //Tell the remote PC/device that the Arduino is ready
    //to begin receiving samples.
    Serial.println(sounddata_length);
    Serial.println(uint8_t(TRANSFER_SIZE));
       
    //There's data now, so start playing.
    //startPlayback();
    Playing =0;
}//End Setup

void loop()
{
  //If audio not started yet...
  if (Playing == 0)
  {
  //Check to see if the first 1000 bytes are buffered.
  if (BufferHead >= HalfBufferSize)
  {
    Playing=1;
    startPlayback();
  }//End if
  }//End if
  
  //While the serial port buffer has data
  while (Serial.available()>0) 
  {
    //If the sample buffer isn't full    
    if (((BufferHead+1) % BUFFER_SIZE) != BufferTail)
    {
    //Store the sample freq.
    sounddata_data[BufferHead] = Serial.read();
    //Increment the buffer's head index.
    BufferHead = (BufferHead+1) % BUFFER_SIZE;
    //Increment the bytes received
    BytesReceived++;
    }//End if
    
    //if the Serial port buffer has room
    if ((BytesReceived % TRANSFER_SIZE) == 0) {
      //Tell the remote PC how much bytes you want.
	  if ((sounddata_length - BytesReceived) < TRANSFER_SIZE) { 
		  Serial.println(uint8_t(sounddata_length - BytesReceived));
	  }
	  else {
		  Serial.println(uint8_t(TRANSFER_SIZE));
	  }
      // Serial.print("!");
      // Serial.println(sounddata_data[BufferHead]);
	  //Serial.println("@");
    }//End if
  }//End While
  
 

}//End Loop

