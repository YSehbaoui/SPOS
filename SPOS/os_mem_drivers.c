/*
 * os_mem_drivers.c
 *
 * Created: 2024-05-29 13:57:27
 *  Author: DoDo
 */ 

#include "os_mem_drivers.h"
#include "defines.h"
#include "os_spi.h"
#include "os_scheduler.h"



// Okay, also, das Struct hat Pointer auf die Funktionen die Wrapper fuer die eigentlichen Funktionen sind. Cool.

//^"Allerdings müssen Anwendungsprogramme direkt auf die Schreib- und Lesefunktionen zugreifen können, 
//sodass der Zeiger auf die Schreibfunktion zwingend den Bezeichner write haben muss. 
//Analog muss der Bezeichner für die Lesefunktion read lauten."


// Private Funktionen damit der Treiber seinen Existenzgrund hat.
//
// INTERNE SRAM FUNKTIONEN
//
//

void initSRAM_internal (void) {
	
}

MemValue readSRAM_internal (MemAddr addr) {
	
	MemValue content = *((MemValue*) addr);
	return content;
	
}

void writeSRAM_internal (MemAddr addr, MemValue value) {
	
	*((MemValue*) addr) = value;
	
}




//
//
//	externe SRAM funktionen:
//
//

void deselect_memory(){
	PORTB |= 0b00010000;
}

void select_memory(){
	PORTB &= 0b11101111;
}

void set_operation_mode(uint8_t mode){
	
	//critical weil wichtig
	os_enterCriticalSection();
	
	//memory CS bit 0 setzen, sodass der slave aktiv wird
	select_memory();
	
	os_spi_send(EXT_WRMR);
	os_spi_send(mode);
	
	
	//slave abschalten
	deselect_memory();
	
	//crit verlassen
	os_leaveCriticalSection();
	
}

void transfer_address(MemAddr addr){
	
	//wir nutzen nur 16 bit von den 24 die die Adressdarstellung haben möchte, also senden wir:
	//8 nullen:
	os_spi_send(0x00);
	//dann die high 8 bit der 16 bit adresse:
	os_spi_send((uint8_t) (addr >> 8));
	//dann die low 8 bit der 16 bit adresse:
	os_spi_send((uint8_t) addr);
	
}

void initSRAM_external (void) {
	
	//init funktion aufrufen
	os_spi_init();
	// CS bit 1en, was in init schon passiert ist, aber doppelt hält besser:
	deselect_memory();
	
	
	
	//Zugriffsmodus auf bitweise einstellen (erste 2 bits 0 setzen, alles andere sind eh 0en, also lesen wir hier nicht nochmal spezifisch aus):
	set_operation_mode(0x00);
}

MemValue readSRAM_external (MemAddr addr) {
	
	//critical weil wichtig
	os_enterCriticalSection();
	
	//memory CS bit 0 setzen, sodass der slave aktiv wird
	select_memory();
	
	// read instruction senden
	os_spi_send(EXT_READ);
	
	// adresse von 24 bit senden (die 16 bit adresse wird in der hilfsfunktion umgebaut)
	transfer_address(addr);
	
	// dann bekommen wir unsere 8 bits übergeben
	MemValue return_value = os_spi_receive();
	
	// slave abschalten
	deselect_memory();
	
	// crit verlassen
	os_leaveCriticalSection();
	
	// als letztes returnen wir den gelesenen wert:
	return return_value;
	
}

void writeSRAM_external (MemAddr addr, MemValue value) {
	
	//critical weil wichtig
	os_enterCriticalSection();
		
	//memory CS bit 0 setzen, sodass der slave aktiv wird
	select_memory();
		
	// write instruction senden
	os_spi_send(EXT_WRITE);
		
	// adresse von 24 bit senden (die 16 bit adresse wird in der hilfsfunktion umgebaut)
	transfer_address(addr);
		
	// dann senden wir unsere 8 bit
	os_spi_send(value);
		
	// slave abschalten
	deselect_memory();
		
	// crit verlassen
	os_leaveCriticalSection();
	
}

// Die Funktion mag zwar gerade komplett redundant sein, aber die ist im Doxygen und ich versuch mich so gut es geht daran zu halten.
void initMemoryDevices (void) {
	initSRAM_internal();
//	initSRAM_external();
}



// Die konkrete Instanz des Speichertreibers nach Vorgabe
MemDriver intSRAM__ = {
	(0x100 + HEAPOFFSET),
	(HEAP_END - (0x100 + HEAPOFFSET)),
	initSRAM_internal,
	readSRAM_internal,
	writeSRAM_internal,
};

// MemDriver extSRAM__ = {
// 	(0x0004),
// 	(0xFFFC),
// 	initSRAM_external,
// 	readSRAM_external,
// 	writeSRAM_external,
// };