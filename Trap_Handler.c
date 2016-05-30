#include <stdio.h>

#include "PCB.h"
#include "PCB_Queue.h"
#include "PCB_Priority_Queue.h"

void trapHandler(PCB_Queue_p createdQueue, PCB_Priority_Queue_p readyQueue, PCB_p current, int* error, int interruptType)
{
	//get the I/O threads to sleep for a few milliseconds

	//change state from running to waiting
	PCB_set_state(current, PCB_STATE_WAITING, error);

	//place PCB into the queue
	PCB_Queue_enqueue(readyQueue, current, error);

	//call scheduler
	scheduler(createdQueue, readyQueue, interruptType);
}
