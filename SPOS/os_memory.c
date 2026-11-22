/*
 * os_memory.c
 *
 * Created: 2024-05-29 15:34:24
 *  Author: DoDo
 */ 

#include "os_memory.h"
#include "os_core.h"
#include "os_memory_strategies.h"
#include "util.h"

// Versuch 5 Neulinge:

// Hilfsfunktion zur Ermittlung des Status eines gemeinsamen Speicherbereichs

// Idee: Solang es mindestens einen Lese-/ Schreibzugriff gibt ist der Owner-Nibble != SH_MEM und somit offen.
bool os_sh_isOpen(const Heap* heap, const MemAddr* ptr) {
	
//	MemAddr realAddr = *ptr;
	uint8_t chunkOwner = getOwnerOfChunk(heap, *ptr);
	
	if (chunkOwner == SH_MEM)
	{return false;}
	else
	{return true;}
}

// Opens a chunk of shared memory to prepare a reading process
MemAddr os_sh_readOpen(const Heap* heap, const MemAddr* ptr) {
	
	os_enterCriticalSection();
	
	uint8_t chunkState = getOwnerOfChunk(heap, *ptr);
	
	// Sanity check (In diesem Fall würden wir auf Prozessspeicher zugreifen):
	if (chunkState < SH_MEM)
	{os_error("NOT SHARED MEMORY"); os_leaveCriticalSection(); return 0;}
	
	// Schleifendurchlauf bis der Prozess lesen darf:
	while(chunkState >= SH_FULL) {
		os_yield();
		chunkState = getOwnerOfChunk(heap, *ptr);
	}
	
	// Sobald wir lesen dürfen tun wir dies und vermerken unseren Zugriff:
	setMapEntry(heap, os_getFirstByteOfChunk(heap, *ptr), chunkState + 1);
	MemAddr ret = *ptr;
	os_leaveCriticalSection();
	
	return ret;
	
}

// Opens a chunk of shared memory to prepare a writing process
MemAddr os_sh_writeOpen(const Heap* heap, const MemAddr* ptr) {
	
	os_enterCriticalSection();
	
	uint8_t chunkState = getOwnerOfChunk(heap, *ptr);
	
	// Sanity check (In diesem Fall würden wir auf Prozessspeicher zugreifen):
	if (chunkState < SH_MEM)
	{os_error("NOT SHARED MEMORY"); os_leaveCriticalSection(); return 0;}
	
	// Geschrieben werden darf erst wenn der Bereich komplett geschlossen ist.
	while(os_sh_isOpen(heap, ptr)) {
		os_yield();
	}
	
	// Sobald wir schreiben dürfen passen wir den Owner-Nibble an.
	setMapEntry(heap, os_getFirstByteOfChunk(heap, *ptr), SH_WRITING);
	MemAddr ret = *ptr;
	
	os_leaveCriticalSection();
	
	return ret;
	
}

// Closes a chunk of shared memory to end an arbitrary access
void os_sh_close(const Heap* heap, MemAddr addr) {
	
	os_enterCriticalSection();
	
	uint8_t chunkState = getOwnerOfChunk(heap, addr);
	
	// Sanity check (In diesem Fall würden wir auf Prozessspeicher zugreifen):
	if (chunkState <= SH_MEM)
	{os_error("NOT OPEN MEMORY");}
	
	// Ein Schreibzugriff wird komplett beendet:
	if (chunkState == SH_WRITING)
	{setMapEntry(heap, os_getFirstByteOfChunk(heap, addr), SH_MEM);}
	
	// Ein Lesezugriff hingegen wird nur dekrementiert:
	else
	{setMapEntry(heap, os_getFirstByteOfChunk(heap, addr), chunkState - 1);}
		
	os_leaveCriticalSection();
}

