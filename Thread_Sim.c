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

// time must be under 1 billion
#define SLEEP_TIME 90000000
#define IO_TIME_MIN SLEEP_TIME * 3
#define IO_TIME_MAX SLEEP_TIME * 5

#define IDL_PID 0xFFFFFFFF

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
PCB_Queue_p readyQueue;
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
    printf("timer: before locking mutex\n");
	pthread_mutex_lock(args->mutex);
    printf("timer: after locking mutex\n");
    sleep_time.tv_sec = 0;
	for(;;) {
		sleep_time.tv_nsec = args->sleep_length_min + rand() % (args->sleep_length_max - args->sleep_length_min + 1);
		//printf("sleeping %d\n", sleep_length);
        printf("timer: before sleeping for %d ns\n", (int) sleep_time.tv_nsec);
		nanosleep(&sleep_time, NULL);
        printf("timer: after sleep\n");
		//printf("done sleeping %d\n", sleep_length);
		args->flag = 0;
		pthread_cond_wait(args->condition, args->mutex);
	}
}

void dispatcher() {
	if (!PCB_Queue_is_empty(readyQueue, &error)) {
		currentPCB = PCB_Queue_dequeue(readyQueue, &error);
	} else {
		currentPCB = idl;
	}
	printf("\nSwitching to:\t");
	PCB_print(currentPCB, &error);
	printf("Ready Queue:\t");
	PCB_Queue_print(readyQueue, &error);
	PCB_set_state(currentPCB, PCB_STATE_RUNNING, &error);
	sysStack = PCB_get_pc(currentPCB, &error);
}

void scheduler(enum INTERRUPT_TYPE interruptType) {
	while (!PCB_Queue_is_empty(terminatedQueue, &error)) {
		PCB_p p = PCB_Queue_dequeue(terminatedQueue, &error);
		printf("Deallocated:\t");
		PCB_print(p, &error);
		PCB_destruct(p, &error);
	}
	while (!PCB_Queue_is_empty(createdQueue, &error)) {
		PCB_p p = PCB_Queue_dequeue(createdQueue, &error);
		PCB_set_state(p, PCB_STATE_READY, &error);
		printf("Scheduled:\t");
		PCB_print(p, &error);
		PCB_Queue_enqueue(readyQueue, p, &error);
	}
	if (interruptType == INTERRUPT_TYPE_TIMER) {
		PCB_set_state(currentPCB, PCB_STATE_READY, &error);
		if (PCB_get_pid(currentPCB, &error) != IDL_PID) {
			printf("Returned:\t");
			PCB_print(currentPCB, &error);
			PCB_Queue_enqueue(readyQueue, currentPCB, &error);
		}
		dispatcher();
	} else if (interruptType == INTERRUPT_TYPE_IO_A) {
		PCB_p p = PCB_Queue_dequeue(waitingQueueA, &error);
		printf("Moved from IO A:");
		PCB_print(p, &error);
		printf("Waiting Queue A:");
		PCB_Queue_print(waitingQueueA, &error);
		PCB_set_state(p, PCB_STATE_READY, &error);
		PCB_Queue_enqueue(readyQueue, p, &error);
	} else if (interruptType == INTERRUPT_TYPE_IO_B) {
		PCB_p p = PCB_Queue_dequeue(waitingQueueB, &error);
		printf("Moved from IO B:");
		PCB_print(p, &error);
		printf("Waiting Queue B:");
		PCB_Queue_print(waitingQueueA, &error);
		PCB_set_state(p, PCB_STATE_READY, &error);
		PCB_Queue_enqueue(readyQueue, p, &error);
	}
}

void isrTimer() {
	printf("\nDue to timer, switching from:\t");
	PCB_print(currentPCB, &error);
	PCB_set_state(currentPCB, PCB_STATE_INTERRUPTED, &error);
	PCB_set_pc(currentPCB, sysStack, &error);
	scheduler(INTERRUPT_TYPE_TIMER);
}

void isrIO(enum INTERRUPT_TYPE interruptType) {
    // call scheduler so that it moves PCB from appropriate waitQ to readyQ
	scheduler(interruptType);
}

