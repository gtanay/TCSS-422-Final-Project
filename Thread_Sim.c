#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "PCB.h"
#include "PCB_Queue.h"
#include "PCB_Errors.h"

// time must be under 1 billion
#define SLEEP_TIME 90000000
#define IO_TIME_MIN SLEEP_TIME * 5
#define IO_TIME_MAX SLEEP_TIME * 5

#define IDL_PID 0xFFFFFFFF

enum INTERRUPT_TYPE {
	INTERRUPT_TYPE_TIMER = 1, 
	INTERRUPT_TYPE_IO_A, 
	INTERRUPT_TYPE_IO_B
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
	struct timespec sleep_time;
	timer_args_p args = (timer_args_p) arguments; 
	pthread_mutex_lock(args->mutex);
	for(;;) {
		int sleep_length = args->sleep_length_min + rand() % (args->sleep_length_max - args->sleep_length_min + 1);
		sleep_time.tv_nsec = sleep_length;
		nanosleep(&sleep_time, NULL);
		printf("timer %u\n", sleep_length); // debug
		pthread_cond_wait(args->condition, args->mutex);
		args->flag = 0;
	}
}

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
		printf("Moved from IO A\t");
		PCB_print(p, &error);
		PCB_set_state(p, PCB_STATE_READY, &error);
		PCB_Queue_enqueue(readyQueue, p, &error);
	} else if (interruptType == INTERRUPT_TYPE_IO_B) {
		PCB_p p = PCB_Queue_dequeue(waitingQueueB, &error);
		printf("Moved from IO B\t");
		PCB_print(p, &error);
		PCB_set_state(p, PCB_STATE_READY, &error);
		PCB_Queue_enqueue(readyQueue, p, &error);
	}
}

void isrTimer() {
	printf("\nSwitching from:\t");
	PCB_print(currentPCB, &error);
	PCB_set_state(currentPCB, PCB_STATE_INTERRUPTED, &error);
	PCB_set_pc(currentPCB, sysStack, &error);
	scheduler(INTERRUPT_TYPE_TIMER);
}

void isrIO(enum INTERRUPT_TYPE interruptType) {
	scheduler(interruptType);
}

void terminate() {
	printf("Switching from:\t");
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
	printf("Switching from:\t");
	PCB_print(currentPCB, &error);
	PCB_set_pc(currentPCB, sysStack, &error);
	PCB_set_state(currentPCB, PCB_STATE_WAITING, &error);
	if (interruptType == INTERRUPT_TYPE_IO_A) {
		printf("Moved to IO A:\t");
		PCB_Queue_enqueue(waitingQueueA, currentPCB, &error);
	} else if (interruptType == INTERRUPT_TYPE_IO_B) {
		printf("Moved to IO B:\t");
		PCB_Queue_enqueue(waitingQueueB, currentPCB, &error);
	} else {
		printf("ERROR: invalid io device");
		exit(EXIT_FAILURE);
	}
	PCB_print(currentPCB, &error);
	dispatcher();
}