// Function used to write onto shared memory
void os_sh_read(const Heap* heap, const MemAddr* ptr, uint16_t offset, MemValue* dataDest, uint16_t length) {
	
	MemAddr addr = os_sh_readOpen(heap, ptr);
	
	// Sanity Check: Wir wollen tunlichst keinen Garbage lesen.
	if (offset + length > os_getChunkSize(heap, addr)) {
		os_error("sh_read OoB");
		os_sh_close(heap, addr);
		return;
	}
	
	// Ermittlung der Startadresse (könnte auch einfach nur ein Reassignment sein, aber ich trau mich nicht)
	MemAddr startAddr = os_getFirstByteOfChunk(heap, addr);
	
	// Wir iterieren über die Länge des zu lesenden Bereiches.
	for (uint16_t it = 0; it < length; it++)
	{
		MemValue value = heap->driver->read(startAddr + offset + it);
		
		// TODO: hier (und bei write) nochmal nachfragen
		// dataDest ist immer auf dem intSRAM, also darf man das hoffentlich auch direkt so beschreiben.
		//intHeap->driver->write((MemAddr)dataDest+it, value);
		*(dataDest + it) = value;
	}
	
	// Abschließen nicht vergessen!
	os_sh_close(heap, addr);
}

// Function used to write onto shared memory
void os_sh_write(const Heap* heap, const MemAddr* ptr, uint16_t offset, const MemValue* dataSrc, uint16_t length) {
	
	MemAddr addr = os_sh_writeOpen(heap, ptr);
	
	// Sanity Check: Wir wollen idealerweise keinen Garbage schreiben.
	if (offset + length > os_getChunkSize(heap, addr)) {
		os_error("sh_read OoB");
		os_sh_close(heap, addr);
		return;
	}
	
	// Ermittlung der Startadresse (könnte auch einfach nur ein Reassignment sein, aber ich trau mich nicht)
	MemAddr startAddr = os_getFirstByteOfChunk(heap, addr);
	
	// Wir iterieren über die Länge des zu beschreibenden Bereiches.
	for (uint16_t it = 0; it < length; it++)
	{
		
		//MemValue value = intHeap->driver->read((MemAddr)dataSrc + it);
		MemValue value = *(dataSrc + it);
		
		// dataSrc ist immer auf dem intSRAM, also darf man das hoffentlich auch direkt so beschreiben.
		heap->driver->write(startAddr + offset + it, value);
	}
	
	// Abschließen nicht vergessen!
	os_sh_close(heap, addr);
	
}

// Doxygen-angeglichene Hilfsfunktion zur Bedienung von Shared Memory

MemAddr os_mallocOwner(Heap *heap, size_t size, ProcessID pid){
	

	if (heap->useSize < size) {return 0;}
	
	MemAddr freeAddr = chunkFinder(heap, size);
	
	if (freeAddr == 0) 
	{return 0;}
	
	setMapEntry(heap, freeAddr, pid);
	
	for (uint16_t i = 1; i < size; i++) {
		setMapEntry(heap, freeAddr + i, 0xF);
	}
	
	return freeAddr;
	
}

MemAddr os_malloc(Heap* heap, size_t size) {
	
	os_enterCriticalSection();
	
	MemAddr freeAddr = os_mallocOwner(heap, size, os_getCurrentProc());

	//Der Initialfall:
	if(heap->allocFrameStart[os_getCurrentProc()] == 0 && heap->allocFrameEnd[os_getCurrentProc()] == 0) {
		heap->allocFrameStart[os_getCurrentProc()] = freeAddr;
		heap->allocFrameEnd[os_getCurrentProc()] = freeAddr + (size - 1);
	}
		
	//Die Laufzeitanpassungen:
	if (freeAddr < heap->allocFrameStart[os_getCurrentProc()]) {
		heap->allocFrameStart[os_getCurrentProc()] = freeAddr;
	}
		
	if (freeAddr + (size - 1) > heap->allocFrameEnd[os_getCurrentProc()]) {
		heap->allocFrameEnd[os_getCurrentProc()] = freeAddr + (size - 1);
	}
	
	os_leaveCriticalSection();

	return freeAddr;

}



void os_free(Heap* heap, MemAddr addr) {
	
	os_freeOwnerRestricted(heap, addr, os_getCurrentProc());

}

