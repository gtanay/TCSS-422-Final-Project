#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "PCB.h"
#include "PCB_Queue.h"

#define SLEEP_TIME 100000000
#define IO_TIME 1000

typedef struct {
	pthread_mutex_t* mutex;
	pthread_cond_t* condition;
} timer_arguments;

typedef timer_arguments* timer_args_p;

void* fixedTimer(void* arguments) {
	struct timespec sleep_time;
	timer_args_p args = (timer_args_p) arguments; 
	sleep_time.tv_nsec = SLEEP_TIME;
	pthread_mutex_lock(args->mutex);
	for(;;) {
		printf("a\n");
		nanosleep(&sleep_time, NULL);
		pthread_cond_wait(args->condition, args->mutex);
	}
}

void* ioTimer(void* arguments) {
	struct timespec sleep_time;
	timer_args_p args = (timer_args_p) arguments; 
	
	sleep_time.tv_nsec = SLEEP_TIME;// * (rand() % IO_TIME);
	pthread_mutex_lock(args->mutex);
	for(;;) {
		printf("bbbb\n");
		nanosleep(&sleep_time, NULL);
		pthread_cond_wait(args->condition, args->mutex);
	}
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
		printf("\nERROR creating io thread");
		return 1;
	}
	if(pthread_create(&io_timer_b, NULL, &ioTimer, (void*) io_timer_a_args)) {
		printf("\nERROR creating io thread");
		return 1;
	}
	
	while(1){
		//pc++
		if(!pthread_mutex_trylock(&mutex_timer)) {
			
			//timer is over
			//timer isr, switch to next pcb
			pthread_cond_signal(&cond_timer);	
			pthread_mutex_unlock(&mutex_timer);
		} 
		//timer is still sleeping
			
		if(!pthread_mutex_trylock(&mutex_io_a)) {
			//move head of waiting queue to ready queue
			pthread_cond_signal(&cond_io_a);
			pthread_mutex_unlock(&mutex_io_a);
		} 
		if(!pthread_mutex_trylock(&mutex_io_b)) {
			pthread_cond_signal(&cond_io_b);
			pthread_mutex_unlock(&mutex_io_b);
		} 


		//if (pc == io trap) {
			//call io trap handler
			//		move pcb to waiting queue
		//}
	

	}
}
