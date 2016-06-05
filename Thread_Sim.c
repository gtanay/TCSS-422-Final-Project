/*
 * TCSS 422 Final Project
 * Team 4
 * Tempest Parr, Paul Zander, Geoffrey Tanay, Justin Clark
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "PCB.h"
#include "PCB_Queue.h"
#include "PCB_Priority_Queue.h"

#define IDL_PID 0xFFFFFFFF

// time must be under 1 billion
#define SLEEP_TIME 90000000
#define IO_TIME_MIN SLEEP_TIME * 3
#define IO_TIME_MAX SLEEP_TIME * 5

#define MAX_IO_PROCESSES 50					// max number of io processes created
#define MAX_PRIORITY_LEVELS 4				// max number of priortiy levels (0 - 5%, 1 - 80%, 2 - 10%, 3 - 5%)
#define MAX_COMPUTE_INTENSIVE_PROCESSES 25	// max number of processes that do no request io or sync services
#define MAX_PRODUCER_CONSUMER_PAIRS 10		// max number of producer-consumer pairs

// global integer for producer-consumer pairs to read from and write to
int producer_consumer_var[10];

//Counts of processes. 
unsigned int TOTAL_PROCESS_COUNT = 0;
unsigned int IO_PROCESS_RUNNING_COUNT = 0;
unsigned int INTENSIVE_PROCESS_RUNNING_COUNT = 0;
unsigned int PRODUCER_CONSUMER_PAIR_RUNNING_COUNT = 0;

enum INTERRUPT_TYPE {
	INTERRUPT_TYPE_TIMER, 
	INTERRUPT_TYPE_IO_A, 
	INTERRUPT_TYPE_IO_B,
    INTERRUPT_TYPE_INVALID // for first call to scheduler to move PCBs out of createdQs to readyQ
};

typedef struct {
	pthread_mutex_t* mutex;
	pthread_cond_t* condition;
	int flag;
	int sleep_length_min;
	int sleep_length_max;
} timer_arguments;

typedef timer_arguments* timer_args_p;

PCB_Queue_p createdQueue;
PCB_Priority_Queue_p readyQueue;
PCB_Queue_p terminatedQueue;
PCB_Queue_p waitingQueueA;
PCB_Queue_p waitingQueueB;
PCB_p currentPCB;
PCB_p idl;
unsigned long sysStack;
enum PCB_ERROR error = PCB_SUCCESS;

void* timer(void* arguments) {
	int sleep_length;
	struct timespec sleep_time;
	timer_args_p args = (timer_args_p) arguments; 
    // printf("timer: before locking mutex\n");
	pthread_mutex_lock(args->mutex);
    // printf("timer: after locking mutex\n");
    sleep_time.tv_sec = 0;
	for(;;) {
		sleep_time.tv_nsec = args->sleep_length_min + rand() % (args->sleep_length_max - args->sleep_length_min + 1);
		//printf("sleeping %d\n", sleep_length);
        // printf("timer: before sleeping for %d ns\n", (int) sleep_time.tv_nsec);
		nanosleep(&sleep_time, NULL);
        // printf("timer: after sleep\n");
		//printf("done sleeping %d\n", sleep_length);
		args->flag = 0;
		pthread_cond_wait(args->condition, args->mutex);
	}
}

void dispatcher() {
	// if the ready queue is not empty
	if (get_PQ_size(readyQueue, &error) > 0) {
		// dequeue from the ready queue
		currentPCB = PCB_Priority_Queue_dequeue(readyQueue, &error);
	// if the ready queue is empty, switch to the idle pcb
	} else {
		currentPCB = idl;
	}

	printf("\nSwitching to:\t");
	PCB_print(currentPCB, &error);
	printf("Ready Queue:\t");
	PCB_Priority_Queue_print(readyQueue, &error);

	// change the state of the pcb that was just dequeued from the
	// ready queue from ready to running
	PCB_set_state(currentPCB, PCB_STATE_RUNNING, &error);

	// push the pcb pc value to the sysStack to replace the interrupted process
	sysStack = PCB_get_pc(currentPCB, &error);
}

void scheduler(enum INTERRUPT_TYPE interruptType) {
	// handling processes that have run to completion
	while (!PCB_Queue_is_empty(terminatedQueue, &error)) {
		PCB_p p = PCB_Queue_dequeue(terminatedQueue, &error);
		printf("Deallocated:\t");
		PCB_print(p, &error);
		PCB_destruct(p, &error);
	}

	// handling processes created for the first time
	// move processes from created queue into the ready queue
	while (!PCB_Queue_is_empty(createdQueue, &error)) {
		PCB_p p = PCB_Queue_dequeue(createdQueue, &error);
		PCB_set_state(p, PCB_STATE_READY, &error);
		printf("Scheduled:\t");
		PCB_print(p, &error);
		PCB_Priority_Queue_enqueue(readyQueue, p, &error);
	}

	// if timer interrupt occurs for the current process running
	if (interruptType == INTERRUPT_TYPE_TIMER) {
		// change the state of the current pcb from interrupted to ready
		PCB_set_state(currentPCB, PCB_STATE_READY, &error);

		// if the current pcb is not the idle process
		if (PCB_get_pid(currentPCB, &error) != IDL_PID) {
			printf("Returned:\t");
			PCB_print(currentPCB, &error);

			// put the pcb in the back of the ready queue
			PCB_Priority_Queue_enqueue(readyQueue, currentPCB, &error);
		}

		// call to dispatcher
		dispatcher();

	// if io device A interrupts the current process running
	} else if (interruptType == INTERRUPT_TYPE_IO_A) {
		// dequeue pcb from wait queue A
		PCB_p p = PCB_Queue_dequeue(waitingQueueA, &error);
		printf("IO COMPLETION: Moved from IO A:\t");
		PCB_print(p, &error);

		// print wait queue A
		printf("Waiting Queue A:");
		PCB_Queue_print(waitingQueueA, &error);

		// set the state of pcb just dequeued to ready
		PCB_set_state(p, PCB_STATE_READY, &error);

		// enqueue the pcb into the ready queue
		PCB_Priority_Queue_enqueue(readyQueue, p, &error);

	// if io device B interrupts the current process running
	} else if (interruptType == INTERRUPT_TYPE_IO_B) {
		// dequeue pcb from wait queue B
		PCB_p p = PCB_Queue_dequeue(waitingQueueB, &error);
		printf("IO COMPLETION: Moved from IO B:\t");
		PCB_print(p, &error);

		// print wait queue B
		printf("Waiting Queue B:");
		PCB_Queue_print(waitingQueueA, &error);

		// set the state of pcb just dequeued to ready
		PCB_set_state(p, PCB_STATE_READY, &error);

		// enqueue the pcb into the ready queue
		PCB_Priority_Queue_enqueue(readyQueue, p, &error);
	}
}


// function for timer interrupts
void isrTimer() {
	// print the current process
	printf("\nTIMER INTERRUPT: Switching from:\t");
	PCB_print(currentPCB, &error);

	// change current process state from running to interrupted
	PCB_set_state(currentPCB, PCB_STATE_INTERRUPTED, &error);

	// then save the cpu state to the pbc
	PCB_set_pc(currentPCB, sysStack, &error);

	// call scheduler with a timer interrupt
	scheduler(INTERRUPT_TYPE_TIMER);
}

void isrIO(enum INTERRUPT_TYPE interruptType) {
    // call scheduler so that it moves PCB from appropriate waitQ to readyQ
	scheduler(interruptType);
}

// process has terminated and needs to be put into the termination list
void terminate() {
	printf("\nTERMINATE: Switching from:\t");
	PCB_print(currentPCB, &error);
	PCB_set_pc(currentPCB, sysStack, &error);
	PCB_set_state(currentPCB, PCB_STATE_HALTED, &error);

	// set the process's termination to the computer clock time
	PCB_set_termination(currentPCB, time(NULL), &error);

	printf("Terminated:\t");
	PCB_print(currentPCB, &error);
	// add the process to the termination list
	PCB_Queue_enqueue(terminatedQueue, currentPCB, &error);

	// call to dispatcher
	dispatcher();
}

// function for io interrupts
void tsr(enum INTERRUPT_TYPE interruptType) {
	printf("\nIO TRAP HANDLER: Switching from:\t");
	PCB_print(currentPCB, &error);

	// change the current process state from running to waiting
	PCB_set_state(currentPCB, PCB_STATE_WAITING, &error);

	// then save the cpu state to the pcb
	PCB_set_pc(currentPCB, sysStack, &error);

	// if IO device A interrupts
	if (interruptType == INTERRUPT_TYPE_IO_A) {
		printf("Moved to IO A:\t");
		// move the current pcb into wait queue A
		PCB_Queue_enqueue(waitingQueueA, currentPCB, &error);
		PCB_print(currentPCB, &error);

		// print wait queue A
		printf("Waiting Queue A:");
		PCB_Queue_print(waitingQueueA, &error);

	// if IO device B interrupts
	} else if (interruptType == INTERRUPT_TYPE_IO_B) {
		printf("Moved to IO B:\t");
		// move the current pcb into wait queue B
		PCB_Queue_enqueue(waitingQueueB, currentPCB, &error);
		PCB_print(currentPCB, &error);

		// print wait queue B
		printf("Waiting Queue B:");
		PCB_Queue_print(waitingQueueB, &error);
	} 

	// call to dispatcher
	dispatcher();
}


// PCB_Queue createProcesses(createdQueue) {

// 	// randomly select a priority from 0 - 4
// 	int priority = rand() % MAX_PRIORITY_LEVELS;

// 	// check if that priority level is full, if full, pick a new priority level
// 	if () {
		
// 	}
// 	// if not full, 
// 	// check if priority is 0 -> make it compute intensive
// 		// if not 0 -> randomly pick process type 
// }


int main() {
    
    int i, j = 0;
    char ioThreadACreated = 0;
    char ioThreadBCreated = 0;
    
	pthread_t       system_timer, io_timer_a, io_timer_b;
	pthread_mutex_t mutex_timer, mutex_io_a, mutex_io_b;
	pthread_cond_t  cond_timer, cond_io_a, cond_io_b;
	timer_args_p    system_timer_args, io_timer_a_args, io_timer_b_args;
	
	pthread_mutex_init(&mutex_timer, NULL);
	pthread_mutex_init(&mutex_io_a, NULL);
	pthread_mutex_init(&mutex_io_b, NULL);
	
	pthread_cond_init(&cond_timer, NULL);
	pthread_cond_init(&cond_io_a, NULL);
	pthread_cond_init(&cond_io_b, NULL);
	
	system_timer_args = malloc(sizeof(timer_arguments));
	io_timer_a_args =   malloc(sizeof(timer_arguments));
	io_timer_b_args =   malloc(sizeof(timer_arguments));
	
	system_timer_args->mutex = &mutex_timer;
	system_timer_args->condition = &cond_timer;
	system_timer_args->sleep_length_min = SLEEP_TIME;
	system_timer_args->sleep_length_max = SLEEP_TIME;
	system_timer_args->flag = 1;
	
	io_timer_a_args->mutex = &mutex_io_a;
	io_timer_a_args->condition = &cond_io_a;
	io_timer_a_args->sleep_length_min = IO_TIME_MIN;
	io_timer_a_args->sleep_length_max = IO_TIME_MAX;
	io_timer_a_args->flag = 1;
	
	io_timer_b_args->mutex = &mutex_io_b;
	io_timer_b_args->condition = &cond_io_b;
	io_timer_b_args->sleep_length_min = IO_TIME_MIN;
	io_timer_b_args->sleep_length_max = IO_TIME_MAX;
	io_timer_b_args->flag = 1;
	
	srand(time(NULL));
	
	if(pthread_create(&system_timer, NULL, timer, (void*) system_timer_args)) {
		printf("\nERROR creating timer thread");
		return 1;
	}

/* =================================================== */
	// create the idle process
	idl = PCB_construct(&error);
	PCB_set_pid(idl, IDL_PID, &error);
	PCB_set_state(idl, PCB_STATE_RUNNING, &error);
	PCB_set_terminate(idl, 0, &error);
	PCB_set_max_pc(idl, 0, &error);
	for (i = 0; i < PCB_TRAP_LENGTH; i++) {
		idl->io_1_traps[i] = 1;
		idl->io_2_traps[i] = 1;
	}
	currentPCB = idl;
