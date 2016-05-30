#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "Trap_Handler.h"
#include "PCB.h"
#include "PCB_Queue.h"

void scheduler(PCB_Queue_p createdQueue, PCB_Priority_Queue_p readyQueue, int interruptType);

#endif /* SCHEDULER_H_ */