MemAddr os_sh_malloc(Heap* heap, size_t size){
	
	os_enterCriticalSection();
	
	MemAddr freeAddr = os_mallocOwner(heap, size, SH_MEM);
	
	os_leaveCriticalSection();
	
	return freeAddr;
	
	
}


void os_sh_free(Heap* heap, MemAddr* addr) {
	
	os_enterCriticalSection();
	
	MemAddr tempAddr = os_getFirstByteOfChunk(heap, *(addr));
	MemValue currentNibble = getOwnerOfChunk(heap, tempAddr);
		
	// Check ob der Bereich auch wirklich shared ist. Shared-Nibble-Values fangen bei 0b1000 an.
	if(currentNibble < SH_MEM) {
		os_leaveCriticalSection();
		os_error("Not Shared Memory!");
		return;
	}
	
	// Mit dem eigentlichen Befreien wird gewartet bis der Prozess exklusiven Zugriff hat.
	while(os_sh_isOpen(heap, addr)) {
		os_leaveCriticalSection();
		os_yield();
		os_enterCriticalSection();
	}
	
	// Da das entsprechende Format hier bspw. 1FFFFFF... oder 2FFFFF... ist brettern wir ein Mal ganz durch. Ist die Idee.
	do {
		setMapEntry(heap, tempAddr, 0x0);
		tempAddr++;
		currentNibble = os_getMapEntry(heap, tempAddr);
	} while (currentNibble == 0x0F && ((((tempAddr - heap->useStart) / 2) + heap->mapStart) < heap->useStart));

	os_leaveCriticalSection();
	
}

//////// Hilfsfunktionen:
void updateAllocFrameRight(Heap *heap, MemAddr addr, ProcessID owner){
	
	MemAddr it = addr;
	
	while((it >= heap->useStart) && (os_getMapEntry(heap, it) != owner)){
		it--;
	}
	
	if(it == heap->useStart){
		heap->allocFrameStart[owner] = 0;
		heap->allocFrameEnd[owner] = 0;
	} else {
		while(os_getMapEntry(heap, it + 1) == 0x0F){
			it++;
		}
		heap->allocFrameEnd[owner] = it;
	}
	
}

void updateAllocFrameLeft(Heap *heap, MemAddr addr, ProcessID owner){
	
	MemAddr it = addr;
	
	while( (it - heap->useStart) / 2 + heap->mapStart < (heap->mapStart + heap->mapSize) && (os_getMapEntry(heap, it) != owner)){
		it++;
	}
	
	if(it == heap->useStart + (heap->useSize - 1)){
		heap->allocFrameStart[owner] = 0;
		heap->allocFrameEnd[owner] = 0;
	} else {
		heap->allocFrameStart[owner] = it;
	}
	
}

void os_freeOwnerRestricted(Heap *heap, MemAddr addr, ProcessID owner) {
	
	os_enterCriticalSection();
	
	MemAddr tempAddr = os_getFirstByteOfChunk(heap, addr);
	MemValue currentNibble = getOwnerOfChunk(heap, tempAddr);
	
	// Check ob der Prozess auch nur seinen eigenen Speicher freigibt.
	if(currentNibble != owner) {
		os_leaveCriticalSection();
		os_error("Unauthorized os_free!");
		return;
	}
	
	//V4: allocframe stuff: 1 F F F F 5 F 17 F F
	if (heap->allocFrameStart[os_getCurrentProc()] == tempAddr)
	{
		
		updateAllocFrameLeft(heap, tempAddr, owner);
		
	}
	
	
	
	// Da das entsprechende Format hier bspw. 1FFFFFF... oder 2FFFFF... ist brettern wir ein Mal ganz durch. Ist die Idee.
	do {
		setMapEntry(heap, tempAddr, 0x0);
		tempAddr++;
		currentNibble = os_getMapEntry(heap, tempAddr);
	} while (currentNibble == 0x0F && ((((tempAddr - heap->useStart) / 2) + heap->mapStart) < heap->useStart));
	
	if (heap->allocFrameEnd[os_getCurrentProc()] == tempAddr - 1)
	{
		
		updateAllocFrameRight(heap, tempAddr, owner);
		
	}
	
	os_leaveCriticalSection();
}



