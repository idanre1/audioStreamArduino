# Arduino audio stream host
# Author: Mitchell Augustin, 2024
# For use with Idan Regev's Streaming Audio library (https://github.com/idanre1/audioStreamArduino) (arduinoMaster/audioSerial.ino)
# Note: This code assumes that you have changed BUFFER_SIZE and TRANSFER_SIZE to 512 in audioSerial.ino and the baud rate to 2000000
# (On my device, the audio was unacceptably choppy before raising the throughput in this way. It is still slightly choppy, but acceptable)
# I also speed up my audio by 22% to account for throughput limitations that I believe are a result of my generic Arduino controller.
# Tested on a generic Arduino ATMega328p
import sys
import serial
import wave
import struct
import time

def send_samples(serial_dev, samples):
    serial_dev.write(samples)

def send_num_samples(serial_dev, num_samples):
    # Convert the unsigned long to a 10-byte string
    num_samples_str = str(num_samples).zfill(10)
    # Convert each character to its ASCII value and send
    for digit in num_samples_str:
        serial_dev.write(struct.pack('B', ord(digit)))

def play(wav_path, serial_dev_path='/dev/ttyUSB0', baud_rate=2000000):
    # Open WAV file
    wave_file = wave.open(wav_path, 'rb')

    # Open serial device
    serial_dev = serial.Serial(serial_dev_path, baud_rate, timeout=1)

    try:
        # Get the number of audio frames (samples)
        num_samples = wave_file.getnframes()

        # Send the number of samples
        send_num_samples(serial_dev, num_samples)

        # Send audio samples in chunks of 512
        chunk_size = 512
        for i in range(0, num_samples, chunk_size):
            # Read audio samples
            samples = wave_file.readframes(chunk_size)

            # Send audio samples
            send_samples(serial_dev, samples)

            # Wait for the Arduino to process the data. In my tests, reducing below 0.069 resulted in the Arduino being unable to keep up.
            time.sleep(0.069)

    finally:
        # Close WAV file and serial device
        wave_file.close()
        serial_dev.close()

if __name__ == "__main__":
    # Must be 8000 hz sample rate, 8-bit unsigned mono wav file sped up 22% faster than desired speed
    wav_path = './music/speaking.wav'
    if len(sys.argv) > 1:
        wav_path = sys.argv[1]
        print("Playing: " + wav_path)
    else:
        print("Playing default audio file: " + wav_path)
    play(wav_path)

