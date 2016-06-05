/*
* TCSS 422 Final Project
* Team 4
* Tempest Parr, Paul Zander, Geoffrey Tanay, Justin Clark
*/

#include "cond_var.h"

/*
 * Creates a new condition variable with the given id.
 */
Condition_p Condition_construct(int id, enum PCB_ERROR *error) {
	Condition_p condition = (Condition_p) malloc(sizeof(Condition_Variable));
	condition->waiting = PCB_Queue_construct(error);
	return condition;
}

/**
 * Frees the memory used by this condition variable. 
 */
void Condition_destruct(Condition_p condition, enum PCB_ERROR *error) {
	PCB_Queue_destruct(condition->waiting);
	free(condition);
}

/**
 * Will enqueue the process to this condition's waiting queue and switch its state
 * to blocked. 
 */
void Condition_wait(Condition_p condition, PCB_p pcb, enum PCB_ERROR *error) {
	PCB_Queue_enqueue(condition->waiting, pcb, error);
	//Set state of process to "blocked". Remove from normal processes. 
}

/**
 * "Wakes up" the processes that are waiting for the condition to change.
 */
PCB_p Condition_signal(Condition_p condition, PCB_Priority_Queue_p readyQ) {
	while(0) {
		//Change state to "Ready"
		//remove all processes from the waiting queue and add them back to the ready queue.
	}
}