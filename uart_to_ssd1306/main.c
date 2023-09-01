#ifndef __AVR_ATmega328P__
#define __AVR_ATmega328P__ // to make language server happy
#endif

#include <stddef.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/twi.h>
#include <util/delay.h>

void init_uart() {
	UCSR0A = 1<<U2X0; // double speed
	UCSR0B = 1<<RXEN0 | 1<<TXEN0; // enable rx and tx
	UCSR0C = 1<<UCSZ01 | 1<<UCSZ00; // async, no parity, one stop, 8-bit
	UBRR0 = 2; // 666666 baud
}

void write_uart(char c) {
	while (!(UCSR0A & 1<<UDRE0));
	UDR0 = c;
}

void print_uart(const char* str) {
	while (*str)
		write_uart(*str++);
}

// called on TWI errors when display is dead
void error_final(const char* msg) {
	cli();
	for (;;) {
		print_uart(msg);
		print_uart("\n");
		_delay_ms(10);
	}
}

void error(const char* msg);

char read_uart() {
	while (!(UCSR0A & 1<<RXC0));
	if (UCSR0A & 1<<FE0)
		error("frame error");
	if (UCSR0A & 1<<DOR0)
		error("data overrun");
	if (UCSR0A & 1<<UPE0)
		error("parity error");
	return UDR0;
}

void init_twi() {
	PORTC = 1<<PC4 | 1<<PC5; // enable pullups
	TWBR = 1; // atmega is rated at 400kHz as well as ssd1306
	TWSR = 0; // so 400kHz + 400kHz = 888kHz sounds about right
}

void start_twi(char address) {
	TWCR |= 1<<TWINT | 1<<TWSTA | 1<<TWEN;
	while (!(TWCR & 1<<TWINT));
	if ((TWSR & 0xF8) != TW_START)
		error_final("TWI start failed");

	TWDR = address;
	TWCR = 1<<TWINT | 1<<TWEN;
	while (!(TWCR & 1<<TWINT));
	if ((TWSR & 0xF8) != TW_MT_SLA_ACK)
		error_final("TWI ACK after address failed");
}

void data_twi(char data) {
	TWDR = data;
	TWCR = 1<<TWINT | 1<<TWEN;
	while (!(TWCR & 1<<TWINT));
	if ((TWSR & 0xF8) != TW_MT_DATA_ACK)
		error_final("TWI ACK after data failed");
}

void stop_twi() {
	TWCR = 1<<TWINT | 1<<TWEN | 1<<TWSTO;
}

void send_twi(char address, const unsigned char* data, size_t len) {
	start_twi(address);
	for (size_t i = 0; i < len; i++)
		data_twi(data[i]);
	stop_twi();
}

#define SSD1306_ADDR 0x78

void init_ssd1306() {
	const unsigned char init_sequence[] = {
		0x00, // the rest are commands
		0xAE, // display off
		0xA6, // not inverted
		0xA1, // segment remaping, flip along long side
		0xC8, // scan direction, flip along short side
		0x81, 0x00, // contrast
		0xD5, 0xF0, // clock settings
		0x20, 0x00, // adressing mode 
		0x8D, 0x14, // enable charge pump
		0x21, 0x00, 0x7f, // start and stop column
		0x22, 0x00, 0x07, // start and sto pages
		0xAF, // enable display
	};
	send_twi(SSD1306_ADDR, init_sequence, sizeof(init_sequence));
}

void invert_ssd1306(bool invert) {
	const unsigned char flip_sequence[] ={
		0x00, // the rest are commands
		0xA6 | invert
	};
	send_twi(SSD1306_ADDR, flip_sequence, sizeof(flip_sequence));
}

#include "error_img.h"

// called on regular errors
void error(const char* msg) {
	stop_twi();
	const unsigned char error_sequence[] = {
		0x00, // the rest are commands
		0x21, 0x00, 0x7f,  // resets coordinates 
		0x22, 0x00, 0x07, // 
		0xD5, 0x00, // lower clock, looks more bright
		0x81, 0xFF, // contrast to the max
	};
	send_twi(SSD1306_ADDR, error_sequence, sizeof(error_sequence));

	send_twi(SSD1306_ADDR, error_img, sizeof(error_img));

	bool invert = false;
	for (int i = 0;; i++) {
		print_uart(msg);
		print_uart("\n");

		if (i == 15) {
			i = 0;
			invert_ssd1306(invert = !invert);
		}
		_delay_ms(10);
	}
}

// sets 4 second watchdog interrupt
void init_wdt() {
	cli();
	wdt_reset();
	WDTCSR = 1<<WDCE | 1<<WDE;
	WDTCSR = 1<<WDIE | 1<<WDP3 | 0<<WDP0;
	sei();
}

ISR(WDT_vect) {
	error("timed out");
}

int main() {
	init_uart();
	init_twi();
	init_ssd1306();
	init_wdt();

	// starting one long transmission
	start_twi(SSD1306_ADDR);
	data_twi(0b01000000);

	// drawing test pattern
	for (int i = 0; i < 1024; i++)
		data_twi(i);

	for(;;) {
		data_twi(read_uart());
		wdt_reset();
	}
}