int main() {
	pthread_t system_timer, io_timer_a, io_timer_b;
	pthread_mutex_t mutex_timer, mutex_io_a, mutex_io_b;
	pthread_cond_t cond_timer, cond_io_a, cond_io_b;
	timer_args_p system_timer_args, io_timer_a_args, io_timer_b_args;
	
	pthread_mutex_init(&mutex_timer, NULL);
	pthread_mutex_init(&mutex_io_a, NULL);
	pthread_mutex_init(&mutex_io_b, NULL);
	
	pthread_cond_init(&cond_timer, NULL);
	pthread_cond_init(&cond_io_a, NULL);
	pthread_cond_init(&cond_io_b, NULL);
	
	system_timer_args = malloc(sizeof(timer_arguments));
	io_timer_a_args = malloc(sizeof(timer_arguments));
	io_timer_b_args = malloc(sizeof(timer_arguments));
	
	system_timer_args->mutex = &mutex_timer;
	system_timer_args->condition = &cond_timer;
	system_timer_args->sleep_length_min = SLEEP_TIME;
	system_timer_args->sleep_length_max = SLEEP_TIME;
	
	io_timer_a_args->mutex = &mutex_io_a;
	io_timer_a_args->condition = &cond_io_a;
	io_timer_a_args->sleep_length_min = IO_TIME_MIN;
	io_timer_a_args->sleep_length_max = IO_TIME_MAX;
	
	io_timer_b_args->mutex = &mutex_io_b;
	io_timer_b_args->condition = &cond_io_b;
	io_timer_b_args->sleep_length_min = IO_TIME_MIN;
	io_timer_b_args->sleep_length_max = IO_TIME_MAX;
	
	srand(time(NULL));
	
	if(pthread_create(&system_timer, NULL, &timer, (void*) system_timer_args)) {
		printf("\nERROR creating timer thread");
		return 1;
	}
	if(pthread_create(&io_timer_a, NULL, &timer, (void*) io_timer_a_args)) {
		printf("\nERROR creating io thread a");
		return 1;
	}
	if(pthread_create(&io_timer_b, NULL, &timer, (void*) io_timer_b_args)) {
		printf("\nERROR creating io thread b");
		return 1;
	}
	
	idl = PCB_construct(&error);
	PCB_set_pid(idl, IDL_PID, &error);
	PCB_set_state(idl, PCB_STATE_RUNNING, &error);
	PCB_set_terminate(idl, 0, &error);
	PCB_set_max_pc(idl, 0, &error);
	for (int i = 0; i < PCB_TRAP_LENGTH; i++) {
		idl->io_1_traps[i] = 1;
		idl->io_2_traps[i] = 1;
	}
	currentPCB = idl;

	createdQueue = PCB_Queue_construct(&error);
	waitingQueueA = PCB_Queue_construct(&error);
	waitingQueueB = PCB_Queue_construct(&error);
	readyQueue = PCB_Queue_construct(&error);
	terminatedQueue = PCB_Queue_construct(&error);

	for (int j = 0; j < 5; j++) {
		PCB_p p = PCB_construct(&error);
		PCB_set_pid(p, j, &error);
		PCB_Queue_enqueue(createdQueue, p, &error);
		printf("Created:\t");
		PCB_print(p, &error);
	}

	while(1) {
		if (error != PCB_SUCCESS) {
			printf("\nERROR: error != PCB_SUCCESS");
			return 1;
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
		
		if(system_timer_args->flag == 0 && !pthread_mutex_trylock(&mutex_timer)) {
			system_timer_args->flag = 1;
			isrTimer();
			pthread_cond_signal(&cond_timer);	
			pthread_mutex_unlock(&mutex_timer);
		} 
		if(io_timer_a_args->flag == 0 && !pthread_mutex_trylock(&mutex_io_a)) {
			io_timer_a_args->flag = 1;
			// isrIO(INTERRUPT_TYPE_IO_A);
			pthread_cond_signal(&cond_io_a);
			pthread_mutex_unlock(&mutex_io_a);
		} 
		if(io_timer_b_args->flag == 0 && !pthread_mutex_trylock(&mutex_io_b)) {
			io_timer_b_args->flag = 1;
			// isrIO(INTERRUPT_TYPE_IO_B);
			pthread_cond_signal(&cond_io_b);
			pthread_mutex_unlock(&mutex_io_b);
		} 

		for (int i = 0; i < PCB_TRAP_LENGTH; i++) {
			if (sysStack == currentPCB->io_1_traps[i]) {
				// tsr(INTERRUPT_TYPE_IO_A);
			} else if (sysStack == currentPCB->io_2_traps[i]) {
				// tsr(INTERRUPT_TYPE_IO_B);
			}
		}
	}
}
