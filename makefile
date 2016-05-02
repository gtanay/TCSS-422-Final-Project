CFLAGS=-Wall -std=c99




all: CPU


CPU: CPU.o PCB.o PCB_Queue.o
	gcc CPU.o PCB.o PCB_Queue.o -o CPU





CPU.o: CPU.c
	gcc $(CFLAGS) -c CPU.c
PCB_Queue.o: PCB_Queue.c
	gcc $(CFLAGS) -c PCB_Queue.c 
PCB.o: PCB.c
	gcc $(CFLAGS) -c PCB.c