/*
* TCSS 422 Final Project
* Team 4
* Tempest Parr, Paul Zander, Geoffrey Tanay, Justin Clark
*/

#include "cond_var.h"

/*
 * Creates a new condition variable with the given id.
 */
Condition_p Condition_construct(enum PCB_ERROR *error) {
	Condition_p condition = (Condition_p) malloc(sizeof(Condition_Variable));
	condition->waiting = PCB_Queue_construct(error);
	return condition;
}

/**
 * Frees the memory used by this condition variable. 
 */
void Condition_destruct(Condition_p condition, enum PCB_ERROR *error) {
	PCB_Queue_destruct(condition->waiting, error);
	free(condition);
}

/**
 * Will enqueue the process to this condition's waiting queue and switch its state
 * to waiting. 
 */
void Condition_wait(Condition_p condition, Mutex_p mutex, enum PCB_ERROR *error) {
	PCB_set_state(mutex->owner, PCB_STATE_WAITING, error);
	PCB_Queue_enqueue(condition->waiting, mutex->owner, error);
	Mutex_unlock(mutex, mutex->owner, error);
}

/**
 * "Wakes up" the processes that are waiting for the condition to change.
 */
void Condition_signal(Condition_p condition, PCB_Priority_Queue_p readyQ, enum PCB_ERROR *error) {
	PCB_p temp;
	while (!PCB_Queue_is_empty(condition->waiting, error)) {
		temp = PCB_Queue_dequeue(condition->waiting, error);
		PCB_set_state(temp, PCB_STATE_READY, error);
		PCB_Priority_Queue_enqueue(readyQ, temp, error);
	}
}