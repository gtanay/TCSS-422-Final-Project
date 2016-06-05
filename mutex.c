/*
 * TCSS 422 Final Project
 * Team 4
 * Tempest Parr, Paul Zander, Geoffrey Tanay, Justin Clark
 */

#include "mutex.h"

/**
 * Creates new Mutex and returns pointer to it.
 */
Mutex_p Mutex_construct(int id, enum PCB_ERROR *error) {
	Mutex_p m = (Mutex_p) malloc(sizeof(Mutex));
	if (m == NULL) {
		*error = PCB_MEM_ALLOC_FAIL;
		return NULL;
	}
	m->is_locked = 0;
	m->id = id;
	m->waiting = PCB_Queue_construct(error);
	return m;
}

/**
 * Frees the memory taken up by the mutex.
 */
void Mutex_destruct(Mutex_p m, enum PCB_ERROR *error) {
	PCB_Queue_destruct(m->waiting, error);
	free(m);
}

/*
 * Attempts to lock the mutex, will put PCB in waiting queue if already locked.
 * Returns 1 if lock was successful and 0 if not.
 */
int Mutex_lock(Mutex_p m, PCB_p pcb, enum PCB_ERROR *error) {
	if (m->is_locked) {
		PCB_Queue_enqueue(m->waiting, pcb, error);
		return 0;
	} else {
		m->owner = pcb;
		m->is_locked = 1;
		return 1;
	}
}

/**
 * Checks if the mutex is locked and will return 0 if it already is locked, otherwise
 * it will lock the mutex with the pcb.
 */
int Mutex_trylock(Mutex_p m, PCB_p pcb, enum PCB_ERROR *error) {
	if (m->is_locked) {
		return 0;
	}
	return Mutex_lock(m, pcb, error);
}

/**
 * Attempts to unlock the mutex.
 */
void Mutex_unlock(Mutex_p m, PCB_p pcb, enum PCB_ERROR *error) {
	if (m->is_locked && m->owner == pcb) {
		if (PCB_Queue_is_empty(m->waiting, error)) {
			m->is_locked = 0;
			m->owner = NULL;
		} else {
			m->owner = PCB_Queue_dequeue(m->waiting, error);	// If PCB waiting, it will get the lock.
		}
	}
}