void os_freeProcessMemory(Heap *heap, ProcessID pid) {
	
	// Idee: Immer wenn ein Chunk von Prozess pid geclaimed wurde wird der weggewischt.
	
	os_enterCriticalSection();
	
	// Der ganze Heap muss abgesucht werden.
	for (MemAddr i = heap->allocFrameStart[pid]; ((i >= heap->useStart) && (i <= heap->allocFrameEnd[pid])); i++) {
		if (os_getMapEntry(heap, i) == pid) {
			os_freeOwnerRestricted(heap, i, pid);
		}
	}
	
	// V4: allocframe stuff
	heap->allocFrameStart[pid] = 0;
	heap->allocFrameEnd[pid] = 0;

	
	os_leaveCriticalSection();
}



MemAddr chunkFinder(Heap* heap, size_t size) {
	AllocStrategy currentStrat = os_getAllocationStrategy(heap);
	switch(currentStrat) {
		case OS_MEM_FIRST:
			return os_Memory_FirstFit(heap, size);
		case OS_MEM_BEST:
			return os_Memory_BestFit(heap, size);
		case OS_MEM_NEXT:
			return os_Memory_NextFit(heap, size);
		case OS_MEM_WORST:
			return os_Memory_WorstFit(heap, size);
		default:
			return os_Memory_FirstFit(heap, size);
		break;
	}
}

//////// Setter und Getter

MemAddr os_getFirstByteOfChunk(const Heap *heap, MemAddr addr) {
	
	while (os_getMapEntry(heap, addr) == 0x0F) {
		addr--;
	}
	return addr;

}



ProcessID getOwnerOfChunk(const Heap *heap, MemAddr addr) {
	
	return os_getMapEntry(heap, os_getFirstByteOfChunk(heap, addr));
}



uint16_t os_getChunkSize(const Heap *heap, MemAddr addr) {
	
	uint16_t size = 0;
	MemAddr addrIndex = os_getFirstByteOfChunk(heap, addr);
	MemValue chunkStart = os_getMapEntry(heap, addrIndex);
	
	if (chunkStart == 0) {return 0;} // Da ist gar kein Chunk, weird.
	else {
		do {
		size = size + 1;
		addrIndex = addrIndex + 1;
		
		} while (os_getMapEntry(heap, addrIndex) == 0x0F);
	}
	
	return size;

}



MemValue getHighNibble(const Heap *heap, MemAddr addr) {
	return ((heap->driver->read(addr) & 0xF0) >> 4);
}



MemValue getLowNibble(const Heap *heap, MemAddr addr) {
	return (heap->driver->read(addr) & 0x0F);
}



void setHighNibble(const Heap *heap, MemAddr addr, MemValue value) {
	
	MemValue nibbleValue;
	MemValue mapData = heap->driver->read(addr);
	
	nibbleValue = (mapData & 0x0F) | (value << 4);
	heap->driver->write(addr, nibbleValue);
}



void setLowNibble(const Heap *heap, MemAddr addr, MemValue value) {
		MemValue nibbleValue;
		MemValue mapData = heap->driver->read(addr);
		
		nibbleValue = (mapData & 0xF0) | (value & 0x0F);
		heap->driver->write(addr, nibbleValue);
}



MemValue os_getMapEntry(const Heap *heap, MemAddr addr) {
	
	// 2:1 Ratio Use:Map, Offset ist immer mapStart
	// Wenn hier irgendwas dummes passiert ist es vielleicht notwendig volatile einzusetzen.
	MemAddr mapEntry = ((addr - heap->useStart) / 2) + heap->mapStart;
	
	if ((addr - heap->useStart) % 2 == 0) { // Gibt es einen besseren Weg?
		return getHighNibble(heap, mapEntry);
	} else {
		return getLowNibble(heap, mapEntry);
	}
	
}



