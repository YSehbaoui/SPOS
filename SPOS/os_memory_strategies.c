/*
 * os_memory_strategies.c
 *
 * Created: 2024-05-29 15:05:45
 *  Author: DoDo
 */ 

#include "os_memory_strategies.h"
#include "os_memory.h"
#include "os_core.h"

//Der Vorgeschlagene Fix:
// for(MemAddr offset = 0; offset < heap->useSize; i++) {
//
// 	if (os_getMapEntry(heap, heap->mapStart + offset) == 0) {fitSize = fitSize + 1;}
// 	else {fitSize = 0;}
//
// 	if (fitSize == size) {return (heap->mapStart + offset + 1) - size;}
// }

MemAddr os_Memory_FirstFit(Heap* heap, size_t size) {
	
	size_t fitSize = 0;
	
	// 	for(MemAddr i = heap->useStart; (i - heap->useStart) / 2 + heap->mapStart < heap->useSize; i++) {
	//
	// 		if (os_getMapEntry(heap, i) == 0) {fitSize = fitSize + 1;}
	// 		else {fitSize = 0;}
	//
	// 		if (fitSize == size) {return (i + 1)- size;}
	// 	}

	for(volatile MemAddr  offset = 0; offset < heap->useSize; offset++) {

		if (os_getMapEntry(heap, (heap->useStart + offset)) == 0) 
		{fitSize = fitSize + 1;}
		else {fitSize = 0;}

		if (fitSize == size) 
		{
			if ( ((heap->useStart + (offset + 1 - size)) < heap->useStart) )
			{
				os_error("Door Stuck");
			}
			return (heap->useStart + (offset + 1 - size));}
	}

	
	return 0;

}

MemAddr os_Memory_NextFit(Heap* heap, size_t size) {
	
	size_t fitSize = size;
	
	for(MemAddr i = heap->last_fit; (i - heap->useStart) / 2 + heap->mapStart < (heap->mapStart + heap->mapSize); i++) {
		
		if (os_getMapEntry(heap, i) == 0) {fitSize--;}
		else {fitSize = size;}
		if (fitSize == 0){heap->last_fit = i+1; return (i-size)+1;}
	}
	
	fitSize = size;
	MemAddr temp = os_Memory_FirstFit(heap,size);
	if (temp != 0) {heap->last_fit = temp+size;}
	
	return temp;
	
}

MemAddr os_Memory_BestFit(Heap* heap, size_t size) {
	
	// die größe der aktuellen lücke
	size_t fitSize = 0;
	// kommentar für fehlersuche: bestSize soll sehr groß sein, also versuchen wir bitweise negation um es 111111111111 ganz viele zu machen
	size_t bestSize = 0xFFFF;
	// die adresse unserer bisher besten stelle
	MemAddr bestFit = 0;
	
	// ein mal über den ganzen use-space iterieren:
	for(MemAddr i = heap->useStart; (i - heap->useStart) / 2 + heap->mapStart < (heap->mapStart + heap->mapSize); i++) {
		
		if (os_getMapEntry(heap, i) == 0) {
			fitSize = fitSize + 1;
		}
		else {
			if(fitSize == size) {
				return i-size;
			}	
			else if(bestSize > fitSize && fitSize > size){
				bestSize = fitSize;
				bestFit = i - fitSize;
			}
			
			fitSize = 0;
		}
		

	}
	
	if(bestSize > fitSize && fitSize >= size){
		bestSize = fitSize;
		bestFit = heap->useStart + (heap->useSize - fitSize);
	}
	

	
	return bestFit;

}

MemAddr os_Memory_WorstFit(Heap* heap, size_t size) {
	
	// die größe der aktuellen lücke
	size_t fitSize = 0;
	// bestSize startet hier 0 und beschreibt größte bisherige lücke
	size_t worstSize = 0;
	// die adresse unserer bisher besten stelle
	MemAddr worstFit = 0;
	
	// ein mal über den ganzen use-space iterieren:
	for(MemAddr i = heap->useStart; (i - heap->useStart) / 2 + heap->mapStart < (heap->mapStart + heap->mapSize); i++) {
		
		if(worstSize < fitSize && fitSize >= size){
			worstSize = fitSize;
			worstFit = i - fitSize;
		}
		
		if (os_getMapEntry(heap, i) == 0) {
			fitSize = fitSize + 1;
		}
		else {
			fitSize = 0;
		}
		
	}
	
	if(worstSize < fitSize && fitSize >= size){
		worstSize = fitSize;
		worstFit = heap->useStart + (heap->useSize - fitSize);
	}
	
	return worstFit;

}

