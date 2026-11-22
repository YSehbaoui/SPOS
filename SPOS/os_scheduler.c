/*! \file
 *  \brief Scheduling module for the OS.
 *
 * Contains everything needed to realise the scheduling between multiple processes.
 * Also contains functions to start the execution of programs.
 *
 *  \author   Lehrstuhl Informatik 11 - RWTH Aachen
 *  \date     2013
 *  \version  2.0
 */

#include "os_scheduler.h"

#include "lcd.h"
#include "os_core.h"
#include "os_input.h"
#include "os_memheap_drivers.h"
#include "os_memory.h"
#include "os_scheduling_strategies.h"
#include "os_taskman.h"
#include "util.h"

#include <avr/interrupt.h>
#include <stdbool.h>

//----------------------------------------------------------------------------
// Private Types
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------

//! Array of states for every possible process
Process os_processes[MAX_NUMBER_OF_PROCESSES];

//! Index of process that is currently executed (default: idle)
ProcessID currentProc; // Auf den Namen muss man erstmal kommen, wa?

//----------------------------------------------------------------------------
// Private variables
//----------------------------------------------------------------------------

//! Currently active scheduling strategy
SchedulingStrategy activeStrat;

//! Count of currently nested critical sections
uint8_t criticalSectionCount = 0;

//----------------------------------------------------------------------------
// Private function declarations
//----------------------------------------------------------------------------

//! ISR for timer compare match (scheduler)
ISR(TIMER2_COMPA_vect)
__attribute__((naked));

//----------------------------------------------------------------------------
// Function definitions
//----------------------------------------------------------------------------

/*!
 *  Timer interrupt that implements our scheduler. Execution of the running
 *  process is suspended and the context saved to the stack. Then the periphery
 *  is scanned for any input events. If everything is in order, the next process
 *  for execution is derived with an exchangeable strategy. Finally the
 *  scheduler restores the next process for execution and releases control over
 *  the processor to that process.
 */
ISR(TIMER2_COMPA_vect) {
	// Aaaaaaaalrighty then Chatroom.
	
	// 1. Nichts zu tun.
	
	// 2. Laufzeitkontext sichern
	saveContext();
	
	// 3. Stackpointer sichern
	(os_processes[os_getCurrentProc()]).sp.as_int = SP; // enterprisecode.png
	
	// 4. SP-Register setzen (auf den Scheduler-Stack)
	SP = BOTTOM_OF_ISR_STACK;
	
	// (2.4.6) Erkennung von Stackinkonsistenzen kommt hier hin, right?
	(os_processes[os_getCurrentProc()]).checksum = os_getStackChecksum(os_getCurrentProc());
	
	// (2.4.7) Dann auch hier.
	if (os_getInput() == 0b00001001) { os_waitForNoInput(); os_taskManMain(); }

	
	// 5. Prozesszustand setzen (auf Ready) (wenn er wirklich existiert)
	if (os_processes[os_getCurrentProc()].state == OS_PS_RUNNING) {
		os_processes[os_getCurrentProc()].state = OS_PS_READY;
	}

	// 6. Auswahl via Schedulungstrategie
	currentProc = isrScheduler(os_processes, os_getCurrentProc()); // Ich habe im echten Leben gelitten weil hier beim Array mal eckige Klammern dabei waren haha :)
	
	//if(currentProc >= MAX_NUMBER_OF_PROCESSES){ os_error("ISR new PID OOB");}
		
	// Versuch 5.4.1: Prozesse durchlaufen nach einmaligem ausführen des Schedulers, um alle Blocked Prozesse zu unblocken, weil sie ja jetzt einmal ignoriert wurden:
	for(uint8_t i = 0; i < 8; i++){
		if(os_processes[i].state == OS_PS_BLOCKED){
			os_processes[i].state = OS_PS_READY;
			//fehlerquelle in scheduling strategy tests wenn mehr als 1 prozess gleichzeitig auf blocked stehen kann! dann kann der "falsche" prozess gewählt werden, und wir sollten statt normaler auswahl nochmal die strategy callen!
			if(currentProc == 0) {
				currentProc = i; 
			//	os_error("IDLE to BLOCKED");
			}
		}
	}
	
	// 7. Neuen Prozesszustand setzen (auf Running)
	os_processes[currentProc].state = OS_PS_RUNNING;

	
	// 8. Stackpointer wiederherstellen
	SP = os_processes[os_getCurrentProc()].sp.as_int;
	
	// (2.4.6) Naja, und hier.
	if ((os_processes[os_getCurrentProc()]).checksum != os_getStackChecksum(os_getCurrentProc())) { os_error("Inconsistency!"); }

	// 9. Konzext wiederherstellen
	restoreContext();
	
	// 10. Nichts zu tun.
	
	// Das war nicht so schlimm wie erwartet, ist ja quasi alles vorgegeben und schwer nur semi-richtig umzusetzen. Bis auf Pointer. >:(
}

