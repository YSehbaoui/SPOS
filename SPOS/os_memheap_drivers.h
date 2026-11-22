/*
 * os_memheap_drivers.h
 *
 * Created: 2024-05-29 15:03:07
 *  Author: DoDo
 */ 


#ifndef OS_MEMHEAP_DRIVERS_H_
#define OS_MEMHEAP_DRIVERS_H_

#include "os_mem_drivers.h"
#include <stddef.h>

// Die enum die analog zu Scheduling funktioniert
typedef enum AllocStrategy {
	OS_MEM_FIRST,
	OS_MEM_NEXT,
	OS_MEM_BEST,
	OS_MEM_WORST
	
} AllocStrategy;


// Die Heaptreiberstruktur
typedef struct Heap
{
	MemDriver* driver;
	const MemAddr mapStart;
	const uint16_t mapSize;
	const MemAddr useStart;
	const uint16_t useSize;
	AllocStrategy strategy;
	uint16_t allocFrameStart[MAX_NUMBER_OF_PROCESSES];
	uint16_t allocFrameEnd[MAX_NUMBER_OF_PROCESSES];
	const char* name;
	MemAddr last_fit;
} Heap;

extern Heap intHeap__;
// extern Heap extHeap__;

#define intHeap (&intHeap__)
// #define extHeap (&extHeap__)

//////// Funktionen
void os_initHeaps(void);

size_t os_getHeapListLength(void);

Heap* os_lookupHeap(uint8_t index);

#endif /* OS_MEMHEAP_DRIVERS_H_ */