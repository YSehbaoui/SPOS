/*! \file

Scheduling strategies used by the Interrupt Service RoutineA from Timer 2 (in scheduler.c)
to determine which process may continue its execution next.

The file contains five strategies:
-even
-random
-round-robin
-inactive-aging
-run-to-completion
- multilevel feedback queue
*/

#include "os_scheduling_strategies.h"
#include "defines.h"
#include <stdlib.h>

SchedulingInformation scheduling_info;
ProcessQueue mlf_queues[4];

void os_initSchedulingInformation(void) {
	for(uint8_t i = 0; i < NUMBER_OF_LEVELS; i++){
		scheduling_info.timeSlicePerQueue[i] = (uint8_t)(1<<(NUMBER_OF_LEVELS-i-1)); //8,4,2,1
		
		ProcessQueue* q = &(mlf_queues[i]);
		scheduling_info.mlf_queues[i] = q;
		
		pqueue_reset(scheduling_info.mlf_queues[i]);
	}
}

/*!
 *  Reset the scheduling information for a specific strategy
 *  This is only relevant for RoundRobin and InactiveAging
 *  and is done when the strategy is changed through os_setSchedulingStrategy
 *
 *  \param strategy  The strategy to reset information for
 */
void os_resetSchedulingInformation(SchedulingStrategy strategy) {
	switch(strategy){
		case OS_SS_ROUND_ROBIN: scheduling_info.timeSlice = os_getProcessSlot(os_getCurrentProc())->priority; break;//aktueller process
		case OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE:
		for(uint8_t i = 0; i < NUMBER_OF_LEVELS; i++){
			pqueue_init(scheduling_info.mlf_queues[i]);
		}
		for (uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++){
			if(os_getProcessSlot(i)->state != OS_PS_UNUSED){
				os_resetProcessSchedulingInformation(i);
			}
		}
		break;
		case OS_SS_INACTIVE_AGING:
		for (uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++){
			if(os_getProcessSlot(i)->state != OS_PS_UNUSED){
				os_resetProcessSchedulingInformation(i);
			}
		}
		break;
		default: break;
	}
}

/*!
 *  Reset the scheduling information for a specific process slot
 *  This is necessary when a new process is started to clear out any
 *  leftover data from a process that previously occupied that slot
 *
 *  \param id  The process slot to erase state for
 */
void os_resetProcessSchedulingInformation(ProcessID pid) {

	os_enterCriticalSection();
	if(os_getSchedulingStrategy() == OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE){
		if(!pid)		{
			os_leaveCriticalSection(); return;
		}
		for (uint8_t i = 0; i < NUMBER_OF_LEVELS; i++) {
			pqueue_removePID(MLFQ_getQueue(i), pid);
		}
		const uint8_t val = ((os_getProcessSlot(pid)->priority)) >> 6;
		pqueue_append(MLFQ_getQueue(val), pid);
		scheduling_info.age[pid] = scheduling_info.timeSlicePerQueue[val];
	}

	if(os_getSchedulingStrategy() == OS_SS_INACTIVE_AGING) scheduling_info.age[pid] = 0; os_leaveCriticalSection(); return;
	
	
	os_leaveCriticalSection();

}

/*!
 *  This function implements the even strategy. Every process gets the same
 *  amount of processing time and is rescheduled after each scheduler call
 *  if there are other processes running other than the idle process.
 *  The idle process is executed if no other process is ready for execution
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed determined on the basis of the even strategy.
 */
ProcessID os_Scheduler_Even(Process const processes[], ProcessID current) {
    uint16_t number_of_ready_processes = 0; 
	for (uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES;i++){
		if (processes[i].state == OS_PS_READY) number_of_ready_processes++;
	}
    
    if (number_of_ready_processes == 0) return 0;
	
	uint8_t process_id_to_exec = current;
	
	do {
		if(process_id_to_exec+1 > MAX_NUMBER_OF_PROCESSES-1) process_id_to_exec = 1;
		else process_id_to_exec++;
	} while (processes[process_id_to_exec].state != OS_PS_READY);
		
	return process_id_to_exec;

}

/*!
 *  This function implements the random strategy. The next process is chosen based on
 *  the result of a pseudo random number generator.
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed determined on the basis of the random strategy.
 */
ProcessID os_Scheduler_Random(Process const processes[], ProcessID current) {
	uint8_t number_of_ready_proc = 0;
	for(uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++) {
		if(processes[i].state == OS_PS_READY) number_of_ready_proc++;
	}
	
	ProcessID id_to_return = rand() % number_of_ready_proc;
	
	uint8_t countReady = 0;
	for(uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++) {
		if(processes[i].state == OS_PS_READY) {
			if(countReady == id_to_return) return i;
			else countReady++;
		}
	}
	return 0;
	
}

