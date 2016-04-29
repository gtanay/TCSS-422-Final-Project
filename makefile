CFLAGS=-Wall -std=c99




all: Round_Robin


Round_Robin: Round_Robin.o PCB.o PCB_Queue.o
	gcc Round_Robin.o PCB.o PCB_Queue.o -o Round_Robin





Round_Robin.o: Round_Robin.c
	gcc $(CFLAGS) -c Round_Robin.c
PCB_Queue.o: PCB_Queue.c
	gcc $(CFLAGS) -c PCB_Queue.c 
PCB.o: PCB.c
	gcc $(CFLAGS) -c PCB.c