#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "PCB.h"
#include "PCB_Queue.h"
#include "PCB_Errors.h"

#define SLEEP_TIME 100000000
#define IO_TIME 1000
#define IDL_PID 0xFFFFFFFF

typedef struct {
	pthread_mutex_t* mutex;
	pthread_cond_t* condition;
} timer_arguments;

typedef timer_arguments* timer_args_p;

PCB_Queue_p createdQueue;
PCB_Queue_p readyQueue;
PCB_Queue_p terminatedQueue;
PCB_p currentPCB;
PCB_p idl;
unsigned long sysStack;
enum PCB_ERROR error = PCB_SUCCESS;

void* fixedTimer(void* arguments) {
	struct timespec sleep_time;
	timer_args_p args = (timer_args_p) arguments; 
	sleep_time.tv_nsec = SLEEP_TIME;
	pthread_mutex_lock(args->mutex); 
	for(;;) {
		// nanosleep(&sleep_time, NULL);
		sleep(3);
		pthread_cond_wait(args->condition, args->mutex);
	}
}

void* ioTimer(void* arguments) {
	struct timespec sleep_time;
	timer_args_p args = (timer_args_p) arguments; 
	
	sleep_time.tv_nsec = SLEEP_TIME;// * (rand() % IO_TIME);
	pthread_mutex_lock(args->mutex);
	for(;;) {
		// nanosleep(&sleep_time, NULL);
		sleep(3);
		pthread_cond_wait(args->condition, args->mutex);
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

void scheduler(int interruptType) {
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

void terminate() {
	PCB_set_pc(currentPCB, sysStack, &error);
	PCB_set_state(currentPCB, PCB_STATE_HALTED, &error);
	printf("Terminated:\t");
	PCB_print(currentPCB, &error);
	PCB_Queue_enqueue(terminatedQueue, currentPCB, &error);
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
	
	io_timer_a_args->mutex = &mutex_io_a;
	io_timer_a_args->condition = &cond_io_a;
	
	io_timer_b_args->mutex = &mutex_io_b;
	io_timer_b_args->condition = &cond_io_b;
	
	srand(time(NULL));
	
	if(pthread_create(&system_timer, NULL, &fixedTimer, (void*) system_timer_args)) {
		printf("\nERROR creating timer thread");
		return 1;
	}
	if(pthread_create(&io_timer_a, NULL, &ioTimer, (void*) io_timer_a_args)) {
		printf("\nERROR creating io thread a");
		return 1;
	}
	if(pthread_create(&io_timer_b, NULL, &ioTimer, (void*) io_timer_a_args)) {
		printf("\nERROR creating io thread b");
		return 1;
	}
	
	idl = PCB_construct(&error);
	PCB_set_pid(idl, IDL_PID, &error);
	PCB_set_state(idl, PCB_STATE_RUNNING, &error);
	PCB_set_terminate(idl, 0, &error);
	currentPCB = idl;

	createdQueue = PCB_Queue_construct(&error);
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
			exit(EXIT_FAILURE);
		}

		sysStack++;
		if(!pthread_mutex_trylock(&mutex_timer)) {
			printf("\n");
			printf("Switching from:\t");
			PCB_print(currentPCB, &error);
			isrTimer();
			pthread_cond_signal(&cond_timer);	
			pthread_mutex_unlock(&mutex_timer);
			sleep(1); //temp fix
		} 
			
		if(!pthread_mutex_trylock(&mutex_io_a)) {
			//isr
			pthread_cond_signal(&cond_io_a);
			pthread_mutex_unlock(&mutex_io_a);
		} 
		if(!pthread_mutex_trylock(&mutex_io_b)) {
			//isr
			pthread_cond_signal(&cond_io_b);
			pthread_mutex_unlock(&mutex_io_b);
		} 


		//if (pc == io trap) {
			//call io trap handler
			//		move pcb to waiting queue
		//}
		if (sysStack >= currentPCB->max_pc) {
			sysStack = 0;
			currentPCB->term_count++;
			if (currentPCB->terminate != 0 && 
					currentPCB->terminate <= currentPCB->term_count) {
				terminate();
			}
		}

	}
}