/*!
 *  This function implements the round-robin strategy. In this strategy, process priorities
 *  are considered when choosing the next process. A process stays active as long its time slice
 *  does not reach zero. This time slice is initialized with the priority of each specific process
 *  and decremented each time this function is called. If the time slice reaches zero, the even
 *  strategy is used to determine the next process to run.
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed determined on the basis of the round robin strategy.
 */
ProcessID os_Scheduler_RoundRobin(Process const processes[], ProcessID current) {
    uint8_t number_of_ready_proc = 0;
    for(uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++) {
	    if(processes[i].state == OS_PS_READY) number_of_ready_proc++;
    }
    if (number_of_ready_proc == 0) return 0;
    
	ProcessID next;
	
	scheduling_info.timeSlice--;
	
	if(scheduling_info.timeSlice == 0){
		next = os_Scheduler_Even(processes, current);
		scheduling_info.timeSlice = processes[next].priority;
	} else {
		next = current;
	}
   
    return next;
	
}

/*!
 *  This function realizes the inactive-aging strategy. In this strategy a process specific integer ("the age") is used to determine
 *  which process will be chosen. At first, the age of every waiting process is increased by its priority. After that the oldest
 *  process is chosen. If the oldest process is not distinct, the one with the highest priority is chosen. If this is not distinct
 *  as well, the one with the lower ProcessID is chosen. Before actually returning the ProcessID, the age of the process who
 *  is to be returned is reset to its priority.
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed, determined based on the inactive-aging strategy.
 */
ProcessID os_Scheduler_InactiveAging(Process const processes[], ProcessID current) {
	for (ProcessID i = 1; i < MAX_NUMBER_OF_PROCESSES; i++){
		if(processes[i].state == OS_PS_READY) scheduling_info.age[i] += processes[i].priority;
	}
	ProcessID max = 0;
	for(uint8_t i = 1; i< MAX_NUMBER_OF_PROCESSES; i++){
		// for all ready processes
		if(processes[i].state == OS_PS_READY){
			// check for age
			if(scheduling_info.age[i] > scheduling_info.age[max]) max = i;
			// if ages are equal, pick the one with the highest priority
			else if(scheduling_info.age[i] == scheduling_info.age[max]){
				if(processes[i].priority > processes[max].priority) max = i;
				// if priorities are equal, then pick the one with the lowest id
				else if(processes[i].priority == processes[max].priority){
					if(i < max) max = i;
				}
			}
		}
	}
	os_resetProcessSchedulingInformation(max);
	return max;
	
}

/*!
 *  This function realizes the run-to-completion strategy.
 *  As long as the process that has run before is still ready, it is returned again.
 *  If  it is not ready, the even strategy is used to determine the process to be returned
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed, determined based on the run-to-completion strategy.
 */
ProcessID os_Scheduler_RunToCompletion(Process const processes[], ProcessID current) {
    // If the process is still ready to be executed, then execute it
    if(processes[current].state == OS_PS_READY) return current;
	// Else choose like even
	return os_Scheduler_Even(processes, current);
}

uint16_t findPIDinPQueue(ProcessQueue *queue, ProcessID pid){
	uint16_t search = queue->tail;
	for(uint16_t counter = 0; counter < queue->size; counter++){
		if (queue->data[search] == pid) return search;
		search = (search+1) % queue->size;
		if (search == queue->head) break;
	}
	return queue->size; // error code
}

void degradeProcess(ProcessID pid){
	for(uint8_t qid = 0; qid<NUMBER_OF_LEVELS; qid++){
		if(findPIDinPQueue(MLFQ_getQueue(qid), pid) < MLFQ_getQueue(qid)->size){
			pqueue_removePID(MLFQ_getQueue(qid), pid);
			if(qid == 0){
				pqueue_append(MLFQ_getQueue(qid), pid);
				scheduling_info.age[pid] = scheduling_info.timeSlicePerQueue[qid];
				return;
			} else {
				pqueue_append(MLFQ_getQueue(qid-1), pid);
				scheduling_info.age[pid] = scheduling_info.timeSlicePerQueue[qid-1];
				return;
			}
		}
	}
}

