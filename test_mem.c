// test stack and heap growth
// command line argument: 1 for stack, 2 for heap

#include <hardware.h>
#include <yalnix.h>
#include <stdlib.h>
#include <assert.h>

void stack(int turn){
	int a[2000]; // 8K, one page
	TtyPrintf(0, "%d page allocated.\n", turn+1);
	stack(turn+1);
}

void heap(int turn){
	int a[2000]; // 8K, one page
	TtyPrintf(0, "%d page allocated.\n", turn+1);
	heap(turn+1);
}

int main(int argc, char* argv[]){
	TracePrintf(0, "argc: %d\n", argc);
	if (argc == 0){
		TracePrintf(0, "need an argument. please enter 1 or 2. exiting.\n");
		Exit(0);
	}

	if (argv[0][0] == '1')
		stack(0);
	else if (argv[0][0] == '2')
		heap(0);
	else{
		TracePrintf(0, "bad argument. please enter 1 or 2. exiting.\n");
		Exit(0);
	}

}