/*!
 *  This is the idle program. The idle process owns all the memory
 *  and processor time no other process wants to have.
 */
void idle(void){

	while(1) { lcd_writeChar('.'); delayMs(DEFAULT_OUTPUT_DELAY); }
}

/*!
 *  This function is used to register the given program for execution.
 *  A stack will be provided if the process limit has not yet been reached.
 *  This function is multitasking safe. That means that programs can repost
 *  themselves, simulating TinyOS 2 scheduling (just kick off interrupts ;) ).
 *
 *  \param program  The function of the program to start.
 *  \param priority A priority ranging 0..255 for the new process:
 *                   - 0 means least favourable
 *                   - 255 means most favourable
 *                  Note that the priority may be ignored by certain scheduling
 *                  strategies.
 *  \return The index of the new process or INVALID_PROCESS as specified in
 *          defines.h on failure
 */
ProcessID os_exec(Program *program, Priority priority) {

	// Schritt 0: Crits
	os_enterCriticalSection();
	
	// Schritt 1: Suche freien Platz
	uint8_t freeSpace = MAX_NUMBER_OF_PROCESSES;
	
	for (uint8_t i=0; i < MAX_NUMBER_OF_PROCESSES; i++) {
		if (os_processes[i].state == OS_PS_UNUSED) { freeSpace = i; break; }
	}
	
	if (freeSpace == MAX_NUMBER_OF_PROCESSES) { os_leaveCriticalSection(); return INVALID_PROCESS; }
	
	// Schritt 2: Sanity check (Finde Schritt 1 eigentlich passender aber wir halten uns ans Buch)
	if (program == NULL) { os_leaveCriticalSection(); return INVALID_PROCESS; }
	
	// Schritt 3: Programm, Zustand, Prio (Im Buch halten die sich nicht an die beschriebene Reihenfolge, das geht mir zu weit)
	os_processes[freeSpace].program = program;
	os_processes[freeSpace].state = OS_PS_READY;
	os_processes[freeSpace].priority = priority;
	
	// Versuch 5.4.2:
	
	// kein Schritt sondern Zusatz: 2.5.2 Implementierung der Schedulingstrategien (in Versuch 2 optional):
	os_resetProcessSchedulingInformation(freeSpace);
	
	// Schritt 4: Prozessstack
	
	// Unansehnliche Einfassung der Rücksprungadresse, Low Byte als erstes (wichtig, probably) (denk ausserdem dran dass der Stack in die physikalische Stromrichtung stackt)
	os_processes[freeSpace].sp.as_int = PROCESS_STACK_BOTTOM(freeSpace);
	
	// Versuch 3 3.4.3: Der Toggle. Oben Normal, unten neu und weird. (Derzeitig aus)
	//*(os_processes[freeSpace].sp.as_ptr) = (uint16_t) program & 0xFF;
	*(os_processes[freeSpace].sp.as_ptr) = (uint16_t) &os_dispatcher & 0xFF;
	os_processes[freeSpace].sp.as_ptr--;
	//*(os_processes[freeSpace].sp.as_ptr) = ((uint16_t) program >> 8) & 0xFF;
	*(os_processes[freeSpace].sp.as_ptr) = ((uint16_t) &os_dispatcher >> 8) & 0xFF;
	os_processes[freeSpace].sp.as_ptr--;
	
	for (uint8_t i=0; i <= 32; i++) { // Ausnahmsweise lag der Off-by-One NICHT hier. Gute Arbeit.
		*(os_processes[freeSpace].sp.as_ptr) = 0;
		os_processes[freeSpace].sp.as_ptr--;
	}
	
	// 2.4.6
	os_processes[freeSpace].checksum = os_getStackChecksum(freeSpace);

	os_leaveCriticalSection();
	
	return freeSpace;
	
}

/*!
 *  If all processes have been registered for execution, the OS calls this
 *  function to start the idle program and the concurrent execution of the
 *  applications.
 */
void os_startScheduler(void) {

	// Dope, die Schritte sind auch alle vorgegeben
	// 1. Leerlaufprozess setzen
	currentProc = 0;
	
	// 2. Leerlaufprozess auf Running stellen
	os_processes[0].state = OS_PS_RUNNING;
	
	// 3. Stackpointer auf Leerlaufprozess
	SP = os_processes[0].sp.as_int;
	
	// 4. In Leerlaufprozess springen
	restoreContext();

}

