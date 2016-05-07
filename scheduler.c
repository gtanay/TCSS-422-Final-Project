#include <stdio.h>

#include "Trap_Handler.h"
#include "PCB.h"
#include "PCB_Queue.h"

void scheduler(PCB_Queue_p createdQueue, PCB_Priority_Queue_p readyQueue, int interruptType)
{
	//make sure queue is not empty
	while (!PCB_Queue_is_empty(createdQueue, &error))
	{
		//dequeue this recently created PCB
		PCB_p p = PCB_Queue_dequeue(createdQueue, &error);

		//set this PCB state to ready
		PCB_set_state(p, PCB_STATE_READY, &error);

		//enqueue this PCB into the ready queue
		PCB_Queue_enqueue(readyQueue, p, &error);
	}

	//if it's a timer interrupt
	if (interruptType == 1)
	{
		//set state of PCB to ready
		PCB_set_state(currentPCB, PCB_STATE_READY, &error);

		//enqueue current PCB into the ready queue
		PCB_Queue_enqueue(readyQueue, currentPCB, &error);

		//call the dispatcher
		dispatcher();
	}
}
