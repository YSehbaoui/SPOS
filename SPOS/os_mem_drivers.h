/*
 * os_mem_drivers.h
 *
 * Created: 2024-05-29 13:56:43
 *  Author: DoDo
 */ 


#ifndef OS_MEM_DRIVERS_H_
#define OS_MEM_DRIVERS_H_

#include "defines.h"
#include <inttypes.h>

// Globale Variablen und Defines

// Realizes a pointer to the MemDriver intSRAM__
#define intSRAM (&intSRAM__)
// #define extSRAM (&extSRAM__)

#define EXT_WRMR 0x01
#define EXT_RDMR 0x05
#define EXT_RSTIO 0xFF
#define EXT_EQIO 0x38
#define EXT_EDIO 0x3B
#define EXT_WRITE 0x02
#define EXT_READ 0x03


// Type used instead of uint8_t* pointers to avoid direct dereferencing
typedef uint16_t MemAddr;

// Type for a single value (used instead of uint8_t to increase readability) 
typedef uint8_t MemValue;

// Die drei grundlegenden Funktionen
// void init(void);
// MemValue read(MemAddr addr);
// void write(MemAddr addr, MemValue value);

// Ein Struct zur Speicherformunabhaengigen Wiederverwendung
typedef struct MemDriver {
	const MemAddr startAddr;
	const uint16_t size;
	void (*init)(void);
	MemValue (*read)(MemAddr addr);
	void (*write)(MemAddr addr, MemValue value);
} MemDriver;

extern MemDriver intSRAM__;
// extern MemDriver extSRAM__;

// Initialize all memory devices
void initMemoryDevices(void);






#endif /* OS_MEM_DRIVERS_H_ */