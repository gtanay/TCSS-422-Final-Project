#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "PCB.h"
#include "PCB_Queue.h"

#define SLEEP_TIME 1000000
#define IO_TIME 1000

typedef struct timer_arguments{
	pthread_mutex_t* mutex;
	pthread_cond_t* condition;
};

typedef timer_arguments* timer_args_p;


void* fixedTimer(void* arguments) {
	struct timespec sleep_time;
	timer_args_p args = (timer_args_p) arguments 
	
	sleep_time.tv_nanoseconds = SLEEP_TIME;
	pthread_mutex_lock(args->mutex);
	for(;;) {
		nanosleep(sleep_time, NULL);
		pthread_cond_wait(args->condition, args->mutex);
	}
}

void* ioTimer(void* arguments) {
	struct timespec sleep_time;
	timer_args_p args = (timer_args_p) arguments 
	
	sleep_time.tv_nanoseconds = SLEEP_TIME * (rand() % IO_TIME);
	pthread_mutex_lock(args->mutex);
	for(;;) {
		nanosleep(sleep_time, NULL);
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
	
	system_timer_args = malloc(sizeof(struct timer_arguments));
	io_timer_a_args = malloc(sizeof(struct timer_arguments));
	io_timer_b_args = malloc(sizeof(struct timer_arguments));
	
	system_timer_args->mutex = &mutex_timer;
	system_timer_args->condition = &cond_timer;
	
	io_timer_a_args->mutex = &mutex_io_a;
	io_timer_a_args->condition = &cond_io_a;
	
	io_timer_a_args->mutex = &mutex_io_a;
	io_timer_a_args->condition = &cond_io_a;
	
	srand(time(NULL));
	
	if(pthread_create(&system_timer, NULL, fixed_timer, (void*) system_timer_args)) {
		printf("\nERROR creating timer thread");
		return 1;
	}
	if(pthread_create(&io_timer_a, NULL, random_timer, (void*) io_timer_a_args)) {
		printf("\nERROR creating io thread");
		return 1;
	}
	if(pthread_create(&io_timer_b, NULL, random_timer, (void*) io_timer_a_args)) {
		printf("\nERROR creating io thread");
		return 1;
	}
	
	while(1){
		if(!pthread_mutex_trylock(&mutex_timer)) {
			printf("foo");
			pthread_cond_signal(&cond_timer);
			pthread_mutex_unlock(&mutex_timer);
		}
		if(!pthread_mutex_trylock(&mutex_io_a)) {
			printf("bar");
			pthread_cond_signal(&cond_io_a);
			pthread_mutex_unlock(&mutex_io_a);
		}
		if(!pthread_mutex_trylock(&mutex_io_b)) {
			printf("foo");
			pthread_cond_signal(&cond_io_b);
			pthread_mutex_unlock(&mutex_io_b);
		}
	}
}