/*!
 *  In order for the Scheduler to work properly, it must have the chance to
 *  initialize its internal data-structures and register.
 */
void os_initScheduler(void){
	
	// Array auf Startkondition
	for(uint8_t i=0; i < MAX_NUMBER_OF_PROCESSES; i++) {
		os_processes[i].state = OS_PS_UNUSED;
	}

	// Autostart-Start
	os_exec(idle, DEFAULT_PRIORITY); // Leerlaufprozess my beloved
	
	struct program_linked_list_node startupProcesses = *autostart_head;
	
	while (1) {
		os_exec(*(startupProcesses.program), DEFAULT_PRIORITY);
		if (startupProcesses.next != NULL) { startupProcesses = *startupProcesses.next; }
		else { break; }
		
	}
	
	os_initSchedulingInformation();

}

/*!
 *  A simple getter for the slot of a specific process.
 *
 *  \param pid The processID of the process to be handled
 *  \return A pointer to the memory of the process at position pid in the os_processes array.
 */
Process *os_getProcessSlot(ProcessID pid) {
    return os_processes + pid;
}

/*!
 *  A simple getter to retrieve the currently active process.
 *
 *  \return The process id of the currently active process.
 */
ProcessID os_getCurrentProc(void) {
	
	return currentProc; // Wow.
}

/*!
 *  Sets the current scheduling strategy.
 *
 *  \param strategy The strategy that will be used after the function finishes.
 */
void os_setSchedulingStrategy(SchedulingStrategy strategy){
	activeStrat = strategy;
	os_resetSchedulingInformation(strategy);
}

/*!
 *  This is a getter for retrieving the current scheduling strategy.
 *
 *  \return The current scheduling strategy.
 */
SchedulingStrategy os_getSchedulingStrategy(void) {
	return activeStrat;
}

/*!
 *  Enters a critical code section by disabling the scheduler if needed.
 *  This function stores the nesting depth of critical sections of the current
 *  process (e.g. if a function with a critical section is called from another
 *  critical section) to ensure correct behaviour when leaving the section.
 *  This function supports up to 255 nested critical sections.
 */
void os_enterCriticalSection(void) {
	
	// 1. Speichern des GIEB
	uint8_t globalInterruptEnableBit = (SREG >> 7);
	
	// 2. Deaktivieren des GIEB
	SREG &= 0x7F; // Ja, ich hab das extra konvertiert um Platz zu sparen.
	
	// 3. Inkrementieren der Tiefe (inkl. Sanity Check)
	if (criticalSectionCount < 255) { criticalSectionCount++; } 
	else { os_error("Critical Overflow"); }
	
	// 4. Deaktivieren des Schedulers
	TIMSK2 &= 0b11111101; // Okay, hier lass ich das mit dem Hex, FD ist echt unintuitiver als 7F da oben.
	
	// 5. Wiederherstellen des GIEB
	SREG |= (globalInterruptEnableBit << 7);
	
}

/*!
 *  Leaves a critical code section by enabling the scheduler if needed.
 *  This function utilizes the nesting depth of critical sections
 *  stored by os_enterCriticalSection to check if the scheduler
 *  has to be reactivated.
 */
void os_leaveCriticalSection(void){

	// 1. Speichern des GIEB
	uint8_t globalInterruptEnableBit = (SREG >> 7);
	
	// 2. Deaktivieren des GIEB
	SREG &= 0x7F;
	
	// 3. Dekrementieren der Tiefe (inkl. Sanity Check)
	if (criticalSectionCount > 0) { criticalSectionCount--; }
	else { os_error("Critical Underflow"); }
	
	// 4. Reaktivieren des Schedulers (aber nur ganz am Ende vom Deep Dive
	if (criticalSectionCount == 0) { TIMSK2 |= 0b00000010; }
		
	// 5. Wiederherstellen des GIEB
	SREG |= (globalInterruptEnableBit << 7);
}

/*!
 *  Calculates the checksum of the stack for a certain process.
 *
 *  \param pid The ID of the process for which the stack's checksum has to be calculated.
 *  \return The checksum of the pid'th stack.
 */
StackChecksum os_getStackChecksum(ProcessID pid) {
	StackChecksum checksum = 0;
	StackPointer checkPointer;
	
	//sanity check für den fall, dass eine unserer strategien unfug zurück gibt:
	if(pid >= MAX_NUMBER_OF_PROCESSES){ os_error("PID OOB in CS");}
		
	checkPointer.as_int = PROCESS_STACK_BOTTOM(pid);

	while (os_processes[pid].sp.as_int < checkPointer.as_int) {
		checksum ^= *(checkPointer.as_ptr); // Sieht fies aus tbh.
		checkPointer.as_ptr--;
	}

	return checksum;

}


