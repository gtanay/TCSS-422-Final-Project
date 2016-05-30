#ifndef TRAP_HANDLER_H_
#define TRAP_HANDLER_H_

#include "PCB.h"
#include "PCB_Queue.h"
#include "PCB_Priority_Queue.h"

//trap handler
void trapHandler(PCB_Queue_p createdQueue, PCB_Priority_Queue_p readyQueue, PCB_p current, int* error, int interruptType);

#endif /* TRAP_HANDLER_H_ */
