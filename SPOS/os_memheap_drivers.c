/*
 * os_memheap_drivers.c
 *
 * Created: 2024-05-29 15:04:43
 *  Author: DoDo
 */ 

#include "os_memheap_drivers.h"
#include "defines.h"
#include "os_memory_strategies.h"
#include <avr/pgmspace.h>
#include "os_mem_drivers.h"


#define MAPSTART 0x0000
#define MAPSIZE 0xFFFF / 3


// Der Heap des internen SRAM
Heap intHeap__ = {
	intSRAM,
	(0x100 + HEAPOFFSET),
	(HEAP_END - (0x100 + HEAPOFFSET)) / 3,
	(0x100 + HEAPOFFSET) + ((HEAP_END - (0x100 + HEAPOFFSET)) / 3),
	2 *((HEAP_END - (0x100 + HEAPOFFSET)) / 3),
	OS_MEM_FIRST,
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	NULL,
	(0x100 + HEAPOFFSET) + ((HEAP_END - (0x100 + HEAPOFFSET)) / 3)
};

//der heap des externen RAM
// Heap extHeap__ = {
// 	extSRAM,
// 	MAPSTART,
// 	MAPSIZE,
// 	MAPSTART + MAPSIZE,
// 	MAPSIZE * 2,
// 	OS_MEM_FIRST,
// 	{0, 0, 0, 0, 0, 0, 0, 0},
// 	{0, 0, 0, 0, 0, 0, 0, 0},
// 	NULL,
// 	MAPSTART + MAPSIZE
// };


//////// Funktionen
void os_initHeap(Heap* heap) {
	
	for (MemAddr i = heap->mapStart; i < heap->mapStart + heap->mapSize; i++) {
		heap->driver->write(i, 0);
	}
	
	//hacky test wegen weirdem edge case (hat eh nix gebracht tbh)
//  		extHeap->driver->write(0x0004, 0);
// 		extHeap->driver->write(0x0005, 0);
}

void os_initHeaps(void) {
	
	// SRAM Initialisieren:
	intSRAM->init();
//	extSRAM->init();
	
	// dann SRAM Nibbles setzen:
	os_initHeap(intHeap);
//	os_initHeap(extHeap);
	
	// und heaps benennen:
	intHeap->name = PSTR("SRAM");
//	extHeap->name = PSTR("XRAM");

}

size_t os_getHeapListLength() {
	return 1;
}

Heap* os_lookupHeap(uint8_t index) {
	if (index == 0) {return intHeap;}
//	if (index == 1) {return extHeap;}
	return NULL; // lol, lmao
}