// Ich erinnere mich aus letztem Jahr dass man bei der ISR am besten ne separate Funktion bzgl. SchedulingStrategy anlegt.

ProcessID isrScheduler(Process const procs[], ProcessID oldProc) {
	SchedulingStrategy nextStrat = os_getSchedulingStrategy();
	switch(nextStrat) {
		case OS_SS_EVEN:
		return os_Scheduler_Even(procs, oldProc);
		case OS_SS_RANDOM:
		return os_Scheduler_Random(procs, oldProc);
		case OS_SS_RUN_TO_COMPLETION:
		return os_Scheduler_RunToCompletion(procs, oldProc);
		case OS_SS_ROUND_ROBIN:
		return os_Scheduler_RoundRobin(procs, oldProc);
		case OS_SS_INACTIVE_AGING:
		return os_Scheduler_InactiveAging(procs, oldProc);
		case OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE:
		return os_Scheduler_MLFQ(procs, oldProc);
		default:
		return os_Scheduler_Even(procs, oldProc);
		break;
	}
}

//////// Versuch 3:

void os_dispatcher(void) {
	
	//Schritt 1: Passiert in os_exec.
	
	//Schritt 2:
	ProcessID activeProc = os_getCurrentProc();
	Program* program = os_processes[activeProc].program;
	
	//Schritt 3:
	program();
	
	//Schritt 4: Nichts.
	
	// Schritt 5:
	os_kill(os_getCurrentProc());
	
	// Schritt 6: macht nix, weil eigentlich os_kill die schleife ist
	// while (1){} // Door stuck! Door Stuck! Scheduler please! I beg of you!
	// Versuch 5.4.1: warteschleife durch yield ersetzen:
	os_yield();

}

bool os_kill(ProcessID pid) {
	
	os_enterCriticalSection();
	
	bool killed = false;
	
	// Alle Prozesse (ohne idle) werden durchsucht:
	for (uint8_t i=1; i < MAX_NUMBER_OF_PROCESSES; i++) {
		if (pid == i) {killed = true;}
	}
	
	// Wenn die Pid valide ist geht es weiter:
	if (killed) {
		os_freeProcessMemory(intHeap, pid); // Vielleicht irgendwann outdated idk
//		os_freeProcessMemory(extHeap, pid);
		os_processes[pid].state = OS_PS_UNUSED;
	}
	
	
	// Bei einer Selbstterminierung muss man die kritischen Bereiche zumachen und warten:
	if (os_getCurrentProc() == pid) {
// 		while (criticalSectionCount > 1) { os_leaveCriticalSection(); } // Zumachen.
// 		os_leaveCriticalSection();
// 		while(1){} // Warten.
		//Versuch 5.4.1: statt busy waiting und critical sections selbers schließen, lassen wir yield das machen:
		os_yield();
	}
	os_leaveCriticalSection();
	return killed;

	
	
}


//! Versuch 5:
// os_yield: Gibt verbleibende Rechenzeit ab und called ISR
// "if a process yields, it is moved to the end of the queue"!!!!! AUSRUFEZEICHEN - TO BE IMPLEMENTED ERROR:
void os_yield(void){
	os_enterCriticalSection();
	
	if(os_processes[os_getCurrentProc()].state != OS_PS_UNUSED){
		os_processes[os_getCurrentProc()].state = OS_PS_BLOCKED;
	}
	
	//sichern anzahl kritischer bereiche:
	uint8_t section_count = criticalSectionCount;
	
	//GIEB sichern:
	// 1. Speichern des GIEB
	uint8_t globalInterruptEnableBit = (SREG >> 7);
		
	// 2. Deaktivieren des GIEB
	SREG &= 0x7F;
	
	//alle kritischen sektionen schließen und GIEB 0 setzen:
	for(uint8_t it = 0; it < section_count; it++){
		os_leaveCriticalSection();
	}
	
	//dann ISR CALLEN
	TIMER2_COMPA_vect();
	
	//wenn dann durch ISR der prozess fortgesetzt wird, vorigen kritischen status widerherstellen:
	//GIEB setzen:
	// 5. Wiederherstellen des GIEB
	SREG |= (globalInterruptEnableBit << 7);
	SREG &= (globalInterruptEnableBit << 7);
	
	//sektionen hochzählen:
	for(uint8_t it = 0; it < section_count; it++){
		os_enterCriticalSection();
	}
	
	//NEU!! AUSRUFEZEICHEN!!! Behebt hoffentlich overflow Probleme, derp!!!
	os_leaveCriticalSection();
	
	
}