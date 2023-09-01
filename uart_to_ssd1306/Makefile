PORT=/dev/ttyUSB0
CFLAGS=-O3 -DF_CPU=16000000UL -mmcu=atmega328p

.PHONY: all upload monitor

all: flash upload

flash: main.c error_img.h
	avr-gcc $(CFLAGS) main.c -o flash

upload: flash
	avrdude -p m328p -c arduino -P $(PORT) -b 115200 -U flash

picocom:
	picocom $(PORT) -b 666666 -s cat --imap lfcrlf

clean:
	rm -f flash