void terminate() {
	printf("\nTerminate: switching from:\t");
	PCB_print(currentPCB, &error);
	PCB_set_pc(currentPCB, sysStack, &error);
	PCB_set_state(currentPCB, PCB_STATE_HALTED, &error);
	PCB_set_termination(currentPCB, time(NULL), &error);
	printf("Terminated:\t");
	PCB_print(currentPCB, &error);
	PCB_Queue_enqueue(terminatedQueue, currentPCB, &error);
	dispatcher();
}

void tsr(enum INTERRUPT_TYPE interruptType) {
	printf("\nTrap: switching from:\t");
	PCB_print(currentPCB, &error);
	PCB_set_pc(currentPCB, sysStack, &error);
	PCB_set_state(currentPCB, PCB_STATE_WAITING, &error);
	if (interruptType == INTERRUPT_TYPE_IO_A) {
		printf("Moved to IO A:\t");
		PCB_Queue_enqueue(waitingQueueA, currentPCB, &error);
		PCB_print(currentPCB, &error);
		printf("Waiting Queue A:");
		PCB_Queue_print(waitingQueueA, &error);
	} else if (interruptType == INTERRUPT_TYPE_IO_B) {
		printf("Moved to IO B:\t");
		PCB_Queue_enqueue(waitingQueueB, currentPCB, &error);
		PCB_print(currentPCB, &error);
		printf("Waiting Queue B:");
		PCB_Queue_print(waitingQueueB, &error);
	} 
	dispatcher();
}

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
    //
    // moved IO thread creates because IO timers need to start when appropriate trap is handled for the first
    // time
    //
	
	idl = PCB_construct(&error);
	PCB_set_pid(idl, IDL_PID, &error);
	PCB_set_state(idl, PCB_STATE_RUNNING, &error);
	PCB_set_terminate(idl, 0, &error);
	PCB_set_max_pc(idl, 0, &error);
	for (i = 0; i < PCB_TRAP_LENGTH; i++) {
        // is this ok?
		idl->io_1_traps[i] = 1;
		idl->io_2_traps[i] = 1;
	}
	currentPCB = idl;

	createdQueue = PCB_Queue_construct(&error);
	waitingQueueA = PCB_Queue_construct(&error);
	waitingQueueB = PCB_Queue_construct(&error);
	readyQueue = PCB_Queue_construct(&error);
	terminatedQueue = PCB_Queue_construct(&error);

	for (j = 0; j < 16; j++) {
		PCB_p p = PCB_construct(&error);
		PCB_set_pid(p, j, &error);
		PCB_Queue_enqueue(createdQueue, p, &error);
		printf("Created:\t");
		PCB_print(p, &error);
	}
    
    // call scheduler to move all created PCBs to ready queue
    //scheduler(INTERRUPT_TYPE_INVALID);
    
    // this will move everything from createdQ to readyQ and then will switch from idle to first process created
    isrTimer();

	while(!PCB_Queue_is_empty(waitingQueueA, &error) || !PCB_Queue_is_empty(waitingQueueB, &error) || !PCB_Queue_is_empty(readyQueue, &error) || currentPCB != idl) {
		if (error != PCB_SUCCESS) {
			printf("\nERROR: error != PCB_SUCCESS");
			return 1;
		}
		
        //
        // try to lock timer mutex
        // if we succeed, it means the timer is done with the lock and we now have the lock
        // if we don't succeed, it means timer is still nanosleeping, we move on
        //
		if(pthread_mutex_trylock(&mutex_timer) == 0) {
            if (system_timer_args->flag == 0) {
                //
                // if flag is 0 it means timer is done nanosleeping
                //
                system_timer_args->flag = 1;
                isrTimer();
                pthread_cond_signal(&cond_timer);	
            }
            //
            // unlock because if we entered this if statement then we must have acquired the lock
            // unlock so that timer can restart
            //
            pthread_mutex_unlock(&mutex_timer);
		} 

		if(pthread_mutex_trylock(&mutex_io_a) == 0) {
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
		if(pthread_mutex_trylock(&mutex_io_b) == 0) { 
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

		if (sysStack >= currentPCB->max_pc) {
			sysStack = 0;
			currentPCB->term_count++;
			if (currentPCB->terminate != 0 && 
					currentPCB->terminate <= currentPCB->term_count) {
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