ProcessID os_Scheduler_MLFQ(Process const processes[], ProcessID current){
	
//	if(os_getProcessSlot(current)->state == OS_PS_UNUSED) current = 0;
	
	for(uint8_t pid = 0; pid < MAX_NUMBER_OF_PROCESSES; pid++){
		if(os_getProcessSlot(pid)->state == OS_PS_UNUSED){
			for(uint8_t qid = 0; qid < NUMBER_OF_LEVELS; qid++){
				pqueue_removePID(MLFQ_getQueue(qid), pid);
			}
		}
	}
	if(scheduling_info.age[current] == 0){
		degradeProcess(current);
	}
	
	
	//schleife um den nächsten task mit der höchsten prio zu finden:
	for(uint8_t qid = NUMBER_OF_LEVELS; qid > 0; qid--){
		if (pqueue_hasNext(MLFQ_getQueue(qid-1))){
			if(os_getProcessSlot(pqueue_getFirst(MLFQ_getQueue(qid-1)))->state == OS_PS_READY){
				current = pqueue_getFirst(MLFQ_getQueue(qid-1));
				scheduling_info.age[current]--;
				return current;
				} else { // BLOCKED - edgecase: wenn nen geblockter prozess nen high prio startet, wird er in der queue nicht nach hinten gesetzt!
				ProcessID blockedPID = pqueue_getFirst(MLFQ_getQueue(qid-1));
				ProcessID maybeReturn = INVALID_PROCESS;
				pqueue_dropFirst(MLFQ_getQueue(qid-1));
				if(pqueue_hasNext(MLFQ_getQueue(qid-1))) maybeReturn = pqueue_getFirst(MLFQ_getQueue(qid-1));
				pqueue_append(MLFQ_getQueue(qid-1), blockedPID);
//				os_getProcessSlot(blockedPID)->state = OS_PS_READY;
				
				if(maybeReturn != INVALID_PROCESS){
					scheduling_info.age[maybeReturn]--;
					return maybeReturn;
				}
			}
		}
	}
	
// 	//nach der schleife updaten wir das age des neu asgewählten prozesses:
// 	scheduling_info.age[current]--;
// 	
// 	//und returnen diesen:
// 	return current;
// 	
//
	return 0;

}

// Zurgiffsfunktionen für ProcessQueue

// My own helper functions
bool pqueue_isFull(const ProcessQueue *queue){
	return (queue->head+1)%queue->size == queue->tail;
}



bool pqueue_isIndexInData(const ProcessQueue *queue, uint16_t index){
	return (queue->tail < queue->head)	? (queue->tail < index && index < queue->head)
										: (index < queue->head || queue->tail < index);
}

// Actuall functions
void pqueue_init(ProcessQueue *queue){
// 	for(uint8_t i = 0; i<MAX_NUMBER_OF_PROCESSES; i++){
// 		(*(queue->data))[i] = 0;
// 	}
	queue->size = MAX_NUMBER_OF_PROCESSES;
	queue->head = 0;
	queue->tail = 0;
}

void pqueue_reset(ProcessQueue *queue){
	queue->head = 0;
	queue->tail = 0;
}

bool pqueue_hasNext(const ProcessQueue *queue){
	return queue->head != queue->tail;
}

ProcessID pqueue_getFirst(const ProcessQueue *queue){
	if(!pqueue_hasNext(queue)) return 0;
	return queue->data[queue->tail];
}

void pqueue_dropFirst(ProcessQueue *queue){
	if(!pqueue_hasNext(queue)) return;
	queue->data[queue->tail] = 0;
	
	queue->tail = (queue->tail+1) % queue->size;
}

void pqueue_append(ProcessQueue *queue, ProcessID pid){
	//if(pqueue_isFull(queue)) return;
	queue->data[queue->head] = pid;
	
	queue->head = (queue->head+1) % queue->size;
}

void pqueue_removePID(ProcessQueue *queue, ProcessID pid){
	uint8_t killIndex = findPIDinPQueue(queue, pid);
	if(killIndex >= queue->size) return; // index is not in data 
	
	// Case: Kill first Element
	if(killIndex == queue->tail) pqueue_dropFirst(queue);
	
	// Case: Kill last Element
	else if (killIndex == queue->head) {
		queue->data[queue->head] = 0;
		queue->head = (queue->head) ? (queue->head-1) : (queue->size-1);
	}
	
	// Case: Kill element in the middle 
	else {
		// Move all other elements
		while(pqueue_isIndexInData(queue, killIndex)){
			killIndex = (killIndex) ? (killIndex-1) : (queue->size-1); // decrement killIndex in mod space
			queue->data[(killIndex+1) % queue->size] = queue->data[killIndex]; // move data in mod space
		}
		pqueue_dropFirst(queue); // first element needs to be killed (already copied to the next space)
	}
}

ProcessQueue* MLFQ_getQueue(uint8_t queueID) {
	if (queueID < NUMBER_OF_LEVELS) {
		return (scheduling_info.mlf_queues[queueID]);
	}
	return NULL; // Null Pointer Errors??? :DDDDD
}
	