/* =================================================== */

	createdQueue = PCB_Queue_construct(&error);
	waitingQueueA = PCB_Queue_construct(&error);
	waitingQueueB = PCB_Queue_construct(&error);
	readyQueue = PCB_Priority_Queue_construct(&error);
	terminatedQueue = PCB_Queue_construct(&error);

	// create processes
	// createProcesses(createdQueue);
	for (j = 0; j < 5; j++) {
		PCB_p p = PCB_construct(&error);
		PCB_set_pid(p, j, &error);
		PCB_Queue_enqueue(createdQueue, p, &error);
		printf("Created:\t");
		PCB_print(p, &error);
	}
    
    // this will move everything from createdQ to readyQ
    // then will switch from idle process to first process created
    isrTimer();


    // while the wait queues aren't empty, or the ready queue isn't empty, or the current pcb running isn't the idle process
    // basically, while there is a process that isn't done yet
	while(!PCB_Queue_is_empty(waitingQueueA, &error) || !PCB_Queue_is_empty(waitingQueueB, &error) || get_PQ_size(readyQueue, &error) > 0 || currentPCB != idl) {
		
		if (error != PCB_SUCCESS) {
			printf("\nERROR: error != PCB_SUCCESS");
			return 1;
		}
		
        /*
         *
         *	CHECK FOR TIMER INTERRUPT
         *
		 * try to lock timer mutex
         * if we succeed, it means the timer is done with the lock and we now have the lock
         * if we don't succeed, it means timer is still nanosleeping, we move on
         */
		if(pthread_mutex_trylock(&mutex_timer) == 0) {

			// if flag is 0, timer is done nanosleeping
            if (system_timer_args->flag == 0) {
            	// set the flag to 1 to signal that the timer is nanosleeping now
                system_timer_args->flag = 1;

                // call isrTimer to switch to the next process
                isrTimer();
                pthread_cond_signal(&cond_timer);	
            }

            // unlock because if we entered the if statement then we must have acquired the lock
            // unlock so that timer can restart
            pthread_mutex_unlock(&mutex_timer);
		} 

		/*
		 *
		 *	CHECK FOR IO COMPLETION INTERRUPT FROM DEVICE A
		 *
		 */
		if(pthread_mutex_trylock(&mutex_io_a) == 0) {

			// check if timer a is done nanosleeping and if there's a process to be moved out of waitQ A
            if (io_timer_a_args->flag == 0 && !PCB_Queue_is_empty(waitingQueueA, &error)) {

                // call scheduler so that it moves PCB from appropriate waitQ to readyQ
                isrIO(INTERRUPT_TYPE_IO_A);

                // if the waitQ is not empty, then restart the io timer
                if (!PCB_Queue_is_empty(waitingQueueA, &error)) {
                    io_timer_a_args->flag = 1;
                    pthread_cond_signal(&cond_io_a);
                }
            }

			pthread_mutex_unlock(&mutex_io_a);
		} 

		/*
		 *
		 *	CHECK FOR IO COMPLETION INTERRUPT FROM DEVICE B
		 *
		 */
		if(pthread_mutex_trylock(&mutex_io_b) == 0) { 

			// check if timer a is done nanosleeping and if there's a process to be moved out of waitQ B
            if (io_timer_b_args->flag == 0 && !PCB_Queue_is_empty(waitingQueueB, &error)) {

                // call scheduler so that it moves PCB from appropriate waitQ to readyQ
                isrIO(INTERRUPT_TYPE_IO_B);

                // if the waitQ is not empty, then restart the io timer
                if (!PCB_Queue_is_empty(waitingQueueB, &error)) {
                    io_timer_b_args->flag = 1;
                    pthread_cond_signal(&cond_io_b);
                }
            }
			pthread_mutex_unlock(&mutex_io_b);
		}

		/*
		 *
		 *	CHECK IF WE'VE REACHED THE MAX PC
		 *
		 */
		if (sysStack >= currentPCB->max_pc) {
			sysStack = 0;
			currentPCB->term_count++;

			// if the process is one that is supposed to terminate (!= 0)
			// and if the process's terminate is 
			if (currentPCB->terminate != 0 && 
					currentPCB->terminate == currentPCB->term_count) {
				terminate();
			}
		} else {
			sysStack++;
		}

		for (i = 0; i < PCB_TRAP_LENGTH; i++) {
			if (sysStack == currentPCB->io_1_traps[i]) {
                // enQ pcb into waitQ, get pcb from readyQ
				tsr(INTERRUPT_TYPE_IO_A);

                if (ioThreadACreated == 0) {
                    // this means it's the first IO trap for this IO timer
                    ioThreadACreated = 1;
                    if (pthread_create(&io_timer_a, NULL, timer, (void*) io_timer_a_args)) {
                        printf("\nERROR creating io thread a");
                        return 1;
                    } else {
                        printf("\nIO Timer A thread created\n");
                    }
                } else if (pthread_mutex_trylock(&mutex_io_a) == 0) {
                    // if we're able to lock, means the IO device is stuck waiting on condition
					io_timer_a_args->flag = 1;
					pthread_cond_signal(&cond_io_a);
					pthread_mutex_unlock(&mutex_io_a);
				} 
			} else if (sysStack == currentPCB->io_2_traps[i]) {
				tsr(INTERRUPT_TYPE_IO_B);
				
                if (ioThreadBCreated == 0) {
                    // this means it's the first IO trap for this IO timer
                    ioThreadBCreated = 1;
                    if(pthread_create(&io_timer_b, NULL, timer, (void*) io_timer_b_args)) {
                        printf("\nERROR creating io thread b");
                        return 1;
                    } else {
                        printf("\nIO Timer B thread created\n");
                    }
                } else if(pthread_mutex_trylock(&mutex_io_b) == 0) {
					io_timer_b_args->flag = 1;
					pthread_cond_signal(&cond_io_b);
					pthread_mutex_unlock(&mutex_io_b);
				} 
			}
		}
	}
    
    // todo: deallocate memory, destroy stuff like mutexes etc
    
    return 0;
}