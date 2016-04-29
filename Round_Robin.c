#include "PCB_Queue.h"
#include "PCB.h"
#include "PCB_Errors.h"
#include <stdlib.h>
#include <stdio.h>

#define IDL_PID 0xFFFFFFFF

unsigned long sysStack;
PCB_Queue_p createdQueue;
PCB_Queue_p readyQueue;
PCB_p currentPCB;
PCB_p idl;

enum PCB_ERROR error = PCB_SUCCESS;

void dispatcher() {
	if (!PCB_Queue_is_empty(readyQueue, &error)) {
		currentPCB = PCB_Queue_dequeue(readyQueue, &error);
	} else {
		currentPCB = idl;
	}

	printf("Switching to:\t");
	PCB_print(currentPCB, &error);

	PCB_set_state(currentPCB, PCB_STATE_RUNNING, &error);
	sysStack = PCB_get_pc(currentPCB, &error);
}

void scheduler(int interruptType) {
	while (!PCB_Queue_is_empty(createdQueue, &error)) {
		PCB_p p = PCB_Queue_dequeue(createdQueue, &error);
		PCB_set_state(p, PCB_STATE_READY, &error);

		printf("Scheduled:\t");
		PCB_print(p, &error);

		PCB_Queue_enqueue(readyQueue, p, &error);
	}
	if (interruptType == 1) {
		PCB_set_state(currentPCB, PCB_STATE_READY, &error);
		if (PCB_get_pid(currentPCB, &error) != IDL_PID) {

			printf("Returned:\t");
			PCB_print(currentPCB, &error);

			PCB_Queue_enqueue(readyQueue, currentPCB, &error);
		}
	} 
	dispatcher();
}

void isrTimer() {
	PCB_set_state(currentPCB, PCB_STATE_INTERRUPTED, &error);
	PCB_set_pc(currentPCB, sysStack, &error);
	scheduler(1);
}

int main() {
	idl = PCB_construct(&error);
	PCB_init(idl, &error);
	PCB_set_pid(idl, IDL_PID, &error);
	PCB_set_state(idl, PCB_STATE_RUNNING, &error);
	currentPCB = idl;

	createdQueue = PCB_Queue_construct(&error);
	readyQueue = PCB_Queue_construct(&error);
	
	for (int i = 0; i < 6; i++) {
		int newProcesses = rand() % 6;
		for (int j = 0; j < newProcesses; j++) {
			PCB_p p = PCB_construct(&error);
			PCB_init(p, &error);
			PCB_set_pid(p, (i << 4) + j, &error);
			PCB_Queue_enqueue(createdQueue, p, &error);
			
			printf("Created:\t");
			PCB_print(p, &error);
		}
		if (PCB_get_pid(currentPCB, &error) != IDL_PID) {
			sysStack += rand() % 1001 + 3000;
		}
		
		printf("Switching from:\t");
		PCB_print(currentPCB, &error);

		isrTimer();
		PCB_Queue_print(readyQueue, &error);
	}

	printf("\n\n%u", error);
	return 0;
}