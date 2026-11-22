/*
 * os_spi.c
 *
 * Created: 14.06.2024 15:31:38
 *  Author: Ich
 */ 

#include "os_spi.h"

void os_spi_init(){
	

	//CS pin als output setzen, deselected wird es in der heap init (PORTB bit 4 auf 1 gesetzt, um mit der high flank im output den slave nicht mehr anzusprechen):
	DDRB |= 0b00010000;
	
	//MOSI pin als output (master out), hierüber (bit 3 in portb) werden unsere daten übertragen:
	DDRB |= 0b00100000;
	
	//CLK als ausgang setzen, hierüber takten wir bei der übertragung (sowohl beim empfangen als auch beim senden):
	DDRB |= 0b10000000;
	
	//MISO pin als eingang setzen, und pullup widerstand aktivieren:
	DDRB &= 0b10111111;
	PORTB |= 0b01000000;
	

	
	//aktivieren des moduls und AVR als master konfigurieren:
	SPCR = 0b01010000;
	
	//SPIE wird nicht verwendet, wir nullen also den interrupt:
	SPCR &= 0b01111111;
	
	//Data order: 3tes bit auf 0 -> MSB zuerst
	SPCR &= 0b11011111;
	
	//CPOL und CPHA auf 0 -> IDLE low, ACTIVE high, leading edge
	SPCR &= 0b11110011;
		
	//Clock Speeds einstellen:
	SPCR &= 0b11111100;
	SPSR |= 0b00000001;

}

uint8_t os_spi_send(uint8_t data){
	
// 	os_enterCriticalSection();
// 	
// 	SPDR = data;
// 	while((SPSR >> 7) != 1);
// 	
// 	os_leaveCriticalSection();
// 	return SPDR;
	
	//Atomarisierter Funktionswrapper:
	uint8_t globalInterruptEnableBit = (SREG >> 7);
	
	// 2. Deaktivieren des GIEB
	SREG &= 0x7F; // Ja, ich hab das extra konvertiert um Platz zu sparen.

	uint8_t sregStorage = SREG;
	SREG &= 0b01111111;
	
	SPDR = data;
	while((SPSR >> 7) != 1);
	
	SREG = sregStorage;
	
	// 5. Wiederherstellen des GIEB
	SREG |= (globalInterruptEnableBit << 7);
	
	return SPDR;
}

uint8_t os_spi_receive(){
	
	return os_spi_send(0xFF);
	
}