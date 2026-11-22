/*! \file
 *  \brief Scheduling library for the OS.
 *
 *  Contains the scheduling strategies.
 *
 *  \author   Lehrstuhl Informatik 11 - RWTH Aachen
 *  \date     2013
 *  \version  2.0
 */ 

#ifndef _OS_SCHEDULING_STRATEGIES_H
#define _OS_SCHEDULING_STRATEGIES_H

#include "os_scheduler.h"
#include "defines.h"
#include <stdbool.h>
#include "os_mem_drivers.h"

#define NUMBER_OF_LEVELS 4

typedef struct {
	ProcessID data[MAX_NUMBER_OF_PROCESSES];
	uint8_t size;
	MemAddr head;
	MemAddr tail;
}ProcessQueue;

typedef struct {
	uint8_t timeSlice;
	uint8_t age[MAX_NUMBER_OF_PROCESSES];
	ProcessQueue* mlf_queues[NUMBER_OF_LEVELS];
	uint8_t timeSlicePerQueue[NUMBER_OF_LEVELS];
} SchedulingInformation;


//! Used to reset the SchedulingInfo for one process
void os_resetProcessSchedulingInformation(ProcessID id);

//! Used to reset the SchedulingInfo for a strategy
void os_resetSchedulingInformation(SchedulingStrategy strategy);

//! Even strategy
ProcessID os_Scheduler_Even(Process const processes[], ProcessID current);

//! Random strategy
ProcessID os_Scheduler_Random(Process const processes[], ProcessID current);

//! RoundRobin strategy
ProcessID os_Scheduler_RoundRobin(Process const processes[], ProcessID current);

//! InactiveAging strategy
ProcessID os_Scheduler_InactiveAging(Process const processes[], ProcessID current);

//! RunToCompletion strategy
ProcessID os_Scheduler_RunToCompletion(Process const processes[], ProcessID current);

//! Multi Level Feedback Queue strategy
ProcessID os_Scheduler_MLFQ(Process const processes[], ProcessID current);

// Zurgiffsfunktionen für ProcessQueue
void pqueue_init(ProcessQueue *queue);
void pqueue_reset(ProcessQueue *queue);
bool pqueue_hasNext(const ProcessQueue *queue);
ProcessID pqueue_getFirst(const ProcessQueue *queue);
void pqueue_dropFirst(ProcessQueue *queue);
void pqueue_append(ProcessQueue *queue, ProcessID pid);
void pqueue_removePID(ProcessQueue *queue, ProcessID pid);
ProcessQueue* MLFQ_getQueue(uint8_t queueID);
void os_initSchedulingInformation(void);


#endif
