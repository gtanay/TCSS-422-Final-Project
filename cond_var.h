/*
 * TCSS 422 Final Project
 * Team 4
 * Tempest Parr, Paul Zander, Geoffrey Tanay, Justin Clark
 */

#pragma once

#include "PCB.h"
#include "mutex.h"

typedef struct condition_var_type {
	PCB_Queue_p waiting;
} Condition_Variable;

typedef Condition_Variable * Condition_p;

Condition_p Condition_construct(int, enum PCB_ERROR);
void Condition_destruct(Condition_p, enum PCB_ERROR);