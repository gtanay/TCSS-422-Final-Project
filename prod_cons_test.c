#include "PCB.h"
#include "mutex.h"
#include "cond_var.h"

int producer_consumer_var[10];
enum PCB_ERROR err;


int main() {
	PCB_p mutualResourceUser1 = PCB_construct(&err);
	PCB_set_type(mutualResourceUser1, MUTUAL_RESOURCE_PROCESS, &err);


	PCB_p mutualResourceUser2 = PCB_construct(&err);
	PCB_set_type(mutualResourceUser2, MUTUAL_RESOURCE_PROCESS, &err);

	PCB_print(mutualResourceUser1, &err);
	PCB_print(mutualResourceUser2, &err);
}