void setMapEntry(const Heap *heap, MemAddr addr, MemValue value) {
	
	MemAddr mapEntry = ((addr - heap->useStart) / 2) + heap->mapStart;
	
	if ((addr - heap->useStart) % 2 == 0) {
		setHighNibble(heap, mapEntry, value);
		} else {
		setLowNibble(heap, mapEntry, value);
	}

}



size_t os_getMapSize (Heap const* heap) {
	return heap->mapSize;
}

size_t os_getUseSize(Heap const* heap) {
	return heap->useSize;
}

MemAddr os_getMapStart(Heap const* heap) {
	return heap->mapStart;
}

MemAddr os_getUseStart(Heap const* heap) {
	return heap->useStart;
}

AllocStrategy os_getAllocationStrategy(Heap const* heap) {
	return heap->strategy;
}

void os_setAllocationStrategy(Heap* heap, AllocStrategy allocStrat) {
	heap->strategy = allocStrat;
}

//helferfunktion für defragmentieren von Daten: WIR NUTZEN SIZE_T statt UINT16_T, weil die das im doxygen so haben, wenn unvorhergesehener GARBAGE passiert, HIER HIN SCHAUN!!!!!!!!
void moveChunk(Heap* heap, MemAddr orig_first_byte, size_t orig_size, MemAddr new_first_byte, size_t size){
	
	for(size_t i = 0; i < orig_size; i++){
		
		//temp values für Map und Data werte abfragen:
		MemValue temp_data = heap->driver->read(orig_first_byte + i);
		MemValue temp_nibble = os_getMapEntry(heap, orig_first_byte + i);
		
		//an die richtige stelle kopieren:
		heap->driver->write(new_first_byte + i, temp_data);
		setMapEntry(heap, new_first_byte + i, temp_nibble);
		
		//als letztes alte stellen bereinigen:
		heap->driver->write(orig_first_byte + i, 0x00);
		setMapEntry(heap, orig_first_byte + i, 0x0);
	}
	
	//danach verbleibende bytes bis size in Map bereich mit F füllen
	for(size_t i = orig_size; i < size; i++){
		
		setMapEntry(heap, new_first_byte + i, 0xF);
		
	}
	
	
}


