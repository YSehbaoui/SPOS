/*
 * os_memory.h
 *
 * Created: 2024-05-29 15:34:07
 *  Author: DoDo
 */ 


#ifndef OS_MEMORY_H_
#define OS_MEMORY_H_

#include "os_mem_drivers.h"
#include "os_memheap_drivers.h"
#include "os_scheduler.h"

//////// Funktionen


bool os_sh_isOpen(const Heap* heap, const MemAddr* ptr);

// Opens a chunk of shared memory to prepare a reading process
MemAddr os_sh_readOpen(Heap const* heap, MemAddr const *ptr);

// Opens a chunk of shared memory to prepare a writing process
MemAddr os_sh_writeOpen(Heap const* heap, MemAddr const *ptr);

// Closes a chunk of shared memory to end an arbitrary access
void os_sh_close(Heap const* heap, MemAddr addr);

// Function used to write onto shared memory
void os_sh_read(Heap const* heap, MemAddr const* ptr, uint16_t offset, MemValue* dataDest, uint16_t length);

// Function used to write onto shared memory
void os_sh_write(Heap const* heap, MemAddr const* ptr, uint16_t offset, MemValue const* dataSrc, uint16_t length);

// malloc owner als hilfsfunktion mit der kompletten bisherigen malloc drin, sodass wir die aus malloc und sh_malloc callen können:
MemAddr os_mallocOwner(Heap *heap, size_t size, ProcessID pid);

// Function used to allocate private memory.
MemAddr os_malloc(Heap *heap, size_t size);

// Function used by processes to free their own allocated memory.
void os_free(Heap *heap, MemAddr addr);

// Function used to allocate shared memory.
MemAddr os_sh_malloc(Heap *heap, size_t size);

// Function used by processes to free shared memory.
void os_sh_free(Heap *heap, MemAddr* addr);

// Frees the chunk if and only if it is owned by the given owner.
void os_freeOwnerRestricted(Heap *heap, MemAddr addr, ProcessID owner);

// Function used to get the value of a single map entry, this is made public so the allocation strategies can use it.
MemValue os_getMapEntry(const Heap *heap, MemAddr addr);

// Function that realizes the garbage collection.
void os_freeProcessMemory(Heap *heap, ProcessID pid);

// Get the size of the heap-map.
size_t os_getMapSize(const Heap *heap);

// Get the size of the usable heap.
size_t os_getUseSize(const Heap *heap);

// Get the start of the heap-map.
MemAddr os_getMapStart(const Heap *heap);

//Get the start of the usable heap.
MemAddr os_getUseStart(const Heap *heap);

// Get the size of a chunk on a given address.
uint16_t os_getChunkSize(const Heap *heap, MemAddr addr);

// 	Get the address of the first byte of chunk.
MemAddr os_getFirstByteOfChunk(const Heap *heap, MemAddr addr);

// Changes the memory management strategy.
void os_setAllocationStrategy(Heap *heap, AllocStrategy allocStrat);

// Returns the current memory management strategy.
AllocStrategy os_getAllocationStrategy(const Heap *heap);

MemAddr os_realloc(Heap* heap, MemAddr addr, uint16_t size);

// Bonus helper
MemAddr chunkFinder(Heap* heap, size_t size);

void setMapEntry(const Heap *heap, MemAddr addr, MemValue value);

ProcessID getOwnerOfChunk(const Heap *heap, MemAddr addr);

#endif /* OS_MEMORY_H_ */