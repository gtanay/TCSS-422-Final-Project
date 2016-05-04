CFLAGS=-Wall 




all: Thread_Sim


Thread_Sim: Thread_Sim.o PCB.o PCB_Queue.o
	gcc Thread_Sim.o PCB.o PCB_Queue.o -o Thread_Sim





Thread_Sim.o: Thread_Sim.c
	gcc $(CFLAGS) -c Thread_Sim.c
PCB_Queue.o: PCB_Queue.c
	gcc $(CFLAGS) -c PCB_Queue.c 
PCB.o: PCB.c
	gcc $(CFLAGS) -c PCB.c