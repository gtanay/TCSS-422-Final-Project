#pragma once

#include "PCB_Errors.h"

#define PCB_PRIORITY_MAX 15

enum PCB_STATE_TYPE {
	PCB_STATE_NEW = 0, 
	PCB_STATE_READY, 
	PCB_STATE_RUNNING, 
	PCB_STATE_INTERRUPTED, 
	PCB_STATE_WAITING, 
	PCB_STATE_HALTED,

	PCB_STATE_LAST_ERROR, // invalid type, used for bounds checking

	PCB_STATE_ERROR // Used as return value if null pointer is passed to getter.
};

struct PCB {
    unsigned long pid;        // process ID #, a unique number
	enum PCB_STATE_TYPE state;    // process state (running, waiting, etc.)
	unsigned short priority;  // priorities 0=highest, 15=lowest
	unsigned long pc;         // holds the current pc value when preempted
};

typedef struct PCB * PCB_p;

PCB_p PCB_construct(enum PCB_ERROR*); // returns a pcb pointer to heap allocation
void PCB_destruct(PCB_p, enum PCB_ERROR*);  // deallocates pcb from the heap
void PCB_init(PCB_p, enum PCB_ERROR*);       // sets default values for member data

void PCB_set_pid(PCB_p, unsigned long, enum PCB_ERROR*);///////
void PCB_set_state(PCB_p, enum PCB_STATE_TYPE, enum PCB_ERROR*);
void PCB_set_priority(PCB_p, unsigned short, enum PCB_ERROR*);
void PCB_set_pc(PCB_p, unsigned long, enum PCB_ERROR*);

unsigned long PCB_get_pid(PCB_p, enum PCB_ERROR*);  // returns pid of the process
enum PCB_STATE_TYPE PCB_get_state(PCB_p, enum PCB_ERROR*);
unsigned short PCB_get_priority(PCB_p, enum PCB_ERROR*);
unsigned long PCB_get_pc(PCB_p, enum PCB_ERROR*);

void PCB_print(PCB_p, enum PCB_ERROR*);  // a string representing the contents of the pcb

