#include "yalnix.h"
#ifdef LINUX
#define NULL 0
#endif

int main(){

	TtyPrintf(0, "I'm exec!\n");
	TtyPrintf(0, "I'm exiting with 42...\n");
	Exit(42);
}