MemAddr os_realloc(Heap* heap, MemAddr addr, uint16_t size){
	
	//critical:
	os_enterCriticalSection();
	
	//sanity check: ruft der richtige prozess den richtigen speicher auf?
	if(os_getCurrentProc() != getOwnerOfChunk(heap, addr)) {
		os_error("ACCESS DENIED");
 		os_leaveCriticalSection();
		return 0;
	}
	
	MemAddr orig_first_byte = os_getFirstByteOfChunk(heap, addr);
	uint16_t orig_size = os_getChunkSize(heap, addr);
	uint16_t space_toRight = 0;
	uint16_t space_toLeft = 0;
	
	//2ter sanity check: sollte die neue größe die alte größe sein, verändert sich nichts:
	if(size == orig_size) {
		os_leaveCriticalSection();
		return orig_first_byte;
	}
	
	//ist die neue größer kleiner als die alte größe werden die überflüssigen hinteren größen aus dem adressraum gestrichen
	if(size < orig_size){
		
		//hintere addressen durchlaufen
		for(uint16_t i = orig_first_byte + orig_size; i >= orig_first_byte + size; i--){
			
			//entries auf 0 setzen, also speicher frei geben, der zuvor F war nach definition, ohne sanity check... und data auch 0en
			setMapEntry(heap, i, 0x0);
			heap->driver->write(i, 0x00);
			
		}
		
		//V4: allocframe stuff
		if (heap->allocFrameEnd[os_getCurrentProc()] == orig_first_byte + (orig_size - 1))
		{heap->allocFrameEnd[os_getCurrentProc()] = orig_first_byte + (size - 1);}
		
		
		//leave crit
		os_leaveCriticalSection();
		
		//nach verkleinerung wird die alte startadresse wieder zurückgegeben (verkleinerung von hinten)
		return orig_first_byte;
		
	}
	
	//jetzt bleibt nur noch wenn die neue sektion größer ist, dazu unterteilen wir wieder in 3 fälle:
	//1. wenn rechts genug platz zur erweiterung ist
	//2. wenn rechts +  links genug platz zur erweiterung ist
	//3. wenn wir nach einem neuen speicherort suchen müssen, weil nicht genug platz zum erweitern um den orginalen speicherblock existiert
	
	//1. checke auf welchem platz der letzte zu diesem bereich gehörende chunk liegt, ist er 0xFFFF, dann brauchen wir nicht nach rechts zu suchen!
	//iterator für while schleifen definieren, der rausfindet wie viel platz wir haben:
	MemAddr it = orig_first_byte + orig_size;
	
	//finde raus, wie viel platz rechts ist:
	while((os_getMapEntry(heap, it) == 0) && ((it - heap->useStart) / 2 + heap->mapStart < (heap->mapStart + heap->mapSize)) && (space_toRight + orig_size < size)){
		space_toRight++;
		it++;
	}
	
	//reicht der space rechts bereits aus, dann können wir das feld erweitern:
	if(space_toRight + orig_size >= size){
		
		for(MemAddr i = orig_first_byte + orig_size; i < orig_first_byte + size; i++){
			
			setMapEntry(heap, i, 0xF);
			
		}
		
		//V4: allocframe stuff
		if (heap->allocFrameEnd[os_getCurrentProc()] < orig_first_byte + (size - 1))
		{heap->allocFrameEnd[os_getCurrentProc()] = orig_first_byte + (size - 1);}
		
		//wenn wir genug platz hatten, können wir crit leaven und den neuen raum am alten start returnen:
		os_leaveCriticalSection();
		return orig_first_byte;
		
	}
	
	it = orig_first_byte - 1;
	//finde raus, wie viel platz links ist (wir laufen nur bis 1, weil 0 keine valide adresse ist und zum zurückgeben von falschem MALLOC genutzt wird):
	while((os_getMapEntry(heap, it) == 0) && (it >= heap->useStart)){
		space_toLeft++;
		it--;
	}
	
	//it ist in der while schleife nachdem es auf dem letzten freien gemessenen slot war nochmal um 1 heruntergesetzt worden, das gleichen wir hier aus:
	it++;
	
	//wenn dann der gesamte freie raum + orginale raum größer ist als die angefragte size, dann defragmentieren wir den speicher nach links:
	if(space_toRight + space_toLeft + orig_size >= size){
			
		moveChunk(heap, orig_first_byte, orig_size, it, size);
		
		//V4: allocframe stuff:
		if (heap->allocFrameStart[os_getCurrentProc()] > it)
		{heap->allocFrameStart[os_getCurrentProc()] = it;}
		
		if (heap->allocFrameEnd[os_getCurrentProc()] == orig_first_byte + (orig_size - 1))
		{heap->allocFrameEnd[os_getCurrentProc()] = it + (size - 1);}
			
		//wenn wir genug platz hatten, können wir crit leaven und den neuen raum an der neuen startposition returnen (it ist ja auf die linkeste zusammenhängende 0 gelaufen):
		os_leaveCriticalSection();
		return it;
			
	}
	
	//was übrig bleibt ist: was wenn um den orig chunk nicht genug platz war:
	//dann können wir einfach malloc callen, und danach den orig bereich free'n
	MemAddr malloc_return = os_malloc(heap, size);
	if(malloc_return != 0){
		//kein free, weil moveChunk den alten bereich bereits bereinigt
		 moveChunk(heap, orig_first_byte, orig_size, malloc_return, size);
	}
	
	//V4: allocframe stuff:
	if (heap->allocFrameStart[os_getCurrentProc()] > malloc_return)
	{heap->allocFrameStart[os_getCurrentProc()] = malloc_return;}
		
	if (heap->allocFrameEnd[os_getCurrentProc()] < malloc_return + (size - 1))
	{heap->allocFrameEnd[os_getCurrentProc()] = malloc_return + (size - 1);}
	
	//crit beenden
	os_leaveCriticalSection();
	
	//entweder malloc hat was gefunden, dann wurde der bereich migriert, oder es hat nichts gefunden, dann returned es 0
	return malloc_return;
	
}